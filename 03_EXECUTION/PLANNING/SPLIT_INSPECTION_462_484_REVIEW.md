# Independent split-inspection review — 462–484 m

Source: `2a05fb47760a7cbec9fe3995ba3635533fc9db54`, `ChatGPT`, IssisX/ScraperX; inspected 2026-10-13.
Scope: proposal review only, MACRO-TRAVERSAL-STRICT; root owns final placement/implementation after catch delivery GREEN.
Evidence: source reading and geometric derivation only. No builds, tests, gameplay, Actions query, or device observations.
Launch: caller reports saved GPT-6.1 Sol/medium/Standard, ChatGPT authentication; this session exposes no independent launch receipt/live effort control.
All new placements/material parameters below are CHOSEN; geometric consequences are DERIVED, not runtime passes.

## Decision

Keep the corrected west-column approach, separate southward gap and receiver, and eastward ascending girder, **with the girder shifted outside the 484 m north-roof edge**. The current Z≈−133 ascent is geometrically blocked overhead. Reachability remains conditional on native toe/transfer and braking observations.

## Authoritative geometry and approach

- [Brace](../../src/sim/world_solids.inc#L1159) plus [native quaternion normalization](../../src/sim/simulation.cpp#L3314): low endpoint near (0,462,−128.55), high endpoint near (−21.45,473,−128.55). Reject the retired west-low→center-high interpretation and x0/473 takeoff.
- [462 m slabs](../../src/sim/world_solids.inc#L1154): north strip top462.25, X±21.45, Z[−132.55,−128.55]; west strip X[−21.45,−17.45], Z[−167.45,−132.55]. Earned (−19.5,462.25,−142) joins the north strip continuously.
- Brace is only0.40m wide in Z; its low toe overlaps the roof edge by about0.20m. Approach the toe from inside the north roof, then establish genuine brace contact. Do not infer this edge seam is walkable from its centerline alone.
- [Capsule/traversal](../../src/sim/simulation.cpp#L155): radius0.35m, standing height1.8m, step maximum0.35m, mantle rise0.35–1.85m, reach0.95m. At X≈−19.3 the brace upper face is approximately472.12m: crosshead top473.25 is about1.13m above, so an ordinary supported Jump is the primary transfer candidate; mantle is conditional on actual probe/sweep acceptance.
- [Next west column](../../src/sim/world_solids.inc#L1164) occupies X[−21.641,−20.349], Z[−129.651,−128.359], Y[473,484]. It prevents standing directly atop the old column. Keep transfer center X≈−19.3; east-side capsule clearance requires X>−19.999 before moving south. Target crosshead center Z≈−129.7, avoiding both column faces. Existing upper brace stays well above this transfer.

## Selected physical arrangement

- Retain takeoff X[−21.5,−18], Z[−130.2,−128.15], top473.25; recognize its northwest portion is obstructed by the continuing column. The usable east/south portion is sufficient in plan, pending sweeps.
- Retain empty gap Z[−132.3,−130.2] and receiver X[−20.7,−18], Z[−134.6,−132.3], top473.25. Place no chord, tie, bearer top, or collider across this flight corridor at a catchable intermediate elevation.
- Correct girder walking-top endpoints to (−18,473.25,−134.15)→(10,484.25,−134.15), width0.55m. Run28m/rise11m yields21.45° and length30.08m. Its southward placement is within the existing receiver; player must brake, then move south/east onto its toe.
- [484 m north slab](../../src/sim/world_solids.inc#L1174) has underside483.75 and outer edgeZ=−133.46. At Z=−133 a standing capsule meets that underside once its soles approach481.95m, aroundX=4.15, several meters before the endpoint. Reject that ascent; do not cut the existing roof or lower the capsule.
- At corrected Z=−134.15, even radius0.35 leaves0.34m horizontal clearance to the north slab. The 484 m west slab overlaps the early girder footprint but is high overhead there; recheck its corner sweep, not just the north slab.
- Add a level exit shoe top484.25, X[9.45,10.55], Z[−134.45,−132.65]. It physically overlaps the roof by0.81m. Join the slope with overlapping below-top steel and no protruding lip; then walk north onto real484m roof and continue east along its open outer lane. Reaching the girder endpoint alone is not completion.
- Use the existing [top-endpoint girder convention](../../src/sim/braced_bay.cpp#L16), adapted to the X/Y plane: rotate the walking face, then subtract its rotated half-depth from the top-center midpoint. Generic beam-between endpoints specify centerlines and are unsuitable for authoring the walking top unchanged.

## Minimal honest support/API binding

- Crosshead: floor plate, two below-floor transverse bearers and a visible knee brace into the existing west-column side, clear of the arriving brace capsule. Receiver: separate below-floor frame with two visible posts/knees down to the existing462m west roof, placed within its X strip; keep posts away from the intentional miss lane nearX−19.3. No takeoff-to-receiver walking connection.
- Girder: one welded compound truss body,0.55m top chord, two lower chords and visible diagonals wholly below the walking face; CHOSEN≈0.8m truss depth. Seat its toe on receiver framing and its high end on a visible shoe/bearer reaching the484m roof. No side chord broadens the top footing or becomes a bypass. A30m structural span still needs authored section/load plausibility; this proposal is not a stress calculation.
- Reuse [pipe-route bearer placement](../../src/sim/pipe_service_route.cpp#L99) for geometry, but prefer [finite facade bodies/mounts](../../src/sim/facade_route.cpp#L199): constituent hollow-steel mass at CHOSEN7850kg/m³ and6mm walls, summed body mass/native COM/inertia, friction0.8, zero artificial body damping. Author floor plate mass as actual plate volume, not a hollow box formula.
- Separate finite crosshead and receiver bodies; fixed-joint receiver↔truss↔exit shoe where steel is welded. Passive mounts at actual visible column/roof attachments may use the facade's CHOSEN stiffness/damping as an initial precedent, subject to loaded observations. Roof/column backbone reactions use `add_elastic_mount({}, body, world_point, …)`; only actual returned `BodyIndex` values enter `add_fixed_joint`. WorldSolid51 is an entity identifier, never a Kit index.
- Native contact carries85kg player load and support momentum; mounts transmit reaction to the static tower backbone without supplying elevation work. These mounts model rigid members with attachment compliance, not calibrated truss bending. Kit `Part` collision and presentation must show identical members/openings. No new controller, recentering, lift, or invisible bridge.

## Few decisive acceptance trajectories — proposed, not executed

1. Earned entry (−19.5,462.25,−142)→north roof→toe→brace→supported Jump onto clear crosshead area near(−19.3,473.25,−129.7). Record actual support/sole height and capsule sweep at toe and column transfer; reject column penetration, rim-only support, or extra relocation.
2. Launch south nearX−19.3 from the gap edge with ordinary Jump; land center nearZ−133.1, brake inside receiver, turn onto toe nearZ−134.15, ascend corrected girder, cross level shoe, and walk onto484m roof beyond the new structure. Require real flight/contact, stable stopping and preserved exit clearance; test player loading without arbitrary movement tuning.
3. Intentionally short transfer: lose takeoff contact, fall through gap nearX−19.3, land on462m slab, return through toe/brace and retry without reset. North-strip landing needs Z≥−132.55; farther south is still above the actual west roof while X remains in[−21.45,−17.45]. Moving east beyond that strip can instead enter the shaft. Do not promise recovery from every miss.
4. Delayed braking/receiver overshoot and a small girder-side error must follow actual collision/gravity, not automatic capture. Record where real462m west footing ends under the error path; preserve consequential shaft falls and existing impact criteria.

Remaining decisive observations: toe contact, supported column-transfer sweep, receiver stopping footprint, roof-corner/exit sweep, and loaded mount response. Source changes to geometry, capsule, movement, Kit mounting or presentation invalidate the corresponding conclusions. No runtime feasibility or APK pass is claimed.
