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

## How you work (email only: never act on GitHub yourself)
Every GitHub action of yours (a comment, a push, a PR) needs Cory to tap "allow", and he can't sit
there doing that. So you never touch GitHub directly; email does everything:
- **Code:** put the change in the email body as a unified diff (`git diff` output) against the
  `ScraperX-Claude` sha you started from, with a subject like
  `MUSE_TO_CLAUDE: J-002 patch on <sha>`. The integrator puts it on `fb-bot/integrate-<job>` and opens
  the PR for you. A diff over about 50,000 characters: split it into numbered emails ("part 1 of 2").
- **Verdicts and questions:** by email as above.
- Test checks, world geometry and her limits (8 kN / 44 kW budget, lunge limits) change only in their
  own protected PR with a plain before/after: send that diff in its own email. Never weaken a test.
- Test only what your change reaches (`SCRAPERX_ONLY=<group>`); the full suite is CI's job. Put the
  PASS/FAIL lines you ran in the email.
- Two red proof runs on the same failure: stop changing numbers, email what you measured, what you
  ruled out and the one observation that would settle it, and mark the job BLOCKED.
- Human sounds are real recordings, never synthesized.

## Your open work now (11-03, Cory: "Muse ... should do main, difficult and long-winded tasks")
You lead the fight. Use your subagents: run independent stages or files in parallel, and send each
finished piece as its own diff email as soon as it is proven, not all at the end.
0. **J-001 (#108), her hold with real arms, is now yours (Cory, 11-03), first in line.** Its code is on
   PR #152 (`chatgpt/integrate-j001-swat-budget`, head `50e1bf49`, on top of #102's hold), red on two checks
   that are not the swat's: `matron load S2 over-base: a carry swing crossed the crouch base` and
   `matron crash: she went to the hit, found nobody, and is the warier for it`. ChatGPT's diagnosis is on #152
   (two candidate causes: synthetic held-load attribution while regripping, or the new `drag_pose()` target
   against the old counter-lean), and a Claude session is posting the failing-tick values there. Find the cause,
   fix it at the owner, and send the diff on top of `50e1bf49`. The integrator puts it on that branch.
   Protected checks (#103's) go on `fb-bot/integrate-protected-j001-hold-checks`. ChatGPT reviews it.
1. **Finish J-002 (#109):** the late require ("once the rip has begun, his blow breaks nothing") read
   at its peak, as its own diff on top of `1620c32e` (PR #158), so #153 goes green.
2. **The fight, S2-S5, built end to end (J-005, #112, then one job each):**
   - Send your corrected `FIGHTING_SYSTEM_SPEC.md` as a diff adding `03_EXECUTION/FIGHT/FIGHTING_SYSTEM_SPEC.md`
     (docs only).
   - For each of S2, S3, S4 and S5, email `MUSE_TO_CLAUDE: J-005 scope S<n>` with four lines:
     invariant, files, reach (which native groups and touch scenarios), finish fact. The integrator
     opens each as its own job issue owned by you, the same hour.
   - Then build them in order of dependency, each stage its own diff (and its own PR); a new or
     changed test check goes in its own protected diff with a before/after. Native kernel first
     (`src/sim/`), then the Godot view where the stage shows on screen.
   - Neighbours: S1 is yours (#153); S6, her tells (J-004), and S7 + S8, damage and death body (J-003),
     are ChatGPT's. Read their fields, never edit their functions; ask with `To ChatGPT:`.
3. **J-022 (#129), environment lane D:** gusts, wind you can see, clouds, dust in the stack. Long job;
   work it when a fight stage waits on review.
4. J-023 (#130, PR #95): its in-game stills, as the issue says.
