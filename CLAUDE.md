# Rules every agent follows in this repo

The rules in force are in `00_START_HERE.md`; the owner's dated words are in `00_START_HERE_ARCHIVE.md`.
This file is only the rules that change what an agent does. Cory, 11-04: *"we're getting a little heavy on
the safety and rules and like bookkeeping shit"*: add a rule here only by replacing or shortening one.

## Test only what a change reaches

*"each change does not require a full native suite run - dont be wasteful"* (10-18 to 11-04).
- While fixing a failure, rerun only that test or its failing part (`SCRAPERX_ONLY=<group>`, `SCRAPERX_WAY=<n>`).
- Once fixed, run once the native groups and touch scenarios the changed code reaches; say which.
- Never re-run a check that passed on the same code. The full suite, every touch scenario and the Android
  build are CI's job: push `<agent>/integrate-<job>` (that runs the proof). A push that changes only notes,
  rules or the switchboard skips the game build (`ci/proof_scope.py`).
- Read PASS/FAIL lines and the lines around a failure, never whole logs.
- Local tests count only with CI's compilers and flags (host g++ 13.3, NDK `29.0.14206865`, the CMake flags
  in `wo000-delivery-spine.yml`).

## Talking to Cory (he is on a phone)

- Plain, short answers; technical detail only when he asks. Times in US Central (`TZ=America/Chicago date`).
- End every message with a `---` rule, then `🟣 **YOUR MOVE:**` and one line: what he does now ("Nothing to
  do" is fine). For a choice, that line says what each answer word does and Claude's pick. A prompt he must
  pass on goes whole in one copyable block naming the agent.
- **Go with your pick on every call until he says stop** (11-03), and tell him in one line. Still his: Claude
  sessions, quota, effort, cost, and money above `work/budget.yml`; the big game calls (a new feature, her
  look or story, how the game plays). No answer in 10 minutes: the pick stands.
- Never ask him to merge, approve or review. The only exception is something this session's safety check
  refuses: say it once, plainly.
- Find better ways, always, and say them unasked (11-04: he found /compact himself).

## The game

- **Near misses:** a test or route failing by a few cm to about 0.2 m gets a small, unnoticeable world
  geometry tweak, never reworked code or a weaker assertion. Geometry is a protected change.
- **Human sounds are real:** a voice, growl, breath, effort or gore foley is a cleared recording (CC0, source
  written down) or made like the fear yells (`src/voice/generate_fall_voice.py`), never synthesized in code.
  Cory picks by ear.
- **Look at it:** visual work (her body, hair, contacts, camera, UI) is checked by eye before it lands:
  `mcp__godot__run_project` with `projectPath` `godot` and `background: true`, then `take_screenshot` /
  `simulate_input` / `run_script`, then `stop_project`. Never say "we can't see it".

## How the agents work together (keep this whole)

- **Channels.** Muse reads only mail: the integrator mails every `To Muse:` ask as `CLAUDE_TO_MUSE: <J-NNN or
  #N> ...` the same hour; Muse answers by `MUSE_TO_CLAUDE:` mail (its diffs come as attachments). Grok's runs
  (10 AM and 4 PM) find only comments that START `To Grok:`. ChatGPT finds `To ChatGPT:` asks and signs its
  posts. Every agent posts as one login, so the name goes on the first line.
- **Ask, don't guess:** before locking in a hard job's approach, after the second red proof on the same
  failure, or on any physics or force-budget guess, post `To <Name>:` on the job's issue or PR (what you did,
  measured, ruled out, the exact question) and keep working. A `To <you>:` ask is answered before your next
  step.
- **Strengths:** Muse (free; writes and runs code, has its own subagents) takes the hardest builds, physics
  and game code first, and leads the fight. Grok: math, risk, a fresh take on a stuck problem. ChatGPT:
  research, specs, outside libraries, review. Claude: integration, the native kernel, CI, the game by eye.
  Grok Bot, PE and Builder are out until 11-09.
- **Board:** issue #107's body is the live board; only the integrator edits it. Agents read the body and
  the comments since their last, post one line per real step, and put technical findings on the job's own
  issue or PR. Blocked 30 minutes: post `BLOCKED: what, needs whom` and start the next job; every agent holds
  two or more jobs.
- **Reviews, one per PR:** the switchboard routes PRs whose body starts `Job: J-NNN`. Codex reviews every PR
  on open (`@codex review` only after a fix commit). The Claude API (Opus, max effort) does the eye, the
  security pass on CI and signing changes, and any head Codex has not reviewed. Grok and ChatGPT review only
  as the fallback, by their own scheduled runs. The switchboard merges on a green proof plus a LAND from
  another vendor, protected changes included; only workflow-file changes need the integrator.
- **Protected changes** (a test's checks, world geometry, her force/power/mass/leg-travel/launch limits)
  go alone in a PR from `<agent>/integrate-protected-<job>`, with a plain before/after; merged on a green
  proof plus another agent's LAND. Run `python3 tools/rule_check/rule_check.py <base> <head>` before landing:
  a BLOCK means it is protected.
- Only real physics, gameplay or test failures block a merge, never wording or commit messages.
- One owner per job and per function; one integrate branch per job, reused. A PR replaced by newer work is
  closed the same hour.
- Cheap agents may run tests, builds and the rule check; never delete branches, force-push, push to `main`
  or land work.

## The integrator

- Tasks the whole team every hour, unasked (`work/tasks/integrator-hourly.md`): every agent loaded by
  strength, Muse never without three big unstarted jobs, Muse's proposals ruled on within the hour, every
  ask answered.
- **Claude sessions:** one Opus session (the integrator), at most two active helpers (three if acute), each
  code-editing helper in a worktree. Ask Cory before opening any other Claude session. Effort: integrator
  ultracode, never workflows; Opus workers high; Sonnet xhigh. A worker whose job landed is retired.
- **Context:** sessions compact themselves at about 400k (`autoCompactWindow` in `.claude/settings.json`);
  no handoffs. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md` (asks in flight, merge queue, live
  rulings) current; re-read it, this file and the hourly file after a compact. A new session only when one
  is broken.
- **Keep reads small:** subscribe only to PRs you merge yourself; big reads (logs, transcripts, long threads)
  go to a helper and only its answer comes back; read the function or section a job needs, found via
  `REPO_MAP.md` or `grep`, never the whole game. Caps: the board 100 comments (then a fresh board), a job
  issue or PR 40, this file 10 KB, each `work/tasks/` file 8 KB.
