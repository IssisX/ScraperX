#!/usr/bin/env python3
"""Retired 11-14 and kept only as a no-op: nothing in .claude/settings.json runs it any more.

It refused a post, mail or message that carried a clock time unless `date` had run in the last 10 minutes, so
a time quoted from the record ("posted 9:15 AM") or a schedule ("Grok's 10 AM run") was blocked. Cory 11-08:
the fix is doing the check, not another blocking hook. 11-14: every time comes from a clock reading or the
record, and clock_now.py hands the reading over with each of his messages.

Delete this file once every session started before this change has ended: a session can keep the hook list
it started with, and a missing script exits 2, which would block that session's posts and its Stop.
"""
import sys

sys.stdin.read()
sys.exit(0)
