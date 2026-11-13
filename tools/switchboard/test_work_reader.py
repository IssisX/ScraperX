"""The work reader: every open PR the sweep steered becomes its Work, the stacked-fix compare and the ask-codex
mark are read as the contract says, integrate heads with no PR are proof-read newest first, and every bound is
kept. Real timelines from fixtures/jobs (#343 and #473) drive the PR side; no test touches the network."""
import dataclasses
import datetime as dt
import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import ghapi  # noqa: E402
import machine as M  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402
import work_reader as W  # noqa: E402

FIXTURES = pathlib.Path(__file__).resolve().parent / "fixtures" / "jobs"
REPO = "IssisX/ScraperX"
PLAN = ("limit claude 2\nlimit muse 3\n"
        "J-059 muse #307 | her black eye: the per-region injury feed\n"
        "J-079 muse #397 | her hold is a cinematic\n"
        "J-201 claude #201 | Claude's\n")
TEAM = rules.Team({"claude": "anthropic", "muse": "meta", "codex": "openai"},
                  [("claude/", "anthropic"), ("fb-bot/", "meta")], {}, ["muse"], "", {"muse": "meta", "codex": "openai"},
                  frozenset({"issisx", "github-actions[bot]"}))
MODEL = plan.parse(PLAN)
assert not MODEL.errors, MODEL.errors


def case(name: str) -> dict[str, dict]:
    """A fixture's events by id."""
    return {e["id"]: e for e in json.loads((FIXTURES / f"{name}.json").read_text(encoding="utf-8"))["events"]}


J059 = case("j059-343-stacked-deadlock-scope")
J079 = case("j079-473-unasked-codex")


def full(sha8: str) -> str:
    """A 40-character sha for a fixture's 8-character one (the fixtures keep sha8; the prefix is what matters)."""
    return sha8 + "0" * (40 - len(sha8))


def t(ev: dict) -> str:
    return ev["t"]


def at(stamp: str) -> dt.datetime:
    return M.when(stamp)


def br(name: str, sha: str) -> dict:
    return {"name": name, "commit": {"sha": sha}}


def verdict(vendor, who, word, sha, created, login="IssisX", kind="review", **kw) -> rules.Verdict:
    return rules.Verdict(vendor, who, kind, word, sha, created, login, False, **kw)


class FakeGh:
    repo = REPO

    def __init__(self, answers: dict | None = None):
        self.answers, self.calls = dict(answers or {}), []

    def get(self, path: str):
        self.calls.append(path)
        if path not in self.answers:
            raise AssertionError(f"unexpected GitHub read {path}")
        got = self.answers[path]
        if isinstance(got, Exception):
            raise got
        return got


class FakeSw:
    """What the reader uses of the Switch: team, gh (compares), out, and proof(sha)."""

    def __init__(self, compares: dict | None = None, proofs: dict | None = None):
        self.team, self.gh, self.lines = TEAM, FakeGh(compares), []
        self.proofs, self.proof_calls = dict(proofs or {}), []

    def out(self, text: str) -> None:
        self.lines.append(text)

    def proof(self, sha: str):
        self.proof_calls.append(sha)
        got = self.proofs.get(sha, ("none", "", 0, ""))
        if isinstance(got, Exception):
            raise got
        return got


def steered(f: rules.PrFacts, ctx: dict | None = None, token: bool = True, key: bool = True) -> switch.Steered:
    d = rules.merge_decision(f, TEAM.vendor_of_branch(f.head_ref), TEAM, token, key)
    return switch.Steered(f, {"comments": [], "proof_done_at": "", "job": "", **(ctx or {})}, d)


def compare(base: str, head: str) -> str:
    return f"repos/{REPO}/compare/{base}...{head}"


def row(job: str, work: M.Work) -> M.Step:
    ln = MODEL.by_job()[job]
    return M.step(M.Evidence(job, ln, (ln.issue,), works=(work,)))


# ---------- #343 (J-059): the stacked fix and the FIX deadlock ----------
FEED = J059["open343"]["refs"]["branch"]                       # fb-bot/integrate-j059-injury-feed
PROTECTED = "fb-bot/integrate-protected-j059-injury-save"      # the fixture's jobs.J-059.branches
B117 = full(J059["push43014238252"]["refs"]["sha"])            # b1177151, the head the deadlock sat on
AD9F = full("ad9fc702")                                        # c6796877236: "protected proof passed @ ad9fc702"


