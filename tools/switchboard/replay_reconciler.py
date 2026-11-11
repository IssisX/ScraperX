#!/usr/bin/env python3
"""The reconciler's cutover proof (J-072): replay real repo history through tools/switchboard/reconciler.py on a
fake clock, then check every obligation it opened against what really happened.

  fetch   reads the last WINDOW of history with read-only `gh api` GETs (comments, job issues and their label
          events, open and recently closed PRs with their reviews and files, proof runs and their jobs, the repo's
          push activity, where each pushed head landed) and saves it to a fixture (gzipped JSON).
  replay  drives the reconciler one pass per STEP of that window. Each pass sees only what existed at its time:
          the ledger's own reader (ledger.read_feed) runs unchanged against a fake client that answers each GET
          from the fixture as of that time, the PRs are steered by rules.merge_decision from the facts as of that
          time, and the reconciler writes to a LocalStore with the shadow outboxes, so nothing is posted.
          It then prints the table: every obligation, when it would have nagged, re-sent, been acked, closed or
          dead-lettered, what Cory's hourly batches would have said, and the check against the real history
          (false nags, misses, wrong holders, wrong due times). Exit 0 when the verdict is clean, 1 when not.

What the fixture cannot give back, and how the replay stands in for it (each is printed with the verdict):
  - mergeability as of a past time: taken as mergeable (no conflict is ever replayed);
  - the rule check: taken as no BLOCK (it runs only for a MERGE, which merges either way);
  - the automatic review's MAC: unsigned (a workflow-file PR reads HUMAN-MERGE, as with no review key);
  - a head's commit time: its push time from the activity feed;
  - comment edits: each comment as it reads now, at its creation time.

Usage (read-only against GitHub; the fixture and the state go where you say):
  python3 tools/switchboard/replay_reconciler.py fetch --out /tmp/replay.json.gz [--hours 48]
  python3 tools/switchboard/replay_reconciler.py replay --fixture /tmp/replay.json.gz [--state-dir DIR] [--md OUT]
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import gzip
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import types
import urllib.parse
from typing import Callable

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ledger as L  # noqa: E402
import reconciler as R  # noqa: E402
import rules  # noqa: E402
import asks  # noqa: E402

REPO = "IssisX/ScraperX"
BOARD = 340
OLD_BOARD = 107   # the board until #340 opened (11-09 8:03 AM Central; #107 closed at 100 comments)
PROOF = "wo000-delivery-spine.yml"
NOT_COUNTED = {"publish-checkpoint", "playtest-apk", "wake-switchboard"}   # as switch.py
WAKE_JOB = "wake-switchboard"
SYSTEM_BRANCHES = {"switchboard/ledger", "claude/integrator-inbox", R.STATE_BRANCH}
SYSTEM_PRS = {282}
STEP = dt.timedelta(minutes=10)
LOOKBACK = L.LIVE + dt.timedelta(hours=2)   # each pass reads asks and pushes this far back
MAX_BODY = 8000                             # comment text kept per comment (asks read 4000, commands a line)


# ---------- fetch (read-only) ----------
def gh_get(path: str):
    for attempt in range(3):   # a 5xx is retried; a 4xx is not
        out = subprocess.run(["gh", "api", path], capture_output=True, timeout=90)
        if out.returncode == 0:
            text = out.stdout.decode("utf-8")
            return json.loads(text) if text.strip() else None
        err = out.stderr.decode("utf-8", "replace")
        if "HTTP 5" not in err:
            break
    raise RuntimeError(f"gh api {path}: {err[:300]}")


def gh_pages(path: str, key: str = "", stop: Callable[[dict], bool] | None = None, limit: int = 40) -> list:
    sep = "&" if "?" in path else "?"
    items: list = []
    for page in range(1, limit + 1):
        got = gh_get(f"{path}{sep}per_page=100&page={page}") or ([] if not key else {key: []})
        batch = got[key] if key else got
        items += batch
        if len(batch) < 100 or (stop and batch and stop(batch[-1])):
            break
    return items


def activity_pages(since: dt.datetime, limit: int = 40) -> list:
    """The repo's push activity, newest first, back to `since` (cursor-paged, as ledger.cursor_pages)."""
    base = f"repos/{REPO}/activity?time_period=week&per_page=100"
    url, items = base, []
    for _ in range(limit):
        got = subprocess.run(["gh", "api", "-i", url], capture_output=True, timeout=90)
        if got.returncode != 0:
            raise RuntimeError(f"gh api {url}: {got.stderr.decode('utf-8', 'replace')[:200]}")
        text = got.stdout.decode("utf-8", "replace").replace("\r\n", "\n")
        head, _, data = text.partition("\n\n")
        batch = json.loads(data) if data.strip() else []
        items += batch
        if batch and (L.parse(batch[-1].get("timestamp")) or since) < since:
            break
        m = re.search(r'<[^>]*[?&]after=([^&>]+)[^>]*>;\s*rel="next"', head)
        if not m:
            break
        url = f"{base}&after={m.group(1)}"
    return items


def slim_user(u: dict | None) -> dict:
    return {"login": (u or {}).get("login") or ""}


