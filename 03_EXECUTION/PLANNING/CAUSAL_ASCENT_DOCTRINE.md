# SCRAPERX
# PROJECT CAUSALITY — COMPLETE ASCENT MASTER SPECIFICATION
## PINNACLE CAUSAL-PHYSICS STANDARD

**Coverage:** 280 m → 680 m  
**Stages:** 13 causally linked ascent mechanisms  
**Ultimate tower target:** approximately 1600 m  
**Authoritative simulation:** native C++17 `scraperx_sim` + Jolt  
**Simulation cadence:** fixed 90 Hz  
**Presentation / input / camera / HUD / Android:** Godot 4.7

---

# 0. DOCUMENT STATUS AND SCOPE

This is a **timeless architecture and physics doctrine** for ScraperX.

It defines:
- causal-physics laws;
- mechanism design standards;
- simulation ownership;
- energy, mass, travel, and capture requirements;
- stage designs for 280 m → 680 m;
- failure behavior;
- falsification tests;
- verification doctrine.

It does **not** contain:
- current branch state;
- current commit SHAs;
- current CI run IDs;
- present debugging status;
- temporary blockers;
- active work-item sequencing;
- “do not proceed until X is green” execution notes;
- any other transient project-management state.

Those belong in continuity / project-state documents, never in this master specification.

---

# 1. PRIME DIRECTIVE

ScraperX is not a sequence of scripted elevators disguised as machinery.

It is a **causal physical world**.

The player ascends because stored energy is released, transmitted through actual machinery, converted into motion, constrained by geometry, dissipated through physical processes, and finally captured by another mechanism.

The governing chain is:

> **STORED ENERGY → RESTRAINT → PHYSICAL TRIGGER → FORCE TRANSMISSION → CONSTRAINED MOTION → GOVERNING / DISSIPATION → TERMINAL CAPTURE → PHYSICAL HANDOFF**

Every consequential ascent mechanism must contain that entire chain.

A mechanism fails specification if any missing physical link is replaced by:
- an arbitrary height threshold;
- a timer;
- an invisible completion flag;
- an animation callback;
- a teleport;
- direct position assignment;
- fabricated velocity;
- an unexplained impulse;
- a hidden motor supplying undeclared energy;
- a Godot animation that disagrees with the native rigid body;
- a test-only shortcut that does not exist in play.

A script may **observe** physics.

It may not secretly **manufacture the result**.

---

# 2. AUTHORITY CLASSES

Not every number in this document has the same authority.

Every mechanism parameter belongs to one of four classes.

## LAW
A project invariant.

Examples:
- `g = 9.81 m/s²`
- native simulation owns consequential physics
- moving supports transfer velocity to the player
- next-stage activation requires a physical terminal state

## DERIVED
A value that mathematically follows from accepted geometry, mass, or energy state.

Examples:
- counterweight potential energy
- drum revolutions
- hydraulic displacement
- required static torque
- ballistic velocity

## DESIGN TARGET
An engineering value deliberately selected but still subject to solver validation.

Examples:
- governor speed
- clutch torque limit
- brake coefficient
- snubber travel
- capture tolerance

## SOLVER-FIT
A value that must be established from actual Jolt behavior.

Examples:
- constraint compliance
- contact friction
- restitution
- damping
- motor force limits
- latch tolerance
- CCD settings

These values may be adjusted to make the declared physical mechanism behave correctly.

They may not be adjusted to introduce an undeclared source of energy.

---

# 3. GLOBAL PHYSICAL CONSTANTS

Gravitational acceleration:

`g = 9.81 m/s²`

Reference player mass:

`m_player = 80 kg`

Atmospheric reference pressure:

`P_atm = 101.3 kPa absolute`

Water density:

`rho_water ≈ 1000 kg/m³`

Gravitational potential energy:

`ΔE_g = m g Δh`

Linear kinetic energy:

`E_k = 0.5 m v²`

Rotational kinetic energy:

`E_rot = 0.5 I ω²`

Hydraulic force:

`F = ΔP A`

Hydraulic volume continuity:

`Q = A v`

Buoyancy:

`F_b = rho V_displaced g`

Quadratic fluid drag:

`F_d = 0.5 rho C_d A v²`

Rotational work:

`W = ∫ τ dθ`

Linear work:

`W = ∫ F dx`

No ideal equation above is by itself a sufficient simulation model.

Where materially significant, the system must also represent:
- bearing loss;
- drivetrain efficiency;
- friction;
- spring preload;
- rotating inertia;
- fluid restriction;
- gas depletion;
- hydrodynamic or aerodynamic drag;
- added fluid mass;
- impact absorption;
- constraint compliance;
- rope / cable elasticity or constraint response;
- terminal dissipation.

---

# 4. THE CAUSAL COMPLETENESS CONTRACT

Every mechanism must explicitly identify all of the following.

## 4.1 Energy reservoir

Where did the usable energy physically exist **before movement began**?

Permitted examples:
- elevated mass;
- elevated water;
- compressed gas;
- compressed spring;
- spinning flywheel;
- hydraulic accumulator;
- buoyant displacement.

If the reservoir cannot be identified, the mechanism has no legitimate power source.

---

## 4.2 Restraint

What prevents the stored energy from releasing early?

Examples:
- dog;
- pawl;
- brake;
- catch;
- valve;
- clutch;
- lock pin;
- hold-down.

Stored energy without restraint is not an armed machine.

---

## 4.3 Trigger

What physically changes the restraint?

The previous stage may:
- depress a plunger;
- pull a cable;
- rotate a dog;
- shift a spool;
- seat a follower;
- release a pawl.

The trigger **permits** energy release.

It does not automatically **provide** that energy.

---

## 4.4 Transmission

A continuous physical load path must exist between source and carrier.

Example:

