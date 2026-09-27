**Ascent Slice:** `AS-017`
**Lifecycle:** IMPLEMENTED
**Provenance:** re-derived here; implementation adapted from `ScraperX-Claude` `f872c41`
**Implementation gate:** satisfied: static route placement derived against the existing rings; the pre-integration normal-input test captured the missing-cabinet failure. Delivery acceptance is separate.
**Evidence:** [Start Here](../../00_START_HERE.md) §2 owns current source, CI and artifact status. This ticket retains the design and source-specific observations below.

# Façade climb from +11 m to +33 m

**Objective:** Continue from the pipe bridge onto an athletic façade route, using real reachable handholds, and stand on the +33 m tower ring.

**Existing truth:** ChatGPT's pipe route reaches +11 m. Claude's independent native C1 proof reaches +44 m from +22 m. Its normal-input grade-to-55 m test passed locally on ARM at `f872c41`; its water hoist and world builder are not being adopted.

**Authority:** Laws 4–6, 9 and 25; GDD §7; Atlas §4–5 and §10; TDD native traversal, fixed-step, presentation and checkpoint ownership.

**Owner / seam:** `Simulation::PhysicsWorld` owns geometry queries, player movement, support and hand contacts. Kit owns one new static route body, ID 1600. Godot reads the native geometry and traversal state, submits input and presents hands/camera. Existing Kit mass/shape layout and vault interpolation remain authoritative.

**Spatial derivation:** The current tower rings are 11 m apart. Translate Claude C1 down 11 m: loading platform +11, cabinet +12.7, duct top +16.3, vent top-out +22.03, narrow monorail +22.3, davit arm +33.3, final tower ring +33. Existing front beam outer face is Z=-123.7; route spans X=12.05..24.67, Z=-127.5..-119.3. Clearance against the retained frame must pass native traversal, not be assumed from the source branch. Atlas owns this placement.

**Mechanical close:** Static anchored architecture; no stored-energy machine or new dynamic actuator. Player muscle supplies climbing work; native support and swept collision constrain movement. Climbing a dynamic member applies the climber's 85 kg load to its body; carried/light handles are excluded as holds. No mechanism terminal basin is claimed by this slice.

**Forbidden shortcuts:** no whole-branch merge, new physics owner, duplicated body IDs, fixture teleport in acceptance, changed jump/reach to force a route, removal of existing parkour feel, or claims based on another branch's APK.

**Proof path:** first failing normal-input grade→pipe→façade test; native supported +33 m arrival with no deaths; missing-hold and blocked-clearance cases; existing vault/moving-support and bridge regressions; bridge and touch-input wiring; rendered scene and exact-source Android delivery. Full +66 m, route choice and device gates remain subsequent work.

**Completion:** AS-017 is implemented in source. Its current exact-source delivery state is owned by `00_START_HERE.md`; source lifecycle and delivery acceptance are separate fields.

## Reuse boundaries and local observations — 2026-09-26

- Source provenance: Claude movement commit `6fdf8d0`, with current C1 grip/top-out refinements from `f872c41`. Adapted only movement, input and hand presentation; no sibling world construction, process systems, density layout or weakened CI gates.
- Geometry uses source-local coordinates translated down 11 m by its Kit body. Current `Part` box/shape/mass layout is unchanged. ID 1600 is unique; the opening test checks all Kit entity IDs before operation.
- The test first failed against the prior native library at the missing cabinet (exit 40), after successfully traversing the active pipe bridge.
- Native route reached capsule Y=33.8936 on tower support 11, zero deaths before the recovery exercise. Duct shimmy, controlled lowering and top-out pass; the standing ladder is out of grip reach and requires a leap. Touch viewport events also reached Y=33.894, zero deaths, using full forward input for the ladder leap.
- Sprint reaches 8 m/s, retains 8 m/s through a jump, and carries 8.978 m versus walking's 6.172 m in the flat-ground probe. Crouch and sideways input do not sprint. Touch overshoot starts native sprint and release stops it. Existing vault-entry/exit and camera interpolation checks pass.
- The full local ARM suite has the same AS-002 legacy deployed-flight failure and preceding measurements as `/data/data/com.termux/files/usr/tmp/scraperx-baseline-tests.log`. It is not waived: required x86 host CI remains a delivery gate.
- This slice does not complete the +66 m milestone, establish Android execution, or establish Fold performance. Its successor must supply the next machine and upper connection.

## First host run and render-cost repair

Run `36278331701`, source `4dd1ca3`, passed the complete host native suite,
camera proof, default rendered foundation and native/keyboard/gamepad pipe
routes. Its software-rendered touch pipe route reached the raised bridge,
then hit the 600-second process timeout before crossing. No APK was produced;
this run is failed delivery evidence, not an accepted candidate.

Kit presentation now batches each body's parts by material and compatible
vertex channels. A local before/after comparison of every colored triangle
retained all 8,992 triangles with the same quantized geometry SHA-256
`8af5fe7697d4a78133bbbe3a28a2196cdb9a079d14f92897683514e353aaaa22`.
Mesh submissions fall from 176 to 76; the façade falls from 69 to 5.
Indexed boxes and unindexed tube bores remain separate surfaces. These counts
prove less submission work, not measured device FPS.

The rendered 30 FPS touch gate runs the complete pipe route once, inside the
continuous grade-to-33 m façade scenario. All pipe assertions and four pipe
captures remain required. Run `36280224425` used an 864×742 Fold-aspect
window and hit the 900-second bound; the later docs-only run `36285233929`
hit the same bound. A smaller `--resolution` window alone did not reduce
Godot's internal 2160×1856 canvas viewport: a local rendered launch still
reported 2161×1856. The next candidate temporarily sets both the design
viewport and window to 432×371 for this one CPU-rendered route, then restores
the shipped project settings before subsequent gates and APK export. A local
launch confirmed a 432×371 viewport; full rendered traversal remains unproven.
Input frames stay at 30 FPS, native physics at 90 Hz, and all six captures
remain required. Default and regression captures retain their larger windows.
Failed-run logs and available captures remain diagnostics, distinct from a
published APK. Milestone logs expose progress even without screenshots.

## Delivery checkpoint — 2026-09-27

Gameplay source `13e1146b8fcebeb4e8d7cd18087cfb26bf83a84e`, workflow
`36280224425`, failed the continuous rendered touch step and produced no APK.
The local native/headless +33 m observations above do not establish rendered
delivery. This is the recorded result for that source; current candidate and
artifact status remain in Start Here §2.
