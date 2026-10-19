extends Node

# Initial grade footing and the separate beam-landing view are staged.
# Native public-route programs prove approach, placement and crossing.
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
	if "--quit-after-loose-gangway-check" in OS.get_cmdline_user_args():
		_run.call_deferred()
	else:
		var start := Button.new()
		start.name = "RunLooseGangwayCheck"
		start.text = "Run ScraperX loose gangway interaction"
		start.pressed.connect(func() -> void:
			start.disabled = true
			_run.call_deferred())
		add_child(start)


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_LOOSE_GANGWAY_VIEW FAIL " + why)


func step(count: int) -> void:
	var before: int = native.get_tick_index()
	for _i in count:
		main._process(STEP)
	check(native.get_tick_index() == before + count, "ordered native input ticks")
	check(native.get_death_count() == 0, "no hidden recovery")


func key(code: int, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	get_viewport().push_input(event, true)


func _run() -> void:
	get_tree().root.size = Vector2i(960, 540)
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene, "shipping Main/native normal world")
	if native == null:
		_finish()
		return
	main._router.clear_held()
	main._yaw = 0.0
	main._pitch = -0.65
	check(native.debug_restart_at(Vector3(-71, 0.9, -26.97)), "stage only initial ground footing")
	step(45)
	check(native.is_player_grounded() and native.get_support_entity_id() == 1, "real grade contact")
	check(native.get_carry_target_entity_id() == 2490 and
		main._ctx.action.id == &"pick_up" and main._ctx.action.detail == "LOOSE GANGWAY",
		"real native grip drives clear contextual Action")
	var board: Node3D = main._kit_view.get_node_or_null("KitBody2490")
	check(board != null, "native free board appears in the shipping view")
	key(KEY_E, true)
	key(KEY_E, false)
	step(20)
	check(native.get_carrying_entity_id() == 2490 and main._ctx.action.id == &"set_down",
		"keyboard Action reaches actual carry owner and exposes release")
	var index: int = native.get_kit_body_index(2490)
	var before: Transform3D = native.get_kit_body_transform(index)
	key(KEY_D, true)
	step(60)
	key(KEY_D, false)
	step(30)
	var after: Transform3D = native.get_kit_body_transform(index)
	check(native.get_carrying_entity_id() == 2490 and
		after.origin.distance_to(before.origin) > 0.3, "movement transports a real free body")
	if board != null:
		var shown: Transform3D = native.get_kit_body_render_transform(index)
		check(board.transform.is_equal_approx(shown), "visible board follows authoritative native pose")
	key(KEY_E, true)
	key(KEY_E, false)
	step(45)
	check(native.get_carrying_entity_id() == 0, "same contextual Action releases actual carry constraint")
	if not capture_dir.is_empty() and DisplayServer.get_name() != "headless":
		DirAccess.make_dir_recursive_absolute(capture_dir)
		await RenderingServer.frame_post_draw
		var picture := get_viewport().get_texture().get_image()
		check(picture != null and not picture.is_empty(), "actual rendered gangway view")
		if picture != null and not picture.is_empty():
			check(picture.save_png(capture_dir.path_join("loose-gangway.png")) == OK, "save actual view")
	main._router.clear_held()
	main._yaw = PI / 2.0
	main._pitch = -0.4
	check(native.debug_restart_at(Vector3(-89.5, 3.3, -44)), "stage separate beam-landing view")
	step(45)
	check(native.is_player_grounded() and native.get_support_entity_id() == 1491,
		"new beam landing supplies actual native footing")
	var landing: Node3D = main._kit_view.get_node_or_null("KitBody1491")
	check(landing != null, "receiving trestle appears in shipping view")
	if landing != null:
		var landing_index: int = native.get_kit_body_index(1491)
		check(landing.transform.is_equal_approx(native.get_kit_body_render_transform(landing_index)),
			"receiving trestle view follows native geometry")
	if not capture_dir.is_empty() and DisplayServer.get_name() != "headless":
		await RenderingServer.frame_post_draw
		var landing_picture := get_viewport().get_texture().get_image()
		check(landing_picture != null and not landing_picture.is_empty(), "actual rendered beam landing")
		if landing_picture != null and not landing_picture.is_empty():
			check(landing_picture.save_png(capture_dir.path_join("beam-landing.png")) == OK,
				"save beam landing view")
	_finish()


func _finish() -> void:
	finished = true
	print("SCRAPERX_LOOSE_GANGWAY_VIEW %s checks=%d failures=%d staged_initial_footing=1 staged_beam_landing=1" % [
		"PASS" if failures == 0 else "FAIL", checks, failures])
	if "--quit-after-loose-gangway-check" in OS.get_cmdline_user_args():
		get_tree().quit(1 if failures else 0)
