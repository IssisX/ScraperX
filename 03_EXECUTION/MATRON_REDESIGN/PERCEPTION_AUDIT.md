# Perception audit: where the Matron reads the player's true state

Her law (`00_START_HERE.md` rule 29): perception is evidence with an age. She may know where he is only from what she sees (line of sight), hears (sounds he makes) or remembers (old evidence that ages), or feels (physics contact, as when she holds him). This is a read-only audit of `src/sim/matron*.cpp` and `matron.hpp`, and of what hands her player data. No code was changed and nothing was run.

Branch of record: `claude/perception-audit-20261029`, on trunk at b551b24.

## Result

| Class | Meaning | Decisions | Source lines that name the player |
|---|---|---|---|
| (a) lawful | through a sight or hearing check, a contact she feels, a hold, or a filter that only narrows what she senses | 14 groups (A1-A14) | 270 |
| (b) cheat | his true position, noise level, carried stock or distance steers her with no sight, hearing or contact | **5** (B1-B5) | 11 |
| (c) unclear | not a decision of hers, a stand-in for a physical load, or a channel that leaks nothing yet | 4 (C1-C4) | 16 |

Every line in `matron*.cpp` and `matron.hpp` that names `player` (a `grep` for `player`, `player_`, `playerX`, comments dropped: 297 lines) is classified in the appendix; no line is left out (the script that checked this stopped on any line without a class). Values derived from his position (`px`, `pz`, `d`, `on_deck`, `loudness`) were traced by hand to every use; those uses are in the rows below. Worst first:

1. **B1 The trail**: she records the exact path he walks on her deck every tick, seen or not, and follows it when searching (`matron.cpp:2311-2314`).
2. **B2 The hush**: her "silence for silence" reads his raw noise level at any distance and cancels when he is within 4 m, unseen (`matron_moves.cpp:308,318`).
3. **B3 Giving way**: on her own journeys she yields to him on the ladder she means to take by reading his position, with no sight (`matron_journey.cpp:202-215,326`).

## The channel

Everything comes in through one door: `Matron::pre_step(float dt, JPH::RVec3 player, float loudness)` (`matron.hpp:1134`), called at `physics_world.cpp:720` with the player body's true position and a loudness. Inside it, `px`, `pz`, `d` (`matron.cpp:2444`, distance on the floor plane) and `on_deck` (`matron.cpp:2284`) are computed from the true position before any perception test, and stay in scope for the whole function. Four of the five cheats are one of those values used where a sight or hearing result should stand. Other things handed to her, all classified below:

- `physics_world.cpp:704` `set_player_holding(carried_entity_)`: what he carries (B4).
- `physics_world.cpp:718` `set_player_ankle(...)` every tick, held or not (C4).
- `physics_world_grab.cpp:288-289` `set_player_braced`, `set_thrash` (C2; `thrash_` is stored and never read).
- `physics_world_arm.cpp:149` `set_player_body(wrist, arms_left)`, `physics_world_strike.cpp:253` `struck_by_player`: contact (A).
- `physics_world.cpp:854-867` `caught`, `begin_grab`, `shove_direction` with his position: hands-on range (A).
- The loudness itself, `matron_loudness` (`physics_world_movement.cpp:1218-1257`, from his speed, grounded, crouch, sprint, climb, landing speed and forklift), scaled by `bay_->mask_factor(player_at)` and `max`ed with the microphone's `room_loudness`: the noise he makes, lawful as a source model so long as she hears it only through reach and carried path (A7).

## Cheats (b), worst first

### B1. The trail of crumbs: she remembers where he walked, unseen and unheard

