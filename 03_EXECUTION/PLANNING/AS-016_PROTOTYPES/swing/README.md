# AS-016 isolated bridge swing evidence

Profile: **MACRO-TRAVERSAL-STRICT**. This package proves an isolated, preloaded
four-bar bridge and finite receiver model. It does **not** prove pipe loading,
release controls, pipe/arm clearance, character traversal, or Android gameplay.
No production files were changed by these probes.

## Selected layout and evidence

The chosen geometry, masses, material parameters and sampled cases are in
`cases.json`. The deck slab is 20 x 3 x 0.4 m, 8,500 kg, with its COM 0.2 m below
the walking plane. Two 500 kg arms have vector (-5, +2.8) m. Matching fixed and
pan hinge pairs have 1.2 m vertical separation. The 17,000 kg pan is preloaded;
it represents the 1,000 kg pan plus twenty 800 kg pipes. The pan stays level by
native hinges, not orientation corrections. This fixture uses centreline arms
and disables moving/moving collisions. Production arms must be placed outboard
of the pipe volume and checked with collisions enabled.

The receiver lies below the descending pan. Its upward normal opposes bridge
rise. Receiver entry height is +0.805371263 m at the pan striker, 1 m below the
main pan pin. Available stroke is 0.65 m. A real static ultimate floor is below
that stroke; none of the evaluated runs touches it.

The selected material has yield 90 kN, stiffness 15 MN/m and a chosen yield band
of +/-5%. These are declared simulation material choices, **not measured timber
properties**. The force law is unilateral elastic/plastic contact:

```
penetration = max(0, receiver_top - striker_y)
plastic_front = max(previous_front, penetration - yield / stiffness)
elastic = max(0, penetration - plastic_front)
normal_force = stiffness * elastic
plastic_work += yield * change_in_plastic_front
spring_energy = stiffness * elastic^2 / 2
```

Native hinge dry friction and plastic crushing dissipate energy. There is no
linear/angular damping, prescribed motion, pose snap, sleep, completion-driven
capture, or velocity reset. The probe's `settled_s` is only a diagnostic; it
never changes state. Jolt exposes hinge friction through its motor-constraint
impulse accessor, but no powered motor is enabled.

The independent evaluator derives:

```
Qgravity = 9.81 * [(2500 - rider_mass*rider_radius)*cos(theta)
                  + 47300*sin(theta)]
I = 1703020.8333333333 + rider_mass*(rider_radius^2 + 0.0016666666666667)
Qreceiver = normal_force * [5*cos(theta) + 2.8*sin(theta)]
```

The small rider inertia term belongs to the 0.1 m cube used as an attached load
proxy. It is not an approximation of the production character's shape.

## Results

All 27 combinations of hinge resistance (1.5/3/4.5 kN m), load (empty or 85 kg at
10/20 m), and material yield (85.5/90/94.5 kN) ran at both 90 and 360 Hz. Native
solver settings were the defaults: 10 velocity and 2 position iterations.
The independent evaluator ran every case at 1 ms and 0.25 ms.

- First turn: 5.922–9.456 s. Peak tip speed: 3.5822 m/s.
- Maximum total rigid-load acceleration: 0.46755 g.
- Terminal tip: +7.5538 to +8.1713 m. Entire post-turn envelope:
  +7.5356 to +8.1821 m.
- Two seconds after first turn, remaining motion spans at most 2.927 cm per
  case, with tip speed at most 0.231 m/s. Native character traversal must
  decide usability; exact zero velocity is not being claimed.
- Maximum receiver penetration: 0.50990 m, leaving at least 0.14010 m.
- Maximum hinge mismatch: 0.394 mm; pan angular error: 0.000754 rad.
- Maximum native late energy residual: 0.609% of released energy.
- Maximum evaluator residual: 0.000254%.
- Maximum native/evaluator discrepancies: 13.31 ms to first turn,
  0.00618 m/s peak speed, 4.35 mm plastic stroke, 14.57 mm tip envelope,
  0.00871 g peak acceleration. All pass the declared bounds in `evaluate.py`.
- Evaluator refinement changes tip-envelope metrics by at most 0.113 mm.

The bed can briefly unload during small rebounds. At +2 s, separation from the
crushed face is at most 2.918 mm. The frame hinges remain engaged and gravity
returns the body to its receiver basin. Continuous bed contact is not claimed.

`native_results.json` contains compact case measurements. `evaluation_results.json`
contains the independent model/refinement/cross-model record. Summary files are
small entry points. Raw CSV traces and compiled binaries are deliberately not
included in this source evidence bundle; the driver recreates them.

## Rest matrix and limitations

- Full source present, no rider: evaluated.
- Full source present, fixed rider at 10 m or 20 m: evaluated.
- Rider gone: evaluated as separate empty-load initial conditions only.
  **Dynamic detachment or walking off a settled bridge has not been tested.**
- Partial pipe load, pipe loss, reset/rearm, and release mechanism: not covered.
- No source-mass edits, player teleports, or hidden lifting forces occur during
  a probe, but the initial preloaded condition is an explicit fixture abstraction.
- Source unloading can reverse the load. The one-sided receiver is not claimed
  to hold that untested case.

## Rejected exploratory choices

- 4 t deck: excessively large surplus gravitational energy; not retained.
- 5 MN/m receiver stiffness: about 7–10 cm post-turn tip oscillation and up to
  0.44 m/s residual motion. Increasing real material stiffness reduced it.
- 30 MN/m with explicit force evaluation at 90 Hz: nominal stop differs by
  about 19.5 mm from 360 Hz and ledger error rises to about 0.91%. Retained
  15 MN/m avoids relying on that short, under-resolved elastic entry.
- 95 kN +/-5% yield at 15 MN/m: high-yield early-rider corner approaches 0.5 g
  too closely for the cross-model margin. Selected 90 kN +/-5%, recomputing
  receiver entry and preserving the same target pose through longer arrest.

## Reproduce

Use the same Jolt revision/configuration as the project. The build flags match
its cached release archive with debug renderer, object stream and profiling
compiled in. If Jolt is built with other ABI features, match those flags.

```
sh build_probe.sh /path/to/JoltPhysics /path/to/libJolt.a
python3 run_native.py
python3 evaluate.py
```

For the current Termux/Ubuntu environment, compile inside Ubuntu and run the
native driver with `--command-prefix 'proot-distro login ubuntu --'`. `--exe`
selects a binary outside the bundle; `--output` selects an output directory.
The evaluator accepts `--directory` for that directory. No network is used.

Probe positional arguments:

```
native_probe HZ FRICTION RIDER_MASS RIDER_RADIUS YIELD CSV STIFFNESS VELOCITY_ITERS POSITION_ITERS NOMINAL_YIELD
```

The recorded run used 15,000,000 N/m, 10/2 solver iterations, nominal yield
90,000 N, and the Cartesian case grid in `cases.json`.