def fetch(hours: float, out_path: pathlib.Path, log=print) -> dict:
    now = dt.datetime.now(dt.timezone.utc).replace(microsecond=0)
    start = now - dt.timedelta(hours=hours)
    since = start - LOOKBACK
    S = L.iso(since)
    log(f"fetch: window {L.iso(start)} .. {L.iso(now)}, reading back to {S}")

    comments = []
    for c in gh_pages(f"repos/{REPO}/issues/comments?since={S}&sort=created&direction=asc"):
        comments.append({"id": c["id"], "issue_url": c.get("issue_url") or "", "html_url": c.get("html_url") or "",
                         "created_at": c.get("created_at"), "updated_at": c.get("updated_at"),
                         "user": slim_user(c.get("user")), "body": (c.get("body") or "")[:MAX_BODY]})
    log(f"fetch: {len(comments)} comments")

    threads: dict[int, dict] = {}
    for it in gh_pages(f"repos/{REPO}/issues?state=all&since={S}") + gh_pages(f"repos/{REPO}/issues?state=open"):
        threads[it["number"]] = {
            "number": it["number"], "title": it.get("title") or "", "state": it.get("state"),
            "state_reason": it.get("state_reason") or "", "created_at": it.get("created_at"),
            "closed_at": it.get("closed_at"), "updated_at": it.get("updated_at"), "html_url": it.get("html_url"),
            "labels": [l["name"] for l in it.get("labels") or []], "pr": "pull_request" in it}
    log(f"fetch: {len(threads)} threads")

    events = []
    for e in gh_pages(f"repos/{REPO}/issues/events", stop=lambda e: (L.parse(e.get("created_at")) or now) < since):
        if (L.parse(e.get("created_at")) or now) < since:
            continue
        events.append({"event": e.get("event"), "created_at": e.get("created_at"), "actor": slim_user(e.get("actor")),
                       "label": (e.get("label") or {}).get("name") or "",
                       "issue": {"number": (e.get("issue") or {}).get("number")}})
    log(f"fetch: {len(events)} issue events")

    closed = gh_pages(f"repos/{REPO}/pulls?state=closed&sort=updated&direction=desc",
                      stop=lambda p: (L.parse(p.get("updated_at")) or now) < since - L.CLOSED_WINDOW)
    prs: dict[int, dict] = {}
    for p in gh_pages(f"repos/{REPO}/pulls?state=open") + closed:
        if p["number"] in prs:
            continue
        prs[p["number"]] = {
            "number": p["number"], "state": p.get("state"), "title": p.get("title") or "",
            "body": p.get("body") or "", "draft": bool(p.get("draft")), "created_at": p.get("created_at"),
            "closed_at": p.get("closed_at"), "merged_at": p.get("merged_at"), "updated_at": p.get("updated_at"),
            "html_url": p.get("html_url"), "labels": [l["name"] for l in p.get("labels") or []],
            "head": {"ref": p["head"]["ref"], "sha": p["head"]["sha"],
                     "repo": {"full_name": ((p["head"].get("repo") or {}).get("full_name") or "")}},
            "base": {"ref": p["base"]["ref"]}}
    detail = [n for n, p in prs.items() if n not in SYSTEM_PRS and (not p["closed_at"] or L.parse(p["closed_at"]) >= since)]
    for n in detail:
        p = prs[n]
        p["reviews"] = [{"body": r.get("body") or "", "submitted_at": r.get("submitted_at") or "",
                         "user": slim_user(r.get("user")), "commit_id": r.get("commit_id") or ""}
                        for r in gh_pages(f"repos/{REPO}/pulls/{n}/reviews")]
        p["workflows"] = any(f["filename"].startswith(".github/workflows/")
                             for f in gh_pages(f"repos/{REPO}/pulls/{n}/files", limit=3))
        p["commits"] = [{"sha": c["sha"], "date": c["commit"]["committer"]["date"]}
                        for c in gh_pages(f"repos/{REPO}/pulls/{n}/commits", limit=3)]
    log(f"fetch: {len(prs)} PRs ({len(detail)} with reviews, files and commits)")

    runs = []
    for r in gh_pages(f"repos/{REPO}/actions/workflows/{PROOF}/runs?created=%3E%3D{S}", key="workflow_runs"):
        run = {"id": r["id"], "html_url": r.get("html_url") or "", "head_branch": r.get("head_branch") or "",
               "head_sha": r.get("head_sha") or "", "created_at": r.get("created_at"),
               "updated_at": r.get("updated_at"), "status": r.get("status"), "conclusion": r.get("conclusion") or "",
               "jobs": None}
        if r.get("status") != "completed" or (r.get("conclusion") or "") not in ("success", "cancelled", "skipped"):
            jobs = (gh_get(f"repos/{REPO}/actions/runs/{r['id']}/jobs?per_page=50") or {}).get("jobs") or []
            run["jobs"] = [{"name": j["name"], "status": j.get("status"), "conclusion": j.get("conclusion") or "",
                            "completed_at": j.get("completed_at")} for j in jobs]
        runs.append(run)
    log(f"fetch: {len(runs)} proof runs ({sum(1 for r in runs if r['jobs'] is not None)} judged by their jobs)")

    acts = [{"ref": a.get("ref") or "", "before": a.get("before") or "", "after": a.get("after") or "",
             "timestamp": a.get("timestamp"), "activity_type": a.get("activity_type") or ""}
            for a in activity_pages(since)]
    log(f"fetch: {len(acts)} activity entries, oldest {min((a['timestamp'] for a in acts), default='-')}")

    # where each pushed head went: the PRs that contain it (merged ones landed it), else the trunk compare
    landed: dict[str, dict] = {}
    commits: dict[str, str] = {}
    heads = sorted({a["after"] for a in acts if a["ref"].startswith("refs/heads/") and a["after"]
                    and a["ref"] != f"refs/heads/{rules.TRUNK}" and a["activity_type"] != "branch_deletion"
                    and not a["ref"][len("refs/heads/"):] in SYSTEM_BRANCHES})
    for sha in heads:
        got = gh_get(f"repos/{REPO}/commits/{sha}/pulls") or []
        merged = sorted(p["merged_at"] for p in got if p.get("merged_at") and (p.get("base") or {}).get("ref") == rules.TRUNK)
        rec = {"merged_at": merged[0] if merged else None, "prs": [p["number"] for p in got]}
        if not merged:
            try:
                cmp = gh_get(f"repos/{REPO}/compare/{rules.TRUNK}...{sha}?per_page=1") or {}
                rec["status"], rec["ahead_by"] = cmp.get("status") or "", int(cmp.get("ahead_by") or 0)
            except RuntimeError:
                rec["status"], rec["ahead_by"] = "", 0
        landed[sha] = rec
    for a in acts:
        name = a["ref"][len("refs/heads/"):]
        if a["after"] and not L.job_of_branch(name) and name not in SYSTEM_BRANCHES and name != rules.TRUNK \
                and not name.startswith(L.SYSTEM_PREFIXES) and a["after"] not in commits:
            try:
                commits[a["after"]] = ((gh_get(f"repos/{REPO}/commits/{a['after']}") or {}).get("commit") or {}).get("message", "")
            except RuntimeError:
                commits[a["after"]] = ""
    log(f"fetch: {len(landed)} pushed heads traced, {len(commits)} head commit messages")

    data = {"fetched_at": L.iso(now), "start": L.iso(start), "end": L.iso(now), "since": S, "repo": REPO,
            "comments": comments, "threads": list(threads.values()), "events": events, "prs": list(prs.values()),
            "runs": runs, "activity": acts, "landed": landed, "commits": commits}
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with gzip.open(out_path, "wt", encoding="utf-8") as f:
        json.dump(data, f, sort_keys=True)
    log(f"fetch: saved {out_path} ({out_path.stat().st_size // 1024} KB)")
    return data


def load(path: pathlib.Path) -> dict:
    opener = gzip.open if str(path).endswith(".gz") else open
    with opener(path, "rt", encoding="utf-8") as f:
        return json.load(f)


# ---------- history as of a time ----------
def T(stamp) -> dt.datetime | None:
    return L.parse(stamp) if isinstance(stamp, str) else stamp


def thread_of(c: dict) -> int:
    try:
        return int((c.get("issue_url") or "").rsplit("/", 1)[1])
    except (ValueError, IndexError):
        return 0


