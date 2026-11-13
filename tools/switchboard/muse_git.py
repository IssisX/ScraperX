"""Muse's git channel (trial, 11-13): Muse talks to the team by pushing files, with no approval pop-up.

Muse pushes its own fb-bot/ branches without Cory's tap, while every GitHub comment it posts needs one; the mail
bridge (muse_bridge.gs) works around that through Gmail, which has failed in four ways: connector outages, a cut
near 4,000 characters, status mail read as asks, and Muse's injection check refusing some mail shapes. This
channel carries the same messages through git. The mail bridge keeps running beside it as the fallback.

  OUT of Muse (relay)  Muse commits one file per message to OUTBOX_BRANCH (fb-bot/mail), at mail/<name>.md: first
        line the subject it would mail today (`MUSE_TO_CLAUDE: J-002 FYI second hit now sounds`), then the body.
        Each new file is posted once, as one comment under the team login (POST_TOKEN_ENV), on the issue the mail
        bridge would pick for that subject (target), with the bridge's own first line, BRIDGE_POST. So every reader
        (the inbox's ASK and FYI, review verdicts, asks.py, the ledger, the reconciler, the pipe health) reads a git
        post exactly as a mailed one, with no change; only the footer (FOOTER) names the transport.
  IN to Muse (inbox)   Each CLAUDE_TO_MUSE notice (a comment by the switchboard or the team login whose first line
        starts CLAUDE_TO_MUSE:) is written once as inbox/<time>-<comment id>.md on INBOX_BRANCH (muse-inbox), which
        only this module writes and Muse fetches each step. Its text is shaped by work/tasks/mail-to-muse.md, as
        muse_bridge.gs mailBody_ shapes a mail: no hidden marker, backtick or link, and who sent it first.
  Team posts to Muse (forward)   A team-login post that asks Muse (`To Muse:`, `cc Muse`: to_muse, as asks.py reads
        asks) is relayed once as a switchboard CLAUDE_TO_MUSE comment plus the wake:muse label, so the mail bridge's
        OUT mails it while mail is the fallback, and the same run writes it to the inbox. Once: RELAYED_FILE, plus the
        relay's hidden mark (RELAY_MARK) read on the issue whenever the state lacks the post.

Why not a workflow `on: push` of fb-bot/mail: GitHub runs a push from the workflow file in the pushed commit, and a
branch that holds only messages has none, so trunk's code would never run. The workflow (muse-git.yml) runs after
each switchboard sweep (`workflow_run`, which runs the default branch's file) and on its own schedule. Reading
both branches is `git fetch` of a public repo, which spends nothing of the REST limit; the REST calls are the
notice listing (one a run), and per new message its target reads, its duplicate check and the post.

Once only, by path and blob: a message's key (message_key) is its path and blob sha. A posted key is kept in the
inbox branch's state (POSTED_FILE), and every post carries the key in a hidden mark (POST_MARK). A key missing from
the state is posted only when no team-login comment on the target already carries its mark, so a lost state, a
re-push or a replay never posts twice; an edited file has a new blob and is posted again, as a new message.

Muse's text is untrusted data: read with `git cat-file` only (nothing is checked out, so nothing in the branch can
run), size-bounded (MAX_READ_BYTES read, MAX_TEXT posted), and posted only. It cannot choose the repo, the path
beyond the one comments endpoint, or the first line; mentions, hidden markers (`<!--`) and Claude's footer are cut
from it (harmless), so it cannot forge this module's mark, a switchboard marker or another agent's signature.
Muse's own words keep their lines, as the mail bridge posts them: a FIX verdict's `command:` lines must count.

Run: python3 muse_git.py relay|inbox --git-dir DIR [--remote URL] [--dry-run]   (stdlib only; muse-git.yml)
Tests: tools/switchboard/test_muse_git.py
"""

from __future__ import annotations

import argparse
import dataclasses
import datetime as dt
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.parse
import urllib.request
from typing import Callable, Iterable

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "board"))
import asks  # noqa: E402  tools/board/asks.py: who wrote a post and whom it asks (the one judge's reading)

