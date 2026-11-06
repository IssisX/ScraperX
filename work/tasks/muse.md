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
- **Trunk merges are the integrator's (11-05).** When one of your PRs conflicts with trunk, the integrator merges
  trunk in and mails you the new sha; pull it before your next push. Never start an agent for a trunk merge. The
  one exception: a mail that says "Muse: resolve" (the conflict is in your own logic).

## Your queue (11-06)
The rule: take the next one yourself; mail when 3 are left (`MUSE_TO_CLAUDE: queue To Claude: 3 left`). When you wait on Claude for anything, the subject has `To Claude` (it wakes him, J-052). Each
job is written whole in the mail `CLAUDE_TO_MUSE: ready queue: take the next one yourself, no go needed` (11-06).
1. J-053 #82: the Void Crossing (Cory 11-06 2:15 PM). Measure the 750 m glide first, post it on #82.
2. J-054 #293: the Counterweight Crossing (Cory 11-06), a cradle tipped by real mass and torque.
3. J-007 #114: her exit items 4, 6, 7 (arm's-length holds, the pipe, the hoists).
4. J-032 #157: her hands within reach (re-measure with the J-028 probe, fix what is left).
5. J-008 #115: her "258 down" leg; a strict total order for the ladder-bar sort.
6. J-011 #118: her skirt as cloth, bone-driven panels (from `edad91a3`).
7. J-024 #131: plants, step 2 (from `builder/integrate-lane-e2` @ `b7c4ca1a`).
8. J-021 #128: the far world and water (from `b532042b` and `95587901`).
9. J-009 #116: device determinism (from `fb96ead9`), once J-008's sort is pushed.

Live lanes: J-042 windup #291, J-001 #227, J-050 #281, J-038 slice 1, J-044l, J-043 #241, J-045 (sim first
step), J-022 #129, the wreck fix (J-016 #123), sense_near, J-049 #278, J-013 #120, J-014 #121, J-015 #122,
J-052 #292. Waiting: J-055 #294 Blast Door (on #291 and J-049); J-018 #125 (on J-001); S5 J-036 #167 and J-040 #228 (on J-044d); J-044e, f, g, k (on c, d).

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
