# Shared survival — native adoption handoff

**Scope:** opt-in native implementation and executable Jolt encounter fixture, based on remote `ScraperX-Claude` at `5f34d3dcefdbe2dd3eff3fa2dcac4fc34848e879`. Work is isolated on `dot/matron-shared-survival`. This is not a finished campaign encounter or device proof. The owner's current side-branch instruction overrides the older repository instruction to write/merge the implementation branch.

## What is delivered

- `src/sim/shared_survival.hpp/.cpp`: finite compliant hand contact, real foot-contact validation, measured load/effort, equal-and-opposite forces at the same point, and independent district drive channels using existing Jolt hinges. No position, velocity, mass or gravity override; no second physics integrator.
- `src/sim/matron.hpp/.cpp`: optional attachment; actual Matron yaw determines the hand frame. Loaded hands suspend pursuit/catching without changing her hostile state. Hearing still feeds rage. Lost contact, ineligible movement/stagger/down/death, or a prop taking the load releases the interaction; ordinary AI resumes. Planted-foot friction is finite and restored on release, detach/reset/destruction without overwriting a newer knockdown setting.
- `tests/shared_survival_tests.cpp`: executable real-physics loop plus adversarial falsifiers. The same flap is motor-driven, isolated, caught by hand support, propped with a loose colliding body, released and restarted. A separate test uses the actual Matron, requires sustained support, then transfers the load to a prop and verifies hostile contact returns.
- `CMakeLists.txt`: builds/registers the fixture. `scraperx_sim_tests` depends on it so the existing CI's explicit target list cannot omit the new CTest executable.

## Physical contract

A 100 kg, 2 m service flap turns on a fixed hinge; the actor is 220 kg. At the fixture's geometry, its weight needs about **545 N** of hand effort; the stance budget limits support to about **926 N**, below the 1,600 N grip ceiling. The hinge/frame takes the remaining reaction. This is a small route member, not a building held by a person.

The palm is an authored, compliant contact patch, queried only within ±4 cm of the actual actor-relative hand location. The spring/damper has finite force and travel. Both feet contact and a suitable underside normal are required; sensors, invalid/stale IDs and missing bodies cannot provide support. Capacity is the minimum of grip, stance moment and friction-limited traction. A force beyond capacity remains a real unbalanced load: the flap moves out of the hand patch and falls. There is no exhaustion countdown. Equal reaction enters the actor and its real contact solver, including on inclined surfaces.

This models a bounded human actuator on the existing upright capsule; it is **not articulated hands/feet or a validated biomechanics model**. The stance radius is an authored conservative support envelope. Tested at 60 and 120 Hz; unbounded timesteps and arbitrary geometry are not validated.

District isolation turns motors off, preserving momentum and gravity. A zero off-brake coasts; finite hinge friction brakes dissipate motion and can be overcome by load. Restoring supply does not start anything. Each fresh start needs a live enabled drive/guard constraint, a closed guard, near-zero angular speed and unoccupied hands. Losing interlocks trips the channel. Bracing on one member trips this bounded district's running channels. Selective restarts act on actual bodies, so geometry and stored motion change.

## Intended player loop and remaining world work

The player isolates the small service district to pass its moving route. A flap/counterbalanced member loses drive support. The Matron, already physically beside its reachable support face, plants her feet and braces it because its loss would deny or threaten her own route too. The player works within her reach to carry a steel prop into compression. If the prop takes the load, both retain a usable route and her hands become dangerous again. Removing footing, adding too much load or failing to place the prop permits collapse. Restarting selected machinery changes the escape geometry again.

**The fixture proves mechanics, not that narrative spatial dependency:** its test floor is safe. Campaign placement must make the player and Matron physically depend on the supported route, without placing the entire load on her or adding a survival flag. No approach-to-brace navigation, shared-risk campaign placement, player delivery prompts, animation or playable start was added. Ordinary gameplay is unchanged until attachment. Existing cranes, hook/rigging, tipping encounters and parked chiller/glass work were not modified.

## Exact integration ownership

