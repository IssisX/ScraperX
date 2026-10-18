# Ground two-bar swing transfer

Base source: `337cf5bcd0e04202a045fab5b9f965bd270d8d07`.

The ground bay near `(-50, -59)` has ordinary concrete steps, two independently hinged steel bars, an east rest pad and open ground for retries. Jump from the entry plinth to the first bar, lean with its swing and press Action when the next bar is within reach. After the first hand crosses, lean toward the receiving bar and press Action again. Jump or Drop lets go; the contextual button says REACH BAR. Surface arrows explain the route without supplying collision.

Each Action replaces only the actual trailing hand. The other hand keeps its support-local anchor and generation. Candidates require the existing visible part approach, 1.05 m shoulder reach, 1.75 m body reach, distinct palms and eighteen-tick placement cadence. A missing or unreachable candidate rejects Action and retains the actual hands. Mixed support stays under the existing Cartesian swing owner; principal support changes only after both hands reach the receiving bar.

Bounded torso progression uses one existing finite hand command per tick, sharing its stroke/work limits with ordinary lean and posture. Full standing endpoint and sweep clearance reject obstruction. Goal rebasing retains the current commanded point. Jolt retains gravity, velocity, real body momentum and reciprocal load. No actuator laws or force/work limits changed. Each passive hanger has twelve kilograms of declared constituent mass; mating-frame collision is enabled through real open bearings.

Drop now cancels held automatic reach across nearby bodies. Fresh Action/Jump, neutral stick or actual footing rearms it. This prevents an explicit drop from immediately grabbing a different nearby bar. Restart clears that intent and the transient hand topology.

## Observed execution

[swing-transfer-window.json](swing-transfer-window.json) records exact commands, source hashes, outputs and retained failures. A strict Release C++17 build compiled all 79 native translation units. The added private World field required rebuilding all 26 actual header dependants before relinking; only unchanged non-World objects were reused.

- `swing_transfer_tests flow`: exit 0, actual earned jump/catch; missing-bar Action rejection; one-hand crossing at tick92; thirty neutral mixed-support solves; second crossing at tick231; Drop with forward held; real landing and sixty ticks of onward movement.
- `swing_transfer_tests release`: exit 0, Drop during mixed support remains released until real ground.
- `swing_transfer_tests jump`: exit 0, Jump releases mixed support under ordinary gravity.
- `swing_transfer_tests restart`: exit 0, retry clears hands/plan while retaining both real bars.

Every mode checks native fixed ticks, survival, unchanged gravity, finite forces/work, a single changed generation per placement and the planted hand in its own moving material frame. Mixed-support acceptance requires measured load in both hands and actual receiving-bar motion. A separate uninterrupted candidate completed both placements at tick272 and landed at tick338 after the Drop repair.

The first implementation could not bring the torso across; finite torso-target progression repaired that cause. The first completed transfer exposed unintended automatic reacquisition after Drop; the input owner now cancels it. A test driver then pumped the old bar backwards after a neutral pause. It now follows the same forward-lean instruction shown to the player; no reach, force, pause, placement or cancellation assertion was removed.

This slice does not implement radial/material cross-body regrips, arbitrary grip supports or articulated limbs. The host staged only initial actual entry footing. Normal from-spawn discovery, phone controls/feel and full commercial release remain unverified. Matching development APK CI owns full Godot import and export evidence.
