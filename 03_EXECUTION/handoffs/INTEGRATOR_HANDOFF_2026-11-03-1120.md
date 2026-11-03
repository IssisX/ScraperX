# Integrator handoff, 11-03, 11:20 AM Central

From `session_01W3SSN2XCUdfNoPWyFD2tJL` (346k, $14.82) to a fresh integrator, handed off before the
11:17 hourly check so the check runs in a small session. Read `CLAUDE.md` (today's rules are in it),
then this file, then do the hourly check (`work/tasks/integrator-hourly.md`). Nothing else unless an
event needs it.

## What changed this morning (all in CLAUDE.md and work/tasks/)
- **Prevent the growth.** Every Claude session checks its own size after each commit, proof or big
  read; it writes a handoff at 300k and stops at 400k. You check every worker's size on each event you
  handle. Logs go to a file, and you read only the PASS/FAIL lines; big reads go to a subagent.
- **Tasking the team is your job, unasked** (hourly step T). Muse is the strongest builder and is free:
  keep it in grand tasks, and rule on its proposals within the hour.
- The hourly prompt is a pointer: change `work/tasks/integrator-hourly.md`, never the prompt.
- #3 is closed. The board (#107) body was rewritten at 10:40 AM. Replace its "This hour" section each
  check; never post it as a comment.

## Claude sessions (2 workers allowed; a finished one may be replaced on the same model)
- `session_01W3SSMzEJS6CgEiSuViQkHm`, Opus, J-032 (#157), started 11:02 from
  `03_EXECUTION/handoffs/J032_HANDOFF_2026-11-03.md`. Its effort level could not be set (API default);
  Cory may set it. Open item: the stall after a tucked riser catch, asked of Muse by mail at 11:01.
- `session_01W3SSMzLpbNubHcNNTBee63`, Sonnet, #150's FIXes, pushed `083cf22`, waiting on review.
  Retire it when #150 lands.

## Open, and what is next
- **#159** (J-031 session watch): ChatGPT LAND @ `e5fd9c42`, proof was running. Merge on green
  (no workflow files). Its old session is archived; if it gets a FIX, a fresh Sonnet session takes it.
- **#153** (J-002 S1): needs only Muse's fix (one hit sound per blow; mailed 10:27). #162 and #163 merged.
- **#152** (J-001, Muse's): Muse has the trace and ChatGPT's next step (mailed). The `To Grok:` from
  9:28 waits for Grok's 4 PM run.
- **Muse's queue** (`work/tasks/muse.md`): J-001, the #153 fix, fight S2-S5 (#164-#167), S6 (#111),
  J-038 destructible loose things (#169, after the fight), the grade ladder J-006 (#113, PR #91, on a
  subagent), lane D, the stills. Reserve G1: her mind runs on evidence. Cory's rulings on J-038: loose
  things only (cargo, barrels, boxes, crates, shipping containers); the tower's machines are never
  destructible; a ram moves and dents; slice 0 makes the containers movable as a protected PR.
- **J-037 (#168)**, the HUD washes out over snow and crane: unowned; give it to the next Claude session
  free for eye work.
- **The sweep** takes over 2 minutes: run it in the background. The old QUIET/FIX-CI branches
  (#14, #29, #58, ...) are J-030's (#137). Step 0 (dispatch the switchboard) is still needed: 0 schedule
  runs so far.
- The switchboard did not merge the green+LAND PRs #141/#142/#144/#161; I merged them as the
  backstop. Check whether it merges #159 on its own.

## Moving in
1. `get_trigger trig_01XSFNLGb5vjQ2ZsL8r6C7bt`; create the same prompt and cron bound to you
   (initiation human_schedule); delete the old one.
2. No PR subscriptions to move (none open that the integrator must merge).
3. Archive `session_01W3SSN2XCUdfNoPWyFD2tJL`.
4. Do the hourly check now. First message to Cory: one line saying you took over, then
   `🟣 **YOUR MOVE:** Nothing to do.`
