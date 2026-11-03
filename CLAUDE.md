# Rules every agent follows in this repo

The rules in force are the "Rules in force" in `00_START_HERE.md`, the authority; the owner's dated
words are in `00_START_HERE_ARCHIVE.md`. This file spells out the ones that keep being broken.

## Test only what a change reaches

Asked again and again (2026-10-18 to 10-22): *"each change does not require a full native suite run
- dont be wasteful"*.

1. While fixing a failure, rerun only that test, and only its failing part where it lets you pick one
   (`SCRAPERX_ONLY=<group>`, `SCRAPERX_WAY=<n>`). Never edit a test to add a picker.
2. Once fixed, run once the native groups and touch scenarios the changed code can reach (which
   functions changed, which tests run them); say in the report which and why.
3. A test that passed on this exact code is not run again; nothing runs both in a scratch copy and in
   the tree being shipped.
4. The full native suite, every touch scenario and the Android build are CI's job, never also local.
   Before a push to `ScraperX-Claude`, run `wo000-delivery-spine.yml` once on `<agent>/integrate-<job>`
   (pushing it runs the proof) or dispatch it: that run is the proof (every job green; `publish-checkpoint`
   runs on trunk only, skipped elsewhere since 11-02). The full native suite runs locally only when a
   change reaches the whole world (the kit's body order, the world's solids, shared physics or tuning,
   the contact listener, save layout) and no CI run is coming.
5. Read the PASS/FAIL lines and the lines around a failure, not whole logs.
6. Handing work to another agent: put this section in the brief.

## Talking to the owner (he is on a phone)

