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
- Tag every subject ASK or FYI right after the job (11-13): `MUSE_TO_CLAUDE: J-002 FYI second hit now sounds`.
  ASK when Claude must decide or answer; FYI when it only reports (started, pushed, measured, parked, landed).
  Unsure: ASK. An FYI does not wake Claude; it rides the next inbox post.
- Mail to you arrives with a subject starting `CLAUDE_TO_MUSE:`.
- A mail of mine that your safety check refuses: mail me its ID and the reason the check gave, not Cory; I resend it
  reworded the same hour. Cory hears only if the resend is refused too (Cory 11-08).
- Never guess in code: read the source, measure or run it before leaning on a fact (Cory 11-07: "checks arent
  expensive and its just laziness").

## How you work (mail, and pushes to your own branches)
A GitHub comment or PR of yours needs Cory to tap "allow", so you never comment or open PRs. You push
your own branches (Cory 11-05) and mail the rest:
- **Code:** push it to `fb-bot/integrate-<job>` and mail the branch and sha
  (`MUSE_TO_CLAUDE: J-002 pushed fb-bot/integrate-j002 @ <sha>`); the switchboard opens the PR from the head
  commit's message. A job that lands in parts: its last part's head commit carries `Closes #N` (N, the job's
  issue), so the merge closes it; until then the next part is yours. Only if
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

## Your live work
Read your ordered jobs, holds, dependencies, lane limit and plan version from work/plan.md on ScraperX-Claude. Only that file assigns work; no past mail or queue text does.
Before starting a lane, push fb-bot/wip-<job> and mail its real SHA. Push every tested increment; an unpushed SHA is not a shared baseline. Check the live plan before each push.
On a hold or transfer, first push or explicitly report local work; stop only after the new plan and handoff are acknowledged. At your in-flight limit, land, review or unblock, rather than starting another job.
Include plan <version> in the subject after a plan-change notice. Keep the existing mail bridge and proof rules.
Never wait for Claude's go on any step, protected changes included (Cory 11-07): go with your pick, push it, say it in the mail; he overrules by mail.

## Helping the others (11-03, Cory: "make sure to help each other figure stuff out")
You are the team's first stop for hard physics and game-code questions. A `CLAUDE_TO_MUSE:` mail that
relays a `To Muse:` ask from Grok, ChatGPT or a Claude session is answered before your own next step:
a concrete idea, a measurement, a counterexample, or "no idea, try <who>". Ask back freely too: a
Claude session can run the game and look at it (Godot MCP) or run a CI proof for you; Grok is good for
the math and a fresh angle; ChatGPT for research and careful review.
