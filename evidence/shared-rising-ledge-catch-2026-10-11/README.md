# Ordinary rising-jump ledge catch — 2026-10-11

This bounded integration is complete. Native rising acquisition, existing
thin-edge/ledge checks, exact-bridge headless touch and the maintained simulation
suite pass. The exposed carry-driver regression is corrected with ordinary
per-leg facing input, retaining its physical arrival checks and budget.

The ledge acquisition path previously returned while vertical speed exceeded
0.2m/s, although reachable bars already permitted rising catches. The repair
removes only this ledge arbitration veto. Existing ground/crouch/carry/intent
refusals, original COM reach and unobstructed actual lip checks, regrab lockout,
finite hands, gravity and body motion remain authoritative.

## Rejecting baseline

Baseline HEAD `fb5da165ac636aa20483db5d2eadf8d81a767362` has Simulation SHA
`63decfa120157d6686e483626c209138332e60cafcb76185d6168020e0d158fa` and verified
native archive `aa19c19dd0110d6d153ed3b75465e942761ecc17deb0cd97db67bc9fb300de0d`.
Apply `baseline-test.patch` to reconstruct test source SHA
`6280a05095daa910d282076624800194e0cb7af17912ec6775692495a89dfbdb`.
Compilation with the unchanged warning checks passes; actual native execution
exits1 on the first-catch rising-flight assertion.

Existing RegressionFixtures ledge7 is approached from initial-only placement
`(7.6,0.95,4)`. The player settles on actual ground for45ticks, faces/moves+X
and requests one ordinary Jump. No later placement, velocity write, Action,
second Jump or new geometry is used. Actual takeoff speed is5.391m/s.
At36flightticks COMY2.2934/vertical speed1.576m/s is within the original catch-height band,
yet no catch occurs. The first actual two-hand Hanging7 occurs at50flightticks,
with incoming/outgoing vertical speeds0.158998/0.0596616m/s. Actual lips have
rise1.18917m and distance1.29747m; force/work checks pass before the rising oracle
fails. The test requires preceding speed>1m/s and outgoing speed>0.5m/s.

One initial compile launcher selected an inherited archive path and was stopped
before runtime. Its private directory is retained. The recorded baseline uses
the corrected explicit verified thin-edge archive; no aborted build is reported
as behavioral evidence.

## Ownership and limits

Super_Agent MCP session `01a12c9d-549d-7322-bb5b-2872f8fb7d84` changed only
Simulation.cpp: one veto removed and two comment lines added. Final source SHA
`fa9cda0e37720b713a61ac8af1544af6ba6562f5f9aeb86ec3e28f66160e5f60`.
Root inspected the actual diff. Independent Sol/High review found no blocker,
verified unchanged acquisition invariants and confirmed that the velocity oracle
rejects the recorded delayed catch. It proves rising acquisition, not a complete
impulse ledger; force bounds remain owned by the actual finite hand backend.

Broader mass/shape policy, articulated limbs and phone feel remain open. No new
Android build, fullGREEN delivery or physical-device observations are claimed.


## Initial new-source execution and captured affected failure

Actual first catch occurs35flightticks after the one ordinary Jump, versus50
in the baseline. Incoming/outgoing vertical speeds are1.794/1.60065m/s. Both
actual lips have rise1.34283m and distance1.43962m; two hands, full gravity,
airborne ownership and finite force/work checks pass. Existing thin-edge and
static/moving ledge modes also pass. New bridge SHA256 is
`5f18c9daa29900a52554e206d2cee824ed3069a6a10e80f1312e79be49cd2fab`.
Its complete headless touch encounter passes receiver/recovery/retry/onward
with zero deaths. These are distinct proof boundaries from the failed suite.

The simulation suite exits1 at “the carry must reach the handoff deck”. It
reaches receiver45, then the second loaded steering leg overshoots targetZ−109
to−107.417 and loses support. Both old/new loaded phases retain block56 and
traversalNone; acquisition is excluded while an actual carry constraint exists.
The first logged difference is already at the stair foot, after the same Hook5
instance's pre-pickup belt jump/grab. A private exact-prefix old/new comparison
is being prepared to discriminate an earlier changed catch from other state
divergence. No carry/locomotion repair is justified by the present evidence.

Root verified all recorded current source hashes, dependencies, step logs and
binaries, including the actual suite failure and exact-bridge touch success.
The last complete verified source remainsfb5da16; this candidate remains local.


The identical private extracted Hook5 probe now discriminates the source change:
verified thin archive exits0 on actual handoff45 at tick19642, while rising
archive exits1 with the same support loss at tick19592 as the full suite. Both
compile with unchanged warning checks. Original maintained simulation tests are
untouched. `carry-pair-probe.patch` reconstructs the probe from their exact
source hash; `carry-pair-receipt.json` records both archive identities. The
introduced outcome is established; an earlier changed catch and later load
reorientation still need execution-level discrimination.

The driver updates Facing toward its positional error every tick. Native carry
uses facing to rotate its real point-constraint anchor at up to3rad/s, producing
reciprocal physical impulses. A waypoint crossing can therefore turn the load
while the movement input is braking. A private one-heading-per-leg experiment
is pending, retaining both targets, original movement law, settled speed/distance,
real height/contact/support/carry assertions and12s budget. It is not an applied
repair or full-suite success.


Final private diagnostic passes with one heading per handoff leg: compile0/run0,
actual receiver45/contactY24.1872/held56, settled speed0.1481m/s, position
`(-9.00801,25.0672,-108.966)` at tick19618. Arrival uses574ticks within the
original1080tick budget. The exact private variant diff and receipt are retained.
No maintained driver source was changed. This supports a careful carry-heading
driver repair; it is not a full-suite or rising-integration completion.
The maintained integration and its terminal suite result are recorded below.


## Maintained repair and completed goal

Root applied the exact verified per-leg heading latch to maintained
`tests/simulation_tests.cpp`; source SHA256
`7c0a9086981cc21f6fa460549d578818226bfe146f654ac97895ecde5d18f43c`.
Independent Sol/High review found no blocker and confirmed unchanged targets,
movement PD, arrival thresholds, real support/contact/carry, height and12s
budget. The flag controls normal facing input and supplies no physical state.
The carry anchor remains a real point constraint; continually reversing facing
can still swing the load and disturb footing. This repair does not change that
shipping mechanism or claim robustness to arbitrary repeated load reorientation.

The maintained simulation suite now compiles with unchanged warning checks and
exits0 against the same nativefa9c source/e2ee archive. Hook5 reaches the actual
settled handoff after574ticks and passes onward forty/checkpoint carry; the full
remaining suite tail also passes. Root independently verified source/dependency
and log/binary hashes in the final receipt. Earlier REDs are retained as rejecting
evidence, not current completion status. Native and presentation sources did not
change during this driver repair, so their previously recorded passing thin-edge,
ledge and new-bridge touch executions remain source-matched.

The first pre-pickup acquisition divergence was not separately instrumented;
no exact intermediate catch timeline is claimed. The introduced outcome and
supported driver correction are established by the source-bound paired probe,
physical carry-anchor trace, diagnostic repair and maintained-suite PASS.