OUTBOX_BRANCH = "fb-bot/mail"     # Muse's: one file per message, mail/<name>.md
INBOX_BRANCH = "muse-inbox"       # the switchboard's: inbox/<time>-<comment id>.md, and this module's state/
BRANCHES = frozenset({OUTBOX_BRANCH, INBOX_BRANCH})   # system branches: the ledger lists neither as a lane
OUTBOX_DIR = "mail/"
INBOX_DIR = "inbox/"
POSTED_FILE = "state/posted.txt"         # the keys posted, one a line, of the files still on the outbox branch
SINCE_FILE = "state/notices-since.txt"   # the notice listing's mark: the newest comment update read
OUTBOX_REF = "refs/muse-git/outbox"      # where the two branches are fetched, in a bare repo of data only
INBOX_REF = "refs/muse-git/inbox"
# The post token (11-13): the hosted agent's token of the team login, in environment switchboard-merge, reused so Cory
# adds no secret. Its posts need Issues: read and write on this repo; it is sent on the comment POST and GET /user only.
POST_TOKEN_ENV = "AGENT_GITHUB_TOKEN"
BRIDGE_POST = "**Muse (via email bridge)"    # switch.BRIDGE_POST: the first line every reader knows (module doc)
MUSE_MARKER = "MUSE_TO_CLAUDE:"
NOTICE_MARKER = "CLAUDE_TO_MUSE:"
# Starts `_Mailed `, like the bridge's footer, so switch.unquoted drops it; no hex, `?` or asking word in it, so it
# never turns a report into an ask (switch.MUSE_WAKES, MUSE_ASKS).
FOOTER = f"_Mailed by git: {OUTBOX_BRANCH}, the git channel (trial)._"
POST_MARK = "<!-- muse-git:posted key={key} -->"
POST_KEY = re.compile(r"<!-- muse-git:posted key=(\d{1,20}) -->")
# A message file: under mail/, one level, a plain name. Anything else on the branch is never read.
MESSAGE_NAME = re.compile(r"^mail/[A-Za-z0-9][A-Za-z0-9._-]{0,119}\.md$")
MUSE_LABEL = "wake:muse"      # switch.MUSE_LABEL: the mail bridge's OUT mails a labeled issue's new switchboard notices
RELAY_MARK = "<!-- muse-git:relayed id={id} -->"   # on the switchboard's relay of team post `id` to Muse
RELAYED = re.compile(r"<!-- muse-git:relayed id=(\d+) -->")
RELAYED_FILE = "state/relayed.txt"     # the team posts relayed to Muse, by comment id
MAX_RELAY_TEXT = 50_000                # characters of a team post relayed
MAX_RELAYED_KEPT = 2000                # relayed ids kept (the listing window is far shorter)
REC_LINE = "_The mail bridge's record of a mail"   # muse_bridge.gs recordBody_: a hand mail Muse already got by mail
TEAM_LOGIN = "IssisX"         # every agent's and the owner's login (work/agents.yml verdict_logins), the bridge's too
BOT_LOGIN = "github-actions[bot]"   # rules.BOT_LOGIN: the switchboard
NOTICE_LOGINS = frozenset({TEAM_LOGIN.lower(), BOT_LOGIN})   # whose CLAUDE_TO_MUSE comments are notices
MAX_READ_BYTES = 1_000_000    # a message file larger than this is not read at all: its post says so
MAX_TEXT = 60_000             # characters of a message posted (GitHub refuses a comment over 65,536)
MAX_SUBJECT = 300             # characters of a subject kept in the first line
MAX_NOTICE_TEXT = 60_000      # characters of a notice written to the inbox
MAX_POSTS = 25                # messages posted in one run; the rest go in the next, oldest first
MAX_NOTICES = 50              # notices written in one run
MAX_PAGES = 10                # pages of 100 read from one listing
NOTICE_OVERLAP = dt.timedelta(minutes=30)   # the listing starts this long before the mark (late-indexed comments)
NOTICE_FIRST = dt.timedelta(minutes=30)     # the first run (no mark) mirrors this far back: older ones were mailed
INBOX_KEEP = dt.timedelta(days=30)          # inbox files older than this are dropped from the branch
API = "https://api.github.com/"
REPO_NAME = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
COMMITTER = {"GIT_AUTHOR_NAME": "ScraperX switchboard", "GIT_COMMITTER_NAME": "ScraperX switchboard",
             "GIT_AUTHOR_EMAIL": "41898282+github-actions[bot]@users.noreply.github.com",
             "GIT_COMMITTER_EMAIL": "41898282+github-actions[bot]@users.noreply.github.com"}


class ApiError(RuntimeError):
    def __init__(self, method: str, path: str, status: int, detail: str = ""):
        super().__init__(f"GitHub {method} {path} answered HTTP {status}: {detail[:200]}")
        self.status = status


