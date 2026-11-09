"""Unit tests for the switchboard's ledger (tools/switchboard/ledger.py): every open item, its holder and deadline.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock, no files: GitHub and the time are given.
"""

from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ledger as L  # noqa: E402
import asks  # noqa: E402  (ledger puts tools/board on the path)

AGENTS = {"agents": {
    "claude": {"names": ["Claude"], "branch_prefixes": ["claude/"], "owner_labels": ["owner:claude"], "wake": "api"},
    "chatgpt": {"names": ["ChatGPT"], "branch_prefixes": ["chatgpt/"], "owner_labels": ["owner:chatgpt"], "wake": "issue"},
    "grok": {"names": ["Grok", "PE", "Builder"], "branch_prefixes": ["grok/", "pe/", "builder/"],
             "owner_labels": ["owner:grok", "owner:pe", "owner:builder"], "wake": "issue"},
    "codex": {"names": ["Codex"], "branch_prefixes": ["codex/"], "owner_labels": [], "wake": "mention"},
    "muse": {"names": ["Muse", "FB"], "branch_prefixes": ["muse/", "fb-bot/"], "owner_labels": ["owner:muse"],
             "wake": "email"},
}}
ROSTER = L.Roster(AGENTS)
NOW = dt.datetime(2026, 11, 9, 18, 0, tzinfo=dt.timezone.utc)
H = dt.timedelta(hours=1)
SHA = {k: (k * 40)[:40] for k in "abcdef0123"}


def ago(hours: float) -> dt.datetime:
    return NOW - dt.timedelta(hours=hours)


def pr(n, ref, job="J-059", action="WAIT", reasons=(), proof="green", touched=1.0, needs_review=False,
       reviewer="", done=1.0, sha="a"):
    return L.PrIn(n, f"https://github.com/IssisX/ScraperX/pull/{n}", ref, SHA[sha], f"Job: {job}\n\nbody" if job else
                  "no job line here", action, tuple(reasons), proof, L.iso(ago(done)), L.iso(ago(touched + 1)),
                  needs_review, reviewer, ago(touched))


def run(i, branch, sha, hours, conclusion="failure", state=""):
    return L.RunIn(i, f"https://github.com/IssisX/ScraperX/actions/runs/{i}", branch, SHA[sha], ago(hours + 0.5),
                   ago(hours), conclusion, state or ("red: native" if conclusion == "failure" else "green"))


def feed(**kw):
    base = dict(prs=[], open_prs=[], branches=[], issues=[], asks=[], runs=[],
                system_branches=frozenset({"switchboard/ledger", "claude/integrator-inbox"}),
                system_prs=frozenset({282}))
    base.update(kw)
    if "open_prs" not in kw:
        base["open_prs"] = [p.number for p in base["prs"]]
    return L.Feed(**base)


def by_ref(led, ref):
    got = [it for it in led.items if it.ref == ref]
    assert len(got) == 1, (ref, [it.ref for it in led.items])
    return got[0]


class FakeGH:
    repo = "IssisX/ScraperX"

    def __init__(self):
        self.comments: list[tuple[int, str]] = []
        self.writes: list[tuple[str, str, object]] = []

    def comment(self, number, body, what=""):
        self.comments.append((number, body))
        return {"id": 1000 + len(self.comments)}

    def write(self, method, path, body=None, what=""):
        self.writes.append((method, path, body))
        return None


BOARD = "# Board: live jobs (3)\n\nCory's notes stay exactly as they are.\n\n## Lanes\n- one\n- two\n"


class Names(unittest.TestCase):
    def test_job_of_branch(self):
        self.assertEqual(L.job_of_branch("fb-bot/integrate-protected-j044b-grant"), "J-044b")
        self.assertEqual(L.job_of_branch("fb-bot/integrate-j059-cheek"), "J-059")
        self.assertEqual(L.job_of_branch("claude/integrate-J-061"), "J-061")
        self.assertEqual(L.job_of_branch("grok/j009-258-drag"), "J-009")
        self.assertEqual(L.job_of_branch("claude/integrate-ledger"), "")
        self.assertEqual(L.job_of_branch("claude/rejoin-1234"), "")   # no j-id: not a stray digit run

    def test_roster(self):
        self.assertEqual(ROSTER.of_branch("fb-bot/x"), L.Person("Muse", "muse"))
        self.assertEqual(ROSTER.of_branch("builder/x"), L.Person("Builder", "grok"))
        self.assertIsNone(ROSTER.of_branch("gemini/x"))
        self.assertEqual(ROSTER.of_labels(["job", "owner:builder"]), L.Person("Builder", "grok"))
        self.assertEqual(ROSTER.of_labels(["owner:claude"]), L.Person("Claude", "claude"))
        self.assertEqual(ROSTER.address("grok"), "Grok")   # Grok's runs read only `To Grok:`


