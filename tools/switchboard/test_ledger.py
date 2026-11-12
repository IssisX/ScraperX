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
                      pr(5, "fb-bot/integrate-j005", job="J-005", action="FIX", reasons=("open FIX by Codex",), touched=5),
                      pr(6, "fb-bot/integrate-j006", job="J-006", action="MERGE", touched=3),
                      pr(8, "fb-bot/integrate-j008", job="J-008", action="MERGE", touched=50),
                      pr(7, "fb-bot/integrate-j007", job="J-007", reasons=("needs:eyes and no eye LAND on this head",))])
        led = L.classify(f, NOW, ROSTER)
        p1, p2, p3, p4, p5, p6, p7, p8 = (by_ref(led, f"#{n}") for n in range(1, 9))
        self.assertEqual((p1.holder.name, p1.deadline, p1.stage(NOW)), ("Muse", L.ACT, 1))   # red 3 h: past 2 h
        self.assertEqual((p2.holder.name, p2.deadline, p2.stage(NOW)), ("Codex", L.ACT, 0))  # green 1.5 h, no review
        self.assertEqual((p3.holder.name, p3.stage(NOW)), ("ChatGPT", 1))
        self.assertEqual(p4.holder.name, "Claude")   # the automatic review is late: the integrator looks
        self.assertEqual((p5.holder.name, p5.deadline, p5.stage(NOW)), ("Muse", L.STALE, 1))   # untouched 5 h: past 4 h
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
        f = feed(branches=[L.BranchIn("grok/integrate-j010-x", SHA["a"], ago(5))],   # past 4 h, not 8 h
                 issues=[L.IssueIn(9, "u9", "J-011 thing", ("job", "owner:chatgpt"), ago(49))],
                 runs=[run(1, "grok/integrate-j010-x", "a", 4.5, "success")])
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

    def test_muse_mail_has_no_markup_links_or_orders(self):
        # J-072: Muse's injection check refused the 11-09 17:04Z ledger mail (backticked branches, "act, or say").
        f = self.red_branch()
        texts = []
        body = BOARD
        for at in (NOW + 2.5 * H, NOW + 4.5 * H):           # stage 1, then stage 2
            gh = FakeGH()
            body = self.sweep(gh, f, at, body)
            self.assertEqual(len(gh.comments), 1)
            texts.append(gh.comments[0][1])
        for text in texts:                                  # (no link: test_once_per_stage_then_escalated)
            self.assertIn("- fb-bot/integrate-j044b-x", text)  # the branch, plain
            self.assertNotIn("`", text)
            for phrase in ("act, or say", "You hold"):
                self.assertNotIn(phrase, text)
            self.assertIn("The next step for each is a PR, a fix, or one line on its issue saying why it waits.", text)

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
        # J-072: the holder's own prefix (Builder's notice went out as "To Grok: for Builder"), and a Codex item goes
        # to the integrator: the switchboard's own @codex mention never woke Codex (2 of 2, #191 and #150).
        self.assertTrue(got[50].startswith("To Builder: 1 item(s) past deadline"), got[50])
        self.assertTrue(got[60].startswith("To Claude: for Codex, 1 item(s) past deadline"), got[60])
        self.assertIn("post `codex review` with an at-sign in front on #60 as IssisX", got[60])
        for text in got.values():
            self.assertNotIn("@", text)
            self.assertIn(asks.LEDGER_MARK, text)

    def test_muse_gets_one_mail_per_sweep(self):
        f = feed(issues=[L.IssueIn(200 + i, f"u{i}", f"J-2{i:02d} x", ("job", "owner:muse"), ago(30 + i))
                         for i in range(20)])
        gh = FakeGH()
        body = self.sweep(gh, f, NOW, BOARD)
        self.assertEqual(len(gh.comments), 1)
        n, text = gh.comments[0]
        self.assertEqual(n, 340)   # on the board: an item's thread may be a PR, which the bridge cannot mail from
        self.assertEqual(text.count("\n- "), L.MAX_MAIL_ITEMS)
        gh = FakeGH()
        self.sweep(gh, f, NOW, body)
        self.assertEqual(len(gh.comments), 1)
        self.assertEqual(gh.comments[0][1].count("\n- "), 20 - L.MAX_MAIL_ITEMS)   # the rest, none twice

    def test_notices_v2_the_ledger_writes_its_section_and_posts_nothing(self):
        # J-072: one owner of notices. With the reconciler live and `notices: v2` (work/switchboard.yml) the ledger
        # posts no notice comment, keeps no clock for one, and still writes the section with the item escalated.
        f = self.red_branch()
        gh = FakeGH()
        led = L.run(gh, f, ROSTER, NOW + 5 * H, "noon", self.out.append, "wake:muse", (), board_body=lambda: BOARD,
                    notify=False)
        self.assertEqual(gh.comments, [], "no notice comment")
        self.assertEqual([p for m, p, _ in gh.writes if "labels" in p], [], "no wake:muse label")
        self.assertEqual(led.pending, [], "no notice clock for a fast sweep to fire or to force a full pass")
        (m, path, body), = gh.writes
        self.assertTrue(path.endswith("/issues/340"))
        text = body["body"]
        self.assertIn("### ESCALATED (1)", text)
        self.assertIn("fb-bot/integrate-j044b-x", text)
        self.assertIn("The ledger sends no notices", text)
        self.assertNotIn("holder is told once", text)
        self.assertEqual(L.section_state(text)[0], [], "no key is recorded as sent")
        # the default is today's behavior, unchanged: the holder is told
        gh = FakeGH()
        led = L.run(gh, f, ROSTER, NOW + 5 * H, "noon", self.out.append, "wake:muse", (), board_body=lambda: BOARD)
        self.assertEqual(len(gh.comments), 1)
        self.assertIn("holder is told once", [b["body"] for m, p, b in gh.writes if p.endswith("/issues/340")][-1])

    def test_the_switchboard_is_never_told(self):
        f = feed(prs=[pr(6, "fb-bot/integrate-j006", job="J-006", action="MERGE", touched=3)])
        gh = FakeGH()
        self.sweep(gh, f, NOW, BOARD)
        self.assertEqual(gh.comments, [])
        gh = FakeGH()
        self.sweep(gh, f, NOW + 2 * H, BOARD)                 # past 4 h: the integrator is told
        self.assertEqual(len(gh.comments), 1)
        self.assertTrue(gh.comments[0][1].startswith("To Claude:"))

    def test_a_notice_is_not_an_ask(self):
        body = "To Grok: 1 item(s) past deadline\n\n- x\n\n" + L.NOTICE_MARK.format(keys="ab")
        c = [{"n": 5, "t": ago(1), "url": "u", "body": body}]
        self.assertEqual(asks.unanswered(c, {5}, ago(10)), [])
        c = [{"n": 5, "t": ago(1), "url": "u", "body": "Claude: To Grok: is the bound right?"}]
        self.assertEqual([a["target"] for a in asks.unanswered(c, {5}, ago(10))], ["Grok"])

    RESEND = ("**Muse (via email bridge): To Claude: refused mail 1a1c14471ca3de39 — please resend reworded**\n\n"
              "To Claude: my read of your mail 1a1c14471ca3de39 was refused by a safety check.\n\nPlease resend it "
              "reworded.\n\nMain (FB Bot)")
    GROK = ("Claude (integrator): both To Grok asks above are done, so nothing more is needed here.\n\n---\n"
            "_Generated by [Claude Code](https://claude.ai/code)_")

    def asks_after(self, *replies):
        c = [{"n": 340, "t": ago(2), "url": "https://github.com/IssisX/ScraperX/issues/340#issuecomment-6746462436",
              "body": self.RESEND, "id": "6746462436"}]
        for i, (n, body) in enumerate(replies):
            c.append({"n": n, "t": ago(1.9 - i * 0.1), "url": f"u{i}", "body": body, "id": str(i)})
        return [(a["id"], a["target"]) for a in asks.unanswered(c, {340, 350}, ago(10)) if a["id"] == "6746462436"]

    def test_an_unrelated_later_post_by_the_addressee_is_no_answer(self):
        # J-072: Muse's "please resend" (6746462436) dropped out after Claude's post about Grok's asks (6746481113)
        self.assertEqual(self.asks_after((340, self.GROK)), [("6746462436", "Claude")])
        self.assertEqual(self.asks_after((350, "Claude: about #340 and the board.\n\nGenerated by [Claude Code]")),
                         [("6746462436", "Claude")])

    def test_a_reply_that_addresses_it_is_an_answer(self):
        cases = [
            (340, "Claude (integrator): To Muse: resent, reworded.\n\nGenerated by [Claude Code]"),   # back to her
            (340, "CLAUDE_TO_MUSE: J-072 the ledger mail again, reworded"),                         # a relay to her
            (350, "Claude: done, see 6746462436.\n\nGenerated by [Claude Code]"),                  # names the ask
            (350, "Claude: done.\n\n> Please resend it reworded.\n\nGenerated by [Claude Code]"),  # quotes it
            (350, "Claude: To Muse: about #340, resent.\n\nGenerated by [Claude Code]"),            # names the thread
        ]
        for n, body in cases:
            self.assertEqual(self.asks_after((n, body)), [], body[:60])

    def test_a_quote_too_short_names_nothing(self):
        self.assertEqual(self.asks_after((340, "Claude: noted.\n\n> Please\n\nGenerated by [Claude Code]")),
                         [("6746462436", "Claude")])

    def test_notices_are_capped(self):
        f = feed(asks=[L.AskIn(str(i), 100 + i, "u", "ChatGPT", "Claude", ago(3)) for i in range(15)])
        gh = FakeGH()
        body = self.sweep(gh, f, NOW, BOARD)
        self.assertEqual(len(gh.comments), L.MAX_NOTICES)
        gh = FakeGH()
        self.sweep(gh, f, NOW, body)
        self.assertEqual(len(gh.comments), 15 - L.MAX_NOTICES)   # the rest next sweep, none twice


