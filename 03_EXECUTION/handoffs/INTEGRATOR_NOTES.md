# Integrator notes (live state only; re-read after every /compact)

Session `session_01W3SSMz5zMw3Tm5DXRpisrG`, chain 7 of 8; compacts itself at about 400k; a new session only if broken.
History, done jobs and old rulings: `INTEGRATOR_NOTES_LOG_2026-11-05.md` (the full notes up to 2:15 PM 11-05, verbatim).
Keep this file to what changes the next action; move anything done to the log.

## After every compact
1. Re-read `CLAUDE.md`, `work/tasks/integrator-hourly.md` and this file.
2. `python3 tools/board/asks.py --hours 24`; clear every line not under "Asks in flight".
3. Muse's mail: `subject:MUSE_TO_CLAUDE newer_than:3h`. Its diffs come as attachments, which the Gmail tools do not
   return, so a helper decodes them from the RAW message.
4. Hourly task `trig_0139GQPXpF7YWbvHwgEn98a3` at :17 is the only timed wake (no check-ins, Cory 11-05).

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
- Helpers: Opus high or xhigh, never Sonnet; at most 2. Now one at a time, so the weekly limit lasts to Mon 11 AM.
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
- J-046 #259 merged 7:50 PM (switchboard). Mine next: add matron-ammo to kScenarios after #256 lands. New leaks mailed
  to Muse for J-044l (crate shove in walk_toward 8 kN, knee 5 kN, let-go 6.9 kN); asked whether the Turn lunge stays exempt.
- #258 merged 9 PM (switchboard).
- J-045 #260 (perf_walk + kit pose loop, fd0638a6) opened 10:23 PM; switchboard merges; unsubscribed (the hourly sweep
  shows a Codex P0/P1). Helper retired. No helper running. J-045 left: native kit-pose batch getter, idle skips for her
  view/gore/canopy, the shadow call (Cory), perf_walk on the Fold; then Sentry native crash capture (its own PR).
- J-045 #258 (draws) opened 5:40 PM; switchboard merges it (no workflow file); unsubscribed. Red on the floating audit
  (LOAD label vs merged ticks); fixed by me at d63e8ab9 (audit off_board 0; perf_grade worst_draws_low 700 = budget, no
  headroom left). Foot at HIGH shadows still ~930 draws: look call put to Cory (LOW / SPLITS / KEEP). Waits on him.
- J-045 after that: walk p50/p95/p99; a gore warm-up; Sentry native crash capture (Option A), its own workflow/signing PR
  with a security review.
- Retired: the J-042 builder, J-042 fix, J-044m, J-045 draws, J-046 and J-045 walk helpers.

## Muse's queue (mail only)
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

Claude lanes later: J-044h (presentation), i (injury render), j (foley/voice: real recordings only), n (demo).

## Asks in flight
- To Grok on #244 (J-044b math). If Grok stays silent, route math to ChatGPT.
- To Claude: none.

## Known here
- Commit messages never put fix/fixes/close/resolve before #N: trunk is the default branch, so GitHub closes #N
  (11-05: my notes commit "fix #260 time" closed PR #260; reopened 11:20 PM).
- `.claude/worktrees/` is in `.git/info/exclude`.
- Stale temp worktrees in the scratchpad (`tmp.C1BZusDQkr/*`) are from guard PRs that have merged.
- The godot MCP connects since the 6 PM container restart (MCP_TIMEOUT=120000 on the environment took). Use it for eye checks.
