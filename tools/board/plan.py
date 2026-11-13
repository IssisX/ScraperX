#!/usr/bin/env python3
"""Edit only work/plan.md; GitHub writes are performed separately by the integrator.

All mutations are validated, version-checked and atomically saved locally.
This CLI never mails agents, changes labels or writes to a remote branch.
"""
from __future__ import annotations

import argparse
import datetime as dt
from dataclasses import replace
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(HERE / "tools/switchboard"))
sys.path.insert(1, str(HERE / "tools/board"))   # after the switchboard: `plan` is its plan.py, never this file
import plan as core  # noqa: E402
from ghapi import GitHub  # noqa: E402

REPO = "IssisX/ScraperX"


def sha(data: str) -> str:
    return hashlib.sha256(data.encode("utf-8")).hexdigest()[:12]


def recent_flip(path: Path, job: str) -> bool:
    """An order changed in a recent commit; an initial snapshot insertion is not a flip."""
    try:
        p = subprocess.run(["git", "-C", str(HERE), "log", "--since=1.day", "--format=COMMIT:%H", "-p", "--",
                            str(path.relative_to(HERE))], text=True, capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError, ValueError):
        return True  # cannot prove the edit is safe
    for commit in p.stdout.split("COMMIT:")[1:]:
        removed = re.search(rf"^-{re.escape(job)}(?:\s|$)", commit, re.M)
        added = re.search(rf"^\+{re.escape(job)}(?:\s|$)", commit, re.M)
        if removed and added:
            return True
    return False


def in_flight(job: str, branches: list, prs: list[dict], inactive: set[str] | frozenset[str] = frozenset(),
              kept: tuple = ()) -> bool:
    """Remote work still needing disposition, including foreign owners and closed-unmerged PR heads. A head the
    plan keeps as a record (`kept <branch>@<sha>`, still at that sha) has had its disposition."""
    ruled = {core.branch_sha(b) for b in branches
             if any(core.branch_name(b) == ref and core.branch_sha(b).startswith(pin) for ref, pin in kept)}
    if core.live_refs(job, branches, frozenset(inactive) | ruled):
        return True
    return any((m := core.PR_JOB.match(pr.get("body") or "")) and m.group(1) == job
               for pr in prs if pr.get("state") != "closed")


def stamp(t: dt.datetime) -> str:
    return t.astimezone(dt.timezone.utc).strftime("%Y-%m-%dT%H:%MZ")


