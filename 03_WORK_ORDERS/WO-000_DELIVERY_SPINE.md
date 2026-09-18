# SCRAPERX — WORK ORDER 000 — DELIVERY SPINE

**Status:** READY

## Objective
Prove the real delivery spine:

`Godot 4.7 app → ScraperX GDExtension loads → authoritative native fixed-step simulation advances → Android arm64 APK is produced`

No gameplay-system implementation belongs in this work order.

## Existing truth
- Product design is frozen in the GDD package.
- Engineering architecture is frozen in the TDD baseline.
- No ScraperX implementation/runtime proof is claimed by this package.

## Authority
- `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md`
- `01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md`
- `02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md`
- TDD sections 1, 3, 4, 20, 21, and 24.

The atlas is not required for this work order. Do not build kernel geometry here.

## Owner
Native/Godot integration and build-delivery spine.

## Allowed seam
Thin GDExtension bridge around portable `scraperx_sim`.

## Required causal path
`Godot process/input loop → bridge → fixed-step native sim tick counter/state → snapshot/readback → visible diagnostic → Android arm64 export`

## Forbidden shortcuts
- No fake JavaScript/web substitute.
- No Godot-only dummy counter presented as native simulation.
- No prebuilt APK unrelated to current source.
- No gameplay mechanics added to make the demo look impressive.
- No claim of installation/execution/Fold behavior unless actually performed.
- No custom engine fork unless GDExtension is proven insufficient.

## Implementation scope
- minimal Godot project;
- native `scraperx_sim`;
- thin GDExtension bridge;
- CMake build;
- Android arm64 native artifact;
- Godot Android export configuration;
- minimal diagnostic presentation proving native state advances.

## Out of scope
- parkour;
- Jolt gameplay integration;
- structure;
- rigging;
- machinery;
- process systems;
- NPCs;
- missions;
- final UI/art.

## Proof path
1. Native host test proves fixed-step state advances.
2. GDExtension builds and is loadable by the Godot project.
3. Godot runtime visibly reads native-authoritative state.
4. Android arm64 native library builds.
5. Godot export produces a real APK from current source.
6. If device access exists: install, boot, observe native state advancing.
7. Keep APK production, installation, execution, and Fold verification as separate claims.

## Completion
This work order is complete when:
- the current source builds the native library;
- Godot loads the extension;
- the authoritative native simulation advances through the bridge;
- a current-source Android arm64 APK is produced;
- all performed verification is recorded without upgrading unperformed checks into claims.

Stop. Do not begin WO-001 inside this work order.
The next file in this same folder is `WO-001_EMBODIED_AUTHORITY.md`. Its world is atlas kernel `KX-DECK`, not a new document.
