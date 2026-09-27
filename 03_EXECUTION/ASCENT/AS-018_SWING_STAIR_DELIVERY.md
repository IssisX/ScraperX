# AS-018 — swinging stair delivery

**Status:** Green exact-source desktop route and Android ARM64 artifact at
`68e9422cf6a1b7282264b2fd9eed7ae10ba25bf8`, 2026-09-27.
[Run 36295602489](https://github.com/IssisX/ScraperX/actions/runs/36295602489)
is the delivery gate. This does not establish installation, Android execution
or sustained Fold performance.

## Connected route

Normal gameplay starts at grade, loads the pipe bridge, reaches the supported
+11 m ring, traverses AS-017's façade to the +33 m ring, then crosses the
swinging stair to tower support at +44.9 m. The stair is a visible 4 t flight
with a 13.5 t counterweight, a hand-pulled chain and trip lever, and two
finite-stroke receiver pads. The player must pull the chain: taking the handle
or waiting does not release the flight. The north-side exit joins the existing
tower deck. The old machine-linked stair geometry and entity IDs stay absent;
the ordinary stairs to +154 m remain an optional fallback.

`src/sim/swing_stair.*` constructs the only active stair assembly through the
existing Kit/Jolt owner. Entity 1601 is its fixed frame; 2600 is the flight,
2601 the trip lever, 2602 the carry handle and 2603–2604 the yielding pads.
`Simulation` advances the receiver material and checkpoints/restores its
plastic front and work. Godot presents native body poses and submits ordinary
touch input; it does not move the stair on a presentation clock. Geometry,
mass, receiver and exit bounds are recorded in [the Atlas](../../01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md) §7.

## Exact-source proof

- The host suite passed **15/15** tests. Five new grade-to-stair native modes
  cover settled and early boarding, fatal fall with moving-support checkpoint
  restore and continuation, no interaction, and grab/release without pulling.
  Mode 2 commits its checkpoint only after observing actual stair contact;
  its earlier x86 failure was an airborne sample at one horizontal waypoint.
- The continuous rendered 30 FPS touch scenario used ordinary input from grade
  to supported tower Y=44.900, zero deaths. It captured pipe, façade, chain,
  moving flight, exit and arrival milestones. The route used a temporary
  432×371 Fold-aspect CI canvas and restored the shipped viewport before APK
  export. Local ARM Godot headless also completed the touch scenario.
- Retained parkour, default-scene inventory/fallback stairs, fixture input,
  audio, both native-vs-rendered solids tables, Android ARM64 native build and
  APK export passed in the same run. The rendered release and climb captures
  show the treads and supported checkpoint; the final deck view is partly
  obscured by an existing tower brace. These desktop frames do not establish
  device readability.
- [Artifact 10924760938](https://github.com/IssisX/ScraperX/actions/runs/36295602489/artifacts/10924760938)
  contains `ScraperX-68e9422cf6a1-arm64.apk` (32,895,735 bytes). The embedded
  checkpoint names branch `ChatGPT` and the full source SHA; the APK includes
  `lib/arm64-v8a/libscraperx_native.so`. The artifact manifest and downloaded
  APK agree on SHA-256
  `9810ecc08824c979308c46e7ff8465b86fc343ad7b71ada678d80fb6e5abd1b6`.
  A checksum-matched copy is in Android Downloads.

The generated `GROUND-RESET-EVIDENCE.md` in the artifact summarizes the
foundation and opening pipe mechanism. This ticket and the exact run's route
log own the later +33→44 m claim. At this run's source SHA, the +44→66 m
continuation was unauthored; the later AS-019 candidate is tracked in
`00_START_HERE.md` §2. A green Android export is not a completed tower ascent.
