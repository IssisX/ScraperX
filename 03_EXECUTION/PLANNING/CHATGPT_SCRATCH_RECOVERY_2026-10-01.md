# ScraperX Scratch Recovery and Development Handoff October 1 2026

This document preserves the ScraperX development state and recovery instructions from the disconnected Codex environment on October 1, 2026. It is a recovery artifact for the owner and a successor coding session, not another product specification.

**Backup scope — recovery completed, October 1, 2026:** The original scratch environment became accessible again. The full original checkout, including `.git`, tracked modifications and untracked source, is preserved in the owner’s existing private recovery Page as `scraperx-chatgpt-scratch-full.tar.gz` (5,648,417 bytes; SHA-256 `b407b2770b665709b84f7b1a22d0d7c24e8510e2ae3b175d037bac43459e1918`). All 597 files and 237 directories match the original exactly by path, type, bytes and permissions. An isolated extraction is a self-contained Git worktree and passes `git fsck --full`. The separate evidence archive contains 281 files and is also uploaded, in six checksum-verified parts. This GitHub directory remains the earlier partial fallback; the full archive is a separate private attachment. See [integrity receipt](SCRATCH_RECOVERY_2026-10-01/INTEGRITY_VERIFICATION.json). The latest gameplay candidate still requires its outstanding runtime verification.

**Current status:** Final slingshot verification is incomplete. Earlier successful results predate the latest physical aiming mechanism. The environment returned 409 environment_offline and subsequently failed to start with an executor configuration capabilities error. Some final patches were rejected before mutation; some documentation edits have an unknown outcome and must be inspected before retrying.

## Repository and recovery locations

- Repository: https://github.com/IssisX/ScraperX
- Branch: ChatGPT
- Scratch clone: /tmp/scraperx-chatgpt-scratch
- Original clone head: f7a584dcceb576068f62e030e35d1b88e5cd4c5a
- Build directory: /tmp/scraperx-build-work
- Evidence directory: /tmp/scraperx-current-evidence
- GPU screenshots: /tmp/scraperx-runtime/slingshot-visual-proof
- Character captures: /tmp/scraperx-runtime/climber-proof and climber-comedy-proof

All gameplay changes remained local and uncommitted at the outage. No gameplay-source push, merge, deployment, or final Android release occurred. This recovery document and the earlier GitHub fragments preserve the outage history. The full original source is now a separately saved private archive; cloning the remote branch alone still does not include the local gameplay changes. Resuming a CLI session does not automatically move the filesystem.

Before changing recovered files, capture the entire scratch clone including untracked sources. A Git diff alone omits new untracked files. Preserve the whole evidence directory too. Verify the archive contents and the Git patch against the original head before claiming a source backup exists.

## Game objective and authority

ScraperX is a first-person 3D vertical traversal game inside and around a gigantic open industrial megastructure. The intended game exceeds 1,000 m. The core objective is unbroken physical ascent from grade through hundreds of metres using parkour, industrial obstacles, moving machinery, and large visible mechanical systems.

Jolt/C++ owns consequential physics. Godot owns presentation, input, camera, HUD, rendering, audio presentation, and Android integration. Earned progress must arise from actual geometry, collision, force, constraint, momentum, torque, moving supports, and mechanical state. No progress teleportation, invisible blockers, fake route flags, or authored animation pretending machinery moved physically.

The larger traversal vocabulary includes jump, vault, mantle, hang, shimmy, crouch, and moving-support traversal. Mechanisms should be varied and readable: counterweights, racks, pulleys, pivots, gears, cranes, linkages, catches, lifts, and hinged structures. Avoid repetitive machines, ordinary ramp/stair solutions, tiny unreadable mechanisms, and generic obstacle courses.

The slingshot is one bounded opening encounter. The current +352 m receiving rings are interim geometry, not completion of the 1,000 m game or proof of a continuous whole-campaign route. After the launcher gate closes, inspect the actual next ascent geometry/mechanical gap and select one complete causal traversal slice. Preserve existing Laws/GDD/Atlas authority and proven historical routes.

