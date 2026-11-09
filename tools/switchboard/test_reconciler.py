"""Unit tests for the shadow reconciler (tools/switchboard/reconciler.py, J-072 Design B stage 1): the twelve fault
cases of #350 section 6, plus landed work, due times and the shadow diff.

Run: python3 -m unittest discover -s tools/switchboard -p 'test_*.py'
No network, no clock: a fake GitHub (with the Git Data API the state branch uses) and the time are given.
"""

from __future__ import annotations

import base64
import datetime as dt
import hashlib
import json
import pathlib
import sys
import tempfile
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import ledger as L  # noqa: E402
import reconciler as R  # noqa: E402
import rules  # noqa: E402
from ghapi import GitHubError  # noqa: E402
from test_ledger import AGENTS, ROSTER, NOW, SHA  # noqa: E402

REPO = "IssisX/ScraperX"
TMP = tempfile.TemporaryDirectory()


def tearDownModule():
    TMP.cleanup()
H = dt.timedelta(hours=1)
M = dt.timedelta(minutes=1)


def t(hours: float) -> dt.datetime:
    return NOW + dt.timedelta(hours=hours)


def pr(n, ref, sha="a", job="J-500", action="WAIT", reasons=(), proof="green", needs_review=False, reviewer="",
       pushed=-1.0, now=0.0):
    return L.PrIn(n, f"https://github.com/{REPO}/pull/{n}", ref, SHA[sha], f"Job: {job}\n\nbody" if job else "no job",
                  action, tuple(reasons), proof, L.iso(t(pushed + 0.2)), L.iso(t(pushed)), needs_review, reviewer,
                  t(now))


def issue(n, job="J-500", owner="owner:claude"):
    return L.IssueIn(n, f"https://github.com/{REPO}/issues/{n}", f"{job} a job", ("job", owner) if owner else ("job",),
                     t(-2))


def feed(**kw):
    base = dict(prs=[], open_prs=[], branches=[], issues=[], asks=[], runs=[],
                system_branches=frozenset({"switchboard/ledger", "claude/integrator-inbox", R.STATE_BRANCH}),
                system_prs=frozenset({282}))
    base.update(kw)
    if "open_prs" not in kw:
        base["open_prs"] = [p.number for p in base["prs"]]
    return L.Feed(**base)


def mk_pass(f, now, comments=(), sent=()):
    return R.Pass(f, L.classify(f, now, ROSTER), list(comments), set(sent))


def cmt(cid, thread, when, body, login="IssisX"):
    return R.Comment(str(cid), thread, when, login, body)


class FakeLookups(R.Lookups):
    def __init__(self, landed=None, branches=None, prs=None, issues=None):
        self.l, self.b, self.p, self.i = landed or {}, branches or {}, prs or {}, issues or {}
        self.calls = 0

    def landed(self, branch, sha):
        self.calls += 1
        return self.l.get((branch, sha), "")

    def branch_sha(self, name):
        return self.b.get(name)

    def pr(self, n):
        return self.p.get(n, {})

    def issue(self, n):
        return self.i.get(n, {})


