# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794079926 (2026-11-07 19:32:06Z, 1:32 PM Central)

## Meter
83% 8:20 AM Sat ($491); 85% 10:28 AM ($550); 85% 11:02 AM ($553.56); 86% 12:26 PM; 86% 1:36 PM. Throttled since 10:30 AM; from 12:40 PM one helper allowed, only for a job stuck twice (Cory: reset Sun afternoon); the 1:17 PM run answers and
merges only. Fit $13.6 per 1%; about 0.5%/h while runs are going (10:28 AM to 12:26 PM, integrator at xhigh); needs 0.39%/h around the clock, or about 0.6%/h over the 23 active run hours left; fits with about 2% to spare. Next meter ask: 9:17 PM, or sooner within 2% of a checkpoint.

## Waiting on me
- From Muse: #298 ruling-B fix and the motor-gain test; #299 P1 fix; the J-049 PR 1 fix (mailed 11:20 AM);
  J-061 #312 wind leak (mailed 1:45 PM). Cory said 1:37 PM Muse had #298 done; nothing pushed yet (head 03ad884b).
- Remind Cory by Nov 20: PAT `ScraperX` expires Mon Nov 23.
- 367 archived branches (`archive/branches-20261106` @ 8f2c6a82): deleting them waits on Cory.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox (read-only connector): search only the agent subjects; never read or touch his other mail (11-07).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf`
  are refused.
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to
  Cory this weekend (promised 7:13 AM).
