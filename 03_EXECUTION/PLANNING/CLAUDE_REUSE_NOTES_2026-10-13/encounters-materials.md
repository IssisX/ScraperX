# Claude reference — encounter/material reuse

Reader: claude_encounter_material_reuse, GPT-6.1 Sol/Medium, read-only. Reference branch ScraperX-Claude27cedcaa648c5d2a8ad19edb8a9e67af5313a1d1; target ChatGPT d37b793f5e6c12a0338b59552acda68350b3a338. Actual source/test implementation only; no executed test or phone-quality claim.

## Highest-value reuse for current next encounter

Reference `src/sim/band_stack.cpp:652–662` has separate galvanized takeoff/receiver slabs, rear yellow rails, a small marked receiving edge and a perpendicular onward strip. Reuse that broken-catwalk geometry pattern at473m: real open gap, limited receiver/braking room, then a deliberate turn to the ascending girder. Adapt dimensions/supports to actual462–484m target geometry; target already has general gap routes, so this is an authored arrangement rather than a missing engine capability.

Compute the gap from collision Parts: reference comment says2.20m, while receiving lip begins at16.38m and yields2.18m physical clearance. No specific source test for that broken-catwalk jump was located. Do not import the comment as proof or blindly copy dimensions.

## Other retained arrangement

Reference `band_stack.cpp:645–647` supplies a narrow overhead beam and actual support columns. `tests/simulation_tests.cpp:6161` contains approach, grounded-balance and crossing assertions; execution is unobserved. Omit its access stairs for this crosshead. Its width comment says0.40m, but an overlapping hazard member widens the collision envelope to0.50m; author visible warning details flush to avoid changing the challenge accidentally.

## Integration boundaries

Reference constructs these in`build_c1()` and one static Kit body (`band_stack.cpp:861,1642`). Target `facade_route.cpp:217` has physical bodies/fixed joints/elastic mounts. Adopt target's native load path, mass and actual reactions, not the reference's static-body implementation wholesale. Native Parts must remain the collision/render source.

Target materials already provide entity-aware worn footing/frame/grip distinctions and shared mipmapped tread (`godot/presentation/kit_view.gd:266,656`). Use those owners for the new crosshead; no material-system rewrite was identified as necessary. Target `inspection_junction_route.cpp:121` demonstrates flush warning faces.

Next use: author the real receiver/open gap and visible support geometry, map the new Kit entities through existing materials, and demonstrate ordinary jump plus short-miss/retry. No separate broad art queue or new dispatcher.