def branch(name, sha, hours, closed=(), vs_trunk="ahead", ahead=(), ahead_by=None):
    """A branch with no open PR, with the facts the reader adds for one: its closed PRs (newest first) and its
    trunk compare (`ahead` are SHA keys, oldest first)."""
    b = L.BranchIn(name, SHA[sha], ago(hours))
    b.closed, b.vs_trunk = tuple(closed), vs_trunk
    b.ahead = tuple(SHA[k] for k in ahead)
    b.ahead_by = len(ahead) if ahead_by is None else ahead_by
    return b


class ReadFeedAsks(unittest.TestCase):
    """read_feed gives the judge (tools/board/asks.py) what the reconciler gives it: the job threads, the roster's
    agents and the reconciler's own id for each ask (J-072: one judge)."""

    class GH:
        repo = "IssisX/ScraperX"

        def __init__(self, comments, issues):
            self.comments, self.issues = comments, issues

        def get(self, path, accept=""):
            if "/issues/comments?" in path:
                return self.comments if "page=1" in path else []
            if "/activity?" in path or "/issues/events" in path or "/pulls?state=closed" in path:
                return []
            raise AssertionError(f"unexpected GET {path}")

        def paged(self, path, key=""):
            if "labels=job" in path:
                return [i for i in self.issues if i["labels"]]
            if path.endswith("/issues?state=open"):
                return self.issues
            return []   # the board's comments, the open PRs, the proof runs

    def comment(self, cid, n, hours, body, login="IssisX"):
        return {"id": cid, "issue_url": f"https://api.github.com/repos/IssisX/ScraperX/issues/{n}",
                "html_url": f"https://github.com/IssisX/ScraperX/issues/{n}#issuecomment-{cid}", "body": body,
                "user": {"login": login}, "created_at": L.iso(ago(hours)), "updated_at": L.iso(ago(hours))}

    def test_the_asks_are_judged_by_job_thread_agent_and_the_reconcilers_id(self):
        import types
        foot = "\n\n---\n_Generated by [Claude Code](https://claude.ai/code)_"
        issue = lambda n, title, job=True: {"number": n, "title": title, "html_url": f"u{n}", "created_at": L.iso(ago(90)),  # noqa: E731
                                            "updated_at": L.iso(ago(1)), "labels": [{"name": "job"}] if job else []}
        issues = [issue(128, "J-021 Environment lane C: the far world and water"),
                  issue(121, "J-014 Stock above 396 m (PE 2)"), issue(340, "Board: live jobs (3)", job=False)]
        water = L.ob_id("J-021", "ask", "claude", "6757835992")
        comments = [
            # #128: answered on the board, by the reconciler's id for it (O-ce356a89 on the real repo)
            self.comment(6757835992, 128, 9, "**Muse (via email bridge): J-021 job 11 pushed — water**\n\nTo Claude: "
                                             "J-021 job 11 is pushed."),
            self.comment(6758874231, 340, 8, f"Claude (integrator), to Muse: `/ack {water}`. Rulings are mailed." + foot),
            # #121: Claude's next post on the job's own issue
            self.comment(6742896304, 121, 7, "**Muse (via email bridge): To Claude: J-014 magnet fixed**\n\nTo Claude: "
                                             "the J-014 red is diagnosed and fixed."),
            self.comment(6743008924, 121, 6, "Claude (integrator): Muse, answered by mail this morning. Nothing owed." + foot),
            # the switchboard's own notice about an ask is no answer to it; this one stays open
            self.comment(6761158625, 340, 5, "**Muse (via email bridge): J-044c snag BLOCKED**\n\nTo Claude: J-044c snag "
                                             "— BLOCKED."),
            self.comment(6761877119, 340, 4, "To Claude: ask from Muse on #340\n\nAck with `/ack O-e5e89c0c`, or act.",
                         L.rules.BOT_LOGIN),
        ]
        sw = types.SimpleNamespace(gh=self.GH(comments, issues), team=None, run_state=None)
        from unittest import mock
        with mock.patch.object(L, "cursor_pages", return_value=([], False)) as activity:   # (it shells out to gh)
            f, _ = L.read_feed(sw, [], NOW, 340, "proof.yml", set(), set(), ROSTER)
            self.assertEqual(water, "O-ce356a89")
            self.assertEqual([(a.cid, a.thread, a.target, a.author) for a in f.asks],
                             [("6761158625", 340, "Claude", "Muse")])
            # without the roster the judge knows no job thread and no id: the ask answered by `/ack O-...` is listed
            f, _ = L.read_feed(sw, [], NOW, 340, "proof.yml", set(), set())
            self.assertEqual([a.cid for a in f.asks], ["6757835992", "6761158625"])
        self.assertEqual(activity.call_count, 2)
        self.assertEqual(L.thread_jobs(f.issues, f.prs, f.system_prs), {128: "J-021", 121: "J-014"})


