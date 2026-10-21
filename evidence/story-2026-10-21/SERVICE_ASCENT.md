# Freight One service ascent — 2026-10-21

## Result

One ordinary CLIMB UP request at a passive duct hang now commands the existing finite hand pull. Actual shoulder reach starts the existing physical top-out; actual cabinet, duct, vent and tower contacts carry the rest of the route. The 22 m receiver plays Iona's recorded rail-and-ladder direction. The maintained service finish continues onto that actual equipment; no arrows, navigation placards or permanent objective card are added.

## Cause and repair

The native intent waited for shoulder-to-ledge distance at most1.05m without commanding the pull that could close the gap. Existing explicit touch tuck proved the available actuator could perform it. Native arbitration now requests that same shared0.25m posture allowance while a bounded request remains valid on the exact two grip generations. Existing500W supplied spring work,0.6m/s target stroke,1500N hands, gravity, clearance, receiver identity and actual contact gates are unchanged. Cancellation/release/regrip/restart cannot replay the request; rejected clearance retains the real hang.

The maximum preparation lifetime is146native ticks. The existing ordinary eleven-tick jump buffer remains unchanged. This duration is a bounded command policy, not a promise that every catch is reachable or settles in time.

## Executed acceptance

- `SERVICE_ASCENT_NATIVE.json` includes exact commands/output: five unchanged intent/lifecycle cases reused, three actual duct cases and default/blocked shared top-out pass. Jump and Action each raise the actual body0.127889m before transfer; no shimmy or explicit tuck. A real shared step measured428.896N hand load and98.1484N foot load.
- `SERVICE_ASCENT_MCP.json` contains the actual Godot MCP calls/source hashes/native identity and result:107checks, zero failures, default explicit-tuck optionfalse, one declared initial11m stage, all later shipping touch. Cabinet→duct→vent→90firm22mreceiver ticks→supported onward tower11walking. Finaltick2354, position(22.043518,22.9,-125.180473), gravity1, no deaths, hands released, packaged rail recording playing.
- `SERVICE_ASCENT_ASSETS.json` retains changed guidance40checks,31-cue generation/manifest/hash/non-silent RMS and runtime import commands/results. Prior30clips are retained. `OPENING_CI_3651447.json` independently records the earlier no-stage normal-spawn→11m journey passing568checks in its matching green candidate.

## Test corrections disclosed

The first new power assertion used the conservative Fmax-distance receipt against the500W supplied-energy limit. The actuator source separates those quantities. The corrected test checks positive spring energy against500W and independently retains force, speed and shared stroke bounds. Its original failure remains in the native receipt.

The old cancellation comparison expected Action's first tick of real work to disappear. Both comparison worlds now perform the same initial physical preparation; all exact state/load/grip/contact comparisons remain. A separate duct case cancels after automatic work and rejects any delayed transfer even after reach becomes sufficient.

## Limits

The MCP viewport had3Drendering disabled and used Dummy audio. Stream selection/playing was observed; speaker output, whole-world appearance, phone feel/FPS and the following33m touch leg were not tested. The local coherent native library predates one explanatory comment correction only; final candidate CI must build the published batch. Matching APK acceptance is recorded in the live checkpoint after CI; this document does not claim it already exists. The separate365full suite's rail, west-brace and unknown fork-exchange failures remain full-release blockers.
