extends Node

# Shipping Main/input/native body, viewed from the player's normal pitch limit.
const STEP := 1.0 / 90.0
var main: Node
var native: Object
var capture_dir := ""
var baseline_only := false
var inspect_first := false
var checks := 0
var failures := 0
var finished := false


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	if "--quit-after-body-view-check" in OS.get_cmdline_user_args():
		_run.call_deferred()
	else:
		var start := Button.new()
		start.name = "RunBodyViewCheck"
		start.text = "Run ScraperX first-person body view"
		start.position = Vector2(24, 80)
		start.pressed.connect(func() -> void:
			start.disabled = true
			_run.call_deferred())
		add_child(start)


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_BODY_VIEW FAIL " + why)


func key(code: Key, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	get_viewport().push_input(event, true)


func step(count: int) -> void:
	var before: int = native.get_tick_index()
	for _i in count:
		main._process(STEP)
	check(native.get_tick_index() == before + count, "exact native input time")
	check(native.get_death_count() == 0, "no hidden death or restart")
	check(main._legs.visible, "normal first-person legs visible")
	for side in 2:
		var hip: Vector3 = main._legs._body.joint_world(side, "hp")
		var knee: Vector3 = main._legs._body.joint_world(side, "kn")
		var ankle: Vector3 = main._legs._body.joint_world(side, "an")
		check(hip.is_finite() and knee.is_finite() and ankle.is_finite()
			and absf(hip.distance_to(knee) - main._legs.THIGH) < 0.0001
			and absf(knee.distance_to(ankle) - main._legs.SHIN) < 0.0001,
			"both skinned leg chains retain their anatomical reach")
	if main._legs.planted_foot_index() >= 0:
		var state: Dictionary = native.get_landing_state()
		check(bool(state.foot_push_active) and
			main._legs.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.0001,
			"drawn planted sole stays on actual native material point")


func capture(label: String) -> void:
	if capture_dir.is_empty():
		return
	check(DisplayServer.get_name() != "headless", "actual rendered body view")
	if DisplayServer.get_name() == "headless":
		return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	var tick: int = native.get_tick_index()
	await RenderingServer.frame_post_draw
	var picture := get_viewport().get_texture().get_image()
	check(picture != null and not picture.is_empty(), "viewport image: " + label)
	if picture != null and not picture.is_empty():
		check(picture.save_png(capture_dir.path_join(label + ".png")) == OK, "save " + label)
	check(native.get_tick_index() == tick, "capture preserves native time")


func _run() -> void:
	var start := get_node_or_null("RunBodyViewCheck")
	if start != null:
		start.hide()
	get_tree().root.size = Vector2i(1280, 900)
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	main._pitch = -1.25
	native = main._native
	check(native != null and not main._regression_scene, "shipping Main and real native world")
	if native == null:
		_finish()
		return
	step(60)
	check(native.is_player_grounded(), "ordinary freight arrival footing")
	await capture("01-standing")
	if baseline_only:
		_finish()
		return
	if inspect_first:
		print("SCRAPERX_BODY_VIEW STANDING_READY")
		return
	await _movement()


func _movement() -> void:
	check(main._legs._body.ready_to_pose() and main._legs._body.pelvis_world().is_finite(),
		"weighted workwear joins both thighs at the hips")
	for mesh in main._legs._body._meshes:
		check(mesh.mesh is ArrayMesh and mesh.skin != null and mesh.skeleton != NodePath(),
			"clothing, boots and shadow stay bound to the real posed skeleton")
	# Use real player input in the clear eastward arrival aisle.
	main._yaw = -PI * 0.5
	key(KEY_W, true)
	step(45)
	check(native.get_player_linear_velocity().x > 1.0, "keyboard walking reaches native movement")
	await capture("02-walking")
	key(KEY_W, false)
	step(60)
	key(KEY_CTRL, true)
	step(24)
	check(native.is_player_crouched(), "held Crouch reaches the real capsule")
	await capture("03-crouching")
	key(KEY_CTRL, false)
	step(30)
	check(not native.is_player_crouched(), "standing resumes after release")
	key(KEY_SPACE, true)
	key(KEY_SPACE, false)
	step(8)
	check(not native.is_player_grounded() and native.get_player_linear_velocity().y > 0.0,
		"ordinary Jump gives actual airborne body pose")
	await capture("04-airborne")
	step(100)
	check(native.is_player_grounded(), "actual landing restores footing")
	await capture("05-landed")
	_finish()


func _finish() -> void:
	finished = true
	print("SCRAPERX_BODY_VIEW %s checks=%d failures=%d baseline_only=%d" % [
		"BASELINE" if baseline_only else ("PASS" if failures == 0 else "FAIL"),
		checks, failures, int(baseline_only)])
	if "--quit-after-body-view-check" in OS.get_cmdline_user_args():
		get_tree().quit(1 if failures else 0)