class FakeGH:
    """GitHub's Git Data API for one branch, plus comments, with failures injected per call."""
    repo = REPO
    dry_run = False

    def __init__(self):
        self.blobs: dict[str, str] = {}
        self.trees: dict[str, dict[str, str]] = {}
        self.commits: dict[str, dict] = {}
        self.refs: dict[str, str] = {}
        self.comments: list[tuple[int, str, int]] = []
        self.comment_errors: list[Exception] = []
        self.before_ref_update = None   # called once, just before a ref update (a competing writer)

    @staticmethod
    def _sha(*parts) -> str:
        return hashlib.sha1("|".join(map(str, parts)).encode()).hexdigest()

    def get(self, path, accept=""):
        rest = path.split(f"repos/{REPO}/", 1)[1]
        if rest.startswith("git/ref/heads/"):
            name = rest[len("git/ref/heads/"):]
            if name not in self.refs:
                raise GitHubError("GET", path, "HTTP 404: Not Found")
            return {"object": {"sha": self.refs[name]}}
        if rest.startswith("git/trees/"):
            commit = rest[len("git/trees/"):].split("?")[0]
            tree = self.commits[commit]["tree"]
            return {"sha": tree, "truncated": False,
                    "tree": [{"path": p, "type": "blob", "sha": b} for p, b in self.trees[tree].items()]}
        if rest.startswith("git/blobs/"):
            return {"content": base64.b64encode(self.blobs[rest[len("git/blobs/"):]].encode()).decode()}
        raise AssertionError(f"unexpected GET {path}")

    def write(self, method, path, body=None, what=""):
        rest = path.split(f"repos/{REPO}/", 1)[1]
        if method == "POST" and rest == "git/trees":
            files = dict(self.trees[body["base_tree"]]) if body.get("base_tree") else {}
            for e in body["tree"]:
                b = self._sha("blob", e["content"])
                self.blobs[b] = e["content"]
                files[e["path"]] = b
            sha = self._sha("tree", sorted(files.items()))
            self.trees[sha] = files
            return {"sha": sha}
        if method == "POST" and rest == "git/commits":
            sha = self._sha("commit", body["tree"], body["parents"], len(self.commits))
            self.commits[sha] = {"tree": body["tree"], "parents": body["parents"]}
            return {"sha": sha}
        if rest.startswith("git/refs"):
            hook, self.before_ref_update = self.before_ref_update, None
            if hook:
                hook()
            if method == "PATCH":
                name = rest[len("git/refs/heads/"):]
                if not body.get("force") and self.commits[body["sha"]]["parents"] != [self.refs.get(name)]:
                    raise GitHubError(method, path, "HTTP 422: Update is not a fast forward")
                self.refs[name] = body["sha"]
                return {}
            name = body["ref"][len("refs/heads/"):]
            if name in self.refs:
                raise GitHubError(method, path, "HTTP 422: Reference already exists")
            self.refs[name] = body["sha"]
            return {}
        raise AssertionError(f"unexpected {method} {path}")

    def comment(self, number, body, what=""):
        if self.comment_errors:
            raise self.comment_errors.pop(0)
        self.comments.append((number, body, len(self.comments) + 1))
        return {"id": 9000 + len(self.comments)}

    def bot_comments(self, when):
        return [R.Comment(str(9000 + i), n, when, rules.BOT_LOGIN, body) for n, body, i in self.comments]


class Harness:
    """Passes over one store, the fake clock moved by the test."""

    def __init__(self, store=None, lookups=None, outbox=None):
        self.store = store or R.LocalStore(pathlib.Path(tempfile.mkdtemp(dir=TMP.name)))
        self.lookups = lookups or FakeLookups()
        self.outbox = outbox
        self.log: list[str] = []

    def run(self, f, now, comments=(), sent=(), **kw):
        p = mk_pass(f, now, comments, sent)
        return R.run_pass(self.store, p, ROSTER, self.lookups, now, self.outbox, self.log.append, **kw)

    def state(self, now=NOW) -> R.State:
        snap = self.store.read(now.strftime("%Y-%m"))
        return R.State.load(snap.files, now.strftime("%Y-%m"))

    def events(self, now=NOW) -> list[dict]:
        return R.read_events(self.state(now).events_text)

    def obs(self, job, now=NOW, kind=None):
        return [o for o in self.state(now).jobs[job]["obligations"] if kind is None or o["kind"] == kind]


def fb_red(sha="a", n=401):
    return pr(n, "fb-bot/integrate-j500-thing", sha, action="FIX-CI", proof="red: native")


