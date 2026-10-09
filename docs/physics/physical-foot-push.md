# Physical foot push on supported timber

`PhysicalFootPush` is a short-lived native leg surrogate between the actual timber contact point and the player's centre of mass. It gives the coupled plank time to transmit a reaction through its elastic joints and supports. It replaces the weak instantaneous timber impulse, which produced only about 0.198 m/s player vertical speed in the captured baseline. It does not guarantee the ordinary 5.5 m/s Jump speed on weak wood.

This is a reduced-order gameplay model, not an anatomical leg, foot-placement system or second collision solver. Jolt remains the sole body/constraint authority; native capsule contact remains the grounded authority. The module does not add player weight, write velocity, credit welded-neighbour inertia or fund a launch using one free cell's mass.

## Row and mathematics

Owner: [physical_foot_push.hpp](../../src/sim/physical_foot_push.hpp) and [physical_foot_push.cpp](../../src/sim/physical_foot_push.cpp), using the pinned Jolt 5.6 `TwoBodyConstraint` and `AxisConstraintPart`. The private row adapts Jolt's MIT-licensed `DistanceConstraint.cpp`; its attribution and permission notice are retained in the source. Native simulation uses C++17 and a 90 Hz outer update. The row's impulse ceiling uses the **actual collision-step duration**, not the outer frame duration.

Let the material point be \(p_s\), the player COM be \(x_p\), and the support COM be \(x_s\). Define

\[
u=x_p-p_s,\quad L=\lVert u\rVert,\quad n=u/L,\quad C=L-L_0.
\]

The spring operates only when \(C<0\), with positive accumulated impulse \(0\le\lambda\le F_{max}h\), in N·s. It pushes the player along \(+n\) and the support along \(-n\). The support receives angular impulse \((p_s-x_s)\times(-\lambda n)\); the player receives zero leg torque because its endpoint is the COM. Jolt's axis Jacobian uses \(r_s+u\). Here \(u\times n=0\), so its support lever is the actual material-point lever. A fixed-normal SixDOF translation motor would not preserve that identity with lateral COM offset. The built-in final `DistanceConstraint` has no force ceiling, which warrants this single private distance row.

There is no tensile force, target-velocity source or hard position correction. The original contact point and normal are stored in support COM-local coordinates. They move with that body and never migrate automatically to another plank cell. The leg's tangential demand must remain inside the chosen friction cone: if \(a=n\cdot n_s>0\), then \(\lVert n-a n_s\rVert\le\mu a\), using the captured surface normal and supplied friction coefficient.

## Chosen parameters and energy meaning

These are explicit gameplay assumptions, not physiological constants.

| Quantity | Value |
| --- | --- |
| Force ceiling | 3,500 N |
| Spring stiffness | 40,000 N/m |
| Damping coefficient | 1,000 N·s/m |
| Command-power ceiling | 5,000 W |
| Command-work allowance | 1,285.625 J |
| Maximum positive rest extension | 0.30 m; ordinary mounted-timber request 0.15 m |
| Maximum duration | 0.25 s |
| Maximum lateral reach | 0.45 m |

Attachment starts with the rest distance equal to the actual representable distance, with zero preload and zero command debit. The target rate is requested stroke / 0.25 s: 0.6 m/s for the ordinary 0.15 m timber request, or 1.2 m/s for the isolated default 0.30 m request. The request must be finite, positive and no greater than 0.30 m. Each positive **stored float** rest increment \(\Delta L_0\) is charged \(F_{max}\Delta L_0\), bounded by power times actual step duration, remaining allowance and remaining stroke. Round-up cannot purchase extra travel. A full 0.30 m extension therefore costs at most about 1,050 J; the 1,285.625 J allowance is the ordinary 85 kg, 5.5 m/s kinetic-energy reference, not a promise to deliver it.

This debit is a conservative command-work bound, not measured delivered mechanical work. The separately reported nominal compression storage, \(\tfrac12k\max(L_0-L,0)^2\), and its change at fixed poses are diagnostics for the force-capped implicit row. They are not a second actuator budget or a reusable energy reservoir. Full energy closure, including damping, solver residual and the complete articulated plank, remains unverified.

## API, release and integration

