# Fight perf probes (J-044m, #245)

Part of J-044 (#242): console-quality fighting on the Galaxy Fold 6 at 90 Hz.
These probes only measure. No budget is asserted yet. At 90 Hz one fixed step
has 11.1 ms (11111 us) for everything; the numbers below show how much of that
the native step uses while she fights.

## What is measured

| Probe | Where | Unit |
|---|---|---|
| `step` | the whole world step (`PhysicsWorld::step`, timed in `Simulation::step_fixed`) | us per fixed step |
| `physics` | `PhysicsSystem::Update` inside it | us per fixed step |
| `matron_pre` | `Matron::pre_step` | us per fixed step |
| `matron_post` | `Matron::post_step` | us per fixed step |

Each probe keeps the last step's value, the max and the running mean since the
`Simulation` was made (the numbers carry over when she ends a run and the world
is rebuilt). The clock is `std::chrono::steady_clock`, which is monotonic.
Nothing in the sim reads these numbers. They are not in `Snapshot`, a save or
`world_bits`, so the determinism hash cannot see them.

- Native tests: `Simulation::step_timing()` returns `StepTiming` (`src/sim/simulation.hpp`).
- Godot: `get_step_timing()` on the native node returns
  `{step, physics, matron_pre, matron_post}`. Each is `{last_us, max_us, mean_us, samples}`.

## Host numbers (CI)

- **Native, `perf-step`** (`tests/sim_tests_perf.cpp`, `SCRAPERX_ONLY=perf-step`): the matron-damage staging on
  the 154 deck. She is woken 1.05 m in front of the player and set changed. He punches every 0.5 s for 6 s
  (540 steps), and the same fight runs twice. The test prints one `PERF perf-step <probe>` line per probe
  (n, min, mean, p95, max, plus the running mean and max), one line per second of the fight, and then
  `PASS scraperx_sim perf-step`. It asserts only that every span takes one sample per step and that the two
  runs end in identical `world_bits`. In CI it runs in the `matron-slices-3` shard, and that step greps the
  PERF lines out of CTest's `LastTest.log`.
- **Godot, `perf_fight`** (`godot/presentation/ui/ui_test_act.gd`, touch shard 2): the player on the 154 deck
  taps PUNCH every 30 frames at her up close for 10 s. Her hands are held so a grab cannot end the fight, she
  is changed, and she is put back 1.2 m off whenever she is carried past 3 m. Every process frame it logs
  `Performance.TIME_PROCESS`, `TIME_PHYSICS_PROCESS` (ms), `RENDER_TOTAL_DRAW_CALLS_IN_FRAME`, and the native
  probes' `last_us` from each frame that ran a new fixed step. It prints one
  `SCRAPERX_PERF perf_fight <series> n= min= mean= p95= max=` line per series and a `native_running` line, and
  puts the headline numbers on its `SCRAPERX_UITEST PASS perf_fight` line. On headless CI the draw-call count
  is 0, because nothing renders there.

Host CPU numbers are not phone numbers. They show where the time goes, and they
flag a change that makes the fight cost more than it did.

## Reading the same numbers on the Fold 6

What exists today, checked in the repo on 2026-11-05:

1. **SHOW FPS overlay** (pause menu, SHOW FPS; `main.gd` `_fps_label`). It shows
   `Engine.get_frames_per_second()`, updated every 0.5 s. That is frame rate only: it does not show the
   native probes or frame times.
2. **logcat.** Godot's `print()` goes to Android logcat under the tag `godot`. The playtest APK is a debug
   export (`--export-debug`, so it is debuggable), and its native library is built Release
   (`template_release`), the same code the probes time on the host. With the phone on USB debugging:
   `adb logcat -s godot | grep -E "SCRAPERX_PERF|SCRAPERX_UITEST"`.
   The `perf_fight` scenario ships in the APK (the export excludes only `tests/*` and `tools/*`), and
   `main.gd` starts a scenario from `--uitest=<name>` in `OS.get_cmdline_user_args()`. The repo has no
   adb launch tooling yet. Godot's Android launcher takes launch arguments through the
   `command_line_params` string-array intent extra (the one the editor's one-click deploy uses), so
   this should start it:
   `adb shell am start -n com.cory.scraperx.claude/com.godot.game.GodotApp --esa command_line_params "--,--uitest=perf_fight"`.
   **Nobody has tried that launch on the Fold 6 yet.** The first person to run it should write here
   whether the scenario started (a `SCRAPERX_UITEST PASS perf_fight` line in logcat) and, if not, what
   the launcher did with the arguments.
3. **Sentry** (`godot/presentation/error_reporter.gd`, J-031) sends script errors and `push_error()` only,
   capped at 30 events per session. It sends no warnings, breadcrumbs or metrics, so it is **not** a perf
   channel. Do not route perf numbers through it: that would spend the free plan's error quota.

Not built: an on-screen readout of `get_step_timing()` during real play. When J-044 needs one, it is a small
UI job: add the native step's mean and max to the SHOW FPS label.
