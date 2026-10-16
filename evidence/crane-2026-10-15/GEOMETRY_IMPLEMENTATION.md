# Yard crane geometry implementation — 2026-10-15

Bounded worker source: `src/sim/yard_crane_geometry.hpp/.cpp`, based on branch
`ChatGPT`, HEAD `8569c721432e4139df6286b8e8d1cce94b2db9ca`. Existing dirty and
untracked files were preserved. This module creates solids only: no joints,
collision filters, motors, listeners, pose controllers or line ownership.

`build_yard_crane_geometry(kit::Kit&)` returns Kit body indices for foundation
1450, docks1451/1452, mast2450, boom2451, trolley2452 and spreader2453. It rejects
an already-owned ID before allocating bodies. Kit owns the resulting solids.

## Frames and connections

All returned attachment vectors use the authored shape frame, not the COM
frame. Foundation and mast origin are world(-60,0,-150); boom origin/pivot is
(-60,36,-150), initial yaw -pi/2. Boom local+Z extends35m forward and10m aft;
its lower and upper chords are2m apart. Initial trolley origin is
(-80,35.65,-150), oriented with the boom, local rail travel8..35m. Its support
eye(0,-.15,0) lies on the actual sheave's lower circumference. The sheave has
connected cheek plates, axle, crosshead and trolley frame. Wheels touch the
underside of the lower boom chords at the initial pose; the native guide and
bearing belong to the controller.

Spreader origin is(-80,4.4,-150), yaw+pi/2, matching pickup chassis
(-80,.604,-150). Its eyes, in exact `WarehouseForkliftYard::lifting_points()`
order, are(-.52,-.125,+.75), (+.52,-.125,+.75),
(-.52,-.125,-.75), (+.52,-.125,-.75)m. Direct plumb lengths are
3.356,3.356,3.106,3.106m. The open rectangular frame leaves the forklift's tall
mast clear. No intermediate shackle bodies are created.

The hoist eye is(0,.24,-.275599)m, above the derived spreader COM
(0,-.014400,-.275599)m. The added crosshead and hook hardware each have the
same z centroid as the existing assembly; their actual material therefore
preserves the z alignment. Orientation remains a real dynamic degree of
freedom. Neither COM nor inertia is independently relocated.

## Material and support

Moving steel masses use7800kg/m³; concrete ballast uses2400kg/m³. RHS members
have four explicit wall solids and real hollow apertures, with nonoverlapping
wall sections. Kit/Jolt derives compound COM/inertia from these part masses;
the body mass argument is the sum of those masses. Weld intersections and the
solid axle/sheave bearing regions have small local overlaps; these remain
ordinary authored compound solids, not hidden mass offsets.

Analytic part sums (float rounding applies at runtime): mast43720.62kg,
boom28232.68kg, trolley433.60kg, spreader228.6452kg. The mast includes a real
3x3x.3m steel bearing head; its substantial mass is explicit. Boom includes a
14000kg concrete block on connected steel bearers, centered7m aft of pivot.
The spreader includes100x150x5mm RHS rails/crossmembers, a56kg rear ballast
block on an actual29.952kg support plate, open lifting eyes and a connected
central hook/crosshead. Its returned mass and COM are derived from the actual
part list. Structural sizes are chosen game geometry, not calibrated load
capacity or safety certification.

The12x12m concrete foundation reaches grade and has a west-side support pad
and a real cabinet at the ground station(-66.5,1,-150). Root supplies the
rated foundation/mast and mast/boom interfaces. Foundation and dock bodies
are static structural supports with legitimate ground reaction.

Dock decks occupy x[-36,-25.5], z[-144,-136] at y11 and z[-164,-156] at y22.
Their eastern .5m overlaps the existing ring. Ground columns and diagonal
braces stay at zcenter±3.75m; crossbeams remain below deck. West receiving
faces are open. East guardrails terminate1.4m either side of center; no new
member blocks the center driving corridor. Existing world solids are untouched.
The previously accepted full-world entrance sweeps are root evidence; this
module's final integrated clearance and moving load path remain root acceptance.

## Actual compile evidence

Final source passed (exit0) with:

```sh
proot-distro login ubuntu -- /usr/bin/c++ -std=c++17 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Werror -DSCRAPERX_HAS_JOLT=1 -DJPH_DEBUG_RENDERER -DJPH_OBJECT_STREAM -DJPH_PROFILE_ENABLED -I/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src -isystem /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src -c /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src/sim/yard_crane_geometry.cpp -o /data/data/com.termux/files/usr/tmp/yard_crane_geometry-worker.o
```

Output contained only the host's existing proot `/proc/self/fd/1` and
`/proc/self/fd/2` binding warnings. An earlier source revision also compiled;
the final compile followed the actual56kg ballast dimension correction and
addition of the trolley's connected sheave crosshead/axle. No whole native
build, runtime physics test, APK build or device observation was performed by
this worker. Root owns integrated mechanics and delivery.
