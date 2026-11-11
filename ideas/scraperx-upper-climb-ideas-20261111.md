# Upper climb — 340 m to 750 m — 2026-11-11

The climb from 340 m to 750 m is still a ladder and a board every 22 m, with a machine beside it. The machines are already built. They do not have to stay as one short lift each.

No new ladders. Nothing here starts by pulling a handle. She can reach him at every height. A shelf only he can jump to is not allowed: her highest jump-and-hold is 3.64 m over her feet, and his is 4.14 m.

Heights, masses, and energies below are from the band write-ups and from her limits in code (`src/sim/matron_traversal.hpp`: 220 kg, 8,000 N, 44,000 W, rung climb 0.6 m/s). The player is 85 kg, from the stage evaluators.

## Where it is now

| Place | Height | What he does today |
|---|---|---|
| Plate shop start | 340.25 m | Three lifts (scaffold, girder, monolith), or ladders |
| Cage stop | 462.25 m | Then a ladder to the 484 ring |
| Facade start | 484.25 m | Wagon, crane jib, cart, or ladders |
| Transfer plate | 640.25 m | Reel up 22 m, then a 10-move climb |
| Upper decks | 684, 706, 728, 750 m | Gravel hoist, another 10-move climb, gravel wheel |

The six ladder laps from 340 to 484 take 169 seconds with no machine. The six from 484 to 640 take 184 seconds. That is the part to stop rebuilding.

## 1. One cage through the plate shop

**340.25 m to 462.25 m. It lifts him 122 m.**

He takes the scaffold rope off its cleat and hooks it on the cage. That hook-up is already how this band is linked. He does not pull the pin. The scaffold drops, and that drop knocks the girder loose. The girder's tip knocks the monolith loose. He stays in the one cage the whole way. The cage stops at 462.25 m, which is where the monolith's cage already stops.

**Uses:** the scaffold (3 t, 34 m drop, 1.00 MJ), the girder and its 14 t trolley (1.73 MJ), and the 20 t monolith (0.91 MJ). All three are built.

**Numbers:** cage 900 kg plus him is 985 kg. Over 122 m that is 1.18 MJ. The three drops are 3.64 MJ, about three times what the cage needs. If she is on the cage too (220 kg), it is 1.44 MJ, still under 3.64. The catches are the same ones already on the reel: one every 0.1 m, so a stall holds the cage on a tooth instead of dropping it. At the reel's kind of speed, about 3 m/s, one tick at 90 Hz is about 3 cm, so the cage cannot skip a tooth.

**Her:** the cage is a floor. She gets on it, or she is still on the plate when it leaves. She climbs rungs at 0.6 m/s, so 122 m of rung would take her about 200 seconds. The ride is under a minute. She is not shut out: the slumped tower and the fallen monolith are already climbs in the tests, and she uses those to the next floor. He gets away by being on the cage when the rope goes on.

**Watch:** the tower drops, the girder turns, the monolith falls, and the cage goes past four rings in one go.

**What goes wrong:** if the three ropes stay separate, the cage stops at 34 m and the rest never happens. One guide to 462.25 m, and each mass hits the next latch itself. The monolith is 20 t. It stays on the hinge the built test already uses. Do not let it fall free through a tick.

**Build:** a few days. The three lifts exist. The new work is one cage, one guide, and the two hits.

## 2. One cab up the outside

**484.25 m to 640.25 m. It lifts him 156 m.**

He sets the 30 kg rail block into the traveler's tray. The code already refuses to move until that block is in. He does not pull the wagon's chock, the jib's pin, or the winch clutch. The wagon runs and takes the cab up the north face to 528 m. The cab's arrival swings the jib. The jib takes the same cab to 572 m. The cab's arrival there lets the freight cart go, and the cart's winch takes him to 640.25 m.

**Uses:** the 4 t wagon (44 m at 70°, 1.62 MJ), the 8 t jib (0.96 MJ), and the 4 t cart (51 m at 80°, 1.98 MJ).

