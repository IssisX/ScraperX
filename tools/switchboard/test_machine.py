"""The job machine: every state is reached by evidence alone, each names who owes the next step, and the only
alarm is a job past its limit. Raw GitHub-shaped reads go in (machine.collect), so the readers are under test too."""
import datetime as dt
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import machine as M  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402

T0 = dt.datetime(2026, 11, 12, 12, 0, tzinfo=dt.timezone.utc)
PLAN = ("limit claude 2\nlimit grok 1\nlimit muse 2\n"
        "J-101 muse #101 | first\n"
        "J-102 muse #102 | second\n"
        "J-103 muse #103 | third\n"
        "J-104 muse #104 after J-101 | waits on the first\n"
        "J-105 muse #105 hold: Cory parked it | held\n"
        "J-201 claude #201 | Claude's\n"
        "J-301 grok #301 | Grok's\n")
TEAM = rules.Team({"claude": "anthropic", "muse": "meta", "codex": "openai"},
                  [("claude/", "anthropic"), ("fb-bot/", "meta")], {}, ["muse"], "", {"muse": "meta"},
                  frozenset({"issisx", "github-actions[bot]"}))


def at(hours: float) -> dt.datetime:
    return T0 + dt.timedelta(hours=hours)


def iso(hours: float) -> str:
    return at(hours).strftime("%Y-%m-%dT%H:%M:%SZ")


def issue(n: int, job: str, body: str = "", labels=()) -> dict:
    return {"number": n, "title": f"{job} work", "state": "open", "body": body,
            "labels": [{"name": name} for name in labels]}


def br(name: str, sha: str) -> dict:
    return {"name": name, "commit": {"sha": sha}}


def comment(cid: int, hours: float, body: str, login: str = "IssisX", thread: int = 340) -> dict:
    return {"id": cid, "created_at": iso(hours), "body": body, "user": {"login": login},
            "issue_url": f"https://api.github.com/repos/IssisX/ScraperX/issues/{thread}"}


def mail(subject: str, body: str = "") -> str:
    return f"**Muse (via email bridge): {subject}**\n\n{body}"


class World:
    """A repo as the switch reads it. Tests add evidence and read the table back."""

    def __init__(self, text: str = PLAN):
        self.model = plan.parse(text)
        assert not self.model.errors, self.model.errors
        self.issues = [issue(ln.issue, ln.job) for ln in self.model.lines]
        self.branches, self.prs, self.comments = [], [], []
        self.inactive, self.closed, self.merged_at, self.head_at, self.works, self.missing = set(), set(), {}, {}, {}, {}

    def version(self, owner: str = "muse") -> str:
        bodies = {rules.job_of_title(i["title"]): i.get("body") or "" for i in self.issues}
        return M.order_version(self.model, owner, bodies)

    def tell(self, hours: float, owner: str = "muse", ver: str = "") -> None:
        ver = ver or self.version(owner)
        self.comments.append(comment(len(self.comments) + 1, hours, f"{M.NOTICE_HEAD}{ver}\n\n" +
                                     M.NOTICE_MARK.format(owner=owner, ver=ver, jobs=""), login=rules.BOT_LOGIN))

    def muse(self, hours: float, subject: str, body: str = "", login: str = "IssisX", thread: int = 340) -> None:
        self.comments.append(comment(len(self.comments) + 1, hours, mail(subject, body), login, thread))

    def hand(self, hours: float, subject: str) -> None:
        self.comments.append(comment(len(self.comments) + 1, hours, f"CLAUDE_TO_MUSE: {subject}\n\nbody"))

    def close(self, job: str) -> None:
        self.issues = [i for i in self.issues if rules.job_of_title(i["title"]) != job]
        self.closed.add(job)

    def rows(self) -> dict[str, M.Step]:
        post = M.Mail(tuple(M.posts(self.comments, TEAM.verdict_logins)), tuple(M.sent_notices(self.comments)),
                      dict(self.missing))
        reads = {"issues": self.issues, "branches": self.branches, "prs": self.prs, "inactive": frozenset(self.inactive),
                 "closed": frozenset(self.closed), "merged_at": self.merged_at, "head_at": self.head_at}
        self.table = M.table(self.model, M.collect(self.model, reads, works=self.works, mail=post, planned_at=T0))
        return {r.job: r for r in self.table}


