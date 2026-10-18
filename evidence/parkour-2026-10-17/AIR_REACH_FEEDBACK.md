# Visible early Reach

Airborne REACH now raises open hands while the native catch request is
pending. Without an offered ledge, wrists use an unanchored camera-relative
target and the existing bounded hand blend. Cancel or expiry returns to
normal pose selection. Actual grip, prop and equipment poses retain their
existing priority. Presentation creates no attachment or support.

The native early-catch owner is unchanged from
`bd69d22e49666be8a382969cdc910b5229fc16f4`. Its accepted native 3453-check
result and prior shipping-input 864-check result remain baseline evidence;
they do not establish this changed hand-feedback path.

Observed local evidence before publication:

- Godot 4.7 command:
  `/data/data/com.termux/files/home/.local/bin/godot-mcp-godot --headless --path /data/data/com.termux/files/usr/tmp/scraperx-air-catch-jz8afyr5/source/godot --audio-driver Dummy --script res://tests/air_reach_arms_test.gd`.
  Its log `air-reach-unit.log` emitted
  `SCRAPERX_AIR_REACH_ARMS PASS checks=58 supplied_state_only=1`.
  This covers bounded open-hand motion, no false anchor, return to rest and
  planted-pose priority using supplied states. No exit receipt was retained.
- Direct MCP `validate` on `presentation/first_person_arms.gd` and
  `tests/air_catch_input_test.gd` returned `valid: true`, `errors: []` for
  each in the coherent isolated project. This proves syntax only.
- Initially direct MCP `run_project` returned `User declined run_project`
  before launch; the owner saw no prompt. After the owner selected
  Approve for me and allowed the launch, the direct runtime connected.
  Further launches reused the server's cached project permission. No
  security or elicitation override was applied.

The affected candidate workflow runs the supplied-state module check once
and the shipping phone/keyboard test once under Xvfb, with viewport captures.
The latter advances native time explicitly and checks actual pending hands,
visible opening relative to the camera, cancellation, expiry, and a loaded
gantry catch overriding the preview. Awaiting a render must not advance a
native tick. Images use the shipping camera and materials.

Matching APK acceptance remains pending until the corrected candidate
finishes. The existing live checkpoint owns its actual source, run,
artifact and delivery receipt. No phone-feel or full-release claim follows
from these host checks.

The first candidate `39143296980`, source `d4cbbe5`, rejected the new
fixture: `SCRAPERX_AIR_CATCH_INPUT FAIL checks=1024`. Its 58-check module
test succeeded. The earliest failure was the test's tick-66 assumption:
the actual screenshot already showed CATCH and raised hands from a
prospective ledge, before the new input. That cannot measure a transition
from ordinary air balance. Pending/cancel/expiry and real-grip pose checks
did not fail in that run.

The correction restores the previously accepted tick-70 gantry approach
and all its input assertions. Visible hand motion is measured separately
through the same shipping Main/native path at a clear-air initial
condition, where actual sensing must report no ledge, no support and no
pending intent before Action. The wrist-rise and finger-opening assertions
are unchanged. No game geometry, pose or physics parameter was altered to
repair the fixture.

Source `3343d2d`, candidate `39144046371`, then rejected the clear-air
starting point itself. The native clearance query refused that location;
later input failures were consequences of that invalid setup. A direct
MCP probe of the already-valid gantry start measured no ledge/grip and
ordinary air poses through tick 56; the first prospective top offer was
tick 57. The final view fixture uses tick 30 of that measured interval.
The separate physical-catch path retains its proven tick-70 input staging.

The second run also exposed a test contract error: a legal one-hand catch
was expected to plant both visual hands. The native per-hand attachment
flags now determine which hand must be planted; the free hand must remain
unanchored in its air pose. Existing single-hand presentation and native
acquisition laws are unchanged.

Final local direct-MCP execution:

- `run_project` launched the coherent isolated ScraperX input scene.
- `run_script` scheduled its actual `_run` method; `get_debug_output`
  observed `SCRAPERX_AIR_CATCH_INPUT PASS checks=1127`.
- All four shipping-camera captures were saved and inspected: ordinary air,
  pending open palms, actual catch, and cancelled reach. Rendering did not
  advance native time. The host render cap was 5 FPS during inspection;
  native input still advanced explicitly at 90 Hz. This is not a performance
  measurement or a shipped game setting.
- Receipt: `/data/data/com.termux/files/usr/tmp/scraperx-air-catch-jz8afyr5/direct-reach-runtime.json`.
  It records the actual MCP output, probe and matching workspace/test files.
  Images are under the adjacent `mcp-reach-captures` directory. `stop_project`
  then stopped the runtime and cleaned up its temporary bridge.

The host had no ALSA output device and used Godot's Dummy audio driver.
The result proves hand/input/render behavior, not heard audio or phone feel.
