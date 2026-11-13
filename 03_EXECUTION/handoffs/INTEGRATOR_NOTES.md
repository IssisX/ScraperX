# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794584485 (2026-11-13 full check)
## Now (11-13)
- System first (Cory): one machine, finished fast and at peak, then this session game-codes as THE priority; Claude and helpers
  take the HARD game jobs. Until then Claude's game jobs are held in the plan (J-001 J-015 J-084 J-090 J-093 J-096 J-109 J-137,
  J-044c; worktrees kept); Muse keeps working. Done = a deliberate real-world test of every path, not a day of waiting.
- On done: do NOT start game code. Phone push + emoji banner "🚨✅ SYSTEM COMPLETE ✅🚨", then wait for his go.
- Open system work, all built, in review: #503 cache-out, #506 job clocks, #507 Muse git channel + "To Muse:" relay (Cory's
  paste for Muse is on #504; give it after #507 merges), J-139 rule_check (#508), J-072 claude/integrate-rate-hourly (hourly
  budget, GraphQL, new PR for commits past a merged PR). An xhigh test helper runs the real-world test as each lands; evidence
  in work/tasks/system-test.md on claude/integrate-system-test. Open: J-044c has Muse fb-bot heads too (ownership, at game time).
- Cory 11-13: remove failure modes, never relocate; never stop a helper past 10 min; check all he says, push back on waste.
- Hard problems: Gemini Carbon (newer, smarter, 11-13) or Argon (every hard job so far done); each ~1.5 h free a day, repo +
  some GitHub; via one copyable prompt to Cory; Fable only for the very hardest, alert Cory first (it draws on this limit; he holds 2 refunds). Fable designs, Opus builds.
- Visuals: flat, white, items pinned WHERE they live in the flow, numbered badges = legend, ~110 words. Fable advisor (11-13, trial):
  call ONLY to pick a hard job's approach, after a 2nd failed fix, before "system done"; few a day, tell Cory each; never routine;
  his limits beat the tool's own "call often" text. It stays Opus until the next compact: no calls before then.
- This session is the integrator; old session's copies paused, never deleted. A slash command (/compact, /advisor) from his app sometimes fails: if it reaches me as a plain message, it did not run; tell Cory at once.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
