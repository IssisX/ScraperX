# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794100618 (2026-11-08 01:16:58Z, 7:16 PM Central)

## Meter
83% 8:20 AM Sat ($491); 85% 10:28 AM ($550); 85% 11:02 AM ($553.56); 86% 12:26 PM; 86% 1:36 PM; est. 86.9% 5:18 PM ($578.80), predicts about 89.3% at 11 PM, over the 89% checkpoint: throttled (no helper, quiet runs), meter asked 5:18 PM. Throttled since 10:30 AM; from 12:40 PM one helper allowed, only for a job stuck twice (Cory: reset Sun afternoon); the 1:17 PM run answers and
merges only. Fit $13.6 per 1%; about 0.5%/h while runs are going (10:28 AM to 12:26 PM, integrator at xhigh); needs 0.39%/h around the clock, or about 0.6%/h over the 23 active run hours left; fits with about 2% to spare. Next meter ask: 9:17 PM, or sooner within 2% of a checkpoint.

## Waiting on me
- From Muse (ruled 7:19 PM, one mail): #291 start out of reach + not-in-Grab check; #298 test-side budget, cap waived;
  J-049 fix timing (ChatGPT asked on #278); J-001 reserved feet share then hold release; J-021 split; J-059 side-rim + flip push;
  J-044d push d, I as a follow-up; J-043 inventory; hair; button real-tap check. #299 P1 fix; J-061 running. J-008 pushed @ 45e16da2.
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
