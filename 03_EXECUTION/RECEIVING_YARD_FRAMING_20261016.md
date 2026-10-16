# Receiving yard framing — 2026-10-16

Implemented on `ChatGPT` at base `658c8d2` in `godot/presentation/freight_yard_arrival.gd`. Scope is the physical receiving-yard composition visible from the normal arrival `(-94,1.2,-44)`. Source inspiration: `origin/ScraperX-Claude` / `505ca38ed7baf1ebccfd5914122224ac72c1b2ff`, `main_world.gd::_build_stock_yard`: recognizable containers, sleepers, layered beams and plate stock. Its coordinates and texture generation are not imported.

The arrival now looks into a working stock street: a two-tier west container mass, two separated east stores and supported material stock terminate the empty apron at a readable human scale. A broad central yard remains available for driving, recovery and loading. The existing forklift stays in the open foreground; the crane remains the distant destination. Stock roofs, skins, ribs, corner rails, door rods, handles, I-beam flanges/webs and sleepers all use the existing exported `YardStock` / `YardStructure` classes. There are no new Godot physics bodies, lights, textures, process callbacks, dynamic state or materials.

## Added solid bounds

All coordinates are native/world meters. Bounds include visible ribs, rails and locking bars; labels and flush paint do not establish collision. Values below are source-derived, before root's independent Godot export.

| Assembly | X bounds | Y bounds | Z bounds | Solid boxes |
| --- | --- | --- | --- | --- |
| West lower 12.2m container | -113.29 .. -110.71 | 0 .. 2.6 | -78.10 .. -65.835 | 59 |
| West upper 12.2m container | -113.29 .. -110.71 | 2.6 .. 5.2 | -78.10 .. -65.835 | 59 |
| East 12.2m container | -76.29 .. -73.71 | 0 .. 2.6 | -75.10 .. -62.835 | 59 |
| East 6.1m container | -76.29 .. -73.71 | 0 .. 2.6 | -97.05 .. -90.885 | 43 |
| West nine-I-beam stack | -112.4 .. -109.6 | 0 .. 1.38 | -98 .. -90 | 33 |
| East five-plate stack | -76.7 .. -73.3 | 0 .. 1.04 | -111.2 .. -104.8 | 7 |
| **Total added solids** | | | | **260** |

Containers use 120mm floor/roof skins, 80mm side/end skins, continuous edge rails and full-height corner posts. Stacked corner posts bear directly on the lower corners at 2.6m; roof skins meet side skins. Corrugation ribs touch the wall skin and also export as collision. Three 12.2m units have 16 ribs per side; the shorter unit has eight. These are closed containers, with no claim of usable interiors or moving doors.

The nine long beams have actual I sections: two 60mm flanges and a 60mm web, in three tiers on timber cross-sleepers. Each sleeper tier bears on the preceding beam flanges. The plate stack has five 160mm layers supported by two 240mm-high timber bearers; no hovering stock or unsupported rack shelves.

## Retained operating spaces

Placement was checked against `warehouse_forklift_yard.cpp`, `yard_crane_geometry.cpp`, `tests/freight_yard_arrival_tests.cpp`, `tests/forklift_route_walk.hpp`, `tests/forklift_public_driver.hpp`, and `tests/yard_crane_route_tests.cpp`. This is source placement reasoning, not runtime acceptance.

- No added solids intersect the retained truck turn corridor X[-102,-86], Z[-58,-47.5], the starting truck near(-100,-56), its ordinary doorway approach(-98,-57.65), or exchange cradle(-88,-70).
- Between the new stock rows, X[-109.6,-76.7] remains free of added steel. Existing receiving frame/cargo remain native authority inside the yard. The documented ordinary truck transit near X=-100 toward Z=-160, its west-to-east turn south of the crane, and pickup(-80,-150) do not pass through added stock.
- The nearest new steel to the crane is north of Z=-111.2. The crane pivot is(-60,36,-150), with35m boom reach: even its northmost boom/load operating region starts at Z=-115. This also leaves the actual booth(-66.5,-150), its western approach, rigging and elevated dock mouths clear. No overhead stock structure crosses a mast or crane route.
- The east side includes a four-meter painted service strip X[-85,-81], between truck aisle and stores. Dashes span Z[-135.2,-56.8]; three chevrons reinforce travel toward negative Z. The strip remains flush/non-solid. Open gaps between the east stores and stock allow lateral walking and cargo access; this paint is wayfinding rather than a replacement for native routes.

## Geometry and rendering budget

Added: 260 solid box meshes, 46 flush `YardPaint` boxes, three labels. Existing two canopy spotlights and all existing shared materials remain unchanged. New static geometry stays inside `FreightArrival`, which already carries `static_render_container`; root's static batching/export path remains authoritative. Every new rigid visible item uses an already classified solid part name. No giant wheels, repeated giant structural spans, per-item resources or new fake climbing surfaces.

## Evidence and handoff

Worker inspection: `git diff --check -- godot/presentation/freight_yard_arrival.gd` passed. Reviewed native source bounds and known public-route waypoints once. The worker did not run Godot, native compilation, route replay, Android build or device play. Root owns independent export part counts, native integration, actual rendered opening and public-route acceptance. These checks are required before claiming delivered gameplay or final visual acceptance. Other dirty/untracked work was preserved; only the assigned presentation source and this document were written.
