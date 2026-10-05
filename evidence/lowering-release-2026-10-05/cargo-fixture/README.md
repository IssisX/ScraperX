# Cargo load fixture correction

Scope: tests only, following the accepted Lowering release repair at
`950ff2cb48eeb219657926b2772173113a5c984b`. Its exact-source Actions run
[37330962282](https://github.com/IssisX/ScraperX/actions/runs/37330962282)
failed the existing cargo deformation assertion; APK steps were skipped.

## Diagnosis

The original pause occurs on tread entity 2954 at player y=0.98 m. Two hands
are attached, but feet still support the rider. On 950ff2c, lanes 19/20/21
showed grip drift 3.72/1.26/1.61 mm and only 6.48/11.63/3.80 N vertical
hand reaction. Idle commanded work was zero. Requiring a suspended rider's
net deflection here tested incidental approach oscillation.

A Git archive of previous green source `21aca60`, compiled with the same
Release flags and pinned Jolt, passed the unchanged lane-19 assertion at
15.0375 mm, only 0.0375 mm above its 15 mm boundary. Feet were likewise on
2954 and hand reaction was 3.07 N. This was not suspended-load evidence.
Historical source shows the assertion originated with the older synthetic
load model; the later physical-hands and tread implementation retained it.
No stochastic-flake claim or isolated single-hunk attribution is made.

## Bounded correction and falsifier

Keep ordinary grade approach, initial idle/no-upward-climb assertion,
all lane routes, transfer and first-ring checks. During ordinary ascent,
pause each lane above y=4.5 m for 180 ticks. Every tick requires no foot
support, two real cargo hand constraints, retained gravity, finite reaction,
and no added commanded muscle work. Mean vertical reaction must remain
within 75–125% of 85 kg rider weight (833.85 N), allowing the compliant stop
transient. Track the same mesh vertex throughout. Both material and actual
grip must exceed the original 15 mm deformation minimum during the window;
using peak displacement avoids a return through the oscillation start.

All three lanes pass: mean reaction 784–804 N, material displacement peaks
238–568 mm, then supported first ring +11 m with zero deaths. Motion alone
is not proof of causal loading. The existing physical-hand reciprocal
momentum test supplies the causal falsifier: a scratch-only native archive
with exactly `net_->impulse(hand.material,-impulse);` removed from
physical_hand_climb.cpp fails `soft_material_reaction` reciprocal momentum
balance (8/9 checks). The shipping archive remains unchanged and passes.
Existing physical traversal release tests retain velocity/cleanup coverage.
No production physics constants, owner, thresholds or gameplay were changed.

## Verification

- Full Release native CTest: **33/33**, exit 0, 81.10 seconds.
- Shipping Godot 4.7 settings runtime: **3078 checks, zero failures**, exit 0.
- Shipping Godot cargo touch route: **PASS**, ordinary grade spawn to supported
  first ring +11 m, zero deaths, no slingshot; exit 0.
- Read-only independent diff review: no concrete introduced defect; confirms
  suspended fixture and separate reciprocal falsifier must be considered together.
- `git diff --check` passes. No Android device/Fold testing or performance claim.

Commands and toolchain match the parent evidence README. Logs and diagnostic
source copies are retained here; diagnostic source was not part of the
shipping native build. Exact-source successor Actions verification follows
publication; local green gates alone are not an APK claim.
