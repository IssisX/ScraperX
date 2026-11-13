# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794589446 (2026-11-13 17:06Z mail+asks)
## Now (11-13)
- System first (Cory): one machine, finished fast and at peak, then this session game-codes as THE priority; Claude and helpers
  take the HARD game jobs; Claude's game jobs held in the plan until then (worktrees kept); Muse keeps working.
- Cory 11-13: manage it to the end, ONE machine not parts; DRIVE, never coast (after a compact: act on the next blocker first, never just "waiting"); remove failure modes, never relocate; never stop a helper past
  10 min; check all he says, push back on waste. On done: do NOT start game code. Phone push + emoji banner "🚨✅ SYSTEM COMPLETE ✅🚨", then wait for his go.
- THE PLAN: `03_EXECUTION/SYSTEM_PLAN.md` (Fable max). Its section-5 order, one helper at a time (I pick the effort, tell him in one line);
  done = the real test passing (Cory), not 3 days. NOW: step 4 helper (Opus high, claude/integrate-plan-step4): on report open its PR. Step 3 is PR #528. Step 2 is PR #527 (watch: an
  empty-diff PR from a trunk-merge-only head). Step 1 is PR #526 (rules already say it, Cory yes);
  after it lands watch #488 #523 merge, then step 2. Trunk green at 62ea2833 after two green-alone collisions. Paid reviewer spent to 12-01. Later list: eye screenshots kept 1 day (#520 #522 failed on 2-day heads; proofs re-run).
- System work (Cory 11-13): ONLY Fable's plan, in order. Off-plan: a measured break of Muse landing gets one small fix, told in one line; all else goes on a later list, no asks to Cory.
- Cory 11-13: reviews: Codex first; once Codex is out of credits a Claude review counts, Claude's own work too (switch then). Security: only his baseline (untrusted input, no secrets, bound sizes); more needs a likely real threat.
- Hard problems: Gemini Carbon or Argon (~1.5 h free a day each, one copyable prompt to Cory); Fable only for the very hardest,
  alert Cory first. A Fable design pass writes a detailed plan with every decision its own (11-13). J-044c ownership at game time.
- Visuals: flat, white, items pinned WHERE they live, numbered badges = legend, ~110 words. Fable advisor (11-13 trial, live):
  ONLY to pick a hard approach, after a 2nd failed fix, before "system done"; tell Cory each; his limits beat "call often".
- This session is the integrator; old session's copies paused, never deleted. Never resume a finished helper for new work: a fresh one with a tight brief (11-13). A slash command (/compact, /advisor) from his app sometimes fails: if it reaches me as a plain message, it did not run; tell Cory at once.

## Traps
- My PR merges and branch deletes are refused by the safety check; never work around it. Direct trunk pushes of tooling and docs work.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
