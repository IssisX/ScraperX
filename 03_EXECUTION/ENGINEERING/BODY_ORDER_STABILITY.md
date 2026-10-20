# Body-order stability

**Problem (the owner, 2026-10-20):** adding, removing or splitting one physics body anywhere in the
world changed simulations far away from it, until tests failed. **Root cause:** Jolt sorts an
island's contact constraints, before its solver runs them, by a byte hash of the two bodies' ids.
A body created anywhere shifts the id of every body created after it, the hashes change, the
contacts of islands the new body never touches are solved in another order, and the same physics
summed in another order differs in its last bits. From there every moving thing drifts.
**Fix:** the sort key is now built from what the bodies are (their user data, the game's entity
ids) and the sub-shapes in contact, with the body ids only breaking ties, whose relative order a
new body never changes. One patch on the pinned Jolt (`third_party/jolt_patch.cmake`), and a
native falsifier (`scraperx_sim body order`) that builds the world with a box in one band and
requires every simulation elsewhere unchanged, bit for bit.

This document is the diagnosis, the measurements that support it, the designs weighed, and the
evidence for the one built. Determinism stays the law (plan rule 10,
`03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` §2.6): the same start and inputs give the same
end, bit for bit; the native `determinism` check keeps passing.

---

## 1. What was observed

- Commit `e439ca1` split the shaft rails on the 143 m and 154 m decks and added four small stub
  boxes (`src/sim/world_solids.inc`, 1011 to 1015 boxes). The native yard-crane test then failed
  120 m below the change: the forklift hoisted onto the 22 m dock landed at
  (-30.6617, 21.9841, -127.781) instead of (-30.8683, 21.9829, -127.24), and the test's steer-in
  drove it into the dock's side rail (CI run 39440071455). Every dynamic result drifted in its
  last digits (loose stock settle 0.015534 to 0.0151985 m, the forklift at grade 2.4957 to
  2.56396 m in 2 s, the strain lift 33706 to 35372 N, the container ride 50.3908 to 50.3913).
  Static results were identical. Reverting the rail (`b17d7de`) made CI green again.
- The same on 2026-10-19 (`2257fae`): one extra fixed box broke `touch_crane_magnet`; the
  playthrough was retuned (the hook height) rather than the cause found.

## 2. The controlled experiment

The instrument is a test-only constructor, `Simulation(InitialSpawn, std::vector<TestExtraSolid>)`
(`src/sim/simulation.hpp`), that builds extra static boxes into the world: in the order of
`world_solids.inc`, before the first box of the 22 m band the extra's height falls in, or after
every body the world builds (`at_end`). Each extra is a half-meter box 300 m east and south of the
tower, in the air, that nothing ever touches. Five worlds are built: as built; one box in the
yard's band (0.5 m); one in the 143 m deck's band; one in the crane band (500.5 m); one after
every body. In each, two scenarios run and the whole world's state is compared word for word
(`world_bits`: the climber, every kit body's position, rotation, velocity and mass, every cable):

- at grade: a second of settling, the climber walks round the beam stack onto the yard's
  forklift's floor to its wheel, DRIVE, two seconds forward, a second of rest;
- in a far band: the climber on the 706 m deck walks east then west for three seconds while the
  tower's stock settles.

Before the fix (`SCRAPERX_ONLY=body-order ./build/host/scraperx_sim_tests` on the pinned Jolt):

```
  body order: as built: 1633 solids; at grade the truck ends at (37.9996, -0.014769, 20.7407), the climber at (37.9717, 1.20401, 20.5795); on the 706 m deck the climber ends at (-4.5, 707, -147.5)
  body order: a box in the yard's band: 1634 solids; at grade the truck ends at (37.9996, -0.0147703, 20.6282), the climber at (37.9729, 1.20401, 20.4675); ...
  body order: a box in the 143 m deck's band: 1634 solids; at grade the truck ends at (37.9996, -0.0147703, 20.6282), ...
  body order: a box in the crane band: 1634 solids; at grade the truck ends at (37.9996, -0.0147703, 20.6282), ...
  body order: a box after every body: 1634 solids; at grade the truck ends at (37.9996, -0.014769, 20.7407), the climber at (37.9717, 1.20401, 20.5795); ...
body order: with a box in the yard's band the state differs: at grade from word 2 of 7792, on the 706 m deck from word 769 of 7792 (-1: the same)
body order: with a box in the 143 m deck's band the state differs: at grade from word 2 of 7792, on the 706 m deck from word 769 of 7792 (-1: the same)
body order: with a box in the crane band the state differs: at grade from word 2 of 7792, on the 706 m deck from word 769 of 7792 (-1: the same)
FAIL: body order: a body built in one band leaves every simulation elsewhere unchanged, bit for bit
```

What this says:

1. A box in the yard's band, the 143 m band or the crane band perturbs the drive at grade (the
   truck 11 cm short, the climber with it) and the stock settling on the 706 m deck (from word
   769: a kit body). The box is inert; what it touches is nothing.
