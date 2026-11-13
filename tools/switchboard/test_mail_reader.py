"""The mail reader (mail_reader.py): the bridge's posts and the switch's plan notices from whole thread reads, trusted
logins only, and `missing`: a commit the worker's newest mail names that GitHub does not have. Real timelines go in
(fixtures/jobs: J-106's local "baseline" f5ee22d6b, J-059's local protected head ad9fc702); GitHub is a fake that
answers as the REST API did on 11-12 (GET commits/<sha> 422 for those two). No network."""
from __future__ import annotations

import dataclasses
import datetime as dt
import json
import pathlib
import re
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import mail_reader  # noqa: E402
import machine  # noqa: E402
import plan  # noqa: E402
import rules  # noqa: E402
import switch  # noqa: E402

REPO = "IssisX/ScraperX"
BOARD = 384
BOT = rules.BOT_LOGIN
TEAM = rules.team_from_config(switch.load_yaml(HERE.parents[1] / "work/agents.yml"))
FIXTURES = HERE / "fixtures" / "jobs"


def fixture(name: str) -> dict:
    return json.loads((FIXTURES / f"{name}.json").read_text(encoding="utf-8"))


def comment(cid: int, number: int, body: str, at: str, login: str = "IssisX") -> dict:
    return {"id": cid, "issue_url": f"https://api.github.com/repos/{REPO}/issues/{number}", "body": body,
            "user": {"login": login}, "created_at": at, "updated_at": at}


def as_comment(e: dict) -> dict | None:
    """A fixture event as the comment GitHub holds: the bridge's first line for Muse's mail, the record's for the
    integrator's, the switch's own login for its notices."""
    text = "\n".join([e.get("text") or ""] + list(e.get("lines") or []))
    issue, cid = (e.get("refs") or {}).get("issue"), e["id"]
    if not issue or not cid.startswith("c"):
        return None
    body = {"mail_in": f"**Muse (via email bridge): {e.get('subject')}**\n\n{text}\n\n_Mailed by the bridge._",
            "mail_out": f"CLAUDE_TO_MUSE: {e.get('subject')}\n\n" + "\n".join("| " + ln for ln in text.split("\n")),
            "notice_to_muse": f"CLAUDE_TO_MUSE: {e.get('subject')}\n\n{text}",
            "post": f"{e.get('subject') or ''}\n\n{text}"}.get(e["kind"])
    return body and comment(int(cid[1:]), int(issue), body, e["t"], e["actor"])


def comments_to(name: str, last_i: int) -> list[dict]:
    return [c for e in fixture(name)["events"] if e["i"] <= last_i and (c := as_comment(e))]


class GH:
    """GitHub's REST reads as the switch's client makes them: paged() stops at ghapi.MAX_PAGES pages without saying
    so, get() reads one page or one commit. `absent`: commit shas (any prefix) GitHub answers 422 for."""

    def __init__(self, comments=(), absent=(), fail=None):
        self.repo = REPO
        self.by_issue: dict[int, list[dict]] = {}
        for c in comments:
            self.by_issue.setdefault(int(c["issue_url"].rsplit("/", 1)[1]), []).append(c)
        self.absent, self.fail = tuple(absent), dict(fail or {})
        self.gets: list[str] = []

    def commits_read(self) -> list[str]:
        return [g.rsplit("/", 1)[1] for g in self.gets if "/commits/" in g]

    def get(self, path, accept=""):
        self.gets.append(path)
        base = path.split("?")[0]
        if base in self.fail:
            raise self.fail[base]
        m = re.fullmatch(rf"repos/{REPO}/commits/([0-9a-f]+)", base)
        if m:
            if any(a.startswith(m.group(1)) or m.group(1).startswith(a) for a in self.absent):
                raise switch.GitHubError("GET", path, f"gh: No commit found for SHA: {m.group(1)} (HTTP 422)", 422)
            return {"sha": m.group(1)}
        page = int((re.search(r"[?&]page=(\d+)", path) or [0, 1])[1])
        if base == f"repos/{REPO}/issues/comments":
            since = re.search(r"since=([^&]+)", path).group(1)
            items = sorted((c for cs in self.by_issue.values() for c in cs if c["updated_at"] >= since),
                           key=lambda c: c["created_at"], reverse="direction=desc" in path)
        else:
            m = re.fullmatch(rf"repos/{REPO}/issues/(\d+)/comments", base)
            if not m:
                raise switch.GitHubError("GET", path, "gh: Not Found (HTTP 404)", 404)
            items = sorted(self.by_issue.get(int(m.group(1)), []), key=lambda c: c["created_at"])
        return items[(page - 1) * 100: page * 100]

    def paged(self, path, key=""):
        sep, items = "&" if "?" in path else "?", []
        for page in range(1, switch.ghapi.MAX_PAGES + 1):   # as ghapi.paged: stops at the cap, silently
            batch = self.get(f"{path}{sep}per_page=100&page={page}")
            items.extend(batch)
            if len(batch) < 100:
                break
        return items


