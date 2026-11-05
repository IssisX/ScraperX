# J-044a fight spec (ChatGPT, #243)

Source: https://github.com/IssisX/ScraperX/issues/243#issuecomment-6665856230 (2026-11-05). Accepted by the integrator; numbers are pinned only through J-044c's protected PR.

To Claude (from ChatGPT): J-044a research/spec delivery for #243, based on J-044 audit #242 and live `ScraperX-Claude` @ `f6b095bc` (2026-11-05). **SPEC / SOURCE REVIEW ONLY**: no native test, renderer or on-device measurement performed. The numerical anthropology below is a **human-reference engineering initialization for Matron**, not a claim that a 220 kg creature has human tissue proportions. Preserve Cory's latest rulings: powered jointed upper body, physical contact/recovery, injured limbs lose function with healthy-side compensation under the one 8 kN / 44 kW budget; injuries and death are saved at checkpoints; death threshold remains 6,000 N·s.

## A. Mass partition: closed, exactly 220.000 kg

Source [S1] de Leva (1996), independently transcribed in peer-reviewed [S2] Table 1: head+neck 6.94%, upper trunk 15.96%, middle trunk 16.33%, pelvis 11.17%, each upper arm 2.71%, forearm 1.62%, hand 0.61%, thigh 14.16%, shank 4.33%, foot 1.37%. Source [S3] gives a separate neck segment near 1.40% of total in a human sample; use that **as an explicit engineering split** of de Leva's 6.94% head+neck group (not as a second independent added mass).

| Native body / segment owner | Derivation from published fractions | kg |
|---|---:|---:|
| Existing root / legs / middle trunk / pelvis / all lower limbs | residual after five upper bodies, `67.22% × 220` | **147.884** |
| Chest/spine | de Leva upper trunk `15.96% × 220` | **35.112** |
| Neck | [S3] reference `1.40% × 220`, subtracted from head+neck | **3.080** |
| Head | `(6.94% - 1.40%) × 220` | **12.188** |
| Left shoulder/arm **aggregate** | `(2.71% + 1.62% + 0.61%) × 220` | **10.868** |
| Right shoulder/arm **aggregate** | same | **10.868** |
| **TOTAL** | exactly 100.00% | **220.000** |

The shoulder bodies currently proxy each **whole upper limb**; the hands/forearms are *not* extra independent masses. Never leave the old 220 kg capsule unchanged and add these bodies on top (that would create 292.116 kg). No hidden mass duplicates in either physics or `capture_point`. Root's collider and effective inertia must be recalculated after subtracting upper segments; preserve existing rigid-body ordering by creating added bodies last, as #242 ruled. Mass numbers are a proposed *protected* split, not permission to edit limits. De Leva describes average young humans; the unusual creature requires a rig-based geometry check before final protected PR.

## B. Inertia, centers and geometry: source-derived reference, not made-up inertias

Use a right-handed *segment-local* frame, clearly document each body's long axis and its rotation into Jolt coordinates. For an anthropometric segment with length `L = stature × published length_fraction` and radius-of-gyration ratio `r_i`, `I_i = m (L r_i)^2`. These ratios are in [S2]. Source stature 1.80 m is Cory's #243 brief.

- **Chest / upper trunk:** `L=1.8×0.2961=0.53298 m`; published `r=(0.505,0.320,0.465)`. Source-axis reference inertia `(2.5437, 1.0214, 2.1567) kg·m²`, about its own CoM; CoM along source segment is `50.66%` of its segment length.
- **Head + neck parent reference:** `L=1.8×0.1899=0.34182 m`; published `r=(0.303,0.315,0.261)`. **Combined parent** reference inertia `(0.1638,0.1770,0.1215) kg·m²`. This is NOT a permissible inertia to copy separately onto the head and neck: they are two coupled bodies whose *aggregate* is checked using the parallel-axis theorem and current rig joint transforms.
- **Each shoulder/arm aggregate:** [S2] upper arm/forearm/hand lengths `16.18%/15.45%/4.95%` of 1.8 m, masses `5.962/3.564/1.342 kg`, CoM offsets `57.72%/45.74%/36.24%` from proximal endpoints and radii from [S2]. For a collinear extended-arm *reference pose*, combined length `0.65844 m`, combined CoM `0.30373 m` from shoulder; derived `I=(0.3386,0.0565,0.2927) kg·m²` about that aggregate CoM in source axes. Must **recompute** from actual shoulder/arm collision shapes and posed link arrangement; these are audit values, not locked engine tensor components.
- **Head / neck individually and pooled root:** do not invent numeric inertia tensors without measured collider extents. Compute their positive-definite inertia and CoM from the **actual imported collision shapes and the mass above** using Jolt shape mass properties; reconcile the head+neck assembly against the published *combined* parent values and publish the discrepancy. Composite inertias use `I_composite = sum(R I_local R^T + m ((d·d)E - d d^T))`. An asymmetrical Matron mesh can legitimately differ from human parent tables. The current rig JSON [R1] locates neck base at y≈1.541 m, upper neck y≈1.588 m, head joint y≈1.644 m, and shoulders at x≈±0.157 m/y≈1.446 m; the **collision widths and shapes still need measurement**. Never fabricate them to force a table match.