class States(unittest.TestCase):
    """The worker path, one state at a time. Each step adds one piece of evidence and nothing else."""

    def test_the_whole_path_from_the_plan_to_a_closed_issue(self):
        w = World()
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.owes), ("planned", "switchboard"))
        self.assertIn(f"plan {w.version()}", r.next)

        w.tell(1)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.owes, r.since), ("told", "muse", at(1)))

        w.muse(1.5, f"plan {w.version()} read")
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.owes, r.since), ("acknowledged", "", "muse", at(1.5)))
        self.assertIn("fb-bot/wip-j101", r.next)

        w.branches.append(br("fb-bot/wip-j101", "w1"))
        w.head_at["w1"] = at(2)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.owes, r.since), ("started", "muse", at(2)))
        self.assertIn("fb-bot/integrate-j101", r.next)

        w.branches.append(br("fb-bot/integrate-j101", "h1"))
        w.works["h1"] = M.branch_work("fb-bot/integrate-j101", "h1", "running", at(3))
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.owes), ("pushed", "running", "ci"))

        w.works["h1"] = M.branch_work("fb-bot/integrate-j101", "h1", "green", at(4), auto_pr=True)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.owes), ("proven", "auto-pr", "switchboard"))

        w.prs.append({"number": 900, "state": "open", "body": "Job: J-101\n",
                      "head": {"ref": "fb-bot/integrate-j101", "sha": "h1"}})
        w.works["h1"] = M.Work("fb-bot/integrate-j101", "h1", "green", at=at(4), pr=900)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.owes, r.next), ("proven", "", "reviewer", "review #900 @ h1"))

        w.works["h1"] = M.Work("fb-bot/integrate-j101", "h1", "green", land=True, at=at(5), pr=900)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.owes, r.next), ("reviewed", "switchboard", "merge #900"))

        w.prs.clear()
        w.inactive.add("h1")
        w.inactive.add("w1")   # the wip head is in trunk too
        w.merged_at["J-101"] = iso(6)
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.owes, r.since), ("landed", "integrator", at(6)))
        self.assertIn("#101", r.next)

        w.close("J-101")
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.limit), ("closed", None))

    def test_every_state_is_in_the_table_and_names_a_next_step(self):
        self.assertEqual({state for state, _ in M.TABLE} - {"*"}, set(M.STATES))
        for (state, gate), rule in M.TABLE.items():
            self.assertTrue(rule.next, (state, gate))
            self.assertTrue(rule.owes or state == "held", (state, gate))
            self.assertTrue(rule.limit is None or rule.limit >= M.NOW)

    def test_a_held_job_owes_nothing_and_a_job_behind_another_is_blocked_on_its_owner(self):
        rows = World().rows()
        self.assertEqual((rows["J-105"].state, rows["J-105"].owes, rows["J-105"].limit), ("held", "", None))
        self.assertIn("Cory parked it", rows["J-105"].next)
        self.assertEqual((rows["J-104"].state, rows["J-104"].owes, rows["J-104"].next),
                         ("blocked", "muse", "land J-101 first"))

    def test_a_hold_outranks_a_branch_but_never_an_open_pr(self):
        w = World()
        w.branches.append(br("fb-bot/integrate-j105", "h5"))
        self.assertEqual(w.rows()["J-105"].state, "held")
        w.prs.append({"number": 905, "state": "open", "body": "Job: J-105\n",
                      "head": {"ref": "fb-bot/integrate-j105", "sha": "h5"}})
        w.works["h5"] = M.Work("fb-bot/integrate-j105", "h5", "green", land=True, at=at(1), pr=905)
        self.assertEqual((w.rows()["J-105"].state, w.rows()["J-105"].next), ("reviewed", "merge #905"))

    def test_claude_wrote_the_plan_so_its_jobs_need_no_notice_and_grok_has_no_channel(self):
        rows = World().rows()
        self.assertEqual((rows["J-201"].state, rows["J-201"].owes), ("acknowledged", "claude"))
        self.assertEqual((rows["J-301"].state, rows["J-301"].gate, rows["J-301"].owes, rows["J-301"].limit),
                         ("planned", "by-hand", "integrator", None))

    def test_a_dependency_that_closed_unblocks(self):
        w = World()
        w.close("J-101")
        self.assertEqual(w.rows()["J-104"].state, "planned")