Development restart/spawn remains available and is explicitly outside earned progress. The owner wants it to remain as a future unlockable Easter egg or cheat. An unlock has not been implemented or verified.

## Owner requirements and development process

The owner selected exaggerated arcade player strength with manual pull-back. The machine must look sturdy and expensive to play: substantial wood, thick rubber bands, a life-sized leather pouch, smooth aim, stretch-dependent output, and launch far above 40–60 m. Moving the machine farther from the tower was authorized.

Touch controls are the current focus. Do not spend new effort on keyboard/gamepad UX. Existing compatible bindings need not be removed. Approach/back into the pouch, explicitly board, aim, manually pull backward, release, and manually retrieve/reset through physical controls.

The visible climber must be recognizably human, not a capsule. A dramatic short slow-motion accelerating 360-degree camera sequence returns to real first-person POV, with restrained speed warp/blur. Humorous poses, mechanical sounds, and brief thought bubbles accompany existing recorded fear/profanity. Facial beauty/detail is not a priority.

ALL landings need actual impact and support-relative momentum response, physical slip/recovery/catch movement, and smooth POV compression, bob, sway, and recovery footwork. Do not substitute a visual shake for physical landing mechanics.

Treat the game as a serious future paid/public Google Play product. Focus on visible feel and real causal mechanics, not superficial test proxies. Runtime behavior proves behavior; source/configuration proves implementation; documentation states a contract only while it agrees with reality.

Maintain a running to-do. Update the relevant existing authoritative document immediately when a behavior is established, changed, verified, or invalidated. Repair locally as development proceeds; do not perform a giant speculative cleanup or create competing authorities. Preserve established code-facing terms. Reject architecture-breaking or fake mechanisms while retaining the underlying objective.

Skills already applied include Causal Mechanism Compiler, Superpowers, repo-surgeon, threespine for the bounded mechanism, dispatching/review skills, and verification-before-completion. Delegated agents used the same shared scratch tree.

## Slingshot native implementation state

Native implementation is in files named slingshot.hpp, slingshot.cpp, simulation.hpp, simulation.cpp, bridge.hpp, bridge.cpp, slingshot_machine_tests.cpp, launcher_tests.cpp, simulation_tests.cpp, and landing_tests.cpp. Discover exact native subdirectories with rg before editing; do not infer unknown paths from this recovery note.

Calibration:
- Neutral pouch position: (6, 0.35, -55), moved farther from the original tower-adjacent machine.
- Maximum manual draw: 12 m.
- Fixed fork default elevation: 82 degrees; yaw 0.
- Anchors use actual fixed fork transform; local offsets approximately (+/-3, 0, -10).
- Band rest length: sqrt(109), approximately 10.4403 m.
- Two tension-only elastic bands: 10,000 N/m each; damping 15.
- Rider 85 kg; pouch 15 kg; physical launch rail 60 kg.
- Draw source 200 kW, efficiency 0.88, force ceiling 160 kN, speed ceiling 4 m/s.
- Launch guide travel 12 m.
- Retrieval source 20 kW and 16 kN, with real finite-force travel and passive capture.
- Entities: fixed frame 1950, grade support 1951, pouch 2900, launch rail 2901.
- Kit body count 25; moving body count now 15 because the timber fork is static.
- Normal tower body count 385; old regression fixture tower count 169.

The latest design bolts the timber fork to grade and aims only the dynamic steel launch rail through a SixDOF gimbal. Linear axes are locked at the grade pivot; pitch/yaw are bounded and roll locked. Native angles are measured from the actual rail quaternion. A PD actuator with gravity compensation applies actual AddTorque, bounded to 20 kNm and the 200 kW source. Work uses torque times measured rotation. No gameplay aiming SetPose or SetVelocity should exist. Construction, checkpoint restoration, and explicitly authorized development restart are separate cases.

A passive catch engages only after the actual angular error and angular speed settle; aiming then transitions to the actual measured launch Slider. Winching waits for settled aim. The requested touch target must persist after the finger lifts while the camera, rail, reticle and prediction use actual measured state.