class History:
    """The fixture, indexed, answering "what did GitHub say at time t"."""

    def __init__(self, data: dict, team: rules.Team):
        self.data, self.team = data, team
        self.start, self.end = T(data["start"]), T(data["end"])
        self.since = T(data["since"])
        self.comments = sorted(data["comments"], key=lambda c: (c["created_at"], c["id"]))
        for c in self.comments:
            c["_n"], c["_t"] = thread_of(c), T(c["created_at"])
            # the Codex app edits one state comment per PR as it reviews each head: as of now it shows the last
            c["_vis"] = T(c["updated_at"]) if rules.codex_review_state(c["body"], c["user"]["login"]) else c["_t"]
        self.threads = {t["number"]: t for t in data["threads"]}
        self.prs = {p["number"]: p for p in data["prs"]}
        for p in self.prs.values():   # a PR the thread listing missed is still a thread
            self.threads.setdefault(p["number"], {"number": p["number"], "title": p["title"], "state": p["state"],
                                                  "state_reason": "", "created_at": p["created_at"],
                                                  "closed_at": p["closed_at"], "updated_at": p["updated_at"],
                                                  "html_url": p["html_url"], "labels": p["labels"], "pr": True})
        self.events = sorted(data["events"], key=lambda e: e["created_at"])
        self.ev_by: dict[int, list] = {}
        for e in self.events:
            self.ev_by.setdefault((e.get("issue") or {}).get("number") or 0, []).append(e)
        self.acts = sorted(data["activity"], key=lambda a: a["timestamp"])
        self.act_by: dict[str, list] = {}
        for a in self.acts:
            if a["ref"].startswith("refs/heads/"):
                self.act_by.setdefault(a["ref"][len("refs/heads/"):], []).append(a)
        self.runs = sorted(data["runs"], key=lambda r: r["created_at"])
        self.runs_by: dict[str, list] = {}
        for r in self.runs:
            self.runs_by.setdefault(r["head_sha"], []).append(r)
        self.landed = data["landed"]
        self.commits = data["commits"]
        self.pushed_at: dict[str, dt.datetime] = {}
        for a in self.acts:
            if a["after"] and a["activity_type"] != "branch_deletion":
                self.pushed_at.setdefault(a["after"], T(a["timestamp"]))
        for p in self.prs.values():
            for c in p.get("commits") or []:
                self.pushed_at.setdefault(c["sha"], T(c["date"]))
        trunk = self.act_by.get(rules.TRUNK, [])
        self.trunk_pushes = [T(a["timestamp"]) for a in trunk if a["activity_type"] != "branch_deletion"]
        self._touch: dict[int, list] = {}
        for c in self.comments:
            if c["user"]["login"] != rules.BOT_LOGIN:
                self._touch.setdefault(c["_n"], []).append(c["_t"])
        for e in self.events:
            if (e.get("actor") or {}).get("login") != rules.BOT_LOGIN:
                self._touch.setdefault((e.get("issue") or {}).get("number") or 0, []).append(T(e["created_at"]))

    def board_at(self, t: dt.datetime) -> int:
        """The live board at t: #340 from its opening, #107 before (switch.BOARD_ISSUE moved on 11-09)."""
        new = self.threads.get(BOARD)
        return BOARD if new is None or T(new["created_at"]) <= t else OLD_BOARD

    # threads
    def open_at(self, n: int, t: dt.datetime) -> bool:
        th = self.threads.get(n)
        if th is None or T(th["created_at"]) > t:
            return False
        last = None
        for e in self.ev_by.get(n, []):
            if e["event"] in ("closed", "reopened") and T(e["created_at"]) <= t:
                last = e["event"]
        if last:
            return last == "reopened"
        closed = T(th.get("closed_at"))
        if closed and closed > t and any(e["event"] == "closed" for e in self.ev_by.get(n, [])):
            return True
        return not (closed and closed <= t)

    def labels_at(self, n: int, t: dt.datetime) -> list[str]:
        labels = list((self.threads.get(n) or {}).get("labels") or [])
        for e in reversed(self.ev_by.get(n, [])):   # undo every label change after t, newest first
            if T(e["created_at"]) <= t or not e.get("label"):
                continue
            if e["event"] == "labeled" and e["label"] in labels:
                labels.remove(e["label"])
            elif e["event"] == "unlabeled" and e["label"] not in labels:
                labels.append(e["label"])
        return labels

    def touched(self, n: int, t: dt.datetime) -> str:
        th = self.threads[n]
        times = [T(th["created_at"])] + [x for x in self._touch.get(n, []) if x <= t]
        return L.iso(max(times))

    def comments_at(self, t: dt.datetime, n: int | None = None, since: dt.datetime | None = None) -> list[dict]:
        return [c for c in self.comments if c["_vis"] <= t and (n is None or c["_n"] == n)
                and (since is None or c["_t"] >= since)]

    # heads
    def head_at(self, ref: str, t: dt.datetime) -> tuple[str, dt.datetime | None] | None:
        """(sha, pushed at) of `ref` at t; ("", None) when the feed says nothing; None when it is not there."""
        acts = self.act_by.get(ref, [])
        done = [a for a in acts if T(a["timestamp"]) <= t]
        if done:
            a = done[-1]
            if a["activity_type"] == "branch_deletion":
                return None
            return a["after"], T(a["timestamp"])
        later = [a for a in acts if T(a["timestamp"]) > t]
        if later and later[0]["activity_type"] == "branch_creation":
            return None   # not created yet
        if later and later[0]["before"] and not set(later[0]["before"]) <= {"0"}:
            sha = later[0]["before"]
            return sha, self.pushed_at.get(sha)
        return "", None

    def pr_head(self, p: dict, t: dt.datetime) -> tuple[str, dt.datetime]:
        commits = p.get("commits") or []
        got = self.head_at(p["head"]["ref"], t)
        if got and got[0] and (not commits or got[0] in {c["sha"] for c in commits}):
            return got[0], got[1] or self.pushed_at.get(got[0]) or T(p["created_at"])
        done = [c for c in commits if T(c["date"]) <= t]   # the newest of the PR's own commits by t
        if done:
            return done[-1]["sha"], self.pushed_at.get(done[-1]["sha"]) or T(done[-1]["date"])
        return p["head"]["sha"], self.pushed_at.get(p["head"]["sha"]) or T(p["created_at"])

    def landed_at(self, sha: str) -> dt.datetime | None:
        rec = self.landed.get(sha) or {}
        if rec.get("merged_at"):
            return T(rec["merged_at"])
        if rec.get("status") in ("behind", "identical"):   # pushed to trunk directly: the first trunk push after
            pushed = self.pushed_at.get(sha)
            after = [x for x in self.trunk_pushes if not pushed or x >= pushed]
            return after[0] if after else self.start
        return None

    def landed_by(self, sha: str, t: dt.datetime) -> bool:
        when = self.landed_at(sha)
        return bool(when) and when <= t

    # proof runs
    def run_at(self, r: dict, t: dt.datetime) -> dict | None:
        """The run as GitHub listed it at t (status, conclusion), with its judged state and finish time."""
        if T(r["created_at"]) > t:
            return None
        jobs = r.get("jobs")
        failed: list[str] = []
        if jobs is None:   # a success, cancelled or skipped run: done at its last update
            done = T(r["updated_at"])
            state = "green" if r["conclusion"] == "success" else r["conclusion"]
        else:
            counted = [j for j in jobs if j["name"] != WAKE_JOB]
            ends = [T(j["completed_at"]) for j in counted if j.get("completed_at")]
            done = max(ends) if ends and len(ends) == len(counted) else None
            if r["status"] == "completed" and done is None:
                done = T(r["updated_at"])
            failed = [j["name"] for j in jobs if j["name"] not in NOT_COUNTED
                      and j["conclusion"] not in ("success", "skipped")]
            state = ("red: " + ", ".join(failed)) if failed else "green"
        if done is None or done > t:
            return dict(r, status="in_progress", conclusion="", updated_at=L.iso(t), _state="running", _done="")
        conclusion = r["conclusion"] if r["status"] == "completed" and T(r["updated_at"]) <= t else (
            "failure" if failed else "success")
        return dict(r, status="completed", conclusion=conclusion, _state=state, _done=L.iso(done))

    def proof_of(self, sha: str, t: dt.datetime) -> tuple[str, str]:
        runs = [x for x in (self.run_at(r, t) for r in self.runs_by.get(sha, [])) if x]
        if not runs:
            return "none", ""
        r = max(runs, key=lambda x: x["created_at"])
        return r["_state"], r["_done"]


