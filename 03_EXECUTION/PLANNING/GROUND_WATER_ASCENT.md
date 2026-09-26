# Ground ascent — screw, water-weight lift and stair handoff

**Scope:** the B00 water route; write branch `ChatGPT`.
**Source basis:** `7e66eb6`, 2026-09-26. Screw, bucket/cage and dock-to-+12 m
connection are implemented. Latest integrated proof/artifact: `00_START_HERE.md` §2.
This file owns their paired interfaces; AS-001/Atlas own how the route joins B00.

## Backward causal contract

| Receiver/output | Immediate required producer | Native owner / limits |
|---|---|---|
| Stable existing +12.1872 m stair landing | Walk the existing flight from +8.1872 m | `simulation.cpp`, `MOD-STAIR-A`; no movement retuning |
| Player stands on +8 m stair | Jump solid transverse stiffener on supported grating, land and walk across | `band_ground_water_lift.cpp` frame parts; real bridge deck and braces |
| Player reaches bridge from dock | Step from caught cage to fixed +8.25 m dock | Dynamic cage contact → static frame; support identity/momentum owned natively |
| Cage rises 8 m | Water bucket falls 4 m through weighted pulley constraint | Cage 500 kg + rider 85 kg; bucket 200 kg dry + up to 2000 kg water |
| Loaded bucket descends | Reachable release control removes actual top catch after boarding | Kit guided bodies/rope/catches; no actuator writes cage height |
| Bucket has enough weight | Player opens fill valve while bucket is caught beneath the tank | `update_water_lift` transfers actual tank inventory to bucket; mass updated in Kit |
| Upper tank contains water | Finite screw motor transports basin inventory upward | `water_screw.cpp`/`WaterScrewConfig`; no timer-created mass |
| Player can start operation | Normal apron approach to screw/fill/release controls | Existing input bridge and native station reach |

## Screw model and producer output

Current configuration: 11 m flighted length, 30° incline, 0.70 m outside diameter,
0.20 m core, 0.60 m pitch, 22 rpm target; 1500 N·m finite motor, 160 kg·m² rotor
inertia, bearing/viscous resistance. Basin initially 2.4 m³ (capacity 2.5), tank
capacity 2.0 m³, discharge elevation 5.5 m. Water density 1000 kg/m³; gravity
9.81 m/s². These are source configuration, not laboratory-calibrated pump data.

Declared reduced displacement:
`D = π/4 (0.70²-0.20²) × 0.60 × 0.31 × (1-0.087)` ≈ 0.0600 m³/rev.
At full immersion and nominal speed, `Q=D×22/60` ≈ 0.0220 m³/s. The 90.9 s
nominal fill estimate excludes acceleration and changing immersion/head. Source
bounds delivery by basin inventory/tank capacity and models leakage/recycle,
blocked outlet, drive direction and torque response. The 0.55 hydraulic efficiency
and fill/leak coefficients are explicit model assumptions, not measured performance.

Producer output is **conserved water in the same tank state**, not a `pump_done`
flag. The paired receiver needs up to 2.0 m³, physical fill alignment, reachable
control, an empty/caught bucket and its load-bearing frame.

## Lift transmission and reset

Source cage floor +0.25→+8.25 m at x=-12.50, z=-108.20. Bucket authored centre
+4.50→+0.50 m at x=-18.00, z=-111.50. Sheaves at +10.50 m. Source pulley ratio
0.50 implements `Δy_bucket + 0.50 Δy_cage = 0`: 4 m descent buys 8 m ascent.
Rope rating parameter 50000 N; cage brake 30000 N, target 2.20 m/s, leveling
parameter 2.0 m/s². The native speed test permits ≤2.35 m/s, not exactly 2.20.
Upper catch position derives from actual Jolt centre of mass, not shape origin.

Ideal full-stroke released potential is `2200×9.81×4 = 86328 J`; cage/rider gain
is `585×9.81×8 = 45910.8 J`, leaving 40417.2 J before other hardware, kinetic
energy and losses. The earlier 70% transmission efficiency was a sizing assumption,
not a measured/source efficiency; do not apply it as though the solver enforces it.
The ideal static water threshold is `2×585-200 = 970 kg` before friction; runtime
partial-fill/overload behavior must be measured rather than gated at that number.

