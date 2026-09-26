# Native static receiver prototype — isolated evidence

**Selected geometry:** the upper side cheek is 0.25 m below the nominal bridge plane. It ends at +7.75 m with a 0.25 m step onto the +8 m dock. Geometry scope is frozen after the targeted native and analytic checks below.

This package verifies only downstream walking geometry. It does **not** implement or prove the pipe-loaded mechanism. The fixture adds a static 20 m bridge, grade approach, side cheeks, dock and onward incline to an outside-repository copy of simulation.cpp. The original native controller, default grade spawn, tower and exported world collisions remain in use.

The selected geometry passed native walks at bridge-tip +7.53 and +8.20 m, bracketing the dynamics probe's +7.53556…+8.18202 m post-turn envelope. Both runs began at actual default spawn and finished grounded on native +11 m tower support entity 11, player centre Y=11.8999996 m. No jump request, teleport, player-position seeding, death or movement retuning was used. Grounded support is asserted at settled waypoints; continuous grounding at every intervening frame is not claimed.

## Portable reproduction

Keep these four files together:

- `static-walk-fixture.patch`: narrow 56-line patch; the full copied engine is generated.
- `static_walk_main.cpp`: ordinary public walk/facing input driver and support assertions.
- `make_static_walk_probe.py`: makes an outside-repository copy and applies the patch with zero fuzz.
- `run_static_walk_probe.py`: builds against cached native modules/Jolt and runs the cases.

```sh
python run_static_walk_probe.py \
  --repo /path/to/ScraperX \
  --build /path/to/cached-native-build \
  --jolt-src /path/to/JoltPhysics \
  --compiler c++ \
  --tip-heights 7.53 8.20 \
  --out /tmp/scraperx-receiver-proof-new-run
```

The cache must provide compatible `libscraperx_sim.a` and `_deps/jolt-build/libJolt.a`. Compile definitions are explicit in the driver. For the observed Linux ARM64 build under Ubuntu proot, add `--compiler /usr/bin/c++ --runner 'proot-distro login ubuntu --'`. Execution prefixes are parsed into arguments without a shell. The output directory must be new and outside the repository; prior observations are never overwritten.

## Selected observations

`selected-offset-025-observation/` contains the selected `results.json`, generated-fixture manifest, build log, and immutable `static-walk-7.53.log` and `static-walk-8.20.log`. Results record exact commands, source/artifact hashes and settled waypoint positions/support IDs.

The separate analytic checker now samples +7.53/+8.00/+8.20 m and passes ten checks. Across the full declared side-exit corridor, the worst upward step is **0.272183 m**, leaving **0.077817 m** below the controller's 0.35 m limit. The cheek-to-dock step leaves **0.10 m**. The worst downward side transition is **0.476961 m**; the native high-pose walk passed at the chosen middle crossing. The conservative pan envelope remains at least **0.189962 m** above grade. That pan clearance is analytic, not a native pan-contact result.

## Superseded offset −0.20 m

The earlier cheek passed five native cases (+7.53/+7.55/+8/+8.17/+8.20 m), preserved in `portable-observation-envelope/`. It was **rejected for insufficient declared corridor margin, not for failed walking**: worst upward step 0.322183 m left only 0.027817 m. Its narrow patch is retained as `observed-static-proof/static-walk-fixture-offset-020.patch`. The selected −0.25 m refinement was then checked only at the low/high envelope endpoints to resolve that specific concern.

## Limits

Not constructed in the native fixture: pipes, rack gate, pan, four-bar, release, arrest or actual mechanism body contacts. Reserved-volume clearance belongs to the separate analytic checker. No Godot presentation, touch/controller UI or Android device execution was performed. A successful static crossing cannot establish those missing proofs.
