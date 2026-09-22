# Work Order 008 — Fall, Parachute, Checkpoint

**Note on numbering:** this is the "Work Order 004 — fall / parachute / checkpoint" named in TDD §24's original first-implementation sequence. It is renumbered WO-008 because WO-004 through WO-007 were already committed under different objectives (Fold stage, exterior approach, coupled machine, Kellerworks identity) before this slice was picked up — a deliberate, previously-stated sequence delta. The content matches the TDD's intent for "the fall/parachute/checkpoint work order," not a new invention.

## Objective

Give the native player controller the three subsystems Governing Laws 6–10 and GDD §§8–9 require and that currently do not exist at all:

`airborne state → parachute deploy with real drag → lethal-impact detection at landing → automatic frequent checkpoint commit while grounded → full-state restore on death`

## Existing proven truth

At source commit `d7868f7` the player can fall and land, but nothing tracks fall severity, no parachute exists, and there is no checkpoint or death handling of any kind — an unsurvivable fall currently just leaves the player wherever they land (or clipped into geometry), with no defined recovery.

## Governing authority

- Governing Law 6 — height and falling are real gameplay; no invisible safety walls.
- Governing Law 7 — parachuting preserves consequence: uses actual position/velocity/clearance; may permit survival, redirection, controlled descent; **may not** guarantee survival, cancel arbitrary momentum, teleport, auto-restore the route, or provide powered ascent.
- Governing Law 8 — falling must sound human (fear vocalization). **Explicitly out of scope for this work order** — no audio asset pipeline exists in this environment. This WO exposes the seam (`fall_state`, `fall_peak_speed_mps`) a future audio layer needs; it does not fabricate audio.
- Governing Law 9 — checkpoints commit both recovery position and consequential world state; death/unrecoverable failure restores that committed state; post-checkpoint changes are discarded.
- Governing Law 10 — a survived fall's resulting position is real; only checkpoint restoration on actual death may move the player.
- Governing Law 18 — history is gameplay; persistent state includes machinery.
- Governing Law 21 — no hidden teleportation. Checkpoint restore is the one sanctioned, explicit exception this law itself names.
- GDD §8.1–8.2 — falling is allowed; a **survived** fall (impact below the lethal threshold) is never rolled back — the landing position is the new real state.
- GDD §8.3 — the parachute is an **always-carried, reusable** recovery skill, not a guaranteed save.
- GDD §9.1 — checkpoints are **frequent automatic commits**, not a player-triggered save button.
- GDD §9.2 — a commit captures the recovery location **and the consequential world state needed for correct continuation**.
- GDD §9.3 — hard-fail mission rollback is a separate, explicitly-designated exception. Out of scope: no missions exist yet.
- TDD §14.1 — a commit captures stable entity transforms/velocities and **machine/control state**, at an authoritative tick boundary.

## Owner

`scraperx_sim` owns fall state, parachute state, checkpoint capture/restore, and death detection. `SteamPlant` owns its own restorable mass state. Godot owns input (parachute toggle) and presentation (telemetry, and an explicit non-claim that fear audio is not implemented).

## Allowed seam

Extend `PhysicsWorld::step`, the immutable `Snapshot`, `SteamPlant`, the thin bridge, and the existing command-queue idiom (`request_jump`-style: queue a flag, resolve it authoritatively inside the tick).

## Design decisions (stated, not left implicit)

- **Checkpoint trigger:** commit automatically every tick the player is grounded and not mid-traversal. This is the simplest rule that is maximally "frequent" (GDD §9.1) without an arbitrary dwell-timer hyperparameter, and it means the checkpoint is always "wherever the player was last standing," which is exactly what "encourage physical experimentation" (GDD §9.1) requires.
- **What a commit captures:** player position; and, because the machine is the only other consequential dynamic state that currently exists, the five dynamic machine bodies (ballast, tipper, valve lever, lift platform, counterweight — position, rotation, linear and angular velocity) plus the steam plant's two integrated mass variables (vessel, cylinder). The kinematic bodies (translating/rotating supports, moving ledge, hoist scoop) are **not** captured: they are pure deterministic functions of the authoritative tick counter, never drift, and are therefore always correct without restoration. Cumulative telemetry counters (`vented_mass_kg`, `fed_mass_kg`, traversal accept/reject counts) are **not** rolled back — they are a permanent record, not consequential physical state that affects future continuation, matching GDD §9.2's "state needed for correct continuation."
- **Death trigger:** the instantaneous vertical impact speed at the tick a genuine (non-traversal) grounded transition occurs, compared against a fixed lethal threshold. Using the impact instant rather than a whole-fall peak is the physically correct measure — late deceleration (a well-timed parachute) is what should determine survival, and this makes the parachute's effect and the "deployed too late" failure mode a single, unified, falsifiable mechanism rather than two special cases.
- **Initial checkpoint:** seeded at construction to a known-safe point on the static deck for the player, and to each machine body's actual constructed resting configuration for the machine. This guarantees a defined, non-lethal outcome even for a spawn that starts the player airborne, before any real commit has ever happened.
- **Parachute physics:** quadratic drag, `a = -k·v·|v|`, opposing the full velocity vector (so existing air-control horizontal steering, already present in the airborne branch, doubles as the "redirection" Law 7 permits — no separate steering system needed). `k` is chosen so terminal descent speed is ≈9 m/s, comfortably under the lethal threshold and comfortably above zero (never a hover, never ascent).

## Forbidden shortcuts

