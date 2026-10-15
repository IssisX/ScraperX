extends SceneTree

# One existing normal-world guardrail stage. All catches/reaches/releases use
# the shipping router and native owner; no automatic promotion or fake grip.
const STEP := 1.0 / 90.0
# Four float32 ULPs at this rail's118m world coordinate. A material point
# survives world/local conversion without imposing a false static-world pin.
const MATERIAL_POINT_EPS := 4.0 / 131072.0
const UiTestDriver := preload("res://presentation/ui/ui_test_driver.gd")
var main: Node
var native: Object
var failures := 0
var checks := 0
var capture_dir := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SECOND_HAND_REACH FAIL " + why)

func step(count: int = 1) -> void:
	for i in count:
		main._process(STEP)

func key_action() -> void:
	var event := InputEventKey.new()
	event.physical_keycode = KEY_E
	event.pressed = true
	root.push_input(event, true)
	event.pressed = false
	root.push_input(event, true)

func touch_button(id: StringName) -> void:
	var event := InputEventScreenTouch.new()
	event.index = 5
	event.position = main._touch.button_center(id)
	event.pressed = true
	root.push_input(event, true)
	event.pressed = false
	root.push_input(event, true)

func local_hand() -> Vector3:
	var body: int = native.get_kit_body_index(2952)
	var support: Transform3D = native.get_kit_body_transform(body)
	return support.affine_inverse() * native.get_traversal_left_hand()

func capture(why: String) -> void:
	if capture_dir.is_empty():
		return
	await process_frame
	await RenderingServer.frame_post_draw
	check(root.get_texture().get_image().save_png(capture_dir.path_join(why + ".png")) == OK,
		"actual software phone-control capture " + why)

func solo(generation: int, anchor: Vector3) -> bool:
	var state: Dictionary = native.get_landing_state()
	return int(native.get_traversal_state()) in [1, 4] and int(native.get_traversal_support_entity_id()) == 2952 \
		and int(state.traversal_hand_constraint_count) == 1 \
		and bool(state.left_hand_attached) and not bool(state.right_hand_attached) \
		and int(state.left_hand_generation) == generation and int(state.right_hand_generation) == 0 \
		and local_hand().distance_to(anchor) <= MATERIAL_POINT_EPS

func _run() -> void:
	if not capture_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(capture_dir)
	root.size = Vector2i(2160, 1856)
	main = load("res://main.tscn").instantiate()
	main._settings_persistent = false
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene and not main._ci_mode, "shipping Main/native normal world")
	step(180)
	check(native.debug_restart_at(Vector3(16.85, 11.5, -118.0)), "single real guardrail approach stage")
	main._yaw = -PI * 0.5
	main._pitch = 0.0
	main._crouch_toggled = false
	main._router.clear_held()
	step()
	check(native.get_tick_index() == 181 and native.is_grip_available(), "actual existing rail visible before catch")
	key_action()
	step()
	var caught: Dictionary = native.get_landing_state()
	var generation: int = caught.left_hand_generation
	var anchor := local_hand()
	check(generation > 0 and solo(generation, anchor), "ordinary E catches only the actual available left hand")
	check(caught.has("missing_hand_grip_available"), "new native ABI supplies missing-hand grip availability")
	check(main._ctx.action.id == &"reach_hand" and main._ctx.action.label == "REACH HAND", "solo Action always offers explicit Reach Hand")
	check(not bool(caught.missing_hand_grip_available) and main._ctx.action.detail == "NO CLEAR GRIP", "blocked native target exposes honest detail")
	check(main._touch.is_button_shown(&"action") and not main._touch._buttons[&"action"].enabled
		and main._touch.button_label(&"action") == "REACH HAND", "phone Action remains visible but disabled without real second grip")
	check(main._touch.button_label(&"jump") == "LET GO" and main._touch.button_label(&"drop") == "LET GO", "Jump and Drop preserve passive release labels")
	main._hud.touch_active = false
	var prompts: Array = main._hud._build_prompts(main._ctx)
	check(prompts.any(func(prompt: Dictionary) -> bool:
		return prompt.verb == &"action" and prompt.text == "REACH HAND" and prompt.detail == "NO CLEAR GRIP"), "keyboard and pad HUD distinguish Reach Hand from release")
	var legacy_driver := UiTestDriver.new()
	legacy_driver._main = main
	check(legacy_driver._single_hand_controls_honest("TUCK"), "retained solo-control scenario helper accepts new explicit Reach contract")
	legacy_driver.free()
	key_action()
	step()
	check(native.get_tick_index() == 183 and solo(generation, anchor), "blocked keyboard E preserves solo grip instead of dropping")
	touch_button(&"action")
	step()
	check(solo(generation, anchor) and main._touch._buttons[&"action"].index == -1, "disabled phone Action cannot dispatch or retain touch capture")
	await capture("blocked-reach")
	var passive_kept_solo := true
	while native.get_tick_index() < 230:
		step()
		passive_kept_solo = passive_kept_solo and solo(generation, anchor)
	check(not bool(native.get_landing_state().missing_hand_grip_available), "30 mm alternative still rejected by real palm bound at230")
	while native.get_tick_index() < 259:
		step()
		passive_kept_solo = passive_kept_solo and solo(generation, anchor)
	check(passive_kept_solo, "neutral hanging never automatically promotes missing hand")
	check(bool(native.get_landing_state().missing_hand_grip_available) and bool(main._ctx.missing_hand_grip_available), "native later reports actual reachable own-hand grip")
	check(main._ctx.action.id == &"reach_hand" and main._ctx.action.detail == ""
		and main._touch._buttons[&"action"].enabled, "phone Reach Hand enables only when native query accepts")
	await capture("ready-reach")
	touch_button(&"action")
	check(solo(generation, anchor), "phone press queues request without optimistic second hand")
	step()
	var pair: Dictionary = native.get_landing_state()
	check(native.get_tick_index() == 260 and int(pair.traversal_hand_constraint_count) == 2
		and bool(pair.left_hand_attached) and bool(pair.right_hand_attached), "actual phone Action adds second native hand on next solve")
	check(int(pair.left_hand_generation) == generation and int(pair.right_hand_generation) > 0
		and local_hand().distance_to(anchor) <= MATERIAL_POINT_EPS, "new right generation preserves original planted left row and anchor")
	check(not bool(main._ctx.single_hand_hold) and main._ctx.action.id != &"reach_hand", "acknowledged pair removes solo Reach Hand offer")
	check(int(native.get_traversal_state()) in [1, 4] and not bool(pair.foot_push_active), "second hand does not fabricate topout or foot support")
	await capture("paired-hands")
	print("SECOND_HAND_STATE tick=", native.get_tick_index(), " traversal=", native.get_traversal_state(), " generations=", pair.left_hand_generation, ",", pair.right_hand_generation, " material_error_m=", local_hand().distance_to(anchor))
	var right_generation: int = pair.right_hand_generation
	for i in 4:
		main._process(0.0)
	check(native.get_tick_index() == 260 and int(native.get_landing_state().right_hand_generation) == right_generation, "duplicate zero-tick render cannot add another hand")
	touch_button(&"drop")
	step()
	check(int(native.get_landing_state().traversal_hand_constraint_count) == 0, "ordinary phone Drop still releases actual hands")
	check(native.get_death_count() == 0, "bounded diagnostic causes no death or recovery")
	print("SECOND_HAND_REACH %s checks=%d native_tick=%d diagnostic_stage=1 phone_input=1 device_play=0" % ["FAIL" if failures else "PASS", checks, native.get_tick_index()])
	quit(1 if failures else 0)