class Lanes(unittest.TestCase):
    """An owner's limit counts its jobs in started..reviewed; only free lanes are owed a start."""

    def acked(self) -> World:
        w = World()
        w.tell(1)
        w.muse(2, f"J-101 note, plan {w.version()}")
        return w

    def test_only_as_many_jobs_as_free_lanes_are_owed_a_start_in_plan_order(self):
        w = self.acked()
        rows = w.rows()
        self.assertEqual([rows[j].gate for j in ("J-101", "J-102", "J-103")], ["", "", "queued"])
        self.assertIsNone(rows["J-103"].limit)
        self.assertIn("free lane", rows["J-103"].why)

    def test_work_in_flight_takes_the_lanes(self):
        w = self.acked()
        w.branches += [br("fb-bot/wip-j102", "a"), br("fb-bot/integrate-j103", "b")]
        rows = w.rows()
        self.assertEqual(M.wip(w.model, w.table)["muse"], (2, 2))
        self.assertEqual((rows["J-101"].state, rows["J-101"].gate), ("acknowledged", "queued"))
        self.assertIn("2 of 2 lanes", rows["J-101"].why)
        self.assertFalse(rows["J-101"].late(at(999)))   # at the limit nobody is late for not starting

    def test_real_work_over_the_limit_is_counted_never_hidden(self):
        w = self.acked()
        w.branches += [br("fb-bot/wip-j101", "a"), br("fb-bot/wip-j102", "b"), br("fb-bot/integrate-j103", "c")]
        w.rows()
        self.assertEqual(M.wip(w.model, w.table)["muse"], (3, 2))

    def test_held_closed_and_landed_jobs_hold_no_lane(self):
        w = self.acked()
        w.branches.append(br("fb-bot/integrate-j105", "h5"))   # held: the branch stays, the lane does not
        w.merged_at["J-102"] = iso(1)
        w.rows()
        self.assertEqual(M.wip(w.model, w.table)["muse"], (0, 2))


