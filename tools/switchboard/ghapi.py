"""GitHub through `gh api`, as tools/board/sweep.py reads it, with writes that a dry run only prints.

The token is whatever `gh` holds (GH_TOKEN in Actions: the workflow token). A write may name another
token for that one call (the merge token, J-056): it goes to `gh` in its environment only, never in the
arguments, a log line or an error. Arguments go to `gh` as a list and bodies as JSON on stdin, so no issue
or comment text ever reaches a shell.

Rate (J-072: run 42667757904 failed on "API rate limit exceeded for installation", the 10-minute clock plus the
event sweeps at about 420-480 calls each being over 5,000 an hour):
  - Every JSON call is made with `gh api -i`, so its status and headers are read. `remaining` and `reset` hold
    the newest x-ratelimit-remaining and x-ratelimit-reset seen.
  - A GET whose answer carried an ETag is kept in `etags` (path -> etag and body), and the next read of that
    path sends If-None-Match: a 304 is answered from the cache and does not count against the primary limit.
    `load_etags`/`save_etags` carry the cache between runs: switchboard.yml keeps the file in the Actions cache, but
    only its secret-free cache-in and cache-out jobs touch that cache; the switch gets the one file as this run's
    artifact in $RUNNER_TEMP and reads it only once its MAC checks (J-072, the security pass on #360).
  - A rate-limit answer (429, or 403 with x-ratelimit-remaining 0 or GitHub's "rate limit" words) is tried at
    most RATE_TRIES times with full-jitter backoff, then raises RateLimited, which the switch reads as "leave it
    for the next sweep", never as an error.
"""

from __future__ import annotations

import contextlib
import hashlib
import hmac
import json
import os
import random
import re
import subprocess
import time
from typing import Any

TIMEOUT_S = 60
MAX_PAGES = 5          # at most 500 items read from any one listing
# `wake:muse` tells the Muse bridge to mail an issue's CLAUDE_TO_MUSE comments. The bridge's token is Issues-only,
# so on a pull request the label is never mailed and never cleared (J-072: it sat on #337, #343 and #372 while
# the notices under it reached no one). Every writer posts on an issue by construction; this is the backstop.
ISSUE_ONLY_LABELS = frozenset({"wake:muse"})
_LABELS_PATH = re.compile(r"repos/[^/]+/[^/]+/issues/(\d+)/labels")
GET_ATTEMPTS = 3       # GETs are idempotent: bounded retry with full jitter on transport failure
RATE_TRIES = 2         # a rate-limited call is tried this often in all, then left for the next sweep
RATE_BACKOFF_S = 8.0   # the full-jitter ceiling of a rate-limit retry; a Retry-After up to RATE_WAIT_MAX_S wins
RATE_WAIT_MAX_S = 30.0
CACHE_MAX_BODY = 1_000_000   # bytes of one answer kept for its ETag; larger ones are read in full each time
CACHE_MAX_ENTRIES = 8000
# bytes of bodies saved in all. J-072: at 20 MB a warm full pass found the cap full, so 51 of its prior-PR reads had
# lost their entries and came back counted 200s; actions/cache compresses the file about 7 to 1 (20.9 MB to 2.9 MB).
CACHE_MAX_TOTAL = 80_000_000
CACHE_MAX_AGE_S = 7 * 86400    # an entry no run has read for this long is dropped
STATUS = re.compile(r"^HTTP/\S+ (\d{3})")
ESCAPES_FLAG = "--allow-escape-sequences"
_STAMP = re.compile(r"(?:since|created)=(?:%3E%3D|>=)?\d{4}-\d\d-\d\dT\d\d(?::|%3A)(\d\d)(?::|%3A)(\d\d)Z")


def once_only(path: str) -> bool:
    """A read whose path is never asked again, so its answer is never kept for an ETag (11-13): a listing from a time
    that is not on the hour (sweep_delta's "since the last sweep" reads: a new path every sweep) or read newest first
    (its pages shift with each new item). Kept, they filled the file to its cap every hour (a 500 KB comment page a
    sweep, 25 newest-first pages a pass) and pushed out the entries the hourly full pass reads again, so a live full
    pass found 128 of its 1,005 reads in the cache, where a dry run on a cache that kept them found 814 of 1,038."""
    query = path.partition("?")[2]
    if "direction=desc" in query:
        return True
    return any(m.group(1) != "00" or m.group(2) != "00" for m in _STAMP.finditer(query))