class Api:
    """The few GitHub calls this module makes. Reads carry the workflow's token; a comment POST, and only that,
    carries the post token (the team login), so that token never reaches a read or any other path."""

    def __init__(self, repo: str, read_token: str, post_token: str = "", opener: Callable | None = None):
        if not REPO_NAME.match(repo):
            raise ValueError(f"not a repo name: {repo!r}")
        self.repo, self._read, self._post = repo, read_token, post_token
        self._open = opener or urllib.request.urlopen
        self.calls = 0

    def _call(self, method: str, path: str, token: str, body: dict | None = None):
        req = urllib.request.Request(API + path, method=method, data=None if body is None else
                                     json.dumps(body).encode("utf-8"))
        req.add_header("Accept", "application/vnd.github+json")
        req.add_header("X-GitHub-Api-Version", "2022-11-28")
        if token:
            req.add_header("Authorization", f"Bearer {token}")
        if body is not None:
            req.add_header("Content-Type", "application/json; charset=utf-8")
        self.calls += 1
        try:
            with self._open(req, timeout=30) as r:
                text = r.read().decode("utf-8")
        except urllib.error.HTTPError as e:
            raise ApiError(method, path, e.code, e.read().decode("utf-8", "replace")) from None
        return json.loads(text) if text else None

    def get(self, path: str):
        return self._call("GET", path, self._read)

    def paged(self, path: str) -> tuple[list, bool]:
        """Every page of a listing up to MAX_PAGES; True as well when it was cut there."""
        out: list = []
        sep = "&" if "?" in path else "?"
        for page in range(1, MAX_PAGES + 1):
            got = self.get(f"{path}{sep}per_page=100&page={page}") or []
            out += got
            if len(got) < 100:
                return out, False
        return out, True

    def poster(self) -> str:
        """The login the post token acts as (one read, with that token)."""
        if not self._post:
            raise RuntimeError(f"{POST_TOKEN_ENV} is not set")
        return str((self._call("GET", "user", self._post) or {}).get("login") or "")

    def switchboard_comment(self, number: int, body: str) -> dict:
        """A comment as the switchboard (the workflow's token), for a relay to Muse: the mail bridge mails only the
        workflow bot's CLAUDE_TO_MUSE comments."""
        return self._call("POST", f"repos/{self.repo}/issues/{int(number)}/comments", self._read, {"body": body}) or {}

    def label(self, number: int, name: str) -> None:
        self._call("POST", f"repos/{self.repo}/issues/{int(number)}/labels", self._read, {"labels": [name]})

    def comment(self, number: int, body: str) -> dict:
        if not self._post:
            raise RuntimeError(f"{POST_TOKEN_ENV} is not set")
        return self._call("POST", f"repos/{self.repo}/issues/{int(number)}/comments", self._post, {"body": body}) or {}


class Git:
    """git on the data repo (bare): fetch, read blobs, write the inbox commit. Never a checkout."""

    def __init__(self, git_dir: str, remote: str):
        self.dir, self.remote = git_dir, remote

    def run(self, *args: str, data: bytes | None = None, env: dict | None = None, check: bool = True) -> bytes:
        full = dict(os.environ, **(env or {}))
        p = subprocess.run(["git", f"--git-dir={self.dir}", *args], input=data, capture_output=True, env=full)
        if check and p.returncode:
            raise RuntimeError(f"git {args[0]} failed: {p.stderr.decode('utf-8', 'replace').strip()[:300]}")
        return p.stdout

    def text(self, *args: str, check: bool = True) -> str:
        return self.run(*args, check=check).decode("utf-8", "replace")

    def fetch(self) -> dict[str, str]:
        """Fetches whichever of the two branches exist into OUTBOX_REF and INBOX_REF; returns {branch: sha}."""
        if not os.path.isdir(self.dir):
            subprocess.run(["git", "init", "-q", "--bare", self.dir], check=True, capture_output=True)
        heads = {}
        for line in self.text("ls-remote", self.remote, f"refs/heads/{OUTBOX_BRANCH}",
                              f"refs/heads/{INBOX_BRANCH}").splitlines():
            sha, _, ref = line.partition("\t")
            heads[ref[len("refs/heads/"):]] = sha
        for branch, ref in ((OUTBOX_BRANCH, OUTBOX_REF), (INBOX_BRANCH, INBOX_REF)):
            if branch in heads:
                self.run("fetch", "-q", "--no-tags", self.remote, f"+refs/heads/{branch}:{ref}")
            else:
                self.run("update-ref", "-d", ref, check=False)
        return heads

    def has(self, ref: str) -> bool:
        return bool(self.text("rev-parse", "-q", "--verify", f"{ref}^{{commit}}", check=False).strip())

    def show(self, ref: str, path: str) -> str:
        if not self.has(ref):
            return ""
        return self.text("cat-file", "-p", f"{ref}:{path}", check=False)

    def tree(self, ref: str, prefix: str) -> list[tuple[str, str, str, str]]:
        """(mode, type, blob, path) of every entry under prefix at ref."""
        if not self.has(ref):
            return []
        out = []
        for rec in self.run("ls-tree", "-r", "-z", ref, "--", prefix).split(b"\0"):
            if not rec:
                continue
            meta, _, path = rec.partition(b"\t")
            mode, typ, blob = meta.decode().split(" ")
            out.append((mode, typ, blob, path.decode("utf-8", "replace")))
        return out

    def size(self, blob: str) -> int:
        return int(self.text("cat-file", "-s", blob).strip() or 0)

    def blob(self, blob: str) -> bytes:
        return self.run("cat-file", "blob", blob)

    def first_seen(self, ref: str, prefix: str) -> dict[tuple[str, str], int]:
        """The order each (path, blob) first appeared in the branch's history, oldest 0: the posting order."""
        if not self.has(ref):
            return {}
        order: dict[tuple[str, str], int] = {}
        raw = self.text("log", "--reverse", "--no-renames", "--no-abbrev", "--format=%x01", "--raw", ref, "--", prefix)
        n = 0
        for line in raw.splitlines():
            if line.startswith(":") and "\t" in line:
                meta, _, path = line.partition("\t")
                parts = meta.split(" ")
                if len(parts) >= 5:
                    order.setdefault((path, parts[3]), n)
                    n += 1
        return order

    def commit(self, parent_ref: str, writes: dict[str, str], removes: Iterable[str], message: str) -> str:
        """A commit on parent_ref (an orphan when it is missing) with these files written and removed; "" when the
        tree would not change."""
        parent = self.text("rev-parse", "-q", "--verify", f"{parent_ref}^{{commit}}", check=False).strip()
        with tempfile.TemporaryDirectory() as tmp:
            # an empty work tree of its own: update-index refuses a bare repo, and nothing is ever written to it
            os.mkdir(os.path.join(tmp, "tree"))
            env = dict(COMMITTER, GIT_INDEX_FILE=os.path.join(tmp, "index"), GIT_WORK_TREE=os.path.join(tmp, "tree"))
            self.run("read-tree", *([parent] if parent else ["--empty"]), env=env)
            for path in removes:
                self.run("update-index", "--force-remove", "--", path, env=env)
            for path, content in sorted(writes.items()):
                blob = self.run("hash-object", "-w", "--stdin", data=content.encode("utf-8")).decode().strip()
                self.run("update-index", "--add", "--cacheinfo", f"100644,{blob},{path}", env=env)
            tree = self.run("write-tree", env=env).decode().strip()
            if parent and tree == self.text("rev-parse", f"{parent}^{{tree}}").strip():
                return ""
            args = ["commit-tree", tree, "-m", message] + (["-p", parent] if parent else [])
            return self.run(*args, env=env).decode().strip()

    def push(self, sha: str, branch: str) -> None:
        self.run("push", "-q", self.remote, f"{sha}:refs/heads/{branch}")


