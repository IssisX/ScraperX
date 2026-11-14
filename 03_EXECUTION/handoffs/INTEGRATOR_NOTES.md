# Integrator notes
Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794658801 (2026-11-14 mail+asks)
## Now
- System first (Cory): one machine, finished fast and at peak, then this session game-codes as THE priority; Claude and helpers
  take the HARD game jobs; Claude's game jobs held in the plan until then (worktrees kept); Muse keeps working.
- Cory 11-13: manage it to the end, ONE machine not parts; DRIVE, never coast (after a compact: act on the next blocker first, never just "waiting"); remove failure modes, never relocate; never stop a helper past
  10 min; check all he says, push back on waste. On done: do NOT start game code. Phone push + emoji banner "🚨✅ SYSTEM COMPLETE ✅🚨", then wait for his go; after it, fix obvious system problems as I see them, not only what alarms; no endless tweaking (Cory 11-14).
- THE PLAN (11-14): Carbon's answer (verdict: targeted fixes, no rebuild; coordination signals become standing state). DONE TEST
  (Cory 11-14 "yes"): the next 6 Muse jobs each land within 1 h of going green, no hand fixes, trunk never red unowned.
  Count: 2/6 = #544 J-040 repin (8 min green to merged), #547 J-116 MAT1 (9 min). In flight: #545 quiet-asks, proof-unittests,
  carbon-fixes, delete-dead, board-roll. Not taken: Carbon's 60-min expiry of a pause (it would override Cory). Later list:
  eye screenshots kept 1 day; a PR proven before trunk gained a check merges unproven; Carbon rank 4 (test lists to
  ci/shards.json, re-pin split rule, first FAIL in late notices) after proof-unittests lands. Paid reviewer spent to 12-01.
- Cory 11-13: reviews: Codex first; once Codex is out of credits a Claude review counts, Claude's own work too (switch then). Security: only his baseline (untrusted input, no secrets, bound sizes); more needs a likely real threat.
- Hard problems: Gemini Carbon or Argon (~1.5 h free a day each, one copyable prompt to Cory); Fable only for the very hardest,
  alert Cory first. A Fable design pass writes a detailed plan with every decision its own (11-13). J-044c ownership at game time.
- Visuals: flat, white, items pinned WHERE they live, numbered badges = legend, ~110 words. Fable advisor (11-13 trial, live):
  ONLY to pick a hard approach, after a 2nd failed fix, before "system done"; tell Cory each; his limits beat "call often".
- This session is the integrator; old session's copies paused, never deleted. Never resume a finished helper for new work: a fresh one with a tight brief (11-13). Each helper report or mid-job message wakes me on this big context (Cory 11-13: expensive): whole brief up front, never message a running helper. Parallel helpers OK (Cory 11-13: fast), max four. A slash command (/compact, /advisor) from his app sometimes fails: if it reaches me as a plain message, it did not run; tell Cory at once.

## Traps
- My PR merges and branch deletes are refused by the safety check; never work around it. Direct trunk pushes of tooling and docs work.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