def make(gh, now: str) -> switch.Switch:
    sw = switch.Switch.__new__(switch.Switch)
    sw.gh, sw.team, sw._board_issue, sw.lines = gh, TEAM, BOARD, []
    sw.now = dt.datetime.fromisoformat(now.replace("Z", "+00:00"))
    sw.out = sw.lines.append
    return sw


def issue(number: int, title: str, comments: int = 1) -> dict:
    return {"number": number, "title": title, "state": "open", "comments": comments}


def branch(name: str, sha: str) -> dict:
    return {"name": name, "commit": {"sha": sha}}


def full(prefix: str) -> str:
    return prefix + "0" * (40 - len(prefix))


J106_PLAN = plan.parse("limit muse 6\nlimit claude 4\n"
                       "J-063 muse #321 | Her climbing on real support\n"
                       "J-106 muse #425 | matron-ladder fails on its own baseline\n")
J106_READS = {"issues": [issue(321, "J-063 Her climbing on real support"), issue(425, "J-106 matron-ladder"),
                         issue(BOARD, "Board")],
              "branches": [branch("fb-bot/integrate-j063-torso-drive", full("007f4efe8"))], "prs": []}
J059_PLAN = plan.parse("limit muse 6\nJ-059 muse #307 | Her black eye\n")


def j059_reads(head: str) -> dict:
    feed = "fb-bot/integrate-j059-injury-feed"
    return {"issues": [issue(307, "J-059 Her black eye"), issue(BOARD, "Board")], "branches": [branch(feed, head)],
            "prs": [{"number": 343, "state": "open", "head": {"ref": feed, "sha": head}, "body": "Job: J-059\n"}]}


