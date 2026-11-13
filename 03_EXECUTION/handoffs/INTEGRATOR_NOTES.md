# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794589446 (2026-11-13 17:06Z mail+asks)
## Now (11-13)
- System first (Cory): one machine, finished fast and at peak, then this session game-codes as THE priority; Claude and helpers
  take the HARD game jobs. Until then Claude's game jobs are held in the plan (J-001 J-015 J-084 J-090 J-093 J-096 J-109 J-137,
  J-044c; worktrees kept); Muse keeps working. Done = a deliberate real-world test of every path, not a day of waiting.
- Cory 11-13: manage it to the end, ONE machine not parts; DRIVE, never coast (after a compact: act on the next blocker first, never just "waiting"); remove failure modes, never relocate; never stop a helper past
  10 min; check all he says, push back on waste. On done: do NOT start game code. Phone push + emoji banner "🚨✅ SYSTEM COMPLETE ✅🚨", then wait for his go.
- THE PLAN: `03_EXECUTION/SYSTEM_PLAN.md` (Fable max, 11-13). Run it in its section-5 order, one helper at a time
  (ask Cory the effort first), then the real test; done = that test passing (Cory 11-13), not the plan's 3 days. Today items done (#517 #518
  drafts; #520-#525 opened). Paid reviewer's month spent ($98.24/$100) until 12-01. #502/#505 reverted (afff9d5f).
- System work (Cory 11-13): ONLY Fable's plan, in order. Off-plan: a measured break of Muse landing gets one small fix, told in one line; all else goes on a later list, no asks to Cory.
- Security (Cory 11-13): only his baseline (untrusted input, no secrets, bound sizes); more needs a likely real threat.
- This session is the integrator; old session's copies paused, never deleted. Never resume a finished helper for new work: a fresh one with a tight brief (11-13). A slash command (/compact, /advisor) from his app sometimes fails: if it reaches me as a plain message, it did not run; tell Cory at once.

## Traps
- My PR merges and branch deletes are refused by the safety check; never work around it. Direct trunk pushes of tooling and docs work.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
