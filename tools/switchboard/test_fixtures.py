"""The job machine against eight real timelines (fixtures/jobs): the evidence as it stood after each event goes in,
and the machine's state for the job must be the one the fixture expects at that step.

The fixtures were recorded under the old system, so three of its forms are translated into the machine's, here
and nowhere else:
  * the plan. The old plan was a queue file, labels and mail. The plan the machine reads is the fixture's own
    `owner` column (who the plan should hold the job for at that step), plus the rulings in RULINGS, each of
    which is an act the integrator really made at that event (closing a replaced PR, calling a branch a record).
  * telling. There was no versioned notice: a mail or switchboard notice that names a job to its plan owner is
    read as the notice of that owner's order, and the owner's acknowledging mail as its receipt.
  * reviews. A Codex finding is a FIX on the commit it was raised on (the Codex state comment, as
    rules.codex_state_verdict reads it); a new head is a new commit, to be proven and reviewed again.
Where the fixture's judgment and the machine's definitions differ, the difference is one of the SAME rules below
(a definition, stated once) or a line in EXPLAINED (a single step, with its reason). Anything else fails.
"""
import datetime as dt
import json
import pathlib
import re
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import machine as M  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402

FIXTURES = pathlib.Path(__file__).resolve().parent / "fixtures" / "jobs"
TEAM = rules.Team({"claude": "anthropic", "muse": "meta", "codex": "openai"},
                  [("claude/", "anthropic"), ("fb-bot/", "meta")], {}, ["muse"], "", {"muse": "meta", "codex": "openai"},
                  frozenset({"issisx", "github-actions[bot]"}))
LIMITS = {"claude": 4, "muse": 6}   # work/plan.md's own
ACK = re.compile(r"\back\b|\backnowledg|\bre-ack\b", re.IGNORECASE)

# The integrator's rulings the old system kept in mail or made by closing a PR: (case, from event, job, kept head).
RULINGS = [
    # #372 was closed as replaced by #469 (event 47): Muse's head on it is a record from then on
    ("j038-built-twice", 47, "J-038", ("fb-bot/integrate-protected-j038-slice1-r3", "3e249d09")),
    # muse.md "Not yours ... J-109" (event 30): the handover push fb-bot/integrate-j109 is a record, not a lane
    ("j109-built-twice", 30, "J-109", ("fb-bot/integrate-j109", "7791c2a8")),
]
# Lanes the fixture names as running ahead of its jobs (jobs.context): they fill Muse's limit.
CONTEXT = {"j119-j124-j125-start-notices": ["J-116", "J-117", "J-118", "J-133", "J-134", "J-043"]}


def full(sha: str) -> str:
    """A 40-character sha for a fixture's short one (the fixtures keep 8 characters; the prefix is what matters)."""
    return sha[:8] + "0" * 32


def when(stamp: str) -> dt.datetime:
    return M.when(stamp)


