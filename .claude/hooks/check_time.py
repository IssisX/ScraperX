#!/usr/bin/env python3
"""PreToolUse hook: refuse a GitHub post or a mail whose stated time is not the clock's.

Cory, 11-07: every time written is read from the clock first (`TZ=America/Chicago date`).
A stamp is checked when it is
  - the first H:MM AM/PM on the first line of a body, title or message, or in a mail subject, or
  - any H:MM AM/PM followed by Central, CT, CST or CDT in the first three lines.
Later times in the text (quoted pushes, next runs) are not stamps and are left alone.
A stamp more than PAST_MIN minutes before the clock, or FUTURE_MIN after it, blocks the
call: exit 2, the reason on stderr, which Claude Code feeds back to the model.
"""
import json
import re
import sys
from datetime import datetime
from zoneinfo import ZoneInfo

PAST_MIN = 10
FUTURE_MIN = 2
TIME = re.compile(r"\b(1[0-2]|0?[1-9]):([0-5]\d)\s*([AaPp])\.?\s*[Mm]\b\.?")
TAGGED = re.compile(TIME.pattern + r"\s*\(?(Central|CT|CST|CDT)\b")
TEXT_FIELDS = ("body", "title", "subject", "message")


def minutes(h: str, m: str, ap: str) -> int:
    hour = int(h) % 12 + (12 if ap.lower() == "p" else 0)
    return hour * 60 + int(m)


def stamps(field: str, text: str):
    lines = text.splitlines()[:3]
    if not lines:
        return
    first = TIME.search(lines[0]) if field != "subject" else TIME.search(text)
    if first:
        yield first
    for line in lines:
        for t in TAGGED.finditer(line):
            yield t


def main() -> int:
    try:
        event = json.load(sys.stdin)
    except (json.JSONDecodeError, ValueError) as e:
        print(f"check_time: hook input is not JSON ({e}); not checked", file=sys.stderr)
        return 0
    tool_input = event.get("tool_input") or {}
    now = datetime.now(ZoneInfo("America/Chicago"))
    now_min = now.hour * 60 + now.minute
    bad = []
    for field in TEXT_FIELDS:
        text = tool_input.get(field)
        if not isinstance(text, str):
            continue
        seen = set()
        for t in stamps(field, text):
            if (t.string, t.start()) in seen:
                continue
            seen.add((t.string, t.start()))
            diff = (minutes(*t.group(1, 2, 3)) - now_min + 720) % 1440 - 720
            if diff > FUTURE_MIN or diff < -PAST_MIN:
                bad.append(f"{field}: '{t.group(0)}' ({diff:+d} min)")
    if not bad:
        return 0
    print(
        f"BLOCKED by .claude/hooks/check_time.py: the clock says {now:%-I:%M %p} Central, "
        f"but the post states {'; '.join(bad)}. Run `TZ=America/Chicago date` and rewrite "
        f"every stated time from it (CLAUDE.md, Cory 11-07).",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    sys.exit(main())