def facts_343_deadlock() -> rules.PrFacts:
    """#343 as the sweep read it at FIX-STUCK (c6826814806, 11-12 14:46): b1177151 green (run43014762134), the eye's
    LAND (c6796017258), Codex's clean receipt (c6796059343) beside its state comment's unresolved P1 thread
    4534667539 on the same head. The switch's own FIX-STUCK notice on b1177151 is its reading that a FIX is open."""
    clean = t(J059["c6796059343"])
    return rules.PrFacts(
        number=343, head_sha=B117, head_ref=FEED, base_ref=rules.TRUNK, draft=False, mergeable=True,
        head_committed_at=t(J059["kb1177151"]), proof="green",
        verdicts=(verdict("anthropic", "Claude", "LAND", B117[:8], t(J059["c6796017258"]), rules.BOT_LOGIN, "eye"),
                  verdict("openai", "Codex", "FIX", B117[:8], clean, rules.CODEX_LOGIN),
                  verdict("openai", "Codex", "LAND", B117[:10], clean, rules.CODEX_LOGIN, receipt=True)),
        needs=frozenset({"eyes"}), rule_check_block=False, touches_workflows=False)


def ctx_343() -> dict:
    return {"job": "J-059", "proof_done_at": t(J059["run43014762134"])}


class Stacked343(unittest.TestCase):
    def test_the_real_343_had_no_pushed_fix_branch_so_its_gate_is_the_plain_fix_and_nothing_is_compared(self):
        # probes: the protected branch was absent from the remote and GET commits/ad9fc702 said 422
        sw = FakeSw()
        works = W.read(sw, MODEL, {"branches": [br(FEED, B117)]}, [steered(facts_343_deadlock(), ctx_343())])
        w = works[B117]
        self.assertEqual((w.state, w.gate, w.why, w.pr), ("proven", "fix", "Codex (review)", 343))
        self.assertEqual(w.at, at(t(J059["c6796059343"])))
        self.assertEqual(sw.gh.calls, [])
        self.assertEqual(row("J-059", w).owes, "muse")

    def test_a_pushed_fix_branch_ahead_of_the_pr_head_is_the_deadlock_said_plainly(self):
        sw = FakeSw({compare(B117, AD9F): {"status": "ahead", "ahead_by": 1, "behind_by": 0}})
        works = W.read(sw, MODEL, {"branches": [br(FEED, B117), br(PROTECTED, AD9F)]},
                       [steered(facts_343_deadlock(), ctx_343())])
        w = works[B117]
        self.assertEqual((w.gate, w.why), ("stacked", PROTECTED))
        self.assertEqual(sw.gh.calls, [compare(B117, AD9F)])
        r = row("J-059", w)
        self.assertEqual((r.owes, r.limit), ("muse", M.hours(6)))
        self.assertEqual(r.next, f"push the fix onto {FEED}: it sits on {PROTECTED}, which waits for #343 to land")

    def test_a_branch_that_is_not_ahead_of_the_pr_head_is_not_its_fix(self):
        for status in ("diverged", "behind", "identical"):
            sw = FakeSw({compare(B117, AD9F): {"status": status}})
            w = W.read(sw, MODEL, {"branches": [br(FEED, B117), br(PROTECTED, AD9F)]},
                       [steered(facts_343_deadlock(), ctx_343())])[B117]
            self.assertEqual(w.gate, "fix", status)

    def test_only_a_live_branch_of_the_same_job_and_owner_is_compared(self):
        other = full("c0ffee11")
        branches = [br(FEED, B117), br("claude/integrate-j059-help", other),      # another owner's
                    br("fb-bot/integrate-j079", full("d982c18b")),                # another job's
                    br(PROTECTED, AD9F)]                                          # landed: in `inactive`
        sw = FakeSw()
        w = W.read(sw, MODEL, {"branches": branches, "inactive": frozenset({AD9F})},
                   [steered(facts_343_deadlock(), ctx_343())])[B117]
        self.assertEqual((w.gate, sw.gh.calls), ("fix", []))

    def test_no_compare_without_an_open_fix_or_off_trunk(self):
        # run43278562443: 8df92be5 red at scope; Codex's FIX named b1177151, not this head
        h = full(J059["push43278477845"]["refs"]["sha"])
        red = rules.PrFacts(number=343, head_sha=h, head_ref=FEED, base_ref=rules.TRUNK, draft=False, mergeable=True,
                            head_committed_at=t(J059["k8df92be5"]), proof="red: scope",
                            verdicts=facts_343_deadlock().verdicts, needs=frozenset({"eyes"}),
                            rule_check_block=False, touches_workflows=False)
        sw = FakeSw()
        branches = [br(FEED, h), br(PROTECTED, AD9F)]
        w = W.read(sw, MODEL, {"branches": branches}, [steered(red, {"job": "J-059", "proof_done_at": t(J059["run43278562443"])})])[h]
        self.assertEqual((w.state, w.gate, w.why, w.at), ("pushed", "red", "scope", at(t(J059["run43278562443"]))))
        stacked_base = dataclasses.replace(facts_343_deadlock(), base_ref=FEED + "-base")
        w = W.read(sw, MODEL, {"branches": branches}, [steered(stacked_base, ctx_343())])[B117]
        self.assertEqual(w.gate, "off-trunk")
        self.assertEqual(sw.gh.calls, [])

    def test_a_compare_that_fails_leaves_the_plain_fix_and_says_so(self):
        sw = FakeSw({compare(B117, AD9F): ghapi.GitHubError("GET", "compare", "502 Bad Gateway", 502)})
        w = W.read(sw, MODEL, {"branches": [br(FEED, B117), br(PROTECTED, AD9F)]},
                   [steered(facts_343_deadlock(), ctx_343())])[B117]
        self.assertEqual(w.gate, "fix")
        self.assertTrue(any("compare" in x and PROTECTED in x for x in sw.lines), sw.lines)


