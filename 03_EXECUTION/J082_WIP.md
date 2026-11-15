# J-082 build, work in progress (stopped 11-15 to save quota)

Cory's pick 11-15 06:12 Central: designs 4, 6, 2, built in that order (doc: `J082_CLIMB_STAGES.md`).
Nothing landed and no PR. This branch (`claude/wip-j082`) is a save point only.

## Done
- Design 4 geometry drafted: `build_swing_launch` in `src/sim/band_facade_crane.cpp`, appended last to the
  route vector so no existing part moves. It sits in the 484-506 well's south half along z -154.5, west to east:
  a pier and pole on the 484 west band, a lane at 494.0 (x -9.2 to -2.0), one bar on rods from a girder under the
  506 ring, a block (face x 4.0, a 6 m gap) reaching onto the east band, a second pole beside a fascia girder on
  the 506 east band at x 7.10, and a grating at 484.25 under the gap joined to the south band. Sited clear of the
  canonical walk (north band, NW corner), J, I's ladder top (-8.21, -148.1) and `run_crane_route` (east band, x 10.3).
- **TEMP probe code, which must be removed before any real commit:** every line marked `//TEMPPROBE` in
  band_facade_crane.cpp (env-driven geometry and the `J082_BASELINE` switch), and all of
  `tests/sim_tests_zz_j082_probe.cpp` (never to be pushed in a PR).

## Measured (host g++ 13.3, CI's CMake flags, Release)
- Her map, trunk vs this geometry: her plan 484 to 506 is the same ladder (rung_up at 7.3, -150) and 616 to 640
  is the same hatch ladder (4.9, -150.4). The stage adds 7 patches, 1 bar, 2 poles, 2 risers, 2 regions and 2 edges
  (trunk: 1671 patches, 35 ladders, 674 regions, 608 usable edges).
- **The doc's sizing fails.** At a 6 m gap with the lip 1.5 m up, a plain sprint jump lands on the block and a
  let-go also gets there (air control: 14 m/s2 up to 5.5 m/s). Jump 5.5 m/s gives a 1.54 m apex.
- Sweep (walk-speed run, jump at x -2.8, bar at -1.0 and 3.1 m over the lane, face at 4.0, 1-tick steps from the
  first front-of-swing tick):
  - Lip 3.5-3.7 m: a direct sprint jump hangs the lip. Not allowed.
  - Lip 3.8-3.9 m: no direct jump and no let-go at any delay. The throw lands at delays 9-17 (9 ticks, 0.1 s).
  - Lip 4.0-4.1 m: throw at 10-17. Lip 4.2 m: throw at 12-16.
  - Bar at -0.5 (jump at -2.4), lip 3.9: throw at 5-14, no let-go, no direct jump. This is the best window
    found, still 10 ticks.
  - Bar at -1.5: throw at 10-17. Bar 2.9 m up: nothing lands.
  - Bar 3.3 m up: let-go also lands (9-22). Looks wrong, re-check before using it.
  - With the bar 3 m out (x +1.0) the catch comes late and nothing lands.
  - A sprint jump at the bar flies past without catching it. Only a walk-speed run catches it.
- Window's late edge is code (the launch needs >0.5 m/s forward, `kSwingLaunchMinMps`), not geometry.

## Open questions found
- Design 4's throw window is about 0.1 s on the first swing. Untested: whether later pumped swings give
  retries. Untested: whether a longer sprint (flow, up to 9 m/s) makes the direct jump at a 3.9 m lip.
  If 0.1 s is too tight for a phone, that is Cory's call. A wider window needs code (the launch's rule), not geometry.
- Design 6 likely does not fit as written. Why, from the code (no test run): the swing's lip exit
  (`update_swing`) takes a lip about 0.3-0.4 m above the rail, inside 1.0 m of the shoulders. TP-640 is 1.5 m thick
  (638.75-640.25), so no rail under it reaches the 640.25 lip. Run J-043's `hang_rail` once to see whether its
  lip exit ever fires. Then decide whether added steel alone closes it; if not, stop 6 and report.
- Design 2 needs a second `Trapeze` (today one instance with static constants). Check the save layout and the
  `TrapezeSnapshot` and bridge shape before refactoring.

## Exact next step
1. Fix design 4's constants at bar -0.5, jump -2.4, face 4.0, lip 3.9. Drop every `//TEMPPROBE` line and the probe file.
2. Write the stage test in `tests/sim_tests_crane.cpp`, as `run_springboard_stage` does, from Ring484North on
   real inputs:
   - the walk to the pier and pole 1, then a direct sprint jump that misses (lands on the grating, alive);
   - a let-go at the front of the swing that misses;
   - a Jump at the front of the swing that takes the lip; the pull up; pole 2 to the 506 ring;
   - her plan 484 to 506 still the ladder.
   Register it with GROUP, SELECT and SLICE as J-105 does (pick a free order key). Shard 3 picks up a new slice on its own.
3. Run that slice, `crane_route` and the J-105 group once, then `rule_check`. Push to
   `claude/integrate-protected-j082` and open a draft PR starting `Job: J-082`. Then go on to 6 (fit check), then 2.
- Local build: `cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DSCRAPERX_BUILD_BRIDGE=ON
  -DSCRAPERX_WITH_JOLT=ON -DGODOTCPP_TARGET=template_release`, plus `FETCHCONTENT_SOURCE_DIR_*` pointing at copies
  of `/home/user/ScraperX/build/host/_deps/*-src`. A first build takes about 25 min; an edit takes about 10 s.