New native state fields include aim_ready, aim_source_power_w, target_yaw_rad, target_elevation_rad, and aim_locked. Root forwarded the first four through Simulation/bridge before the outage. aim_locked still requires end-to-end exposure. Use the exact native predicate rather than a competing UI approximation.

The latest raw gimbal run physically settled low/high aim but failed a checkpoint-history test fixture because its old fixed wait/charge window could undercharge during the newly physical turn. The fixture was changed to wait for actual aim_ready and assert a real launch before rewind; the correction has not been rerun. Full-world aiming with ground/tower contacts remains unverified.

The launch shoe was replaced by two 2 kg pivot cheeks at x +/-1.05, local y/z 0, half extents (0.22, 0.12, 0.22). Calculated minimum grade clearance is about 0.0994 m. This algebra and ground-free raw tests do not replace a shipping-world collision test.

Energy ledger notes:
- Positive source power/work and braking are explicit.
- Band/pouch/rail/rider energy is tracked; rider membership joins/leaves at actual board/drop.
- Pre-seat aiming and checkpoint restore must preserve membership and baseline correctly.
- Pouch/player impact witnesses and external forces invalidate the closed-flight ledger rather than fitting away residual.
- Carrier contact must also invalidate the audit during AIM/DRAW/flight because the rail's 60 kg energy is included. This remaining fix had not been implemented.
- Only the player/pouch contact pair is excluded by the harness; internal Kit member exclusions also exist for intersecting constrained geometry. Do not claim every internal member pair collides.

## Exact pending boarding patch

The presentation-side approach gate was rejected before mutation by 409 environment_offline. In main.gd, in the existing set_slingshot_input call, replace the condition that includes station_available with only actual seated or recovering state:

```gdscript
_native.set_slingshot_input(
    maxf(0.0, -desired.y) if sling_seated or sling_recovering else 0.0,
    sling_yaw,
    sling_elevation
)
```

The old condition was:
```gdscript
if sling_seated or sling_recovering or bool(launcher.get("station_available", false)) else 0.0
```

Native boarding work also remains pending: remove draw-input auto-attach, require explicit BOARD/Action authorization, and keep the unoccupied zero-travel seat stop until actual authorized paid draw. Generic backwards approach must not immediately store energy and lock aim. Coordinate the native and Godot changes as one behavior. Preserve approach -> BOARD -> AIM -> backwards DRAW. Retrieval muscle input remains allowed.

Known touch aperture issue was physical standing contact near pouch Z +0.30001/sole -0.170001. Native boarding slop was widened to approximately [-0.31, +0.32]. Verify the actual viewport touch path against the rebuilt final bridge rather than only a positioned native fixture.

## Landing and rendering implementation

Native contact handling records actual pre-solve support-relative contact velocity and full contact normal. A separate strongest-impact witness prevents a gentle dynamic support contact from masking a hard static impact.

Native landing snapshot channels include:
- landing_count and support_entity_id
- landing_approach_energy_j: normal component only, 0.5 * 85 * vn squared
- landing_tangent_energy_j and normal/tangent speed
- landing_observed_normal_impulse_ns
- landing_balance, recovery_seconds, recovering and slip_velocity
- landing_recovery_work_j
- landing_jump_work_j, a separate push-off channel

Observed impulse is actual body momentum change and can include gravity/other simultaneous contacts; it is not an isolated measured contact energy loss. The player remains an upright arcade capsule with restricted rotation. Balance/recovery is an impact-driven controller state, not a fully articulated foot or centre-of-mass solver.

Recovery retains arrival momentum and applies finite grounded muscle force toward walking. Traction is bounded approximately by 0.85*m*g*normalY and positive work by 3 kW. Dynamic supports receive opposite force at the contact. Work uses actual player displacement relative to the support material point, including support rotation.

A latest fix evaluates Jump before recovery foot force, uses actual upward AddImpulse toward 5.8 m/s plus support motion, preserves horizontal momentum and applies opposite impulse to dynamic support. It adds measured jump work separately. The source correction was reported, but the final rebuilt bridge has not verified it. This matters because the latest settings runtime gate failed when recovery swallowed Jump.