# ---------- the fake GitHub that ledger.read_feed reads, as of a time ----------
class AsOfGH:
    """The GETs ledger.read_feed makes, answered from the history as of `t`. Writes are refused."""

    def __init__(self, h: History, t: dt.datetime, prs_open: list[dict]):
        self.h, self.t, self.repo, self.dry_run = h, t, REPO, True
        self.prs_open = prs_open

    def paged(self, path: str, key: str = "") -> list:
        got = self.get(f"{path}{'&' if '?' in path else '?'}per_page=100&page=1")
        return got[key] if key else got

    def write(self, *a, **kw):
        raise RuntimeError("the replay writes nothing")

    comment = write

    def get(self, path: str, accept: str = ""):
        h, t = self.h, self.t
        u = urllib.parse.urlsplit(path)
        q = dict(urllib.parse.parse_qsl(u.query))
        route = u.path[len(f"repos/{REPO}/"):]
        if q.get("page", "1") != "1":   # everything comes on page 1
            return {"workflow_runs": []} if route.endswith("/runs") else []
        if route == "issues/comments":
            return h.comments_at(t, since=T(q.get("since")) or h.since)
        if route == "issues/events":
            return list(reversed([e for e in h.events if T(e["created_at"]) <= t]))
        m = re.fullmatch(r"issues/(\d+)/comments", route)
        if m:
            return h.comments_at(t, int(m.group(1)))
        if route == "pulls" and q.get("state") == "open":
            return self.prs_open
        if route == "pulls" and q.get("state") == "closed":
            out = []
            for p in h.prs.values():
                c = T(p["closed_at"])
                if c and c <= t:
                    out.append(dict(p, updated_at=p["closed_at"]))
            return sorted(out, key=lambda p: p["updated_at"], reverse=True)
        m = re.fullmatch(r"commits/([0-9a-f]{40})", route)
        if m:
            return {"commit": {"message": h.commits.get(m.group(1), "")}}
        m = re.fullmatch(re.escape(f"compare/{rules.TRUNK}...") + r"([0-9a-f]{40})", route)
        if m:
            sha = m.group(1)
            if h.landed_by(sha, t):
                return {"status": "behind", "ahead_by": 0, "commits": []}
            n = int((h.landed.get(sha) or {}).get("ahead_by") or 1)
            return {"status": "ahead", "ahead_by": n, "commits": [{"sha": sha}]}
        if route == f"actions/workflows/{PROOF}/runs":
            since = T(q.get("created", "").lstrip(">=")) or h.since
            return {"workflow_runs": [x for x in (h.run_at(r, t) for r in h.runs if T(r["created_at"]) >= since) if x]}
        if route == "issues":
            out = []
            for n, th in h.threads.items():
                if not h.open_at(n, t):
                    continue
                labels = h.labels_at(n, t)
                if q.get("labels") == "job" and (th["pr"] or "job" not in labels):
                    continue
                item = {"number": n, "title": th["title"], "html_url": th["html_url"],
                        "labels": [{"name": x} for x in labels], "created_at": th["created_at"],
                        "updated_at": h.touched(n, t)}
                if th["pr"]:
                    item["pull_request"] = {}
                out.append(item)
            return out
        if re.fullmatch(r"issues/(\d+)", route):
            return {"body": ""}
        raise KeyError(f"the replay has no answer for GET {path}")


class AsOfLookups(R.Lookups):
    """The reconciler's point reads, from the history as of the pass's time. No history: an error (read fails)."""

    def __init__(self, h: History, t: dt.datetime):
        self.h, self.t = h, t

    def landed(self, branch: str, sha: str) -> str:
        if self.h.landed_by(sha, self.t):
            return "in trunk"
        for p in self.h.prs.values():
            c = T(p["closed_at"])
            if p["head"]["ref"] == branch and p["head"]["sha"] == sha and c and c <= self.t:
                return f"merged #{p['number']}" if p["merged_at"] else f"closed #{p['number']}"
        return ""

    def branch_sha(self, name: str) -> str | None:
        got = self.h.head_at(name, self.t)
        if got is None:
            return None
        if not got[0]:
            raise LookupError(f"no history for {name}")
        return got[0]

    def pr(self, n: int) -> dict:
        p = self.h.prs.get(n)
        if p is None:
            raise LookupError(f"no history for PR #{n}")
        c = T(p["closed_at"])
        closed = bool(c and c <= self.t)
        return {"merged": bool(closed and p["merged_at"]), "state": "closed" if closed else "open"}

    def issue(self, n: int) -> dict:
        th = self.h.threads.get(n)
        if th is None:
            raise LookupError(f"no history for #{n}")
        if self.h.open_at(n, self.t):
            return {"state": "open", "state_reason": ""}
        return {"state": "closed", "state_reason": th.get("state_reason") or ""}


def verdicts_at(h: History, team: rules.Team, p: dict, t: dt.datetime, off_pr: list[dict]) -> list:
    """switch.verdicts plus switch.bridged_verdicts, as of t."""
    out = []
    for c in h.comments_at(t, p["number"]):
        v = rules.parse_verdict(c["body"], c["created_at"], c["user"]["login"], team) or \
            rules.parse_codex_state(c["body"], c.get("updated_at") or c["created_at"], c["user"]["login"])
        if v:
            out.append(v)
    for r in p.get("reviews") or []:
        if r["submitted_at"] and T(r["submitted_at"]) <= t:
            v = rules.parse_verdict(r["body"], r["submitted_at"], r["user"]["login"], team)
            if v:
                out.append(v)
    names = re.compile(rf"\breview of #{p['number']}\b", re.IGNORECASE)
    for c in off_pr:
        body = c["body"].lstrip()
        if body.startswith(BRIDGE_POST) and names.search(body.split("\n", 1)[0]):
            v = rules.parse_verdict(body, c["created_at"], c["user"]["login"], team)
            if v:
                out.append(v)
    return out


BRIDGE_POST = "**Muse (via email bridge)"   # as switch.py


