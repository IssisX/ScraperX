"""The plan part of the sweep (Switch.plan_check) against an in-memory GitHub.

Live: one Muse job is walked from its plan line to a closed issue. Each step adds one piece of evidence the way
its real actor would (the switch's own writes land in the same GitHub the next pass reads), and the row, the one
alarm, the notice, the labels and the board section are read back.
Shadow: the same pass prints what it would do, writes nothing, and no failure in it reaches the sweep.
"""
import contextlib
import datetime as dt
import pathlib
import re
import sys
import tempfile
import unittest
import urllib.parse

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import machine as M  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402

REPO = "IssisX/ScraperX"
BOARD = 340
T0 = dt.datetime(2026, 11, 12, 8, 0, tzinfo=dt.timezone.utc)
TEAM = rules.Team({"claude": "anthropic", "muse": "meta", "codex": "openai"},
                  [("claude/", "anthropic"), ("fb-bot/", "meta")], {}, ["muse"], "", {"muse": "meta", "codex": "openai"},
                  frozenset({"issisx", "github-actions[bot]"}))
PLAN = ("limit muse 6\nlimit claude 4\n"
        "J-501 muse #501 | The job this test walks\n"
        "J-502 muse #502 hold: Cory parked it | A held job\n"
        "J-601 claude #601 | Claude's own job\n")
AGENTS = "agents:\n  muse:\n    owner_labels: [\"owner:muse\"]\n  claude:\n    owner_labels: [\"owner:claude\"]\n"
WIP, INTEGRATE = "fb-bot/wip-j501", "fb-bot/integrate-j501"
W1, I1 = "a1" + "0" * 38, "b2" + "0" * 38


def at(hours: float) -> dt.datetime:
    return T0 + dt.timedelta(hours=hours)


def iso(hours: float) -> str:
    return at(hours).strftime("%Y-%m-%dT%H:%M:%SZ")


