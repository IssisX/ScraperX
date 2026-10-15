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
| 3 Continuous climb, mantle, vault, catch, swing | ~45% | Mantle closing and the two-phase vault are native. New (2026-10-12): the yard's trapeze east of the start (`src/sim/trapeze.cpp`). A running jump off its launch stand brings the hanging bar into reach; GRAB (held 0.33 s, native) closes the hands. The hands are a constraint on the body's centre of mass: fixed across the arm, a 2 Hz spring at the arms' straight stop (a 5.5 m/s catch is taken over ~0.1 s, ~100–145 J lost, ~3.4 kN peak), and a pull-only motor of at most 1.5 kN up the arm (Crouch). Past 3 kN for 0.15 s the hands slip. The body's mass swings with the bar through the solver. Pulled up through the bottom and let down at the ends, the swing grows (0.60 to 1.21 rad); against that timing it dies (to 0.31); held still it keeps its first swing. The ledger of the arms' work against bar and body closes within ~40 J. LET GO removes the constraint and keeps the velocity; near the far end of a pumped swing it carries the body onto the landing stand, which the run's own swing cannot reach. Proof: native `trapeze`, `touch_trapeze`. Open: the AS-012 swing is still struck, not pumped; ledge catches are not compliant; mantles and vaults keep their prescribed paths. |
| 4 Impact recovery, rolls, stumbles, balance | ~60% | Native landing recovery (`kLandingFreeSpeed` in `simulation.cpp`): the legs take up to a 1.8 m drop free (3.3 m with crouch held). Beyond that, a balance debt from the normal energy lowers traction and reachable speed until it is paid back, never a lockout. A landing sliding across the facing adds a quarter of the tangential energy, which makes it a stumble. A later landing never forgives debt still owed: the larger is carried. A roll needs crouch held at contact, 2.5 m/s forward and a swept, supported lane. It keeps the momentum (2.7 m of travel against 0.96 m planted) and a quarter of the debt, and holds the body low even if crouch is let go. An obstructed lane is refused. The 20 m/s lethal rule is decided first. Edge balance uses the XCoM (`kTeeterComHeight`). Entered at a sprint with the XCoM past the edge, the brake along the motion falls to the soles' traction until the body stops or goes over. Past the XCoM no brake saves the body (the stop needs more than the v/omega left), so pulling back only buys time. "The edge" is where the ground stops running toward the XCoM, so slopes and stairs are ground. Beams (2026-10-12, `kBalanceHipMargin`): across the line the body is the same inverted pendulum, x'' = omega^2 (x - p), with the centre of pressure clamped to the beam plus 0.12 m of hips and arms. That replaced the 22 m/s/s centring, an invisible rescue force. A shove that keeps the XCoM within that reach is caught; one that does not goes off. The stick leans within it, and the view banks with the XCoM. Proof: native `landing`, `teeter`, `step2` beam pendulum; `touch_roll`, `touch_teeter`, and every route test that crosses boards. An independent adversarial review of this code confirmed 8 defects, all fixed (`e2fdf67`). Limits: a rigid capsule (the normal impulse is the solver's own). A support jerked under the feet (2026-10-12, `kSupportJoltMargin`): the soles re-match it only at the ground's traction, so a jolt's XCoM shift past half a foot takes a step, a stumble with debt. The crown gondola's swinging cabin does this to its rider (native `high`); smooth supports never do (moving-support truth). Not yet done: stumbles from off-axis impacts beyond a landing's slide, rotating footing under a beam, and a test of a roll blocked mid-lane (only a mover entering the swept lane can do it). |
| 5 Controls and tactile feedback | ~60% | Touch, pad and keyboard share one verb set. Haptics (2026-10-15) now carry the body's physics, each scaled from a native event or load (see below), coalesced at 50 ms on a monotonic clock, and gated by the vibration setting. Proof: `touch_roll`, `touch_teeter`, `touch_pipe_gallery` and `touch_machines` fail if their pulse is not sent. A hang torn off now drops and tips the view and is heard (a low scrape); a stumble scuffs; hauling the pitman's line heaves the view with the pull. REDUCED MOTION (Display) turns off the roll's turnover, head bob and the speed FOV kick, and quarters stagger, sway and body dips (`touch_roll_reduced`: the same roll, the view upright). Open: the phone (no vibration has been felt on a device, nothing seen on its screen). |
| 6 Coherent places and encounters | ~45% | Assessed 2026-10-14. Five of the eight supplied machines are now worked with the body, each by a different decision; nothing is pressed. The pitman lift (the rider's own work: hauled by hand on a 2:1 line against a counterweight, a dead-man brake; see below). The first exposed alternative: C7 climbs from the 264 ring to the mast–pitman walkway without the cascade mast (see below). The stone wheel (load fed: stone poured from the hopper). The slab incline (position and restraint: a trip plate pulls the slab's pin, a brake treadle holds or lets the trolley run; unbraked it strikes the head and kills). The traction tram (timing: a shuttle that waits 6 s and 4 s; missed at the head, it carries the rider back down). The luffing derrick (load removed: iron pigs thrown over the cradle's side until a chain-balanced counterweight wins; stepped off lightened, it leaves empty; CALL brings it back). Proof: native `incline`, `tram`, `derrick`, `high`; `touch_incline`, `touch_tram`, `touch_high`; CI green on `27cedca`. Open: the cascade mast, barrel helix and crown gondola are still board, press RAISE, wait; the incline's hop-on-hop-off braking may feel twitchy on a phone (not observed on a device); regions' identities, more sheltered vs exposed alternatives (C7 is the only one) and the encounter-to-encounter consequences in the scope are not yet authored. |
| 7 The gravity-fed wheel | ~70% | Worked with the body (2026-10-09): the feed plank opens the hopper and holds the wheel, stone alone drives it, and the brake only resists. Proven: too little stone doesn't lift the rider, and a missed boarding can be retried (native `wheel`, `touch_wheel`). Open: the stone's look and sound (high-quality material, spill and pile), reload of the finite hopper, the phone. |
| 8 Remaining supplied designs | ~60% | Eight of ten designs are placed and played on touch. The rack climber and gravity balance are not placed. The press-RAISE repetition is under phase 6. |
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
