# Native deformable timber plank

The normal Slingshot world now contains a segmented, elastic timber member with native player loading, grip and strength failure. Recorded checks cover elastic calibration, walking, standing, a finite-stroke foot push and Jump, landing, unloading, grip/release, interior fracture and recovery onto the existing tray. Scoped Godot rendering also follows the actual native segments. These are development receipts, not a commercial-release or phone-performance certification.

## Ownership and material assumptions

[DeformablePlank](../../src/sim/deformable_plank.hpp) owns the constraints; [Kit](../../src/sim/mechanism_kit.hpp) owns twelve actual bodies. [Simulation](../../src/sim/simulation.cpp) constructs the member, handles its collision-step lifecycle and restores its state after Kit body restoration. Logical member 1962 maps segment entities 2880–2891 without supplying a substitute collider or player footing. Seat 1937 carries the endpoint attachment. Adjacent segment contact is filtered while its joint is intact; fracture restores that pair's collision. Mating seat/end-segment contact is filtered while attached, and walking steel retains collision.

The plank is 2.4 m long, 0.19 m wide and 0.038 m thick, divided into twelve 0.2 m segments. Density 500 kg/m³ gives 0.722 kg per segment and 8.664 kg total. Young's modulus remains 9 GPa. Local X is length, Y thickness and Z width. Eleven internal SixDOF joints lock transverse translation and use zero-rest Position springs in physical `StiffnessAndDamping` mode:

| Axis | Stiffness | Units |
| --- | ---: | --- |
| Axial X | 297,825,000 | N/m |
| Weak bending Z | 39,096.3 | N·m/rad |
| Strong bending Y | 977,407.5 | N·m/rad |
| Torsion X | 52,570.5 | N·m/rad |

Weak-axis `I = b*t^3/12 = 8.688066667e-7 m^4`, hence `EI = 7819.26 N·m^2` and `k_b = EI/0.2 = 39096.3 N·m/rad`. Axial stiffness is `EA*11/L`. Fixed-end translation is locked while rotational stiffness is doubled to represent the half-cell boundary; the first segment is not welded. Ideal pin/roller supports have free rotation, and the roller releases axial X. The playable guided supports additionally restrain roll/yaw while retaining weak bending and axial roller travel.

Torsion uses an isotropic surrogate with assumed Poisson ratio 0.30 and rectangular Saint-Venant torsion. Damping is a Kelvin–Voigt surrogate, `c = tau*k`, with `tau = 0.002508006 s`, derived from an assumed 0.10 first-mode damping ratio. These are authoring approximations, not measured orthotropic timber data or validated all-mode damping.

Enabled strength limits are 40 MPa tension, 45 MPa compression and 6 MPa shear. They represent authored sound-timber assumptions, not certified wood grading. Static stiffness and elastic acceptance were not weakened to accommodate fracture or player tests. Fatigue, moisture, knots, grain defects and crack propagation are outside this model.

## Native solve, fracture and finite bearing

[The implementation](../../src/sim/deformable_plank.cpp) registers an intact multi-body `PlankSolveGroup` within Jolt's existing contact/island solve. Its constituent SixDOF joints are not separately registered. Setup and warm-start run once per collision substep; each velocity solve performs up to eight forward/reverse sweeps with impulse-convergence early exit. Island linking includes every dynamic participant, and the group uses the nonparallel split. Fracture rebuilds these groups for contiguous surviving fragments. This increases local solver work and limits parallelism for each group, without introducing another collision authority.

Default pin/guided budgets are 80 velocity/8 position iterations; cantilever defaults are 250/32. These are numerical budgets, not material changes. The normal Slingshot world retains its existing 90 Hz input/update interval and four Jolt collision substeps. Segment angular caps are 500 rad/s to avoid clipping free endpoint-load integration before the solve; the player cap is unchanged.

Strength demand is derived from solved impulses divided by substep duration, in the actual section frame. Combined axial/bending tension or compression and transverse/torsional shear produce `ratio = sqrt(max(tension_ratio^2, compression_ratio^2) + shear_ratio^2)`. An interior seam reaching one queues failure. The peak survives later unloaded substeps. `finish_collision_steps()` removes failed constraints outside `Update`, restores pair collision and activates the actual bodies. It does not write body pose, velocity or mass. Estimated discarded strain energy is recorded as dissipation; complete fracture-energy closure is not established.

The playable right support is a finite guided fork: axial travel beyond ±0.045 m releases the endpoint outside `Update`, restores seat/last-segment collision and leaves a real detached fragment. This bounds the authored 90 mm fork/shoe engagement instead of retaining an infinite roller after rupture. `fragment_for_body()` identifies contiguous native pieces, and `fragment_supported()` recognizes only pieces still attached to a support. Footing rays reject unsupported timber; hands retain actual fragment/body anchors.

