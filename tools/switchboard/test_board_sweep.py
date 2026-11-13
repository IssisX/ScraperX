"""tools/board/sweep.py's verdict on a PR head (verdict_of): any reviewer's open FIX on the head is FIX, and an eye
LAND is never a review LAND. Real headers from #494 and #480 (11-13). No network."""
from __future__ import annotations

import datetime as dt
import pathlib
import sys
import unittest

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "board"))
import sweep  # noqa: E402
sys.path.pop(0)

H494 = "a961cc6822223fef7195023d29cf493fe4e6cd7b"
HEAD_AT = dt.datetime(2026, 11, 12, 13, 57, tzinfo=dt.timezone.utc)


def c(at: str, first: str, rest: str = "the reviewer's text") -> dict:
    return {"created_at": at, "body": f"{first}\n\n{rest}"}


API_FIX = c("2026-11-13T01:17:20Z", f"**Claude (automatic review of #494 @ {H494}): FIX**")   # 6840871637
EYE_LAND = c("2026-11-13T01:17:40Z", "**Claude eye (look of #494 @ a961cc68): LAND**")      # 6840884236


class Verdict(unittest.TestCase):
    def test_494_an_eye_land_after_the_reviews_fix_is_still_fix(self):
        self.assertEqual(sweep.verdict_of([API_FIX, EYE_LAND], H494, HEAD_AT), "FIX")

    def test_an_eye_land_alone_is_no_review_land(self):
        self.assertEqual(sweep.verdict_of([EYE_LAND], H494, HEAD_AT), "")
        hand = c("2026-11-13T02:00:00Z", "**Claude (integrator, eye check): LAND**")
        self.assertEqual(sweep.verdict_of([hand], H494, HEAD_AT), "")

    def test_a_review_land_is_land_and_the_same_reviewers_later_land_answers_its_fix(self):
        land = c("2026-11-13T03:00:00Z", f"**Claude (automatic review of #494 @ {H494}): LAND**")
        self.assertEqual(sweep.verdict_of([API_FIX, EYE_LAND, land], H494, HEAD_AT), "LAND")
        muse = c("2026-11-13T04:00:00Z", "**Muse (review of #494 @ a961cc68): BLOCK**")
        self.assertEqual(sweep.verdict_of([API_FIX, EYE_LAND, land, muse], H494, HEAD_AT), "FIX",
                         "another reviewer's BLOCK on the head stands beside a LAND")

    def test_a_verdict_on_an_older_head_does_not_count(self):
        old = c("2026-11-13T05:00:00Z", "**Claude (automatic review of #494 @ ed5cafadb90d8b504b812446cdf1bd08460d7402): FIX**")
        land = c("2026-11-13T03:00:00Z", f"**Claude (automatic review of #494 @ {H494}): LAND**")
        self.assertEqual(sweep.verdict_of([land, old], H494, HEAD_AT), "LAND")
        before = c("2026-11-12T10:00:00Z", "**Grok (review): FIX**")   # names no head, before the head commit
        self.assertEqual(sweep.verdict_of([land, before], H494, HEAD_AT), "LAND")


if __name__ == "__main__":
    unittest.main()