Godot landing_camera_response.gd uses closed-form critically damped compression/pitch and slip-energy-driven sway/roll, plus native recovery posture. It affects presentation only. Head-bob/comfort off and restart clear the response immediately. The old authored sine-dip response was removed.

Fixed-tick render interpolation was added independently of raw physics:
- render Kit position and quaternion SLERP
- interpolated slingshot pouch and anchors from the actual fork transform
- cable-point interpolation when topology matches
- get_kit_body_render_transform
- get_slingshot_render_state
- get_kit_cable_render_points

History is captured before every native fixed tick and explicitly rebased after reset/death/crouch/checkpoint. The old greater-than-1-m-per-tick heuristic was removed because real high-speed launch motion is not a teleport. Native raw getters remain authoritative and unchanged.

## Presentation audio settings and debug tools

Implemented Godot work includes slingshot_view.gd, wood/leather/speed shaders, main.gd, landing_camera_response.gd, climber_character.gd, audio_director.gd, sound_bank.gd, pause/settings scripts, and tests. Locate exact file paths with rg; authoritative known paths include godot/presentation/main.gd, godot/presentation/landing_camera_response.gd and godot/tests.

Presentation:
- Substantial beveled wood matching native collision members; thick rubber, stitched leather, metal hardware and actual retrieval handwheel.
- Human climber with helmet/lamp, jacket, gloves/fingers, harness/carabiners, pack, kneepads and boots. No CapsuleMesh character substitute and no new consequential physics.
- Humorous brace/tuck, air-swimming arms, bicycle feet and falling flail react to actual draw/velocity/launch state.
- Approximately 2.1-second launch sequence, accelerated 360 orbit, initial time scale 0.16, short radial speed blur/warp, then true first-person.
- Brief rising-flight bubble: MY STOMACH / TOOK THE STAIRS.
- Bubble width changed to 560 logical units and font 32; some aiming labels enlarged. Latest source has these changes, but latest GPU captures precede the final gimbal and larger bubble.
- Comfort launch-cinematic/head-bob settings cancel effects; checkpoint restart clears cinematic and prevents replay.
- Native capsule-clipped advisory trajectory, actual-power HUD, reticle and contact marker.
- Persistent requested touch aim targets are implemented; actual rail motion remains finite.
- Manual chute is offered during actual launched ascent, enabling a real user-commanded high roof landing.

Audio:
- Seeded synthesized mechanical stretch/release/boing/squeak cues and actual-speed wind.
- Existing recorded human fear/profanity retained; do not misrepresent synth clips as new recorded voices.
- 27 sound groups and master limiter/mute/bus integration.

Settings:
- Persistent graphics/video/audio/quality/LOD/VSync/fullscreen, mute and FPS options including 45 FPS.
- Scrollable touch pause pages with wrapped row sizing.
- Restart destinations: grade plus 32 supported ring choices every 11 m to +352 m, arbitrary clear height/side/offset and exact XYZ.
- Native validation rejects non-finite, outside-bounds or occupied standing capsules. Rejected input leaves the menu paused with feedback.
- Supported grade preset approximately (6, 0.92, -58); ring preset (20, height +0.92, -128).
- Decorative LOD affects 704 explicitly selected dressing meshes; 2,033 structural meshes remain.
- Ordinary menu help is touch-only. Historical binding functionality remains.

Debug restart explicitly relocates and resets player state, checkpoints, harness contact exclusions, movement/traversal/carry/chute and recovery. It is not earned traversal proof. Actual supported solver standing centre can be around 0.8948 m; validation permits small solver slop down to 0.88 m.

## Prior evidence and limits

These receipts prove prior candidates only. They do not certify the latest static-fork powered-gimbal source.

