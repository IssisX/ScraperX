# Work Order 006 — First Coupled Machine

## Objective

Build the first length of the tower's Rube Goldberg spine as one causally complete, physically coupled machine the player can enter mid-cycle, walk into, ride, and disturb:

`hoist lifts ballast → discharge → ballast falls → strikes a hinged tipper → tipper drags a tension-only rope → rope opens a real valve → valve vents a finite pressure vessel into an actuator cylinder → cylinder pressure pushes a piston → piston lifts a counterweighted platform the player can stand on → cylinder bleeds → platform sinks → ballast returns down a basin into the hoist → repeat`

## Governing authority

- Governing Law 11 — the principal systems must function as one coupled industrial machine rather than isolated minigames.
- Governing Law 13 — causal stacking: important interventions propagate through authoritative systems.
- Governing Law 14 — emergence must remain legible through inspectable mechanisms.
- Governing Law 15 — randomness may not replace mechanism.
- **Governing Law 24 — machinery cannot create capability from nothing.** Finite mass, force, power, stored energy.
- **Governing Law 25 — reduced-order physics can be real physics** when it is the authoritative representation, preserves the quantities gameplay needs, and is validated for the behaviours claimed.
- Governing Law 26 — no physics theater; no scripted collapse presented as emergent, no fake telemetry, no presentation acting as a second authority.
- TDD §4.2 — authoritative tick ordering: commands, actuators, process state, apply to Jolt, advance Jolt, read back, publish snapshot.
- TDD §9 — machinery is explicit mechanisms with finite limits; actuator logic requests forces, it does not write a successful transform.

## Owner

`scraperx_sim` owns every link. Jolt owns the rigid bodies, hinges, sliders, the tension-only rope and the pulley. `SteamPlant` — a ScraperX-owned reduced-order solver layered beside Jolt, exactly as TDD §0 permits — owns vessel and cylinder pressure, orifice mass flow and piston force.

Godot owns input and presentation. The steam plume is a one-way consumer of the native orifice mass flow.

## Reduced-order model and its bounds

The plant is a fired vessel venting through a thin orifice into an actuator cylinder. Pressure is ideal-gas, `P = mRT/V`, at fixed 450 K with water-vapour `R` and `γ = 1.30`. Orifice flow is standard compressible flow, choked above the critical pressure ratio and subcritical below it, and identically zero when area or pressure difference vanishes. The boiler feed is pressure-regulated and rate-limited. The cylinder has a permanent bleed port, which is why the platform sinks again.

Explicitly **not** modelled, and stated rather than implied: condensation, heat transfer, temperature change on expansion, backflow, and cylinder volume change under piston travel. The blowdown time constant is ≈1.7 s against an 11.1 ms fixed step (ratio ≈0.006), so explicit integration with a transported-mass clamp is stable here; that clamp is what keeps both reservoirs monotone.

## Forbidden shortcuts

- Any link driven by a timer, animation, trigger volume or mission flag instead of the previous link's actual body state.
- A valve fraction that is not read from the real lever body angle.
- A piston force that does not fall as the vessel empties.
- A plume with its own clock.
- Teleporting the ballast back to the top of the hoist.
- Presentation writing any machine state back into the simulation.

## Proof path

1. Host tests prove the chain fires from one cycle: valve past half open, piston past 8 kN, platform lifted past 6 m.
2. Host tests prove a shut valve passes **exactly zero** mass flow.
3. Host tests prove the loop closes: a second unattended cycle lifts past 6 m, the platform sinks again, and the counterweight returns.
4. **Law 24 falsifier:** with the boiler feed disabled the vessel's available energy never increases across five cycles, the reservoir draws down, and the drained plant leaves the lift low instead of holding it up for free.
5. **Player-disturbance falsifier:** a player standing on the tipper rotates it off its rest stop, opens the same real valve, and charges the same cylinder.
6. Host tests prove machine state is frame-partition invariant.
7. The Godot runtime observes the chain from the yard and prints peak valve, peak lift, peak flow and shut-valve flow.
8. The same source cross-compiles for Android arm64 and exports an APK.

## Completion

Complete when all proof steps pass from one source commit.

## Result record

- **Changed:** new `src/sim/steam_plant.{hpp,cpp}`; `src/sim/simulation.{hpp,cpp}` (machine bodies, hinge/slider/rope/pulley constraints, hoist kinematics, plant stepping, machine snapshot fields, `set_boiler_feed_enabled`); `src/bridge/scraperx_simulation.{hpp,cpp}` (machine accessors); `tests/simulation_tests.cpp`; `godot/presentation/main.gd` (machine mirror, state-driven plume); CI workflow.
- **Built:** host `scraperx_sim` + `scraperx_sim_tests` and the Linux GDExtension, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`.
- **Executed:** `./build/host/scraperx_sim_tests`; a seven-cycle soak harness; the Godot runtime under Xvfb.
- **Observed:** `PASS scraperx_sim coupled machine: valve=0.726923 piston=14691N lift=8.80227m second_lift=8.80816m starved_energy=685947J player_valve=0.325553 tower_m=1600`. Seven consecutive unattended cycles each fired with peak lift 8.80–8.83 m and the vessel settling to a 3.76 bar feed/vent equilibrium. Starved from 7.17 MJ, the vessel drained to 0.69 MJ and the lift faded. Runtime: `SCRAPERX_WO006_MACHINE_PROOF peak_valve=0.73 peak_lift=7.74 peak_flow=2.033 shut_flow=0.00000 vessel_bar=3.90 vented=2.14`.
- **Android build:** CI run [35227271830](https://github.com/IssisX/ScraperX/actions/runs/35227271830) on this exact commit cross-compiled the native graph for Android arm64-v8a and exported a debug APK containing `libscraperx_native.so`, with checksum recorded. This proves **implemented + built (Android arm64) + APK produced**, from the same source commit as the host/runtime evidence above.
- **Unverified boundary:** APK installation, Android execution, Fold 6 panel observation, touch ergonomics, sustained frame rate and thermals with the machine running; structural coupling from the machine into the tower (WO-007+); persistence of machine state across save/load; multiple simultaneous machines; and the rest of the Rube Goldberg spine above this first length. Those claims remain separate per Execution Protocol §11 claim discipline.
- **Regressions:** none. Every WO-001..003 assertion passes unchanged.