class Fixtures(unittest.TestCase):
    def test_j106_the_local_baseline_is_missing_and_the_pushed_head_is_not(self):
        # 11-12 16:02: Muse's correction names f5ee22d6b, "our own J-063 head, not trunk". GitHub: 422.
        gh = GH(comments_to("j106-local-head-not-trunk", 28), absent=["f5ee22d6b"])
        sw = make(gh, "2026-11-12T16:10:00Z")
        mail = mail_reader.read(sw, J106_PLAN, J106_READS)
        self.assertEqual(mail.missing, {"J-106": "f5ee22d6b"})
        self.assertIn("J-106 started", [p.subject for p in mail.posts if not p.hand])
        self.assertIn("J-106 #425 taking it", [p.subject for p in mail.posts if p.hand])
        self.assertNotIn("007f4efe8", gh.commits_read())   # J-063's head is a branch head in the reads: no GET
        self.assertLessEqual(len(gh.commits_read()), mail_reader.SHA_READS)
        ev = machine.collect(J106_PLAN, J106_READS, mail=mail)["J-106"]
        self.assertEqual(ev.unpushed, "f5ee22d6b")
        # this plan still gives J-106 to Muse, so the "taking it" mail is the conflict; without that mail the row
        # is the push itself, owed by Muse at once
        self.assertEqual(machine.step(ev).state, "conflict")
        row = machine.step(dataclasses.replace(ev, clashes=()))
        self.assertEqual((row.gate, row.owes, row.limit), ("unpushed", "muse", machine.NOW))
        self.assertIn("push f5ee22d6b to GitHub", row.next)

    def test_j059_ad9fc702_never_reached_github(self):
        # 11-11 17:27: the re-ack says P1 #1 is "answered by the protected branch ad9fc702 (proof PASSED)".
        gh = GH(comments_to("j059-343-stacked-deadlock-scope", 38), absent=["ad9fc702"])
        mail = mail_reader.read(make(gh, "2026-11-11T18:00:00Z"), J059_PLAN, j059_reads(full("b1177151")))
        self.assertEqual(mail.missing, {"J-059": "ad9fc702"})
        self.assertIn("ad9fc702", gh.commits_read())
        self.assertNotIn("b1177151", gh.commits_read())   # #343's head: known from the reads
        for not_a_commit in ("2c7fcb6a", "8adc27c8", "5854689010", "3696ab5f"):   # /ack O-..., a review id, "local"
            self.assertFalse(any(not_a_commit in g for g in gh.gets), not_a_commit)
        notices = {int(e["id"][1:]) for e in fixture("j059-343-stacked-deadlock-scope")["events"]
                   if e["kind"] == "notice_to_muse" and e["i"] <= 38}
        self.assertTrue(notices)
        self.assertFalse(notices & {p.id for p in mail.posts}, "the switch's own CLAUDE_TO_MUSE posts are not mail")
        mails = {int(e["id"][1:]) for e in fixture("j059-343-stacked-deadlock-scope")["events"]
                 if e["kind"] == "mail_in" and e["i"] <= 38}
        self.assertEqual({p.id for p in mail.posts if not p.hand}, mails)   # every one of Muse's, on #307 and #384

    def test_j059_once_the_fix_is_pushed_the_claim_moves_on(self):
        # 11-12 18:32: the newest J-059 mail names the pushed merge 8df92be5, its "local merge commit 9c5a44ee1" and
        # live-trunk parents; 17:22 named "tree 8d2ce0d4". Neither the local merge nor the tree is a claim.
        gh = GH(comments_to("j059-343-stacked-deadlock-scope", 65), absent=["ad9fc702", "9c5a44ee1", "8d2ce0d4"])
        mail = mail_reader.read(make(gh, "2026-11-12T18:50:00Z"), J059_PLAN,
                                j059_reads("8df92be54d124273a8d0e67b396b3bfeb4f50855"))
        self.assertEqual(mail.missing, {})
        self.assertTrue(gh.commits_read(), "the newest mail's other commits were checked")
        self.assertFalse(set(gh.commits_read()) & {"9c5a44ee1", "8d2ce0d4"}, gh.commits_read())
        self.assertTrue(any(p.subject.startswith("To Claude: J-059 merge pushed") for p in mail.posts))