- A checkpoint restore that is not full deterministic body/plant-mass restoration (no partial "just reset position" shortcut, per TDD §14.1).
- A parachute that clamps velocity to a fixed value instead of applying a real drag force.
- A parachute that can accelerate the player upward.
- Treating a mantle/vault/hang landing as a fall-death check (it never is: traversal completion always sets a support-relative landing velocity, not an accumulated fall velocity).
- Rolling back the kinematic bodies (they need no restoration and touching them would be pure waste).
- Any claim of implemented fear-response audio.

## Proof path

1. Host tests prove every WO-001..006 invariant is unchanged.
2. Host tests prove an unmitigated high fall (a new `InitialSpawn::HighDrop`, ~61 m above the static deck) kills the player: `death_count` increments exactly once, and the player is restored to the safe checkpoint position, not left at the fatal impact site.
3. Host tests prove the same fall, with the parachute deployed early enough, does **not** kill the player: `death_count` stays zero, impact speed is measurably reduced versus the unmitigated case.
4. Host tests prove the parachute is not a guaranteed save: deployed too late (insufficient altitude remaining), the player still dies.
5. Host tests prove ordinary platforming falls (off the lift platform, ~8.8 m) never trigger death — "survived falls remain real" (GDD §8.2).
6. Host tests prove automatic commit: walking to a new grounded position advances `checkpoint_commit_count` and moves the recorded checkpoint position; leaving the ground (a jump) freezes it at the last grounded position rather than tracking mid-air position.
7. Host tests prove a death restores **machine** state too, not just the player: the hoist/ballast cycle runs autonomously (WO-006) and measurably changes within seconds with zero player input; letting it run for part of a fall that never touches ground (so the only checkpoint in play is the one seeded at construction) and then dying must restore the ballast to its pristine seeded height, not the height the autonomous cycle had already carried it to — proof this is a real state rollback, not a scripted reset to spawn defaults.
8. Host tests prove frame-partition invariance for the whole subsystem.
9. The Godot runtime exposes the new telemetry without regressing any existing WO-001..007 runtime proof line.

## Completion

Complete when all proof steps pass from one source commit.

## Result record

- **Changed:** `src/sim/steam_plant.{hpp,cpp}` (`restore_state`); `src/sim/simulation.{hpp,cpp}` (`FallState`, `HighDrop`/`SurvivableDrop` spawns, parachute drag, lethal-impact detection, automatic checkpoint commit, full machine-state restore); `src/bridge/scraperx_simulation.{hpp,cpp}` (`request_parachute` and seven new telemetry accessors); `tests/simulation_tests.cpp` (seven new falsifiers); `godot/presentation/main.gd` + `godot/main.tscn` (FALL telemetry line, PARACHUTE touch button, `F` key binding).
- **Built:** host `scraperx_sim` + `scraperx_sim_tests` and the Linux GDExtension, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, clean on every intermediate revision (two real bugs were caught and fixed by the test suite itself before this build, not after).
- **Executed:** `./build/host/scraperx_sim_tests`; `ctest --test-dir build/bridge`; the Godot runtime under Xvfb at Fold aspect.
- **Observed:** `PASS scraperx_sim fall/parachute/checkpoint: lethal_impact=34.6619 chuted_impact=8.8914 late_chute_deaths=1 grounded_deploy_blocked=1 commits=52 machine_restored=1 deaths=1`. An unmitigated ~61 m fall reaches 34.66 m/s and is lethal; the same fall parachuted early lands at 8.89 m/s (between the ~9 m/s design terminal speed and the 20 m/s lethal threshold) and survives; the same parachute deployed too late still kills (`late_chute_deaths=1`) — not a guaranteed save; a deploy attempt while grounded produces no state change; the checkpoint advanced 52 times while walking/standing grounded and froze the instant the player jumped; and a death correctly restored the ballast to its pristine pre-cycle height rather than wherever the autonomous hoist cycle had carried it by the moment of death. Every WO-001..006 host and Godot-runtime proof line is unchanged: `viewport 2160x1856 aspect=1.164`, `tower_height=1600`, `peak_valve=0.73 peak_lift=7.49 shut_flow=0.00000`.
- **Unverified boundary:** fear-vocalization audio (Law 8, explicitly out of scope — no asset pipeline exists in this environment; `fall_state`/`fall_peak_speed_mps` are the exposed seam for it); Android install/execution, Fold 6 panel observation, touch ergonomics, sustained frame rate/thermals — CI run [35288090672](https://github.com/IssisX/ScraperX/actions/runs/35288090672) on this exact commit (3cb4171) confirmed green at job-step granularity, not just the aggregate conclusion: "Build native host graph and run the full native suite" (`conclusion: success`), "Run Godot at Fold aspect through native machine authority and capture proof" (`conclusion: success`, WO-001..007 proof lines intact), "Cross-compile native graph for Android arm64" (`conclusion: success`), and "Export checkpoint Android arm64 APK" (`conclusion: success`) all passed, so — unlike the presentation-only WO-007 case — the real risk this WO introduced (native + bridge changes reaching the Android cross-compile and export) is now confirmed rather than merely predicted; still unverified is anything requiring physical hardware (install, execution, on-panel observation, touch ergonomics, sustained frame rate/thermals — no device access in this environment); hard-fail mission rollback (GDD §9.3, no missions exist); persistence to disk (TDD §14 serialization format, not selected yet — this WO is in-memory checkpoint only, one authoritative tick to the next, not save/load across sessions or app restarts).
- **Regressions:** none observed. Every prior host-test assertion and Godot runtime proof line passed unchanged after this work order's changes.