| Concern | Prior observed result | Current limit |
| --- | --- | --- |
| Native regression | Original 24 tests passed; an intermediate expanded candidate passed 27 | Current suite has 28 including landings; latest full candidate unrun |
| Full elastic launch | Actual raw apex about 384.34 / 384.17 / 384.22 m at 90 / 180 / 360 Hz | Before final gimbal |
| Touch full draw | 11.996 m draw, 419.93 kJ bands, 475.15 kJ manual work, 197.97 kW source peak, 384.20 m apex | Before final gimbal and boarding changes |
| Native supported roof | Manual chute around ascent y >354 m; apex about 361.788 m; supported +352 m roof; zero deaths | Native prior candidate; final viewport-touch roof scenario pending |
| Retrieval | Real return about 4.644 s; source peak about 9.055 kW; residual band energy about 0.235 J; second board/aim/shot | Before final gimbal |
| Touch retrieval cancel | Returned control cancelled and 2.64 m ordinary walking | Prior candidate |
| Landing physics | Grade falls 0.4 / 4 / 10 / 25 m produced normal approach energies about 335 / 3333 / 8370 / 20860 J; dynamic pouch/Teeter support and recovery/jump checks | Final rebuild with gimbal/jump correction pending |
| Landing camera | Prior native integration 258 checks; actual 417 J / 2581 J impacts produced dips about 3.53 / 8.99 cm; frame partition and comfort checks | Latest source integration rerun pending |
| Settings | Earlier native 2894/0; rendered 2898/0; isolated menu/store 94/0 | Latest native 2894 checks with 1 failure, rendered 2900 with 1 failure: recovery swallowed Jump |
| Touch audio | 95 checks, 34 actual touch events, 2 comedy cues, no repeated weak/held cues; peak -0.8002 dBFS, RMS -18.071 dBFS | Before final gimbal |
| Visual proof | Six real GPU screenshots plus character and settings captures were produced and inspected | Final gimbal and latest HUD font captures pending |
| Geometry | Normal and historical fixture solid tables compared exactly; normal 32 rings / historical 14 retained | Recheck if geometry changes |

Latest audio receipt paths:
- /tmp/scraperx-current-evidence/slingshot-audio-current-final-receipt.json
- /tmp/scraperx-current-evidence/current-source-final-import-receipt.json

Native library hash in those receipts:
f262cc74fe566b72e45ee72e2c05645f3a485c77ba6b61ce5a79c52e278fc985

Godot import for that staged source passed with 80 stable source files. Do not claim this hash is the latest edited source or final verified native candidate.

No final Android APK, current CI result, on-device touch feel, Google Play readiness, Fold frame rate or thermal proof was established. Software OpenGL captures demonstrate rendered output, not target-device performance.

## Build environment and commands

Known tool locations:
- CMake: /tmp/scraperx-tooling/cmake/data/bin/cmake
- CTest: /tmp/scraperx-tooling/cmake/data/bin/ctest
- Godot 4.7: /tmp/scraperx-runtime/Godot_v4.7-stable_linux.x86_64
- Godot helper: /tmp/scraperx-runtime/run-godot.sh
- Python: /opt/codex/runtimes/codex-primary-runtime/dependencies/python/bin/python3
- ffmpeg: /usr/bin/ffmpeg
- Jolt source: /tmp/scraperx-deps/JoltPhysics-e77f175595e64cb44218cc9d9d56fc365ad0e36a
- godot-cpp: /tmp/scraperx-deps/godot-cpp-507ed9d840c01a3c5b2a39af8bb4000bfac30bf5
- Ignored native extension: godot/addons/scraperx_native/bin/linux/libscraperx_native.so

Runtime used Xvfb display :97 and a 2160x1856 viewport with genuine llvmpipe OpenGL. X11/editor imports needed executor network permission because local sockets otherwise produced _sock==-1 failures. Preserve injected proxy/runtime configuration. Do not dump credentials or replace system HOME/CODEX_HOME.

After recovering and archiving the actual sources, standard verification starts with:
```bash
/tmp/scraperx-tooling/cmake/data/bin/cmake --build /tmp/scraperx-build-work -j 3
/tmp/scraperx-tooling/cmake/data/bin/ctest --test-dir /tmp/scraperx-build-work --output-on-failure -j 3
```

Run exact existing project test commands after reading scripts/workflow; do not guess missing scenario flags.