def steer_at(h: History, team: rules.Team, t: dt.datetime) -> tuple[list, list[dict], dict]:
    """The sweep's reading at t: (Steered-like PRs, the open-PR listing, PR number -> (decision, facts))."""
    jobs: dict[str, int] = {}
    for n, th in sorted(h.threads.items()):
        job = rules.job_of_title(th["title"])
        if job and not th["pr"] and h.open_at(n, t) and "job" in h.labels_at(n, t):
            jobs.setdefault(job, n)
    board = h.comments_at(t, h.board_at(t))
    steered, listing, decided = [], [], {}
    for n, p in sorted(h.prs.items()):
        if not h.open_at(n, t):
            continue
        if n in SYSTEM_PRS:
            listing.append(p)
            continue
        sha, pushed = h.pr_head(p, t)
        listing.append(dict(p, head=dict(p["head"], sha=sha)))
        if "reviews" not in p:
            continue   # closed before the read window
        proof, done = h.proof_of(sha, t)
        job = rules.job_of_pr(p["body"])
        issue = jobs.get(job)
        off = board + (h.comments_at(t, issue) if issue else [])
        labels = h.labels_at(n, t) + (h.labels_at(issue, t) if issue else [])
        draft = p["draft"] or any(e["event"] == "ready_for_review" and T(e["created_at"]) > t
                                  for e in h.ev_by.get(n, []))
        facts = rules.PrFacts(number=n, head_sha=sha, head_ref=p["head"]["ref"], base_ref=p["base"]["ref"],
                              draft=draft, mergeable=True, head_committed_at=L.iso(pushed), proof=proof,
                              verdicts=verdicts_at(h, team, p, t, off),
                              needs=frozenset(x.split(":", 1)[1] for x in labels if x.startswith("needs:")),
                              rule_check_block=False, touches_workflows=bool(p.get("workflows")))
        d = rules.merge_decision(facts, team.vendor_of_branch(facts.head_ref), team, True, True)
        ctx = {"job": job, "comments": h.comments_at(t, n), "body": p["body"], "proof_done_at": done}
        steered.append(types.SimpleNamespace(facts=facts, ctx=ctx, decision=d))
        decided[n] = (d, facts)
    return steered, listing, decided


def pass_at(h: History, roster: L.Roster, cfg: dict, t: dt.datetime) -> tuple[R.Pass, dict]:
    """What one switchboard pass at t would have read: the ledger's reader, run unchanged against the history."""
    steered, listing, decided = steer_at(h, h.team, t)
    gh = AsOfGH(h, t, listing)

    def run_state(run: dict):
        return run.get("_state") or "running", run.get("html_url") or "", run["id"], run.get("_done") or ""

    def cursor_pages(repo, path, stop=None, limit=L.MAX_PAGES):   # the push activity as of t, newest first
        return [a for a in reversed(h.acts) if T(a["timestamp"]) <= t], False

    shim = types.SimpleNamespace(gh=gh, team=h.team, run_state=run_state)
    saved = L.cursor_pages
    L.cursor_pages = cursor_pages
    try:
        feed, sent = L.read_feed(shim, steered, t, h.board_at(t), PROOF, SYSTEM_BRANCHES, SYSTEM_PRS)
    finally:
        L.cursor_pages = saved
    led = L.classify(feed, t, roster)
    comments = [R.Comment(str(c["id"]), c["_n"], c["_t"], c["user"]["login"], c["body"])
                for c in h.comments_at(t, since=t - L.LIVE)]
    trusted = frozenset(cfg.get("verdict_logins") or ["IssisX"]) - {rules.BOT_LOGIN}
    open_threads = frozenset(int(i["number"]) for i in gh.paged(f"repos/{REPO}/issues?state=open"))   # as read_pass
    p = R.Pass(feed, led, comments, set(sent), trusted)
    p.open_threads = open_threads   # set after: the same replay runs against an older reconciler for comparison
    return p, decided


def replay(h: History, cfg: dict, state_dir: pathlib.Path, step: dt.timedelta = STEP, log=print) -> list[dict]:
    """One reconciler pass per `step` over the window, into a LocalStore with the shadow outboxes."""
    roster = L.Roster(cfg)
    store = R.LocalStore(state_dir)
    t, passes = h.start, []
    while t <= h.end:
        p, decided = pass_at(h, roster, cfg, t)
        res = R.run_pass(store, p, roster, AsOfLookups(h, t), t, R.ShadowOutbox(), out=lambda *_: None,
                         cory=R.ShadowCory())
        passes.append({"t": t, "status": res.status, "degraded": res.degraded,
                       "asks": [(a.cid, a.thread, a.target) for a in p.feed.asks],
                       "prs": {n: (d.action, d.reasons, f.head_sha, f.proof) for n, (d, f) in decided.items()},
                       "branches": {b.name: b.sha for b in p.feed.branches},
                       "items": [(it.kind, it.ref, it.holder.agent, it.state) for it in p.led.items]})
        t += step
    log(f"replay: {len(passes)} passes, {R.central(h.start)} to {R.central(h.end)} Central")
    return passes


# ---------- the check against what really happened ----------
# The answer oracle is the tight rule of PR #360 (tools/board/asks.py): a later post by the addressee that names
# the ask (its id, URL or a quote), or is addressed back to the asker on its thread, or on another thread naming
# it; or the addressee's verdict on a PR the ask names. Written out here so the check does not lean on the rule
# the reconciler itself uses.
QUOTE = re.compile(r"^>\s?(.+)$", re.MULTILINE)


def addressees(body: str) -> set[str]:
    text = body.lstrip()
    out = {m.group(1) for m in asks.ASK.finditer(body[:4000])}
    if text.startswith("CLAUDE_TO_MUSE:"):
        out.add("Muse")
    if text.startswith(BRIDGE_POST) or "MUSE_TO_CLAUDE:" in text.split("\n", 1)[0]:
        out.add("Claude")
    return out


def poster(body: str) -> str:
    return "Claude" if body.lstrip().startswith("CLAUDE_TO_MUSE:") else asks.author(body)


def names_ask(body: str, ask: dict) -> bool:
    body = body[:4000]
    cid = str(ask["id"])
    if re.search(rf"(?<!\d){re.escape(cid)}(?!\d)", body) or (ask.get("html_url") and ask["html_url"] in body):
        return True
    flat = " ".join(ask["body"].split())
    return any(len(q := " ".join(m.group(1).split())) >= 12 and q in flat for m in QUOTE.finditer(body))


def real_answer(h: History, ask: dict, target: str) -> dict | None:
    """The first post that really answers `ask` for `target` (the oracle), or None."""
    asker = poster(ask["body"])
    refs = {int(r) for r in asks.PR_REF.findall(ask["body"][:600])} | {ask["_n"]}
    for d in h.comments:
        if d["_t"] <= ask["_t"] or d["user"]["login"] == rules.BOT_LOGIN:
            continue
        verdicts = {(m.group(1), int(m.group(2))) for m in asks.VERDICT.finditer(d["body"][:400])}
        if any((target, r) in verdicts for r in refs):
            return d
        if poster(d["body"]) != target:
            continue
        if names_ask(d["body"], ask):
            return d
        back = asker in addressees(d["body"])
        if back and (d["_n"] == ask["_n"] or re.search(rf"#{ask['_n']}\b", d["body"][:2000])):
            return d
    return None


def integrator_runs_between(a: dt.datetime, b: dt.datetime) -> list[dt.datetime]:
    """Every integrator run from a to b, listed out (weekdays 8:17 AM-11:17 PM Central, Sat-Sun 9:17 AM-9:17 PM)."""
    out, day = [], a.astimezone(R.CENTRAL).date() - dt.timedelta(days=1)
    while day <= b.astimezone(R.CENTRAL).date() + dt.timedelta(days=1):
        hours = range(9, 22) if day.weekday() >= 5 else range(8, 24)
        out += [dt.datetime(day.year, day.month, day.day, hh, 17, tzinfo=R.CENTRAL).astimezone(dt.timezone.utc)
                for hh in hours]
        day += dt.timedelta(days=1)
    return sorted(x for x in out if a - dt.timedelta(days=1) <= x <= b + dt.timedelta(days=3))