class Classify(unittest.TestCase):
    def everything(self):
        return feed(
            prs=[pr(343, "fb-bot/integrate-j059-cheek"),
                 pr(337, "claude/integrate-ledger", job=""),               # no Job: line
                 pr(282, "claude/integrator-inbox", job="")],              # the inbox: system
            open_prs=[343, 337, 282, 299],                                  # 299 was not read this sweep
            branches=[L.BranchIn("fb-bot/integrate-protected-j044b-grant", SHA["b"], ago(3)),   # lettered job
                      L.BranchIn("gemini/try-things", SHA["c"], ago(5)),                       # unknown prefix
                      L.BranchIn("claude/notes", SHA["d"], ago(6)),                            # known, no job
                      L.BranchIn("fb-bot/integrate-j059-cheek", SHA["a"], ago(2)),             # PR #343's
                      L.BranchIn("archive/old", SHA["e"], ago(1)),
                      L.BranchIn("ScraperX-Claude", SHA["f"], ago(1)),
                      L.BranchIn("switchboard/ledger", SHA["0"], ago(1)),
                      L.BranchIn("muse/x", SHA["1"], ago(1), job_line="fix\n\nJob: J-070\n")],  # job from commit
            issues=[L.IssueIn(400, "u400", "J-044b: grant", ("job", "owner:muse", "state:active"), ago(4)),
                    L.IssueIn(401, "u401", "J-071 no owner", ("job",), ago(4)),
                    L.IssueIn(402, "u402", "Grant cleanup", ("job", "owner:grok"), ago(4))],
            asks=[L.AskIn("555", 343, "u555", "Grok", "Claude", ago(1))],
            runs=[run(1, "ScraperX-Claude", "f", 1),                        # trunk newest red
                  run(2, "ScraperX-Claude", "e", 5),                        # older trunk red: resolved
                  run(3, "fb-bot/integrate-protected-j044b-grant", "b", 2.5),   # red on the live head
                  run(4, "fb-bot/integrate-protected-j044b-grant", "c", 9),     # an older head: resolved
                  run(5, "fb-bot/gone-branch", "c", 9),                          # branch gone
                  run(6, "claude/notes", "d", 5, "success")],
            problems=("repo activity: more than 1000 entries this week, the rest not read",))

    def test_every_input_kind_is_placed_and_nothing_is_dropped(self):
        f = self.everything()
        led = L.classify(f, NOW, ROSTER)
        self.assertEqual(led.dropped(f), [])
        self.assertTrue(all(i in led.accounted for i in L.inputs_of(f)))
        self.assertFalse([it for it in led.items if "dropped" in it.unmatched])
        kinds = {it.kind for it in led.items}
        self.assertEqual(kinds, {"PR", "BRANCH", "JOB", "ASK", "TRUNK", "READ"})

    def test_unmatched(self):
        led = L.classify(self.everything(), NOW, ROSTER)
        self.assertIn("Job:", by_ref(led, "#337").unmatched)
        self.assertEqual(by_ref(led, "#337").holder.name, "Claude")
        self.assertIn("not read", by_ref(led, "#299").unmatched)
        g = by_ref(led, "gemini/try-things")
        self.assertEqual((g.unmatched, g.holder.name), ("unknown branch prefix", "Claude"))
        n = by_ref(led, "claude/notes")
        self.assertIn("no job id", n.unmatched)
        self.assertEqual(n.holder.name, "Claude")
        self.assertIn("no owner", by_ref(led, "#401").unmatched)
        self.assertIn("J-NNN", by_ref(led, "#402").unmatched)
        self.assertTrue(by_ref(led, "read 1").unmatched)
        for ok in ("#343", "fb-bot/integrate-protected-j044b-grant", "muse/x", "#400"):
            self.assertEqual(by_ref(led, ok).unmatched, "", ok)

    def test_lettered_job_owner_and_red_head(self):
        led = L.classify(self.everything(), NOW, ROSTER)
        b = by_ref(led, "fb-bot/integrate-protected-j044b-grant")
        self.assertEqual((b.job, b.owner.name, b.holder.name), ("J-044b", "Muse", "Muse"))
        self.assertIn("red", b.state)
        self.assertEqual(b.deadline, L.ACT)
        self.assertEqual(b.thread, 400)   # its notice goes on the job issue
        self.assertEqual(by_ref(led, "muse/x").job, "J-070")
        i = by_ref(led, "#400")
        self.assertEqual((i.owner.name, i.job), ("Muse", "J-044b"))

    def test_system_and_resolved_are_counted_not_listed(self):
        f = self.everything()
        led = L.classify(f, NOW, ROSTER)
        refs = {it.ref for it in led.items}
        for gone in ("#282", "archive/old", "switchboard/ledger", "fb-bot/integrate-j059-cheek"):
            self.assertNotIn(gone, refs)
        self.assertEqual(led.accounted["branch:fb-bot/integrate-j059-cheek"], "PR|#343")
        self.assertEqual(led.counts["system branch"], 3)
        self.assertEqual(led.counts["system PR"], 1)
        self.assertEqual(led.accounted["run:3"], "BRANCH|fb-bot/integrate-protected-j044b-grant")
        self.assertIn("resolved", led.accounted["run:2"])
        self.assertIn("resolved", led.accounted["run:4"])
        self.assertIn("gone", led.accounted["run:5"])
        t = by_ref(led, "ScraperX-Claude")
        self.assertEqual((t.kind, t.holder.name, t.deadline), ("TRUNK", "Claude", L.ACT))

    def test_ask(self):
        a = by_ref(L.classify(self.everything(), NOW, ROSTER), "ask on #343")
        self.assertEqual((a.holder.name, a.holder.agent, a.deadline, a.thread), ("Grok", "grok", L.ACT, 343))

    def test_a_dropped_input_is_loud(self):
        f = self.everything()
        led = L.Ledger([], {}, {})
        self.assertEqual(len(led.dropped(f)), len(L.inputs_of(f)))
        bad = L.Ledger([], {k: "x" for k in L.inputs_of(f)[1:]}, {})
        self.assertEqual(bad.dropped(f), [L.inputs_of(f)[0]])


