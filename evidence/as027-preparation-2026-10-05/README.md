# AS-027 engineering preparation, not integrated gameplay

Base source560618372ad9ea18a6e037b1d7d197557fb350ad (ChatGPT). Production files changed only to record the planned AS-027 contract and Atlas entry. All native experiments run in `/workspace/module-import`, using current built Kit/Jolt libraries. No new remote push or second Android build was started. Repair Actions37340947930 remains the delivery candidate.

## Topology and falsifiers

Two18m scissor stages,15→65degrees: geometric travel23.309595m. Nine mechanism movers plus optional85kg test box.200kN actuator and220kN passive brake are screening ratings; no force increases were used to cure failure.

The imported-style offset single arms were rejected: unloaded90Hz peak pin-position error92mm, speed transient3.32m/s, without mechanism contacts. Symmetric paired arms about the pin planes reduce peak error to2.089mm and remove the large transients. This proves better numerical closure, not steel strength. An unintended upper-arm/middle-rail contact was then found with corrected contact logging; moving the paired side members toZ±0.65/±1.1 leaves100mm rail clearance. The final contact gate rejects every contact except deck/rider2973/2979. Do not use earlier CONTACT lines: the diagnostic initially held dangling references from std::minmax on temporary userdata, subsequently fixed.

`probe/run.py` explicitly rejects errors, pin mismatch≥5mm, rider-relative footprint drift≥100mm, cap excess, failed stall/return/brake, >10mm checkpoint replay error or >5mm90/360 endpoint disagreement. These are declared engineering screening thresholds, not shipping acceptance. All8 final cases pass: loaded, refined, unloaded,100kN stall, power loss, longitudinal eccentric load, restart and return. See `probe/results.json` and `final-*.log`.

At90Hz the powered loaded endpoint rise is23.282614m from initial construction pose;360Hz gives23.282331m. Different initial settling means settled-to-final travel must not be conflated with this metric. Nominal scripted input ends before the asymptotic target is exact.100kN stalls at the lower stop, as the independently derived~181kN initial loaded demand predicts. A midstroke power loss loses8.8mm relative to pre-switch height, then holds. Restart completes ascent; reverse returns close to the starting construction height. Cold Kit checkpoint replay after reset warm-start differs by~4.5mm after5s, not bit-identical.

Motor work is impulse/dt times coordinate displacement. `positive_work_minus_pe_fraction` is NOT a complete energy residual: it excludes kinetic change and dissipation, especially descent/braking. The initial fixture's always-zero process exit was replaced by the independent screening runner. World Update error flags are checked. Pin error is positional; guide angular reaction capacity remains unproven. Testbox retention is not real-player support evidence.

Pinned SliderConstraint.cpp confirms friction acts only with motor Off. It must not be counted as extra running friction when the velocity motor is active. Observed sampled peak mechanical power~35.8kW is not an enforced45kW electrical/80%-efficiency supply law. Battery depletion, power limiting and full energy/heat accounting remain production gates.

## Actual current native player: separate explicit fixture

`player-probe/native-fixture.patch` applies ONLY in scratch to current simulation.cpp: append the candidate after existing content, register deck2973 with the existing causal-support selector, call its pre/post step, and destroy it before Kit. No other player/controller changes. It retains the existing dynamic85kg capsule,90Hz owner and four collision substeps. Fixture direction is an explicit native test command, not a shipping reachable control. The candidate is placed at the proposed121m west entry; stage uses the existing debug_restart_at boundary, not campaign ascent. No extra body integration, player-weight force, or driven lift pose is added.

The first20s loaded rise keeps both centered and longitudinally eccentric real players grounded on2973 throughout1800ticks, with maximum vertical support offset error0.087mm. Estimated motor work is355226.899J without the player versus367892.012J centered and367891.111J eccentric. This is a physical effort difference under the same actuator input, not authored wobble.

The subsequent22s comparison in `departure-*.log` uses a fresh successful build. At20s, case3 issues ordinary request_jump and lateral move input; cases0/1 retain unloaded/loaded controls. Final means:

| Case | Mean actuator force | Actual final support |
|---|---:|---|
| Unloaded | −47519.844N | Player remains on Tower11 |
| Real player aboard | −49210.513N | grounded on2973 |
| Real player departed | −47519.918N | airborne, support0 |

The departing player has actual falling velocity−6.538495m/s; loaded carrier/player rise velocity before departure is0.475113m/s. Reaction load returns to the unloaded level (within0.074N) after contact is lost. This is reciprocal contact/unloading evidence, not a completed landing or fall-recovery route. The earlier failed compile of an incorrect snapshot field briefly left the old executable available; those stale departure outputs were discarded and replaced after an explicitly successful rebuild.

No missing shared player-load capability was found. Required future production seam is local deck identity registration plus native machine/control/checkpoint state. The paused agent's broader causal-world architecture remains untouched.

## Source and reference handoff

`SOURCE_MANIFEST.json` verifies all7 supplied module C++ files against supplied original hashes. The supplied Jolt license hash also matches current pinned dependency bytes. Full module archive is NOT materialized here;4 original files remain missing locally: AGENT_IMPORT.md, integration/integration-notes.md, modules.json, README.md. Do not describe this as complete module import.

All16 skill handoff files, including13 references and original READ_FIRST/MANIFEST/HANDOFF_INVENTORY, are hash verified under `/workspace/skill-reference-handoff`; exact upstream tracked directories are separately preserved under `/workspace/skill-references`. READ_FIRST was read; pinned engine source wins over upstream examples. Nothing installed or silently replaced. Raw34 packet JSON was not saved; the reconstruction receipt truthfully records the verified methods.

## Remaining implementation gates

