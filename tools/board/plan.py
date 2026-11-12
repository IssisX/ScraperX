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
import plan as core  # noqa: E402
from ghapi import GitHub  # noqa: E402

REPO = "IssisX/ScraperX"


def sha(data: str) -> str:
    return hashlib.sha256(data.encode("utf-8")).hexdigest()[:12]


def recent_flip(path: Path, job: str) -> bool:
    """Worktree evidence of a recently edited job order, not a wall-clock guess."""
    try:
        p = subprocess.run(["git", "-C", str(HERE), "log", "--since=1.day", "--format=", "-p", "--",
                            str(path.relative_to(HERE))], text=True, capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError, ValueError):
        return True  # cannot prove the edit is safe
    return bool(re.search(rf"^[+-]{re.escape(job)}(?:\s|$)", p.stdout, re.M))


def in_flight(job: str, branches: list, prs: list[dict], inactive: set[str] | frozenset[str] = frozenset()) -> bool:
    """Remote work still needing disposition, including foreign owners and closed-unmerged PR heads."""
    if core.live_refs(job, branches, inactive):
        return True
    return any((m := core.PR_JOB.match(pr.get("body") or "")) and m.group(1) == job
               for pr in prs if pr.get("state") != "closed")


def change(source: str, action: str, args: list[str], *, branches: list = (),
           prs: list[dict] = (), inactive: set[str] | frozenset[str] = frozenset(),
           stop_work: str = "", again: str = "", flipped: bool = False) -> str:
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
        ls.append(core.Line(job, owner, int(issue[1:]), (), "", " ".join(args[3:])))
    elif action in {"give", "hold", "release", "move", "tidy"}:
        if action == "tidy":
            raise ValueError("tidy requires explicit verified closed-job input; refusing to guess")
        if not args:
            raise ValueError(f"{action} requires a job")
        job = args[0]
        found = next((i for i, ln in enumerate(ls) if ln.job == job), None)
        if found is None:
            raise ValueError(f"{action} {job}: not found")
        if flipped and not again:
            raise ValueError(f"{action} {job}: recently changed; --again reason is required")
        if action in {"give", "hold"} and in_flight(job, branches, prs, inactive) and not stop_work:
            raise ValueError(f"{action} {job}: remote work in flight; --stop-work reason is required")
        old = ls[found]
        if action == "give":
            if len(args) != 2 or args[1] not in limits:
                raise ValueError(f"give {job}: expected valid owner")
            ls[found] = replace(old, owner=args[1])
        elif action == "hold":
            if len(args) < 2:
                raise ValueError(f"hold {job}: explicit reason required")
            ls[found] = replace(old, hold=" ".join(args[1:]))
        elif action == "release":
            ls[found] = replace(old, hold="")
        elif action == "move":
            if len(args) != 2 or args[1] not in p.by_job():
                raise ValueError(f"move {job}: expected another planned job")
            target = args[1]
            if target == job:
                raise ValueError("move cannot target itself")
            item = ls.pop(found)
            ls.insert(next(i for i, ln in enumerate(ls) if ln.job == target), item)
    else:
        raise ValueError(f"unsupported change: {action}")
    result = core.Plan(tuple(ls), limits, ())
    normalized = core.render(result)
    errors = core.parse(normalized).errors
    if errors:
        raise ValueError("result invalid: " + "; ".join(errors))
    return normalized


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
    ap.add_argument("action", choices=("status", "check", "add", "give", "hold", "release", "move", "limit", "tidy"))
    ap.add_argument("args", nargs="*")
    ns = ap.parse_args(argv)
    try:
        src = ns.plan.read_text(encoding="utf-8")
        parsed = core.parse(src)
        needs_read = ns.action in {"status", "check", "give", "hold"}
        gh = GitHub(REPO, dry_run=True, log=lambda line: print(line, file=sys.stderr)) if needs_read else None
        observed = core.observe(gh, parsed, dt.datetime.now(dt.timezone.utc),
                                lambda line: print(line, file=sys.stderr)) if needs_read else None
        if ns.action in {"status", "check"}:
            issues, branches, prs = observed["issues"], observed["branches"], observed["prs"]
            params = {name: observed[name] for name in ("inactive", "merged", "dormant")}
            problems = core.findings(parsed, issues, branches, prs, **params)
            open_jobs = {ln.job for ln in parsed.lines if any(
                i.get("number") == ln.issue and i.get("state", "open") == "open" for i in issues)}
            closed = {ln.job for ln in parsed.lines if ln.job not in open_jobs}
            for line in parsed.lines:
                print(f"{line.job} {line.owner} #{line.issue}: "
                      f"{core.status(line, closed=closed, prs=prs, branches=branches, **params)}")
            for problem in problems:
                print("PLAN " + problem)
            print(f"PLAN {'BLOCKED' if problems else 'CLEAN'} {parsed.fingerprint()}")
            return 1 if problems else 0
        if not ns.expect or ns.expect != sha(src):
            raise ValueError(f"{ns.action}: --expect {sha(src)} required (stale plan must not overwrite)")
        job = ns.args[0] if ns.args else ""
        result = change(src, ns.action, ns.args,
                        branches=observed["branches"] if observed else (),
                        prs=observed["prs"] if observed else (),
                        inactive=observed["inactive"] if observed else frozenset(),
                        stop_work=ns.stop_work, again=ns.again,
                        flipped=recent_flip(ns.plan, job) if ns.action in {"give", "hold", "release", "move"} else False)
        write_atomic(ns.plan, src, result)
        print(f"PLAN EDIT {ns.action}: {sha(src)} -> {sha(result)}; review diff, commit through MCP")
        return 0
    except (OSError, RuntimeError, ValueError) as err:
        print(f"plan: {err}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