class Resolved(unittest.TestCase):
    """A branch with no open PR whose work already landed, or whose PR was closed at its head, is counted with its
    reason, never listed as needing a PR (the ledger's first live sweep escalated six such branches)."""

    def green(self, *branches):
        key = {v: k for k, v in SHA.items()}
        return feed(branches=list(branches),
                    runs=[run(10 + i, b.name, key[b.sha], 1, "success") for i, b in enumerate(branches)])

    def test_landed(self):
        f = self.green(branch("claude/integrate-ledger", "a", 5, vs_trunk="behind"),
                       branch("fb-bot/integrate-sense-near-followup", "b", 6, vs_trunk="identical"),
                       branch("claude/integrate-j090-live", "c", 2, vs_trunk="ahead", ahead=("c",)))
        f.runs.append(run(20, "claude/integrate-ledger", "a", 0.5))   # a red run on a landed head
        led = L.classify(f, NOW, ROSTER)
        refs = {it.ref for it in led.items}
        self.assertNotIn("claude/integrate-ledger", refs)
        self.assertNotIn("fb-bot/integrate-sense-near-followup", refs)
        self.assertEqual(led.accounted["branch:claude/integrate-ledger"], "landed")
        self.assertEqual(led.counts["branch landed"], 2)
        self.assertIn("landed", led.accounted["run:20"])
        self.assertEqual(by_ref(led, "claude/integrate-j090-live").state, "green, no PR: open one")
        self.assertEqual(led.dropped(f), [])
        self.assertIn("2 branch landed", L.section(led, NOW, "noon", [], []))

    def test_merged_at_head(self):
        f = self.green(branch("claude/integrate-j051b-autopr-errors", "a", 30, vs_trunk="",
                              closed=[L.ClosedPr(287, SHA["a"], True)]))
        led = L.classify(f, NOW, ROSTER)
        self.assertEqual([it.ref for it in led.items if it.kind in ("BRANCH", "SUPERSEDED")], [])
        self.assertEqual(led.accounted["branch:claude/integrate-j051b-autopr-errors"],
                         "its PR #287 was merged at this head")
        self.assertEqual(led.counts["branch at its merged PR's head"], 1)
        self.assertEqual(led.dropped(f), [])

    def test_closed_unmerged_at_head(self):
        f = self.green(branch("claude/integrate-american-spelling", "b", 30, vs_trunk="diverged", ahead=("b",),
                              closed=[L.ClosedPr(325, SHA["b"], False), L.ClosedPr(300, SHA["c"], True)]))
        led = L.classify(f, NOW, ROSTER)
        self.assertEqual([it.ref for it in led.items if it.kind in ("BRANCH", "SUPERSEDED")], [])
        self.assertEqual(led.accounted["branch:claude/integrate-american-spelling"],
                         "its PR #325 was closed at this head")
        self.assertEqual(led.counts["branch at its closed PR's head"], 1)
        self.assertEqual(led.dropped(f), [])

    def test_commits_after_a_closed_pr_are_live_again(self):
        f = self.green(
            # merged with a merge commit: the PR's head is in trunk, the one commit on the branch came after it
            branch("claude/integrate-j051-asks-since", "a", 3, vs_trunk="diverged", ahead=("a",),
                   closed=[L.ClosedPr(289, SHA["0"], True)]),
            # closed unmerged at c, then two commits on top
            branch("fb-bot/integrate-j052-x", "e", 3, vs_trunk="diverged", ahead=("c", "d", "e"),
                   closed=[L.ClosedPr(330, SHA["c"], False)]),
            # the trunk compare failed: still listed, and says it could not count
            branch("grok/integrate-j053-y", "1", 3, vs_trunk="", closed=[L.ClosedPr(331, SHA["2"], False)]))
        led = L.classify(f, NOW, ROSTER)
        self.assertEqual(by_ref(led, "claude/integrate-j051-asks-since").state,
                         "1 commit after closed PR #289; green, no PR: open one")
        self.assertEqual(by_ref(led, "fb-bot/integrate-j052-x").state,
                         "2 commits after closed PR #330; green, no PR: open one")
        self.assertTrue(by_ref(led, "grok/integrate-j053-y").state.startswith(
            "commits after closed PR #331 (not counted"))
        self.assertEqual(led.counts, {})
        self.assertEqual(led.dropped(f), [])

    def test_resolved_branches_are_neither_superseded_nor_current(self):
        f = self.green(branch("claude/integrate-j060-old", "a", 20, vs_trunk="behind"),    # landed: not superseded
                       branch("claude/integrate-j060-mid", "b", 10, vs_trunk="ahead", ahead=("b",)),
                       branch("claude/integrate-j060-new", "c", 1, vs_trunk="",            # closed at head: not current
                              closed=[L.ClosedPr(340, SHA["c"], False)]))
        led = L.classify(f, NOW, ROSTER)
        self.assertFalse([it for it in led.items if it.kind == "SUPERSEDED"])
        self.assertEqual(by_ref(led, "claude/integrate-j060-mid").kind, "BRANCH")
        self.assertEqual(led.accounted["branch:claude/integrate-j060-old"], "landed")
        self.assertEqual(led.dropped(f), [])

    def test_a_branch_whose_open_pr_was_not_read_is_that_prs(self):
        f = feed(branches=[branch("claude/integrate-j061", "a", 2)], open_prs=[299],
                 open_heads={"claude/integrate-j061": 299})
        led = L.classify(f, NOW, ROSTER)
        self.assertEqual(led.accounted["branch:claude/integrate-j061"], "PR|#299")
        self.assertEqual([it.ref for it in led.items], ["#299"])


