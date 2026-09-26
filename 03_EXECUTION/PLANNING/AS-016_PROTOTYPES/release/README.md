# Native loaded-release fixtures

Profile: **MACRO-TRAVERSAL-STRICT**. Evidence: **OBSERVED_NATIVE_FIXTURE**.
These are bounded contact experiments, not a completed mechanism implementation
or a causal-compiler sign-off. No project files were changed by this work.

## Observations

`observations.json` records twelve runs from the checked-in source: 90 Hz and
360 Hz; active and no-input rack cases; bridge-prop offsets 35/40/45 mm plus
no-input controls. Every listed outcome/input-budget check passed.

- Rack: all twenty separate pipes reach the fixed receiver at both steps after
  a 150 N handle pull. Handle travel 0.289993 m; input work 43.499–43.500 J.
  Dead-centre crossing occurs 0.211–0.231 s after the pull starts. Maximum
  joint displacement is 0.466 mm at 90 Hz, 0.0993 mm at 360 Hz.
- Bridge prop: the 4.905 kN test load releases for all three offsets at both
  steps. Work to dead centre is 5.15–6.68 J; input travel there 34.28–44.20 mm.
  Work over the full allowed handle stroke stays below 43.55 J.
- No-input controls remain held and transfer no pipes.
- Bridge-prop joint displacement **through release** is at most 0.222 mm.
  The later laboratory load-drop impact reaches **5.071 mm at 90 Hz versus
  1.106 mm at 360 Hz**. This later impact is not a converged terminal model.

These are matching observed outcomes at two steps. A complete energy ledger,
calibrated metric convergence tolerances, and production cross-model agreement
have not been established; do not relabel this report INTEGRATED or READY.

## Reproduce

Use Jolt commit `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, built with the same
configuration as this repository. The driver lists the ABI preprocessor flags;
other Jolt configurations may require different flags. Tested with GCC15,
Linux ARM64 under Ubuntu proot. The source must include `Jolt/Jolt.h` first.

```sh
python3 run_probes.py \
  --jolt-source /path/to/jolt-src \
  --jolt-library /path/to/libJolt.a \
  --raw-dir /path/to/scratch/release-raw
```

Local reproduction used:

```sh
python3 run_probes.py \
  --jolt-source /data/data/com.termux/files/usr/tmp/scraperx-restore-build/_deps/jolt-src \
  --jolt-library /data/data/com.termux/files/usr/tmp/scraperx-restore-linux-build/_deps/jolt-build/libJolt.a \
  --cxx /usr/bin/c++ \
  --runner 'proot-distro login ubuntu --' \
  --raw-dir /data/data/com.termux/files/usr/tmp/scraperx-release-raw
