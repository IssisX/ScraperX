"""GitHub through `gh api`, as tools/board/sweep.py reads it, with writes that a dry run only prints.

The token is whatever `gh` holds (GH_TOKEN in Actions: the workflow token). Arguments go to `gh` as
a list and bodies as JSON on stdin, so no issue or comment text ever reaches a shell.
"""

from __future__ import annotations

import json
import random
import subprocess
import time
from typing import Any

TIMEOUT_S = 60
MAX_PAGES = 5          # at most 500 items read from any one listing
GET_ATTEMPTS = 3       # GETs are idempotent: bounded retry with full jitter on transport failure


class GitHubError(RuntimeError):
    def __init__(self, method: str, path: str, detail: str):
        super().__init__(f"gh api -X {method} {path} failed: {detail[:300]}")
        self.method, self.path, self.detail = method, path, detail


class GitHub:
    def __init__(self, repo: str, dry_run: bool, log=print):
        self.repo, self.dry_run, self.log = repo, dry_run, log

    def _run(self, method: str, path: str, body: Any = None, accept: str = "", binary: bool = False):
        args = ["gh", "api", "-X", method, path]
        if accept:
            args += ["-H", f"Accept: {accept}"]
        if body is not None:
            args += ["--input", "-"]
        data = json.dumps(body).encode("utf-8") if body is not None else None
        attempts = GET_ATTEMPTS if method == "GET" else 1
        for attempt in range(1, attempts + 1):
            try:
                out = subprocess.run(args, input=data, capture_output=True, timeout=TIMEOUT_S)
            except subprocess.TimeoutExpired:
                detail = f"timed out after {TIMEOUT_S}s"
            else:
                if out.returncode == 0:
                    if binary:
                        return out.stdout
                    text = out.stdout.decode("utf-8")
                    return json.loads(text) if text.strip() else None
                detail = out.stderr.decode("utf-8", "replace").strip()
                if "HTTP 4" in detail:  # a client error will not get better on retry
                    break
            if attempt < attempts:
                time.sleep(random.uniform(0, 2 ** attempt))
        raise GitHubError(method, path, detail)

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
    def write(self, method: str, path: str, body: Any = None, what: str = "") -> Any:
        if self.dry_run:
            self.log(f"DRY  {method} {path}  {what}".rstrip())
            return None
        return self._run(method, path, body)

    def comment(self, number: int, body: str, what: str = "") -> Any:
        return self.write("POST", f"repos/{self.repo}/issues/{number}/comments", {"body": body},
                          what or f"comment on #{number}: {body.splitlines()[0][:120]}")