class Trust(unittest.TestCase):
    NOW = "2026-11-12T20:00:00Z"
    VER = machine.order_version(J059_PLAN, "muse")   # what machine.collect computes for these reads

    def base(self, extra=()):
        mark = machine.NOTICE_MARK.format(owner="muse", ver=self.VER, jobs="")
        return [comment(1, BOARD, f"{machine.NOTICE_HEAD}{self.VER}\n\nyour order\n{mark}", "2026-11-12T19:00:00Z",
                        BOT),
                comment(2, 307, f"**Muse (via email bridge): J-059 plan {self.VER} read**\n\nPlan {self.VER}"
                        f" read. Mail id 1a1d05f880ea4e5d. /ack O-2c7fcb6a. Head abc1234def.", "2026-11-12T19:10:00Z"),
                *extra]

    def test_a_forged_bridge_post_or_notice_is_not_mail(self):
        forged = [comment(3, 307, "**Muse (via email bridge): J-059 started**\n\nHead deadbee1234.",
                          "2026-11-12T19:20:00Z", "drive-by-user"),
                  comment(4, BOARD, "CLAUDE_TO_MUSE: plan fedcba987654\n\n" + machine.NOTICE_MARK.format(
                      owner="muse", ver="fedcba987654", jobs=""), "2026-11-12T19:21:00Z", "drive-by-user"),
                  comment(5, BOARD, machine.NOTICE_MARK.format(owner="muse", ver="aaaaaaaaaaa1", jobs=""),
                          "2026-11-12T19:22:00Z", "IssisX")]   # a team login, but only the switch sends notices
        gh = GH(self.base(forged), absent=["abc1234def", "deadbee1234"])
        mail = mail_reader.read(make(gh, self.NOW), J059_PLAN, j059_reads(full("b1177151")))
        self.assertEqual([p.id for p in mail.posts], [2])
        self.assertEqual([(o, v) for o, v, *_ in mail.notices], [("muse", self.VER)])
        self.assertEqual(mail.missing, {"J-059": "abc1234def"})
        self.assertNotIn("deadbee1234", gh.commits_read())
        rows = machine.collect(J059_PLAN, j059_reads(full("b1177151")), mail=mail)
        self.assertIsNotNone(rows["J-059"].notice_at)
        self.assertIsNotNone(rows["J-059"].receipt_at)

    def test_receipts_obligations_and_mail_ids_are_not_commits(self):
        gh = GH(self.base(), absent=["abc1234def"])
        mail_reader.read(make(gh, self.NOW), J059_PLAN, j059_reads(full("b1177151")))
        self.assertEqual(gh.commits_read(), ["abc1234def"])   # not the plan version (either case), O-..., mail id

    def test_a_tree_and_a_head_the_mail_calls_local_are_not_claims(self):
        post = comment(6, 307, "**Muse (via email bridge): J-059 pushed @ 2d330dc0**\n\nfeed @ 2d330dc0 (tree "
                       "8d2ce0d4, parent b1177151; remote object for local 3696ab5f; local merge commit 9c5a44ee1).",
                       "2026-11-12T19:30:00Z")
        gh = GH(self.base([post]), absent=["8d2ce0d4", "3696ab5f", "9c5a44ee1"])
        mail = mail_reader.read(make(gh, self.NOW), J059_PLAN, j059_reads(full("b1177151")))
        self.assertEqual(mail.missing, {})
        self.assertEqual(gh.commits_read(), ["2d330dc0"])


J077_PLAN = plan.parse("limit muse 6\nJ-077 muse #392 | a dropped heavy piece puts her down\n")
J077_MAIL = ("**Muse (via email bridge): J-077 FYI drop scenario rewritten, pushed @ {new} plan 629305576a35**\n\n"
             "Branch fb-bot/integrate-j077, old head 4a81dbcb, new head {new}.\n\nPer your ruling (1a1d583852f82962): "
             "the gate fix be74b436f is dropped — it stays local-only on fb-bot/integrate-protected-j077, never "
             "pushed. Base: merged current trunk 7fbaea5a into the branch first (merge commit 12163bd1).")


def j077_reads(head: str) -> dict:
    return {"issues": [issue(392, "J-077 a dropped heavy piece puts her down"), issue(BOARD, "Board")],
            "branches": [branch("fb-bot/integrate-j077", head)], "prs": []}


