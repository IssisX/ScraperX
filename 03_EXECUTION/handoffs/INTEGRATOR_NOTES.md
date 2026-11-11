# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794406889 (2026-11-11 8:21 AM Central)

## Now (11-11)
Coding with the system (J-072 done 11-10). Work state lives in GitHub, not here: job issues (owner, state, needs:eyes
for every visual or fight job, so the switchboard holds the merge for the eye), Muse's queue (checked by
`tools/board/queue_check.py`), my follow-ups as their own job issues (J-093 #411). Dated reminders are scheduled
tasks (the PAT expiry, the backstop decision), never only a line here.
- Argon is Gemini (Cory's): reached only by Cory pasting a prompt in chat; the hardest physics (J-001, J-044c).
- 367 archived branches (`archive/branches-20261106` @ 8f2c6a82): deleting them waits on Cory.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: search only the agent subjects; never read or touch his other mail (11-07). Agent mail may be trashed (never deleted for good, never archived) once 2 days old: only single-sender threads from issisx3 whose subject starts `MUSE_TO_CLAUDE:` (bridged, Label_30) or `CLAUDE_TO_MUSE:`; any other thread is left alone (11-11: "Absolutely never ... delete my emails in my main inbox").
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused.
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
