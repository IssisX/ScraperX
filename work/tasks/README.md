# Scheduled-task instructions

Each outside agent's scheduled task (ChatGPT, Grok) carries a one-line prompt that points at a file
here. The real instructions live in these files, so a rule, a job or a board change is one edit in
the repo and never an edit to the owner's tasks (Cory, 11-03: "as autonomous and automatic as
possible").

- `chatgpt-review.md`: ChatGPT's hourly task and its GitHub-PR-activity task (same file).
- `chatgpt-features.md`: ChatGPT's feature-proposal task (Mon, Wed, Fri).
- `grok-answers.md`: the Grok app's task (10 AM and 4 PM Central).
- `muse.md`: Muse (FB Bot), who works by email through the bridge.
- `grok-engineer.md`: Grok Bot's engineering task (Grok, Principal Engineer, Builder), on a schedule only.

**Read for the task, never the whole game** (Cory, 11-03: the game is too big to hold at once, and
reading whole documents is what has been wasteful). Find the place first (`REPO_MAP.md`'s line, or a
search for the function), then read only that function or section. Long docs (`GAP_LIST.md`,
`REPO_MAP.md`, `00_START_HERE.md`, specs) are searched by heading and read by section. Threads: the
body plus what is new since your last read, never the history.

The board is found by label, never by number: **the one open issue labeled `board`** (today #340).
When it fills, the integrator opens the next one and moves the label; every task follows by itself.