class Hub:
    """GitHub as the switch's client reads and writes it. Reads answer from what is here; writes change it."""
    repo = REPO

    def __init__(self):
        self.issues: dict[int, dict] = {}
        self.comments: list[dict] = []
        self.branches: dict[str, str] = {}     # name -> head sha
        self.pushed: dict[str, str] = {}       # sha -> its commit time
        self.in_trunk: set[str] = set()        # heads trunk already holds
        self.pulls: list[dict] = []
        self.proofs: dict[str, tuple] = {}     # sha -> switch.proof()'s answer
        self.writes: list[tuple[str, str]] = []
        self.reads: list[str] = []
        self.fail: dict[str, Exception] = {}   # a piece of a path -> what reading it raises
        self.clock = iso(0)
        self.state: dict = {}
        self.fresh_only = False
        self.issue(BOARD, "Board", ["board"], body="Written by a person.\n")

    # the world
    def issue(self, number: int, title: str, labels=(), body: str = "") -> None:
        self.issues[number] = {"number": number, "title": title, "state": "open", "body": body,
                               "labels": [{"name": n} for n in labels]}

    def labels(self, number: int) -> set[str]:
        return {lab["name"] for lab in self.issues[number]["labels"]}

    def post(self, number: int, body: str, login: str, when: str) -> dict:
        c = {"id": 1000 + len(self.comments), "body": body, "user": {"login": login}, "created_at": when,
             "updated_at": when, "issue_url": f"https://api.github.com/repos/{REPO}/issues/{number}"}
        self.comments.append(c)
        return c

    def mail(self, hours: float, subject: str, body: str = "", number: int = BOARD) -> dict:
        """A mail from Muse as the bridge files it: the team login, the bridge's first line."""
        return self.post(number, f"**Muse (via email bridge): {subject}**\n\n{body}", "IssisX", iso(hours))

    def push(self, name: str, sha: str, hours: float) -> None:
        self.branches[name], self.pushed[sha] = sha, iso(hours)

    def by(self, login: str, needle: str = "") -> list[dict]:
        return [c for c in self.comments if c["user"]["login"] == login and needle in c["body"]]

    # the client
    @contextlib.contextmanager
    def fresh(self):
        yield self

    def get(self, path: str, accept: str = ""):
        self.reads.append(path)
        for piece, err in self.fail.items():
            if piece in path:
                raise err
        base, _, query = path.partition("?")
        q = dict(urllib.parse.parse_qsl(query))
        rest = base.removeprefix(f"repos/{REPO}/")
        page = int(q.get("page", 1))

        def some(items: list) -> list:
            return items[(page - 1) * 100: page * 100]

        if rest == "issues":
            want = q.get("labels")
            return some([i for n, i in sorted(self.issues.items())
                         if i["state"] == q.get("state", "open") and (not want or want in self.labels(n))])
        if rest == "branches":
            return some([{"name": n, "commit": {"sha": s}} for n, s in self.branches.items()])
        if rest == "pulls":
            return some([p for p in self.pulls if p["state"] == q["state"]])
        if rest == "commits":
            return [{"commit": {"committer": {"date": iso(0)}}}]
        if rest == "issues/comments":
            return some(sorted((c for c in self.comments if c["updated_at"] >= q["since"]), key=lambda c: c["created_at"]))
        if m := re.fullmatch(r"compare/[^.]+\.\.\.([0-9a-f]+)", rest):
            sha = m.group(1)
            if sha in self.in_trunk:
                return {"status": "behind", "commits": []}
            return {"status": "ahead", "commits": [{"sha": sha, "commit": {"committer": {"date": self.pushed[sha]}}}]}
        if m := re.fullmatch(r"commits/([0-9a-f]+)", rest):
            full = next((s for s in self.pushed if s.startswith(m.group(1))), "")
            if not full:
                raise switch.GitHubError("GET", path, "gh: No commit found (HTTP 422)", 422)
            job = next((re.search(r"j(\d{3,})", n) for n, s in self.branches.items() if s == full), None)
            return {"sha": m.group(1), "commit": {"message": f"Job: J-{job.group(1)}\n" if job else "a commit\n",
                                                   "committer": {"date": self.pushed[full]}}}
        if m := re.fullmatch(r"issues/(\d+)/comments", rest):
            return some([c for c in self.comments if c["issue_url"].endswith(f"/{m.group(1)}")])
        if m := re.fullmatch(r"issues/(\d+)", rest):
            return self.issues[int(m.group(1))]
        raise AssertionError(f"a read this test did not expect: {path}")

    def paged(self, path: str, key: str = "") -> list:
        sep, items = "&" if "?" in path else "?", []
        for page in range(1, 6):
            batch = self.get(f"{path}{sep}per_page=100&page={page}")
            items.extend(batch)
            if len(batch) < 100:
                break
        return items

    def write(self, method: str, path: str, body=None, what: str = "", token: str = ""):
        rest = path.removeprefix(f"repos/{REPO}/")
        self.writes.append((method, rest))
        if m := re.fullmatch(r"issues/(\d+)/comments", rest):
            return self.post(int(m.group(1)), body["body"], rules.BOT_LOGIN, self.clock)
        if m := re.fullmatch(r"issues/(\d+)/labels", rest):
            have = self.issues[int(m.group(1))]["labels"]
            have += [{"name": n} for n in body["labels"] if {"name": n} not in have]
        elif m := re.fullmatch(r"issues/(\d+)/labels/(.+)", rest):
            issue = self.issues[int(m.group(1))]
            issue["labels"] = [lab for lab in issue["labels"] if lab["name"] != urllib.parse.unquote(m.group(2))]
        elif m := re.fullmatch(r"issues/comments/(\d+)", rest):
            c = next(c for c in self.comments if c["id"] == int(m.group(1)))
            c["body"], c["updated_at"] = body["body"], self.clock
        elif m := re.fullmatch(r"issues/(\d+)", rest):
            self.issues[int(m.group(1))]["body"] = body["body"]
        else:
            raise AssertionError(f"a write this test did not expect: {method} {path}")
        return None

    def comment(self, number: int, body: str, what: str = ""):
        return self.write("POST", f"repos/{REPO}/issues/{number}/comments", {"body": body}, what)


