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
