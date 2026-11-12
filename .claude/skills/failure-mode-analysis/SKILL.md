---
name: failure-mode-analysis
description: Cory's "failure mode analysis" of the agent system. Trace every hand-off where a job, ask, mail, review or follow-up can drop between agents, and give each hole a check or a tracked home. Use when he asks for one, or after a miss the system let through (an announced job never pushed, an ask unanswered, a mail lost).
---

# Failure mode analysis

Cory 11-11: *"I think I want you to do a failure mode analysis"*, after a job was announced and never
pushed. The goal: find every place in the agents' system where work can silently drop, and stop each one
with a mechanism. Remembering harder is not a mechanism (*"I know that doesn't work"*).

Cory 11-12 named the whole thing **the collaboration system**: who gets what work, Claude and Muse's mail, helpers,
the switchboard, the stuck-work alarms. *"Make sure this is working"* means the whole loop is closed: every job has an
owner matched by `work/tasks/job-routing.md`, the work actually gets done, nothing piles up and nothing is missed.

1. **List the hand-offs.** These are: Cory to integrator, integrator to Muse mail, Muse mail to the bridge
   and inbox, asks (`To <Name>:`), proofs, reviews, merges, helper briefs and reports, the queue
   (`work/tasks/muse.md`), the board, scheduled tasks and reminders, and follow-ups promised in chat.
2. **For each one, name how it fails silently.** For example: sent but not read, read but not acted on,
   acted on but not reported, a promise with no home, a state that ages with no alarm, or a check that
   reads the wrong source.
3. **Prove each hole from the record**, never by guessing. Use a real case: a mail, comment, commit or
   run where it happened or nearly did. A hole with no case is a risk; say it as one.
4. **Give each hole the cheapest mechanism that would have caught it.** That is a check in
   `tools/board/` or the switchboard (with a test), a field in an existing report, or a line in its one
   tracked home. Reuse an existing check before adding one.
5. **Build, test and push the mechanisms.** Code goes in its own commit and runs CI; notes go in a
   `[skip ci]` commit.
6. **Tell Cory in a few plain lines:** the holes found, what now catches each, and what is left.

Worked example: dc59f337 (11-11). queue_check reports waits and parked answers, and follow-ups and
reminders got a tracked home.
