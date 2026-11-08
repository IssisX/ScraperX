# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794139747 (2026-11-08 12:09:07Z, 6:09 AM Central)

## Meter
88% Sun 7:03 AM (Cory); one helper only from 89%. Weekly reset Mon Nov 9 11 AM. Older readings: the log file.

## Waiting on me
- Argon is Gemini (Cory's): reached only by Cory pasting a prompt; it cracked #298. Use it for a problem stuck an hour or failed by others; the prompt goes whole in one block for Cory (11-07, 11-08).
- Cory 7:12 AM Sun prompt (fight, damage, ladder): J-060 #320 and J-063 #321 opened, mailed to Muse (top two); J-064 #322 her warps (audit) To Grok for 10 AM; To ChatGPT research asks on #320/#321. Mine: the measured repro (helper), drawn limbs, visible damage (J-059), two silent recordings as proof, audio/haptic timing. #319 merged; cheek PR #323 open. Hair helper on claude/integrate-hair. Argon prompt: J-063 climb on real support plus the warps, answer on #321.
- Cory 11-08: American spelling in chat only (#325 closed). Job numbers: read the highest J-NNN on all job issues first.
- API: reviews split (J-066 PR #327). Cory said go (11-08) on the automatic eye for fight and ladder frames: the one helper builds it after the repro and hair helpers hand over. $48.74 of $100 by Nov 8.
- From Muse (ruled Sun morning, six mails; rulings on each job's issue): J-044l (b) the Turn fires from 3-5 m (told Cory). Pushed,
  PRs/CI pending: J-049 PR1, J-044b grant, J-044d, J-015 slice 1, #299 P1, J-045. #316 eye OK, waits on Codex (workflow).
- Remind Cory by Nov 20: PAT `ScraperX` expires Mon Nov 23.
- 367 archived branches (`archive/branches-20261106` @ 8f2c6a82): deleting them waits on Cory.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox (read-only connector): search only the agent subjects; never read or touch his other mail (11-07).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused.
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
