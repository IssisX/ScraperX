# Split inspection ballast retention repair — 2026-10-17

Scope: route-owned physical geometry only; player, shared solver, cable strength, test expectations and unrelated dirty work remain under their existing owners. Initial checkout: `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`, branch `ChatGPT`, HEAD `5bd7ccd77cea010d13fa971d5926929e94ecaa72`.

## Mechanical contract

- Outcome: ordinary pickup/drop of the real25kg block, departure to the fixed waiting pad, actual28m span seat contact, original-body recovery and retry, then the narrow girder exit.
- Cause: gravity on the retained block pitches the finite steel hatch, pulling the real cable over the fixed guides and lowering the counterbalanced span.
- Reaction: block/hatch contact → hatch hinge/receiver and cable → span hinge and receiving seat → existing static backbone.
- Storage/loss: real gravitational and kinetic energy, passive20Nm hatch/30Nm bearing friction, actual stop and seat contacts; no motor or prescribed pose.
- Operating range: the unchanged fixture accepts gravity-projected block centers within ±0.16m laterally and1.54–1.86m forward, with horizontal speed below0.3m/s. Actual dropped orientation and retained momentum must fit the pocket.
- Failure/exit: removing the ballast must raise the span; missing the gap must retain the actual roof recovery and same life.

## Existing failure evidence

Full native92bd194 run39048317301, `scraperx_sim.split_inspection_recovery`, fails at tick11228. The first wait sample at tick5829 already reports block1902 at(-19.652500,462.327484,-131.388901)m, on the roof below its473m hatch. The span then correctly raises from toe473.269873m toward474.8m. Its final1.681952m seat gap is a downstream result of losing the ballast, not evidence that the seat tolerance should change.

Current initial route identity matches the frozen native build used for local diagnosis: cpp SHA256 `c4080d9b721d62ea6f4f9efbbcfe45988c8e13a5e83e7fa605d77014ce7d05c6`; hpp `793262c156652cf9ebdac4c9448068c2c499977befc05012504d7cce1b1aa893`.

Private diagnostic files/experiment state: `/data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/catch-return/split-seat/`. The copied fixture adds observations only; repository tests remain unchanged. Its first baseline uses the matching frozen20m/s native archive, distinct from root’s18m/s release build.

## Causal observation and selected repair

The ARM baseline does not reproduce the first-drop escape. It exposes the marginal catch: copied observation-only fixture tick4135 releases near hatch-local center(0.029467,1.283142,1.800746)m; tick4175 is(-0.031167,0.431837,1.970444)m with vertical speed-4.103113m/s. The front wall is at localZ1.985–2.0m and only0.15m high above the deck. At tick4180 the block strikes this rim and is retained on this host. The CI x86 result loses it. This supports an insufficient receiving envelope; it does not establish an ARM first-drop failure. The baseline was deliberately stopped after causal observation (runner exit255/SIGTERM), not counted as a passing route.

The physical pocket now has350mm walls and900mm outer width, retaining the same15mm steel thickness, original front edge and open rear/top. The taller front wall receives the falling side before the corner can tip past the deck; side walls move to±450mm from the center, outside the standing capsule. No floor extends across the gap. All new mass/first moments feed the existing authored rear counterbalance. No force, strength, hinge, cable, position controller or test expectation changes.

Repaired route cpp SHA256: `28d860940e3d06f4d265425f9988f0e757845c829668cff122b772853d645146`; unchanged fixture SHA256 `27ef2f7c4884646734e16df8edb08666f251720b175be3c5425cf294fd11ee4d`.

## Build and acceptance

Private build now reuses root’s completed clean71-TU18m/s native archive, copies its complete matching source, recompiles only the route TU and replaces that object in a private archive. The route TU does not include simulation.hpp; its Kit header was checked byte-identical before reuse. Exact commands/exits are in `split-seat/build-receipt.json`; source/base archive identity is in `split-seat/source-receipt.json`. Native compiler flags retain C++17/O3/Wall/Wextra/Wpedantic/Werror and the pinned Jolt library. Build/link exit0. `git diff --check -- src/sim/split_inspection_route.cpp` exit0.

Unchanged full miss-retry acceptance **PASS**, exit0 in160.556s, final tick14566, zero deaths. Actual observed output: `PASS SPLIT_INSPECTION mode=miss-retry actual_gap_airborne=1 actual_receiver=1 loaded_hatch_motion=1 hinge_transfer=1 conduit_crouch=1 actual484roof=1 ordinary_inputs_only=1 initial_staging_only=1 deaths=0 miss_retry=1`. This includes original-body pickup/recovery, empty span restoration, second placement/retry and actual484m roof exit. Its first ballast-alone450tick seat hold ends at tick5803; second operator-assisted settle ends tick13038. No expectations were changed.

Actual command (run through existing `ci/mechanics_trial.py` with route/test/archive hashes):

```sh
proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/catch-return/split-seat/route miss-retry
```

Exact runner receipt/output: `split-seat/window.json` trial2 and `split-seat/window.trial-2.log`. Repaired native archive SHA256 `fcb62dcd3a77a4120df3857282b187f44122054f12c905169bc55329df8a4f65`; executable `ddb12a765de61ff2801ef123eb3f0293913120d94492bbb7bbfad06351b73c0d`.

Unchanged full primary acceptance **PASS**, exit0 in154.280s, final tick13726, zero deaths. Actual output: `PASS SPLIT_INSPECTION mode=primary actual_gap_airborne=1 actual_receiver=1 loaded_hatch_motion=1 hinge_transfer=1 conduit_crouch=1 actual484roof=1 ordinary_inputs_only=1 initial_staging_only=1 deaths=0 miss_retry=0`.

```sh
proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/catch-return/split-seat/route primary
```

Exact runner receipt/output: `split-seat/window.json` trial3 and `split-seat/window.trial-3.log`; it reuses trial2’s matching source/archive as its prerequisite. Both complete affected routes ran once after the repair; no additional repair iteration or coefficient sweep was needed. `git diff --check -- src/sim/split_inspection_route.cpp` exit0 after the final code edit.

**Remaining limit:** CI x86 portability remains unverified until that actual platform runs the repaired source. The earlier x86 failure and current ARM causal trace support the corrected receiving geometry, but two ARM route passes cannot establish x86 success. No APK/device result is claimed. Parent retains integration/publication ownership; worker ownership of this bounded route repair is released.
