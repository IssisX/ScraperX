# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794139747 (2026-11-08 12:09:07Z, 6:09 AM Central)

## Meter
83% 8:20 AM Sat ($491); 85% 10:28 AM ($550); 85% 11:02 AM ($553.56); 86% 12:26 PM; 86% 1:36 PM; est. 86.9% 5:18 PM ($578.80); 87% 8:16 PM ($581.11, Cory; fit holds). Burn 5:18-8:16 PM $2.3 (quiet); predicted 87.5% at 11 PM, under 89%. 87% Sun 6:11 AM (Cory): flat overnight, second reading under; throttle off, helpers per CLAUDE.md again. 98% by Sun 11 PM leaves 11% for 17 h. 88% Sun 7:03 AM (Cory): the #319 helper cost about 1%; 10% left for 16 h. Throttled since 10:30 AM; from 12:40 PM one helper allowed, only for a job stuck twice (Cory: reset Sun afternoon); the 1:17 PM run answers and
merges only. Fit $13.6 per 1%; about 0.5%/h while runs are going (10:28 AM to 12:26 PM, integrator at xhigh); needs 0.39%/h around the clock, or about 0.6%/h over the 23 active run hours left; fits with about 2% to spare. Next meter ask: 9:17 PM, or sooner within 2% of a checkpoint.

## Waiting on me
- Argon is Gemini (Cory's): reached only by Cory pasting a prompt; it cracked #298. Use it for a problem stuck an hour or failed by others; the prompt goes whole in one block for Cory (11-07, 11-08).
- Cory 7:12 AM Sun prompt (fight, damage, ladder): J-060 #320 and J-063 #321 opened, mailed to Muse (top two); J-064 #322 her warps (audit) To Grok for 10 AM; To ChatGPT research asks on #320/#321. Mine: the measured repro (helper), drawn limbs, visible damage (J-059), two silent recordings as proof, audio/haptic timing. #319 merged; cheek PR #323 open. Hair helper on claude/integrate-hair. Argon prompt: J-063 climb on real support plus the warps, answer on #321.
- Cory 11-08: American spelling in every word I write in chat (not a repo sweep: #325 closed). One helper only now (89% Sun). Job numbers: read the highest J-NNN on all job issues first (J-061/J-062 were reused by mistake; now J-063/J-064).
- API: reviews split (J-066 #326/PR #327): ordinary at high, security pass at max (Cory 11-08). Next: an automatic eye on fight and ladder frames (Opus vision on CI renders) is my pick when Cory asks; $48.74 of $100 spent by Nov 8.
- From Muse (ruled 6:05 AM Sun, six mails): J-044l (b) the Turn fires from 3-5 m (told Cory; he may stop it); J-009 flags
  stay, 258 down is a traversal fix on the J-008 line; #298 gains PR waits on matron-sweep; #298 budget fix alone (loop kept
  on one line), no speed gate, StockHold made budget-bound, snag chain fix; J-021 job 4 cap waived; J-043 new hang rail
  (protected); J-011 rigid threads then wider blend. Pushed, PRs/CI pending: J-049 PR1, J-044b grant, J-044d, J-015 slice 1,
  #299 P1, J-045. #318 (J-053) eye OK. #309 and J-022 merged; side-view redo is #319 (bruise on her skin; eye OK; cheeks still flat quads). #316 eye OK, waits on Codex (workflow).
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
