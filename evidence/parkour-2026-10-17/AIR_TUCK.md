# Physical aerial tuck — native implementation acceptance

Base source:27a5ea86417b067665a444ae47569f91028fc7f9. Changed native owner: `src/sim/simulation/locomotion.cpp`; source SHA256 `502cc05d8ff90e1f4476da1a437ec8524e8fc80b87c8d571c5a938ecbeb6c56c`. Coherent73-owner baseline is retained at `/data/data/com.termux/files/usr/tmp/scraperx-hang-intent-ib_dhuxi`; only the changed locomotion TU replaces its matching archive member. No World layout change or unrelated hook-gate work is included.

Airborne Crouch changes the existing capsule from0.9m to0.6m half-height around unchanged body center. Air extension tests full standing clearance at that exact center. It cannot shift the body, change velocity, supply takeoff work or expand into ground/obstacles. Existing grounded planted-feet stance behavior,85kg mass, gravity and actuator limits remain. Flush route paint identifies a real existing duct crossing and onward ground exit; paint creates no support.

## Observed commands and results

Strict native variant compile/archive: `/data/data/com.termux/files/usr/tmp/scraperx-air-tuck-juisw8ui/commands.json`, result `build-result.json` exit0, archive SHA256 `4df4e3b20fdbc1170ffbf4d46ad84cd98548c4fdf10865f7296a9bcf07efb4fe`.

Actual new-test compile/link/runs: `/data/data/com.termux/files/usr/tmp/scraperx-hang-intent-ib_dhuxi/air-tuck-test-commands.json`; corrected route command `air-tuck-route-corrected-commands.json` in the same directory. Logs `air-tuck-flight.log`, `air-tuck-blocked.log`, `air-tuck-route-corrected.log` each exit0:

- `AIR_TUCK_FLIGHT samples=60 peak_position_delta_m=0 peak_velocity_delta_mps=0`; paired repeated tuck/release also requires identical finite actuator work and no extra support/takeoff.
- `PASS AIR_TUCK mode=blocked`; actual grade blocks full airborne expansion; actual contact then permits ordinary standing.
- `AIR_TUCK_ROUTE duct_top_m=1.65 contact_tick=214 stand_tick=215 grade_tick=289`; real sprint/jump, compact receiving contact, clear supported stand and onward actual ground landing. Only initial grade staging is used; no body writes during the route.

Affected existing landing modes: `landing-commands.json` and `landing-results.json` in the aerial scratch. Jump and guards exit0. Original flow comparison failed because equal COM starting height now gives the tucked player0.3m more actual sole descent:10.2188m/s versus9.919m/s impact, with matching physical impulse difference. `landing-probe-commands.json`/`landing-probe-flow.log` record that independent diagnosis. Corrected flow fixture matches5m sole descent (compact COM5.6m, standing COM5.9m); all original impact/impulse tolerances remain. `landing-equal-descent-commands.json`/`landing-equal-descent-results.json` exit0: normal9.919m/s, prepared response2,5.39647m/s after0.2s. Jump/guards source paths were unchanged by this flow-only fixture correction; their existing results are reused.

The first duct test incorrectly required a moving player to remain on a3m roof for30ticks. It had already reached actual compact roof support and clear standing, then ran off. The corrected check observes real supported stand, then requires the onward grade landing rather than permanent roof occupancy. No input or physics tuning changed.

## Boundaries

This is native behavior and strict build evidence. Route paint needs the matching Godot/Android CI artifact. Device feel, appearance and frame cost are unverified until played; no phone FPS or full-release claim is made.
