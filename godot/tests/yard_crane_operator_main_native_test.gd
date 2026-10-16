extends SceneTree

# Real Main/router/native/camera/hands/audio path. One initial booth stage only;
# full public freight delivery and four on-foot sling eyes have separate tests.
const STEP := 1.0 / 90.0
var failed := false
var checks := 0
var capture_dir := ""

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failed = true
		push_error("YARD_CRANE_OPERATOR_MAIN FAIL " + label)

func key(main: Node, code: Key, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	main._router._input(event)

func mouse_look(main: Node, relative: Vector2) -> void:
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	main._router._input(click)
	click.pressed = false
	main._router._input(click)
	var motion := InputEventMouseMotion.new()
	motion.relative = relative
	main._router._input(motion)

func step(main: Node, count: int) -> void:
	for i in count:
		main._process(STEP)

func neutral(state: Dictionary) -> bool:
	return state.hoist_target_mps == 0.0 and state.slew_target_rad_s == 0.0 and state.trolley_target_mps == 0.0

func capture(label: String) -> void:
	if capture_dir.is_empty():
		return
	await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and image.save_png(capture_dir.path_join(label + ".png")) == OK,
		"actual Main viewport capture " + label)

func check_hands(main: Node, state: Dictionary, label: String) -> void:
	check(not main._arms._remote.visible, label + " has no handheld radio")
	for hand in main._arms._hands:
		var grip: Vector3 = state.operator_left_hand if hand.side < 0.0 else state.operator_right_hand
		check(hand.world_wrist.distance_to(grip) < 0.025, label + " world wrist matches native cab grip " + str(hand.side))

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	_run.call_deferred()

