# Canvas tether attachment patch — next-pass proposal

Status: **design only; all proposed values are CHOSEN and all patch behavior is UNVERIFIED.** Root selects the next implementation after current delivery. No patch is present in the frozen candidate.

Source inspected: `IssisX/ScraperX`, `ChatGPT`, `2a05fb47760a7cbec9fe3995ba3635533fc9db54`; native C++17/Jolt pinned by `CMakeLists.txt`, Godot API 4.7. This is a bounded extension of the existing membrane owner, using Superpowers brainstorming and the [physics reuse guide](../../02_ENGINEERING_AUTHORITY/PHYSICS_REUSE_GUIDE.md).

## Existing attachment

[catch_net_config](../../src/sim/catch_net_route.cpp) declares 41 × 41 physical vertices at 0.3 m pitch, 150 kg, 160 perimeter clips, and a 600 N/m, 300 N·s/m tether with 2.5 m rest length. Its finite mounted frame carries the body-origin-local anchor `(-34,.4,-150)`.
[CargoNet::sample/internal_clip_step](../../src/sim/cargo_net.cpp) evaluates actual face barycentrics; exact material `(20,20)` has only one nonzero weight. Tether impulse enters that real vertex, with equal/opposite frame impulse and anchor torque. A painted square or moving the coordinate into one triangle alone cannot create a square load spreader.
The [existing local acceptance record](../../evidence/canvas-contact-resolution-2026-10-13/FINAL_ACCEPTANCE.md) reports 45 m/20 s PASS, late peak 0.139552 m/s and vertical window 0.0233545 m, plus 36 m/cell-offset/exit/failure and rebuilt-bridge day checks. These are baseline evidence, not patch results or Android/device proof.

## Small alternatives

| Option | Actual load path and tradeoff |
|---|---|
| **Recommend: sewn second-ply square** | Center eye → finite reinforced native edges → sewn perimeter/bulk cloth → clips/frame. Added mass and local edge stiffness reuse existing vertices; no new body, constraint family, or tether controller. Requires proving that corner reactions actually spread the center load. |
| Four-leg textile bridle below a reinforced square | Four distinct material attachments → finite tensile legs → physical junction/main cord → anchor. Direct corner loading is clearer, but requires an actual junction mass, leg slack/energy accounting and visible legs; a virtual average endpoint would omit that mechanism. |
| Finite steel backing plate with sewn attachments | Eye → plate mass/inertia → multiple compliant cloth attachments. Mechanically credible but adds rigid collision, concentrated impact risk and more native/render integration; exceeds this small fabric enhancement. |

## Recommended physical slice

- **CHOSEN geometry:** 1.2 × 1.2 m underside reinforcement, spanning existing columns/rows 18..22 around the eye at 20,20: 16 native cells, 25 existing vertices. Keep their rest positions, face diagonal, 0.3 m pitch, tether endpoint/rest length and ground anchor unchanged; do not add coincident solver vertices or a hidden plate.
- **CHOSEN material interpretation:** an added 2 mm woven ply with effective bulk density 520.833 kg/m³ (areal mass 1.041667 kg/m²), giving 1.5 kg additional material. A nominal 2 mm base ply yields 4 mm local fabric thickness; this is an authoring interpretation of the 150 kg/144 m² base, not a measured fabric specification. The existing uniform base-node lumping remains an approximation.
- **CHOSEN mass:** add that 1.5 kg by cell-area lumping: each added 0.09375 kg cell contributes one quarter to its four real corners. Increment interior nodes 0.09375 kg, patch-edge nodes 0.046875 kg, and patch-corner nodes 0.0234375 kg. Existing masses remain 150/1681 kg each; membrane total becomes 151.5 kg. No mass is reassigned to the frame or silently removed elsewhere.
- **CHOSEN finite stiffness:** two sewn plies share in-plane strain at their sampled nodes. Halve local native stretch/shear/bend distance compliances from `5e-6/5e-5/2e-3` to `2.5e-6/2.5e-5/1e-3 m/N`. This is a reduced-order parallel-ply law, not calibrated Young's modulus; sewn layers do not justify a bonded-plate thickness-cubed bending claim.
- In the existing constructor, reuse `CreateConstraints` and classify real`mEdgeConstraints` before `Optimize`: scale only edges whose complete rest segment lies inside the square, including its boundary. Retain the original positive compliance outside/across the seam. Existing mesh connectivity transfers the reinforcement load into surrounding cloth; do not pin the patch or overwrite vertex velocities.
- Native axial damping continues to use each edge's actual compliance and inverse masses with the existing 0.05 damping ratio. Preserve clip/tether laws and iteration count; increased mass/stiffness changes their actual response, so previous settling evidence is invalidated.
- Reaction/energy path: player/contact loads and gravity act on the real membrane masses; the center eye strains patch edges, producing opposing pair reactions and elastic storage. Four patch corners move independently relative to the center and each other. Native edge loss, ordinary cloth loss and the finite unilateral tether dissipate motion; clips and the actual frame/mounts carry reactions to the foundation. No new actuator or external work source is introduced.