Known real touch audio recipe from the prior environment:
```bash
/tmp/scraperx-runtime/run-godot.sh --path godot --rendering-method gl_compatibility --fixed-fps 30 --audio-driver Dummy --write-movie /tmp/scraperx-current-evidence/recovered-slingshot-audio.avi --script res://tests/slingshot_audio_test.gd
```

Run from the recovered scratch clone. Reconfiguration or new machines may need tool/dependency setup; paths are receipts of the old environment, not universal installation instructions.

## Existing documentation and CI changes

Confirmed before the outage:
- 00_START_HERE.md current-position update preserves historical sections.
- CONTINUE_HERE_CHATGPT_CODEX.md handoff and running to-do.
- 01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md current opening and full-game charter.
- AS-023_MANUAL_SLINGSHOT.md execution ticket.
- evidence/manual_slingshot.md ledger.

These describe the fixed 82-degree timber fork/finite physical rail gimbal as the current correction, mark final proof pending, preserve prior candidate results as historical evidence, and disclose physical/controller approximations. Locate ticket/evidence exact repository paths before editing.

The most recent owner-policy and future-cheat documentation patch did not receive a success receipt. Inspect current contents first; do not blindly duplicate it.

.github/workflows/wo000-delivery-spine.yml was updated with slingshot/model/launcher/landing native targets, touch-only new launch/landing scenarios, Godot view/character/settings/landing-camera checks, real GL probe and MovieMaker touch audio. Inventory expectations were changed to 15 moving, 25 Kit and 385 normal Tower bodies. Existing historical PipeBridge/route gates remain explicit regression fixtures. Evidence wording no longer presents the old PipeBridge route as the current opening. YAML parse and git diff --check passed before the outage. CI/Android export was not actually run for the final source.

Normal world is Slingshot. PipeBridge is an explicit historical fixture with 14-ring geometry. Normal geometry uses 32 rings to +352 m. Do not delete old tested fixture routes or silently treat fixture route proofs as current gameplay completion.

## Pending documentation text

The settings agent retained this planned patch. It is not a confirmed applied patch. Existing Markdown context and exact repository paths must be read before applying.

Add to the existing Atlas receiving-support concern:
```text
Development restart/spawn remains available as an explicit non-progress tool. The owner intends future unlockable Easter egg/cheat access, but no unlock is currently implemented or verified. Such relocation is not normal earned ascent, and it cannot serve as route-completion evidence.
```

Add to the existing current-position summary and handoff owner-direction concern:
```text
Development restart/spawn remains available. Future unlockable Easter egg/cheat access is product intent, not an implemented unlock or normal earned progress. Update the existing authority/status documents as behavior is established, changed, verified or invalidated, and keep the handoff's running to-do current.
```

Replace the broad future-authoring checkbox in the existing handoff:
```markdown
- [ ] After the launcher gate closes, inspect the actual next ascent geometry/mechanical gap and select one complete physically causal traversal slice. Preserve the larger game objective; do not speculatively author a new 1000 m campaign or machine system now.
- [ ] Future product intent: unlockable Easter egg/cheat access; preserve current development restart/spawn and its non-progress boundary.
```

## Running to do at the outage

