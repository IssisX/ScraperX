# Idle mechanism native sleep — 2026-10-20

## Cause and correction

Kit explicitly disabled native sleeping for every body, keeping resting mechanisms in the collision/constraint solve. Actual control owners already activate commanded forklift, crane, lift, powered-cable and supplied-machine bodies/constraints. Native contact and grip/foot owners wake interacting supports.

Use Jolt's existing native sleep policy for Kit bodies. Player sleep remains disabled. No new force, damping, mass, power, solver-iteration, collision-substep, visual-quality or input-timing bounds are introduced. This permits ordinary native equilibrium approximation for resting objects; collisions, real input and supporting topology changes resume their finite physical solve.

Both authoritative cable disconnect owners now activate their enabled dynamic endpoints. Jolt constraint removal does not do that automatically. Without this repair, cutting a sleeping suspended load can leave it floating. The regression waits for actual native sleep, cuts each kind of line and requires both endpoint activation and gravity-driven falling.

## Actual Android native evidence

On the Android host, directly link the native library extracted from green-candidate e9c6a24 APK. The final candidate compiles only the edited Kit/RiggingSystem owners into the executable; remaining game/Jolt code comes from that actual APK. Public Simulation layout is unchanged. This is an Android native probe, not an installed new APK, Godot renderer measurement or whole-game FPS proof. Probe helpers provide pinned Jolt's omitted SingleThreaded test runner and inactive/null profiling TLS; neither replaces physics.

Identical stationary freight-arrival replay, 540 warmup + 180 measured ticks at 90 Hz, four collision substeps, original one-worker configuration:

| Native tick cost | Published APK baseline | Edited owners |
|---|---:|---:|
| Median | 6.746 ms | 2.252 ms |
| 95th percentile | 10.441 ms | 3.327 ms |
| Maximum | 18.078 ms | 4.215 ms |

Commands:

```sh
/data/data/com.termux/files/usr/tmp/scraperx-touch-playability-20261020/android-native/benchmark
/data/data/com.termux/files/usr/tmp/scraperx-touch-playability-20261020/android-native/benchmark-final
```

Both exit 0. Final output: `CARGO_CPU median_ms=2.252 p95_ms=3.327 max_ms=4.215` (diagnostic label retained from the earlier cargo investigation; this measures the whole native world advance).

Existing affected native checks actually executed on the same Android runtime with final owner overrides:

- `android-native/slingshot_machine-sleep`: exit 0, charged/manual release, recovery/reboard, fixed source/energy bounds and 90/180/360 physics-rate convergence retained.
- `android-native/warehouse_forklift-sleep`: exit 0, loaded lift/tilt, finite source, brakes/reverse, tool exchange, checkpoint and tipping retained. Observed loaded cargo height 1.05517 m; tip minimum-up -0.46156.
- `android-native/rigging_system-sleep`: exit 0, real counterweight 1308 N, original graph/lifecycle checks, and both naturally sleeping-line cuts wake and fall.

Exact compiler/link/run argv, output and source hashes are retained under `/data/data/com.termux/files/usr/tmp/scraperx-touch-playability-20261020/android-native/`: `final-check-commands.json`, `final-source.json`, `*-run-final.json`. The APK library came from producer-accepted candidate build214664317; `producer.json` identifies its path and entry.

## Rejected approaches and limits

More physics workers were slower on both measured hosts; zero workers also failed to improve median Android cost. Worker count stays unchanged. Opting the vertical cargo cloth into paired sleep did not meet existing equilibrium thresholds; that candidate and its fixture are preserved under `cargo-sleep/rejected-source`, excluded from production. No threshold was weakened.

Combined final-owner Godot touch audio acceptance also passed: exit 0, `SCRAPERX_SLINGSHOT_AUDIO PASS checks=145 mix_peak_db=-0.80 driver=Dummy touch_events=56`, no SCRIPT ERROR/Parse Error. This is the same existing real touch/MovieMaker path, repeated once because the sleep policy changed. Exact command/output: `final-native/touch-audio-command.json`, `touch-audio-result.json`, `touch-audio.log` under the directory above. Android full-game rendering, thermal behavior over a play session and device listening quality remain unverified until the matching APK is played. These native timings do not establish a shipped frame-rate improvement.

Publication of new source is currently host-blocked: `git push origin ChatGPT` was rejected before execution with `approval required by policy, but AskForApproval is set to Never`. Owner authorization is already clear. Do not claim the local commits are published, bypass the rejection, or deliver the previous green APK as containing this fix.
