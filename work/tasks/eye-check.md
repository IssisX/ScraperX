# The eye check: how to look at the game (Muse and helpers, 11-07)

An eye check proves or kills one claim, with pictures and numbers. Walking around and taking
screenshots proves nothing. Cory 11-07: use the tool "with sophistication and intelligence ... not
just for kicks".

## Before you start
1. Write the claim in one sentence ("a punch to her left eye darkens her left lower lid"), and what
   you would see if it were false: the mirrored side, a flat quad, a seam, clipping, a pop.
2. Pick the tool:
   - **A posed, repeatable look at her** (marks, hair, contacts, a pose): a scripted shot test in
     the pattern of `godot/tests/injury_shots_test.gd` and `gore_shots_test.gd`: a fixed camera,
     `--fixed-fps 30`, a set state, PNGs via `--out=`. The same input gives the same picture, so
     before and after compare, and CI can run it.
   - **The live game** (light in place, UI, how a fight reads, motion): the Godot MCP
     (`run_project`, `projectPath` `godot`, `background: true`). Stage it with `run_script`
     (her state, the camera, time), not by walking there with keys: walking is not repeatable.
3. Pause the CPU jobs first (a busy box made start-up 29 s instead of 8 s).

## Staging with run_script
- Set the exact state: the camera at a known pose (position and look-at point; write the numbers
  down), her pose or state, then the event. When the claim is about play, cause the event through
  the real path (the sim's blow, contact or wind); feed the render's input directly only when the
  claim is about the render alone.
- Let it settle a fixed number of frames (physics and hair need 1-2 s), then freeze
  (`get_tree().paused = true` or `Engine.time_scale = 0`) before the shot, so two shots show the
  same moment.
- Read numbers in the same script: a bone's global transform, the region values, a distance. The
  picture says it looks right; the number says it is right.
- Left and right are hers, never the screen's: decide them in her own frame (her head bone's +X is
  her left). Her cheek marks first shipped mirrored (#280, fixed by #306); a still with only one side
  marked proves the side.

## Shots
- At least two angles that would expose the failure: front and three-quarters; add a side view
  for depth (a mark floating off the skin or sunk into it, clipping). Close for detail, wide for
  context.
- Same framing before and after. Name files by claim, angle and sha.
- Know what preview hides: `SCRAPERX_PREVIEW=1` drops shadows, quality and render scale. Judge
  shape, placement and side there; judge shadow, light and fine texture only without preview.
- Look for: the wrong side, z-fighting or flicker, a decal floating or sunk, stretched UVs, seams,
  clipping through clothes or hair, a hard edge where it should fade, and popping between frames
  (take three frames 0.1 s apart).

## After
- `get_debug_output`: a SCRIPT ERROR, `push_error` or warning in the run fails the check until it
  is explained.
- `stop_project`, every time.
- Report in five lines: claim; setup (scene or script, camera numbers, state); what you saw; the
  numbers; PASS or FAIL. Attach the stills. A FAIL names the frame and what is wrong.

## Known quirks
- A bridge click does not fire the game's buttons. Emitting the button's `pressed` signal gets you
  through menus, but checking a button itself needs a real input event (`simulate_input`).
- A fresh run opens on the microphone screen: choose NOT NOW.
- `run_script` refuses `OS.execute` and similar calls (its safety gate). Don't try to get round it.
