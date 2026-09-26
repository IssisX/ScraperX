# Ground ascent: Archimedes screw to +8 m

## Backward requirement

The terminal condition is the player's physical body standing on the **fixed
+8 m landing**. Standing on the moving cage is an intermediate condition, not
completion. The landing has no stair, ramp, or simple ground route.

Working backward, the player needs a moving support that spans eight metres.
The cage can rise only when a descending water bucket is heavy enough. That
bucket can fill only through the upper tank valve. The tank can fill only with
water lifted from the finite grade basin by the driven Archimedes screw. The
screw must have an immersed inlet, open outlet, enough torque and time, and
available basin water. Thus each upstream state physically enables the next.

This is the **general mechanism profile**: the player's expressly requested
screw takes precedence over the compiler skill's narrower profile restriction
on screws. The causal and conservation checks still apply.

## Quantities and constraints

| Component | Model value | Effect |
| --- | ---: | --- |
| Grade basin | 2.4 m³ initial | finite feed inventory |
| Screw | 1.2 m outside diameter, 0.8 m pitch, 11 m flighted length, 30° incline | raises water about 5.5 m |
| Motor | 18 rpm maximum, 5 kN·m torque limit, 500 kg·m² rotor inertia | finite startup and coast |
| Upper tank / bucket | 2.0 m³ each | bucket takes actual tank water |
| Dry bucket / cage / rider | 200 / 500 / 80 kg | changing water and rider mass enter the lift equation |
| Rope | bucket travel = half cage travel | bucket falls 4 m for an 8 m cage rise |
| Lift | 700 N static and 400 N kinetic friction; speed governor and terminal brake | loaded cage can start, travel and settle safely |

For cage rise coordinate `q`, the ideal gravity drive is
`g × (bucket_mass / 2 − cage_mass − rider_mass)`. Its effective moving mass is
`cage_mass + rider_mass + bucket_mass / 4 + transmission_mass`. At a full bucket,
the drive is about **5.1 kN** before resistance. An empty bucket drives the
cage down for reset. The 2:1 constraint fixes bucket position at `4.5 − q/2`
in the scene; rope segments follow both bodies.

The native model advances at a fixed 90 Hz. Water moving from basin to tank to
bucket and back to basin is accounted for, and the screw records shaft and
hydraulic work. The scene reads native values to position the flight, water,
bucket, rope, and cage. It never moves the player by scripted teleport during
the ride: Godot's CharacterBody3D stands on an AnimatableBody3D cage, with a
floor contact ray checking its actual support.

## Failure and recovery

- A dry or underfilled bucket cannot lift the rider. A release that cannot
  move re-latches at grade, so the player can add water and try again.
- A weak motor, dry inlet, blocked outlet, full tank, or reverse direction
  cannot fabricate an upward transfer.
- The cage catches at the upper deck. Its bucket drains to the basin before a
  reset can be released.
- The empty cage returns to grade. A reset command is available at both the
  upper landing and the grade pump, so a fall cannot strand the cage overhead.
- The player respawns at grade after falling below the yard.

## Evidence boundary

`tests/ground_screw_tests.cpp` checks the isolated mechanism's capacity,
energy direction, conservation, failure states, ride, catch, drain and reset.
`godot/tests/ascent_smoke.gd` checks the integrated scene and player contact.
The CI job also builds an Android arm64 APK. The host tests are necessary but
cannot alone prove Godot collision handoff or touch controls; the scene test
addresses the former and a device playtest is needed for the latter.
