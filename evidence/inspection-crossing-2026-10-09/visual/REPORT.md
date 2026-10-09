# Inspection encounter appearance — bounded local evidence

Source baseline: ChatGPT `ba40e21e170b167328be1d9ebf023ca75da9c681`.
Production owner changed only `godot/presentation/kit_view.gd`; other dirty files belong to concurrent owners.
Current script SHA256: `10b817d20bf999e09b1314aafb388a4252610b8f03b503ea8a782ed5eefb1fb8`.
Confirmed native library SHA256: `7796d8e777e828bc5fcbc83fcf5ae859a4a5bda42f094d45047e3e1c5f79eca1`.

## Implemented

Scoped 1935/1936/1937/2935 finish hierarchy: cool weathered structure, light galvanised footing, quieter recovery tray, legible painted return brace, worn oxide grip/seat pins, restrained yellow passive hanger. Existing native bolts and seat geometry remain the detail. Four shallow surface-mounted maintenance stencils name407inspection,418north frame,403.5return andJ-407hanger. No arrow or release-timing cue. Timber 2880..2891 uses existing shared wood shader with weathered tan light/dark colours; original continuous grain coordinates remain attached to actual pieces. No new lights, textures, collision or simulation.

Eight shared material variants reuse supplied surface resources and one existing 128px mipmapped tread texture. New bright bolt/return finishes split two fixed-body batches; four Labels have 28m visibility limits. These counts are implementation bounds, not measured handset performance.

## Executed

- Fresh baseline 16-body X11/softpipe render: exit0, five captures.
- Current Godot 4.7 headless check-only of kit_view: exit0; `parse-current.log`.
- Current 16-body X11/softpipe render: exit0, five captures; `finished-visual.log`.
- Canonical geometry comparison with original kit_view: all 16 actual bodies / 7,644 triangle+normal tuples unchanged.
- All actual dynamic nodes match native interpolation transforms; shared wood shader enabled. Native player remains grounded on 2886 under gravity at 3s.
- Source comparison preserves original primitive builders, plank UV geometry, render transform owner and earlier1932/1933inspection material/practical functions. `git diff --check` passes.

## Inspected

`timber_seats_loaded.png`: actual fork/pin assemblies visible below ends; real loaded timber bends, longitudinal grain colour stays continuous. Segmented native surface joins remain visible.
`arrival_hanger_receiver.png`: worn grip distinguishes moving bar from background support. `recovery_return.png`: return brace lighter than recovery tray; surface maintenance identification visible. `junction_overview.png`: primary and recovery structure read as separate surfaces.

## Limits

Filtered 16-body harness omits surrounding Tower, rooftops, complete normalworldlighting and player UI. Captures are 384×256 software rendering, not phone-quality or full-route sightline proof. The connected full-route software attempt was too slow on this host. The subsequent explicitly staged production-main stills below provide full Tower context without repeating route verification. Renderer emitted existing proot --shm-helper and unsupportedVsync notices; successful script/run receipt has no failures. Native build, Android frame budget, delivery and human readability were not tested by this visual slice.

## Final full-Tower visual observations

The actual production main scene ran under the same existing Godot 4.7/X11 softpipe path, with an explicitly private 648×557 viewport and existing quality preset 0 at render scale 1.0. Actual observed shadow quality is 1; FOV is the production 82°. Full-context capture-only staging is separate from the connected headless touch proof. Forty native ticks settled each location on real expected footing, then main processing froze for two draws.

Final parser and full-world visual run both exited 0. All three captures have expected grounded support (1935/1935/1936), native/render agreement true, and failures empty. See `final-visual-receipt.json` and `full-world/fullworld-visual-receipt.json`.

The first full-context image exposed a pure-metal grip that rendered almost black under the actual Tower lighting. Final scoped diffuse oxide/wear replaces that finish without emission or new lights. Before/after images are `full-world/before-grip-oxide/staged_left407_pov.png` and `full-world/staged_left407_pov.png`. Receiver and tray stills show actual timber grain/fork seats/pins and the return brace within the Tower. Explicit initial player/camera staging means these images do not prove earned approach or control usability. Phone appearance and performance remain unobserved.