**Acceptance for J-044c:** log per-body `m, COM, Ixx,Iyy,Izz` and local axes; sum masses within float tolerance to 220; validate global COM, physically correct impulses at off-center hits, no negative/near-zero inertias, no collision with the root from connected self-bodies, repeated deterministic replay. Log native effective-mass/torque response rather than asserting a screenshot proves it.

## C. Joint angular ranges: evidenced human envelope, conservative engineering envelope

[S4] 250-person cervical measurement gives aggregate sagittal ROM ~122°, lateral ~88°, axial ~144° (total excursions), which is **not** the range of *each* of two stacked joints. [S5] thoracic pooled total flex+extension ~28°, side ~36°, twist ~45°. [S6] shoulder norms: flexion/abduction approximately 180°, extension 60°, external rotation 90°; these are *whole shoulder-complex* norms, not a single glenohumeral joint's unconstrained cone.

Engineering initial limits (NOT independent anatomical measurements; sums below are deliberately inside [S4–S6]). Joint-local rest orientation is the imported rig rest pose; all values are hard-angle **limits**, not animation target amplitudes.

| Joint | Flex / extend | Lateral swing | Axial twist |
|---|---|---|---|
| Root→chest | ±14° | ±18° | ±22.5° |
| Chest→neck | ±15° | ±10° | ±15° |
| Neck→head | ±35° | ±25° | ±55° |
| Chest→left/right shoulder/arm | forward +150°, back −45° | abduction to 130° | internal ±65° / external 90° |

Two cervical links together offer at most ~100° sagittal excursion, 70° lateral excursion and 140° axial excursion, all beneath the measured global normals in [S4] (with possible further coupled-motion reduction). These ARE controller starting envelopes, not medically validated joint-injury cutoffs. Check axes and asymmetrical swing constraint geometry in the engine's exact Jolt version before wiring. Choose tight ordinary pose-target ranges *within* these hard stops; an impulse can disturb the pose without allowing a forbidden rotation. Do not set rotational velocity or snap the head back to guard.

Jolt [S7–S8] provides `SwingTwistConstraint` angle limits, target orientation/velocity, motor torque limits and solver impulse readback. The hit goes to the contacted head or limb body, so joints transmit reaction back to torso/root; motors oppose according to finite shared budget, and the previous strike's residual angular velocity participates in the next impact.

## D. Hit-location map: 12 regions; same authoritative contact

Proposed stable enum / export schema **for review, not an already-landed API**:

| Region keys | Native body | Classification from actual contact |
|---|---|---|
| `face_l`, `face_r` | head | frontal/lateral skin zones, split by *head-local* sagittal plane, never world X |
| `jaw` | head | subface mandibular zone, front/underside |
| `skull` | head | crown/occipital/back-of-head contact |
| `neck` | neck | neck collider, excluding lower jaw |
| `chest` | chest | anterior upper trunk |
| `belly` | root (middle trunk) | anterior lower trunk |
| `back` | chest or root | posterior side of the actually contacted torso collider |
| `arm_l`, `arm_r` | arm proxies | corresponding physically contacted shoulder/arm geometry; a block lands **here**, not behind it |
| `leg_l`, `leg_r` | root, with measured shape/limb contact attribution | left/right planted leg geometry (root alone cannot separate legs by arbitrary world-side guessing) |

