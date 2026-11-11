#!/usr/bin/env python3
"""PreToolUse hook: refuse a GitHub post or a mail whose stated time is not the clock's.

Cory, 11-07: every time written is read from the clock first (`TZ=America/Chicago date`).
A stamp is checked when it is
  - the first H:MM AM/PM on the first line of a body, title or message, or in a mail subject, or
  - any H:MM AM/PM followed by Central, CT, CST or CDT in the first three lines.
Later times in the text (quoted pushes, next runs) are not stamps and are left alone.
Any such stamp typed by hand blocks the call: exit 2, the reason on stderr, which Claude Code feeds back to
the model.

Cory, 11-11: promises to "check first" kept failing, so the stamp is no longer typed. Write {NOW} where the
time goes; this hook replaces every {NOW} in the call with the clock's Central time (h:MM AM) through
updatedInput, so the time sent is the clock's by construction; {{NOW}} writes the token itself. A hand-typed stamp in a post or mail is
refused outright, right or wrong: only {NOW} writes the time of sending.
"""
import json
import re
import sys
from datetime import datetime
from zoneinfo import ZoneInfo

TIME = re.compile(r"\b(1[0-2]|0?[1-9]):([0-5]\d)\s*([AaPp])\.?\s*[Mm]\b\.?")
TAGGED = re.compile(TIME.pattern + r"\s*\(?(Central|CT|CST|CDT)\b")
TEXT_FIELDS = ("body", "title", "subject", "message")
NOW_TOKEN = "{NOW}"
NOW_FIELDS = TEXT_FIELDS + ("command", "content", "new_string")


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
    stamp = f"{now:%-I:%M %p}"
    if any(isinstance(tool_input.get(f), str) and NOW_TOKEN in tool_input[f] for f in NOW_FIELDS):
        updated = dict(tool_input)
        for f in NOW_FIELDS:
            if isinstance(updated.get(f), str):   # {9:26 AM} writes the token itself (rules, this hook's own code)
                updated[f] = (updated[f].replace("{" + NOW_TOKEN + "}", "\0").replace(NOW_TOKEN, stamp)
                              .replace("\0", NOW_TOKEN))
        print(json.dumps({"hookSpecificOutput": {"hookEventName": "PreToolUse", "updatedInput": updated}}))
        return 0
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
            bad.append(f"{field}: '{t.group(0)}' ({diff:+d} min from the clock)")
    if not bad:
        return 0
    print(
        f"BLOCKED by .claude/hooks/check_time.py: a hand-typed stamp ({'; '.join(bad)}). Write {NOW_TOKEN} "
        f"where the time of sending goes; this hook fills in the clock's time (now {stamp} Central). "
        f"Cory 11-11: the time is never typed.",
        file=sys.stderr,
    )
    return 2


if __name__ == "__main__":
    sys.exit(main())