class Replay:
    """One fixture, replayed event by event into the reads the machine takes."""

    def __init__(self, case: str):
        self.case = case
        self.data = json.loads((FIXTURES / f"{case}.json").read_text(encoding="utf-8"))
        self.jobs = {j: v for j, v in self.data["jobs"].items() if j.startswith("J-")}
        self.expected: dict[int, list[dict]] = {}
        for x in self.data["expected"]:
            self.expected.setdefault(x["at_i"], []).append(x)
        opened = {e["refs"]["issue"]: e["i"] for e in self.data["events"] if e["kind"] == "issue_opened"}
        self.born = {j: opened.get(v["issue"], 0) for j, v in self.jobs.items()}
        self.prs = {n for v in self.jobs.values() for n in v.get("prs", [])}
        self.undrafted = {e["refs"]["issue"] for e in self.data["events"] if e["kind"] == "ready_for_review"}
        self.merged = {e["refs"]["issue"] for e in self.data["events"] if e["kind"] == "merged"}
        self.codex_dated = {full(e["refs"].get("sha") or re.search(r"on ([0-9a-f]{7,40})", e.get("text") or "").group(1))
                            for e in self.data["events"] if e["kind"] == "codex_review"}
        self.needs = {j: frozenset(lab.split(":", 1)[1] for e in self.data["events"]
                                   if e["kind"] == "issue_opened" and e["refs"]["issue"] == v["issue"]
                                   for lab in e.get("labels_now") or [] if lab.startswith("needs:"))
                      for j, v in self.jobs.items()}
        pushed = {e["refs"]["branch"] for e in self.data["events"] if e["kind"] == "push"}
        absent = {p["what"].split()[-1] for p in self.data["probes"]
                  if p.get("what", "").startswith("remote ref ") and str(p.get("result", "")).startswith("absent")}
        self.standing = [b for v in self.jobs.values() for b in v.get("branches", []) if b not in pushed | absent]
        self.missing_shas = {m.group(1) for p in self.data["probes"]
                             if (m := re.match(r"GET commits/([0-9a-f]{7,40})", p.get("what", "")))
                             and re.match(r"4(?:04|22)", str(p.get("result", "")))}
        self.rows: dict[int, dict[str, M.Step]] = {}
        self.run()

    def owner(self, job: str, i: int) -> str:
        entries = [x for x in self.data["expected"] if x["job"] == job]
        past = [x for x in entries if x["at_i"] <= i]
        return (past[-1] if past else entries[0])["owner"]

    def model(self, i: int) -> plan.Plan:
        lines = [f"limit {o} {n}" for o, n in LIMITS.items()]
        for k, job in enumerate(CONTEXT.get(self.case, [])):
            lines.append(f"{job} muse #{9000 + k} | a lane ahead")
        for job, v in self.jobs.items():
            if self.born[job] <= i and any(x["job"] == job for x in self.data["expected"]):
                kept = "".join(f" kept {ref}@{sha}" for case, at, j, (ref, sha) in RULINGS
                               if case == self.case and j == job and at <= i)
                lines.append(f"{job} {self.owner(job, i)} #{v['issue']}{kept} | {job}")
        model = plan.parse("\n".join(lines) + "\n")
        assert not model.errors, model.errors
        return model

    def run(self) -> None:
        heads: dict[str, tuple[str, dt.datetime]] = {}      # branch -> (sha, pushed at)
        proofs: dict[str, tuple[str, dt.datetime]] = {}     # sha -> (proof, at)
        open_prs: dict[int, dict] = {}
        verdicts: dict[int, list[rules.Verdict]] = {}
        woken: dict[int, dict[str, str]] = {}               # PR -> head sha -> when a person's login asked Codex
        found: set[int] = set()                             # PRs Codex raised a finding thread on
        closed_issues: set[int] = set()
        inactive: set[str] = set()
        merged_at: dict[str, str] = {}
        posts: list[M.Post] = []
        notices: list[tuple[str, str, dt.datetime]] = []
        edits: dict[str, int] = {}                          # job -> how often the old queue file moved it
        unmergeable: set[str] = set()
        for k, ref in enumerate(self.standing):   # a branch already on the remote when the fixture begins
            heads[ref] = (full(f"5eed{k:04d}"), when(self.data["events"][0]["t"]))
        for k, job in enumerate(CONTEXT.get(self.case, [])):
            heads[f"fb-bot/integrate-{job.lower().replace('-', '')}"] = (full(f"c0{k}"), when(self.data["events"][0]["t"]))
        for e in self.data["events"]:
            i, t, kind, refs = e["i"], when(e["t"]), e["kind"], e.get("refs") or {}
            number = refs.get("issue") or refs.get("pr") or 0
            if kind == "push":
                heads[refs["branch"]] = (full(refs["sha"]), t)
                proofs.setdefault(full(refs["sha"]), ("running", t))
            elif kind == "proof":
                word = {"success": "green", "failure": "red: proof"}.get(e.get("result"), "running")
                proofs[full(refs["sha"])] = (word, t)
            elif kind == "pr_opened":
                open_prs[refs["pr"]] = {"number": refs["pr"], "state": "open", "body": e.get("body_first_line") or "",
                                        "head": {"ref": refs["branch"]}, "base": refs.get("base") or rules.TRUNK,
                                        "job": e["job"][0]}
            elif kind in ("merged", "closed") and number in open_prs and number in self.merged:
                pr = open_prs.pop(number)
                inactive.add(heads[pr["head"]["ref"]][0])
                merged_at[pr["job"]] = e["t"]
            elif kind == "closed":
                open_prs.pop(number, None)
                if number not in self.prs:
                    closed_issues.add(number)
            elif kind == "plan_edit":
                for job in e["job"]:
                    edits[job] = edits.get(job, 0) + 1
            elif kind == "notice_to_muse" and " CONFLICT on #" in (e.get("subject") or ""):
                unmergeable.add(full(re.search(r"@ ([0-9a-f]{7,40})", e["subject"]).group(1)))
            elif kind == "notice_to_muse" and (m := re.search(r" FIX on #(\d+) @ ([0-9a-f]{7,40})", e.get("subject") or "")):
                # The switch's own FIX notice: an earlier finding thread was still open, so the Codex state comment
                # held this head too (a clean receipt does not close a thread). The fixtures carry no event for it.
                n, sha = int(m.group(1)), full(m.group(2))
                if n in found and not any(v.verdict == "FIX" and v.sha == sha for v in verdicts.get(n, [])):
                    verdicts.setdefault(n, []).append(rules.Verdict(
                        "openai", "Codex", "review", "FIX", sha, e["t"], rules.CODEX_LOGIN, False))
            elif kind == "codex_review" and number:
                sha = refs.get("sha") or re.search(r"on ([0-9a-f]{7,40})", e.get("text") or "").group(1)
                clean = e.get("verdict") == "LAND"
                verdicts.setdefault(number, []).append(rules.Verdict(
                    "openai", "Codex", "review", "LAND" if clean else "FIX", full(sha), e["t"], rules.CODEX_LOGIN, False,
                    receipt=clean))
            elif kind == "codex_finding":
                found.add(refs["pr"])
                verdicts.setdefault(refs["pr"], []).append(rules.Verdict(   # the state comment's FIX on that commit
                    "openai", "Codex", "review", "FIX", full(refs["sha"]), e["t"], rules.CODEX_LOGIN, False))
            elif kind == "codex_state" and not (e.get("codex") or {}).get("has_unresolved_findings"):
                # The Codex app edits this one comment in place: its time is the first review's, its words the
                # last one's. It is a review of its commit only if that commit was the head then and no dated
                # review event names it.
                sha = full(e["codex"]["commit_sha"])
                if sha not in self.codex_dated and any(h == sha for h, _ in heads.values()):
                    verdicts.setdefault(number, []).append(rules.Verdict(
                        "openai", "Codex", "review", "LAND", sha, e["t"], rules.CODEX_LOGIN, False))
            elif kind == "verdict" and number:
                v = e["verdict"]
                verdicts.setdefault(number, []).append(rules.Verdict(
                    "anthropic", "Claude", "eye" if v["by"] == "claude-eye" else "review", v["word"], full(v["sha"]),
                    e["t"], rules.BOT_LOGIN, False, signed=True))
            elif kind == "codex_ask" and e["actor"].lower() in TEAM.verdict_logins - {rules.BOT_LOGIN.lower()}:
                # a person's login asked (the switch with the team token, or the integrator by hand): Codex is woken
                m = re.search(r"head=([0-9a-f]{40})", e.get("text") or "")
                woken.setdefault(number, {})[full(m.group(1)) if m else heads[open_prs[number]["head"]["ref"]][0]] = e["t"]
            elif kind in ("mail_out", "mail_in"):
                body = "\n".join([e.get("text") or ""] + list(e.get("lines") or []))
                posts.append(M.Post(i, number, t, e.get("subject") or "", body, kind == "mail_out"))
            model = self.model(i)
            issues = [{"number": line.issue, "title": f"{line.job} job", "state": "open",
                       "body": f"queue edit {edits.get(line.job, 0)}"} for line in model.lines
                      if line.issue not in closed_issues]
            vers = M.versions(model)
            told = [j for j in e.get("job") or [] if j in model.by_job() and model.by_job()[j].owner == "muse"]
            if kind in ("mail_out", "notice_to_muse") and told and ("muse", vers["muse"]) not in {n[:2] for n in notices}:
                notices.append(("muse", vers["muse"], t))
            if kind == "mail_in" and told and (e.get("acks") or ACK.search(e.get("subject") or "")):
                posts.append(M.Post(-i, number, t, f"plan {vers['muse']}", "", False))
            works = {}
            for n, pr in open_prs.items():
                ref = pr["head"]["ref"]
                if ref not in heads:
                    continue
                sha, pushed = heads[ref]
                pr["head"]["sha"] = sha
                proof, proof_at = proofs.get(sha, ("none", pushed))
                seen = list(verdicts.get(n, []))
                f = rules.PrFacts(number=n, head_sha=sha, head_ref=ref, base_ref=pr["base"],
                                  draft=n in self.undrafted and not any(
                                      x["kind"] == "ready_for_review" and x["refs"]["issue"] == n and x["i"] <= i
                                      for x in self.data["events"]),
                                  mergeable=sha not in unmergeable, head_committed_at=pushed.strftime("%Y-%m-%dT%H:%M:%SZ"), proof=proof,
                                  verdicts=tuple(seen), needs=self.needs.get(pr["job"], frozenset()),
                                  rule_check_block=False,
                                  touches_workflows=self.case.startswith("j079"))
                d = rules.merge_decision(f, TEAM.vendor_of_branch(ref), TEAM, True, True)
                works[sha] = M.work_of(f, d, TEAM, proof_at=proof_at, codex_woken=woken.get(n, {}).get(sha, ""))
            pr_refs = {pr["head"]["ref"] for pr in open_prs.values()}
            for ref, (sha, pushed) in heads.items():
                if ref not in pr_refs and sha in proofs:
                    works[sha] = M.branch_work(ref, sha, proofs[sha][0], proofs[sha][1], ref.startswith("fb-bot/integrate-"))
            by_thread = {line.issue: line.job for line in model.lines}
            mail = M.Mail(tuple(posts), tuple(notices))
            missing = {job: next(s for s in shas if any(s.startswith(m) or m.startswith(s) for m in self.missing_shas))
                       for job, (_, shas) in M.named_shas(mail, by_thread).items()
                       if any(s.startswith(m) or m.startswith(s) for s in shas for m in self.missing_shas)}
            mail = M.Mail(tuple(posts), tuple(notices), missing)
            reads = {"issues": issues,
                     "branches": [{"name": ref, "commit": {"sha": sha}} for ref, (sha, _) in heads.items()],
                     "prs": list(open_prs.values()), "inactive": frozenset(inactive), "merged_at": dict(merged_at),
                     "head_at": {sha: at for sha, at in heads.values()},
                     "closed": frozenset(line.job for line in model.lines if line.issue in closed_issues)}
            self.rows[i] = {r.job: r for r in M.table(model, M.collect(model, reads, works=works, mail=mail))}


