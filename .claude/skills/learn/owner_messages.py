#!/usr/bin/env python3
"""The owner's own words from a Claude Code session record, in order.

Usage: python3 -I owner_messages.py [--since ISO_TIMESTAMP] [--session PATH] > messages.txt

Reads the session's JSONL record (the newest one for this project when --session is not given)
and prints only what the human typed: plain user turns and the messages sent mid-turn
(queued commands from a human). Tool results, system reminders, hook feedback, task and agent
notifications, compaction summaries and local-command echoes are left out.
"""
import argparse
import json
import os
import re
import sys

SKIP_PREFIXES = (
    "<task-notification>", "<agent-message", "This session is being continued from a previous conversation",
    "Stop hook feedback", "<local-command-caveat>", "<command-name>", "<local-command-stdout>",
    "[SYSTEM NOTIFICATION", "Goal check-in", "<webhook-payload>", "Another Claude session sent a message",
    "<cross-session-message", "Caveat:",
)
REMINDER = re.compile(r"<system-reminder>.*?</system-reminder>", re.S)
TAGGED = re.compile(r"<(local-command-[a-z]+|command-[a-z]+)>.*?</\1>", re.S)
MARKERS = re.compile(r"<artifact-content-authored-by-others/>")


def default_session() -> str:
    slug = os.getcwd().replace("/", "-")
    folder = os.path.join(os.path.expanduser("~"), ".claude", "projects", slug)
    records = [os.path.join(folder, name) for name in os.listdir(folder) if name.endswith(".jsonl")]
    if not records:
        sys.exit(f"owner_messages: no session record in {folder}")
    return max(records, key=os.path.getmtime)


def clean(text: str) -> str:
    text = MARKERS.sub("", TAGGED.sub("", REMINDER.sub("", text))).strip()
    if not text or text.startswith(SKIP_PREFIXES) or "This session is being continued from a previous" in text[:600]:
        return ""
    return text


def text_of(content) -> str:
    """The typed text of a message's content; a picture is noted, never dumped."""
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for block in content:
            if isinstance(block, dict) and block.get("type") == "text":
                parts.append(block.get("text", ""))
            elif isinstance(block, dict) and block.get("type") == "image":
                parts.append("[a picture]")
        return "\n".join(parts)
    return ""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--since", default="", help="only messages after this ISO timestamp")
    parser.add_argument("--session", default="", help="the session record (default: this project's newest)")
    args = parser.parse_args()
    path = args.session or default_session()
    seen = set()
    count = 0
    with open(path, encoding="utf-8", errors="replace") as record:
        for line in record:
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            stamp = str(entry.get("timestamp") or "")
            text = ""
            if entry.get("type") == "user" and not entry.get("isMeta") and not entry.get("isSidechain"):
                text = text_of((entry.get("message") or {}).get("content"))
            elif entry.get("type") == "attachment":
                attachment = entry.get("attachment") or {}
                if attachment.get("type") == "queued_command" and \
                        (attachment.get("origin") or {}).get("kind") == "human":
                    text = text_of(attachment.get("prompt"))
                    stamp = str(attachment.get("timestamp") or stamp)
            text = clean(text)
            if not text or (args.since and stamp and stamp <= args.since):
                continue
            key = text[:400]
            if key in seen:
                continue
            seen.add(key)
            count += 1
            print(f"[{stamp}]\n{text}\n")
    print(f"# {count} owner messages from {path}", file=sys.stderr)


if __name__ == "__main__":
    main()