class Deadlines(unittest.TestCase):
    def test_pr_holders_and_clocks(self):
        f = feed(prs=[pr(1, "fb-bot/integrate-j001", action="FIX-CI", proof="red: native", done=3),
                      pr(2, "claude/integrate-j002", job="J-002", needs_review=True, done=1.5),
                      pr(3, "claude/integrate-j003", job="J-003", needs_review=True, reviewer="chatgpt", done=2.5),
                      pr(4, "claude/integrate-j004", job="J-004", needs_review=True, reviewer="claude-api", done=1),
                      pr(5, "fb-bot/integrate-j005", job="J-005", action="FIX", reasons=("open FIX by Codex",), touched=30),
                      pr(6, "fb-bot/integrate-j006", job="J-006", action="MERGE", touched=5),
                      pr(8, "fb-bot/integrate-j008", job="J-008", action="MERGE", touched=50),
                      pr(7, "fb-bot/integrate-j007", job="J-007", reasons=("needs:eyes and no eye LAND on this head",))])
        led = L.classify(f, NOW, ROSTER)
        p1, p2, p3, p4, p5, p6, p7, p8 = (by_ref(led, f"#{n}") for n in range(1, 9))
        self.assertEqual((p1.holder.name, p1.deadline, p1.stage(NOW)), ("Muse", L.ACT, 1))   # red 3 h: past 2 h
        self.assertEqual((p2.holder.name, p2.deadline, p2.stage(NOW)), ("Codex", L.ACT, 0))  # green 1.5 h, no review
        self.assertEqual((p3.holder.name, p3.stage(NOW)), ("ChatGPT", 1))
        self.assertEqual(p4.holder.name, "Claude")   # the automatic review is late: the integrator looks
        self.assertEqual((p5.holder.name, p5.deadline, p5.stage(NOW)), ("Muse", L.STALE, 1))   # untouched 30 h
        self.assertEqual((p6.holder, p6.stage(NOW)), (L.SWITCHBOARD, 0))
        self.assertEqual((p8.holder.name, p8.stage(NOW)), ("Claude", 2))   # the merge never happened
        self.assertIn("the switchboard has not done it", p8.state)
        self.assertEqual(p7.holder.name, "Claude")

    def test_stage_edges(self):
        it = L.Item("ASK", "a", "", "", None, ROSTER.person("grok"), "", ago(0), L.ACT, "v")
        for hours, stage in ((1.99, 0), (2.0, 1), (3.99, 1), (4.0, 2)):
            it.clock = ago(hours)
            self.assertEqual(it.stage(NOW), stage, hours)

    def test_stale_branch_and_issue(self):
        f = feed(branches=[L.BranchIn("grok/integrate-j010-x", SHA["a"], ago(25))],
                 issues=[L.IssueIn(9, "u9", "J-011 thing", ("job", "owner:chatgpt"), ago(49))],
                 runs=[run(1, "grok/integrate-j010-x", "a", 24.5, "success")])
        led = L.classify(f, NOW, ROSTER)
        b = by_ref(led, "grok/integrate-j010-x")
        self.assertEqual((b.holder.name, b.deadline, b.stage(NOW)), ("Grok", L.STALE, 1))
        self.assertIn("green, no PR", b.state)
        self.assertEqual(by_ref(led, "#9").stage(NOW), 2)

    def test_an_issue_is_touched_by_its_branch(self):
        f = feed(branches=[L.BranchIn("grok/integrate-j011-y", SHA["a"], ago(1))],
                 issues=[L.IssueIn(9, "u9", "J-011 thing", ("job", "owner:chatgpt"), ago(49))])
        self.assertEqual(by_ref(L.classify(f, NOW, ROSTER), "#9").stage(NOW), 0)

    def test_touches_skip_the_switchboard(self):
        comments = [{"issue_url": "x/issues/9", "created_at": L.iso(ago(1)), "user": {"login": L.rules.BOT_LOGIN}},
                    {"issue_url": "x/issues/9", "created_at": L.iso(ago(30)), "user": {"login": "IssisX"}}]
        events = [{"event": "labeled", "issue": {"number": 9}, "created_at": L.iso(ago(2)),
                   "actor": {"login": L.rules.BOT_LOGIN}}]
        t = L.touch_times(comments, events, L.rules.BOT_LOGIN)
        self.assertEqual(t[9], ago(30))
        window = ago(48)
        self.assertEqual(L.touched_at(L.iso(ago(100)), L.iso(ago(1)), t, 9, window), ago(30))
        self.assertEqual(L.touched_at(L.iso(ago(100)), L.iso(ago(1)), {}, 9, window), window)  # only bot touches
        self.assertEqual(L.touched_at(L.iso(ago(100)), L.iso(ago(60)), {}, 9, window), ago(60))


