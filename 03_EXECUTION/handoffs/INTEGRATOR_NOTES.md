# Integrator notes (live state only; re-read after every /compact)

Session `session_01W3SSMz5zMw3Tm5DXRpisrG`, chain 7 of 8; compacts itself at about 400k; a new session only if broken.
History, done jobs and old rulings: `INTEGRATOR_NOTES_LOG_2026-11-05.md` (the full notes up to 2:15 PM 11-05, verbatim).
Keep this file to what changes the next action; move anything done to the log.

## After every compact
1. Re-read `CLAUDE.md`, `work/tasks/integrator-hourly.md` and this file.
2. `python3 tools/board/asks.py --hours 24`; clear every line not under "Asks in flight".
3. Muse's mail: `subject:MUSE_TO_CLAUDE newer_than:3h`. Its diffs come as attachments, which the Gmail tools do not
   return, so a helper decodes them from the RAW message.
4. The check task `trig_0139GQPXpF7YWbvHwgEn98a3` runs every 2 hours, 7:17 AM-11:17 PM Central; it is the only timed wake.

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
  to Sunday night). Builds go to Muse.
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

## Open PRs (5:40 PM)
- Merged: #252 and #250 (by hand, 2:40 PM), #253 (by hand, 3:00 PM); #254 and #255 (switchboard).
- #256 J-042 protected ammo (5ea9f03e, trunk merged in). It replaces #251, which is closed. Workflow file, so I merge it on
  green plus Codex or the API LAND. Her pallet take is marginal: asked of Muse on #240 and mailed.
  HELD on Codex P1 (confirmed): the windup lift moves the take_stock constraint anchor and is not budgeted. The fix is
  Muse's (mailed 2:55 PM, ahead of J-044c): pushed to fb-bot/integrate-protected-j042-windup, then I merge it in.
- #95 J-023 plants: red, plant worst 0.111 m > 0.079. Muse fixes it; no bound raise. Workflow file, I merge after the stills.
- #227 J-001 HOLD: Muse's past-rail agent. Rulings: force-based snag break only; both hands' haul comes out of her one
  8 kN/44 kW account.
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
- Commit messages never put fix/fixes/close/resolve before #N: trunk is the default branch, so GitHub closes #N
  (11-05: my notes commit "fix #260 time" closed PR #260; reopened 11:20 PM).
- `.claude/worktrees/` is in `.git/info/exclude`.
- Stale temp worktrees in the scratchpad (`tmp.C1BZusDQkr/*`) are from guard PRs that have merged.
- The godot MCP connects since the 6 PM container restart (MCP_TIMEOUT=120000 on the environment took). Use it for eye checks.
