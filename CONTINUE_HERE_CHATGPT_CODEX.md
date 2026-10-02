# CONTINUE HERE — ScraperX, canonical Termux / ChatGPT Codex handoff

**Revised 2026-10-02. Repository `IssisX/ScraperX`; write branch `ChatGPT` only. The recovered original scratch is committed on GitHub. Do not restart source recovery or reset to an earlier delivery. This document is the continuation record; Start Here owns the current immutable APK receipt, the Atlas owns spatial parameters and AS-023 owns the launcher contract.**

## Current handoff: device playtest repair

The owner manually tested the preceding `3d664e4` APK after discovering that the separate **ScraperX APKs** downloader selected an old build. The current report is real feedback on the slingshot: an unsuitable fork shape, pitch/yaw locked after drawing, choppy flight/collision, and a request for **Matrix-style accelerating 360° bullet time**. The next 10–20 m tower section is paused until the launcher is accepted on device.

The bounded repair in this checkout does the following:

- Replaces the rectangular frame with a conventional stout rounded wooden Y fork. Native capsule timber parts are also the rendered geometry. The original owner image is preserved at [evidence/as-023/reference/owner-slingshot-1423243.jpg](evidence/as-023/reference/owner-slingshot-1423243.jpg), SHA-256 `c23564db183f463fd3a10ac3ba26440ba5637edd063d4ea4614d32e6da6c9983`. Keep this file with the implementation; the terminal does not need the ChatGPT attachment path.
- Permits touch pitch/yaw aiming **before or after drawing** through the existing native finite-work gimbal. The fixed band anchors, ground ratchet extension and launch calibration remain. Feasible limits are 20–85° elevation and ±46° yaw. Charged turning records source work; it does not move the heavy fork kinematically or inject rubber energy.
- Derives the aiming lens angles from the same interpolated rail quaternion as its mesh. A no-physics half-tick regression verifies that camera angles differ from raw tick angles and match the rendered rail while native state remains unchanged.
- Opens the release camera at **0.06× elapsed native time**, accelerates through a complete orbit over **2.12 s**, and returns to the player's current POV by **2.70 s**. The opening lens follows rider displacement instead of blending from a world position left behind at release. Speed warp and the existing humanoid/humor remain presentation. Comfort controls still cancel the orbit/time scaling. The native fixed tick stays 90 Hz.
- Keeps the Grade start/backward boarding approach `(6,0.92,-58)` clear of the new visible/native steel foundation. The first model revision blocked this point; the real touch test caught it and the collider/mesh depth was repaired together.

**Verification so far:** full native suite **28/28**, 124.24 s; charged rail settles at yaw 0.279991 / elevation 1.10286 rad with 11.9798 m retained draw and 376.31 J added work. A release directly from that charged angle reaches `(27.8535,141.095,-119.02)` after two seconds through real guide exit and native momentum. Real shipping-scene headless touch launch reaches apex **383.91 m**; touch brake/landing reaches supported **+352 m**, **zero deaths**, **1.53 m** continued walking, 3,024 J contact and a −0.0583 m camera dip. All **2,894** settings checks pass. View/cancellation and actual native visual probe pass; the rounded fork/rider screenshots were explicitly opened. Read-only review found no Critical/Important defects; both minor findings were fixed. The 648×557 rendered touch route also passes (+352 m / zero deaths / 2.27 m walking); its charged pitch, rider, true POV and roof images were explicitly opened. Inspection caught a stale DRAW MORE action during released guide travel; the context condition is now repaired and headless proof rejects that action. Its final render recheck and exact-source CI/APK are pending at this publication checkpoint; update this paragraph when their results arrive.

**Device boundary:** the owner observed Android execution and defects in the preceding APK. This repair has no direct device execution path here. The two identified camera interpolation/transition causes are repaired, but phone frame pacing, tower collision feel, cinematic comfort and touch ergonomics still need a fresh device playtest. Do not call desktop/native success a phone performance fix. The separate APK downloader has not been inspected.

## Terminal continuation commands

GitHub carries the source, reference image and this handoff. A ChatGPT session UUID does not transfer its executor or caches into Termux. Start a terminal Codex session in a fresh checkout so existing personal scratch work stays intact:

```sh
cd ~
git clone --branch ChatGPT --single-branch https://github.com/IssisX/ScraperX.git ScraperX-terminal-20261002
git -C ScraperX-terminal-20261002 status --short --branch
codex -C "$HOME/ScraperX-terminal-20261002" "Read CONTINUE_HERE_CHATGPT_CODEX.md and 00_START_HERE.md. Continue from the current launcher playtest repair on ChatGPT. Do not restart recovery. Check the exact-source APK receipt and running to-do first. Preserve native Jolt/C++ physics and Godot touch/presentation ownership. Wait for fresh device feedback before selecting the next 10–20 m tower section. Update the authoritative docs as you work."
```