# ---------- #473 (J-079): the Codex review nobody asked for ----------
J079_REF = J079["open473"]["refs"]["branch"]                     # fb-bot/integrate-j079
D982 = J079["c6832651575"]["codex"]["commit_sha"]                # d982c18b..., the whole head Codex reviewed


def comment(ev: dict, body: str | None = None, login: str | None = None) -> dict:
    return {"id": int(ev["id"].lstrip("c")), "created_at": ev["t"], "body": ev.get("subject", "") if body is None else body,
            "user": {"login": ev["actor"] if login is None else login}}


def facts_473(verdicts=()) -> rules.PrFacts:
    """#473 at open473: d982c18b green (run43268830347), .github/workflows changed (probe), needs:eyes."""
    return rules.PrFacts(number=473, head_sha=D982, head_ref=J079_REF, base_ref=rules.TRUNK, draft=False,
                         mergeable=True, head_committed_at=t(J079["kd982c18b"]), proof="green", verdicts=tuple(verdicts),
                         needs=frozenset({"eyes"}), rule_check_block=False, touches_workflows=True)


def ctx_473(comments=()) -> dict:
    return {"job": "J-079", "proof_done_at": t(J079["run43268830347"]), "comments": list(comments)}


def ask_mark(head: str = D982) -> str:
    return "Asking Codex to review this head.\n\n" + switch.MARK.format(kind="ask-codex", pr=473, head=head)