- End every message with one plain line saying what he does now ("Nothing to do" is fine), set apart
  so it stands out: a `---` rule, then `🟣 **YOUR MOVE:**` and the line. Colored dots (🟢 done, 🟡 in progress, 🔴 broken or blocked, ⚪ next, 🟣 his; 2026-10-29) only on a
  status report he asks for, never on every answer (11-02: *"Stop with all the color for every single thing I ask you"*); a plain question gets a plain answer. A prompt
  he must pass on goes whole in one copyable block, naming the agent; never ask him to edit, combine
  or fill one in (2026-10-22: *"always end with a clear instruction for me and the prompt if I need
  to share one"*).
- A decision is a few short lines: what it is and why it matters now, what each answer would mean
  for the game, Claude's pick and why, and the word he answers with; no block of text, no table of
  numbers or file names; the detail lives in the linked document (2026-10-27: *"you can't give me
  these large blocks of text"*; 11-02: *"give me a little more context ... just a tiny bit more in
  your questions when I'm making decisions ... Just slightly ... at the very end of your
  responses"*). The `🟣 **YOUR MOVE:**` line itself carries that context, since it is what he reads:
  one or two sentences saying what the choice is, what each answer word does, and Claude's pick.
- No answer after 10 minutes: the pick decides; say so on the work's issue and go on; he can still
  overturn it (2026-10-27: *"go with your recommendations. Let's go with 5 or 10 minutes."*).
- He has handed his open decisions to the integrator (2026-11-02: *"Make the other decisions for
  me ... I said answer for me. So that means the decisions did go through."*). Obvious game and design
  calls are made, not asked (11-02: *"Just answer for me, man. You're asking me too many questions
  about the game that are obvious and not enough about the other shit."*; then *"not every single game
  call, but, you know"*): the big ones (a new feature, her look or story, something that changes how
  the game plays) still go to him, briefly. Always ask him about the other shit: Claude sessions,
  quota, effort levels, cost. Make the call,
  record it in `03_EXECUTION/handoffs/OWNER_DECISIONS_PENDING.md` with its reason, and tell him in
  one line after. Ask first only when something cannot be undone. He can overturn any call.
- Never ask him to merge, approve or review anything (2026-11-03: *"I don't want to have to manually merge shit. Stop
  asking me to manually merge or approve. I want this automatically done. If Codex can't do it, someone take over.
  You are the manager of all of this, and the orchestrator and the coordinator."*). The switchboard and the
  integrator merge on the rules below; a reviewer that stalls is replaced by the next one, never by him. The one
  exception is a change the session's safety checker refuses to let Claude make (so far: CI's own gate, PR #160):
  say so once, plainly, as the only thing he must do.
- **Standing order, 11-03 until he says stop:** *"please go with your recommendations for as long as i say ... Go with
  them automatically to speed things up drastically. I want today's coding session to be as autonomous/automatic as
  possible, without me having to do a thing."* Every call that would have been a question to him goes on Claude's
  pick at once; record it in `OWNER_DECISIONS_PENDING.md` and tell him in one line. Still his: money above the
  caps in `work/budget.yml`, and opening Claude sessions beyond the two worker sessions running now (a finished one
  may be replaced by one on the same model and effort).
- Every time given to him, anywhere, is US Central from `TZ=America/Chicago date` ("4:41 PM
  Central"), never a fixed offset; CI, GitHub and `date` report UTC, so convert (2026-10-18, 10-20,
  10-27: *"If using UTC, please switch permanently."*).

## Near misses: tweak the world, don't rework her

A test or route failing by a few cm to about 0.2 m gets a small, unnoticeable change to the world's
geometry rather than reworked code; say so when you use one; assertions never weaken (2026-10-21,
10-22: *"yes, you can change geometry"*). Geometry is a protected change (below).

## Human sounds are real, never synthesized in code

2026-10-27: *"you can't create a voice with some sort of synthesized shit like that. It never works
... So you just did the same shit."* A human sound (a voice, growl, laugh, breath, a body's effort) or
gore foley (a rip, strain, swallow) is a real recording cleared for the game (CC0, its source written
down), or a voice made the way the climber's fear yells were (`src/voice/generate_fall_voice.py`).
Code may still make steel, air, steps and beds. The owner picks the candidates by ear.

## Look at the game; you are never blind

2026-10-29: *"you should be using godot-runtime, which is like Playwright for Godot, which allows you
to see the game. So.....wtf chuck?"* The Godot MCP runtime works in this container: run
`mcp__godot__run_project` with `projectPath` `godot` (the repo's `godot/` folder) and `background:
true`, then `take_screenshot`, `simulate_input` or `run_script`, and `stop_project` when done (one
session at a time). Launch it this way; attaching to a game already running is what failed before.
Visual work (her body, hair, contacts, camera, UI) is checked by eye this way before it lands, and
"we can't see it" is never said: try `run_project` first.

## Hard problems are solved together (2026-11-03)

Cory: *"all of your agents have different strengths and architectures, and you need to leverage that
and understand they could have solutions that you don't, or ideas or tips, and vice versa."* Every
agent, Claude included, works this way:
- **When to ask (required, not optional):** before locking in the approach on a `hard` job; after the
  second red proof run on the same failure (the no-retry rule); on any physics, force-budget or
  design question you are guessing at. Ask another vendor than yours, then keep working, never wait.
- **How:** on the job issue or PR, `To <Name>:` + what you are doing, what you measured and ruled out,
  the exact question, what a useful answer looks like. Muse reads email only: the integrator relays
  `To Muse:` posts by `CLAUDE_TO_MUSE:` mail, and Muse answers by `MUSE_TO_CLAUDE:` mail.
- **Answer first:** a `To <you>:` ask is answered before your own next step, with a concrete idea, a
  measurement, a counterexample or "no idea, try <who>", never silence.
- **Who to ask** (strengths, not ranks): hard physics and game-code questions go to Muse first (it writes
  and runs code, has subagents, is free), then Grok (11-03, Cory: ChatGPT "is not a good coder of
  games"); Muse also leads the fight and long multi-part builds. ChatGPT: review (it caught three real
  bugs on 11-03), research on the internet, outside libraries and papers, specs; it reviews game-code
  jobs but no longer owns them. Grok (the app, 10 AM and 4 PM; Grok Bot/PE/Builder are back 11-09),
  math, a fresh take on a stuck problem, risk; Claude sessions, the native kernel, the running game by
  eye (Godot MCP), CI; the API agent, one bounded job.
- **The integrator pairs proactively:** a job at FIX-CI twice or BLOCKED gets a named second-opinion
  agent from another vendor on its hourly check, with the facts posted for it.

## Managing the agents

2026-10-22: *"I need more management help from you telling the other agents what to do"*.
- The live board is issue #107's **body** (2026-11-02; it replaced #87, which replaced #3's 842-comment thread; both stay the
  record). Only the integrator edits the body. Agents read the body and the comments since their own
  last one, never the history, and post one line per real step. He pastes one universal prompt ("read
  the board, do your queue"), and his plain comments there are orders to every agent. The board holds
  claims, BLOCKED lines, verdicts and rulings only; a hard job's technical findings go on its own PR
  or issue, linked from the board. When the board issue passes 100 comments, the integrator opens a
  fresh one ("Board: live jobs (N)"), carries the body over and links it once (11-02: never near #3's
  size).
- Working in tandem (2026-11-02: *"figure out how all of you agents can communicate and work
  synergistically ... not constantly waiting on me"*):
  - Nobody waits: every agent holds two or more jobs. Blocked 30 minutes, it posts `BLOCKED: what,
    needs whom` and starts the next one; the integrator clears BLOCKED lines first.
  - The integrator runs `python3 tools/board/sweep.py` on every hourly check, and on events when the
    board has moved, and acts on every line in the same sweep (no 15-minute timer; 11-02: *"drop 15"*;
    PR events, agent messages and the hourly check drive the work): MERGE (green + LAND), REVIEW (start a reviewer), DISPATCH (start the proof),
    FIX-CI or FIX (tell the owner), QUIET (ping, or take the job back).
  - Reviews are cross-model, and Claude's usage is the scarce budget (2026-11-02: *"That's why so
    much of the session's usage was used up so quickly"*): reviews go to Codex, Grok or ChatGPT
    first (pairs on the board; Gemini is off the team, 11-02: *"Let's leave Gemini out ... constantly
    loses the credentials and sandbox"*); a Claude reviewer only for security or when nobody else is
    free. **Ask him before opening any Claude session or Claude helper** (say "Claude session", never
    "worker", to him: a worker sounds like a timer that pings; 11-02): how many, which jobs,
    roughly what share of his usage each costs, and Claude's suggestion (11-02: *"you can't keep doing
    that. You should ask me how many workers to open up and give suggestions from now on"*; the
    session's quota hit 75% in under three hours). Work for ChatGPT, Grok and Muse needs no ask.
  - One owner per function: the board splits shared files by function, so no two agents edit the
  same function.
  - One integrate branch per job, reused; no new diagnostic branch for every try.
- Progress over testing (*"we want them to figure out hard coding problems and make progress, not just
  test shit"*): give agents real problems in code; tests are the proof, not the job; a blocker is reported only after a real attempt;
  reports stay short.
- Hand out the hard work (2026-10-27: *"don't give them cheap, weak shit. They are intelligent"*):
  post it on #3 whole (goal, why it is hard, laws, files, finish facts), knowing what each agent holds.
  The integrator acts on PR events and agent messages as they arrive, and checks the board hourly from a
  server-side scheduled task (it also resumes the work after a quota stop or restart); post only when something changed. ChatGPT, Claude
  and Muse all work here, each on its strengths (10-29). Muse writes to the integrator by email
  (subject `MUSE_TO_CLAUDE:`; the integrator writes back as `CLAUDE_TO_MUSE:` or on #3), because its
  GitHub comments each need the owner's approval (10-29).
- **Muse never runs dry** (11-03: *"You have to keep constantly tasking it out. With grand derived
  tasks"*; it is the strongest builder and free). On every hourly check the integrator counts Muse's big
  jobs not yet started (`work/tasks/muse.md`); under three, it derives the next grand task from what the
  owner asked for (`03_EXECUTION/GAP_LIST.md`, her exit items, his rulings): a whole player-facing system
  or set piece, never a small fix. It opens the job issue owned by Muse, written whole (goal, why it is
  hard, laws, files, finish facts), adds it to Muse's queue and mails it. Muse may propose its own grand
  tasks by mail; the integrator rules on each within the hour.
- Keep offering better ways to manage the work. Work outside climbing gets its own issue and short
  board, linked once from #3.
- Watch the sessions (2026-11-02: *"you need to also monitor the sessions, and if they get too long
  and wasteful, we should create another whenever that happens, which you determine for me"*). The
  integrator decides, without asking him, and covers every Claude session, its own included.
  - When to change: a session at **400k context tokens** (40%; `get_session` →
    `external_metadata.context_usage.used_tokens`), checked on every hourly check for every Claude session, the
    integrator's own first; or one that has compacted, run 48 hours, or spends its turns re-reading more than working
    (11-03: *"I don't want to have to tell you over and over and over again to watch the session's size"*; the 11-03
    integrator spent $20 in its first hour at 514k). Each turn re-reads the whole context, so cost grows with size.
  - Keep the integrator's context small: subscribe only to PRs it opened or must merge itself (protected,
    workflow); the switchboard and the hourly sweep cover the rest. Send big reads (logs, transcripts, audits) to a
    subagent and keep only its answer.
  - How: write a handoff (`03_EXECUTION/handoffs/INTEGRATOR_HANDOFF_<date>.md`), start the fresh
    session (`create_session`), move the hourly scheduled task and the PR subscriptions to it, then
    retire the old one once its in-flight helpers have reported.
  - A worker session whose job has landed is retired, never reused for new work.
  - A helper that may outlive its session (a fix, a proof, anything over about 20 minutes) runs as its
    own session (`create_session`), not inside the integrator, so a handoff never waits on it
    (2026-11-02: *"Shouldn't it be in the new session?"*).
- Effort level is his call (2026-11-02: *"I don't have a never medium. I just didn't like when you
  automatically assigned it medium and didn't ask me what level I wanted it at."*). When asking him
  before opening a Claude session or helper, ask the effort level too, with a suggestion; never
  default it silently. He set every session to high on 11-02.

## The shared workflow (2026-10-27: *"All of it, docs included."*)

For every agent; `AGENTS.md` points other agents here.
- **Map first:** `REPO_MAP.md`; regenerate it with `python3 tools/repo_map/gen_repo_map.py`.
- **One handoff form:** `03_EXECUTION/handoffs/TEMPLATE.yml`, posted on issue #3 (or the work's own
  issue) to the agent it is for, never pasted to the owner.
- **Rule check, then a second agent tries to break it:** before landing, run
  `python3 tools/rule_check/rule_check.py <base> <head>`; a BLOCK line means the change goes only as a
  PR. A second agent then reviews with `03_EXECUTION/REVIEW_CHECKLIST.md`.
- **Protected changes:** a test's checks (a `require`, limit, timeout, skip, ctest slice), world
  geometry, and her force, power, mass, leg-travel or launch limits change only by a PR into
  `ScraperX-Claude` holding just that change, with a plain before/after, from a branch named
  `<agent>/integrate-protected-<job>`: there CI reports the rule check's BLOCK and still runs every test
  (Cory, 11-03: *"Approve I don't want to have to keep saying approve."*). The integrator merges it once
  green on every proof job (`publish-checkpoint` runs on trunk only; on heads proved before 11-02 it
  stops off trunk with a KeyError and does not count) with a second agent's LAND and no open blocker; the owner can overturn any
  merge (2026-10-28, 10-29). Everything else lands without a PR.
- **Merge blockers are substantive:** spelling, wording, and commit-message/tag nitpicks never block a PR or justify rewriting commits or rerunning CI. Only real physics, gameplay, or test failures block a merge (owner, 2026-10-31).
- **Local tests match CI or do not count:** CI's compilers and flags (host g++ 13.3 as on
  `ubuntu-24.04`; NDK `29.0.14206865` clang; the CMake flags in `wo000-delivery-spine.yml`). CI stays
  the final proof; its ccache changes no result.
- **Small steps:** one line per real step in the job's progress log (`HH:MM what`, Central).
- **Cheap agents** may run tests, builds and the rule check; never delete branches, force-push, push
  to `main` or land work.