`counterweight → rope → drum → shaft → gearbox → screw → carriage`

or:

`pressure vessel → regulator → valve → chamber → piston → carriage`

Every consequential force-producing link must have one authoritative owner.

---

## 4.5 Geometric closure

The claimed travel must emerge from actual geometry.

For a drum:

`x = r θ`

For a screw:

`x = N P`

For gears:

`ω_out / ω_in = N_in / N_out`

and approximately:

`τ_out / τ_in = N_out / N_in × efficiency`

For hydraulic displacement:

`A_in x_in = A_out x_out`

For pulley systems:

rope-length conservation must produce the claimed inverse force / travel relationship.

A mechanism may not multiply both force and distance without another energy source.

---

## 4.6 Governed movement

Having enough energy to move is not enough.

The mechanism must explain why it does **not accelerate without bound**.

Permitted control elements include:
- centrifugal governor;
- friction band;
- hydraulic orifice;
- torque-limited clutch;
- eddy-current brake;
- pneumatic restriction;
- aerodynamic or hydrodynamic drag;
- cammed brake application;
- dashpot.

The player may ride large machinery.

The player may not be subjected to arbitrary solver violence because the design omitted a governor.

---

## 4.7 Dissipation

Excess mechanical energy must have a physical destination.

Valid destinations include:
- friction heat;
- hydraulic throttling;
- gas exhaust;
- fluid drag;
- residual flywheel energy;
- residual spring preload;
- compliant deformation;
- braking work;
- residual counterweight potential.

Energy does not disappear because a mechanism reached a convenient elevation.

---

## 4.8 Terminal capture

Position alone is never a terminal condition.

Every occupied moving carrier requires a sequence equivalent to:

> **approach → decelerate → align → capture → load transfer**

The terminal assembly may use:
- brake;
- governor;
- hydraulic cushion;
- pneumatic cushion;
- snubber;
- compliant bumper;
- pawl;
- dog;
- receiver;
- docking fork;
- over-centre latch.

The final load must be physically supported.

---

## 4.9 Handoff

The terminal mechanism must physically create the next causal opportunity.

Examples:

`capture dog seats → linkage pulls next pawl`

`carriage finishes cushion stroke → linkage opens sluice`

`index receiver locks → receiver depresses next pilot`

The chain is continuous because one machine physically arrives and operates another.

---

# 5. OBSERVATIONAL STATE MODEL

Mechanisms may be described for telemetry and reasoning as:

`ARMED`

`TRIGGERED`

`POWERING`

`GOVERNED TRAVEL`

`TERMINAL APPROACH`

`CAPTURED`

`HANDOFF`

`SPENT`

These are descriptions of physical reality.

They are **not** permission for software state to create physical outcomes.

`CAPTURED` means a real catch or structural constraint is engaged.

`POWERING` means measurable force or torque is crossing the declared drivetrain.

`SPENT` means the actual reservoir has lost usable energy, pressure, angular velocity, mass, or travel.

> **The state label follows the machine. The machine never follows the label.**

---

# 6. ENERGY LEDGER LAW

Every stage must maintain an explainable energy ledger.

At minimum:

`E_source_initial`

`E_source_remaining`

`ΔPE_carrier`

`ΔKE_linear`

`ΔKE_rotational`

`E_dissipated_estimated`

A valid stage approximately satisfies:

`E_source_released >= ΔPE + ΔKE + modeled losses`

subject to numerical and modeling tolerance.

A stage creating substantially more mechanical energy than its source releases is broken.

A stage with large unexplained disappearing energy is also suspect.

## Engineering margin

The reservoir should ordinarily contain at least approximately **15% usable margin** beyond required work and expected losses.

This is a design target, not a universal magic constant.

High-dissipation systems may legitimately require far more.

The excess must still have a known destination.

---

# 7. MASS AND INERTIA LEDGER

Mass never teleports between mechanisms.

The only mass automatically travelling through the ascent is the player.

Each stage must separately state:
- player mass;
- local carrier mass;
- local ballast;
- counterweight mass;
- moving drivetrain mass;
- materially significant rotational inertia.

When the player leaves one carrier and enters another, the previous carrier does not remain in downstream calculations.

Likewise, large arms, wheels, drums, and flywheels require real inertia.

Payload mass alone is not enough.

---

# 8. ONE-OWNER PHYSICS LAW

Every consequential physical quantity has exactly one authoritative owner.

The native simulation owns:
- rigid-body transforms;
- linear velocity;
- angular velocity;
- counterweight travel;
- cable / rope state;
- guide travel;
- hinge angle;
- drum rotation;
- spring deflection;
- hydraulic pressure;
- pneumatic pressure;
- fluid inventory;
- force-producing valve state;
- latch engagement;
- terminal capture;
- player support body.

Godot renders and presents those results.

Godot does not independently animate a consequential mechanism to where it is “supposed” to be.

If presentation and simulation disagree, the presentation must be corrected.

---

# 9. MOVING-SUPPORT LAW

When the player stands on a moving rigid body, support velocity becomes part of player motion.

This includes:
- linear support velocity;
- angular motion around the support body's instantaneous rotation.

Jumping from the support inherits appropriate momentum.

This applies to:
- elevator cages;
- screw carriers;
- pendulum platforms;
- hydraulic lifts;
- rotating arms;
- gondolas;
- pneumatic capsules;
- buoyancy capsules.

A mechanism that moves correctly while the player slides off or fails to inherit motion is incomplete.

---

# 10. FAILURE MUST ALSO BE CAUSAL

A causal world needs legitimate mechanical failure.

Examples:

insufficient counterweight  
→ carrier stalls

insufficient spring preload  
→ anti-backdrive dog holds intermediate position

low hydraulic pressure  
→ ram stops short

flywheel underspeed  
→ clutch cannot overcome load torque