# ---------------- the twelve fault cases (#350, section 6) ----------------
class FaultCases(unittest.TestCase):
    def test_01_dropped_notification_sends_once_with_attempt_2(self):
        gh = FakeGH()
        h = Harness(outbox=R.CommentOutbox(gh, ROSTER, 340))
        gh.comment_errors.append(GitHubError("POST", "comments", "HTTP 502: Bad Gateway"))
        f = feed(prs=[pr(402, "claude/integrate-j500-x", action="FIX-CI", proof="red: native")])
        h.run(f, NOW)
        ob = h.obs("J-500")[0]
        self.assertEqual((ob["attempts"], ob["sends"]), (1, 0))
        self.assertIn("HTTP 502", ob["last_error"])
        self.assertEqual(gh.comments, [])
        h.run(f, NOW + 10 * M, comments=gh.bot_comments(NOW + 10 * M))
        h.run(f, NOW + 20 * M, comments=gh.bot_comments(NOW + 10 * M))
        self.assertEqual(len(gh.comments), 1, "exactly one delivery")
        self.assertIn(f"id={ob['id']} send=1 attempt=2", gh.comments[0][1])
        ob = h.obs("J-500", NOW + 20 * M)[0]
        self.assertEqual((ob["attempts"], ob["sends"]), (2, 1))

    def test_02_duplicate_command_is_one_transition(self):
        h = Harness()
        f = feed(prs=[pr(403, "claude/integrate-j500-x", action="FIX-CI", proof="red: native")])
        h.run(f, NOW)
        oid = h.obs("J-500")[0]["id"]
        ack = cmt(77, 403, NOW + 5 * M, f"Claude: on it\n/ack {oid}")
        mail = cmt(78, 403, NOW + 6 * M, f"**Muse (via email bridge)**\n/ack {oid}")
        block = cmt(79, 403, NOW + 7 * M, "/blocked waiting on the NDK mirror, needs Cory")
        block2 = cmt(80, 403, NOW + 8 * M, "/blocked waiting on the NDK mirror, needs Cory")
        cs = [ack, ack, mail, block, block, block2]
        h.run(f, NOW + 10 * M, comments=cs)
        h.run(f, NOW + 20 * M, comments=cs)   # the same comments read again next pass
        evs = h.events(NOW + 20 * M)
        self.assertEqual(sum(1 for e in evs if e["ev"] == "acked"), 1)
        self.assertEqual(sum(1 for e in evs if e["ev"] == "blocked"), 1)
        rec = h.state().jobs["J-500"]
        self.assertEqual(rec["state"], "blocked")
        self.assertEqual(rec["blocked"]["holder"], "cory")
        self.assertEqual(h.obs("J-500")[0]["ack"]["how"], "/ack")

    def test_03_unavailable_agent_resent_at_due_dead_lettered_at_twice(self):
        h = Harness()
        f = feed(prs=[fb_red()], issues=[issue(500, owner="owner:muse")])
        h.run(f, NOW)
        ob = h.obs("J-500")[0]
        self.assertEqual((ob["to"], ob["via"], ob["due"]), ("muse", "email", L.iso(NOW + 4 * H)))
        h.run(f, NOW + 3 * H + 59 * M)
        self.assertEqual(h.obs("J-500")[0]["sends"], 1, "nothing before the due time")
        h.run(f, NOW + 4 * H)
        self.assertEqual(h.obs("J-500")[0]["sends"], 2, "re-sent once at the due time")
        h.run(f, NOW + 6 * H)
        self.assertEqual(h.obs("J-500")[0]["sends"], 2, "only once")
        self.assertIsNone(h.obs("J-500")[0]["dead_lettered"])
        h.run(f, NOW + 8 * H)
        ob = h.obs("J-500")[0]
        self.assertEqual(ob["dead_lettered"], L.iso(NOW + 8 * H))
        self.assertIsNone(ob["ack"])
        md = h.store.root.joinpath(R.SHADOW_DIR, f"{NOW + 8 * H:%Y-%m-%d}.md").read_text()
        self.assertIn(f"`{ob['id']}` J-500 fix to muse", md.split("## Needs you")[1].split("##")[0])
        # its PR and issue close with nothing done: the job still never shows as done while the dead-letter stands
        h.lookups.i[500] = {"state": "closed", "state_reason": "completed"}
        h.run(feed(), NOW + 9 * H)
        rec = h.state().jobs["J-500"]
        self.assertIsNone(rec["outcome"])
        self.assertEqual(rec["state"], "blocked")
        self.assertIsNone(h.obs("J-500")[0]["closed"])

    def test_04_crash_after_delivery_records_marker_without_resend(self):
        gh = FakeGH()
        h = Harness(outbox=R.CommentOutbox(gh, ROSTER, 340))
        f = feed(prs=[pr(404, "claude/integrate-j500-x", action="FIX-CI", proof="red: native")])
        real = h.store.commit

        def crash(*a, **kw):
            raise OSError("worker killed")
        h.store.commit = crash
        with self.assertRaises(OSError):
            h.run(f, NOW)
        self.assertEqual(len(gh.comments), 1)
        h.store.commit = real
        h.run(f, NOW + 10 * M, comments=gh.bot_comments(NOW))
        self.assertEqual(len(gh.comments), 1, "no second delivery")
        ob = h.obs("J-500", NOW + 10 * M)[0]
        self.assertEqual((ob["sends"], ob["attempts"], ob["last_sent"]), (1, 1, L.iso(NOW)))
        self.assertTrue(any(e["ev"] == "delivery_found" for e in h.events()))

    def test_05_stale_review_new_head_gets_a_new_review_obligation(self):
        h = Harness()
        h.run(feed(prs=[pr(405, "claude/integrate-j500-x", "a", needs_review=True)]), NOW)
        old = h.obs("J-500", kind="review")[0]
        self.assertEqual((old["to"], old["version"]), ("codex", SHA["a"]))
        h.run(feed(prs=[pr(405, "claude/integrate-j500-x", "a", action="MERGE")]), NOW + 10 * M)
        self.assertEqual(h.state().jobs["J-500"]["state"], "landable")
        self.assertEqual(h.obs("J-500", kind="review")[0]["ack"]["how"], "verdict on the head")
        # a new head: the LAND on the old one does not carry (the switch's merge rule says WAIT), a review is owed again
        h.run(feed(prs=[pr(405, "claude/integrate-j500-x", "b", needs_review=True,
                           reasons=(f"no LAND on @{SHA['b'][:8]} from a vendor other than anthropic",))]), NOW + 20 * M)
        rec = h.state().jobs["J-500"]
        self.assertEqual(rec["state"], "review")
        self.assertEqual(rec["head"], SHA["b"])
        revs = h.obs("J-500", kind="review")
        self.assertEqual([o["version"] for o in revs], [SHA["a"], SHA["b"]])
        self.assertTrue(R.is_open(revs[1]))

    def test_06_delayed_ci_owes_the_ci_owner_and_a_late_green_completes(self):
        h = Harness()
        h.run(feed(prs=[pr(406, "fb-bot/integrate-j500-x", proof="running", pushed=-0.5)]), NOW)
        self.assertEqual(h.obs("J-500", kind="ci"), [], "not late yet")
        h.run(feed(prs=[pr(406, "fb-bot/integrate-j500-x", proof="running", pushed=-0.5)]), NOW + H)
        ci = h.obs("J-500", kind="ci")
        self.assertEqual([(o["to"], o["via"]) for o in ci], [("claude", "api")])
        h.run(feed(prs=[pr(406, "fb-bot/integrate-j500-x", needs_review=True, pushed=-0.5)]), NOW + 2 * H)
        rec = h.state().jobs["J-500"]
        self.assertEqual(rec["state"], "review")
        self.assertEqual(h.obs("J-500", kind="ci")[0]["ack"]["how"], "proof finished")
        self.assertEqual(len(h.obs("J-500", kind="review")), 1)

    def test_07_reassign_cancels_the_old_obligation_never_two_owners(self):
        h = Harness()
        h.run(feed(prs=[fb_red()], issues=[issue(500, owner="owner:muse")]), NOW)
        h.run(feed(prs=[fb_red()], issues=[issue(500, owner="owner:claude")]), NOW + H)
        rec = h.state().jobs["J-500"]
        self.assertEqual(rec["owner"], "claude")
        fixes = h.obs("J-500", kind="fix")
        self.assertEqual([(o["to"], R.is_open(o)) for o in fixes], [("muse", False), ("claude", True)])
        self.assertEqual(fixes[0]["closed"]["why"], "reassigned to claude")
        self.assertEqual(len({o["to"] for o in fixes if R.is_open(o)}), 1)
        self.assertTrue(any(e["ev"] == "reassigned" for e in h.events()))

    def test_08_competing_writer_cas_rejected_rereads_no_lost_update(self):
        gh = FakeGH()
        store = R.GitStore(gh)
        h = Harness(store=store)
        h.run(feed(issues=[issue(900, job="J-900")]), NOW)   # the branch exists
        other = Harness(store=store)

        def race():   # another pass lands first, with a record this pass never saw
            other.run(feed(issues=[issue(900, job="J-900"), issue(901, job="J-901", owner="owner:muse")]),
                      NOW + 9 * M)
        gh.before_ref_update = race
        res = h.run(feed(issues=[issue(900, job="J-900"), issue(902, job="J-902")]), NOW + 10 * M)
        self.assertEqual((res.status, res.tries), ("committed", 2))
        self.assertTrue(any("re-reading" in line for line in h.log))
        st = h.state()
        self.assertTrue({"J-900", "J-901", "J-902"} <= set(st.jobs))
        files = gh.trees[gh.commits[gh.refs[R.STATE_BRANCH]]["tree"]]
        self.assertIn("state/jobs/J-901.json", files)
        self.assertIn("state/jobs/J-902.json", files)
        passes = [e for e in h.events() if e["ev"] == "pass"]
        self.assertEqual(len(passes), 3, "no lost event either")

    def test_09_rate_limit_backs_off_not_a_refusal(self):
        gh = FakeGH()
        h = Harness(outbox=R.CommentOutbox(gh, ROSTER, 340))
        gh.comment_errors.append(GitHubError("POST", "comments", "HTTP 429: Too Many Requests (Retry-After: 30)"))
        f = feed(prs=[pr(407, "claude/integrate-j500-x", action="FIX-CI", proof="red: x"),
                      pr(408, "claude/integrate-j501-y", job="J-501", action="FIX-CI", proof="red: y")])
        res = h.run(f, NOW)
        self.assertEqual(res.status, "backoff")
        obs = h.obs("J-500") + h.obs("J-501")
        self.assertEqual([(o["attempts"], o["sends"]) for o in obs], [(0, 0), (0, 0)], "no attempt counted, none sent")
        self.assertFalse(any(e["ev"] == "send_failed" for e in h.events()))
        self.assertIn("retry after 30s", obs[0]["last_error"] + obs[1]["last_error"])
        h.run(f, NOW + 10 * M)
        self.assertEqual(len(gh.comments), 2)
        # a 403 that is not a rate limit is a refusal: counted, retried, never a backoff
        self.assertIsNone(R.rate_limit_of(GitHubError("POST", "x", "HTTP 403: Resource not accessible")))
        self.assertIsNotNone(R.rate_limit_of(GitHubError("POST", "x", "HTTP 403: API rate limit exceeded")))

    def test_10_dry_dispatch_writes_nothing_and_never_pings(self):
        h = Harness()
        pings = []
        res = h.run(feed(issues=[issue(500)]), NOW, dry=True, ping=lambda: pings.append(1))
        self.assertEqual((res.status, pings), ("dry", []))
        self.assertIsNone(h.store.read("2026-11").head)
        res = h.run(feed(issues=[issue(500)]), NOW, ping=lambda: pings.append(1))
        self.assertEqual((res.status, pings), ("committed", [1]))
        self.assertEqual([e["ev"] for e in h.events() if e["ev"] == "pass"], ["pass"])

    def test_11_unparseable_input_is_unmatched_with_an_age_never_dropped(self):
        h = Harness()
        f = feed(prs=[pr(409, "weird/thing", job="")],
                 branches=[L.BranchIn("odd-branch", SHA["c"], t(-1))])
        bad = cmt(81, 409, NOW, "/ack O-deadbeef")
        h.run(f, NOW, comments=[bad])
        un = h.state().unmatched
        self.assertIn("PR|#409", un)
        self.assertIn("BRANCH|odd-branch", un)
        self.assertIn("CMD|81|O-deadbeef", un)
        res = h.run(f, NOW + 25 * H, comments=[bad])
        un = h.state(NOW + 25 * H).unmatched
        self.assertEqual(un["PR|#409"]["first_seen"], L.iso(NOW))
        self.assertGreaterEqual(res.slo["unmatched_over_24h"], 3)

    def test_12_conflict_with_no_event_goes_to_the_author_with_a_due_time(self):
        h = Harness()
        f = feed(prs=[pr(410, "fb-bot/integrate-j500-x", reasons=(R.CONFLICT,)),
                      pr(411, "claude/integrate-j501-y", job="J-501", reasons=(R.CONFLICT,))])
        h.run(f, NOW)
        a, b = h.obs("J-500")[0], h.obs("J-501")[0]
        self.assertEqual((a["kind"], a["to"], a["due"]), ("fix", "muse", L.iso(NOW + 4 * H)))
        self.assertEqual((b["kind"], b["to"], b["due"]), ("fix", "claude", L.iso(NOW + 30 * M)))
        self.assertEqual(h.state().jobs["J-500"]["state"], "working")
        # the author merges trunk in: a new head is the proven ack
        h.run(feed(prs=[pr(410, "fb-bot/integrate-j500-x", "b", needs_review=True)]), NOW + H)
        self.assertEqual(h.obs("J-500", kind="fix")[0]["ack"]["how"], "new head")