def same(x: dict, row: M.Step) -> bool:
    """Is the machine's row the state the fixture expects? The definitions that differ, each stated once:
      1. Claude writes the plan, so its own jobs need no notice: planned and told read `acknowledged`.
      2. `started` is a branch on the remote. A start that is only a mail, or work on a commit only a mail
         names, keeps its state and owes a push (gates `claimed`, `unpushed`); one mailed while every lane is
         taken is told to park it (gate `no-lane`, on no clock): the fixture calls each of these started.
      3. `reviewed` is a LAND with no FIX open. A head whose review said FIX is `proven` (or `pushed`) with the
         fix owed by its owner (gates `fix`, `stacked`; `unpushed` when the fix is a commit only a mail names):
         the fixture calls that reviewed with fix_open.
      4. A PR that conflicts with its base is the owner's step on its head (gate `base`), not the plan's
         conflict: the fixture marks it conflict with the merge_conflict flag.
      5. A PR whose LAND counts and that waits only for one more required review (the Codex app's on a workflow
         PR, the security pass, the eye) is `reviewed`, and its step names that review and who owes it (for
         Codex: the integrator's `@codex review`). The fixture calls it proven, with the review owed.
    """
    want, flags = x["state"], x.get("flags") or []
    if want == row.state:
        return True
    if want in ("planned", "told") and x["owner"] in M.SELF_TOLD:
        return row.state == "acknowledged"
    if want == "started":
        return row.gate in ("claimed", "unpushed", "no-lane")
    if want == "reviewed" and (x.get("next") or {}).get("who") == x["owner"]:
        return row.gate in ("fix", "stacked", "unpushed") and row.owes == x["owner"]
    if want == "conflict" and "merge_conflict" in flags:
        return row.gate == "base"
    if want == "proven" and row.state == "reviewed":
        return row.gate in ("codex", "asked", "security", "eye")
    return False