class Superseded(unittest.TestCase):
    def test_older_branch_of_a_job_is_superseded(self):
        f = feed(branches=[L.BranchIn("fb-bot/integrate-j059-cheek", SHA["a"], ago(10)),
                           L.BranchIn("fb-bot/integrate-j059-cheek-v2", SHA["b"], ago(1)),
                           L.BranchIn("claude/integrate-j059", SHA["c"], ago(20)),
                           L.BranchIn("fb-bot/integrate-protected-j059-geo", SHA["d"], ago(30))],
                 prs=[pr(343, "claude/integrate-j059")],
                 overlap={(SHA["a"], SHA["b"]): "its head is in fb-bot/integrate-j059-cheek-v2",
                          (SHA["c"], SHA["b"]): "fb-bot/integrate-j059-cheek-v2 changes every file it changes"})
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

    def test_a_shared_job_number_alone_is_not_superseded(self):
        # J-072: #315 (J-007 hoists) was shown as superseded by #336 (J-007 riser) only for sharing the job
        f = feed(branches=[L.BranchIn("fb-bot/integrate-protected-j007-exit-items", SHA["a"], ago(10)),
                           L.BranchIn("fb-bot/integrate-protected-j007-item6", SHA["b"], ago(1))],
                 prs=[pr(315, "fb-bot/integrate-protected-j007-exit-items")],
                 overlap={(SHA["a"], SHA["b"]): ""})
        led = L.classify(f, NOW, ROSTER)
        self.assertFalse([it for it in led.items if it.kind == "SUPERSEDED"])
        self.assertEqual(by_ref(led, "#315").kind, "PR")
        f.overlap = {}   # not read (the compare failed): not superseded either
        self.assertFalse([it for it in L.classify(f, NOW, ROSTER).items if it.kind == "SUPERSEDED"])

    def test_held_by_reads_ancestry_then_files(self):
        files = lambda *names: {"files": [{"filename": n} for n in names]}   # noqa: E731
        self.assertIn("head is in", L.held_by({"status": "ahead"}, {}, {}, "new"))
        self.assertIn("head is in", L.held_by({"status": "identical"}, {}, {}, "new"))
        self.assertIn("every file", L.held_by({"status": "diverged"}, files("a.cpp"), files("a.cpp", "b.cpp"), "new"))
        # the #315/#336 shape: some files shared (CMakeLists.txt, the test list), but not the work
        self.assertEqual(L.held_by({"status": "diverged"}, files("CMakeLists.txt", "src/sim/matron_hoist.cpp"),
                                   files("CMakeLists.txt", "src/sim/matron_traversal.cpp"), "new"), "")
        self.assertEqual(L.held_by({"status": "diverged"}, files(), files("a.cpp"), "new"), "")

    def test_lane_groups_match_the_classifier(self):
        bs = [L.BranchIn("fb-bot/integrate-j059-cheek", SHA["a"], ago(10)),
              L.BranchIn("fb-bot/integrate-j059-cheek-v2", SHA["b"], ago(1)),
              L.BranchIn("fb-bot/integrate-protected-j059-geo", SHA["d"], ago(30)),
              L.BranchIn("fb-bot/integrate-j060-x", SHA["c"], ago(2))]
        groups = L.lane_groups(bs, set(), ())
        self.assertEqual({k: [b.name for b in g] for k, g in groups.items()},
                         {("J-059", False): ["fb-bot/integrate-j059-cheek-v2", "fb-bot/integrate-j059-cheek"]})

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
