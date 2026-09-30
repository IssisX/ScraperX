**Ascent Slice:** `AS-021`
**Lifecycle:** IN PROGRESS
**Provenance:** re-derived here
**Implementation gate:** the fixed assembly has a closed support path, source-derived capsule/jump/clearance bounds, and native tests for its intended traversal and misses; native route and recovery tests pass; rendered route and delivery remain open
**Evidence:** current delivery remains in `00_START_HERE.md` §2; local AS-021 observations and handoff are recorded below

# AS-021 east braced bay

**Objective / player problem.** Continue the +77 m teeter exit to the +88 m ring through an exposed structural bay. Read two diagonal girders, balance up the first, jump a broken transfer landing, crouch under a low crossmember on the second and mantle onto the tower. This is a parkour section: placement, momentum and body clearance are the decisions. It adds no automatic ride or repeated cabinet–duct–vent sequence. A missed transfer loses elevation onto a real maintenance deck; deliberate side exits remain possible.

**Existing truth / authority.** `dfcfa53` has exact-source desktop/Android build proof through supported +77 m. The next ring is +88 m. Atlas §10 owns placement; Laws §§4–7, 15, 17, 22 and 25, GDD §§7, 8 and 16, TDD §§8, 14 and 16 govern movement, falls, ownership and evidence. Preserve all earlier identities and the active/retired split.

**Owner / allowed seam.** One new static Kit compound, entity 1900, appended after AS-020 in the active `WorldContent::PipeBridge` builder. Native Jolt owns contact; native traversal owns input, balance, crouch, jump, mantle and checkpoint state. Godot draws the same Kit parts. No second Godot collider, route flag, moving body, controller retune or custom gravity is required by this design. `InitialSpawn::BracedBayEntry` is a test entry on existing tower support, not the normal campaign spawn.

**Compiler interface.** Profile `MACRO-TRAVERSAL-STRICT` [DEFAULT]; SI/Y-up [MEASURED project]. CMC LINK checks rider continuity from AS-020's fixed +77 m catwalk to the new base and the +88 m receiver. All new geometry is fixed structure: it reacts to contact and performs zero work. The 85 kg player [MEASURED source] supplies locomotor work through the retained controller. There is no machine coordinate, stored-energy release, capture/reset or applicable `stage1dof.py` mechanism sweep. ThreeSpine's contact, geometry and locomotion calculations below screen the route; native traversal is the decisive model. Static structural strength is an existing unmodelled abstraction, not an inferred load-capacity result.

**Closure calculations.** Each inclined top rises 5 m over an 11 m horizontal run [CHOSEN], hence length sqrt(146)=12.083 m, slope atan(5/11)=24.444 degrees and upward normal Y=11/sqrt(146)=0.91037 [DERIVED]. This is above the current beam classifier's 0.9 normal threshold [MEASURED source], with no retune. Nominal beam width 0.46 m is below its 0.50 m maximum [CHOSEN / MEASURED source]. The ascending player's potential gain over the complete 11 m slice is 85×9.81×11=9,172.35 J [DERIVED]; the stationary frame is not an energy source.

The 2.2 m transfer gap [CHOSEN] has broad 2.5 m longitudinal landings. With 5.5 m/s vertical takeoff and g=9.81 m/s² [MEASURED source], the ideal same-height arc lasts 1.1213 s and rises 1.5418 m; at 5.5 m/s horizontal speed it spans 6.1672 m [DERIVED screening, excluding finite control acceleration, collision and braking]. Native runs passed launch requests at X=29.8/30.0/30.3 with lanes Z=-144.2/-144/-143.8; actual discrete-tick takeoffs were X=29.8528/30.001/30.3605 [OBSERVED RUNTIME]. These three cases bound the evidence; they do not prove every point in a continuous corridor. Portal underside +85.75 m over the high edge of the ascending top at Z=-137.8 leaves 1.3864 m [DERIVED]. Standing capsule height 1.8 m fails; crouched 1.2 m plus 0.05 m stand-clearance skin fits [MEASURED source / DERIVED]. Legal mantles or jumps around the member are not prohibited. A 10 m drop to the maintenance deck reaches 14.007 m/s in free fall [DERIVED], below the existing lethal-impact bound; actual contact and recovery remain runtime obligations.

**Rejected candidates.** Another cabinet–duct–vent climb repeats AS-017/019. Another bare pole repeats AS-020's long exit. A new lift after the teeter repeats the recent ride pattern without a distinct physical decision. A continuous broad diagonal walkway removes the intended balance and gap decisions.

**Proof / completion.** Native tests must reach real +82 m, +87 m and tower +88 m supports, observe balance on the inclined girders, distinguish walking off the gap from a successful jump, distinguish standing obstruction from crouched passage, preserve deliberate departure and verify recovery/retry. Test more than one takeoff location on the same geometry. The full gameplay proof must extend the existing grade→+77 m rendered touch sequence with ordinary input, explicitly capture approach/transfer/low member/arrival, inspect those images, preserve the retained routes and run the GitHub Actions APK path. Do not promote native-only proof to completed delivery.

