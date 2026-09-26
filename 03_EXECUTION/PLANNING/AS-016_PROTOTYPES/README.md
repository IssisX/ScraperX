# AS-016 bounded native prototypes

Profile: **MACRO-TRAVERSAL-STRICT**. These files preserve local design work against
the cleared foundation at `360cffb` (gameplay unchanged at `64b8014`). They are
engineering fixtures, excluded from normal gameplay and the Android package.

| Evidence | Result | Limit |
|---|---|---|
| [Swing](swing/README.md) | 27 load/friction/yield combinations, each at 90 and 360 Hz; independent converged calculation agrees within declared tolerances. | Preloaded rigid ballast, centreline arms, no moving-body contacts or player. |
| [Geometry](geometry/STATIC_WALK_README.md) | Native player walks from ordinary spawn across static bridge poses at +7.53 and +8.20 m, then reaches the existing +11 m ring. | Static supports; no mechanism loading, release or moving crossing. |
| [Releases](release/README.md) | 12 bounded observations: finite 150 N pulls release the rack and loaded prop; no-input controls hold. Actual twenty pipes reach the fixed receiver. | Separate fixtures; complete beam inertia, moving pan, sheave inertia and impact ledger remain open. |

The selected upper entry cheek is 0.25 m below the nominal bridge plane. Its
worst upward transition over the screened stopping band is 0.272183 m, leaving
77.817 mm beneath the native step limit. The earlier 0.20 m offset is retained
as rejected for inadequate margin, despite successful static walking.

The swing's entire observed post-turn tip range is +7.5356 to +8.1821 m. Peak
rigidly attached load acceleration is 0.46755 g. The finite receiver uses CHOSEN
90 kN yield ±5%, 15 MN/m stiffness and 0.65 m stroke; its largest observed
penetration is 0.50990 m. These values describe the isolated fixture, not the
unbuilt complete mechanism or measured timber material.

The independent evaluator was rerun after adding exact case-grid validation.
All 54 declared observations pass; deliberately missing, duplicated and NaN
observations are rejected. Each subdirectory records reproduction instructions
and remaining abstractions. Source, compact observations and rejected cases are
preserved; executables and generated copies of the engine are not included.

**Next gate:** actual rack pipes → physically held moving pan → loaded prop
release → finite arrest, with outboard arms and contacts enabled. Then bind the
same construction into normal native/Godot gameplay and verify ordinary input,
crossing, persistence, rendering and the exact Android candidate. No completed
mechanism, APK or device execution is claimed by these prototypes.
