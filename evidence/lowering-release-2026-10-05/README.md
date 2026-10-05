# Drop during lowering — bounded repair

Base: `d06b36543158e8a19a42d67b8cd68e64bb984911`, branch `ChatGPT`, remote
`https://github.com/IssisX/ScraperX.git`. This isolated checkout had no local
changes. Remote ChatGPT was still at that revision after verification.

The public `Simulation::request_release()` rejected Lowering; the native
command handler also rejected it, and Godot hid the Drop button. The repair
admits the request, calls the existing `release_hang` owner, and exposes DROP
through the existing touch context. That owner clears physical hands and
traversal state, retains actual body velocity, restores gravity and applies
the existing regrab lockout. Lowering forces, targets, geometry, masses and
Jolt ownership are unchanged. Gamepad Back uses the same release request.

## Executed evidence

- `native-red.log`: before production edits, immediate and delayed cargo
  receiver releases and airborne duct lowering all rejected Drop and retained
  state 5 / two hands. Adjacent existing departure checks passed.
- `touch-red.log`: unchanged production code failed four new assertions:
  hidden Drop, wrong caption, missing touch verb and failed detachment.
- `native-release-green.log`: all three release cases pass after the repair,
  state 0 / zero hands, gravity retained, six subsequent approach ticks without
  regrab. Airborne vertical velocity differs from pre-release velocity minus
  gravity by only `3.24249e-08 m/s`. Grounded cargo cases retain real contact
  and are not claimed as free-fall measurements.
- `touch-green.log`: shipping Godot 4.7 scene, 3,078 checks / zero failures,
  including two real touch hit tests through router, dispatch and native state.
  Explicit supported staging at `(22,12,-120.3)` isolates the receiver; it is
  not ordinary ground-to-receiver route proof.
- `native-suite.log`: 32/33 native CTest gates pass (70.60 seconds). This includes
  physical hands, physical traversal, climbing, hanging/shimmy/lowering ascent,
  landing, parkour, suspended ladder, full athletic traversal and retained
  machine routes. The cargo ground route fails its existing loaded-net grip
  deflection assertion.
- `cargo-baseline.log`: that same cargo assertion fails on unchanged `d06b365`
  using the same Release compiler flags, pinned Jolt library and cargo test
  object. The baseline simulation translation unit was compiled from a Git
  archive and substituted into a copy of the native archive; all other native
  sources and the cargo test are unchanged by this repair. No baseline or
  other branch was edited. This establishes a pre-existing local failure,
  not its root cause or expected CI result. The test was not weakened.

Build: GCC 14.2.0, CMake Release, pinned repository Jolt and godot-cpp;
`cmake --build /tmp/scraperx-build -j 6` succeeded. Godot 4.7 binary ZIP SHA-256
matched the workflow's `0b1a6c54c2c619c12e169fe9241edda4b81080b519451cec2984bf0d2c6cb73c`.
Checks: `ctest --test-dir /tmp/scraperx-build --output-on-failure -j 3` and
`Godot_v4.7-stable_linux.x86_64 --headless --path godot --fixed-fps 90 --script
res://tests/settings_runtime_test.gd`. A transient misindented GDScript branch
was caught by parsing and read-only review, corrected, and the shipping test
then exited 0. Diff whitespace checks pass; review found no further introduced
defect. No screenshot or device-performance claim is made.

## Delivery boundary and continuation

Local repair only: no push, new Actions run or APK. The configured `GH_TOKEN`
is invalid, and the read-only Actions request to
`https://api.github.com/repos/IssisX/ScraperX/actions/runs` returned `Forbidden`.
No alternate access route was attempted. Consequently the required current
candidate check and exact-source Actions/APK verification cannot be completed.
The existing local cargo gate also remains red; report it before widening
scope. The previous cargo APK at `21aca60` is not a build of this repair.

The saved façade checkpoint remains paused and unfinished; no façade or
swing-stair implementation was changed. No Fold execution, feel, ergonomics
or performance has been tested. Required Godot skill files were not present
in the catalog or the inspected local skill locations; source and the pinned
repository workflow supplied the actual runtime procedure.

Resume the other Codex session by checking `git status` first and preserving
any uncommitted work. After this fix is published, fetch ChatGPT and incorporate
it only by fast-forward when the checkout is clean; never reset local work or
force-push. Restore Actions access and inspect outstanding candidates before
publication, then verify the exact-source run and APK if green.
