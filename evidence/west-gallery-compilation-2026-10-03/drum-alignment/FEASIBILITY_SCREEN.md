# Loaded tongue: contact geometry before integration

Private compilation note, 2026-10-03. Production source inspected: IssisX/ScraperX `ChatGPT`, `0ecf34196c59d9e521f11e529e3da65644ec29a9`. This is an analytical screen, not native contact or ordinary player proof.

## Selected experience

From supported inspection footing, align a sliding loading tongue. Its actual obstructing geometry holds a loose drum on a sloped rack. Withdrawal opens the rolling path; gravity drives the drum into the bascule pan. The player observes or boards reachable moving geometry, then crosses the supported leaf and climbs onward. Show the drum path, pivot, counterweight and arrest seats together from the handle position. Preserve the mixed +121→187 m layout.

Use a shallow rack and one integral, rounded, downhill-relieving tongue edge as the first contact candidate. The bevel progressively permits forward travel during lateral withdrawal. Dimensions and guide/contact topology must be solved before treating this as a compiled assembly. No powered reset is specified; drum position and arrest compaction persist.

## Source and geometry distinctions

- MEASURED source: player is an 85 kg dynamic upright Jolt capsule; cylinder half-height 0.55 m plus radius 0.35 m gives total half-height 0.9 m. Native body friction is zero; native locomotion supplies support-relative traction. A floor friction coefficient alone cannot establish planted effort in this controller.
- MEASURED source: existing handle uses a hard PointConstraint, released after >900 N for two ticks. Ordinary locomotion writes velocity. This establishes neither a continuous actuator force cap nor a work limit.
- CHOSEN prior screen: 360 kg solid drum, radius 0.60 m and axial width 1.10 m; 60 kg tongue; 1.50 m channel; initial lateral offset +0.35 m; final offset +0.10 m; 0.25 m withdrawal; guide resistance 35 N.
- DERIVED ideal clearance: (1.50−1.10)/2 = 0.20 m. Under a translated-opening interpretation, the initial obstruction overlaps one drum end by 0.15 m. Release therefore occurs after 0.15 m withdrawal, leaving 0.10 m of alignment travel. This corrects the advisory example that assumed an initial 0.25 m offset.
- UNRESOLVED: whether authored rounded collision faces implement this translated opening; collider margins, drum yaw, guide loading, opposing contacts, loaded receiver height and release transient.

## Conditional force and energy screen

For a flat stop normal to the downhill rack axis, with its reaction through the drum center, no yaw/pinch and loaded guide resistance already included:

    N_stop = m g sin(theta)
    F_pull = 35 N + mu_face N_stop + 60 kg a_lateral
    W_pull = 35 N * 0.25 m + mu_face N_stop * 0.15 m

These equations are conditional equilibria, not upper bounds for multiple contacts. A world-vertical stop has a different normal load. Concentrating the reaction near one drum end introduces yaw and guide forces.

Retaining the prior chosen 2.0 m path and 0.18 m descent gives theta=5.163607° and N_stop=317.844 N:

| Chosen face friction | Constant-speed pull | Ideal total withdrawal work |
|---|---:|---:|
| 0.30 | 130.3532 N | 23.05298 J |
| 0.60 | 225.7064 N | 37.35596 J |
| 0.90 | 321.0596 N | 51.65894 J |

A CHOSEN 250 N effort target rejects the 0.90 corner even before acceleration or wedging. Meeting that corner with this flat-face model requires theta≤3.878634°. Do not certify the old friction band or silently discard that corner. Either solve a shallower rack with sufficient onward energy or demonstrate an actual relieving contact geometry with the retained slope.

The ideal solid-cylinder rolling model gives axial inertia 64.8 kg m², downhill acceleration 0.5886 m/s², exit speed 1.534405 m/s and available energy 635.688 J. This arrival energy must enter the drum/pan/arrest accounting; it cannot disappear at transfer. Rolling/slip, impacts and actual released height remain native-model obligations.

For a bevel q→x(q), gravity contributes m g sin(theta) dx. Player work must include guide/contact dissipation, changed kinetic energy and grip elastic energy, minus that released gravitational energy. Actual contact slip includes drum rotation; using lateral travel as every contact's slip is invalid once geometry changes.

## Integration gate

Close one actual loaded withdrawal using the production-shaped gravity-on player, finite compliant grip, bounded control effort/work, real supporting contact and genuine drum/tongue/guide collisions. Record peak force, positive command work, stored elastic energy, travel, drum yaw and release outcome. A chosen 250 N force/80 J positive-work screen is a candidate target, not a character-wide stamina redesign or an observed result. The existing hard carry constraint and velocity controller cannot be used as that proof.

For the flexible approach, 11 m of elevation costs at least 9172.35 J of player gravitational work before losses. Breeze supplies sway through drag, not free upward climbing. New hand-mode command work must be explicit at the native owner.

Advisory source: Super_Agent session `sess_0f79cb21f74e3f7b006ac17a74fd948195bf35cb3cb56db030`; arithmetic recomputed locally against the actual prior offset and dimensions. Advisory geometry remains unverified.