2. The three perturbed worlds end in **exactly the same** perturbed state. Every dynamic body is
   created after the whole solids table, so each of those boxes shifts the same set of ids by
   the same one. The perturbation is a function of which ids shift, not of where the box is.
3. A box built **after every body** shifts no id and changes nothing, bit for bit, though it is
   one more body in the broad phase. The broad phase is not the carrier; the ids are.

## 3. What carries it

In the pinned Jolt (5.6.0, `e77f175`), `ContactConstraintManager::TemplatedAddContactConstraint`
computes, for each manifold,

```cpp
SubShapeIDPair key { inBody1.GetID(), inManifold.mSubShapeID1, inBody2.GetID(), inManifold.mSubShapeID2 };
uint64 key_hash = key.GetHash();          // HashBytes over the four fields
```

and passes `key_hash` to `CreateConstraint` as the constraint's `mSortKey`; the cached path
(`TemplatedGetContactsFromCache`) does the same with `input_hash`. Before the solver runs an
island, `IslandBuilder::Finalize` calls `ContactConstraintManager::SortContacts`, which orders the
island's contacts by `mSortKey`, then body 1's id, then body 2's id. The solver is sequential
impulses (Gauss-Seidel): every constraint reads the velocities the constraints before it wrote,
so the order is part of the arithmetic, and two orders give results that agree to a few ulps and
then part under the game's own feedback (the forklift's controller, the stock's friction).

A byte hash is not monotonic in its input. Shifting one body's id by one changes its contacts'
keys arbitrarily, so an island whose bodies were all created after the new one (all of them, in
this world: every machine, every piece of stock, the player) is sorted differently although
nothing in it changed. Every other use of body ids in the step is monotonic and so unmoved by an
insertion: which of a pair is body 1 (`PhysicsSystem::ProcessBodyPair`: the higher motion type,
else the lower id), the island's own body order, the CCD order, and the two tie-breaks in
`SortContacts` itself. The manifold cache is looked up by value. The contact listener here
already picks the support by a total order of its own (`PlayerContactListener::supports_better`),
because Jolt's callback order was never fixed.

Evidence that this and nothing else is the carrier: with the sort key changed and nothing else
(§5), all five worlds end in the same 7792 words in both scenarios (§6). With it unchanged, they
do not (§2).

## 4. Designs weighed

| Design | What it does | Verdict |
|---|---|---|
| **Key the sort on what the bodies are** (built) | The sort key becomes a hash of (body 1's user data, sub-shape 1, body 2's user data, sub-shape 2); ties fall to the body ids, whose relative order an insertion never changes. User data here is the entity id, a constant per thing (the player 2, every world solid 51, each machine part its own). | Removes the dependency at its root: the solve order is a function of what is in contact, not of when it was made. Twelve lines in the engine, applied by the patch script the tree already has, idempotent, and the configure stops if a Jolt bump moves the pinned text. No change to body creation, saves, memory or speed (one 32-byte hash per contact per step, where one was already taken). |
| Reserved id ranges per band (`BodyInterface::CreateBodyWithID`) | Every builder takes ids from a range for its band, so an insertion shifts only its own band's later ids. | The pinned Jolt has `CreateBodyWithID` and `BodyManager::AddBodyWithCustomID`; the cost is the holes: `mBodies`, the active lists, the sequence numbers and the broad phase's tracking are sized to the highest id (about 25 bytes an id), and a body created at run time (a corpse, the hook) takes the free list's newest hole, whose place depends on how full the bands are, so those too would need a range. Every creation site (the kit, the crew, the Matron, the corpses, the hook, the player) changes, and the hash on the ids stays underneath, so the next id scheme breaks it again. Rejected: more surface, the cause left in place. |
| Fewer, larger static bodies per region | Merge the dressing into few bodies a band so there are fewer ids to shift. | Does not touch the cause (one shifted id is enough), and merging changes contact geometry where the player stands on a seam. Rejected. |
| Region-local build order | Build each band's statics and machines together so an insertion shifts only ids after it in its band. | Every dynamic body is created after the solids and all of them shift together whatever the order; and the save format already depends on the kit's build order. Rejected. |
| Drift-sensitive tests driven closed-loop | A test steers for its target from where the machine is, as a player would, rather than replaying fixed inputs. | A complement, not a substitute: it hides a drift, it does not remove one, and a test must still prove what it proved. The yard crane's dock drive already steers for the dock's middle from the truck's own position; it failed because the landing itself moved 54 cm. Nothing changed in the tests for this. |

