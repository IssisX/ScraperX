# Integrator handoff, 11-03, 2:35 PM Central

From `session_01W3SSN1FPwfgHU4Lk6auREp` (592k: past the 400k line; it should have handed off an hour ago) to a
fresh integrator. Read `CLAUDE.md`, then step K of `work/tasks/integrator-hourly.md` (how the agents reach each
other), then this file; run `python3 tools/board/asks.py --hours 24` and clear every line not covered below.

## Cory's rulings today (all in the repo)
- **Reviews: one reviewer per PR, whoever is open** (*"I'm not asking for two agents to review the same shit"*):
  the GitHub Actions Claude review (Sonnet 5.5 xhigh; Claude's PRs too, its LAND counts there), Codex, Muse,
  ChatGPT. A clean Codex review on the exact head is a LAND. This is `review.pool`, PR #178 (not merged yet).
- **No $10 API day cap**; only $100/month and $8/job (`work/budget.yml`, on trunk).
- **Effort: Opus sessions high, Sonnet xhigh.** `create_session` can't set it and the repo setting doesn't take:
  `get_session` 6 min after start; if off, tell Cory which session to change. Never send `/effort` yourself.
- **Say no to him; check before you answer** (*"do your checks first"*). He graded today's integrator a C for
  claiming things it had not checked and reading each sentence alone instead of his history.
- **Nothing sits 15 minutes:** check `MUSE_TO_CLAUDE` mail on every event (hourly step K); post Muse's mail on
  GitHub yourself when the bridge hasn't (it missed three today).

## Merge queue (subscribe to #100, #176, #178, #180)
- **#178** (switchboard review pool) @ `2e135e19`: Codex clean on that head; CI was running. Merge on green.
  Then the switchboard does reviews by itself (its 15-minute schedule fires since ~2 PM).
- **#176** (J-037 HUD contrast and touch-label wrap) @ `c1d656dd`: the eye passed the approach frame; its summit
  FIX (LOWER LAST BEAM into JUMP) is fixed in `c1d656dd`, checked by render. Codex asked. Merge on green + LAND.
- **#150** (J-025 steel): its eye FIX was the HUD wash-out; after #176 lands, merge trunk into it and let the eye re-run.
- **#100** (Grok drawn-body checks, changes a workflow so the integrator merges): trunk merged in at `7bfa4782`,
  proof dispatched 2:20 PM; ChatGPT LAND @ `6258ebae` carries (the delta is trunk only).
- **#180** (J-017 stage 352-374 wall-climb, protected, Sonnet worker): Codex clean @ `a25252a8`; merge on green.
- **#179** (Muse's S2 low sweep, J-033): Codex found two real issues (no vertical bound on sweep contact; no foot
  animation). Mailed to Muse 2:19 PM; push its fix diff to `fb-bot/integrate-j033-sweep` when it comes.
- **#171** (mine, draft, J-003 S8 on S7): Muse FIX: (1) athletic-traversal timed out, full re-run requested
  2:30 PM (re-run whole runs only: hourly step 6); (2) a lethal-collision demo with stills and a reload check is
  owed, after #83 lands (Grok reviews #83 at 4 PM).

## Claude sessions (two worker slots)
- `session_01W3SSN1tztErnCCGAZSUT51` Opus high, J-016 her wrecking (#123): rerouting a carry stall at z=-126.6.
- `session_01W3SSN35wEYLgGqKQKgmNDJ` Sonnet xhigh, J-017 route above 221 m (#124): plan approved (440-462 stays a
  ladder); #180 open; next 374-396.

## Asks in flight
- **Grok (4 PM run):** #152 J-001 second opinion; #157 the frame-154 fall (J-032 has since landed; still useful);
  #83 review; #116 the J-009 comparator, now with Muse's #97 answer.
- **Muse (mailed):** #179 fixes (2:19 PM); J-017 numbers on #124 (2:05 PM); J-001 fix on #102's hold line (its
  queue). Answered today: #97 comparator (posted 2:16 PM), #171 review (posted), S2 diff (applied as #179).
- **ChatGPT:** reviews #105, #106; #97 has Muse's answer (do not land #97 as is); J-003 S7/S8.
- **To Claude:** none open.

## Next
1. Subscribe to #100, #176, #178, #180; move the hourly task (`trig_01Xsgz1q3fyv7dQEc2WgueiG`: get_trigger,
   create_trigger with the same prompt and cron, initiation human_schedule, delete the old one); archive
   `session_01W3SSN1FPwfgHU4Lk6auREp`.
2. The API agent is idle and the day cap is gone: label one bounded job `needs:orchestrator` (step 5).
3. Hourly step 0 can be deleted: the switchboard's schedule now fires (2 scheduled runs by 2:17 PM).