Every **accepted** impulse generates one `HitContact` carrying body/entity id, local hit point and normal, `impulse_Ns`, origin/ownership, surface-feature geometry, tick and resolved region. Preserve `damage_ns` as THE authoritative total. Per-region credited increments must satisfy `sum(region_new_impulses) + unclassified_new_impulse == damage_ns_delta` **in each tick**. Unknown legacy or composite contact goes into an explicit `unclassified` reconciliation bucket and creates **no fabricated facial injury** until source geometry disambiguates it. Multiple simultaneous contacts apportion only the already-judged impulse, never a second addition. Keep `player_damage_ns` attribution unchanged. A glancing hit and guard contact cannot be mapped to the cheek solely because a cast ray crosses it. The fist must actually meet the appropriate body shape first.

## E. Progressive injury: game-calibrated thresholds, not fake forensic certainty

There is **no credible universal N·s-to-bruise threshold**. Controlled bruising research [S9] explicitly found impact force was not reliably inferable from bruise appearance. Consequently, claiming '50 N·s medically causes a cut' would be false. The following is a **predeclared testable game calibration** anchored to the existing engine's validated nominal punch reference `J_ref = 1500 N × (3 / 90 s) = 50 N·s` [R2]; stage boundaries and death remain entirely unchanged. No new free-floating health bar.

