# Rules every agent follows in this repo

The owner's directives, dated, are in `00_START_HERE.md`; that file is the authority. This file
holds the ones that keep being broken, so every session has them without being told.

## Test only what a change reaches

Asked again and again (2026-10-18, 2026-10-19 night, 2026-10-20, 2026-10-22): *"each change does
not require a full native suite run - dont be wasteful"*; *"try to see if just the changes could be
rerun instead of rerunning everything all the time"*.

1. While fixing one failure, rerun only that test, and only its failing part where the test lets
   you pick one (`SCRAPERX_ONLY=<group>`, `SCRAPERX_WAY=<n>`). Never edit a test to add a picker.
2. Once the fix is in, run once the native groups and touch scenarios the changed code can reach.
   Find them by looking: which functions changed, and which tests run them. Say in the report which
   you ran and why.
3. A test that passed on this exact code is not run again. Nothing runs both in a scratch copy and
   in the tree being shipped.
4. The full native suite, every touch scenario and the Android build are CI's job. Do not also run
   them locally. Before a push to `ScraperX-Claude`, run `wo000-delivery-spine.yml` once on the
   integration branch: name it `<agent>/integrate-<job>` and push it (such branches run the proof on
   push), or dispatch it. That run is the proof (off `ScraperX-Claude` it stops with the expected
   KeyError at "Preserve Android updates"). Run the full native suite locally only when a change
   reaches the whole world (the kit's body order, the world's solids, shared physics or tuning,
   the contact listener, save layout) and no CI run is coming.
5. Read the PASS/FAIL lines and the lines around a failure, not whole logs.
6. Handing work to another agent: put this section in the brief.

## Every message to the owner ends with what they do next

2026-10-22: *"always end with a clear instruction for me and the prompt if I need to share one"*.
End each message with one plain line saying what the owner should do now ("Nothing to do" is
fine). If they need to give another agent something, put the complete prompt in one copyable code
block in that same message, and name the agent it goes to. The owner is on a phone: never ask them to
edit, combine or fill in a prompt ("use the earlier prompt with one change"). Give the final text, whole.

## Decisions come pre-digested

2026-10-27: *"you can't give me these large blocks of text ... break the stuff down in simple terms
for me every time from now on ... It's difficult for me to make decisions when I have to read a ton
of shit and put it all together myself."*
Every decision put to the owner is two or three short lines: what it is, Claude's pick, the word
the owner answers with. No block of text, no table of numbers or file names. The detail lives in the
document the item links to, never in the message.

2026-10-27, 4:00 PM Central: *"If I don't answer your questions within a reasonable amount of time
to keep the coding flowing, go with your recommendations. Let's go with 5 or 10 minutes."* A
question with no answer after 10 minutes is decided by the pick it carried: say so on the work's
issue, go on, and the owner can still overturn it later.

## Times are Central, never UTC

2026-10-18, again 2026-10-20 and 2026-10-27: *"Are you using UTC or Central Time Chicago like you
should? If using UTC, please switch permanently."* Every time given to the owner, in chat, on
GitHub or in a document, is US Central, read with `TZ=America/Chicago date` ("4:41 PM Central"),
never a fixed offset. CI, GitHub and `date` report UTC: convert before writing a time down.

## Near misses: tweak the world, don't rework her

