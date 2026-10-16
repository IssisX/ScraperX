# Physical fork parking cradle

Source implementation on `ChatGPT`, base revision `46374e2`. This record covers
the new geometry only; root owns combined native/public-route acceptance.

`WarehouseForkliftYard::kParkingCradleEntity` is **2447**, previously unused in
source, tests and Godot. `parking_cradle` is an ordinary static Kit body and
`parking_center` exposes its authored shape origin. The existing Kit export
path supplies its visible parts and collision; no separate visual or support
authority is introduced. All coupler/contact/force/limit laws remain unchanged.

Chosen geometry is two solid steel dunnage rails, each 0.34m wide, 0.20m high
and 1.20m long, centered at cradle-local X +/-0.48m, Y0.10m, Z0. Their bottoms
bear on grade and tops are Y0.20m. Each part records its volume-derived steel
mass, 636.48kg at 7800kg/m3; body mass zero selects static construction. Friction
is 0.85 and the normal box contact bevel is 5mm. Each blade's bottom center is
above a rail's interior flat face. The 72kg fork remains entity2443.

For the default production yard, cradle origin is **(-88,0,-70)**, with unchanged
yard orientation. Its bounds are X[-88.65,-87.35], Y[0,.20], Z[-70.60,-69.40].
The intended truck faces world -Z (yaw PI). At nominal centered parking:

- Fork shape origin: approximately **(-88,.25,-69.10)**, yaw PI.
- Chassis shape origin: approximately **(-88,.624,-67.37)**, yaw PI; actual settled
  ride height is physics-dependent. Lift above its zero setting is about .106m.
- Rails in fork-local space span X[-.65,-.31] and [.31,.65], Z[.30,1.50].
- Fork COM projection is Z.525: derived from (12*.12 + 12*.62 + 6*.29 +
  42*0)/72 = .1475m vertically and 42*.9/72 = .525m longitudinally. The
  longitudinal support margins are .225m rearward and .975m forward.
- The carriage's foremost steel is approximately tool-local Z-.10, leaving
  .40m to the nearest cradle steel. The tool rear frame occupies Z[-.06,.06].
  There is no rear stop or upper capture geometry. Reverse is toward world +Z.

Load path: fork gravity -> two blade underside contacts -> static steel rails
-> grade/backbone. The support supplies no work. Actual contact persistence and
the unchanged supported joint-load condition still decide whether detach succeeds.

Source clearance inspection found no generated world box or convex-hull AABB
overlap in X[-90,-86], Y[0,4], Z[-73,-63], covering the bed and straight withdrawal
corridor. This is a conservative source geometry scan, not runtime collision proof.
The original chassis spawn is (-100,.65,-56), warehouse cargo remains around
X[-105,-95], Z[-48,-44], the retained raised gallery is at X-100/Z-94 and the
crane pivot remains (-60,36,-150). The new bay sits east of their operating lane.
The original spawn can approach by turning east, crossing into X-88 and turning
toward -Z before entering the bay. Turning dynamics and that human drive route
remain acceptance work for root.

No build, simulation, public-input or device test was run by this geometry worker,
as assigned. Unverified risks are actual return alignment and settling, ground
clearance while turning, and full collision sweeps beyond the inspected corridor.
The new body is appended after existing yard bodies; subsequent crane Kit/body
indices shift by one, while all retained entity IDs and authored poses remain
unchanged. Checkpoint compatibility must follow the combined owner acceptance.