low pneumatic pressure  
→ emergency catch captures lower

governor overspeed  
→ brake closes harder

insufficient flood level  
→ buoyancy hold-down remains locked

Failure may not silently invoke a scripted success state.

---

# 11. REARMING LAW

The active ascent may be one-way, but every consumed reservoir requires a believable maintenance cycle.

Examples:
- service winches raise counterweights;
- compressors recharge receivers;
- pumps refill elevated water tanks;
- motors rewind springs;
- service drives spin flywheels;
- pumps return dense process fluid;
- ballast systems drain flooded shafts.

These systems need not participate in the active ascent.

Their conceptual existence keeps the tower a machine rather than a set of supernatural one-shot props.

---

# 12. COLLISION TRUTH LAW

Visible mechanical structure and collision structure must tell the same story.

The player must not:
- walk through structural beams;
- stand on decorative geometry with no physical support;
- be blocked by empty space;
- pass through a massive terminal receiver;
- fall through a moving carrier.

Likewise, a moving mechanism must not be required to pass through static frame geometry to complete its designed travel.

Every moving body requires a verified swept volume.

For a body with path `P(t)` and collision shape `S`, the swept region:

`⋃ S(P(t))`

must remain clear of all static structure except deliberate contacts, guides, brakes, stops, and receivers.

This requirement applies to the entire travel range, not only the start and end poses.

---

# 13. MECHANISM PROOF STANDARD

Every completed mechanism requires both positive proof and destructive falsifiers.

## Positive proof

Show:
- source armed;
- physical trigger occurs;
- force enters transmission;
- carrier moves;
- governor bounds behavior;
- terminal assembly captures;
- player rides the actual moving support;
- next mechanical linkage becomes available.

## Source falsifier

Remove, immobilize, discharge, or empty the declared source.

Success must become impossible.

## Transmission falsifier

Disconnect one force-transmission link.

Success must become impossible.

## Trigger falsifier

Place the player near the machine without allowing the previous terminal actuator to operate.

The machine must remain restrained.

## Capture falsifier

Remove or disable the terminal latch.

The next mechanism must not activate anyway.

## Geometry proof

Record actual:
- shaft turns;
- cable travel;
- piston displacement;
- guide travel;
- hinge angle;
- cam travel;
- pulley movement.

Claimed carrier travel must follow from those values.

## Robustness proof

Test a defined operating envelope around:
- player payload;
- source charge;
- friction;
- initial velocity;
- timing;
- contact conditions.

The mechanism must tolerate ordinary variation rather than depending on one numerically perfect trajectory.

---

# 14. TELEMETRY CONTRACT

Every major mechanism should expose enough solver-derived telemetry to locate the broken causal seam without turning CI into exploratory guesswork.

Useful fields include:

`source_energy`

`source_position`

`source_velocity`

`transmitted_force`

`transmitted_torque`

`rope_tension`

`drum_angle`

`carrier_travel`

`carrier_velocity`

`carrier_peak_velocity`

`governor_force`

`brake_force`

`distance_to_terminal_seat`

`catch_engaged`

`support_entity`

`terminal_linkage_position`

Hydraulic and pneumatic mechanisms additionally expose:

`reservoir_pressure`

`chamber_pressure`

`valve_fraction`

`flow_rate`

`fluid_volume`

Fluid-transfer mechanisms expose conserved inventories.

Telemetry reports physical facts.

It never drives them.

---

# 15. CAUSAL DEBUGGING STANDARD

When a mechanism fails, do not begin by asking:

> Which assertion should be loosened?

Ask:

> At which physical seam did the declared causal chain stop being true?

Inspect in this order:

## SOURCE
Did the reservoir actually contain the declared usable energy?

## RESTRAINT / RELEASE
Did the physical catch, pawl, valve, or clutch actually change state?

## TRANSMISSION
Did measurable force or torque cross the connection?

## GEOMETRY
Did the mechanism have enough real travel and clear swept volume?

## FORCE BALANCE
Was source force / torque sufficient against load and resistance?

## GOVERNOR
Did control hardware only oppose motion, or did it accidentally become the dominant restraint?

## COLLISION
Did unintended contact steal travel, jam the system, or create false support?

## TERMINAL APPROACH
Did the carrier actually enter the capture envelope at an acceptable velocity and orientation?

## CAPTURE
Could the latch physically seat at the carrier's real position, COM, velocity, and orientation?

This diagnostic order is mandatory because it follows causality rather than symptoms.

---

# 16. VERIFICATION DISCIPLINE

GitHub Actions or equivalent remote CI is a **verification environment**, not the preferred exploratory debugger.

A verification candidate should be pushed only after source inspection and local reasoning have resolved the known causal chain as far as available evidence permits.

For one unresolved work item, sequential verification candidates should share a stable lineage:

`MECHANISM-NAME FIX · 01 · specific repair`

`MECHANISM-NAME FIX · 02 · specific repair`

`MECHANISM-NAME FIX · GREEN · full causal verification`

The purpose is to make all repair attempts visibly part of one campaign.

A red candidate does not redefine the mechanism, weaken its falsifiers, or justify unrelated work.

---

# 17. ASCENT ARCHITECTURE

| Stage | Elevation | Primary reservoir | Primary transmission |
|---|---:|---|---|
| 1 | 280 → 300 m | elevated counterweight | helical carrier |
| 2 | 300 → 320 m | pendulum gravitational energy | geared hoist |
| 3 | 320 → 340 m | torsional spring | capstan |
| 4 | 340 → 360 m | elevated drive block | hydraulic displacement |
| 5 | 360 → 380 m | elevated water | waterwheel → flywheel → swing arm |
| 6 | 380 → 400 m | compressed nitrogen | regulated piston |
| 7 | 400 → 420 m | counterweight | rope / sheave elevator |
| 8 | 420 → 450 m | spinning flywheel | geared carrier wheel |
| 9 | 450 → 520 m | compressed gas | pneumatic launch + coast |
| 10 | 520 → 560 m | elevated dense fluid | escapement + screw |
| 11 | 560 → 600 m | accumulator / captive ram | ram → hub → pendulum |
| 12 | 600 → 640 m | 8 tonne elevated weight | variable-pitch helical cam |
| 13 | 640 → 680 m | hydrostatic / buoyant state | displaced water |

