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
import re
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
# test_ledger's roster as work/agents.yml has it for the reconciler: Muse cannot open PRs (checked below)
RROSTER = L.Roster({"agents": {k: dict(v, **({"opens_prs": False} if k == "muse" else {}))
                               for k, v in AGENTS["agents"].items()}})
# A roster whose ChatGPT runs only when Cory starts it: the fixture for the manual-agent path (Cory's channel).
# The repo's ChatGPT is polled hourly since 11-09; these tests keep the manual path covered.
MROSTER = L.Roster({"agents": {k: dict(v, **({"opens_prs": False} if k == "muse" else {}),
                                       **({"wake": "manual"} if k == "chatgpt" else {}))
                               for k, v in AGENTS["agents"].items()}})
CENTRAL = R.CENTRAL
UTC = dt.timezone.utc


def ct(y, mo, d, h, mi):
    """A Central wall-clock time, in UTC."""
    return dt.datetime(y, mo, d, h, mi, tzinfo=CENTRAL).astimezone(UTC)


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
        self.label_errors: list[Exception] = []
        self.writes: list[tuple[str, str, dict]] = []   # issue writes: labels, comment edits
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
        if method == "POST" and rest.startswith("issues/") and rest.endswith("/labels"):
            if self.label_errors:
                raise self.label_errors.pop(0)
            self.writes.append((method, rest, body))
            return [{"name": n} for n in body["labels"]]
        if method == "PATCH" and rest.startswith("issues/comments/"):
            i = int(rest.rsplit("/", 1)[1]) - 9001
            n, _, k = self.comments[i]
            self.comments[i] = (n, body["body"], k)
            self.writes.append((method, rest, body))
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

    def __init__(self, store=None, lookups=None, outbox=None, cory=None, roster=None):
        self.store = store or R.LocalStore(pathlib.Path(tempfile.mkdtemp(dir=TMP.name)))
        self.lookups = lookups or FakeLookups()
        self.outbox, self.cory = outbox, cory
        self.roster = roster or RROSTER
        self.log: list[str] = []

    def run(self, f, now, comments=(), sent=(), tokens=None, open_threads=None, **kw):
        p = mk_pass(f, now, comments, sent)
        p.tokens = tokens or {}
        p.open_threads = open_threads
        return R.run_pass(self.store, p, self.roster, self.lookups, now, self.outbox, self.log.append,
                          cory=self.cory, **kw)

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
        # #350 hole 2: recorded is not enough. The message Cory would get is written: one line with what, who
        # failed and since when, in one "To Cory:" batch (stage 2 posts this same text on #340).
        cory = h.state(NOW + 8 * H).cory
        mine = [b for b in cory["batches"] if ob["id"] in b["keys"]]   # (the passes here are hours apart: PIPE
        self.assertEqual(len(mine), 1)                                   # lines for the gaps share the batches)
        text = mine[0]["text"]
        self.assertTrue(text.startswith("To Cory: Needs you ("), text)
        self.assertFalse(mine[0]["live"])
        line = next(x for x in text.splitlines() if ob["id"] in x)
        for part in ("J-500 fix", "#401 proof red: native", "Muse did not act (2 send(s) by email, no ack)",
                     f"since {R.central(NOW + 8 * H)} Central", f"/ack {ob['id']}"):
            self.assertIn(part, line)
        self.assertEqual(cory["reported"][ob["id"]], L.iso(NOW + 8 * H))
        self.assertIn(line, md.split("to Cory today")[1])
        self.assertEqual(sum(1 for e in h.events(NOW + 8 * H) if e["ev"] == "would_notify_cory"
                             and ob["id"] in e["keys"]), 1)
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
        ci = h.obs("J-500", kind="ci")   # a late proof is the integrator's: it waits on its hourly runs
        self.assertEqual([(o["to"], o["via"]) for o in ci], [("claude", "session")])
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
        # NOW is Monday 12:00 PM Central: the integrator's next run is 12:17 PM, due one hour on, at 1:17 PM
        self.assertEqual((b["kind"], b["to"], b["due"]), ("fix", "claude", L.iso(ct(2026, 11, 9, 13, 17))))
        self.assertEqual(h.state().jobs["J-500"]["state"], "working")
        # the author merges trunk in: a new head is the proven ack
        h.run(feed(prs=[pr(410, "fb-bot/integrate-j500-x", "b", needs_review=True)]), NOW + H)
        self.assertEqual(h.obs("J-500", kind="fix")[0]["ack"]["how"], "new head")


