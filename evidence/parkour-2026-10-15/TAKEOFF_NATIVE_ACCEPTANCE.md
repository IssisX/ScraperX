# Running takeoff and Jump intent — native acceptance

Status: **native implementation goal complete; source remains local dirty work, matching Godot/Android delivery pending.** This closes only the responsive-takeoff goal, not the complete movement system. Production source/test hashes and commands are retained in the linked receipts and persistent trial record. Do not rerun accepted checks merely because a goal, session or model changes.

## Behavior and evidence

| Requirement | Actual result | Recorded evidence |
|---|---|---|
| Useful real late takeoff | Jump two native ticks after actual stock-support loss; peak upward velocity1.22795m/s, positive work414.746J; original >1 and passive-control+1 predicates retained | `coyote-window.json`, trial21 log |
| Sprint-jump carry | Actual preflight7.52298m/s after95 acceleration ticks; original sprint/carry and airborne predicates PASS | trial22 log; bounded acceleration wait replaces insufficient fixed90ticks, no physics change |
| Early input waits for real contact | Contact at native tick23, single physical launch5.391m/s; expiry and repeated caller-frame input do not add a launch | `early-caller-rates.json`; buffer worker receipts |
| No fabricated support | Empty air, expired plant, Jump→Drop cancellation and restart clear prior acquisition | trial23 log |
| Crouch intent behavior | Crouch/release cancel prior buffer:0 upward/0J. Fresh Jump while crouch held stands where clear and launches5.391m/s | `crouch-correction.json` |
| Caller-frame equivalence | Early buffer:60/90/360Hz and grouped3ticks,60 shared checkpoints exactly equal. Real coyote motor: same schedules,90 shared checkpoints exactly equal; one motor activation,520.469J | `early-caller-rates.json`, `coyote-caller-rates.json` |
| Finite reciprocal actuator | Actual offset/spinning dynamic support, linear/angular reaction and event-energy accounting; negative-velocity zero crossing, solver iterations/substeps, hold, reach/expiry and legacy cases PASS | trial20 log, `finite-motor-primitive-build.json` |
| Independent consequential review | Sol/high review found no blocking defect; actual source and existing receipts inspected without redundant reruns | `FINITE_MOTOR_REVIEW.md` |

## Implemented physical contract

`simulation/parkour_intent.cpp` retains a real solved trailing contact on the same body/subshape. Fresh contact refreshes only input age, never the original anchor, rest length or actuator clock. Jump intent buffer11native ticks (~122ms); coyote intent ceiling8ticks (~89ms), shortened by actual reach, traction and the original250ms actuator lifetime.

Activated planted takeoff uses a finite direct motor inside the existing reciprocal foot row. Original3500N/5000W/1285.625J/.30m radial stroke bounds remain. One work-admitted impulse occurs per collision substep; no warm-start duplication, target velocity assignment, new airborne anchor, manual opposite impulse or second body owner. A dissipative clutch retires passive spring storage without funding rebound. Positive impulse-event work and absorbed work have separate receipts. The detailed decision/equations are in `FINITE_PLANTED_MOTOR_DECISION.md`.

The planted swiveling strut has positive normal height, friction-cone validity and radial length<=original+.30m. The legacy/plank/topout spring path retains its distinct.45m lateral-cylinder rule and constitutive law. These are explicit profile geometries, not a claim that the cylinder was redundant.

## Source and limits

Final native archive: `/data/data/com.termux/files/usr/tmp/scraperx-planted-force-20261015/libscraperx_sim.a`, SHA256 `a662ca86c2417b2e074831d53fdb472799c07fb406020fdbff0c2a1ed1788954`; pinned patched Jolt SHA256 `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`. All five production foot-readback readers were rebuilt; `finite-motor-build-receipt.json` owns the actual compile commands.

Crouch correction receipt uses the earlier archive91051fda… with the same final tick cancellation code; later changes affect only the activated planted motor, absent from its grounded early-input fixture. Caller test source was subsequently extended for coyote schedules; receipts retain each exact tested revision. Reuse these behavior-matching results honestly.

Energy proof concerns frozen-Jacobian impulse events, not continuous whole-world/contact energy closure. Reach release is discrete at the native collision-step boundary. Frame equality concerns these native fixtures on this host, not cross-platform bit identity. No matching movement APK, installed-device play or full canonical-suite PASS is claimed. Earlier failed candidates and invalid trials remain in the persistent history; they are superseded, not erased.