## 5. What was built

`third_party/jolt_patch.cmake`, applied to the pinned checkout by FetchContent's `PATCH_COMMAND`
(and by hand to an existing `build/*/_deps/jolt-src` with
`cmake -DJOLT_SOURCE_DIR=<checkout> -P third_party/jolt_patch.cmake`), adds to
`Jolt/Physics/Constraints/ContactConstraintManager.cpp`:

```cpp
static JPH_INLINE uint64 sStableContactSortKey(const Body &inBody1, const SubShapeID &inSubShapeID1, const Body &inBody2, const SubShapeID &inSubShapeID2)
{
	const uint64 words[4] = { inBody1.GetUserData(), uint64(inSubShapeID1.GetValue()), inBody2.GetUserData(), uint64(inSubShapeID2.GetValue()) };
	return HashBytes(words, sizeof(words));
}
```

and passes it, in place of `key_hash` and `input_hash`, to both `CreateConstraint` calls. The
manifold cache keeps its own hash of the ids; only the solver's order changes. `SortContacts` is
untouched: equal keys (the player on three world solids, all entity 51) still fall to the ids,
a strict total order, so the simulation stays deterministic across runs and threads as before.

Where entity ids are a running count (the loose stock from 5000, the ziplines' lines, the
corpses), they are a function of the build and of the inputs, not of the ids, and so unmoved by
a body added elsewhere. The one way a change in a band still reaches another is a real one: a
new body that takes a place a piece of stock was to have moves that piece and every later one in
the kit's list, which the save refuses as it did ("the world's bodies stand in another order").

**Saves.** Nothing in a save names a Jolt id: it holds the kit's entity list and its checkpoint
tables, in the kit's build order, which this change does not touch. Saves written before it load
as they did, and a save whose entity list does not match is refused exactly as before
(`src/sim/physics_world_state.cpp`, `read_save`).

**What changes once.** The solve order of every island is re-dealt by the new key, so every
dynamic result moves in its last digits once, the way the rail moved them, and then holds. The
groups below were run before and after to see what moved; every bound held.

## 6. Evidence

The falsifier, `scraperx_sim body order` (`tests/sim_tests_body_order.cpp`, in the full suite
and `SCRAPERX_ONLY=body-order`), on the patched build:

```
  body order: as built: 1633 solids; at grade the truck ends at (37.9996, -0.014769, 20.7346), the climber at (37.972, 1.20401, 20.5735); on the 706 m deck the climber ends at (-4.5, 707, -147.5)
  body order: a box in the yard's band: 1634 solids; at grade the truck ends at (37.9996, -0.014769, 20.7346), the climber at (37.972, 1.20401, 20.5735); on the 706 m deck the climber ends at (-4.5, 707, -147.5)
  body order: a box in the 143 m deck's band: 1634 solids; at grade the truck ends at (37.9996, -0.014769, 20.7346), the climber at (37.972, 1.20401, 20.5735); on the 706 m deck the climber ends at (-4.5, 707, -147.5)
  body order: a box in the crane band: 1634 solids; at grade the truck ends at (37.9996, -0.014769, 20.7346), the climber at (37.972, 1.20401, 20.5735); on the 706 m deck the climber ends at (-4.5, 707, -147.5)
  body order: a box after every body: 1634 solids; at grade the truck ends at (37.9996, -0.014769, 20.7346), the climber at (37.972, 1.20401, 20.5735); on the 706 m deck the climber ends at (-4.5, 707, -147.5)
PASS scraperx_sim body order: worlds=5 grade_words=7792 high_words=7792 truck_x=37.9996 truck_z=20.7346
```

Every world builds its one extra solid (1633 to 1634), the truck has been driven (4.2 m from its
park), and all five worlds end in the same 7792 words in both scenarios. The same test fails on
the pinned code (§2), so it cannot pass vacuously.