# ---------------------------------------------------------------- Muse to the team (relay)

@dataclasses.dataclass(frozen=True)
class Message:
    path: str
    blob: str
    text: str
    size: int
    order: int

    @property
    def key(self) -> str:
        return message_key(self.path, self.blob)


def message_key(path: str, blob: str) -> str:
    """A message's key, from its path and blob sha: decimal digits, so no reader takes it for a sha (MUSE_WAKES)."""
    return str(int(hashlib.sha256(f"{path}\0{blob}".encode("utf-8")).hexdigest()[:15], 16))


def outbox(git: Git, log: Callable[[str], None] = print) -> list[Message]:
    """The message files on the outbox branch, oldest first, unread (text "", size -1: load reads one). A file not
    named MESSAGE_NAME, a link, a submodule or a tree is skipped and named in the log."""
    order = git.first_seen(OUTBOX_REF, OUTBOX_DIR)
    out = []
    for mode, typ, blob, path in git.tree(OUTBOX_REF, OUTBOX_DIR):
        if typ != "blob" or mode not in ("100644", "100755") or not MESSAGE_NAME.match(path):
            log(f"SKIP   {plain(path)[:140]}: not a message file (mail/<name>.md, a plain file)")
            continue
        out.append(Message(path, blob, "", -1, order.get((path, blob), len(order) + len(out))))
    return sorted(out, key=lambda m: (m.order, m.path))


def load(git: Git, m: Message) -> Message:
    """The message with its text; a file over MAX_READ_BYTES is not read (its post says so)."""
    size = git.size(m.blob)
    text = "" if size > MAX_READ_BYTES else git.blob(m.blob).decode("utf-8", "replace")
    return dataclasses.replace(m, text=text, size=size)


def plain(text: str) -> str:
    """One line, no markup a comment's first line could misread."""
    return re.sub(r"\s+", " ", re.sub(r"[*`\r\n]", " ", str(text))).strip()


def harmless(text: str) -> str:
    """Muse's text made safe to post under the team login: no mention, no hidden marker (so no switchboard marker
    and no POST_MARK), no Claude footer (switch ask_inbox_items reads one as Claude's own post). As the mail
    bridge's harmless_, plus the footer."""
    text = re.sub(r"@(?=[\w-])", "", str(text)).replace("<!--", "<!- -")
    return text.replace("Generated by [Claude Code]", "Generated by Claude Code")


def subject_of(first: str) -> str:
    """A message's subject words: after MUSE_TO_CLAUDE: when it is there (as the bridge's subjectRest_), else the
    whole first line; never empty."""
    rest = first[first.index(MUSE_MARKER) + len(MUSE_MARKER):] if MUSE_MARKER in first else first
    return harmless(plain(rest))[:MAX_SUBJECT].strip() or "(no subject line)"