If that directory already exists, inspect its Git status and preserve any local edits before `git fetch origin ChatGPT` / `git merge --ff-only origin/ChatGPT`; do not overwrite it, reset it or force-push. Use the current branch head and Start Here's APK source SHA rather than assuming the latest documentation commit requires another APK. All APKs are built in GitHub Actions. Keep the branch-required `ScraperX-ChatGPT.apk` / `com.cory.scraperx.chatgpt` identity and stable debug signer; versionCode 1 / versionName 0.1.0 do not identify a source revision.

The live executor scratch is `/tmp/scraperx-chatgpt-scratch`, build `/tmp/scraperx-build-work`, runtime/evidence `/tmp/scraperx-runtime`. These are provenance, not Termux dependencies. The prior source-recovery archives remain untouched; GitHub contains the original recovered implementation plus this repair.

## Historical model-switch and migration record

The sections below preserve earlier receipts and paths. Their pause/recovery/publication instructions are historical. The current handoff above and running to-do below supersede them; retain the source IDs when interpreting old measurements.

### Model-switch checkpoint — 2026-10-01, GPT-6 Sol 6.1

**Continuation after the switch:** the owner authorized and the agent completed explicit BOARD/draw restraint, native `aim_locked` bridge, real pouch/rail contact-audit and rejected-harness restore-membership repairs. Fresh final build passes **28/28 native gates**, shipping touch launch/retrieval and +352 m supported arrival (zero deaths), 2,894 settings checks and landing/parkour/presentation tests. Read-only review finds no remaining Important/Critical defects. Both real-rendered touch paths and 95 actual-mixer audio checks pass; final launcher/pouch/humanoid/HUD/POV/roof/options images were opened and inspected. This local verification slice is complete, with Android/device acceptance and the wider ascent still open. The paragraphs below record the historical pause boundary, not current stop/recovery instructions.

**Owner requested pause at the model-switch boundary (historical):** stop gameplay implementation before switching models. After the environment recovered, only read-only source inspection occurred; no new gameplay edits, builds or runtime tests started. This handoff update is the only repository edit made to prepare the switch. The review agent was interrupted and no child agent is running. Resume implementation when the owner continues with the selected model.

**Actual working source:** `/tmp/scraperx-chatgpt-scratch`, Git base `f7a584dcceb576068f62e030e35d1b88e5cd4c5a`, 46 modified/untracked status entries. The original `src/sim/slingshot.*`, `slingshot_model.*`, `godot/presentation/slingshot_view.gd`, `landing_camera_response.gd`, shaders, character, settings and tests are present. The separate Termux session found no missing edits in its log search; that finding does not describe the later recovery of the original cloud filesystem. Keep the recovered original source as the implementation authority.

**Verified backup:** `/workspace/scraperx-rescue-2026-10-01/scraperx-chatgpt-scratch-full.tar.gz`, 5,648,417 bytes, SHA-256 `b407b2770b665709b84f7b1a22d0d7c24e8510e2ae3b175d037bac43459e1918`. Independent comparison found 597 files and 237 directories matching exactly; an isolated extraction passed `git fsck --full` and contains self-contained Git metadata. The archive predates this documentation-only checkpoint. Full source, separate evidence archive, integrity receipts and restore instructions are also saved outside the executor in the owner's [private recovery Page](https://chatgpt.com/space/page_b7ba9eea8ef081918148f248dd200836). Its latest attached copy of this handoff carries the model-switch checkpoint. If the environment becomes inaccessible, use those verified files; a new remote clone alone lacks the local gameplay changes. Remote commit `6deb651de90ffa5020a1be35faef10c8f3820b95` records recovery documentation only.

**First complete repair slice at that pause (now repaired):** preserve approach → explicit BOARD → aim while slack → paid backward DRAW → physical release. Source inspection at the pause confirmed that `Slingshot::pre_step` auto-attached on positive draw input while in the pouch, and `godot/presentation/main.gd` forwards backward movement as draw when merely `station_available`. These cause approach motion to charge the machine and lock aiming prematurely. Remove that accidental authorization at the native owner and restrict the presentation request to seated/recovering control; preserve the real unoccupied seat restraint until authorized draw. Prove that backward approach cannot seat/charge, explicit BOARD works, and deliberate draw still stores work.

