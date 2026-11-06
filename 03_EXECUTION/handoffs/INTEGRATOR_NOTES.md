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
  to Sunday night). Builds go to Muse. Target (Cory 11-06): the weekly limit lasts to Sunday bedtime; check the meter with him each night. Meter log (ask Cory at the 11:17 AM, 5:17 PM and 11:17 PM runs): Fri ~7:10 AM 70%. Needed pace 0.5%/h:
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
- Helper effort (Cory 11-06, 7:55 AM: "xhigh+MAX"): .claude/agents/helper.md at xhigh for every helper job; a job it fails (red proof it can't fix, or a real review finding) is redone on helper-max.md at max, handed the first helper's branch and the failure lines. No standing helper; helper-max counts toward the two-helper cap. Log per helper job: type, tokens, needed a max redo? (Cory: xhigh may cost less per finished job by avoiding redos; check after a few jobs.) Check both types are listed before the first Monday helper.
- J-048 #276 (Cory 7:30 AM bug: running into things hurts her). Cause kHitFeltSpeed 6 m/s < 8 m/s sprint. Ruling: her own run never hurts; real blows still do. Mailed Muse 7:35 AM, after the windup and ahead of J-001. Protected branch fb-bot/integrate-protected-j048-no-run-hurt. Add it to board #107 at the 8:17 run.
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
- #264-#275 opened 8:30 AM 11-06 from Muse's held-mail branches (all green, 0 BLOCK, clean; unsubscribed). #272 widens the cascade mast beam: geometry, needs another agent's LAND. Switchboard merges them.
- Still red, Muse's: rungdown-bottom-gap, j044c, 95-plant-fix; j001-pastrail not pushed.
- Subscribed: #256.

## Helpers
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
Muse's mail: 17 held mails all arrived 6:52 AM 11-06 (its sends were held ~18 h). Ruled 7:40 AM (mailed): J-001 hands
first; windup does not wait on J-001; J-044c cap waived; wreck re-baseline only as its own protected PR; keep Grok-less
gains. Prompt for Muse given to Cory (no parking jobs on decisions; send mail at once). 9 expand-tests/world guard branches
wait behind windup, J-001, #95, J-044c. Rule: Muse 3 h with no mail while pushing -> tell Cory at once.
Muse pushed `fb-bot/integrate-traversal-rungdown-bottom-gap` @ed8e32a3 2:47 AM 11-06 with no mail yet; red on
matron-slices-2. Wait for its mail.
1. J-001 #227
2. #95 plant fix
3. sense_near nearest-32
4. J-044c, then d, e, f, g, k, l
5. J-043 #241 (Belly of the Deck)
6. S5 after J-044d
7. J-040 #228 after J-044d
8. J-038 slice 1. Rulings: settle exclusion replaces the vertical gate; support is a gravity-opposing normal held
   2+ ticks; 1 mm quantize; stable merge ties.

Two budget leaks are closed in J-044l/J-040: the grab hand's separate 2600 N spring, and the shove that writes
velocity (physics_world.cpp:1437).

Moved to Muse 11-06 (mailed): J-044h (presentation), J-044i (injury render), matron-ammo in kScenarios after #256.
Claude lanes later: J-044j (foley/voice: real recordings only), n (demo).

## Asks in flight
- To Grok on #244 (J-044b math). If Grok stays silent, route math to ChatGPT.
- To Claude: none.

## Known here
- Any PR this session opens (MCP or gh api) subscribes it: 12 at once cost 26 wake notices (11-06). Open PRs one
  at a time and unsubscribe each at once; J-047 (switchboard opens Muse's PRs) ends this.
- Commit messages never put fix/fixes/close/resolve before #N: trunk is the default branch, so GitHub closes #N
  (11-05: my notes commit "fix #260 time" closed PR #260; reopened 11:20 PM).
- `.claude/worktrees/` is in `.git/info/exclude`.
- Stale temp worktrees in the scratchpad (`tmp.C1BZusDQkr/*`) are from guard PRs that have merged.
- The godot MCP connects since the 6 PM container restart (MCP_TIMEOUT=120000 on the environment took). Use it for eye checks.