**Every affected native group, as built and with an extra solid in the 143 m band.** Each group
was run twice on the patched build: as built, and with `SCRAPERX_EXTRA_SOLID_Y=143.5` (every
world of the run gets the inert box, built with the 143 m deck's solids). The two runs' every
line, numbers included, were diffed (`compare_group.sh` in the session's scratchpad: the group
twice, `diff`):

| Group (`SCRAPERX_ONLY=`) | Its proof line, as built, patched | With the extra solid |
|---|---|---|
| `crane` | `PASS scraperx_sim crane: forked_up_44_m=0.363091 forklift_x=-22.5477 stall_n=75000`; the forklift landed at (-30.8683, 21.9829, -127.238), driven in to (-22.5477, 21.9857, -127.186); the container ridden to 50.3909 | identical, every line |
| `magnet` | `PASS scraperx_sim magnet: slab_held_n=49000 beam_held_n=16333.3 stack_rose_m=-0.00648805 box_held_n=4900` | identical |
| `spreader` | `PASS scraperx_sim spreader: lifted_to_y=3.45839 set_down_y=1.29495` | identical |
| `forklift` | `PASS scraperx_sim forklift: carried_m=3.31455`; `PASS scraperx_sim forklift strain: coil_rise_m=1.05531 stillage_pitch_deg=9.91471` | identical |
| `stock` | `PASS scraperx_sim stock: pieces=325 settle_m=0.0153381 shoved_m=0.00119781` | identical |
| `determinism` | `PASS scraperx_sim determinism: words=7792 ticks=4895` | identical |

**What moved once, pinned to patched** (the one re-deal of the solve order, §5), from the same
groups run on the pinned engine before the patch: the forklift landed on the dock at
(-30.8683, 21.9829, -127.24) and now at (-30.8683, 21.9829, -127.238); driven in to x -22.558,
now -22.5477; the container ridden to 50.3908, now 50.3909; `forked_up_44_m` 0.363388, now
0.363091; the forklift's `carried_m` 3.25749, now 3.31455; the strain's `coil_rise_m` 1.05542 and
`stillage_pitch_deg` 9.9545, now 1.05531 and 9.91471; the stock's settle 0.015534 m (the owner's
figure from before the rail), now 0.0153381. The same order of change the rail made, once, and
then held: every bound held, and nothing in a test was moved to make it hold.

**The full native suite** on the patched build
(`cmake --build build/host --target scraperx_sim_tests scraperx_native`, then
`./build/host/scraperx_sim_tests`): still running when this commit was made (75 proof lines passed, none failed, at the AS-010 band); its verdict is recorded by the commit after this one.

**The touch scenarios** the change plausibly reaches, played as CI plays them
(`tools/godot/godot --headless --fixed-fps 60 --path godot -- --uitest=<name>`, the patched
extension): 

| Scenario | Result |
|---|---|
| `touch_forklift` | `SCRAPERX_UITEST PASS touch_forklift` (14 s) |
| `touch_forklift_strain` | `SCRAPERX_UITEST PASS touch_forklift_strain` (12 s) |
| `touch_yard_crane` | `SCRAPERX_UITEST PASS touch_yard_crane` (16 s) |
| `touch_crane_magnet` | `SCRAPERX_UITEST PASS touch_crane_magnet slab=5318 lifted_to=1.46 held_read='HOLDING 2.0 T OF 5.0 T'` (40 s) |
| `touch_crane_spreader` | `SCRAPERX_UITEST PASS touch_crane_spreader` (54 s) |
| `touch_save` | `SCRAPERX_UITEST PASS touch_save` (5 s) |
| `touch_stack` | still playing when this commit was made; recorded by the commit after this one |


CI plays the whole list on the owner's push of `ScraperX-Claude`; the suite's proof lines now
include `PASS scraperx_sim body order`.

## 7. The rule from here

- A body added, removed or split in one band changes that band's own contacts and nothing
  else's. The falsifier holds the world to that; a change that fails it has found a second
  carrier, to be diagnosed the same way (§2's instrument, five worlds, the first differing word).
- A new static that takes a place a piece of stock or a zipline was to have moves that piece
  in the kit's list: that is a real change in the band, and the save refuses as it does.
- The patch is part of the pinned Jolt: a Jolt bump stops the configure until someone re-checks
  how contacts are sorted for the solver (`third_party/jolt_patch.cmake`,
  `third_party/README.md`).