**Adjacent unfinished integration at that pause (now repaired):** native `Slingshot::State::aim_locked` exists, but `SlingshotSnapshot` and the Godot dictionary do not yet forward it; complete that existing seam. The contact audit currently observes pouch contacts and phase-gates invalidation to release/recovery. Trace real launch-rail entity 2901 contacts through the production listener and invalidate the initialized closed-system ledger for outside contact during aim/draw as well; preserve the raw residual. Inspect before editing to avoid duplicating an owner or counting filtered internal joints as outside contacts.

**Verification boundary at that pause (historical):** no verification job was running. The final physical-gimbal candidate still needs those repairs and a fresh build, all 28 native gates, shipping-scene touch boarding/draw/release/retrieval, real supported tower arrival, landing/Jump recovery, settings and camera/audio checks. Earlier ~384 m launch, +352 m arrival, 27 native gates and settings/audio receipts belong to preceding candidates. Actual Android device play remains unverified. Use `/tmp/scraperx-build-work`, `/tmp/scraperx-runtime/run-godot.sh` and `/tmp/scraperx-current-evidence` when still available; inspect their current configuration rather than assuming binaries match source. The game remains a 1,000 m-plus physically causal industrial ascent, with this slingshot as one opening encounter.

**Post-integration backup:** the complete revised scratch is independently preserved locally at `/workspace/scraperx-rescue-2026-10-01/scraperx-local-verified-2026-10-01.tar.gz`. Its external integrity receipt verifies 597 files, zero byte/permission/path differences, self-contained Git and identical extracted Git status. This archive predates the owner's later commit/push instruction and corresponding documentation update. Pages work was stopped at the owner's correction; the current source/docs destination is the GitHub `ChatGPT` branch. Original recovery archives remain intact.

**Skill continuity:** the Superpowers and Causal Mechanism Compiler root skills were read again during the brief resume. Two attempted Superpowers module reads failed to resolve before the owner paused. Discover valid module resource identifiers before claiming those workflows were applied; the failed reads made no code changes. Preserve the existing instructions to use `threespine` and applicable Godot verification guidance when their domains are touched.

## 1. The exact point to resume

**Current AS-023 boundary, 2026-10-01:** The normal grade opening is the manual wooden slingshot. The higher-launch source and receiving-frame extension through +352 m are implemented in the agent-owned checkout based on `f7a584dcceb576068f62e030e35d1b88e5cd4c5a`; the current bounded local native/touch/render/audio proof is complete, with device and wider-route acceptance still open. Preserve the authored upper machinery/parkour. The old pipe bridge is selected explicitly by `WorldContent::PipeBridge` fixture tests; passing its grade→+110 m route does not prove continuity from the replacement opening. Do not restart the original opening design or silently restore the old bridge to normal play.

