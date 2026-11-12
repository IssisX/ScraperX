"""The job machine against eight real timelines (fixtures/jobs): the evidence as it stood after each event goes in,
and the machine's state for the job must be the one the fixture expects at that step.

The fixtures were recorded under the old system, so three of its forms are translated into the machine's, here
and nowhere else:
  * the plan. The old plan was a queue file, labels and mail. The plan the machine reads is the fixture's own
    `owner` column (who the plan should hold the job for at that step), plus the rulings in RULINGS, each of
    which is an act the integrator really made at that event (closing a replaced PR, calling a branch a record).
  * telling. There was no versioned notice: a mail or switchboard notice that names a job to its plan owner is
    read as the notice of that owner's order, and the owner's acknowledging mail as its receipt.
  * reviews. A Codex finding thread stays an open FIX on the PR until Codex reports no unresolved finding.
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
    return sha + "0" * (40 - len(sha))


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
        threads: dict[int, set[str]] = {}                   # PR -> shas with an unresolved Codex finding
        asked: dict[int, set[str]] = {}
        closed_issues: set[int] = set()
        inactive: set[str] = set()
        merged_at: dict[str, str] = {}
        posts: list[M.Post] = []
        notices: list[tuple[str, str, dt.datetime]] = []
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
            elif kind == "merged" and number in open_prs:
                pr = open_prs.pop(number)
                inactive.add(heads[pr["head"]["ref"]][0])
                merged_at[pr["job"]] = e["t"]
            elif kind == "closed":
                open_prs.pop(number, None)
                if number not in self.prs:
                    closed_issues.add(number)
            elif kind == "codex_review" and number:
                sha = refs.get("sha") or re.search(r"on ([0-9a-f]{7,40})", e.get("text") or "").group(1)
                clean = e.get("verdict") == "LAND"
                verdicts.setdefault(number, []).append(rules.Verdict(
                    "openai", "Codex", "review", "LAND" if clean else "FIX", sha, e["t"], rules.CODEX_LOGIN, False,
                    receipt=clean))
            elif kind == "codex_finding":
                threads.setdefault(refs["pr"], set()).add(refs["sha"])
            elif kind == "codex_state" and not (e.get("codex") or {}).get("has_unresolved_findings"):
                threads.pop(number, None)
                verdicts.setdefault(number, []).append(rules.Verdict(
                    "openai", "Codex", "review", "LAND", e["codex"]["commit_sha"], e["t"], rules.CODEX_LOGIN, False))
            elif kind == "verdict" and number:
                v = e["verdict"]
                verdicts.setdefault(number, []).append(rules.Verdict(
                    "anthropic", "Claude", "eye" if v["by"] == "claude-eye" else "review", v["word"], v["sha"], e["t"],
                    rules.BOT_LOGIN, False))
            elif kind == "codex_ask" and e.get("asked_by") == "switchboard":
                asked.setdefault(number, set()).add(re.search(r"head=([0-9a-f]{40})", e.get("text") or "").group(1))
            elif kind in ("mail_out", "mail_in"):
                body = "\n".join([e.get("text") or ""] + list(e.get("lines") or []))
                posts.append(M.Post(i, number, t, e.get("subject") or "", body, kind == "mail_out"))
            model = self.model(i)
            issues = [{"number": line.issue, "title": f"{line.job} job", "state": "open"} for line in model.lines
                      if line.issue not in closed_issues]
            vers = M.versions(model, issues)
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
                if threads.get(n):   # an unresolved finding thread holds whatever head the PR is on
                    seen.append(rules.Verdict("openai", "Codex", "review", "FIX", sha, e["t"], rules.CODEX_LOGIN, False))
                f = rules.PrFacts(number=n, head_sha=sha, head_ref=ref, base_ref=pr["base"],
                                  draft=n in self.undrafted and not any(
                                      x["kind"] == "ready_for_review" and x["refs"]["issue"] == n and x["i"] <= i
                                      for x in self.data["events"]),
                                  mergeable=True, head_committed_at=pushed.strftime("%Y-%m-%dT%H:%M:%SZ"), proof=proof,
                                  verdicts=tuple(seen), needs=frozenset(), rule_check_block=False,
                                  touches_workflows=self.case.startswith("j079"))
                d = rules.merge_decision(f, TEAM.vendor_of_branch(ref), TEAM, True, True)
                works[sha] = M.work_of(f, d, TEAM, proof_at=proof_at, codex_asked=sha in asked.get(n, ()) and
                                       rules.needs_review(f, TEAM.vendor_of_branch(ref), TEAM))
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
      2. `started` is a branch on the remote. A start that is only a mail keeps its state and owes a push
         (gate `claimed`): the fixture calls that started.
      3. `reviewed` is a LAND with no FIX open. A head whose review said FIX is `proven` (or `pushed`) with the
         fix owed by its owner: the fixture calls that reviewed with fix_open.
      4. A PR that conflicts with its base is the owner's step on its head (gate `base`), not the plan's
         conflict: the fixture marks it conflict with the merge_conflict flag.
    """
    want, flags = x["state"], x.get("flags") or []
    if want == row.state:
        return True
    if want in ("planned", "told") and x["owner"] in M.SELF_TOLD:
        return row.state == "acknowledged"
    if want == "started":
        return row.gate == "claimed"
    if want == "reviewed" and (x.get("next") or {}).get("who") == x["owner"]:
        return row.gate in ("fix", "stacked") and row.owes == x["owner"]
    if want == "conflict" and "merge_conflict" in flags:
        return row.gate == "base"
    return False


# Single steps where the machine reads the evidence differently from the fixture's judgment, and why.
EXPLAINED: dict[tuple[str, int, str], str] = {}

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


if __name__ == "__main__":
    unittest.main()