def render(m: Message) -> tuple[str, str]:
    """(subject, comment body) for one message."""
    text = m.text.lstrip("﻿").replace("\r\n", "\n").replace("\r", "\n")
    first, _, rest = text.partition("\n")
    subject = subject_of(first)
    body = harmless(rest.strip("\n")).rstrip()
    if m.size > MAX_READ_BYTES:
        body = f"[cut: the file is {m.size} bytes, over the channel's {MAX_READ_BYTES}; it is {m.path} on {OUTBOX_BRANCH}]"
    elif len(body) > MAX_TEXT:
        body = body[:MAX_TEXT] + f"\n\n[cut: the message is {len(body)} characters; the rest is in {m.path} on {OUTBOX_BRANCH}]"
    return subject, f"{BRIDGE_POST}: {subject}**\n\n{body}\n\n{FOOTER}\n{POST_MARK.format(key=m.key)}"


class Targets:
    """The issue a subject's comment goes on, as muse_bridge.gs target_ picks it: #N when that is an open issue;
    a closed issue, the board; a PR #N, its job issue (the PR's `Job:` line, then a job title naming #N, then the
    subject's J-NNN); else the job issue J-NNN names; else the board. Never a PR. Reads are kept for the run."""

    def __init__(self, api: Api):
        self.api = api
        self._jobs: list | None = None
        self._board = 0

    def jobs(self) -> list:
        if self._jobs is None:
            self._jobs, _ = self.api.paged(f"repos/{self.api.repo}/issues?state=open&labels=job")
        return self._jobs

    def board(self) -> int:
        if not self._board:
            got, _ = self.api.paged(f"repos/{self.api.repo}/issues?state=open&labels=board")
            found = [int(i["number"]) for i in got if not i.get("pull_request")]
            if not found:
                raise RuntimeError("no open issue is labeled board")
            self._board = max(found)
        return self._board

    def by_job(self, job: str) -> int:
        for it in self.jobs():
            if not it.get("pull_request") and str(it.get("title") or "").startswith(job + " "):
                return int(it["number"])
        return 0

    def issue(self, number: int) -> dict:
        try:
            it = self.api.get(f"repos/{self.api.repo}/issues/{number}") or {}
        except ApiError as e:
            if e.status in (403, 404):   # unseen: as a PR with no body, like the bridge
                return {"pull_request": {}}
            raise
        if it.get("pull_request") is None:
            it.pop("pull_request", None)
        return it

    def of(self, subject: str) -> int:
        num = re.search(r"#(\d{1,6})\b", subject)
        job = re.search(r"\bJ-\d{3,}[a-z]?\b", subject)
        target = 0
        if num:
            n = int(num.group(1))
            rec = self.issue(n)
            if "pull_request" not in rec:
                target = n if rec.get("state") != "closed" else 0
            else:
                line = re.search(r"^\s*Job:\s*(J-\d{3,}[a-z]?)\b", str(rec.get("body") or ""), re.MULTILINE)
                target = self.by_job(line.group(1)) if line else 0
                named = re.compile(rf"#{n}\b")
                for it in self.jobs():
                    if not target and not it.get("pull_request") and named.search(str(it.get("title") or "")):
                        target = int(it["number"])
                if not target and job:
                    target = self.by_job(job.group(0))
        elif job:
            target = self.by_job(job.group(0))
        return target or self.board()


def posted_on(api: Api, number: int, key: str) -> bool:
    """Does a team-login comment on #number already carry this key's mark? Only the team login counts: the repo is
    public, so anyone could type a mark to hold a message back; Muse's own text cannot carry one (harmless)."""
    got, _ = api.paged(f"repos/{api.repo}/issues/{number}/comments")
    for c in got:
        if ((c.get("user") or {}).get("login") or "").lower() != TEAM_LOGIN.lower():
            continue
        if key in POST_KEY.findall(c.get("body") or ""):
            return True
    return False


def read_posted(git: Git) -> set[str]:
    return {k for k in git.show(INBOX_REF, POSTED_FILE).split() if k.isdigit()}


def check_poster(api: Api) -> str:
    """Fail closed (11-13): the post token must act as the team login, or nothing is posted. A token missing,
    refused by GET /user, or of another login raises, naming the cause; posts under another login would be read by
    no one (every reader trusts the team login only)."""
    try:
        who = api.poster()
    except ApiError as e:
        raise RuntimeError(f"{POST_TOKEN_ENV} was refused by GET /user (HTTP {e.status}): nothing is posted") from None
    if who.lower() != TEAM_LOGIN.lower():
        raise RuntimeError(f"{POST_TOKEN_ENV} acts as {plain(who)[:40] or 'no login'}, not {TEAM_LOGIN}: "
                           "no reader would see its posts, so none is made")
    return who