class Strengthened(unittest.TestCase):
    """Cases 1, 3 and 8 as the red team (#350 comment 6746976080) found them weak."""

    def test_01_a_receipt_needs_the_channel_muse_label_and_codex_relay(self):
        # Muse: the bridge mails only a labelled issue, so a comment whose label failed is not delivered
        gh = FakeGH()
        h = Harness(outbox=R.CommentOutbox(gh, RROSTER, 340))
        f = feed(prs=[fb_red(n=401)], issues=[issue(500, owner="owner:muse")])
        gh.label_errors.append(GitHubError("POST", "labels", "HTTP 502: Bad Gateway"))
        h.run(f, NOW)
        ob = h.obs("J-500")[0]
        self.assertEqual((ob["attempts"], ob["sends"]), (1, 0), "no label, no receipt: not delivered")
        self.assertNotIn("reconciler:sent", gh.comments[0][1])
        h.run(f, NOW + 10 * M, comments=gh.bot_comments(NOW + 10 * M))
        self.assertEqual([n for n, _, _ in gh.comments], [500, 500], "on the job's issue, never the PR")
        self.assertEqual(gh.writes[0], ("POST", "issues/500/labels", {"labels": ["wake:muse"]}))
        self.assertEqual(gh.writes[1][:2], ("PATCH", "issues/comments/9002"), "the receipt goes in after the label")
        body = gh.comments[1][1]
        self.assertTrue(body.startswith("CLAUDE_TO_MUSE: J-500 "), body)
        self.assertIn(f"id={ob['id']} send=1 attempt=2", body)
        mailed = body.split("\n\n<!--")[0]   # what the bridge mails (it strips the marker too)
        for bad in ("`", "http", "<", "@"):
            self.assertNotIn(bad, mailed)
        ob = h.obs("J-500", NOW + 10 * M)[0]
        self.assertEqual((ob["attempts"], ob["sends"]), (2, 1))
        # Codex: the bot's own mention does not wake it, so the ask goes to the integrator to post
        gh2 = FakeGH()
        h2 = Harness(outbox=R.CommentOutbox(gh2, RROSTER, 340))
        h2.run(feed(prs=[pr(402, "fb-bot/integrate-j501-y", job="J-501", needs_review=True)]), NOW)
        rev = h2.obs("J-501", kind="review")[0]
        self.assertEqual((rev["to"], rev["via"]), ("codex", "relay"))
        n, body, _ = gh2.comments[0]
        self.assertEqual(n, 402)
        self.assertTrue(body.startswith("To Claude: relay to Codex on #402"), body)
        self.assertIn("Post `@codex review` on #402 yourself", body)
        self.assertNotIn("To Codex", body)
        self.assertEqual(re.findall(r"@[\w-]+", body), ["@codex"], "the one mention is in a code span: no ping")

    def test_03_a_dead_letter_reaches_cory_once_in_a_slot_batch(self):
        gh = FakeGH()
        h = Harness(roster=MROSTER, cory=R.CoryNotifier(gh, 340))
        a, b, c = (pr(450 + i, f"chatgpt/integrate-j57{i}-x", job=f"J-57{i}", action="FIX-CI", proof="red: x")
                   for i in range(3))
        h.run(feed(prs=[a]), NOW)
        self.assertEqual(len(gh.comments), 1)
        n, body, _ = gh.comments[0]
        self.assertEqual(n, 340)
        self.assertTrue(body.startswith("To Cory: Needs you (1), from the switchboard at"), body)
        self.assertIn("ChatGPT runs only when you start it", body)
        self.assertRegex(body, r"<!-- reconciler:cory batch=\S+ keys=O-[0-9a-f]{8} -->")
        for k in range(1, 6):   # a pass every 10 min; the new items wait for the next slot (Monday 2:17 PM)
            h.run(feed(prs=[a, b] if k == 1 else [a, b, c]), NOW + k * 10 * M, comments=gh.bot_comments(NOW))
        self.assertEqual(len(gh.comments), 1, "never one message per item")
        h.run(feed(prs=[a, b, c]), NOW + H, comments=gh.bot_comments(NOW))
        for k in range(1, 8):   # passes every 10 min up to 2:07 PM: still nothing
            h.run(feed(prs=[a, b, c]), NOW + H + k * 10 * M, comments=gh.bot_comments(NOW))
        self.assertEqual(len(gh.comments), 1, "an hour on is not a slot: at most every 3 hours (11-09)")
        h.run(feed(prs=[a, b, c]), ct(2026, 11, 9, 14, 17), comments=gh.bot_comments(NOW))
        self.assertEqual(len(gh.comments), 2)
        second = gh.comments[1][1]
        self.assertTrue(second.startswith("To Cory: Needs you (2)"), second)
        self.assertIn("J-571", second)
        self.assertIn("J-572", second)
        self.assertNotIn("J-570", second.split("<!--")[0].split("Still standing")[0])
        self.assertIn("Still standing from earlier batches: 1", second)
        for k in range(1, 8):
            h.run(feed(prs=[a, b, c]), ct(2026, 11, 9, 14, 17) + k * 10 * M, comments=gh.bot_comments(NOW))
        self.assertEqual(len(gh.comments), 2, "each item once")
        self.assertTrue(all(e.get("ev") != "would_notify_cory" for e in h.events()), "live: sent, not would-send")

    def test_08_a_retried_commit_posts_nothing_twice(self):
        gh = FakeGH()
        store = R.GitStore(gh)
        h = Harness(roster=MROSTER, store=store, outbox=R.CommentOutbox(gh, RROSTER, 340), cory=R.CoryNotifier(gh, 340))
        h.run(feed(issues=[issue(900, job="J-900")]), NOW)
        other = Harness(roster=MROSTER, store=store)
        gh.before_ref_update = lambda: other.run(feed(issues=[issue(900, job="J-900")]), NOW + 9 * M)
        f = feed(issues=[issue(900, job="J-900")],
                 prs=[pr(460, "claude/integrate-j902-x", job="J-902", action="FIX-CI", proof="red: x"),
                      pr(461, "chatgpt/integrate-j903-y", job="J-903", action="FIX-CI", proof="red: y")])
        res = h.run(f, NOW + 10 * M)
        self.assertEqual((res.status, res.tries), ("committed", 2))
        self.assertEqual(sum(1 for _, b, _ in gh.comments if b.startswith("To Claude:")), 1)
        self.assertEqual(sum(1 for _, b, _ in gh.comments if b.startswith("To Cory:")), 1)
        st = h.state()
        fix = h.obs("J-902", kind="fix")[0]
        self.assertEqual(fix["sends"], 1)
        cory_ob = h.obs("J-903")[0]["id"]
        self.assertIn(cory_ob, st.cory["reported"])
        evs = h.events()
        self.assertEqual(sum(1 for e in evs if e["ev"] == "sent" and e.get("id") == fix["id"]), 1)
        self.assertEqual(sum(1 for e in evs if e["ev"] == "needs_you"), 1)
        # a crash after the post and before any commit: the marker on #340 is the receipt
        h2 = Harness(roster=MROSTER, cory=R.CoryNotifier(gh, 340))
        h2.run(f, NOW + 3 * H, comments=gh.bot_comments(NOW + 10 * M))
        self.assertEqual(sum(1 for _, b, _ in gh.comments if b.startswith("To Cory:")), 1)