**Numbers:** 156 m for a 985 kg cab is 1.51 MJ. The three sources are 4.56 MJ, about three times enough. With her on the cab it is 1.84 MJ, still under 4.56. The wagon with no governor would hit about 28 m/s at the bottom of its 44 m run (`sqrt(2 × 9.81 × sin 70° × 44)`). That would skip through the catches. The built wagon is already governed. Keep that. Cap the cab near the 3.5 m/s the reel and the gravel hoist already use.

**Her:** the cab is a floor on the face, so she can be in it. If she misses it, the spent wagon and the hanging jib are already climbs in the tests (the jib chord is 23 m, about 40 seconds at her rung speed). She can catch him on the next deck if the cab pauses. He gets away by setting the block and going. There is no lip here that only he can reach.

**Watch:** a cab climbing the outside of the tower while a crane jib swings down beside it and a cart drops on the far face.

**What goes wrong:** the same as the plate shop. Three separate stops leave him short. One cab, one guide, each mass tripped by the cab arriving. The jib's rope must not be asked to hit the tower. The cab hits a latch. The rope only pulls the cab.

**Build:** about a week. Three machines on one cab, and the speed has to stay under 3.5 m/s.

## 3. The reel goes past the gantry

**640.25 m to 684.1 m. It lifts him 44 m.** This deletes the 10-move gantry climb.

The built reel lifts about 22 m and then fades, because it only has 23 m of cable at 22 kg/m. Give it 50 m of that same cable. He hooks the shackle, which is the link already there. He does not pull the chock. The reel is already heavier than the cage once the rope is on the cage, so it goes.

**Uses:** the service reel. Drum and yoke stay 1,339.5 kg. Cage stays 1,500 kg.

**Numbers:** 50 m of cable is 1,100 kg. The reel starts at 2,439.5 kg. The cage with him is 1,585 kg. It is 854 kg heavy at the start, and it loses 22 kg every metre the cable pays off. It matches the cage after about 39 m. The built reel coasts on past the match and tops out near twice that distance, so this one should clear 44 m and settle back onto the 684 deck. That coast is the part to rerun in the same evaluator before anyone builds it. Starting pull is about 8,200 N after the 135 N shoe friction. Shared by both masses, that is about 2.1 m/s² at the first instant. The built reel peaks at 2.85 m/s on a milder start, and the evaluator rejects anything over 3.5 m/s. If this peaks hotter, raise the shoe friction inside the band the code already tests (85, 135, 185 N). Do not write a speed onto the cage.

**Her:** she is in the cage or she is on the 640 plate. The 662 deck is still there, so a stall is a floor she can step onto, not a hole. The ride is short, well under a minute. She does not get a private ladder around it.

**Watch:** the reel shrinking as the cable jumps to the head beam, and the cage rising past the old gantry without him touching it.

**What goes wrong:** too much spare weight means a hard hit at the top. The built reel's rule is that it is caught on the teeth and the stop impact is zero. Same rule here. If the evaluator misses the 684 deck, add cable in small steps. Do not add a second motor.

**Build:** a day if the evaluator lands on the deck. Another day if the friction has to move.

## 4. The gravel keeps going

**684.1 m to 728.1 m. It lifts him 44 m.** This deletes the cooling-plant climb.

He is still in the cage from the reel. The cage arriving tips the gravel flap. He does not grab the chain. The bin pours, the hopper sinks, the cage rises to the 728 deck.

**Uses:** the gravel hoist. The bin already holds 1,213 kg. The hopper steel is 500 kg. The cage is 2,000 kg. The pour is 400 kg/s. The chain is 33 kg/m.

**Numbers:** the built chain is only 23 m, which is why the lift dies at about 22 m. Make the chain 46 m. Start of the heavy side: 500 + 1,213 + 46×33 = 3,231 kg, against the cage and him at 2,085 kg. It is 1,146 kg heavy, and it loses 33 kg a metre, so it matches the cage after about 35 m and should coast to the 728 deck. Same warning as the reel: rerun the evaluator. Starting acceleration is about 2.1 m/s². The built gravel hoist peaks at 3.06 m/s. If this one passes 3.5 m/s, use the same shoe-friction band. The rope on the built hoist stays above 18.6 kN. A longer chain does not change that duty much, but the evaluator has to say so. Do not claim it until that run.