```

`--skip-build` repeats the cases using existing binaries. `build/` and raw CSVs
are scratch outputs. The driver exits nonzero for a failed outcome/input budget.

## Exact selected rack geometry

SI; all dimensions and friction torques here are **CHOSEN**. Shape half-extents,
positions and constituent masses are explicit in `rack_release.cpp`.

- Rack slopes 5° downhill toward −Z, centred X6/Z−79.6, half footprint
  2.35×3.1 m. Floor surface at Z−82.7 is Y2.4. Side walls end at Z−81.95
  to clear the gate. Four pipe lanes have X centres 4.35,5.45,6.55,7.65.
- Each lane starts with five 800 kg, 1 m long, OD0.8 m pipe sections. Axes lie
  along X. Cylinder collision gives the real outside contact surface; hollow
  inertia is overridden from steel density7850kg/m³ and derived ID≈0.71431m.
  The hollow bore has no contact role in this fixture.
- Gate world hinge is `(6,4.4,-82.3)`, axis X. The lower blocking crossbar is
  4.4×0.3×0.3 m, mass200kg, centre1.6m below the hinge. It fits *inside* the
  receiver side walls. Two inner arms at local X±2.05 are each50kg, centres
  Y−0.8; an overhead 7.3m crossbeam is100kg at the hinge. An outboard east
  arm at localX3.5, centreY−1, is50kg; its40kg contact shoe is at Y−2.
  A20kg upper arm has centreY+0.6, and a500kg visible counterweight has
  centreY+1.2. These constituent shapes determine the actual COM/inertia.
- Gate lower gravity moment is530kg·m, upper moment612kg·m: net82kg·m
  above the hinge. After initial pipe thrust opens it, gravity continues
  opening toward a real contact stop. Gate hinge friction is30N·m.
- Open stop centre `(9.5,5.08,-83.1)`, half-extents `(0.3,0.15,0.3)`.
  Observed resting gate angle≈1.89663rad (108.67°). The stop is rigid,
  zero restitution; its finite stopping stroke/impact ledger is still open.
- Retainer: horizontal 5m prop about a vertical hinge, base X9.5/Y2.4;
  baseZ=`-82.75-5*cos(asin(.04/5))`. It starts40mm west of dead centre,
  bearing against a broad stop at its halfway station, then swings east.
  Steel member mass200kg, centre2.35m from its pivot; a40kg moving tackle
  block is4.8m out. Hinge friction30N·m.
- Rolling head: radius0.3m, axial length0.4m, mass50kg, axisY, hinge friction
  5N·m. Initial centre is `(9.46,2.4,-82.75)`, touching the gate shoe face.
- Operator handle: independent3kg cube0.3m wide at `(12,1.25,-82)`.
  A physical end stop limits its −Z travel to0.29m. It connects to the
  prop point4.8m out through a Jolt tension-only pulley relation with
  `ratio=1/3`. Fixed routing points are `(15,2.4,-82.95)` and
  `(12,1.25,-81)`. This represents a **3:1 visible tackle**; its actual
  reeving/visible sheaves and spin inertia must be supplied for production.
- Receiver is fixed: floor centre `(6,2.35,-85)`, half-extents
  `(2.3,.05,2.25)`; north wall centred Z−87.4; side walls X3.55/8.45,
  all wall half-height0.65, centreY3.05. This is not the moving pan.

## Bridge holding-prop fixture

`bridge_prop_release.cpp` is local geometry. To place the prop outside the
moving pan, translate X by≈3m and Z by−85m; confirm final integrated sweeps.

- Prop base `(0,.4,0)`, hinge axisZ. Axis-to-head length2.4m; initial head
  X offset is35/40/45mm. It leans east into its stop and folds west.
- The150kg member is0.3m square, from localY0 to2.1m, COM atY1.05m.
  A separate50kg roller has radius0.3m, axial length0.6m, axisZ, atY2.4.
  Prop and roller friction torques are15 and2N·m respectively.
- Stop acts0.3m above the base. Its half-extents are`(.15,.15,.4)`,
  positioned0.3m east of that point. Stop top≈Y0.85; maximum globalX≈3.455
  after the proposed translation. It must stay below the final tail seat.
- A500kg guided laboratory shoe supplies4.905kN, the derived nominal excess
  tail load of the 8.5t deck/17t loaded-pan balance. Shoe size0.3×0.3×1.2m.
  Its initial underside touches the roller; the vertical guide has no motor.
- A separate3kg handle at `(-3,1.25,.5)` pulls through a1:1 trip line attached
  2.3m up the prop, with fixed points `(-3,2.7,0)` and `(-3,1.25,1)`.
  Its physical stop permits0.29m. Both fixture operator inputs are150N.
- **This shoe reproduces the static support load only.** It is500kg, not the
  complete bridge's much greater reflected inertia. Production must attach
  this release to the actual bridge and include its effects during lifting
  through dead centre. Allow the bridge's lower rest seat at q≈−0.001rad:
  an exact zero-clearance lower deck seat would forbid the small required
  counter-motion. At40mm offset the geometric roller rise is0.333mm.

## Native contacts and representation

All moving/moving and moving/static collisions are enabled except the pair
joined directly at the rolling-head hinge (prop/head), which overlap at their
assembly. No pipe/gate/receiver contacts are filtered. Static/static pairs do
not collide. Hinges stay present throughout; no release deletes a constraint.
Physics uses20 velocity iterations,4 position iterations, no damping/sleep,
zero restitution, gravity9.81m/s²; body friction0.8 except the handle0.1.

The input starts at3s as a laboratory operator action, not a production
mechanism guard. Stops and loaded contact geometry determine release. Hinges
are tied to fixed world points in this fixture; production must show and rate
these support frames. The handle stop bounds player work; these runs do not
exercise the game's actual carry controller. Existing kit carry selection is
by body COM and has900N grip failure; the independent short handle avoids
making a distant point on a long body artificially reachable.

## Rejected cases and remaining proof

1. Bottom-hinged gate: its fallen panel would hold pipe weight above the pan,
   or strike the pan floor if allowed past horizontal. Replaced by upward gate.
2. Gate crossbar spanning across receiver side walls: native contacts blocked
   its sweep and only five pipes transferred. Narrowed load bar and moved
   outboard connection through an overhead member.
3. Gate without an open structural stop: its descending counterweight trapped
   one pipe. Added actual outboard stop;20/20 then transferred.
4. **Direct1:1 rack release:**90Hz150N released;360Hz150N remained locked.
   `rejected_direct_rack.cpp` retains this case. The fine-step450N diagnostic
   released and needed18.148J over40.329mm to cross dead centre. Prediction:
   3:1 tackle gives this output at150N and≈121mm input. The selected fixture
   confirms release at both steps. The detailed friction/contact cause of the
   higher breakaway load was not isolated; do not state a specific cause as
   proven. A likely contribution is slip required by a touching cylinder train.
5. Mid-height bridge-prop stop: falling shoe hit it before sufficient descent.
   Relocated stop near the foot; production must preserve that clearance.

Open: sheave rotational inertia; actual reeving and cable sweeps; all-friction
bands; full mechanism energy/impact ledger; complete bridge reflected inertia;
pipe containment during pan motion; actual player input and reach; production
solver agreement; Android export/device behavior. The fixture's later rigid
impacts are not evidence of a suitable production stopping strategy.