class SubjectClaim(unittest.TestCase):
    """11-13 (6858217631 on #392): the mail's subject says what it pushed; a commit its body calls dropped is not owed."""
    NOW = "2026-11-13T16:10:00Z"

    def test_j077_the_pushed_head_clears_the_mail_and_the_dropped_commit_is_not_owed(self):
        gh = GH([comment(6858217631, 392, J077_MAIL.format(new="20ac7723"), "2026-11-13T15:59:00Z")],
                absent=["be74b436f"])
        reads = j077_reads(full("20ac7723"))
        mail = mail_reader.read(make(gh, self.NOW), J077_PLAN, reads)
        self.assertEqual(mail.missing, {})
        self.assertEqual(machine.collect(J077_PLAN, reads, mail=mail)["J-077"].unpushed, "")

    def test_a_subject_claim_read_on_github_also_clears_the_body_history(self):
        gh = GH([comment(6858217631, 392, J077_MAIL.format(new="20ac7723"), "2026-11-13T15:59:00Z")],
                absent=["be74b436f"])
        reads = j077_reads(full("4a81dbcb"))   # the branch listing is older than the push: the claim is read
        mail = mail_reader.read(make(gh, self.NOW), J077_PLAN, reads)
        self.assertEqual(gh.commits_read()[0], "20ac7723")   # the claim first
        self.assertEqual(mail.missing, {})

    def test_a_subject_head_github_lacks_is_still_owed_while_the_old_head_stands(self):
        gh = GH([comment(6858217631, 392, J077_MAIL.format(new="30bd8834"), "2026-11-13T15:59:00Z")],
                absent=["30bd8834", "be74b436f"])
        reads = j077_reads(full("4a81dbcb"))   # the branch still holds the old head the body names
        mail = mail_reader.read(make(gh, self.NOW), J077_PLAN, reads)
        self.assertEqual(mail.missing, {"J-077": "30bd8834"})
        row = machine.step(machine.collect(J077_PLAN, reads, mail=mail)["J-077"])
        self.assertEqual((row.gate, row.owes), ("unpushed", "muse"))
        self.assertIn("push 30bd8834 to GitHub", row.next)