`begin(support, material_point, normal, friction, requested_stroke_m)` acquires the actual bodies outside `PhysicsSystem::Update`. An existing row rejects another begin, including while removal is pending. `pre_step(collision_dt, face_valid, traction_valid)` runs in the existing serialized step listener: it samples the preceding solved impulse, validates ownership/reach/traction, advances the bounded target, or disables/reset-warm-starts the row. It never adds or removes constraints inside Update. `post_step` samples the final solve and removes requested stops outside Update. `clear` must run before either body is destroyed or topology restored.

Finite duration, stroke/work exhaustion, owner/traction loss, lateral reach loss or separation beyond initial distance + 0.30 m + 0.0001 m disable the row before another solve. A stopped row cannot reacquire the support. Readback separates current geometry from the actual last solve axis/duration; force uses the latter. Peak load includes earlier collision substeps as a maximum, without summing or spending impulses twice.

This row is transient: constraint settings return null, and Jolt state save/restore explicitly rejects an active row. Clear it before physics recording/restoration. Both module files guard their Jolt definitions. This does not establish a supported no-Jolt simulation.

Current [simulation integration](../../src/sim/simulation.cpp) starts it only from actual contact with a supported deformable-plank fragment. Buffered Jump intent is consumed once; failed begin has no instantaneous fallback or active-budget refresh. Active foot push does not create fake grounding or checkpoint footing. Cancel, crouch, carrying, hand traversal, launcher control, fracture/support loss, death and restart clear it. The actual plank-cell BodyID carries the reaction; logical-member classification does not merge twelve bodies. [Bridge readback](../../src/bridge/scraperx_simulation.cpp) exposes its receipts through `get_landing_state`.

## Verified scope

Final ARM64 Release receipts are in [the scoped evidence ledger](../../evidence/wood-foot-push-2026-10-09/README.md). Pinned Jolt 5.6 and the existing 90 Hz native owner are unchanged.

- Isolated row: zero preload, centred/offset/rotated actual-point reaction and torque, no tensile impulse, owner/traction/reach cancellation and finite input pass. An externally imposed −50 m/s closing speed saturates the row at 9.722222328 N·s = 3,500 N × 1/360 s. That initial energy is explicitly external, not actuator output.
- The default 0.30 m heavy-receiver case spends 1,049.999 J, peaks at 1,459.05 N and stops at actual distance 1.207106 m from 0.900000 m. Its measured 0.250000006 s duration has float-roundoff-scale excess. This is an isolated row receipt, not a normal timber launch.
- Ordinary mounted timber requests 0.15 m. Native actual player peak vertical speed is 0.961710393 m/s; COM apex gain is 0.146911621 m. Command debit is 524.999499 J. Twenty-two repeated Jump requests do not refresh the anchor, start count or budget. Actual wood landing, quiet support-relative settling and right-steel exit pass with intact joints and gravity enabled.
- Ordinary viewport touch passes the same jump/landing/steel-exit sequence in 1,432 native ticks: peak vertical speed 0.964341 m/s, apex gain 0.147766 m and peak leg load 1,387.82 N. This is desktop Godot input evidence, not physical-phone evidence.
- Drop and active-push checkpoint restart remove the transient row without further force or spending. The saved checkpoint remains unchanged while the push is active. Existing static steel Jump still gives 5.390995502 m/s after its first gravity tick. Existing moving-support departure, connected inspection route, all eleven timber seams, grip/release, unload and genuine impact-fracture recovery pass.

The full 0.30 m prototype caused genuine landing overload. It is retained as a failed development attempt; normal wood now deliberately requests the smaller stroke without softening material or increasing strength. The intact tested landing's peak strength ratio is approximately 0.974, so it establishes one successful tested trajectory, not a blanket safe-landing guarantee. Earlier cleanup assertions confused checkpoint commit eligibility with actual support; only the driver was corrected, preserving production checkpoint rules.

Full energy closure, anatomical realism, phone feel, GPU/CPU release performance and exact-source Android delivery remain unverified. Previous cb19b6e CI passed all new encounters and both plank gates, but failed five inherited native gates; Android export was skipped. Build evidence and actual runtime receipts are recorded separately.