class Sweeps(unittest.TestCase):
    mode = "live"

    def setUp(self):
        self.hub = Hub()
        self.hub.issue(501, "J-501: The job this test walks", ["job"])
        self.hub.issue(502, "J-502: A held job", ["job", "owner:muse"])
        self.hub.issue(601, "J-601: Claude's own job", ["job", "owner:claude"])
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = pathlib.Path(tmp.name)
        (self.root / "work").mkdir()
        self.write("work/switchboard.yml", f"plan: {self.mode}\n")
        self.write("work/plan.md", PLAN)
        self.write("work/agents.yml", AGENTS)

    def write(self, name: str, text: str) -> None:
        (self.root / name).write_text(text, encoding="utf-8")

    def sweep(self, hours: float, steered=()):
        """One full pass's plan part at T0 + hours, as run_sweep calls it: a new Switch (nothing is remembered
        between runs but what is on GitHub). Returns the switch and the rows it computed."""
        self.hub.clock = iso(hours)
        self.hub.writes, self.hub.reads = [], []
        sw = switch.Switch.__new__(switch.Switch)
        sw.gh, sw.team, sw.root, sw.now, sw.lines, sw._board_issue = self.hub, TEAM, self.root, at(hours), [], BOARD
        sw.out = sw.lines.append
        sw.proof = lambda sha: self.hub.proofs.get(sha, ("none", "", 0, ""))
        sw.plan_check(list(steered))
        memo = sw.__dict__.get("_plan_table")
        return sw, {r.job: r for r in memo[3]} if memo else {}

    def version(self) -> str:
        return M.versions(plan.parse(PLAN))["muse"]

    def jobs(self, hours: float) -> str:
        """The mark's jobs for Muse's lines first told at T0 + hours."""
        return ",".join(f"{ln.job}:{M.line_hash(ln)}@{M.stamp(at(hours))}" for ln in plan.parse(PLAN).owner_lines("muse"))

    def pr(self, hours: float, verdicts=()) -> "switch.Steered":
        f = rules.PrFacts(number=900, head_sha=I1, head_ref=INTEGRATE, base_ref=rules.TRUNK, draft=False,
                          mergeable=True, head_committed_at=iso(6), proof="green", verdicts=tuple(verdicts),
                          needs=frozenset(), rule_check_block=False, touches_workflows=False)
        d = rules.merge_decision(f, TEAM.vendor_of_branch(INTEGRATE), TEAM, True, True)
        return switch.Steered(f, {"comments": [], "proof_done_at": iso(6.5), "job": "J-501"}, d)