def expected_due(to: str, kind: str, created: dt.datetime) -> tuple[str, dt.datetime]:
    """The due time each channel should give, worked out independently of reconciler.due_at: Muse's mail 4 h;
    Claude's reviews (the API) 30 min; anything else of Claude's at the integrator's first hourly run at or after
    its next run + 1 h; Codex (relayed by the integrator) that + 30 min; Grok its next 10 AM or 4 PM Central run
    + 1 h; ChatGPT its next :27 run + 1 h; Grok Bot (reads all the time, PR #362) 1 h."""
    if to == "claude" and kind == "review":
        return "api", created + dt.timedelta(minutes=30)
    if to in ("claude", "codex"):
        runs = integrator_runs_between(created, created + dt.timedelta(days=3))
        nxt = next(r for r in runs if r > created)
        due = next(r for r in runs if r >= nxt + dt.timedelta(hours=1))
        return ("session", due) if to == "claude" else ("relay", due + dt.timedelta(minutes=30))
    if to == "muse":
        return "email", created + dt.timedelta(hours=4)
    if to == "grok_bot":
        return "watch", created + dt.timedelta(hours=1)
    local = created.astimezone(R.CENTRAL)
    if to == "grok":
        slots = [(10, 0), (16, 0)]
    elif to == "chatgpt":
        slots = [(hh, 27) for hh in range(24)]
    else:
        return "cory", created + dt.timedelta(minutes=15)
    for day in range(3):
        d = local.date() + dt.timedelta(days=day)
        for hh, mm in slots:
            c = dt.datetime(d.year, d.month, d.day, hh, mm, tzinfo=R.CENTRAL)
            if c > local:
                return "issue", c.astimezone(dt.timezone.utc) + dt.timedelta(hours=1)
    raise AssertionError("no run in three days")


def owed_at(h: History, ob: dict, t: dt.datetime, ask_cache: dict) -> tuple[bool, str]:
    """Was this obligation really still owed at t, by the history alone? (owed, why not)."""
    kind, version, ref = ob["kind"], ob["version"], ob["ref"]
    tag, _, sha = version.rpartition(":")
    refkind, name = ref.split("|", 1)
    if kind == "ask":
        key = (version, ob["to"])
        if key not in ask_cache:
            ask = next((c for c in h.comments if str(c["id"]) == version), None)
            target = L.Roster(CFG).address(ob["to"]) if ob["to"] in L.Roster(CFG).agents else ob["to"].capitalize()
            if ob["to"] == "grok_bot":
                hit = [x for x in ("Builder", "PE") if ask and re.search(rf"\b{x}\b", ask["body"][:4000])]
                target = hit[0] if hit else "Builder"
            ask_cache[key] = (ask, real_answer(h, ask, target) if ask else None)
        ask, ans = ask_cache[key]
        if ask is None:
            return True, ""
        if ans is not None and ans["_t"] <= t:
            return False, f"answered {R.central(ans['_t'])} ({ans['html_url'].rsplit('#', 1)[-1]})"
        if not h.open_at(ask["_n"], t):
            return False, f"#{ask['_n']} closed"
        return True, ""
    if refkind == "PR":
        n = int(name.lstrip("#"))
        p = h.prs.get(n)
        if p is None:
            return True, ""
        if not h.open_at(n, t):
            return False, f"#{n} {'merged' if p['merged_at'] and T(p['merged_at']) <= t else 'closed'}"
        head, _ = h.pr_head(p, t)
        want = sha if tag else version
        if head != want:
            return False, f"#{n} head moved to {head[:8]}"
        proof, _ = h.proof_of(head, t)
        if tag == "red" and not proof.startswith("red"):
            return False, f"#{n} proof {proof}"
        if tag in ("land", "eye"):
            vs = verdicts_at(h, h.team, p, t, h.comments_at(t, h.board_at(t)))
            author = h.team.vendor_of_branch(p["head"]["ref"])
            if tag == "land" and any(v.kind == "review" and v.verdict in ("LAND", "FIX") and rules.names_head(v, head)
                                     and rules.land_counts(v, author, h.team) for v in vs):
                return False, f"#{n} has a LAND or FIX that counts on the head"
            if tag == "eye" and any(v.kind == "eye" and v.verdict == "LAND" and rules.names_head(v, head)
                                    and v.author_login == rules.BOT_LOGIN for v in vs):
                return False, f"#{n} has the eye's LAND on the head"
            return True, ""
        if kind == "review":
            vs = verdicts_at(h, h.team, p, t, [])
            # as rules.needs_review: only a LAND or a FIX answers a head; a NOTE lands nothing (J-072, #376)
            if any(v.kind == "review" and v.verdict in ("LAND", "FIX") and rules.names_head(v, head) for v in vs):
                return False, f"#{n} has a LAND or a FIX on the head"
        if kind == "ci" and proof != "running":
            return False, f"#{n} proof {proof}"
        return True, ""
    if refkind == "BRANCH":
        got = h.head_at(name, t)
        if got is None:
            return False, "branch deleted"
        if got[0] and got[0] != sha:
            return False, f"head moved to {got[0][:8]}"
        if h.landed_by(sha, t):
            return False, "landed in trunk"
        if tag in ("open-pr", "red"):
            for p in h.prs.values():
                if p["head"]["ref"] == name and T(p["created_at"]) <= t and h.open_at(p["number"], t):
                    return False, f"PR #{p['number']} open"
        if tag == "red" and not h.proof_of(sha, t)[0].startswith("red"):
            return False, f"proof {h.proof_of(sha, t)[0]}"
        return True, ""
    if refkind == "JOB":
        n = int(name.lstrip("#"))
        if not h.open_at(n, t):
            return False, f"#{n} closed"
        if tag == "assign" and any(x.startswith("owner:") for x in h.labels_at(n, t)):
            return False, "owner labelled"
        if tag == "start":
            job = rules.job_of_title(h.threads[n]["title"])
            for ref_, acts in h.act_by.items():
                if L.job_of_branch(ref_) == job and any(T(a["timestamp"]) <= t and T(a["timestamp"]) >= t - L.LIVE
                                                         for a in acts):
                    return False, f"work pushed ({ref_})"
        return True, ""
    return True, ""


CFG: dict = {}


@dataclasses.dataclass
class Row:
    job: str
    ob: dict
    opened: dt.datetime
    sends: list
    acked: tuple | None
    closed: tuple | None
    dead: dt.datetime | None
    due_exp: tuple[str, dt.datetime]
    flags: list