# ---------------- landed work, terminal outcomes ----------------
class Landed(unittest.TestCase):
    def test_branch_in_trunk_is_merged_not_an_obligation(self):
        b = L.BranchIn("claude/integrate-j510-done", SHA["d"], t(-3))
        h = Harness(lookups=FakeLookups(landed={(b.name, b.sha): "in trunk"}))
        green = L.RunIn(1, "u", b.name, b.sha, t(-3), t(-2.5), "success", "green")
        h.run(feed(branches=[b], runs=[green]), NOW)
        rec = h.state().jobs["J-510"]
        self.assertEqual((rec["outcome"], rec["state"]), ("merged", "merged"))
        self.assertEqual(rec["obligations"], [])
        self.assertFalse(any(e["ev"] == "obligation" for e in h.events()))
        h.run(feed(branches=[b], runs=[green]), NOW + 10 * M)
        self.assertEqual(h.lookups.calls, 1, "read once per head")

    def test_merged_pr_at_the_head_with_the_issue_open_owes_nothing(self):
        b = L.BranchIn("fb-bot/integrate-j511-x", SHA["e"], t(-3))
        h = Harness(lookups=FakeLookups(landed={(b.name, b.sha): "merged #300"}))
        green = L.RunIn(2, "u", b.name, b.sha, t(-3), t(-2.5), "success", "green")
        h.run(feed(branches=[b], runs=[green], issues=[issue(511, job="J-511", owner="owner:muse")]), NOW)
        rec = h.state().jobs["J-511"]
        self.assertEqual((rec["state"], rec["outcome"], rec["obligations"]), ("merged", None, []))
        h.lookups.i[511] = {"state": "closed", "state_reason": "completed"}
        h.run(feed(branches=[b], runs=[green]), NOW + H)
        self.assertEqual(h.state().jobs["J-511"]["outcome"], "merged")

    def test_every_finished_job_has_exactly_one_outcome(self):
        h = Harness(lookups=FakeLookups(prs={420: {"merged": True, "state": "closed"},
                                             421: {"merged": False, "state": "closed"}},
                                        issues={522: {"state": "closed", "state_reason": "not_planned"}}))
        h.run(feed(prs=[pr(420, "claude/integrate-j520-a", job="J-520"),
                        pr(421, "claude/integrate-j521-b", job="J-521")],
                   issues=[issue(522, job="J-522")]), NOW)
        h.run(feed(), NOW + H)
        h.run(feed(), NOW + 2 * H)
        jobs = h.state().jobs
        self.assertEqual({j: jobs[j]["outcome"] for j in ("J-520", "J-521", "J-522")},
                         {"J-520": "merged", "J-521": "failed", "J-522": "cancelled"})
        outs = [e for e in h.events() if e["ev"] == "outcome"]
        self.assertEqual(sorted(e["job"] for e in outs), ["J-520", "J-521", "J-522"], "one outcome event each")