class Escalation(unittest.TestCase):
    def setUp(self):
        self.out: list[str] = []

    def sweep(self, gh, f, now, body, sent=()):
        L.run(gh, f, ROSTER, now, "noon", self.out.append, "wake:muse", sent, board_body=lambda: body)
        patches = [b["body"] for m, p, b in gh.writes if m == "PATCH" and p.endswith("/issues/340")]
        return patches[-1] if patches else body

    def red_branch(self):
        return feed(branches=[L.BranchIn("fb-bot/integrate-j044b-x", SHA["b"], ago(4))],
                    runs=[run(3, "fb-bot/integrate-j044b-x", "b", 0)])

    def test_once_per_stage_then_escalated(self):
        f = self.red_branch()
        gh = FakeGH()
        body = self.sweep(gh, f, NOW, BOARD)                 # red 0 h: nothing owed
        self.assertEqual(gh.comments, [])
        self.assertIn(L.START, body)
        gh = FakeGH()
        body = self.sweep(gh, f, NOW + 2.5 * H, body)       # past 2 h: Muse is told, once
        self.assertEqual(len(gh.comments), 1)
        n, text = gh.comments[0]
        self.assertEqual(n, 340)                             # no job issue: the board
        self.assertTrue(text.startswith("CLAUDE_TO_MUSE: "))
        self.assertNotIn("http", text)                      # Muse's injection check refuses a mail with links
        self.assertNotIn("](", text)
        self.assertNotIn(asks.LEDGER_MARK, text)            # the marker goes in after the label (as muse())
        self.assertIn(("POST", "repos/IssisX/ScraperX/issues/340/labels", {"labels": ["wake:muse"]}), gh.writes)
        marked = [b["body"] for m, p, b in gh.writes if p.endswith("/comments/1001")]
        self.assertEqual(len(marked), 1)
        self.assertIn(asks.LEDGER_MARK, marked[0])
        gh = FakeGH()
        body = self.sweep(gh, f, NOW + 3 * H, body)         # still stage 1: no repeat
        self.assertEqual(gh.comments, [])
        gh = FakeGH()
        body = self.sweep(gh, f, NOW + 4.5 * H, body)       # past 2 x 2 h: once more, and ESCALATED
        self.assertEqual(len(gh.comments), 1)
        self.assertIn("twice past", gh.comments[0][1])     # mail words: no shouted urgency for Muse's check
        self.assertNotIn("http", gh.comments[0][1])
        esc = body.split("### ESCALATED", 1)[1].split("###", 1)[0]
        self.assertIn("fb-bot/integrate-j044b-x", esc)
        gh = FakeGH()
        self.sweep(gh, f, NOW + 9 * H, body)
        self.assertEqual(gh.comments, [])                   # every stage told once

    def test_keys_in_comments_count_as_sent(self):
        f = self.red_branch()
        it = L.classify(f, NOW + 2.5 * H, ROSTER).items[0]
        gh = FakeGH()
        self.sweep(gh, f, NOW + 2.5 * H, BOARD, sent=[it.key(1)])
        self.assertEqual(gh.comments, [])

    def test_a_new_head_is_a_new_notice(self):
        f = self.red_branch()
        gh = FakeGH()
        body = self.sweep(gh, f, NOW + 2.5 * H, BOARD)
        self.assertEqual(len(gh.comments), 1)
        f2 = feed(branches=[L.BranchIn("fb-bot/integrate-j044b-x", SHA["c"], ago(1))],
                  runs=[run(4, "fb-bot/integrate-j044b-x", "c", 0)])
        gh = FakeGH()
        self.sweep(gh, f2, NOW + 2.5 * H, body)
        self.assertEqual(len(gh.comments), 1)

    def test_channels(self):
        f = feed(asks=[L.AskIn("9", 50, "u", "Builder", "Claude", ago(3))],
                 prs=[pr(60, "claude/integrate-j060", job="J-060", needs_review=True, done=3)])
        gh = FakeGH()
        self.sweep(gh, f, NOW, BOARD)
        got = dict(gh.comments)
        self.assertTrue(got[50].startswith("To Grok: for Builder,"))   # Grok's runs read only `To Grok:`
        self.assertTrue(got[60].startswith("@codex review\n"))
        self.assertEqual(got[60].count("@"), 1)
        for text in got.values():
            self.assertIn(asks.LEDGER_MARK, text)

    def test_muse_gets_one_mail_per_sweep(self):
        f = feed(issues=[L.IssueIn(200 + i, f"u{i}", f"J-2{i:02d} x", ("job", "owner:muse"), ago(30 + i))
                         for i in range(20)])
        gh = FakeGH()
        body = self.sweep(gh, f, NOW, BOARD)
        self.assertEqual(len(gh.comments), 1)
        n, text = gh.comments[0]
        self.assertEqual(n, 219)                               # on her most urgent item's own thread
        self.assertEqual(text.count("\n- "), L.MAX_MAIL_ITEMS)
        gh = FakeGH()
        self.sweep(gh, f, NOW, body)
        self.assertEqual(len(gh.comments), 1)
        self.assertEqual(gh.comments[0][1].count("\n- "), 20 - L.MAX_MAIL_ITEMS)   # the rest, none twice

    def test_the_switchboard_is_never_told(self):
        f = feed(prs=[pr(6, "fb-bot/integrate-j006", job="J-006", action="MERGE", touched=5)])
        gh = FakeGH()
        self.sweep(gh, f, NOW, BOARD)
        self.assertEqual(gh.comments, [])
        gh = FakeGH()
        self.sweep(gh, f, NOW + 20 * H, BOARD)                # past 24 h: the integrator is told
        self.assertEqual(len(gh.comments), 1)
        self.assertTrue(gh.comments[0][1].startswith("To Claude:"))

    def test_a_notice_is_not_an_ask(self):
        body = "To Grok: 1 item(s) past deadline\n\n- x\n\n" + L.NOTICE_MARK.format(keys="ab")
        c = [{"n": 5, "t": ago(1), "url": "u", "body": body}]
        self.assertEqual(asks.unanswered(c, {5}, ago(10)), [])
        c = [{"n": 5, "t": ago(1), "url": "u", "body": "Claude: To Grok: is the bound right?"}]
        self.assertEqual([a["target"] for a in asks.unanswered(c, {5}, ago(10))], ["Grok"])

    def test_notices_are_capped(self):
        f = feed(asks=[L.AskIn(str(i), 100 + i, "u", "ChatGPT", "Claude", ago(3)) for i in range(15)])
        gh = FakeGH()
        body = self.sweep(gh, f, NOW, BOARD)
        self.assertEqual(len(gh.comments), L.MAX_NOTICES)
        gh = FakeGH()
        self.sweep(gh, f, NOW, body)
        self.assertEqual(len(gh.comments), 15 - L.MAX_NOTICES)   # the rest next sweep, none twice


