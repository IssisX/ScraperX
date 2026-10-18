# Active goal: progress on ScraperX-Claude

On 2026-10-09 the owner set the ten-phase goal, plus Phase 10, paid-release readiness. Its full
specification is `03_EXECUTION/ACTIVE_GOAL_PHASED_SCOPE.md` on branch `ChatGPT`. The owner chose to
pursue the same goal on this branch: *"Same goal, ScraperX-Claude"*. Read that file from `origin/ChatGPT`.
This branch's plans of record stay `PLANNING/MECHANISM_ASCENT_PLAN.md` and `PLANNING/RELEASE_PLAN.md`.

The percentages are rough estimates. Each one says what it rests on. A phase marked *not assessed*
has not been checked against its scope here.

| Phase | Estimate | Basis on this branch |
|---|---|---|
| 0 Preserve and set the frontier | 80% | Every route and repair below is kept and passing (the native suite and 55 touch scenarios, local, on `ea58a2b`'s tree). This file sets the frontier. |
| 1 Finish the current connection | 95% | The tram, stone wheel and slab incline are placed, played on touch and green in CI (`ea58a2b`). The one-run climb from grade to 803.5 m passes locally (`a4a7a19`); CI has not yet run on it. |
| 2 Responsive locomotion, momentum | ~55% | Assessed 2026-10-12 against the scope. Kept: the 85 kg dynamic body at 90 Hz, 5.5/8.0 m/s, 5.5 m/s jump, 22/14 m/s² controllers. Movement is resolved against the support's contact-point velocity, rotation included (`current_support_point_velocity`). A jump carries the run and the support's velocity, and only from valid contact: no coyote time (decided earlier against Governing Law 40). The air keeps excess speed it left the ground with (`air_full_speed_`). Fixed now: a released stick in flight (a jump, or no ground within a step below) no longer brakes the arc, where the old air control erased a running jump's speed in 0.4 s (native `landing` air case). A jump pushes off a dynamic support with the equal and opposite impulse at the contact (native `stack`: S2's cage dips 2.50 to 2.15 m/s). Ground traction now scales with the slope of the face under the soles (`ground_grip`). Open: traction from surface friction (the bodies' friction values were set for machine contacts, not grip) and from a moving support's load (it changed S1's start, which rides on the rider's weight); walking on a dynamic body applies no horizontal reaction to it; intent buffering is not checked against the scope. |
| 3 Continuous climb, mantle, vault, catch, swing | ~55% | Mantle closing and the two-phase vault are native. New (2026-10-12): the yard's trapeze east of the start (`src/sim/trapeze.cpp`). A running jump off its launch stand brings the hanging bar into reach; GRAB (held 0.33 s, native) closes the hands. The hands are a constraint on the body's centre of mass: fixed across the arm, a 2 Hz spring at the arms' straight stop (a 5.5 m/s catch is taken over ~0.1 s, ~100–145 J lost, ~3.4 kN peak), and a pull-only motor of at most 1.5 kN up the arm (Crouch). Past 3 kN for 0.15 s the hands slip. The body's mass swings with the bar through the solver. Pulled up through the bottom and let down at the ends, the swing grows (0.60 to 1.21 rad); against that timing it dies (to 0.31); held still it keeps its first swing. The ledger of the arms' work against bar and body closes within ~40 J. LET GO removes the constraint and keeps the velocity; near the far end of a pumped swing it carries the body onto the landing stand, which the run's own swing cannot reach. Proof: native `trapeze`, `touch_trapeze`. Ledge catches are compliant (corrected 2026-10-15; this row said otherwise): the hands close on the lip and the shoulders hang from them on a tension-only line, a spring at the arms' stop (the trapeze's) that takes a falling catch over about a tenth of a second, gravity on, and the hands slip past 4.5 kN held or tear past 350 N·s over the grip (`kLedgeGripMaxForceN`, `kLedgeGripTearImpulseNs`; MEASURED on the proving ledge: a 1 m fall holds, a 6 m fall tears). Bars and pipes are caught and swung pipe to pipe (the yard's gallery, C7). Open: the AS-012 swing is still struck, not pumped; mantles and vaults keep their prescribed paths. |
| 4 Impact recovery, rolls, stumbles, balance | ~65% | Native landing recovery (`kLandingFreeSpeed` in `simulation.cpp`): the legs take up to a 1.8 m drop free (3.3 m with crouch held). Beyond that, a balance debt from the normal energy lowers traction and reachable speed until it is paid back, never a lockout. A landing sliding across the facing adds a quarter of the tangential energy, which makes it a stumble. A later landing never forgives debt still owed: the larger is carried. A roll needs crouch held at contact, 2.5 m/s forward and a swept, supported lane. It keeps the momentum (2.7 m of travel against 0.96 m planted) and a quarter of the debt, and holds the body low even if crouch is let go. An obstructed lane is refused. The 20 m/s lethal rule is decided first. Edge balance uses the XCoM (`kTeeterComHeight`). Entered at a sprint with the XCoM past the edge, the brake along the motion falls to the soles' traction until the body stops or goes over. Past the XCoM no brake saves the body (the stop needs more than the v/omega left), so pulling back only buys time. "The edge" is where the ground stops running toward the XCoM, so slopes and stairs are ground. Beams (2026-10-12, `kBalanceHipMargin`): across the line the body is the same inverted pendulum, x'' = omega^2 (x - p), with the centre of pressure clamped to the beam plus 0.12 m of hips and arms. That replaced the 22 m/s/s centring, an invisible rescue force. A shove that keeps the XCoM within that reach is caught; one that does not goes off. The stick leans within it, and the view banks with the XCoM. Proof: native `landing`, `teeter`, `step2` beam pendulum; `touch_roll`, `touch_teeter`, and every route test that crosses boards. An independent adversarial review of this code confirmed 8 defects, all fixed (`e2fdf67`). Limits: a rigid capsule (the normal impulse is the solver's own). A support jerked under the feet (2026-10-12, `kSupportJoltMargin`): the soles re-match it only at the ground's traction, so a jolt's XCoM shift past half a foot takes a step, a stumble with debt. The crown gondola's swinging cabin does this to its rider (native `high`); smooth supports never do (moving-support truth). Done 2026-10-15: stumbles from a blow by something moving (see below). Not yet done: rotating footing under a beam, and a test of a roll blocked mid-lane (only a mover entering the swept lane can do it). |
| 5 Controls and tactile feedback | ~60% | Touch, pad and keyboard share one verb set. Haptics (2026-10-15) now carry the body's physics, each scaled from a native event or load (see below), coalesced at 50 ms on a monotonic clock, and gated by the vibration setting. Proof: `touch_roll`, `touch_teeter`, `touch_pipe_gallery` and `touch_machines` fail if their pulse is not sent. A hang torn off now drops and tips the view and is heard (a low scrape); a stumble scuffs; hauling the pitman's line heaves the view with the pull. REDUCED MOTION (Display) turns off the roll's turnover, head bob and the speed FOV kick, and quarters stagger, sway and body dips (`touch_roll_reduced`: the same roll, the view upright). Open: the phone (no vibration has been felt on a device, nothing seen on its screen). |
| 6 Coherent places and encounters | ~55% | Assessed 2026-10-14; raised 2026-10-15 for each band's own sound, Wet Isolation drawn wet and the Plate Shop's welders (below). Five of the eight supplied machines are now worked with the body, each by a different decision; nothing is pressed. The pitman lift (the rider's own work: hauled by hand on a 2:1 line against a counterweight, a dead-man brake; see below). The first exposed alternative: C7 climbs from the 264 ring to the mast–pitman walkway without the cascade mast (see below). The stone wheel (load fed: stone poured from the hopper). The slab incline (position and restraint: a trip plate pulls the slab's pin, a brake treadle holds or lets the trolley run; unbraked it strikes the head and kills). The traction tram (timing: a shuttle that waits 6 s and 4 s; missed at the head, it carries the rider back down). The luffing derrick (load removed: iron pigs thrown over the cradle's side until a chain-balanced counterweight wins; stepped off lightened, it leaves empty; CALL brings it back). Proof: native `incline`, `tram`, `derrick`, `high`; `touch_incline`, `touch_tram`, `touch_high`; CI green on `27cedca`. C7 now carries real pressure: wind off the west face pushes a climber off their feet out over the drop (task 91, below). The height is felt: a cloud layer at 455–505 m and a wind that grows with height (task 90). Open: the cascade mast, barrel helix and crown gondola are still board, press RAISE, wait (the rework waits on the owner); eleven stages from D to O still start on the spent hanging handle (decision 5 below); the incline's hop-on-hop-off braking may feel twitchy on a phone (not observed on a device); regions' identities beyond sound (each band now has its own ambient sound, below), more sheltered vs exposed alternatives (C7 is the only one) and the encounter-to-encounter consequences in the scope are not yet authored. |
| 7 The gravity-fed wheel | ~78% | Worked with the body (2026-10-09): the feed plank opens the hopper and holds the wheel, stone alone drives it, and the brake only resists. Proven: too little stone doesn't lift the rider, and a missed boarding can be retried (native `wheel`, `touch_wheel`). The stone falls under gravity with dust and a pour sound (2026-10-14). The finite hopper is reloaded by the rider's own work at the top (2026-10-15, below). Open: a closer look at the stone and its pile, the phone. |
| 8 Remaining supplied designs | ~70% | Eight of ten designs are placed and played on touch. The other two are deferred, each for a stated reason (the scope's rule; see "Phase 8, the last two designs" below): the gravity balance would repeat AS-006 A's counter-mass lift, and the rack climber is a motor ride whose kind waits on decision 1. The press-RAISE repetition is under phase 6. |
| 9 Verification and delivery | ~60% | The suite is proportional and CI builds a signed APK. No device run has been recorded. |
| 10 Paid-release readiness | ~30% | The unbroken route (no slingshot) is done. Save and continue is done (native `save`, `touch_save`). The title over the live tower is done (`touch_title`), and the START list is development-only. The top of the route is a native latch with an arrival beat, and the altimeter is scaled to it (what stands there is the owner's). The pause menu carries the licences the build ships under (`touch_pause`). The launcher icon and boot splash are the game's own, no engine branding (placeholder icon, owner may replace). The ending beyond the arrival, the release build, store identity, credits, the voice's clearance and device measurement are open (`RELEASE_PLAN.md`). |

**Frontier (2026-10-13): Phase 6, the remaining RAISE machines.** The incline, tram and derrick are worked with the body (row 6). Next: the cascade mast, pitman lift, barrel helix and crown gondola, each by a decision not used yet (alignment, route, or the rider's own work), then the places around them.
continue, the title, the top and the licences are done (see the rows). The landing, roll and edge code was
reviewed and repaired, and beams now balance by the pendulum. Next: stumbles from support acceleration and
off-axis impacts, then phase 6's machine-control repetition.

**Owner decisions (2026-10-14).** Movement should feel acrobatic and primate-like. A grappling hook
comes later, as a toy like the slingshot (it may skip things; it is never the intended route), found as an
extra about 25-30% up the structure (around 200-240 m) where finding it feels like a reward. Order: the
draw-call repair and a build first; then the parkour overhaul (physical ledge catches and mantles,
brachiation along bars and pipes, slides and speed vaults, wall-runs), then the hook.

**Parkour step 1, physical ledge catch (native).** A hang is now held by the arms: a tension-only
spring line from the hands on the lip to the shoulders, with gravity on. The body swings under the
hands with the momentum it caught them with. A swing let go keeps its velocity. The grip is finite:
a catch after a 1 m fall holds (peak ~10.5 kN, ~170 N*s over the grip); a catch after a 6 m fall tears
(~18 kN, ~700 N*s over; the tear threshold is 350 N*s). The mantle from a hang is unchanged. Native
falsifiers are in the hang test (`HangHighDrop` spawn, `hang_slip_count`). There is no Godot
read-out of a torn grip yet.

**Parkour step 2, brachiation (native + Godot, proving ground only).** A bar or pipe caught overhead
in flight hangs the body from the same physical arms (new traversal state Swinging). Pressed along the
swing it kips (≤160 N, in phase only). The stick let go stills the swing. Held forward, the free
hand takes the next hold ahead within 1.0 m of the shoulders: bars a metre apart need a swing.
DROP or JUMP flies on with the swing's velocity. Proven on a six-bar fixture: native `brachiation`
(five bars crossed in 1.92 s, no skip, no snap, a fling onto the landing block, a still hang reaches
nothing until pumped) and touch `touch_monkey_bars`. **In the game:** the pipe gallery in the yard
south of the trapeze (`sim/pipe_gallery.cpp`). Ten pipes at 4.6 m from a 1.5 m start stand to a 4.7 m
platform whose lip the swinging hand takes; swinging onto a ledge is now part of the move. Played
natively (`pipe_gallery`) and by touch in the game's world (`touch_pipe_gallery`), with rendered
captures. Not yet in the tower itself.

**Arms overhead (Godot).** The camera's near plane was 0.25 m, so hands at the face were clipped away.
It is now 0.05 m; the far views keep their draw counts and change only along near shadow edges.
Overhead grips place the shoulders so whole sleeved arms rise from the lower corners. A bar has its
own fist pose, and the view pitches to keep the grip in frame while swinging. The hang and swing
touch scenarios fail if a hand leaves the screen or an arm comes within 0.15 m of the lens. The
hands themselves are still crude primitives.

**Parkour step 3, the slide (native + Godot).** Crouching at a run (5 m/s or more) slides on, braked
only by 0.35 g of friction, and passes under the 1.45 m crawl beam. The native `slide` test checks the
deceleration against the friction; `touch_slide` plays it by touch. Limitation: a slope brakes like
the flat.

**Parkour step 3, the speed vault (native + touch).** At 6 m/s or more:
- the vault is offered a quarter second's travel ahead (up to 2 m);
- it crosses at the run's pace and holds its apex past the far edge;
- it then completes into free flight at the run's speed.

The fixed 0.38 s crossing used to take a sprint from 8 m/s to about 1 m/s over the rail. Native
`speed vault` and touch `touch_speed_vault` prove it.

**Parkour step 4, the wall-run (native + touch).** In flight from a jump, running along a wall, a foot
plants on it as the body comes down, at most 3 times. Each step pushes off by 1.2 m/s, and the wall's
friction (0.8 of that) resists only the sole's slide down. Steps slow the fall but never lift the
body: about 0.14 s more airtime than the same jump away from a wall. Jumping on the wall kicks off it.
Proven on a proving wall (native `wall run`, touch `touch_wall_run`). Not placed in the tower yet.
**Parkour step 5, the grappling hook (native + touch).** A toy, not the route:
- **Where it is.** A gun on a rack at the 220 ring's north-west corner. ACTION takes it (TAKE HOOK).
  Taking it parks the gun's body, and a save keeps it.
- **Firing.** HOOK (touch), G (keyboard) or the right shoulder (pad) fires along the view. The hook
  is a 1.5 kg body at 40 m/s on a line of up to 30 m.
- **Biting.** It bites only on holds (bars, pipes, rungs). Plain walls and the sky do not catch it,
  and it comes back.
- **Reeling.** Holding HOOK reels the line in at 3 m/s while its tension is under 1.2 kN. A body
  hung too heavily stalls the reel instead of being pulled up faster. DROP lets go.
- **Proof.** Native `hook`: it lifts the body off the deck and climbs no faster than the reel turns.
  It also checks no bite on sky or wall, the game-world pickup, and keeping the hook through a save.
  Touch `touch_hook` takes, aims, bites, reels and drops through the touch layer.
- **Drawing.** While the hook is out, the right fist closes on the line, held out toward it. The line
  leaves the top of the fist (`touch_hook` checks the pose; a rendered capture was inspected).

Old saves load: the kit's body list in a save may be shorter than the current world's (the pipe
gallery and the hook rack added bodies, built last), and the missing tail keeps its current state.
No test yet loads a save written before them.
Next: place the wall-run and brachiation in the tower.

**Life L1, birds (Godot, presentation only).** The world had no living thing in it. Now:
- **Pigeons.** Flocks of 4–8 stand on real surfaces near the player. Each bird's footing is the native's
  first surface below it (`get_surface_below`, a read-only ray that ignores the player). They peck,
  turn and hop.
- **Flushing.** A flock flushes when the player comes within 4.5 m, runs at it from 9 m, or lands hard
  within 14 m. It rises with a clatter of wings, wheels round and comes down on another real surface.
  If no surface is found, it flies off.
- **Gulls.** Above 60 m, gulls circle at the player's height.
- **Cost.** All birds are one MultiMesh (one draw call), wings beat in the vertex shader, and the
  native probes are capped at 10 a frame.
- **Proof.** `touch_birds` runs at a flock: it flushed 8.9 m short of the player, rose 3.5 m and
  landed twice. Rendered captures were inspected.
- **Not covered.** No proof of the gulls, and no device measurement.

**Life L2, the yard crew (native + Godot).** Six workers on the grade between the start and the
tower's base (GDD §19, Inhabitants):
- **The crew.** A foreman walking the yard, two at the slingshot, a rigger at the base, a runner
  between start and base, and someone watching the trapeze.
- **Real bodies** (`sim/crew.cpp`). Each worker is an 80 kg upright capsule in the Jolt world, moved
  only by its feet. Its acceleration is bounded (2.5 m/s², 4.5 m/s² when sidestepping) and it does
  not drive while airborne. Workers step over the yard's 0.28 m kerbs with a lift from the feet.
  Their routes keep to lanes mapped clear of machines with the native's own probe. Nothing
  teleports them, and they are not saved.
- **Around the player:**
  - A walking player coming at a worker makes it step aside, earlier the faster they come. It
    watches them pass, then goes back to work.
  - A sprint at it, or a hard landing within 12 m, makes it flinch back and stare.
  - A shove its feet did not drive makes it stumble, then recover.
- **Drawn** (`life/crew_view.gd`). Boots, trousers, a hi-vis vest with a reflective stripe,
  sleeves, hands, a head and a hard hat, posed from the native state: walking, working (a
  hammering arm), watching, flinching and stumbling. The whole crew is 11 MultiMesh draw calls,
  4 of them casting shadows.
- **Proof.** Native `workers`:
  - 30 s with the player idle has no shove, startle or yield; nobody walks faster than 1.5 m/s,
    and both walkers cover more than 20 m;
  - walking at the foreman makes him yield with no shove, and he resumes;
  - a hard landing makes someone flinch;
  - sprinting into a rigger makes him stumble, and he recovers.
- **Touch.** `touch_workers` sees six workers and walks at the foreman; he steps aside and nobody
  is shoved. Rendered captures were inspected.
- **Gaps:**
  - The crew has no voice (footsteps, tool strikes and faces were added after this note).
  - The player's ledge, vault and wall probes do not yet ignore workers, so a worker could in
    principle be climbed.
  - There were no workers in the tower's bands yet (see Crews on the decks below).

**Life L3, the structure at work (Godot, presentation only).** The world's machines and stacks
show that they are running (`life/works.gd`):
- **Beacons.** Every kit machine of 250 kg or more (65 of them) carries an amber beacon on its top.
  It turns and flashes while that machine actually moves (its speed sampled each frame, as the audio
  hears it) and for 2.5 s after; a still machine's beacon is dark. All beacons are two MultiMesh
  draws.
- **Steam.** The tower stack's vent stacks breathe, each on its own 7 s rhythm. A pool of four
  emitters follows the vents nearest the camera.
- **Proof.** `touch_works`, beside the traction tram: 65 beacons, 4 lit while the machines moved,
  and a vent steaming in sight. Rendered captures were inspected.
- **Gaps.** The existing vent stacks are all on the stack's back face, so no steam is visible from
  the start; a climber sees it. There are no lit windows or moving cloth yet.

**The crew heard and faced.** Each walker's footfalls sound where they land, and each working
arm's strike sounds as it comes down, within 30 m. Heads have eyes and a brow. `touch_workers`
fails if the crew is silent: 8 steps and 4 strikes were heard in its run. These are measured
counts, not listened to.

**Crews on the decks.** Three more workers in the stack itself, on its ring decks, with lanes mapped
clear with the probe and kept off the route's south band:
- deck 2 (22 m): a walker on the west band, stopping to look out, and someone at work on the north
  band;
- deck 4 (44 m): a walker on the east band.

The native `workers` test checks all nine: each on its own floor and each walker covering its
route, with no shove.

**The crew higher up (2026-10-15).** Three more workers stand on the high rings, so the climb is
not empty above 44 m:
- One walks the 220 ring's north band and stops to look out north.
- One walks the 264 ring's north band.
- One works on the 264 ring's east band.

The bands were mapped with the native surface probe (each is a 4 m band round the well). The
routes keep clear of the swing's gangway, C6's arrival, C7's lane and the cascade mast.

- **Native `workers`:** all twelve stand on their own floors, and the new walkers cover their
  routes.
- **Touch:** `touch_workers` sees twelve figures. `touch_slingshot`, `touch_swing`, `touch_c6`,
  `touch_c7` and `touch_machines` still pass with the new crew in place.

Next for life: a shout when a worker is startled (needs a licensed recording).

**Phase 6, the pitman lift is hauled by hand (native + touch).** It was "board, press RAISE, wait";
now no motor turns it:
- **Counterweights.** A 1,667.5 kg counterweight on a rope over two head sheaves balances the deck,
  the rod's deck-side half and half a rider. A rope over a sheave balances at every height; the
  crank's own pin-and-rod weight would not have, because the pitman's lift is lopsided (1.66× near
  the bottom, 0.34× near the top). A 554 kg block on the crank opposite its pin cancels the pin
  and the rod's other half.
- **The haul.** On the deck, ACTION held (HAUL) pulls a hand line rigged as a gun tackle: each
  newton at the hands lifts two. The hands pull at most 700 N, falling to nothing at 2.2 m/s of
  line (a human force-speed limit, CHOSEN). The 2:1 is applied as an upward force on the deck; the
  rider's own unweighting is folded into it.
- **Brake and calls.** Let go, a brake on the deck's guide (30 kN) holds it. From the lower
  receiver the same hold hauls the deck down on the call line, reeved through the same tackle
  (2:1). From the upper receiver it frees the brake, and the empty deck (light by half a rider)
  rises at a governed 0.6 m/s.
- **Placement and saves.** RAISE does nothing to it. The counterweight and its rope are built last
  in the kit, so older saves' lists still line up.
- **Proof.**
  - Native `pitman`: RAISE does nothing; hauled halfway and let go, the deck holds within 4 mm for
    4 s; the deck never outruns the hands (fastest 0.56 m/s); the haul from the lower to the upper
    receiver takes 53 s.
  - Touch `touch_machines`: hauled by holding ACTION, mast to pitman to helix to TP-340, with the
    right fist checked closed on the line while hauling.
  - Native `pitman`, the calls: the deck is hauled 0.3 m up, or to 0.24 m short of the top, and
    left empty. The brake holds it for 3 s. Held from the lower receiver, the call line brings it
    down; freed from the upper receiver, it rises to the receiver. The first version pulled the
    call line 1:1. That left about 130 N net against several tonnes of deck, counterweight and
    crank, so a full-height call would have taken minutes. Reeved 2:1 it nets the same 833 N as
    the haul up, so a full call should take about as long as the haul (derived, not run).
- **Not tested:** a full-height call or brake release; a rider can only leave the deck at a
  receiver, so the tests start from short of one.
- **Feel.** The 53 s haul may be long for a phone.

Next: the cascade mast, barrel helix and crown gondola, each by a different decision.

**Phase 7, the stone wheel's stone (Godot).**
- **The fall.** Poured stone now falls under gravity from the spout: 56 tumbling stones a stream
  (it was 18 at a constant 6 m/s). They speed up, string out and spread as they go.
- **Dust.** A soft dust plume rises where each stream lands.
- **Sound.** A positional pour sound (a dense rattle of small stones over the low rush of the mass)
  plays from the middle of each fall while it pours.
- **Proof.** `touch_wheel` fails if stone pours and nothing is heard. It passed.
- **Not observed:** the new stream up close. From the plank the drop is about 2.7 m at about 20 m,
  too small in a capture to judge.
- **Still open:** reloading the finite hopper, and a closer look at the pile.

**The hook's gun (Godot).** Once taken, the right fist carries a compact launcher: a steel body
over a grip, a yellow reel drum, a barrel and the hook's claw at the muzzle. The line leaves from
the muzzle. Hauling the pitman's line, the fist holds the line instead. `touch_hook` fails if the
gun is not in hand after the take. Rendered captures were inspected after a fix: the first was
drawn too large near the lens, so it is drawn at 0.62 of its model and the arm holds the line
further out.

**Phase 6, C7: an exposed way past the cascade mast (native + touch).** From the 264 ring's west
band to the mast–pitman walkway at 291.25 m, 27 m up, with no machine. The mast stays the sheltered
way; C7 goes out over the drop on the tower's west face. All of it is static steel and concrete in
one kit body (`src/sim/climb_c7.cpp`, built last so older saves line up):
- **The kentledge.** A concrete block on the band, 1.6 m to mantle. Its striped north end is the
  take-off.
- **Gallery one.** A running leap from the stripe catches the first of five pipes 3.1 m up. Forward
  held, the hands swing pipe to pipe to platform one's lip.
- **The outrigger.** A hang-up from platform one's north edge onto a deck hung out west over the
  drop from a cantilever.
- **Gallery two.** From the outrigger's west end, a standing leap to the first of eight pipes. Each
  is 0.95 m on and 0.3 m higher than the last, so the swing climbs. It ends at platform two.
- **Platforms three and four, and the 286 ring.** Three hang-ups of about 3.25 m, east, north, east,
  each onto a painted fascia hung from the deck's edge.
- **The block and the walkway.** South along the 286 band to a block (a mantle), up its 0.3 m step,
  and a last hang-up onto the walkway's south edge.
- **Proof.**
  - Native `c7`: the climb on player inputs in 34.6 s, every pipe of both galleries swung (4 + 7
    reaches), standing on the walkway at 292.17 m, alive, with the mast never moving.
  - The falsifier: from the band under platform one, a jump at its lip catches nothing. The
    galleries are the way up.
  - Touch `touch_c7` (in CI): the same climb through the touch controls in 41.6 s, with the body
    and the view checked for jumps each frame.
- **Repair on the way.** The last mantle stalled at 291.07 m. A 5 cm gap between the walkway's edge
  and its fascia took the ledge point onto the fascia's lip strip, not the walkway. The fascia now
  sits flush with the walkway's edge.
- **Rendered (local, 540×464, software renderer).** Six frames from a rendered `touch_c7`
  (passed, 41.7 s):
  - From the kentledge, gallery one's pipes and platform one's yellow lip read as the way ahead.
  - On gallery two both fists are closed on a pipe, with the platform's lip beyond.
  - On the walkway the checkpoint is secured at 292.2 m.
  - Seen in passing: the distant mountains are flat grey cones with white caps. They look cheap
    against the bar and belong in the look pass (task 93).
- **Not observed:** whether a first-time player sees the galleries as the way from the band, and
  the phone.

**The look: the basin's mountains (Godot).** The peaks were 9-sided cones with a white cone on
each tip. They went through the static batcher, which recolours every vertex to its material's
colour and draws far sections in one flat material, so they read as pale grey tents.
- **Collision is unchanged.** The cones stay in the scene, hidden, because they are the world's
  solid (`solid_export.gd` reads them). The regenerated `world_solids.inc` is byte-identical.
- **What is drawn.** A carved peak stands on each cone's foot (`godot/presentation/mountain_mesh.gd`).
  - **Shape.** Ridged noise cuts ridges and gullies down the flanks; the summit is ragged and the
    profile slightly concave. Nothing stands proud of the cone by more than 3% of its radius.
  - **Snow.** It comes lower in the gullies.
  - **Haze.** Distance lifts the rock toward the horizon's haze. The haze is part of the lit
    surface, so the slopes still go dark at night.
- **Cost.** There are eight carved variants, each drawn as one MultiMesh with the rock tint and
  snow carried per instance. Worst draw calls at LOW from the top (`perf_top`) are 389, against
  398 recorded before.
- **Rendered (local, 540×464).** On the title, the ridges now read as layered, hazy rock behind
  the tower instead of pale tents. From the 751 m deck the far peaks read as snowy pyramids in
  the haze.
- **Not observed:** sunrise or night, the higher graphics presets, and the phone.

**Phase 5, the body in the hands (Godot + bridge).** Haptics fired only for grabs, vaults, mantles,
landings, deaths and the fall warning; the balance profile was defined and never used. Now each
of these sends a pulse, scaled by the native's own numbers:
- **A stumble:** scaled by the balance the landing took.
- **A roll:** one long, soft pulse.
- **An edge teeter:** a pulse as the XCoM passes the footing, and every 0.3 s while it stays there.
- **A beam:** corrections from |XCoM| 0.5 of the hips' reach, faster and harder toward 1.
- **The hands:** a hang torn off by its load, or a trapeze slip; a reach from bar to bar; letting
  go of a bar into free flight. The bridge's new `get_hand_state()` exposes the hang-slip, catch
  and reach counts.
- **The pitman's line:** strain while hauling, scaled by the pull in the hands.

Pulses closer than 50 ms are dropped, except landings, deaths, warnings and slips. Every pulse is
counted in `haptic_counts`, in CI too, though there nothing is sent. `touch_roll`,
`touch_teeter`, `touch_pipe_gallery` and `touch_machines` fail if their pulse is missing.

**Not observed:** any of it in a hand. No device has run it.

**Phase 2, walking push-back: tried and taken out (2026-10-15).** Making the feet push a dynamic
support back horizontally, the equal and opposite of the change they make to the body over it,
was built and passed the S2 cage test (503 N·s pushed against 466 N·s gained). It broke the
summit route, so it is not in:
- The luffing derrick's cradle hangs free. Pushed by its rider's steps, it moved while iron was
  thrown off it at the bottom, and swung away from the upper receiver as the rider stepped off.
- `touch_stack`, `touch_high` and native `high` failed.

The fix belongs to the derrick. Its cradle must seat against a stop at each receiver, as real
cradles do, so a step pushes the frame rather than swinging the cradle. Then the push-back can
return. (S1's balance check also moved by about 1 kg with it; see that test when it does.)

**Phase 4, struck by something moving (native).** A moving body coming at the player across a side
contact (its own velocity toward the player at 0.5 m/s or more) now reaches the player's balance.
What the blow gave the body since the feet last set its velocity joins the support-jolt reckoning:
an XCoM shift past half a foot (0.15 m) is a stumble, with debt.
- **What doesn't count.** Walking into a wall, or into anything standing still, is never being
  struck. A first version counted the player's own closing speed and stumbled a rider walking into
  G's cleat; the body's own motion is now the test.
- **Native `workers`:** walking across the yard (kerbs, walls) is never a shove. Standing in the
  runner's lane, the player is walked into at 1.45 m/s by an 80 kg worker and stumbles (3 blows,
  one stumble, balance loss 0.12) and lives. The whole native suite passes with it.
- **Haptics.** The stumble pulse and scuff already follow every stumble.
- **Not observed:** a machine part (a ram, a swinging cabin) striking a rider in play.

**The height felt (Godot): a cloud layer and the wind (task 90).**
- **The clouds.** A broken layer lies between 455 and 505 m, partway up the climb: 42 clouds of
  four soft lobes each, with lit tops and shaded flat bases. They drift slowly and wrap round an
  annulus about the tower, kept 75 m clear of its axis so no cloud cuts the structure. From the
  yard they are a ceiling overhead around the tower. The climb through AS-008's band passes them
  at their own height; from above they lie below. A cloud fades out as the eye comes into it.
  One MultiMesh draw (`perf_top` LOW worst 390, against 389 before).
- **The wind.** It kept growing only to 150 m. It now keeps growing to 600 m, drops in pitch with
  exposure, and comes in slow gusts that deepen with height.
- **Proof.** `touch_clouds` (in CI) checks that the layer exists and that a cloud lies within
  200 m of the climb's axis at 480 m. Rendered locally from the yard looking up, the clouds read
  as broken cumulus round and above the tower. The first version (single crisp ovals) read as
  white petals and was redone.
- **Not observed:** the view from inside or just above the layer, overdraw on a phone, and the
  wind by ear.

**Pressure from the world: wind on C7's face (native + Godot, task 91).** C7 was the exposed way
past the cascade mast with no exposure but the drop. Now the wind off the tower's west face pushes
a climber out over it.
- **Where.** West of the face (x < -26.6, rising over 2 m), between 262 and 300 m.
- **How hard.** Outward (-x) by drag, 0.5 rho CdA v^2 (rho 1.2, CdA 0.7 m², CHOSEN). The speed
  swells between 4 and 16 m/s on two incommensurate periods of the simulation's own clock, so it is
  deterministic: the same inputs give the same gusts, never long calm, and now and then both swells
  at once give the full gust, about 107 N, an eighth of the body's weight.
- **Who it pushes.** A body off its feet: on the pipes, in a leap, hanging. Standing, the soles take
  it.
- **Heard.** In the zone the wind heard follows the native wind (the bridge's `get_wind_mps()`).
- **Proof.** Native `c7` climbs it in 34.63 s (34.61 calm) and fails unless the wind pushed the
  climber (peak 84 N). Native `C6` fails if any wind reached its climb, which is below the zone.
  `touch_c7` and `touch_machines` pass.
- **A dead end on the way.** A first cut swelled once every 11 s; C7's whole gallery-two swing fell
  in its calm. Real wind is not still for six seconds.
- **Not observed:** how it feels on a phone, and whether players read the gusts before they leap.

**The first sight: the title (Godot, task 93).** The title's view drifted only between about 0.37
and 0.47 rad up the tower's foot. It now sweeps slowly over 76 s from the foot up to where the
tower climbs into the cloud layer, and back, so the first thing seen is how far it goes.
- **A fix on the way.** The clouds were updated from the main loop, which the title pauses, so
  they were never placed behind it. They now update themselves, and drift behind the title and the
  pause menu.
- **Proof.** `touch_title` passes and captures both ends of the sweep.
- **Rendered locally (540×464).** At the top of the sweep the tower rises into broken cloud;
  several near puffs stretch at that steep angle.
- **Not observed:** the phone.

**Phase 7, reloading the wheel's hopper (native + touch).** The hopper held 16 t and was never
refilled. It now starts with 6 t of its 16. Over it, a skip of 8 t stands on four posts on the
hopper's rim, its mouth under a gate like the hopper's own: hinged, shut by its counterweight.
- **The line.** A trip line runs from that counterweight up over a sheave, across, and down to a
  yellow handle at the upper receiver's north-east corner (GRAB SKIP LINE).
- **Pouring.** Taken and pulled back until taut, the line lifts the counterweight. The gate swings
  open and the skip pours into the hopper (250 kg/s; a bin pours into the first bin under it).
  Let go, the counterweight shuts it.
- **The finite part.** The supply is still finite: the skip holds 8 t. Refilling is work done at
  the top of the ride, for the next ride.
- **Built late.** The skip (2976), its gate (2978) and the handle (2979) are built after every
  other body. Bins added since a save now start as built when it loads, as bodies already did, so
  older saves still load.
- **Fixed on the way.**
  - The gate's counterweight overlapped the skip's floor; the contact flung it open at once.
    They are now kept from colliding.
  - The mouth was over the frame's arm that carries the hopper, so the stream landed on the arm.
    It is now in the skip's front half.
  - Stepping back a full metre tore the handle from the hands: the gate's stop lets out only about
    0.3 m of line.
- **Proof.**
  - Native `wheel`: untouched for 3 s, the skip keeps its stone. Taken and held 10 s, the hopper
    gains 2,583 kg and the skip loses the same, kilogram for kilogram. Let go, the pour stops.
  - `touch_wheel`: GRAB SKIP LINE at the top, a step back, at least 500 kg poured, LET GO.
- **Rendered (local, 540×464).** Holding the line, both fists are closed on the yellow grip and
  the line runs up to the gantry. The first grip was a 0.44 m block that filled the lower view; it
  is now 0.32 × 0.24 m.
- **Not observed:** the skip pouring, seen from the receiver; the phone.

**Phase 6, each band heard as itself (Godot).** Apart from the wind, the tower sounded the same
from bottom to top. Each band now has its own one-shot, played now and then somewhere around the
listener (`audio_director.gd` `REGIONS`). A band is heard soon after it is entered.

| Band | Sound | Spacing |
|---|---|---|
| Wet Isolation (220 m) | water dripping, 4–14 m away | 0.9–3 s |
| Plate Shop (340 m) | plate clanking far off, 18–55 m | 3.5–9 s |
| Facade Crane Stack (484 m) | cables creaking overhead | 2.5–7 s |
| Machinery bands (640 m and up) | a low boom through the frame | 5–12 s |

- **The drip.** A new clip, synthesised like the rest: the tick of the drop, then the bubble it
  traps ringing up in pitch, and a duller echo off the walls. Measured on the three variants:
  96–100% of the energy sits at 1.2–1.6 kHz, which a phone speaker carries; the peak is 0.9.
- **Seeding.** The clip is built last, so every other clip draws the same seeded noise as before.
- **Proof.** `touch_wet`, `touch_shop`, `touch_crane` and `touch_c4` fail if their band played
  none of its own sounds. Under CI's dummy audio driver, that proves the scheduling only.
- **Not observed:** the mix heard through a speaker, here or on the phone.

**Phase 6, Wet Isolation looks wet; beacons no longer dazzle their riders (Godot).**
- **The wet core.** The Atlas makes B03 a band of live water: a header, a sump, a stair it can
  flood. Its standing structure (219–341 m) is now drawn wet.
  - Porous surfaces (rust, concrete, timber, paint) darken to 0.8 and take a film: roughness
    × 0.35.
  - Bare metal (metallic ≥ 0.5) keeps its colour and gains only a little gloss (× 0.85).
  - Done in the static batcher, so it costs a handful of extra batches. Moving machines keep
    their own finish: they travel between bands.
- **Two tries that failed.**
  - All surfaces at 0.62 darkened the rust wall of E's shaft to near-black.
  - Wet metal went black with white glitter on C7's walkway. The sky is not mirrored (it turned
    the asphalt white), so a glossy metal shows only pinpoint sun glints off its bump map.
- **The beacons.** A machine's amber beacon sat in the middle of its top, which is where its
  rider stands. Riding E's cab, its glare filled the lower half of the view (seen in
  `touch_wet`'s frame at 299 m). It now sits on a corner of the machine's highest part, set in
  0.15 m. Its blades and glow fade within about 2.5–7 m of the eye, so it is still seen from
  across the well.
- **Rendered (local, low resolution).**
  - `touch_wet` at 257 m and 299 m: the glare is gone.
  - `touch_c7`: the rust reads as wet steel, and the walkway is no longer black glitter.
- **Not observed:** the phone; the band at night.

**Phase 6, the Plate Shop at work: two welders across the well (native + Godot).**
- **Who.** A welder on the east band of the 374 ring and one on the 418 ring, across the well
  from G, H and I. Each is a real 80 kg crew body (now 14 in all). It works two seams in turn,
  crouched 0.7 m from the band's open inner edge, visor down, an electrode in the right fist.
- **The arc.** It burns in bursts (3.4 s in every 5). Sparks spill over the edge and fall down
  the well. A blue-white light flickers on what is round it. A crackle with the welding set's
  buzz is heard within 70 m. Presentation only.
- **Defect found and fixed: workers walked off edges.** A worker stepping aside for a passing
  player went 1.1 m sideways with no look at the floor there. Next to the well, the 374 m welder
  stepped in and fell 33 m onto TP-340 (native `workers`, reproduced before the fix). The crew's
  feet now never drive toward a spot without floor within 0.5 m below. A sidestep with no floor
  holds its ground. A shove can still carry a worker over: that is physics.
- **Defect found and fixed: the high crew was not drawn.** The crew's parts were culled by a box
  around the yard (y −2 to 8 m). So the workers on the 22/44 m decks and the 220/264 m rings
  were dropped whenever the yard was out of view. The box now covers the structure, and
  `touch_workers` fails if any worker stands outside it.
- **Proof.**
  - Native `workers`: 14 workers, each on its floor. Walked past on the outer side, the welder
    yields and stays on its ring.
  - `touch_shop`: a welder's arc burns within drawing range during the climb, and its crackle is
    assigned within earshot.
- **Rendered (local, 540×464).**
  - From G's platform (`touch_shop`'s capture-only `shop_welder` pose), the welder is a small
    hi-vis figure across the well with a spark trail.
  - Up close (a throwaway local scenario, not committed): crouched at the edge, the electrode
    to the seam, a blue-white pool of light on the deck, and sparks thrown over the edge. The
    first spark spray (56) was too thin; it is now 120.
- **Not observed:** the phone; the crackle through a speaker.

**Saves refuse a world laid out otherwise (native).** A save held each machine body's state in
build order, with no record of which body it was. A world with a body removed or moved would
have loaded every later state onto the wrong body, silently. A save now carries the bodies'
entities in order, and a load is refused unless this world begins with the same ones. Bodies
built since (always last) still start as built. The format key moves to SXCLIMB2.
- **One reset.** Saves written by earlier builds are refused once. The game already sets a
  refused save aside as `.bad` and starts a new climb ("SAVE UNREADABLE").
- **Proof.** Native `save`: a save whose first two bodies' entities are swapped is refused and
  changes nothing. With the check taken out, the same test fails.

**The first sight: the yard's ground and the skyline (Godot, task 93).**
- **The ground.** The yard was one flat, dark plane under a kerb and a few dashes. It is now
  asphalt that has aged (`yard_ground.gd`): broad worn patches gone grey, darker repair
  patches in blocks, the aggregate's grain, tar-sealed cracks where it has broken up, and oil
  stains.
  - Drawn from four noise textures read at each pixel's world position: no cost per frame.
  - Its average colour is the old flat one's, so the light is unchanged. The collision is the
    same box.
- **The skyline.** From the yard, the basin's rim read as a row of teeth: 44 similar cones at
  even spacing. A higher range now stands beyond it, out of reach: 22 peaks, 680–1100 m tall,
  1.9–2.4 km out. Each keeps its foot outside the rim's own, and is drawn only (nothing can get
  there). The rim's collision is unchanged.
  - The haze eases (0.0009 to 0.0007 per metre, at most 0.6 of the way to the horizon), so the
    near rock keeps its contrast and the far range reads paler and bluer.
- **Rendered (local, 540×464).** `touch_workers` looking south from the start: worn,
  stained ground, and a layered skyline behind the rim. `touch_title` unchanged in framing.
- **Not observed:** the phone; the yard at night.

**Ropes no longer drop their load for a tick (native, every rope machine).** Adding the stock
yard's solids (below) failed O: its rope read no tension for one tick, the cab fell free at g for
11 ms, and the next tick snatched it at 24 kN, twice its load.
- **Cause.** Every kit rope and trip line was Jolt's pulley constraint. Jolt solves one only in a
  step that begins with the rope at or past its length. A loaded rope sits exactly at its length,
  and 735 m up, single-precision positions put that length in steps of 6.1e-5 m. A taut rope read
  one step short was left out of the whole tick. Which ticks did turned on rounding, so any change
  to the world could move them: the yard's solids, 700 m below, did.
- **Fix.** The kit's ropes and trip lines are now the kit's own constraint
  (`src/sim/rope_constraint.hpp`). It is solved every step, speculatively, as Jolt solves contacts:
  a slack rope may run out the slack it has within the step and no more, so it takes up its load
  in the step it comes taut. The arithmetic is Jolt's, plus that one bias.
- **Proof (native).** O with the stock yard: lowest rope tension 11,528 N (it was 0), as it was
  before the yard. All 100 native tests pass; 47 moved within their bounds. S1's cage now leaves the
  yard with 243 kg of water in its bucket, 8 kg past its 235 kg balance; it took 278 kg while its
  rope flickered. O's ride is 1.1 s shorter. The snatch when AS-006's falsifiers drop a load is
  lower (A's bollard 62.6 to 37.7 kN, B's cleat 113 to 72 kN). A save's original now runs 0.17 mm
  from fresh loads of it (bound 1 mm): ropes keep their last impulse, which a save does not carry.
- **Not observed:** any rope machine ridden by hand or on the phone since the change.

**The first sight: stock in the yard (Godot, task 93).** South of the start, the grade was empty.
- **What stands there.**
  - On the west, a block of shipping containers, two high in places.
  - On the east, steel stock: beams on sleepers, plate, drums on pallets and a pipe rack.
  - 95 m south of the start, a row of seven containers, two high in three places, closes the yard.
  - The containers are ribbed (a corrugation normal map) in five paints. Each has the four locking
    bars of its doors.
- **All solid.** The export adds 69 bodies (`world_solids.inc`: 1,059 boxes, 641 hulls). The door
  bars, 5 cm and flush, are drawn only. All of it is clear of the start, the proving ground's
  fixtures and the crew's routes.
- **Rendered (local, 540×464).** `touch_workers` from the start: the container row stands on the
  horizon line, the west block to the left.
- **Not observed:** the phone; any of it climbed (no scenario climbs the containers).

**The first sight: mountains that read as mountains (Godot, task 93).** From the start the basin's
peaks read as pale, near-identical cones.
- **Why.** Their shader added its own distance haze on top of the scene's fog, so the rim, about
  1 km out, was some 44% haze where the fog alone gives 20%. And every carved peak kept its cone's
  outline: the carving cut gullies but never the silhouette.
- **Now** (`mountain_mesh.gd`). Each peak is a height field carved down from its cone, never above
  it.
  - The top is a crest that rises and dips to a summit and lesser tops; gullies cut the flanks;
    rock bands step the faces.
  - The lowest 12% of each peak, the part a walker can reach, is the cone itself, so the rock seen
    there is the rock that stops them. Checked over every vertex of all eight variants: none
    above its cone, and the foot exactly on it.
  - Colour comes from height and slope: scrub low down where it is not too steep, rock in bands,
    scree on benches, snow above each peak's snowline (lower in gullies) only where it can lie.
  - The fog is the only haze. Peaks scale uniformly, so the lighting's normals are true. The four
    far cones in the basin now share the 0.62 width-to-height ratio, so their drawn feet are their
    solid feet (`world_solids.inc`: four cones narrower).
- **The yard's oil stains** were 5–10 m hard-edged black blobs that read as holes. They are now a
  metre or two across, soft-edged and lighter.
- **Rendered (local, 540×464).** `touch_workers` from the start: dark ridged rock with snow on the
  higher tops, scrub at the feet, the back range layered paler behind. `touch_title`: ridged slopes
  behind the tower.
- **Not observed:** sunrise, night, the phone.

**The clouds, looking up (Godot; the owner's report from the phone).** Looking up from the ground,
the owner saw pine-tree shapes inside the clouds and the sky sheared into straight-edged bands.
- **Not reproduced here.** The owner had graphics near LOW. The desktop renderer is clean at
  HIGH and at LOW (0.75 scale, no MSAA, no bloom), by day and at dusk, with the old clouds and
  the old mountains as on the phone. The dusk peaks there are the flat brown cones in the owner's
  shot, so the build and the hour match; the bands and the trees do not appear. They belong to
  the phone's GPU. `touch_workers` now also captures three views straight up.
- **Fixed what can make flat bands on any GPU.**
  - The puffs' noise hashed with `sin()` of large numbers, which a phone's float precision turns
    into noise of its own.
  - Their alpha was not forced to zero outside the puff, so bad noise showed the whole quad.
  - Each puff was turned toward the eye on the CPU every frame. The shader now squares each puff
    to the screen from the camera's own axes.
- **Not yet known:** whether the tree shapes are gone. They need a look on the phone.

**Phase 2, a jump pressed a moment early is kept (native).** The scope asks for short native-tick
buffering of intent, executed only from valid contact. A jump pressed a tick before the feet
landed was dropped: on touch that reads as "I pressed jump and nothing happened".
- **Now.** A jump pressed in the air within 0.1 s (9 ticks) of touching down is spent on the first
  tick the feet stand. It fires only from real support, and not when crouched under a ceiling.
  It is never a jump in the air, and gives no free jump after leaving an edge.
- **Spent by anything else that takes the press.** A wall kick, the double-tap vault's window, a
  hang, a harness or a respawn clears it.
- **Proof (native `jump buffer`).** On the flat grade a jump lands after 101 ticks. Pressed 5
  ticks before landing, the body takes off again on landing; pressed 25 ticks before, it does
  not. In neither case does it rise in the air. With the buffer at zero the first case fails.
- **Not observed:** on the phone.

**Phase 2, wet footing: tried and taken out (native).** The scope asks for traction earned from
friction. The only surface the world marks as different is Wet Isolation's, drawn wet from 219
to 341 m, so its static footing was given the grip of wet steel (0.7 of dry). The native suite
then failed "along the 220 ring, D's spool seated and its fill thrown": the carried spool could
no longer be walked to its seat and stopped there. The scope also says to keep a route's feel
and clearances until a comparison shows an improvement, and slicker footing on a carried route
is not one. Left out. Real grip per surface needs the surfaces to carry a material, and routes
proven against it, first.

**Phase 8, the last two designs: deferred, with reasons (2026-10-15).** The scope says to defer a
design that cannot add a distinct, coherent experience without disproportionate changes, and to
say why. Both archive designs are in the source (`src/sim/vertical/`) and unplaced.
- **The gravity balance** (`gravity_balance.cpp`): a 2.5 t ballast falls on a rope over a sheave
  and lifts the deck, its slide motor only resisting. That is the counter-mass lift the route
  already has at AS-006 A, with S1 and O as variants. Placed, it would be one more stage where the
  rider waits on a falling weight (the no-repetition rule). The release plan (item 16) sites it at
  462.25–487.25 m, the only rise above 340 m with only a ladder; its site and whether it gets an
  action of its own are the owner's there.
- **The rack climber** (`rack_climber.cpp`): a motor turns the pinion that walks the deck up its
  rack: board, press RAISE, wait. That is the kind of machine decision 1 asks the owner about, and
  its drive is a motor inside the pinion. If the owner chooses rework, it can be reworked with the
  others: the pinion turned by a hand crank, as the pitman's is, would give the rider a different
  job.

**Phase 10, captions and a voice slider (Godot; release plan item 13).**
- **Captions** (AUDIO page, off by default). While the climber says a line, it is printed low and
  centred, word for word as said and uncensored, and it goes when the line ends or is cut. The
  words come from the voice's own generator (`src/voice/generate_fall_voice.py`). It now writes
  each clip's line into `voice_lines.gd` beside the clip lists, and its `--table-only` run rebuilds
  that table from the clips on disk without making any audio.
- **VOICE** volume (AUDIO page) sets the climber's voice on its own bus.
- **Proof.** `touch_lethal_feedback` turns captions on for its fall. Every line printed must be one
  of the voice's own lines, and nothing may be left printed once the impact cuts the voice. It
  printed "Fuck, fuck!" (rendered locally, 540×464).
- **Danger shown by more than colour.** The fall gauge was white, amber, then pulsing red. Its
  label now also names what the landing would be, at the same thresholds: M/S FALL, then HARD
  LANDING from 0.55 of the native lethal speed, then LETHAL from 0.8. At LETHAL the arc thickens.
  `touch_lethal_feedback` requires LETHAL at 0.8.
- **An in-world cue for the first moves.** The yard's painted pedestrian walkway, as any works
  has, runs from the start to S1's cage door: two edge lines with chevrons pointing the way, broken
  where the two kerbs cross it. It is site marking, not a painted parkour stripe (which the plan
  forbids), and paint only: `world_solids.inc` is unchanged. Rendered on the title view.
- **Still open (item 13):** cues for moves beyond walking to the first machine. Touch already
  labels MOVE, DRAG TO LOOK and each button's verb.

**The slingshot's launch shot, and a canopy you can see (Godot; the owner's 1A).** The owner
remembered a slow-motion 360° launch. It was the ChatGPT branch's (`launch_cinematic.gd`); this
branch never had one. The owner's notes on it: it looked scripted, as if the sling were not what
threw the rider, and its slow motion ran too long and slowed the push itself.
- **The shot** (`launch_shot.gd`).
  - The push plays at full speed and in the rider's own eyes.
  - The shot starts on the tick the pouch lets the rider go (the native's `flight`), when the
    bands have done all their work.
  - Time then eases to 0.3× in 0.12 s and holds to 0.55 s. Meanwhile the view turns back to see
    the bands and the yard below, then forward to the tower, and all is normal by 1.2 s.
  - Slow time is fewer native ticks per second and nothing else, so the flight is the flight it
    would have been. The mix slows with it.
  - REDUCED MOTION keeps the slow time but not the turn.
- **The canopy** (`canopy_view.gd`). A chute showed only two risers running out of frame. Now a
  ram-air canopy of eleven cells, orange and cream, flies 4.4 m overhead on lines fanned from
  the risers in the fists. It fills from its bag in 0.7 s, and its trailing edge works in the
  air. Its drag is still the native's.
- **Proof** (`touch_slingshot`). Time never slows while the bands push. The rider leaves the
  bands at 69.1 m/s. 0.3 s later time is at 0.3× and the view is on the bands. 1.3 s after the
  release all is back to full speed, and those 1.3 s ran 70 native ticks against 117 at full
  rate. The apex is 254.2 m, the same as before. The canopy must be seen fully open within 0.9 s
  of CHUTE. Rendered (local, 540×464): `launch_back`, `launch_forward`, `chute_up`.
- **Not observed:** on the phone.

**Parkour, the movement under everything (native, task #103; the owner's IMPORTANT TASK).**
The research and the move set are in `03_EXECUTION/PLANNING/PARKOUR_MOVE_SET.md`: what an
elite traceur's body does (eight findings), an audit of the native's eleven built moves with
five gaps, thirteen moves specified (trigger, physics, feedback, legal transitions), the timing
windows, and the tuning values. Built this pass, all in `src/sim/simulation.cpp`:
- **G1, momentum through a mantle** (M5). A mantle made at a run leaves at a run: the speed
  carried in along the facing less `kMantleExitCostMps` (1.5) is the speed it steps off with.
  Proof: an 8 m/s running mantle leaves at 6.3 m/s; a standing one still stands.
- **G2, the rising catch** (M6, the cat leap). The hands close on a lip or a hold while still
  rising, up to `kHangMaximumClimbSpeed` (2.5 m/s, was 0.2) -- in flight from a jump only, and
  on a hold only where the first pull up from it is clear. Proof: a jump at the proving ground's
  hang ledge catches at +1.58 m/s rising, peak 2.27 m.
- **G3, the tic-tac** (M7). A kick off a wall goes where the stick points along it as well as
  out from it (`kWallKickAlongMps` 2.0), and the steps start again off the kick, up to
  `kWallKicksMax` (2) kicks a flight. Proof: the same kick with the stick pulled back leaves
  2.0 m/s slower along the wall than with it neutral.
- **G4, the precision jump** (M3). The look (facing and pitch, `set_look_pitch`, new on the
  bridge) is cast ahead; a standable surface 0.8-4.5 m out and within -2.0..+1.2 m of the feet,
  with a clear stand on it, is the sighted landing, and JUMP takes off at the one horizontal
  speed that puts the feet there, within the legs' push (`kPrecisionPushMps` 3.0). Landing on it
  counts as prepared. The HUD lays an amber ring on the sighted point. Proof: native, the vault
  rail's top sighted from x 2.0 lands the feet on it at x 4.66; touch (`touch_precision`, new,
  73rd scenario, in CI), a walk up, the eyes down until the ring is on the rail, JUMP: landed
  x 5.12, prepared.
- **Tried and withdrawn.** A momentum catch with the stick neutral (closing at 1 m/s or more)
  caught holds the thumb never pointed at: a walk-off the 220 ring south grabbed AS-007's
  ladder below the lip instead of falling, a run up the yard curb hung from the compressor's
  top, and a slingshot flight would have snagged the first hold in front of the eyes. Hands reach
  where the thumb points. The first rising catch, taken on a step's lift too, swung from a rail
  over deck 8 on the walk into S3's cage; and a rising catch pinned under the rung the jump had
  just struck (C2's davit ladder) never climbed. Both are the two conditions above.
- **Still open from the owner's list:** rolling on a landing, the slide, coyote time and input
  buffering, the ledge grab and wall-run were already built (M9-M12, audit §2); physical mantles
  are decision 2 below.
- **Not observed:** on the phone.

**The four giant files split (task #112, the owner's top priority).** `simulation.cpp` (6,863
lines), `simulation_tests.cpp` (9,990), `ui_test_driver.gd` (5,005) and `main.gd` (4,209) are now
37 files of 270-960 lines, each one subsystem or one family of tests:
- **Native** (`d73a89d`): `physics_world_tuning.hpp` (every constant), `physics_world_support.hpp`,
  `physics_world.hpp` (the class), and `physics_world.cpp`, `_probes`, `_movement`, `_traversal`,
  `_climb`, `_state` (the members by subsystem); `simulation.cpp` keeps the public API.
- **Native tests** (`48717ae`): `sim_tests.hpp` and fourteen files, one per band or family;
  `simulation_tests.cpp` keeps `main()`, its run order and its `SCRAPERX_ONLY` groups.
- **GDScript** (`069a4a9`): `main.gd` and `ui_test_driver.gd` are each the top of an `extends`
  chain (geometry, world, machines, context, feel, main; base, moves, bands, rides, service, stack,
  driver). Each layer calls only itself and below; the engine checks that on every load.
- **Proof that nothing changed.** All four splits were done by script as pure moves. Every
  original line has its counterpart; GCC and clang build clean with `-Werror`; the native suite's
  output is byte-identical to the run before (105 PASS, DEBUG lines included); the world's solids
  re-exported through the split builders are byte-identical to `world_solids.inc`; all 73 touch
  scenarios pass. Runtime speed is unchanged; what changes is the work: an edit to one subsystem
  recompiles ~800 lines, and the files build in parallel.

**The grappling hook's line is a real rope (task #102, native + touch).** The hook already flew as
a body and bit only on holds; its line was a straight Jolt distance constraint that passed through
steel and sat out any tick it began a hair slack. Now:
- **The rope wraps.** Where the line from the chest to its anchor would pass through a static or
  heavy body, it catches on that body's edge: a bend there (found by sweeping the line from where
  the chest was a tick before; `kHookBendsMax` 8), the rope beyond it laid against the steel, the
  free length to the bend worked by the reel and the swing. Swung back round, it comes off. No rope
  is made or lost. Walk off the 220 ring's edge with the hook bitten and the line catches over the
  deck's lip and holds you hanging below it.
- **The line is the kit's rope, run direct** (`rope_constraint`): solved every step, so a hung body
  never drops its load for a tick at altitude.
- **The reel is a winch**: it takes the line in at up to 3 m/s pulling with at most 1.2 kN, and its
  brake holds when it stalls; it no longer moves the body by position. A line carrying part of the
  body's weight takes that share of the feet's grip, so a winch drags the soles toward the bar
  instead of being out-pushed by the legs, then lifts and swings the body.
- **Drawn as it lies**: the fist holds the line toward its first bend; a segment per bend runs on to
  the hook.
- **Proof.** Native `hook`: the reel takes the line in no faster than 3 m/s (measured on the line
  itself: 3.0); bitten on bar 0 and walked past the fixture's post, the line bends round it, comes
  off when walked back, never crosses the post's box (checked against the box in the test, not by
  the native), and the rope out is unchanged. Touch `touch_hook_rope` (new, 74th, in CI): on the
  220 ring, take the hook, bite a member over the well, walk off the ring's south edge; the line
  catches over the lip (y 220.28), the body hangs at 218.0 m alive, the reel winds it up. Rendered:
  `hook_rope`, looking up the line over the lip.
- **Changed by it.** A long diagonal reel now drags the rider and swings them (it used to lift them
  straight up, which a winch cannot); swung into a bar at speed, the line's peak tension reads the
  impact (16 kN). DROP lets go.
- **Not observed:** on the phone.

**The top of the route (task #92, the owner chose A + B on 2026-10-16).** West of the derrick's
upper landing (802.64 m) there is now a summit deck, `src/sim/summit.{hpp,cpp}`:
- **A dead searchlight** on a 30 m lattice mast at the deck's north edge: a drum 6.4 m across on a
  turntable. Dark as found, with a faint amber ember now and then. It is drawn at any distance.
- **Lit with your body.** A steel striker plate stands a metre proud of the deck on four guides, held
  up by gas struts (5 kN over its own weight). Up two concrete blocks (a pull-up onto the first, a
  jump-and-hang onto the second) is a gantry 6.4 m over the plate. Step off its edge onto the plate:
  the landing drives the plate 0.27 m down, past the 0.20 m that lights the lamp. Standing on the
  plate does nothing, three hops on it reach 0.14 m, and a step down from the first block nothing:
  only a real drop lights it. Once lit it stays lit (past a death too), and its own motor turns the
  turntable at 0.35 rad/s, so the beam sweeps the sky.
- **Seen from the ground.** The beam points 34 degrees up and runs 2.4 km, so from the game's start
  at night it rises out of the top of the tower (rendered: `summit_from_ground_night`). The first
  time it is lit the game remembers it (`user://summit.lit`, not in tests): every later climb
  starts under the lit beam.
- **A runway arm** 38 m out of the deck's west end, a windsock on a pole at its end. Out along it the
  wind off the tower takes hold (the same gusts as C7's, outward). Run off the end and open the
  parachute: down alive in about 80 s (native: landed at x -61.7, on the ground). Without the
  parachute the fall kills.
- **Proof.** Native `summit` (spawn, dark and still for 10 s, the walk to the arm's end, the plate's
  falsifiers, the strike, the turning beam, the plate's return) and `summit_arm` (with and without
  the parachute; the wind's push). Touch `touch_summit` (new, 75th, in CI): from the summit spawn,
  up the blocks, off the gantry, lamp lit, out the arm, CHUTE, landed alive (8.9 m/s).
- **Closest thing already in the game:** the slab ramp's trip plate, which you stand on. This one
  you have to land on hard from a height; standing on it does nothing.

**The grappling hook can tear loose (task #102, the last missing part).** The bite was a weld. Now
the hook's claws hold up to 6 kN on the line (about 7.6 body weights: hanging, swinging and reeling
never reach it). Past that for 0.15 s they slide off, and a jolt that adds up past it rips them out.
A torn hook springs back along the rope toward you and flies again: it can catch something else or
be reeled in. Native `hook`: a 12.6 m slack fall caught on the line (46.5 kN) tears it; the same bite
on a tight line, walked off the 220 ring's south edge, holds.

**The Matron: a released thing that hunts you (native + Godot + touch; the owner's idea, 2026-10-16).**
On the 154 m deck, on the north side east of where you arrive, stands a steel locker with a gauge on
its side. A loud noise near it (a sprint, a hard landing) wakes her: the locker shakes, the gauge
climbs for about sixteen seconds, the four big bolts on its door pop one by one and a red lamp
flashes faster and faster; then the door tears off and flies out over the drop and she steps out.
She hunts by sound (a sprint carries about 44 m, a walk 16 m, a crouched creep a couple of metres)
and by sight at close range, runs faster than you walk and slower than you sprint, and catches
you by touch, which is a death like a fall: you go back to where you were when she burst out (the
last save point is not renewed while she hunts and you are on her deck; leave the deck by a
machine and save points work as ever). She is a real body that really walks, nothing teleports her,
and the same start and the same moves give the same chase.
*The one thing that kills her* is the rotten catwalk across the middle of the deck: eleven steel
plates hung on bolts that give way under about 2 tonnes-force. You (80 kg) cross it; she
(220 kg, and she stamps) brings the plate she steps on down and falls the height of the tower.
It is hard to get: she only steps onto it when she can plainly see you on it, close ahead of her,
so you have to know it is there, lead her to its end and run across with her right behind you.
Otherwise she takes the long way round the deck. Nothing else hurts her.
*The Turn* (the owner's "ghoul mode", 2026-10-16): now and then, on the hunt, something in her
breaks. She stops dead for just under a second (the warning: a crack of bones, her music box
warps), then comes in jerks, bursts faster than a sprint and stumbling pauses, her spine bent,
neck turned the wrong way, elbows and a knee folded backwards. It starts when she has a clear
line to you within 13 m, or you land hard within 20 m, never in her first nine seconds out and
never twice within eighteen. It is no faster on the whole than her usual run, only harder to read.
*Kept simple:* she is not saved across a quit and reload (the world starts her asleep); the
locker and its gauge are solid; every sound is made in the game (a music-box tune she carries,
breath, scuffs, bolts, the bone cracks, the shriek). Native tests (`SCRAPERX_ONLY=matron`) cover:
asleep when quiet; the wake and sixteen-second build-up; the hunt and the catch; the frozen save
point; the catwalk holding you; the catwalk dropping her; the Turn; two identical chases ending
identically. `touch_matron` plays it on touch from the deck's start; `look_matron_*` scenes
render her. Judged and left for later: the owner's idea of boxes, beams and containers to move
about on that deck to use against her. Taking only what the physics already gives (a heavy crate
dropped on a plate would bring it down; a crate crashing over the edge would be heard and could
draw her off) so no new rules are needed; to be built after the owner has played her.

**Loose stock on the Matron's deck (native + Godot; the owner's idea, 2026-10-16/17).** Seven
real bodies on the 154 m deck, each shoved by walking into it, with no rule of its own
(`src/sim/loose_stock.cpp`, the first seven pieces): three 160 kg timber crates by the south band's rail, two side by side
and one on top like a step (climb the low one and shove the top one off); two loose crates either
side of the catwalk's east end; a 420 kg box of ingots on a rolling dolly against the shaft's rail
just north of the catwalk; a timber beam on the south band. What they do comes from the physics:
a crate is heavy to push (about 2 m in 4 s from a standing start); a crate that hits something hard
(a sudden change of 2 m/s or more) is a crash she hears exactly as she hears the player, and a
crash louder than the player draws her to it; the ingot dolly pushed out onto the rotten catwalk
brings the plate under it down and falls through, which breaks the way across (it does not kill
her: only a plate under her does). They are saved and restored with the machines. Proof: native
`matron` (a crate shoved; the dolly round behind, into line, out onto the plates, a plate down,
no one killed); `look_matron_stock` renders them. Not proven in a test: the crash luring her
(the crash's loudness was measured in a probe, about 2.4 m/s off the stack's top).

**Loose stock everywhere, first the yard (2026-10-17; the owner: "if SOME objects are moveable,
MOST objects should be moveable").** Everything lying about that is not holding the tower up is
now a real body (`src/sim/loose_stock.cpp`, kit entities from 5000): the stock yard south of the
start, which was static scenery, is 72 pieces: 16 shipping containers (3.75 t, two high in
places), 18 steel beams on 9 sleepers in three layers, a plate stack and slab, 3 pallets with 12
oil drums, and two pipe racks of three 8 m pipes on steel cradles. A drum or a pallet is carried
(PICK UP, named on the prompt), the middling pieces shoved, the heaviest wait for a machine (a
container walked into does not move). They sleep once at rest and wake when touched, so they cost
nothing while left alone; restore wakes them. Judged and changed: the yard's pyramid of six pipes
sank into itself in the solver whatever the iterations (a single tier holds), so each rack holds
one tier. Drawn by the kit's own renderer; `stock_view.gd` paints the containers, drums and
rusted beams, and the hands grip a carried drum by its rim. Proof: native `stock` (79 pieces
settle within 1.5 cm and all sleep; a drum carried 3.9 m and set down sleeps again; a container
does not move); `touch_stock` (the same carry through the touch controls); `look_stock_yard`
renders it. Not yet: stock on the decks up the tower, designed spots where moving stock opens a
route, the forklift and the crane.

**The yard's forklift (2026-10-17; the owner's to-do, item 2: heavy stock moved by a machine).**
A counterbalance truck of about 2.7 t parked north of the drums (`src/sim/forklift.cpp`): two
driven front wheels and two steered rear wheels (Jolt's vehicle wheels, with solid hubs inside
so nothing walks through a tyre), a counterweight behind the rear axle, an overhead guard, and a
mast whose forks ride up it 1.8 m on a powered slide of finite force (30 kN). The driver steps up
onto its floor under the guard; at the wheel DRIVE takes the controls. The stick drives and
steers (forward and right of the view; rear-wheel steering), Jump and Crouch held become RAISE
and LOWER, and STEP OFF lets go; let go of, it brakes. Nothing is scripted: it drives through its
tyres' friction (about 1.1 m/s/s, 3.4 m/s at most), the forks lift through their motor, and a
load sits on them by its weight. The forks are 5 cm thick and fit under a pallet's deck between
its stringers (and, by their size, under the bottom layer of steel beams between its sleepers;
not yet tried). Taken from the ChatGPT branch's truck: building it on Jolt's own vehicle (wheels
cast against the ground, a wheeled controller). Changed: four wheels not three, tyres that grip
from no slip (Jolt's default let a parked truck wander), no battery or tool coupler, the driver
stands at the wheel rather than on a rear step. Rejected for now: their energy ledger and
hydraulic power reservation (nothing the player meets yet). A restore lets go of
the controls and stills the wheels and engine, so every world loaded from one save drives on
alike. Proof: native `forklift` (parked 10 s it moves 0.1 mm; driven, turned right, stopped;
the first pallet and its four drums forked, lifted 0.39 m, carried back 3.5 m and set down,
the drums still on it; two runs end the same to the bit); `touch_forklift` (the same through the
touch controls).

**Mirror's Edge moves, first two (2026-10-17; the owner's to-do, item 3).** The wall climb: a
sprint jumped square at a tall wall (probed at the chest and above the head) with the stick held
at it plants a foot on it, and the leg takes the speed into the wall while the sole's friction
(at most 0.8 of it) turns it upward, capped at a fast jump's 7 m/s: about a metre of reach over a
standing jump, so a ledge up to about 4.7 m is caught. Once a flight; the foot counts as on the
wall all the way up, so a jump off it kicks the body back out (the wall turn). The tuck: Crouch
while rising in a jump draws the legs up (the head stays, the feet rise 0.6 m) to clear what the
legs would strike, at full speed; let go, the legs reach down where there is room; pressed
while falling, Crouch still readies a roll. On touch a tuck made in the air lets go on landing.
Proof: native `wall climb` (the proving ground's 4 m run wall: run up and its top taken; the same
jump with the stick let go peaks 1.5 m lower and misses; a jump off the wall kicks out at
3.5 m/s) and `tuck` (the 1.85 m crawl beam cleared by 20 cm; with the legs down the jump stops
at it); `touch_wall_climb`, `touch_tuck`. A tucked landing still rolls: the roll's clear-lane
check is measured from where the body would stand on the ground it struck (a hard landing sinks
it a few centimetres for a tick; the full native suite caught the refused roll). Next of item 3:
ziplines, swing poles, speed carried.

**The forklift's power and strain (2026-10-17; the owner's to-do, item 6).** The truck is now
weighed as a 3.5 t-class counterbalance truck is: about 5.1 t empty (the engine and
transmission in its frame, the drive axle, a 600 kg mast), about three-quarters of it on the
rear wheels, so the driven front wheels grip; the propane tank lies across the top of the
counterweight in its cradle. Its rear axle steers as a real one does (the inner wheel turns
further, so the two roll about one point and do not scrub the truck to a stop on full lock), and
the stick asks for a speed, as a hydrostatic drive does: a small push is a slow, steady creep.
The lift is a single-acting ram: RAISE drives it with up to 50 kN (its relief valve, about 1.4
times the rating); let go, its check valve holds whatever is on the forks; LOWER lets the load
down, metered, by its own weight; it can never pull the forks down. The limit is the real one:
the 2.8 t coil goes up with the pump at 30 to 35 kN and the rear axle unloaded to a third; under
the 5 t stillage of billets the rear wheels leave the ground when the load has risen 1.4 cm and
the truck goes down on its nose, about 10 degrees, with the ram at its relief; let go, it stays
tipped on the load; LOWER brings it back down onto its rear wheels with a slam. Heard: the
engine idles, climbs with the throttle and the lever, and lugs down as the pressure rises; the
pump whines higher with the engine and harder with the pressure; at the relief valve's pressure
the valve screams; letting a load down, the lowering valve hisses; the check valve clunks shut
on a load; the mast groans under strain; the rear wheels lift off and slam back down. Felt, by
the driver: the seat shakes harder and faster as the pressure rises, a jolt at the clunk, a
heavy one at the slam; the view turns with the truck and tilts with its floor. Drawn: the ram's
rod rising out of its barrel with a crosshead and two sheaves, and two leaf chains over them to
the carriage, which rises twice as far as the rod. Proof: native `forklift` (2.5 m in its first
2 s, a right turn, a stop; the pallet forked and carried; two runs end the same to the bit) and
`forklift strain` (the coil lifted 1.1 m at 35 kN, held when let go, set down; the stillage: the
rear wheels off at 1.4 cm of rise, nose down 9.9 degrees, back on its wheels when lowered);
`touch_forklift`, `touch_forklift_strain` (the same through the touch controls: the strain felt
in the hands, the lift-off and the mast heard, the slam heard and felt).

**Flow: speed built and kept (2026-10-17; the research's first).** A sprint still reaches its 8
m/s at once, so every route tuned to it stands; held on cleanly, it gathers on toward 9 m/s at
0.6 m/s each second (8 to 9 in about 1.7 s). The legs run at what the body carries, so whatever a
move brought in is kept -- a jump, a clean landing, a vault (a vault now carries up to 9 m/s, not
8), a wall-run, a slide -- and whatever a collision, a heavy landing (its balance debt) or a stop
took has to be gathered again from 8. No meter: the speed itself is the state. Felt: the view
widens on as the run gathers (4 degrees more by 9 m/s), and the air tears past, softer than a
fall's. Proof: native `flow` (8.2 m/s at 0.75 s, 9 at 2.5 s, 9 through a jump and run on after
it, 8.1 after letting go and running again; the sprint test now allows the gather) and
`touch_flow` (the same through the touch stick: 8.23 to 9.00 m/s, 9.00 in the jump, the view
86 to 90 degrees).

**The springboard (2026-10-17; the research's second).** Jump at a run (4.5 m/s or more) with
something knee- to waist-high a stride ahead, with half a metre of top to stand on and room
above it: a foot goes onto its top (a vault's path) and the jump goes from there -- the same
jump, from that much higher. The step up keeps 92 % of the run, and its push goes into what the
foot stood on (a loose crate or drum is shoved). Any piece of loose stock that size, anywhere,
is one; a rail or a thin wall stays a vault (Action, or Jump twice). Two faults found on the
way and fixed: after any vault finished in the air, the air's steering held the run to a walk's
5.5 m/s (a speed vault's flow was bled off in its flight); and a vault held over its top to the
very end worked out its height as 0/0 (the springboard's plant sent the body's height to NaN).
Proof: native `springboard` (the yard's plate stack, 1.1 m: 8.49 m/s in, 7.81 launched, the
soles to 2.63 m against 1.51 m for the same jump 3 m short); `touch_springboard` (the same through
the touch controls: 8.50 to 7.82 m/s, 2.63 m); `touch_double_tap_vault` and `touch_vault` still
vault the rail.

**The swing's launch (2026-10-17; the research's third).** Swinging under any bar was already a
real pendulum that keeps the run it caught the bar with; what Mirror's Edge adds is the launch.
JUMP OFF in a swing's forward half now throws the body on as the hands open -- the arms' pull,
2 m/s forward and up at 45 degrees on top of the swing's own speed -- and the bar takes the
push; in the back half, or near still, it only lets go, and DROP only ever lets go. A swing let
go also keeps its speed in the air now: the air's steering held a fling to a walk's 5.5 m/s.
Proof: native `swing launch` (two worlds alike to the tick on the monkey bars' last bar: thrown,
0.21 m higher and 1.8 m farther than let go); `touch_swing_launch` (JUMP OFF up the front of the
last bar's swing: up 0.24 m, down at x 9.35 against DROP's 6.6).

**The quickturn (2026-10-17; the research's fourth).** A TURN button over Crouch, for the same
thumb: half a turn of the view in about a fifth of a second, smooth, not a cut. The stick is the
view's, so a run held on turns about with it: the feet brake it and run it back. Shown on foot,
in the air and on a wall; not with the hands on something, in a seat or at the forklift's wheel.
Proof: `touch_quickturn` (half round, 1.43 rad of it at 0.1 s; a run east at 5.5 m/s turned
about to 5.5 m/s west within 0.9 s).

**Loose stock up the tower (2026-10-17; the owner: little lived-in spots all over the structure,
never confined to a few places).** When the world is built, a spot finder walks every deck ring of
the stack (11 m to 143 m; the 154 m deck keeps the seven set for the Matron) and every upper ring
(176 m to 396 m), and leaves little spots of real, loose stock against the shaft's railed edge:
a step of crates; welding gas bottles (oxygen black, acetylene maroon, argon blue) and a toolbox;
a reel of cable on its rims and a bundle of planks; a pallet of drums. Four new kinds: the gas
bottle (60 kg, carried), the cable reel (180 kg, it rolls), the toolbox (18 kg, carried), the
plank bundle. A place is taken only where the world says it fits: a level floor under every
metre of it and a 1.5 m walkway past it; nothing at all in the way above it; no machine, nor
anything that moves, within 3 m; the last spot 9 m back along the ring; 4 m clear of every place
the player is put down and of every lane the yard crew walk (a crate step first set across a
worker's lane on the 264 ring was walked into all day and never slept; the settle test found
it). Against the rail where the edge has one, 0.6 m back from an open edge; each ring dressed
from another side and place, so no two rings look alike. Up to three spots a stack ring and two
an upper ring: 195 pieces on all 24 rings, and none on a machine's route (the full native
suite's routes all still pass). The
finder costs about 80 ms when the world is built (first written, 540 ms: it now feels for
machines only among machines and moving bodies, and samples a metre apart). Every piece, the
yard's and the tower's, settles where it was put and sleeps: native `stock` (276 pieces settled
within 1.5 cm, all asleep after 12 s).

**Mirror's Edge research (2026-10-17; the owner: the movement alone should be a reason to play).**
What its running is made of, against what ScraperX has. Already here: run and sprint, the
wall-run, the kick off a wall, the wall climb, the tuck (its coil), the slide, vaults and the
speed vault, the roll (its skill roll), mantle, hang, shimmy, ladders and pipes, hand over hand
under bars, balance beams, the view widening with speed. Not here yet, in the order to build
(each a rule that works anywhere it physically can, never in a marked spot):
1. Speed built and kept: a run gathers to its top speed over a few seconds uninterrupted, and a
   vault, a roll, a wall-run, a slide or a tuck keeps it; a collision, a heavy landing or a stop
   loses it. Clean movement is fast movement; ugly movement is slow (its central idea).
2. The springboard: a foot planted on anything knee- to waist-high at a run launches the jump
   from it, higher and farther; every crate, drum, pallet and beam of the loose stock is one.
3. Swing poles: any horizontal bar or pipe within a jump; caught at a run, the body swings under
   and round it, and let go near the top of the swing it flies up and on.
4. The quickturn: half a turn (or a quarter) in one flick, on the ground, in the air, and off a
   wall climb to jump to the ledge behind. On touch, a flick on the look side of the screen.
5. Wall-run chains: from one wall to the next, across between two walls or round a corner.
6. Ziplines: down any sloping cable or rope, with a strap or a length of pipe over it.
7. Off a pipe or ladder: a jump back or to the side; a slide down a ladder.
8. The body felt at speed: breathing that follows the effort, the wind's roar and a slight shake
   at top speed, the view leaning into a wall-run.
9. A hint of the route: what the hands can take ahead picked out faintly (a setting, off by
   default), as its runner vision picks out its red.
10. Time trials against an exact ghost of your own best run: the simulation's determinism means
    a recorded run replays exactly. A reason to run one stretch again and again.
Left out: its fighting (disarms, attacks, the focus shield). Sources: EA's movement overview
(ea.com, "Movement in Mirror's Edge Catalyst: The Basics"), the series wiki
(mirrorsedge.fandom.com, Upgrades), StrategyWiki's controls page, Speed Demos Archive's notes,
and essays on its design (gardinerbryant.com; DICE interviews on its camera and dot).

**The slide jump, the side jump off a pipe, the fireman's slide (2026-10-17; the research's
seventh, and the slide's own jump).** Jump out of a slide and the body leaves the ground with the
slide's speed, not a standing jump's: at 7.65 m/s it flies 8.9 m before it lands (proof
`run_slide_jump`). On a ladder, a pipe or a wall's holds, Jump with the stick to the side throws the
body sideways off it (3.5 m/s across, kClimbJumpSideSpeed, less of the push back), to a ledge or a
hold beside it; the stick straight back is the jump back as before. Crouch held on a climb is the
fireman's slide: the hands loosen and the body runs down the holds under gravity to 4.5 m/s
(kClimbSlideMps), the hands taking each hold as it passes, and at the bottom it stands, or where
the holds end it lets go at the slide's speed; Crouch released stops the slide and the hands hold
again (`run_climb_moves`: slide 4.50 m/s, side jump 3.64 m/s across).

**Wall-run chains (2026-10-17; the research's fifth).** Two walls facing each other a passage
apart: the kick off one lands the feet on the other, running again, and the next kick comes back,
each kick higher; the chain is capped at four kicks a flight (kWallKicksMax: enough to climb a
passage, not to float up one). The proving ground has the pair south of the run wall, 18 m long
and 2.5 m apart (the width a kick crosses without losing height). Proof `run_wall_chain`: four
kicks, 2.8 s in the air, 2.86 m gained.

**Ziplines (2026-10-17; the research's sixth).** A taut steel cable from a post on one deck down
across the well to the far band of the deck below, on every stack level and ring where it fits
(swept for a hanging body's room along its whole length, built only where clear), and the yard
line from deck 2's south band down to the yard, 58 m at 23 degrees: 22 lines. On each a trolley,
two sheaves in a bracket over the cable and a bar hung under them, parked on a hook at the top
post; up the ramp onto the stand behind the post, a jump with the hands up takes the bar and the
body's weight pulls the trolley off the hook (the owner, 2026-10-17: hands never slide on a long
cable; the trolley rolls, nothing of the rider touches the wire). The slope drives it, the
sheaves' rolling drag takes a little (12 N), and over the line's last part the trolley runs into a
brake block, a clamp on the cable sized per line to take the speed off at half a g, pushing it
down the cable to a sprung stop 0.8 m short of the bottom post. JUMP OFF throws the body on
(the swing's launch); DROP lets go. The trolley and the block stay where the ride left them, so a
line is one ride until a death or a load parks them again. The trolley, its bar in the hands and
the block are drawn where the native has them (`zipline_view.gd`); the sheaves' buzz and the
block's squeal are heard at the trolley and felt in the hands; the HUD names the line and its
speed. Found by the full suite and fixed: the yard line's cable ran through the incline's
counter-mass shaft (clear at build time, the slab comes down it on a ride) and stopped the
incline; the line now runs at x = -2, east of the shaft. Found by the touch run and fixed, in the
movement itself: a run over a ramp's crest sailed on in a hop (2 m/s up for half a second,
the jump pressed on the stand refused in the air, the body carried off the deck's edge). Now
the feet just off the ground at a walk's speeds are swept down a step and put back on what
is there (`try_step_down`: the legs extend over a crest or a stair's step, as a runner's
do), and a jump pressed within a tenth of a second of the feet leaving (kCoyoteTicks) still
jumps; never off a jump, a kick, a launch or a let-go. The stand's ramp was also built on a
mirrored frame (no rotation at all, so its quaternion tilted it to 39 degrees); it is 27 now. Proof `run_zipline`: 22 lines, 16.5 m/s
down the yard line, braked and stopped at 57.45 of 58.23 m, DROP lands on the yard alive, the
trolley and block left at the bottom; a second ride's JUMP OFF at 10.8 m/s throws the body on at
11.4 m/s. Touch: `touch_zipline`.

**What the step-down shook loose, and the fixes (2026-10-17).** The full suite and touch run
after the step-down found four things, each fixed at its cause:
- The step-down pulled a body back down that a hook's reel was lifting; it now acts only on a
  body going over the ground at a step's walking speed, never one a bitten line holds.
- Its step-up grace (for a body settling onto a step) also held back a body launched off a
  ramp's head onto a flat top; the grace now ends for a body still rising faster than 1 m/s.
- AS-010 M's cage (the 662 m lift) lost 0.8 m of its ride whenever the chock's lanyard handle,
  let go in the cage, was pinned against the north rail and carried up until its cord stopped
  the cage: which way the handle fell came down to millimetres of where the rider stood. The
  chock now tips over onto a stop just past its release (it used to flop 70 degrees and pay out
  0.63 m of cord, so the handle dropped slack into the cage), and the cage's north rail has a
  gap where the lanyard comes in, so the handle swings back out on its line. Swept over nine
  places to stand and pull: eight reach the 662 deck, one still snags a dog short (0.8 m below
  the deck, climbable); to revisit with breakable trip-line cords.
- The arms' moving-hold fix carried a hand with a hold that jumped (a load handed over): a hand
  now rides a hold only while the hold moves with the body (the eye), else reaches for it.
Two proofs were walking paths only the old hop allowed: Deck 2's runway (now stepped onto from
the stair's landing, square to the beam) and the zipline stand (now run up from a stop).

**Where a zipline may stand (2026-10-17).** The ziplines' stands, moved under the parked
trolley, broke three things the full suite and touch run found: deck 6's line ran across S2's
cage shaft (the cage, parked at deck 4 when the line was built, rose into its cable 0.75 m short
of the top), the 220 ring's stand sat on the spawn in the path to well D's spool, and its post
then sat where the hook's rope sweeps from its rack. A line is now built only where nothing is
worked: its cable, with a rider's room under it, crosses no machine's travel (every guide's and
lever's whole stroke, part by part, from the kit), and its posts and stand keep 2 m from that
and from everything the hands take hold of, 4 m from every spawn and crew lane (as the tower's
stock does) and off the hook's practice ground. The top post stands 1.4 m in from the band's
edge, so the stand hugs the edge and the rest of the band stays open; where a band has no room
the line starts from the far band. 22 lines; none on the 220 ring, whose bands are taken by
C, D, the spawn and the hook. The touch proof of the hook's rope now finds the lip among the
line's bends (the body swings back in under the band and the line wraps its underside too).

**Deaths you see, and the broken ankle (2026-10-17).** The owner: deaths looked like garbage;
real physical consequences, bone breaks and blood, the camera shaking and the hands feeling it,
the voice yelling. A lethal impact now leaves the body where it hit: a ragdoll of eleven parts
in the climber's proportions (`sim/corpse.hpp`, Jolt's ragdoll; joints bending as a body's do),
laid down with its knees a little bent and carrying at most 4 m/s of the hit downward (the rest
spent crushing it), so it buckles and folds where it lands instead of standing on locked knees
or bouncing a metre off the floor. Eight lie in the world at most; a loaded game has none. The
view rides the dead head through the hit, shaken, then pulls up over the body to show it and
the blood: a burst of drops thrown up off the hit and the splash left on the floor, both as big
as the hit was hard (meshes, as the compatibility renderer the game ships on has no decals);
the joints are heard cracking and felt as pulses, more the harder it hit. The restored climber
waits until the body lies still (1.8 to 2.8 s). A landing harder than the leg's bones take now
breaks an ankle: 12 m/s planted (a 7.3 m drop), 14 m/s with the knees bent for it, 16 m/s rolled
out; the speed alone decides. For 30 s the stride is a hobble (2.5 m/s, no sprint), the jump a
weak push (3.2 m/s), the hands as before, and the view dips and tips on the bad leg's steps; the
break is cracked at the feet, cried out once, felt twice, and named. Proof: native `run_corpse`
(a 29.7 m/s death rests lying 2.2 m from the hit), `run_landing` (the 12 m drop breaks an ankle
planted, not rolled; hobble 2.475 m/s, jump 3.19 m/s, 5.5 m/s once healed); touch
`touch_lethal_feedback` (the view on the body's head, the blood down, two breaks felt, back to the
restored climber) and `fall_voice` (15.4 m/s: broken, one cry, a 2.47 m/s hobble on the stick).

**Hits, and the zipline's slam (2026-10-17).** The owner's own idea for the zipline: a brake
the rider uses, or a death slamming into the end. Ridden flat out, the stop's jolt tore the hands
off and the body went on into the bottom post at 10.7 m/s and lived: the lethal rule only knew
falls onto the floor. Now any blow counts. Running into anything that is not the floor under the
body (a wall, a post, an overhang, a machine), or being run into, the blow is the closing speed
along the contact as the step begins. Side-on or head-on there are no legs to spring it: 10 m/s
kills (a 5 m fall onto the face, half the feet-first 20 m/s) and leaves the body as a fall does,
named HIT AT, not FELL AT. From 6 m/s (a sprint into a wall; flow tops out at 9) it is a blow the
body feels: a thud, the view and the hand jolted, and from 8 m/s a cry and a red flash. So the
unbraked zipline now kills at its post. Proof: `run_zipline` (flat out: torn off, into the post
side-on, dead, the body by the post), `touch_zipline_slam` (new: ridden flat out on touch, the
death named a hit, the body by the post).

**Running to-do (owner, 2026-10-17; sequenced by Claude: quicker, broader, prototype first and
fill in after).**
1. The zipline's hand brake: done 2026-10-17. Stick back squeezes it (900 N); the end's buffer
   (3 m) stops a rider who comes in at 5.4 m/s or under; faster, the sprung stop's jolt tears the
   hands off (as a ledge grip) and the body goes on into the bottom post. Flat out on the yard
   line that is 20.6 m/s into the post and a 2.4 m drop, survived until injuries exist.
2. The whole first-person body, and injuries and deaths as physical consequences. Done
   2026-10-17: the corpse the view rides, the blood, the cracks felt and heard, the broken ankle
   and its limp, the cry; hits felt, and lethal side-on (the zipline's slam). Next: a broken
   arm, blood on the lens, new break lines for the voice, and the body itself, legs and torso
   seen from inside.
3. Route-building's reach: richer spots, heavy stock up the tower only where the crane or a
   forklift could really have put it (the owner, 2026-10-17), the big yard crane, forklifts up
   there.
4. Mirror's Edge's last two: the body felt at speed, the faint route hint.
5. Every machine its own start: S3 and the rest.
6. The Matron's AI (she uses the lifts, ziplines and moves, not the forklift or the stock) and
   her extras; her mind decided 2026-10-18 (decision 9: A + B + C).
7. Open, asked 2026-10-17: diegetic machine controls (grab the forklift's real lift lever and
   the forks rise); judged feasible, 5 for the forklift alone, about 8 for every machine.

**Ideas for pressure (two research agents, 2026-10-16; not built, the owner picks).** Ranked:
1. Gust fronts: above about 400 m a streak of mist races across the tower before each gust; the
   gust pushes jumps, tugs hanging hands, bows the hook's rope and the parachute; crouch or duck
   behind steel. Builds on the wind (`apply_wind`).
2. The falling walkway: planks drop away one by one just behind you, each one's fall pulling the
   next one's pin; walk and you fall, sprint and leap and you make it.
3. The drum run: a steep walkway where heavy drums roll down on a rhythm set by a filling bucket;
   dodge, slide under, vault.
4. Wreckage slides (a reason to go down): slide down the fallen slabs to a lower ring.
5. Rope down with the hook, walking down a wall.
Turned down: a swinging ferry (too like the swing), a stomp pump (too like the summit's plate),
breath at altitude (not real at 800 m), draining meters (annoying), rising water (no swimming).

**Decisions waiting on the owner (written in plain words, 2026-10-16).** Each one holds up work
that is otherwise ready. A letter is enough to answer.

1. **What's at the top:** decided 2026-10-16, A + B, built (above).
2. **How fast you pull yourself up over a ledge.** Today it takes under half a second: snappy and
   athletic, faster than a real person. Realistic would be about twice as slow, on every ledge in
   the game. A (recommended): keep it fast. B: make it realistic.
3. **Eleven machines start the same way** (between 220 m and 750 m): grab a hanging handle and
   step back. You said nothing should repeat. A (recommended): give each its own way to start
   (about 2 to 4 days; the machines stay, only how you set them off changes; the game first needs
   a few new moves, like shoving something by walking into it, stomping, or hanging your weight
   on a lever). B: redo the three most alike first, then decide. C: leave them; build new places.
4. **Three machines run on motors** (the tall mast, the spiral barrel, the gondola near the top);
   everything else moves by weight, gravity or your own effort. A (recommended): rework them so
   you power them, like the hand-crank lift. B: keep the motors.
5. **Platforms that dip when you walk on them.** Built, but it broke the crane-arm ride, which needs
   a latch first. A (recommended): leave it out for now. B: add the latch and bring it back.
6. **The store-ready version of the app.** Today's download is a test build. A store build needs
   your release key added to GitHub's secret settings, which only you can do. Can wait until
   you're ready to publish.
7. **The escape pressure (owner, 2026-10-16): something is RELEASED and chases you.** Decided and
   built: the Matron (above), a frail old woman in a steel locker who hunts by sound, with one
   way to kill her. The owner asked for the Turn (her ghoul mode, built) and for movable crates on
   the deck (judged, kept small, not yet built). The falling walkway and the drum run stay as
   smaller escape beats for later.
8. **Still yours to decide when ready:** the store name, the price, whether the code stays public,
   the credits, and whether you have the rights to the fall-scream voice clip.
9. **The Matron's mind:** decided 2026-10-18, A + B + C. A: two tiers, frail first (she rides the
   lifts and lines like anyone), then the Turn (she runs the parkour moves). B: she meddles with
   your route only in ways that can be undone (sends a lift away, waits quiet at a choke point,
   boards the lift you called), never making the summit impossible. C: she learns a trick you
   repeat within a run and forgets it when the run ends. Her senses stay honest: hearing and
   sight, a last-known place and a guess where you are going; never the forklift or carrying.
