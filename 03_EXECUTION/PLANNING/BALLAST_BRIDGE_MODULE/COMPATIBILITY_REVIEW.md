# Compatibility with the active pipe-loaded bridge

Reviewed 2026-09-26 against archive commit `e8d9ca8` and the active local AS-016 implementation. This review supplements the immutable archive; it does not apply `integration.patch` or activate `macro_bridge.cpp`.

## Placement decision

The useful immediate home is the shared native Kit and the active pipe bridge's presentation/proof path. Keep the pipe-fed pan, physical props, four-bar linkage and finite crush receiver as AS-016. Do not add a second grade-to-11 m bridge. The complete ballast assembly remains a possible later cable-lift archetype; it has no authorized location or receiving support in the current Atlas, so no upper-level placement is claimed.

## Component comparison

| Component | Archived design | Active design and decision |
|---|---|---|
| Kit shapes/mass | Existing boxes and effective compound mass | Keep weighted compound parts, cylindrical pipe envelopes with hollow inertia, body-to-body hinges and direct tension-only cords. Replacing Kit would lose these additions. |
| Energy observation | Principal-inertia `body_kinetic_energy()` | Reused as read-only Kit telemetry, excluding kinematic material surfaces. It measures the actual solver state, including hollow-pipe inertia; it supplies no work. |
| Brake | 150 kN guide friction, governor and arm-angle resistance | Not imported. AS-016 uses loaded props and irreversible crush work. Adding this brake would change the mechanism and its energy budget. |
| Grip reach | Actual grip point for Handle; COM for loads/shackles | Adopted the handle-only scope. Preserve the newer contact-point attachment for fixed controls; the archive does not contain that improvement. |
| Grip classification | Sets `carry_target_kind` before normal-world return | Reused: active candidate handles previously returned kind zero. Native grip position is also exported so rendered hands use the actual anchor. |
| Checkpoint | Guide brake work and historical peak speed, plus existing body/rope state | Reused peak-speed persistence. No unused brake-work subsystem added. AS-016 still separately captures/restores plastic front and plastic work alongside Kit bodies and velocities. The material never re-arms through a presentation flag. |
| Input proof | Touch/keyboard/gamepad, bounded observation waits, support checks, timing variants | Adapted into the existing pipe route, keeping its own grip IDs and physical waypoints. Existing walking helper already accepts a timeout. No duplicate input controller. |
| Audio | Motion-driven positional loop, absent plant hum suppressed | Reused the regression-only hum gate and adapted the positional loop to actual pan speed/pipe kinetic energy, with a compression cue when plastic deformation begins. Ballast speed/tension and 10.70 m arrival thresholds are not valid pipe-bridge inputs. Fall recordings remain in the independent player-audio path. |

## Concrete conflicts

- The patch's unconditional non-regression builder would instantiate the archived mechanism beside the active pipe bridge. There must remain one active construction owner in `scraperx_sim`/Jolt.
- Its control pedestal at `(7,0.45,-84)` lies inside the active pan's loading/swept footprint, approximately X=3.4..8.6, Z=-87.55..-82.7. Its sign support at X=4.5,Z=-84 also occupies that footprint. The archived receiver support at X=10.5,Z=-123.3 lies beside/within the active connector's X=7.56..10.56 corridor. These are concrete conflicts, not merely similar level themes.
- Archive IDs `1016,2016..2019` are distinct from active `1500+`/`2500+`; they are not an automatic collision today. No archived IDs or second bodies are instantiated. Any later reuse requires a new allocation and spatial contract.
- Archived counts (five moving/kit bodies), grip 2018, 32 m deck, 10 t guided source, cable-index-zero styling and nose/receiver thresholds describe a different mechanism. Copying them would corrupt the active proof/presentation contract.
- The archived 11-float box readback cannot replace the active 13-float shape/bore format.
- The old patch does not preserve current fall audio, finite-material checkpoint state, direct cords or pipe rendering if whole files are replaced.

## Evidence boundaries

The archive's native suite and corrected headless touch run establish its own observations. Its earlier 250 kN cable passes do not establish the corrected 1 MN rendered route. Neither archived success nor this source review establishes the adapted pipe route: current native/input/render checks must pass separately.

Final geometry passes all six native route/control/recovery cases, including partial regrab and passive riding. Touch at 30 FPS, keyboard/gamepad at 60 FPS and first-person route captures pass locally. Ordinary startup now constructs this same assembly; the proofs use that default. On this ARM64 host, the retained full legacy suite currently fails at the old AS-002 mid-landing; an untouched `4cdb2bc` reconstruction fails at the identical assertion. This baseline comparison does not waive the delivery workflow's required regression gate.

## Delivered adaptation

The adapted production implementation is now `254eb28` on `ChatGPT`. [Exact delivery evidence](../../ASCENT/AS-016_DELIVERY_EVIDENCE.md) records the green full workflow, normal native and three-device input route, rendered capture inspection, decoded fall audio and verified ARM64 APK. The archived files remain unchanged and inactive. No second physics owner, overlapping assembly or archived entity ID was introduced.
