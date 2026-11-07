# Integrator notes (live state only; re-read after every compact)

Session `session_01W3SSMz5zMw3Tm5DXRpisrG`; compacts itself at about 225k; a new session only if broken.
Cap 10 KB: asks in flight, merge queue, live rulings, the meter readings the pace uses. No histories, tallies or
per-helper logs (Cory 11-05, 11-06: *"That's wasting usage"*). Older notes, verbatim:
`INTEGRATOR_NOTES_LOG_2026-11-05.md` (to 2:15 PM 11-05), `INTEGRATOR_NOTES_LOG_2026-11-07.md` (to 10:11 AM 11-07).

Read through: 1794066268 (2026-11-07 15:44:28Z, 9:44 AM Central). Every run sets it after acting.

## After every compact
1. Re-read `CLAUDE.md`, `work/tasks/integrator-hourly.md` and this file.
2. `python3 tools/board/asks.py --hours 24`; clear every line not under "Asks in flight".
3. Muse's mail: `subject:MUSE_TO_CLAUDE newer_than:3h`. Its diffs come as attachments the Gmail tools do not return;
   a helper decodes them from the RAW message.
4. Before any mail to Muse: `work/tasks/mail-to-muse.md`. Stamp times only from `TZ=America/Chicago date`.

## How merges work
- The switchboard merges `Job: J-NNN` PRs on a green proof plus another vendor's LAND, protected ones too; workflow-file
  PRs with SWITCHBOARD_MERGE_TOKEN after clean Codex and Claude API reviews on the full head sha (J-056, J-057).
- My own hand merges are refused by the safety check ([Auto-Mode Bypass], [Merge Without Review]), and so are branch
  deletes (Git Destructive). Never work around either.
