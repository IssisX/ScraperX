# Ground ascent: Archimedes screw to weight-driven lift

**Status:** DESIGN TARGET; neither mechanism is implemented or runtime verified.
**Write branch:** `ChatGPT`.
**Purpose:** Preserve one connected ground-to-+8 m ascent contract while building and proving one mechanism per work cycle. This is a proposed B00 route change, not a revision of historical `AS-001`–`AS-003` evidence. The Atlas and relevant ascent ticket must be reconciled with the route before code is called complete.
**Priority:** Ground mechanism rehabilitation before further `AS-006` upward construction. Restore the current required proof path to GREEN first.

## Source and design boundary

At inspected commit `11c21d116c9d95ae2b60f418474a1ab02b7dccb5`, the moving belt is a level kinematic deck whose position is `z = -96 + 9 sin(0.40t)` at `y = 1.38`. The 4 t crate starts at `x = 0, z = -112.2`, outside the deck's `x = 4..8` width. The ramp reaches the crane controls from grade. The implemented crate → gate → stair route reaches approximately +24 m, and the separate weight-driven swinging stair reaches about +40 m. A simulator test asserts a belt ride to the hook cage; the player's report that standing on the belt did not carry them remains a conflicting runtime observation to reproduce. This proposed water route neither repairs nor silently retires that defect.

The Archimedes screw is the **first mechanism**: electric shaft rotation transports conserved water from the apron basin to an upper tank. The receiving **second mechanism** is a controlled water-weight lift. It consumes the raised water and carries the player from 0 to a stable +8 m landing. Both belong to one ascent stage; finish them in separate work cycles. A full tank alone is not player ascent.

## First mechanism: Archimedes screw — NEXT WORK CYCLE

**Input:** a finite basin with at least 2.0 m³ available above the screw's minimum inlet immersion, player-accessible start/stop controls, and a torque-limited motor. The basin and upper tank water inventories must have one native simulation owner. The inlet must remain immersed during the claimed delivery; water does not appear by timer or mission state.

**Transfer:** inclined trough, shaft, flights, bearings, reducer, and supported tank. Initial targets: 30° inclination, 11 m flighted length, 5.5 m maximum discharge elevation, outside diameter 0.70 m, core diameter 0.20 m, pitch 0.60 m, 22 rpm, and upper tank capacity 2.0 m³ with floor near +5 m and maximum water level near +5.5 m. Derive actual effective displacement from this geometry, immersion, trough gap, leakage, and outlet backpressure. Nominal target `D_eff = 0.060 m³/rev` gives `Q = D_eff × n/60 = 0.022 m³/s`, or 2.0 m³ in 90.9 s **only if flow stays nominal**. Low inlet level, outlet submergence, a full tank, or a blocked outlet must alter flow through the producing physics.

**Work budget:** `rho = 1000 kg/m³`, `g = 9.81 m/s²`; provisional worst-head `H = 5.5 m`, hydraulic efficiency `eta_p = 0.55`. At nominal flow, `P_shaft >= rho g Q H / eta_p = 2.158 kW`; `omega = 22 × 2 pi/60 = 2.304 rad/s`; `T_shaft >= P/omega = 0.937 kN·m`. Target a finite 1.5 kN·m shaft rating at 22 rpm and verify the motor/reducer power curve, startup, loss, and stall. A 2.0 m³ delivery needs at least `rho g V H / eta_p = 196.2 kJ` at this conservative constant head; changing water levels require integration of actual head and power. These are sizing targets, not measured performance.

**Output port to next mechanism:** 2.0 m³ of measured, conserved water in the supported upper tank, near +5 to +5.5 m, with a real outlet above the receiving bucket near +4.5 m. Record mass, tank position, accessible control, outlet geometry, and any residual basin volume. Do not implement the lift in this work cycle.

**First-mechanism proof:** operate from the real apron using normal controls; measure shaft angular speed and torque, basin loss, upper tank gain, leakage/backflow, and work. Falsify with motor off, insufficient inlet level, under-torque, blocked outlet, full tank, and reversed or stopped motion. Inspect the rendered screw, water, supports, collisions, and state-driven sound. Run affected regressions and a delivery build. Record observed runtime separately from source and design targets.

## Second mechanism: water bucket and player lift — PLANNED / NOT BUILT

**Receiving input:** the same 2.0 m³ (`2000 kg`) water in the upper tank. A physical valve at the upper tank fills a bucket held near +4.5 m; the valve cannot create mass or open the lift by a completion flag. The bucket and its catch must support its full wet load before release. The player boards a guided cage at 0 m and releases the brake/catch from a reachable station after filling.

**Motion:** target dry bucket mass 200 kg; target empty cage mass 500 kg plus 85 kg player. A 2:1 rope/pulley constraint couples a 4 m bucket descent (+4.5 → +0.5 m) to an 8 m cage ascent (0 → +8 m). Check the actual rope path, anchor loads, slack, sheaves, guides, swept clearance, and support-point velocity. Motor/valve flags cannot move the cage. A finite governor/brake limits speed; physical travel stops and a loaded upper catch permit the player to step onto a fixed +8 m landing.

**Energy check:** bucket gravitational release `(2000 + 200) × g × 4 = 86.33 kJ`; cage plus player requires `(500 + 85) × g × 8 = 45.91 kJ`. At provisional transmission efficiency `eta_l = 0.70`, available energy is 60.43 kJ, leaving 14.52 kJ for modeled losses and arrest. Verify inertias, friction, acceleration, governor dissipation, and actual loads; if they exceed that margin, change physical ratings or geometry before implementation. A smaller fill or heavier cage must fail by force/energy, not by an arbitrary gate.

**Reset and handoff:** after the cage is caught at +8 m, drain the bucket at +0.5 m by gravity into the ground basin and let a controlled cage descent raise the now-empty bucket to its upper catch. Prove that loss, drainage, and rearming conserve water and account for energy. The +8 m dock must meet real tower geometry and a legal subsequent route; inspect the existing gate, stairs, crane, outside climb, and checkpoint behavior. If this lift makes a new bypass, update Atlas B00 and the active ascent ticket explicitly rather than pretending the old gate still controls all ascent.

**Second-mechanism proof, later cycle:** no water/no lift; partial fill gives force-limited behavior; rope disconnect, brake, overload, and travel stops have causal effects; the player rides as a moving support through normal controls and stands on the fixed +8 m dock. Only then may the ground-to-+8 m stage be called complete.

## Repository handoff discipline

This file holds the paired contracts and statuses, not implementation proof. The project's `00_START_HERE.md` points here while ground rehabilitation has priority. Execute the screw first; then update this same file with measured output and the receiving lift's re-derived input before opening the lift cycle. Keep one mechanism in progress. Preserve existing identifiers and historical proof records.