# Yard crane operator presentation — 2026-10-15

Implemented at base revision `470cc214035da1143927e38fe538baddc63f7944` on `ChatGPT`; unrelated dirty Main/grapple/ParkourYard work preserved. Assigned Sol/medium; no live effort control changed. Skills: Godot input handling, camera systems, game feel; Superpowers affected-check guidance.

## Behavior

- Native `controls_reachable` exposes ENTER CAB. Entry changes view heading to native `operator_forward` only; camera/player position remains on the actual dynamic player. Free mouse/right-stick/right-touch look remains available.
- Keyboard A/D slew, W/S trolley, Up/Down hoist, E/Q leave. Pad left stick slew/trolley, RT/LT hoist, X/B leave. Touch uses two-axis left stick, large HOIST +/− and DONE controls. Rigging occurs on foot at each actual eye; contextual Action is E/padX/touch, with G/R or padY alternatives where mapped. Cab controls do not offer remote attachment.
- Cab mode suppresses movement, sprint, crouch, jumping, grapple and other machine axes/verbs. Entry, leave and focus/pause clear held input; native commands zero on pause, lost reach, exit and restart. Existing forklift selector/gear logic is unchanged.
- Outside the cab, native `rigging_reachable` and `rigging_leg` expose RIG LEG/UNRIG LEG via contextual Action (E/pad X/touch). G/R also request the nearby leg transaction. Requests never mutate the presented mask; panel reads only native acknowledgments.
- View has two MultiMesh groups: native cable endpoints and two control knobs at native hand anchors. Existing native Kit rendering owns mast/boom/trolley/spreader. First-person hands reuse accepted cab-anchor IK, and handheld remote is hidden in the crane cab.

## Exact bridge assumptions

`get_yard_crane_state()->Dictionary`, `set_yard_crane_input(slew,trolley,hoist)->bool`, `request_yard_crane_attach()->bool`, `request_yard_crane_detach()->bool`. Commands are normalized signed axes. Missing snapshot API produces no crane affordance; this compatibility guard does not prove native integration.

Fields read: available, controls_reachable, rigging_reachable, rigging_leg (0..3 or −1), attached_mask, failure (0 ready/1 obstructed/2 stock/3 sling), operator_forward (world +X), operator_left_hand/operator_right_hand (world vectors), optional operator_frame (Transform3D, local +Z front), lines (Array of native from/to vectors), hoist_actual_length_m, hoist_force_n, trolley_position_m, slew_angle_rad, hoist_source_used_j, hoist_source_capacity_j. Absent operator_frame is derived only from supplied native forward; no assumed cab/grip position is synthesized. Native request owner must gate all attachment/detachment by actual current leg reach and physical constraints.

## Execution evidence and limits

Godot 4.7 stable official `5b4e0cb0f`, Linux arm64 through Ubuntu proot. Isolated project `/data/data/com.termux/files/usr/tmp/scraperx-crane-operator-ui-20261015/godot` copied from root-selected accepted forklift project. Only this private Main excludes existing WIP ParkourYard preload/build and uses accepted one-argument grapple geometry configure call, matching the base. Repository Main retains both unrelated WIP changes.

- `--headless --path <private project> --script tests/yard_crane_operator_inputs_test.gd`: PASS (`YARD_CRANE_OPERATOR_INPUTS_PASS`, exit 0). Injected keyboard/pad/touch events exercise all machine axes, verb isolation, focus neutralization, paused intent, exit to walking, two-axis touch stick and hoist release, native hand target dictionary, grouped cable/knob rendering and actual-mask panel count.
- `--headless --path <private project> --script tests/forklift_operator_inputs_test.gd`: PASS (`SCRAPERX_FORKLIFT_OPERATOR_INPUTS PASS`, exit 0), after shared router/touch behavior changes. Later touch change only hides the crane's generic MOVE hint; forklift behavior/result remains applicable.
- `--headless --path <private project> --check-only --script presentation/main.gd`: PASS, exit 0. This is parse evidence only.
- `git diff --check` on the four modified existing presentation owners: PASS.

Initial test attempts found and repaired a new test constant shadowing native Panel, a new panel inferred-type parser error, and incomplete test context dictionaries. Final passing results above supersede those setup failures. No native crane Main runtime, APK, rendered-device, contact, force or transfer claim: private selected SO predates crane APIs. Root owns matching native integration/build and real connected transfer acceptance.

## Source receipts

- `godot/presentation/main.gd` SHA256 `2b3685949408080b0937bc54e0b73b1e38ccb234d25ca4e9cdb691b9bbfc19aa`
- `godot/presentation/first_person_arms.gd` SHA256 `f8226deadfb149d502346d41168349563ee9895b48d6add79bc3a7978b02595b`
- `godot/presentation/ui/input_router.gd` SHA256 `229cf7eff8453e63d13d72ba226a6102a30a522298658f20e399ed296df36578`
- `godot/presentation/ui/touch_controls.gd` SHA256 `9f73d119c4442a73c9360a8317f9537ce221da152fd9c6b8e26219633e261a64`
- `godot/presentation/yard_crane_view.gd` SHA256 `5d1e287ffa6c49e3d0a29a1ec66eda4132236339e476c63571a1026f7c6686ba`
- `godot/presentation/ui/yard_crane_panel.gd` SHA256 `a7676a6b3db88aa80043bc2467b1d03489d4a0951ab075f0626c163caad5b2d4`
- `godot/tests/yard_crane_operator_inputs_test.gd` SHA256 `41c9e888ed84d8cb214838ce9a554c751843ae60e3bf08b21820fcadb7244ff2`

Final actual Main/native integration: see OPERATOR_MAIN_NATIVE.md (40checks,3inspected captures). Final short native-rate-driven lever travel: see LEVER_FEEDBACK.md. Earlier absent-native limitation and hashes above describe the initial UI-only stage, superseded by those source-bound records.
