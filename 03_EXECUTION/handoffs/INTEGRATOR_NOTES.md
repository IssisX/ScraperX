# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794151022 (2026-11-08 15:17:02Z, 9:17 AM Central)

## Meter
90% Sun 7:46 AM, still 90% after J-068, 91% at 11:21 AM with the fight read running (Cory). Helpers: one at high (Cory 11-08), free (J-068 done); read packs (Cory pick A 11-08): #320 fight now by a helper; what comes next Cory decides after it (reminder off) (functions, their history, what not to undo, what passing looks like), Argon only for stuck jobs; measuring leftovers go to Muse (free).

## Waiting on me
- Argon is Gemini (Cory's): reached only by Cory pasting a prompt; it cracked #298. Use it for a problem stuck an hour or failed by others; the prompt goes whole in one block for Cory (11-07, 11-08).
- Cory 7:12 AM Sun prompt (fight, damage, ladder): repro posted on #320/#321/#322/#307 and mailed (J-060, J-063, J-059 feed). Mine: drawn limbs, eye-check of visible damage, two silent recordings, audio/haptic timing. Kick/sweep and ladder per-tick numbers: Muse measures before and after. Hair done: J-067 #328, PR #329 (eye OK); neck ring mailed to Muse; back-hair split traced to Muse bffb8b38 (one bone per vertex, pushed off cloth), mailed as J-067; #329 red (hair check count over bound) folded into J-067 on fb-bot/integrate-j067, close #329 when its PR opens. Argon answered (on #321, verified): its section 4 is the J-063 design; warp fixes mailed as J-064; tick 14 is test-only.
- Cory 11-08: American spelling in chat only (#325 closed). Job numbers: read the highest J-NNN on all job issues first.
- API: reviews split (J-066 PR #327). Cory said go (11-08) on the automatic eye for fight and ladder frames: built as J-068 PR #331 @ 580d5c5c (fight_contact, ladder_descent; known_defect J-063 waives the ladder set on other PRs while #321 is open); waits on Codex + Claude API review. $48.74 of $100 by Nov 8.
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
