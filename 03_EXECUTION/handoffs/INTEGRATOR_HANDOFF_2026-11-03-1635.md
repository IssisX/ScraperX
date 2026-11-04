# Integrator handoff, 11-03, 4:35 PM Central

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

## Resumed 11-04, 10:14 AM Central, with new orders from Cory (all in CLAUDE.md)
- **One review mechanism per PR:** Codex Code Review reviews every PR on open; `@codex review` only after a fix commit
  that needs a fresh pass. The Claude API takes the eye, the security pass on CI/signing changes, and any PR Codex has not
  reviewed at its exact head. ChatGPT and Grok are no longer asked for PR reviews. The switchboard's `review.pool` still
  assigns ChatGPT/Muse/API reviews: change it to match (Codex primary, API fallback) as a J-031 PR.
- **Concurrency:** one Claude Opus session at a time (the integrator), at most two active subagents (three for acute work).
- **Muse heavily:** Muse runs parallel agents of its own; it was tasked at 10:15 AM with J-001 #193 fixes, J-033 #179,
  J-034..J-036 fight S3-S5, the J-038 design proposal (rule within the hour), and the J-016 drum follow-up. Its "agents
  started" reply and each job's diff come as MUSE_TO_CLAUDE; push diffs via a worktree subagent and open the PRs.
- **Cost of Claude API calls (measured 11-04 from `switchboard/ledger:work/archive/runs.log`; metering from #139 and #159):**
  eye, Opus medium: 5 calls, mean $0.093. Review, Sonnet xhigh: 6 calls, mean $0.137 (mean 11,046 output tokens; #150's
  stopped at the 16,000 max_tokens with no verdict). Review, Opus high: 1 call, $0.092 (1,690 output). Month to date
  $9.38 of $100: $1.38 measured plus an $8.00 orphan reserve from API-agent session #136, which the watcher never closes
  because it reads only open issues (switch.py:811). Price table: work/budget.yml, Opus $4/$20 and Sonnet $2/$10 per MTok.
  Cache tokens are not counted, and none are used. Integrator's pick, put to Cory: switch the API review to Opus high
  (about $0.09 a review, roughly 20 a day affordable). J-031 PR to do: close orphan reserves for closed issues, and
  raise the review max_tokens or move off Sonnet xhigh.
- **Codex review rules: PR #195** (`claude/integrate-review-guidelines`, bfc10897): `## Code Review Rules` in AGENTS.md
  (OpenAI's current name for the section) plus a new SECURITY.md. Codex reads AGENTS.md from the PR's own code, so main
  needs nothing for reviews (main's settings-page links stay 404 until someone puts the files on main; cosmetic). Docs:
  https://learn.chatgpt.com/docs/third-party/github ("Codex flags only P0 and P1 issues"). SECURITY.md is documented only
  for Codex Security scans; PR security review uses a threat-model file only if one is set in the settings. Merge #195 on
  Codex clean (it is docs only, `[skip ci]`).
- Resume steps done: hourly task on, 20-minute sweep re-armed, board and Muse told, #134 `needs:orchestrator` back,
  J-017 worker resumed (#183: re-check the pin after #192/#191 landed overnight, then #185). **Switchboard: Cory was asked
  to re-enable it** (Actions -> Switchboard -> ... -> Enable workflow); `gh api repos/IssisX/ScraperX/actions/workflows/switchboard.yml -q .state`.
- Overnight the switchboard merged #192 (5:22 PM) and #191 (1:08 AM); trunk proof green on both.

## For Cory
- Nothing. He merged #187 (switchboard idle runs on `main`) at 4:36 PM Central, so the review, eye and sweep runs
  are no longer displaced by comment runs.

## Merged today since 2:35 PM
#178, #181, #184, #180, #188, #176, #182, #187 (Cory), #186, #189, #190.

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
