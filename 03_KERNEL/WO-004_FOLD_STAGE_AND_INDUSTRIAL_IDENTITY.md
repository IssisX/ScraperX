# Work Order 004 — Fold Stage and Industrial Identity

## Objective

Unlock the game from a phone-safe rectangle and replace the neon placeholder look with a sourced industrial one:

`real device bounds → viewport, frustum and HUD sized to the unfolded inner panel → industrial material and sourced light → a frame that reads as a working plant at night`

This work order owns composition and material identity. It does not add gameplay.

## Existing proven truth

At source commit `f06ef520e95202eb70449de8b80325378accb609` the project was configured `display/window/handheld/orientation=1` (portrait) at a 1600x900 design viewport with `stretch/aspect` unset, a 78 degree camera and a **400 m far plane**. The tower the GDD requires is 1.6 km tall, so the far plane alone clipped the subject. Presentation used saturated emissive greens and cyans on ordinary structure.

## Governing authority

- Governing Law 28 — Galaxy Fold 6-class Android is the shipping truth; interface design targets sustained real-device play.
- Governing Law 26 — no physics theater; emissive candy on inert edges misrepresents which surfaces are doing work.
- GDD §7.1 — first-person perspective exists to maximise embodiment, height, machine scale and physical readability.
- GDD §7.3 — traversal readability comes from world geometry and material context.
- GDD §15 — motion comfort is solved through camera behaviour and presentation, not by flattening height.
- TDD §1.1 — Godot 4.7.x Mobile/GL-compatibility renderer is the host.

## Device truth

Galaxy Z Fold 6, checked 2026-09-17 against current published specifications: inner display **2160 x 1856** (20.9:18, ~1.164:1, 7.6 in); cover display **2376 x 968** (22.1:9, 6.3 in). The inner panel is the primary composition; the cover is secondary.

## Owner

Godot presentation owns viewport configuration, camera frustum, HUD layout, materials and lighting. It owns no consequential state.

## Allowed seam

`godot/project.godot`, `godot/main.tscn`, and `godot/presentation/main.gd`.

## Forbidden shortcuts

- A fixed design resolution letterboxed into the device bounds.
- HUD positions expressed as constants tuned for one screen size.
- Emissive materials used as a substitute for lighting.
- Widening the field of view so far that architecture distorts.
- Raising the far plane without addressing depth precision on the GL-compatibility renderer.

## Proof path

1. Source inspection shows the design viewport at 2160x1856, `stretch/mode=canvas_items`, `stretch/aspect=expand`, and a landscape-sensor orientation rather than forced portrait.
2. The Godot runtime prints its actual viewport size, aspect, field of view and far plane, and CI asserts 2160x1856, aspect 1.164, and `stretch=expand`.
3. The captured frame is rendered at the Fold inner-panel aspect, not 16:9.
4. Source inspection shows the HUD laid out from the measured viewport rectangle and re-laid out on `size_changed`.
5. Source inspection shows the palette expressed as mill scale, oxidised steel, poured concrete, wet asphalt, galvanised mesh, faded warning yellow and chipped hazard orange, with emission restricted to sodium fittings, a fire box, a vent glow and lit floor bands.

## Completion

Complete when all proof steps pass from one source commit. This proves composition and identity in a desktop GL-compatibility runtime at the Fold aspect. It does **not** prove the real panel, touch ergonomics, sustained frame rate or thermals.

## Result record

- **Changed:** `godot/project.godot` (Fold design viewport, expand stretch, sensor-landscape orientation, clear colour), `godot/main.tscn` (industrial environment, sodium-weighted fog and tonemap, anchored HUD, 82 degree / 2600 m camera), `godot/presentation/main.gd` (industrial material set, sourced light rig, viewport-derived HUD layout).
- **Built:** Linux `scraperx_native` GDExtension, Release, GCC 13.3, `-Werror`.
- **Executed:** `tools/godot/godot --path godot --rendering-method gl_compatibility` under Xvfb at 1728x1485 (Fold inner-panel aspect).
- **Observed:** `SCRAPERX_WO004_VIEWPORT_PROOF width=2160 height=1856 aspect=1.164 fov=82.0 far=2600 stretch=expand`.
- **Unverified boundary:** the real Fold 6 panel, folded/cover composition, touch ergonomics, sustained frame rate, thermals, HDR and colour management on device.
- **Regressions:** none observed.
