# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794409124 (2026-11-11 8:58 AM Central)

## Now (11-11, before Cory's compact at about 9 AM)
Work state lives in GitHub: job issues (owner, state, needs:eyes), Muse's queue (`tools/board/queue_check.py`, floor 8,
refill to 15), my follow-ups as job issues (J-093 #411, J-097 #415). Three helpers at high, the cap (Cory 11-11):
- J-001 #108: window pose @ 67837d5e and Floor checks on `claude/integrate-protected-j093` @ 56624498 (on top of
  it; land after the hold branch). 9 left (B1 timing x6, two entry-swing reaches, snag setup): pass 6 helper, 9:05 AM.
- J-096 #414 planks: slice 1 @ 4bb372c4 (inertia x233 ruled ok, Grok checks); proof 43044737336; my eye check
  before it lands. J-084 #402 (taken from Muse 9:06 AM): helper on `claude/integrate-j084`; J-082 waits on its table.
- J-090 #408 hook her: pass 1 c755689d (proof 43041899766). Ruled: physics stands (a planted
  Matron is dragged along a lip, not pulled off; she goes over only when not planted). Pass 2 after J-001 lands:
  her hands on the line, the 67.7 kN one-tick jolt before the 6 kN tear, the touch scenario.
- J-092 #410 slab probe (taken from Muse 9:19 AM): helper on `claude/integrate-j092`. J-097 landed 9:18 AM.
Gmail filter (Cory, 8:45 AM): agent mail skips his inbox to the label "ScraperX agents" (new mail checked 9 AM).
- Argon is Gemini (Cory's): reached only by Cory pasting a prompt in chat; the hardest physics (J-001, J-044c).
- 367 archived branches (`archive/branches-20261106` @ 8f2c6a82): deleting them waits on Cory.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused.
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