def change(source: str, action: str, args: list[str], *, branches: list = (),
           prs: list[dict] = (), inactive: set[str] | frozenset[str] = frozenset(),
           stop_work: str = "", again: str = "", flipped: bool = False,
           verified_closed: set[str] | frozenset[str] = frozenset(), now: dt.datetime | None = None,
           history: dict[str, str] | None = None) -> str:
    """One edit. `now` stamps a job added or given (its own clock, core.AT); `history` (job -> when its line got its
    owner, read from the plan file's commits: assigned_times) is what `stamp` writes on the lines that have none."""
    now = now or dt.datetime.now(dt.timezone.utc)
    p = core.parse(source)
    if p.errors:
        raise ValueError("plan invalid: " + "; ".join(p.errors))
    ls, limits = list(p.lines), dict(p.limits)
    if action == "limit":
        if len(args) != 2 or not args[1].isdigit() or int(args[1]) < 1:
            raise ValueError("limit expects <owner> <positive count>")
        limits[args[0]] = int(args[1])
    elif action == "add":
        if len(args) < 4:
            raise ValueError("add expects J-NNN <owner> #N <title>")
        job, owner, issue = args[:3]
        if not core.JOB.fullmatch(job) or not issue.startswith("#") or not issue[1:].isdigit():
            raise ValueError(f"add {job}: invalid job ID or issue number")
        if owner not in limits or job in p.by_job():
            raise ValueError(f"add {job}: unknown owner or existing job")
        ls.append(core.Line(job, owner, int(issue[1:]), (), "", " ".join(args[3:]), (), stamp(now)))
    elif action == "stamp":   # 11-13, once: each line with no time gets the one its own history gives
        if args:
            raise ValueError("stamp takes no job IDs; it reads each line's history")
        ls = [replace(ln, at=(history or {}).get(ln.job, "")) if not ln.at else ln for ln in ls]
    elif action in {"give", "hold", "release", "move", "drop", "tidy", "keep"}:
        if action == "tidy":
            if args:
                raise ValueError("tidy takes no job IDs; it checks remote issue closure")
            stale = [ln.job for ln in ls if ln.job in verified_closed]
            if any(in_flight(job, branches, prs, inactive, p.by_job()[job].kept) for job in stale):
                raise ValueError("tidy: closed issue still has unresolved remote work; reconcile it first")
            ls = [ln for ln in ls if ln.job not in verified_closed]
        else:
            if not args:
                raise ValueError(f"{action} requires a job")
            job = args[0]
            found = next((i for i, ln in enumerate(ls) if ln.job == job), None)
            if found is None:
                raise ValueError(f"{action} {job}: not found")
            if flipped and not again:
                raise ValueError(f"{action} {job}: recently changed; --again reason is required")
            if action in {"give", "hold"} and in_flight(job, branches, prs, inactive, ls[found].kept) and not stop_work:
                raise ValueError(f"{action} {job}: remote work in flight; --stop-work reason is required")
            old = ls[found]
            if action == "give":
                if len(args) != 2 or args[1] not in limits:
                    raise ValueError(f"give {job}: expected valid owner")
                ls[found] = replace(old, owner=args[1], at=stamp(now) if args[1] != old.owner else old.at)
            elif action == "hold":
                if len(args) < 2:
                    raise ValueError(f"hold {job}: explicit reason required")
                ls[found] = replace(old, hold=" ".join(args[1:]))
            elif action == "release":
                ls[found] = replace(old, hold="")
            elif action == "keep":
                # the ruling that clears a conflict: a head of the job stays as a record at this sha, not a lane
                m = core.KEPT.fullmatch("kept " + args[1]) if len(args) == 2 else None
                live = {core.branch_name(b): core.branch_sha(b) for b in branches}
                if not m or core.job_of_branch(m.group(1)) != job:
                    raise ValueError(f"keep {job}: expected <branch naming the job>@<sha>")
                if not live.get(m.group(1), "").startswith(m.group(2)):
                    raise ValueError(f"keep {job}: {m.group(1)} is not on the remote at {m.group(2)}")
                ls[found] = replace(old, kept=tuple(k for k in old.kept if k[0] != m.group(1)) + ((m.group(1), m.group(2)),))
            elif action == "move":
                if len(args) != 2 or args[1] not in p.by_job():
                    raise ValueError(f"move {job}: expected another planned job")
                target = args[1]
                if target == job:
                    raise ValueError("move cannot target itself")
                item = ls.pop(found)
                ls.insert(next(i for i, ln in enumerate(ls) if ln.job == target), item)
            elif action == "drop":
                if len(args) != 1 or job not in verified_closed:
                    raise ValueError(f"drop {job}: issue must be independently verified closed")
                if in_flight(job, branches, prs, inactive, old.kept):
                    raise ValueError(f"drop {job}: unresolved remote work remains")
                ls.pop(found)
    else:
        raise ValueError(f"unsupported change: {action}")
    if tuple(ls) == p.lines and limits == p.limits:
        return source  # a no-op must not reformat the plan or create a phantom version
    result = core.Plan(tuple(ls), limits, ())
    # The provisional warning and reconciliation evidence are part of the file. Editing
    # one order must not silently strip every preceding comment or its shadow-only warning.
    preamble = []
    for line in source.splitlines(keepends=True):
        if line.lstrip().startswith("#") or not line.strip():
            preamble.append(line)
        else:
            break
    normalized = "".join(preamble) + core.render(result)
    errors = core.parse(normalized).errors
    if errors:
        raise ValueError("result invalid: " + "; ".join(errors))
    return normalized


def assigned_times(path: Path) -> dict[str, str]:
    """job -> when its line got its present owner, from the plan file's commits (oldest first): the commit after
    which the job's owner never changed again. A job whose owner the history never shows is left out."""
    try:
        p = subprocess.run(["git", "-C", str(HERE), "log", "--reverse", "--format=COMMIT %cI", "-p", "--",
                            str(path.relative_to(HERE))], text=True, capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError, ValueError):
        return {}
    return owner_times(p.stdout)