def check(h: History, passes: list[dict], state_dir: pathlib.Path, cfg: dict) -> dict:
    """Every obligation against the history: false nags, false acks, misses, wrong holders, wrong due times."""
    global CFG
    CFG = cfg
    roster = L.Roster(cfg)
    month = h.end.strftime("%Y-%m")
    evs = R.read_events((state_dir / R.EVENTS_DIR / f"{month}.ndjson").read_text(encoding="utf-8"))
    idx = json.loads((state_dir / R.INDEX).read_text(encoding="utf-8"))["jobs"]
    threads = json.loads((state_dir / R.THREADS).read_text(encoding="utf-8"))
    cory = json.loads((state_dir / R.CORY_FILE).read_text(encoding="utf-8"))
    by_id: dict[str, list] = {}
    for e in evs:
        if e.get("id"):
            by_id.setdefault(e["id"], []).append(e)
    rows: list[Row] = []
    cache: dict = {}
    for job, rec in sorted(list(idx.items()) + [("", threads)], key=lambda kv: kv[0]):
        for ob in rec["obligations"]:
            es = by_id.get(ob["id"], [])
            opened = T(next(e["t"] for e in es if e["ev"] == "obligation"))
            sends = [(T(e["t"]), e["send"]) for e in es if e["ev"] == "would_send"]
            ack = next(((T(e["t"]), e["how"]) for e in es if e["ev"] == "acked"), None)
            closed = next(((T(e["t"]), e["why"]) for e in es if e["ev"] == "closed"), None)
            dead = next((T(e["t"]) for e in es if e["ev"] == "dead_letter"), None)
            flags = []
            # the holder: an ask goes to the agent it names; work goes to its owner (the job's label, else the
            # branch prefix), a review to its reviewer, a ruling to the integrator; Muse's PRs to the integrator
            if ob["kind"] == "ask":
                ask = next((c for c in h.comments if str(c["id"]) == ob["version"]), None)
                named = {roster.of_name(m.group(1)).agent for m in asks.ASK.finditer((ask or {}).get("body", "")[:4000])
                         if roster.of_name(m.group(1))}
                if ask and ob["to"] not in named:
                    flags.append(f"WRONG HOLDER: the ask names {sorted(named)}")
            # the due time by channel, at creation (or the reopen that restarted the clock)
            first = next(e for e in es if e["ev"] == "obligation")
            restart = [T(e["t"]) for e in es if e["ev"] == "reopened"]
            due_exp = expected_due(ob["to"], ob["kind"], opened)
            if (first["via"], T(first["due"])) != due_exp:
                flags.append(f"WRONG DUE: {first['via']} {R.central(T(first['due']))}, the channel says "
                             f"{due_exp[0]} {R.central(due_exp[1])}")
            if restart and not ob.get("dead_lettered"):
                exp = expected_due(ob["to"], ob["kind"], restart[-1])
                if T(ob["due"]) != exp[1]:
                    flags.append(f"WRONG DUE after reopen: {R.central(T(ob['due']))}, expected {R.central(exp[1])}")
            # was it real when it opened?
            real, why = owed_at(h, ob, opened, cache)
            if not real and (sends or dead):   # opened and settled in the same pass, never sent: no one was told
                flags.append(f"NOT REAL at open: {why}")
            for s, n in sends:
                ok, why = owed_at(h, ob, s, cache)
                if not ok and real:
                    flags.append(f"FALSE NAG: send {n} at {R.central(s)}, but {why}")
            if dead:
                ok, why = owed_at(h, ob, dead, cache)
                if not ok and real:
                    flags.append(f"FALSE DEAD-LETTER at {R.central(dead)}: {why}")
            if ack and ack[1] in ("reply on its thread", "reply naming the asker"):
                # the step short of #360: the oracle (#360's rule) does not take it, so each one is read by hand
                # (the PR lists them with the ask and the reply)
                flags.append(f"READ: acked by a plain reply on its thread at {R.central(ack[0])}")
            elif ack and ack[1] in ("reply on the thread", "reply naming it", "reply to the asker",
                                    "verdict on a PR it names"):
                ok, _ = owed_at(h, ob, ack[0], cache)
                if ok:
                    flags.append(f"FALSE ACK: {ack[1]} at {R.central(ack[0])}, but nothing answered it then")
            if ob["kind"] == "ask" and ob["to"] == "claude" and R.is_live(ob) and "ask from Muse" in ob["what"]:
                # the integrator answers Muse by mail, which GitHub never sees (the coordinator, 11-09): an ask to
                # Claude still open at the end with no reply on GitHub is "answered off-GitHub?", not a miss
                flags.append("OFF-GITHUB? Muse's ask to Claude, no reply on GitHub (the integrator answers by mail)")
            elif real and not ack and not closed:   # still open at the end: was it answered meanwhile?
                ok, why = owed_at(h, ob, h.end, cache)
                if not ok and R.is_live(ob) and ob.get("dead_lettered"):
                    # #350: a dead-letter ends only by an ack (test_03): it was reported to Cory once and stays in
                    # his standing count. Listed, not a defect.
                    flags.append(f"STANDS (dead-letter, ends by /ack): {why}")
                elif not ok and R.is_live(ob):
                    flags.append(f"NEVER SETTLED: {why}, still {'unseen' if ob.get('unseen') else 'open'}")
            rows.append(Row(job, ob, opened, sends, ack, closed, dead, due_exp, flags))

    # misses: every real unanswered ask, and every stuck PR or red branch, the reconciler never opened
    misses = []
    opened_asks = {(r.ob["version"], r.ob["to"]) for r in rows if r.ob["kind"] == "ask"}
    for c in h.comments:
        if c["_t"] < h.start - L.LIVE or c["_t"] > h.end or c["user"]["login"] == rules.BOT_LOGIN:
            continue
        if not h.open_at(c["_n"], c["_t"]) or asks.author(c["body"]) == "noise" or asks.LEDGER_MARK in c["body"]:
            continue
        for m in sorted({m.group(1) for m in asks.ASK.finditer(c["body"][:4000])} - {asks.author(c["body"])}):
            who = roster.of_name(m)
            agent = who.agent if who else m.lower()
            if (str(c["id"]), agent) in opened_asks:
                continue
            ans = real_answer(h, c, m)
            due = expected_due(agent, "ask", max(c["_t"], h.start))[1]
            if (ans is None or ans["_t"] > due) and due <= h.end and h.open_at(c["_n"], min(due, h.end)):
                misses.append(f"MISS ask {c['html_url'].rsplit('/', 1)[-1]} on #{c['_n']} to {m} at "
                              f"{R.central(c['_t'])}: " + (f"answered only {R.central(ans['_t'])}" if ans else
                                                            "never answered"))
    # when each obligation was open (owed, seen): from its opening to its ack, close or going unseen; a dead-letter
    # stays open (Cory holds it)
    spans: dict[str, list] = {}
    for r in rows:
        es = by_id.get(r.ob["id"], [])
        start, out = None, []
        for e in es:
            if e["ev"] in ("obligation", "reopened") and start is None:
                start = T(e["t"])
            elif e["ev"] in ("acked", "closed", "unseen") and start is not None:
                out.append((start, T(e["t"])))
                start = None
        if start is not None:
            out.append((start, h.end + STEP))
        spans.setdefault(r.ob["ref"], []).extend(out)

    def owed(ref: str, t: dt.datetime) -> bool:
        return any(a <= t < b for a, b in spans.get(ref, []))

    # a stuck PR: the same head, not merging, 4 h or more with nobody owing anything on it (a red or
    # conflicted PR is owed by its fixer; a running proof by no one until CI_LATE)
    for n in sorted({n for p in passes for n in p["prs"]}):
        run: list = []
        head = None
        worst = None
        for p in passes:
            got = p["prs"].get(n)
            if got is None:
                run, head = [], None
                continue
            action, reasons, sha, proof = got
            idle = action not in ("MERGE",) and proof not in ("running", "none") and not owed(f"PR|#{n}", p["t"])
            if sha != head or not idle:
                run, head = [], sha
                if not idle:
                    continue
            run.append((p["t"], action, reasons))
            if run[-1][0] - run[0][0] >= dt.timedelta(hours=4) and (worst is None or len(run) > len(worst)):
                worst = list(run)
        if worst:
            misses.append(f"STUCK PR #{n} @ {head[:8] if head else '?'} {worst[-1][1]} from {R.central(worst[0][0])} for "
                          f"{L.age_text(worst[-1][0] - worst[0][0])} with no one owing anything: "
                          f"{L.clean('; '.join(worst[-1][2]), 140)}")
    red_branches: dict[str, list] = {}
    for p in passes:
        for kind, ref, holder, state in p["items"]:
            if kind == "BRANCH" and "proof red" in state and not owed(f"BRANCH|{ref}", p["t"]):
                red_branches.setdefault(ref, []).append(p["t"])
    for name, ts in sorted(red_branches.items()):
        if ts[-1] - ts[0] >= L.ACT:
            misses.append(f"MISS red branch {name}: red {L.age_text(ts[-1] - ts[0])} with no PR, no obligation")
    batches = [b for b in cory.get("batches") or [] if T(b["at"]) >= h.start]
    # Cory's batches (the coordinator, 11-09): inside the integrator's hours (weekdays 8 AM-11 PM, Sat-Sun 9 AM-9 PM
    # Central), one per full-run slot (weekdays 8:17, 11:17, 2:17, 5:17, 8:17, 11:17; Sat-Sun 9:17, 1:17, 5:17,
    # 9:17); a batch with a new pipe item may go at once inside hours
    seen_slots = set()
    for b in batches:
        local = T(b["at"]).astimezone(R.CENTRAL)
        weekend = local.weekday() >= 5
        hours = range(9, 22) if weekend else range(8, 24)
        if local.hour not in hours:
            misses.append(f"CORY: a batch at night, {R.central(T(b['at']))}")
            continue
        if any(k.startswith(("gap:", "token:", "down:")) for k in b["keys"]):
            continue
        slots = [(9, 17), (13, 17), (17, 17), (21, 17)] if weekend else [(h_, 17) for h_ in (8, 11, 14, 17, 20, 23)]
        mine = [s for s in slots if (local.hour, local.minute) >= s]
        if not mine:
            misses.append(f"CORY: a batch before the first slot, {R.central(T(b['at']))}")
            continue
        key = (local.date(), mine[-1])
        if key in seen_slots:
            misses.append(f"CORY: two batches in one slot, {R.central(T(b['at']))}")
        seen_slots.add(key)
    return {"rows": rows, "misses": misses, "batches": batches, "passes": passes}


