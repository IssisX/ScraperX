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
- Direct MCP `run_project` returned `User declined run_project` before
  launch. The owner reported that no prompt appeared. Its required launch
  confirmation therefore remains unresolved in this client; no safeguard
  or confirmation override was applied. No direct-MCP runtime claim is made
  for this revision.

The affected candidate workflow runs the supplied-state module check once
and the shipping phone/keyboard test once under Xvfb, with viewport captures.
The latter advances native time explicitly and checks actual pending hands,
visible opening relative to the camera, cancellation, expiry, and a loaded
gantry catch overriding the preview. Awaiting a render must not advance a
native tick. Images use the shipping camera and materials.

Runtime/render acceptance and a matching APK remain pending until that
candidate finishes. The existing live checkpoint owns its actual source,
run, artifact and delivery receipt. No phone-feel or full-release claim
follows from these host checks.

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
repair the fixture. Its new execution remains pending.
