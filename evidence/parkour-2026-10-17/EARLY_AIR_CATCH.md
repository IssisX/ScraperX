# Early airborne reach

An airborne Action can wait briefly for a real grip to enter hand reach.
The phone offers **REACH** before a grip is available and **CATCH** when
native geometry reports one. While the request is pending, **CANCEL** uses
the existing Drop input. Keyboard E and Q retain Action and Drop.

The native owner reuses `kOrdinaryJumpIntentTicks`: 11 fixed 90 Hz ticks,
approximately 122 ms including the request tick. Each retry queries current
grip geometry and existing clearance. Pending input stores no anchor, grants
no support, and changes no hand reach, force, work, gravity or release law.
A successful acquisition consumes the request once. Expiry rejects it once.
Drop, crouch, restart, grounding or incompatible tool/attachment ownership
clears it. Drop wins over Action within the same input batch.

`Snapshot::air_catch_intent_pending`, exposed in the existing landing-state
dictionary, supplies waiting/cancellation feedback. Godot cannot extend the
window or decide that a physical catch succeeded. The separate grapple
equipment control remains available while the tool is stowed.

Accepted baseline checks for the native/input implementation published as
`bd69d22e49666be8a382969cdc910b5229fc16f4`:

- `air-catch-tests` exited 0: `SCRAPERX_AIR_CATCH PASS checks=3453`.
  Actual shipping gantry 2952 covers an early loaded catch, expiry/fresh
  retry, Drop/crouch cancellation, same-batch ordering, restart and
  equivalent 90/360 Hz caller traces.
- Godot 4.7 ran `res://tests/air_catch_input_test.tscn` with
  `--quit-after-air-catch-check` through shipping Main and the matching
  native bridge: `SCRAPERX_AIR_CATCH_INPUT PASS checks=864`.
- The first actual MCP/Main run found a touch-input defect: a shown,
  enabled Cancel was ignored while its visual fade was below 0.2. The
  final run measured fade 0.088889 and proved the actual Drop verb queued
  and cancelled the pending catch. Fade now affects presentation only;
  hidden, disabled and already-held buttons still reject presses.

The MCP launch/observation attempts are retained separately from the direct
Godot success. An MCP click-to-start returned no target hit and did not
execute the check; it is not counted as a passing game test. Direct Godot
and the existing CI path keep APK delivery independent of that automation.

Required delivery evidence:

- `scraperx_air_catch_intent_tests`: final native owner and pending readback.
- `res://tests/air_catch_input_test.tscn`: shipping Main, real keyboard/touch
  events, early phone Action, pending feedback, Cancel and actual loaded catch.
- The exact-source green APK candidate, which runs both checks and produces
  its normal artifact-bound Android receipt. Device feel remains unmeasured.

Local experiment history is retained in `air-catch-window.json`. The final
source/run/artifact receipt belongs in the existing live checkpoint.
The subsequent visible open-hand change and its separate acceptance are
documented in [AIR_REACH_FEEDBACK.md](AIR_REACH_FEEDBACK.md).
