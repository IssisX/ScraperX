# Work Order 007 — Kellerworks Identity and Alpine Backdrop

## Objective

Replace the ad hoc WO-004/005 night-urban dressing with the real visual identity:

`overcast alpine daylight → mountain/waterfall backdrop → Kellerworks signage and material identity → a decorative gear/drum motif driven by real machine phase → a scale-telegraphing crane`

Presentation only. No consequential state changes.

## Existing proven truth

WO-004..006 (commit `92f12f6`, CI run 35227271830, green including Android arm64 build and APK export) proved a Fold-aspect viewport, an exterior grade spawn 120 m from a 1600 m tower mass, and a fully coupled, falsifier-verified steam plant. The backdrop, palette balance and lighting built for that work order were the author's own placeholder guess (no reference existed yet) and used a night-dominant sodium urban yard.

## Governing authority

- `01_PRODUCT_AUTHORITY/support/06_VISUAL_IDENTITY_KELLERWORKS.md` — the product decision this work order implements.
- Governing Law 26 — no physics theater; the decorative gear/crane must never be mistaken for a second authority.
- Governing Law 28 — Fold-class Android is the shipping truth; new geometry must stay presentation-only and cheap (flat-shaded distant meshes, no new physics bodies, no new textures beyond procedural `StandardMaterial3D`).
- GDD §1 — the tower is "built from large-scale, life-size machinery"; the crane and exposed gear serve that reading, not decoration for its own sake.
- GDD §2 — core fantasy is climbing an *industrial* megastructure; alpine backdrop and Kellerworks branding do not change the fantasy, only where it's staged.

## Owner

Godot presentation only (`godot/presentation/main.gd`, `godot/main.tscn`). No native, bridge, or test changes. Every native entity position/size referenced by the mirror functions is unchanged from WO-006.

## Forbidden shortcuts

- Any new collidable geometry (mountains, waterfall, crane, banners, gear are all non-authoritative set dressing, explicitly outside the native Jolt world).
- The decorative gear claiming to be, or being read as, the real machine authority — it is driven *from* `machine_cycle_phase_seconds`, one-way, and documented as decorative.
- A copied or trademark-imitating logo; the Kellerworks mark is an original geometric chevron-K.
- Regressing the Fold viewport, HUD layout, or any WO-001..006 native accessor call.

## Proof path

1. Source inspection shows the environment moved from night-dominant (background ~0.15 value, ambient 0.6, Overcast light 0.34) to overcast daylight (background/ambient/directional energy raised, fog cooled).
2. Source inspection shows non-collidable mountain and waterfall geometry added as `TowerPresentation` children only, never referencing a Jolt body or entity ID.
3. Source inspection shows Kellerworks banners rendered with real `Label3D` text nodes carrying the propaganda copy from the identity doc, and a chevron-K mark built from primitives.
4. Source inspection shows the tower-face gear/drum rotation computed from `_native.get_machine_cycle_phase_seconds()`, with a code comment stating it is decorative.
5. The Godot runtime still loads the extension, completes the existing WO-002/003/006 CI proof sequence unmodified, and captures a frame showing the new backdrop, signage and lighting.
6. Native host tests are unaffected (not rebuilt, not required to be — no native file touched) and remain green from the last build.

## Completion

Complete when all proof steps pass and the captured frame shows overcast daylight, mountains, a Kellerworks banner with real slogan text, and the phase-driven gear, without altering any WO-001..006 proof assertion.

## Result record

- **Changed:** `godot/presentation/main.gd` (alpine backdrop, Kellerworks signage, phase-driven gear motif, crane, lighting rebalance to daylight), `godot/main.tscn` (environment/fog/directional-light daylight values); new `01_PRODUCT_AUTHORITY/support/06_VISUAL_IDENTITY_KELLERWORKS.md`.
- **Built:** no native rebuild required (presentation-only change); prior Linux GDExtension binary reused unchanged. CI run [35286733368](https://github.com/IssisX/ScraperX/actions/runs/35286733368) on this exact commit confirmed the full pipeline green, including the Android arm64 cross-compile and APK export (unchanged inputs, but exercised and passing).
- **Executed:** `tools/godot/godot --path godot --rendering-method gl_compatibility` under Xvfb, same `--ci --capture=` sequence as WO-006.
- **Observed:** at close range (78 m from the tower face, the WO-006 proof vantage) `KELLERWORKS_wordmark`, the `LIFT A / CAGE 3` sign, the `PEOPLE POWER PROGRESS` banner, timber cladding, sodium accent lamps, and the crane with its hanging crate all render correctly under overcast daylight; every WO-004..006 proof line is unchanged: `viewport 2160x1856 aspect=1.164 stretch=expand`, `tower_face_distance=78.0 tower_height=1600`, `peak_valve=0.73 peak_lift=7.93 shut_flow=0.00000`. From a wider approach vantage (28 m out instead of 66 m, temporary camera constants used only for this observation and reverted before commit — working tree diff confirmed clean against the committed values), the mountain backdrop and the `HIGHER STRONGER FURTHER` banner are clearly legible, confirming the near-tower proof frame's flat mountain visibility is real angular occlusion by a 120 m-wide tower at 78 m range, not a rendering defect. Two real defects were caught and fixed before this: the initial daylight pass overexposed the whole frame (directional light 2.4 + ambient 1.15 under `gl_compatibility` tonemapping), and the initial fog density (tuned for the ~150 m tower) reduced 700-1300 m mountains to near-zero transmittance; both were corrected and reverified.
- **Unverified boundary:** Android install/execution, Fold 6 panel observation, touch ergonomics, sustained frame rate/thermals with the new geometry, and whether the new meshes hold their triangle/light budget on-device (not measured, only eyeballed for restraint).
- **Regressions:** none intended; WO-001..006 native/bridge/test surface is untouched.
