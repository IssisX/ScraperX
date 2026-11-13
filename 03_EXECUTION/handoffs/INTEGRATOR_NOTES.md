# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794584485 (2026-11-13 full check)

## Now (11-13)
- System first (Cory): one machine, finished fast and at peak, then this session game-codes as THE priority; Claude and helpers
  take the HARD game jobs. Until then Claude's game jobs are held in the plan (J-001 J-015 J-084 J-090 J-093 J-096 J-109 J-137,
  J-044c; worktrees kept); Muse keeps working. Done = a deliberate real-world test of every path, not a day of waiting.
- On done: do NOT start game code. Phone push + emoji banner "🚨✅ SYSTEM COMPLETE ✅🚨", then wait for his go.
- Open system work: per-hour GitHub call budget + GraphQL batching (switchboard helper; next switchboard helper at xhigh);
  Muse's git channel #507 (+ auto-relay of "To Muse:" posts, rule_check save-layout/raised-limit BLOCKs); #503, #506.
- Cory 11-13 ways of working: remove failure modes, never relocate them; never stop a helper past 10 min (escalate effort on
  its next run); check everything he says, push back when it costs more than it gains.
- Hard problems: Gemini Carbon (newer, smarter, 11-13) or Argon (every hard job so far done); each ~1.5 h free a day, repo +
  some GitHub; via one copyable prompt to Cory; Fable only for the very hardest, alert Cory first (it draws on this limit; he holds 2 refunds). Fable designs, Opus builds.
- Visuals for his art AI: flat, white, no cartoon; each item pinned WHERE it lives in the flow, numbered badges matching a
  legend, ~110 words. Crowded and a bare 40-word list both failed; refine from his feedback.
- Session wiring (11-13): this session is the integrator and holds the overview, both checks and the PAT reminder; the old
  session's copies are paused, not deleted (deleting a task can delete sessions it touched).

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
