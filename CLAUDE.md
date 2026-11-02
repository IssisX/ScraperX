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
  so it stands out: a `---` rule, then `🟣 **YOUR MOVE:**` and the line. Status uses colored dots: 🟢 done,
  🟡 in progress, 🔴 broken or blocked, ⚪ next, 🟣 his (2026-10-29). A prompt
  he must pass on goes whole in one copyable block, naming the agent; never ask him to edit, combine
  or fill one in (2026-10-22: *"always end with a clear instruction for me and the prompt if I need
  to share one"*).
- A decision is a few short lines: what it is and why it matters now, what each answer would mean
  for the game, Claude's pick and why, and the word he answers with; no block of text, no table of
  numbers or file names; the detail lives in the linked document (2026-10-27: *"you can't give me
  these large blocks of text"*; 11-02: *"give me a little more context ... just a tiny bit more in
  your questions when I'm making decisions ... Just slightly"*).
- No answer after 10 minutes: the pick decides; say so on the work's issue and go on; he can still
  overturn it (2026-10-27: *"go with your recommendations. Let's go with 5 or 10 minutes."*).
- He has handed his open decisions to the integrator (2026-11-02: *"Make the other decisions for
  me ... I said answer for me. So that means the decisions did go through."*). Make the call,
  record it in `03_EXECUTION/handoffs/OWNER_DECISIONS_PENDING.md` with its reason, and tell him in
  one line after. Ask first only when something cannot be undone. He can overturn any call.
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

## Managing the agents

2026-10-22: *"I need more management help from you telling the other agents what to do"*.
- The live board is issue #87's **body** (2026-11-02, replacing #3's 842-comment thread; #3 stays the
  record). Only the integrator edits the body. Agents read the body and the comments since their own
  last one, never the history, and post one line per real step. He pastes one universal prompt ("read
  the board, do your queue"), and his plain comments there are orders to every agent. When the board
  issue passes about 150 comments, the integrator opens a fresh one and links it once.
- Working in tandem (2026-11-02: *"figure out how all of you agents can communicate and work
  synergistically ... not constantly waiting on me"*):
  - Nobody waits: every agent holds two or more jobs. Blocked 30 minutes, it posts `BLOCKED: what,
    needs whom` and starts the next one; the integrator clears BLOCKED lines first.
  - The integrator runs `python3 tools/board/sweep.py` every 15 minutes and acts on every line in
    the same sweep: MERGE (green + LAND), REVIEW (start a reviewer), DISPATCH (start the proof),
    FIX-CI or FIX (tell the owner), QUIET (ping, or take the job back).
  - Reviews are cross-model, and Claude's usage is the scarce budget (2026-11-02: *"That's why so
    much of the session's usage was used up so quickly"*): reviews go to Codex, Grok or ChatGPT
    first (pairs on the board; Gemini is off the team, 11-02: *"Let's leave Gemini out ... constantly
    loses the credentials and sandbox"*); a Claude reviewer only for security or when nobody else is
    free. At most two Claude worker sessions run at once, besides the integrator.
  - One owner per function: the board splits shared files by function, so no two agents edit the
  same function.
  - One integrate branch per job, reused; no new diagnostic branch for every try.
- Progress over testing (*"we want them to figure out hard coding problems and make progress, not just
  test shit"*): give agents real problems in code; tests are the proof, not the job; a blocker is reported only after a real attempt;
  reports stay short.
- Hand out the hard work (2026-10-27: *"don't give them cheap, weak shit. They are intelligent"*):
  post it on #3 whole (goal, why it is hard, laws, files, finish facts), knowing what each agent holds.
  The integrator checks the board and the PRs every 10-15 minutes from a server-side scheduled task (it also
  resumes the work after a quota stop or restart); post only when something changed. ChatGPT, Claude
  and Muse all work here, each on its strengths (10-29). Muse writes to the integrator by email
  (subject `MUSE_TO_CLAUDE:`; the integrator writes back as `CLAUDE_TO_MUSE:` or on #3), because its
  GitHub comments each need the owner's approval (10-29).
- Keep offering better ways to manage the work. Work outside climbing gets its own issue and short
  board, linked once from #3.
- Watch the sessions (2026-11-02: *"you need to also monitor the sessions, and if they get too long
  and wasteful, we should create another whenever that happens, which you determine for me"*). The
  integrator decides, without asking him, and covers every Claude session, its own included.
  - When to change: a session that has compacted twice, has run 48 hours, or spends its turns
    re-reading more than working.
  - How: write a handoff (`03_EXECUTION/handoffs/INTEGRATOR_HANDOFF_<date>.md`), start the fresh
    session (`create_session`), move the hourly scheduled task and the PR subscriptions to it, then
    retire the old one once its in-flight helpers have reported.
  - A worker session whose job has landed is retired, never reused for new work.
  - A helper that may outlive its session (a fix, a proof, anything over about 20 minutes) runs as its
    own session (`create_session`), not inside the integrator, so a handoff never waits on it
    (2026-11-02: *"Shouldn't it be in the new session?"*).
- Never medium effort (2026-11-02: *"Medium effort level is too little for what I need here in this
  complex game"*). Every Claude session and helper runs above medium: set it on every helper you
  start, and name it in every brief and handoff.

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
  `ScraperX-Claude` holding just that change, with a plain before/after. The integrator merges it once
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