# ---------------- due times, acks by reply, the shadow diff ----------------
class Rules(unittest.TestCase):
    def test_due_by_wakeability(self):
        self.assertEqual(R.due_at(ROSTER, "codex", NOW), ("mention", NOW + 30 * M))
        self.assertEqual(R.due_at(ROSTER, "claude", NOW), ("api", NOW + 30 * M))
        self.assertEqual(R.due_at(ROSTER, "muse", NOW), ("email", NOW + 4 * H))
        # NOW is 12:00 PM Central: Grok's next run is 4 PM Central (22:00 UTC), plus 1 h
        self.assertEqual(R.due_at(ROSTER, "grok", NOW), ("issue", dt.datetime(2026, 11, 9, 23, 0, tzinfo=dt.timezone.utc)))
        self.assertEqual(R.due_at(ROSTER, "chatgpt", NOW), ("cory", NOW + 15 * M), "manual goes to Cory")

    def test_manual_agent_goes_straight_to_cory_never_resent(self):
        h = Harness()
        f = feed(prs=[pr(412, "chatgpt/integrate-j530-x", job="J-530", action="FIX-CI", proof="red: x")])
        h.run(f, NOW)
        h.run(f, NOW + 3 * H)
        ob = h.obs("J-530")[0]
        self.assertEqual((ob["via"], ob["sends"], ob["dead_lettered"]), ("cory", 1, None))

    def test_reply_on_the_thread_by_the_addressee_acks(self):
        h = Harness()
        f = feed(prs=[pr(413, "claude/integrate-j500-x", action="FIX-CI", proof="red: x")])
        h.run(f, NOW)
        other = cmt(90, 413, NOW + 5 * M, "Grok: drive-by note")
        h.run(f, NOW + 10 * M, comments=[other])
        self.assertIsNone(h.obs("J-500")[0]["ack"])
        mine = cmt(91, 413, NOW + 12 * M, "Claude (integrator): fixing it now\n\n_Generated by [Claude Code]_")
        h.run(f, NOW + 20 * M, comments=[other, mine])
        self.assertEqual(h.obs("J-500")[0]["ack"]["how"], "reply on the thread")

    def test_untrusted_login_cannot_command(self):
        h = Harness()
        f = feed(prs=[pr(414, "claude/integrate-j500-x", action="FIX-CI", proof="red: x")])
        h.run(f, NOW)
        oid = h.obs("J-500")[0]["id"]
        h.run(f, NOW + 5 * M, comments=[cmt(92, 414, NOW, f"/ack {oid}", login="someone")])
        self.assertIsNone(h.obs("J-500")[0]["ack"])

    def test_start_is_owed_once_an_assigned_job_is_quiet_a_day(self):
        h = Harness()
        fresh = issue(540, job="J-540", owner="owner:muse")   # touched 2 h ago
        h.run(feed(issues=[fresh]), NOW)
        self.assertEqual((h.state().jobs["J-540"]["state"], h.obs("J-540")), ("assigned", []))
        h.run(feed(issues=[fresh]), NOW + 23 * H)
        starts = h.obs("J-540", NOW + 23 * H)
        self.assertEqual([(o["kind"], o["to"]) for o in starts], [("start-agent", "muse")])
        b = L.BranchIn("fb-bot/integrate-j540-go", SHA["f"], t(23.5))
        h.run(feed(issues=[fresh], branches=[b]), NOW + 24 * H)
        self.assertEqual(h.obs("J-540", NOW + 24 * H)[0]["ack"]["how"], "work pushed")

    def test_config_key_absent_is_off_and_only_shadow_is_on(self):
        root = pathlib.Path(tempfile.mkdtemp(dir=TMP.name))
        self.assertEqual(R.mode(root), "off")
        (root / "work").mkdir()
        for text, want in (("reconciler: shadow\n", "shadow"), ("reconciler: live\n", "off"), ("other: 1\n", "off")):
            (root / R.CONFIG).write_text(text)
            self.assertEqual(R.mode(root), want, text)

    def test_shadow_diff_compares_with_what_the_ledger_sent(self):
        h = Harness()
        p = pr(415, "fb-bot/integrate-j500-x", needs_review=True, now=-3)
        f = feed(prs=[p])
        led = L.classify(f, NOW, ROSTER)
        item = next(it for it in led.items if it.ref == "#415")
        h.run(f, NOW, sent={item.key(1)})
        md = h.store.root.joinpath(R.SHADOW_DIR, f"{NOW:%Y-%m-%d}.md").read_text()
        self.assertIn("Agreement 100.0%: 1 matched of 1", md)
        self.assertIn("| Largest gap between non-dry passes, last 24 h |", md)
        h.run(feed(prs=[p, pr(416, "claude/integrate-j501-x", job="J-501", action="FIX-CI", proof="red: y")]),
              NOW + 10 * M, sent={item.key(1)})
        md = h.store.root.joinpath(R.SHADOW_DIR, f"{NOW:%Y-%m-%d}.md").read_text()
        self.assertIn("Agreement 50.0%", md)
        self.assertIn("PR|#416 to claude", md)
        self.assertIn("| 10.0 min (2 passes) |", md)
        self.assertEqual(sum(1 for e in h.events() if e["ev"] == "ledger_sent"), 1, "a key is recorded once")


if __name__ == "__main__":
    unittest.main()
