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
- Ask Claude early (Cory 11-06: J-001 ran 8 h without asking). Two red proof runs on the same failure,
  or a subagent 60 minutes without a commit or a new number: stop it and email `To Claude: <job> stuck`
  with what you measured, what you ruled out and the one observation that would settle it.
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
In flight (finish these): J-094 #412, J-091 #409, J-055 #294 (started 9:09 AM), J-043 #241 (started 8:27 AM), J-054 #293 (protected branch), J-044l #242 (unparked 11-11, Grok answered), J-044c Argon's three fixes (top lane, 11-11: J-060 waits on it), J-063 #321 (chest at the face), J-059 #307, J-007 #114, J-009 #116, J-015 #389, J-038 #372, J-079 #397, J-080 #398, J-014 #121, J-021 #128. Claude has: J-090 #408, J-096 #414, J-084 #402, J-092 #410, J-082 #400. Parked: J-053 #82 (on Grok), J-060 #320 (on J-044c).
Unstarted, in order (11-11 7 AM; new playable content first):
0e. J-095 #413: pull the line out from under her (Grok idea 2, Cory 11-11): the cable becomes a falling rope.
4. J-083 #401: a forklift lever he grabs and moves (D4).
5. J-087 #405: story radio cues fire in play (B3); wiring only.
6. J-081 #399: a readable night climb (B5).
7. J-088 #406: her fingers wrap what she holds; the pipe warning in CI (A6).
8. J-089 #407: her spoken lines wired to her events; candidate recordings for Cory (rule 43).
9. J-085 #403: she hunts him at every height; after J-063 lands (same climb code).
10. J-086 #404: she pulls him off a ladder, every ladder kind; after J-001 lands (Claude's helper is in the grab code now).
11. J-098 #416: no synthesized human sounds: her wake and his breath become real recordings (law).
12. J-099 #417: dents: containers, barrels, steel boxes dent by real impulse (J-038's second half, Cory).
13. J-100 #418: she reads a load hung over her and gets out from under it (her side of J-092's slab trap).

Live lanes: J-042 windup #291, J-050 #281, J-038 slice 1, J-044l, J-043 #241, J-045 (sim first
step), J-022 #129, the wreck fix (J-016 #123), sense_near, J-049 #278, J-013 #120, J-014 #121, J-015 #122. Waiting: J-055 #294 Blast Door (on #291 and J-049); J-018 #125 (on J-001); S5 J-036 #167 and J-040 #228 (on J-044d); J-044e, f, g, k (on c, d).

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