- [x] Recover the original scratch tree and evidence; archive tracked AND untracked files before edits, upload independent private copies, verify all source bytes and isolated Git integrity.
- [ ] Read current files to resolve unknown patch outcomes; do not recreate source from this summary as if it were a backup.
- [ ] Apply coordinated explicit BOARD/approach draw gating and passive seat stop until actual manual draw.
- [ ] Expose native aim_locked through Simulation, bridge and Godot; preserve touch requested-angle persistence.
- [ ] Invalidate the energy audit on carrier contacts during AIM/DRAW as well as flight/retrieval.
- [ ] Verify pre-seat aim, Drop and checkpoint restore preserve actual rider membership/source baseline/residual.
- [ ] Test actual shipping-world low/high pitch around 20–85 degrees and yaw extremes with all relevant collisions active; settle through real torque and prove source stays at or below 200 kW.
- [ ] Assert a real first launch before checkpoint history rewind; rerun the corrected raw fixture.
- [ ] Rebuild exact source and run all 28 native gates; fix failures through actual behavior rather than proxy assertions.
- [ ] Run genuine touch approach -> BOARD -> one-shot AIM/finger lift/settle -> DRAW -> RELEASE, retrieval cancellation and second shot.
- [ ] Run genuine touch manual chute near 354 m, supported +352 m landing, zero deaths and continued ordinary walking.
- [ ] Verify all-landing recovery and Jump on actual static/dynamic supports; rerun native/Godot camera integration with comfort/reset checks.
- [ ] Rerun settings integration including supported restart destinations and touch help; close the recovery-swallowed-Jump failure.
- [ ] Rerun real touch MovieMaker audio on the final gimbal candidate and retain exact source/library provenance.
- [ ] Capture and inspect final actual-game timber, pouch, aim, human orbit/comedy and return-to-first-person output; include larger HUD text.
- [ ] Independent review of physical authority, collision honesty, ledger ownership, interpolation and checkpoint seams.
- [ ] Reconcile existing authoritative docs/to-do/evidence immediately with final measured behavior. Keep CI/APK/device limits explicit.
- [ ] After this bounded launcher gate closes, select the next highest-value complete physically causal ascent slice from actual geometry.

No final approval or completion claim is justified until these gates are closed. Preserve every successful historical behavior while resolving causes.

## Successor session prompt

Paste this into a successor coding session after recovering the actual scratch files:

```text
Continue ScraperX branch ChatGPT from the recovered uncommitted scratch tree. First preserve the entire tree including untracked files and evidence, then inspect current source and existing AGENTS/authority documentation. The recovery Page now contains the verified full original checkout archive and separate evidence archive. Its prose remains a handoff, not a competing product authority.

Maintain Jolt/C++ consequential physics and Godot presentation/input/camera/HUD/Android ownership. ScraperX is a 1000 m-plus physically causal first-person industrial ascent game; the slingshot is one opening encounter. Current focus is touch controls, sturdy visible wood/thick bands/leather, exaggerated manual player pull-back, persistent physical torque-driven aim, high actual launch, visible humorous human climber, all-contact momentum/footwork recovery and polished POV. Face detail is not a priority.

The final gimbal candidate remains unverified. Complete explicit BOARD/manual-draw gating, exact aim_locked bridge exposure and carrier-contact energy-audit invalidation. Test real shipping-world aim clearance/source bounds, then rebuild all28 native tests and exercise genuine touch launch, retrieval, supported352m roof landing and continuedwalking. Rerun landing Jump/camera/settings/audio/captures on that exact source. Earlier384m/95audio/2894settings receipts predate current changes; never promote them to finalcandidate proof.

Update the existing authoritative documentation and running to-do as each real behavior is established, changed, verified or invalidated. Preserve historical fixtures/proofs and code-facing names. Development restart/spawn remains outside earned progress; futureunlockablecheat is intent only. Do not fake physics, test-only gameplay or residual accounting. Do not perform a giant documentation cleanup, premature refactor or speculative newcampaign. Use evidence, repair causes and keep the current bounded slice moving. No push, publication or finalAndroidrelease is authorized by this handoff.
```

## GitHub recovery checkpoint

The connector confirmed ChatGPT was still at f7a584dcceb576068f62e030e35d1b88e5cd4c5a when this recovery record was prepared. The current remote gameplay therefore does not contain the local AS-023 slingshot changes described above. This documentation commit preserves the handoff only. The platform subsequently restored access to the original scratch tree. Use the verified full original archive; reimplementation from summary fragments is no longer necessary.

The save was requested by the owner after the environment failed. It does not change product authority, merge gameplay code, or establish current runtime verification. No new APK/CI candidate should be inferred from this file.

## Additional cached implementation details

These details came from the implementation agents after the outage. They preserve known paths, small cached fragments and planned fixes. At the time these notes were collected, full source text was unavailable. The original filesystem and full archive are now recovered; inspect their actual files before applying any proposed patch from this historical appendix.

### Exact source locations

