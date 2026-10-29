# Integrator handoff, 2026-10-29 (about 2:20 PM Central)

For the next integrator session. Read CLAUDE.md, 00_START_HERE.md "Rules in force", then issue #3's
Matron exit board (comment 6504954652) and the latest comments. Times are Central.

## The goal now

Leave the Matron, with quality (owner, 1:24 PM: "I want the ball to roll as fast as we can out of
this matron editing section"; "We're going to finish it with quality"). The 13 items on the exit board
(below) are the whole remaining Matron scope; new ideas about her wait. After her: the summit ending
(seat the crew's last beam), dread before she is seen, then the route above 221 m.

## Who holds what (one owner per item; never assign a second)

| # | Item | Owner | Where |
|---|---|---|---|
| 1 | Honest perception | done | #35 landed (27aa5eb) |
| 2 | Every state has a clean exit (outside agent's patch) | Sonnet session session_01W3SSMxuour5YpKoTX1W3AP, JOB 11 | gemini/matron-state-exits-20261029 @ 7e9ee6a, integrating onto trunk |
| 3 | A restore brings her back (owner: "back") | Sonnet, merging trunk | PR #38; merge on green |
| 4 | Holds at a real arm's length, as a cinematic | Claude helper (wt-arm2) | PR #37 (draft) |
| 5 | Collision body follows her pose (M2) | Claude helper (wt-shape) | claude/matron-body-shape-20261029(-wip) |
| 6 | Pipe climb as a general move, touch_matron_pipe in CI | Claude helper (wt-pipe), building on Muse's fb-bot/j1-pipe-general @ 3535d41 | claude/matron-pipe-general-20261029 |
| 7 | Walk from her scan (J2) | Grok | PR #15 |
| 8 | Ledge jump (J4) | Muse sub-agent | fb-bot/matron-ledge-jump-20261029 @ 617e5e3, red on matron-tower |
| 9 | Ladder pull | ChatGPT | PRs #30/#34 |
| 10 | Hair (M1) | Muse | fb-bot/m1-hair-posed-volume-20261029; final answers sent 1:35 PM ("ANSWER on the hair") |
| 11 | The CI hang inside athletic_traversal (J3) | Grok | no branch yet |
| 12 | Her voice: 43 lines (03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md), all AI-generated like the fear yells, many takes, no repeats | Sonnet JOB 12 (candidates for the owner's ear) | claude/matron-voice-candidates-20261029 |
| 13 | Drawn-body checks in CI (bone stretch, hand on grip, penetration) | Sonnet JOB 13 | claude/drawn-body-checks-20261029 |

Also in flight: PR #39 (an outside agent's parallel CI layout; proof on gemini/integrate-ci-parallel-proof-20261029;
merge on green with a much shorter wall time; then "green" means every job but publish-checkpoint);
ChatGPT's #27 (Work ID + unapproved voices kept out of the APK), #29 (pipe-proof drum), C1 parachute
(#14) and C2 patrols from the scan after J2; tools/look stills suite (claude/matron-look-suite-20261029,
proof rerun pending).

## Channels

- Muse: email. It writes "MUSE_TO_CLAUDE:"; answer "CLAUDE_TO_MUSE:" to issisx3@gmail.com. Gmail search is
  by relevance: run "in:anywhere newer_than:3h from:issisx3@gmail.com -subject:CLAUDE_TO_MUSE" too, or
  you miss mail. Show the owner every subtask sent to Muse.
- Sonnet worker session: send_message to session_01W3SSMxuour5YpKoTX1W3AP; it reports by send_message;
  answer LAND or FIX. Its queue: JOB 11, 12, 13, 9 (maps refresh).
- Grok, ChatGPT: issue #3. Gemini Spark: prompts through the owner; it hands back format-patches,
  which the integrator applies to a gemini/* branch and proves.
- Checks: a 15-minute send_later reminder (general wording) and an hourly scheduled task.

## Owner calls today (all in the archive's words table)

She comes back after dying; her voice all AI-generated; all 43 lines kept, with variety; briefs leave
the how to the agent; the hold is a whole cinematic; status dots (green done, yellow in progress,
red blocked, white next, purple his) and "🟣 YOUR MOVE" at the end of every message; ChatGPT, Claude
and Muse all work here; checks every 10-15 minutes; look at the game with the Godot MCP runtime.
Tests must cover the drawn body, not only the sim (her 12-foot arm passed every test).
