# SCRAPERX — AS-012 THE SWING (the 220 ring → the 242 ring)

**Ascent Slice:** `AS-012`
**Lifecycle:** `BUILT` — off the 220 ring's south face, played through touch from the ring onto the
242 ring.
**Provenance:** the owner (2026-10-06): the canonical climb ends at the +221 m ring, and the climb
goes on from there; and *"I didn't ask you to make me a game of 3D chutes and ladders … There's
lifts that need a pull of something, and fucking ladders throughout the whole damn game. So
boring."* Plan §2.6 rule 11 lists what a stage may be instead: thrown, **swung on a hanging mass**,
carried by something falling, tipping or rolling, a run and leap. This is the swing. A
counterweighted boom was weighed first and dropped: a beam on a trunnion with a weight on one arm
and the rider on the other is S2's walking beam again (rule 9).
**Profile:** `MACRO-TRAVERSAL-STRICT`. Every part that carries the drive is large and in view: two
28.8 m arms, a 235 kg timber ram, a 2 m rubber buffer, the seat, a toothed rack on the face.
**Depends on:** the 220 ring (the route from grade, or the slingshot, AS-011, arrives there).

## What it is

- **The jib:** two girders 34.5 m long off posts on the 242 ring, out over the yard at 252 m,
  stayed to the 286 ring. Under them, two axles 3.15 m apart on one line at 251.5 m, in the plane
  x = −15.
- **The seat:** 150 kg, on a 28.8 m arm from the north axle, at the end of a gangway from the 220
  ring (its floor 220.30 m, the gangway's 220.25 m). Its east side is open to the gangway, its north
  side to the deck it is sent to. A stop under the axle holds its arm at plumb: it swings only
  north, toward the face.
- **The ram:** 235 kg, the seat's weight with an 85 kg rider aboard, on an arm of the same make
  from the south axle, hung back 82.5° south on its hook as found (its pin 28.6 m south of its axle, at 247.7 m).
  On its face, a rubber buffer: 2.0 m long, 25 kN/m, 80 N·s/m of loss, pushing only.
- **Level:** each hung body is kept level by a parallel link (declared: its rotation is not a
  degree of freedom; the link is drawn, the arm and the pin carry it).
- **The trip:** a kick bar across the seat's front at the rider's shins, on a slide with a return
  spring; a wire from it up the arm to the ram's hook. The bar pushed 0.10 m lets the hook go.
  Nothing but the rider's leg moves the bar (it touches nothing).
- **The rack:** 30 teeth on a curved rail east of the seat, from 1.20 to 1.374 rad of the seat's
  arm, every 0.006 rad (0.17 m of floor at the top); the pawl on the seat drops into each tooth it
  passes and rests on the last. A stop at 1.42 rad above them.
- **The harness:** fixed across, along and up the seat. It opens before the kick, or with the seat
  at rest on the rack; never mid-ride.

## Playing it (touch)

From the 220 ring, west along its south side to the gangway at x −13.35 and out along it, 28 m
over the yard, to its end; west into the seat. STRAP IN. Facing the tower, KICK THE TRIP: the ram
comes off its hook and down its arc, 3 s, and strikes the seat's back through the buffer; it stops
nearly dead, and the seat goes up its arc to the 242 ring's edge in 4 s, where the rack holds it.
UNBUCKLE and step down north onto the ring.

## Results (native, this tree)

| | |
|---|---|
| The blow, after the kick | 3.0 s; the ram at 23.0 m/s |
| The ram, 0.5 s after the blow | 0.09 m/s |
| Peak buffer force · the seat's peak acceleration | 43.8 kN · 14.6 g |
| The seat floor's apex · held on tooth 24 | 242.78 m · 242.62 m (the ring's top 242.25 m: a 0.37 m step down) |
| Lost in the buffer | 5.0 kJ of 73.9 kJ the seat side gained |
| Ledger residual (from the hook's release) | −1.67 kJ (−2.3 %): the buffer's loss is counted; the rest is the rack's catch (≈ 0.4 kJ) and Jolt's projection of each pendulum onto its arc at 90 Hz (≈ 1.5 %) |
| On the 242 ring | body centre 243.13 m, alive |

## Falsifiers (`scraperx_sim_tests`, `swing`; `touch_swing`)

- Left alone 30 s, the ram stays on its hook and the seat hangs plumb.
- Standing in the seat unstrapped, the ram stays on its hook (5 s).
- STRAP IN harnesses the rider; strapped and not kicked, nothing moves (5 s).
- KICK THE TRIP lets the ram go; it strikes the buffer 2–3.5 s later.
- The ram gives the seat its swing and nearly stops (under 3 m/s).
- The seat's floor swings past 242.5 m, the rack holds it with the floor at the deck or a step
  above (242.25–243.1 m), and it does not move from its tooth (5 s).
- The blow stays under 20 g.
- The ledger: the buffer takes 1–10 % of the energy; nothing comes from nowhere (residual under
  +0.5 %, over −3 %).
- UNBUCKLE and the step north put the rider on the 242 ring, alive.
- The same ride twice ends in the same state, bit for bit.
- Mid-ride, LEAVE is refused and the rider stays in the harness.
- Stepped off the seat's open front at the gangway's end, the rider falls and dies; the restore
  brings back the rider in the seat and the ram on its hook, and the swing rides again to the 242
  ring.
- `touch_swing`: the same, from the 220 ring through the touch controls: on the 242 ring at
  243.15 m, apex 242.78 m, 14.6 g, 19.2 s of play.

## The action it introduces (rule 9)

The kick: strapped in, the rider's own leg lets the machine go. The form is a ram striking a seat,
both hung on arms; the look is a jib and gangway out from the tower's face. No ladder, no pull
(rule 11).

## Open

- The swing is spent by one ride: the ram stays down and the seat on the rack. A rider who comes
  back to the 220 ring has the slingshot (a 2.9 s draw lands on the 242 deck) or `AS-007`'s ladder
  (not canonical); dying restores the last commit, machines included.
- No sound of its own yet for the ram, the blow or the rack.
- Above the 242 ring: C6, a climb on the ring's west band to the 264 ring (plan §3).