---

# 18. STAGE 1 — GRAVITY-DRIVEN HELICAL CARRIER
## 280 m → 300 m

### Functional role

The first mechanism establishes the tower's fundamental grammar:

> **something heavy falls, therefore the player rises.**

### Occupied system

Player:

`80 kg`

Carrier, guide rollers, restraint frame:

`140 kg`

Total lifted mass:

`m_load = 220 kg`

Counterweight:

`340 kg`

### Helical geometry

Axial advance:

`P = 2.00 m/revolution`

Required lift:

`20 m`

Required shaft rotation:

`10 revolutions`

Drive spool radius:

`R = 0.300 m`

Counterweight travel:

`x = 10 × 2π × 0.300`

`x = 18.85 m`

Use nominal 1:1 rotational drive between spool and helical carrier shaft.

### Energy

Carrier:

`E_load = 220 × 9.81 × 20 = 43.16 kJ`

Counterweight:

`E_source = 340 × 9.81 × 18.85 = 62.87 kJ`

At approximately 75% effective efficiency:

`E_usable ≈ 47.15 kJ`

The stage has modest but positive margin.

### Torque

Ideal carrier torque:

`τ = m g P / 2π ≈ 687 Nm`

Approximate required input at 75% efficiency:

`≈ 916 Nm`

Counterweight spool torque:

`340 × 9.81 × 0.300 ≈ 1001 Nm`

The drivetrain remains torque-positive.

### Trigger

Player mass deflects the carrier suspension approximately 20–30 mm.

That physical movement withdraws the counterweight pawl.

### Governor

A shaft governor applies a friction band.

The counterweight remains the only positive power source.

The governor may oppose motion.

It may not accelerate the carrier.

### Terminal

Carrier enters upper receiver  
→ brake application increases  
→ drive clutch disengages  
→ structural dog seats  
→ load transfers into tower  
→ dog travel operates Stage 2 sear.

The counterweight receives its own compliant lower stop after drivetrain separation.

---

# 19. STAGE 2 — GRAVITY PENDULUM HOIST
## 300 m → 320 m

### Source

Pendulum mass:

`1500 kg`

Length:

`15 m`

Powered sweep:

`90° → 15° from vertical`

Vertical bob descent:

`Δh = 15(cos15° - cos90°) ≈ 14.489 m`

Energy:

`≈ 213.2 kJ`

### Occupied load

`200 kg`

Lift work:

`200 × 9.81 × 20 = 39.24 kJ`

Energy supply is abundant.

End-of-sweep torque is the controlling requirement.

### Drum closure

Drum radius:

`0.80 m`

Required drum turns:

`20 / (2π × 0.80) ≈ 3.979 rev`

Pendulum rotation:

`75° = 0.2083 rev`

Required speed increase:

`3.979 / 0.2083 ≈ 19.10 : 1`

### End torque

Pendulum torque at 15°:

`1500 × 9.81 × 15 × sin15° ≈ 57.1 kNm`

After approximately 19.1:1 speed increase and 80% efficiency, available output torque remains sufficient for the approximately:

`1.57 kNm`

required at the hoist drum.

### Governing

A rotary hydraulic damper limits pendulum speed.

A one-way sprag prevents carriage rollback.

The pendulum never needs to strike bottom dead centre.

### Terminal

Tapered receiver  
→ hydraulic snubber  
→ structural pawl  
→ cable unload  
→ Stage 3 spring-dog withdrawal.

---

# 20. STAGE 3 — TORSIONAL MAINSPRING CAPSTAN
## 320 m → 340 m

### Occupied mass

`300 kg`

Lift requirement:

`58.86 kJ`

### Spring

`k = 750 Nm/rad`

Initial deflection:

`20.566 rad`

Final retained deflection:

`8 rad`

Released angle:

approximately:

`4π rad = 2 revolutions`

Released energy:

`≈ 134.6 kJ`

At approximately 70% usable efficiency:

`≈ 94.2 kJ`

The spring retains substantial preload at the terminal state.

It does not unwind to zero torque.

### Transmission

Spring shaft to capstan:

`2:1 speed increase`

Two spring revolutions therefore produce four drum revolutions.

Drum radius:

`20 / 8π ≈ 0.7958 m`

### Residual torque

At terminal preload:

`τ_spring = 750 × 8 = 6000 Nm`

After the 2:1 speed increase and approximately 82% gear efficiency:

`τ_output ≈ 2460 Nm`

Static drum requirement:

`≈ 2342 Nm`

Positive margin remains.

### Control

A centrifugal governor operates the brake.

A ratchet prevents reverse travel.

The final approximately 3 m progressively increase brake torque.

### Terminal

Brake ramp  
→ drum clutch disengagement  
→ residual spring captured behind ratchet  
→ carriage receiver seats  
→ receiver withdraws Stage 4 block restraint.

---

# 21. STAGE 4 — GRAVITY-HYDRAULIC DISPLACEMENT LIFT
## 340 m → 360 m

### Source

Drive block:

`5000 kg`

Drop:

`4 m`

Potential energy:

`196.2 kJ`

Input piston area:

`4.0 m²`

Output piston area:

`0.8 m²`

### Volume closure

Input displacement:

`4 × 4 = 16 m³`

Output stroke:

`16 / 0.8 = 20 m`

### Pressure

Drive-block gauge pressure:

`49,050 / 4 ≈ 12.26 kPa`

Output force:

`12.26 kPa × 0.8 m² ≈ 9.81 kN`

Occupied carriage:

`400 kg`

Weight:

`3.924 kN`

There is substantial positive force margin.

### Flow governor

Unrestricted acceleration is unacceptable.

Meter-out flow is limited to approximately:

`0.8 m³/s`

Output velocity:

`1.0 m/s`

Input block velocity:

`0.20 m/s`

Nominal stroke duration:

`20 s`

Excess hydraulic power is dissipated primarily across the restriction.

### Hydraulic implementation

Authoritative state must include:
- chamber volume;
- piston displacement;
- pressure differential;
- flow restriction;
- fluid inventory;
- leakage if significant;
- end cushioning.

No direct player-force assignment is permitted.

### Terminal

Final metre enters hydraulic cushion  
→ upper dog seats  
→ carriage load transfers structurally  
→ Stage 5 sluice linkage completes its stroke.

---

# 22. STAGE 5 — WATERWHEEL / FLYWHEEL / CONTROLLED SWING ARM
## 360 m → 380 m

### Water reservoir

Water mass:

`3000 kg`

Effective head:

`12 m`

Available energy:

`353.16 kJ`

### Flywheel

`I = 25,000 kg·m²`

Target:

`ω = 3.5 rad/s`

Stored energy:

`153.1 kJ`

At approximately 50% water-to-flywheel conversion:

required water energy is approximately:

`306 kJ`

which fits under the available hydraulic head.

### Arm geometry

Passenger radius:

`25 m`

Counterweight radius:

`5 m`

Passenger carrier:

`250 kg`

Counterweight:

`900 kg`

Starting arm:

horizontal

Target angle:

`53.13°`

Passenger rise:

`25 sin53.13° = 20 m`

Counterweight fall:

`5 sin53.13° = 4 m`

Passenger PE increase:

`49.05 kJ`

Counterweight PE release:

`35.32 kJ`

Net static gravitational requirement:

`13.73 kJ`

### Static torque envelope

Maximum gravity opposition occurs at the horizontal start:

`(250×25 - 900×5)g ≈ 17.17 kNm`

A provisional approximately `20:1` reduction at 80% efficiency would require about:

`17.17 / (20 × 0.80) ≈ 1.07 kNm`

at the flywheel merely to overcome initial static gravity.

Actual clutch torque must also accelerate the authored arm inertia.

Therefore final clutch rating is `SOLVER-FIT`.

### Transmission

The force path is:

water fall  
→ waterwheel  
→ one-way clutch  
→ flywheel  
→ torque-limited clutch  
→ reduction gearbox  
→ arm shaft  
→ occupied swing arm.

### Terminal

Over-centre receiver  
→ hydraulic angular snubber  
→ arm dog seats  
→ clutch opens  
→ structural receiver owns the load  
→ dog linkage shifts Stage 6 pilot.

---

# 23. STAGE 6 — TWO-PHASE PNEUMATIC ASCENDER
## 380 m → 400 m

### Reservoir

High-pressure nitrogen receiver:

approximately:

`10 MPa`

`0.5 m³`

The reservoir feeds the actuator only through a regulator.

### Cylinder

Area:

`0.30 m²`

Stroke:

`20 m`

Occupied mass:

`180 kg`

### Powered phase

First 12 m:

`ΔP ≈ 8.0 kPa`

Force:

`2400 N`

Weight:

`1766 N`

Net:

`634 N`

Acceleration:

`≈ 3.52 m/s²`

Velocity after 12 m:

`≈ 9.19 m/s`

### Braking phase

Final 8 m:

`ΔP ≈ 2.715 kPa`

Upward force:

`≈ 815 N`

Gravity exceeds pneumatic thrust.

Approximate downward net acceleration while still travelling upward:

`≈ 5.29 m/s²`

The ideal trajectory approaches zero speed near the top.

Exact zero is not required.

### Terminal redundancy

Final approximately 1 m includes:
- trapped-gas cushioning;
- progressive elastomer stop;
- capture dog.

It must tolerate variation in:
- gas temperature;
- seal friction;
- payload;
- regulator response.

### Pneumatic model

Track:
- reservoir gas mass;
- reservoir pressure;
- actuator chamber volume;
- chamber pressure;
- mass flow;
- valve state.

Pressure evolves from actual gas state.

No valve directly assigns acceleration.

### Handoff

Upper dog  
→ approximately 120 mm linkage pull  
→ Stage 7 counterweight restraint withdraws.

---

# 24. STAGE 7 — GOVERNED COUNTERWEIGHT ELEVATOR
## 400 m → 420 m

### Masses

Occupied cage:

`400 kg`

Counterweight:

`445 kg`

Equivalent rotating inertia:

approximately `40 kg` translational equivalent

Effective accelerated mass:

`885 kg`

Driving imbalance:

`45 kg`

Force:

`441 N`

Initial acceleration:

`≈ 0.499 m/s²`

### Governor

Target cage speed:

`≈ 2.5 m/s`

Without governing, the cage would reach approximately:

`4.47 m/s`

over 20 m.

Distance required to reach 2.5 m/s under ideal initial acceleration:

`≈ 6.26 m`

The remaining travel is governor-limited.

### Upper transition

Final approximately 4 m use a large cam rail.

Follower rollers progressively rotate the cradle instead of the carrier striking a tipping object.

Guide rollers carry side load.

Rope remains primarily tensile.

### Terminal

Cam transition  
→ speed reduction  
→ upper pawl capture  
→ hoist unloading  
→ cradle completes Stage 8 clutch-pawl motion.

---

# 25. STAGE 8 — STORED-INERTIA GONDOLA WHEEL
## 420 m → 450 m

### Flywheel

`I = 30,000 kg·m²`