## Local evidence and model handoff

The initial route test failed before the new geometry supplied a +82 m receiver. Implemented geometry then exposed two concrete placement problems: an overlong landing made a wall above the ascending capsule’s step limit, and an outer floor stringer intercepted the old teeter grip top-out. Moving the landing edge up-slope and splitting the stringer closed those contact failures. The earlier teeter route passes with the new bay present. Review also identified a 0.09 m gap between the inner return post and its beam; the post now sits directly beneath the beam.

A flat approach and settling at `(28,-131)` are consequential: entering the narrow slope sideways with residual lateral velocity can cause a real fall. The route does not cancel velocity to hide that failure.

| Native observation | Obtained result |
|---|---|
| First rest | Capsule `(28,82.9,-144.198)`, grounded on 1900; real balance during ascent |
| Nominal transfer | Takeoff `(30.001,82.9,-143.974)` at 5.5 m/s; arrival `(33.6285,82.895,-143.999)` grounded |
| Standing / crouch | Standing stops at Z=-138.711; crouched capsule passes to Z=-136.797 on the inclined support |
| Upper junction / ring | +87 m junction reached; mantle then capsule `(24.7807,88.9,-132)`, support 11, zero deaths |
| Missed jump | Walking across gap does not invent support; hang/release falls to capsule Y=77.88; route walks around posts back to ring |
| Deliberate side exit | Full sideways input leaves balance and falls to Y=77.8951 on maintenance deck, zero deaths |
| Launch variation | Two additional launch/lane combinations above pass |
| Fatal fall / retry | After restore and 20 seconds with no input: capsule `(25.8549,88.9,-127.783)`, velocity zero, support 11, death count remains 1 |

Values are observations of the native run before the minor post alignment correction; the final-source rerun is recorded in the handoff. They are not screenshot measurements or Android observations.

### Checkpoint owner repair

The extended retry test first failed: a centre ray accepted lower decking while the capsule perched on a higher rounded edge. After restore it drifted and died again without input (death count 2 within 20 seconds). `Simulation::footing_is_firm` now requires the actual resting depth of an upright capsule on the observed support plane and a small cross of nearby support points.

For capsule radius `r`, half-height `H` and upward unit plane normal `n`, vertical distance from capsule centre to plane is `H-r+r/n.y` [DERIVED from sphere/plane tangency]. Acceptance uses twice Jolt’s declared penetration slop for depth and coplanarity. Four rays at ±0.10 m in X/Z [CHOSEN, fits existing 0.30 m beams] must hit walkable points on the same local plane. The held object is excluded as before. This changes checkpoint eligibility, not locomotion, gravity, restore velocity or contact forces. It is a conservative firm-footing filter, not a proof of stability on every curved or moving support.

### Verification boundary

Local ARM native CTest selection excluding `scraperx_sim.athletic_traversal` passed 20/20. The excluded broad executable was also run: early foundation, locomotion, support and checkpoint checks passed before the known retired AS-002 mid-landing failure. That failure was separately reproduced on the preceding baseline; do not delete or weaken its CI gate. A fresh full x86 Actions run remains required. Independent review found no critical/important defect and reproduced the route and retry pass.

Godot currently adds only an explicitly requested `braced_bay_preview` pose to the existing +77 m touch scenario. It does not claim rendered +88 m traversal. Full normal-input grade→+88 m, first-person inspection, current-source Android build and Fold execution remain distinct outstanding gates.

### Exact input handoff

Port the operations in `tests/braced_bay_tests.cpp` through the existing viewport input helpers; never call native setters from the Godot route proof.

1. Call `_touch_teeter()` to earn +77 m from grade. Move onto the flat approach `(28,-131)` and settle, then face/walk toward `(28,-144)`.
2. On the +82 m landing move to `(29,-144)`, settle, face +X and accelerate. At X>=30 while still grounded, press the touch jump control. Steer toward `(33.7,-144)` using `clamp_length((target-position)/1.4,1)` in world X/Z converted to view right/forward; wait for an observed airborne interval then grounded arrival near Y=82.9. Stop input and settle. Use state observations for success; a timeout only diagnoses failure.
3. Align `(34,-144)` while on the flat landing. Walk to `(34,-139.4)`; show standing obstruction, toggle crouch through the actual touch button, move to `(34,-136.7)`, then stand after clear.
4. Continue `(34,-131.5)`, settle, align `(34,-132)`, then balance to `(26.55,-132)`. Face -X, require the real mantle affordance, press Action and walk onto `(24.8,-132)`.
5. Require grounded support 11, centre Y=88.9 within the native test’s tolerance and zero deaths. Capture approach, first landing/gap, crouch passage, upper junction and supported arrival, with camera directions that expose the relevant geometry. Inspect each actual image.

The larger coding assignment and running to-do are in [the Sol package](../PLANNING/SOL_ASCENT_HANDOFF.md). Completing this touch port alone does not finish that assignment.
