# ScraperX

ScraperX is a clean-room Godot 4.7 / C++17 project governed by the authority package at the repository root. Read [`00_START_HERE.md`](00_START_HERE.md) before changing implementation.

## Where the project is

Two classes of work, named in [`00_START_HERE.md`](00_START_HERE.md) ("Where things live"; the full vocabulary
table is in [`00_START_HERE_ARCHIVE.md`](00_START_HERE_ARCHIVE.md)) and never sharing an identifier:

- **Kernel Work Orders** `WO-000`–`WO-013` in `03_EXECUTION/KERNEL/` — foundational engineering that proves
  one tool type each on the `KX-*` substrate. A **closed set**. All fourteen are in source.
- **Ascent Slices** `AS-001`–`AS-015` in `03_EXECUTION/ASCENT/` — the actual 1.6 km climb, one Atlas band
  at a time. Consult issue #3 for current assignments.

`B00`–`B11` are Atlas band IDs — floors of the tower. They are never filenames and never tickets.

```
00_START_HERE.md              the rules in force and where things are — read this first
01_PRODUCT_AUTHORITY/         what the game is     (laws, GDD, atlas, + support/ provenance)
02_ENGINEERING_AUTHORITY/     how work is executed (protocol, TDD, evidence snapshot)
03_EXECUTION/KERNEL/          WO-000 .. WO-013     closed; regression substrate
03_EXECUTION/ASCENT/          AS-001 .. AS-015     ascent specifications
03_EXECUTION/PLANNING/        the ascent authoring contract
03_EXECUTION/TEMPLATES/       ticket forms
src/ tests/ godot/            implementation truth
```

The source contains ascent bands beyond AS-001, a Matron, a crew, and summit
mechanics. For example, `src/sim/crew.cpp` creates workers,
`src/sim/simulation.cpp` exposes `Simulation::summit_state`, and
`godot/presentation/main_feel.gd` presents the summit state. Source presence
alone is not a playthrough or a claim that these features are complete.

**Next work:** use the [assignment board and latest comments on issue #3](https://github.com/IssisX/ScraperX/issues/3),
check open pull requests, and reconcile candidates against current trunk. The
[numbered gap list](03_EXECUTION/GAP_LIST.md) records unfinished requirements;
its source and proof references must be checked against the current revision.
Unmerged branches are not trunk.

**Historical evidence:** [CI run 35727055407](https://github.com/IssisX/ScraperX/actions/runs/35727055407)
reported the earlier kernel proof, not the current trunk's full-game status.

## Build

Dependencies are fetched at exact commits by CMake:

- `godot-cpp` `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` (10.0.0 stable, API 4.7)
- Jolt Physics `e77f175595e64cb44218cc9d9d56fc365ad0e36a` (5.6.0)

Host build:

```bash
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

The configured GitHub Actions workflow, `.github/workflows/wo000-delivery-spine.yml`,
contains native, Godot, touch, Android export and artifact jobs. Inspect a specific run and
its revision for actual outcomes; configured jobs, skipped jobs and screenshots are not
proof of a complete ascent or a device playthrough.

## Claim boundary

An APK artifact proves an APK was produced by the source revision and build identified in its evidence. It does not prove installation, execution on Android, or Fold 6 behavior. Those remain separate evidence states.
