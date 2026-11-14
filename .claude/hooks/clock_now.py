#!/usr/bin/env python3
"""UserPromptSubmit hook: hand Claude the clock with every message from Cory.

Cory 11-14: a time was given unchecked ("about 10 minutes" for a helper 5 minutes old), and blocking the reply
afterward is the wrong fix. So the reading comes first, every turn: any time or elapsed time in the reply is
worked out from this line and the record, never from memory.
"""
import json
import sys
from datetime import datetime, timezone
from zoneinfo import ZoneInfo

now = datetime.now(timezone.utc)
central = now.astimezone(ZoneInfo("America/Chicago"))
print(json.dumps({"hookSpecificOutput": {
    "hookEventName": "UserPromptSubmit",
    "additionalContext": (f"Clock read at Cory's message: {now:%Y-%m-%dT%H:%M:%SZ} (Central {central:%H:%M}). "
                          "Work every elapsed time from this and the record; never state one from memory."),
}}))