For each region, track accepted `J_region` and last local impact `J_event`; each injury layer is monotonically nondecreasing during a fight (Cory's no-heal ruling). Initial visual calibration / thresholds:
- **Localized red mark**: `J_event ≥ 0.5 J_ref = 25 N·s` for actual skin contact. A light tap below it may have motion/audio but no lasting mark.
- **Bruise seed**: `J_region ≥ 2 J_ref = 100 N·s`; cumulative overlapping impacts matter. A physically bigger impact can exceed it immediately. Color and coverage grow with further *local* dose, never with a generic global stage.
- **Swelling seed**: `J_region ≥ 4 J_ref = 200 N·s`, with increase under continued trauma. Treat the proposed transition rate as an authored, deterministic presentation parameter to tune in the **actual Godot fight**, not as a physiological clock. No sudden discrete face replacement.
- **Abrasion/laceration**: require recorded *surface feature* + relative motion/shear or a highly concentrated collision on a physically susceptible location (e.g., brow ridge). An edge is not automatically 'sharp' because its object is metal: read the actual contact shape/feature. Calibration candidate: concentrated edge contact `J_event ≥ 1 J_ref = 50 N·s`; repeated blunt brow/jaw trauma `J_region ≥ 6 J_ref = 300 N·s` may open a wound only with real local contacts. These are two explicit engineering hypotheses, NOT empirically validated skin-tear loads; test and revise them openly rather than encoding a source-dependent automatic cut.
- **Blood**: originates only at a registered open wound/contact, not from a global damage-stage switch. Pool/drip/smear transforms follow the character's physical location and mesh orientation; do not invent blood from a hit to an uninjured distant region.

Contact 'sharpness' must be derived from available shape geometry/feature curvature and tangential relative velocity; if unavailable report **UNKNOWN** and defer cuts on that event rather than map arbitrary entity types to wounds. The existing punch cast radius is 0.06 m [R2], **not a measured biological contact patch**: do NOT calculate clinical pressure from it. Time-based color is cosmetic and may advance deterministically at 90 Hz, but forensic literature [S9,S10] shows injury color/age is not a reliable clock. All rates and thresholds above are *proposed perceptual targets for controlled act runs*, subject to independent balancing, not factual biomechanical injury thresholds.

Region-local injuries affect later motor capacity only by a single explicit native conversion reviewed in J-044b/J-044l. Disabled limbs may pass their unused demand to healthy limbs subject to their own caps AND total 8 kN/44 kW; unused force is not created. Never weaken a limb at the renderer independently. Death still occurs at global 6,000 N·s, never by a second hidden wound-kill counter.

**Checkpoint contract change (owner-approved on #242):** save/restore the regional injury vector, localized visual wound seeds, and global damage together in one versioned game-state record; after a checkpoint taken when she is dead, restore **dead**. Current J-003 tests expect reset-to-zero: revise them ONLY in the separately approved protected save-contract change, with old-vs-new acceptance tests. Do not call earlier behavior 'broken' without acknowledging that owner has superseded it.

## F. Powered-ragdoll implementation research (no mandatory ML subsystem)

- Jolt [S7,S8] already has swing/twist joints with motor orientation targets, signed torque limits, solver Lagrange-multiplier/impulse readback, and positional limits. Use this first; no new physics engine.
- A motorized-ragdoll controller follows a pose target with finite, damped joint torque and permits *off-target motion from real impacts*. Jolt's architecture documents stiffness-error and damping-velocity components; its solver still needs tuning and force-budget accounting. In particular, **do not sum raw internal constraint impulses blindly as if all were externally supplied energy**: constraint reactions enforce joints, motor work is the active expenditure. Keep the one authoritative budget owner and explicitly meter motor work/forces before applying; ask Grok (#244) to independently validate the force-vs-torque conversion, conservation, and 90 Hz numerical margins.
- [S11] Stable PD research shows better numerical stability for articulated rigid-body tracking than arbitrary high-gain classical PD. This is a technique to evaluate, **not proof Jolt's current motor settings are stable**.
- [S12] DeepMimic demonstrates that physics-driven characters can imitate reference poses while recovering from disturbances. For ScraperX, the useful idea is 'physical actuators follow a target, real collisions alter the motion'; adopting Deep RL itself is unnecessary scope.
- Preserve the root locomotion and graph; head-neck-chest-arm motorized dynamics are new *authoritative physical state*, not presentation-only wobble. The earlier root-capsule balance threshold cannot falsely certify a new capture-point step. Use contact-derived COM/feet support, and an actual stance-support test to award a step or a fall. No new AI decision from cosmetic injury alone.

## G. High-value deterministic checks for the integrator

1. Two identical right crosses, left versus right cheek: actual per-body hit, opposite-sign head angular impulse, equal-magnitude regional attribution, no double count.
2. Weak glancing hit vs solid hit at same input: smaller measured angular recoil and injury dose; guard/body collision intercepts the face when physically present.
3. Neck motors disabled and enabled, same impulse: distinct *measured* rotation and torso reaction without force/power over-budget. Jolt joint constraints must not disappear when motor is off.
4. Simultaneous head and stock impact: `Σ regions + unclassified = Δ damage_ns`; player attribution remains only the fist share.
5. Checkpoint damage/injury/death round trip; reload preserves a dead Matron if checkpoint recorded after death, per latest Cory ruling.
6. Different physical step clearances and ground traction: real success/failure, no decorative recovery when the supported COM cannot be caught.
7. Exact-head deterministic two-run digests; plus device 90 Hz *timed* native and rendered evidence (J-044m owns performance proof). These checks have NOT run for this spec.

## Source key and provenance

- [S1] de Leva (1996), *J Biomech*: https://pubmed.ncbi.nlm.nih.gov/8872282/
- [S2] Published de Leva segment fractions, length, COM and radius-of-gyration Table 1: https://pmc.ncbi.nlm.nih.gov/articles/PMC5298648/
- [S3] Separate neck proportions (population sample, not Matron): https://pmc.ncbi.nlm.nih.gov/articles/PMC2820296/
- [S4] Cervical global range (250 people): https://pubmed.ncbi.nlm.nih.gov/10521629/
- [S5] Thoracic range systematic review: https://pubmed.ncbi.nlm.nih.gov/27487429/
- [S6] Shoulder normative comparison: https://pmc.ncbi.nlm.nih.gov/articles/PMC7549223/
- [S7] Official Jolt SwingTwist API: https://jrouwe.github.io/JoltPhysics/class_swing_twist_constraint.html
- [S8] Jolt motor and limits architecture: https://github.com/jrouwe/JoltPhysics/blob/master/Docs/Architecture.md
- [S9] Controlled bruising study, force not reliably mapped to visible appearance: https://pubmed.ncbi.nlm.nih.gov/31707237/
- [S10] Forensic bruise-age uncertainty: https://pubmed.ncbi.nlm.nih.gov/20211455/
- [S11] Stable PD physical character animation: https://doi.org/10.1111/cgf.14113
- [S12] DeepMimic physics-based pose/perturbation tracking: https://doi.org/10.1145/3197517.3201311
- [R1] Current rig: `godot/presentation/life/matron/matron_rig.json`, body joint transforms.
- [R2] Current strike and thresholds: `src/sim/physics_world_strike.cpp`, `src/sim/player_strike_limits.hpp`, `src/sim/matron.cpp`, `src/sim/matron.hpp`.

**J-044a status: SPEC DELIVERED for Grok/Claude review, implementation UNVERIFIED.** Most important handoff: do not pretend neck/head individual inertias, tissue thresholds or motor torque budgets are measured facts. The mass split is fully closed and source-anchored; the remaining exact inertia tensors require the new real collider geometry and independent #244 budget math.