Read [AS-023's bounded contract](03_EXECUTION/ASCENT/AS-023_MANUAL_SLINGSHOT.md), the [Atlas's current opening](01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md#6-opening-mechanism-the-pipe-loaded-balance-bridge) and [the local evidence ledger](evidence/manual_slingshot.md). The compiled fixed-fork revision is recorded in the Atlas. Fresh native refinement measures apex Y=384.344/384.167/384.223 m at 90/180/360 Hz, closed pre-impact residual 1.17/0.60/0.30%, and peak source 198.205 kW under 200 kW. Manual retrieval takes 5.044 s with 9.123 kW peak, zero final rubber energy and an actual second shot. All 33 restart destinations pass 2,894 checks. The full 28-gate suite and actual shipping-touch +352 m supported arrival pass; final real-render/audio inspection also passes, with actual device acceptance still open. The evidence ledger owns exact receipts. Source publication and exact-source CI/APK delivery now pass; see Start Here for the immutable build. Android/device/Fold acceptance remains separate and open.

**Current causal correction:** bolt the timber fork at 82° and read anchors from its static transform. Aim only the 60 kg rail using an actual SixDOF gimbal and finite 200 kW torque source with slack or drawn bands; measured Jolt angles/settled readiness permit a passive latch. Do not restore the unpaid kinematic fork aim or add gameplay aim/launch pose/velocity setters. Fresh native and touch measurements now verify this correction; use the evidence ledger for receipts. The landing model is an upright arcade capsule with contact impulses and finite ground recovery forces, not articulated feet or a ragdoll. Internal machine-member collision filters are declared separately from the sole player/pouch pair exclusion in AS-023.

**Latest owner amendment:** ordinary gameplay is touch-only. Prioritize whole-body comic poses, thought bubbles and mechanical boing over facial detail. Native pre-contact support-relative normal/tangential energy and bounded physical footwork/recovery must govern all landings; smooth first-person impact/recovery presentation follows that result. Production-native and actual viewport touch full-draw/parachute sequences reach supported +352 m Tower with zero deaths and continued walking. The final 28-test suite passes, including native landing energy/recovery; shipping contact/camera checks pass. Final rendered/audio inspection passes; actual device play remains open.

**Historical migration point before AS-023:**

**The continuous normal touch route from grade to a supported +110 m tower ring has passed in GitHub Actions.** The last session paused before that run finished. Its successful result was checked while preparing this document. Do not restart AS-021/AS-022 implementation or treat that run as still pending.

The next unfinished work at that earlier migration point was **visual review of the captured AS-021/AS-022 gameplay, recording the resulting acceptance/limitations, and any concrete corrections that review reveals**. APK installation, Android execution and sustained Fold performance remain unverified. Above +110 m, further ascent is a design/implementation frontier, not a completed route.

This is a continuity and evidence record, not a second product specification. [Start Here](00_START_HERE.md), the [Atlas](01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md) and the existing slice tickets retain their names and ownership. Older ticket paragraphs saying the +110 m rendered route/APK is pending describe the earlier local handoff; the dated result below supersedes that status. Their physical calculations and open visual/device concerns still matter.

## 2. Verified baseline and evidence

**Historical baseline, preceding the AS-023 opening replacement.** Preserve these immutable run/APK identifiers. They do not describe the local higher-launch candidate; its receipt belongs in [manual_slingshot.md](evidence/manual_slingshot.md) and Start Here §2.

| Item | Observed result |
|---|---|
| Last tested executable source | `4996a6e8cca0575b640a0500956b179e0ea77768` |
| Exact-source Actions run | [36788027524](https://github.com/IssisX/ScraperX/actions/runs/36788027524), **success**, completed 2026-09-30 23:40:03 UTC |
| Full x86 native CTest | **24/24 passed**, 58.85 seconds, in `native-tests.log` |
| Normal world identity | `SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim scene=pipe_bridge` |
| Continuous rendered touch | `SCRAPERX_UITEST PASS touch_north_frame grade_to_110m=1 north_frame=1 hanging_shimmy=1 support=11 deaths=0 arrival_y=110.900` |
| Other required gates | Godot import, camera, normal inventory, prior bridge input, retained regression input/render/audio, both world-solids comparisons, Android ARM64 compile, signed APK export and publication all passed |
| Artifact | [11131319411](https://github.com/IssisX/ScraperX/actions/runs/36788027524/artifacts/11131319411), `ScraperX-build-4996a6e8cca0575b640a0500956b179e0ea77768` |
| APK identity | `ScraperX-ChatGPT.apk`; package `com.cory.scraperx.chatgpt`; installed label `ScraperX-ChatGPT` |
| APK SHA-256 | `c7130842817f8a17c701464e178291b3caa231aa7300077ccbab343dda9fa7d1` |
| Branch signer SHA-256 | `9a06889e7614d140f6cd1fc45634bb1e2391968a9fcc60f1608c9e50e15a1ba4` |

The artifact was downloaded during this handoff. Its normal-route log, native summary, `apk/CHECKPOINT.txt`, badging and signing logs were read. The downloaded APK's SHA-256 was independently recomputed and matches its checksum file. Signature verification was performed by CI and its successful log was inspected; this handoff did not rerun a local APK signature verifier or install the APK.

The new screenshots exist and the workflow's explicit capture assertions passed. **The AS-021/AS-022 images from this run have not yet been visually inspected.** PNG existence, native assertions and a successful export do not establish visual readability or device play. Capture resolution was 432×371 at 30 fixed FPS for the CPU renderer; CI restored the shipping viewport settings before export. The +110 capture occurred at 1,259,108 ms after process start. The workflow now allows 2,400 seconds for this route and 90 minutes for the whole job.

Previous local Termux/ARM testing passed 23 selected tests, including north-frame routes with 90/60/360 Hz caller partitions feeding the native 90 Hz step. The excluded broad ARM executable had a previously reproduced retired AS-002 mid-landing failure. Do not generalize that platform-specific exclusion: the full x86 suite, including the broad executable, passed 24/24 in this run. Do not remove or weaken its CI gate.

## 3. Establish the new environment without touching personal files

Use the ChatGPT environment's isolated project checkout if it provides one, or create your own clone in its agent workspace. Verify the actual environment, available tools, network access and governing instructions. The former Termux filesystem and Downloads directory are **not** locations to recreate or depend on. This is an existing C++/Godot game, not a request to scaffold a web app from unrelated workspace boilerplate.

For a fresh clone, from your actual agent workspace:

```sh
git clone --branch ChatGPT --single-branch https://github.com/IssisX/ScraperX.git ScraperX-codex
cd ScraperX-codex
git status --short --branch
git log -6 --oneline
git fetch origin ChatGPT
git rev-parse HEAD
git rev-parse origin/ChatGPT
```

Recover any newer work before editing. Expect the documentation handoff commit to follow `4996a6e`; it does not represent a new executable build. Do not reset to the old source SHA, force-push, switch write branches or discard other agents' changes.

The previous agent-owned clone was `/data/data/com.termux/files/usr/tmp/scraperx-agent-dlm6eNWc/repo`. Its only untracked content at handoff was `build-make/`, a disposable local native build directory. Source was committed and pushed. This path is provenance for the old environment only. Previous tool sessions, subagents, downloaded files and build caches are not dependencies of the new environment.

## 4. First work after the owner resumes

**Historical continuation at the earlier recovery point:** recover the existing scratch changes and tool/build environment; collect the final rebuilt all-suite and full-world input/collision/supported-arrival receipts; inspect genuine wood/band/humanoid/reticle/prediction/orbit/first-person and final settings/restart views; preserve retained fixture regressions. Update the exact local receipt without inventing CI or device acceptance. Rebuild the bridge after consequential native edits; the current shipping-scene test already verifies all 33 Grade/ring presets. Keep further dynamic encounters around/above the new landing range as future authored slices, not implied by this frame extension.

The latest integrated candidate also needs actual touch-brake roof arrival, landing-response measurements/tests across support/impact conditions, smooth first-person recovery and state-driven humor observation. Keep the 27-gate result attached to the prior stable source until all 28 rebuilt gates pass. Current rendered options proof passes 2,898 checks and its two pause captures have been inspected.

The session's temporary paths and repository-relative Godot/CTest commands are in [the local evidence ledger](evidence/manual_slingshot.md#reproducible-local-proof-paths). A fresh environment must replace those paths with its own tools/output directories. The following list preserves the earlier migration's baseline-evidence recovery steps; it is not an instruction to substitute that old grade route for current AS-023 proof.

1. Recheck the remote head and the run above. If source advanced, identify the newer source/run before applying these conclusions to it.
2. Read this document, [Start Here §2](00_START_HERE.md#2-current-position--66-m-automated-route-passed-layout-rejected-repairs-and-fold-play-unverified-2026-09-27), the [execution protocol](02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md), [AS-021](03_EXECUTION/ASCENT/AS-021_BRACED_BAY.md), [AS-022](03_EXECUTION/ASCENT/AS-022_NORTH_SERVICE_FRAME.md), and the [Sol package/running plan](03_EXECUTION/PLANNING/SOL_ASCENT_HANDOFF.md). Load only the relevant Laws/GDD/Atlas/TDD sections when a decision needs them.
3. Retrieve the immutable evidence into agent-owned storage. With authenticated `gh`, from the clone in the isolated workspace:

```sh
gh run view 36788027524 -R IssisX/ScraperX
gh run download 36788027524 -R IssisX/ScraperX \
  -n ScraperX-build-4996a6e8cca0575b640a0500956b179e0ea77768 \
  -D ../ScraperX-evidence-4996a6e
```

4. Visually inspect the actual images, using the environment's image-viewing capability. The artifact contains `screenshots/north_braced_bay_{preview,entry,transfer,low_member,upper_junction,arrival}.png` and `screenshots/north_north_frame_{entry,first_rest,99m,upper_rest,hang,shimmy,106m,upper_exit,110m}.png`. Those braces describe filename groups, not one literal filename. Check holds, body clearance, the gap and receiver depth, canopy/pocket readability, apparent support connections and camera obstruction. Earlier mechanism captures are also retained under `screenshots/north_*.png`.
5. Record what is actually visible and what remains uncertain. Correct any demonstrated problem at its owner, then rerun the affected input/render/native checks and full delivery path if executable source changed. If no correction is needed, close the corresponding evidence items without inventing another implementation task.
6. Update the existing slice/status documents and running to-do, preserving headings, labels, entity IDs and reference targets. A documentation-only status commit can use `[skip ci]`; it must continue to name the actual tested executable SHA and artifact. Every APK build remains a GitHub Actions operation.

The previous session visually inspected the older `2e622f7` braced-bay preview and teeter views. The preview was partly obscured by a tower post/framing; the teeter shelf view was crowded by the beam underside. Those are observations of older images, not findings about the uninspected new capture set. AS-022's first ladder also exceeds the earlier chosen 4.5 m uninterrupted-grip pacing target. Its source and tests are real, but pacing/visibility must be assessed before repeating that design. Do not present an automated reachability result as proof that the route is fun or clear to a first-time player.

## 5. Current source owners and route

Native C++17/Jolt owns consequential state at 90 Hz. Godot 4.7 owns input, presentation and Android delivery. Current Simulation source uses an 85 kg dynamic player, coupled to a 15 kg pouch; the 60 kg physical carrier is also included in the AS-023 ledger. Actual rubber forces act through the real draw restraint and passive 12 m aimed guide 2901, using four Jolt collision substeps per fixed tick. Release sets no launch pose or velocity. Only the rider/pouch collision pair is filtered while joined and until measured 0.10 m AABB clearance; world/tower contacts remain active, and restart/drop/reset restore the pair. Actual transient CCD contact or external force sets `ledger_valid=false` while retaining the raw residual. The <2% proof concerns the closed pre-impact draw/guided phase only. The humanoid climber is a visual rig following the native capsule. Preserve real contact, rotating-support velocity, departure momentum, unloading and the full parkour system.

| Route / owner | Current role |
|---|---|
| AS-023 / `src/sim/slingshot.*`, `slingshot_model.*` | Current normal grade opening: finite manual draw, held rubber stretch, actual pouch/rider coupling, passive guide release and manual retrieval; fixed-fork/finite-gimbal correction is being integrated, fresh receipt pending. |
| AS-016 through AS-019 | AS-016 is now an explicit pipe-bridge fixture. AS-017 façade, AS-018 swinging stair and AS-019 counterweight lift/exterior parkour remain above the changed opening; the old complete grade chain is historical proof. |
| `src/sim/teeter_rise.cpp`, `evidence/teeter_stage.md` | AS-020 +66→+77 m: dynamic yellow beam entity 2800, captive 65 kg ballast 2801, fixed frame 1800, stops 1801/1802. The player pushes ballast and loads the real hinged beam. This obstacle already exists and has physics/rendered proof. |
| `src/sim/braced_bay.cpp`, `tests/braced_bay_tests.cpp` | AS-021 +77→+88 m: static Kit entity 1900, diagonal balance girders, a gap jump, crouched portal and return mantle |
| `src/sim/north_service_frame.cpp`, `tests/north_service_frame_tests.cpp` | AS-022 +88→+110 m: static Kit entity 1901, offset climbs/rests, real +99 ring, raised jump/hang, canopy shimmy, final catwalk and mantle |
| `src/sim/simulation.cpp`, `.hpp` | Active world construction, player controller, ledge/hang/mantle/contact/checkpoint owner; test-only `InitialSpawn::NorthFrameEntry = 32` |
| `godot/presentation/ui/ui_test_driver.gd` | Ordinary viewport-input scenarios: `touch_north_frame` calls braced bay → teeter → earlier routes, beginning at normal grade spawn |
| `godot/presentation/main.gd`, `slingshot_view.gd`, `climber_character.gd` | Normal/fixture selection; native slingshot input, Kit rendering, wood/leather/rubber, actual humanoid visual rig, aim/energy HUD, native projection, orbital camera/first-person return and audio routing |
| `godot/presentation/ui/pause_menu.gd`, `settings_store.gd`, `tests/settings_runtime_test.gd` | Persistent input/graphics/display/audio controls and native-validated checkpoint, supported-ring, height/side and exact-XYZ restart actions. Grade/ring targets must match the enlarged source geometry. |
| `.github/workflows/wo000-delivery-spine.yml` | Full native, Godot, rendered route, regression, audio, solids and Android build/export gates |

The new static frames each use one authoritative Kit part list for both collision and rendering. Native traversal supplies the player action; these frames contain no moving machine coordinate or powered reset. Compiler LINK/contact/clearance checks and Three Spine locomotion calculations apply. Do not invent a dynamic mechanism sweep result for a static structure. Structural deformation and metal joint strength are not simulated.

Recent changes that explain the source:

- `2e622f7`: AS-021 and a checkpoint-footing repair. A centre ray alone had accepted a precarious edge perch. The owner now checks capsule resting depth and neighboring coplanar support points.
- `0728403`: AS-022 and normal touch continuations. Hanging capture now checks the hanging capsule pose separately from the later standing top-out pose. A canopy can permit a catch while preventing a mantle; a shimmy can expose clear top-out space. Native tests cover blocked early top-out, release/retry, varied catch starts and fatal-fall recovery at +99/+110.
- `c8a0d46`: corrected touch shimmy direction. Facing west is yaw +PI/2; view-right is world north (−Z).
- `4996a6e`: corrected the second scene-selection list in `main.gd`. The preceding run entered retired regression fixtures and failed an audio assertion at the old pipe bridge. Both the UI driver and main scene must recognize extended scenarios as normal routes. `_pipe_bridge()` now rejects a regression scene immediately. This commit also extended CI time limits; its complete run passed.

The retired intake, water-lift/cage campaign and old upper route survive only as explicit regression fixtures. The ordinary ramp/stair bypass to +154 m was removed. Do not restore them as shortcuts or describe fixture successes as normal campaign ascent. Preserve the separate normal and regression world-solids tables; Kit-owned geometry does not require manually adding duplicate entries to those tables.

## 6. Owner direction that must survive the migration

**Durable current charter, 2026-10-01:** first-person 3D, open industrial megastructure, unbroken physical ascent from ground through hundreds of metres and beyond 1,000 m toward the existing 1,600 m tower. Huge exposed height, heavy understandable cause and effect, satisfying danger and physical movement make the environment the main system. [The Atlas §1 summary](01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md#1-content-decision), [Governing Laws](01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md) and [GDD](01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md) retain the authority; do not drift into a launcher-only game or a repeated generic obstacle course.

Consequential C++/Jolt geometry, collision, forces, constraints, momentum, torque, moving supports and mechanical state create the routes. Godot supplies touch input, presentation, camera, HUD and Android delivery. Preserve varied jump/vault/mantle/hang/shimmy/crouch/moving-support parkour and large visible machines that physically create paths: counterweights, racks, pulleys, pivots, gears, cranes, linkages, catches, lifts and hinges. Avoid repetitive machines, regular ramp/stairs and tiny unreadable generic obstacles. No progress teleport, invisible blocker, fake route flag, scripted machine substitute or fake physics may claim success. Authorized development/test restart tools are explicit non-progress actions, never ascent evidence.

- Latest 2026-10-01 direction selects AS-023: sturdy wood, thick rubber, rapid finite arcade manual charging, a several-hundred-metre launch and the enlarged receiving frame. Setback is permitted. Preserve authored upper parkour/machinery and author other dynamic encounters around/above the demonstrated new landing range as continuing work. The latest owner instruction authorizes commit/push of this verified slice to `ChatGPT`.
- Latest follow-up selects touch-only gameplay, body/thought-bubble/mechanical-boing humor, and native support-relative normal/tangential impact-energy response with bounded footwork/recovery for every landing. First-person impact/recovery presentation must remain smooth and follow native state. Facial refinement is not the focus; final touch/landing/camera/humor proof remains open.
- Continue supported parkour/mechanism ascent beyond **1,000 m** toward the existing **1,600 m** tower destination. The earlier +300 m milestone and current +352 m slingshot receiving range are interim stages; the slingshot is one opening encounter, not the whole game.
- Use simple primitives creatively. Many sections should be climbing/parkour; many should use real physical machines. Choose their mix for challenge, readability and pacing rather than a rigid alternation or repeated lift/ladder template.
- Players should think, reposition loads or adjust world objects where the encounter benefits from it, then use the physical result to climb. A small coherent mechanism is enough. Avoid gratuitous chains and recurring tool/handle chores that add no useful decision.
- The causal law is **initial physical state plus physical laws and ordered external inputs determines the outcome**. Do not force success with player-presence animation, invisible supports, authored body poses, trigger impulses, hidden energy, or double-counted player weight. State the tested determinism boundary rather than promising cross-platform bit identity.
- Compile new machines before coding: source of work, attachment, mass/COM/inertia, contact, travel, swept clearance, stops, entry/receiver, failure recovery and any physically powered reset. Repair the authoritative owner and preserve Godot/native Jolt responsibilities and identifiers.
- Read and use the actual installed **Superpowers**, **Causal Mechanism Compiler**, and **`threespine`** instructions. Discover their locations in the new environment; old absolute skill paths are not portable. Do not merely claim use or invent an invocation. If a required skill is unavailable, report that exact limitation before claiming its workflow was applied. Use the actual Godot verification instructions when changing Godot code.
- Latest explicit owner direction resolves contradictions in older project documentation, subject to the environment's higher-priority instructions. Preserve existing labels and links; make focused corrections rather than renaming/reorganizing the documentation.
- Keep a running to-do. Work through routine, reversible implementation without repeated permission prompts. Respect real environment approval rules, explain a concrete access blocker if one occurs, and avoid the owner's personal files. APKs are built by GitHub Actions. Screenshots require explicit invocation and visual inspection; they are not supplied by an APK build automatically.
- The owner prefers substantial, well-specified coding assignments for model handoff. Complete difficult physical/ownership decisions and identify a meaningful handoff boundary. The prior migration session was paused; that historical boundary has since been superseded by the explicit AS-023 continuation above.

## 7. Running to-do at this handoff

**Current AS-023 playtest repair, 2026-10-02:**

- [x] Reproduce charged native aim lock, replace it with paid loaded pitch/yaw aiming and preserve held extension.
- [x] Replace the fork from the committed owner reference using shared native/rendered round timber parts; repair real Grade approach clearance.
- [x] Verify half-tick rail/camera SLERP and displacement-following bullet-time opening; retain the native 90 Hz solver and comfort cancellation.
- [x] Verify a real release directly from adjusted charged aim, full native suite, shipping headless touch launch/+352 m landing and all restart/settings checks.
- [ ] Finish rendered touch inspection and publish the complete repair/reference/handoff on `ChatGPT`; verify the exact-source CI/APK to terminal status.
- [ ] Observe the repair APK on the owner’s device: loaded aim range, collision feel, frame pacing and cinematic comfort. Profile reproducible frame-time problems before claiming a fix.

**Completed preceding AS-023 integration:**

- [x] Recover the original scratch and preserve verified source/evidence backups outside the executor.
- [x] Prepare the GPT-6 Sol 6.1 checkpoint and pause without starting new gameplay edits or builds.
- [x] Repair explicit BOARD/manual-draw authorization in native and touch input; prove approach cannot auto-seat or charge.
- [x] Forward native `aim_locked` through the existing Simulation/bridge state seam.
- [x] Extend outside-contact audit coverage to the launch rail during aim/draw; preserve raw residuals and verify production callbacks.
- [x] Implement the first native manual-draw/held-stretch/release slice and Godot presentation/options.
- [x] Load a locally built native extension in Godot 4.7 and exercise shipping-scene pause/settings/restart integration for the preceding variant.
- [x] Explicitly capture and inspect current graphics/restart pages with the direct 33-choice ring selector.
- [x] Complete the fixed 82° bolted fork / finite physical SixDOF rail-gimbal correction and reconcile final source in the Atlas.
- [x] Measure final fixed-fork power/pre-impact residual/refinement, manual retrieval and second shot.
- [x] Verify all 33 Grade/+11..+352 m restart choices, current standing XYZ, occupied-point rejection and real checkpoint restoration in the shipping scene.
- [x] Assert final-candidate supported +352 m Tower arrival from full draw and ordered native/touch upward parachute, zero deaths and continued walking.
- [x] Complete touch-only brake/roof scenario and native energy/footwork recovery for all landings; measure the resulting response.
- [x] Collect the final rebuilt 28-test suite and actual touch-input/collision/supported-arrival receipt.
- [x] Capture/inspect final wood/bands/pouch/humanoid/aim/launch/POV/roof/humor views and all three current options/help images; complete both real-rendered touch scenarios and 95 actual-mixer audio checks.
- [x] Complete independent read-only candidate review; no remaining Important/Critical findings. State absent CI/APK/device evidence; publication is now explicitly owner-authorized.
- [x] Preserve the post-integration source in an independently verified local backup and commit the exact proof receipts; original recovery backups remain intact.
- [x] Publish the complete verified AS-023 source to `ChatGPT` at `3d664e4` and verify its successful exact-source CI/APK delivery.
- [x] Receive the preceding APK playtest and implement the bounded launcher repair above; renewed device acceptance remains open.
- [ ] Inspect/fix `ScraperX APKs` build selection in its own implementation when that source is available; do not mistake stale downloader output for missing game source.
- [ ] Select a real 10–20 m tower gap with the owner, then compile/implement a climbing section or causal machine encounter with physical entry, receiver, exit and failure recovery.

**Historical migration checklist, preserved for the preceding pipe-bridge source:**

- [x] Commit and push AS-021/AS-022 source, native owner repairs and normal touch scenarios on `ChatGPT`.
- [x] Verify local selected native tests and the complete 24-test x86 CI suite.
- [x] Verify normal rendered grade→+110 m with actual tower support and zero deaths.
- [x] Invoke and retain the braced-bay and north-frame screenshot set.
- [x] Build/export the exact-source Android APK in Actions; inspect its identity/signing logs and independently match the downloaded APK checksum.
- [ ] Visually inspect the new screenshots and resolve any observed readability/geometry defects.
- [ ] Finish AS-021/AS-022 visual acceptance records and reconcile their older pending-status paragraphs in place.
- [ ] Observe actual APK installation/play and Fold performance if the new environment has a real device path. Otherwise retain this boundary explicitly.
- [ ] On the owner's continuation instruction, compile the next useful encounter from the demonstrated +110 m receiver within the broader +300 m objective. No particular mechanism above that ring is selected by this handoff.

There was no native or CI failure to repair at the historical `4996a6e` migration point. Current work is the explicit AS-023 revision above; its changed source needs its own fresh receipt. Preserve the earlier IDs and proof rather than retesting old fixtures as current opening acceptance.
