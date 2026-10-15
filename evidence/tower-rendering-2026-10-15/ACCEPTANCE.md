# Tower rendering improvement — 2026-10-15

Excess submissions in whole-tower views are demonstrated. Root implemented compatible runtime spatial batches, exact local-triplanar instancing, varied foliage instance dimensions/tints, native cable instancing, and80m small-instruction text visibility. Physics, collision geometry, structural triangles, foliage segments and shadow settings are retained. Existing source primitives/part metadata remain for export; runtime sources have zero render layers. Live moving native bodies remain independent. No general batching implementation existed in the published Claude branch; its scene/Kit history differs.

## Actual matched render evidence

Godot4.7.stable.official.5b4e0cb0f, GLCompatibility/Mesa26.0.8 softpipe,160×120, project4096shadow atlas. Render-only production scene construction plus actual initialized native Kit; no gameplay stepping in the profile. Baseline Main includes unpublished west-apron geometry; both sides retain that same geometry. The performance-only publication excludes that unrelated blockout, and its actual Main startup is checked separately. Private bridge is b26e59bb…, not a claim of an exact newly rebuilt Android/native graph.

| Profile/view | Baseline draw calls | Final draw calls |
|---|---:|---:|
|2000m view/800m detail (shipping distance defaults), ground up|4311|1036|
|Same, top down|2893|766|
|2600m/unlimited decorative-detail stress, ground up|4311|1036|
|Same stress, top down|3597|933|

Upward strict<1000 remains36over. Do not call the whole-world budget complete. These counts are approximately76%/74% lower in the shipping-distance cases; they do not establish phone FPS, peak memory or that the reported force-close is gone. Resolution/view/renderer/device matter. Exact poses, sources and logs are in the paired receipts; historical intermediate candidates are retained rather than relabeled final.

## Affected correctness checks

- Actual GL static batch regression PASS: real exported collision entries unchanged; transformed triangles/UVs/normals retained including nonuniform scale; original local-projected geometry instanced at the same poses; all normalized foliage vertices within20µm and authored instance tints retained; dynamic/translucent/shader and distant-cell exclusions.
- Actual native cable regression PASS420checks over4native advances: real endpoints, current bounds, compact visible count, no extra simulation stepping or duplicate source render layers.
- Prospective selective-publication Main startup3frames PASS, exit0, no script/parser errors. First attempt lacked private linux.arm64 extension mapping (environmental); restoring the established private host mapping resolved it. No production export/config workaround was introduced.
- Sol/high independent source review found no blocking defect. Global cable instancing trades individual frustum culling for one small thin-segment batch; spatial grouping changes culling granularity. Actual paired counts capture these costs.

Commands: Godot tests under `proot-distro login ubuntu -- env GALLIUM_DRIVER=softpipe LP_NUM_THREADS=1 xvfb-run -a ... --path <private-project> --script res://tests/static_world_batch_test.gd` and `kit_cable_batch_test.gd`. Headless Dummy MultiMesh transform readback cannot establish actual rendered instance poses; use an actual GL display. Reusable profile script `tests/tower_render_profile.gd`, options `--tower-batch=on|off`, `--shipping-defaults`, `--inventory-only`.

Focused checks are wired into candidate CI after matching host import. Publication/artifact state is tracked in the compact live checkpoint; no candidate/fullGREEN/device claim comes from this document alone. New movement/terrain/forklift/coupler/crane and existing native hook WIP remain separate and preserved.
