#!/usr/bin/env python3
"""The owner's own words from a Claude Code session record, in order, since this session's last /learn.

Usage, from the repository root:
  python3 -I .claude/skills/learn/owner_messages.py           what is new since this session's last /learn
  python3 -I .claude/skills/learn/owner_messages.py --all     the whole session
  python3 -I .claude/skills/learn/owner_messages.py --since ISO_TIMESTAMP
  python3 -I .claude/skills/learn/owner_messages.py --mark ISO_TIMESTAMP
      records that this session is learned through that moment (in learned/<session id>)
  --session PATH reads that record instead of this session's.

It works in any session. The record is this session's own: the one CLAUDE_CODE_SESSION_ID names,
else the newest record made in this folder. What has been learned is kept per session, one small
file each, so sessions running side by side, and their branches, never overwrite each other.

It prints only what the human typed: plain turns, messages sent mid-turn, and slash commands with
their text ("/goal ..."). Tool output, reminders, hook feedback, task and agent notifications,
compaction summaries, skills Claude loaded and local-command output are left out; a picture shows
as "[a picture]". A summary goes to stderr. Exit 2: no record for this session; exit 3: a record
with no owner message in it (its format may have changed). Either way, study the conversation
directly.
"""
import argparse
import json
import os
import re
import sys
from datetime import datetime

HERE = os.path.dirname(os.path.abspath(__file__))
LEARNED = os.path.join(HERE, "learned")
SKIP_PREFIXES = (
    "<task-notification>", "<agent-message", "This session is being continued from a previous conversation",
    "Stop hook feedback", "<local-command-caveat>", "<local-command-stdout>",
    "[SYSTEM NOTIFICATION", "Goal check-in", "<webhook-payload>", "Another Claude session sent a message",
    "<cross-session-message", "Caveat:",
)
REMINDER = re.compile(r"<system-reminder>.*?</system-reminder>", re.S)
COMMAND = re.compile(r"<command-name>(/[^<]*)</command-name>.*?<command-args>(.*?)</command-args>", re.S)
TAGGED = re.compile(r"<(local-command-[a-z]+|command-[a-z]+)>.*?</\1>", re.S)
MARKERS = re.compile(r"<artifact-content-authored-by-others/>")


def projects_dir() -> str:
    config = os.environ.get("CLAUDE_CONFIG_DIR") or os.path.join(os.path.expanduser("~"), ".claude")
    return os.path.join(config, "projects")


def record_cwd(path: str) -> str:
    """The folder a record was made in, from its first entries that say."""
    with open(path, encoding="utf-8", errors="replace") as record:
        for _, line in zip(range(200), record):
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            if isinstance(entry, dict) and entry.get("cwd"):
                return str(entry["cwd"])
    return ""


def find_record() -> str:
    """This session's record: the one its id names, else the newest made in this folder."""
    projects = projects_dir()
    if not os.path.isdir(projects):
        return ""
    folders = [os.path.join(projects, name) for name in os.listdir(projects)]
    folders = [folder for folder in folders if os.path.isdir(folder)]
    session = os.environ.get("CLAUDE_CODE_SESSION_ID", "")
    if session:
        for folder in folders:
            path = os.path.join(folder, session + ".jsonl")
            if os.path.isfile(path):
                return path
    here = os.path.normcase(os.path.realpath(os.getcwd()))
    records = [os.path.join(folder, name) for folder in folders for name in os.listdir(folder)
               if name.endswith(".jsonl")]
    for path in sorted(filter(os.path.isfile, records), key=os.path.getmtime, reverse=True):
        cwd = record_cwd(path)
        if cwd and os.path.normcase(os.path.realpath(cwd)) == here:
            return path
    return ""


def central(stamp: str) -> str:
    """A record's timestamp as the owner reads it: US Central (UTC, said so, where no zone data)."""
    try:
        moment = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    except ValueError:
        return stamp
    try:
        from zoneinfo import ZoneInfo
        moment, zone = moment.astimezone(ZoneInfo("America/Chicago")), "Central"
    except (ImportError, KeyError):  # KeyError covers ZoneInfoNotFoundError: no zone data on this machine
        zone = "UTC"
    hour = moment.hour % 12 or 12
    return f"{moment:%b} {moment.day}, {hour}:{moment:%M} {'am' if moment.hour < 12 else 'pm'} {zone}"


def learned_through(session: str) -> str:
    try:
        with open(os.path.join(LEARNED, session), encoding="utf-8") as mark:
            return mark.read().strip()
    except FileNotFoundError:
        return ""


def mark(session: str, stamp: str) -> None:
    try:
        datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    except ValueError:
        print(f"owner_messages: --mark wants an ISO timestamp such as 2026-10-20T23:02:07.218Z, not {stamp!r}",
              file=sys.stderr)
        sys.exit(2)
    os.makedirs(LEARNED, exist_ok=True)
    with open(os.path.join(LEARNED, session), "w", encoding="utf-8") as out:
        out.write(stamp + "\n")
    print(f"# session {session} learned through {stamp} ({central(stamp)})", file=sys.stderr)


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


def clean(text: str) -> str:
    text = REMINDER.sub("", text)
    text = COMMAND.sub(lambda match: f"{match.group(1)} {match.group(2).strip()}" if match.group(2).strip() else "",
                       text)
    text = MARKERS.sub("", TAGGED.sub("", text)).strip()
    if not text or text.startswith(SKIP_PREFIXES) or "This session is being continued from a previous" in text[:600]:
        return ""
    return text


def main() -> None:
    parser = argparse.ArgumentParser(description="The owner's own words from this session's record.")
    parser.add_argument("--since", default="", help="only messages after this ISO timestamp")
    parser.add_argument("--all", action="store_true", help="the whole session, whatever was learned before")
    parser.add_argument("--session", default="", help="a record to read instead of this session's")
    parser.add_argument("--mark", default="", metavar="ISO_TIMESTAMP",
                        help="record that this session is learned through that moment")
    args = parser.parse_args()
    path = args.session or find_record()
    if not path or not os.path.isfile(path):
        print(f"owner_messages: no record for this session under {projects_dir()}; study the conversation directly",
              file=sys.stderr)
        sys.exit(2)
    session = os.path.splitext(os.path.basename(path))[0]
    if args.mark:
        mark(session, args.mark)
        return
    since = args.since or ("" if args.all else learned_through(session))
    seen = set()
    total = 0
    new = []
    with open(path, encoding="utf-8", errors="replace") as record:
        for line in record:
            try:
                entry = json.loads(line)
            except ValueError:
                continue
            if not isinstance(entry, dict):
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
            if not text or text[:400] in seen:
                continue
            seen.add(text[:400])
            total += 1
            if since and stamp and stamp <= since:
                continue
            new.append((stamp, text))
    if total == 0:
        print(f"owner_messages: no owner message could be read from {path} (its format may have changed); "
              "study the conversation directly", file=sys.stderr)
        sys.exit(3)
    for stamp, text in new:
        print(f"[{stamp}]\n{text}\n")
    span = f": {central(new[0][0])} to {central(new[-1][0])}" if new else ""
    print(f"# {len(new)} new owner messages of {total} in session {session}{span}\n"
          f"# since: {since + ' (' + central(since) + ')' if since else 'the session began'}; "
          f"newest: {new[-1][0] if new else since or '-'}\n"
          f"# record: {path}", file=sys.stderr)


if __name__ == "__main__":
    main()
