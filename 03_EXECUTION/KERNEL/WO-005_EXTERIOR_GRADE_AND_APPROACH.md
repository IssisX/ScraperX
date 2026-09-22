# Work Order 005 — Exterior Grade and Approach

## Objective

Put the player outside, at grade, a measured walk from the tower:

`spawn on the yard → walk the approach → the lower third of the tower fills the frame → the crown is sheared off by haze and plume`

## Existing proven truth

Before this work order the native world was a 32 x 32 m interior test deck and the default spawn placed the player in the hollow middle of it. There was no tower body, no ground beyond the deck, and no atmosphere.

## Governing authority

- Governing Law 2 — one skyscraper, one persistent world, approximately 1.6 km tall.
- Governing Law 6 — height and falling are real gameplay; height retains emotional weight.
- GDD §3 — exterior traversal is a major share of ordinary play, not a rare set piece.
- GDD §7.1 — the perspective exists to maximise height and machine scale.
- TDD §6.2 — ordinary 32-bit positions are sufficient at this scale; no floating origin.

## Owner

`scraperx_sim` owns the grade body, the tower mass and the spawn position. Godot owns the tower's non-collidable skin relief, atmosphere and sky shear.

## Allowed seam

The existing native world construction, the `InitialSpawn` enumeration, and the Godot presentation mirror.

## Forbidden shortcuts

- Spawning inside the building.
- A skybox billboard standing in for the tower.
- An artificial wall or invisible collider stopping the approach.
- Hiding the crown by shortening the tower rather than by occluding it.

## Proof path

1. Native source shows a 480 x 480 m grade body and a 120 x 90 m footprint tower mass 1600 m tall, both real static Jolt bodies.
2. The default `InitialSpawn` is `ExteriorGrade` at `(6.0, 1.2, -25.0)`, outdoors on the yard, 120 m from the tower's approach face at `z = -145`.
3. A host test asserts the default spawn is outdoors and below 2 m of elevation.
4. The Godot runtime walks the approach under its own locomotion and prints its position and distance to the tower face.
5. The captured frame shows the tower's lower third as a wall of structure with its top cut by haze and plume inside the frame.

## Completion

Complete when all proof steps pass from one source commit.

## Result record

- **Changed:** `src/sim/simulation.{hpp,cpp}` (grade enlarged to 480 x 480 m, tower mass added as entity 11, exterior spawns added and made default), `godot/presentation/main.gd` (tower face relief, lit floor bands, yard dressing, sky shear planes, stack plumes, tower-foot floods), `godot/main.tscn` (height fog and aerial perspective tuned so the crown shears).
- **Built:** host `scraperx_sim` + tests and the Linux GDExtension, Release, `-Werror`.
- **Executed:** host suite and the Godot runtime under Xvfb.
- **Observed:** `SCRAPERX_WO005_APPROACH_PROOF spawn_grade=1 position=(-3.47,0.89,-66.97) tower_face_distance=78.0 tower_height=1600`, with the captured frame showing the tower rising out of frame into sodium haze.
- **Unverified boundary:** interior routes into the tower, streaming, region sleep, and anything above the first few metres of the facade. The tower is currently one static mass with a non-collidable skin; it is not yet a traversable building.
- **Regressions:** none. Every WO-001..003 assertion still passes against the enlarged world.
