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
  is changed, and she is put back 1.2 m off whenever she is carried past 3 m. It waits 1.5 s after staging
  and then logs every process frame: the frame's own wall time (`frame_wall_ms`, from `Time.get_ticks_usec`),
  `Performance.TIME_PROCESS`, `TIME_PHYSICS_PROCESS` (ms), `RENDER_TOTAL_DRAW_CALLS_IN_FRAME`, and the native
  probes' `last_us` from each frame that ran a new fixed step. It prints one
  `SCRAPERX_PERF perf_fight <series> n= min= mean= p95= max=` line per series and a `native_running` line, and
  puts the headline numbers on its `SCRAPERX_UITEST PASS perf_fight` line. On headless CI the draw-call count
  is 0, because nothing renders there.

How to read the Godot series:
- `frame_wall_ms` is the true per-frame figure.
- `TIME_PROCESS` is refreshed by Godot once a second with that second's slowest frame, so it reads as a
  step function: its p95 sits at its max, and one slow frame shows for a whole second.
- `main.gd` advances the sim inside `_process`, so `TIME_PROCESS` includes the native step and
  `TIME_PHYSICS_PROCESS` stays near zero.
- The native `running` max includes setup. Her one-time tower scan (`matron_affordances`, seconds on the
  host) runs inside a step when she is first placed in a test. In real play it runs behind the LOADING
  card instead (`main.gd` `_prepare_her`). The window series (`n=` lines) leave setup out.

First host numbers (dev container, g++ 13.3, Release, 2026-11-05), fight window only:

| | mean | p95 | max |
|---|---|---|---|
| native `perf-step`: step / physics / matron_pre / matron_post (us) | 1319 / 797 / 82 / 22 | 1702 / 1026 / 137 / 35 | 5222 / 4867 / 310 / 86 |
| `perf_fight`: native step / physics (us) | 1511 / 838 | 1895 / 1032 | 17988 / 17320 |
| `perf_fight`: frame wall (ms, headless, nothing drawn) | 14.7 | 19.5 | 35.0 |

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

## Rendered probes (J-045, #248: phone stability)

These scenarios use a per-frame recorder in `ui_test_act.gd` (`_probe_*`). For every frame it records:

- the frame's wall time;
- the native fixed steps' time inside that frame (every step counted, from `get_step_timing`'s running totals);
- the viewport's measured render CPU and GPU time;
- draw calls, objects and primitives;
- video, texture and buffer memory;
- Godot's pipeline-compilation counters.

Per segment it prints p50/p95/p99/max, the frames over 50 ms and the most of each counter, then the worst
frames in full. They draw nothing headless, so they are not in a CI touch shard (`SKIP_EXACT` in
`check_touch_shard_coverage.py`). Run them under xvfb with the phone's renderer:

```
godot --headless --path godot --import        # once: without it the tower's surface textures are missing
xvfb-run -a -s "-screen 0 648x557x24" godot --path godot --rendering-method gl_compatibility \
  --fixed-fps 60 --resolution 648x557 -- --uitest=perf_view_bottom --capture=/tmp/shots/perf_view_bottom
```

| Scenario | Spawn | What |
|---|---|---|
| `perf_locker` | 11 (154 deck) | An eye check of the warm-up first. Then real play's `_prepare_her` (card, scan, warm-up), 1 s idle, a sprint that wakes her, her straining, and 6 s from her burst. |
| `perf_view_bottom` | 8 (yard) | The tower's foot, four quarters at pitch 1.35 (straight up) and 0.6. |
| `perf_view_sling` | 8 | Seated in the slingshot's pouch. The pouch's camera follows the launch rail, so every quarter reads the same. |
| `perf_view_top` | 42 (summit) | Four quarters at pitch -1.25 (straight down) and -0.6. |

On the host, software rendering (llvmpipe) makes every frame 0.4-4 s, so frame times there mean nothing.
Draw calls, objects, primitives and memory do carry over to the phone. Two things the host cannot show:

- Godot's `PIPELINE_COMPILATIONS_*` counters stay 0 under gl_compatibility, and `--verbose` names no
  per-material compiles. Whether a shader compiled is seen only as a slow first frame on the phone.
- Real frame times. Those come from the APK through logcat (above): the same scenarios print the same
  `SCRAPERX_PERF` lines there.

First host numbers (2026-11-05, textures imported, default settings: shadow quality 3):

| View | draws | objects | primitives | video / texture / buffer MB |
|---|---|---|---|---|
| 154 deck by her locker, idle | 2438 | 2539 | 960k | 132 / 81 / 51 |
| tower foot, facing it, pitch 0.6 | 2845 | 2845 | 933k | 130 / 80 / 51 |
| tower foot, straight up | 1017-1160 | 1017-1160 | 544-563k | 130 / 80 / 51 |
| slingshot pouch (any look) | **2890** | **3112** | **1094k** | 131 / 80 / 51 |
| summit, straight down | 1049-1097 | 1151-1177 | 646-692k | 132 / 81 / 51 |

The repo's own draw budget is 700 at LOW shadows (`PERF_DRAW_BUDGET`, ui_test_rides.gd). Every full-height
view is over it, by up to four times. Video memory is flat at about 130 MB in every view, so Godot's own
accounting shows no memory blow-up in the views Cory reports. The leading suspect for the force-close is
the GPU work per frame: about 3k draws and about 1M primitives, plus the shadow pass. The phone run has to
confirm it.

The locker freeze: native is not the stall. Her tower scan (3.0 s) already runs under the LOADING card,
and the native share of each frame stays at or under 10.6 ms from idle through the wake, her 16 s of
straining and 6 s after the burst. On the host the burst's frames are no slower than those around them
(wall p50 / p95 / max, ms: straining 730 / 1065 / 1726, burst 646 / 1221 / 1684; draws at most 976 and
945k primitives at the burst). Her body is
hidden until the burst, so the burst is the first time her materials (about 14 StandardMaterial3Ds:
skin, cloth, hair, eyes, teeth) are drawn, and gl_compatibility compiles a shader on its first draw.
`_prepare_her` now draws her for 3 frames under the card as a 0.002-scale speck at the screen's centre
(`matron_view.gd` `warm_up` / `warm_down`). The eye check found 0 of 360,936 pixels changed. Whether it
removes the phone's stall is proved only by `perf_locker` on the phone.
