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

import plan  # noqa: E402
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
                 dry_run=False, fail_write=""):
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
        if "/commits/" in path:
            head = path.rsplit("/commits/", 1)[1]
            return {"sha": head, "commit": {"message": self.commits[head]}}
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


# work/plan.md as the auto-open reads it (SYSTEM_PLAN item 2): a job opens only with a plan line that holds nothing.
# Every job these tests and test_inbox's and test_branch_pages' open has its line; J-084 is held, its Muse head kept.
PLAN_TEXT = """limit claude 4
limit muse 9
""" + "".join(f"J-{n:03d} muse #{200 + n} | Work {n}\n" for n in (1, 2, 3, 4, 5, 21, 42, 44, 47, 48, 51, 72, 98, 105,
                                                                  642)) + """\
J-084 claude #402 kept fb-bot/integrate-j084@56a593ef90c1 hold: paused by Cory 11-13 | One scenario plays the climb
J-090 claude #408 hold: paused by Cory 11-13 | Hook her and let the rope decide
"""
PLAN = plan.parse(PLAN_TEXT)
assert not PLAN.errors, PLAN.errors


def make(gh, team=TEAM, model=PLAN):
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.now, sw.team, sw.lines, sw._load = gh, NOW, team, [], None
    sw.out = sw.lines.append
    sw.read_plan = lambda: model
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
        gh.jobs[7] = [{"name": "native-tests", "status": "in_progress", "conclusion": None}]
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_no_proof_run_opens_none(self):
        gh, head = green_setup()
        gh.runs = {}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

    def test_a_green_head_with_no_plan_job_is_one_inbox_item_per_head(self):
        # lifted from #515 (SYSTEM_PLAN item 2): a green head the switch cannot open was skipped in silence every sweep
        for name, message, job in (("fb-bot/integrate-windup", "Budget the windup lift\n\nNo job line here.", ""),
                                   ("claude/integrate-ammo", "Budget the windup lift\n\nNo job line here.", ""),
                                   ("fb-bot/integrate-j777-orphan", "Budget the windup lift", "J-777"),
                                   ("fb-bot/integrate-windup", "Budget the windup lift\n\nJob: J-778", "J-778")):
            gh, head = green_setup(name=name)
            gh.state = {}
            gh.commits[head] = message
            sw = make(gh)
            self.assertEqual(sw.auto_open_prs(), 0)
            self.assertEqual(gh.writes, [])
            (item,) = sw.auto_pr_failed
            self.assertEqual((item.kind, item.target, item.head), ("AUTO-PR-FAILED", name, head))
            if job:
                self.assertIn(f", {job}: green with no PR, and {job} has no line in work/plan.md", item.line)
            else:
                self.assertIn("no `Job: J-NNN` line", item.line)
            self.assertEqual(switch.inbox_class(item), "hold")   # it reaches the integrator, never a digest entry
            self.assertEqual(sw.auto_open_prs(), 0)   # the inbox never ran (a fast or quiet sweep): made again
            self.assertEqual(sw.auto_pr_failed, (item,))
            gh.state["inbox"] = {"keys": [item.key]}   # the inbox listed it (its ring, as Switch.inbox saves it)
            gh.gets.clear()
            self.assertEqual(sw.auto_open_prs(), 0)   # once per head: no proof or PR read, no item, the next sweep
            self.assertEqual(sw.auto_pr_failed, ())
            self.assertFalse(any("workflows/" in g or "pulls?state=all" in g for g in gh.gets), gh.gets)
            new = sha(5)   # a new head is asked again
            gh.branches = [branch(name, 5)]
            gh.commits[new], gh.runs[new] = message, [green_run(new)]
            self.assertEqual(sw.auto_open_prs(), 0)
            self.assertEqual([(it.target, it.head) for it in sw.auto_pr_failed], [(name, new)])

    def test_a_job_given_its_plan_line_after_the_item_opens_on_the_next_sweep(self):
        gh, head = green_setup(name="fb-bot/integrate-j777-orphan")
        gh.state = {}
        gh.commits[head] = "Budget the windup lift"
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(make(gh, model=plan.parse(PLAN_TEXT + "J-777 muse #777 | The orphan\n")).auto_open_prs(), 1)
        self.assertEqual(gh.writes[0][2]["body"].split("\n")[0], "Job: J-777")

    def test_a_red_head_with_no_plan_job_is_no_item(self):
        gh, head = green_setup(name="fb-bot/integrate-windup")
        gh.commits[head] = "No job line here."
        gh.runs[head][0] = dict(gh.runs[head][0], conclusion="failure")
        gh.jobs[7] = [{"name": "native-tests", "status": "completed", "conclusion": "failure"}]
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual(sw.auto_pr_failed, ())   # a red head is the RED-BRANCH notice's

    def test_a_merge_commit_head_opens_with_its_branch_job(self):
        # fact 3: fb-bot/integrate-j021, -j089, -j094, -j098 and -protected-j105 sat green with a trunk merge on top
        gh, head = green_setup(name="fb-bot/integrate-j098")
        gh.commits[head] = "Merge branch 'ScraperX-Claude' into fb-bot/integrate-j098"
        sw = make(gh)
        self.assertEqual(sw.auto_open_prs(), 1)
        (method, path, body), = gh.writes
        self.assertEqual((body["head"], body["title"]), ("fb-bot/integrate-j098", "J-098: Work 98"))
        self.assertTrue(body["body"].startswith("Job: J-098\n\nOpened by the switchboard from "
                                                "fb-bot/integrate-j098 @ " + head), body["body"])
        self.assertEqual(sw.auto_pr_failed, ())

    def test_a_merge_subject_with_a_job_line_takes_the_plan_title_too(self):
        gh, head = green_setup(name="fb-bot/integrate-j098")
        gh.commits[head] = "Merge trunk in\n\nJob: J-042"
        self.assertEqual(make(gh).auto_open_prs(), 1)
        self.assertEqual(gh.writes[0][2]["title"], "J-042: Work 42")   # the Job: line wins over the name
        self.assertTrue(gh.writes[0][2]["body"].startswith("Job: J-042\n"))

    def test_a_lettered_and_a_protected_branch_name_give_the_job(self):
        for name, job in (("fb-bot/integrate-j044c-wreck", "J-044c"), ("fb-bot/integrate-protected-j105", "J-105"),
                          ("claude/integrate-protected-j042-ammo", "J-042")):
            gh, head = green_setup(name=name)
            gh.commits[head] = "Fold the lunge in"   # no Job: line: the name gives it
            self.assertEqual(make(gh).auto_open_prs(), 1, name)
            (method, path, body), = gh.writes
            self.assertEqual((body["head"], body["title"]), (name, "Fold the lunge in"))
            self.assertTrue(body["body"].startswith(f"Job: {job}\n"), body["body"])

    def test_a_held_job_and_a_kept_head_open_nothing(self):
        kept = "56a593ef90c1"
        for name, n, message in (("claude/integrate-j090", 1, "Hook her\n\nJob: J-090"),
                                 ("claude/integrate-j084", 1, "Climb it\n\nJob: J-084"),
                                 ("fb-bot/integrate-j042-windup", 2, "Wind up\n\nJob: J-090")):
            gh, head = green_setup(n=n, name=name)
            gh.commits[head] = message
            sw = make(gh)
            self.assertEqual(sw.auto_open_prs(), 0, name)
            self.assertEqual((gh.writes, sw.auto_pr_failed), ([], ()), name)
            self.assertTrue(any("SKIP" in line and "holds" in line for line in sw.lines), sw.lines)
            self.assertFalse(any("workflows/" in g for g in gh.gets), gh.gets)   # no proof read for a held job
        kept_head = kept + "0" * 28
        gh = FakeGH(branches=[{"name": "fb-bot/integrate-j042-windup", "commit": {"sha": kept_head}}],
                    commits={kept_head: "Wind up\n\nJob: J-042"}, runs={kept_head: [green_run(kept_head)]},
                    jobs={7: GREEN_JOBS})
        keeps = plan.parse(PLAN_TEXT.replace("J-042 muse #242 |", f"J-042 muse #242 kept fb-bot/integrate-j042-windup@{kept} |"))
        self.assertEqual(keeps.by_job()["J-042"].kept, (("fb-bot/integrate-j042-windup", kept),))
        sw = make(gh, model=keeps)
        self.assertEqual(sw.auto_open_prs(), 0)
        self.assertEqual((gh.writes, sw.auto_pr_failed), ([], ()))
        self.assertTrue(any("SKIP" in line and "keeps this head" in line for line in sw.lines), sw.lines)
        self.assertFalse(any("/commits/" in g for g in gh.gets), gh.gets)   # nothing read for a kept head
        gh.branches = [branch("fb-bot/integrate-j042-windup", 1)]   # the kept branch moved: unlanded work again
        gh.commits[sha(1)], gh.runs[sha(1)] = "Wind up more\n\nJob: J-042", [green_run(sha(1))]
        self.assertEqual(make(gh, model=keeps).auto_open_prs(), 1)

    def test_open_pr_opens_none(self):
        gh, head = green_setup()
        gh.open_prs = [{"number": 256, "head": {"ref": "fb-bot/integrate-j042-windup"}}]
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])
        self.assertFalse(any("/commits/" in g for g in gh.gets))   # nothing read for a branch that has a PR

    def test_a_closed_or_merged_pr_at_this_head_opens_none(self):
        for merged in (None, "2026-11-06T11:00:00Z"):
            gh, head = green_setup()
            gh.prior = {"fb-bot/integrate-j042-windup": [{"number": 250, "state": "closed", "merged_at": merged,
                                                          "head": {"sha": head}}]}
            sw = make(gh)
            self.assertEqual(sw.auto_open_prs(), 0)
            self.assertEqual((gh.writes, sw.auto_pr_failed), ([], ()))

    def test_a_branch_one_commit_past_its_merged_or_closed_pr_opens_a_new_pr(self):
        # fact 3: fb-bot/integrate-j129 sat green one commit past its merged #483; one branch per job, reused
        for name, job in (("fb-bot/integrate-j042-windup", "J-042"), ("claude/integrate-j042-ammo", "J-042")):
            for merged in ("2026-11-06T11:00:00Z", None):
                gh, head = green_setup(name=name, job=job)
                gh.state = {}
                gh.prior = {name: [{"number": 483, "state": "closed", "merged_at": merged, "head": {"sha": sha(9)}}]}
                sw = make(gh)
                self.assertEqual(sw.auto_open_prs(), 1, (name, merged))
                (method, path, body), = gh.writes
                self.assertEqual((body["head"], body["base"]), (name, rules.TRUNK))
                self.assertTrue(body["body"].startswith(f"Job: {job}\n"), body["body"])

    def test_the_head_a_pr_had_is_kept_and_never_asked_again(self):
        gh, head = green_setup()
        gh.state = {"prior_pr": ["fb-bot/integrate-j042-windup"]}   # the memo before item 2: a bare branch name
        gh.prior = {"fb-bot/integrate-j042-windup": [{"number": 250, "state": "closed",
                                                      "merged_at": "2026-11-06T11:00:00Z", "head": {"sha": head}}]}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.state["prior_pr"], [f"fb-bot/integrate-j042-windup@{head}"])   # the bare name dropped
        gh.gets.clear()
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertFalse(any("pulls?state=all" in g or "/commits/" in g for g in gh.gets), gh.gets)
        new = sha(10)   # one more commit pushed after the merge
        gh.branches = [branch("fb-bot/integrate-j042-windup", 10)]
        gh.commits[new], gh.runs[new] = "Pace the windup\n\nJob: J-042", [green_run(new)]
        self.assertEqual(make(gh).auto_open_prs(), 1)
        self.assertEqual(gh.state["prior_pr"], [])   # the old head is gone from the branch: forgotten
        self.assertEqual(gh.writes[0][2]["title"], "Pace the windup")

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

    def test_claude_branch_with_no_job_line_opens_by_its_name_and_never_twice_at_one_head(self):
        gh, head = green_setup(name="claude/integrate-j042-ammo", job="J-042")
        gh.commits[head] = "Work with no job line"
        self.assertEqual(make(gh).auto_open_prs(), 1)
        self.assertTrue(gh.writes[0][2]["body"].startswith("Job: J-042\n"))
        gh, head = green_setup(name="claude/integrate-j042-ammo", job="J-042")
        gh.prior = {"claude/integrate-j042-ammo": [{"number": 497, "state": "closed", "merged_at": "2026-11-13T13:00:00Z",
                                                    "head": {"sha": head}}]}
        self.assertEqual(make(gh).auto_open_prs(), 0)
        self.assertEqual(gh.writes, [])

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


if __name__ == "__main__":
    unittest.main()