# Single steps where the machine reads the evidence differently from the fixture's judgment, and why.
ASK = ("a question to the integrator in a mail or a post is an ask: the ledger holds it and its deadline "
       "(ledger.AskIn), and the job's own step stays what GitHub shows")
RIDES = ("J-109's work was built on J-015's branch and #467 (`Job: J-015`): no head and no PR names J-109, so the "
         "machine sees no start. The plan has no way to say one job rides another's branch; the owner (claude) "
         "is the same")
EXPLAINED: dict[tuple[str, int, str], str] = {
    ("j038-built-twice", 44, "J-038"): "#372, Muse's head for the same job, is still open until event 47: two "
                                       "agents' unlanded heads for one job is the conflict the case is about",
    ("j038-built-twice", 55, "J-038"): ASK,
    ("j059-343-stacked-deadlock-scope", 4, "J-059"): ASK,
    ("j059-343-stacked-deadlock-scope", 48, "J-059"): "the state agrees (a FIX is open, the fix is a commit only "
                                                      "a mail names). A mail the reader could not read is an ask "
                                                      "to resend: " + ASK,
    ("j059-343-stacked-deadlock-scope", 50, "J-059"): "the hold is two mails that disagree; the plan has no hold on "
                                                      "J-059, and a hold that is not in the plan is not one",
    ("j079-473-unasked-codex", 12, "J-079"): "a list of every lane's status is not the words of a start (`J-079 "
                                             "started`); the next step is Muse's either way",
    ("j106-local-head-not-trunk", 1, "J-063"): "fb-bot/integrate-j063-torso-drive is on the remote (checked on "
                                               "11-12), so J-063 is pushed, not started",
    ("j106-local-head-not-trunk", 2, "J-063"): "as at event 1",
    ("j106-local-head-not-trunk", 20, "J-106"): "Muse mailed a start on J-106 and nothing has answered it yet: the "
                                                "plan now gives the job to claude, so it is a conflict until the "
                                                "transfer mail at event 22",
    ("j106-local-head-not-trunk", 23, "J-106"): "a post that says nothing is left to do closes nothing: the job is "
                                                "closed when its issue is (event 25)",
    ("j109-built-twice", 30, "J-109"): RIDES,
    ("j109-built-twice", 31, "J-109"): RIDES,
    ("j109-built-twice", 36, "J-109"): RIDES,
    ("j109-built-twice", 37, "J-109"): RIDES,
    ("j109-built-twice", 38, "J-109"): RIDES,
    ("j118-j137-split", 17, "J-118"): ASK,
}
# Grok #471 hole 5: the order is work/plan.md alone. An edit of the old queue file that moved no plan line (the
# fixture's plan_edit, which this replay once read as a new version through the issue's text) is no new order.
QUEUE_EDIT = ("an edit of the old queue file that changes no plan line is no new order (the version is work/plan.md "
              "alone, never an issue's text): the notice of event 13 still stands, so the job is told")
