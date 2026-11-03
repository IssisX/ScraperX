#!/usr/bin/env python3
"""Render work/BOARD.md from the open GitHub issues labeled `job` (design v3, step A).

Read-only view for the owner; the issues and their labels are the state.
Usage: python3 tools/board/gen_board.py > work/BOARD.md   (needs `gh` authenticated)
"""
import json
import subprocess
import sys

REPO = "IssisX/ScraperX"


def gh(path):
    out = subprocess.run(["gh", "api", path], check=True, capture_output=True, text=True).stdout
    return json.loads(out)


def main():
    issues = gh(f"repos/{REPO}/issues?labels=job&state=open&per_page=100")
    rows = []
    for it in issues:
        if "pull_request" in it:
            continue
        labels = [l["name"] for l in it["labels"]]
        pick = lambda p: ",".join(sorted(l[len(p):] for l in labels if l.startswith(p))) or "-"
        rows.append((it["title"], it["number"], pick("owner:"), pick("state:"), pick("needs:"),
                     "hard" if "hard" in labels else ""))
    rows.sort()
    print("# Board (generated: do not edit)\n")
    print("From the open issues labeled `job`; regenerate with `python3 tools/board/gen_board.py`.\n")
    print("| Job | Issue | Owner | State | Needs | |")
    print("|---|---|---|---|---|---|")
    for title, num, owner, state, needs, hard in rows:
        print(f"| {title} | #{num} | {owner} | {state} | {needs} | {hard} |")


if __name__ == "__main__":
    sys.exit(main())
