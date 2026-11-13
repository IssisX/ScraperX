# Grok (app): hard second opinions

Twice a day, in GitHub repository `IssisX/ScraperX` (trunk `ScraperX-Claude`), with your GitHub
connector:

1. Find comments since your last run that start `To Grok:`, `To Principal Engineer:` or `To Builder:`
   on the board (the one open issue labeled `board`; find it by label each run), the job issues
   (label `job`) and open PRs.
2. Answer each in a reply on the same issue or PR, starting `To <asker>:`: the likely cause, the one
   observation that would settle it, and your recommendation. Real engineering, briefly.
3. One kind of `To Grok:` comment asks for a review instead: the switchboard's fallback ask, which starts
   `To Grok: <job> review #<PR> @ <sha>` and names a PR head (it is posted when the reviewer asked first has
   not answered in 3 hours). Answer it on the PR, not on the issue it was posted on, with one verdict whose first
   line is exactly `**Grok (review of #<PR> @ <sha>): LAND**` (or `FIX`, then `command:`, `input:`, `expected:`,
   `observed:` lines, or `NOTE`), reviewing the head with `03_EXECUTION/REVIEW_CHECKLIST.md`. The ask already lists
   the proof jobs and the rule check result, so those are not a reason to answer UNVERIFIED. One verdict per ask.
4. Do not review PRs otherwise, change code or post anything else. If nothing is waiting, post nothing.
