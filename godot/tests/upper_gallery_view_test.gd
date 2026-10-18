extends Node

# This rendered check stages initial footing on each new deck. The separate
# native upper_gallery_route_tests proves the complete route without staging.
const Yard := preload("res://presentation/parkour_yard.gd")
const STEP := 1.0 / 90.0
var main: Node
var native: Object
var checks := 0
var failures := 0
var finished := false
var capture_dir := ""


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	if "--quit-after-upper-gallery-check" in OS.get_cmdline_user_args():
		_run.call_deferred()
	else:
		var start := Button.new()
		start.name = "RunUpperGalleryCheck"
		start.text = "Run ScraperX upper-gallery view check"
		start.position = Vector2(24, 80)
		start.pressed.connect(func() -> void:
			start.disabled = true
			_run.call_deferred())
		add_child(start)


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_UPPER_GALLERY_VIEW FAIL " + why)


func step(count: int) -> void:
	var before: int = native.get_tick_index()
	for _i in count:
		main._process(STEP)
	check(native.get_tick_index() == before + count, "exact native input ticks")
	check(native.get_death_count() == 0, "no hidden death/recovery")


func capture(label: String) -> void:
	if capture_dir.is_empty():
		return
	check(DisplayServer.get_name() != "headless", "capture uses a real renderer")
	if DisplayServer.get_name() == "headless":
		return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	var tick: int = native.get_tick_index()
	await RenderingServer.frame_post_draw
	var picture := get_viewport().get_texture().get_image()
	check(picture != null and not picture.is_empty(), "actual viewport: " + label)
	if picture != null and not picture.is_empty():
		check(picture.save_png(capture_dir.path_join(label + ".png")) == OK, "save " + label)
	check(native.get_tick_index() == tick, "rendering preserves input time")


func _create_main() -> void:
	get_tree().root.size = Vector2i(1280, 720)
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene, "shipping Main and native normal world")


func _run() -> void:
	_create_main()
	if native == null:
		_finish()
		return
	for index in Yard.UPPER_GALLERY_PERCHES.size():
		var top: Vector3 = Yard.UPPER_GALLERY_PERCHES[index] + Yard.YARD_OFFSET
		main._router.clear_held()
		main._yaw = -0.55 if index == 1 else 0.55
		main._pitch = -0.18
		check(native.debug_restart_at(top + Vector3.UP * 0.95), "stage initial perch footing")
		step(45)
		var position: Vector3 = native.get_player_position()
		check(native.is_player_grounded() and native.get_support_entity_id() == 51,
			"perch %d has actual exported world-solid support" % (index + 1))
		check(absf(position.y - top.y - 0.9) < 0.13 and
			Vector2(position.x - top.x, position.z - top.z).length() < 0.15,
			"perch %d collision agrees with its visible top" % (index + 1))
		await capture("0%d-perch-view" % (index + 1))
		if index == 0:
			var event := InputEventKey.new()
			event.physical_keycode = KEY_SPACE
			event.pressed = true
			get_viewport().push_input(event, true)
			event.pressed = false
			get_viewport().push_input(event, true)
			step(2)
			check(not native.is_player_grounded() and native.get_player_linear_velocity().y > 1.0,
				"real keyboard Jump reaches Main and leaves the perch")
			step(100)
			check(native.is_player_grounded() and absf(native.get_player_position().y - top.y - 0.9) < 0.13,
				"ordinary gravity returns Jump to the same real deck")
	await _wall_exit_checks()
	_finish()


func _run_wall_exit_only() -> void:
	_create_main()
	if native != null:
		await _wall_exit_checks()
	_finish()


func _wall_exit_checks() -> void:
	# Only initial footing is staged here. The native wall_exit_route_tests
	# separately reaches this receiver from arrival through normal inputs.
	var failures_before := failures
	var top: Vector3 = Yard.WALL_EXIT_TOP + Yard.YARD_OFFSET
	main._router.clear_held()
	main._yaw = PI * 0.9
	main._pitch = -0.4
	check(native.debug_restart_at(top + Vector3.UP * 0.95), "stage initial wall-exit deck footing")
	step(45)
	var position: Vector3 = native.get_player_position()
	check(native.is_player_grounded() and native.get_support_entity_id() == 51,
		"wall-exit deck has actual native world-solid support")
	check(absf(position.y - top.y - 0.9) < 0.13 and
		Vector2(position.x - top.x, position.z - top.z).length() < 0.15,
		"wall-exit collision agrees with the visible receiving surface")
	check(int(native.get_landing_state().get("support_sound_surface", 0)) == 2,
		"wall-exit deck exposes its actual steel contact")
	await capture("04-wall-exit-deck")
	var landings_before: int = native.get_landing_state().landing_count
	_key(KEY_SPACE, true)
	_key(KEY_SPACE, false)
	step(8)
	check(not native.is_player_grounded() and native.get_player_linear_velocity().y > 1.0,
		"keyboard Jump leaves the real receiving deck")
	check(int(native.get_landing_state().get("support_sound_surface", -1)) == 0,
		"airborne receiver departure has no remembered support")
	step(100)
	check(native.is_player_grounded() and native.get_support_entity_id() == 51 and
		absf(native.get_player_position().y - top.y - 0.9) < 0.13,
		"ordinary gravity lands the Jump back on the receiving deck")
	var landing: Dictionary = native.get_landing_state()
	check(int(landing.landing_count) == landings_before + 1 and
		int(landing.get("landing_sound_surface", 0)) == 2,
		"actual receiving-deck impact reaches the native landing feedback")
	main._yaw = -PI * 0.5
	main._pitch = -0.05
	_key(KEY_W, true)
	step(80)
	_key(KEY_W, false)
	# A real drop retains its landing momentum and recovery interval. Wait
	# for actual stable footing instead of assuming a fixed braking time.
	for _i in 180:
		if native.is_player_grounded() and native.get_player_linear_velocity().length() < 0.5:
			break
		step(1)
	position = native.get_player_position()
	check(native.is_player_grounded() and absf(position.y - 0.9) < 0.13 and
		position.x > top.x + 2.4 and native.get_player_linear_velocity().length() < 0.5,
		"ordinary controls leave the east deck edge for stable ground recovery")
	main._yaw = PI * 0.5
	step(1)
	await capture("05-wall-exit-ground-return")
	print("SCRAPERX_WALL_EXIT_VIEW %s staged_footing=1 public_route_proof=separate" % [
		"PASS" if failures == failures_before else "FAIL"])


func _key(code: int, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	get_viewport().push_input(event, true)


func _finish() -> void:
	finished = true
	print("SCRAPERX_UPPER_GALLERY_VIEW %s checks=%d failures=%d staged_footing=1" % [
		"PASS" if failures == 0 else "FAIL", checks, failures])
	if "--quit-after-upper-gallery-check" in OS.get_cmdline_user_args():
		get_tree().quit(1 if failures else 0)
