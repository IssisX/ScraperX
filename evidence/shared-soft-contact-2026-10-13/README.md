# Shared unilateral soft-contact correction — 2026-10-13

## What changed and why

The pinned Jolt soft-body collision response used a signed normal correction even when an overlapping vertex was already separating. It could pull the vertex and receiver together and apply friction during that tensile response. The shared source patch now permits velocity response only when the complete normal correction, including restitution history, is non-tensile. Geometric depenetration remains unconditional for penetration. Existing closing-contact math, dynamic effective mass and equal/opposite reactions are preserved.

A current-speed-only gate was rejected: prior normal speed -1 m/s, reconstructed current +0.1 m/s and restitution 0.5 still require repulsive response toward +0.5 m/s. The integrated correction preserves that bounce; it also preserves +1 m/s separation when that already exceeds the restitution target.

## Evidence actually executed

- Original native response: 6 failures among 14 contact cases.
- Earlier current-speed-only proposal: 2 failures, both legitimate restitution-history bounces.
- Integrated correction: all 14 pass, using actual `CustomUpdate`, observed contacts and geometry; dynamic/static receivers, closing/separating, friction, restitution and real long-range-attachment velocity history. The dynamic receiver is 85 kg; the soft vertex is 1 kg.
- Existing integrated `friction` and `force` modes pass. The latter checks 48 native callbacks and reciprocal force/integration behavior.
- Existing 45 m model passes unchanged rebound and settling criteria: first apex 25.2584 m, final speed 0.0327784 m/s, late peak speed 0.041318 m/s and vertical range 0.00327587 m; all 180 late contact ticks and original 7,920 callback cadence retained.
- Pinned source patch fresh application and idempotent hash verification pass.
- Existing normal-world 36 m neutral player catch passes unchanged acceptance: survives, rebounds, separates, recontacts, and achieves 90 consecutive stable-relative-contact ticks at tick 1970. Minimum mesh height 1.53135 m; peak rebound 17.5637 m/s. No checkpoint fabricated on moving cloth. The separate corrected-source `exit` mode also passes ordinary movement onto real frame2963, its treads, and stable grade checkpoint, with zero deaths; see `player36-exit.log`.

See `receipt.json` for source identities, actual commands and statuses; adjacent logs retain the observations. Focused executables link the compiled corrected Jolt translation unit ahead of the unchanged cached archive. This is native execution, not a new full build or phone observation.

## Reuse and boundaries

Every soft contact using the patched dependency receives this correction; no catch-specific force or geometry compensation was added. Reuse the native owner through `CargoNet`; the existing `scraperx_sim.catch_net_contact_friction` group protects the contact contract without increasing the 67-case CTest count. From a configured current-source native build, run `ctest --test-dir <build> -R 'scraperx_sim.catch_net_(contact_friction|force_phase)$' --output-on-failure`.

The manifest intentionally rejects an older-patched dependency cache. Use fresh pinned source for a new configure, or recover pristine files privately with the previous patch and verify original hashes. Never erase another worker's cache. No header ABI change was made.

The regression does not prove arbitrary contact configurations, full energy closure, rotating-response accuracy or the separate kinematic velocity-frame issue. The 45 m model settling case passes; this does not establish all load/geometry combinations or the full player suite. Exact-source GREEN Actions, rendered routes and phone play remain delivery obligations. Use the compact [reuse guide](../../02_ENGINEERING_AUTHORITY/PHYSICS_REUSE_GUIDE.md) for subsequent physics work.
