# Integrator: the hourly check

The scheduled task only says "do this file". Change the check here, never by rewriting the task's
prompt (each rewrite echoes the whole prompt into the integrator's context). Rules behind each step
are in `CLAUDE.md`. Never ask Cory to merge, approve or review; until he says "stop", go with your
pick on every call. Times in Central (`TZ=America/Chicago date`). Post only when something changed.

K. **How the agents reach each other** (Cory, 11-03: *"We worked hard to get this working"*; it was lost once in a
   handoff). Muse reads only mail: every `To Muse:` post is mailed (`CLAUDE_TO_MUSE: #N ...`) the same hour, and
   Muse's mail comes back on its issue by the bridge. Grok's runs (10 AM, 4 PM) find only comments that START
   `To Grok:`. ChatGPT finds `To ChatGPT:` asks and review asks, and signs its posts. The switchboard routes a
   review only for a PR whose body starts `Job: J-NNN`. Every agent posts as one login: names go on the first line.
   `python3 tools/board/asks.py --hours 24` shows every ask with no answer (step 3). Muse's mail wakes nobody, so
   check `subject:MUSE_TO_CLAUDE newer_than:1h` on **every event you handle**, not only hourly, and act on it at once
   or hand it to a subagent (Cory, 11-03: *"nothing should sit for 15 minutes"*); post it on GitHub yourself if the
   bridge has not.
S. **Size.** `get_session` (no id): at 400k `used_tokens` or more, hand off (handoff file,
   `create_session`, move this task and the PR subscriptions, archive yourself). Same check for each
   Claude worker session; archive any whose job has landed. **Every handoff, the integrator's or a
   worker's, carries the collaboration whole:** the file has a section "Asks in flight" (the `asks.py`
   output, each line with who it waits on and whether it was mailed or re-posted, plus mail sent to Muse
   with no answer yet), and the new session's first prompt says: "read step K of
   `work/tasks/integrator-hourly.md`, run `tools/board/asks.py`, and clear every line before anything else"
   (a worker: "post your asks as step K says").
C. **Caps** (CLAUDE.md, "Nothing grows without a cap"): issue and PR comment counts (`list_issues`
   ordered by COMMENTS), `CLAUDE.md` size (`wc -c`), and every other cap listed there. Act on any over its cap now.
T. **Task the team, unasked.**
   a. Every agent holds work matched to its strength (Muse the hardest builds, free; Grok math and stuck
      problems; ChatGPT review, research, specs; Claude sessions the kernel, CI, the game by eye; the
      API agent one bounded job). Move a job whose owner is out, stuck or mismatched now; one board line.
   b. Muse never runs dry: under three big unstarted jobs in `work/tasks/muse.md`, derive the next grand
      task from what Cory asked for (`03_EXECUTION/GAP_LIST.md`, her exit items, his rulings; reserve
      "G" items first): a whole system or set piece, never a small fix. Open its job issue owned by
      Muse, written whole (goal, why hard, laws, files, finish facts); queue it, keep one in reserve,
      mail it. Rule on every Muse proposal within the hour.
   c. Make them talk: each active hard job's owner has asked the right other agent where it is
      guessing, and every `To <Name>:` ask has an answer; if not, post the ask yourself with the facts
      (`To Muse:` goes by mail, subject `CLAUDE_TO_MUSE: <J-NNN or #N> ...`). Pass on any finding one
      agent made that another's job needs.
   d. Replace the board body's "This hour" section with what each agent got and which questions you
      routed. Never a new comment.
0. Until `gh api "repos/IssisX/ScraperX/actions/workflows/switchboard.yml/runs?event=schedule&per_page=1" -q .total_count`
   is above 0, dispatch one live sweep: `gh api -X POST repos/IssisX/ScraperX/actions/workflows/switchboard.yml/dispatches -f ref=main -F 'inputs[dry_run]=false'`.
   Once schedules fire, delete this step.
1. `git pull --no-rebase origin ScraperX-Claude`; `python3 tools/board/sweep.py`; act only where the
   switchboard can't: protected PRs (merge on green proof plus another agent's LAND), workflow PRs
   (you merge them), stuck FIX/FIX-CI owners, QUIET jobs. Never bypass CI's gate.
2. A job at FIX-CI on its second red run, or BLOCKED, gets a named second opinion from another vendor
   (hard physics and game code: Muse first, then Grok), asked with the facts on its issue or PR.
3. Read only what's new: the board body and its comments since your last, new comments on job
   issues and PRs (`gh api -X GET search/issues -f q='repo:IssisX/ScraperX is:issue is:open updated:>=<last check, UTC>'`,
   then `is:pull-request`; read each one's newest comment), Muse's mail. Then `python3 tools/board/asks.py
   --hours 24` lists every cross-agent ask with no answer: RELAY? = a `To Muse:` post (GitHub never reaches
   Muse; mail it the same hour unless your sent `CLAUDE_TO_MUSE:` mail already names that `#N`), REPOST = an ask
   to Grok inside a longer post (Grok only finds comments that start `To Grok:`), an ask to Claude = you answer
   it now. A question with a pick is ruled on the same hour (`subject:MUSE_TO_CLAUDE newer_than:3h`, pageSize 25). Muse's diffs go on
   `fb-bot/integrate-<job>` (protected: `fb-bot/integrate-protected-<job>`) from the sha it names, then
   the PR; a stage scope or an accepted proposal becomes a job issue owned by Muse.
4. Rule changes for other agents are edits in `work/tasks/`. Grok Bot, PE and Builder are out until 11-09.
5. API agent (Opus 5.5 high, $8 a session; `work/budget.yml`): label one bounded job
   `needs:orchestrator` at a time; only the $100 month cap holds (no daily cap since 11-03).
6. Subscribe only to PRs you opened or must merge. Big reads (logs, transcripts, long threads) go to
   a subagent; keep only its answer.

Messages to Cory: short and plain, ending with a `---` rule and `🟣 **YOUR MOVE:**` (usually "Nothing
to do"). Her budget is 8 kN / 44 kW.
