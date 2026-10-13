# Claude reference — native physics/movement reuse

Reader: claude_native_reuse, GPT-6.1 Sol/Medium, read-only. Reference branch ScraperX-Claude at27cedcaa648c5d2a8ad19edb8a9e67af5313a1d1; target ChatGPT d37b793f5e6c12a0338b59552acda68350b3a338. Source/test implementation inspected; no execution or phone observation. Target authority and current native physics contract remain governing.

## Candidate worth retaining

**Contact-backed beam recognition:** reference `src/sim/simulation.cpp:4103` falls back from a missed capsule-center ray to actual recorded support contact before recognizing a narrow beam. Target `src/sim/simulation.cpp:8120` immediately returns on center-ray miss. Possible minimal reuse is target `beam_underfoot()` using existing `support_sample_`, raycasts and `LeafBox`; never manufacture grounding. Rejecting cases: actual edge contact retains classification; lost contact returns invalid; adjacent broad floor does not become a beam. Target width cutoff0.50m and near-horizontal check mean this is not automatically applicable to the proposed0.55m sloping girder. Root must establish an affected actual case before editing those limits or claiming a movement fix.

**Receiver braking experiment:** reference `tests/simulation_tests.cpp:4467` compares neutral release and deliberate reverse input at measured distances from a real edge. Borrow that acceptance pattern for473m landing: early braking succeeds, late braking overruns, short jump reaches real462m recovery footing. Reuse target's existing finite traction/jump/recovery owners (`simulation.cpp:5917`,6029,5987); do not port Claude's velocity-assigned edge prediction/control at2635,2931,2994,3055 as another authority.

## Rejected duplication / unresolved concept

Traction tram already exists in target `src/sim/supplied/traction_tram.cpp:5` (reference `src/sim/vertical/traction_tram.cpp:5`). Jump support momentum, vault continuity and render interpolation also exist, with target parkour-flow tests; no new capability is earned by copying them.

Reference cable payout/load transfer (`mechanism_kit.hpp:268`, `mechanism_kit.cpp:473`) changes mass through`apply_mass()` without establishing complete transported-mass momentum/work closure in the inspected path. Retain as a possible future mechanism concept requiring causal reconstruction, not a ready transplant.

Next use: author the473m receiver through target owners, borrow the braking falsifier, and revisit contact-backed recognition only if a real beam case demonstrates the target seam. Root integrates; no bulk branch merge.