EXPLAINED.update({("j119-j124-j125-start-notices", i, job): QUEUE_EDIT
                  for i, job in ((14, "J-119"), (15, "J-124"), (16, "J-125"))})

CASES = sorted(p.stem for p in FIXTURES.glob("*.json"))
REPLAYS = {case: Replay(case) for case in CASES}


class Fixtures(unittest.TestCase):
    def test_all_eight_are_here(self):
        self.assertEqual(len(CASES), 8)

    def test_every_expected_step_is_the_machines_state_or_is_explained(self):
        wrong, stale = [], []
        for case, r in REPLAYS.items():
            for i, entries in sorted(r.expected.items()):
                for x in entries:
                    row = r.rows[i].get(x["job"])
                    ok = row is not None and same(x, row)
                    key = (case, i, x["job"])
                    if ok and key in EXPLAINED:
                        stale.append(key)
                    if not ok and key not in EXPLAINED:
                        got = f"{row.state}/{row.gate or '-'} owes {row.owes}: {row.why[:90]}" if row else "no row"
                        wrong.append(f"{case} @{i} {x['job']}: expected {x['state']} "
                                     f"(next {x['next']['who']}, flags {x.get('flags')}); machine {got}")
        self.assertEqual(wrong, [], "\n" + "\n".join(wrong))
        self.assertEqual(stale, [], "explained steps that now agree: take them out of EXPLAINED")


