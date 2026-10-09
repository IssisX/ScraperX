# Inspection crossing completion evidence

Starting source `ba40e21e170b167328be1d9ebf023ca75da9c681`, exact IssisX/ScraperX / ChatGPT. The bounded goal is the existing 396.25–418.25 m encounter. Luna's earlier 374–396 m integration and the larger roadmap are separate.

## Implemented experience

The approach, suspended grip, receiving steel, timber and recovery have a coherent inspection-access finish. Worn grip material was corrected after its pure-metal face rendered black in the real Tower lighting. Surface markings describe existing maintenance surfaces. No geometry, support or physics authority was added by presentation.

Swinging now displays LET GO rather than misleading CLIMB UP/JUMP OFF. Touch hides duplicate Action during a swing, preserving the ordinary Jump and Drop release slots. Keyboard/controller dispatch, ordinary hanging/climbing, native momentum, finite hands and all machinery remain unchanged. No InputRouter or native movement repair was warranted by the observed touch baseline failure.

## Actual observations

| Requirement | Evidence | Boundary |
| --- | --- | --- |
| Entry, approach, swing, timber and secure onward exit | `touch/connected-final.log`, receipt and source manifest: 8,711 native ticks, actual floor418.25/support51, firm footing, gravity1, zero deaths | Real normal-world viewport events; sole396 entry staging; not a human/device playtest |
| Practical input timing | Same connected touch run: held lean122–133ms, anticipatory lead150/450ms, real reverse and forward catches/releases | Coarse input diagnostic; no automatic pumping/release shipped; human timing/comfort unobserved |
| Correct release meaning | Two actual touch catches show LET GO, hidden Action; actual-context non-touch HUD check passes | HUD context check is not a separate keyboard/controller physical traversal |
| Miss and recovery/onward | `native-miss-onward.log`: 6,107 ticks; actual tray impact, return brace, stable418 exit. Final touch also returns across the hanger and retries | No reset, later staging, pose/velocity setter or manufactured contact |
| Persistent damage and legitimate alternative | `native-damage-onward.log`: 8,615 ticks; ordinary reachable brace walkoff causes mask80/peak1.078684, sidestep from fallen timber onto steel, real4.603m/s steel run-up and1285.62J jump, no intermediate timber footing, damaged stable418 exit | Native ordinary-input path; touch damage/restart unobserved |
| Visual readability | `visual/`: fresh final parser and full-world648×557 stills, actual1935/1936 footing and dynamic render agreement | Explicit visual staging, quality0/shadow1/render scale1/FOV82; not earned-route or phone-performance proof |
| Preserved machinery | `final-source.json`: simulation, physical hands, plank and inspection native owners unchanged from parent | Existing native90Hz/Jolt5.6; final UI/render changes cannot establish every unrelated route regression |

The first damage driver failed because it held the capsule over the fallen fragment instead of asking the player to step onto steel. The retained first-attempt log records that real condition; only ordinary driver input was corrected. Contact/footing acceptance stayed strict.

The earlier touch receiver miss came from its private driver turning the camera while retaining a screen-relative movement correction. Releasing that stick before a turn restored receiving footing with unchanged production locomotion. Baseline and corrected logs remain separate.

## Commands and sources

Root built the supported private C++17 ARM64 host target with `cmake --build .../host-build --target scraperx_inspection_junction_route_tests -j 2`, then ran `miss-onward` and `damage-onward` directly under Ubuntu proot. Successful final build output reached `[100%] Built target`; final runs exited0. The private host adapter imports the pinned Jolt library rather than replacing production CMake.

The touch worker ran `sh .../inspection-touch/run_touch.sh connected-final miss`; the expanded command, driver, receipts and compact frozen script archive are preserved. Its private scenario mapping, ARM64 native extension mapping and import-cache reuse are documented. The native library is the attested parent build, SHA256 `7796d8e777e828bc5fcbc83fcf5ae859a4a5bda42f094d45047e3e1c5f79eca1`.

Final KitView differs from the successful touch run only in the separately parsed/rendered grip finish. Source and visual receipts retain both versions honestly. `final-source.json` records the final production owners and test source.

## CI and phone boundaries

Parent delivery run37990803171 failed. Three new tests were **Not Run** because the workflow's explicit host build list omitted their executables; this pass adds all three targets and registers connected recovery/damage modes. The corrected workflow has not yet completed a successor run.

Five actual failures (`gravity_cart_integration`, `athletic_traversal`, `swing_stair_route_2`, `_5`, `_7`) match exact preceding source3227730/run37975266277. Four normalized payloads are byte-identical; athletic differs only in initial body counts. They remain unresolved and block full CI/APK delivery. They were not weakened or turned into a separate repair campaign.

Software-render attempts of the whole earned route were too slow on the host and were stopped; separate normal-world staged stills established only appearance. Phone feel, readability, haptics, damage/restart and frame budget remain unobserved. The light timber still gives only a short instantaneous hop; the completed route walks it intact and uses steel takeoff after damage. More deformable members and a generic physical foot actuator belong to a later goal.
