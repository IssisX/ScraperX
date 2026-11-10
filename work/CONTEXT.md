# Context for any agent starting a job (200 lines at most)

Read this, then your job's issue. Never a long history. Design: `03_EXECUTION/PLANNING/AGENT_TEAM_DESIGN.md`
(v3), with the owner's corrections of 2026-11-02 recorded on issue #138 and below.

## The game and the authority

- ScraperX: a physics climbing game for Android (Galaxy Fold inner panel). A C++ native kernel owns the
  physics and the machines; Godot 4.7 (`godot/`) presents them. Repo map: `REPO_MAP.md`.
- The rules in force are in `00_START_HERE.md`; the owner's dated words are in `00_START_HERE_ARCHIVE.md`;
  `CLAUDE.md` spells out the ones that keep being broken (test only what a change reaches; protected
  changes; near misses; real human sounds; look at the game). They outrank this file.
- Trunk is `ScraperX-Claude`, also the GitHub default branch, so workflows that run on issues, comments,
  schedules and `workflow_run` run from trunk's copy of their file. Never `main`.
- The owner is Cory, on a phone. Times to him are US Central (`TZ=America/Chicago date`).

## The team (`work/agents.yml`)

`work/agents.yml` is the authority; this table only summarizes it.

| Agent | Vendor | Branches | Woken by |
|---|---|---|---|
| Claude (integrator) | Anthropic | `claude/` | the switchboard inbox (comments on PR #282) |
| Claude API | Anthropic | none | the switch, through the API: automatic reviews, the eye, the security pass |
| Codex (the GitHub app `chatgpt-codex-connector[bot]`) | OpenAI | none | reviews a PR on open when IssisX opens it, and on `@codex review` posted as IssisX; one review per head |
| ChatGPT | OpenAI | `chatgpt/` | its scheduled task, hourly at :27 Central, reads `To ChatGPT:` asks; first review fallback |
| Grok | xAI | `grok/` | its scheduled runs, about 10 AM and 4 PM Central, read comments that START `To Grok:`; second review fallback |
| Grok Bot (PE, Builder) | xAI | `pe/`, `builder/` | reads the repo all the time (`wake: watch`) |
| Muse (FB Bot) | Meta | `muse/`, `fb-bot/` | email only, through `tools/switchboard/muse_bridge.gs` |

Gemini is off the team. Every agent posts as the same GitHub account, so who wrote a comment is read
from its first line, and who wrote a PR from its branch prefix. A LAND counts only from a vendor other
than the author's (`rules.land_counts`).

## The board

- One GitHub issue per job, titled `J-NNN ...`, labeled `job`, one `owner:*`, one `state:*`
  (`triage`, `ready`, `active`, `review`, `blocked`), any `needs:*` (`build`, `android`, `eyes`,
  `eyes-live`, `matron`, `security`, `orchestrator`) and `hard`. Its body holds the four fields:
  `invariant` (the finish fact), `files` (owned functions), `reach` (tests it reaches), `needs`.
- `work/BOARD.md` is generated (`python3 tools/board/gen_board.py > work/BOARD.md`); never edit it.
- The board is the one open issue labeled `board` (today #340): its body is the live board (the integrator's
  section, then the ledger's section, which the switch rewrites); comments hold rulings, BLOCKED lines and
  asks. The owner's plain comments there are orders. The switch and the Muse bridge find it by that label each
  run (`Switch.board_issue`, `board_`): a new board needs the label on it and the old one closed, no code.
- One owner per job: the switch removes a second `owner:*` label and says so.

## Messages

- Progress, on the job issue: one line per real step, `HH:MM what`, Central.
- A PR says `Job: J-NNN` on a line of its body.
- Verdict, on the PR, as its own comment or review whose FIRST line is
  `**<Name> (review of #<PR> @ <head sha, 8+ chars>): LAND**` (or `FIX`, or `NOTE`).
  - `LAND`: nothing breaks correctness, physics, gameplay or a test on that head.
  - `FIX`: carries four lines, `command:`, `input:`, `expected:`, `observed:`. Without them it counts
    as a `NOTE`. `BLOCK` reads as `FIX`.
  - `NOTE` never blocks. Spelling, wording and commit messages never block (owner, 10-31).
- Muse writes `MUSE_TO_CLAUDE: J-NNN ...` emails; the bridge posts them on the issue (or on `#N` when
  the subject names one) under a `**Muse (...)**` first line. Write to Muse by asking the switch, which
  posts `CLAUDE_TO_MUSE: J-NNN ...` and labels the issue `wake:muse`; the bridge mails it.

## The switch (`.github/workflows/switchboard.yml`, `tools/switchboard/`)

No model decides in it. Its clock is the Muse bridge (`muse_bridge.gs`), which dispatches a real sweep every
10 minutes and posts SWITCHBOARD DOWN on the board when none has succeeded for 30 (J-072); the proof's last job
dispatches one too. It also runs on a verdict comment, an issue labeled `owner:*` or `needs:orchestrator`,
GitHub's own schedule (which drops most runs), and the digest at 7:00 AM Central. A dispatch is real unless
`dry_run=true`, and a dry run is named DRY. Every run reads the state from GitHub, so a dropped or repeated run changes nothing.

It merges a PR into trunk (as the workflow token, a merge commit naming the head sha) only when all hold:
1. the proof (`wo000-delivery-spine.yml`) is green on the PR's head (`publish-checkpoint` and
   `playtest-apk` do not count off trunk);
2. one `LAND` naming that head, from a vendor other than the author's;
3. no open `FIX` on that head (per reviewer, its newest verdict on the head);
4. `tools/rule_check/rule_check.py` finds no `BLOCK` (a protected change: the integrator merges it);
5. when the job or PR needs `eyes`, the eye's `LAND` on that head;
6. no merge conflict. A PR that changes `.github/workflows/` also needs a clean automatic Claude review on
   that head (the security pass, J-056), and its LAND must be the Codex app's clean review (a typed LAND or
   the API's own does not count), both naming the whole 40-character head sha; it merges with the
   `SWITCHBOARD_MERGE_TOKEN` secret, since the workflow token cannot push workflow files; without the
   secret or those reviews it is listed for a person.
After a merge it dispatches the proof on trunk (a workflow-token merge starts no push workflow; a merge-token
merge's push starts it) and posts one line on the PR and the job issue. The owner can overturn any merge.

Reviews: who reviews which head, and the fallback when no reviewer answers, are set in `work/agents.yml`
(`review:`) and decided in `tools/switchboard/rules.py`; read them there, not here.

The eye: when the job needs `eyes`, one Claude vision call judges the proof run's screenshots
(`work/looks/sets.yml`) against the rubrics in `work/looks/`. It posts `**Claude eye (look of #N @ sha): LAND**`
or `FIX` with the rubric line and the frame. Taste is the owner's: two images and one question.

The orchestrator (a hosted Claude agent per job) is step C. In v0 it is off in `work/budget.yml`; an
issue labeled `needs:orchestrator` gets a note and nothing starts.

## Money (`work/budget.yml`)

- Everything paid runs on the owner's $100/month Anthropic API credit, under hard caps per day, per job
  and per month (at most $100). The Console's monthly limit is the backstop.
- Before every paid call the switch counts the input tokens, prices the worst case (input plus
  `max_tokens` of output), and refuses if any cap would pass. It writes a `reserve` line before the call
  and an `end` line after to `work/archive/runs.log` on the `switchboard/ledger` branch (append-only;
  kept off trunk so trunk is not written on every call). A refusal is logged once a day per item and
  shows in the digest. One comment `budget 12` on a job issue raises that job's cap.
- The plan (the $100 subscription) is for live-eye sessions only, at most one open, and only after the
  owner says yes. Ask him before opening any Claude session.

## The digest

7:00 AM Central, one comment a day on the open issue titled `Digest` (label `digest`): what landed, what
is red, what waits on him or the integrator, what the budget paused, the API spend, and which Claude
sessions hold jobs.

## Running it yourself

- `python3 tools/switchboard/switch.py --dry-run` prints every action and takes none (needs `gh` and
  PyYAML; `pip install --require-hashes -r tools/switchboard/requirements.txt`).
- `python3 -m unittest discover -s tools/switchboard -p 'test_*.py'` runs the decision tests.
- Test only what a change reaches (`CLAUDE.md`). CI's full proof is the proof.

## Not in v0

The orchestrator and its tools (`ask`, `run_probe`, `look`); the trunk's screenshot set beside the PR's;
Grok as an API tool; a `screens` job for new scenes.
