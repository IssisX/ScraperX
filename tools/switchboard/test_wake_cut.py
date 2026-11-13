"""Unit tests for the inbox's wake cut (Cory's "A", 11-13; the #282 measure in comment 6853558131): every inbox
comment is a full wake of the integrator, so only what needs it wakes it at once.

(1) A Muse mail is read by its content, never by its "To Claude:" greeting: a report rides the digest, anything that
asks, or that the classifier is unsure of, wakes at once. (2) Each item has a class: an ask posts at once, never
delayed; the integrator's own work posts at most once per INBOX_HOLD; reports ride the next post that wakes, and never
post alone (every #282 comment wakes the integrator: the digest-only post 6854673147 woke it for nothing). (3) Nothing is dropped: what waits is held in the sweep state, and posted later. (4) The keys left the
comment for the sweep state, so no comment passes the relay's cut.
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

import switch  # noqa: E402
import test_inbox as ti  # noqa: E402
import test_sweep_calls as tsc  # noqa: E402

NOW, HEAD, OTHER_HEAD = ti.NOW, ti.HEAD, ti.OTHER_HEAD
M = dt.timedelta(minutes=1)


def bridge(cid: int, title: str, text: str, number: int = 292, hours: float = 1.0) -> dict:
    """A Muse mail as muse_bridge.gs posts it."""
    return ti.comment(cid, number, f"**Muse (via email bridge): {title}**\n\n{text}\n\n_Mailed Nov 6 1:00 PM Central._",
                      hours)


def kind_of(title: str, text: str) -> str:
    body = f"**Muse (via email bridge): {title}**\n\n{text}\n\n_Mailed Nov 6 1:00 PM Central._"
    return switch.muse_routine(body.split("\n", 1)[0], body) or "TO-CLAUDE"


def at(gh, minutes: float):
    return ti.make(gh, NOW + minutes * M)


def ask(cid: int, text: str = "To Claude: which head next?", number: int = 292) -> dict:
    return ti.comment(cid, number, text, hours=0.1)


class MuseMail(unittest.TestCase):
    """Real mails of 11-11 and 11-12 (#282), shortened."""

    def test_the_greeting_no_longer_wakes_a_report(self):
        # 11-12: 13 "started" mails woke only on "To Claude:", the greeting Muse now opens every body with
        for title, text, kind in (
                ("J-133 started", "To Claude: J-133 started. Branch fb-bot/integrate-j133, fresh worktree off live "
                                  "trunk. First step: read issue #459 live.", "STARTED"),
                ("J-021 job 11 pushed — water", "To Claude: J-021 job 11 is pushed. CI dispatched.", "FYI"),
                ("J-009 #377 FIX — ack", "/ack O-f70eab45\n\nTo Claude: J-009 #377 Codex FIX in work. Lane running, "
                                         "failing-first test, then the fix.", "FYI"),
                ("J-078 pushed @ ddceb919 — her motion never snaps, proven green",
                 "To Claude: J-078 pushed @ ddceb919 (branch fb-bot/integrate-j078). CI dispatched. Diff 4 files.", "FYI"),
                ("review of #469 @ 1dd99c44: LAND", "To Claude: review of #469 @ 1dd99c44: LAND. Proof first: all green.",
                 "FYI"),
                ("J-080 status", "J-080 is done: committed on fb-bot/integrate-j080, proof green, in the push queue.",
                 "FYI"),
                ("J-040 take_blow started", "Lane fixing take_blow per your ruling: real external blows count.",
                 "STARTED")):
            self.assertEqual(kind_of(title, text), kind, title)

    def test_anything_that_asks_wakes_at_once(self):
        for title, text in (
                ("J-077 done, proof green — 639 lines, waiver or split?", "The diff is 639 lines."),     # a question
                ("J-059 #343 split verdict — feed must land first, need your ruling", "The split is built."),
                ("J-015 #389 Codex P1 fix pushed @ a949f76f", "Your call whether it rides as one or splits."),
                ("J-089 pushed fb-bot/integrate-j089 @ 028f9a9b", "Over the 300 cap. Your call: a waiver."),
                ("J-063 rail trim started", "matron-ladder: no issue exists for the baseline failure. Yours to open."),
                ("J-090 stopped, nothing pushed", "The lane stopped at once on your mail. Both are yours."),
                ("J-021 pushed", "To Grok: is the mist radius right? CI dispatched."),                     # a relay
                ("J-105 pushed @ d62cd247 — leg spiral step 1", "Two things to know: 1. Deviation from the design."),
                ("J-053 pushed", "Cory should hear this one first."),
                ("J-087 pushed", "Your 09:28 rulings mail would not read, please resend."),
                ("J-094 not started", "Waiting on J-063."),                                               # negated
                ("ASK J-053 pushed", "The bar moved."),                                                   # tagged
                ("queue To Claude: 3 left", "The queue is down to the last lines."),                       # ask subject
                ("J-060 fight-contact probe verdict — FALSE COLLISION, lead 1", "Lead 1 is the mechanism."),  # untagged
                ("J-082 sha — lane touched it, pushed now", "Pushed."),                    # the routine word is no headline
                ("FYI J-053 pushed", "Is the bar right?")):                                # a question beats the tag
            self.assertEqual(kind_of(title, text), "TO-CLAUDE", title + " / " + text)

    def test_a_question_mark_in_a_quote_or_a_link_is_no_question(self):
        self.assertEqual(kind_of("J-021 pushed", "> Which head is it on?\n\nOn fb-bot/integrate-j021. "
                                                 "https://github.com/IssisX/ScraperX/pulls?q=is%3Aopen"), "FYI")
        self.assertEqual(kind_of("J-021 pushed", "Which head?"), "TO-CLAUDE")

    def test_per_your_ruling_reports_and_your_ruling_asks(self):
        self.assertEqual(kind_of("J-059 split green", "Per your ruling the protected branch lands first."), "FYI")
        self.assertEqual(kind_of("J-059 split green", "I need your ruling on which lands first."), "TO-CLAUDE")

    def test_the_fyi_tag_makes_a_plain_title_a_report(self):
        self.assertEqual(kind_of("FYI: J-040 cell C measured", "1339 N, the new trials cause the growth."), "FYI")
        self.assertEqual(kind_of("J-040 cell C measured", "1339 N, the new trials cause the growth."), "TO-CLAUDE")

    def test_an_ask_from_anyone_else_is_never_read_as_a_report(self):
        gh = ti.FakeGH(comments=[ti.comment(504, 292, "To Claude: J-094 pushed, CI dispatched.")], open_issues=[292])
        self.assertEqual([i.kind for i in ti.items_of(gh)], ["TO-CLAUDE"])


class Classes(unittest.TestCase):
    def item(self, kind, dest="claude"):
        return switch.InboxItem(kind, "#1", HEAD, kind, dest)

    def test_each_kind_has_its_class(self):
        self.assertEqual(switch.inbox_class(self.item("TO-CLAUDE")), "now")
        for kind in ("FYI", "STARTED", "ACK", "RERUN"):
            self.assertEqual(switch.inbox_class(self.item(kind)), "ride", kind)
        for owner, cls in (("integrator", "hold"), ("claude", "hold"), ("muse", "ride"), ("switchboard", "ride"),
                           ("reviewer", "ride")):
            self.assertEqual(switch.inbox_class(self.item("PLAN", owner)), cls, owner)
        for kind in switch.INBOX_ORDER:   # every other kind is the integrator's work: it holds, never only rides
            if kind not in switch.DIGEST_KINDS and kind not in ("TO-CLAUDE", "PLAN"):
                for dest in ("claude", "both"):
                    self.assertEqual(switch.inbox_class(self.item(kind, dest)), "hold", (kind, dest))


class Hold(unittest.TestCase):
    def test_an_ask_is_never_delayed(self):
        gh = ti.FakeGH(open_issues=[292])
        self.assertEqual(at(gh, 0).inbox([ti.human_merge()]), 1)   # the integrator's own work, at 0
        gh.comments = [ask(701)]
        self.assertEqual(at(gh, 1).inbox([]), 1, "an ask a minute after a post posts at once")
        self.assertIn("- TO-CLAUDE #292 To Claude: which head next?", gh.posts[1][1])

    def test_own_work_posts_at_most_once_per_hold_and_rides_an_ask_sooner(self):
        gh = ti.FakeGH(open_issues=[292])
        at(gh, 0).inbox([ti.human_merge(number=256)])
        sw = at(gh, 10)
        self.assertEqual(sw.inbox([ti.human_merge(number=256), ti.human_merge(number=260)]), 0)
        self.assertIn("INBOX       nothing new; 1 held for the next post (the hold)", sw.lines)
        self.assertEqual(len(gh.posts), 1)
        self.assertEqual(at(gh, 20).inbox([]), 0, "still inside the hold")
        self.assertEqual(at(gh, 31).inbox([]), 1, "the hold is over: posted from the state, nothing re-read")
        self.assertIn("[#260]", gh.posts[1][1])
        # held work rides an ask at once
        at(gh, 40).inbox([ti.human_merge(number=270)])
        gh.comments = [ask(702)]
        self.assertEqual(at(gh, 41).inbox([]), 2)
        body = gh.posts[2][1]
        self.assertLess(body.index("- TO-CLAUDE #292"), body.index("- MERGE-BY-HAND [#270]"), "the ask goes first")

    def test_nothing_held_is_dropped_and_each_is_listed_once(self):
        gh = ti.FakeGH(open_issues=[292])
        prs = [ti.human_merge(number=300 + n) for n in range(12)]
        for minute in range(0, 120, 5):   # a sweep every 5 minutes; a new PR's item every 10
            seen = prs[:1 + minute // 10]
            if minute == 50:
                gh.comments = [ask(703)]
            at(gh, minute).inbox(seen)
        bodies = "".join(b for _, b in gh.posts)
        for s in prs:
            self.assertEqual(bodies.count(f"[#{s.facts.number}]("), 1, s.facts.number)
        self.assertEqual(bodies.count("To Claude: which head next?"), 1)
        self.assertLessEqual(len(gh.posts), 6, "24 sweeps: about one post per hold, plus the ask's")

    def test_a_held_item_for_an_old_head_is_moot(self):
        gh = ti.FakeGH()
        at(gh, 0).inbox([ti.human_merge(number=256)])
        at(gh, 5).inbox([ti.human_merge(number=260, head=HEAD)])        # held
        self.assertEqual(at(gh, 31).inbox([ti.human_merge(number=260, head=OTHER_HEAD)]), 1)
        body = gh.posts[1][1]
        self.assertIn("[#260](https://github.com/IssisX/ScraperX/pull/260) `fb-bot/integrate-j042-windup` head ffffffff",
                      body)
        self.assertNotIn("head 1a2b3c4d", body)

    def test_a_held_red_whose_log_reads_meanwhile_is_listed_once_with_its_tests(self):
        import test_red_tests as trt
        mine = "claude/integrate-j104"
        gh = trt.LogGH({}, jobs={41: trt.JOBS[:1]}, heads={mine: HEAD})
        at(gh, 0).inbox([ti.human_merge()])                     # a post at 0: the red found at 5 is held
        gh.runs = [ti.run(41, mine, HEAD, hours=2)]
        self.assertEqual(at(gh, 5).inbox([]), 0)
        reads = len(gh.log_reads)
        self.assertGreater(reads, 0)
        self.assertIn("log unread", gh.state["inbox"]["held"][0]["tests"][0])
        gh.logs = {501: trt.NATIVE_LOG}                         # the log reads at 10: the read copy replaces it
        self.assertEqual(at(gh, 10).inbox([]), 0)
        self.assertEqual(len(gh.state["inbox"]["held"]), 1)
        self.assertIn("FAIL: ammo hands", gh.state["inbox"]["held"][0]["tests"][1])
        self.assertEqual(gh.state["inbox"]["held"][0]["since"], switch.iso_z(NOW + 5 * M), "its clock is kept")
        reads = len(gh.log_reads)
        self.assertEqual(at(gh, 20).inbox([]), 0)
        self.assertEqual(len(gh.log_reads), reads, "held with its tests read: its log is not read again")
        self.assertEqual(at(gh, 31).inbox([]), 1)
        body = gh.posts[1][1]
        self.assertEqual(body.count("- RED-BRANCH "), 1)
        self.assertIn("FAIL: ammo hands", body)
        self.assertNotIn("log unread", body)
        self.assertNotIn(switch.READ_NOW, body, "no notice went out with its log unread")

    def test_a_held_item_survives_the_state_and_a_malformed_one_is_dropped(self):
        gh = ti.FakeGH()
        at(gh, 0).inbox([ti.human_merge(number=256)])
        at(gh, 5).inbox([ti.human_merge(number=260)])
        held = gh.state["inbox"]["held"]
        self.assertEqual([(h["kind"], h["target"]) for h in held], [("MERGE-BY-HAND", "#260")])
        gh.state["inbox"]["held"] = held + ["junk", {"kind": "NOPE", "target": "#1", "head": "x", "line": "y"},
                                            {"kind": "FIX", "target": "#2", "head": "x", "line": "y", "tests": "z"}]
        sw = at(gh, 31)
        self.assertTrue(sw.inbox_clock())
        self.assertEqual(sw.inbox([]), 1)
        self.assertEqual(gh.state["inbox"]["held"], [])

    def test_a_digest_alone_never_posts_and_rides_the_next_wake(self):
        # 11-13, 6854673147: a digest-only post on #282 ("Nothing to decide") woke the integrator for nothing
        gh = ti.FakeGH(comments=[bridge(801, "J-021 pushed", "CI dispatched.", hours=0.1)], open_issues=[292])
        sw = at(gh, 0)
        self.assertEqual(sw.inbox([]), 0)
        (held,) = sw.digest_held
        self.assertEqual((held.kind, held.since), ("FYI", switch.iso_z(NOW)))
        for minute in (60, 179, 180, 600, 24 * 60):   # the old clock posted it alone at 180
            later = at(gh, minute)
            later.digest_held = sw.digest_held
            self.assertFalse(later.inbox_clock(), minute)
            self.assertEqual(later.inbox([]), 0, minute)
            self.assertEqual(later.inbox([], flush=True), 0, minute)
            sw = later
        self.assertEqual(gh.posts, [])
        self.assertEqual(len(sw.digest_held), 1)
        woke = at(gh, 24 * 60 + 1)
        woke.digest_held = sw.digest_held
        self.assertEqual(woke.inbox([ti.human_merge()]), 2)
        (_, body), = gh.posts
        self.assertTrue(body.split("\n")[2].startswith("1 new for the integrator at "))
        self.assertIn("- digest, nothing to decide: FYI #292 Muse: J-021 pushed", body)
        self.assertEqual(woke.digest_held, ())

    def test_a_full_posts_worth_of_digest_still_waits_for_a_wake(self):
        gh = ti.FakeGH(comments=[bridge(900 + n, f"J-{n:03d} pushed", "CI dispatched.", hours=0.1)
                                 for n in range(switch.MAX_DIGEST_ITEMS + 2)], open_issues=[292])
        sw = at(gh, 0)
        self.assertEqual(sw.inbox([]), 0)
        self.assertEqual((gh.posts, len(sw.digest_held)), ([], switch.MAX_DIGEST_ITEMS + 2))
        woke = at(gh, 1)
        woke.digest_held = sw.digest_held
        self.assertEqual(woke.inbox([ti.human_merge()]), 1 + switch.MAX_DIGEST_ITEMS)
        self.assertIn(" (and 2 more next time)", gh.posts[0][1])

    def test_other_owners_late_jobs_ride_and_the_integrators_hold(self):
        gh = ti.FakeGH()
        sw = at(gh, 0)
        sw.plan_items = (switch.InboxItem("PLAN", "J-059", "k1", "PLAN J-059: belongs to switchboard", "switchboard"),
                         switch.InboxItem("PLAN", "J-043", "k2", "PLAN J-043: belongs to muse", "muse"))
        self.assertEqual(sw.inbox([]), 0, "nobody's step but the integrator's wakes it")
        self.assertEqual(len(sw.digest_held), 2)
        sw2 = at(gh, 1)
        sw2.digest_held = sw.digest_held
        sw2.plan_items = (switch.InboxItem("PLAN", "J-125", "k3", "PLAN J-125: belongs to integrator", "integrator"),)
        self.assertEqual(sw2.inbox([]), 3)
        body = gh.posts[0][1]
        self.assertIn("- PLAN J-125: belongs to integrator", body)
        self.assertIn("- digest, nothing to decide: PLAN J-059: belongs to switchboard; PLAN J-043: belongs to muse", body)


class Keys(unittest.TestCase):
    def test_the_keys_are_in_the_state_not_the_comment(self):
        gh = ti.FakeGH()
        at(gh, 0).inbox([ti.human_merge()])
        (_, body), = gh.posts
        self.assertNotIn("<!--", body)
        self.assertEqual(ti.ring(gh), [switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)])
        self.assertEqual(gh.state["inbox"]["at"], switch.iso_z(NOW))

    def test_a_marker_from_before_still_dedups_and_moves_into_the_ring(self):
        old = switch.inbox_key("MERGE-BY-HAND", "#256", HEAD)
        gh = ti.FakeGH(inbox=[{"body": "Switchboard (inbox):\n\n" + switch.INBOX_MARK.format(keys=old),
                               "user": {"login": ti.BOT}}])
        self.assertEqual(at(gh, 0).inbox([ti.human_merge(number=256)]), 0)
        self.assertEqual(at(gh, 0).inbox([ti.human_merge(number=256), ti.human_merge(number=260)]), 1)
        self.assertEqual(ti.ring(gh), [old, switch.inbox_key("MERGE-BY-HAND", "#260", HEAD)])

    def test_the_ring_is_capped_oldest_out(self):
        gh = ti.FakeGH()
        gh.state["inbox"] = {"keys": [f"{n:016x}" for n in range(switch.MAX_INBOX_KEYS)]}
        at(gh, 0).inbox([ti.human_merge()])
        self.assertEqual(len(ti.ring(gh)), switch.MAX_INBOX_KEYS)
        self.assertEqual(ti.ring(gh)[-1], switch.inbox_key("MERGE-BY-HAND", "#256", HEAD))
        self.assertNotIn(f"{0:016x}", ti.ring(gh))

    def test_a_dry_run_leaves_the_state_alone(self):
        gh = ti.FakeGH(dry_run=True)
        self.assertEqual(at(gh, 0).inbox([ti.human_merge()]), 1)
        self.assertEqual(gh.state.get("inbox"), {})


class QuietSweep(unittest.TestCase):
    """run_sweep: a quiet sweep (nothing changed) still posts what the inbox holds once it is due."""

    def setUp(self):
        self.prs = [tsc.pr(n) for n in range(361, 368)]

    def held(self, minutes_ago: float) -> dict:
        it = switch.InboxItem("MERGE-BY-HAND", "#361", HEAD, "MERGE-BY-HAND [#361] green, LAND by Codex")
        return {"keys": [], "at": switch.iso_z(NOW - minutes_ago * M), "held": [switch.digest_record(it)]}

    def test_a_held_item_is_posted_by_a_quiet_sweep_once_due(self):
        gh = tsc.CountingGH(self.prs, dry_run=False)
        status, read, sw = tsc.sweep(gh, whole={"steer": tsc.last_state(self.prs), "inbox": self.held(31)})
        self.assertEqual((status, read), (0, []))
        self.assertIn("RIPE        inbox: held items are due (an ask or the hold's end)", sw.lines)
        self.assertEqual(gh.calls.count(f"POST issues/{switch.INBOX_PR}/comments"), 1)
        self.assertIn("- MERGE-BY-HAND [#361] green, LAND by Codex", gh.posts[0][1])
        self.assertEqual(gh.state["inbox"]["held"], [])

    def test_a_quiet_sweep_with_only_the_digest_held_posts_nothing(self):
        gh = tsc.CountingGH(self.prs, dry_run=False)
        fyi = switch.InboxItem("FYI", "#292", "801", "FYI #292 Muse: J-021 pushed", since=switch.iso_z(NOW - 600 * M))
        steer = tsc.last_state(self.prs)
        steer["timers"] = {**(steer.get("timers") or {}), "digest": [switch.digest_record(fyi)]}
        tsc.sweep(gh, whole={"steer": steer, "inbox": {"keys": [], "at": switch.iso_z(NOW - 600 * M), "held": []}})
        self.assertEqual(len(gh.calls), tsc.IDLE, gh.calls)
        self.assertNotIn(f"POST issues/{switch.INBOX_PR}/comments", gh.calls)
        self.assertEqual(gh.posts, [])

    def test_a_quiet_sweep_inside_the_hold_costs_nothing_more(self):
        gh = tsc.CountingGH(self.prs, dry_run=False)
        tsc.sweep(gh, whole={"steer": tsc.last_state(self.prs), "inbox": self.held(10)})
        self.assertEqual(len(gh.calls), tsc.IDLE, gh.calls)
        self.assertEqual(gh.posts, [])
        self.assertEqual(len(gh.state["inbox"]["held"]), 1)


if __name__ == "__main__":
    unittest.main()