class Live(Sweeps):
    def test_one_job_from_its_plan_line_to_a_closed_issue(self):
        hub, ver = self.hub, self.version()

        # 1. planned -> told: the plan holds the job, so the switch sends the notice, once
        sw, rows = self.sweep(0.5)
        self.assertEqual((rows["J-501"].state, rows["J-501"].owes), ("planned", "switchboard"))
        (notice,) = hub.by(rules.BOT_LOGIN, M.NOTICE_HEAD)
        self.assertTrue(notice["body"].startswith(f"CLAUDE_TO_MUSE: plan {ver}\n"))
        self.assertIn(M.NOTICE_MARK.format(owner="muse", ver=ver, jobs=self.jobs(0.5)), notice["body"])
        self.assertIn("Held, not to be worked: J-502", notice["body"])
        self.assertIn("Next, in order: J-501.", notice["body"])
        self.assertTrue(notice["issue_url"].endswith(f"/{BOARD}"))
        self.assertIn(switch.MUSE_LABEL, hub.labels(BOARD))            # the bridge mails what carries this label
        # the labels are a view of the plan: the owner, and parked exactly where the plan holds
        self.assertEqual(hub.labels(501), {"job", "owner:muse"})
        self.assertEqual(hub.labels(502), {"job", "owner:muse", "state:parked"})
        self.assertEqual(hub.labels(601), {"job", "owner:claude"})
        # the board's job section is a view too, and a person's words around it stay
        body = hub.issues[BOARD]["body"]
        self.assertTrue(body.startswith("Written by a person.\n"))
        self.assertEqual((body.count(M.PLAN_START), body.count(M.PLAN_END)), (1, 1))
        self.assertIn("- J-502 #502 held", body)
        self.assertEqual(sw.plan_items, ())

        # 2. told: the next pass reads its own notice back and sends nothing more
        sw, rows = self.sweep(1)
        r = rows["J-501"]
        self.assertEqual((r.state, r.owes, r.since, r.limit), ("told", "muse", at(0.5), M.hours(2)))
        self.assertEqual(r.next, f'answer the notice: put "plan {ver}" in a mail subject')
        self.assertEqual(len(hub.by(rules.BOT_LOGIN, M.NOTICE_HEAD)), 1)
        self.assertIn("- J-501 #501 told; next: muse, answer the notice", hub.issues[BOARD]["body"])
        self.assertEqual((sw.plan_items, [w for w in hub.writes if w[1].endswith("/labels")]), ((), []))
        self.assertEqual(rows["J-601"].state, "acknowledged")           # Claude writes the plan: no notice to it
        self.assertEqual((rows["J-502"].state, rows["J-502"].owes, rows["J-502"].limit), ("held", M.NOBODY, None))

        # 3. told, past its limit: the one alarm, in the integrator's inbox, and the one re-send
        sw, rows = self.sweep(3)
        self.assertTrue(rows["J-501"].late(at(3)))
        (item,) = sw.plan_items
        self.assertEqual(item.kind, "PLAN")
        self.assertIn("J-501", item.line)
        self.assertIn("belongs to muse", item.line)
        self.assertEqual((item.dest, switch.inbox_class(item)), ("muse", "ride"))   # 11-13: Muse's step, no wake
        (again,) = hub.by(rules.BOT_LOGIN, M.NOTICE_AGAIN.format(owner="muse", ver=ver))
        self.assertIn("Sent again", again["body"])
        self.assertIn("- J-501 #501 told **LATE**", hub.issues[BOARD]["body"])
        sw2, _ = self.sweep(3.5)                                        # still late: the same key, nothing sent
        self.assertEqual([i.head for i in sw2.plan_items], [item.head])
        self.assertEqual(len(hub.by(rules.BOT_LOGIN, "CLAUDE_TO_MUSE: plan ")), 2)
        self.assertFalse([w for w in hub.writes if w[1].endswith("/comments")])

        # 4. acknowledged: Muse's mail carries the receipt; the switch writes it into its own notice
        answer = hub.mail(4, f"MUSE_TO_CLAUDE: plan {ver} read")
        sw, rows = self.sweep(4.1)
        r = rows["J-501"]
        self.assertEqual((r.state, r.gate, r.owes, r.since), ("acknowledged", "", "muse", at(4)))
        self.assertEqual(r.next, f"start it: push {WIP}")
        self.assertEqual(sw.plan_items, ())
        (notice,) = hub.by(rules.BOT_LOGIN, M.NOTICE_MARK.format(owner="muse", ver=ver, jobs=self.jobs(0.5)))
        self.assertIn(M.RECEIPT_MARK.format(owner="muse", ver=ver, at=iso(4), comment=answer["id"]), notice["body"])
        hub.comments.remove(answer)                                     # the mail leaves every window that is read
        _, rows = self.sweep(4.5)
        self.assertEqual(rows["J-501"].state, "acknowledged")           # the receipt is on the notice: still read
        self.assertFalse([w for w in hub.writes if w[1].startswith("issues/comments/")])   # and written only once

        # 5. started: the lane is a pushed wip branch, nothing else
        hub.mail(4.8, "J-501 started", number=501)
        _, rows = self.sweep(4.9)
        self.assertEqual((rows["J-501"].state, rows["J-501"].gate), ("acknowledged", "claimed"))   # a mail is no lane
        hub.push(WIP, W1, 5)
        _, rows = self.sweep(5.1)
        r = rows["J-501"]
        self.assertEqual((r.state, r.owes, r.since, r.limit), ("started", "muse", at(5), M.hours(72)))
        self.assertEqual(r.next, f"push {INTEGRATE} so its proof runs")

        # 6. pushed: the integrate head; CI owes the proof
        hub.push(INTEGRATE, I1, 6)
        hub.proofs[I1] = ("running", "", 1, "")
        _, rows = self.sweep(6.1)
        self.assertEqual((rows["J-501"].state, rows["J-501"].gate, rows["J-501"].owes), ("pushed", "running", "ci"))

        # 7. proven: green on that head; the switch owes the PR, then the reviewer owes the review
        hub.proofs[I1] = ("green", "", 1, iso(6.5))
        _, rows = self.sweep(6.6)
        r = rows["J-501"]
        self.assertEqual((r.state, r.gate, r.owes, r.since), ("proven", "auto-pr", "switchboard", at(6.5)))
        hub.pulls.append({"number": 900, "state": "open", "body": "Job: J-501\n", "merged_at": None,
                          "head": {"ref": INTEGRATE, "sha": I1}})
        _, rows = self.sweep(6.8, [self.pr(6.8)])
        r = rows["J-501"]
        self.assertEqual((r.state, r.gate, r.owes, r.limit), ("proven", "", "reviewer", M.hours(3)))
        self.assertEqual(r.next, f"review #900 @ {I1[:8]}")

        # 8. reviewed: another vendor's LAND on that head; the switch owes the merge
        land = rules.Verdict("openai", "Codex", "review", "LAND", I1, iso(7), rules.CODEX_LOGIN, False)
        _, rows = self.sweep(7.1, [self.pr(7.1, [land])])
        r = rows["J-501"]
        self.assertEqual((r.state, r.gate, r.owes, r.next), ("reviewed", "", "switchboard", "merge #900"))

        # 9. landed: the PR merged; the integrator owes the close
        hub.pulls[0].update(state="closed", merged_at=iso(7.5))
        hub.in_trunk.add(W1)                                            # a merge commit: trunk holds the wip head too
        sw, rows = self.sweep(7.6)
        r = rows["J-501"]
        self.assertEqual((r.state, r.owes, r.since, r.limit), ("landed", "muse", at(7.5), M.hours(24)))
        self.assertEqual(r.next, "start its next part: a part merged and #501 is still open (the last part's PR "
                                 "carries `Closes #501`); if no part is left, close #501")
        self.assertIn("muse**: 0 of 6 lanes in flight", hub.issues[BOARD]["body"])

        # 10. closed
        hub.issues[501]["state"] = "closed"
        sw, rows = self.sweep(8)
        self.assertEqual((rows["J-501"].state, rows["J-501"].limit), ("closed", None))
        self.assertNotIn("J-501", hub.issues[BOARD]["body"])
        self.assertEqual(sw.plan_items, ())
        self.assertEqual(len(hub.by(rules.BOT_LOGIN, "CLAUDE_TO_MUSE: plan ")), 2)   # one notice, one re-send, ever

    def test_a_changed_order_is_a_new_version_and_is_told_again(self):
        self.sweep(0.5)
        old = self.version()
        self.hub.mail(1, f"plan {old}")
        _, rows = self.sweep(1.1)
        self.assertEqual(rows["J-501"].state, "acknowledged")
        self.hub.issue(503, "J-503: A new job", ["job"])
        self.write("work/plan.md", PLAN + "J-503 muse #503 | A new job\n")
        _, rows = self.sweep(2)
        new = [c for c in self.hub.by(rules.BOT_LOGIN, M.NOTICE_HEAD) if old not in c["body"]]
        self.assertEqual(len(new), 1)
        self.assertIn("Newly yours: J-503.", new[0]["body"])
        _, rows = self.sweep(2.5)
        # the old receipt is not this order's: the new line is told and owes its receipt; the line that did not
        # change keeps its state and its own clock (Grok #471 hole 2: any plan commit reset every clock)
        self.assertEqual((rows["J-503"].state, rows["J-501"].state, rows["J-501"].since), ("told", "acknowledged", at(1)))
        self.assertIn(f"J-501:{M.line_hash(plan.parse(PLAN).by_job()['J-501'])}@{M.stamp(at(0.5))}!{M.stamp(at(1))}",
                      new[0]["body"])   # the mark carries the run forward: no older board is needed to read it

    def test_a_failed_read_fails_the_part_so_the_sweep_reports_it_and_nothing_is_half_written(self):
        self.hub.fail["branches"] = switch.GitHubError("GET", "branches", "HTTP 502")
        with self.assertRaises(switch.GitHubError):
            self.sweep(0.5)
        self.assertEqual(self.hub.writes, [])
        self.write("work/plan.md", PLAN + "J-501 muse #501 | twice\n")
        self.hub.fail.clear()
        with self.assertRaises(RuntimeError):
            self.sweep(0.6)
        self.assertEqual(self.hub.writes, [])

    def test_the_digest_reads_blocked_from_the_machine_not_the_label(self):   # cutover: state:blocked can go
        self.hub.issue(503, "J-503: Labelled blocked, nothing waits", ["job", "state:blocked"])
        self.hub.issue(504, "J-504: Waits on the first", ["job"])
        self.write("work/plan.md", PLAN + "J-503 muse #503 | Labelled blocked, nothing waits\n"
                                          "J-504 muse #504 after J-501 | Waits on the first\n")
        sw, rows = self.sweep(0.5)
        self.assertEqual(sw.blocked_lines(), ["#504 J-504: Waits on the first is blocked on J-501"])
        self.assertNotIn("state:blocked", self.hub.labels(503))   # and the live pass takes the label off
        self.hub.issue(503, "J-503: Labelled blocked, nothing waits", ["job", "state:blocked"])
        self.write("work/switchboard.yml", "plan: shadow\n")   # before cutover: the label, as before
        sw._jobs = None
        self.assertEqual(sw.blocked_lines(), ["#503 J-503: Labelled blocked, nothing waits is blocked"])