def verdict_counts(res: dict) -> dict:
    kinds = ("FALSE NAG", "FALSE DEAD-LETTER", "FALSE ACK", "NOT REAL", "NEVER SETTLED", "WRONG HOLDER", "WRONG DUE",
             "STANDS", "READ", "OFF-GITHUB?")
    out = {k: sum(1 for r in res["rows"] for f in r.flags if f.startswith(k)) for k in kinds}
    out["MISS"] = sum(1 for m in res["misses"] if m.startswith("MISS"))
    out["STUCK PR"] = sum(1 for m in res["misses"] if m.startswith("STUCK"))
    out["CORY"] = sum(1 for m in res["misses"] if m.startswith("CORY"))
    out["obligations"] = len(res["rows"])
    out["rows flagged"] = sum(1 for r in res["rows"] if r.flags)
    return out


def table(res: dict, h: History, flagged_only: bool = False) -> str:
    def c(t):
        return R.central(t) if t else ""
    lines = ["| id | job | kind | to (via) | ref | opened | nagged | due | acked | closed | dead-letter | check |",
             "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for r in sorted(res["rows"], key=lambda r: r.opened):
        if flagged_only and not r.flags:
            continue
        ob = r.ob
        nag = ", ".join(f"{n}: {c(s)}" for s, n in r.sends) or "-"
        ack = f"{c(r.acked[0])} {r.acked[1]}" if r.acked else ""
        clo = f"{c(r.closed[0])} {L.clean(r.closed[1], 40)}" if r.closed else ""
        chk = "; ".join(L.clean(f, 120) for f in r.flags) or "agrees"
        lines.append(f"| `{ob['id']}` | {r.job or '-'} | {ob['kind']} | {ob['to']} ({ob['via']}) | "
                     f"{L.clean(ob['ref'].split('|', 1)[1], 40)} | {c(r.opened)} | {nag} | {c(T(ob['due']))} | {ack} | "
                     f"{clo} | {c(r.dead)} | {chk} |")
    return "\n".join(lines)


def report(res: dict, h: History, counts: dict) -> str:
    out = [f"Replay of {R.central(h.start)} to {R.central(h.end)} Central: {len(res['passes'])} passes, one per "
           f"{int(STEP.total_seconds() // 60)} min.", "",
           "Verdict counts: " + ", ".join(f"{k} {v}" for k, v in counts.items()), ""]
    out += ["Misses (a real unanswered ask, stuck PR or red branch it never opened):", ""]
    out += [f"- {m}" for m in res["misses"]] or ["- none"]
    out += ["", f"Cory's hourly batches ({len(res['batches'])}):", ""]
    for b in res["batches"]:
        first = b["text"].split("\n", 1)[0]
        out.append(f"- {c_(b['at'])}: {len(b['keys'])} item(s). {L.clean(first, 100)}")
    return "\n".join(out)


def c_(stamp) -> str:
    return R.central(T(stamp))


def replay_main(args) -> int:
    import yaml
    cfg = yaml.safe_load((HERE.parents[1] / "work/agents.yml").read_text(encoding="utf-8")) or {}
    h = History(load(pathlib.Path(args.fixture)), rules.team_from_config(cfg))
    state = pathlib.Path(args.state_dir) if args.state_dir else pathlib.Path(tempfile.mkdtemp(prefix="replay-"))
    if (state / "HEAD").exists():
        print(f"replay: {state} already holds a state; give an empty directory")
        return 2
    passes = replay(h, cfg, state)
    res = check(h, passes, state, cfg)
    counts = verdict_counts(res)
    text = report(res, h, counts) + "\n\n" + table(res, h)
    if args.md:
        pathlib.Path(args.md).write_text(text + "\n", encoding="utf-8")
    print(report(res, h, counts))
    print()
    print(table(res, h, flagged_only=True))
    bad = sum(v for k, v in counts.items() if k not in ("obligations", "rows flagged", "STANDS", "READ", "OFF-GITHUB?"))
    print(f"\nVERDICT {'CLEAN' if not bad else 'NOT CLEAN'}: {counts['obligations']} obligations, "
          f"{counts['rows flagged']} flagged, {counts['MISS']} misses, {counts['STUCK PR']} stuck PRs")
    return 0 if not bad else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    f = sub.add_parser("fetch")
    f.add_argument("--out", required=True)
    f.add_argument("--hours", type=float, default=48)
    r = sub.add_parser("replay")
    r.add_argument("--fixture", required=True)
    r.add_argument("--state-dir", default="")
    r.add_argument("--md", default="")
    args = ap.parse_args()
    if args.cmd == "fetch":
        fetch(args.hours, pathlib.Path(args.out))
        return 0
    return replay_main(args)


if __name__ == "__main__":
    sys.exit(main())