class Bounds(unittest.TestCase):
    NOW = "2026-11-12T20:00:00Z"

    def test_at_most_sha_reads_gets_a_pass_and_the_rest_is_said(self):
        n = mail_reader.SHA_READS + 3
        text = "limit muse 20\n" + "".join(f"J-{500 + i} muse #{500 + i} | job {i}\n" for i in range(n))
        model = plan.parse(text)
        posts = [comment(10 + i, 500 + i, f"**Muse (via email bridge): J-{500 + i} pushed**\n\nHead {i:04d}abcdef0.",
                         f"2026-11-12T19:{i:02d}:00Z") for i in range(n)]
        gh = GH(posts, absent=[f"{i:04d}abcdef0" for i in range(n)])
        sw = make(gh, self.NOW)
        mail = mail_reader.read(sw, model, {"issues": [issue(500 + i, f"J-{500 + i} job") for i in range(n)]})
        self.assertEqual(len(gh.commits_read()), mail_reader.SHA_READS)
        self.assertEqual(len(mail.missing), mail_reader.SHA_READS)
        self.assertIn(f"J-{500 + n - 1}", mail.missing)   # newest mail first
        self.assertTrue(any("3 named commit(s)" in ln and f"{mail_reader.SHA_READS}-read bound" in ln
                            for ln in sw.lines), sw.lines)

    def test_a_failed_commit_read_is_unknown_not_missing(self):
        gh = GH([comment(2, 307, "**Muse (via email bridge): J-059 pushed**\n\nHead abc1234def.", "2026-11-12T19:00:00Z")],
                fail={f"repos/{REPO}/commits/abc1234def": switch.GitHubError("GET", "x", "gh: HTTP 502", 502)})
        sw = make(gh, self.NOW)
        mail = mail_reader.read(sw, J059_PLAN, j059_reads(full("b1177151")))
        self.assertEqual(mail.missing, {})
        self.assertEqual(len(mail.posts), 1)
        self.assertTrue(any("not counted missing" in ln for ln in sw.lines), sw.lines)

    def test_the_board_is_read_past_the_client_cap(self):
        noise = [comment(1000 + i, BOARD, f"Grok: note {i}", "2026-11-01T00:00:00Z") for i in range(640)]
        late = comment(9999, BOARD, "**Muse (via email bridge): J-059 started**\n\nstarted.", "2026-11-09T10:00:00Z")
        gh = GH(noise + [late])
        mail = mail_reader.read(make(gh, self.NOW), J059_PLAN, {"issues": [issue(BOARD, "Board")]})
        self.assertEqual([p.id for p in mail.posts], [9999])   # comment 641 on the board: page 7
        self.assertTrue(any("page=7" in g for g in gh.gets))

    def test_threads_past_the_bound_are_unread_never_a_raise_and_never_no_mail(self):   # Grok #471 hole 8
        n = mail_reader.MAIL_THREADS + 2
        model = plan.parse("limit muse 9\n" + "".join(f"J-{600 + i} muse #{600 + i} | job {i}\n" for i in range(n)))
        issues = [dict(issue(600 + i, f"J-{600 + i} t"), updated_at=f"2026-11-{1 + i % 9:02d}T00:00:00Z")
                  for i in range(n)]
        issues[0]["updated_at"] = "2026-11-12T19:00:00Z"   # the newest thread is read first
        start = comment(7, 600, "**Muse (via email bridge): J-600 started**\n\nstarted.", "2026-11-12T19:00:00Z")
        gh = GH([start])
        sw = make(gh, self.NOW)
        mail = mail_reader.read(sw, model, {"issues": issues})   # returns: nothing raised
        self.assertEqual((len(mail.unread), mail.bound), (2, mail_reader.MAIL_THREADS))
        self.assertNotIn("J-600", mail.unread)
        self.assertEqual([p.id for p in mail.posts], [7])
        self.assertTrue(any("2 job thread(s) past the" in ln for ln in sw.lines), sw.lines)
        rows = {r.job: r for r in machine.table(model, machine.collect(model, {"issues": issues}, mail=mail,
                                                                                planned_at=sw.now - machine.hours(24)))}
        for job in mail.unread:   # shown unread on the switch's own clock, never as told-and-silent
            self.assertEqual((rows[job].gate, rows[job].owes), ("mail-unread", "switchboard"), job)
        self.assertEqual(rows["J-600"].gate, "claimed")
        lines = machine.alarm_lines(list(rows.values()), sw.now + machine.hours(9999))
        self.assertEqual(sum("mail thread" in ln for ln in lines), 1)   # one alarm line for the bound, not one per job

    def test_a_receipt_on_a_closed_board_is_read_with_its_notice(self):   # Grok #471 hole 10: the board rolled over
        ver = machine.order_version(J059_PLAN, "muse")
        old_board = [comment(1, 300, f"{machine.NOTICE_HEAD}{ver}\n\n" + machine.NOTICE_MARK.format(
                         owner="muse", ver=ver, jobs=""), "2026-11-10T19:00:00Z", BOT),
                     comment(2, 300, f"**Muse (via email bridge): plan {ver} read**\n\nread.", "2026-11-10T19:10:00Z")]
        gh = GH()   # the live board is new and empty; the 24 h window holds neither
        mail = mail_reader.read(make(gh, self.NOW), J059_PLAN, {"issues": []}, also=old_board)
        self.assertEqual([p.id for p in mail.posts], [2])
        ev = machine.collect(J059_PLAN, {"issues": []}, mail=mail)["J-059"]
        self.assertEqual(ev.receipt_at, dt.datetime(2026, 11, 10, 19, 10, tzinfo=dt.timezone.utc))
        self.assertEqual(mail_reader.read(make(GH(), self.NOW), J059_PLAN, {"issues": []}).posts, ())   # without it: lost

    def test_a_failed_thread_read_raises_never_an_empty_mail(self):
        gh = GH([comment(2, 307, "**Muse (via email bridge): J-059 started**\n\nstarted.", "2026-11-12T19:00:00Z")],
                fail={f"repos/{REPO}/issues/307/comments": switch.GitHubError("GET", "x", "gh: HTTP 502", 502)})
        with self.assertRaises(switch.GitHubError):
            mail_reader.read(make(gh, self.NOW), J059_PLAN, j059_reads(full("b1177151")))

    def test_threads_with_no_comments_are_not_read(self):
        gh = GH([comment(2, BOARD, "**Muse (via email bridge): J-059 started**\n\nstarted.", "2026-11-10T19:00:00Z")])
        mail = mail_reader.read(make(gh, self.NOW), J059_PLAN, {"issues": [issue(307, "J-059 x", comments=0)]})
        self.assertEqual([p.id for p in mail.posts], [2])
        self.assertFalse(any("issues/307/" in g for g in gh.gets), gh.gets)


if __name__ == "__main__":
    unittest.main()