class Conflict(unittest.TestCase):
    """One rule: evidence that disagrees is state conflict, and the next step is the integrator's."""

    def conflict(self, w: World, job: str, *needles: str) -> M.Step:
        r = w.rows()[job]
        self.assertEqual((r.state, r.owes, r.limit), ("conflict", "integrator", M.NOW), r)
        for needle in needles:
            self.assertIn(needle, r.why)
        self.assertTrue(r.late(T0))
        return r

    def test_two_agents_branches_for_one_job(self):   # J-109, J-038: built twice
        w = World()
        w.branches += [br("claude/integrate-j201", "c1"), br("fb-bot/integrate-j201", "m1")]
        self.conflict(w, "J-201", "claude", "fb-bot/integrate-j201")

    def test_a_landed_foreign_head_is_history_not_a_conflict(self):
        w = World()
        w.branches += [br("fb-bot/integrate-j201", "m1")]
        w.inactive.add("m1")
        self.assertNotEqual(w.rows()["J-201"].state, "conflict")

    def test_an_open_issue_the_plan_does_not_hold(self):   # J-105..J-128 created bare
        w = World()
        w.issues.append(issue(999, "J-999"))
        r = self.conflict(w, "J-999", "#999", "no line")
        self.assertEqual(r.owner, "")

    def test_a_strangers_issue_with_a_jobs_title_is_not_a_job(self):   # the repo is public
        w = World()
        before = (w.version(), {j: (r.state, r.gate) for j, r in w.rows().items()})
        w.issues.insert(0, dict(issue(1101, "J-101", body="another order"), author_association="NONE"))
        w.issues.append(dict(issue(999, "J-999"), author_association="FIRST_TIME_CONTRIBUTOR"))
        rows = w.rows()
        self.assertEqual((w.version(), {j: (r.state, r.gate) for j, r in rows.items()}), before)
        self.assertEqual(M.versions(w.model, w.issues)["muse"], before[0])   # its text is no part of the order
        w.issues[0]["labels"] = [{"name": "job"}]      # the team labeled it: now it is a second issue for J-101
        self.assertEqual(w.rows()["J-101"].state, "conflict")

    def test_two_open_issues_for_one_job(self):
        w = World()
        w.issues.append(issue(1101, "J-101"))
        self.conflict(w, "J-101", "#101 and #1101")

    def test_a_plan_line_with_the_wrong_issue_number(self):
        w = World()
        w.issues = [i for i in w.issues if i["number"] != 101] + [issue(100, "J-101")]
        self.conflict(w, "J-101", "#101")

    def test_an_issue_that_vanished_from_the_listing_is_not_closed_until_read_closed(self):
        w = World()
        w.issues = [i for i in w.issues if i["number"] != 101]
        self.conflict(w, "J-101", "no open issue")
        w.closed.add("J-101")
        self.assertEqual(w.rows()["J-101"].state, "closed")

    def test_a_closed_issue_with_unlanded_work(self):   # closed-unmerged #389, #367
        w = World()
        w.close("J-102")
        w.branches.append(br("fb-bot/integrate-j102", "x"))
        self.conflict(w, "J-102", "closed", "fb-bot/integrate-j102")

    def test_a_commit_only_a_mail_names_is_its_owners_push_owed_at_once(self):   # J-106: f5ee22d6b, a local head
        w = World()
        before = w.rows()["J-103"]
        w.muse(2, "J-103 red on its baseline", "matron-ladder fails on baseline f5ee22d6b")
        w.missing["J-103"] = "f5ee22d6b"
        r = w.rows()["J-103"]
        # the state is still what GitHub shows; the step is the push, the owner's, and it is late the moment it is read
        self.assertEqual((r.state, r.gate, r.owes, r.limit), (before.state, "unpushed", "muse", M.NOW))
        self.assertEqual(r.since, at(2))
        self.assertIn("push f5ee22d6b to GitHub", r.next)
        self.assertTrue(r.late(at(2)))
        self.assertIn("belongs to muse", r.alarm(at(3)))
        # a head of the job pushed after that mail ends the claim: what was done next is on the remote
        w.branches.append(br("fb-bot/wip-j103", "w103"))
        w.head_at["w103"] = at(1)
        self.assertEqual(w.rows()["J-103"].gate, "unpushed")
        w.head_at["w103"] = at(3)
        self.assertEqual((w.rows()["J-103"].state, w.rows()["J-103"].gate), ("started", ""))
        # a conflicting job keeps its own row, and a job the plan gives someone else is not the worker's to push
        w.missing["J-201"] = "0123abcd"
        self.assertEqual(w.rows()["J-201"].gate, "")
        w.branches.append(br("claude/integrate-j103", "x"))
        self.assertEqual(w.rows()["J-103"].state, "conflict")

    def test_the_worker_mails_a_start_on_a_job_the_plan_gives_someone_else(self):
        w = World()
        w.muse(1, "J-201 started")
        self.conflict(w, "J-201", "muse mailed", "claude")

    def test_the_worker_mails_a_start_on_a_held_job(self):
        w = World()
        w.muse(1, "J-105 started")
        self.conflict(w, "J-105", "holds it")

    def test_a_hand_mail_that_orders_what_the_plan_does_not_hold(self):   # 6825967953: taken by mail only
        w = World()
        w.hand(1, "J-102 taking it, not yours")
        self.conflict(w, "J-102", "hand mail", "still gives it to muse")
        w2 = World()
        w2.hand(1, "J-201 is yours, start it")
        self.conflict(w2, "J-201", "gives it to claude")
        w3 = World()
        w3.hand(1, "J-103 stop, parked")
        self.conflict(w3, "J-103", "neither holds it nor moves it")

    def test_a_hand_mail_that_agrees_with_the_plan_is_no_conflict(self):
        w = World()
        w.hand(1, "J-201 taking it")          # the plan gives J-201 to Claude
        w.hand(1, "J-105 stop, parked")       # the plan holds J-105
        w.hand(1, "J-102 FIX on #900 @ 1a2b3c4d")   # a notice, not an order
        rows = w.rows()
        self.assertEqual([rows[j].state for j in ("J-201", "J-105", "J-102")], ["acknowledged", "held", "planned"])

    def test_a_newer_hand_mail_supersedes_an_older_one(self):
        w = World()
        w.hand(1, "J-102 taking it")
        w.hand(2, "J-102 is yours again, start it")
        self.assertNotEqual(w.rows()["J-102"].state, "conflict")