- **Where**: `matron.cpp:2311-2314` in `Matron::pre_step`: `if (on_deck && body_in_world_) drop_crumb(px, pz, player_floor_y);`. Stored by `Matron::drop_crumb` (`matron_moves.cpp:100-116`: 24 crumbs, one every 1.5 m, so the last 36 m of his path on her level). Read by `Matron::crumb_ahead` (`matron_moves.cpp:120-150`), called from the Searching state at `matron.cpp:2995`.
- **What she learns that she should not**: his exact path, floor by floor, on her deck, every tick she is out of her locker and no journey or climb of her own has her body (those return earlier, `matron.cpp:2202-2275`), in every state that reaches it, whether or not she saw him, whether or not any sound of his reached her. A crouched walk (loudness 0.04 at `physics_world_movement.cpp:1230`; reach is loudness times `kFullHearingM` 55 m, so 2.2 m) lays the same trail as a run.
- **When it matters**: she loses him and searches. Once she meets any crumb within 6 m (`kCrumbMeetM`) she walks "the way they went", along a path she never witnessed, to where he is. Doubling back, going quiet and breaking line of sight all fail against it; and the trail's last crumb is his hiding place.
- **Smallest lawful replacement**: drop the crumb where she takes the lead, not from his true position. Remove the unconditional call at 2311-2314 and call `drop_crumb` in the `saw` branch next to `take_lead(px, pz, ...)` (`matron.cpp:2481`) and, in the heard branch, only when `selected_player_sound` is true (`matron.cpp:2489-2503`), with the position and floor she took as the lead. The trail then holds only positions she saw or heard, and each is as old as the evidence.

### B2. The hush: she reads his raw noise level and his nearness

- **Where**: `matron.cpp:2949` `hush_step(dt, saw, d, loudness)`, with `d` the true floor-plane distance (`matron.cpp:2444`) and `loudness` the unattenuated source level; used at `matron_moves.cpp:308` (`loudness > kQuietLoudness` ends the hush) and `matron_moves.cpp:318` (`saw || distance < 4.0F` ends it).
- **What she learns**: (i) whether he is making noise anywhere, at the level he makes it, however far or muffled: the real hearing test is `heard = player_reach >= player_carry` (`matron.cpp:2449`) and this call ignores it; (ii) that he is within 4 m on the floor plane (the distance ignores height, so a deck above or below counts), with no sight and no sound.
- **When it matters**: every chase. She stands still and listens when he goes quiet (`kHushAfterS` 1.2 s). A player who hides silently within 4 m, behind a crate or a wall or a deck away, breaks her hush the moment he is near; a player who sprints on a far deck keeps resetting her hush though nothing reached her ear.
- **Smallest lawful replacement**: pass the heard level, not the source level: `heard ? loudness : 0.0F` (`heard` is already computed at 2449). Replace `distance < 4.0F` with the distance to her own lead, `distance_xz(ax, az, heard_x_, heard_z_)`, or drop it: `saw` is already in that condition.

### B3. Giving way on a journey: she reads his position with no sight