class Shadow(Sweeps):
    mode = "shadow"

    def test_it_prints_the_states_the_alarm_and_the_views_and_writes_nothing(self):
        sw, rows = self.sweep(0.5)
        out = "\n".join(sw.lines)
        self.assertEqual(self.hub.writes, [])
        self.assertEqual(sw.plan_items, ())
        self.assertIn("PLAN        3 jobs, 0 late, lanes in flight: none, mode=shadow", out)
        self.assertIn("PLAN-STATE  planned        1  J-501", out)
        self.assertIn("PLAN-STATE  held           1  J-502", out)
        self.assertIn("PLAN-LABEL  #501 add owner:muse; remove -", out)
        self.assertIn("PLAN-LABEL  #502 add state:parked; remove -", out)
        self.assertIn(f"PLAN-NOTICE muse plan {self.version()} (shadow: not sent)", out)
        sw, rows = self.sweep(3)   # nothing was sent, so the job is still planned, and late: the switch's own step
        self.assertEqual(self.hub.writes, [])
        self.assertEqual(sw.plan_items, ())
        self.assertIn("PLAN-LATE   J-501", "\n".join(sw.lines))
        self.assertEqual(self.hub.issues[BOARD]["body"], "Written by a person.\n")
        self.assertEqual(self.hub.labels(501), {"job"})

    def test_no_failure_in_it_reaches_the_sweep(self):
        broken = {"a read GitHub refuses": lambda: self.hub.fail.update(
                      branches=switch.GitHubError("GET", "branches", "HTTP 502")),
                  "a rate limit": lambda: self.hub.fail.update(
                      pulls=switch.RateLimited("GET", "pulls", "API rate limit exceeded")),
                  "a plan that does not parse": lambda: self.write("work/plan.md", PLAN + "not a line\n"),
                  "no plan file": lambda: (self.root / "work/plan.md").unlink(),
                  "an agents file that is not a mapping": lambda: self.write("work/agents.yml", "- a list\n"),
                  "an answer of the wrong shape": lambda: self.hub.issues.update({777: {"number": 777}})}
        for what, breaks in broken.items():
            with self.subTest(what):
                self.setUp()
                breaks()
                sw, _ = self.sweep(0.5)   # returns: nothing raised
                notes = [ln for ln in sw.lines if ln.startswith("NOTE        plan (shadow): not computed this pass")]
                self.assertEqual(len(notes), 1, sw.lines)
                self.assertEqual((self.hub.writes, sw.plan_items), ([], ()))

    def test_off_reads_nothing(self):
        self.write("work/switchboard.yml", "merge: true\n")
        sw, rows = self.sweep(0.5)
        self.assertEqual((self.hub.reads, self.hub.writes, sw.lines, rows), ([], [], [], {}))