class UnaskedCodex473(unittest.TestCase):
    def test_a_team_logins_own_codex_ask_after_the_head_commit_is_codex_woken(self):
        # c6832643514: the integrator's own `@codex review` on d982c18b, with no switchboard mark
        lands = (verdict("anthropic", "Claude", "LAND", D982, t(J079["c6832452993"]), rules.BOT_LOGIN, signed=True),
                 verdict("anthropic", "Claude", "LAND", D982[:8], t(J079["c6832471420"]), rules.BOT_LOGIN, "eye"))
        asked = comment(J079["c6832643514"])
        self.assertTrue(asked["body"].lower().startswith("@codex review"))
        s = steered(facts_473(lands), ctx_473([comment(J079["c6832452993"]), comment(J079["c6832471420"]), asked]))
        self.assertEqual(W.codex_woken(FakeSw(), s.facts, s.ctx), asked["created_at"])
        # the same words from a login off the team, or from the workflow bot, wake nobody
        for login in ("someone-else", rules.BOT_LOGIN):
            other = ctx_473([comment(J079["c6832643514"], login=login)])
            self.assertEqual(W.codex_woken(FakeSw(), s.facts, other), "", login)
        # and an ask from before the head commit was for another head
        early = dict(asked, created_at="2026-11-12T00:00:00Z")
        self.assertEqual(W.codex_woken(FakeSw(), s.facts, ctx_473([early])), "")

    def test_green_with_no_review_and_no_mark_is_owed_by_the_reviewer(self):
        w = W.read(FakeSw(), MODEL, {}, [steered(facts_473(), ctx_473())])[D982]
        self.assertEqual((w.state, w.gate, w.at), ("proven", "", at(t(J079["run43268830347"]))))

    def test_the_switchs_own_mark_on_this_head_while_it_needs_review_is_the_integrators_step(self):
        bot = {"id": 1, "created_at": t(J079["open473"]), "body": ask_mark(), "user": {"login": rules.BOT_LOGIN}}
        w = W.read(FakeSw(), MODEL, {}, [steered(facts_473(), ctx_473([bot]))])[D982]
        self.assertEqual((w.state, w.gate), ("proven", "codex"))

    def test_a_mark_by_anyone_else_or_for_another_head_is_not_the_switchs_ask(self):
        when = t(J079["open473"])
        for c in ({"id": 1, "created_at": when, "body": ask_mark(), "user": {"login": "someone-else"}},
                  {"id": 2, "created_at": when, "body": ask_mark(), "user": {"login": "IssisX"}},
                  {"id": 3, "created_at": when, "body": ask_mark(full("0b2f39aa")), "user": {"login": rules.BOT_LOGIN}}):
            w = W.read(FakeSw(), MODEL, {}, [steered(facts_473(), ctx_473([c]))])[D982]
            self.assertEqual(w.gate, "", c)

    def test_the_mark_counts_only_while_the_head_still_needs_a_review(self):
        bot = {"id": 1, "created_at": t(J079["open473"]), "body": ask_mark(), "user": {"login": rules.BOT_LOGIN}}
        answered = facts_473((verdict("unknown", "Somebody", "LAND", D982, t(J079["c6832452993"])),))
        self.assertFalse(rules.needs_review(answered, "meta", TEAM))
        w = W.read(FakeSw(), MODEL, {}, [steered(answered, ctx_473([bot]))])[D982]
        self.assertEqual(w.gate, "")

    def test_codexs_p0_on_the_head_is_the_owners_fix(self):
        fix = verdict("openai", "Codex", "FIX", D982, t(J079["c6832651575"]), rules.CODEX_LOGIN)
        bot = {"id": 1, "created_at": t(J079["open473"]), "body": ask_mark(), "user": {"login": rules.BOT_LOGIN}}
        w = W.read(FakeSw(), MODEL, {"branches": [br(J079_REF, D982)]}, [steered(facts_473((fix,)), ctx_473([bot]))])[D982]
        self.assertEqual((w.gate, w.why), ("fix", "Codex (review)"))
        self.assertEqual(row("J-079", w).owes, "muse")

    def test_before_the_pr_the_branch_head_is_read_by_its_proof(self):
        # push43268770324: pushed, its run still going; run43268830347: green 18:18, before open473 (18:21)
        pushed = t(J079["kd982c18b"])
        sw = FakeSw(proofs={D982: ("running", "u", 43268830347, "")})
        reads = {"branches": [br(J079_REF, D982)], "head_at": {D982: pushed}}
        w = W.read(sw, MODEL, reads, [])[D982]
        self.assertEqual((w.state, w.gate, w.at, w.pr), ("pushed", "running", at(pushed), 0))
        self.assertEqual(row("J-079", w).owes, "ci")
        sw = FakeSw(proofs={D982: ("green", "u", 43268830347, t(J079["run43268830347"]))})
        w = W.read(sw, MODEL, reads, [])[D982]
        self.assertEqual((w.state, w.gate, w.at), ("proven", "auto-pr", at(t(J079["run43268830347"]))))
        r = row("J-079", w)
        self.assertEqual((r.owes, r.next), ("switchboard", f"open the PR for {J079_REF}"))
        self.assertEqual(sw.proof_calls, [D982])


# ---------- the branch side and the bounds ----------
def sha(n: int) -> str:
    return f"{n:040x}"


def stamp(hours: float) -> str:
    return (dt.datetime(2026, 11, 12, 0, 0, tzinfo=dt.timezone.utc) + dt.timedelta(hours=hours)).strftime("%Y-%m-%dT%H:%M:%SZ")