- **Where**: `matron_journey.cpp:202-212` (`player_on` reads `bodies.GetPosition(player_)`), used at `matron_journey.cpp:215` (cancels the move she has begun) and `matron_journey.cpp:326` (backs off the climb's foot, counts `travel_.yields`, waits `kYieldGiveUpS`, then marks that edge avoided).
- **What she learns**: that he is on, at the foot of, or at the head of the very climb her plan uses (within `kYieldReachM` 1.3 m of its line, in its span), including when he is behind the plate or on the far side of the ladder.
- **When it matters**: she is on her own `travel_to` journey (her own climbing between decks). Her `pre_step` returns for a journey before any sight test runs (`matron.cpp:2265-2275`), so nothing in that path is gated. A player in a gap she cannot see makes her stop and step away from the ladder; the wait is a tell, and the avoided edge changes her plan.
- **Smallest lawful replacement**: let `player_on` see him: within `kSightM` and `sees(eyes, his chest)` as `notice_ridden_line` does (`matron_line.cpp:128`), or, for a body in her way, `physics_.WereBodiesInContact(body_, player_)`. Either gives the same behavior with him in view and none without.

### B4. Handled stock: she knows what he touched and what he carries

- **Where**: `matron.cpp:2231-2236` sets `handled_` for any throwable whose entity equals `player_holding_` (handed in at `physics_world.cpp:704`, `matron.hpp:1169`); `stock_to_fetch` (`matron.cpp:3241`) takes only handled stock and skips what he holds; `fetch_stock` re-checks it at `matron.cpp:3277`.
- **What she learns**: every piece of stock he has picked up, and what he is carrying now, at any distance, unseen.
- **When it matters**: Pest stage, hunting without sight (`matron.cpp:2775`): a crate he moved in silence in a far corner becomes her ammunition (`matron.hpp:529-547`: "light stock the player has handled is theirs to her"); stock in his hands is skipped.
- **Smallest lawful replacement**: set `handled_` only on a tick when she saw him with it (move the loop into the `saw` branch of the Hunting block, `saw && player_holding_ == entity`); a heard impact of that entity already reaches her through `hear` (`physics_world.cpp:688-690`). Drop the `player_holding_` tests at 3241 and 3277: `stock_to_fetch` already refuses stock that moves faster than 0.3 m/s or sits off her floor, which covers a carried crate.

### B5. Pace by true distance: she hurries when he is far

- **Where**: `matron.cpp:2956` `choose_pace(dt, d, saw, heard, loudness)` with `d` true; `matron_moves.cpp:60-70`: with no sight and no sound this tick, `tier = distance > 15.0F ? 2 : 1`.
- **What she learns**: whether he is more than 15 m away on the floor plane, unseen and unheard.
- **When it matters**: any chase with a cold lead. She creeps when he is near and stalks when he is far: her speed tells the player, and she closes distance faster the farther he is.
- **Smallest lawful replacement**: the comment there already says "after the lead": pass `saw ? d : distance_xz(ax, az, heard_x_, heard_z_)`. The distance used with `saw` (`matron_moves.cpp:39,58`) is already sight-gated and stays.

## Unclear (c)

- **C1. `player_on_deck_`** (`matron.cpp:2302-2310`, `matron.hpp:1212,2189`): the true "he is on her deck, up a ladder from it, or on a deck her pipe goes to" flag, published by `freezes_checkpoint()` so the world holds the checkpoint while she is out. It never steers her; it is the world's checkpoint rule. Kept unclear because it publishes the true flag from her side. Suggest the rule live in `physics_world.cpp` using the world's own deck test, so her class holds no such flag.
- **C2. `player_braced_`** (`matron_hold.cpp:644`; set at `physics_world_grab.cpp:288`; reset `matron.cpp:3532`): while she holds him, her pull ramps when he is braced on a rail. She would feel the rail's resistance, but the code reads the flag of his grip, not the load on her hand. Lawful replacement: use the measured hand load (`grip_load_n_` in `physics_world_grab.cpp`) as the input.
- **C3. `bay_hooked_`** (`matron.cpp:1922` condition to set her trap, `matron.cpp:2065-2066` to let it go; `physics_world.cpp:710` `set_bay`): world state that his act (the chain on the leaf) changes. Holding the leaf she would feel the chain take it, which makes 2065 lawful; the start test at 1922 reads it with no perception.
- **C4. True ankle and position handed every tick**: `physics_world.cpp:718` calls `set_player_ankle(player_ankle(bodies))` also when nothing is held, and `pre_step` takes `player_at` every tick. Only hold code reads the ankle today (`matron_hold.cpp:127-128,656,809-811`), so it leaks nothing now; the channel is the reason B1-B5 were possible. Fix by structure, below.

## Lawful but generous (not cheats; for the redesign)

- Sight is one ray (two a head's width apart) from her eyes to his chest, 360 degrees, up to `kSightM` 9 m (`kCommitSightM` 50 m for the long look), with no facing, light or motion needed (`matron.cpp:387-410`, `2447-2458`).
- A heard sound gives the lead its exact coordinates (`matron.cpp:2488-2503`): hearing carries no direction error.
- He is heard through `room_loudness` (the microphone) from his true position as if the sound were in the tower.

## Structural fix (the earliest invalid transition)

The state that became invalid: a Matron function holding his true position with no evidence behind it. The transition that allowed it: `pre_step` takes `player` and keeps `px`, `pz`, `d`, `on_deck` in scope for the whole tick. To make it unreachable, split the tick in two: one function that takes `player` and `loudness` and returns an evidence record (seen: bool and where; heard: bool, where and level at her ear; level of the floor he was last placed on), and a body of the tick that takes only that record. `hold` and `contact` code take a handle to the body in her hand, not a position. Then B1-B5 do not compile. This is bigger than the five replacements above and is not part of this audit.

## Appendix: every line that names the player

Counted by `grep` over `matron*.cpp` and `matron.hpp`, comments dropped. Class (a) groups: A1 body identity; A2 body filters (the player left out of her floor, support and contact probes); A3 `step_turn`; A4 the bay trap, gated by sight or hearing; A5 `pre_step` entry, `notice_ridden_line` (sight-gated), `step_knocked` (argument unused); A6 level gates; A7 hearing; A8 sight; A9 where he is going, only seen or heard doing it; A10 throwing, only in sight; A11 contact and hold; A12 her hold on him; A13 declarations, stored fields, contact setters; A14 the rope on her way out's eye, read only at that mouth (`matron.cpp:3011,3021`).

| Class | Group | Lines | Where |
|---|---|---|---|
| a | A1 body identity | 2 | matron.cpp:75,77 |
| a | A10 throwing at him: only with him in sight | 7 | matron.cpp:2772,2775,3361,3364-3365,3371,3468 |
| a | A11 contact and hold: hands on him, his blow on her, her view export | 29 | matron.cpp:2238,2403,2543,2943,3097-3098,3180,3190-3191,3194-3196,3207-3208,3561,3759-3760,3828,3832,3840-3841,3847,3849,3863,3868-3870,3877,4291 |
| a | A12 her hold on him: he is in her hand, her view of his body is touch | 87 | matron_hold.cpp:47,84,97,131-132,138,183,187-189,192,201,206,225-226,236,289,342,368-369,456,471-474,515,574,579,582,585,589,615-616,620,656,663,696,707,761,809-813,818,847,891,915,930,981-982,985-986,1006,1016,1040,1046,1075,1088,1097,1114,1116,1130,1132,1137,1176,1205,1224,1227,1242,1268,1274,1280,1293,1397,1404,1408,1411,1419,1425,1430,1451,1459,1470-1471,1491,1511 |
| a | A13 declarations, stored fields and setters of contact data (his blow, his wrist, his arm count): no read of his state outside contact | 43 | matron.hpp:947,1127,1205,1236,1308,1353-1355,1371,1635,1713-1716,1718,1720-1722,1725-1727,1846,1850,1870-1872,1892,1896,1908,1919,1923,1927,1935,1952,2039,2087,2097-2098,2155,2250-2251,2359-2360 |
| a | A2 body filters: the player is left out of her floor, support and contact probes | 34 | matron.cpp:1084,1172,2863,3703,3716,3983,4063,4127,4394,4419; matron_climb.cpp:111; matron_contact.cpp:92; matron_found_climb.cpp:499,642; matron_journey.cpp:37-38,40,44,78,171,356,362,367,411; matron_moves.cpp:244; matron_pipe.cpp:72,112,212,231; matron_traversal.cpp:74,76,104,106,2054 |
| a | A3 step_turn: distance counts only while her sight has held (sight_far_s_), the landing only as carried to her ear | 5 | matron.cpp:1752,1756,2559,2766,2938 |
| a | A4 bay trap: the player's position read only on (saw || heard) or a perceived flag | 15 | matron.cpp:1911,1915-1916,1918,1925,1927-1928,1939,1992,2030,2063,2072-2074,2583 |
| a | A5 pre_step entry; notice_ridden_line (sight-gated); step_knocked (argument unused) | 9 | matron.cpp:2157,2212,2266,2271,3104,4163-4164; matron_line.cpp:115,128 |
| a | A6 level gates: on her level or up a ladder from it, the only places she can see or hear him (narrows what she can learn) | 6 | matron.cpp:2278-2279,2284,2287,2296-2297 |
| a | A7 hearing: his noise tested against its reach and carried path to her ear | 24 | matron.cpp:2364-2366,2373-2374,2376-2378,2391,2393-2394,2412-2413,2447-2449,2465-2467,2488-2489,2491,2493,2502 |
| a | A8 sight: a ray from her eyes; the lead is taken from the position she sees | 4 | matron.cpp:2452,2454,2458,2481 |
| a | A9 where he is going: only when seen or heard doing it | 5 | matron.cpp:2519,2530-2531,2535,2541 |
| b | B1 trail crumbs | 1 | matron.cpp:2313 |
| b | B3 journey yield | 5 | matron_journey.cpp:202-203,206,215,326 |
| b | B4 handled stock | 5 | matron.cpp:2233,3241,3277; matron.hpp:1169,2173 |
| c | C1 player_on_deck_ | 7 | matron.cpp:2302,2307-2310; matron.hpp:1212,2189 |
| c | C2 player_braced_ | 4 | matron.cpp:3532; matron_hold.cpp:644; matron.hpp:1318,2076 |
| c | C4 true ankle handed every tick | 5 | matron_hold.cpp:127-128; matron.hpp:1134,1328,2116 |
