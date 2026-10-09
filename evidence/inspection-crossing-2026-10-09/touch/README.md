**Archived reproduction:** unpack `tested-scripts.tar.gz` in this folder before using `prepare_runtime.py`. The archive contains only the small tested script/config snapshots; it contains no import cache or native binary. The original private harness remains at the task-root path below.

# Inspection crossing touch evidence

This private harness drives the actual normal Slingshot `main.tscn` through viewport `InputEventScreenTouch` / `InputEventScreenDrag`, unchanged InputRouter and native bridge. Its only staging operation is the accepted 396 m entry at `(-22.5,397.15,-142)`. Every later movement, Jump, Action and Drop comes through touch. Native bodies, gravity, contact and hand constraints remain authoritative.

- `baseline.log` / `baseline-receipt.json`: actual hanger catch showed misleading Jump `JUMP OFF`, Drop `DROP`, Action `CLIMB UP`. Drop released real hands and landed on the actual 407 m receiver. The private helper then turned its camera while retaining an old screen-relative stick command, causing an unintended receiver walk-off. This was a harness sequencing defect.
- `input-sequencing.log` / `input-sequencing-receipt.json`: releasing the old stick before view turns and retaining the view during near-target braking passes the primary entry396 → hanger → timber → actual418.25 floor → onward path. Production movement and InputRouter were unchanged.
- `connected-final.log` / `connected-final-receipt.json`: refreshed production presentation passes entry396 → deliberate no-Jump gap miss → actual403.5 tray → native return brace → reverse hanger crossing → forward retry → actual timber → upper brace → actual418.25 floor → stable onward footing. Final native tick8711, position `(-20.99981,419.15,-142.0047)`, support51, gravity1, zero deaths, free hands and valid checkpoint footing.

The final test holds ordinary lean input for 11–12 native ticks (~122–133 ms). It estimates the visible tangent turn from observed travel/acceleration, alternating anticipatory lead150/450 ms around300 ms nominal. Reverse release used84 held updates/1003 ticks; forward retry used12 updates/136 ticks. This is a private coarse human-pattern diagnostic, not automatic shipping control or proof of human usability. Ordinary Drop earned both releases and actual receiving contact followed.

Both real catches show Jump/Drop `LET GO` and hidden redundant touch Action. The actual native swing context also passes the direct non-touch HUD prompt check (`LET GO`, `MOVE TO LEAN`). That presentation check temporarily changes only HUD touch mode; it does not simulate a keyboard release or alter physics.

Only production `godot/presentation/ui/touch_controls.gd` was edited by this worker. Root owns `main.gd` and HUD changes; the visual worker owns KitView. InputRouter, native code and dependencies were not changed. No host builds were run. Physics bridge SHA-256 remains `7796d8e777e828bc5fcbc83fcf5ae859a4a5bda42f094d45047e3e1c5f79eca1`, matching the final plank bridge receipt. `source-start.json` records clean baseline ChatGPT `ba40e21e170b167328be1d9ebf023ca75da9c681`; each run receipt records its private script/library hashes.

Executed final command:

```sh
sh /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/inspection-touch/run_touch.sh connected-final miss
```

`command.txt` and `render-command.txt` preserve expanded invocations. Final process session77175 finished with exit0; earlier worker processes were completed or terminated.

`driver-field-error.log` records a corrected private dictionary-key mistake; `missing-private-imports.log` records missing texture imports corrected by copying matching cached imports from the established harness. Software-render attempts (`full-logical-canvas-slow.log`, `small-canvas-before-ready-disable.log`, `capture-only-software-limit.log`) were stopped because full-world X11 softpipe remained too slow, even with verified 3D disable and private648×557 canvas. No screenshot, phone performance, audio, human feel, touch damage/restart, CI or APK proof is claimed here. Separately staged visual stills belong to the visual owner's evidence.

## Reproduction without copying the import cache

Preserve `tested-scripts/` (small actual runtime source snapshots), `tested-source-manifest.json`, `inspection_driver.gd`, `prepare_runtime.py`, `run_touch.sh`, `summarize_receipt.py`, `command.txt`, `render-command.txt`, and the three route logs/receipts. The full `godot/` cache is not needed as an evidence bundle.

`prepare_runtime.py` accepts an existing fully imported production project, the bridge matching the attested SHA-256, and a fresh private target. It copies small presentation scripts plus the frozen tested overrides and bridge, reuses assets/imported textures through links, and keeps new runtime cache writes local. It refuses an existing target or a mismatching bridge. It was supplied for reproduction; no additional run was performed after final acceptance.

```sh
python prepare_runtime.py --imported-project /path/to/imported/godot --bridge /path/to/attested/libscraperx_native.so --target /path/to/fresh/private/godot
```

Then use the expanded headless command from `command.txt` with that target and add `--inspection-miss` for the final connected case. `run_touch.sh` is the exact session invocation with its original task-root path; adjust only its private `base` path when reproducing elsewhere. The renderer wrapper and command are optional environmental diagnostics, not required route acceptance.