**Her:** same cage, so she is either in it or waiting on the 684 deck. The 706 deck is still beside the guide. If the cage sits on a tooth there, she can step in. That is how she catches a stall. He gets away by not stalling. Nothing on this ride is above her 3.64 m jump-and-hold, because she is not jumping. She is walking onto a floor.

**Watch:** gravel hammering into the hopper while the cage climbs through two decks that used to be a crawl and a sprint.

**What goes wrong:** the pour is 400 kg/s. If the flap sticks half open, the hopper never outweighs the cage and he sits there. The flap is already a plate that falls open past a dead point and stays open. The cage hits that arm. It does not depend on how hard he tugs. A stall leaves the cage on a tooth at a real deck.

**Build:** a few days. The hoist exists. The chain length and the one guide are the work, after the evaluator agrees.

## 5. The wheel, with no handle

**728.3 m to 750.4 m. It lifts him 22 m.** Short on purpose. It is the last move, and it is a different machine from the four above.

The cage from the gravel ride is the wheel's cab, or he steps across one deck into it. He does not pull the T-handle. The arriving cab knocks the chock off the ram. The ram is the built one: 800 kg on a 4 m arm, hung back at 35°. It swings into the latch, the gate opens, and 498 kg of gravel pours at 400 kg/s.

**Uses:** the gravel wheel as built. Wheel 2,200 kg, inertia 3,893 kg·m² (973 kg reflected at the bucket). Bucket 400 kg. Chain 16 kg/m over 45 m. Cab 2,400 kg. Rim twice the drum, so the bucket drops two metres for every metre the cab rises.

**Numbers:** these are the built ones, not a stretch. Cab apex about 22.1 m, peak about 2.6 m/s, rope stays above 11.3 kN on the cab side. Breakaway is 26, so it does not hesitate. With him aboard the floor stays under him (normal load never under 774 N in the evaluator). If he steps out, the empty cab runs high and the bucket hits the plant floor. That case is already in the write-up. The catches hold.

**Her:** the cab is a floor from 728 to 750. The ride is about 10 seconds at 2.6 m/s. She catches him only if she is already in the cab or on the 728 deck when the ram swings. After it, both of them step off onto the 750 deck. The well beside the cab is a 22 m hole and a fall past 20 m is fatal for him, so the rail around that well stays.

**Watch:** the ram swinging in, the wheel turning, gravel pouring, the cab rising the last floor.

**What goes wrong:** if the cab misses the chock, nothing starts and he is standing at 728 with no handle to pull. The chock post has to sit in the cab's path, not off to the side where a good arrival misses it. The ram's swing is already damped in the code (0.1 per second linear, 0.3 per second angular). Leave that. An undamped 800 kg ram will bounce the latch and the gate will chatter.

**Build:** a day. The wheel is built. Remove the handle and put the chock where the cab arrives.

## The backup

If a ride has already been spent, he goes up what that ride knocked down. Those climbs are already in the tests: the slumped tower, the fallen monolith, the spent wagon, the hanging jib, the spent cart. No new ladder, and no rebuild of the board-and-ladder route. From 640 m up, the backup is the same cage on its teeth. A spent cage still stops at a deck. He climbs nothing new. She uses that same wreck, so the backup is not a place she cannot go.

**Build:** short. It is the wrecks the tests already climb. Do not add rungs.

## What this does not copy

Below 340 m he already has the slingshot, the ram-hit swing, the girder he leaps along, the scale plate, the shoot bolt, the iron pig, the spool, and the chiller blast. None of the five above is one of those. The plate shop is a collapse he rides. The outside cab is a vehicle on the face. The reel is a fading counterweight. The gravel is a pour. The wheel is a wheel. She is on the floor with him, not left at the bottom.
