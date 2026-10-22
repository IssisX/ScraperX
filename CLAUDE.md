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
   them locally. Before a push to `ScraperX-Claude`, dispatch `wo000-delivery-spine.yml` once on the
   `claude/*` branch: that run is the proof (off `ScraperX-Claude` it stops with the expected
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

## Near misses: tweak the world, don't rework her

Said more than once (2026-10-21, 2026-10-22: *"I don't like that so many tests fail for such
minuscule distances ... yes, you can change geometry"*). When a test or route fails by a small
distance (a few cm to about 0.2 m), a small, unnoticeable change to the world's geometry is allowed
and preferred over reworking code. Say so when you use one. Test assertions still never weaken.

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
- This holds for all work, not only climbing. Issue #3 is the climbing lab; work on anything else
  gets its own GitHub issue with its own short board in the same format, linked once from #3.
