# Work Order 009 — First Full Causal Chain

**Note on numbering:** this is the "Work Order 008 — first full causal chain" named in TDD §24's original first-implementation sequence. It is renumbered WO-009 for the same reason WO-008 (fall/parachute/checkpoint, itself originally TDD's "Work Order 004") was renumbered: WO-004 through WO-008 were already committed under other objectives before this slice was picked up. The content matches the TDD's intent for "the first full causal chain work order," not a new invention.

## Objective

Demonstrate, in one falsifiable sequence, the chain TDD §24 names as the architecture's proof point:

`player intervention → freight/load change → structural/process consequence → changed traversal/world capability → persistent checkpoint/reload`

and close the two links nothing had yet exercised: that a structural/process consequence changes what the player can *reach*, and that the newly reached position is what the checkpoint system *persists*.

## Existing proven truth

- WO-002 moving-support truth: support-point-velocity carry is real and entity-general. `entity_is_moving_support()` already lists `kLiftPlatformEntityId` alongside the kinematic supports — the lift platform was already wired in as a real moving support, just never ridden by any test.
- WO-003 athletic traversal: mantle/vault/hang are gated purely by a live geometry probe (`probe_ledge`) against whatever real Jolt geometry is actually there — not a hand-authored ledge whitelist. Anything with a climbable top surface within reach is a valid target, by construction.
- WO-006 first coupled machine: a player standing on the tipper opens the same valve that drives the whole plant (already-proven player intervention on this exact mechanism, at source commit `92f12f6`); the same valve/orifice/cylinder/piston chain lifts the platform, with peak lift height already asserted `> 6.0 m` within one 26 s cycle (`first_cycle.peak_lift_height`).
- WO-008 fall/parachute/checkpoint: checkpoint commit and restore are fully position- and entity-agnostic. `capture_body`/`restore_body`/`BodyCheckpoint` operate on whatever `JPH::BodyID` and transform they are given, with no per-location special case.
- An unused spawn stub, `InitialSpawn::LiftPlatform` (`{16.4, 2.0, -100.0}`, the platform's own centre), and a built-but-never-tested static catwalk (`kCatwalkEntityId`, galvanised mesh plus handrails already dressed in `main.gd` since WO-006/WO-007) were placed in the world but never exercised by any falsifier or reached by any scripted path.

## Governing authority

- TDD §24, "Work Order 008 — first full causal chain" (original numbering): "This is the first point at which ScraperX has proven its defining architecture. Do not scale content before this slice is real."
- TDD §8.4 — moving-support law: a constitutional discriminator, not optional polish.
- TDD §13.2 — traversal truth.
- Governing Law 24 — machinery cannot create capability from nothing: the capability must be a real consequence of real geometry changing, not a flag.
- Governing Law 26 — no physics theater / no second authority: the mantle gate must be the same unmodified `probe_ledge` every other mantle uses, not a bespoke check.
- GDD §9.1/9.2 — automatic checkpoint commit, capturing the state needed for correct continuation.

## Owner

`scraperx_sim` (`PhysicsWorld`) already owns every piece this WO touches — the moving-support rank table, the traversal probe, and the checkpoint commit. This WO adds no new owner and no new mechanism, only a falsifier that exercises their existing composition in a configuration nothing had reached before.

## Allowed seam

The existing, previously-unused `InitialSpawn::LiftPlatform` value; the existing `probe_ledge`/mantle traversal path; the existing automatic checkpoint commit. Godot: one new entity-ID constant (`CATWALK_ENTITY_ID`) and one HUD status-text branch, matching the existing `LIFT_PLATFORM_ENTITY_ID`/`TIPPER_ENTITY_ID` pattern exactly.

## Design decisions (stated, not left implicit)

- **Player intervention is cited, not re-derived.** WO-006 already proved a player's own weight on the tipper opens the same valve the autonomous cycle uses — one mechanism, two possible triggers, not two mechanisms. There is no pathfinding in this codebase to script a literal walk from the tipper to the platform mid-test without inventing test-only navigation machinery the claim does not need. The falsifier below uses the autonomous cycle to drive the identical valve/orifice/cylinder/piston code path; proving the piston/platform/catwalk links this way proves the same links for a player-triggered cycle, because it is the same code.
- **Restore-to-the-catwalk is not re-tested with a fresh death.** The catwalk sits at 8.55 m, deliberately under the lethal-impact threshold by the same design margin already documented for the 8.8 m platform fall (`kLethalImpactSpeedMps` comment, WO-008) — every direction off the catwalk is a survivable fall onto ordinary yard ground, by design. Manufacturing a lethal drop reachable only from the catwalk would mean inventing new geometry to serve a test, which is out of scope (Change Law: no unrequested new geography). What restore does with an arbitrary committed position was already exhaustively proven position- and entity-agnostic in WO-008; re-triggering it here would exercise the identical code path already covered, not a new risk. This WO proves the *commit* half updates to the newly reached position — the genuinely new claim.
- **No bespoke partition-invariance twin for this scenario.** Frame-partition invariance for `step_fixed`/`advance_frame` is already proven generically (the file's foundational test), specifically for traversal commands (`run_mantle_command_stream`), and specifically for checkpoint commit/restore (`fall_partitioned`/`fall_batched`). This WO composes exactly those already-partition-proven mechanisms in a new geometric arrangement; it introduces no new integrator, command type, or state-mutation path. Re-deriving partition invariance for this specific composition would test arithmetic already tested, not a new risk surface, so no duplicate fixed-duration pair was added.
- **Walking to the platform's edge uses a predicate, not a guessed duration.** A first attempt drove the player toward the catwalk edge for a fixed time and consistently overshot it — residual velocity at release (`kGroundAcceleration = 22 m/s²` decelerating from the 5.5 m/s cap) coasts roughly another 0.69 m before stopping, which is enough to walk clean off the platform at rest height. The falsifier instead walks to a real position threshold (`player_position.z <= -101.25`), releases input, and lets one settle second absorb the coast — empirically confirmed to settle at `z ≈ -101.92`, stable and grounded on the platform, for the remainder of the ride.

## Forbidden shortcuts

- No new "capability" flag or scripted gate — the capability change must be the same unmodified `probe_ledge` result every other mantle/vault uses.
- No teleportation of the player from the tipper to the platform to fake a monolithic walk.
- No new geometry manufactured solely to produce a lethal fall from the catwalk.
- No change to the existing WO-004..008 Godot CI capture walk, timing, or assertions — it proves a different, already-settled claim and must not regress.

## Proof path

1. Host tests prove the mantle gate to the catwalk is closed while the platform sits at rest, with the player positioned and facing exactly as it would need to be for the gate to open — the closure is a real geometric fact (rise far outside the mantle band), not an untested default.
2. Host tests prove the autonomous plant cycle — the same mechanism WO-006 proved a player triggers by weight alone — elevates the platform into real mantle range of the catwalk, and that a requested mantle at that moment completes grounded on the real catwalk entity, not a scripted teleport.
3. Host tests prove standing on the newly reached catwalk keeps the automatic checkpoint committing, and that the persisted checkpoint position tracks the catwalk, not the spawn the player started from.
4. Every WO-001..008 host assertion is unchanged.
5. The Godot runtime under Xvfb at Fold aspect reproduces every WO-004..008 proof line unchanged (the new entity-ID constant and status branch are additive and unreachable on the existing scripted approach path).

## Completion

Complete when all proof steps pass from one source commit and no existing assertion regresses.

## Result record

- **Changed:** `tests/simulation_tests.cpp` (one new falsifier, "first full causal chain"); `godot/presentation/main.gd` (`CATWALK_ENTITY_ID` constant, one HUD status-text branch — no native or bridge source touched).
- **Built:** host `scraperx_sim` + `scraperx_sim_tests`, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, clean, in both the host (`build/host`) and bridge (`build/bridge`) configurations. No native rebuild was required for the GDExtension `.so` itself — this WO changes no file it depends on.
- **Executed:** `./build/host/scraperx_sim_tests`; `ctest --test-dir build/bridge`; `tools/godot/godot --path godot --rendering-method gl_compatibility` under Xvfb with the standard `--ci --capture=` sequence, same recipe as every WO since WO-006.
- **Observed:** `PASS scraperx_sim first full causal chain: gate_closed_at_rest=1 lift_at_gate=7.38522m rise=1.16479m reached_catwalk=1 commits=1673`. With the player positioned and facing the catwalk from the platform's own edge, the mantle gate stayed closed for the platform's entire time at rest (verified continuously, not just at one instant); the autonomous cycle then lifted the platform to 7.39 m, at which point the catwalk became a real, geometrically valid mantle target (1.16 m rise, inside the same [0.35, 1.85] m band every other mantle in the game uses); the requested mantle committed and completed grounded on the real catwalk entity (`kCatwalkEntityId`), landing at `z ≈ -103.47`, on the catwalk deck itself; standing there kept the automatic checkpoint committing, and the persisted checkpoint position tracked the catwalk position within 0.05 m on both horizontal axes. Every WO-001..008 host assertion passed unchanged: `lift=8.80227m` (autonomous coupled-machine test, unaffected by the new spawn used elsewhere), `lethal_impact=34.6619`, `chuted_impact=8.8914`, `machine_restored=1`. The Godot runtime reproduced every WO-004..008 proof line unchanged: `viewport 2160x1856 aspect=1.164 stretch=expand`, `tower_height=1600`, `peak_valve=0.73 peak_lift=7.74 shut_flow=0.00000`.
- **Unverified boundary:** a live, on-device (or even interactive-desktop) demonstration of a player actually riding the platform and mantling onto the catwalk — this WO's proof is the deterministic host simulation, matching how the architecture-level claims in WO-002/WO-003/WO-008 were primarily proven; the Godot runtime check confirms only that nothing regressed, not a new interactive walkthrough of this specific path (doing so would need either a native default-spawn change plus a rebuilt `.so`, or new scripted CI input-choreography, and neither was judged to earn its cost/risk for what is a visual bonus on top of an already-complete proof). Android install/execution, Fold 6 panel observation, touch ergonomics, and sustained frame rate remain unverified as before (no device access in this environment) — unchanged from every prior WO, and this WO added no Android-relevant code (native/bridge untouched), so no new Android risk was introduced.
- **Regressions:** none observed. Every prior host-test assertion and Godot runtime proof line passed unchanged after this work order's changes.