class Alarm(unittest.TestCase):
    """The only alarm: job J in state S past its limit; the next step belongs to X."""

    def test_a_job_is_late_only_past_its_states_limit(self):
        w = World()
        w.tell(1)
        r = w.rows()["J-101"]
        self.assertFalse(r.late(at(2.9)))
        self.assertTrue(r.late(at(3.1)))
        self.assertEqual(r.alarm(at(4)), f'J-101 in told 1 h past its limit (2 h): next step (answer the notice: put '
                                         f'"plan {w.version()}" in a mail subject) belongs to muse')

    def test_one_unanswered_notice_is_one_alarm_line_not_one_per_job(self):
        w = World()
        w.tell(1)
        w.rows()
        lines = M.alarm_lines(w.table, at(4))
        self.assertEqual(len([ln for ln in lines if " in told " in ln]), 1)
        self.assertIn("J-101 and 2 more (J-102, J-103)", lines[0])

    def test_a_start_that_is_only_a_mail_owes_a_push_in_two_hours(self):   # R5: a started lane was a mail
        w = World()
        w.tell(1)
        w.muse(2, f"plan {w.version()}")
        w.muse(3, "J-101 started")
        r = w.rows()["J-101"]
        self.assertEqual((r.state, r.gate, r.owes, r.since), ("acknowledged", "claimed", "muse", at(3)))
        self.assertFalse(r.late(at(4.9)))
        self.assertTrue(r.late(at(5.1)))
        self.assertEqual(M.wip(w.model, w.table)["muse"][0], 0)   # a mail is not a lane
        w.branches.append(br("fb-bot/wip-j101", "w1"))
        self.assertEqual(w.rows()["J-101"].state, "started")

    def test_a_wip_branch_with_no_commit_for_three_days_is_late(self):
        w = World()
        w.branches.append(br("fb-bot/wip-j101", "w1"))
        w.head_at["w1"] = at(0)
        r = w.rows()["J-101"]
        self.assertFalse(r.late(at(71)))
        self.assertTrue(r.late(at(73)))
        self.assertIn("belongs to muse", r.alarm(at(73)))

    def test_a_state_with_no_clock_is_never_late(self):
        rows = World().rows()
        for job in ("J-104", "J-105", "J-301"):
            self.assertFalse(rows[job].late(at(10_000)), job)

    def test_an_alarm_is_new_when_its_step_or_its_ower_changes_and_not_otherwise(self):
        w = World()
        w.tell(1)
        k1 = w.rows()["J-101"].key()
        self.assertEqual(k1, w.rows()["J-101"].key())
        w.muse(2, f"plan {w.version()}")
        self.assertNotEqual(k1, w.rows()["J-101"].key())


class Version(unittest.TestCase):
    """The notice version hashes only the authored order: never a state, a title or another owner's lines."""

    def test_work_moving_keeps_the_version_so_no_new_notice_is_minted(self):
        w = World()
        v = w.version()
        w.tell(1)
        w.muse(2, f"plan {v}")
        w.branches += [br("fb-bot/wip-j101", "a"), br("fb-bot/integrate-j102", "b")]
        w.prs.append({"number": 900, "state": "open", "body": "Job: J-102\n",
                      "head": {"ref": "fb-bot/integrate-j102", "sha": "b"}})
        w.merged_at["J-103"] = iso(3)
        rows = w.rows()
        self.assertEqual([rows[j].state for j in ("J-101", "J-102", "J-103")], ["started", "pushed", "landed"])
        self.assertEqual(w.version(), v)
        self.assertTrue(all(M.collect(w.model, {"issues": w.issues})[j].version == v for j in ("J-101", "J-102")))

    def test_a_title_or_another_owners_line_does_not_change_it(self):
        a = plan.parse(PLAN)
        b = plan.parse(PLAN.replace("| first", "| a new title").replace("J-201 claude #201", "J-201 claude #201 hold: x"))
        self.assertEqual(M.order_version(a, "muse"), M.order_version(b, "muse"))
        self.assertNotEqual(M.order_version(a, "claude"), M.order_version(b, "claude"))

    def test_owner_order_hold_dependency_limit_and_issue_text_each_change_it(self):
        base = M.order_version(plan.parse(PLAN), "muse")
        for changed in (PLAN.replace("limit muse 2", "limit muse 3"),
                        PLAN.replace("J-103 muse #103 | third\n", "").replace("J-101 muse", "J-103 muse #103 | third\nJ-101 muse"),
                        PLAN.replace("J-102 muse #102", "J-102 muse #102 hold: wait"),
                        PLAN.replace("J-103 muse #103", "J-103 muse #103 after J-102"),
                        PLAN.replace("J-103 muse #103", "J-103 claude #103")):
            self.assertNotEqual(M.order_version(plan.parse(changed), "muse"), base, changed)
        self.assertNotEqual(M.order_version(plan.parse(PLAN), "muse", {"J-101": "new text"}), base)

    def test_a_changed_order_untells_the_owner_until_the_new_version_is_sent_and_answered(self):
        w = World()
        w.tell(1)
        w.muse(2, f"plan {w.version()}")
        self.assertEqual(w.rows()["J-102"].state, "acknowledged")
        old = w.version()
        w.issues[2]["body"] = "the job text changed"
        self.assertNotEqual(w.version(), old)
        self.assertEqual(w.rows()["J-102"].state, "planned")
        w.tell(3)
        self.assertEqual(w.rows()["J-102"].state, "told")
        w.muse(4, f"plan {old}")   # a receipt for the old version answers nothing
        self.assertEqual(w.rows()["J-102"].state, "told")
        w.muse(5, f"plan {w.version()}")
        self.assertEqual(w.rows()["J-102"].state, "acknowledged")


