# Integrator: the hourly check

It fires at 8:17 AM, 11:17 AM, 2:17 PM, 5:17 PM, 8:17 PM and 11:17 PM Central on weekdays (Sat-Sun 9:17 AM, 1:17 PM,
5:17 PM, 9:17 PM), every run full. It is a backstop (Cory 11-10, "drop"): the switchboard inbox wakes the integrator on
every Muse mail, red proof, review and merge, and it acts then; these runs catch what no event raised. Kept to 11-12,
then dropped if events covered everything.
The day's last run (11:17 PM; Sat-Sun 9:17 PM) is quiet:
it answers asks and lands merges, no new jobs, no board edits (Cory 11-06).
Pace: no throttling (Cory 11-10: "you can stop throttling completely"; he has usage resets to spend). No pace
prediction, no meter asks, no quiet runs to save quota.
Specs, ruling math and research go to ChatGPT or Grok first; the integrator checks, never drafts them (11-06).
How: helpers get the important work (Claude's 1-2 key jobs, long reads), never chores (Cory 11-06
9:30 AM); the integrator runs its own check-in lean and edits the board only when an owner or a hold changes. No coding inline.
Fewer, batched calls; no re-reads; short messages to Cory.
Quality floor (Cory 11-06 9:45 AM: "don't degrade the quality"): never cut reviews, tests on what a change reaches, checks by
eye, or checking a claim before stating it. A slip (a wrong claim, a missed bug, a redo) is told to Cory the same hour.
Helpers (sub-agents): as many as the work needs (11-10), each code-editing one in a worktree. Builds go to Muse first; Claude's own coding goes to a helper, not inline in the integrator's big context.
Every run, first: `get_session` (no id). Effort is his call, seen in `get_session`: mention it only when he throttles; ultracode on: tell him once.

Change the check here, never in the task's prompt. Post only when something changed.
Never ask Cory to merge, approve or review; go with your pick on every call. Times in Central.

1. **Mail and asks first, every run and every event you handle.** The mark is the notes' `Read through:` Unix time.
   Note `date +%s`, then mail `after:<mark> (subject:MUSE_TO_CLAUDE OR from:sentry.io OR subject:"Run failed: Switchboard")`
   and `python3 tools/board/asks.py --since <mark + 120>` (no overlap): both list only what came after the mark, so empty means nothing
   new. Act on each, then set the mark to the time you noted less 120 s (mail indexes late; a repeat is in the notes) and push it (never before acting; nothing new: leave it). Muse's mail: act now or hand it
   to a helper; post its verdicts on GitHub if the bridge has not. Muse's diffs go on `fb-bot/integrate-<job>`
   (protected: `fb-bot/integrate-protected-<job>`) from the sha it names, then a PR starting `Job: J-NNN`.
   Asks: RELAY? = a `To Muse:` post (mail it); REPOST = an ask to Grok not at the start of a comment (repost it
   starting `To Grok:`); an ask to Claude = answer it now. Full runs also run `asks.py --hours 24` and answer any
   ask to Claude still open (a miss); its other lines were handled when first listed.
   Sentry (phone errors from the APK): each new issue goes to its owner's job (Muse for game code) with the error,
   file and line. A failed Switchboard run: read its failing job's error lines; fix it or hand it to a helper.
   Before ruling on or reporting a branch, read its live head (`git ls-remote`) and proof: mail can be an hour old
   (Muse 11-06). So does a mail that changes a ruling.
   Muse 3 h with no mail while its fb-bot branches move: tell Cory at once (11-06: 18 h went unreported).
2. **Sweep.** `git pull --no-rebase origin ScraperX-Claude`; `python3 tools/board/sweep.py`. Act only where
   the switchboard can't (J-056: it merges workflow PRs too): FIX or
   FIX-CI owners who are stuck, QUIET jobs. A job at FIX-CI twice or BLOCKED gets a second opinion from another
   vendor (Muse first for physics and game code, then Grok), asked with the facts on its PR.
   A piece that holds 2+ jobs, stuck 6 h or two red proofs with its owner, about an hour's work: take it (Cory 11-06).
   First mail the owner `CLAUDE_TO_MUSE: taking <piece>, stop your lane; if you pushed it, mail the sha`, check its
   fb-bot branches and mail again before starting and before pushing, and stop if its version is there.
3. **Open PRs don't pile up.** Close a PR the hour newer work replaces it; a PR idle 24 h with no owner step
   gets `To <Name>: close or next step`, and is closed the next hour if nothing comes.
4. **Task the team.** Every agent holds work matched to its strength (CLAUDE.md). Muse never has fewer than
   six big unstarted jobs in `work/tasks/muse.md`: derive the next from `03_EXECUTION/GAP_LIST.md` and
   Cory's asks, a whole system or set piece, written whole on its own job issue, and mail it. Rule on Muse's
   proposals within the hour. Every hard job's owner has asked the right other agent where it is guessing.
   Replace the "This hour" section of the board's body (the open issue labeled `board`) with who got what; never a new comment.
5. **Size and caps.** `get_session` (no id) for this session. Helpers are its sub-agents and end with
   their job. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md` current: only what has no other home. Over a cap in CLAUDE.md: trim it now.
6. **Stop waste seen this run; no searching for new tools or setups** (Cory 11-06 10 AM) unless a system keeps failing (11-09). Is the weekly pace on track for the Monday 11 AM reset? List every wake-up
   that had nothing to do (a PR event, CI note, helper ping or reminder that changed no step) and stop its
   source: unsubscribe, mute or reroute it. If a real saving shows up that costs no quality, build it and tell
   Cory in one line; if none does, say nothing and build nothing (Cory 11-05: never invent work).

Between runs: the switchboard merges by itself; the inbox (#282), PR events on Claude's own PRs and helper reports wake
the integrator. Mail wakes nothing: only the runs read it. Free Muse takes the builds first.

Trunk merges on any PR are yours, done the hour it conflicts, and the owner is mailed the new sha. Hand a merge to
Muse only when the conflict is in its own logic, by a mail that says "Muse: resolve". Never ask for a job and then
do it yourself: if you take a job back, mail that first (11-05: Muse's merge agent was wasted on #232/#233).
Before a hand merge, check that the PR head equals `git ls-remote` (GitHub can miss a push, #235).

Notes: before merging, check the PR's base is `ScraperX-Claude` (#171 merged into #83, 11-04). Re-run a proof as the whole run (`.../runs/<id>/rerun`), never only its failed jobs. Subscribe to PRs you merge yourself
and to Claude's own PRs (11-06: #277 sat 2 h on a Codex P1 nobody was woken for). Before `@codex review` or
calling a PR clean, read its open review threads (`get_review_comments`), not only the newest review. Opening a PR subscribes you already: never subscribe again, and unsubscribe at once
from one the switchboard merges (11-05: #250-#253 woke it 14 times in 3 min). A helper that edits code runs in a worktree and never subscribes to a PR. Every helper brief ends: "Report to me in 10
lines or fewer (branch, sha, PASS/FAIL, what is left); put the numbers and details on the job's issue. Run tests in the foreground; never end a turn waiting on a
background job." (11-05: they woke it 10 times.) The
Claude API does reviews and the eye only (the API coder is off); one review per PR.
