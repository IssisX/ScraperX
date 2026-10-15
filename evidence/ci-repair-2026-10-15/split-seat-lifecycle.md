# Split span seat-contact lifecycle diagnosis — 2026-10-15

Status: bounded diagnosis complete. Sleeping-callback explanation rejected; original x86 hold failure remains unresolved. No production edits or supported repair. Owner: `src/sim/simulation/contacts.hpp` accepted contact receipt, surfaced through `snapshot.cpp`; physical bodies/load path belong to `mechanism_kit.cpp` and the existing split encounter.

Repository inspected: IssisX/ScraperX, ChatGPT, c2b2517d67d19e8ae4843db6177048db546277ad with concurrent dirty work preserved. Private diagnostic directory `/data/data/com.termux/files/usr/tmp/scraperx-ci-seat-lifecycle-20261015/` froze source plus archive before instrumentation. `freeze.json` records archive `5d267760bb4aa52781067d9ff578438e89e59efb89693756f40c904542d0b30b` and zero mismatches against the supplied gate integration `native-build.json` source hashes. Jolt is the supplied `scraperx-ci-soft-velocity-20261015/{jolt-src,jolt-build/libJolt.a}`. No ABI mixing.

Baseline: full x86 CI38753146977 rejects the450tick physical-ballast hold at tick4322, reason `physical ballast keeps span at actual seat without player load`. Previous private ARM primary route passed tick11302, all451 seated samples contact=true with0.024–7.816mm plane separation. That pass does not identify the x86 failure.

Serious rivals: (1) actual span separation/bounce; (2) contact exists but accepted child/normal differs; (3) callback delivery absent because sleeping; (4) receipt cadence/final-substep sampling differs from current geometry. Cheapest first discriminator was a real-Jolt compound span-part1/seat-part0 fixture forwarding every callback into the actual production `PlayerContactListener`, with active flags, accepted manifold identity/depth/normal, cache state and departure negative.

## First experiment: sleeping explanation rejected

Actual command:

```sh
proot-distro login ubuntu -- /usr/bin/python3 /data/data/com.termux/files/usr/tmp/scraperx-ci-seat-lifecycle-20261015/run.py
```

Private `probe.cpp`, `run.py`, `compile-command.json`, `compile.log`, `run.log` and `receipt.json` preserve exact source/build/output. Compile0. Diagnostic run1 means the proposed sleep-hole condition was NOT observed, rather than a shipping check failure. The first compile failed on a private helper's nonexistent `Part.local`; using the existing `Part.offset` corrected that helper before runtime.

Production `Kit::add_body` sets `settings.mAllowSleeping = false` unconditionally (`mechanism_kit.cpp:217`). No inspected simulation owner reenables sleeping for the span. Thus pinned Jolt's documented absence of sleeping-body callbacks is real but does not explain this encounter under the existing creation policy.

Measured fixture:

- Initial accepted actual seat-part0/span-part1 contact: normalY1,4points, speculative penetration-0.0100000091m, active1, cache1, predicate1.
- After1600 collision steps at1/360s: active1, predicate1, cache1, persisted callback1, gap7.45e-9m, vy-7.16e-11m/s, sleeping_steps0.
- Explicit activation retained the same correct contact receipt.
- Real upward velocity then separation produced predicate0/cache0 at0.69168967m gap, vy3.33041883m/s; meaningful no-contact negative passed.

No sleep-cache production fix is supported. A persistent receipt added now could conceal true departure. The next discriminating measure is one private original-route execution with read-only native instrumentation at each predicate edge: final-substep callback count/exact child/normal/depth, active/allow_sleep/cache, actual span-plane gap and transforms/velocities. This is a new cause discriminator, not a replay to collect another passing route.

## Instrumented original-route discriminator

Actual command:

```sh
proot-distro login ubuntu -- /usr/bin/python3 /data/data/com.termux/files/usr/tmp/scraperx-ci-seat-lifecycle-20261015/run_route.py
```