`State` preserves the broken mask, right-support release flag, peak ratio, fracture serial and discarded strain estimate. Restoration rebuilds constraint topology and collision filtering after Kit restores bodies. The separately executed module restore check covers ideal PinRoller intact/broken states. Guided release restoration is implemented, but a normal-world post-damage restart has not been exercised by the receipts below.

## Recorded isolated calibration and fracture

[The benchmark](../../tests/deformable_plank_tests.cpp) disables gravity, applies forces at physical material points, then unloads and checks return. Centre loading is 500 N at each adjoining endpoint at X = 1.2 m, rather than their COMs. Cantilever loading is 100 N at the actual X = 2.4 m tip, including its torque lever. Acceptance remains 2% against independently derived discrete compliance.

| Case and budget | Jolt deflection | Discrete | Continuum | Jolt/discrete error | Discrete/continuum error |
| --- | ---: | ---: | ---: | ---: | ---: |
| Pin/roller 1000 N, 80/8 | 37.303760648 mm | 37.343687254 mm | 36.832129895 mm | −0.1069166% | +1.3888889% |
| Cantilever 100 N, 250/32 | 59.075400233 mm | 59.136030775 mm | 58.931407831 mm | −0.1025272% | +0.3472222% |

For twelve segments, discrete centre compliance is continuum compliance multiplied by `1 + 2/12^2`. Cantilever compliance includes `L^2/(2*k_b)` half-cell boundary compliance and is continuum compliance multiplied by `1 + 1/(2*12^2)`. Discretization error is separate from Jolt's numerical error.

Receipts are under `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/plank-probe/`. `elastic-fracture-final.log` records both elastic PASS results, signed/rotated-axis checks and fracture PASS. Local isolated mean Update costs were 4.009 ms per pin tick and 9.974 ms per cantilever tick, including four substeps. These harness wall times are not Android release frame-budget measurements.

The fracture receipt records mask 508, seven failed interior seams, peak ratio 1.931498408 and serial 7. It verifies retained substep overload, no instantaneous pose/velocity change at fracture commit, actual native contact/query/filter behavior and Kit-plus-member intact/broken restoration without extra events. The test deliberately retains an overload queue through subsequent relaxation; its tiny discarded strain estimate is not a physical fracture-work calibration.

Maximum partial elastic energy excess was 0.000013259 J. That diagnostic includes native kinetic energy, planar quaternion bending and axial strain against conservative dead-load work, including unloading. It rejects the previously observed runaway but does not establish full three-axis, contact or damping energy closure.

## Recorded normal-world player and render evidence

[The player driver](../../tests/deformable_plank_player_tests.cpp) stages once on left steel at (−8.8, 407.9, −128.2), then uses ordinary inputs. Timber occupies X [−8, −5.6], centred at Z = −128.2, with neutral centre Y = 406.981 m/top 407 m. It records actual native poses and requires grounded crossings of all eleven seams; capsule manifolds need not select every segment as principal support.

`player-strength-final.log` records walking/standing/Jump/landing/unloading/onward-steel PASS with strength enabled: 31.295776 mm extra sag under the native 85 kg player, within the 20–45 mm acceptance band; all eleven grounded seam crossings; 30.914307 mm board-motion change; unload return within 0.030518 mm of the gravity-only baseline; native gravity and zero deaths. The separate grip sequence passes actual two-hand segment-edge capture, finite forces, native player loading, passive release, real recovery-tray contact and unload return.

The original point impulse produced only 0.1978 m/s upward speed. [The native compression-only foot push](physical-foot-push.md) now requests 0.15 m over at most 0.25 s through the actual segment contact point, with 3,500 N force and finite command-work bounds. Normal native and viewport-touch input produce approximately 0.962/0.964 m/s peak vertical speed and 0.147/0.148 m COM rise, actual intact landing and onward steel footing. Full-stroke prototype overload is retained as failure evidence; material strength is unchanged. The new retained-player run passes all eleven seams, grip/release, unloading (0.061035 mm maximum Y return error) and real impact-fracture recovery. These receipts do not guarantee ordinary 5.5 m/s takeoff or establish phone feel.

`player-impact-final.log` stages an actual 85 kg player for a roughly 3 m fall above the timber. Native impact breaks mask 112 (three interior seams), with peak ratio 1.341147542. Ordinary air steering reaches the existing 403.5 m tray with valid checkpoint footing, persistent damage and zero deaths. Body 2891 finishes at centre Y = 403.518982 m, versus neutral 406.981 m, demonstrating the released right bearing and real fragment fall. This is a staged local impact/recovery proof, not a connected route approach or a verified restart after damage.

`visual-native/visual-receipt.json` records a 14-body presentation selection from the actual normal-world bridge, using [KitView](../../godot/presentation/kit_view.gd). All twelve segment render transforms agree with native readback within render interpolation, including the player-loaded three-second pose; the receipt has no failures. Its scope is X11 render/native-transform evidence with explicit player staging. Low-resolution screenshots do not establish full-game phone fidelity, performance, route usability or commercial readiness.
