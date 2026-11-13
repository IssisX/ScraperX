"""Unit tests for the switchboard auto-opening PRs for finished branches (J-047).

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

import rules  # noqa: E402
import switch  # noqa: E402

AGENTS = {
    "agents": {
        "claude": {"vendor": "anthropic", "names": ["Claude"], "branch_prefixes": ["claude/"]},
        "chatgpt": {"vendor": "openai", "names": ["ChatGPT"], "branch_prefixes": ["chatgpt/"]},
        "grok": {"vendor": "xai", "names": ["Grok", "PE"], "branch_prefixes": ["grok/", "pe/"]},
        "muse": {"vendor": "meta", "names": ["Muse"], "branch_prefixes": ["muse/"]},
    },
    "review": {"claude_authored_asks": ["chatgpt", "muse"], "matron_reviewer": "muse"},
    "verdict_logins": ["IssisX", "github-actions[bot]"],
}
TEAM = rules.team_from_config(AGENTS)
NOW = dt.datetime(2026, 11, 6, 13, 0, tzinfo=dt.timezone.utc)
HEAD = "1a2b3c4d5e6f708192a3b4c5d6e7f8091a2b3c4d"
RUN_URL = "https://github.com/IssisX/ScraperX/actions/runs/7"


def sha(n: int) -> str:
    return f"{n:040x}"


def branch(name: str, n: int) -> dict:
    return {"name": name, "commit": {"sha": sha(n)}, "protected": False}


def green_run(sha_value: str) -> dict:
    return {"id": 7, "head_sha": sha_value, "status": "completed", "conclusion": "success",
            "created_at": "2026-11-06T12:00:00Z", "updated_at": "2026-11-06T12:30:00Z", "html_url": RUN_URL}


GREEN_JOBS = [{"name": "native-tests", "status": "completed", "conclusion": "success"},
              {"name": "host-build", "status": "completed", "conclusion": "success"},
              {"name": "wake-switchboard", "status": "completed", "conclusion": "success"}]


class FakeGH:
    """The GitHub calls auto_open_prs makes: reads from tables, writes recorded (printed in a dry run)."""

    repo, dry_run = "IssisX/ScraperX", False

    def __init__(self, branches=(), open_prs=(), prior=None, commits=None, runs=None, jobs=None,
                 dry_run=False, fail_write="", compares=None):
        self.compares = compares or {}   # head sha -> the compare of trunk with it
        self.branches = list(branches)
        self.open_prs = list(open_prs)
        self.prior = prior or {}
        self.commits = commits or {}
        self.runs = runs or {}
        self.jobs = jobs or {}
        self.dry_run = dry_run
        self.fail_write = fail_write
        self.gets, self.writes, self.log, self.next_pr = [], [], [], 100

    def paged(self, path, key=""):
        self.gets.append(path)
        if path.startswith(f"repos/{self.repo}/branches"):
            return list(self.branches)
        if "pulls?state=open" in path:
            return list(self.open_prs)
        if "pulls?state=all&head=" in path:
            name = path.split("head=")[1].split("&")[0].split(":", 1)[1]
            return list(self.prior.get(name, []))
        return []

    def get(self, path, accept=""):
        self.gets.append(path)
        if "/actions/workflows/" in path and "/runs" in path:
            head = path.split("head_sha=")[1].split("&")[0]
            return {"workflow_runs": list(self.runs.get(head, []))}
        if "/actions/runs/" in path and "/jobs" in path:
            rid = int(path.split("/actions/runs/")[1].split("/")[0])
            return {"jobs": list(self.jobs.get(rid, []))}
        if "/compare/" in path and path.rsplit("...", 1)[1] in self.compares:
            return dict(self.compares[path.rsplit("...", 1)[1]])
        if "/commits/" in path:
            head = path.rsplit("/commits/", 1)[1]
            got = self.commits[head]   # a message, or {"message", "committer"}; no date: committed just now
            return {"sha": head, "commit": got if isinstance(got, dict) else
                    {"message": got, "committer": {"date": NOW.strftime("%Y-%m-%dT%H:%M:%SZ")}}}
        raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)")

    def write(self, method, path, body=None, what=""):
        self.writes.append((method, path, body))
        if self.dry_run:
            self.log.append(f"DRY  {method} {path}  {what}")
            return None
        if self.fail_write:
            raise switch.GitHubError(method, path, self.fail_write)
        if method == "POST" and path.endswith("/pulls"):
            self.next_pr += 1
            return {"number": self.next_pr, "html_url": f"https://github.com/{self.repo}/pull/{self.next_pr}"}
        return {}


def make(gh, team=TEAM):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, NOW, team, [], None
    sw.out = sw.lines.append
    return sw


def green_setup(n=1, name="fb-bot/integrate-j042-windup", job="J-042"):
    """One branch whose head is finished: Job line in the message, proof green, no PR anywhere."""
    head = sha(n)
    gh = FakeGH(
        branches=[branch(name, n)],
        commits={head: f"Budget the windup lift\n\nJob: {job}\n\nWhole hold from the one account."},
        runs={head: [green_run(head)]},
        jobs={7: GREEN_JOBS},
    )
    return gh, head


class AutoOpen(unittest.TestCase):
    def test_green_head_opens_one_pr(self):
        gh, head = green_setup()
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 1)
        (method, path, body), = gh.writes
        self.assertEqual((method, path), ("POST", "repos/IssisX/ScraperX/pulls"))
        self.assertEqual(body["title"], "Budget the windup lift")
        self.assertEqual(body["head"], "fb-bot/integrate-j042-windup")
        self.assertEqual(body["base"], "ScraperX-Claude")
        self.assertEqual(body["body"], "Job: J-042\n\nOpened by the switchboard from "
                         "fb-bot/integrate-j042-windup @ " + head + "\n\nJob: J-042\n\nWhole hold from the one account.")
        self.assertTrue(any("OPENED" in line and "#101" in line for line in sw.lines))

    def test_red_head_opens_none(self):
        gh, head = green_setup()
        gh.runs[head][0] = dict(gh.runs[head][0], conclusion="failure")
        gh.jobs[7] = [{"name": "native-tests", "status": "completed", "conclusion": "failure"}]
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        self.assertTrue(any("SKIP" in line and "red" in line for line in sw.lines))

    def test_running_proof_opens_none(self):
        gh, head = green_setup()
        gh.runs[head][0] = dict(gh.runs[head][0], status="in_progress", conclusion=None)
        gh.jobs = {7: [{"name": "native-tests", "status": "in_progress", "conclusion": None}]}
        gh.jobs[7] = [{"name": "native-tests", "status": "in_progress", "conclusion": None}]
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_no_proof_run_opens_none(self):
        gh, head = green_setup()
        gh.runs = {}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_missing_job_line_opens_none_and_a_green_head_is_one_inbox_item(self):
        # 11-13 (the system test): a new branch's green head with no Job line was skipped in silence every sweep
        for name in ("fb-bot/integrate-j042-windup", "claude/integrate-j042-ammo"):
            gh, head = green_setup(name=name)
            gh.state = {}
            gh.commits[head] = "Budget the windup lift\n\nNo job line here."
            sw = make(gh)
            self.assertEqual(sw.auto_open_prs(), 0)
            self.assertEqual(gh.writes, [])
            (item,) = sw.auto_pr_failed
            self.assertEqual((item.kind, item.target, item.head), ("AUTO-PR-FAILED", name, head))
            self.assertIn("no `Job: J-NNN` line", item.line)
            self.assertEqual(switch.inbox_class(item), "hold")   # it reaches the integrator, never a digest entry
            gh.gets.clear()
            self.assertEqual(sw.auto_open_prs(), 0)   # once per head: no read, no item, the next sweep
            self.assertEqual(sw.auto_pr_failed, ())
            self.assertFalse(any("workflows/" in g or "pulls?state=all" in g for g in gh.gets), gh.gets)

    def test_new_heads_with_no_job_line_are_checked_a_few_a_sweep(self):
        names = [f"fb-bot/integrate-j{n:03d}" for n in range(1, switch.MAX_NO_JOB_CHECKS + 3)]
        gh = FakeGH(branches=[branch(nm, i + 1) for i, nm in enumerate(names)],
                    commits={sha(i + 1): "no job line" for i in range(len(names))},
                    runs={sha(i + 1): [green_run(sha(i + 1))] for i in range(len(names))}, jobs={7: GREEN_JOBS})
        gh.state = {}
        sw = make(gh)
        sw.auto_open_prs()
        self.assertEqual(len(sw.auto_pr_failed), switch.MAX_NO_JOB_CHECKS)
        sw.auto_open_prs()   # the next sweep: the rest
        self.assertEqual(len(sw.auto_pr_failed), 2)
        self.assertEqual(len(gh.state["no_job_at"]), len(names))

    def test_heads_whose_proof_has_not_run_never_hide_a_green_one_after_them(self):
        n = switch.MAX_NO_JOB_CHECKS
        names = [f"fb-bot/integrate-a{i:02d}" for i in range(n)] + ["fb-bot/integrate-z-green"]
        gh = FakeGH(branches=[branch(nm, i + 1) for i, nm in enumerate(names)],
                    commits={sha(i + 1): "no job line" for i in range(len(names))},
                    runs={sha(n + 1): [green_run(sha(n + 1))]}, jobs={7: GREEN_JOBS})   # only the last has a proof
        gh.state = {}
        sw = make(gh)
        sw.auto_open_prs()
        self.assertEqual(sw.auto_pr_failed, ())   # the first sweep's count went on the waiting heads
        self.assertEqual(len(gh.state["no_job_wait"]), n)
        sw = make(gh)   # each sweep is a new switch (its per-run memos start empty)
        sw.auto_open_prs()   # the next sweep: the waiting heads wait on their clock, the green one is read
        self.assertEqual([it.target for it in sw.auto_pr_failed], ["fb-bot/integrate-z-green"])
        gh.gets.clear()
        make(gh).auto_open_prs()
        self.assertFalse(any("workflows/" in g for g in gh.gets), gh.gets)   # nothing read again inside the hour
        sw = make(gh)
        sw.now = NOW + switch.NO_JOB_RECHECK
        sw.auto_open_prs()
        self.assertEqual(len([g for g in gh.gets if "workflows/" in g]), n)   # each waiting head once, no PR listing
        self.assertFalse(any("pulls?state=all" in g for g in gh.gets), gh.gets)

    def test_missing_job_line_on_a_red_running_or_old_head_is_no_item(self):
        gh, head = green_setup()
        gh.state = {}
        gh.commits[head] = "No job line here."
        gh.runs[head][0] = dict(gh.runs[head][0], conclusion="failure")
        gh.jobs = {7: [{"name": "native-tests", "status": "completed", "conclusion": "failure"}]}
        sw = make(gh)
        sw.auto_open_prs()
        self.assertEqual(sw.auto_pr_failed, ())   # a red head is the RED-BRANCH notice's
        self.assertEqual(gh.state["no_job_at"], {"fb-bot/integrate-j042-windup": head})
        gh, head = green_setup()
        gh.state = {}
        gh.commits[head] = "No job line here."
        gh.runs[head][0] = dict(gh.runs[head][0], status="in_progress", conclusion=None)
        gh.jobs = {7: [{"name": "native-tests", "status": "in_progress", "conclusion": None}]}
        sw = make(gh)
        sw.auto_open_prs()
        self.assertEqual((sw.auto_pr_failed, gh.state["no_job_at"]), ((), {}))   # read again when its proof ends
        self.assertEqual(gh.state["no_job_wait"]["fb-bot/integrate-j042-windup"]["head"], head)
        gh.runs[head][0] = dict(gh.runs[head][0], status="completed", conclusion="success")
        gh.jobs = {7: GREEN_JOBS}
        sw = make(gh)
        sw.auto_open_prs(only={"fb-bot/integrate-j042-windup"})   # a fast sweep: its proof just finished
        self.assertEqual([it.kind for it in sw.auto_pr_failed], ["AUTO-PR-FAILED"])
        self.assertEqual(gh.state["no_job_wait"], {})
        gh, head = green_setup()
        gh.commits[head] = {"message": "No job line here.", "committer": {"date": "2026-10-27T12:00:00Z"}}
        sw = make(gh)
        sw.auto_open_prs()
        self.assertEqual(sw.auto_pr_failed, ())
        self.assertFalse(any("workflows/" in g or "pulls?state=all" in g for g in gh.gets), gh.gets)

    def test_open_pr_opens_none(self):
        gh, head = green_setup()
        gh.open_prs = [{"number": 256, "head": {"ref": "fb-bot/integrate-j042-windup"}}]
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        self.assertFalse(any("/commits/" in g for g in gh.gets))   # nothing read for a branch that has a PR

    def test_closed_pr_opens_none(self):
        gh, head = green_setup()
        gh.prior = {"fb-bot/integrate-j042-windup": [{"number": 250, "state": "closed", "merged_at": None}]}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_merged_pr_opens_none(self):
        gh, head = green_setup()
        gh.prior = {"fb-bot/integrate-j042-windup": [{"number": 250, "state": "closed",
                                                      "merged_at": "2026-11-06T11:00:00Z"}]}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_protected_branch_opens(self):
        gh, head = green_setup(name="fb-bot/integrate-protected-j048-no-run-hurt", job="J-048")
        self.assertEqual(make(gh).auto_open_prs(), 1)
        self.assertEqual(gh.writes[0][2]["head"], "fb-bot/integrate-protected-j048-no-run-hurt")

    def test_claude_branch_opens_so_no_helper_opens_one_by_hand(self):
        # 11-13: a helper's hand-opened PR subscribed the integrator session to it (#497, #498); the switch opens it
        for name in ("claude/integrate-protected-j042-ammo", "claude/integrate-j042-ammo"):
            gh, head = green_setup(name=name, job="J-042")
            self.assertEqual(make(gh).auto_open_prs(), 1, name)
            (method, path, body), = [w[:3] for w in gh.writes]
            self.assertEqual((method, body["head"], body["base"]), ("POST", name, rules.TRUNK))
            self.assertTrue(body["body"].startswith("Job: J-042\n"), body["body"])

    def test_claude_branch_with_no_job_line_or_a_prior_pr_never_opens(self):
        gh, head = green_setup(name="claude/integrate-j042-ammo", job="J-042")
        gh.commits[head] = "Work with no job line"
        self.assertEqual(make(gh).auto_open_prs(), 0)
        gh, head = green_setup(name="claude/integrate-j042-ammo", job="J-042")
        gh.prior = {"claude/integrate-j042-ammo": [{"number": 497, "state": "closed", "merged_at": "2026-11-13T13:00:00Z"}]}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_an_old_claude_branch_is_never_opened_or_listed(self):
        # 11-13: 77 old claude/integrate-* heads with no PR; the first pass after #498 listed every one
        gh, head = green_setup(name="claude/integrate-agent-tools-20261027", job="J-042")
        gh.commits[head] = {"message": gh.commits[head], "committer": {"date": "2026-10-27T12:00:00Z"}}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual((gh.writes, [g for g in gh.gets if "pulls?state=all" in g]), ([], []))
        gh, head = green_setup(name="fb-bot/integrate-j042-windup", job="J-042")   # Muse's: unchanged
        gh.commits[head] = {"message": gh.commits[head], "committer": {"date": "2026-10-27T12:00:00Z"}}
        self.assertEqual(make(gh).auto_open_prs(), 1)

    def test_the_integrators_own_branches_never_open(self):
        for name in ("claude/integrator-inbox", "claude/j042-ammo", "claude/wip-j042"):
            gh, head = green_setup(name=name, job="J-042")
            self.assertEqual(make(gh).auto_open_prs(), 0, name)

    def test_other_prefixes_never_open(self):
        gh, head = green_setup(name="fb-bot/j001-pastrail", job="J-001")
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_cap_of_3_holds_in_branch_name_order(self):
        branches, commits, runs = [], {}, {}
        for n, name in enumerate(["fb-bot/integrate-j044c-wreck", "fb-bot/integrate-j042-windup",
                                  "fb-bot/integrate-j048-no-run-hurt", "fb-bot/integrate-95-plant-fix",
                                  "fb-bot/integrate-j044l"], start=1):
            head = sha(n)
            branches.append(branch(name, n))
            commits[head] = f"Work {n}\n\nJob: J-{n:03d}"
            runs[head] = [green_run(head)]
        gh = FakeGH(branches=branches, commits=commits, runs=runs, jobs={7: GREEN_JOBS})
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 3)
        self.assertEqual(len(gh.writes), 3)
        self.assertEqual([w[2]["head"] for w in gh.writes],
                         ["fb-bot/integrate-95-plant-fix", "fb-bot/integrate-j042-windup",
                          "fb-bot/integrate-j044c-wreck"])

    def test_commit_body_is_kept_verbatim_even_when_it_is_only_the_job_line(self):
        gh, head = green_setup()
        gh.commits[head] = "Budget the windup lift\n\nJob: J-042"
        self.assertEqual(make(gh).auto_open_prs(), 1)
        self.assertEqual(gh.writes[0][2]["body"], "Job: J-042\n\nOpened by the switchboard from "
                         "fb-bot/integrate-j042-windup @ " + head + "\n\nJob: J-042")

    def test_dry_run_prints_only(self):
        gh, head = green_setup()
        gh.dry_run = True
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 1)
        self.assertEqual(len(gh.writes), 1)   # the fake records the attempt; the real write sends nothing
        self.assertTrue(any("DRY" in line and "open PR" in line for line in sw.lines))

    def test_permission_refusal_reports_exactly_and_stops_the_sweep(self):
        gh, head = green_setup()
        gh.fail_write = "gh: Resource not accessible by integration (HTTP 403): not permitted to create pull requests"
        branches, commits, runs = list(gh.branches), dict(gh.commits), dict(gh.runs)
        head2 = sha(2)
        branches.append(branch("fb-bot/integrate-j044l", 2))
        commits[head2] = "Fold the lunge in\n\nJob: J-044"
        runs[head2] = [green_run(head2)]
        gh.branches, gh.commits, gh.runs = branches, commits, runs
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(len(gh.writes), 1)   # the second branch was never attempted
        self.assertTrue(any("not permitted to create pull requests" in line for line in sw.lines))
        self.assertTrue(any("repo setting" in line for line in sw.lines))


class PastAMergedPr(unittest.TestCase):
    """11-13: c200cd50 and 566cd4bd sat on claude/integrate-rate-budget after #505 merged, and nothing saw them.
    Commits on a branch past its merged PR that trunk lacks are unlanded work: a new PR, by the same path."""

    NAME = "claude/integrate-rate-budget"
    OLD = sha(9)   # the head #505 merged at

    def moved(self, files=("tools/switchboard/switch.py",), ahead=2, prs=None, message=None):
        gh, head = green_setup(name=self.NAME, job="J-072")
        gh.state = {}
        gh.prior = {self.NAME: prs if prs is not None else
                    [{"number": 505, "state": "closed", "merged_at": "2026-11-13T15:00:00Z", "head": {"sha": self.OLD}}]}
        gh.compares = {head: {"ahead_by": ahead, "files": [{"filename": f} for f in files]}}
        if message is not None:
            gh.commits[head] = message
        return gh, head

    def test_new_commits_past_a_merged_pr_open_a_new_pr(self):
        gh, head = self.moved()
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 1)
        (method, path, body), = gh.writes
        self.assertEqual((body["head"], body["base"]), (self.NAME, rules.TRUNK))
        self.assertTrue(body["body"].startswith("Job: J-072\n"), body["body"])
        self.assertTrue(any("past merged #505" in line for line in sw.lines), sw.lines)
        self.assertEqual(sw.auto_pr_failed, ())

    def test_fb_bot_branch_reused_after_its_merge_opens_too(self):
        gh, head = green_setup(name="fb-bot/integrate-j042-windup", job="J-042")
        gh.prior = {"fb-bot/integrate-j042-windup": [{"number": 250, "state": "closed",
                                                      "merged_at": "2026-11-06T11:00:00Z", "head": {"sha": sha(9)}}]}
        gh.compares = {head: {"ahead_by": 1, "files": [{"filename": "src/a.cpp"}]}}
        self.assertEqual(make(gh).auto_open_prs(), 1)

    def test_the_merged_head_itself_opens_none_and_reads_no_compare(self):
        gh, head = self.moved()
        gh.prior[self.NAME][0]["head"]["sha"] = head
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        self.assertFalse(any("/compare/" in g for g in gh.gets))

    def test_a_head_that_only_took_trunk_in_opens_none(self):
        for ahead, files in ((0, ()), (1, ())):   # nothing trunk lacks / a merge of trunk that changes no file
            gh, head = self.moved(files=files, ahead=ahead)
            self.assertEqual(make(gh).auto_open_prs(), 0, (ahead, files))
            self.assertEqual(gh.writes, [])

    def test_a_pr_closed_unmerged_or_still_open_opens_none(self):
        gh, head = self.moved(prs=[{"number": 505, "state": "closed", "merged_at": "2026-11-13T15:00:00Z",
                                    "head": {"sha": self.OLD}},
                                   {"number": 510, "state": "closed", "merged_at": None, "head": {"sha": sha(8)}}])
        self.assertEqual(make(gh).auto_open_prs(), 0)
        gh, head = self.moved()
        gh.open_prs = [{"number": 512, "head": {"ref": self.NAME}}]
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_a_red_moved_head_waits_for_green(self):
        gh, head = self.moved()
        gh.runs[head][0] = dict(gh.runs[head][0], conclusion="failure")
        gh.jobs[7] = [{"name": "native-tests", "status": "completed", "conclusion": "failure"}]
        self.assertEqual(make(gh).auto_open_prs(), 0)
        gh.runs[head][0] = green_run(head)   # the fix lands green: the next sweep opens it
        gh.jobs[7] = GREEN_JOBS
        self.assertEqual(make(gh).auto_open_prs(), 1)

    def test_a_covered_head_is_kept_and_a_new_head_is_asked_again(self):
        gh, head = self.moved()
        gh.prior[self.NAME][0]["head"]["sha"] = head
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(gh.state["prior_pr_at"], {self.NAME: head})
        gh.gets.clear()
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertFalse(any("pulls?state=all" in g or "/commits/" in g for g in gh.gets))   # no read at all
        new = sha(10)   # two more commits pushed after the merge
        gh.branches = [branch(self.NAME, 10)]
        gh.commits[new] = "Pace each run on the hour\n\nJob: J-072"
        gh.runs[new] = [green_run(new)]
        gh.compares[new] = {"ahead_by": 2, "files": [{"filename": "tools/switchboard/switch.py"}]}
        self.assertEqual(sw.auto_open_prs(), 1)
        self.assertEqual(gh.writes[0][2]["body"].split("\n")[0], "Job: J-072")

    def test_names_only_memo_is_dropped_so_a_moved_head_is_seen(self):
        gh, head = self.moved()
        gh.state = {"prior_pr": [self.NAME]}   # the memo before 11-13: a name was never asked again
        self.assertEqual(make(gh).auto_open_prs(), 1)
        self.assertNotIn("prior_pr", gh.state)

    def test_a_moved_head_with_no_job_line_is_one_inbox_item(self):
        gh, head = self.moved()
        gh.state = {"prior_pr_at": {self.NAME: self.OLD}}
        gh.commits[head] = "Pace each run on the hour, no job line"
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        (item,) = sw.auto_pr_failed
        self.assertEqual((item.kind, item.target, item.head), ("AUTO-PR-FAILED", self.NAME, head))
        self.assertIn("past merged #505", item.line)
        self.assertIn("Job: line", item.line)
        gh.gets.clear()
        self.assertEqual(sw.auto_open_prs(), 0)   # asked once per head
        self.assertEqual(sw.auto_pr_failed, ())
        self.assertFalse(any("pulls?state=all" in g for g in gh.gets))

    def test_a_moved_head_with_no_job_line_and_nothing_unlanded_is_quiet(self):
        gh, head = self.moved(files=(), ahead=1)
        gh.state = {"prior_pr_at": {self.NAME: self.OLD}}
        gh.commits[head] = "Merge trunk in"
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(sw.auto_pr_failed, ())


if __name__ == "__main__":
    unittest.main()
