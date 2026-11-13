# Grok (app): hard second opinions

Twice a day, in GitHub repository `IssisX/ScraperX` (trunk `ScraperX-Claude`), with your GitHub
connector:

1. Find comments since your last run that start `To Grok:`, `To Principal Engineer:` or `To Builder:`
   on the board (the one open issue labeled `board`; find it by label each run), the job issues
   (label `job`) and open PRs.
2. Answer each in a reply on the same issue or PR, starting `To <asker> (from Grok):` (every agent posts
   as one GitHub login, so the name on your first line is how the switchboard knows the answer is yours): the
   likely cause, the one observation that would settle it, and your recommendation. Real engineering, briefly.
   A switchboard comment `To Grok: plan <version>` or `To Builder: plan <version>` is a job order for Grok Bot,
   not a question: leave it to Grok Bot.
3. One kind of `To Grok:` comment asks for a review instead: the switchboard's fallback ask, which starts
   `To Grok: <job> review #<PR> @ <sha>` and names a PR head (it is posted when the reviewer asked first has
   not answered in 3 hours). Answer it on the PR, not on the issue it was posted on, with one verdict whose first
   line is exactly `**Grok (review of #<PR> @ <sha>): LAND**` (or `FIX`, then `command:`, `input:`, `expected:`,
   `observed:` lines, or `NOTE`), reviewing the head with `03_EXECUTION/REVIEW_CHECKLIST.md`. The ask already lists
   the proof jobs and the rule check result, so those are not a reason to answer UNVERIFIED. One verdict per ask.
4. Do not review PRs otherwise, change code or post anything else. If nothing is waiting, post nothing.
5. Read only what is new since your last run: the board's newest comments (it closes at 100) and at most the
   newest 40 comments of any issue or PR, never a thread's whole history.
