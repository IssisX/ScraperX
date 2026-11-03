#!/usr/bin/env python3
"""The switchboard (design v3 step B, job J-031): GitHub's wake switch and merger. No model decides here.

Grown from tools/board/sweep.py. Each run reads the state from GitHub and acts on it, so a run that
is dropped or repeated changes nothing: every write is either guarded by a marker comment for the
head it acts on, or idempotent at GitHub (a merge names the head sha it merges).

Parts (a workflow job runs one; `all` runs both, for a dry run):
  ask    the paid calls, the only part that holds ANTHROPIC_API_KEY: the automatic Claude review of
         a non-Claude PR, the eye on a `needs:eyes` PR's screenshots, the orchestrator (off in v0)
  steer  everything free: the merge rule, review asks to ChatGPT or Muse, one owner:* label per
         issue, the 7:00 AM Central digest

Usage:
  python3 tools/switchboard/switch.py --dry-run                     # every action printed, none taken
  python3 tools/switchboard/switch.py --part steer [--pr N | --sha S]
  python3 tools/switchboard/switch.py --part ask [--pr N | --sha S]
  python3 tools/switchboard/switch.py --part steer --issue N --label L   # an issues.labeled event
  python3 tools/switchboard/switch.py --part steer --digest [--force-digest]
Exit status: 0 done, 2 the switch could not read what it needs (nothing was half-done).
"""

from __future__ import annotations

import argparse
import base64
import dataclasses
import datetime as dt
import io
import os
import pathlib
import re
import subprocess
import sys
import time
import uuid
import zipfile
from decimal import Decimal
from zoneinfo import ZoneInfo

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import budget as money  # noqa: E402
import rules  # noqa: E402
from ghapi import GitHub, GitHubError  # noqa: E402

REPO = "IssisX/ScraperX"
PROOF_WORKFLOW = "wo000-delivery-spine.yml"
NOT_COUNTED = {"publish-checkpoint", "playtest-apk"}   # as sweep.py: skipped or signing-only off trunk
LEDGER_BRANCH = "switchboard/ledger"
LEDGER_PATH = "work/archive/runs.log"
DIGEST_TITLE = "Digest"
DIGEST_LABEL = "digest"
MUSE_LABEL = "wake:muse"
BOARD_ISSUE = 107
CENTRAL = ZoneInfo("America/Chicago")
DIGEST_HOUR = 7
MAX_DIFF_CHARS = 400_000
MAX_ARTIFACT_BYTES = 60_000_000
MAX_IMAGE_BYTES = 3_750_000            # base64 of this stays under 5 MB
MAX_PRS = 60
ACT_WITHIN_DAYS = 3                   # older heads get no new review or ask: J-030 claims or closes them
MARK = "<!-- switchboard:{kind} pr={pr} head={head} -->"
RUN_MARK = "<!-- switchboard:orchestrator-run id={run} -->"   # in the start comment: the ledger run its session pays
RUN_ID = re.compile(r"<!-- switchboard:orchestrator-run id=([0-9a-f]{12}) -->")
MAX_WATCHED = 30                       # open orchestrator issues the watcher reads (one page)
CONSOLE_SESSION = "https://platform.claude.com/workspaces/default/sessions/{session}"   # `default` workspace
OUTCOMES = {"end_turn": "finished its turn", "budget_reached": "stopped at its session budget",
            "retries_exhausted": "stopped after repeated errors", "refusal": "stopped on a refusal",
            "terminated": "ended (terminated)"}


def central(t: dt.datetime) -> str:
    return t.astimezone(CENTRAL).strftime("%-I:%M %p Central")


def load_yaml(path: pathlib.Path):
    import yaml
    with path.open(encoding="utf-8") as f:
        return yaml.safe_load(f)


