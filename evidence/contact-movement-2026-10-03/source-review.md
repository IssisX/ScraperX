# Independent source review at the pause boundary

Existing movement architect `/root/west_gallery_interaction_depth` reviewed the uncommitted native rigid-hand module, Simulation integration, CMake registration and new tests against base `543d13e04c615af6869d46826214878446513200`. This was read-only source review; the reviewer ran no builds or runtime tests.

The reviewer found a P1 ownership defect: `set_hand` could attach a rigid hand while gravity-off cargo-net climbing remained selected, and initial rigid acquisition could select a visual-only net hand before the first physical constraint existed. Unrestricted hand candidates and a one-direction-only movement guard supported those paths. Neither path was reproduced at runtime.

Parent repaired the selected-backend boundary: `grip_matches_climb_backend` compares candidate and principal hold before either hand attaches. Initial hands use the compatible selected fallback; regrip and carrying candidates reject cross-backend transitions before mutation; invalid physical acquisition retains the previous hand and its generation. The subsequent source recheck reported the P1 closed and no remaining scoped blocker. It found gravity-on rigid dispatch, reciprocal limits, real foot samples, passive departure and reset/destruction ordering supported in source.

`SCRAPERX_WITH_JOLT=OFF` is already unsupported at the inspected base: Simulation explicitly requires pinned Jolt and existing sources include it unconditionally. No no-Jolt repair was made or claimed.

Soft coupling, Hanging, receiving mantle, powered jump and full positive-work/energy closure remain documented boundaries. Passing component checks and this review do not complete the wider movement goal.
