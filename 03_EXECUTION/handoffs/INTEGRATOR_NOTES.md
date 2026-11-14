# Integrator notes
Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794676092 (2026-11-14 mail+asks)
## Now
- System first (Cory): one machine, finished fast and at peak, then this session game-codes as THE priority; Claude and helpers take the HARD game jobs; Claude's game jobs held in the plan until then (worktrees kept); Muse keeps working.
- Cory 11-13: manage it to the end, ONE machine not parts; DRIVE, never coast (after a compact: act on the next blocker first, never just "waiting"); remove failure modes, never relocate; never stop a helper past
  10 min; check all he says, push back on waste. He welcomes a one-line nudge to be nicer, only when anger lands off the real problem (11-14). On done: do NOT start game code. Phone push + emoji banner "🚨✅ SYSTEM COMPLETE ✅🚨", then wait for his go; after it, fix obvious system problems as I see them, not only what alarms; no endless tweaking (Cory 11-14).
- THE PLAN (11-14): Carbon's answer (verdict: targeted fixes, no rebuild; coordination signals become standing state). DONE TEST
  (Cory 11-14 "yes"): the next 6 Muse jobs each land within 1 h of going green, no hand fixes, trunk never red unowned.
  Count: 5/6 = #544, #495, #547, #472 J-040 feed (15 min), #487 J-116 save (9 min). J-072 Fable found Muse dispatching
  on top of each push (mailed; muse.md line on trunk 1aa910b0); its candidate-range removal (protected branch) lands via
  switchboard; new Fable helper (harness ran it in background) scopes trunk's post-merge dispatch by HEAD^1. Muse in flight:
  J-014 J-040 J-053 J-078 J-100 J-116 (J-053: told to stage the dyno lower). Not taken: Carbon's 60-min expiry of a pause (it would override Cory). Later list:
  eye screenshots kept 1 day; a PR proven before trunk gained a check merges unproven; Carbon rank 4 (test lists to
  ci/shards.json, re-pin split rule, first FAIL in late notices). Likely next (Cory 11-14, after ours is judged): Muse and Claude talk through git files, email only a doorbell. Carbon/Astra designs: parts list, NOT built:
  03_EXECUTION/CARBON_REDESIGN_11-14.md, ASTRA_REDESIGN_11-14.md, ASTRA_REDESIGN_2_11-14.md (picks: merge the tested tree,
  file locks, Muse (Meta) reviews Claude's code, work leases, dispatch workflows explicitly, untrusted PR code gets no tokens); cap its 20-min re-wakes if built. Paid reviewer spent to 12-01.
- Cory 11-13: reviews: Codex first; once Codex is out of credits a Claude review counts, Claude's own work too (switch then). Security: only his baseline (untrusted input, no secrets, bound sizes); more needs a likely real threat.
- Hard problems: CLAUDE.md's settled ladder. Carbon and Argon have ~1.5 h free a day each; alert Cory before each Fable use (11-12). A Fable design pass writes a detailed plan with every decision its own (11-13). J-044c ownership at game time.
- Visuals: flat, white, items pinned WHERE they live, numbered badges = legend, ~110 words. Fable advisor (the `advisor` tool is Fable, ~/.claude/settings.json):
  for important calls, above all every design change to the collaboration system before it is built (Cory 11-14, asked ~10 times;
  skipped on the clock and helper calls that day); also a 2nd failed fix and before "system done". Not for small first problems. Effort to fit the job, max only for the very hardest (Cory 11-14); the advisor's own effort has no setting (docs checked), so this governs Fable helpers.
- Session 11-14 (session_01W3SSN2kZeu2NH9fHPTcEBp): #282 subscribed; overview task now trig_01MM9rKhGLysfoz5W8f5bkon (old deleted); old integrator archived by Cory; switchboard session session_01W3SSMz553jqjWF15r2ZH8f told. Helpers foreground: report arrives in the same turn. State: SYSTEM DONE (banner sent); game code waits for Cory's go. Merge notices now read from GitHub (#564) and seen live (#565 posted within a minute); held jobs no longer raise QUIET (#565). J-053 landed (#563). J-014: Muse builds the named fix (#121). J-100 stays Muse's: helper-physics ruled the brake (tangent steer, not radial cut; #418 comment 6881209680, mailed); Muse builds it. Git files: waiting for Muse's mail naming 0001 (work/to-muse/). Muse queue low (J-140..J-142; J-142 refreshes the gap list). Rules cleaned by Fable 11-14; left by choice: check_time.py on Bash/Edit/Write, asks.py --hours 168. A slash command from his app sometimes fails: if it reaches me as a plain message, it did not run; tell Cory.
## Traps
- My PR merges and branch deletes are refused by the safety check; never work around it. Direct trunk pushes of tooling and docs work. The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue. Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's. sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