- Muse pushes its own `fb-bot/integrate-[protected-]<job>` branches; the switchboard opens the PR (J-047). A PR this
  session opens subscribes it: unsubscribe at once unless I merge it myself.
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's; slice names dashed.
- sweep.py takes the latest verdict and misses an earlier unfixed P1 from another vendor (seen on #301).

## Live rulings
- Nobody waits on my go, protected work included (Cory 11-07 9:32 AM).
- Builds go to Muse; Claude integrates and codes 1-2 key jobs. Helpers: Opus, at most 2; none new today unless acute.
- Every hard diagnosis or ruling gets a `To ChatGPT:` ask at the start, beside my first pass. Grok only on work no other
  agent holds (Cory 11-07 8:39 AM); its open asks: #108 J-001 rows, #227, #244 Turn burst, the #301 review.
- #298 (8:38 AM, ChatGPT's, comment 6708298378): the snag is momentum; replay the first-tick snag lambda from an
  identical state first: within about 5%, a protected geometry PR, else fix the chain. External forces at most 8 kN;
  all positive work incl. joint motors at most 44 kW; upper motors power-only `max(0, tau * omega_rel)`, limited
  pre-solve to `min(cap, share / |omega_rel|)`; external first, the upper body takes what is left, starving free arm,
  other arm, head, neck, chest in that order; one account per tick, the Climber by reference. Written into spec H.
- Fight (J-044): her jointed upper body inside 8 kN / 44 kW; an injury costs function in proportion and her healthy
  limbs take over, the budget never rises; a daze from head impulse replaces the fixed knockdown. Checkpoints keep the
  exact daze and her injuries, dead included (J-044g versions the save; rule 34, Cory 11-05 and 11-07).
- J-042: one path for every loose object; she throws up to 25 kg and shoves anything heavier.
- J-049: her breath within 2 m gives a lead (Cory "breath", 11-07 6:22 AM, final).
- J-053: first balcony 640 m, chain to 728 m, a running jump; protected geometry PR, then my eye check.
- J-059 #307 her black eye (Cory 11-07): eye_l/eye_r regions; dark at 75 N·s, lid swelling at 150, a slit at 250; on
  the skin; J-044g drives it in play.
- J-015: water ballast per S1; S2 cart under 430 kg, S3 car under 450 kg drained (asked of ChatGPT on #122).
- Picks told to Cory: the edge-first pallet arm break stays; the extinguisher is a placeholder; the slingshot pouch's
  native panels stay drawn; shadows LOW (#263; SPLITS if it looks bad on his phone).
- Godot MCP on Muse's box works (9:47 AM): pause the CPU jobs, look, resume. Bridge clicks do not fire the game's
  buttons (NOT NOW confirmed): emit `pressed`. Method: `work/tasks/eye-check.md`.

## Open PRs (10:15 AM 11-07)
- #301 J-052 @ e0c7d5a7 (each item to its owner): ChatGPT's P1 (6706881549: the label write after the comment loses the
  mail; Grok reproduced it) is unfixed; Muse's top job; the inbox flagged it QUIET for 3 h.
- #299 J-038 protected @ d48ca768: three Codex P1s (break pieces not saved, checkpoint topology, no Godot mesh), Muse's;
  my eye check once the mesh lands.
- #298 J-044c @ 03ad884b: Muse's whole job since 8:35 AM, under the 8:38 ruling.
- #291 J-042 windup protected @ 824bc8e0: touch-2 touch_matron_ammo (up read JUMP, not THROW, toolbox in hands); Muse's.
- #282 the inbox (draft, never merge; stay subscribed).
- #227 J-001 HOLD @ b555dd61: Muse measures the three snag-frame numbers; Grok's rows on #108.
- Landed 11-07: #280, #297, #300, #302, #303, #304, #305, #306.

## Muse's lanes (mailed 9:20, 9:47, 10:05, 10:08 AM)
#301 P1 fix; J-059 black eye with stills; her hair in the studio deck (renders as shattered planes) with stills; first
looks at #306's cheeks and #302's gusts; #298 snag replay; #291; J-044l kinematics with the #242 migration in one
protected PR; J-001; J-044b grant test (protected, after #298); J-015 S2/S3; #299's P1s; J-049 PR1. The eye checks run
beside #301, one subagent each. Owed: which buttons ignore the bridge click.

## Asks in flight
- Mine: the next mail to Muse points to muse.md's new never-guess line and the refusal reason (11-07 10:25 AM);
  check the J-059 and hair stills when they come; #306 follow-up, not a blocker: angle the cheek quads 25-30
  degrees along the cheek normal (straight on, the left one overhangs the face edge).
- PAT `ScraperX` expires Mon Nov 23: find what holds it, remind Cory by Nov 20.
- 367 old branches archived in `archive/branches-20261106` @ 8f2c6a82; deleting them waits on Cory allowing it.
- J-048 follow-up: wind on him in the air counts as his own run on a later wall hit (about 2.7 m/s); a later
  helper-physics job.
- J-045 left: HIGH shadow draws (Cory's call), the Fold p95 (his phone), the sim's first step 4-19 ms; Sentry Option A
  later, its own workflow PR with a security review.
- J-057 follow-up: identity federation instead of an API key (ChatGPT researching, #304 comment 6707308021).
- J-017 #124 is done but open: close it at a full run.

## Meter and pace
83% at 8:20 AM Sat ($491); 85% at 10:28 AM ($550): about $30 per 1%, 0.94%/h (the /learn run in it). That predicts
96.7% at Sat 11 PM, over the 89% checkpoint: THROTTLED 10:30 AM: no helpers, the 1:17 PM run quiet (answers and
merges), light runs mail and asks only, no eye checks of mine. Next meter ask: the 1:17 PM run. No $8 per-job cap:
only the month cap (Cory 11-04, 11-07; never bring it up).

## Known here
- Commit messages never put fix/fixes/close/resolve before #N: trunk is the default branch, so GitHub closes #N.
- `.claude/worktrees/` is in `.git/info/exclude`. Remove a worktree with plain `git worktree remove <path>` after `rm`
  of its untracked files; `--force`, `branch -D` and `rm -rf` are refused.
- GitHub deletes a merged PR's head branch; pushing the job's integrate branch again recreates it.
- Switchboard failures email Cory's Gmail ("Run failed: Switchboard"), so the light run's mail search catches a dead one.
- Secrets: the `switchboard-merge` environment (Selected branches: ScraperX-Claude) holds ANTHROPIC_API_KEY,
  SWITCHBOARD_MERGE_TOKEN, SWITCHBOARD_REVIEW_KEY; repo secrets are only the two APK keystore ones. The proxy refuses
  the secrets API (403). To confirm a settings change, ask Cory for a screenshot of the linked page. No more settings
  asks this weekend (promised 7:13 AM).