class RedTeam(unittest.TestCase):
    """The red team's fault cases (a)-(h) that fall within the reconciler (#350 comment 6746976080)."""

    def test_a_a_one_pass_input_gap_never_closes_an_obligation(self):
        h = Harness()
        merge = pr(420, "claude/integrate-j500-x", action="HUMAN-MERGE")
        computing = pr(420, "claude/integrate-j500-x", action="WAIT", reasons=(R.COMPUTING,))
        h.run(feed(prs=[merge]), NOW)
        oid = h.obs("J-500", kind="ruling")[0]["id"]

        def ruling():
            return next(o for o in h.obs("J-500") if o["id"] == oid)
        h.run(feed(prs=[computing]), NOW + 10 * M)   # mergeability flickers to "computing"
        self.assertTrue(R.is_open(ruling()), ruling())
        self.assertEqual(ruling()["missing"], 1)
        h.run(feed(prs=[merge]), NOW + 20 * M)
        self.assertTrue(R.is_open(ruling()))
        self.assertEqual(ruling()["missing"], 0)
        # the PR is open but this pass could not read it: never "PR closed", and the job takes no outcome
        for k in (3, 4):
            h.run(feed(prs=[], open_prs=[420]), NOW + k * 10 * M)
        self.assertTrue(R.is_open(ruling()))
        self.assertIsNone(h.state().jobs["J-500"]["outcome"])
        h.run(feed(prs=[], open_prs=[420]), NOW + 50 * M)   # missing 3 passes in a row: unseen, not closed
        ob = ruling()
        self.assertEqual((ob["ack"], ob["closed"], ob["unseen"]), (None, None, L.iso(NOW + 50 * M)))
        self.assertFalse(R.is_open(ob))
        h.run(feed(prs=[merge]), NOW + H)                    # it comes back: reopened, on a fresh clock
        self.assertTrue(R.is_open(ruling()))
        self.assertEqual((ruling()["due"], ruling()["dead_lettered"]), (L.iso(R.session_due(NOW + H)), None))
        self.assertTrue(any(e["ev"] == "reopened" and e["id"] == oid for e in h.events()))
        self.assertFalse(any(e["ev"] in ("acked", "closed") and e.get("id") == oid for e in h.events()))
        # a review whose PR drops out of the listing: one read; unknown leaves it, closed closes it
        h2 = Harness()
        h2.run(feed(prs=[pr(421, "fb-bot/integrate-j501-y", job="J-501", needs_review=True)]), NOW)
        h2.run(feed(), NOW + 10 * M)
        self.assertTrue(R.is_open(h2.obs("J-501", kind="review")[0]), "a read that told nothing is no evidence")
        h2.lookups.p[421] = {"merged": False, "state": "closed"}
        h2.run(feed(), NOW + 20 * M)
        self.assertEqual(h2.obs("J-501", kind="review")[0]["closed"]["why"], "PR closed without merging")

    def test_b_an_unrelated_reply_is_not_an_ack(self):
        h = Harness()
        ask = L.AskIn("6746462436", 340, "u", "Claude", "Muse", NOW - 5 * M)
        h.run(feed(asks=[ask]), NOW)
        ob = h.state().threads["obligations"][0]
        self.assertEqual((ob["kind"], ob["to"]), ("ask", "claude"))
        unrelated = cmt(6746481113, 340, NOW + M, "Claude (integrator): Grok's asks are answered.\n\n"
                        "_Generated by [Claude Code]_")
        h.run(feed(), NOW + 10 * M, comments=[unrelated])   # asks.py dropped the ask on that post
        ob = h.state().threads["obligations"][0]
        self.assertEqual((ob["ack"], ob["closed"], ob["missing"]), (None, None, 0), "still owed")
        named = cmt(6746490000, 340, NOW + 12 * M, "Claude (integrator): resent it reworded (re issuecomment-"
                    "6746462436).\n\n_Generated by [Claude Code]_")
        h.run(feed(), NOW + 20 * M, comments=[unrelated, named])
        self.assertEqual(h.state().threads["obligations"][0]["ack"]["how"], "reply naming it")
        # a ruling: a reply before it is not its answer; one after it is only when it names it (#360's rule, J-072
        # replay: a post on the thread that names nothing is not an answer; the merge itself is the proof)
        h2 = Harness()
        f = feed(prs=[pr(422, "fb-bot/integrate-j502-z", job="J-502", action="HUMAN-MERGE")])
        early = cmt(93, 422, NOW - 5 * M, "Claude: looking at it.\n\n_Generated by [Claude Code]_")
        h2.run(f, NOW, comments=[early])
        h2.run(f, NOW + 5 * M, comments=[early])
        self.assertIsNone(h2.obs("J-502", kind="ruling")[0]["ack"])
        late = cmt(94, 422, NOW + 6 * M, "Claude: merged it by hand.\n\n_Generated by [Claude Code]_")
        h2.run(f, NOW + 10 * M, comments=[early, late])
        self.assertIsNone(h2.obs("J-502", kind="ruling")[0]["ack"], "a post that names nothing is not an ack")
        oid = h2.obs("J-502", kind="ruling")[0]["id"]
        named = cmt(95, 422, NOW + 11 * M, f"Claude: {oid} merged it by hand.\n\n_Generated by [Claude Code]_")
        h2.run(f, NOW + 15 * M, comments=[early, late, named])
        self.assertEqual(h2.obs("J-502", kind="ruling")[0]["ack"]["how"], "reply naming it")

    def test_c_the_dead_letter_path_failing_is_retried_and_reported(self):
        f = feed(prs=[pr(412, "chatgpt/integrate-j530-x", job="J-530", action="FIX-CI", proof="red: x")])
        gh = FakeGH()
        h = Harness(roster=MROSTER, cory=R.CoryNotifier(gh, 340))
        gh.comment_errors.append(GitHubError("POST", "comments", "HTTP 401: Bad credentials"))
        res = h.run(f, NOW)
        self.assertEqual(gh.comments, [])
        st = h.state()
        self.assertIn("HTTP 401", st.cory["last_error"])
        self.assertEqual((res.slo["cory_unreported"], st.cory["reported"]), (1, {}))
        self.assertTrue(any("Needs-you batch for Cory did not go" in line for line in h.log))
        h.run(f, NOW + 10 * M)   # next pass, not an hour later: nothing went
        self.assertEqual(len(gh.comments), 1)
        self.assertEqual(h.state().cory["last_error"], "")
        for k in range(2, 14):   # two hours of passes, 10 min apart
            h.run(f, NOW + k * 10 * M, comments=gh.bot_comments(NOW + 10 * M))
        self.assertEqual(len(gh.comments), 1)
        # the second path failing after the board comment went is reported, never a reason to post again
        gh2 = FakeGH()

        def down(subject, text):
            raise OSError("mail relay down")
        h2 = Harness(roster=MROSTER, cory=R.CoryNotifier(gh2, 340, mailer=down))
        h2.run(f, NOW)
        self.assertEqual(len(gh2.comments), 1)
        self.assertIn("the mail failed", h2.state().cory["last_error"])
        for k in range(1, 13):
            h2.run(f, NOW + k * 10 * M, comments=gh2.bot_comments(NOW))
        self.assertEqual(len(gh2.comments), 1)
        mails = []
        h3 = Harness(roster=MROSTER, cory=R.CoryNotifier(FakeGH(), 340, mailer=lambda s, t: mails.append((s, t))))
        h3.run(f, NOW)
        self.assertEqual(len(mails), 1)
        self.assertTrue(mails[0][0].startswith("To Cory: Needs you (1)"))

    def test_d_a_clock_that_stopped_is_a_line_for_cory(self):
        h = Harness()
        h.run(feed(), NOW)
        h.run(feed(), NOW + 10 * M)
        self.assertEqual(h.state().cory["batches"], [])
        h.run(feed(), NOW + 50 * M)   # 40 minutes with no pass
        batches = h.state().cory["batches"]
        self.assertEqual(len(batches), 1)
        self.assertIn(f"PIPE: no switchboard pass from {R.central(NOW + 10 * M)} to {R.central(NOW + 50 * M)} "
                      "Central (40 min", batches[0]["text"])
        h.run(feed(), NOW + 60 * M)
        h.run(feed(), NOW + 3 * H)   # a later gap is its own line, once
        texts = [b["text"] for b in h.state().cory["batches"]]
        self.assertEqual(len(texts), 2)
        self.assertIn("(120 min", texts[1])

    def test_e_the_integrators_due_times_follow_its_runs_never_overnight(self):
        cases = [(ct(2026, 11, 9, 12, 0), ct(2026, 11, 9, 13, 17)),     # Monday noon: the 12:17 run, plus 1 h
                 (ct(2026, 11, 9, 22, 50), ct(2026, 11, 10, 8, 17)),    # 10:50 PM: not 12:17 AM, the 8:17 AM run
                 (ct(2026, 11, 10, 3, 0), ct(2026, 11, 10, 9, 17)),     # overnight: the 8:17 run, plus 1 h
                 (ct(2026, 11, 13, 23, 30), ct(2026, 11, 14, 10, 17)),  # Friday 11:30 PM: Saturday starts 9:17
                 (ct(2026, 11, 14, 20, 50), ct(2026, 11, 15, 9, 17))]   # Saturday 8:50 PM: past 9:17 PM is Sunday
        for t0, want in cases:
            self.assertEqual(R.due_at(RROSTER, "claude", t0, "ruling"), ("session", want), R.central(t0))
        for k in range(0, 7 * 24 * 60, 11):   # every 11 minutes of a week: due and dead are integrator runs
            t0 = NOW + k * M
            via, due = R.due_at(RROSTER, "claude", t0, "fix")
            dead = R.dead_time("claude", via, t0, due)
            self.assertTrue(t0 < due < dead)
            for x in (due, dead):
                loc = x.astimezone(CENTRAL)
                first, last, minute = R.INTEGRATOR_RUNS[loc.weekday() >= 5]
                self.assertTrue(loc.minute == minute and first <= loc.hour <= last, f"{R.central(t0)} -> {loc}")
        # end to end: an obligation made at 10:50 PM is not re-sent or dead-lettered overnight
        h = Harness()
        f = feed(prs=[pr(470, "claude/integrate-j580-x", job="J-580", action="FIX-CI", proof="red: x")])
        start = ct(2026, 11, 9, 22, 50)
        for t in (start, ct(2026, 11, 10, 2, 0), ct(2026, 11, 10, 7, 59)):
            h.run(f, t)
        ob = h.obs("J-580", ct(2026, 11, 10, 7, 59))[0]
        self.assertEqual((ob["sends"], ob["dead_lettered"], ob["due"]), (1, None, L.iso(ct(2026, 11, 10, 8, 17))))
        self.assertEqual(ob["dead"], L.iso(ct(2026, 11, 10, 10, 17)))

    def test_f_a_closed_unmerged_head_is_not_landed_and_holders_who_cannot_act(self):
        b = L.BranchIn("fb-bot/integrate-j550-rungdown", SHA["a"], t(-3), closed=(L.ClosedPr(288, SHA["a"], False),),
                       vs_trunk="diverged")
        green = L.RunIn(5, "u", b.name, b.sha, t(-3), t(-2.5), "success", "green")
        h = Harness()
        h.run(feed(branches=[b], runs=[green], issues=[issue(550, job="J-550", owner="owner:muse")]), NOW)
        rec = h.state().jobs["J-550"]
        self.assertEqual((rec["outcome"], rec["state"]), (None, "working"))
        obs = h.obs("J-550")
        self.assertEqual([(o["kind"], o["to"], o["thread"]) for o in obs], [("ruling", "claude", 288)])
        self.assertIn("closed #288 without merging at this head", obs[0]["what"])
        # and with no issue left, it still never reads as merged; deleting the branch is the answer
        h.run(feed(branches=[b], runs=[green]), NOW + 10 * M)
        self.assertIsNone(h.state().jobs["J-550"]["outcome"])
        h.run(feed(), NOW + 20 * M)
        self.assertEqual(h.obs("J-550")[0]["ack"]["how"], "branch deleted")
        self.assertNotEqual(h.state().jobs["J-550"]["outcome"], "merged")
        # green with no PR: Muse cannot open one (work/agents.yml), so the integrator owes it
        nojob = L.BranchIn("fb-bot/integrate-j551-x", SHA["b"], t(-1))
        mine = L.BranchIn("claude/integrate-j552-y", SHA["c"], t(-1))
        runs = [L.RunIn(6 + i, "u", x.name, x.sha, t(-1), t(-0.5), "success", "green") for i, x in enumerate((nojob, mine))]
        h2 = Harness()
        h2.run(feed(branches=[nojob, mine], runs=runs), NOW)
        a, c = h2.obs("J-551")[0], h2.obs("J-552")[0]
        self.assertEqual((a["to"], a["version"]), ("claude", f"open-pr:{SHA['b']}"))
        self.assertIn("Muse cannot open PRs", a["what"])
        self.assertEqual(c["to"], "claude")
        import yaml
        cfg = yaml.safe_load((HERE.parents[1] / "work/agents.yml").read_text(encoding="utf-8"))
        self.assertFalse(R.opens_prs(L.Roster(cfg), "muse"))
        self.assertTrue(R.opens_prs(L.Roster(cfg), "claude"))

    def test_g_a_partial_failure_is_degraded_not_down(self):
        class Flaky(FakeLookups):
            def landed(self, branch, sha):
                if "j560" in branch:
                    raise GitHubError("GET", "compare", "unexpected end of JSON input")
                return super().landed(branch, sha)
        h = Harness(lookups=Flaky())
        f = feed(branches=[L.BranchIn("claude/integrate-j560-a", SHA["a"], t(-1))],
                 prs=[pr(430, "claude/integrate-j561-b", job="J-561", action="FIX-CI", proof="red: x")])
        res = h.run(f, NOW)
        self.assertEqual((res.status, res.degraded), ("committed", ("J-560",)))
        st = h.state()
        self.assertIn("ERROR|J-560", st.unmatched)
        self.assertEqual([o["kind"] for o in st.jobs["J-561"]["obligations"]], ["fix"], "the rest of the pass ran")
        self.assertEqual(res.slo["degraded"], 1)
        self.assertTrue(any(line.startswith("DEGRADED") and "J-560" in line for line in h.log))
        h.lookups = FakeLookups()
        h.run(f, NOW + 10 * M)
        self.assertNotIn("ERROR|J-560", h.state().unmatched)

    def test_h_a_token_expiring_soon_is_a_line_for_cory(self):
        h = Harness()
        h.run(feed(), NOW, tokens={"GITHUB_TOKEN (bridge)": NOW + 5 * 24 * H, "ACTIONS_TOKEN": NOW + 30 * 24 * H})
        text = h.state().cory["batches"][0]["text"]
        self.assertIn("the GITHUB_TOKEN (bridge) token expires", text)
        self.assertNotIn("ACTIONS_TOKEN", text)
        h.run(feed(), NOW + 2 * H, tokens={"GITHUB_TOKEN (bridge)": NOW + 5 * 24 * H})
        self.assertEqual(sum("GITHUB_TOKEN" in b["text"] for b in h.state().cory["batches"]), 1, "once")

    def test_the_reconcilers_own_notices_are_not_asks(self):
        gh = FakeGH()
        h = Harness(outbox=R.CommentOutbox(gh, RROSTER, 340))
        f = feed(prs=[pr(440, "claude/integrate-j500-x", action="FIX-CI", proof="red: x")])
        h.run(f, NOW)
        notice = gh.bot_comments(NOW)[0]
        self.assertTrue(notice.body.startswith("To Claude:"))
        ask = L.AskIn(notice.id, 440, "u", "Claude", "ChatGPT", NOW)   # asks.py lists it: it skips only the ledger's
        h.run(feed(prs=f.prs, asks=[ask]), NOW + 10 * M, comments=gh.bot_comments(NOW))
        st = h.state()
        self.assertEqual([o["kind"] for r in st.records() for o in r["obligations"]], ["fix"])

    def test_stage_2_is_one_switch(self):
        root = pathlib.Path(tempfile.mkdtemp(dir=TMP.name))
        (root / "work").mkdir()
        (root / R.CONFIG).write_text("reconciler: live\n")
        old = R.STAGE2_READY
        try:
            R.STAGE2_READY = False
            self.assertEqual(R.mode(root), "off", "live reads as off until STAGE2_READY")
            R.STAGE2_READY = True
            self.assertEqual(R.mode(root), "live")
        finally:
            R.STAGE2_READY = old
        self.assertTrue(R.STAGE2_READY, "J-072: the replay's clean verdict set it")
        out, cory = R.outboxes(False, FakeGH(), RROSTER, 340)
        self.assertEqual((out.live, cory.live), (False, False))
        out, cory = R.outboxes(True, FakeGH(), RROSTER, 340)
        self.assertIsInstance(out, R.CommentOutbox)
        self.assertIsInstance(cory, R.CoryNotifier)


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

    def test_the_ledgers_own_trunk_compare_is_the_reader(self):
        b = L.BranchIn("claude/integrate-j512-done", SHA["d"], t(-3), vs_trunk="behind")
        live = L.BranchIn("claude/integrate-j513-wip", SHA["c"], t(-3), vs_trunk="ahead", ahead_by=2)
        h = Harness()
        green = [L.RunIn(3, "u", x.name, x.sha, t(-3), t(-2.5), "success", "green") for x in (b, live)]
        h.run(feed(branches=[b, live], runs=green), NOW)
        jobs = h.state().jobs
        self.assertEqual((jobs["J-512"]["outcome"], jobs["J-512"]["obligations"]), ("merged", []))
        self.assertIsNone(jobs["J-513"]["outcome"])
        self.assertEqual([o["kind"] for o in jobs["J-513"]["obligations"]], ["fix"], "green, no PR: open one")
        self.assertEqual(h.lookups.calls, 0, "no lookup when the feed read it")

    def test_merged_pr_at_the_head_with_the_issue_open_owes_nothing(self):
        b = L.BranchIn("fb-bot/integrate-j511-x", SHA["e"], t(-3), closed=(L.ClosedPr(300, SHA["e"], True),),
                       vs_trunk="diverged")
        h = Harness()
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
        # Codex: the integrator posts the mention at its run (12:17 PM, due 1:17 PM), then Codex has 30 min
        self.assertEqual(R.due_at(ROSTER, "codex", NOW), ("relay", ct(2026, 11, 9, 13, 47)))
        self.assertEqual(R.due_at(ROSTER, "claude", NOW, "review"), ("api", NOW + 30 * M), "the API reviews")
        self.assertEqual(R.due_at(ROSTER, "claude", NOW, "ruling"), ("session", ct(2026, 11, 9, 13, 17)))
        self.assertEqual(R.due_at(ROSTER, "muse", NOW), ("email", NOW + 4 * H))
        # NOW is 12:00 PM Central: Grok's next run is 4 PM Central (22:00 UTC), plus 1 h
        self.assertEqual(R.due_at(ROSTER, "grok", NOW), ("issue", dt.datetime(2026, 11, 9, 23, 0, tzinfo=dt.timezone.utc)))
        # ChatGPT's review task runs hourly at :27 Central: its next run after 12:00 PM is 12:27 PM, plus 1 h
        self.assertEqual(R.due_at(ROSTER, "chatgpt", NOW), ("issue", ct(2026, 11, 9, 13, 27)))
        self.assertEqual(R.due_at(MROSTER, "chatgpt", NOW), ("cory", NOW + 15 * M), "manual goes to Cory")

    def test_grok_bot_reads_all_the_time_grok_runs_on_its_schedule(self):
        # Cory 11-09: Grok Bot (Builder, PE) reads always; Grok runs at 10 AM and 4 PM. The repo file says so.
        try:
            import yaml
        except ImportError:
            self.skipTest("PyYAML not installed")
        repo = L.Roster(yaml.safe_load((HERE.parents[1] / "work/agents.yml").read_text(encoding="utf-8")))
        self.assertEqual(repo.wake("grok_bot"), "watch")
        self.assertEqual(R.due_at(repo, "grok_bot", NOW), ("watch", NOW + H), "Builder and PE: one hour")
        self.assertEqual(R.due_at(repo, "grok", NOW), ("issue", dt.datetime(2026, 11, 9, 23, 0, tzinfo=dt.timezone.utc)))
        self.assertEqual(R.due_at(repo, "chatgpt", NOW), ("issue", ct(2026, 11, 9, 13, 27)))

    def test_manual_agent_goes_straight_to_cory_never_resent(self):
        h = Harness(roster=MROSTER)
        f = feed(prs=[pr(412, "chatgpt/integrate-j530-x", job="J-530", action="FIX-CI", proof="red: x")])
        h.run(f, NOW)
        h.run(f, NOW + 3 * H)
        ob = h.obs("J-530")[0]
        self.assertEqual((ob["via"], ob["sends"], ob["dead_lettered"]), ("cory", 1, None))

    def test_a_reply_by_the_addressee_acks_only_when_it_names_it(self):
        h = Harness()
        f = feed(prs=[pr(413, "claude/integrate-j500-x", action="FIX-CI", proof="red: x")])
        h.run(f, NOW)
        other = cmt(90, 413, NOW + 5 * M, "Grok: drive-by note")
        h.run(f, NOW + 10 * M, comments=[other])
        self.assertIsNone(h.obs("J-500")[0]["ack"])
        mine = cmt(91, 413, NOW + 12 * M, "Claude (integrator): fixing it now\n\n_Generated by [Claude Code]_")
        h.run(f, NOW + 20 * M, comments=[other, mine])
        self.assertIsNone(h.obs("J-500")[0]["ack"], "on its thread, naming nothing: not an ack (#360's rule)")
        oid = h.obs("J-500")[0]["id"]
        named = cmt(96, 340, NOW + 22 * M, f"Claude (integrator): {oid} is mine, fixing it now")
        h.run(f, NOW + 30 * M, comments=[other, mine, named])
        self.assertEqual(h.obs("J-500")[0]["ack"]["how"], "reply naming it", "a post anywhere that names it")

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
        old = R.STAGE2_READY
        try:
            for ready, live in ((False, "off"), (True, "live")):   # J-072: STAGE2_READY is set; False gates it again
                R.STAGE2_READY = ready
                for text, want in (("reconciler: shadow\n", "shadow"), ("reconciler: live\n", live),
                                   ("other: 1\n", "off")):
                    (root / R.CONFIG).write_text(text)
                    self.assertEqual(R.mode(root), want, (ready, text))
        finally:
            R.STAGE2_READY = old

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