Only private frozen `world.cpp`, `snapshot.cpp` and `tick.cpp` were recompiled against matching headers, and their objects replaced in a private archive copy. Contact callbacks add read-only scalar receipts to an independent diagnostic mutex; existing contact selection and all physical inputs remain unchanged. The original route driver/assertions/budgets are preserved. `route.receipt.json` and `route.log` bind the output when execution ends. An initial private snapshot helper compile used nonexistent `BodyInterface::GetAllowSleeping`; a legal owner-thread `BodyLockRead`/`Body::GetAllowSleeping` corrected that helper. World/tick successful objects were reused.

First observed true→false edge is **pre-hold**, tick3303/substep13212:

- Actual spanID8391222/seatID8391223, active1/1, span allow_sleep0.
- Predicate1→0, `WereBodiesInContact=0`, final-substep callbacks0/accepted0.
- Previous manifold exact span1/seat0, normalY0.931217849,4points, last depth-0.020037597m.
- Current actual underside/seat-plane gap0.0211291481m; angular velocityZ-0.00164021179rad/s.

This particular edge reflects real separation beyond the ordinary20mm speculative contact range, not skipped sleeping callbacks or a wrong child/normal. Before that edge the observer acquired at tick3258 (18.948mm gap); it reacquired tick3760 (19.694mm gap). This is expected initial lowering dynamics and does not identify the later x86 tick4322 rejection. The ARM run has since passed the entire450tick hold and entered recovery without a contact-loss edge there.


## Completed result and repair boundary

One instrumented original primary route exited0 in123.500297s, finaltick11302, zero deaths, actual484m roof firm footing90ticks. The original450tick ballast hold passed, with no predicate-loss edge between acquisition tick3760 and deliberate recovery separation tick7697. Retry transient lost contact tick9223 at21.102mm gap and reacquired tick9561 at19.867mm. All7 observed predicate edges are in `route.log`. This is ARM native route evidence, not x86/CI/APK/device proof.

Source/artifact identity:

- Base native archive: `5d267760bb4aa52781067d9ff578438e89e59efb89693756f40c904542d0b30b`.
- Private instrumented archive: `f02318b407da6bf5e54b5b5e9cc5136d7257d8f0088e85be9f72efc6f6e45018`.
- Exact original driver: `698bbbf8fbda96b9113ebb1684d718001cf987dca0ed3cceebaa5f1aec1ce2ee`.
- Jolt archive: `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`.
- Executable: `db2dea37435e0b2eb130253cee97a00a232e5e1df8433bfaa8244e7b633c1b5e`.
- Route log: `091ddf3bf099d7b521cdc308a6f736015ae74005f8e3f135a371afa262c2119a`.

Supported causal chain for observed losses: physical span moves beyond the native speculative-contact range → Jolt no longer accepts/caches the span/seat contact → no final-substep callback → observer resets false. Active flags/creation policy reject sleeping as the explanation. Preserving that false is correct. A stale persistent contact flag would fail the actual raise/departure negative; changing tolerance would admit measured separation. Neither is an owner-correct repair.

**Unavailable evidence:** exact native x86 CI tick4322 final-substep contacts, transforms and dynamics. The ARM reproduction did not reproduce that failure. Architecture/compiler-dependent dynamics are a plausible reason for divergence, not a demonstrated root cause. No local native source, numerical coefficient, geometry or test-budget change is justified by this bounded diagnosis.

**Smallest useful next action:** carry the private scalar receipt into the failing x86 route path (diagnostic-only, assertions untouched), capture the first hold true→false at tick4322 or its replacement, then choose the repair owner from that edge. If current exact-contact geometry shows real separation, inspect the existing ballast→hatch→rope→span load path at the encounter owner. If it shows a still-valid pair but rejected exact children/normal, repair that receipt/geometry identity boundary. If callbacks and accepted receipt are present but final flag is false, repair cadence/reset publication at `contacts.hpp`/`tick.cpp`. Do not add a persistent cache without a demonstrated callback lifecycle hole. The existing fixture's actual upward departure and the route's raised-span recovery are meaningful negatives for any proposed observation change.

Only this assigned notes file and private diagnostics were written. No shipping code/test assertion, shared archive, geometry, mass, damping, timeout or tolerance was changed. No further replay is warranted without x86 evidence or a relevant owner change.
