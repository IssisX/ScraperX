# Integrator handoff, 11-03, 5:35 PM Central

From `session_01W3SSN2Y8JGNMvmxc394dkf` (written at 516k; the line is now 700k, note at 600k) to the next integrator.
Read `CLAUDE.md`, then steps K, D, S and C of `work/tasks/integrator-hourly.md`, then this file; run
`python3 tools/board/asks.py --hours 24` and clear every line not covered below. Re-arm the 20-minute self check-in
(step D, "Between hours") on your first turn, and move the hourly scheduled task (`trig_01BdP4yuyP2yAqoUErjiMSyn`).

## Cory's rulings this afternoon (all in the repo)
- Session size: handoff note at 600k, stop at 700k ("Yes, go with your pick").
- Workers start the next job while CI runs, always.
- Nothing builds up: a replaced PR is closed by its owner the same hour; idle 24 h goes to its owner, then the integrator.
- A 20-minute self check-in between hourly checks ("if I'm not here to prompt you ... things will pile up").
- He judges catch-up reading at a session's start as normal, not its working pace.
- Muse will do destructible-environment work: that is J-038 (#169), queued behind the fight.

## For Cory (the one thing only he can do)
- **Merge #187** (switchboard on `main`: idle comment/label runs get their own concurrency group). The session's safety
  checker refuses Claude's push to `main`. Trunk's copy (#188) is merged; selftest green; Codex clean on both.
  Until #187 lands, comment runs still displace the switchboard's review, eye and sweep runs: dispatch
  `switchboard.yml` (main, dry_run=false) or post a `: NOTE` line on a PR to nudge one.

## Merged today since 2:35 PM
#178, #181, #184, #180, #188, #176, #182.

## Merge queue (integrator; merge from a scratchpad worktree, never in the main checkout)
- **J-017 (route above 221 m), worker `session_01W3SSN2utVR8V9YiKAfLJee` (Sonnet xhigh, ~475k):** #183 (dd74a1d,
  trunk merged in, E-guard stock hash re-pinned 13642769397912826673 -> 17708029113149545001; CI running) -> #185
  (re-pin again after #183) -> #186 -> #189 -> #190 -> #191 (stacked: merge trunk in after each parent) and #192
  (Muse's chained exit check, standalone test PR; Codex P1 fixed at b4f4edf). Protected: green proof plus a clean
  review on the exact head; `@codex review` re-asks Codex. Its route is fully built; when the last PR lands, retire it.
- **#193 J-001** (Muse's hold fix on #102's line, `fb-bot/integrate-j001-hold`, d9d3ee5f): Codex P1 (the swat and
  sweep are not charged to her force budget, `physics_world_grab.cpp:369`) and P2 (target velocity from the pre-clamp
  pose, `matron_hold.cpp:1240`) were mailed to Muse at 4:28 PM; push Muse's fix diff to that branch when it comes.
- **#150 J-025** (steel): trunk merged in (8e68589d) so its eye re-checks after #176.
- **#100** (Grok's drawn-body checks): red because the J-032 shoulder push reads as a stretch. Grok was asked to fix the
  measure; ChatGPT added: split socket displacement from segment length, and re-baseline grip/plant/skirt only with a trace.

## Asks in flight
- **Muse (mail):** #193 Codex fixes (4:28 PM); #97 ChatGPT's pre-sort trace request (3:49 PM); J-038 prep heads-up
  (no reply needed). Answered this afternoon: J-001 fix (now #193), J-017 physics (posted on #124), J-016 drum
  (posted by the bridge on #123; follow-up queued for Muse).
- **Grok (4 PM run):** #100 measure; #169 dent model; #137 the #58 verdict; #152; #83; #116.
- **ChatGPT:** J-030 (closed #78, #79, #81; owners/conditions for the rest on #137); J-038 research (#169); #183
  review (switchboard).
- **API agent:** J-027's `touch_matron_pipe` into CI (#134, `needs:orchestrator`).
- **To Claude:** none open.

## Parked for Cory
- Making the grappling hook required (it would replace 66 m of ladders at 440-506 m and change the game's flow).

## Known here
- A subagent that edits code runs with `isolation: "worktree"`; `.claude/worktrees/` is in `.git/info/exclude`.
- The safety checker refuses: pushes to `main`, bulk closing other agents' PRs, archiving a stopped session.
- The session chain is 6 deep of 8: the next integrator (7) can start one more session; after that Cory starts one.
