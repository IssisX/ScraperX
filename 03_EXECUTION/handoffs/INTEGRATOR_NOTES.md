# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.
Read through: 1794492989 (2026-11-11 last quiet run)

## Now (11-12 handover)
- Collaboration system LIVE (#471, #476, #477 merged; doc COLLABORATION_SYSTEM_REBUILD.md section C). Priority one until proven
  (Cory): NO new game code from Claude or helpers till then. Expect many label changes and real conflicts on the first live passes
  (J-044: fb-bot and claude j044c heads both exist); that is the true state, act on it.
- FIRST JOB after the reset (Cory yes, 11-12): the switchboard posts to the #282 inbox only when the next step is Claude's, so
  plain events stop waking (and spending) Claude; add it via plan.py. Other gaps: a job waiting on a question doesn't show it;
  Grok/Builder have no notice channel; per-pass read bounds; Grok Bot sees only what Cory hands it; sTiX_ww4 = Muse AND ChatGPT.
- Paused game work, resume after: J-015 #466 (Codex FIX open, trunk merged; worktree agent-a2ed081112df776e3), J-001 grip charge
  red (protected; agent-a169bc3f55f0e7fef; next fail -> "Argon?" prompt to Cory, then Fable), J-137 #474 fix on claude/integrate-j137
  (pin was a test freeze; Muse merges trunk into J-118 after it lands; agent-a1d4035b049d06e9f), J-044c gains tune.
- Open with Muse: J-053 swing bar up <=0.2 m (else Cory's game call); J-077 gate fix; J-059 gap-89 (protected); J-078 #396 FIX nudged; J-044c held.
- Session wiring: sessions made by another session hit the depth limit and can't create scheduled tasks or send_later. The old
  session's overview and both integrator checks are PAUSED (not deleted); the PAT reminder is still on. This
  session is paused too (off the #282 inbox). Cory starts a fresh
  integrator from the app after the quota resets; it recreates all four on itself and deletes the old ones.
- Fable low fixed named causes right first try (J-106, J-015). Fable designs, Opus builds from a finished design (Cory 11-12).

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused. Keep a helper's worktree until its PR lands: removing it kills the resume (11-12, #469).
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
