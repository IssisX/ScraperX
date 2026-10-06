# SCRAPERX — AS-011 GROUND SLINGSHOT (grade → the 220 ring, or higher)

**Ascent Slice:** `AS-011`
**Lifecycle:** `BUILT` — the slingshot in the yard south of the tower, played through touch from
the game's start onto the 220 ring.
**Provenance:** the owner (2026-09-30): *"I did want a working slingshot (like angry birds) on the
ground that can launch the player far up the structure (for dynamic fun) with working stretching
of large rubber bands."* Ported from the `ChatGPT` branch's AS-023 (`src/sim/slingshot*.{hpp,cpp}`,
2026-10-01 to 2026-10-05; the owner, to that branch: *"The rubber bands were good"*), adapted to
this branch's kit, controller and route; the owner (2026-10-06) asked for that branch's good parts
to be brought here.
**Profile:** the throw is the bands' stored energy and nothing else. The draw is not
`MACRO-TRAVERSAL-STRICT`: like the owner's Angry Birds, the player pulls the pouch back, through a
declared, bounded winch (the stick's effort, at most 200 kW, 88 % transmission, 160 kN on the
pouch, 4 m/s), so the energy stored is work the player paid for, on the ledger.
**Depends on:** nothing below it; it stands in the yard from the game's start.

## What it is

- **Fork:** a rooted Y of round timber (capsules), its two tips 3 m either side of a point 10 m from
  the pouch at rest, along the default aim (82° up, toward the tower). Static.
- **Bands:** two rubber bundles, each from a tip to the pouch: natural length 10.44 m, 10 kN/m,
  tension only, 15 N·s/m loss in the rubber (`slingshot_model`). Forces, not constraints, applied at
  every physics step from the pouch's real position and speed.
- **Pouch:** 15 kg of cupped leather panels, upright (translation only), at rest at (−3.0, 0.35,
  −55.0): on the line of the route's arrival on the 220 ring (x −3), where the tower face's brace is
  lowest above that ring.
- **Draw rail and ratchet:** the pouch rides back along the grade on a slider whose lower limit is
  the ratchet face, up to 12 m; a carriage (the 60 kg launch rail) rides with it.
- **Aim:** the launch rail turns on a gimbal, ±0.8 rad of yaw and 0.35–1.48 rad of elevation, by a
  bounded torque paid from the same budget; the fork never moves.
- **Rider:** walked into the pouch and harnessed (ENTER POUCH): fixed across, sprung up and down by
  the leather's own law; the pair does not collide.
- **Release:** the draw slider comes off and the pouch runs up the aimed rail under the bands; after
  12 m of rail the harness comes off and the rider flies on with what the bands gave. No velocity is
  set anywhere.
- **In flight:** the stick steers with a bounded force (2 m/s² at full stick) instead of air
  control, which would brake the throw to a walk; the chute is offered from the top of the throw.

## Playing it (touch)

From the start, round the pouch's east block to its low front lip at (−3, −57.6) and step in;
ENTER POUCH. Pull the stick back to draw (the Action reads PULL BACK, then RELEASE with the draw and
the stored energy); drag to aim, the beads showing the shot. RELEASE. Over the tower and coming
down, CHUTE, and steer onto a ring. DROP leaves the pouch. After a shot, the spent pouch is reeled
back from the control post beside it (RETRIEVE POUCH, the stick held back).

## Results (native, this tree)

| Draw | Stored | Apex | With the chute over the tower |
|---|---|---|---|
| 2.0 s (8.0 m) | 124 kJ (work 142 kJ) | 118.7 m | — (lands short of the tower) |
| 2.8 s (10.2 m) | 263 kJ (work 298 kJ) | 254.2 m | standing on the 220 ring, (−3.0, 221.15, −129.3) |
| 2.9 s | | 270.7 m | the 242 deck |
| 3.0 s (10.7 m) | 297 kJ | 286.1 m | the 264 deck |

Peak speed 69.8 m/s at a 2.8 s draw.

## Falsifiers (`scraperx_sim_tests`, `slingshot`; `touch_slingshot`)

- Walked from grade into the pouch, ENTER harnesses the rider.
- 2.8 s of the stick held back draws over 9 m and stores over 200 kJ; the rubber never holds more
  than the draw put in.
- Let go of, the ratchet holds the pouch where it is (to 3 cm, 1 s).
- RELEASE throws the rider: apex 240–275 m, under 90 m/s.
- With the chute over the tower and the stick steering, the rider stands on the 220 ring, alive.
- The same shot twice lands at the same place on the same tick, bit for bit.
- A 2.0 s draw throws lower (apex more than 80 m lower), and without the chute that fall kills.
- Used again: a short shot comes down in the yard under the chute; at the post RETRIEVE POUCH starts
  the reel, the stick held back brings the pouch home to rest, and it takes a rider again.
- `touch_slingshot`: the same, from the game's start, through the touch controls: 263 kJ, apex
  254.1 m, on the 220 ring at 221.13 m, 25.3 s of play.

## What changed from the `ChatGPT` branch

- Placed at x −3 (theirs x 6), on the line of this route's arrival on the 220 ring; their receiving
  stack above 154 m is not built here — this tower's own decks receive the shot.
- One collision step a tick, as the rest of this world (theirs, four with the slingshot).
- The chute is this branch's; the thrown rider steers by force, as theirs.
- Their energy ledger, aim gimbal, ratchet, harness and retrieval are kept as written.
- Not taken: their launch cinematic, third-person climber and bullet time; the shot is flown in
  first person. The tooth rack has one tooth every 0.2 m (theirs 0.1 m).

## Open

- The landing is the player's skill: the chute and the stick. A caught landing (a net) is not built.
- No sound for the bands, the draw or the release yet.