class MailTrust(unittest.TestCase):
    """The repo is public and ids look like commits: who posted it and what a sha is are checked, not assumed."""

    def test_a_receipt_from_a_login_outside_the_team_is_not_a_receipt(self):
        w = World()
        w.tell(1)
        w.muse(2, f"plan {w.version()}", login="mallory")
        self.assertEqual(w.rows()["J-101"].state, "told")

    def test_a_receipt_before_the_notice_or_a_notice_not_from_the_bot_proves_nothing(self):
        w = World()
        w.muse(0.5, f"plan {w.version()}")
        w.tell(1)
        self.assertEqual(w.rows()["J-101"].state, "told")
        w2 = World()
        w2.comments.append(comment(1, 1, f"{M.NOTICE_HEAD}x\n" + M.NOTICE_MARK.format(owner="muse", ver=w2.version(), jobs="")))
        self.assertEqual(w2.rows()["J-101"].state, "planned")

    def test_a_start_typed_by_a_stranger_is_not_a_start(self):
        w = World()
        w.muse(1, "J-201 started", login="mallory")
        self.assertNotEqual(w.rows()["J-201"].state, "conflict")

    def test_plan_versions_obligation_ids_and_mail_ids_are_never_read_as_commits(self):
        body = ("plan 2c7fcb6a91d0 read. /ack O-2c7fcb6a done. mail 1a1d05f880ea4e5d resent, id 6825967953ab. "
                "The fix is at f5ee22d6b and 7791c2a8.")
        post = M.Post(1, 106, T0, "J-106 failing test", body, False)
        found = M.named_shas(M.Mail((post,)), {106: "J-106"})
        self.assertEqual(found["J-106"][1], ("f5ee22d6b", "7791c2a8"))

    def test_a_mail_is_about_the_job_in_its_subject_else_the_issue_it_was_filed_on(self):
        a = M.Post(1, 106, T0, "the test fails", "see 1234abcd", False)
        b = M.Post(2, 340, T0, "J-103 pushed", "see 5678abcd", False)
        found = M.named_shas(M.Mail((a, b)), {106: "J-106"})
        self.assertEqual({j: v[1] for j, v in found.items()}, {"J-106": ("1234abcd",), "J-103": ("5678abcd",)})

    def test_not_started_is_not_a_start(self):
        post = M.Post(1, 340, T0, "J-101 not started, J-102 never started", "", False)
        self.assertEqual(M.started_posts(M.Mail((post,))), {})


def verdict(vendor: str, who: str, word: str, sha: str, hours: float, login: str = "IssisX", kind: str = "review"):
    return rules.Verdict(vendor, who, kind, word, sha, iso(hours), login, False)


def facts(**kw) -> rules.PrFacts:
    base = dict(number=473, head_sha="a" * 40, head_ref="fb-bot/integrate-j101", base_ref=rules.TRUNK, draft=False,
                mergeable=True, head_committed_at=iso(0), proof="green", verdicts=(), needs=frozenset(),
                rule_check_block=False, touches_workflows=False)
    base.update(kw)
    return rules.PrFacts(**base)