class InTheSweep(unittest.TestCase):
    """run_sweep itself (test_sweep_calls' harness): where the plan part runs, and what its failure costs."""

    def run_sweep(self, mode: str, check, state=None, remaining=None, hour=18):
        from unittest import mock
        import test_sweep_calls as T
        now = T.NOW if state is not None else T.NOW.replace(hour=hour)   # a kept state is stamped against T.NOW
        gh = T.CountingGH([T.pr(1)])
        seen = []

        def plan_check(sw, steered=()):
            seen.append(dict(sw.gh.state.get("steer") or {}).get("stamp"))
            if remaining is None:
                return check()
            raise AssertionError("not reached under the rate floor")

        probe = switch.rate_probe

        def rate(sw):   # the sweep's first act: what GitHub says is left of the hour
            probe(sw)
            if remaining is not None:
                sw.gh.limit, sw.gh.remaining = 1000, remaining

        with mock.patch.object(switch.Switch, "plan_mode", lambda self: mode), \
                mock.patch.object(switch.Switch, "plan_check", plan_check), \
                mock.patch.object(switch, "rate_probe", rate):
            status, _, sw = T.sweep(gh, state, now, plan_mode=mode)
        return status, seen, gh.state.get("steer"), T

    def boom(self):
        raise RuntimeError("a bug in the plan part")

    def test_in_shadow_it_runs_once_after_the_sweep_state_is_set_and_its_failure_costs_nothing(self):
        quiet, _, base, T = self.run_sweep("off", lambda: None)
        for check in (lambda: None, self.boom):
            status, seen, state, _ = self.run_sweep("shadow", check)
            self.assertEqual(seen, [T.iso(T.NOW.replace(hour=18))])   # one call, after this sweep's state was written
            self.assertEqual((status, state), (quiet, base))   # the sweep ends as it does with no plan at all

    def test_live_it_runs_before_the_state_is_set_and_a_failure_is_the_sweeps_to_report_and_retry(self):
        status, seen, state, T = self.run_sweep("live", lambda: None)
        self.assertEqual((seen, status), ([None], 0))       # before the inbox and the state: its late jobs are listed
        self.assertNotIn("plan", state["retry"])
        status, seen, state, _ = self.run_sweep("live", self.boom)
        self.assertEqual((status, seen), (2, [None]))
        self.assertIn("plan", state["retry"])

    def test_in_shadow_it_is_left_out_under_the_rate_floor(self):
        quiet, _, base, _ = self.run_sweep("off", lambda: None, remaining=1)
        status, seen, state, _ = self.run_sweep("shadow", lambda: None, remaining=1)
        self.assertEqual((seen, status, state), ([], quiet, base))
        floor = switch.rate_floor(1000)
        _, seen, _, _ = self.run_sweep("shadow", lambda: None, remaining=2 * floor - 1)   # the live parts all ran
        self.assertEqual(seen, [])
        _, seen, _, _ = self.run_sweep("shadow", lambda: None, remaining=2 * floor)
        self.assertEqual(len(seen), 1)

    def test_in_shadow_it_runs_in_the_full_pass_of_every_sixth_hour_and_live_in_every_one(self):
        ran = {hour: len(self.run_sweep("shadow", lambda: None, hour=hour)[1]) for hour in range(24)}
        self.assertEqual([h for h, n in ran.items() if n], [0, 6, 12, 18])
        self.assertEqual({len(self.run_sweep("live", lambda: None, hour=hour)[1]) for hour in (5, 6, 7)}, {1})

    def test_a_fast_sweep_never_runs_it(self):
        import test_sweep_calls as T
        for mode in ("shadow", "live"):
            _, seen, _, _ = self.run_sweep(mode, self.boom, state=T.last_state([T.pr(1)], minutes=10))
            self.assertEqual(seen, [], mode)


if __name__ == "__main__":
    unittest.main()