Said more than once (2026-10-21, 2026-10-22: *"I don't like that so many tests fail for such
minuscule distances ... yes, you can change geometry"*). When a test or route fails by a small
distance (a few cm to about 0.2 m), a small, unnoticeable change to the world's geometry is allowed
and preferred over reworking code. Say so when you use one. Test assertions still never weaken.

## Human sounds are real, never synthesized in code

2026-10-27, after listening to her new bank: *"you can't create a voice with some sort of synthesized
shit like that. It never works. That's why I had you make new stuff, because we had the same shit.
So you just did the same shit."* A human sound (a voice, a growl, a laugh, breath, a body's effort)
and gore foley (a rip, a strain, a swallow) is a real recording cleared for the game (CC0, its source
written down), or a voice made the way the climber's fear yells were (`src/voice/generate_fall_voice.py`).
Code may still make steel, air, steps and beds. The owner picks the candidates by ear.

## Managing the agents for the owner

2026-10-22: *"I need more management help from you telling the other agents what to do ... I'm on
a phone"*, then *"Way better ... keep trying to manage our processes in a more intelligent manner"*.
- Issue #3 holds one ASSIGNMENT BOARD: every agent's queue in order, who integrates whose work
  (never their own), and the rules. Agents work down their queue without waiting for the owner,
  post after each step, and mark ⏳/✅ on the board. Update the board instead of handing the owner
  separate prompts.
- The owner gets one universal prompt ("read the board, do your queue") to paste into any agent.
  The owner can also give orders as a plain comment on issue #3, and every agent follows them.
- Keep offering better ways to manage the work when you see them.
- Progress over testing (2026-10-22: *"we want them to figure out hard coding problems and make
  progress, not just test shit"*). Give agents real problems to solve in code; tests are the proof,
  not the job. A blocker is reported only after a real attempt at the fix. Reports stay short.
- Hand out the hard work (2026-10-27, 6:00 PM: *"give them jobs that you would have to do if they
  weren't here to do them for you, making progress quicker. And always do this. And don't give them
  cheap, weak shit. They are intelligent ... know what they're handling ... always making progress
  towards the difficult stuff. Efficiently"*). Whenever your own queue holds hard work another agent
  can own, hand it out on issue #3, whole: the goal, why it is hard, the laws, the files, the finish
  facts. Know what each agent holds before you assign. Check issue #3 often (every hour, from a server-side scheduled
  task, so the check and the resume after a quota stop or a container restart happen on their own;
  the owner watches usage too) and post there only when something changed. ChatGPT is not one of the
  agents (2026-10-27: *"ChatGPT is not part of this"*).
- This holds for all work, not only climbing. Issue #3 is the climbing lab; work on anything else
  gets its own GitHub issue with its own short board in the same format, linked once from #3.

## The shared workflow (2026-10-27)

The owner, 2026-10-27, approving the workflow upgrades: *"All of it, docs included."*
These hold for every agent; `AGENTS.md` points other agents here.

- **Map first.** `REPO_MAP.md`: key files, key functions with line numbers, every test group and
  touch scenario. Regenerate it with `python3 tools/repo_map/gen_repo_map.py` when it drifts.
  Finished and superseded rows of `00_START_HERE.md` are in `00_START_HERE_ARCHIVE.md`.
- **One handoff form.** Work handed to another agent or to the integrator uses
  `03_EXECUTION/handoffs/TEMPLATE.yml`, posted as a comment on issue #3 (or the work's own issue)
  addressed to the agent it is for. Not a prompt pasted to the owner.
- **Rule check, then a second agent tries to break it.** Before landing, run
  `python3 tools/rule_check/rule_check.py <base> <head>`. It flags removed or loosened test checks,
  per-route special cases, velocity and position writes in her code, and changes to her budget
  numbers. A BLOCK line means the change goes only as a PR into `ScraperX-Claude`, which only the
  owner merges. A second agent then reviews with `03_EXECUTION/REVIEW_CHECKLIST.md`.
- **Protected changes.** Test checks, world geometry, and her force/physics limits change only by a
  PR into `ScraperX-Claude`, merged by the owner. Everything else lands without a PR.
- **Local tests match CI or do not count.** A local build used as evidence uses CI's compilers and
  flags (host g++ 13.3 as on `ubuntu-24.04`; Android NDK `29.0.14206865` clang; the CMake flags in
  `wo000-delivery-spine.yml`). Before a push, run only the groups the change reaches. CI stays the
  final proof. CI uses ccache; a cache only skips identical compiles and changes no result.
- **Progress, in small steps.** A job writes one line per real step to its progress log
  (`HH:MM what`, Central time). Issue #3 gets a post only when something changed.
- **Cheap agents.** A lower-cost agent may run tests, builds and the rule check. It may not delete
  branches, force-push, push to `main`, or land work.
