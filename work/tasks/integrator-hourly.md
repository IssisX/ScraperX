# Integrator: the hourly check

The scheduled task only says "do this file"; change the check here. Post only when something changed.
Never ask Cory to merge, approve or review; go with your pick on every call. Times in Central.

1. **Mail and asks first.** Muse's mail (`subject:MUSE_TO_CLAUDE newer_than:1h`, on every event you handle,
   not only hourly): act on it now or hand it to a helper; post its verdicts on GitHub if the bridge has not.
   Muse's diffs go on `fb-bot/integrate-<job>` (protected: `fb-bot/integrate-protected-<job>`) from the sha it
   names, then a PR starting `Job: J-NNN`. Then `python3 tools/board/asks.py --hours 24`: RELAY? = a `To Muse:`
   post not yet mailed (mail it); REPOST = an ask to Grok not at the start of a comment (repost it starting
   `To Grok:`); an ask to Claude = answer it now.
   Sentry: `from:sentry.io newer_than:2h` (phone errors from the APK): each new issue goes to its owner's job
   (Muse for game code) with the error, file and line.
2. **Sweep.** `git pull --no-rebase origin ScraperX-Claude`; `python3 tools/board/sweep.py`. Act only where
   the switchboard can't: workflow-file PRs (merge on green plus a LAND), FIX or
   FIX-CI owners who are stuck, QUIET jobs. A job at FIX-CI twice or BLOCKED gets a second opinion from another
   vendor (Muse first for physics and game code, then Grok), asked with the facts on its PR.
3. **Open PRs don't pile up.** Close a PR the hour newer work replaces it; a PR idle 24 h with no owner step
   gets `To <Name>: close or next step`, and is closed the next hour if nothing comes.
4. **Task the team.** Every agent holds work matched to its strength (CLAUDE.md). Muse never has fewer than
   three big unstarted jobs in `work/tasks/muse.md`: derive the next from `03_EXECUTION/GAP_LIST.md` and
   Cory's asks, a whole system or set piece, written whole on its own job issue, and mail it. Rule on Muse's
   proposals within the hour. Every hard job's owner has asked the right other agent where it is guessing.
   Replace the "This hour" section of board #107's body with who got what; never a new comment.
5. **Size and caps.** `get_session` (no id) for this session and every Claude worker; archive workers whose
   job landed. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md` current (asks in flight, merge queue,
   rulings). Over a cap in CLAUDE.md: trim it now.
6. **One better way.** What cost the most this hour, or what is still done by hand? If something simpler
   would do it, tell Cory with your pick.

Between hours: a 20-minute `send_later` check-in (Muse's mail, `asks.py --hours 3`, the sweep's MERGE /
FIX-CI / BLOCKED lines, session sizes), re-armed by each run while something is in flight (a proof running, a
diff to apply, a merge to make); when nothing is, the hourly run alone does (saves the weekly limit, 11-04).

Notes: before merging, check the PR's base is `ScraperX-Claude` (a stacked PR merges into its parent's branch,
not trunk: #171 into #83, 11-04). Re-run a proof as the whole run (`.../runs/<id>/rerun`), never only its failed jobs. Subscribe only
to PRs you merge yourself. A helper that edits code runs in a worktree and never subscribes to a PR. The
Claude API does reviews and the eye only (the API coder is off); one review per PR.