def states(case: str, job: str) -> list[str]:
    """The job's states in the order the machine showed them, each once per run of the same state."""
    out: list[str] = []
    for i in sorted(REPLAYS[case].rows):
        row = REPLAYS[case].rows[i].get(job)
        if row and (not out or out[-1] != row.state):
            out.append(row.state)
    return out


class Cases(unittest.TestCase):
    """Each case's own failure, at the event where the evidence first shows it."""

    def row(self, case: str, i: int, job: str) -> M.Step:
        return REPLAYS[case].rows[i][job]

    def at(self, case: str, i: int) -> dt.datetime:
        return when(REPLAYS[case].data["events"][i - 1]["t"])

    def test_j109_a_job_taken_by_mail_alone_is_a_conflict_at_the_mail_not_after_the_second_build(self):
        case = "j109-built-twice"
        for job in ("J-109", "J-015"):   # event 10: "taking J-015 + J-109 ... stop those lanes", and the plan says muse
            r = self.row(case, 10, job)
            self.assertEqual((r.state, r.owes, r.limit), ("conflict", "integrator", M.NOW))
            self.assertIn("taking J-015 + J-109", r.why)
            self.assertTrue(r.late(self.at(case, 10)))
        # it stays one through Muse's second build (pushed at event 17) and the mails that only mention the job,
        # until the plan itself moves the job (the old system first showed it at event 28)
        self.assertEqual(REPLAYS[case].data["events"][16]["refs"]["branch"], "fb-bot/integrate-j109")
        self.assertTrue(all(self.row(case, i, "J-109").state == "conflict" for i in range(10, 30)))
        r = self.row(case, 28, "J-109")   # the plan gives it to claude; Muse's head is still unlanded work
        self.assertIn("fb-bot/integrate-j109", r.why)
        for i in range(30, 39):           # the head is ruled a record (kept): nothing is owed by Muse on it again
            r = self.row(case, i, "J-109")
            self.assertEqual((r.state, r.owes), ("acknowledged", "claude"), i)

    def test_j038_two_heads_for_one_job_is_a_conflict_until_one_is_ruled_a_record(self):
        case = "j038-built-twice"
        r = self.row(case, 28, "J-038")   # the mail that took the job, with the plan still Muse's
        self.assertEqual((r.state, r.owes), ("conflict", "integrator"))
        self.assertIn("taking J-038", r.why)
        for i in range(38, 47):           # claude's head and Muse's #372 are both live
            r = self.row(case, i, "J-038")
            self.assertEqual(r.state, "conflict", i)
            self.assertIn("fb-bot/integrate-protected-j038-slice1-r3", r.why)
        self.assertEqual(self.row(case, 47, "J-038").state, "proven")   # #372 closed, its head kept as a record
        self.assertTrue(all(self.row(case, i, "J-038").owes != "muse" for i in range(47, 74)))
        self.assertEqual(states(case, "J-038")[-4:], ["pushed", "proven", "reviewed", "landed"])

    def test_j118_a_split_made_in_the_plan_needs_no_mail_and_makes_no_conflict(self):
        case = "j118-j137-split"
        r = self.row(case, 18, "J-137")   # the issue and the plan line are born together
        self.assertEqual((r.state, r.owes), ("acknowledged", "claude"))
        self.assertIn("claude/wip-j137", r.next)
        self.assertNotIn("conflict", states(case, "J-118") + states(case, "J-137"))
        self.assertEqual(self.row(case, 18, "J-118").state, "pushed")   # the job it was split from keeps its lane

    def test_j059_a_fix_that_is_only_in_a_mail_is_the_owners_push_and_the_pr_keeps_its_state(self):
        case = "j059-343-stacked-deadlock-scope"
        r = self.row(case, 30, "J-059")
        self.assertEqual((r.state, r.gate, r.owes), ("proven", "fix", "muse"))
        r = self.row(case, 36, "J-059")   # "protected proof passed @ ad9fc702": GET commits/ad9fc702 is 422
        self.assertEqual((r.state, r.gate, r.owes, r.limit), ("proven", "unpushed", "muse", M.NOW))
        self.assertIn("push ad9fc702 to GitHub", r.next)
        self.assertTrue(r.late(self.at(case, 36)))
        for i in range(36, 54):           # a day of mails; nothing but a push changes the step
            self.assertEqual(self.row(case, i, "J-059").gate, "unpushed", i)
        self.assertEqual(self.row(case, 54, "J-059").gate, "running")   # a real head: the claim is history
        r = self.row(case, 56, "J-059")
        self.assertEqual((r.gate, r.owes), ("base", "muse"))
        self.assertIn("merge trunk into fb-bot/integrate-j059-injury-feed", r.next)

    def test_j106_a_start_that_is_only_a_mail_owes_a_push_and_a_start_on_anothers_job_is_a_conflict(self):
        case = "j106-local-head-not-trunk"
        r = self.row(case, 8, "J-106")
        self.assertEqual((r.gate, r.owes, r.limit), ("claimed", "muse", M.hours(2)))
        self.assertIn("fb-bot/wip-j106", r.next)
        r = self.row(case, 16, "J-106")
        self.assertEqual((r.state, r.owes), ("conflict", "integrator"))
        self.assertIn("muse mailed that it started; the plan gives it to claude", r.why)
        self.assertEqual(self.row(case, 22, "J-106").state, "acknowledged")   # the transfer mail answers the start
        self.assertEqual(self.row(case, 25, "J-106").state, "closed")
        # the local "baseline" f5ee22d6b itself: test_mail_reader reads it from this timeline (GitHub's 422)

    def test_j079_the_codex_review_nobody_asked_for_is_the_integrators_step_said_plainly(self):
        case = "j079-473-unasked-codex"
        for i in (20, 21):                # 21 is the inbox line that said only MERGE-BY-HAND
            r = self.row(case, i, "J-079")
            self.assertEqual((r.state, r.gate, r.owes, r.limit), ("reviewed", "codex", "integrator", M.hours(1)))
            self.assertEqual(r.next, "post `@codex review` on #473 (only a person's login wakes Codex)")
        late = self.row(case, 21, "J-079").alarm(self.at(case, 20) + M.hours(2))
        self.assertIn("post `@codex review` on #473", late)
        self.assertIn("belongs to integrator", late)
        r = self.row(case, 22, "J-079")   # asked by hand: the step is Codex's, and nobody is told to ask again
        self.assertEqual((r.gate, r.owes), ("asked", "reviewer"))
        r = self.row(case, 24, "J-079")
        self.assertEqual((r.state, r.gate, r.owes), ("proven", "fix", "muse"))

    def test_j119_a_job_queued_behind_full_lanes_is_never_owed_a_start(self):
        case = "j119-j124-j125-start-notices"
        for i, job in ((21, "J-119"), (23, "J-124"), (25, "J-125")):   # the three start notices the old system sent
            r = self.row(case, i, job)
            self.assertEqual(r.state, "told")
            self.assertNotIn("start", r.next)
            self.assertIn("answer the notice", r.next)
        for job in ("J-119", "J-124", "J-125"):
            r = self.row(case, 30, job)
            self.assertEqual((r.state, r.gate, r.limit), ("acknowledged", "queued", None))
            self.assertIn("6 of 6 lanes are in flight", r.next)
            self.assertFalse(r.late(self.at(case, 30) + M.hours(24 * 30)))   # no clock: nothing to be late on

    def test_j126_the_clean_run_goes_through_every_state_in_order_with_no_conflict(self):
        self.assertEqual(states("j126-clean-planned-to-closed", "J-126"),
                         ["acknowledged", "pushed", "proven", "reviewed", "landed", "closed"])
        r = self.row("j126-clean-planned-to-closed", 18, "J-126")
        self.assertEqual((r.gate, r.owes), ("draft", "claude"))


if __name__ == "__main__":
    unittest.main()