class GitHubError(RuntimeError):
    def __init__(self, method: str, path: str, detail: str, status: int = 0):
        super().__init__(f"gh api -X {method} {path} failed: {detail[:300]}")
        self.method, self.path, self.detail, self.status = method, path, detail, status


class LabelRefused(GitHubError):
    """A label that only works on an issue was about to go on a pull request (ISSUE_ONLY_LABELS)."""


class RateLimited(GitHubError):
    """GitHub's rate limit answered, RATE_TRIES times: the item waits for the next sweep."""


class OverBudget(RateLimited):
    """This run's own call budget (GitHub.budget) is spent (11-13): the read waits for the next sweep, exactly as a
    rate-limited one does, so every part degrades the same way and nothing fails."""


class TimedOut(GitHubError):
    """A call that failed after at least one of its attempts hit TIMEOUT_S: a GitHub that hangs, not one that answered
    (the switch bounds how many of these one sweep pays for, switch.py red_tests)."""


def split_response(raw: bytes) -> tuple[int, dict[str, str], str]:
    """`gh api -i` output: (status, lower-cased headers, body text). Status 0 when there is no status line."""
    text = raw.decode("utf-8", "replace")
    head, sep, body = text.partition("\r\n\r\n")
    if not sep:
        head, sep, body = text.partition("\n\n")
    if not sep or not STATUS.match(head):
        return 0, {}, text
    lines = head.replace("\r\n", "\n").split("\n")
    status = int(STATUS.match(lines[0]).group(1))
    headers: dict[str, str] = {}
    for line in lines[1:]:
        k, _, v = line.partition(":")
        headers[k.strip().lower()] = v.strip()
    return status, headers, body


def rate_limited(status: int, headers: dict[str, str], detail: str) -> bool:
    if status == 429:
        return True
    if status == 403 or "HTTP 403" in detail:
        return headers.get("x-ratelimit-remaining") == "0" or "rate limit" in detail.lower()
    return "rate limit exceeded" in detail.lower()


_NUM = re.compile(r"/\d+(?=/|$)")
_SHA = re.compile(r"/[0-9a-f]{7,40}(?=/|$)")
_REPO = re.compile(r"^/?repos/[^/]+/[^/]+")


def call_kind(method: str, path: str) -> str:
    """One call's place in the GITHUB line's breakdown: its method and path with the repo, numbers, shas and query
    values taken out ("GET pulls/N/files"), so a creep shows which read grew."""
    base, _, query = path.partition("?")
    base = _SHA.sub("/S", _NUM.sub("/N", _REPO.sub("", base))).lstrip("/")
    keys = sorted({k.split("=")[0] for k in query.split("&") if k} - {"per_page", "page"})
    return f"{method} {base}" + (f"?{','.join(keys)}" if keys else "")


