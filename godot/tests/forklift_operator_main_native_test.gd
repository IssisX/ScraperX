extends SceneTree

# Real Main -> router -> ordered native controls -> native/readback -> cab/arms.
# One initial platform stage; public boarding and cargo path have a separate fixture.
const STEP := 1.0 / 90.0
var failed := false
var checks := 0
var capture_dir := ""

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failed = true
		push_error("FORKLIFT_OPERATOR_MAIN FAIL " + label)

func key(main: Node, code: Key, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	main._router._input(event)

func step(main: Node, count: int) -> void:
	for i in count:
		main._process(STEP)

func capture(label: String) -> void:
	if capture_dir.is_empty():
		return
	await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and image.save_png(capture_dir.path_join(label + ".png")) == OK,
		"actual viewport capture " + label)

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
	check(native.debug_restart_at(Vector3(-100.0, 1.34, -57.65)), "initial stage above real rear floor")
	step(main, 90)
	var ready: Dictionary = native.get_forklift_state()
	check(native.is_player_grounded() and native.get_support_entity_id() == 2440 and ready.controls_reachable,
		"actual operator-floor contact grants controls")
	check(main._ctx.station == &"forklift" and main._ctx.action.id == &"operate", "Main offers native station")
	key(main, KEY_E, true); step(main, 1); key(main, KEY_E, false)
	check(main._operating == &"forklift" and main._router.forklift_mode, "Action enters dedicated mode")
	check(main._forklift_controls.gear == 0 and main._forklift_controls.parking, "entry neutral and parked")
	var frame: Transform3D = main._forklift_chassis_frame()
	check(absf(wrapf(main._yaw - atan2(-frame.basis.z.x, -frame.basis.z.z), -PI, PI)) < 0.001,
		"initial operator view faces real truck front")
	check(main._camera.position.distance_to(native.get_player_render_position() + main.EYE_OFFSET) < 0.1,
		"camera remains at actual rider eye")
	await capture("01-operator-forward")
	if not capture_dir.is_empty():
		main._pitch = -0.35
		step(main, 30)
		await capture("04-fork-tip-view")
		main._router.set_device(2) # Actual touch device switch, no substitute HUD.
		step(main, 30)
		await capture("05-touch-operator-layout")
		main._router.set_device(0)
		main._pitch = -0.12
	key(main, KEY_F, true); step(main, 1); key(main, KEY_F, false)
	check(main._forklift_controls.gear == 1 and not main._forklift_controls.parking, "F selects forward with released accelerator")
	key(main, KEY_W, true); key(main, KEY_D, true); step(main, 8)
	var driven: Dictionary = native.get_forklift_state()
	check(driven.applied_drive > 0.99 and absf(driven.steering_rad) > 0.0, "independent accelerator and actual rear steering")
	check(main._operating == &"forklift" and driven.controls_reachable, "actual rider stays in compartment")
	check(main._arms._remote.visible == false, "forklift hands have no handheld radio")
	key(main, KEY_W, false); key(main, KEY_D, false); step(main, 1)
	check(native.get_forklift_state().applied_brake == 0.0, "released accelerator coasts without service brake")
	key(main, KEY_S, true); step(main, 90); key(main, KEY_S, false)
	check(absf(native.get_forklift_state().forward_speed_mps) < 0.05, "explicit service brake physically stops truck")
	key(main, KEY_P, true); step(main, 1); key(main, KEY_P, false)
	check(native.get_forklift_state().applied_brake > 0.9, "parking holds with actual native brake")
	var before: Dictionary = native.get_forklift_state()
	key(main, KEY_UP, true); step(main, 90); key(main, KEY_UP, false)
	var lifted: Dictionary = native.get_forklift_state()
	check(lifted.lift_position_m > before.lift_position_m + 0.12 and lifted.battery_j < before.battery_j and lifted.positive_work_j > before.positive_work_j,
		"independent lift spends actual native source and work")
	check(lifted.applied_drive == 0.0 and lifted.applied_brake > 0.9, "lifting preserves parking brake")
	key(main, KEY_LEFT, true); step(main, 45); key(main, KEY_LEFT, false); step(main, 3)
	check(native.get_forklift_state().tilt_angle_rad > before.tilt_angle_rad + 0.01, "left hydraulic axis tilts actual mast back")
	key(main, KEY_H, true); step(main, 1)
	check(main._forklift_audio._horn.playing, "held horn reaches actual spatial audio source")
	key(main, KEY_H, false); step(main, 1)
	check(not main._forklift_audio._horn.playing, "horn release is quiet")
	main._yaw += 0.5; step(main, 1)
	frame = main._forklift_chassis_frame()
	check(absf(wrapf(main._yaw - atan2(-frame.basis.z.x, -frame.basis.z.z), -PI, PI) - 0.5) < 0.01,
		"operator can freely look away from actual front")
	await capture("02-hydraulic-controls")
	main._yaw += PI - 0.5; step(main, 1)
	await capture("03-operator-rearward")
	key(main, KEY_ESCAPE, true); step(main, 1); key(main, KEY_ESCAPE, false)
	var paused_tick: int = native.get_tick_index()
	check(main._paused and paused, "Pause uses actual pause flow")
	await process_frame
	check(native.get_tick_index() == paused_tick, "paused native time remains frozen")
	main._resume(); step(main, 1)
	check(main._forklift_controls.gear == 0 and main._forklift_controls.parking and native.get_forklift_state().applied_brake > 0.9,
		"resume starts neutral and parked with held controls cleared")
	key(main, KEY_R, true); step(main, 1); key(main, KEY_R, false)
	key(main, KEY_W, true); step(main, 8)
	check(native.get_forklift_state().applied_drive < -0.99 and main._forklift_audio._reverse.playing,
		"reverse selects actual reverse drive and native-backed warning")
	key(main, KEY_E, true); step(main, 1); key(main, KEY_E, false)
	var left: Dictionary = native.get_forklift_state()
	check(main._operating == &"" and left.applied_drive == 0.0 and left.applied_brake > 0.9,
		"Done clears held accelerator and native brake applies")
	check(not main._router.forklift_mode, "leaving restores walking input")
	var render: Dictionary = native.get_forklift_render_state()
	check(main._forklift_view.visible and main._forklift_view._wheels.size() == render.wheels.size(), "native wheel visuals retained with cab")
	for i in render.wheels.size():
		check(main._forklift_view._wheels[i].global_transform.is_equal_approx(render.wheels[i].transform), "native render wheel " + str(i))
	print("FORKLIFT_OPERATOR_RECEIPT ", JSON.stringify({"support":native.get_support_entity_id(), "ready_reachable":ready.controls_reachable,
		"drive":driven.applied_drive,"steer_rad":driven.steering_rad,"lift_m":lifted.lift_position_m,
		"battery_before_j":before.battery_j,"battery_after_j":lifted.battery_j,"work_before_j":before.positive_work_j,"work_after_j":lifted.positive_work_j,
		"paused_tick":paused_tick,"leave_brake":left.applied_brake,"same_chassis_entity":left.chassis_entity,"captured":not capture_dir.is_empty()}))
	main.free()
	native = null
	print("SCRAPERX_FORKLIFT_OPERATOR_MAIN ", "FAIL" if failed else "PASS", " checks=",checks)
	quit(1 if failed else 0)