# ---------------- the cutover replay's findings (J-072, tools/switchboard/replay_reconciler.py) ----------------
class ReplayFindings(unittest.TestCase):
    """Each defect the 48 h replay of real history found, as the case that failed before its fix."""

    def test_an_ask_reply_acks_only_back_to_the_asker_or_naming_it(self):
        # #360's rule: Muse asks Claude on a job thread; Claude's next post there is to Grok: not an answer (the
        # old rule acked it, "a reply on the thread" where Claude owed nothing else)
        h = Harness()
        ask = L.AskIn("7001", 500, "u", "Claude", "Muse", NOW - 5 * M)
        f = feed(issues=[issue(500)], asks=[ask])
        text = cmt(7001, 500, NOW - 5 * M, "**Muse (via email bridge): To Claude: rule on the grip cap**")
        h.run(f, NOW, comments=[text])
        to_grok = cmt(7002, 500, NOW + 2 * M, "Claude (integrator): To Grok, check the grip math.")
        h.run(f, NOW + 10 * M, comments=[text, to_grok])
        ob = h.obs("J-500", kind="ask")[0]
        self.assertIsNone(ob["ack"], "a post to someone else is not the answer")
        back = cmt(7003, 500, NOW + 12 * M, "CLAUDE_TO_MUSE: J-500 the grip cap stays at 3800 N.")
        h.run(f, NOW + 20 * M, comments=[text, to_grok, back])
        self.assertEqual(h.obs("J-500", kind="ask")[0]["ack"]["how"], "reply to the asker")
        # one step short of #360: on the ask's own job thread (never the board), the addressee's next post there,
        # addressed to no one else, answers it ("Claude: accepted as spec H.4" on #244)
        h3 = Harness()
        ask3 = L.AskIn("7004", 500, "u", "Claude", "ChatGPT", NOW - 5 * M)
        f3 = feed(issues=[issue(500)], asks=[ask3])
        text3 = cmt(7004, 500, NOW - 5 * M, "To Claude (from ChatGPT): limb maxima are not shares.")
        h3.run(f3, NOW, comments=[text3])
        plain = cmt(7005, 500, NOW + 3 * M, "Claude: accepted as spec H.4.\n\n_Generated by [Claude Code]_")
        h3.run(f3, NOW + 10 * M, comments=[text3, plain])
        self.assertEqual(h3.obs("J-500", kind="ask")[0]["ack"]["how"], "reply on its thread")
        # on the board only a post whose first line names the asker (#340, 6743006899); a post to Grok there does not
        h4 = Harness()
        ask4 = L.AskIn("7006", 340, "u", "Claude", "Muse", NOW - 5 * M)
        text4 = cmt(7006, 340, NOW - 5 * M, "**Muse (via email bridge): To Claude: expand-tests ready to land**")
        to_grok4 = cmt(7007, 340, NOW + M, "Claude (integrator): both To Grok asks above are done.")
        status = cmt(7008, 340, NOW + 2 * M, "Claude (integrator): merged #343.")
        h4.run(feed(asks=[ask4]), NOW + 5 * M, comments=[text4, to_grok4, status])
        self.assertIsNone(h4.state().threads["obligations"][0]["ack"], "the board: a post to Grok, a status line")
        named = cmt(7009, 340, NOW + 6 * M, "Claude (integrator): Muse, the eight asks above were answered by mail.")
        h4.run(feed(asks=[ask4]), NOW + 10 * M, comments=[text4, to_grok4, status, named])
        self.assertEqual(h4.state().threads["obligations"][0]["ack"]["how"], "reply naming the asker")
        # on another thread: it acks when it names the ask's thread and is addressed back (the old rule read only
        # the ask's own thread, so an answer elsewhere was never seen and the ask was re-sent)
        h2 = Harness()
        ask2 = L.AskIn("7101", 340, "u", "Claude", "Muse", NOW - 5 * M)
        text2 = cmt(7101, 340, NOW - 5 * M, "**Muse (via email bridge): To Claude: which head do I prove?**")
        h2.run(feed(asks=[ask2]), NOW, comments=[text2])
        elsewhere = cmt(7102, 128, NOW + 3 * M, "Claude (integrator) to Muse, cc Muse: on your #340 ask, prove 1a2b3c4d.")
        h2.run(feed(asks=[ask2]), NOW + 10 * M, comments=[text2, elsewhere])
        self.assertEqual(h2.state().threads["obligations"][0]["ack"]["how"], "reply to the asker")

    def test_an_ask_asks_py_dropped_is_still_opened_from_the_stream(self):
        # asks.py on trunk drops an ask on any later post by its addressee on the thread: the feed lists nothing,
        # yet the ask is unanswered. The reconciler reads asks from the comment stream itself.
        h = Harness()
        ask = cmt(7201, 340, NOW - 5 * M, "**Muse (via email bridge): To Claude: refused mail, please resend**")
        unrelated = cmt(7202, 340, NOW - 4 * M, "Claude (integrator): Grok's asks are answered.")
        h.run(feed(), NOW, comments=[ask, unrelated], open_threads=frozenset({340}))
        obs = h.state().threads["obligations"]
        self.assertEqual([(o["kind"], o["to"], o["version"]) for o in obs], [("ask", "claude", "7201")])
        self.assertIsNone(obs[0]["ack"])

    def test_an_ask_on_a_thread_that_closed_is_closed_not_resent(self):
        # the board moved from #107 to #340: an ask on #107 was re-sent and dead-lettered after #107 closed
        h = Harness()
        ask = cmt(7301, 107, NOW - 5 * M, "Claude (integrator): To Builder, take J-071 step 2.")
        h.run(feed(asks=[L.AskIn("7301", 107, "u", "Builder", "Claude", NOW - 5 * M)]), NOW, comments=[ask],
              open_threads=frozenset({107, 340}))
        ob = h.state().threads["obligations"][0]
        self.assertEqual((ob["to"], ob["sends"]), ("grok", 1))
        h.lookups.i[107] = {"state": "closed", "state_reason": "completed"}
        h.run(feed(), NOW + 10 * M, comments=[ask], open_threads=frozenset({340}))
        h.run(feed(), NOW + 20 * H, comments=[ask], open_threads=frozenset({340}))
        ob = h.state().threads["obligations"][0]
        self.assertEqual((ob["closed"] or {}).get("why"), "its thread closed")
        self.assertEqual((ob["sends"], ob["dead_lettered"]), (1, None), "never re-sent, never dead-lettered")

    def test_a_head_that_lands_after_it_was_read_is_landed(self):
        # fb-bot/integrate-j024-plants: read at its head before its PR opened (not landed), then its PR merged at
        # that head; the cached first reading made it "green with no PR: open one" after the merge
        name = "fb-bot/integrate-j524-plants"
        h = Harness()
        early = L.BranchIn(name, SHA["c"], t(-1), vs_trunk="ahead", ahead_by=1, ahead=(SHA["c"],))
        run = L.RunIn(1, "u", name, SHA["c"], t(-1), None, "")
        h.run(feed(branches=[early], runs=[run]), NOW)
        self.assertEqual(h.state().jobs["J-524"]["obligations"], [])
        merged = L.BranchIn(name, SHA["c"], t(-1), closed=(L.ClosedPr(311, SHA["c"], True),))
        done = L.RunIn(1, "u", name, SHA["c"], t(-1), t(-0.5), "success")
        h.run(feed(branches=[merged], runs=[done]), NOW + 30 * M)
        self.assertEqual([o["kind"] for o in h.state().jobs["J-524"]["obligations"]], [], "landed: nothing owed")
        self.assertEqual(h.state().jobs["J-524"]["landed"][name][1], "merged #311")

    def test_a_verdict_that_is_not_a_land_answers_the_review_and_owes_a_land(self):
        # #318: the automatic review's NOTE answered the review ask, then the PR sat 10 h waiting for a LAND from
        # another vendor with no one owing anything, until the integrator asked Codex by hand
        h = Harness()
        p1 = pr(425, "fb-bot/integrate-j525-x", job="J-525", needs_review=True)
        h.run(feed(prs=[p1]), NOW)
        review = h.obs("J-525", kind="review")[0]
        self.assertEqual((review["to"], review["via"]), ("codex", "relay"))
        noted = pr(425, "fb-bot/integrate-j525-x", job="J-525",
                   reasons=(f"no LAND on @{SHA['a'][:8]} from a vendor other than meta",
                            "needs:eyes and no eye LAND on this head"))
        h.run(feed(prs=[noted]), NOW + 10 * M)
        obs = {o["version"].split(":")[0] if ":" in o["version"] else "review": o for o in h.obs("J-525")}
        self.assertEqual(obs["review"]["ack"]["how"], "verdict on the head")
        self.assertEqual((obs["land"]["kind"], obs["land"]["to"], obs["land"]["via"]), ("review", "codex", "relay"))
        self.assertEqual((obs["eye"]["kind"], obs["eye"]["to"]), ("ruling", "claude"))
        landed = pr(425, "fb-bot/integrate-j525-x", job="J-525", action="MERGE")
        h.run(feed(prs=[landed]), NOW + 20 * M)
        obs = {o["version"].split(":")[0]: o for o in h.obs("J-525") if ":" in o["version"]}
        self.assertEqual(obs["land"]["ack"]["how"], "a LAND or FIX on the head")
        self.assertEqual(obs["eye"]["ack"]["how"], "the eye's verdict on the head")

    def test_an_unseen_obligation_still_settles_on_positive_evidence(self):
        # a review whose PR dropped out of the listing went unseen and then stayed live forever after its PR merged
        h = Harness()
        h.run(feed(prs=[pr(426, "fb-bot/integrate-j526-x", job="J-526", needs_review=True)]), NOW)
        for i in range(1, 4):
            h.run(feed(issues=[issue(526, job="J-526")]), NOW + i * 10 * M)
        ob = h.obs("J-526", kind="review")[0]
        self.assertIsNotNone(ob["unseen"])
        h.lookups.p[426] = {"merged": True, "state": "closed"}
        h.run(feed(issues=[issue(526, job="J-526")]), NOW + 2 * H)
        self.assertIsNone(h.obs("J-526", kind="review")[0]["closed"], "dormant ones are re-read every RECHECK")
        h.run(feed(issues=[issue(526, job="J-526")]), NOW + 30 * M + R.RECHECK)
        self.assertEqual(h.obs("J-526", kind="review")[0]["closed"]["why"], "PR merged")

    def test_a_red_branch_with_no_job_id_is_owed_by_its_owner(self):
        # fb-bot/integrate-traversal-rungdown-bottom-gap: red 37 h, no job id; the ledger notified Muse, the
        # reconciler owed nothing
        name = "fb-bot/integrate-traversal-gap"
        b = L.BranchIn(name, SHA["e"], t(-3), vs_trunk="ahead", ahead_by=1, ahead=(SHA["e"],))
        run = L.RunIn(2, "u", name, SHA["e"], t(-3), t(-2.5), "failure", "red: native-tests")
        h = Harness()
        h.run(feed(branches=[b], runs=[run]), NOW)
        obs = h.state().threads["obligations"]
        self.assertEqual([(o["kind"], o["to"], o["ref"]) for o in obs], [("fix", "muse", f"BRANCH|{name}")])
        landed = L.BranchIn(name, SHA["e"], t(-3), vs_trunk="behind")
        h.run(feed(branches=[landed], runs=[run]), NOW + 10 * M)
        self.assertTrue(h.state().threads["obligations"][0]["closed"]["why"].startswith("landed"))

    def test_a_job_id_back_after_its_outcome_starts_a_new_round(self):
        # J-060's PR #310 merged on 11-07 (outcome merged); #320 opened as J-060 on 11-08 and its asks were never
        # opened: a terminal record ignored everything after
        h = Harness()
        b = L.BranchIn("claude/integrate-j529-sheen", SHA["d"], t(-3), vs_trunk="behind")
        h.run(feed(branches=[b]), NOW)
        self.assertEqual(h.state().jobs["J-529"]["outcome"], "merged")
        ask = L.AskIn("7401", 529, "u", "Muse", "Claude", NOW + 50 * M)
        f = feed(branches=[b], issues=[issue(529, job="J-529", owner="owner:muse")], asks=[ask])
        h.run(f, NOW + H)
        rec = h.state().jobs["J-529"]
        self.assertEqual((rec["outcome"], rec["issue"], rec["past"][0]["outcome"]), (None, 529, "merged"))
        self.assertEqual([(o["kind"], o["to"]) for o in rec["obligations"]], [("ask", "muse")])
        # a cancelled job stays cancelled while its own issue is open
        h2 = Harness()
        f2 = feed(issues=[issue(530, job="J-530")])
        h2.run(f2, NOW, comments=[cmt(7501, 530, NOW - M, "/cancel J-530")])
        h2.run(f2, NOW + 10 * M)
        self.assertEqual(h2.state().jobs["J-530"]["outcome"], "cancelled")

    def test_an_old_claude_ask_with_no_reply_is_one_daily_line_never_a_nag(self):
        # the integrator answers Muse by mail, which GitHub never sees: an ask to Claude from before today with no
        # reply on its thread is not nagged one by one; Cory gets one line for all of them, once a day
        h = Harness()
        old = L.AskIn("7601", 340, "u", "Claude", "Muse", NOW - 20 * H)   # 11-08, 4 PM Central
        old2 = L.AskIn("7602", 340, "u", "Claude", "Muse", NOW - 19 * H)
        new = L.AskIn("7603", 340, "u", "Claude", "Muse", NOW - 10 * M)
        f = feed(asks=[old, old2, new])
        for i in range(0, 50):
            h.run(f, NOW + i * 10 * M)
        obs = {o["version"]: o for o in h.state().threads["obligations"]}
        self.assertEqual((obs["7601"]["sends"], obs["7601"]["dead_lettered"]), (0, None))
        self.assertEqual(obs["7603"]["sends"], 2, "today's ask is sent, then re-sent at its due time")
        texts = [b["text"] for b in h.state().cory["batches"]]
        lines = [x for t in texts for x in t.splitlines() if "ask(s) to Claude from before today" in x]
        self.assertEqual(len(lines), 1, "one line, once")
        self.assertIn(obs["7601"]["id"], lines[0])
        self.assertIn(obs["7602"]["id"], lines[0])

    def test_two_open_asks_on_a_thread_a_reply_naming_neither_acks_neither(self):
        # O-f8e65587: two of Muse's asks to Claude open on #107; "Answering Muse's J-044c ask" named neither
        # specifically and acked her expand-tests ask too. With two or more owed there, only a named reply acks.
        h = Harness()
        a1 = L.AskIn("7701", 340, "u", "Claude", "Muse", NOW - 9 * M)
        a2 = L.AskIn("7702", 340, "u", "Claude", "Muse", NOW - 8 * M)
        t1 = cmt(7701, 340, NOW - 9 * M, "**Muse (via email bridge): To Claude: expand-tests scope-step pin ready**")
        t2 = cmt(7702, 340, NOW - 8 * M, "**Muse (via email bridge): To Claude: J-044c wreck fix pushed @ 8657f4f4, "
                                         "balance red, not from this fix**")
        f = feed(asks=[a1, a2])
        h.run(f, NOW, comments=[t1, t2])
        vague = cmt(7703, 340, NOW + M, "Claude (integrator): Muse, answering your ask: #298 covers it.")
        h.run(f, NOW + 10 * M, comments=[t1, t2, vague])
        self.assertEqual([o["ack"] for o in h.state().threads["obligations"]], [None, None])
        quoted = cmt(7704, 340, NOW + 11 * M, "Claude (integrator): Muse,\n> J-044c wreck fix pushed @ 8657f4f4\n"
                                               "parked: #298 covers it.")
        h.run(f, NOW + 20 * M, comments=[t1, t2, vague, quoted])
        acks = {o["version"]: o["ack"] for o in h.state().threads["obligations"]}
        self.assertIsNone(acks["7701"])
        self.assertEqual(acks["7702"]["how"], "reply naming it")

    def test_cory_batches_follow_the_integrators_hours_at_most_every_3_hours(self):
        # 28 batches in 48 h, some at night, was too many (the coordinator, 11-09)
        mon = ct(2026, 11, 9, 0, 0)
        self.assertFalse(R.cory_due(None, mon + 60 * M), "1 AM: none outside the integrator's hours")
        self.assertFalse(R.cory_due(None, mon + 8 * H + 5 * M), "8:05 AM: the morning batch is at 8:17")
        self.assertTrue(R.cory_due(None, mon + 8 * H + 17 * M))
        self.assertFalse(R.cory_due(mon + 8 * H + 17 * M, mon + 10 * H), "one per slot")
        self.assertTrue(R.cory_due(mon + 8 * H + 17 * M, mon + 11 * H + 17 * M))
        self.assertTrue(R.cory_due(mon + 8 * H + 17 * M, mon + 10 * H, pipe=True), "a pipe item goes at once")
        self.assertFalse(R.cory_due(None, mon + 2 * H, pipe=True), "but not at night")
        sat = ct(2026, 11, 7, 0, 0)
        self.assertFalse(R.cory_due(sat + 9 * H + 17 * M, sat + 12 * H), "Sat-Sun slots: 9:17, 1:17, 5:17, 9:17")
        self.assertTrue(R.cory_due(sat + 9 * H + 17 * M, sat + 13 * H + 17 * M))
        self.assertFalse(R.cory_due(None, sat + 22 * H), "Sat-Sun hours end at 9 PM")

    def test_the_first_live_pass_sends_what_shadow_only_recorded(self):
        # shadow counts its would-be sends: left alone, the first live notice would be the re-send at the due
        # time (or none), and a manual ask shadow batched for Cory would never reach him
        h = Harness(roster=MROSTER)
        f = feed(prs=[pr(427, "claude/integrate-j527-x", job="J-527", action="FIX-CI", proof="red: x"),
                      pr(428, "chatgpt/integrate-j528-x", job="J-528", action="FIX-CI", proof="red: y")])
        h.run(f, NOW)
        h.run(f, NOW + 10 * M)
        self.assertEqual(h.obs("J-527")[0]["sends"], 1)
        manual = h.obs("J-528")[0]
        self.assertIn(manual["id"], h.state().cory["reported"])
        gh = FakeGH()
        h.outbox, h.cory = R.CommentOutbox(gh, MROSTER, 340), R.CoryNotifier(gh, 340)
        h.run(f, NOW + 90 * M)
        ob = h.obs("J-527")[0]
        bodies = [b for _, b, _ in gh.comments]
        self.assertTrue(any(f"id={ob['id']} send=1 attempt=1" in b for b in bodies), "its first real notice")
        self.assertEqual((ob["sends"], ob.get("sent_by")), (1, "live"))
        self.assertEqual(ob["due"], L.iso(R.session_due(NOW + 90 * M)), "a fresh clock from the cutover")
        self.assertTrue(any(b.startswith("To Cory: Needs you") and manual["id"] in b for b in bodies),
                        "Cory gets what shadow only recorded")
        self.assertEqual(h.state().cory["mode"], "live")
        n = len(gh.comments)
        h.run(f, NOW + 100 * M, comments=gh.bot_comments(NOW + 90 * M))
        self.assertEqual(len(gh.comments), n, "once: the next live pass sends nothing again")


if __name__ == "__main__":
    unittest.main()