`ω_initial = 4 rad/s`

Stored energy:

`240 kJ`

### Passenger carrier

`350 kg`

Vertical gain:

`30 m`

Required gravitational work:

`≈ 103 kJ`

### Geometry

Carrier-wheel radius:

`15 m`

Half revolution:

`180°`

Vertical rise:

`30 m`

### Transmission

Provisional reduction:

`20:1`

Carrier-wheel maximum gravitational torque near side position:

`350 × 9.81 × 15 ≈ 51.5 kNm`

Assuming approximately 85% gearing efficiency, flywheel-side clutch torque must be at least roughly:

`51.5 / (20 × 0.85) ≈ 3.03 kNm`

plus rotational acceleration demand.

### Energy extraction

The flywheel must visibly slow as the gondola rises.

The player should be able to see energy leaving the reservoir.

### Terminal

Clutch unloads  
→ caliper brake engages  
→ wheel speed falls  
→ index dog enters receiver  
→ basket remains gravity-levelled  
→ transfer gate opens mechanically.

---

# 26. STAGE 9 — REGULATED PNEUMATIC LAUNCH TUBE
## 450 m → 520 m

This is the longest and fastest occupied stage in this band.

It therefore receives one of the strictest proof burdens.

### Capsule

`480 kg`

Cylinder area:

`1.50 m²`

Total rise:

`70 m`

Nominal powered region:

approximately `55 m`

Nominal coast region:

approximately `15 m`

### Ideal coast requirement

For 15 m under gravity alone:

`v_cut = sqrt(2gh) ≈ 17.16 m/s`

Ideal powered net acceleration:

`≈ 2.675 m/s²`

Ideal piston differential pressure:

`≈ 4.00 kPa`

Ideal actuator work:

`≈ 329.6 kJ`

### Energy-margin correction

`329.6 kJ` is essentially the exact ideal mechanical requirement.

It is **not** an acceptable reservoir budget by itself.

Real operation requires margin for:
- seal friction;
- aerodynamic drag;
- regulator loss;
- gas cooling;
- guide friction;
- terminal correction.

Therefore:

> **4.00 kPa is an analytical reference, not a frozen operating pressure.**

The reservoir must contain materially more usable energy than the ideal trajectory consumes.

### Reservoir

A several-cubic-metre receiver around approximately `2.5 MPa gauge` is a plausible order-of-magnitude source.

Final sizing must be derived from:
- required downstream gas mass;
- permissible pressure droop;
- polytropic expansion;
- regulator capacity.

### Cut-off

A real piston-position mechanism closes the main supply valve.

After cut-off, the capsule coasts.

No height-threshold propulsion switch is authoritative.

### Upper correction zone

The stage does not depend on reaching exactly 520.000 m at exactly 0.000 m/s.

The upper approximately 2 m contain:
- air dashpot;
- compliant docking carriage;
- capture dogs.

Overspeed is absorbed.

Slight underspeed is caught lower and completed through finite mechanical travel.

### High-speed collision law

At approximately 17 m/s, the capsule travels roughly:

`0.19 m`

per 90 Hz physics tick.

Terminal geometry must therefore have meaningful thickness and appropriate continuous collision handling.

---

# 27. STAGE 10 — SEALED DENSE-FLUID ESCAPEMENT SCREW
## 520 m → 560 m

### Working fluid

Mass:

`8000 kg`

Effective drop:

`15 m`

Available gravitational energy:

`≈ 1.177 MJ`

The fluid remains in a closed industrial circuit.

### Mass-flow closure

Target stage duration:

approximately `66.7 s`

If the full 8000 kg inventory participates once per ascent, average mass flow is approximately:

`120 kg/s`

This ties visible operation to finite flow rate.

### Escapement

Cycle frequency:

`≈ 1.2 Hz`

Reduction:

`10:1`

Output:

`0.12 rev/s`

`7.2 rpm`

### Lift screw

Pitch:

`5 m/rev`

Eight revolutions:

`40 m`

Nominal carriage speed:

`0.60 m/s`

### Payload

`620 kg`

Required lift energy:

`≈ 243.3 kJ`

The source contains substantial margin for:
- chamber impact;
- gearing;
- bearings;
- governor loss;
- turbulence.

### Terminal

Coarse-thread terminal collar  
→ drive separation  
→ hold pawl  
→ follower unload  
→ Stage 11 pilot shift.

---

# 28. STAGE 11 — CAPTIVE-RAM MOMENTUM PENDULUM
## 560 m → 600 m

No high-speed projectile may directly strike the occupied carrier.

### Captive ram

Mass:

`600 kg`

Target velocity:

`46 m/s`

Kinetic energy:

`634.8 kJ`

Acceleration stroke:

approximately `6 m`

Average acceleration:

approximately:

`176 m/s²`

Average required drive force:

approximately:

`106 kN`

This high acceleration occurs entirely inside restrained machinery.

### Energy receiver

The ram enters a long rack capture.

The rack converts linear ram energy into:
- hub flywheel energy;
- hydraulic accumulator pressure;
- controlled braking loss.

The ram remains captive.

### Occupied pendulum

Mass:

`800 kg`

Arm length:

`50 m`

Required rise:

`40 m`

Required angular displacement:

`θ = acos(0.2) ≈ 78.46°`

Potential-energy increase:

`313.92 kJ`

### Transfer efficiency target

At approximately 60% ram-to-controlled-drive efficiency:

`≈ 381 kJ`

remains usable.

This leaves roughly:

`67 kJ`

beyond static gravitational work.

### Torque requirement

Pendulum gravitational torque:

`τ_g = m g L sinθ`

Near the terminal angle this approaches roughly:

`385 kNm`

Therefore energy arithmetic alone is insufficient.

The hub transmission must produce this torque.

A high-ratio reduction or hydraulic rotary stage is mandatory.