class Switch:
    def __init__(self, gh: GitHub, root: pathlib.Path, now: dt.datetime, out=print):
        self.gh, self.root, self.now, self.out = gh, root, now, out
        self.team = rules.team_from_config(load_yaml(root / "work/agents.yml"))
        self.budget = money.budget_from_config(load_yaml(root / "work/budget.yml"))
        self.looks = load_yaml(root / "work/looks/sets.yml")
        self._jobs: dict[str, dict] | None = None
        self._ledger: tuple[str, str | None] | None = None
        self._claude = None

    # ---------- facts ----------
    def jobs(self) -> dict[str, dict]:
        """Open issues labeled `job`, by J-NNN."""
        if self._jobs is None:
            self._jobs = {}
            for it in self.gh.paged(f"repos/{self.gh.repo}/issues?labels=job&state=open"):
                if "pull_request" not in it and rules.job_of_title(it["title"]):
                    self._jobs[rules.job_of_title(it["title"])] = it
        return self._jobs

    def proof(self, sha: str) -> tuple[str, str, int]:
        """The newest proof run on this commit: (state, url, run id)."""
        runs = self.gh.get(f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/runs?head_sha={sha}&per_page=5")["workflow_runs"]
        if not runs:
            return "none", "", 0
        run = max(runs, key=lambda r: r["created_at"])
        if run["status"] != "completed":
            return "running", run["html_url"], run["id"]
        jobs = self.gh.get(f"repos/{self.gh.repo}/actions/runs/{run['id']}/jobs?per_page=50")["jobs"]
        failed = [j["name"] for j in jobs if j["name"] not in NOT_COUNTED and j["conclusion"] not in ("success", "skipped")]
        return ("red: " + ", ".join(failed) if failed else "green"), run["html_url"], run["id"]

    def verdicts(self, number: int) -> tuple[list[rules.Verdict], list[dict]]:
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        reviews = self.gh.paged(f"repos/{self.gh.repo}/pulls/{number}/reviews")
        out = []
        for c in comments:
            v = rules.parse_verdict(c.get("body") or "", c["created_at"], c["user"]["login"], self.team)
            if v:
                out.append(v)
        for r in reviews:
            v = rules.parse_verdict(r.get("body") or "", r.get("submitted_at") or "", r["user"]["login"], self.team)
            if v:
                out.append(v)
        return out, comments

    def rule_check_block(self, number: int, head: str) -> bool | None:
        """Run trunk's rule check over base...head (it reads the diff only). None if it cannot run."""
        try:
            subprocess.run(["git", "fetch", "-q", "--no-tags", "origin", rules.TRUNK,
                            f"+refs/pull/{number}/head:refs/remotes/switchboard/pr-{number}"],
                           cwd=self.root, check=True, capture_output=True, timeout=180)
            r = subprocess.run([sys.executable, "tools/rule_check/rule_check.py", f"origin/{rules.TRUNK}", head],
                               cwd=self.root, capture_output=True, text=True, timeout=180)
        except (subprocess.SubprocessError, OSError) as err:
            self.out(f"NOTE  #{number} rule_check could not run: {err}")
            return None
        if r.returncode not in (0, 1, 2):
            self.out(f"NOTE  #{number} rule_check exited {r.returncode}: {r.stderr.strip()[:200]}")
            return None
        return r.returncode == 2

    def pr_facts(self, pr: dict) -> tuple[rules.PrFacts, dict]:
        number, sha = pr["number"], pr["head"]["sha"]
        full = self.gh.get(f"repos/{self.gh.repo}/pulls/{number}")
        commit = self.gh.get(f"repos/{self.gh.repo}/commits/{sha}")
        proof, url, run_id = self.proof(sha)
        verdicts, comments = self.verdicts(number)
        job = rules.job_of_pr(pr.get("body")) or \
            rules.jobs_by_pr_mention({j: it["title"] for j, it in self.jobs().items()}).get(number, "")
        issue = self.jobs().get(job)
        labels = [l["name"] for l in pr.get("labels", [])] + ([l["name"] for l in issue["labels"]] if issue else [])
        needs = frozenset(l.split(":", 1)[1] for l in labels if l.startswith("needs:"))
        files = self.gh.paged(f"repos/{self.gh.repo}/pulls/{number}/files")
        facts = rules.PrFacts(
            number=number, head_sha=sha, head_ref=pr["head"]["ref"], base_ref=pr["base"]["ref"], draft=bool(pr["draft"]),
            mergeable=full.get("mergeable"), head_committed_at=commit["commit"]["committer"]["date"], proof=proof,
            verdicts=verdicts, needs=needs, rule_check_block=False,
            touches_workflows=any(f["filename"].startswith(".github/workflows/") for f in files))
        ctx = {"job": job, "issue": issue, "proof_url": url, "run_id": run_id, "comments": comments,
               "title": pr["title"], "body": pr.get("body") or "", "files": files}
        return facts, ctx

    def target_prs(self, only_pr: int, only_sha: str) -> list[dict]:
        prs = self.gh.paged(f"repos/{self.gh.repo}/pulls?state=open")[:MAX_PRS]
        if only_pr:
            prs = [p for p in prs if p["number"] == only_pr]
        if only_sha:
            prs = [p for p in prs if p["head"]["sha"] == only_sha]
        return prs

    def stale(self, facts: rules.PrFacts) -> bool:
        return rules.is_stale(facts.head_committed_at, self.now.strftime("%Y-%m-%dT%H:%M:%SZ"), ACT_WITHIN_DAYS)

    def marked(self, comments: list[dict], kind: str, pr: int, head: str) -> bool:
        tag = MARK.format(kind=kind, pr=pr, head=head)
        return any(tag in (c.get("body") or "") and c["user"]["login"] == rules.BOT_LOGIN for c in comments)

    # ---------- the ledger (append-only, on its own branch so trunk is never written per call) ----------
    def ledger(self) -> tuple[str, str | None]:
        if self._ledger is None:
            try:
                got = self.gh.get(f"repos/{self.gh.repo}/contents/{LEDGER_PATH}?ref={LEDGER_BRANCH}")
            except GitHubError as err:
                if "HTTP 404" not in err.detail:
                    raise
                self._ledger = ("", None)
            else:
                if got.get("size", 0) > self.budget.max_log_bytes or got.get("encoding") != "base64":
                    raise money.BudgetConfigError(f"{LEDGER_PATH} is {got.get('size')} bytes; rotate it before any paid call")
                self._ledger = (base64.b64decode(got["content"]).decode("utf-8"), got["sha"])
        return self._ledger

    def entries(self) -> list[money.Entry]:
        return money.parse_log(self.ledger()[0])

    def append(self, line: str, what: str) -> None:
        if self.gh.dry_run:
            self.out(f"DRY  append {LEDGER_BRANCH}:{LEDGER_PATH}  {what}")
            return
        for _ in range(3):
            text, sha = self.ledger()
            if sha is None:
                self.ensure_ledger_branch()
            body = {"message": f"[skip ci] runs.log: {what}"[:120], "branch": LEDGER_BRANCH,
                    "content": base64.b64encode((text + line + "\n").encode("utf-8")).decode("ascii")}
            if sha:
                body["sha"] = sha
            try:
                got = self.gh.write("PUT", f"repos/{self.gh.repo}/contents/{LEDGER_PATH}", body, what)
            except GitHubError as err:
                if "HTTP 409" in err.detail or "HTTP 422" in err.detail:   # someone appended first: re-read
                    self._ledger = None
                    continue
                raise
            self._ledger = (text + line + "\n", got["content"]["sha"])
            return
        raise money.BudgetConfigError(f"could not append to {LEDGER_PATH} after 3 tries")

    def ensure_ledger_branch(self) -> None:
        try:
            self.gh.get(f"repos/{self.gh.repo}/git/ref/heads/{LEDGER_BRANCH}")
        except GitHubError as err:
            if "HTTP 404" not in err.detail:
                raise
            trunk = self.gh.get(f"repos/{self.gh.repo}/git/ref/heads/{rules.TRUNK}")["object"]["sha"]
            self.gh.write("POST", f"repos/{self.gh.repo}/git/refs",
                          {"ref": f"refs/heads/{LEDGER_BRANCH}", "sha": trunk}, f"create {LEDGER_BRANCH}")

    def job_cap(self, ctx: dict) -> str:
        issue = ctx.get("issue")
        if not issue:
            return ""
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{issue['number']}/comments")
        return rules.budget_override(((c.get("body") or "", c["user"]["login"]) for c in comments), self.team)

    def gate(self, kind: str, job: str, cost: Decimal, cap: str, key: str) -> money.Check:
        """The budget check before a paid call; a refusal is logged once per key per UTC day."""
        entries = self.entries()
        verdict = money.check(self.budget, entries, self.now, job, cost, cap)
        if not verdict.ok:
            today = self.now.date()
            if not any(e.phase == "refused" and e.note.startswith(key) and e.ts.date() == today for e in entries):
                self.append(money.record(self.now, uuid.uuid4().hex[:12], "refused", kind, job, Decimal(0),
                                         f"{key} {verdict.reason}"), f"refused {key}")
        return verdict

    def claude(self):
        if self._claude is None:
            import claude_calls
            if not os.environ.get("ANTHROPIC_API_KEY"):
                raise money.BudgetConfigError("ANTHROPIC_API_KEY is not set; no paid call can run")
            self._claude = claude_calls.client()
        return self._claude

    def paid(self, kind: str, job: str, call: str, system: str, content: list, schema: dict, cap: str, key: str):
        """Count tokens, gate on the budget, reserve, call, record the actual cost. None when refused."""
        import claude_calls
        spec = self.budget.calls[call]
        if self.gh.dry_run:
            worst = money.worst_case(self.budget, call, spec.max_input_tokens)
            v = money.check(self.budget, self.entries(), self.now, job, worst, cap)
            self.out(f"DRY  {kind} {key} on {spec.model}: {'would run' if v.ok else 'REFUSED'} ({v.reason}; upper bound)")
            return None
        n = claude_calls.count_tokens(self.claude(), spec.model, system, content)
        if n > spec.max_input_tokens:
            self.out(f"SKIP {kind} {key}: {n} input tokens is above the {spec.max_input_tokens} limit")
            return "too-large"
        worst = money.worst_case(self.budget, call, n)
        v = self.gate(kind, job, worst, cap, key)
        if not v.ok:
            self.out(f"REFUSED {kind} {key}: {v.reason}")
            return None
        run = uuid.uuid4().hex[:12]
        self.append(money.record(self.now, run, "reserve", kind, job, worst, f"{key} in={n}"), f"reserve {key}")
        try:
            res = claude_calls.ask_json(self.claude(), spec.model, spec.max_tokens, spec.effort, system, content, schema)
        except claude_calls.ClaudeCallError as err:
            self.out(f"FAILED {kind} {key}: {err}")   # the reserve stands: an error may still have been billed
            return "error"
        actual = worst
        note = f"{key} {res.model} in={res.input_tokens} out={res.output_tokens} stop={res.stop_reason}"
        if res.model in self.budget.prices:
            actual = self.budget.prices[res.model].cost(res.input_tokens, res.output_tokens)
        else:
            note += " unpriced-model:reserve-kept"
        self.append(money.record(self.now, run, "end", kind, job, actual, note), f"end {key}")
        return res

    # ---------- steer ----------
    def steer_pr(self, facts: rules.PrFacts, ctx: dict) -> None:
        author = self.team.vendor_of_branch(facts.head_ref)
        d = rules.merge_decision(facts, author)
        if d.action == "WAIT" and d.reasons == ("GitHub is still computing mergeability",):
            time.sleep(3)   # the first read starts GitHub's computation; read once more
            facts = dataclasses.replace(facts, mergeable=self.gh.get(f"repos/{self.gh.repo}/pulls/{facts.number}").get("mergeable"))
            d = rules.merge_decision(facts, author)
        if d.action in ("MERGE", "HUMAN-MERGE"):
            facts = dataclasses.replace(facts, rule_check_block=self.rule_check_block(facts.number, facts.head_sha))
            d = rules.merge_decision(facts, author)
        self.out(f"{d.action:<11} #{facts.number} {facts.head_ref} @{facts.head_sha[:8]} author={author} "
                 f"proof={facts.proof} job={ctx['job'] or '-'}: {'; '.join(d.reasons)}")
        if d.action == "MERGE":
            self.merge(facts, ctx, d)
        if rules.needs_review(facts) and self.stale(facts):
            self.out(f"STALE       #{facts.number} head older than {ACT_WITHIN_DAYS} days: no review asked")
        elif rules.needs_review(facts):
            pick = rules.pick_reviewer(author, facts.needs, self.team)
            if pick.route == "ask":
                self.ask_review(facts, ctx, pick.agent, "")

    def merge(self, facts: rules.PrFacts, ctx: dict, d: rules.MergeDecision) -> None:
        got = self.gh.write("PUT", f"repos/{self.gh.repo}/pulls/{facts.number}/merge",
                            {"sha": facts.head_sha, "merge_method": "merge",
                             "commit_title": f"Merge pull request #{facts.number} from {facts.head_ref} (switchboard)"},
                            f"merge #{facts.number} at @{facts.head_sha[:8]}")
        if not self.gh.dry_run and not (got or {}).get("merged"):
            self.out(f"NOTE  #{facts.number} merge not done: {(got or {}).get('message', '')[:200]}")
            return
        # A merge by the workflow token starts no push workflow, so the trunk proof is dispatched.
        self.gh.write("POST", f"repos/{self.gh.repo}/actions/workflows/{PROOF_WORKFLOW}/dispatches",
                      {"ref": rules.TRUNK}, f"dispatch the proof on {rules.TRUNK}")
        line = (f"Switchboard, {central(self.now)}: merged #{facts.number} at @{facts.head_sha[:8]} "
                f"({'; '.join(d.reasons)}; proof {ctx['proof_url']}). The trunk proof is dispatched. "
                f"The owner can overturn any merge.")
        self.gh.comment(facts.number, line, f"merged-note on #{facts.number}")
        if ctx.get("issue"):
            self.gh.comment(ctx["issue"]["number"], line, f"merged-note on job issue #{ctx['issue']['number']}")

    def ask_review(self, facts: rules.PrFacts, ctx: dict, agent: str, why: str) -> None:
        where = ctx["issue"]["number"] if ctx.get("issue") else facts.number
        comments = ctx["comments"] if where == facts.number else self.gh.paged(f"repos/{self.gh.repo}/issues/{where}/comments")
        tag = MARK.format(kind=f"ask-{agent}", pr=facts.number, head=facts.head_sha)
        if self.marked(comments, f"ask-{agent}", facts.number, facts.head_sha):
            self.out(f"ASKED       #{facts.number} review already asked of {agent} on #{where}")
            return
        name = {"chatgpt": "ChatGPT", "muse": "Muse"}.get(agent, agent)
        job = ctx["job"] or f"PR #{facts.number}"
        head = f"CLAUDE_TO_MUSE: {job} review #{facts.number} @ {facts.head_sha[:8]}" if agent == "muse" else \
               f"**Switchboard → {name}:** review #{facts.number} @ {facts.head_sha[:8]} ({job})"
        body = (f"{head}\n\n{why}Proof is green on the head ({ctx['proof_url']}). Review it with "
                f"`03_EXECUTION/REVIEW_CHECKLIST.md` and post one verdict on #{facts.number} whose first line is\n"
                f"`**{name} (review of #{facts.number} @ {facts.head_sha[:8]}): LAND**` (or FIX with command, input, "
                f"expected, observed; a FIX without them counts as a NOTE).\n\n{tag}")
        self.gh.comment(where, body, f"ask {name} to review #{facts.number} on #{where}")
        if agent == "muse":
            self.gh.write("POST", f"repos/{self.gh.repo}/issues/{where}/labels", {"labels": [MUSE_LABEL]},
                          f"label #{where} {MUSE_LABEL} (the bridge emails Muse, then removes it)")

    def owner_labels(self, number: int, just_added: str) -> None:
        issue = self.gh.get(f"repos/{self.gh.repo}/issues/{number}")
        owners = [l["name"] for l in issue["labels"] if l["name"].startswith("owner:")]
        if len(owners) < 2:
            return
        if just_added in owners:
            keep_order = [o for o in owners if o != just_added] + [just_added]
        else:  # the hourly sweep: order by when each label was added
            events = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/events")
            added = {}
            for e in events:
                if e.get("event") == "labeled" and e.get("label", {}).get("name") in owners:
                    added[e["label"]["name"]] = e["created_at"]
            keep_order = sorted(owners, key=lambda o: added.get(o, ""))
        for extra in rules.extra_owner_labels(keep_order):
            self.out(f"OWNER       #{number} has {', '.join(keep_order)}: removing {extra}")
            self.gh.write("DELETE", f"repos/{self.gh.repo}/issues/{number}/labels/{extra}", None, f"remove {extra} from #{number}")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: one owner per job. `{keep_order[0]}` holds "
                                    f"#{number}, so `{extra}` was removed. Free the job first to hand it over.")

    def sweep_owner_labels(self) -> None:
        for it in self.jobs().values():
            if sum(1 for l in it["labels"] if l["name"].startswith("owner:")) > 1:
                self.owner_labels(it["number"], "")

    # ---------- ask (paid) ----------
    def ask_pr(self, facts: rules.PrFacts, ctx: dict) -> None:
        author = self.team.vendor_of_branch(facts.head_ref)
        if self.stale(facts):
            return
        if rules.needs_review(facts) and rules.pick_reviewer(author, facts.needs, self.team).route == "claude-api":
            if self.marked(ctx["comments"], "review", facts.number, facts.head_sha):
                self.out(f"REVIEWED    #{facts.number} automatic review already posted for @{facts.head_sha[:8]}")
            else:
                self.review(facts, ctx, author)
        if rules.needs_eye(facts):
            if self.marked(ctx["comments"], "eye", facts.number, facts.head_sha):
                self.out(f"LOOKED      #{facts.number} eye already posted for @{facts.head_sha[:8]}")
            else:
                self.eye(facts, ctx)

    def review(self, facts: rules.PrFacts, ctx: dict, author: str) -> None:
        import claude_calls
        key = f"review#{facts.number}@{facts.head_sha[:8]}"
        self.out(f"REVIEW      #{facts.number} author={author}: automatic Claude review")
        diff = "" if self.gh.dry_run else self.gh.get_text(f"repos/{self.gh.repo}/pulls/{facts.number}", "application/vnd.github.diff")
        if len(diff) > MAX_DIFF_CHARS:
            self.ask_review(facts, ctx, self.team.human_reviewers[0], f"The diff is {len(diff)} characters, too large for the automatic review. ")
            return
        checklist = (self.root / "03_EXECUTION/REVIEW_CHECKLIST.md").read_text(encoding="utf-8")
        content = [claude_calls.text_block(f"<checklist>\n{checklist}\n</checklist>"),
                   claude_calls.text_block(f"<pr number=\"{facts.number}\" head=\"{facts.head_sha}\" branch=\"{facts.head_ref}\">\n"
                                           f"<title>{ctx['title']}</title>\n<body>\n{ctx['body'][:20000]}\n</body>\n</pr>"),
                   claude_calls.text_block(f"<diff>\n{diff}\n</diff>")]
        res = self.paid("review", ctx["job"], "review", claude_calls.REVIEW_SYSTEM, content, claude_calls.REVIEW_SCHEMA,
                        self.job_cap(ctx), key)
        if res is None:   # refused by the budget, or a dry run: the next run tries again
            return
        if isinstance(res, str) or res.data is None:
            why = {"too-large": "The diff is too large for the automatic review. ",
                   "error": "The automatic review failed to run. "}.get(res) if isinstance(res, str) else \
                  f"The automatic review stopped ({res.stop_reason}). "
            self.ask_review(facts, ctx, self.team.human_reviewers[0], why)
            return
        d = res.data
        verdict = d["verdict"]
        lines = [f"**Claude (automatic review of #{facts.number} @ {facts.head_sha[:8]}): {verdict}**", "", d["summary"]]
        if verdict == "FIX":
            lines += [""] + [f"{k}: {d['repro'][k]}" for k in ("command", "input", "expected", "observed")]
        if d["notes"]:
            lines += ["", "Notes (never blocking):"] + [f"- {n}" for n in d["notes"][:12]]
        lines += ["", f"_{res.model} through the API; {central(self.now)}._",
                  MARK.format(kind="review", pr=facts.number, head=facts.head_sha)]
        self.gh.comment(facts.number, "\n".join(lines), f"post automatic review {verdict} on #{facts.number}")

    def eye(self, facts: rules.PrFacts, ctx: dict) -> None:
        import claude_calls
        key = f"eye#{facts.number}@{facts.head_sha[:8]}"
        self.out(f"EYE         #{facts.number}: Claude vision on the {self.looks['artifact_prefix']}* screenshots")
        if self.gh.dry_run:
            self.paid("eye", ctx["job"], "eye", claude_calls.EYE_SYSTEM, [], claude_calls.EYE_SCHEMA, "", key)
            return
        frames = self.frames(ctx["run_id"])
        content: list = []
        for name, s in self.looks["sets"].items():
            rubric = (self.root / s["rubric"]).read_text(encoding="utf-8")
            content.append(claude_calls.text_block(f"<set name=\"{name}\">\n<rubric>\n{rubric}\n</rubric>\n</set>"))
            for f in s["frames"]:
                if f not in frames:
                    self.out(f"NOTE  #{facts.number} eye: frame {f} is missing from the artifact")
                    return
                content += [claude_calls.text_block(f"Set {name}, frame {f}:"), claude_calls.png_block(frames[f])]
        res = self.paid("eye", ctx["job"], "eye", claude_calls.EYE_SYSTEM, content, claude_calls.EYE_SCHEMA,
                        self.job_cap(ctx), key)
        if not isinstance(res, claude_calls.Result) or res.data is None:
            self.out(f"NOTE  #{facts.number} eye gave no judgment ({res if isinstance(res, str) else getattr(res, 'stop_reason', 'refused')})")
            return
        sets = res.data["sets"]
        expected = set(self.looks["sets"])
        judged = {s["set"] for s in sets}
        overall = "LAND" if judged == expected and all(s["verdict"] == "LAND" for s in sets) else "FIX"
        lines = [f"**Claude eye (look of #{facts.number} @ {facts.head_sha[:8]}): {overall}**", ""]
        for s in sets:
            lines.append(f"- {s['set']}: {s['verdict']}; rubric: \"{s['rubric_line']}\"; frame `{s['frame']}`. {s['reason']}")
        if judged != expected:
            lines.append(f"- sets not judged: {', '.join(sorted(expected - judged)) or '-'}")
        if overall == "FIX":
            lines += ["", f"command: download the `{self.looks['artifact_prefix']}*` artifact of {ctx['proof_url']}",
                      "input: the frame named above", "expected: the rubric line holds", "observed: it does not (reason above)"]
        lines += ["", f"_{res.model} through the API; {central(self.now)}. Taste stays the owner's._",
                  MARK.format(kind="eye", pr=facts.number, head=facts.head_sha)]
        self.gh.comment(facts.number, "\n".join(lines), f"post eye {overall} on #{facts.number}")

    def frames(self, run_id: int) -> dict[str, bytes]:
        """The named PNGs from the proof run's audits artifact, read by name (never extracted to disk)."""
        arts = self.gh.get(f"repos/{self.gh.repo}/actions/runs/{run_id}/artifacts?per_page=100")["artifacts"]
        art = next((a for a in arts if a["name"].startswith(self.looks["artifact_prefix"]) and not a["expired"]), None)
        if not art:
            raise RuntimeError(f"run {run_id} has no {self.looks['artifact_prefix']}* artifact")
        if art["size_in_bytes"] > MAX_ARTIFACT_BYTES:
            raise RuntimeError(f"artifact {art['name']} is {art['size_in_bytes']} bytes, above {MAX_ARTIFACT_BYTES}")
        blob = self.gh.get_bytes(f"repos/{self.gh.repo}/actions/artifacts/{art['id']}/zip")
        wanted = {f for s in self.looks["sets"].values() for f in s["frames"]}
        out = {}
        with zipfile.ZipFile(io.BytesIO(blob)) as z:
            for info in z.infolist():
                if info.filename in wanted and info.file_size <= MAX_IMAGE_BYTES:
                    data = z.read(info)
                    if data[:8] == b"\x89PNG\r\n\x1a\n":
                        out[info.filename] = data
        return out

    def orchestrate(self, number: int) -> None:
        """Design 2.3 step 1, behind `needs:orchestrator` and budget.yml (off in v0)."""
        import claude_calls
        issue = self.gh.get(f"repos/{self.gh.repo}/issues/{number}")
        labels = {l["name"] for l in issue["labels"]}
        if not {"job", "needs:orchestrator"} <= labels:
            return
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        if self.marked(comments, "orchestrator", number, "-"):
            self.out(f"ORCH        #{number} already handled")
            return
        b = self.budget
        job = rules.job_of_title(issue["title"])
        tag = MARK.format(kind="orchestrator", pr=number, head="-")
        if not (b.orchestrator_enabled and b.orchestrator_agent_id and b.orchestrator_environment_id):
            self.out(f"ORCH        #{number}: off in budget.yml (orchestrator.enabled, agent_id, environment_id); not started")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: `needs:orchestrator` is set, but the orchestrator "
                                    f"is off in `work/budget.yml` until step C defines the hosted agent. Nothing was started.\n\n{tag}")
            return
        if not os.environ.get("AGENT_GITHUB_TOKEN") and not self.gh.dry_run:
            self.out(f"ORCH        #{number}: no AGENT_GITHUB_TOKEN in this job; not started")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: `needs:orchestrator` is set, but the workflow has no "
                                    f"`AGENT_GITHUB_TOKEN` secret, so the API agent cannot push. Nothing was started.\n\n{tag}")
            return
        key = f"orchestrator#{number}"
        v = self.gate("orchestrator", job, b.orchestrator_session_usd, self.job_cap({"issue": issue}), key)
        if not v.ok:
            self.out(f"REFUSED orchestrator #{number}: {v.reason}")
            return
        if self.gh.dry_run:
            self.out(f"DRY  beta.sessions.create for #{number} with a ${b.orchestrator_session_usd} budget ({v.reason})")
            return
        client = self.claude()   # before the reserve: no key, no reserve
        run = uuid.uuid4().hex[:12]
        self.append(money.record(self.now, run, "reserve", "orchestrator", job, b.orchestrator_session_usd, key), f"reserve {key}")
        message = (f"Job {job}, GitHub issue #{number} in {self.gh.repo}: {issue['title']}\n\n{(issue.get('body') or '')[:30000]}\n\n"
                   f"Read work/tasks/api-agent.md first, then the issue's newest comments, then do the job.")
        try:
            session = claude_calls.start_session(client, b.orchestrator_agent_id, b.orchestrator_environment_id,
                                                 f"{job} #{number}", message, b.orchestrator_session_usd,
                                                 f"https://github.com/{self.gh.repo}", os.environ.get("AGENT_GITHUB_TOKEN", ""),
                                                 rules.TRUNK, b.orchestrator_vault_id)
        except claude_calls.SessionStartRejected as err:
            # An HTTP error status: the server made no session, so the reserve is released at once.
            self.out(f"FAILED orchestrator #{number}: {err}")
            self.append(money.record(self.now, run, "end", "orchestrator", job, Decimal(0),
                                     f"{key} start rejected HTTP {err.status}"), f"end {key} (start rejected)")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: the orchestrator session was not started. "
                                    f"`beta.sessions.create` answered HTTP {err.status} ({err.error_class}). No session "
                                    f"exists, and the reserve is released. Fix the cause, then re-add `needs:orchestrator`."
                                    f"\n\n{MARK.format(kind='orchestrator-failed', pr=number, head=run)}")
            return
        except claude_calls.SessionStartUnknown as err:
            # No answer: a session may exist. The reserve stands, and the issue is marked handled so a
            # re-label cannot start a second session beside it; a person looks in the Console.
            self.out(f"FAILED orchestrator #{number}: {err}")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: `beta.sessions.create` got no answer "
                                    f"({', '.join(err.classes)}), so a session may exist. The ${b.orchestrator_session_usd} "
                                    f"reserve stays on the ledger (run `{run}`) and no second session is started. Look for "
                                    f"it in the Console.\n\n{tag}")
            return
        except claude_calls.ClaudeCallError as err:
            # Refused locally before any request was sent: no session exists.
            self.out(f"FAILED orchestrator #{number}: {err}")
            self.append(money.record(self.now, run, "end", "orchestrator", job, Decimal(0),
                                     f"{key} start refused locally"), f"end {key} (start refused)")
            self.gh.comment(number, f"Switchboard, {central(self.now)}: the orchestrator session was not started: {err}. "
                                    f"No session exists, and the reserve is released.\n\n"
                                    f"{MARK.format(kind='orchestrator-failed', pr=number, head=run)}")
            return
        self.out(f"ORCH        #{number}: session {session} started on run {run}")
        self.gh.comment(number, f"Switchboard, {central(self.now)}: orchestrator session `{session}` started with a "
                                f"${b.orchestrator_session_usd} budget. Trace: {CONSOLE_SESSION.format(session=session)}\n\n"
                                f"{tag}\n{RUN_MARK.format(run=run)}")

    # ---------- the session watcher (ask) ----------
    def watch_sessions(self) -> int:
        """Each open `job` + `needs:orchestrator` issue with a start comment: answer a paused call with deny
        (nobody is watching), and when the session is over write its `end` and say so once. Returns failures."""
        listed = self.gh.get(f"repos/{self.gh.repo}/issues?labels=job,needs:orchestrator&state=open&per_page={MAX_WATCHED}") or []
        failed = 0
        for it in listed[:MAX_WATCHED]:
            if "pull_request" in it:
                continue
            try:   # one session's trouble never stops the others; every action is safe to repeat next run
                self.watch_session(it)
            except (GitHubError, RuntimeError, KeyError, ValueError) as err:
                failed += 1
                self.out(f"ERROR       #{it['number']}: {type(err).__name__}: {err}")
        return failed

    def start_comment(self, comments: list[dict], number: int) -> tuple[str, str] | None:
        """(session id, run id or "") from the switchboard's own start comment; None if there is none."""
        tag = MARK.format(kind="orchestrator", pr=number, head="-")
        for c in comments:
            body = c.get("body") or ""
            if c["user"]["login"] == rules.BOT_LOGIN and tag in body:
                sid = re.search(r"orchestrator session `(sesn_[A-Za-z0-9]+)`", body)
                if sid:
                    run = RUN_ID.search(body)
                    return sid.group(1), run.group(1) if run else ""
        return None

    def run_of(self, number: int, found_run: str) -> str:
        """The ledger run this session pays: carried in the start comment, else (sessions started before the
        comment carried it) the newest orchestrator reserve noted `orchestrator#N`."""
        if found_run:
            return found_run
        notes = [e.run for e in self.entries() if e.phase == "reserve" and e.kind == "orchestrator" and e.note == f"orchestrator#{number}"]
        return notes[-1] if notes else ""

    def watch_session(self, issue: dict) -> None:
        import claude_calls
        number = issue["number"]
        comments = self.gh.paged(f"repos/{self.gh.repo}/issues/{number}/comments")
        found = self.start_comment(comments, number)
        if not found:
            return
        session, found_run = found
        if self.marked(comments, "session-end", number, session):
            return
        if self.gh.dry_run and not os.environ.get("ANTHROPIC_API_KEY"):
            self.out(f"DRY  WATCH #{number}: would read session {session}")
            return
        st = claude_calls.session_state(self.claude(), session)
        trace = CONSOLE_SESSION.format(session=session)
        if st.status in ("running", "rescheduling"):
            self.out(f"WATCH       #{number} {session} is {st.status}")
        elif st.status == "idle" and st.stop == "requires_action":
            self.out(f"WATCH       #{number} {session} paused on {len(st.pending)} call(s): denying")
            if self.gh.dry_run:
                self.out(f"DRY  deny {len(st.pending)} pending call(s) of {session}")
            else:
                claude_calls.deny_pending(self.claude(), session, st.pending)   # deny first: a comment without it leaves the stall
            if not self.marked(comments, "session-pause", number, st.idle_event):
                self.gh.comment(number, f"Switchboard, {central(self.now)}: session `{session}` paused on {len(st.pending)} "
                                        f"call(s) that needed approval and nobody is watching, so each was denied and the agent "
                                        f"carries on without it. Trace: {trace}\n\n"
                                        f"{MARK.format(kind='session-pause', pr=number, head=st.idle_event)}")
        elif st.status == "terminated" or (st.status == "idle" and st.stop in claude_calls.FINISHED_STOPS):
            self.finish_session(issue, session, found_run, "terminated" if st.status == "terminated" else st.stop,
                                st.list_cost_cents, comments, trace)
        else:
            self.out(f"WATCH       #{number} {session} is {st.status} ({st.stop or 'no idle event'}): nothing to do")

    def finish_session(self, issue: dict, session: str, found_run: str, outcome: str, cents: int | None,
                       comments: list[dict], trace: str) -> None:
        number = issue["number"]
        if cents is None:   # the reserve keeps counting in full until a cost is reported
            self.out(f"WATCH       #{number} {session} is over ({outcome}) but reports no cost yet: the reserve stands")
            return
        cost = Decimal(cents).scaleb(-2)
        run = self.run_of(number, found_run)
        job = rules.job_of_title(issue["title"])
        if not run:
            ledger = "No reserve for it was found on the ledger, so none was closed."
        elif any(e.run == run and e.phase == "end" for e in self.entries()):
            ledger = f"The ledger already holds the end of run `{run}`."
        else:
            self.append(money.record(self.now, run, "end", "orchestrator", job, cost,
                                     f"orchestrator#{number} {session} stop={outcome} list_cost_cents={cents}"), f"end orchestrator#{number}")
            ledger = f"Ledger run `{run}` now counts ${cost:.2f} instead of the ${self.budget.orchestrator_session_usd} reserve."
        self.out(f"WATCH       #{number} {session} {OUTCOMES.get(outcome, outcome)}: ${cost:.2f}")
        self.gh.comment(number, f"Switchboard, {central(self.now)}: orchestrator session `{session}` {OUTCOMES.get(outcome, outcome)}. "
                                f"Cost at list price: ${cost:.2f}. {ledger} Trace: {trace}\n\n"
                                f"{MARK.format(kind='session-end', pr=number, head=session)}")

    # ---------- the digest ----------
    def digest(self, force: bool) -> None:
        local = self.now.astimezone(CENTRAL)
        if local.hour < DIGEST_HOUR and not force:
            self.out(f"DIGEST      not yet: {central(self.now)}")
            return
        issues = self.gh.paged(f"repos/{self.gh.repo}/issues?labels={DIGEST_LABEL}&state=open")
        # Found by its label, not its title, so the owner can rename the issue freely.
        issue = next((i for i in issues if "pull_request" not in i), None)
        today = local.strftime("%A %B %-d")
        if issue:
            recent = self.gh.paged(f"repos/{self.gh.repo}/issues/{issue['number']}/comments?since="
                                   f"{(self.now - dt.timedelta(hours=26)).strftime('%Y-%m-%dT%H:%M:%SZ')}")
            if any(c["user"]["login"] == rules.BOT_LOGIN and f"Digest, {today}" in (c.get("body") or "") for c in recent) and not force:
                self.out(f"DIGEST      already posted for {today}")
                return
        body = self.digest_text(today)
        self.out(body)
        if issue is None:
            got = self.gh.write("POST", f"repos/{self.gh.repo}/issues",
                                {"title": DIGEST_TITLE, "labels": [DIGEST_LABEL],
                                 "body": "The switchboard's 7:00 AM Central digest, one comment a day. Subscribe to this issue."},
                                "create the Digest issue")
            if got is None:
                return
            issue = got
        self.gh.comment(issue["number"], body, f"digest for {today} on #{issue['number']}")

    def digest_text(self, today: str) -> str:
        since = self.now - dt.timedelta(hours=24)
        closed = self.gh.paged(f"repos/{self.gh.repo}/pulls?state=closed&base={rules.TRUNK}&sort=updated&direction=desc")[:100]
        landed = [p for p in closed if p.get("merged_at") and dt.datetime.fromisoformat(p["merged_at"].replace("Z", "+00:00")) >= since]
        red, waiting, asked = [], [], []
        for pr in self.target_prs(0, ""):
            facts, ctx = self.pr_facts(pr)
            author = self.team.vendor_of_branch(facts.head_ref)
            d = rules.merge_decision(facts, author)
            if d.action == "FIX-CI":
                red.append(f"#{facts.number} {facts.head_ref}: {facts.proof}")
            if d.action == "HUMAN-MERGE":
                waiting.append(f"#{facts.number} is ready but changes workflow files: the integrator merges it")
            if rules.needs_review(facts) and not self.stale(facts) and \
                    rules.pick_reviewer(author, facts.needs, self.team).route == "ask":
                asked.append(f"#{facts.number} waits on a review from "
                             f"{rules.pick_reviewer(author, facts.needs, self.team).agent}")
        for it in self.jobs().values():
            if any(l["name"] == "state:blocked" for l in it["labels"]):
                waiting.append(f"#{it['number']} {it['title'][:70]} is blocked")
        entries = self.entries()
        paused = [e.note for e in entries if e.phase == "refused" and e.ts >= since]
        s = money.spent(entries, self.now, "")
        sessions = []
        for it in self.jobs().values():
            claude = [l["name"][6:] for l in it["labels"] if l["name"].startswith("owner:claude")]
            if claude:
                sessions.append(f"{claude[0]} on #{it['number']} {it['title'][:60]}")
        orch = [e.note for e in entries if e.kind == "orchestrator" and e.phase == "reserve" and e.ts >= self.now - dt.timedelta(days=7)]

        def block(title: str, items: list[str]) -> str:
            return f"**{title}**\n" + ("\n".join(f"- {i}" for i in items) if items else "- none")

        return "\n\n".join([
            f"Digest, {today} ({central(self.now)})",
            block("Landed since yesterday", [f"#{p['number']} {p['title'][:80]}" for p in landed]),
            block("Red", red),
            block("Waits on you or the integrator", waiting + asked),
            block("Paused by the budget", paused),
            f"**API spend:** ${s.day:.2f} today, ${s.month:.2f} this month of ${self.budget.month:.2f}.",
            block("Claude sessions holding jobs", sessions + [f"orchestrator: {o}" for o in orch]),
        ])


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--repo", default=REPO)
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[2]))
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--part", choices=("ask", "steer", "all"), default="all")
    ap.add_argument("--pr", type=int, default=0)
    ap.add_argument("--sha", default="")
    ap.add_argument("--issue", type=int, default=0)
    ap.add_argument("--label", default="")
    ap.add_argument("--digest", action="store_true")
    ap.add_argument("--force-digest", action="store_true")
    args = ap.parse_args()
    if args.sha and not all(c in "0123456789abcdef" for c in args.sha):
        ap.error("--sha must be hex")
    now = dt.datetime.now(dt.timezone.utc)
    sw = Switch(GitHub(args.repo, args.dry_run), pathlib.Path(args.root), now)
    sw.out(f"switchboard {args.part}{' DRY RUN' if args.dry_run else ''} at {central(now)} on {args.repo}")
    steer, ask = args.part in ("steer", "all"), args.part in ("ask", "all")
    if args.issue:
        if steer and args.label.startswith("owner:"):
            sw.owner_labels(args.issue, args.label)
        if ask:
            sw.orchestrate(args.issue)
        return 0
    if args.digest or args.force_digest:
        if steer:
            sw.digest(args.force_digest)
        return 0
    if steer and not (args.pr or args.sha):
        sw.sweep_owner_labels()
    failed = 0
    if ask:
        failed += sw.watch_sessions()
    for pr in sw.target_prs(args.pr, args.sha):
        try:   # one PR's trouble never stops the others; every action is safe to repeat next run
            facts, ctx = sw.pr_facts(pr)
            if ask:
                sw.ask_pr(facts, ctx)
            if steer:
                sw.steer_pr(facts, ctx)
        except (GitHubError, RuntimeError, KeyError, ValueError, zipfile.BadZipFile) as err:
            failed += 1
            sw.out(f"ERROR       #{pr['number']}: {type(err).__name__}: {err}")
    return 2 if failed else 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (GitHubError, money.BudgetConfigError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as err:
        print(f"switchboard: {type(err).__name__}: {err}", file=sys.stderr)
        sys.exit(2)
