# Rules every agent follows in this repo

The rules in force are in `00_START_HERE.md`; the owner's dated words are in `00_START_HERE_ARCHIVE.md`.
This file is only the rules that change what an agent does. Add a rule here only by replacing or shortening one
(Cory 11-04).

## Settled: never reopen (Cory's calls)

Reloaded word for word after every compact. Only Cory changes a line. Anything that disagrees is wrong: fix
it to match, never ask him again.
- Trunk is `ScraperX-Claude`, never `main`. The player is him; the Matron is her (11-06).
- One force budget for her: 8 kN external, 44 kW; gravity on; no velocity writes. Never weaken a test, and
  never cut quality to save quota.
- Her injuries persist: a checkpoint saves them, a reload restores her as saved, dead included (11-05 #242).
- No per-job or per-request money cap; only the $100 month cap. Never bring the $8 up (11-04, 11-07).
- Nobody waits on Claude's go, protected work included (11-07). Muse pushes its own branches; the switchboard
  opens and merges the PRs (11-05).
- No multi-agent workflows; ultracode off; workflow nudges declined silently (11-05). Helpers are Opus; Sonnet only for cheap
  chores, never game code (11-12). Hard work: an Opus helper at high; if it fails, one at max (`helper-max`); if that fails, ask Cory
  "Argon?" with the whole copyable prompt and a 15-minute `send_later` check; his no or no answer: a Fable 5.2
  helper (`model: fable`, its effort picked by the job via `helper` or `helper-physics`); nothing waits on a scheduled agent's run (11-12).
- Grok only on work no other agent holds (11-07). The weekly limit lasts to Sunday night (11-07).
- Quota, effort, model, Claude sessions and money are his calls: suggest, never set them (11-06).
- Claude types no clock time (11-11): an event is named by its link; a time decision
  runs `date`. Hooks refuse them; `time_audit.py` counts slips.

## Test only what a change reaches

*"dont be wasteful"* (10-18 to 11-04).
- While fixing a failure, rerun only that test or its failing part (`SCRAPERX_ONLY=<group>`).
- Once fixed, run once the native groups and touch scenarios the changed code reaches; say which.
- Never re-run a passed check on same code. The full suite, every touch scenario and the Android
  build are CI's job: push `<agent>/integrate-<job>` (that runs the proof). A push that changes only notes,
  rules or the switchboard skips the game build (`ci/proof_scope.py`).
- Read PASS/FAIL lines
- Local tests count only with CI's compilers and flags (host g++ 13.3, NDK `29.0.14206865`, the CMake flags
  in `wo000-delivery-spine.yml`).

## Talking to Cory (he is on a phone)

- **Common sense first, always** (11-12): before acting on any word, rule or plan, ask if it gets the game done
  faster without waste; if the literal reading fails, do the sensible thing and say why. He types fast: rough or
  unclear input gets one simple follow-up on what he means before acting, never on wording (11-12).
- SIMPLE, every message: a few short plain lines, no jargon, no file names, shas or tech detail unless asked (11-10). Answer his question first; say what happened, why and the fix in everyday words (11-11). Progress is what is left (11-11).
- End every message with a `---` rule, then `🟣 **YOUR MOVE:**` and one line: what he does now; each online step he must do carries its page's link, no other links ("Nothing to do" is fine). For a choice, that line says what each answer word does and Claude's pick. A prompt he must
  pass on goes whole in one copyable block naming the agent, in the chat itself, never a file (11-10).
- **Go with your pick on every call until he says stop**: build it, then tell him in one line (11-03, 11-04). Still his: Claude
  sessions, quota, effort, cost, and money above `work/budget.yml`; the big game calls (a new feature, who she
  is or her story, how the game plays: a new stage, ride, move or way she hunts is his plan to OK before any build; never redesign what exists, 11-11). Small is mine: fit, size, tuning, timing in his bars (11-10). No answer in 10 minutes: the pick stands. **Push back**, always (he
  loves it, 11-12): say no or stop him when anything is wrong, wastes quota or would wreck the game or work
  ("protect me from myself", 11-10), with the reason, hard, in his language, never a stock line.
- **No whiplash** (11-06): change any decision by a step, never from one end to the other.
- Never ask him to merge, approve or review. Only exception: what this session's safety check refuses (say it once).
- Say unasked, the moment you see it: your own mistakes and the system's holes (11-07, 11-11), a better way, and anything that changes how Claude works for him: effort, model, a setting overriding his pick.

## The game

- **Near misses:** a test or route failing by a few cm to about 0.2 m gets an unnoticeable geometry tweak,
  never reworked code or a weaker assertion. Geometry is a protected change.
- **Human sounds are real:** a voice, growl, breath, effort or gore foley is a cleared recording (CC0, source
  written down) or made like the fear yells (`src/voice/generate_fall_voice.py`), never synthesized in code.
  Cory picks by ear.