Resolve finite supply/energy and heat law, derive final material section masses/inertias, show actual shafts/brackets/base tiebacks/mast engagement, verify lateral and full-stroke reactions/braking, complete native swept collision/standing clearance at both tongues, and wire reachable deck/landing controls and complete checkpoint energy state. Then real player boarding,143m receiver departure, retry/fall recovery, touch/gamepad/presentation, retained campaign gates and exact-source Android delivery. No Fold feel, readability or performance evidence is claimed.

## Finite-energy candidate (subsequent screening)

`energy-probe` reduces the target carriage speed from0.22 to0.18m/s, retains the proposed45kW/0.8 conversion/1.2MJ supply and200kN drive/220kN passive brake, and limits force by the larger of current/requested coordinate speed. Positive discrete actuator work is debited at0.8 conversion efficiency. A reserve based on coordinate speed plus0.02m/s disallows a step when insufficient energy remains; this empirical margin is screened, not a mathematical bound on every possible contact impulse. Cutoff latches until explicit state restoration; no regeneration or automatic refill is provided.

The isolated single-collision-step90Hz suite remains **FAIL** for a near-empty cutoff settling24.1mm against its20mm screening criterion. Full travel, power demand, no overdraft, midstroke cutoff and empty supply checks pass. Red evidence is retained; the criterion was not relaxed. At360Hz the corresponding near-empty case settled11.5mm. The first unlatched candidate is also retained as rejected evidence. No isolated suite green claim is made.

`player-energy-probe` instead accounts every existing Jolt collision substep with a `PhysicsStepListener`, flushes the last sample after the existing update, and never adds another physics update. This matches the game's90Hz host/four collision steps. It uses the same staged actual native player and candidate linkage. Fresh build and runs:

- `run 1 1200000 58`: player remains grounded on2973 through58s of commanded motion, reaches deck-center144.010620 (walk surface144.310620);566201.178J positive work,492248.528J remaining,38485.775W peak electrical, zero overdraft.
- `run 1 250000 30`: cutoff occurs partway up, deck-center128.910751;125.039J remains, no refill or continuing powered motion. Player remains grounded; maximum measured deck/player offset error18.277mm.
- `run 1 1000 20`: near-empty cutoff,22.270J remains, deck-center120.719536, grounded; maximum measured offset error0.912mm.
- `run 1 0 20`: no drive work, no supply change, deck remains at settled start120.693230, grounded.

All four processes return0 and assert nonnegative energy, zero overdraft, peak electrical<=45kW, final two-second release settling<=20mm and the existing support/gap limits. The support/gap counters cover the commanded travel loop; they do not cover the later2s sampling plus2s release windows. Final support is separately logged. The final release measurement after an already latched cutoff does not measure its earlier transient. `mean_force_n` after cutoff is the shared Jolt motor/friction impulse, so it must not be called powered actuator demand. Discrete force-times-displacement work is a numerical quadrature, not a complete solver energy residual. That first energy candidate lacked native checkpoint state; the subsequent serial candidate below adds it. No shipping controls, ordinary boarding, receiver transfer, structural certification, APK inclusion or phone performance is demonstrated.


### Serial callback and checkpoint candidate

The final scratch source routes `collision_step` through PhysicsWorld's existing serial `OnStep`, after hand/launcher work. There is no additional registered listener, preventing future lift hand-interaction races between parallel listeners. The machine checkpoint now captures/restores energy, positive-work ledger, target, test direction and cutoff latch after Kit bodies. It resets only this machine's constraint warm-start state through the existing helper. This patch remains outside production.

Fresh build then five `serial-*.log` runs all returned0: full travel; explicit native checkpoint restore at midstroke exhaustion, near-empty exhaustion and empty supply; and explicit native checkpoint restore during powered motion. Every restore reports exactly0J energy error, identical cutoff latch and0 target error. Full travel results are identical to the earlier candidate. Moving restore remains supported after resuming and subsequent input release settles0.046mm. Existing checkpoint behavior sets player velocity to zero; this test does not claim velocity-identical replay or ordinary gameplay recovery. The remaining structural, control and route gates are unchanged.


### Native boarding and receiver transfer

`player-route-probe` adds the proposed two static tongues (entity1972, same native/rendered Kit parts) and marks that actual support causal alongside deck2973. Freshly rebuilt scratch simulation plus `route.cpp` returns0. Initial staging only: `debug_restart_at(-25,121.9,-162)` on existing lower Tower11. Ordinary bounded movement input then walks onto the deck, direct fixture drive raises it, drive release engages passive brake near143m, and ordinary movement walks off onto existing Tower11.

- Lower ring: support11, grounded, player(-25,121.900002,-162).
- Boarded: support2973, grounded, player(-30.796455,121.894127,-162).
- Receiver alignment: support2973, grounded, player(-30.796455,143.890335,-162),532341.160J remaining.
- Upper ring: support11, grounded, player(-24.739704,143.899994,-162), same remaining energy.

The ride terminates at the player-selected143m receiver; separate full-stroke tests establish the23.31m available travel. This fixture's stop condition reads deck height to automate the operator decision; that condition is test-only and must not become shipping auto-alignment. It does not teleport the rider after initial staging, reposition the machine, or use a completion flag. This proves one native boarding/ride/release/transfer sequence, not touch/gamepad controls or comprehensive swept-clearance coverage. Final structural connections/mass/inertia, reachable station gates, production presentation, ordinary campaign extension and APK remain unfinished.

Integration inventory: reuse existing needle/jib held pendant input; add one finite clamped native drive axis, native station identity and state getters. Full3D reach/support/line-of-sight checks are required for deck/lower/upper stations; the old XZ-only station check is unsuitable. Input must explicitly become0 on operation exit/lost reach; missing setter calls do not release persistent axes. No new input framework or broader causal-world change is needed.