```text
src/sim/slingshot.hpp
src/sim/slingshot.cpp
src/sim/simulation.hpp
src/sim/simulation.cpp
tests/slingshot_machine_tests.cpp
tests/landing_tests.cpp

godot/presentation/main.gd
godot/presentation/slingshot_view.gd
godot/presentation/slingshot_wood.gdshader
godot/presentation/slingshot_leather.gdshader
godot/presentation/slingshot_speed.gdshader
godot/presentation/climber_character.gd
godot/presentation/landing_camera_response.gd
godot/presentation/ui/ui_test_driver.gd
godot/tests/slingshot_view_test.gd
godot/tests/slingshot_visual_probe.gd
godot/tests/landing_camera_test.gd
```

The earlier godot/main.gd reference is corrected to godot/presentation/main.gd.

### Native details to preserve

- Gimbal translation is fixed. Rotation X is limited to .33..1.50 radians, Y to -.82..+.82, Z fixed. Input targets clamp yaw to -.8..+.8 and elevation to .35..1.48.
- Desired angular speed is bounded at .70 rad/s; torque at 20,000 Nm; predicted positive mechanical power at 200,000 * .88 W.
- Gravity compensation uses the actual rail. Actual torque work uses measured quaternion displacement: EnsureWPositive(), then 2*atan2(length(xyz), w). This avoids tiny-angle acos precision loss. Positive work is divided by .88 for source accounting; negative work is dissipation.
- Aim settles at angular error <.003 rad and angular speed <.025 rad/s.
- At geometric track exit, the step listener disables the launch constraint/harness; owner code removes them later. Do not remove constraints inside the callback.
- Spring forces are replaced by substep deltas because Jolt accumulated forces survive until the outer Update ends.
- Weak release stays latched unless available spring work exceeds actual track gravitational work plus 1,000 J.
- Temporary player/pouch collision exclusion ends at actual world-AABB separation with .10 m clearance. Preserve the original player collision group through reset/restore.
- The cached corrected checkpoint rider-membership expression is:

```cpp
saved.seated || (saved.released && !saved.recovering)
```

### Pending native changes

These are reconstruction instructions for the remaining changes, not applied source.

1. In slingshot.cpp pre_step, remove auto-attach based solely on draw_input > 0 while physically in the pouch. Explicit Action authorizes boarding.
2. In create_guide, retain a passive zero upper stop when held draw is <=.0001 m.
3. In drive_ratchet, open travel only for a seated, settled, positive manual draw, or already paid retained draw. Illustrative predicate, with names adapted to actual source:

```cpp
allow_draw = harness_present && aim_ready && draw_effort > 0;
upper_limit = (allow_draw || ratchet_draw_m > 0.0001)
    ? maximum_draw_m
    : 0.0;
```

4. Add an independent atomic carrier-contact witness, with a proposed note_carrier_contact() entry point. Contact Added/Persisted callbacks notify it for entity 2901. Consume it each tick and invalidate an initialized ledger during aim/draw as well as release/recovery. The pouch witness alone is insufficient because its current invalidation is phase-gated.
5. Exercise actual production-world ground/tower geometry at low/high pitch and yaw extremes; bounded waits use actual aim_ready. Check fixed anchors, achieved angles and source peak <=200 kW.

### Presentation integration contracts

```text
SlingshotView:
  setup(main)
  update_view(delta, state, player_render_position, player_velocity, camera)
  get_simulation_scale()
  is_cinematic_active()
  cancel_cinematic()

ClimberCharacter:
  reaction(state, velocity, launch_phase)
  update_pose(position, velocity, forward, delta, draw)

LandingCameraResponse:
  update(native_landing_dictionary, delta, enabled, right, forward)
  reset()
  translation: Vector3
  rotation: Vector2
```

Read landing_approach_energy_j directly as normal energy; do not subtract tangent energy. Use interpolated native getters for visible transforms while inputs, energy and prediction read native authoritative state. Keep requested aim separate from actual rail angles. Gate trajectory forecast on seated && !released.

Retrieval control position (7.5, .70, -56.8) is the post location, not a valid standing capsule centre. Touch staging uses approximately (7.5, .92, -57.2).

The genuine touch scenario names are touch_slingshot and touch_slingshot_landing. No new keyboard/gamepad proof is required for this slice.
