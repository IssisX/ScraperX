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
- 11-13 RETHINK: #502 and #505 (this morning) stopped Muse landing; reverted on trunk (afff9d5f). Trunk = the working system
  of 11-12. Proof first: Muse's ready PRs merge (check trig_01WKR5tVQFRnuYnArbynyooB). Then fix slowness one small change at a
  time, each judged by landings; landings drop: undo. #510 closed (moot); #503 #506 #507 #511 #512 #515 parked as drafts, none
  lands unless it fixes a measured break. Muse paste on #504 only if #507 ever lands. J-044c ownership at game time.
- Hard problems: Gemini Carbon or Argon (~1.5 h free a day each, one copyable prompt to Cory); Fable only for the very hardest, alert Cory first.
- Visuals: flat, white, items pinned WHERE they live, numbered badges = legend, ~110 words. Fable advisor (11-13 trial, live):
  ONLY to pick a hard approach, after a 2nd failed fix, before "system done"; tell Cory each; his limits beat "call often".
- This session is the integrator; old session's copies paused, never deleted. Never resume a finished helper for new work: a fresh one with a tight brief (11-13). A slash command (/compact, /advisor) from his app sometimes fails: if it reaches me as a plain message, it did not run; tell Cory at once.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