class WorkOf(unittest.TestCase):
    """An open PR as the merge rule read it becomes the machine's gate; the merge rule itself is not touched."""

    def work(self, f: rules.PrFacts, **kw) -> M.Work:
        d = rules.merge_decision(f, TEAM.vendor_of_branch(f.head_ref), TEAM, kw.pop("token", False), kw.pop("key", False))
        return M.work_of(f, d, TEAM, **kw)

    def test_proof_states(self):
        self.assertEqual((self.work(facts(proof="running")).state, self.work(facts(proof="running")).gate), ("pushed", "running"))
        red = self.work(facts(proof="red: native, touch"))
        self.assertEqual((red.state, red.gate, red.why), ("pushed", "red", "native, touch"))
        self.assertEqual(self.work(facts(proof="none")).gate, "none")

    def test_green_with_no_review_is_proven_and_owed_by_the_reviewer(self):
        w = self.work(facts(), proof_at=iso(2))
        self.assertEqual((w.state, w.gate, w.at), ("proven", "", at(2)))

    def test_a_land_from_another_vendor_on_the_head_is_reviewed(self):
        w = self.work(facts(verdicts=(verdict("openai", "Codex", "LAND", "a" * 40, 3),)), proof_at=iso(2))
        self.assertEqual((w.state, w.gate, w.at), ("reviewed", "", at(3)))

    def test_a_land_by_the_authors_own_vendor_or_on_an_old_head_does_not_count(self):
        self.assertEqual(self.work(facts(verdicts=(verdict("meta", "Muse", "LAND", "a" * 40, 3),))).state, "proven")
        self.assertEqual(self.work(facts(verdicts=(verdict("openai", "Codex", "LAND", "b" * 40, 3),))).state, "proven")

    def test_an_open_fix_is_the_owners_step_and_the_head_is_not_reviewed(self):
        w = self.work(facts(verdicts=(verdict("openai", "Codex", "LAND", "a" * 40, 2),
                                      verdict("anthropic", "Claude", "FIX", "a" * 40, 3))))
        self.assertEqual((w.state, w.gate, w.at), ("proven", "fix", at(3)))
        row = M.step(M.Evidence("J-101", plan.parse(PLAN).by_job()["J-101"], (101,), work=w))
        self.assertEqual((row.owes, row.limit), ("muse", M.hours(6)))

    def test_a_fix_whose_answer_sits_on_another_branch_is_said_plainly(self):   # #343: the stacked fix
        w = self.work(facts(number=343, verdicts=(verdict("openai", "Codex", "FIX", "a" * 40, 3),)),
                      stacked="fb-bot/integrate-j101-fix")
        row = M.step(M.Evidence("J-101", plan.parse(PLAN).by_job()["J-101"], (101,), work=w))
        self.assertEqual((row.gate, row.owes), ("stacked", "muse"))
        self.assertEqual(row.next, "push the fix onto fb-bot/integrate-j101: it sits on fb-bot/integrate-j101-fix, "
                                   "which waits for #343 to land")

    def test_a_workflow_pr_waiting_on_codex_says_the_one_thing_the_integrator_must_do(self):   # #473
        api = rules.Verdict("anthropic", "Claude", "review", "LAND", "a" * 40, iso(2), rules.BOT_LOGIN, False, signed=True)
        f = facts(head_ref="claude/integrate-switch", touches_workflows=True,
                  verdicts=(api, verdict("meta", "Muse", "LAND", "a" * 40, 3)))
        w = self.work(f, token=True, key=True)
        self.assertEqual((w.state, w.gate), ("reviewed", "codex"))
        ln = plan.parse(PLAN).by_job()["J-201"]
        row = M.step(M.Evidence("J-201", ln, (201,), work=w))
        self.assertEqual((row.owes, row.limit), ("integrator", M.hours(1)))
        self.assertEqual(row.next, "post `@codex review` on #473 (only a person's login wakes Codex)")
        self.assertLess(len(row.alarm(at(9))), 200)

    def test_the_switchs_own_codex_ask_on_a_green_head_is_the_integrators_step(self):
        w = self.work(facts(), codex_asked=True)
        self.assertEqual((w.state, w.gate), ("proven", "codex"))

    def test_a_workflow_pr_with_no_token_is_merged_by_hand(self):
        w = self.work(facts(touches_workflows=True, verdicts=(verdict("openai", "Codex", "LAND", "a" * 40, 3),)))
        self.assertEqual((w.state, w.gate), ("reviewed", "hand"))
        self.assertIn("workflow token cannot merge", w.why)

    def test_a_base_conflict_a_draft_and_a_stacked_base(self):
        self.assertEqual(self.work(facts(mergeable=False, verdicts=(verdict("openai", "Codex", "LAND", "a" * 40, 3),))).gate, "base")
        self.assertEqual(self.work(facts(draft=True)).gate, "draft")
        self.assertEqual(self.work(facts(base_ref="fb-bot/integrate-j100")).gate, "off-trunk")

    def test_the_apis_note_is_ruled_by_the_integrator(self):
        note = rules.Verdict("anthropic", "Claude", "review", "NOTE", "a" * 40, iso(2), rules.BOT_LOGIN, False)
        self.assertEqual(self.work(facts(verdicts=(note,))).gate, "note")

    def test_a_held_or_reassigned_job_still_merges(self):   # Cory 11-12: merges are not the plan's to stop
        f = facts(head_ref="fb-bot/integrate-j105", verdicts=(verdict("openai", "Codex", "LAND", "a" * 40, 3),))
        d = rules.merge_decision(f, "meta", TEAM)
        self.assertEqual(d.action, "MERGE")
        w = World()
        w.branches.append(br("fb-bot/integrate-j105", "a" * 40))
        w.prs.append({"number": 473, "state": "open", "body": "Job: J-105\n",
                      "head": {"ref": "fb-bot/integrate-j105", "sha": "a" * 40}})
        w.works["a" * 40] = M.work_of(f, d, TEAM)
        self.assertEqual((w.rows()["J-105"].state, w.rows()["J-105"].next), ("reviewed", "merge #473"))


