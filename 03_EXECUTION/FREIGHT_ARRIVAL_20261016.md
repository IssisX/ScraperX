# Freight arrival and exact build identity

The default Godot bridge now starts the player on real grade at(-94,1.2,-44), facing the real forklift and skyline crane. This replaces the distant outward-facing opening without changing existing explicit native spawns. Walk toward the truck, around the stock, and onto its open cab floor; normal operation and the existing freight/cargo routes remain. The receiving canopy is behind the arrival apron, with physically colliding benches, stored steel, lamps and lane markings. New static structure joins existing rendering batches. Paint/cloth/lenses supply no support.

Tune layout/material values in godot/presentation/freight_yard_arrival.gd and the asphalt shader in yard_asphalt.gdshader. Any structural geometry change requires the existing clean SolidExport → world_solids.inc process; rendering alone cannot establish support. Current native export1452boxes/666hulls. FreightYardArrival is enum33; bridge uses it, native default ExteriorGrade remains intact.

The final public native path passes1022ticks: ordinary arrival, real grade walk, ordinary truck boarding,90ticks of2440footing, finite hydraulic raise, ordinary grade exit with zero step-pose writes. Actual Main/newbridge rendered test passes with both machine centers in the starting camera. Screenshot reviewed; initial oversized signage corrected.1288drawcalls at this actual opening on softwarehost is an observation, not phoneFPS or closure of the sub1000 target. Visual richness and console-grade feel remain incomplete.

Pause now displays the exact final APK BUILD/SOURCE. Candidate versionName includes source/run/attempt, replacing the reused0.1.0-dev.20261014.10 string. Stable signing embeds assets/build_info.json, validates manifest identity and all retained payload bytes; existing package/signing/monotoniccode policy is preserved. Python7tests and Godotreader/pause test passed; actual signed Android acceptance remains delivery-dependent.

Offscreen membrane canvas presentation now skips geometry/normal/tangent uploads using conservative fresh world bounds. Return to view immediately uploads fresh native geometry. Reinforcement and all physics continue; regression covers fresh re-entry, corner-outside crossing surface, camera switches, transformed owner and no-camera fallback. Device timing remains unmeasured.

Scheduler comparison rejected both candidates: same270ticks/zero deaths/identical truckcoords, current1worker22.505ms/tick,2workers27.933,0workers32.324 on proot. Current production scheduler and all Jolt material/solver/actuator laws are unchanged. Matching earlier accepted freight tests are reused; new static geometry receives focused corridor CI.

Evidence: evidence/opening-yard-2026-10-16. No console-grade, full-suite GREEN or observed Android play claim.