def owner_times(log: str) -> dict[str, str]:
    """assigned_times from `git log --reverse --format='COMMIT %cI' -p` of the plan file."""
    owner: dict[str, str] = {}
    since: dict[str, str] = {}
    at = ""
    for raw in log.splitlines():
        if raw.startswith("COMMIT "):
            at = raw[7:].strip()
            continue
        m = core.ORDER.fullmatch(raw[1:].strip()) if raw.startswith("+") and not raw.startswith("+++") else None
        if m and at:
            job, who = m.group(1), m.group(2)
            if owner.get(job) != who:
                owner[job], since[job] = who, at
    out = {}
    for job, t in since.items():
        when = dt.datetime.fromisoformat(t)
        out[job] = stamp(when)
    return out


def write_atomic(path: Path, old: str, new: str) -> None:
    if path.read_text(encoding="utf-8") != old:
        raise RuntimeError(f"{path}: changed during editing; refusing overwrite")
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=".plan-", text=True)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(new)
            f.flush()
            os.fsync(f.fileno())
        if path.read_text(encoding="utf-8") != old:
            raise RuntimeError(f"{path}: changed during save; refusing overwrite")
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--plan", type=Path, default=HERE / "work/plan.md")
    ap.add_argument("--expect", help="required plan content fingerprint for mutations")
    ap.add_argument("--stop-work", default="", help="reason for stopping published in-flight work")
    ap.add_argument("--again", default="", help="reason for repeated reassignment or hold")
    ap.add_argument("--reason", default="", help="hold: every argument is a job, all held for this reason "
                                                 "(one remote read for them all)")
    ap.add_argument("action", choices=("status", "check", "add", "give", "hold", "release", "move", "limit", "drop",
                                       "tidy", "keep", "stamp"))
    ap.add_argument("args", nargs="*")
    ns = ap.parse_args(argv)
    try:
        src = ns.plan.read_text(encoding="utf-8")
        parsed = core.parse(src)
        if ns.action in {"status", "check"}:   # the job machine's table, from the same plan and reads as the switch
            import job_status
            return job_status.report(GitHub(REPO, dry_run=True, log=lambda line: print(line, file=sys.stderr)),
                                     content=src)
        needs_read = ns.action in {"give", "hold", "drop", "tidy", "keep"}
        # 11-13: several jobs at once (hold --reason, release) share one read; give, hold and keep read only what the
        # in-flight check of their own jobs needs (core.observe `only`)
        batch = (ns.action == "hold" and bool(ns.reason)) or (ns.action == "release" and len(ns.args) > 1)
        edits = [[j, *([ns.reason] if ns.action == "hold" else [])] for j in ns.args] if batch else [ns.args]
        gh = GitHub(REPO, dry_run=True, log=lambda line: print(line, file=sys.stderr)) if needs_read else None
        only = frozenset(e[0] for e in edits if e) if ns.action in {"give", "hold", "keep"} else None
        observed = core.observe(gh, parsed, dt.datetime.now(dt.timezone.utc),
                                lambda line: print(line, file=sys.stderr), only=only) if needs_read else None
        if not ns.expect or ns.expect != sha(src):
            raise ValueError(f"{ns.action}: --expect {sha(src)} required (stale plan must not overwrite)")
        job = ns.args[0] if ns.args else ""
        verified_closed = set()
        if ns.action in {"drop", "tidy"}:
            open_issue_numbers = {issue["number"] for issue in observed["issues"]}
            candidates = [ln for ln in parsed.lines if ln.issue not in open_issue_numbers]
            if ns.action == "drop":
                candidates = [ln for ln in candidates if ln.job == job]
            for ln in candidates:
                issue = gh.get(f"repos/{REPO}/issues/{ln.issue}")
                if (issue.get("state") == "closed" and
                    issue.get("title", "").startswith(ln.job + " ") and
                    not issue.get("pull_request")):
                    verified_closed.add(ln.job)
        result = src
        for args in edits:   # in turn, each on the last one's result: one refused leaves the file as it was
            job = args[0] if args else ""
            result = change(result, ns.action, args, verified_closed=verified_closed,
                            branches=observed["branches"] if observed else (),
                            prs=observed["prs"] if observed else (),
                            inactive=observed["inactive"] if observed else frozenset(),
                            stop_work=ns.stop_work, again=ns.again,
                            flipped=recent_flip(ns.plan, job) if ns.action in {"give", "hold", "release", "move"} else False,
                            history=assigned_times(ns.plan) if ns.action == "stamp" else None)
        write_atomic(ns.plan, src, result)
        print(f"PLAN EDIT {ns.action}: {sha(src)} -> {sha(result)}; review diff, commit through MCP")
        return 0
    except (OSError, RuntimeError, ValueError) as err:
        print(f"plan: {err}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