def many_jobs(n: int, owner: str = "muse") -> plan.Plan:
    text = f"limit {owner} {n}\n" + "".join(f"J-{500 + i} {owner} #{500 + i} | job {i}\n" for i in range(n))
    model = plan.parse(text)
    assert not model.errors, model.errors
    return model


class BranchHeads(unittest.TestCase):
    def test_only_a_planned_jobs_own_integrate_head_with_no_open_pr_is_proof_read(self):
        reads = {
            "branches": [br("fb-bot/integrate-j059-injury-feed", sha(1)),   # read
                         br("fb-bot/wip-j059", sha(2)),                     # a wip head: started, no proof
                         br("claude/integrate-j059", sha(3)),               # another owner's
                         br("fb-bot/integrate-j999", sha(4)),               # no plan line
                         br("fb-bot/integrate-j079", sha(5)),               # an open PR's head (by sha)
                         br("fb-bot/integrate-protected-j079", sha(6)),     # an open PR's head (by ref)
                         br("claude/integrate-j201", sha(7)),               # Claude's, read; the switch opens its PR (11-13)
                         br("fb-bot/integrate-j059-old", sha(8))],          # landed
            "prs": [{"number": 1, "state": "open", "head": {"ref": "fb-bot/integrate-j079", "sha": sha(5)}},
                    {"number": 2, "state": "open", "head": {"ref": "fb-bot/integrate-protected-j079", "sha": sha(66)}}],
            "inactive": frozenset({sha(8)})}
        sw = FakeSw(proofs={sha(1): ("red: native", "u", 1, stamp(3)), sha(7): ("green", "u", 2, stamp(4))})
        works = W.read(sw, MODEL, reads, [])
        self.assertEqual(sorted(sw.proof_calls), [sha(1), sha(7)])
        self.assertEqual(set(works), {sha(1), sha(7)})
        self.assertEqual((works[sha(1)].gate, works[sha(1)].why, works[sha(1)].at), ("red", "native", at(stamp(3))))
        self.assertEqual(works[sha(7)].gate, "auto-pr")

    def test_a_steered_pr_head_is_never_read_again_by_the_branch_side(self):
        sw = FakeSw()
        works = W.read(sw, MODEL, {"branches": [br(FEED, B117)]}, [steered(facts_343_deadlock(), ctx_343())])
        self.assertEqual((sw.proof_calls, works[B117].pr), ([], 343))

    def test_a_head_with_no_proof_run_read_is_none_and_ci_owes_it(self):
        sw = FakeSw(proofs={sha(1): ("none", "", 0, "")})
        w = W.read(sw, MODEL, {"branches": [br("fb-bot/integrate-j059", sha(1))]}, [])[sha(1)]
        self.assertEqual((w.state, w.gate), ("pushed", "none"))
        self.assertEqual(row("J-059", w).owes, "ci")

    def test_at_most_proof_reads_heads_newest_first_and_the_rest_stay_unread_never_none(self):
        n = W.PROOF_READS + 5
        model = many_jobs(n)
        branches = [br(f"fb-bot/integrate-j{500 + i}", sha(100 + i)) for i in range(n)]
        head_at = {sha(100 + i): stamp(i) for i in range(n)}   # the later the job, the newer its head
        sw = FakeSw(proofs={sha(100 + i): ("running", "u", i, "") for i in range(n)})
        works = W.read(sw, model, {"branches": branches, "head_at": head_at}, [])
        self.assertEqual(len(sw.proof_calls), W.PROOF_READS)
        self.assertEqual(sw.proof_calls, [sha(100 + i) for i in reversed(range(5, n))])
        self.assertEqual(set(works), set(sw.proof_calls))
        self.assertTrue(any("5 integrate head(s)" in x for x in sw.lines), sw.lines)
        issues = [{"number": ln.issue, "title": f"{ln.job} x", "state": "open"} for ln in model.lines]
        ev = M.collect(model, {"issues": issues, "branches": branches, "head_at": head_at}, works=works)
        rows = {job: M.step(e) for job, e in ev.items()}
        unread = [f"J-{500 + i}" for i in range(5)]
        for job in unread:
            self.assertEqual((rows[job].state, rows[job].gate, rows[job].owes), ("pushed", "unread", "switchboard"), job)
        self.assertEqual({rows[f"J-{500 + i}"].gate for i in range(5, n)}, {"running"})

    def test_a_proof_read_that_fails_leaves_that_head_unread_and_a_rate_limit_stops_the_reads(self):
        model = many_jobs(4)
        branches = [br(f"fb-bot/integrate-j{500 + i}", sha(200 + i)) for i in range(4)]
        head_at = {sha(200 + i): stamp(10 - i) for i in range(4)}   # j500 newest
        sw = FakeSw(proofs={sha(200): ghapi.GitHubError("GET", "runs", "502", 502), sha(201): ("green", "u", 1, stamp(11)),
                            sha(202): ghapi.RateLimited("GET", "runs", "rate limit", 403), sha(203): ("green", "u", 2, "")})
        works = W.read(sw, model, {"branches": branches, "head_at": head_at}, [])
        self.assertEqual(sw.proof_calls, [sha(200), sha(201), sha(202)])
        self.assertEqual(set(works), {sha(201)})
        self.assertEqual(len([x for x in sw.lines if x.startswith("NOTE")]), 2, sw.lines)

    def test_at_most_stack_reads_compares_a_pass(self):
        n = W.STACK_READS + 2
        model = many_jobs(n)
        prs, branches, answers = [], [], {}
        for i in range(n):
            ref, head, fixed = f"fb-bot/integrate-j{500 + i}", sha(300 + i), sha(400 + i)
            f = rules.PrFacts(number=600 + i, head_sha=head, head_ref=ref, base_ref=rules.TRUNK, draft=False,
                              mergeable=True, head_committed_at=stamp(0), proof="green",
                              verdicts=(verdict("openai", "Codex", "FIX", head, stamp(1), rules.CODEX_LOGIN),),
                              needs=frozenset(), rule_check_block=False, touches_workflows=False)
            prs.append(steered(f, {"job": f"J-{500 + i}"}))
            branches += [br(ref, head), br(f"fb-bot/integrate-protected-j{500 + i}", fixed)]
            answers[compare(head, fixed)] = {"status": "ahead"}
        sw = FakeSw(answers)
        works = W.read(sw, model, {"branches": branches}, prs)
        self.assertEqual(len(sw.gh.calls), W.STACK_READS)
        gates = [works[sha(300 + i)].gate for i in range(n)]
        self.assertEqual(gates, ["stacked"] * W.STACK_READS + ["fix"] * 2)
        self.assertTrue(any("2 stacked-fix compare(s) not read" in x for x in sw.lines), sw.lines)

    def test_a_rate_limited_compare_stops_the_compares_for_the_pass(self):
        model = many_jobs(3)
        prs, branches, answers = [], [], {}
        for i in range(3):
            ref, head, fixed = f"fb-bot/integrate-j{500 + i}", sha(700 + i), sha(800 + i)
            f = rules.PrFacts(number=900 + i, head_sha=head, head_ref=ref, base_ref=rules.TRUNK, draft=False,
                              mergeable=True, head_committed_at=stamp(0), proof="green",
                              verdicts=(verdict("openai", "Codex", "FIX", head, stamp(1), rules.CODEX_LOGIN),),
                              needs=frozenset(), rule_check_block=False, touches_workflows=False)
            prs.append(steered(f, {"job": f"J-{500 + i}"}))
            branches += [br(ref, head), br(f"fb-bot/integrate-protected-j{500 + i}", fixed)]
            answers[compare(head, fixed)] = ghapi.RateLimited("GET", "compare", "rate limit", 403) if i == 0 else {"status": "ahead"}
        sw = FakeSw(answers)
        works = W.read(sw, model, {"branches": branches}, prs)
        self.assertEqual(len(sw.gh.calls), 1)
        self.assertEqual([works[sha(700 + i)].gate for i in range(3)], ["fix"] * 3)


class Contract(unittest.TestCase):
    def test_the_reader_names_the_switchs_own_marks_and_prefixes(self):
        self.assertEqual(W.MARK, switch.MARK)
        self.assertEqual(W.AUTO_PR_PREFIX, switch.AUTO_PR_PREFIX)
        self.assertEqual((W.PROOF_READS, W.STACK_READS), (30, 10))


if __name__ == "__main__":
    unittest.main()
