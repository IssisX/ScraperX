# Look suite: stills of the Matron in her key moments

A quick, cheap way to see how she actually looks: still PNGs from cameras
placed about her own drawn body, framing all of her (and the climber, in her
hands), never the player's eye. Renders only; it checks nothing and is not in CI.

    tools/look/look.sh [matron|moves|hold|fatal] [--out DIR] [--size WxH] [--serial]

Default: set `matron`, `--out build/look`, `--size 640x552`. It needs Godot at
`tools/godot/godot` (or `$GODOT`, or the main checkout's) and this tree's
`godot/addons/scraperx_native/bin/linux/libscraperx_native.so` built with the
look hooks:

    cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DSCRAPERX_BUILD_BRIDGE=ON \
      -DSCRAPERX_WITH_JOLT=ON -DGODOTCPP_TARGET=template_release
    cmake --build build/host --target scraperx_native

(Another checkout's `build/host/_deps/*-src` can be reused with
`-DFETCHCONTENT_SOURCE_DIR_JOLT=... -DFETCHCONTENT_SOURCE_DIR_GODOT_CPP=...
-DFETCHCONTENT_SOURCE_DIR_MONOCYPHER=...`; Jolt's source must already be patched.)

## What it writes

`DIR/<set>_<shot>.png`, `DIR/<set>_index.tsv`, `DIR/<set>.log` and `DIR/index.md`:
for each still its sim time, her state and stage, what it is meant to show, the
wall seconds it took (a set's first still counts Godot's start, about 7 s), the
camera (azimuth from her facing, distance), and what was measured of her there:
on her pipe, each drawn wrist and ankle's distance across the ground from the
riser's faces and the native's contacts; in her hands, his body's up and the
hold's state.

| set | shots |
|---|---|
| `moves` | `walk` (upright, a pest), `crawl` (all fours, dangerous), `ladder` (3 m up her ladder), `pipe` and `pipe_side` (3.5 m down her corner pipe) |
| `hold` | `grab` (her hand just closed on his ankle), `hold` (him upside down over the drop), `rip`, `rip_after` |
| `fatal` | `hang` (the fatality's hang window), `let_go`, `let_go_fall` |

The three sets of `matron` run side by side (one Godot under xvfb each);
`--serial` runs them one after another.

## How she gets there

One look scenario per set (`look_matron_moves`, `look_matron_hold`,
`look_matron_fatal` in `godot/presentation/ui/ui_test_look.gd`, registered in
`ui_test_driver.gd`, spawn 11, her home on the 154 deck). Each starts a moment
the way the native tests do, through TEST-ONLY bridge hooks that only pass the
native tests' own `Simulation::debug_*` through (`debug_place_matron`,
`debug_set_matron_stage`, `debug_matron_climb_way`, `debug_matron_hear`,
`debug_launch_player`, `debug_lose_arm`; nothing in play calls them), and then
the simulation takes her into it for real: her walk to a noise, her run on all
fours, her climb, her hands on him at the rail (`at_the_rail` in
`tests/sim_tests_matron_grab.cpp`), the fatality once his arm is gone. Nothing
poses her figure. Drawing is off between stills; at a still the simulation is
held for its few frames, the HUD hidden, and the look camera made current.

To add a shot: a `_look_until(condition, timeout, after)` then a
`_look_shot(name, what, azimuths, elevation, with_him)` in `ui_test_look.gd`.
The azimuths are tried in order; the first with room behind it (the native's
look rays) is used.
