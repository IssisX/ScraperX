# Native timber material inspection — 2026-10-09

Final source rendered successfully with Godot 4.7 Compatibility / Mesa softpipe at 648×432. `latest/` contains the accepted final images and raw receipt. The paired comparison checks 12 real plank cells at four views (48 samples): vertex, base-normal and triangle-index hashes and recorded native physical/render origins plus physical basis-Y are identical to their baseline counterparts. All presentation nodes follow the native render transform. The final material's fracture-mask uniform matches the actual native mask on all cells.

The open-interface image shows real neighboring cells 2884/2885 at native tick 434: seam 4, mask 112, physical endpoint separation 0.04101567 m. The existing staged 3 m fall supplies the load through native gravity/contact; there is no forced fracture, fragment relocation, manual force or velocity assignment. Presentation interpolation remains active, so the physical and rendered origins are separately recorded rather than assumed equal.

Useful pairs: `baseline-detail/wood_close_unloaded.png` against `latest/wood_close_unloaded.png`, and `baseline-open/wood_actual_open_interface_close.png` against `latest/wood_actual_open_interface_close.png`. The supported-end view retains the real steel occlusion; it does not expose an invented unsupported original end. The whole approach view establishes scale. Lighting and cameras are diagnostic and constant between each pair; the renderer selects only steel entities 1935/1936 and all twelve actual timber entities 2880–2891. Physics remains the full normal native world.

`baseline/` is the original edge-on inspection. `current/` preserves the first smooth/pale material result. `final/` preserves the regular-band intermediate, despite its historical directory name. `latest/` alone is the frozen accepted source. No phone frame budget, full Tower/player POV, earned route or final art approval is inferred from these pictures. Softpipe VSync and proot shared-memory notices appear in raw logs; each completed engine run exited 0 without shader/parser errors.

## Tested identities

- Native bridge: `8c5f73ee23e77f24ab09d3842e3a95da7d1942b5425b2931649475b47671d20c`.
- Final KitView: `394ee5658a7bfe3d77d18d4933993db396531a8807de61ba6ac895cd36c31201`.
- Final plank shader: `a319b5808f91ad35b64f0e4a649ecf4529ce386bea003e769bf98288e271e225`.
- Shared slingshot shader unchanged: `4598952b87ef1464ea9d3ea709aeebb7245698bdb959dbb541915ac04c820cca`.

The final shader changes fragment lighting normals only. Base mesh normals, vertices and indices remain unchanged. UV/UV2 coordinate/face codes are intentionally excluded from the geometry digest.

## Reproduction

The exact already imported private runtime is under `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-visual/latest`. Run:

```sh
sh /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-visual/run_visual.sh latest
```

`latest/command.txt` records the expanded engine invocation. `reproduction/` preserves the exact runner, all used harness variants, project settings, extension descriptor, and old/final material sources. The final harness is `wood_visual_final.gd`, installed in the private runtime as `wood_visual_probe.gd`. To reconstruct elsewhere, copy the existing imported Godot project support files, retain its `assets`/imported refractory material dependencies and registered extension list, install these project/harness/material files, and provide the bridge matching the manifest at `addons/scraperx_native/bin/linux/libscraperx_native.so`. KitView also preloads `presentation/materials/refractory_material.gd`, whose original assets/imports must remain available. No native binary, image import cache or engine is duplicated into this evidence folder.

`paired-comparison.json` records rejecting comparisons; `sha256.json` covers the archived files. Raw per-run logs, exit codes, source manifests, receipts and scripts are preserved for reproducibility. No further render campaign was run after the accepted final source.