class Views(unittest.TestCase):
    """Labels, the board, the worker's queue and its notice are made from the table."""

    def test_labels_follow_the_plan(self):
        w = World()
        w.issues[0]["labels"] = [{"name": "job"}, {"name": "owner:claude"}, {"name": "state:active"}, {"name": "needs:eyes"}]
        w.issues[4]["labels"] = [{"name": "job"}, {"name": "owner:muse"}]
        w.issues[5]["labels"] = [{"name": "job"}, {"name": "owner:claude-lane-a"}]
        changes = {n: (add, rem) for n, add, rem in M.labels(w.model, w.issues, {"claude": frozenset({"owner:claude-lane-a"})})}
        self.assertEqual(changes[101], (("owner:muse",), ("owner:claude", "state:active")))   # needs:eyes is not the plan's
        self.assertEqual(changes[105], (("state:parked",), ()))
        self.assertNotIn(201, changes)                        # an alias of the owner's label is left alone
        self.assertEqual(changes[102], (("job", "owner:muse"), ()))   # a bare issue gets its labels

    def test_a_label_is_never_written_to_an_issue_the_plan_cannot_prove_is_the_jobs(self):
        w = World()
        w.issues[0]["title"] = "J-777 something else"
        self.assertNotIn(101, {n for n, _, _ in M.labels(w.model, w.issues)})

    def test_the_queue_names_work_to_start_only_while_a_lane_is_free(self):
        w = World()
        w.tell(1)
        w.muse(2, f"plan {w.version()}")
        w.branches.append(br("fb-bot/wip-j101", "a"))
        w.rows()
        q = M.queue(w.model, w.table, "muse")
        self.assertIn("In flight (1 of 2): J-101 #101", q)
        self.assertIn("Start next, in this order: J-102 #102\n", q)
        self.assertIn("Waiting for a lane or the notice: J-103 #103", q)
        self.assertIn("Blocked on another job: J-104 #104", q)
        self.assertIn("Held, not to be worked: J-105 #105", q)

    def test_the_notice_is_short_names_the_receipt_and_never_a_held_or_blocked_job_as_work(self):
        w = World()
        w.rows()
        text = M.notice(w.model, w.table, "muse", w.version(), before=["J-101", "J-777"])
        self.assertTrue(text.startswith(f"CLAUDE_TO_MUSE: plan {w.version()}\n"))
        self.assertIn("Newly yours: J-102, J-103, J-104, J-105.", text)
        self.assertIn("No longer yours: J-777.", text)
        self.assertIn("Next, in order: J-101, J-102.", text)
        self.assertIn("Held, not to be worked: J-105", text)
        self.assertIn(f'put "plan {w.version()}" in the subject', text)
        self.assertLessEqual(len(text), M.NOTICE_MAX)

    def test_at_the_limit_the_notice_names_no_job_to_start(self):
        w = World()
        w.branches += [br("fb-bot/wip-j101", "a"), br("fb-bot/wip-j102", "b")]
        w.rows()
        text = M.notice(w.model, w.table, "muse", w.version())
        self.assertIn("In flight: 2 of 2. At or over the limit", text)
        self.assertNotIn("Next, in order", text)

    def test_the_board_lists_every_open_job_with_its_state_and_who_is_next(self):
        w = World()
        w.tell(1)
        w.issues.append(issue(999, "J-999"))
        w.rows()
        text = M.board(w.model, w.table, at(9))
        self.assertIn("**muse**: 0 of 2 lanes in flight", text)
        self.assertIn("- J-101 #101 told **LATE**; next: muse, answer the notice", text)
        self.assertIn("- J-105 #105 held; none while it is held: Cory parked it", text)
        self.assertIn("**not in the plan**\n- J-999 conflict", text)


if __name__ == "__main__":
    unittest.main()