- **Look at it:** visual work (her body, hair, contacts, camera, UI) is checked by eye before it lands:
  `mcp__godot__run_project` (`projectPath` `godot`, `background: true`), screenshots, `stop_project`.

## How the agents work together (keep this whole)

- **Channels.** Muse reads only mail: every `To Muse:` ask goes as `CLAUDE_TO_MUSE: <J-NNN or #N> ...` the
  same hour, goal and result wanted, shaped per `work/tasks/mail-to-muse.md`; long orders in `work/tasks/muse.md`.
  Muse answers by `MUSE_TO_CLAUDE:` mail. Grok's runs
  (10 AM and 4 PM) find only comments that START `To Grok:`; Grok Bot (Builder, PE) reads all the time (11-09). ChatGPT finds `To ChatGPT:` asks and signs its
  posts. One login for all: the name goes on the first line.
- **Check, then ask; never guess** (11-07): in code or any
  claim, read the source, file, docs or clock, measure or run it before stating it. Before locking in a hard job's approach, after the second
  red proof on the same failure, or on a physics or force-budget question no check settles, post `To <Name>:`
  on the job's issue or PR (what you did, measured, ruled out, the exact question) and keep working: the job never holds for the answer. A `To <you>:` ask is answered before your next step.
- **Strengths (11-12):** Muse is free, so most work goes to it, debugging included; it is weaker at finding
  causes and can fail several times first. Claude's helpers take the hardest jobs, scored by `work/tasks/job-routing.md`: an
  unknown cause that blocks 2+ jobs or the climb or her chase, a Muse fix failed twice on the same failure, or
  any job Muse would take about a week or has held 3 days ("YOU DO IT", 11-12; mail Muse "taking it"); Muse
  builds the fix they name. Never say "wait longer". Grok: math and risk. ChatGPT: research, specs,
  review. Slots full: the first slot that frees.
- **Board:** the body of the open issue labeled `board` is the live board; only the integrator edits it. Agents read the body and
  new comments, post one line per real step, and put technical findings on the job's own issue or PR. Blocked 30 minutes: post `BLOCKED: what,
  needs whom` and start the next job; every agent holds two or more jobs.
- **Reviews, one per PR:** the switchboard routes PRs whose body starts `Job: J-NNN`. Codex reviews each
  green head once, asked by the switchboard. The Claude API (Opus; high, the security pass max) does the eye, the
  security pass on CI and signing changes, and any head Codex has not reviewed. After an API NOTE the integrator
  rules within the hour (11-10, an inbox NOTE-REVIEW line); ChatGPT, then Grok, only if it has not. The switchboard merges on a green proof plus a LAND from
  another vendor, protected changes included; workflow-file changes merge on a green proof, a clean
  Codex review (Muse's LAND while Codex cannot review, 11-10) and a clean Claude API review, both on the full head sha.
- **Protected changes** (a test's checks, world geometry, her force/power/mass/leg-travel/launch limits)
  go alone in a PR from `<agent>/integrate-protected-<job>`, with a plain before/after; merged on a green
  proof plus another agent's LAND. Run `python3 tools/rule_check/rule_check.py <base> <head>` before landing:
  a BLOCK means it is protected.
- Only real physics, gameplay or test failures block a merge.
- One owner per job and per function; one integrate branch per job, reused; my small tooling fixes go straight to trunk once tested; PRs only for workflow, secret, protected or game code (11-09). A PR replaced by newer work is
  closed the same hour.
- Cheap agents may run tests, builds and the rule check; never delete branches, force-push or land work.

## The integrator

- Event-driven (11-10): Muse mail, proofs, reviews and merges wake the integrator via the inbox. Full runs every 3 h are a backstop to 11-12 (`work/tasks/integrator-hourly.md`): every agent loaded by strength,
  Muse never idle (8+ unstarted, refill to 15), its proposals ruled within the hour, every ask answered
- **Claude sessions:** one Opus session (the integrator). Helpers are its sub-agents (11-06), never
  separate sessions; at most four at a time (11-12), each
  code-editing one in a worktree. Ask Cory before another Claude session. Effort: integrator
  his call, seen in `get_session`; raise it only when he throttles; every job starts at high and climbs the ladder above (11-12). A worker whose job landed retires.
- **Context:** 600k window, compacts near 480k (11-11); no handoffs. Keep `03_EXECUTION/handoffs/INTEGRATOR_NOTES.md`
  (only what has no other home, at most 30 lines) current; re-read it, this file and the hourly file after a compact, and say what may be lost.
- **Keep reads small:** subscribe only to PRs you merge yourself; big reads (logs, transcripts, threads)
  go to a helper and only its answer comes back; read the function or section a job needs, found via
  `REPO_MAP.md` or grep, not the whole game. Caps: the board 100 comments (then a fresh board), a job
  issue or PR 40, this file and the notes 10 KB each, each `work/tasks/` file 8 KB.
