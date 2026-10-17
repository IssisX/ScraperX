# Climbing handoff baseline

Base: f7e792843deca85827ab48a4034e8634a83cb5fb, ChatGPT.

Exact-source GitHub full run38969060848 fails AS-026 receiver arrest and AS-024 loaded cargo pause. The receiver reaches1.76349m on its left hand at tick1550, beyond the existing1.75m envelope; the next tick releases both hands. AS-024 fails lane20 after lane19 passes; the combined assertion does not identify the first changed field.

The same published native source and test inputs pass Linux ARM64 against the source-matched startup archive: suspended ladder exit0 in52.15s; cargo route lanes19/20/21 exit0 in114.29s. Commands, outputs and archive/source hashes are retained in `/data/data/com.termux/files/usr/tmp/scraperx-climbing-handoff-20261017/baseline-result.json`. This ARM result cannot stand in for failing x86 behavior.

Diagnostic commit3f391aa adds output and enables only the two existing checks in the existing focused native CI runner. No physical law, test input, assertion, threshold or deadline changes. The next repair must follow the first invalid x86 transition; no shared cause is assumed. Existing catch-net and inspection failures stay deferred.

## Supported fixture corrections

The production physics is unchanged. The independent radial-hand contract rejects hidden tangential braking or retained Cartesian preload (`physical_hand_climb_tests.cpp`, `passive_radial_arc` and `ForceObserver`). The receiver route now uses the existing walking driver's position/velocity feedback, through ordinary finite air input, toward its already-required8.25m inboard stance. Input never exceeds the former0.7 side throw. An added assertion requires actual absorbed air-control work before acquisition; existing two-hand catch, arrest, shimmy, force/work/reach, top-out, deadline and exit checks remain.

The AS-024 product contract requires idle sag, real hand load/material deflection, stop/resume and onward footing; it does not ban valid incidental foot contact with the net. Focused x86 sample137 retains both hands and gravity while a real point0.350573m from the lower capsule-cap center contacts the cloth (capsule radius0.35m). Hiding that contact would violate the native authority contract. The corrected pause still requires180 consecutive genuinely unsupported solves with the original mean rider-weight range, zero added command work and material deflection. A real cloth bottom-cap contact resets that load window. One additional two-second settling interval bounds the entire acquisition/measurement to360ticks; perpetual support or a missing180-tick hand-only window still fails. Foreign/side contacts and a powered foot push cannot qualify. The20mm contact margin follows pinned Jolt PhysicsSettings::mPenetrationSlop, not a new physical tolerance.

Actual command/output receipts and current check results are recorded beside this source in `CAUSAL_DECISION.json` and the private matching-source execution receipts. ARM and x86 evidence remain separately identified. Existing catch-net and inspection failures are not repaired or hidden by this change.