1. **`src/sim/physics_world.hpp` / `physics_world.cpp`** own the optional controller and scene bodies. Construct the member/guard/drive with existing kit/Jolt authority, after its structural frame exists. Borrow body IDs and hinge refs; do not create duplicate collision bodies in Godot. Attach using `matron_->set_shared_survival(encounter.get())`. The Matron calls its `pre_step` exactly once before the existing Jolt update; do not also call it from the world. Queue reach-validated panel actions through `PhysicsWorld::StepCommands`, before that tick's Matron update. Supply starts isolated.
2. **`src/sim/mechanism_kit.hpp/.cpp` and the selected band builder** retain body/carry/constraint ownership. Reuse `Kit::set_carry` and existing loose-stock handling for the prop. There is no `brace_installed` flag: contact carries the load. Kit lever hinges currently are private; expose a narrowly owned district-drive seam if adopting them, rather than giving two controllers authority over one motor. Do not register cranes or existing driven constraints that another owner already updates.
3. **`src/sim/matron.cpp`** owns eligibility and locomotion. The current hook does not make her navigate to an arbitrary authored brace point; add an approach action only using existing real movement and then let contact admit support. The configured offset is in her facing frame. Render the hand at the measured `snapshot().hand`, not an unrelated pose. Stagger/down/climbing/carrying cannot brace. Bind the optional controller only to an encounter where bracing is physically self-interested.
4. **`src/sim/physics_world_state.cpp`** must persist scene-specific consequential power/brake configuration alongside its existing body/constraint state before campaign enablement. `Matron::restore` currently calls fail-safe `reset()` (isolates supply and releases transient effort). That is a safe unintegrated default, not a complete persistence policy.
5. **`src/sim/simulation.hpp/.cpp`, `src/bridge/scraperx_simulation.cpp`, `godot/presentation/main.gd`** must expose/present `Matron::View::bracing_structure`, `support_n` and encounter hand/drive measurements, then add reachable panel prompts and contact-aligned hands. These bridge and presentation fields are not wired here. Existing Kit body presentation already owns route geometry.
6. **Acoustics:** `DriveView::motion_fraction` remains nonzero while coasting. Use measured motion and the existing machinery-noise path in `physics_world.cpp`/`Matron::hear`; choose calibrated source levels and test masking in her hearing owner. **District sound emission and acoustic cover are not implemented here**; power must not instantly silence a coasting rotor. Her ordinary hearing/rage remains active during bracing.
7. **Lifetime:** call `set_shared_survival(nullptr)` before destroying the controller; it must die before `PhysicsSystem`. It retains hinge refs, but never owns bodies or physics-system membership. Jolt still requires removing constraints before destroying their bodies. Removed constraints/live bodies and destroyed/reused body IDs fail closed. API is single-threaded, between physics updates; supplied facing must be a finite unit quaternion. Bad channel indices throw `std::out_of_range`; invalid configs throw `std::invalid_argument`.

## Verification and explicit falsifiers

Configure/build (pinned Jolt, GCC Release, warnings as errors):

```sh
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release -DSCRAPERX_BUILD_BRIDGE=OFF -DSCRAPERX_WITH_JOLT=ON
cmake --build build/host --parallel 4
ctest --test-dir build/host -R shared_survival --output-on-failure
./build/host/scraperx_shared_survival_tests
SCRAPERX_ONLY=matron ./build/host/scraperx_sim_tests
SCRAPERX_ONLY=crane ./build/host/scraperx_sim_tests
```

The supplied environment lacked CMake/Ninja, installed under `/tmp/scraperx-build-tools`; its build directory is `/workspace/build-survival`. The code uses the repository's pinned dependencies unchanged.

| Behavior | Falsifier exercised |
|---|---|
| Finite support | Applied effort exceeds capacity; 5 kN additional load stays held |
| Valid contact | Missing palm contact, missing footing, sensor, invalid/destroyed/reused ID produces support |
| Conservation | Net force or world-space torque appears from inclined hand contact |
| Physical prop | Flap falls after the loose prop takes load and hands leave |
| Power loss | Isolated unbraked rotor loses speed instantly or stops changing angle |
| Finite brake | Passive brake increases angular speed/energy on unloaded rotor |
| Selective restart | Supply restoration starts a channel, moving/open-guard/occupied-hand/removed drive starts, or an unselected channel moves |
| Actual Matron | Fewer than 110 of 120 loaded ticks, catches while bracing, ignores audible rage input, fails hostile release, or loses propped route |
| Lifecycle | Interruption/reset retains planted friction; release overwrites newer knockdown friction; stale drive dereferences destroyed body |
| Reproducibility | Identical native inputs diverge; 60/120 Hz steady support differs by >1 cm or >5 N |

Independent review found and repaired sensor support, missing rage input, ignored yaw, unbalanced contact torque, zero-friction Matron footing, knockdown-friction restoration and removed-constraint availability. Final review found no further obvious important issue; this is not a guarantee of all runtime behavior.

Fresh results are recorded in `TEST_RESULTS.md`. The initial broad baseline CTest run was deliberately interrupted while its monolithic traversal suite was still running; it has no full-suite verdict. Final targeted runs are the evidence for this handoff. Godot bridge, game presentation, APK, install/device/Fold execution and performance were not verified. No CI run is claimed: the only workflow triggers on pushes to `ScraperX-Claude`, not this side branch.

## Adoption decision

Adopt/review this commit on the implementation agent's branch, then finish one small band placement with the world/save/presentation owners above and run a visible Godot encounter proof. Do not merge or present this as a shipped encounter merely because the native fixture passes. Her Chart and hypothetical forecasts are intentionally untouched.

Publication check: while this work ran, `ScraperX-Claude` advanced to `c454a7dc8f14b5e9d8794497f707cd2dcb65dc11` (night lighting/sign emission). Fetched and inspected: only `godot/presentation/main.gd`, `main_geometry.gd`, `main_world.gd`, `sky_cycle.gd`, `ui/ui_test_chase.gd`, and `ui/ui_test_driver.gd` changed. No overlap with this commit's changed files. This side branch retains its verified `5f34d3d` base; neither branch was merged/rebased or force-pushed. Preserve those presentation updates when wiring the encounter.
