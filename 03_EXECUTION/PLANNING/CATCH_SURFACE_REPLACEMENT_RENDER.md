# Catch canvas presentation replacement

2026-10-13; source base `ChatGPT` / `d37b793f5e6c12a0338b59552acda68350b3a338`. Presentation owner: `godot/presentation/native_catch_net_view.gd`; bridge snapshots and derived tether endpoints remain native-owned.

## Current presentation

The real bridge constructs independent packed vertex/index snapshots. Presentation consumes actual interpolated native positions and triangle indices in one `Canvas` MeshInstance3D / ArrayMesh surface: 41x41 points at 0.3m spacing, 1681 vertices, 9600 indices and 3200 triangles. It supplies no collision authority or authored replacement geometry. The native 150kg membrane and its 160 perimeter attachments are represented with matching visible clips.

Clockwise area-weighted smooth normals and UV-derived deformed tangents shade the actual surface. UVs are captured from native triangle axes at topology initialization and remain fixed through deformation/restoration. A cloned aged-fiber material receives two static 256px mipmapped woven albedo/normal maps, including shading-only reinforcement bands; the supplied shared material is preserved. These bands do not implement the requested reinforced attachment patch, which remains a follow-up.

Changed snapshots rebuild the indexed mesh and bounds from actual vertices; identical snapshots return before mesh updates. Removed bodies hide presentation. Native four-corner open-weave topology retains the two rounded MultiMesh batches and the 2500-vertex legacy fallback. Topology changes rebuild children. First-snapshot UVs assume the native generator's nondegenerate first triangle.

The orange 20mm ground cord uses the new native getter's derived endpoints, including membrane centre `(20,20)` and actual finite-frame local anchor `(-34,0.4,-150)`. Its native massless tension-only law has 2.5m rest length, 600N/m stiffness, 300N·s/m damping and zero slack force. The image does not establish discretized rope collision or an elastomer material model.

## Current observed integration

Lead-reported real current bridge execution passed 66,749 checks across five poses with actual-input grade exit and onward cargo approach. Host inspection used Godot day/LOW and softpipe. Current native model, force, 36m fall/exit and adverse-contact gates also passed as recorded in the [native implementation note](CATCH_SURFACE_REPLACEMENT_NATIVE.md). Captures and execution evidence reside in `evidence/canvas-contact-resolution-2026-10-13/` and private host workspace `/data/data/com.termux/files/usr/tmp/scraperx-membrane-candidate`.

The first host llvmpipe renderer crashed with SIGILL; softpipe resolved the rendering failure. The host used a private ARM GDExtension mapping, with the original Android mapping unchanged. This is host runtime evidence; normal GitHub/Android GREEN remains pending. Loaded mobile mesh-upload/frame cost and phone performance remain unmeasured.

## Historical isolated fixture

The earlier isolated Godot 4.7 fixture at `/data/data/com.termux/files/usr/tmp/catch-canvas-presentation-check` passed on the superseded 25x25/625-vertex, 3456-index, 1152-triangle candidate. It checked one surface, upward smooth normals, metre UVs, deformation/live bounds, restoration with fixed UVs, removal, legacy fallback and shared-material preservation. A fixture-only mutable packed-array getter was corrected to return independent snapshots matching the real bridge; production assertions were not weakened.

Those fixture and syntax checks describe historical presentation behavior, not current 41x41 runtime acceptance. No phone, APK, cost or full-system energy claim follows from them or from host rendering.
