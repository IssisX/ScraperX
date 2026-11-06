# Integrator: the hourly check

It runs every 3 hours, 8:17 AM to 11:17 PM Central, none overnight (Cory 11-06: the weekly limit must last to Sunday night).
At the 11:17 AM, 5:17 PM and 11:17 PM runs, ask Cory for the usage meter (total %), log it in the notes and redo the
pace: (100 - used) / hours left to Sunday bedtime, against the rate since the last reading.
Helpers (sub-agents): up to 2 at once (Cory 11-05, 11-06). Builds go to Muse first; Claude's own coding goes to a helper, not inline in the integrator's big context.
Every run, first: `get_session` (no id). If `effort_level` is not xhigh (his pick, 11-06 9 AM), or ultracode is on, tell Cory at once. Effort is only his call; Claude never changes it (it sat at high for 45 h, 11-03 to 11-06, after a Claude session put his xhigh back to high).

The scheduled task only says "do this file"; change the check here. Post only when something changed.
Never ask Cory to merge, approve or review; go with your pick on every call. Times in Central.

1. **Mail and asks first.** Muse's mail (`subject:MUSE_TO_CLAUDE newer_than:1h`, on every event you handle,
   not only hourly): act on it now or hand it to a helper; post its verdicts on GitHub if the bridge has not.
   Muse's diffs go on `fb-bot/integrate-<job>` (protected: `fb-bot/integrate-protected-<job>`) from the sha it
   names, then a PR starting `Job: J-NNN`. Then `python3 tools/board/asks.py --hours 24`: RELAY? = a `To Muse:`
   post not yet mailed (mail it); REPOST = an ask to Grok not at the start of a comment (repost it starting
   `To Grok:`); an ask to Claude = answer it now.
   Muse 3 h with no mail while its fb-bot branches move: tell Cory at once (11-06: 18 h went unreported).
   Sentry: `from:sentry.io newer_than:2h` (phone errors from the APK): each new issue goes to its owner's job
   (Muse for game code) with the error, file and line.
2. **Sweep.** `git pull --no-rebase origin ScraperX-Claude`; `python3 tools/board/sweep.py`. Act only where
   the switchboard can't: workflow-file PRs (merge on green plus a LAND), FIX or
   FIX-CI owners who are stuck, QUIET jobs. A job at FIX-CI twice or BLOCKED gets a second opinion from another
   vendor (Muse first for physics and game code, then Grok), asked with the facts on its PR.
   A piece that holds 2+ jobs, stuck 6 h or two red proofs with its owner, about an hour's work: take it (Cory 11-06).
   First mail the owner `CLAUDE_TO_MUSE: taking <piece>, stop your lane; if you pushed it, mail the sha`, check its
   fb-bot branches and mail again before starting and before pushing, and stop if its version is there.
3. **Open PRs don't pile up.** Close a PR the hour newer work replaces it; a PR idle 24 h with no owner step
   gets `To <Name>: close or next step`, and is closed the next hour if nothing comes.
4. **Task the team.** Every agent holds work matched to its strength (CLAUDE.md). Muse never has fewer than
   three big unstarted jobs in `work/tasks/muse.md`: derive the next from `03_EXECUTION/GAP_LIST.md` and
   Cory's asks, a whole system or set piece, written whole on its own job issue, and mail it. Rule on Muse's
   proposals within the hour. Every hard job's owner has asked the right other agent where it is guessing.
   Replace the "This hour" section of board #107's body with who got what; never a new comment.
5. **Size and caps.** `get_session` (no id) for this session. Helpers are its sub-agents and end with
   their job. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md` current (asks in flight, merge queue,
   rulings). Over a cap in CLAUDE.md: trim it now.
6. **Look for a better way, every hour (build one only if found).** What cost the most quota this hour (context size, helpers, wake-ups),
   what is still done by hand, and is the weekly pace on track for the Monday 11 AM reset? List every wake-up
   that had nothing to do (a PR event, CI note, helper ping or reminder that changed no step) and stop its
   source: unsubscribe, mute or reroute it. If a real saving shows up that costs no quality, build it and tell
   Cory in one line; if none does, say nothing and build nothing (Cory 11-05: never invent work).

Between hours: no timed check-ins (Cory 11-05, so the weekly limit lasts until the Monday 11 AM reset). The switchboard
merges by itself, and PR events and helper reports wake the integrator. The hourly run alone reads Muse's mail. A quiet
hourly run ends in one line. Helpers: after the two in flight (J-042, J-044m/J-045), one at a time; free Muse takes the
builds first.

Trunk merges on any PR are yours, done the hour it conflicts, and the owner is mailed the new sha. Hand a merge to
Muse only when the conflict is in its own logic, by a mail that says "Muse: resolve". Never ask for a job and then
do it yourself: if you take a job back, mail that first (11-05: Muse's merge agent was wasted on #232/#233).
Before a hand merge, check that the PR head equals `git ls-remote` (GitHub can miss a push, #235).

Notes: before merging, check the PR's base is `ScraperX-Claude` (a stacked PR merges into its parent's branch,
not trunk: #171 into #83, 11-04). Re-run a proof as the whole run (`.../runs/<id>/rerun`), never only its failed jobs. Subscribe only
to PRs you merge yourself. Opening a PR subscribes you already: never subscribe again, and unsubscribe at once
from one the switchboard merges (11-05: #250-#253 woke the integrator 14 times in 3 minutes, mostly Codex's
running/finished edits). A helper that edits code runs in a worktree and never subscribes to a PR. Every helper brief ends: "Report to me in 10
lines or fewer (branch, sha, PASS/FAIL, what is left); put the numbers and details on the job's issue. Run tests in the foreground; never end a turn waiting on a
background job." (11-05: one helper's background waits woke the integrator about 10 times.) The
Claude API does reviews and the eye only (the API coder is off); one review per PR.