Its final ratio must be chosen from:
- real arm inertia;
- target angular velocity;
- hub flywheel speed;
- clutch torque;
- efficiency.

### Governing

The hub engages through finite torque.

The occupied arm accelerates over seconds.

It is never struck.

### Terminal

Curved docking receivers:
- remove drive torque;
- increase hydraulic braking;
- align the carriage;
- capture structural load.

Capture then:
- seats Stage 12 cam follower;
- releases Stage 12 weight brake.

Both physical conditions must exist before Stage 12 can move.

---

# 29. STAGE 12 — GRAVITY-DRIVEN VARIABLE-PITCH HELICAL CAM
## 600 m → 640 m

### Occupied carriage

`1200 kg`

Rise:

`40 m`

Potential-energy requirement:

`470.9 kJ`

### Drive weight

`8000 kg`

Drop:

`20 m`

Energy:

`1.570 MJ`

### Drive spool

For four rotations over a 20 m cable payout:

`R = 20 / 8π ≈ 0.7958 m`

Use direct 1:1 spool-to-cam rotation.

Four spool revolutions therefore produce four cam revolutions.

### Variable-pitch correction

A constant `10 m/rev` pitch conflicts with a flattened terminal groove.

Therefore use a variable-pitch rise law.

Reference profile:

First `3.5 revolutions`:

`10.5 m/rev`

Travel:

`36.75 m`

Final `0.5 revolution`:

pitch decreases approximately linearly from:

`10.5 m/rev → 2.5 m/rev`

Average final pitch:

`6.5 m/rev`

Final travel:

`3.25 m`

Total:

`36.75 + 3.25 = 40.00 m`

Now terminal flattening and total travel agree.

### Torque envelope

Maximum nominal pitch:

`10.5 m/rev`

Corresponding ideal axial-load torque:

`τ = F P / 2π ≈ 19.67 kNm`

Drive-weight spool torque:

`≈ 62.45 kNm`

Substantial margin remains for friction and governing.

### Governor

An independent shaft governor limits cam speed.

The 8 tonne source weight must never free-fall.

### Terminal

Variable pitch lowers axial speed  
→ upper fork captures carriage  
→ follower unloads  
→ carriage rests structurally  
→ terminal linkage operates Stage 13 flood pilot.

---

# 30. STAGE 13 — CONTROLLED BUOYANCY SHAFT
## 640 m → 680 m

### Capsule

Displaced volume:

`3.0 m³`

Total occupied mass:

`2400 kg`

Equivalent displaced water mass:

`3000 kg`

Gross buoyancy:

`29.43 kN`

Weight:

`23.54 kN`

Net static upward force:

`5.886 kN`

### Simple rigid-body acceleration

Ignoring added fluid inertia:

`a = 5886 / 2400 ≈ 2.45 m/s²`

### Added-mass correction

A body accelerating through water also accelerates surrounding fluid.

For a bluff capsule, an effective added mass on the order of half the displaced water mass is a useful first engineering estimate:

`m_added ≈ 1500 kg`

Effective initial inertial mass:

`≈ 3900 kg`

Approximate initial acceleration:

`5886 / 3900 ≈ 1.51 m/s²`

The exact added-mass coefficient depends on actual capsule geometry.

### Drag-limited velocity

Representative frontal area:

`≈ 2.515 m²`

Representative drag coefficient:

`C_d ≈ 0.47`

Simple quadratic terminal velocity:

`≈ 3.16 m/s`

Actual velocity depends on shaft clearance and capsule geometry.

### Shaft geometry

A shaft internal diameter around approximately `2.6 m` gives roughly:

`5.31 m²`

cross-sectional area.

Over 40 m:

approximately:

`212 m³`

gross water-column volume before subtracting capsule displacement and internal structure.

A `500 m³` elevated reservoir provides comfortable volume margin.

### Hydrostatic pressure

At 40 m:

`ρgh ≈ 392 kPa gauge`

The lower structure must therefore tolerate roughly four atmospheres of additional hydrostatic pressure.

This applies to:
- doors;
- seals;
- valve bodies;
- inspection plates;
- shaft joints;
- capsule seals.

### Flood sequence

#### FLOOD
Stage 12 terminal pilot begins opening the main water gate.

Upper vents remain open.

Capsule remains physically restrained.

#### EQUALIZE
Shaft fills.

A float or hydrostatic differential linkage confirms sufficient water level.

#### RELEASE
Only the physical equalization state permits the hold-down latch to withdraw.

#### ASCEND
The capsule rises because buoyancy exceeds weight.

It is not secretly propelled by incoming flood water.

#### CAPTURE
Upper flare increases drag and lateral control.

Hydraulic bumper absorbs remaining motion.

Mechanical collar captures the capsule.

### Handoff beyond 680 m

The upper filtration collar becomes the physical causal origin of the next ascent band.

Nothing beyond 680 m activates merely because the player crosses an altitude.

---

# 31. COMPLETE 280 m → 680 m CAUSAL SPINE

## Stage 1

player loads carrier  
→ counterweight restraint withdraws  
→ weight descends  
→ helical carrier rises  
→ dock dog captures

↓

## Stage 2

dock dog pulls pendulum sear  
→ pendulum falls  
→ gearbox drives drum  
→ carriage rises  
→ hydraulic receiver captures

↓

## Stage 3

receiver releases spring clutch  
→ spring unloads partially  
→ capstan lifts carrier  
→ brake governs  
→ upper fork captures

↓

## Stage 4

fork releases gravity block  
→ block pressurizes hydraulic circuit  
→ displaced fluid raises ram  
→ cushion arrests ram  
→ top linkage opens sluice

↓

## Stage 5

water descends  
→ waterwheel spins  
→ flywheel charges  
→ clutch feeds swing-arm gearbox  
→ arm rises  
→ receiver captures

