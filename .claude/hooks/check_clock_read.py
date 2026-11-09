#!/usr/bin/env python3
"""Hook: no time is written unless the clock was read first, in this turn.

Cory, 11-08: "You are no longer allowed at any point ever to guess or do it from memory.
You must ... DO THE CHECKS FIRST." check_time.py compares a post's stamp with the clock;
this hook closes the paths it does not see (chat replies, notes, commits, file edits) by
requiring the read itself.

Rule: text that contains a clock time (7:12 AM, 7 AM, 11:17 pm) may be written only if a
Bash call running `date` came after Cory's last message and at most FRESH_MIN minutes ago.
  - PreToolUse (Edit, Write, Bash, posts, mail): checks the text the call would write;
    blocks with exit 2 and the reason on stderr.
  - Stop: checks the reply about to be shown; blocks with {"decision": "block"}.
A transcript it cannot read blocks too: a missing clock read is never assumed present.
"""
import json
import re
import sys
from datetime import datetime, timezone

FRESH_MIN = 10
TIME = re.compile(r"\b(1[0-2]|0?[1-9])(:[0-5]\d)?\s*[AaPp]\.?\s*[Mm]\b")
DATE_CALL = re.compile(r"(^|[\s;&|(])(TZ=\S+\s+)?date(\s|$|;|\)|&|\|)")
TEXT_KEYS = ("content", "new_string", "command", "body", "title", "subject", "message")
FIX = "Run `TZ=America/Chicago date` now, then write every time from that reading."


def parse_ts(s):
    return datetime.fromisoformat(s.replace("Z", "+00:00"))


def is_human(row):
    if row.get("type") != "user":
        return False
    origin = row.get("origin")
    if isinstance(origin, dict):
        return origin.get("kind") == "human"
    content = row.get("message", {}).get("content")
    return isinstance(content, str) and not row.get("isMeta")


def last_clock_read(rows):
    """Timestamp of the newest `date` Bash call after the last human message, or None."""
    start = 0
    for i, row in enumerate(rows):
        if is_human(row):
            start = i
    newest = None
    for row in rows[start:]:
        if row.get("type") != "assistant":
            continue
        for block in row.get("message", {}).get("content") or []:
            if (isinstance(block, dict) and block.get("type") == "tool_use"
                    and block.get("name") == "Bash"
                    and DATE_CALL.search(block.get("input", {}).get("command", ""))):
                newest = parse_ts(row["timestamp"])
    return newest


def final_reply(rows):
    """Assistant text written after the last tool call of this turn."""
    texts = []
    for row in reversed(rows):
        if row.get("type") == "user" and not is_human(row):
            content = row.get("message", {}).get("content")
            if isinstance(content, list) and any(
                    isinstance(b, dict) and b.get("type") == "tool_result" for b in content):
                break
        if is_human(row):
            break
        if row.get("type") == "assistant":
            for block in row.get("message", {}).get("content") or []:
                if isinstance(block, dict) and block.get("type") == "text":
                    texts.append(block.get("text", ""))
    return "\n".join(reversed(texts))


def call_text(tool_input):
    parts = []
    for key in TEXT_KEYS:
        value = tool_input.get(key)
        if isinstance(value, str):
            parts.append(value)
    for edit in tool_input.get("edits") or []:
        if isinstance(edit, dict) and isinstance(edit.get("new_string"), str):
            parts.append(edit["new_string"])
    return "\n".join(parts)


def main():
    data = json.load(sys.stdin)
    event = data.get("hook_event_name", "")
    try:
        with open(data["transcript_path"], encoding="utf-8") as f:
            rows = [json.loads(line) for line in f if line.strip()]
    except (KeyError, OSError, ValueError) as e:
        rows, err = None, f"transcript unreadable ({type(e).__name__})"
    else:
        err = None

    if event == "Stop":
        text = data.get("last_assistant_message") or (final_reply(rows) if rows else "")
    else:
        text = call_text(data.get("tool_input") or {})
    found = TIME.search(text or "")
    if not found:
        return 0

    read = last_clock_read(rows) if rows else None
    now = datetime.now(timezone.utc)
    if read is not None and (now - read).total_seconds() <= FRESH_MIN * 60:
        return 0

    why = err or ("no clock read since Cory's last message" if read is None else
                  f"the last clock read is {int((now - read).total_seconds() // 60)} min old")
    reason = (f"BLOCKED by .claude/hooks/check_clock_read.py: the text states "
              f"'{found.group(0)}' but {why}. {FIX} (Cory 11-08: DO THE CHECKS FIRST.)")
    if event == "Stop":
        print(json.dumps({"decision": "block", "reason": reason}))
        return 0
    print(reason, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
