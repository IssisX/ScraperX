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
- A mail of mine that your safety check refuses: tell Cory and mail me its ID and the reason the check gave; he
  pastes what you need (11-07).
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
- **Trunk merges are the integrator's (11-05).** When one of your PRs conflicts with trunk, the integrator merges
  trunk in and mails you the new sha; pull it before your next push. Never start an agent for a trunk merge. The
  one exception: a mail that says "Muse: resolve" (the conflict is in your own logic).

## Your queue (11-06)
The rule: take the next one yourself; mail when 3 are left (`MUSE_TO_CLAUDE: queue To Claude: 3 left`). When you wait on Claude, the subject has `To Claude` (it wakes him, J-052). Never wait for his go on any step, protected changes included (Cory 11-07: #291 sat 3 h on a go): go with your pick and push it (a protected change on its own protected branch, where review is the gate), say it in the mail; he overrules by mail. Each
job is written whole in the mail `CLAUDE_TO_MUSE: ready queue: take the next one yourself, no go needed` (11-06).
1. J-052 #292: the switchboard routes every item to its owner (top job, 11-06 2:26 PM).
2. J-053 #82: the Void Crossing (Cory 11-06 2:15 PM). Measure the 750 m glide first, post it on #82.
3. J-054 #293: the Counterweight Crossing (Cory 11-06), a cradle tipped by real mass and torque.
4. J-007 #114: her exit items 4, 6, 7 (arm's-length holds, the pipe, the hoists).
5. J-032 #157: her hands within reach (re-measure with the J-028 probe, fix what is left).
6. J-008 #115: her "258 down" leg; a strict total order for the ladder-bar sort.
7. J-011 #118: her skirt as cloth, bone-driven panels (from `edad91a3`).
8. J-024 #131: plants, step 2 (from `builder/integrate-lane-e2` @ `b7c4ca1a`).
9. J-021 #128: the far world and water (from `b532042b` and `95587901`).
10. J-009 #116: device determinism (from `fb96ead9`), once J-008's sort is pushed.

Live lanes: J-042 windup #291, J-001 #227, J-050 #281, J-038 slice 1, J-044l, J-043 #241, J-045 (sim first
step), J-022 #129, the wreck fix (J-016 #123), sense_near, J-049 #278, J-013 #120, J-014 #121, J-015 #122. Waiting: J-055 #294 Blast Door (on #291 and J-049); J-018 #125 (on J-001); S5 J-036 #167 and J-040 #228 (on J-044d); J-044e, f, g, k (on c, d).

## Grand tasks in reserve (Cory, 11-03: "keep constantly tasking it out, with grand derived tasks")
Your queue is never allowed to run dry: whenever fewer than three big jobs above are unstarted, the
integrator derives the next grand task from what Cory asked for and mails it as a new job. Next in line:
- **G1, her mind runs on evidence (rule 29, `03_EXECUTION/GAP_LIST.md` gap 4):** perception is evidence
  with an age, rage is fed by sound that reaches her ear, and hiding from her truly works. Find every
  place her decisions still read his true state (`step_travel`'s `player_on` checks, the braced state,
  the ankle passed during a hold), route each through what she saw or heard, and prove hiding with
  runtime scenarios (he hides and she loses him; he makes a sound and she finds him).
You may propose your own grand tasks too: subject `MUSE_TO_CLAUDE: proposal <title>`, with goal, why it
matters to the player, files, and finish facts. The integrator rules on it within the hour.

## Helping the others (11-03, Cory: "make sure to help each other figure stuff out")
You are the team's first stop for hard physics and game-code questions. A `CLAUDE_TO_MUSE:` mail that
relays a `To Muse:` ask from Grok, ChatGPT or a Claude session is answered before your own next step:
a concrete idea, a measurement, a counterexample, or "no idea, try <who>". Ask back freely too: a
Claude session can run the game and look at it (Godot MCP) or run a CI proof for you; Grok is good for
the math and a fresh angle; ChatGPT for research and careful review.
