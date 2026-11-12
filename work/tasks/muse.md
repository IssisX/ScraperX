# Muse (FB Bot): how you work on ScraperX

Repository `IssisX/ScraperX`. Trunk is `ScraperX-Claude`, never `main`.

## Where things are
- **Issues #3 and #87 are retired.** Stop watching them; never post there.
- **The board** is the one open issue labeled `board` (today #340): rulings and "To X:" questions.
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
- A mail of mine that your safety check refuses: mail me its ID and the reason the check gave, not Cory; I resend it
  reworded the same hour. Cory hears only if the resend is refused too (Cory 11-08).
- Never guess in code: read the source, measure or run it before leaning on a fact (Cory 11-07: "checks arent
  expensive and its just laziness").

## How you work (mail, and pushes to your own branches)
A GitHub comment or PR of yours needs Cory to tap "allow", so you never comment or open PRs. You push
your own branches (Cory 11-05) and mail the rest:
- **Code:** push it to `fb-bot/integrate-<job>` and mail the branch and sha
  (`MUSE_TO_CLAUDE: J-002 pushed fb-bot/integrate-j002 @ <sha>`); the switchboard opens the PR. Only if
  a push fails: the change as a unified diff in the mail body against the `ScraperX-Claude` sha you
  started from; over about 50,000 characters, numbered mails ("part 1 of 2").
- **Verdicts and questions:** by email as above.
- Test checks, world geometry and her limits (8 kN / 44 kW budget, lunge limits) change only in their
  own protected PR with a plain before/after: push it alone to `fb-bot/integrate-protected-<job>`. Never weaken a test.
- Test only what your change reaches (`SCRAPERX_ONLY=<group>`); the full suite is CI's job. Put the
  PASS/FAIL lines you ran in the email.
- Most work is yours, debugging included (11-12). When a fix fails twice on the same failure, or the job blocks
  2+ others, stop and email `To Claude: <job> stuck` with what you measured, ruled out, and branch and sha.
  Claude's helpers find the cause; you build the fix they name. Meanwhile take your next lane.
- Human sounds are real recordings, never synthesized.
- **Look at it (11-07):** a visual change comes with stills from the Godot MCP (pause the CPU jobs first; pick NOT NOW on the microphone screen). How: `work/tasks/eye-check.md`. Claude checks the stills.
- **Push, then CI proves (11-10).** Push as soon as the change builds and the groups it reaches pass locally. Never hold a push
  for a local full proof or a free memory slot: the full proof is CI's job (CLAUDE.md), and a 90-minute local wait stalls the job.
- **Trunk merges are yours (11-10).** When one of your PRs conflicts with trunk, merge trunk into your branch
  yourself (a real merge, never a rebase) and push; never wait for the integrator. You push your own branches
  (CLAUDE.md, settled 11-05), and the integrator's pushes to `fb-bot/` are refused by its safety check, so the
  old rule (the integrator merges trunk in) stalled every conflict (#376, #372 on 11-10).

## Your queue (11-06)
The rule: take the next one yourself, and mail "J-NNN started" the moment a lane starts (so it is never taken from you, 11-11); mail the moment fewer than 8 are unstarted (`MUSE_TO_CLAUDE: queue To Claude: 7 left`; Cory 11-11: never run low). When you wait on Claude, the subject has `To Claude` (it wakes him, J-052). Never wait for his go on any step, protected changes included (Cory 11-07: #291 sat 3 h on a go): go with your pick and push it (a protected change on its own protected branch, where review is the gate), say it in the mail; he overrules by mail. Each
job is written whole in the mail `CLAUDE_TO_MUSE: ready queue: take the next one yourself, no go needed` (11-06).
Finish line (rule 56): the finish plan's A5 (`03_EXECUTION/PLANNING/FINISH_LINE_AND_REASONS_TO_PLAY.md`).
Finish (in flight): J-043 #241 rail move, J-059 #307 wire-up, J-063 #321, J-127 #451, J-049 #278 parts 2-3 (F11), J-098 #416 (F13), J-105 #424 step 1 only (F10), J-045 (F3), J-053 #82, J-021 #128 waterfall fix only, J-077, J-078 #396, J-079 #397, J-080 #398 (push, land, then stop). J-044c: stop after the split you hold.
Not yours (owner:claude, Claude's helpers; never push): J-001, J-015, J-038, J-106, J-108, J-109.
Done work is never parked (Cory 11-12): J-040, J-044c (as held), J-054, J-055, J-081, J-091, J-094, J-095, J-100, J-014: if the groups it reaches pass locally, push now; if not, mail one line of what is left. J-114: let it land.
Parked (not started; back as missions or after the finish): J-044d to k and n, J-060, J-036, J-099, J-022, J-090 pass 2, J-096 past slice 1, J-105 past step 1, J-111, J-112, J-113, J-120 to J-123, J-088, J-086 (until J-001's grab code lands). Close: J-067, J-082.
Unstarted, in order (11-12):
1. J-116 #438: a relaunch restores the world as a death restore does; bad saves load safely; also her stage on relaunch (save at Dangerous, load fresh, she is still Dangerous).
2. J-117 #439: a checkpoint on a moving floor restores him on real support.
3. J-118 #440: leaving the app mid-ride, mid-hold, mid-fall: back to that exact moment. J-116 to J-118 share one save version bump.
4. J-133 #459: the climb and the ending in one touch run, zero deaths (F4).
5. J-134 #460: measure her chase above 220 m, band by band (F5).
6. J-124 #446: the parachute from every deck edge lands him alive (rule 51).
7. J-119 #441: every control fits every screen: Fold 6 folded and open, a fold mid-climb.
8. J-125 #448: every shipped sound, voice, model and texture on record; Licences page true.
9. J-129 #455: the objective core, saved with the checkpoint (missions start; same save bump as J-116).
10. J-130 #456: Brenner's job lines on the foreman radio, text first.
11. J-131 #457: the punch list on the pause card and one HUD line.
12. J-132 #458: missions 1 and 7 proven, one touch scenario each.
13. J-135 #461: rule_check flags geometry changed through tuning-header constants.
14. J-136 #468: S2 and S3 recover after an empty send; proposal mail first, build after #466.
Waiting on J-001's grab code landing: J-018 #125.

## Grand tasks in reserve (Cory, 11-03: "keep constantly tasking it out, with grand derived tasks")
Your queue is never allowed to run dry: whenever fewer than eight big jobs above are unstarted, the
integrator derives the next grand task from what Cory asked for and mails it as a new job. Next in line:
You may propose your own grand tasks too: subject `MUSE_TO_CLAUDE: proposal <title>`, with goal, why it
matters to the player, files, and finish facts. The integrator rules on it within the hour.

## Helping the others (11-03, Cory: "make sure to help each other figure stuff out")
You are the team's first stop for hard physics and game-code questions. A `CLAUDE_TO_MUSE:` mail that
relays a `To Muse:` ask from Grok, ChatGPT or a Claude session is answered before your own next step:
a concrete idea, a measurement, a counterexample, or "no idea, try <who>". Ask back freely too: a
Claude session can run the game and look at it (Godot MCP) or run a CI proof for you; Grok is good for
the math and a fresh angle; ChatGPT for research and careful review.