↓

## Stage 6

receiver shifts pneumatic pilot  
→ regulated nitrogen enters cylinder  
→ carriage accelerates  
→ regulator changes pressure regime  
→ gravity decelerates  
→ capture dog seats

↓

## Stage 7

dog pulls counterweight latch  
→ counterweight falls  
→ elevator rises  
→ governor brakes  
→ cam rotates cradle  
→ upper pawl captures

↓

## Stage 8

captured cradle operates clutch pawl  
→ flywheel engages gearbox  
→ carrier wheel turns  
→ gondola rises  
→ brake and index dog capture

↓

## Stage 9

indexing receiver admits capsule and operates pilot  
→ low-pressure pneumatic force accelerates capsule  
→ mechanical cut-off closes supply  
→ capsule coasts  
→ upper correction system captures

↓

## Stage 10

capture opens dense-fluid gate  
→ fluid descends  
→ escapement meters energy  
→ screw rotates  
→ carriage climbs  
→ terminal collar captures

↓

## Stage 11

collar shifts ram pilot  
→ captive ram accelerates  
→ rack captures ram energy  
→ hub stores / transmits energy  
→ finite clutch torque drives pendulum  
→ progressive dock captures

↓

## Stage 12

dock seats follower and releases weight  
→ weight rotates variable-pitch cam  
→ carriage climbs  
→ cam pitch falls  
→ upper fork captures

↓

## Stage 13

fork operates flood pilot  
→ shaft fills  
→ hydrostatic release becomes valid  
→ hold-down withdraws  
→ buoyancy raises capsule  
→ upper collar captures.

No stage needs supernatural knowledge that a previous puzzle was “completed.”

The next mechanism becomes available because the previous mechanism physically arrived.

---

# 32. NATIVE IMPLEMENTATION CONTRACT

For every mechanism, the native simulation owns:

## Dynamic bodies
The actual moving masses.

## Constraints
Hinges, guides, pulleys, ropes, sliders, catches, clutches, and limits.

## Reservoir state
Mass, elevation, spring deflection, pressure, volume, flywheel angular velocity, fluid level.

## Control geometry
The real mechanism that opens, closes, engages, or releases the power path.

## Force generation
All consequential forces and torques.

## Capture
Real physical constraint state.

## Failure
Stall, overspeed, insufficient energy, disconnected transmission, missed capture, or other legitimate consequence.

Godot receives and presents state.

It does not own an alternate version of the machine.

---

# 33. FLUID MODELING STANDARD

Full CFD is unnecessary.

Decorative fake fluid is also insufficient.

Use the lowest-order authoritative model capable of preserving the relevant causal physics.

## Hydraulic systems

Track:
- pressure;
- volume;
- flow;
- restriction;
- piston displacement;
- piston velocity;
- force.

## Pneumatic systems

Track:
- gas mass;
- reservoir volume;
- reservoir pressure;
- chamber volume;
- chamber pressure;
- mass flow;
- regulator state;
- valve state.

Where appropriate, use an ideal-gas or polytropic approximation rather than assigning pressure directly.

## Free-water inventories

Track explicit conserved volume between reservoirs.

Tank + pipe + bucket + basin water must balance within simulation tolerance.

## Buoyancy systems

Track:
- water level;
- submerged volume;
- buoyant force;
- drag;
- added mass;
- guide contact;
- capsule velocity.

---

# 34. FINAL DEFINITION OF DONE

A ScraperX ascent mechanism is complete only when all of the following are simultaneously true:

**Its stored-energy source physically exists.**

**Its source contains enough usable energy.**

**Its source remains restrained until the correct mechanical trigger occurs.**

**The previous mechanism physically produces that trigger.**

**A continuous transmission connects source to load.**

**Its geometry produces the claimed displacement.**

**Its swept volume is collision-valid.**

**Its mass and inertia ledger closes.**

**Its force and torque envelope closes.**

**Its occupied motion remains within the intended acceleration and speed envelope.**

**Excess energy has an identifiable destination.**

**Movement comes from the declared source rather than an undeclared motor or script.**

**Visible geometry agrees with collision geometry.**

**The player rides the true moving support and inherits its motion.**

**Terminal approach physically dissipates residual motion.**

**A real catch transfers the terminal load into structure.**

**Removing the source prevents success.**

**Breaking the transmission prevents success.**

**Removing the capture prevents the next stage from activating.**

**A plausible mechanical failure mode exists.**

**A plausible rearming path exists.**

**Native state and Godot presentation agree.**

**The complete mechanism passes positive proof and destructive falsifiers.**

Only then is it GREEN.

---

# 35. THE SCRAPERX MACHINE DOCTRINE

The tower should ultimately become readable without explanation.

The player sees a descending weight and understands the rising cage.

They see the rope enter the drum.

They see the shaft enter the gearbox.

They see the governor touch the brake.

They see a cylinder connected to a valve block.

They see the pressure vessel feeding that valve.

They see a flywheel slow as machinery extracts energy.

They see water leave one reservoir and appear in another.

They see a catch physically enter its receiver.

They see that receiver pull the next restraint.

Looking backward through hundreds of metres of machinery, the tower should tell its own story:

> **That moved because this fell.**  
> **This turned because that pulled.**  
> **That pressure existed because this reservoir was charged.**  
> **This machine stopped because that brake absorbed its energy.**  
> **The next machine woke because this one physically arrived.**

That is the defining property of ScraperX.

Not merely physics objects.

Not merely interconnected puzzles.

Not merely spectacle.

**A continuous, inspectable chain of physical consequence.**

The governing rhythm is:

> **STORE → RESTRAIN → RELEASE → TRANSMIT → MOVE → GOVERN → DISSIPATE → CAPTURE → HAND OFF**

Everything else is steel around that idea.
