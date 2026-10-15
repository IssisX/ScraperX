# Main presentation modularization — 2026-10-15

Result: `main.gd` reduced from 4,920 to 2,347 lines (2,573 lines / 52.3%). Source baseline: branch `ChatGPT`, revision `dc53244d99a833888f1004af1b74a392887b34d5`. Existing work and concurrent UI/native changes were preserved. No commit or push.

## Module ownership and API

- `world_geometry.gd` (217 lines): RefCounted geometry/material factory initialized with the existing TowerPresentation Node3D. Owns three shared bump textures. Factories preserve the original meshes/materials/labels/metadata. `presentation_root` supplies original default parenting. No added scene nodes or engine callbacks.
- `stack_world_builder.gd` (798 lines): RefCounted authored stack builder. Explicit constructor inputs: geometry, existing LightRig, regression boolean. Owns stack constants, gear references and builders. API: `build`, `build_accents`, `build_tower_skin`, `build_continuation`, `get_level_count`, `rust_deepen`, `update_machine_phase(phase, period_seconds)`.
- `alpine_world_builder.gd` (237 lines): RefCounted mountain/basin/waterfall/vegetation builder receiving geometry. API: `build_backdrop`, `build_foliage`. Seeded placement, bounds, metadata and particles are preserved.
- `worksite_machine_view.gd` (1,384 lines): RefCounted authored worksite machine presentation. Explicit constructor inputs: geometry, existing LightRig, stack builder, regression boolean. Owns 53 mesh/particle/light references and 115 geometry constants. Existing build functions are called directly in their original world-construction sequence. `mirror(native, valve, flow)`, `mirror_water_screw(native)`, and `update_ambient_dressing(native, ambient_clock)` receive native state as a read-only input: no native request/mutation is added, no native state is cached. Plume frame vents use the stack owner's original `get_level_count` calculation.

Main retains lifecycle, input routing, operation/controller logic, scenario/UI callbacks, feedback, native authority integration, camera/arms and telemetry. Scene node names, parenting, order and transforms are preserved. KitView receives the same label factory Callable through `_geometry.add_sign_text`. Main's existing public `rust_deepen` delegates to the extracted implementation.

## Exact changed function/contract map

WorldGeometry moves `_add_sphere`, `_add_cone`, `_add_strut`, `_add_cylinder`, `_add_gear`, `_add_sign_text`, `_build_bump_textures`, `_make_bump_texture`, `_material`, `_add_box`, `_add_box_to`; names lose the private prefix for direct explicit calls.

StackWorldBuilder moves `_stack_level_count` → `get_level_count`, `_build_stack` → `build`, `_build_stack_accents` → `build_accents`, `_build_tower_skin` → `build_tower_skin`, `_build_stack_continuation` → `build_continuation`, plus unchanged private `_build_stack_interior_dressing`, `_draw_sagging_service_cable`, `_build_stack_megastructure`, `_build_stack_footings`, `_build_stack_halls`, `_build_stack_gearworks`, `_build_stack_lifts`, `_build_stack_jibs`, `_build_stack_signage`, `_build_stack_bridges`, `_build_stack_dressing`, and `rust_deepen`. `update_machine_phase` moves the exact original stack gear loop out of ambient dressing, receiving the unchanged phase/period.

AlpineWorldBuilder moves `_build_mountains_and_waterfall` → `build_backdrop`, `_build_foliage` → `build_foliage`, and private `_build_basin`, `_build_foothill_forest`, `_add_tree`, `_add_bush`, `_build_waterfall`.

WorksiteMachineView moves `_mirror_machine` → `mirror`, `_mirror_water_screw` → `mirror_water_screw`, `_update_ambient_dressing` → `update_ambient_dressing`, all with explicit native input (ambient also receives its existing clock). The original crane-null early return and native gear-phase ordering are preserved. The following builders retain names without the initial underscore: `_build_intake_rise`, `_build_water_screw`, `_build_legal_forty`, `_build_hook5_rack`, `_build_yard`, `_build_legacy_fixtures`, `_build_plant`, `_build_treadle`, `_build_kernel_jib`, `_build_kernel_needle`, `_build_kernel_sump`, `_build_plume`, `_build_lighting`, `_build_gear_motif`, `_build_crane`. Private `_add_barred_wall`, `_span_cable`, `_smoke_texture` move with their callers.

The two intentional retained Main orchestration changes are `_build_world` (construct explicit helpers before the original bump/palette/build sequence) and public `rust_deepen` (compatibility entry point). Other Main edits mechanically qualify calls/constants/texture ownership and supply existing native/clock inputs.

## Checks actually run

- Python full function-body equality against captured unedited source: PASS, 56 moved functions + 60 retained Main functions. Every original function is accounted for; no missing/truncated function declarations. Comparison removes blank/comment-only lines and maps explicit helper/texture/constant/root qualification back to original identifiers. Explicit native/clock parameters are mapped back to their former Main fields; the stack phase handoff is expanded to the exact original loop. No numeric expressions, mesh/material properties, control flow, body statements or ordering are discarded. The two intentional retained Main orchestration edits are excluded and reviewed directly.
- Local private helper-call resolution source assertion: PASS across Main and all four helpers. This caught the interim unqualified stack-level-count plume reference; final code calls the authoritative stack builder method.
- Public-method preservation: original public `rust_deepen` retained.
- `git diff --check` on assigned production/evidence files: PASS.

No Godot import/parser/render/native/shared build or gameplay checks were run by this worker, per parent build ownership. Parent owns combined Godot acceptance, including preload parsing and actual world/solid generation. Source equality is not runtime proof; no gameplay/device acceptance is claimed.

## Exact final source receipt

```json
{
  "godot/presentation/main.gd": {
    "lines": 2347,
    "sha256": "a98d1b757625c3a47080c9c2b59e9574e07b4e117270819c40fa79eb1a794171"
  },
  "godot/presentation/world_geometry.gd": {
    "lines": 217,
    "sha256": "6cdd2c0bb473dcba79daa62a4113afddee1aab95810a521f10f8cedc89882264"
  },
  "godot/presentation/stack_world_builder.gd": {
    "lines": 798,
    "sha256": "85778b414fed774727acc7582aec4cbfb9f35ce1938ab338ab0723ea007d9cf4"
  },
  "godot/presentation/alpine_world_builder.gd": {
    "lines": 237,
    "sha256": "62b938628a6007e72233a5d4436d6bde86e3cd0c8355f3d4da1a4e325266e6b0"
  },
  "godot/presentation/worksite_machine_view.gd": {
    "lines": 1384,
    "sha256": "e8de9b6a5684cb29bdacccf9648450254d52b70460d8260135382c3b40f2acab"
  }
}
```

## Root integration repair after worker handoff

Godot import caught original Main `add_child(_water_screw_root)` moved into a RefCounted. The final helper receives an explicit `scene_root: Node3D` constructor argument from Main and parents that native-owned machine to the original Main root. Parenting it to TowerPresentation was rejected by the actual fixture collision exporter. Both normal and fixture exports are now byte-identical to original tables; import, keyboard-core and second-hand bridge acceptance are recorded in root receipts. The hashes above describe worker handoff; final source hashes are in `source-receipt.json`. Code-linked fixture comments now point to fixture_constants.hpp.
