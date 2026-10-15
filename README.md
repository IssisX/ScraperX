# ScraperX

**“The input state determines the output.”** The project-wide design and engineering motto; see the governing laws and execution protocol.


Godot 4.7 / C++17 / Jolt industrial ascent. Read [`00_START_HERE.md`](00_START_HERE.md) before changes. Writes go to `ChatGPT` only.

## Download the ChatGPT Android APK

**2026-10-15 build:** [Download APK candidate 214230007](https://github.com/IssisX/ScraperX/actions/runs/38733308732/artifacts/11970774522) — source `f6ac5c4`; build/export/package checks passed.

[**Latest downloadable APK candidate builds**](https://github.com/IssisX/ScraperX/actions/workflows/chatgpt-apk-candidate.yml?query=branch%3AChatGPT) — open the newest successful run and download `ScraperX-ChatGPT-candidate-<commit>` under **Artifacts**. Extract `ScraperX-ChatGPT.apk` from the ZIP. GitHub sign-in is required to download Actions artifacts.

Every push to `ChatGPT` builds an APK candidate independently of the full gameplay proof. Candidates include exact-source provenance and package/signature/native checks; full regression and installed-device play remain separate. The [tested release](https://github.com/IssisX/ScraperX/releases/tag/apk-latest) may be older while gameplay proof is blocked. See [delivery details](ci/APK_DELIVERY.md).

## Current game state — 2026-09-27

Current status is maintained in [`00_START_HERE.md` §2](00_START_HERE.md); this dated heading is retained for reference compatibility.

The owner-directed reset retired the old campaign machinery, including the water screw/lift, intake sequence, upper lifts and machine-linked stair assemblies. The normal game keeps the tower, alpine setting, full parkour/controller with authored parkour and mechanism routes. The continuous ordinary stair/ramp bypass was later rejected and removed. The ambient yard crane and gear motifs remain presentation; the crane sway is not simulated rigging and does not operate or lift anything.

Current source includes AS-016’s grade→+8 m pipe bridge and +11 m ring connection, AS-017 to +33 m, AS-018 to +44 m, AS-019 to +66 m, and AS-020’s teeter candidate with a +77 m continuation. See [`00_START_HERE.md`](00_START_HERE.md) for the single current record of source, tests, CI, artifact and device evidence.

The active delivery objective is continuous ground-to-300 m ascent with varied physical challenges and parkour, within the larger 1,600 m tower. The [GDD §16](01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md) owns encounter quality, the [Atlas](01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md) owns space, and the [ascent plan](03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md) owns continuation. The [development proposal](03_EXECUTION/PLANNING/NEXT_ASCENT_PROPOSAL.md) retains earlier planning context. Targets and source presence are not proof of playable height.

Old machinery and traversal remain available only through explicitly selected regression fixtures. Fixture results are not campaign progress. AS-001–015 are retired; do not resume that queue.

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


Normal Godot launch loads the current game world, including the pipe bridge and the AS-017 façade route. The explicit `--uitest=ground_foundation` scenario exercises the cleared-foundation regression case; it is not the normal game. Legacy UI scenarios explicitly load regression fixtures. To regenerate collision tables, use normal `--export-solids=<path>` for `src/sim/world_solids.inc`, and add `--regression-fixtures` for `tests/fixtures/world_solids.inc`.

GitHub Actions produces downloadable APK candidates and runs the separate full gameplay delivery proof. The full proof includes the default-world proof and rendered capture, retained physics/controller regressions, both collision drift checks, Android arm64 build and APK export. Local development APKs are also possible; they are not published automatically. Published candidate artifacts contain exact-commit checksums and evidence boundaries.

## Claim boundary

An exported APK proves packaging. Installation, Android execution, device readability, touch ergonomics and sustained Fold 6 performance remain separate evidence states. See [`00_START_HERE.md`](00_START_HERE.md) for the current candidate's exact evidence boundary.