func _run() -> void:
	var main = load("res://main.tscn").instantiate()
	root.add_child(main)
	main.set_process(false)
	check(main._native != null and main._native.get_class() == "ScraperXSimulation", "actual native Simulation")
	var native = main._native
	check(native.debug_restart_at(Vector3(-66.5, 1.15, -150.0)), "single initial stage above real booth floor")
	step(main, 90)
	var ready: Dictionary = native.get_yard_crane_state()
	check(native.is_player_grounded() and native.get_support_entity_id() == 1450 and ready.controls_reachable,
		"actual 1450 foot contact grants crane controls")
	check(main._ctx.station == &"yard_crane" and main._ctx.action.id == &"operate", "Main offers native crane station")
	var entry_position: Vector3 = native.get_player_position()
	key(main, KEY_E, true); step(main, 1); key(main, KEY_E, false)
	check(main._operating == &"yard_crane" and main._router.yard_crane_mode, "E enters dedicated crane mode")
	check(absf(wrapf(main._yaw + PI / 2.0, -PI, PI)) < 0.001 and absf(main._pitch) < 0.001,
		"entry faces actual +X crane front")
	check(native.get_player_position().distance_to(entry_position) < 0.02, "entry does not teleport player")
	check(main._camera.position.distance_to(native.get_player_render_position() + main.EYE_OFFSET) < 0.1,
		"camera remains at actual grounded operator eye")
	step(main, 30)
	check_hands(main, ready, "forward view")
	mouse_look(main, Vector2(0.0, 150.0)); step(main, 20)
	await capture("01-crane-native-grip-view")
	mouse_look(main, Vector2(0.0, -150.0)); step(main, 1)
	var before: Dictionary = native.get_yard_crane_state()
	key(main, KEY_UP, true); step(main, 90)
	var lifted: Dictionary = native.get_yard_crane_state()
	check(lifted.hoist_target_mps < 0.0 and lifted.hoist_length_m < before.hoist_length_m - 0.15,
		"UP shortens actual finite native hoist")
	check(lifted.hoist_work_j > before.hoist_work_j and lifted.hoist_source_used_j > before.hoist_source_used_j
		and lifted.hoist_source_used_j <= lifted.hoist_source_capacity_j, "hoist spends finite native source on positive work")
	check(main._yard_crane_audio._winch.playing and not main._yard_crane_audio._trolley.playing
		and not main._yard_crane_audio._slew.playing, "actual hoist alone drives spatial winch source")
	key(main, KEY_UP, false); step(main, 3)
	check(neutral(native.get_yard_crane_state()), "release neutralizes all native machine targets")
	check(not main._yard_crane_audio._winch.playing and not main._yard_crane_audio._trolley.playing
		and not main._yard_crane_audio._slew.playing, "released controls are quiet")
	key(main, KEY_D, true); step(main, 90)
	var slewed: Dictionary = native.get_yard_crane_state()
	check(slewed.slew_target_rad_s > 0.0 and slewed.slew_angle_rad > lifted.slew_angle_rad + 0.001
		and slewed.trolley_target_mps == 0.0 and slewed.hoist_target_mps == 0.0, "D independently slews actual native boom")
	check(main._yard_crane_audio._slew.playing and not main._yard_crane_audio._winch.playing, "slew has its own spatial source")
	key(main, KEY_D, false); key(main, KEY_A, true); step(main, 180)
	check(native.get_yard_crane_state().slew_target_rad_s < 0.0, "A reverses native slew target")
	key(main, KEY_A, false); key(main, KEY_W, true); step(main, 90)
	var trolley: Dictionary = native.get_yard_crane_state()
	check(trolley.trolley_target_mps > 0.0 and trolley.trolley_position_m > before.trolley_position_m + 0.02
		and trolley.slew_target_rad_s == 0.0 and trolley.hoist_target_mps == 0.0, "W independently moves actual trolley")
	check(main._yard_crane_audio._trolley.playing and not main._yard_crane_audio._slew.playing, "trolley has its own spatial source")
	key(main, KEY_W, false); key(main, KEY_S, true); step(main, 180)
	check(native.get_yard_crane_state().trolley_target_mps < 0.0, "S reverses native trolley target")
	key(main, KEY_S, false); step(main, 3)
	var mask: int = native.get_yard_crane_state().attached_mask
	check(not native.get_yard_crane_state().rigging_reachable, "cab is outside on-foot sling eye reach")
	for code in [KEY_G, KEY_R]:
		key(main, code, true); step(main, 1); key(main, code, false)
	check(native.get_yard_crane_state().attached_mask == mask, "cab rig/unrig cannot remotely change native sling legs")
	var yaw_before: float = main._yaw
	mouse_look(main, Vector2(50.0, -550.0)); step(main, 30)
	check(absf(main._yaw - yaw_before) > 0.1 and absf(main._pitch) > 0.1, "real mouse freelook survives crane mode")
	check_hands(main, native.get_yard_crane_state(), "free head view")
	check(native.get_player_position().distance_to(entry_position) < 0.08, "machine axes and freelook preserve real foot position")
	await capture("02-crane-free-head-high-view")
	mouse_look(main, Vector2(-50.0, 700.0)); step(main, 20)
	main._router.set_device(2); step(main, 30)
	check(main._touch.yard_crane_mode and main._touch.is_button_shown(&"pendant_up") and main._touch.is_button_shown(&"pendant_down")
		and main._touch.is_button_shown(&"action"), "actual Main touch layout exposes hoist and Done")
	check(not main._touch.is_button_shown(&"yard_crane_attach") and not main._touch.is_button_shown(&"yard_crane_detach")
		and not main._touch.is_button_shown(&"jump"), "actual cab layout hides unreachable sling and walking actions")
	await capture("03-crane-touch-operator-layout")
	main._router.set_device(0)
	key(main, KEY_UP, true); key(main, KEY_D, true); key(main, KEY_W, true); step(main, 2)
	key(main, KEY_ESCAPE, true); step(main, 1); key(main, KEY_ESCAPE, false)
	var paused_tick: int = native.get_tick_index()
	check(main._paused and paused, "Escape uses real pause flow")
	check(neutral(native.get_yard_crane_state()), "pause immediately clears native crane targets")
	await process_frame
	check(native.get_tick_index() == paused_tick, "pause freezes native time")
	main._resume(); step(main, 1)
	check(neutral(native.get_yard_crane_state()) and main._router.frame(STEP).yard_crane == Vector3.ZERO,
		"resume clears all held axes")
	key(main, KEY_UP, true); key(main, KEY_D, true); key(main, KEY_W, true); step(main, 2)
	key(main, KEY_E, true); step(main, 1); key(main, KEY_E, false)
	check(main._operating == &"" and not main._router.yard_crane_mode and neutral(native.get_yard_crane_state()),
		"Done exits mode and clears native commands while axes held")
	check(main._router.frame(STEP).yard_crane == Vector3.ZERO, "Done clears router machine axes")
	print("YARD_CRANE_OPERATOR_RECEIPT ", JSON.stringify({"support":native.get_support_entity_id(), "reachable":ready.controls_reachable,
		"entry_position":entry_position,"hoist_before_m":before.hoist_length_m,"hoist_after_m":lifted.hoist_length_m,
		"source_before_j":before.hoist_source_used_j,"source_after_j":lifted.hoist_source_used_j,"source_capacity_j":lifted.hoist_source_capacity_j,
		"work_before_j":before.hoist_work_j,"work_after_j":lifted.hoist_work_j,"slew_rad":slewed.slew_angle_rad,
		"trolley_m":trolley.trolley_position_m,"rigging_mask":mask,"paused_tick":paused_tick,"captured":not capture_dir.is_empty()}))
	main.free()
	native = null
	print("SCRAPERX_YARD_CRANE_OPERATOR_MAIN ", "FAIL" if failed else "PASS", " checks=",checks)
	quit(1 if failed else 0)
