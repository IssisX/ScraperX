# Muse (FB Bot): how you work on ScraperX

Repository `IssisX/ScraperX`. Trunk is `ScraperX-Claude`, never `main`.

## Where things are
- **Issues #3 and #87 are retired.** Stop watching them; never post there.
- **The board** is the one open issue labeled `board` (today #107): rulings and "To X:" questions.
  Find it by label each time.
- **Your jobs** each have their own issue (label `job` and `owner:muse`). Read only those and the
  board's newest comments, never old history.

## How you talk to the team (email, because each GitHub comment of yours needs Cory's approval)
- Send a NEW email (not a reply) to the address you already use, with a subject that begins exactly
  `MUSE_TO_CLAUDE:` and names the job or PR: `MUSE_TO_CLAUDE: J-002 second hit now sounds`. It is
  posted on that issue within 5 minutes. Without a J-NNN or #N it lands on the board.
- To give a review verdict on someone's PR, end the subject with `review of #N @ <first 8 characters
  of the head sha>: LAND` (or `FIX`). A FIX needs four lines in the body starting `command:`,
  `input:`, `expected:`, `observed:`; without them it counts as a NOTE.
- To ask someone: subject `MUSE_TO_CLAUDE: <J-NNN or #N> To <Name>: <question in a few words>`, and
  in the body what you are doing, what you tried, the exact question, and what a useful reply looks
  like (Cory's collaboration rule). Don't wait for the answer: keep working.
- Mail to you arrives with a subject starting `CLAUDE_TO_MUSE:`.

## How you work
- One branch per job, named `fb-bot/integrate-<job>` (for example `fb-bot/integrate-j002-second-hit`),
  reused for that job. Only `*/integrate-*` branches run the CI proof when pushed.
- Open a PR into `ScraperX-Claude` with `Job: J-NNN` as its first body line. Never merge: the
  switchboard merges after a review from another vendor (Claude reviews yours automatically).
- Test checks, world geometry and her limits (8 kN / 44 kW budget, lunge limits) change only in their
  own protected PR with a plain before/after. Never weaken a test.
- Test only what your change reaches (`SCRAPERX_ONLY=<group>`); the full suite is CI's job.
- Two red proof runs on the same failure: stop changing numbers, email what you measured, what you
  ruled out and the one observation that would settle it, and mark the job BLOCKED.
- Human sounds are real recordings, never synthesized.

## Your open work now
- **J-002 (#109), PR #92, the silent second hit:** your fix is on `fb-bot/fight-s1-20261103-1496f726`
  (78ab1974). Push it to `fb-bot/integrate-j002-second-hit`, open its PR (it supersedes #92), and put
  the `matron-grab` check change ("a blow not his, mid-hold" reading blow_mps after the quiet window)
  in its own protected PR with a before/after.
- J-005 (#112), J-022 (#129), J-023 (#130, PR #95): as their issues say.
