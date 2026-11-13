# Integrator: the hourly check

It fires at 8:17 AM, 11:17 AM, 2:17 PM, 5:17 PM, 8:17 PM and 11:17 PM Central on weekdays (Sat-Sun 9:17 AM, 1:17 PM,
5:17 PM, 9:17 PM), every run full. It is a backstop (11-10): the switchboard inbox wakes the integrator on every Muse mail,
red proof, review and merge; these runs catch what no event raised.
The day's last run (11:17 PM; Sat-Sun 9:17 PM) is quiet:
it answers asks and lands merges, no new jobs, no board edits (Cory 11-06).
Pace: no throttling, no pace prediction, no meter asks, no quiet runs (11-10).
Specs, ruling math and research go to ChatGPT or Grok first; the integrator checks, never drafts them (11-06).
How: helpers get the important work and long reads, never chores (11-06); the integrator runs its own check-in lean and never hand-edits the board (the switchboard writes it from the plan). No coding inline.
Fewer, batched calls.
Quality floor (11-06): never cut reviews, tests on what a change reaches, checks by
eye, or checking a claim before stating it. A slip (a wrong claim, a missed bug, a redo) is told to Cory the same hour.
Helpers (sub-agents): at most four at a time, on the hardest jobs (11-12), each code-editing one in a worktree. Builds go to Muse first; Claude's own coding goes to a helper, not inline in the integrator's big context.
Every run, first: `get_session` (no id). Effort is his call, seen in `get_session`: mention it only when he throttles; ultracode on: tell him once; fast mode: trust the app's /fast line, not get_session (11-12).

Overview: a scheduled task (trig_019Qy9rcek8tdRXTcpNjF1zs, :43 past, 6 AM-9 PM Central) has Claude send Cory a 6-line overview (11-11).
Change the check here, never in the task's prompt. Post only when something changed. Each overview also
says what waits on Claude (asks to Claude over 1 h from `asks.py --hours 48`, Claude's red branches, idle PRs over
4 h) and each taken job's hours since it was taken; any nonzero pile is acted on that run (11-12).
Never ask Cory to merge, approve or review; go with your pick on every call. Times in Central.

1. **Mail and asks first, every run and every event you handle.** The mark is the notes' `Read through:` Unix time.
   Note `date +%s`, then mail `after:<mark> (subject:MUSE_TO_CLAUDE OR from:sentry.io OR subject:"Run failed: Switchboard")`
   and `python3 tools/board/asks.py --since <mark + 120>` (no overlap): both list only what came after the mark, so empty means nothing
   new. Act on each, then set the mark to the time you noted less 120 s (mail indexes late; a repeat is in the notes) and push it (never before acting; nothing new: leave it). Muse's mail: act now or hand it
   to a helper; post its verdicts on GitHub if the bridge has not. Muse pushes its own branches (the integrator's
   pushes to `fb-bot/` are refused); the switchboard opens its PRs.
   Asks: RELAY? = a `To Muse:` post (mail it); REPOST = an ask to Grok not at the start of a comment (repost it
   starting `To Grok:`); an ask to Claude = answer it now. Full runs also run `asks.py --hours 24` and answer any
   ask to Claude still open (a miss); its other lines were handled when first listed.
   Sentry (phone errors from the APK): each new issue goes to its owner's job (Muse for game code) with the error,
   file and line. A failed Switchboard run: read its failing job's error lines; fix it or hand it to a helper.
   Before ruling on or reporting a branch, read its live head (`git ls-remote`) and proof: mail can be an hour old
   (Muse 11-06). So does a mail that changes a ruling.
   Muse 3 h with no mail while its fb-bot branches move: tell Cory at once (11-06).
2. **Sweep.** `git pull --no-rebase origin ScraperX-Claude`; `python3 tools/board/sweep.py`. Act only where
   the switchboard can't (J-056: it merges workflow PRs too): FIX or
   FIX-CI owners who are stuck, QUIET jobs. A job at FIX-CI twice or BLOCKED gets a second opinion from another
   vendor (Muse first for physics and game code, then Grok), asked with the facts on its PR.
   Only the hardest: an unknown cause blocking 2+ jobs, the climb or her chase, or a fix failed twice: take it (11-12).
   Use `tools/board/plan.py give` only after the old owner pushes and acknowledges a stop; do not change the plan for
   unpushed active work (11-12: J-109 built twice).
3. **Open PRs don't pile up.** Close a PR the hour newer work replaces it; a PR idle 4 h with no owner step (11-12)
   gets `To <Name>: close or next step`, and is closed the next hour if nothing comes.
4. **Task the team.** Every agent holds work matched to its strength (CLAUDE.md). Run `tools/board/plan.py check`; rule every LATE line and every conflict, then
   route ready work through the plan only. The switchboard generates the board, labels and Muse notices. A new job
   comes from `03_EXECUTION/GAP_LIST.md` and Cory's asks, written whole on its own job issue, scored by
   `job-routing.md`, and added with `tools/board/plan.py add`. Rule on Muse's
   proposals within the hour. Every hard job's owner has asked the right other agent where it is guessing. My own
   and my helpers' jobs too: a job red twice, or a red Muse's branches share, gets a question mailed to Muse that run, and climbs CLAUDE.md's ladder at once (CLAUDE.md, 11-12); one outside agent per question, a second only if needed (11-11: "Not AND"); nothing waits on Grok's run (Cory 11-11, 11-12).
5. **Size and caps.** `get_session` (no id) for this session. Helpers are its sub-agents and end with
   their job. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md` current: only what has no other home. Over a cap in CLAUDE.md: trim it now.
6. **Stop waste seen this run; no searching for new tools or setups** (Cory 11-06 10 AM) unless a system keeps failing (11-09). Is the weekly pace on track for the Monday 11 AM reset? List every wake-up
   that had nothing to do (a PR event, CI note, helper ping or reminder that changed no step) and stop its
   source: unsubscribe, mute or reroute it. If a real saving shows up that costs no quality, build it and tell
   Cory in one line; if none does, say nothing and build nothing (Cory 11-05: never invent work).

Between runs: the switchboard merges by itself; the inbox (#282: every Muse mail the bridge posts, proofs, reviews),
PR events on Claude's own PRs and helper reports wake the integrator. Free Muse takes the builds first.

Trunk merges: each owner merges trunk into its own branch (Muse too, 11-10); the integrator does it only for its
own and Claude's branches. Never ask for a job and then do it yourself: if you take a job back, mail that first.
Before a hand merge, check that the PR head equals `git ls-remote` (GitHub can miss a push, #235).

Notes: before merging, check the PR's base is `ScraperX-Claude` (#171 merged into #83, 11-04). Re-run a proof as the whole run (`.../runs/<id>/rerun`), never only its failed jobs. Subscribe to PRs you merge yourself
and to Claude's own PRs. Before `@codex review` or
calling a PR clean, read its open review threads (`get_review_comments`), not only the newest review. Opening a PR subscribes you already: never subscribe again, and unsubscribe at once
from one the switchboard merges. A helper that edits code runs in a worktree and never subscribes to a PR. Every helper brief ends: "Report to me in 10
lines or fewer (branch, sha, PASS/FAIL, what is left); put the numbers and details on the job's issue. Run tests in the foreground; never end a turn waiting on a
background job." The
Claude API does reviews and the eye only (the API coder is off); one review per PR.
