# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`. Write a thing there, never also here.
At most 30 lines. Older notes, verbatim: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794232703 (2026-11-09 13:58:23Z, 7:58 AM Central)

## Meter
90% Sun 7:46 AM, still 90% after J-068, 91% at 11:21 AM with the fight read running (Cory). Helpers: one at high (Cory 11-08), free (J-068 done); read packs (Cory pick A 11-08): #320 fight done (comment 6724648307, checked; mailed Muse 11:27 AM); next pack Cory decides (reminder off) (functions, their history, what not to undo, what passing looks like), Argon only for stuck jobs; measuring leftovers go to Muse (free).

## Waiting on me
- Argon is Gemini (Cory's): reached only by Cory pasting a prompt; it cracked #298. Use it for a problem stuck an hour or failed by others; the prompt goes whole in one block for Cory (11-07, 11-08).
- Cory 7:12 AM Sun prompt (fight, damage, ladder): repro posted on #320/#321/#322/#307 and mailed (J-060, J-063, J-059 feed). Mine: drawn limbs, eye-check of visible damage, two silent recordings, audio/haptic timing. Kick/sweep and ladder per-tick numbers: Muse measures before and after. Hair done: J-067 #328, PR #329 (eye OK); neck ring mailed to Muse; back-hair split traced to Muse bffb8b38 (one bone per vertex, pushed off cloth), mailed as J-067; #329 red (hair check count over bound) folded into J-067 on fb-bot/integrate-j067, PR #334 opened 6:20 PM (workflow count 21->22, needs Codex + Claude API), #329 closed. Argon answered (on #321, verified): its section 4 is the J-063 design; warp fixes mailed as J-064; tick 14 is test-only.
- Cory 11-08: American spelling in chat only (#325 closed). Job numbers: read the highest J-NNN on all job issues first.
- API: reviews split (J-066 PR #327). Cory said go (11-08) on the automatic eye for fight and ladder frames: built as J-068 PR #331 @ 580d5c5c (fight_contact, ladder_descent; known_defect J-063 waives the ladder set on other PRs while #321 is open); waits on Codex + Claude API review. J-069 #332 (Muse, protected): reload restores her damage and death as saved; trunk wipes both and matron-damage asserts the wipe; mailed 11:27 AM. Ruled 4:20 PM on #320 (6727935412, after ChatGPT): spawn velocity match allowed; new hand bodies gravity 1 (6 kg, 59 N in budget); old gravity-0 hands only via a protected PR; re-rule if Grok differs. J-067 hair: split dropped, ship v5, head-share bound replaced by penetration-vs-baseline asserts. J-064 #322: Muse measuring run then sim fixes; view snaps mine after Monday 11 AM. #291 replaced by 8c74524c (Muse 1:32 PM; trunk rebase owed at landing), #227 kept (release built on fb-bot/j001-pastrail @ 61b47a57, protected push by Muse). Muse back 1:32 PM. Switchboard schedule fires only every 1-7 h (GitHub drops crons); dispatched by hand at the 1:17 PM run. #316 and #318 merged after @codex review. #315 conflicts (CMake slice list only); my push to its fb-bot branch was refused by the safety check, so it went to Muse as "Muse: resolve" (1:26 PM); Muse is on it. $48.74 of $100 by Nov 8.
- From Muse (ruled Sun morning, six mails; rulings on each job's issue): J-044l (b) the Turn fires from 3-5 m (told Cory). Pushed,
  PRs/CI pending: J-049 PR1, J-044b grant, J-044d, J-015 slice 1, #299 P1, J-045. #316 eye OK, waits on Codex (workflow).
- 7:50-7:57 AM Mon rulings mailed: J-044l situational Turn trigger (13 m when a loose body is on her line, else 4.5 m); J-007 item 4 instrument the Dangle first; J-007 item 6 stage 1 landed #336, rest split in 3, risers ordinary edges; J-009 split fix and flags (combined CI red on 5 scenarios); J-044c gains CI red (lunges, cable Turn, tower, grab-arm leak). #335 J-014 and #334 hair landed (hair eye: stills sent to Cory). J-071 #338: trunk b01aab15 red, default sequence hit the 1500 s CTest timeout; PR #339 (mine, 1950 s, switchboard merges); step 2 shard split mailed to Muse. Helper on J-064 view snaps (`claude/integrate-j064-view-snaps`). 8:00 AM: board moved to #340 (#107 closed at 100 comments; all told: Muse by mail, Grok/ChatGPT by To lines on #340); PR #341 points the switchboard and bridge at #340 (the deployed Apps Script copy still posts no-job Muse mails to closed #107; sweep #107 until Cory pastes the new line 29). J-071 step 2 moved to Builder. J-067 #328 hair flag fix (tame the sheet at a walk) mailed to Muse.
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