After upper capture, the lower bucket drains back into the basin subject to basin
capacity; releasing the empty-cage upper catch allows cage weight to raise the
200 kg bucket. Reset spends cage potential and dissipates the surplus. Preserve
water inventory across basin + tank + bucket, including residual water when the
basin is full. Fill, drain and reset control states are consequential.

**Model boundary:** `update_water_lift` uses bounded constant fill/drain rates (0.10/0.12 m³/s).
Fill requires an open valve and latched bucket; drain requires bucket travel below
-3.92 m and a caught upper cage. This is an inventory/contact-state approximation,
not pressure/head-derived orifice flow or a resolved physical valve linkage. Kit
catch seating uses constraint correction (see AS-006's finite-work debt). No complete
hydraulic or spring-energy fidelity claim follows from the successful ride. Audit
these producing laws before extending them to head-dependent or repeated-energy
mechanisms; do not disguise an inferred interlock as visible modeled hardware.

**Capacity mismatch found by this audit:** `bucket_parts()` gives outside half
width/depth 0.85 m, walls 0.16 m thick, rim at local y=+0.45 and floor top at
-0.29. Its clear internal volume to the rim is therefore
`(1.70-2×0.16)² × (0.45+0.29) = 1.409256 m³`, before freeboard, whereas
`update_water_lift` permits 2.0 m³ (2000 kg). A green seeded inventory/ride test
cannot prove that volume fits the visible vessel. The authoritative seam is the
bucket geometry/capacity/overflow contract; preserve the working rope/controller
when repairing it. This is a source-derived mismatch, not a newly exercised
runtime failure, and remains unresolved by this documentation change.

## Dock-to-stair connection and original failure

Dock top +8.25 m; its towerward end is z=-109.85. Existing stair landing starts
at z=-114.20, leaving 4.35 m of open gap. The original normal walking attempt fell
to the lower stair; a correctly timed jump already crossed it. The failure was a
missing continuous supported connection, **not** proof of an impossible jump or
broken global controller.

The braced grating centre is (-8.55,8.15,-112.15), half extents (0.85,0.10,2.55).
Its z extent -109.60..-114.70 overlaps the dock by 0.25 m and the landing by
0.50 m. Its top is +8.25 m; landing top is +8.1872 m (0.0628 m step down).
A real 0.70 m transverse stiffener obstructs walking. The normal jump clears it;
air braking settles onto the grating, then walking joins the existing +8 landing
and climbs to +12.1872 m. Frame/deck/brace use the same native part definitions
for rendering and collision. No new controller, moving machinery or helper support.

The native `ground water lift` group fills, boards, rides, catches, drains,
steps onto the dock, resets, walks to the stiffener, confirms its obstruction,
jumps/brakes/lands, joins the stair and reaches stable +12 m support. The successful
full run reports `dock_to_stair12=1`; see the ledger. This preserves the existing
crane/gate/outside routes. The lift is a legal entrance above the grade gate.

## Evidence and open boundaries

The screw's delivery and receiving lift have separate runtime groups. The lift
group seeds the conserved tank output with
`set_water_screw_tank_volume_m3_for_proof`; it does **not** operate the screw and
then ride in one uninterrupted normal-input run. The seed does not move the cage,
set a catch or supply extra force, but it limits the end-to-end claim. A complete
apron→start screw→fill→ride→dock→+12 route still needs that combined proof.

Current in-memory checkpoint captures screw state, bucket water, valve state and
Kit bodies/topology. Source inclusion is not proof of every loaded-water restore;
intake machine bodies remain a separate B00 checkpoint gap. No durable save-file
format or Fold-device install/execution is proven.

The user's reported intake-belt failure to carry the player remains unresolved
against headless belt-route evidence. This water route does not repair or retire
it. Keep that observation distinct from the completed dock seam.

No new gameplay/runtime check occurred in this documentation audit. Stop after
the proven dock-to-+12 connection; further ground diagnosis or upper construction
requires its own bounded work cycle.