## Visible agreement and cost

[native_catch_net_view.gd](../../godot/presentation/native_catch_net_view.gd) already builds canvas from interpolated native vertices/indices; [render_catch_net_tether_vertices](../../src/sim/simulation.cpp) already shares the cloth/frame snapshot. Render the added underside ply from precisely those 16 cells, offset 2 mm along their current deformed normals, with a visible sewn perimeter and center eye. This follows real patch deformation; the thin-ply display is not another collider or load source. The native membrane remains a zero-thickness reduced-order surface with its existing 0.035 m collision vertex radius, not a resolved 4 mm solid.
Use the same native patch descriptor for physics selection and presentation bounds/thickness; do not guess a square from texture UVs or duplicate constants in Godot. Reuse the existing catch-state bridge for that small descriptor. Preserve unchanged-snapshot upload skips; the patch adds 32 display triangles and no native vertices/constraint rows. Mobile cost is still UNVERIFIED and must be measured on the resulting build.

## Acceptance that can reject the proposal

- Compare the same finite taut tether pulse with reinforcement disabled/enabled, using unequal masses and an asymmetric off-center patch load. Measure actual center, four-corner and neighboring vertex responses plus native edge reactions: load must leave the single eye through multiple patch paths; corners must retain nonzero relative deformation/velocity and asymmetric reactions. A tinted square, arithmetic force split, or rigid velocity reset fails this criterion.
- In the isolated patch/finite-frame observation, account for 151.5 kg membrane mass and actual point inertia. Internal edge impulses must cancel; tether linear impulse and torque must match the frame's opposite reaction. Separate gravity/mount external impulses from total momentum, and elastic storage/material dissipation from numerical residual; current tether diagnostics are not full-scene energy closure.
- Re-run unchanged 45 m/20 s rebound/settling/clearance criteria, 36 m and cell-offset catches, actual frame/tread exit, failure/restart cases, and original vertical cargo regression. Exercise slack/taut transition and render inspection of the underside square during asymmetric deformation. Require normal exact-source CI/Godot/APK gates before delivery; no fabricated operating range or pass claim.

Minimal future file surface: `src/sim/cargo_net.{hpp,cpp}` (opt-in patch descriptor/mass/compliance), `src/sim/catch_net_route.cpp` (chosen material), existing catch-state plumbing in `src/sim/simulation.{hpp,cpp}`/`src/bridge/scraperx_simulation.cpp`, `godot/presentation/native_catch_net_view.gd`, affected existing model/player/render tests and reuse documentation. Default disabled preserves original vertical open-weave cargo.
Single deciding observation: **does a taut center pulse produce increased native load transfer to all four patch corners while preserving their real relative deformation and reciprocal frame momentum?** If not, reject the sewn-ply proposal before coefficient tuning; root can then select the physical bridle alternative.
