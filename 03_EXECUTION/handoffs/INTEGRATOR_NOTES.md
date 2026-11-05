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
- Never weaken a test; never cut quality to save quota.
- Every bug Cory reports gets a job the same hour.
- Hourly step 6: look every hour; build a saving only when a real one shows up; never invent work.
- Fight (J-044): jointed upper body inside her 8 kN / 44 kW budget; legs and capsule locomotion kept.
  - Injuries cost function in proportion to damage, and her healthy limbs take over; her total budget never rises.
  - Injuries are saved with the checkpoint; a checkpoint taken after her death restores her dead.
  - A daze from head impulse replaces the fixed knockdown timer.
- J-042: one path for every loose object; she throws up to 25 kg and shoves anything heavier.
- Picks told to Cory (stand unless he says otherwise): the edge-first pallet arm break stays; the extinguisher is placeholder.

## Open PRs (2:25 PM)
- #250 protected magnet kit-tail (9829699d), green and Codex clean. The switchboard merges it; it lands BEFORE the ammo PR.
- J-042 ammo: moving from #251 to `claude/integrate-protected-j042-ammo`, since it carries the E guard re-pin (Codex P1, scope
  red). It can't split: the pin hashes the new world (precedent #216).
  - Fix helper a84d8f9 is moving the pallet and extinguisher off the deck-154 test lane (matron-sweep was red).
  - Then open the new PR (Job: J-042, protected, before/after in the body), close #251, and reply on its Codex threads.
  - Workflow file, so I merge it.
- #252 J-044m (ac62fa18, trunk merged in, CMake list = trunk + perf-step). Claude API LAND and Codex clean. I merge on green.
- #253 J-045 part 1 (f61f728c, stacked on #252). After #252, merge trunk in, then merge.
- #254 Muse's sense_near (c7f80b35, trunk merged in) and #255 Muse's guard self-test: the switchboard merges both (unsubscribed).
- #95 J-023 plants: red, plant worst 0.111 m > 0.079. Muse fixes it; no bound raise. Workflow file, I merge after the stills.
- #227 J-001 HOLD: Muse's past-rail agent (Spare waits until he is clear of the y 153.9 body). Rulings: force-based snag
  break only (Codex P0, grab.cpp:584); both hands' haul comes out of her one 8 kN/44 kW account (P1).
- Subscribed: #251, #252, #253.

## Helpers
- Running: the J-042 fix helper (a84d8f9, in the J-042 worktree).
- Next, once it is done (one at a time): re-create the J-045 draws helper. The 2:17 PM container restart lost it.
  - Its partial edits are uncommitted in `.claude/worktrees/agent-a64754fb0b3815142` (ui_test_act.gd,
    ui_test_driver.gd); branch `claude/integrate-j045-draws` from #253.
  - Job: cut the full-height views (2890 draws at the slingshot, 2845 at the tower foot) to the 700 budget, rendering
    only, checked by eye.
- J-045 after that:
  - walk p50/p95/p99;
  - a gore warm-up;
  - Sentry native crash capture (Option A): its own workflow/signing PR with a security review. It needs a Gradle
    export, a sentry-android plugin or addon, and the DSN from a CI secret.
- Retired: the J-042 builder and J-044m helpers.

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
- `.claude/worktrees/` is in `.git/info/exclude`.
- Stale temp worktrees in the scratchpad (`tmp.C1BZusDQkr/*`) are from guard PRs that have merged.