class GitHub:
    def __init__(self, repo: str, dry_run: bool, log=print, runner=None, sleep=None):
        self.repo, self.dry_run, self.log = repo, dry_run, log
        self.runner, self.sleep = runner, sleep   # None: subprocess.run and time.sleep, looked up at each call
        self.remaining: int | None = None   # x-ratelimit-remaining of the newest answer
        self.reset: int | None = None       # x-ratelimit-reset (Unix time) of the newest answer
        self.limit: int | None = None       # x-ratelimit-limit: 1,000 an hour for GITHUB_TOKEN, 5,000+ for an App
        self.etags: dict[str, dict] = {}    # path (and Accept) -> {"etag", "body"}
        self.used: set[str] = set()         # cache keys read or written this run (saved first)
        self.state: dict = {}               # the switch's sweep state (switch.py run_sweep), saved with the cache
        self.calls = self.not_modified = 0
        self.tally: dict[str, int] = {}     # call_kind -> counted calls (304s not counted): the GITHUB line's breakdown
        # 11-13: the most counted calls (a 304 is free) this run may make; switch.py run_sweep sets it per pass. Past it
        # every GET raises OverBudget at once, costing nothing; a write still goes (each is a decision already made).
        self.budget: int | None = None
        self.over_budget = 0                # the GETs refused for the budget
        self.fresh_only = False             # fresh(): no answer comes from the cache (the merge facts, J-072)
        self.cache_key = b""                # the cache file's HMAC key (load_etags/save_etags), b"" for none

    @contextlib.contextmanager
    def fresh(self):
        """Every read inside is GitHub's own answer: no If-None-Match, so no body comes from the cache file (J-072,
        the security pass on #360: a cache entry any trunk job can save must never decide a merge)."""
        was, self.fresh_only = self.fresh_only, True
        try:
            yield self
        finally:
            self.fresh_only = was

    # the ETag cache carried between runs
    def set_cache_key(self, secret: str) -> None:
        """Sign the cache file with a key derived from `secret` (the review key, which only the switchboard-merge
        environment holds): with a key set, a file whose MAC does not verify is read as a cold cache."""
        self.cache_key = hmac.new(secret.encode("utf-8"), b"switchboard etag cache v3", hashlib.sha256).digest() \
            if secret else b""

    def _mac(self, payload: str) -> str:
        return hmac.new(self.cache_key, payload.encode("utf-8"), hashlib.sha256).hexdigest()

    def load_etags(self, path: str) -> None:
        """Read the cache file, or read a cold cache: this never raises (J-072, the security pass on #360: a non-ASCII
        mac made compare_digest raise TypeError, and a lone surrogate in the payload made _mac raise, both before
        main's try, so the bad file stayed on disk and was saved again as the newest entry). A file that is read as
        cold for any reason but its absence is removed at once, so even a run that dies before save_etags hands
        nothing on (switchboard.yml uploads the file only if it exists)."""
        if not os.path.exists(path):
            return
        why = self._read_etags(path)
        if why:
            self.etags, self.state = {}, {}
            self.log(f"::warning::ETAG CACHE  the cache file {why}: read as a cold cache" if self.cache_key
                     else f"NOTE        the ETag cache file {why}: read as a cold cache")
            with contextlib.suppress(OSError):
                os.remove(path)

    def _read_etags(self, path: str) -> str:
        """Load the file into etags and state; "" when read, else why it was not."""
        try:
            with open(path, encoding="utf-8") as f:
                got = json.load(f)
        except (OSError, ValueError) as err:   # UnicodeDecodeError is a ValueError
            return f"could not be read ({type(err).__name__})"
        if not isinstance(got, dict):
            return "is not a JSON object"
        if got.get("v") == 3:   # {"v": 3, "mac": ..., "payload": "<the v2 file as text>"}
            payload, mac = got.get("payload"), got.get("mac")
            if not isinstance(payload, str) or not isinstance(mac, str):
                return "has no payload or mac"
            if self.cache_key:
                try:   # as bytes: compare_digest raises on a str with a non-ASCII character
                    want, seen = self._mac(payload).encode("ascii"), mac.encode("ascii")
                except UnicodeError:   # a lone surrogate in the payload, a non-ASCII mac
                    return "has a payload or mac that is not plain text"
                if not hmac.compare_digest(want, seen):
                    return "has a MAC that does not verify"
            try:
                got = json.loads(payload)
            except ValueError:
                return "has a payload that is not JSON"
            if not isinstance(got, dict):
                return "has a payload that is not a JSON object"
        elif self.cache_key:   # an unsigned file is never read by a run that holds the key
            return "is not signed"
        if got.get("v") == 2:   # {"v": 2, "etags": {...}, "state": {...}}: the switch's sweep state rides along
            self.state = got.get("state") if isinstance(got.get("state"), dict) else {}
            got = got.get("etags") if isinstance(got.get("etags"), dict) else {}
        self.etags = {k: v for k, v in got.items()
                      if isinstance(v, dict) and isinstance(v.get("etag"), str) and isinstance(v.get("body"), str)
                      and isinstance(v.get("t", 0), (int, float))}
        return ""

    def save_etags(self, path: str, now: float | None = None) -> None:
        """Every entry, the most recently used first, within the caps (J-072, Codex on #360: a fast sweep reads no
        unchanged PR, so keeping only this run's reads dropped every PR's entries, and the next read of a PR that
        changed was a counted 200 instead of a free 304). An entry carries `t`, the last run that used it."""
        now = time.time() if now is None else now
        for k in self.used:
            if k in self.etags:
                self.etags[k]["t"] = now
        keep: dict[str, dict] = {}
        total = 0
        order = sorted(self.etags, key=lambda k: (k not in self.used, -float(self.etags[k].get("t") or 0), k))
        for k in order:
            entry = self.etags[k]
            if k not in self.used and now - float(entry.get("t") or 0) > CACHE_MAX_AGE_S:
                continue
            if len(keep) >= CACHE_MAX_ENTRIES or total + len(entry["body"]) > CACHE_MAX_TOTAL:
                continue
            keep[k] = entry
            total += len(entry["body"])
        payload = json.dumps({"v": 2, "etags": keep, "state": self.state})
        try:
            # a cold start has no folder yet: cache-in handed nothing on, so $RUNNER_TEMP/etags was never made
            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            with open(path, "w", encoding="utf-8") as f:
                if self.cache_key:
                    json.dump({"v": 3, "mac": self._mac(payload), "payload": payload}, f)
                else:
                    f.write(payload)
        except OSError as err:
            self.log(f"NOTE        the ETag cache was not saved: {err}")

    def _note_rate(self, headers: dict[str, str]) -> None:
        rem, reset, limit = (headers.get(f"x-ratelimit-{k}", "") for k in ("remaining", "reset", "limit"))
        if rem.isdigit():
            self.remaining = int(rem)
        if reset.isdigit():
            self.reset = int(reset)
        if limit.isdigit():
            self.limit = int(limit)

    def _backoff(self, attempt: int, headers: dict[str, str]) -> None:
        after = headers.get("retry-after", "")
        sleep = self.sleep or time.sleep
        if after.isdigit() and int(after) <= RATE_WAIT_MAX_S:
            sleep(float(after))
        else:
            sleep(random.uniform(0, min(RATE_BACKOFF_S, 2.0 ** attempt)))   # full jitter

    def _run(self, method: str, path: str, body: Any = None, accept: str = "", binary: bool = False, token: str = ""):
        args = ["gh", "api", "-X", method, path]
        if not binary:
            args.insert(2, "-i")   # the status and headers: the rate, the ETag, a 304
        env = dict(os.environ, GH_TOKEN=token) if token else None   # gh reads GH_TOKEN before anything else
        if accept:
            args += ["-H", f"Accept: {accept}"]
        key = f"{path}|{accept}"
        # another token's answers are never cached, nor a read made once (once_only)
        cacheable = method == "GET" and not binary and not token and not once_only(path)
        cached = self.etags.get(key) if cacheable and not self.fresh_only else None
        if cached:
            args += ["-H", f"If-None-Match: {cached['etag']}"]
        if body is not None:
            args += ["--input", "-"]
        data = json.dumps(body).encode("utf-8") if body is not None else None
        attempts = GET_ATTEMPTS if method == "GET" else 1
        rate_tries, attempt, status, detail, timed_out = 0, 0, 0, "", False
        kind = call_kind(method, path)
        if method == "GET" and self.budget is not None and self.spent() >= self.budget:
            self.over_budget += 1
            if self.over_budget == 1:
                self.log(f"::warning::BUDGET      this run's {self.budget} counted GitHub calls are spent at {method} {path}: "
                         "every further read waits for the next sweep")
            raise OverBudget(method, path, f"this run's budget of {self.budget} calls is spent", 0)
        escapes = False
        while True:
            attempt += 1
            headers: dict[str, str] = {}
            status = 0
            self.calls += 1
            self.tally[kind] = self.tally.get(kind, 0) + 1
            try:
                out = (self.runner or subprocess.run)(args, input=data, capture_output=True, timeout=TIMEOUT_S, env=env)
            except subprocess.TimeoutExpired:
                detail = f"timed out after {TIMEOUT_S}s"
                timed_out = True
            else:
                if binary:
                    if out.returncode == 0:
                        return out.stdout
                    detail = out.stderr.decode("utf-8", "replace").strip() or f"exit {out.returncode}, no stderr"
                    if ESCAPES_FLAG in detail and not escapes:
                        # 11-13: a log with colour codes. The bytes go to the caller, never a terminal (red_tests keeps
                        # printable text only, log_text), so ask once more with the flag gh names; any other failure
                        # of this read is not retried (it failed the same way 3 times a read, every sweep).
                        escapes = True
                        args.insert(args.index("-X"), ESCAPES_FLAG)
                        attempt -= 1
                        continue
                    if escapes:
                        break
                else:
                    status, headers, text = split_response(out.stdout or b"")
                    if not token:   # another token's call (the merge token) has its own limit: not this run's
                        self._note_rate(headers)
                    detail = out.stderr.decode("utf-8", "replace").strip()
                    if status == 304 and cached:
                        self.not_modified += 1
                        self.tally[kind] -= 1
                        self.used.add(key)
                        text = cached["body"]
                        return json.loads(text) if text.strip() else None
                    if out.returncode == 0:
                        if cacheable and headers.get("etag") and len(text) <= CACHE_MAX_BODY:
                            self.etags[key] = {"etag": headers["etag"], "body": text}
                            self.used.add(key)
                        return json.loads(text) if text.strip() else None
                    if not detail:
                        detail = f"HTTP {status}"
                if rate_limited(status, headers, detail):
                    rate_tries += 1
                    if rate_tries >= RATE_TRIES:
                        detail = detail.replace(token, "[token]") if token else detail
                        self.log(f"::warning::RATE LIMIT  {method} {path}: GitHub's rate limit answered "
                                 f"{RATE_TRIES} times; left for the next sweep (the call cut was not enough)")
                        raise RateLimited(method, path, detail, status)
                    self.log(f"::warning::RATE LIMIT  {method} {path}: HTTP {status or '?'}, backing off once")
                    self._backoff(rate_tries, headers)
                    continue
                if "HTTP 4" in detail or 400 <= status < 500:  # a client error will not get better on retry
                    break
            if attempt >= attempts:
                break
            time.sleep(random.uniform(0, 2 ** attempt))
        if token:
            detail = detail.replace(token, "[token]")
        raise (TimedOut if timed_out else GitHubError)(method, path, detail, status)

    def spent(self) -> int:
        """The calls this run made that count against the hour's limit (a 304 does not)."""
        return self.calls - self.not_modified

    # reads
    def get(self, path: str, accept: str = "") -> Any:
        return self._run("GET", path, accept=accept)

    def get_text(self, path: str, accept: str) -> str:
        return self._run("GET", path, accept=accept, binary=True).decode("utf-8", "replace")

    def get_bytes(self, path: str) -> bytes:
        return self._run("GET", path, binary=True)

    def paged(self, path: str, key: str = "") -> list:
        sep = "&" if "?" in path else "?"
        items: list = []
        for page in range(1, MAX_PAGES + 1):
            got = self.get(f"{path}{sep}per_page=100&page={page}")
            batch = got[key] if key else got
            items.extend(batch)
            if len(batch) < 100:
                break
        return items

    def is_pull(self, number: int) -> bool:
        """Is #number a pull request? GitHub's issue record carries `pull_request` for one."""
        return "pull_request" in (self.get(f"repos/{self.repo}/issues/{number}") or {})

    def _issue_only(self, method: str, path: str, body: Any) -> None:
        """The one place every label write passes: an ISSUE_ONLY_LABELS label never goes on a pull request."""
        m = _LABELS_PATH.fullmatch(path)
        if method != "POST" or not m or not isinstance(body, dict):
            return
        stuck = sorted(ISSUE_ONLY_LABELS.intersection(body.get("labels") or ()))
        if stuck and self.is_pull(int(m.group(1))):
            raise LabelRefused(method, path, f"#{m.group(1)} is a pull request: `{stuck[0]}` goes on an issue only "
                                             "(the Muse bridge's token is Issues-only: it never mails from a PR "
                                             "and cannot take the label off one)")

    # writes: printed, never sent, in a dry run
    def write(self, method: str, path: str, body: Any = None, what: str = "", token: str = "") -> Any:
        self._issue_only(method, path, body)
        if self.dry_run:
            self.log(f"DRY  {method} {path}  {what}".rstrip())
            return None
        return self._run(method, path, body, token=token)

    def comment(self, number: int, body: str, what: str = "") -> Any:
        return self.write("POST", f"repos/{self.repo}/issues/{number}/comments", {"body": body},
                          what or f"comment on #{number}: {body.splitlines()[0][:120]}")
