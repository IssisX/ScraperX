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
    `load_etags`/`save_etags` carry the cache between runs (switchboard.yml keeps the file in the Actions cache).
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


class GitHubError(RuntimeError):
    def __init__(self, method: str, path: str, detail: str, status: int = 0):
        super().__init__(f"gh api -X {method} {path} failed: {detail[:300]}")
        self.method, self.path, self.detail, self.status = method, path, detail, status


class RateLimited(GitHubError):
    """GitHub's rate limit answered, RATE_TRIES times: the item waits for the next sweep."""


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
        try:
            with open(path, encoding="utf-8") as f:
                got = json.load(f)
        except (OSError, ValueError):
            return
        if not isinstance(got, dict):
            return
        if got.get("v") == 3:   # {"v": 3, "mac": ..., "payload": "<the v2 file as text>"}
            payload = got.get("payload") if isinstance(got.get("payload"), str) else ""
            if self.cache_key and not hmac.compare_digest(self._mac(payload), str(got.get("mac") or "")):
                self.log("::warning::ETAG CACHE  the cache file's MAC does not verify: read as a cold cache")
                return
            try:
                got = json.loads(payload)
            except ValueError:
                return
            if not isinstance(got, dict):
                return
        elif self.cache_key:   # an unsigned file is never read by a run that holds the key
            self.log("NOTE        the ETag cache file is not signed: read as a cold cache")
            return
        if got.get("v") == 2:   # {"v": 2, "etags": {...}, "state": {...}}: the switch's sweep state rides along
            self.state = got.get("state") if isinstance(got.get("state"), dict) else {}
            got = got.get("etags") if isinstance(got.get("etags"), dict) else {}
        self.etags = {k: v for k, v in got.items()
                      if isinstance(v, dict) and isinstance(v.get("etag"), str) and isinstance(v.get("body"), str)
                      and isinstance(v.get("t", 0), (int, float))}

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
        cacheable = method == "GET" and not binary and not token   # another token's answers are never cached
        cached = self.etags.get(key) if cacheable and not self.fresh_only else None
        if cached:
            args += ["-H", f"If-None-Match: {cached['etag']}"]
        if body is not None:
            args += ["--input", "-"]
        data = json.dumps(body).encode("utf-8") if body is not None else None
        attempts = GET_ATTEMPTS if method == "GET" else 1
        rate_tries, attempt, status, detail = 0, 0, 0, ""
        while True:
            attempt += 1
            headers: dict[str, str] = {}
            status = 0
            self.calls += 1
            try:
                out = (self.runner or subprocess.run)(args, input=data, capture_output=True, timeout=TIMEOUT_S, env=env)
            except subprocess.TimeoutExpired:
                detail = f"timed out after {TIMEOUT_S}s"
            else:
                if binary:
                    if out.returncode == 0:
                        return out.stdout
                    detail = out.stderr.decode("utf-8", "replace").strip()
                else:
                    status, headers, text = split_response(out.stdout or b"")
                    if not token:   # another token's call (the merge token) has its own limit: not this run's
                        self._note_rate(headers)
                    detail = out.stderr.decode("utf-8", "replace").strip()
                    if status == 304 and cached:
                        self.not_modified += 1
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
        raise GitHubError(method, path, detail, status)

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

    # writes: printed, never sent, in a dry run
    def write(self, method: str, path: str, body: Any = None, what: str = "", token: str = "") -> Any:
        if self.dry_run:
            self.log(f"DRY  {method} {path}  {what}".rstrip())
            return None
        return self._run(method, path, body, token=token)

    def comment(self, number: int, body: str, what: str = "") -> Any:
        return self.write("POST", f"repos/{self.repo}/issues/{number}/comments", {"body": body},
                          what or f"comment on #{number}: {body.splitlines()[0][:120]}")