def relay(api: Api, git: Git, dry_run: bool = False, log: Callable[[str], None] = print,
          checked: bool = False) -> tuple[list[str], int]:
    """Posts each outbox message not posted yet, oldest first, at most MAX_POSTS. Returns the keys now posted
    (found already posted included, so the state learns them) and the number that failed. `checked`: main ran
    check_poster at start already."""
    done = read_posted(git)
    fresh = [m for m in outbox(git, log) if m.key not in done]
    if not fresh:
        log("RELAY  nothing new on " + OUTBOX_BRANCH)
        return [], 0
    if not dry_run and not checked:
        check_poster(api)
    targets = Targets(api)
    keys, failed = [], 0
    for m in fresh[:MAX_POSTS]:
        try:
            m = load(git, m)
            subject, body = render(m)
            number = targets.of(subject)
            if posted_on(api, number, m.key):
                log(f"RELAY  {m.path} is already on #{number}")
            elif dry_run:
                log(f"DRY    would post {m.path} on #{number}: {subject[:100]}")
                continue
            else:
                api.comment(number, body)
                log(f"RELAY  posted {m.path} on #{number}: {subject[:100]}")
            keys.append(m.key)
        except Exception as e:   # noqa: BLE001  one message's failure never stops the others; it is retried
            failed += 1
            log(f"FAIL   {m.path}: {e}")
    if len(fresh) > MAX_POSTS:
        log(f"RELAY  {len(fresh) - MAX_POSTS} more wait for the next run")
    return keys, failed


# ---------------------------------------------------------------- the team to Muse (inbox)

LINK_MD = re.compile(r"\[([^\]\n]*)\]\((https?://[^)\s]*)\)")
URL = re.compile(r"https?://\S+")
GITHUB_THREAD = re.compile(r"^https://github\.com/[\w.-]+/[\w.-]+/(?:issues|pull)/(\d+)(?:#issuecomment-(\d+))?")
GITHUB_RUN = re.compile(r"^https://github\.com/[\w.-]+/[\w.-]+/actions/runs/(\d+)")


def unlinked(url: str) -> str:
    """A link said in words (mail-to-muse.md 8 and 9: no links): a thread as #N, a run as its number."""
    m = GITHUB_THREAD.match(url)
    if m:
        return f"#{m.group(1)}" + (f" comment {m.group(2)}" if m.group(2) else "")
    m = GITHUB_RUN.match(url)
    return f"run {m.group(1)}" if m else "(link removed)"


def shaped(text: str) -> str:
    """A notice shaped by work/tasks/mail-to-muse.md, as the bridge's mailBody_: hidden markers and backticks out,
    links said in words, no trailing space, no run of blank lines."""
    text = re.sub(r"<!--[\s\S]*?-->", "", str(text))
    text = re.sub(r"<!--[\s\S]*$", "", text).replace("`", "")
    text = LINK_MD.sub(lambda m: m.group(1) if m.group(1).strip() else unlinked(m.group(2)), text)
    def bare(m: re.Match) -> str:   # a sentence's own stop after a link stays
        url = m.group(0).rstrip(".,;:)")
        return unlinked(url) + m.group(0)[len(url):]

    text = URL.sub(bare, text)
    text = re.sub(r"[ \t]+$", "", text, flags=re.MULTILINE)
    return re.sub(r"\n{3,}", "\n\n", text).strip()


def is_notice(c: dict) -> bool:
    """A CLAUDE_TO_MUSE notice: first line starts the marker, by the switchboard or the team login; never the mail
    bridge's record of a hand mail (Muse already got it by mail)."""
    body = (c.get("body") or "").lstrip()
    login = ((c.get("user") or {}).get("login") or "").lower()
    return body.startswith(NOTICE_MARKER) and login in NOTICE_LOGINS and REC_LINE not in body[:600]


def stamp(at: str) -> str:
    t = parse_time(at)
    return t.strftime("%Y%m%dT%H%M%SZ") if t else "00000000T000000Z"


def parse_time(at: str) -> dt.datetime | None:
    try:
        return dt.datetime.fromisoformat(str(at).replace("Z", "+00:00")).astimezone(dt.timezone.utc)
    except ValueError:
        return None


