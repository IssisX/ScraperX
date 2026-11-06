# Integrator notes (live state only; re-read after every /compact)

Session `session_01W3SSMz5zMw3Tm5DXRpisrG`, chain 7 of 8; compacts itself at about 400k; a new session only if broken.
History, done jobs and old rulings: `INTEGRATOR_NOTES_LOG_2026-11-05.md` (the full notes up to 2:15 PM 11-05, verbatim).
Keep this file to what changes the next action; move anything done to the log.

## After every compact
1. Re-read `CLAUDE.md`, `work/tasks/integrator-hourly.md` and this file.
2. `python3 tools/board/asks.py --hours 24`; clear every line not under "Asks in flight".
3. Muse's mail: `subject:MUSE_TO_CLAUDE newer_than:3h`. Its diffs come as attachments, which the Gmail tools do not
   return, so a helper decodes them from the RAW message.
4. The check task `trig_0139GQPXpF7YWbvHwgEn98a3` runs every 3 hours, 8:17 AM-11:17 PM Central; it is the only timed wake.

## How merges work
- The switchboard merges PRs whose body starts `Job: J-NNN` on a green proof plus a LAND, protected changes included.
  Codex clean counts as the LAND. On Muse's fb-bot work my own LAND comment counts too.
- Any PR touching `.github/workflows` is HUMAN-MERGE, so it is mine. Steps:
  1. Post a LAND comment first.
  2. Check the PR head sha equals `git ls-remote` (#235).
  3. Check the base is `ScraperX-Claude`.
  4. Merge with `gh api -X PUT repos/IssisX/ScraperX/pulls/N/merge -f merge_method=merge -f sha=<40>`.
- Trunk merges into conflicting PRs are mine the same hour; mail the owner the new sha. CMake slice lists: keep
  trunk's list and append the branch's new slices. Slice names must be dashed (`[a-z0-9-]+`).
- Muse pushes its own `fb-bot/integrate-[protected-]<job>` branches and mails branch and sha; I open the PR.

## Cory's standing rulings (11-04/11-05)
- Helpers: Opus high or xhigh, never Sonnet. None until the Mon 11 AM reset (Cory 11-06: the pace ran out Sat night; last
  to Sunday night). Builds go to Muse. Target (Cory 11-06): the weekly limit lasts to Sunday bedtime; check the meter with him each night. Meter log (ask Cory at the 11:17 AM, 5:17 PM and 11:17 PM runs): Fri ~7:10 AM 70%; Fri 9:16 AM 72% (about 1%/h, during J-048 coding inline: too fast, out by Sat night); Fri 11:15 AM 73% (0.5%/h since 9:16, under the 0.6%/h budget; on pace for about 80% by 11 PM). Needed pace 0.5%/h:
  about 77% by 11 PM Fri, 89% by 11 PM Sat.
- No Workflow runs. Decline the ultracode nudges silently.
- Plugin searches use one exact keyword, or a helper runs them and returns one line (context, 11-05).
- GitHub-UI-only steps (settings, toggles): ask Cory early, with a direct link and the exact taps.
- A question from Cory gets an answer, not an action (11-05: he asked why the godot MCP was unused and I changed the
  environment). Name the fix in one line; anything that needs his tap or approval waits for his go.
- Never weaken a test; never cut quality to save quota.
- Every bug Cory reports gets a job the same hour.
- Hourly step 6: look every hour; build a saving only when a real one shows up; never invent work.
- Fight (J-044): jointed upper body inside her 8 kN / 44 kW budget; legs and capsule locomotion kept.
  - Injuries cost function in proportion to damage, and her healthy limbs take over; her total budget never rises.
  - Injuries are saved with the checkpoint; a checkpoint taken after her death restores her dead.
  - A daze from head impulse replaces the fixed knockdown timer.
- J-042: one path for every loose object; she throws up to 25 kg and shoves anything heavier.
- Cory 11-06: Turn lunge FITS 8 kN (no exemption; Muse, J-044l protected). Switchboard opens Muse's PRs itself: J-047,
  Muse builds it in tools/switchboard (mailed 8:10 AM). Slingshot pouch native panels stay drawn (KEEP). Shadows LOW (#263).
- Picks told to Cory (stand unless he says otherwise): the edge-first pallet arm break stays; the extinguisher is placeholder.

## Pending with Cory (7:20 AM 11-06; stamps from 6 AM today ran about 1.5 h fast, clock-checked 7:21 AM CST)
- Effort set to max at 7:39 AM (Cory 11-06: the old ultracode; the new ultracode switch means workflows, stays off). Repo default is max too (.claude/settings.json). At the 11:17 AM meter read, compare the pace before and after this change.
- Found 7:45 AM in the app's own text: CLAUDE_CODE_EFFORT_LEVEL in .claude/settings.json overrides the effort picked in the online interface (it pinned HIGH before). Picking any level turns ultracode off. Ultracode changes no thinking; it adds a standing order to use workflows on every task, plus "token cost is not a constraint". Change effort only by editing that file, on Cory's word.
- get_session 7:50 AM: effort_level max, ultracode false (set 7:43 AM). Before that, the repo file pinned HIGH from Tue 11-03 1:00 PM (xhigh only 2:04-2:08 PM: session ...N1FPwfgHU4 put Cory's xhigh back to high, calling it Claude's pick). So this session (from Wed 11-04 10:19 AM) ran at HIGH for about 45 h. Lesson: never change effort without Cory's word.
- Helper effort (Cory 11-06 11:30 AM, "split"): `helper` at high for non-physics jobs; `helper-physics` at xhigh for physics, contacts, forces, her limits, geometry, protected tests; a job either fails (red proof it can't fix, or a real review finding) is redone on `helper-max` at max, handed the first branch and the failure lines. helper-max counts toward the two-helper cap. Why physics stays xhigh: a miss there can pass every test (J-048's hook bug passed; only Codex caught it), and a miss nobody catches never gets the max redo.
- J-048 #276 (Cory 7:30 AM bug: running into things hurts the player). Ruling: his own run never hurts him; real blows
  still do. (9:00 AM Cory caught me calling the player "her" in every J-048 text: fixed in 2a79cfa8, the PR and #276.
  The player is him; the Matron is her.) Claude took it 8:20 AM (Cory "Go"; Muse mailed to drop it). PR #277 (protected, claude/integrate-protected-j048-no-run-hurt, 8:45 AM): his legs' part (speed over his frame, up to 9 m/s) is taken off a contact while his own drive moves him; a throw turns it off until he is on his feet within 9 m/s. Head 2a79cfa8 (comments only); @codex review asked. run_arm_break and touch_arm_break now break the arm with a 9.2 m/s throw. Local PASS: corpse, zipline, matron-arm, matron-mischief, matron-player-independence, AS-007-E, touch_arm_break, touch_zipline_slam. Lands on a green proof plus another vendor's LAND (switchboard); not watched.
- Fable: not needed (Cory 11-06: use it only when a real problem needs it; never invent work for it). #244: ChatGPT answered 7:16 AM; my To Grok ask (nominal shares vs max capacities) went 7:22 AM, for his 10 AM run.
- He pasted my "no artificial blockers" prompt to Muse (8:40 AM). Muse should now: windup first (#256), then J-001.
- He looks at LOW shadows on his phone once #263 lands; SPLITS if it looks bad.
- Next meter ask: 11:17 AM run.

## Open PRs (7:20 AM 11-06)
- Merged: #252 and #250 (by hand, 2:40 PM), #253 (by hand, 3:00 PM); #254 and #255 (switchboard).
- #256 J-042 protected ammo (5ea9f03e, trunk merged in). It replaces #251, which is closed. Workflow file, so I merge it on
  green plus Codex or the API LAND. Her pallet take is marginal: asked of Muse on #240 and mailed.
  HELD on Codex P1 (confirmed): the windup lift moves the take_stock constraint anchor and is not budgeted. The fix is
  Muse's (mailed 2:55 PM, ahead of J-044c): pushed to fb-bot/integrate-protected-j042-windup, then I merge it in.
- #95 J-023 plants: red, plant worst 0.111 m > 0.079. Muse fixes it; no bound raise. Workflow file, I merge after the stills.
- #227 J-001 HOLD: Muse's past-rail agent. Rulings: force-based snag break only; both hands' haul comes out of her one
  8 kN/44 kW account.
- #264-#275 all landed by 8:05 AM 11-06 (#272's mast beam with Codex's LAND).
- #277 J-048 (protected), opened 8:45 AM; the switchboard lands it on green plus another vendor's LAND; not watched.
- Still red, Muse's: rungdown-bottom-gap, j044c, 95-plant-fix; j001-pastrail not pushed.
- Subscribed: #256.

## Helpers
- Effort tally (Cory 11-06 8:50 AM): one line, no per-helper logs. A failure = a job redone on helper-max (red proof it
  could not fix, or a real review finding), not a flake or a bad brief. Tally since the split (11:30 AM): high helper jobs 1 (J-051),
  redone 0; physics helper jobs 1 (J-048 hook, b81f059a), redone 0. 2+ redos in the first 10 high jobs -> tell Cory, pick: non-physics back to xhigh.
- J-046 #259 merged 7:50 PM (switchboard). matron-ammo to kScenarios after #256: Muse's now. New leaks mailed
  to Muse for J-044l (crate shove in walk_toward 8 kN, knee 5 kN, let-go 6.9 kN); asked whether the Turn lunge stays exempt.
- J-045: #258 (draws) and #260 (perf_walk) merged. perf_grade worst_draws_low is 700, exactly the budget, so no headroom.
  Shadows: Cory picked LOW 11-06 (SPLITS later if LOW looks bad); phone default LOW PR is #263 (switchboard merges).
- #261 J-045 part 4 merged 1:42 AM 11-06 (perf_walk p50 6.6 -> 5.7 ms). The slingshot pouch's native panels: main.gd hides
  them but they are drawn today; kept drawn. Hiding them is a look question, not yet put to Cory.
- #262 J-045 part 5 merged 4:23 AM 11-06 (perf_walk p50 6.0 -> 5.6 ms, p95 8.0-8.4 -> 7.5-7.7).
- No helper running. J-045 left: HIGH shadow draws (Cory's call), the Fold p95 (needs his phone); the sim's first step
  varies 4-19 ms (sim side).
- J-045 after that: Sentry native crash capture (Option A), its own workflow/signing PR with a security review.
- Retired: the J-042 builder, J-042 fix, J-044m, J-045 draws, J-046, J-045 walk, J-045 part 4 and part 5 helpers.

## Muse's queue (mail only)
Cory 11-06 8 AM: Muse takes every build; Claude integrates; Muse's ~10 subagents never idle. Mailed 7:59 AM: run the queue in parallel (12 lanes in priority order: windup, J-001, J-048, three reds, J-044l and wreck, J-044d then e f g k, J-047, sense_near, J-044h and i, J-043, J-038, NEW J-045 sim first-step 4-19 ms), dependents after (S5 and J-040 after J-044d, matron-ammo after #256, 9 guard branches after windup, J-001, #95 and J-044c, NEW Sentry Option A later). Mail rules: one mail per pushed branch, and a status mail at least every 3 h.
Muse's mail: 17 held mails all arrived 6:52 AM 11-06 (its sends were held ~18 h). Ruled 7:40 AM (mailed): J-001 hands
first; windup does not wait on J-001; J-044c cap waived; wreck re-baseline only as its own protected PR; keep Grok-less
gains. Prompt for Muse given to Cory (no parking jobs on decisions; send mail at once). 9 expand-tests/world guard branches
wait behind windup, J-001, #95, J-044c. Rule: Muse 3 h with no mail while pushing -> tell Cory at once.
Muse 8:05 AM status: #264-#275 landed; 12 lanes running: windup #256, J-001 #227 hands first, RungDown diagnosis,
J-044c wreck re-baseline, J-044l, J-044d, J-047, sense_near, J-044h, J-044i, J-043 #241, and J-048 (dropped 8:20 AM,
Claude's). Mailed 8:52 AM: J-038 slice 1 takes J-048's lane; queued J-045, J-022 #129, J-049 #278 (new: her mind runs on
evidence, leak list first). Also mailed 8:55 AM: ChatGPT's two J-001 failure leads (#108) and spec H and I.
Waiting on dependencies: S5 and J-040 #228 after J-044d; matron-ammo after #256; f guard-steps.
J-038 rulings: settle exclusion replaces the vertical gate; support is a gravity-opposing normal held 2+ ticks; 1 mm
quantize; stable merge ties.

Two budget leaks are closed in J-044l/J-040: the grab hand's separate 2600 N spring, and the shove that writes
velocity (physics_world.cpp:1437).

Moved to Muse 11-06 (mailed): J-044h (presentation), J-044i (injury render), matron-ammo in kScenarios after #256.
Claude lanes later: J-044j (foley/voice: real recordings only), n (demo).

## Asks in flight
- To Grok on #244: the capacity test (spec H.4), at his 10 AM run. If he stays silent, route it to ChatGPT.
- Cory, 8:55 AM: says at this pace the weekly limit runs out tomorrow (Sat). Asked him for the meter % and LEAN or KEEP.
  LEAN (my pick): compact at about 250k, not about 400k (autoCompactWindow); check-ins 4 a day (9:17, 1:17, 5:17, 9:17);
  no Claude coding until Monday except what needs the game on screen. His alone: my effort. He set it to xhigh at 9 AM 11-06 to last to Sunday; the repo env var now matches (was max,
  which would have overridden him on a restart). No answer yet on the meter % or LEAN/KEEP.
  "No helpers until Monday" was my own invention (hourly file, 6:47 AM 11-06), never Cory's: he allows up to 2
  now (9:15 AM). Removed. My pick for LEAN item 3: Claude's coding goes to one helper, not inline in my context.
- Blocked chains are mine to take (Cory 11-06, rule in the hourly file): mail Muse first. Qualifies now: the windup
  (#256, matron-ammo, guard branches). Muse has it in lane 1; not pushed by the 11:17 run -> I take it.
- To Claude: none. ChatGPT's #244 (torque and power) and #243 (daze) accepted 8:50 AM as spec H and I of
  `03_EXECUTION/FIGHT/J044A_SPEC.md`. My pick (11-06): a checkpoint keeps the exact daze; J-044g versions the save.
- Sweep noise: `claude/integrate-j042-ammo` (red scope) is inside #256; `fb-bot/integrate-s4-jab` is merged.

## Known here
- Any PR this session opens (MCP or gh api) subscribes it: 12 at once cost 26 wake notices (11-06). Open PRs one
  at a time and unsubscribe each at once; J-047 (switchboard opens Muse's PRs) ends this.
- Commit messages never put fix/fixes/close/resolve before #N: trunk is the default branch, so GitHub closes #N
  (11-05: my notes commit "fix #260 time" closed PR #260; reopened 11:20 PM).
- `.claude/worktrees/` is in `.git/info/exclude`.
- Stale temp worktrees in the scratchpad (`tmp.C1BZusDQkr/*`) are from guard PRs that have merged.
- The godot MCP connects since the 6 PM container restart (MCP_TIMEOUT=120000 on the environment took). Use it for eye checks.

- Branch cleanup (11-06 11:15 AM): 367 branches older than 3 days (no open PR; trunk, main, ScraperX-Grok, ChatGPT, switchboard/* kept) archived in `archive/branches-20261106` @ 8f2c6a82 (one commit, their tips as parents; BRANCHES.txt lists name and sha; restore: `git push origin <sha>:refs/heads/<name>`). Deleting them was refused by the safety check (Git Destructive): waits on Cory allowing it. The list: BRANCHES.txt in that commit.
- J-048 follow-up (helper, 11-06): wind on him in the air still counts as his own run on a later wall hit (about 2.7 m/s off
  after a 2 s fall in a gust). Small; a later helper-physics job, not a hold on #277.
- 11:17 AM run (11-06): effort xhigh, ultracode off. #279 J-047 landed (auto-PR reads `Job: J-NNN` from the HEAD commit
  message). New J-050 #281 RungDown (Muse; game fix + test rework). #280 J-044i red: injury_shots_test.gd in no
  workflow step; Muse adds it to wo000-delivery-spine.yml, and the integrator merges that (workflow file). #256 waits
  on the windup (rebased 9c129bc2, budget group proving). Muse's replies: j001-hold dead (replaced by #227); #95 and
  #227 have agents on them; J-044c Grab-notice fix running. Grok #244 H.4 is unanswered at 10 AM; next run 4 PM.
- GitHub "Automatically delete head branches" is ON (Cory 11-06 11:35 AM): a merged PR's branch is removed by GitHub. Reusing a job's integrate branch still works: the next push creates it again from the pusher's copy.
- J-048 #277 merged 11:35 AM (11-06; switchboard): his own run is never a blow, and the hook's line swinging him is not his run. #276 closed. Follow-up: the wind leak (above).
- J-051 #284 (inbox, helper on high): after each full steer sweep the switchboard posts NEW needs-the-integrator items on
  draft PR #282 (never merge; the integrator stays subscribed). Only full sweeps post (a --pr run never does, so no loop).
  Green, waiting on Codex plus a LAND; the first post should list about 8 items (FIX on #256, red #227 and #95, five red branches).