class Superseded(unittest.TestCase):
    def test_older_branch_of_a_job_is_superseded(self):
        f = feed(branches=[L.BranchIn("fb-bot/integrate-j059-cheek", SHA["a"], ago(10)),
                           L.BranchIn("fb-bot/integrate-j059-cheek-v2", SHA["b"], ago(1)),
                           L.BranchIn("claude/integrate-j059", SHA["c"], ago(20)),
                           L.BranchIn("fb-bot/integrate-protected-j059-geo", SHA["d"], ago(30))],
                 prs=[pr(343, "claude/integrate-j059")])
        led = L.classify(f, NOW, ROSTER)
        sup = sorted(it.ref for it in led.items if it.kind == "SUPERSEDED")
        self.assertEqual(sup, ["claude/integrate-j059", "fb-bot/integrate-j059-cheek"])
        old = by_ref(led, "fb-bot/integrate-j059-cheek")
        self.assertEqual(old.holder.name, "Muse")
        self.assertTrue(old.state.startswith("SUPERSEDED by fb-bot/integrate-j059-cheek-v2"))
        self.assertIn("close it or say keep", old.state)
        self.assertEqual(by_ref(led, "claude/integrate-j059").thread, 343)
        self.assertEqual(by_ref(led, "#343").kind, "PR")              # the PR itself stays listed
        self.assertEqual(by_ref(led, "fb-bot/integrate-j059-cheek-v2").kind, "BRANCH")
        self.assertEqual(by_ref(led, "fb-bot/integrate-protected-j059-geo").kind, "BRANCH")   # its own lane
        self.assertEqual(led.dropped(f), [])

    def test_keep_suppresses(self):
        f = feed(branches=[L.BranchIn("fb-bot/integrate-j059-cheek", SHA["a"], ago(10)),
                           L.BranchIn("fb-bot/integrate-j059-cheek-v2", SHA["b"], ago(1))],
                 keeps=frozenset({"fb-bot/integrate-j059-cheek"}))
        led = L.classify(f, NOW, ROSTER)
        self.assertFalse([it for it in led.items if it.kind == "SUPERSEDED"])
        self.assertEqual(by_ref(led, "fb-bot/integrate-j059-cheek").kind, "BRANCH")

    def test_keep_line(self):
        found = {m.group("branch") for m in L.KEEP.finditer("Muse: done.\nkeep fb-bot/integrate-j059-cheek\n"
                                                             "keep: `claude/x`\nwe keep going on y\n")}
        self.assertEqual(found, {"fb-bot/integrate-j059-cheek", "claude/x"})