def iso(t: dt.datetime) -> str:
    return t.astimezone(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def notice_file(c: dict) -> tuple[str, str]:
    """(path, text) of one notice in the inbox."""
    try:
        number = int(str(c.get("issue_url") or "").rsplit("/", 1)[1])
    except (ValueError, IndexError):
        number = 0
    cid = int(c["id"])
    body = shaped(c.get("body") or "")
    first, _, rest = body.partition("\n")
    who = ("From Claude's switchboard (the integrator's tooling)" if (c.get("user") or {}).get("login") == BOT_LOGIN
           else "From Claude (integrator)")
    rest = rest.strip()
    if len(rest) > MAX_NOTICE_TEXT:
        rest = rest[:MAX_NOTICE_TEXT] + f"\n\n[cut: the notice is longer; it is comment {cid} on #{number}]"
    text = (f"{plain(first)[:MAX_SUBJECT]}\n{who}, by the git channel: notice {cid} on #{number}.\n\n{rest}\n\n"
            f"Answers and acks go in a message file on {OUTBOX_BRANCH} whose first line starts {MUSE_MARKER} and "
            f"names the job, as work/tasks/muse.md (git channel) says; name this notice by its number, {cid}.\n")
    return f"{INBOX_DIR}{stamp(c.get('created_at') or '')}-{cid}.md", text


def to_muse(c: dict) -> bool:
    """A team post that asks Muse (11-13: such a post never reached her, since she reads no GitHub; asks.py only
    flagged RELAY?): by the team login (never a stranger: the repo is public, and never the switchboard), judged as
    asks.py judges asks (ASK: a line opening `To Muse`, after a signature, or `→ Muse`, `cc Muse`), not by Muse, not
    already to her (CLAUDE_TO_MUSE), not an ack, not noise or Codex."""
    body = c.get("body") or ""
    if ((c.get("user") or {}).get("login") or "").lower() != TEAM_LOGIN.lower():
        return False
    if body.lstrip().startswith(NOTICE_MARKER) or asks.LEDGER_MARK in body or asks.acks_only(body):
        return False
    who = asks.author(body)
    if who in ("Muse", "noise", "Codex"):
        return False
    return "Muse" in {m.group(1) for m in asks.ASK.finditer(body[:4000])} - {who}


def thread_of(c: dict) -> int:
    try:
        return int(str(c.get("issue_url") or "").rsplit("/", 1)[1])
    except (ValueError, IndexError):
        return 0


def relay_body(c: dict) -> str:
    """The switchboard's relay of one team post to Muse: first line CLAUDE_TO_MUSE (the mail bridge's OUT mails it,
    the inbox mirrors it), the post shaped by mail-to-muse.md (no marker, backtick or link) with every line behind a
    bar, so no line of it starts a line of a switchboard comment, and the hidden mark that keeps it to once."""
    n, cid, body = thread_of(c), int(c["id"]), c.get("body") or ""
    who = asks.author(body)
    jobs = sorted(asks.jobs_named(body))
    ref = jobs[0] if jobs else f"#{n}"
    text = harmless(shaped(body))
    if len(text) > MAX_RELAY_TEXT:
        text = text[:MAX_RELAY_TEXT] + f"\n[cut: the post is longer; it is comment {cid} on #{n}]"
    quoted = "\n".join(f"| {line}" if line else "|" for line in text.split("\n"))
    return (f"{NOTICE_MARKER} {ref} To Muse from {who} on #{n}\n\n"
            f"{who} wrote to Muse on #{n} (comment {cid}). The post follows as data, each line behind a bar:\n\n"
            f"{quoted}\n\nAnswer by mail or by a message file naming {ref}, as work/tasks/muse.md says.\n\n"
            + RELAY_MARK.format(id=cid))


def relayed_on(api: Api, number: int, cid: str) -> bool:
    """Does a switchboard comment on #number already relay comment cid (its mark)? Only the switchboard's count."""
    got, _ = api.paged(f"repos/{api.repo}/issues/{number}/comments")
    return any(((x.get("user") or {}).get("login") or "") == BOT_LOGIN and cid in RELAYED.findall(x.get("body") or "")
               for x in got)


def forward(api: Api, targets: Targets, c: dict, dry_run: bool, log: Callable[[str], None]) -> dict | None:
    """Relays one team post to Muse, once: on its issue (a PR's: its job issue, else the board, as the bridge picks:
    the bridge's token cannot clear a PR's label), the comment and then the wake:muse label, the order the switch
    uses (the bridge's J-072 race). Returns the comment posted, None when it was already there (the label is put back,
    which mails nothing twice: the bridge mails a comment once) or in a dry run."""
    cid = str(c["id"])
    number = targets.of(f"#{thread_of(c)}")
    if relayed_on(api, number, cid):
        if not dry_run:
            api.label(number, MUSE_LABEL)
        log(f"RELAY  comment {cid} was already relayed on #{number}")
        return None
    if dry_run:
        log(f"DRY    would relay comment {cid} to Muse on #{number}")
        return None
    got = api.switchboard_comment(number, relay_body(c))
    api.label(number, MUSE_LABEL)
    log(f"RELAY  comment {cid} on #{thread_of(c)} relayed to Muse on #{number}")
    return got


def inbox(api: Api, git: Git, posted: Iterable[str], now: dt.datetime, dry_run: bool = False,
          log: Callable[[str], None] = print) -> str:
    """Relays each new team post to Muse (forward), writes each new notice to the inbox branch and the posted keys to
    its state, in one commit, and pushes it. Returns the commit pushed ("" when nothing changed)."""
    have = {p for _, _, _, p in git.tree(INBOX_REF, INBOX_DIR)}
    ids = {p.rsplit("-", 1)[-1][:-3] for p in have if p.endswith(".md")}
    relayed = [x for x in git.show(INBOX_REF, RELAYED_FILE).split() if x.isdigit()]
    mark = parse_time(git.show(INBOX_REF, SINCE_FILE).strip())
    since = (mark - NOTICE_OVERLAP) if mark else now - NOTICE_FIRST
    got, cut = api.paged(f"repos/{api.repo}/issues/comments?sort=updated&direction=asc&since={iso(since)}")
    writes: dict[str, str] = {}
    newest = mark
    targets = Targets(api)
    failed = 0
    for c in got:
        at = parse_time(c.get("updated_at") or c.get("created_at") or "")
        if len(writes) >= MAX_NOTICES:
            break            # the mark stops before this one, so the next run reads it again
        if is_notice(c) and str(c.get("id")) not in ids:
            path, text = notice_file(c)
            writes[path] = text
            ids.add(str(c.get("id")))
            log(f"INBOX  {path}: {plain(text.split(chr(10), 1)[0])[:100]}")
        elif to_muse(c) and str(c.get("id")) not in relayed:
            try:
                sent = forward(api, targets, c, dry_run, log)
            except Exception as e:   # noqa: BLE001  the mark stops before it, so the next run tries it again
                failed += 1
                log(f"FAIL   relay of comment {c.get('id')}: {e}")
                break
            if sent and sent.get("id") is not None:   # its inbox file now, so the listing never writes it again
                path, text = notice_file(sent)
                writes[path] = text
                ids.add(str(sent["id"]))
                log(f"INBOX  {path}: {plain(text.split(chr(10), 1)[0])[:100]}")
            relayed.append(str(c["id"]))
        if at and (newest is None or at > newest):
            newest = at
    if failed:
        log("INBOX  a relay failed; the next run retries it from the mark")
    keep_relayed = sorted(set(relayed), key=int)[-MAX_RELAYED_KEPT:]
    if keep_relayed and "\n".join(keep_relayed) + "\n" != git.show(INBOX_REF, RELAYED_FILE):
        writes[RELAYED_FILE] = "\n".join(keep_relayed) + "\n"
    if cut:
        log(f"INBOX  the listing was cut at {MAX_PAGES} pages; the next run reads on from {iso(newest or since)}")
    if newest:
        writes[SINCE_FILE] = iso(newest) + "\n"
    # The state keeps the posted keys of the files still on the outbox branch, so it never grows past the branch.
    present = {m.key for m in outbox(git, lambda _line: None)}
    keep = sorted((read_posted(git) | set(posted)) & present, key=int)
    old = git.show(INBOX_REF, POSTED_FILE)
    if "\n".join(keep) + "\n" != old and (keep or old):
        writes[POSTED_FILE] = "\n".join(keep) + "\n"
    floor = stamp(iso(now - INBOX_KEEP))
    removes = [p for p in have if p.startswith(INBOX_DIR) and p[len(INBOX_DIR):][:16] < floor]
    n = len([p for p in writes if p.startswith(INBOX_DIR)])
    if dry_run:
        log(f"DRY    would write {n} notice(s), {len(removes)} removal(s)")
        return ""
    sha = git.commit(INBOX_REF, writes, removes, f"Muse inbox: {n} notice(s), {len(keep)} posted key(s)")
    if sha:
        git.push(sha, INBOX_BRANCH)
        log(f"INBOX  pushed {sha[:12]} to {INBOX_BRANCH}")
    if failed:   # after the push: what did go through is kept, and the run is red
        raise RuntimeError(f"{failed} relay(s) to Muse failed; the next run retries")
    return sha


# ---------------------------------------------------------------- command line

def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    ap.add_argument("part", choices=("relay", "inbox"))
    ap.add_argument("--git-dir", required=True, help="a bare repo for the two branches' data (created if missing)")
    ap.add_argument("--repo", default=os.environ.get("REPO", ""))
    ap.add_argument("--remote", default="", help="default: https://github.com/<repo>.git")
    ap.add_argument("--posted", default="", help="inbox: the keys relay posted this run, comma-separated")
    ap.add_argument("--output", default="", help="relay: append posted=<keys> to this file ($GITHUB_OUTPUT)")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args(argv)
    # The tokens are taken out of the environment, so no git child process inherits them.
    read_token = os.environ.pop("GH_TOKEN", "")
    post_token = os.environ.pop(POST_TOKEN_ENV, "")
    api = Api(a.repo, read_token, post_token if a.part == "relay" else "")
    if a.part == "relay" and not a.dry_run:
        try:   # fail closed, first: no fetch, no read, no post unless the token is the team login's
            print(f"RELAY  {POST_TOKEN_ENV} acts as {check_poster(api)}")
        except RuntimeError as e:
            print(f"RELAY  FAIL: {e}")
            return 1
    git = Git(a.git_dir, a.remote or f"https://github.com/{a.repo}.git")
    heads = git.fetch()
    print(f"FETCH  {OUTBOX_BRANCH} {heads.get(OUTBOX_BRANCH, 'absent')[:12]}, "
          f"{INBOX_BRANCH} {heads.get(INBOX_BRANCH, 'absent')[:12]}")
    if a.part == "relay":
        keys, failed = relay(api, git, a.dry_run, checked=True)
        if a.output:
            with open(a.output, "a", encoding="utf-8") as f:
                f.write(f"posted={','.join(keys)}\n")
        print(f"RELAY  {len(keys)} posted or found, {failed} failed, {api.calls} REST call(s)")
        return 1 if failed else 0
    posted = [k for k in a.posted.split(",") if k.isdigit()]
    inbox(api, git, posted, dt.datetime.now(dt.timezone.utc), a.dry_run)
    print(f"INBOX  {api.calls} REST call(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
