#!/usr/bin/env python3
"""PreToolUse hook: fill in the clock's time where a call asks for it. It never blocks.

Write the NOW token (the word NOW in braces) where the time of sending or writing goes in a post, mail,
message, command or edit; this hook replaces it with the clock's Central time (h:MM AM) through updatedInput,
so that time is the clock's by construction. The token in doubled braces writes the token itself.

Cory 11-14: every time comes from a clock reading or the record, never memory, and no time is banned. So a
typed or quoted time passes. The 11-11 block on every typed time was a session's workaround, not his rule, and
it refused times copied from the record (11-14: a message quoting a GitHub post's time, twice).
"""
import json
import sys
from datetime import datetime
from zoneinfo import ZoneInfo

NOW_TOKEN = "{" "NOW" "}"   # split so this hook never rewrites its own source in an edit
FIELDS = ("body", "title", "subject", "message", "command", "content", "new_string")


def main() -> int:
    try:
        tool_input = json.load(sys.stdin).get("tool_input") or {}
    except (ValueError, AttributeError):
        return 0
    if not any(isinstance(tool_input.get(f), str) and NOW_TOKEN in tool_input[f] for f in FIELDS):
        return 0
    stamp = f"{datetime.now(ZoneInfo('America/Chicago')):%-I:%M %p}"
    updated = dict(tool_input)
    for f in FIELDS:
        if isinstance(updated.get(f), str):
            updated[f] = (updated[f].replace("{" + NOW_TOKEN + "}", "\0").replace(NOW_TOKEN, stamp)
                          .replace("\0", NOW_TOKEN))
    print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "updatedInput": updated}}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