class Board(unittest.TestCase):
    def test_rewrite_leaves_outside_text_byte_identical(self):
        before, after = "# Board\r\n\r\nTop text.\r\n", "\r\n## Lanes\r\n- a\r\n"
        body = before + L.START + "\nold section\n" + L.END + after
        new = L.rewrite(body, "NEW")
        self.assertEqual(new, before + L.START + "\nNEW\n" + L.END + after)
        self.assertTrue(new.startswith(before) and new.endswith(after))

    def test_markers_go_under_the_first_heading(self):
        new = L.rewrite(BOARD, "S")
        block = f"\n\n{L.START}\nS\n{L.END}"
        self.assertEqual(new, "# Board: live jobs (3)" + block + BOARD[len("# Board: live jobs (3)"):])
        self.assertEqual(new.replace(block, "", 1), BOARD)
        crlf = BOARD.replace("\n", "\r\n")
        self.assertEqual(L.rewrite(crlf, "S").replace(f"\r\n\r\n{L.START}\nS\n{L.END}", "", 1), crlf)
        self.assertEqual(L.rewrite("no heading", "S"), f"{L.START}\nS\n{L.END}\n\nno heading")
        again = L.rewrite(new, "T")
        self.assertEqual(again.replace(f"\n\n{L.START}\nT\n{L.END}", "", 1), BOARD)

    def test_broken_markers_are_not_written(self):
        for body in (f"# B\n{L.START}\nx", f"# B\n{L.END}\nx", f"# B\n{L.END}\n{L.START}\n",
                     f"# B\n{L.START}\n{L.END}\n{L.START}\n{L.END}\n"):
            self.assertIsNone(L.rewrite(body, "S"), body)
        gh = FakeGH()
        out: list[str] = []
        L.run(gh, feed(), ROSTER, NOW, "noon", out.append, board_body=lambda: f"# B\n{L.START}\nx")
        self.assertEqual(gh.writes, [])
        self.assertTrue(any("broken ledger markers" in o for o in out))

    def test_order_and_size(self):
        items = [L.AskIn(str(i), 1000 + i, f"https://github.com/IssisX/ScraperX/issues/{1000 + i}#c{i}",
                         ["ChatGPT", "Grok", "Claude"][i % 3], "Claude", ago(0.5)) for i in range(120)]
        f = feed(asks=items + [L.AskIn("old", 7, "u", "Grok", "Claude", ago(5))],
                 branches=[L.BranchIn("gemini/a", SHA["a"], ago(1))])
        led = L.classify(f, NOW, ROSTER)
        text = L.section(led, NOW, "noon", [], [])
        visible = text.rsplit("\n", 1)[0]
        self.assertLess(len(visible), L.MAX_SECTION)
        self.assertLess(text.index("### ESCALATED"), text.index("### UNMATCHED"))
        self.assertIn("ask on #7", text.split("### UNMATCHED")[0])       # escalated in full
        self.assertIn("gemini/a", text.split("### UNMATCHED")[1])         # unmatched in full
        self.assertIn("Everything else, by holder", text)
        self.assertIn("- **ChatGPT**: 40", text)
        small = L.section(L.classify(feed(asks=items[:3]), NOW, ROSTER), NOW, "noon", [], [])
        self.assertIn("### ChatGPT (1)", small)
        self.assertLess(small.index("items**"), small.index("### ESCALATED"))   # counts first

    def test_outside_text_is_never_in_markup(self):
        f = feed(branches=[L.BranchIn("gemini/<!--x-->@cory`", SHA["a"], ago(1))])
        text = L.section(L.classify(f, NOW, ROSTER), NOW, "noon", [], [])
        self.assertNotIn("@cory", text)
        self.assertNotIn("<!--x", text)
        self.assertEqual(text.count(L.START), 0)

    def test_sent_keys_round_trip(self):
        body = L.rewrite(BOARD, L.section(L.classify(feed(), NOW, ROSTER), NOW, "noon", ["ab12", "cd34"], ["ef56"]))
        self.assertEqual(L.section_state(body), (["ab12", "cd34"], ["ef56"]))
        self.assertEqual(L.section_state(BOARD), ([], []))


if __name__ == "__main__":
    unittest.main()
