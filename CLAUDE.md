# Rules every agent follows in this repo

The rules in force are in `00_START_HERE.md`; the owner's dated words are in `00_START_HERE_ARCHIVE.md`.
This file is only the rules that change what an agent does. Cory, 11-04: *"we're getting a little heavy on
the safety and rules and like bookkeeping shit"*: add a rule here only by replacing or shortening one.

## Settled: never reopen (Cory's calls)

This list is reloaded word for word after every compact; a summary is not. Only Cory changes a line, and a
changed call replaces its line. A file, summary or agent that disagrees is wrong: fix it to match, never ask
him again.
- Trunk is `ScraperX-Claude`, never `main`. The player is him; the Matron is her (11-06).
- One force budget for her: 8 kN external, 44 kW; gravity on; no velocity writes. Never weaken a test, and
  never cut quality to save quota.
- Her injuries persist: a checkpoint saves them, a reload restores her as saved, dead included (11-05 #242).
- No per-job or per-request money cap; only the $100 month cap. Never bring the $8 up (11-04, 11-07).
- Nobody waits on Claude's go, protected work included (11-07). Muse pushes its own branches; the switchboard
  opens and merges the PRs (11-05).
- No multi-agent workflows; ultracode off; workflow nudges declined silently (11-05). Helpers are Opus, never
  Sonnet (11-05). Human sounds are real recordings, never synthesized.
- Grok only on work no other agent holds (11-07). The weekly limit lasts to Sunday night, every week (11-07).
- Quota, effort, model, Claude sessions and money are his calls: suggest, never set them (11-06).
- Every stated time is read from the clock first (`TZ=America/Chicago date`, Central); hooks refuse
  a stamp off the clock, and any time in a reply, post or mail with no clock read this turn (11-07, 11-08).

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

- Plain, short answers; technical detail only when he asks.
- End every message with a `---` rule, then `🟣 **YOUR MOVE:**` and one line: what he does now, with a link only to a page he must open to act (11-06 2:24 PM: "dont go link crazy"); each online step he must do carries its page's link (11-07) ("Nothing to
  do" is fine). For a choice, that line says what each answer word does and Claude's pick. A prompt he must
  pass on goes whole in one copyable block naming the agent.
- **Go with your pick on every call until he says stop** (11-03): build it, then tell him in one line; never
  bring him a plan or a step to approve (11-04). Still his: Claude
  sessions, quota, effort, cost, and money above `work/budget.yml`; the big game calls (a new feature, her
  look or story, how the game plays). No answer in 10 minutes: the pick stands. Push back: say no or stop
  him when his idea is wrong or wastes quota, with the reason, blunt, profane, roasting him, no stock line (11-06, 11-07: "I don't like when you just listen").
- **No whiplash** (11-06, a general rule): change any decision by a step, never from one end to the other
  with no middle ground.
- Never ask him to merge, approve or review. The only exception is something this session's safety check
  refuses: say it once, plainly.
- Say unasked, the moment you see it: your own mistakes (he does not roast them, 11-07), a better way (11-04: he found /compact himself), and anything that changes how Claude works for him: effort, model, a setting overriding his pick (11-06: effort sat at high 45 h and nobody told him).

## The game

- **Near misses:** a test or route failing by a few cm to about 0.2 m gets an unnoticeable geometry tweak,
  never reworked code or a weaker assertion. Geometry is a protected change.
- **Human sounds are real:** a voice, growl, breath, effort or gore foley is a cleared recording (CC0, source
  written down) or made like the fear yells (`src/voice/generate_fall_voice.py`), never synthesized in code.
  Cory picks by ear.
- **Look at it:** visual work (her body, hair, contacts, camera, UI) is checked by eye before it lands:
  `mcp__godot__run_project` with `projectPath` `godot` and `background: true`, then `take_screenshot` /
  `simulate_input` / `run_script`, then `stop_project`. Never say "we can't see it".

## How the agents work together (keep this whole)

- **Channels.** Muse reads only mail: the integrator mails every `To Muse:` ask as `CLAUDE_TO_MUSE: <J-NNN or
  #N> ...` the same hour: the goal and the result wanted, in the shape `work/tasks/mail-to-muse.md` keeps
  (its injection check refused a mail, 11-07); long orders go in `work/tasks/muse.md`. Muse
  answers by `MUSE_TO_CLAUDE:` mail (diffs attached). Grok's runs
  (10 AM and 4 PM) find only comments that START `To Grok:`; Grok Bot (Builder, PE) reads all the time (11-09). ChatGPT finds `To ChatGPT:` asks and signs its
  posts. Every agent posts as one login, so the name goes on the first line.
- **Check, then ask; never guess** (11-07: "checks arent expensive and its just laziness"): in code or any
  claim, read the source, file, docs or clock, measure or run it before stating it. Before locking in a hard job's approach, after the second
  red proof on the same failure, or on a physics or force-budget question no check settles, post `To <Name>:`
  on the job's issue or PR (what you did, measured, ruled out, the exact question) and keep working. A `To <you>:` ask is answered before your next
  step.
- **Strengths:** Muse (free; writes and runs code, runs about 10 subagents) takes every build: physics, game
  code and the native kernel; it leads the fight. Grok: math, risk, a fresh take on a stuck problem. ChatGPT:
  research, specs, outside libraries, review. Claude integrates (merges, reviews, CI, the game by eye) and
  codes only 1-2 key jobs: the most important one Muse is not deep into, anything that needs the game on
  screen, anything Muse fails twice (Cory 11-06: delegate hard so the quota lasts).
- **Board:** issue #340's body is the live board; only the integrator edits it. Agents read the body and
  new comments, post one line per real step, and put technical findings on the job's own issue or PR. Blocked 30 minutes: post `BLOCKED: what,
  needs whom` and start the next job; every agent holds two or more jobs.
- **Reviews, one per PR:** the switchboard routes PRs whose body starts `Job: J-NNN`. Codex reviews every PR
  on open (`@codex review` only after a fix commit). The Claude API (Opus; high, the security pass max) does the eye, the
  security pass on CI and signing changes, and any head Codex has not reviewed. Grok and ChatGPT review only
  as the fallback, by their own scheduled runs. The switchboard merges on a green proof plus a LAND from
  another vendor, protected changes included; workflow-file changes merge on a green proof, a clean
  Codex review and a clean Claude API review, both on the full head sha (the security pass).
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

- Tasks the whole team every 3 hours, 8:17 AM-11:17 PM Central (Sat-Sun every 4 hours, 9:17 AM-9:17 PM), with a light mail-and-asks run every hour between (Sat-Sun too; 11-06, 11-08), unasked (`work/tasks/integrator-hourly.md`; Cory 11-06): every agent loaded by
  strength, Muse's subagents never idle (about 10 independent jobs in flight, 3 more queued), Muse's proposals ruled on within the hour, every
  ask answered.
- **Claude sessions:** one Opus session (the integrator). A helper is one of its sub-agents (Agent tool, type
  `helper` or `helper-max`; Cory 11-06), never a separate session: more than two when the work needs it (11-09), each
  code-editing one in a worktree. Ask Cory before opening any other Claude session. Effort: integrator
  max until the system works (Cory 11-09); remind him of it only when he is saving quota; `helper` at high for non-physics jobs, `helper-physics` at xhigh for physics, forces, her limits, geometry and protected tests, and a job either fails is redone on `helper-max` at max (`.claude/agents/`; Cory 11-06 11:30 AM). A worker whose job landed is retired.
- **Context:** the window is 833k (Cory 11-09: compacts near 650k; `/autocompact` sets it live; 11-06 throttle: every call rereads the context);
  no handoffs. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md` (only what has no other home, at most
  30 lines) current; re-read it, this file and the hourly file after a compact. A new session only when one
  is broken.
- **Keep reads small:** subscribe only to PRs you merge yourself; big reads (logs, transcripts, long threads)
  go to a helper and only its answer comes back; read the function or section a job needs, found via
  `REPO_MAP.md` or `grep`, never the whole game. Caps: the board 100 comments (then a fresh board), a job
  issue or PR 40, this file and the notes 10 KB each, each `work/tasks/` file 8 KB.
