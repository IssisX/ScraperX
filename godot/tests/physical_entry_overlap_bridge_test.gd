extends SceneTree

# Sole initial stage on real steel; ordinary input thereafter.
const STEP := 1.0 / 90.0
const Z := -128.2
var _main: Node3D
var _native: RefCounted
var _failed := false
var _checks := 0
var _capture := ""

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, message: String) -> bool:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_ENTRY_OVERLAP_BRIDGE FAIL " + message)
	return ok

func _wood() -> bool:
	var support := int(_native.get_support_entity_id())
	return _native.is_player_grounded() and support >= 2880 and support <= 2891

func _tick() -> Dictionary:
	_check(_native.advance_frame(STEP) == 1, "ordinary native tick")
	var s: Dictionary = _native.get_landing_state()
	_check(_native.get_player_position().is_finite() and float(s["player_gravity_factor"]) == 1.0
		and _native.get_death_count() == 0, "finite native player with gravity and zero deaths")
	if bool(s["foot_push_active"]):
		_check(int(s["foot_push_support_entity_id"]) >= 2880 and int(s["foot_push_support_entity_id"]) <= 2891,
			"active finite leg retains actual timber")
	_check(float(s["foot_push_command_work_bound_j"]) <= 1285.62501 and float(s["foot_push_stroke_m"]) <= 0.300001
		and float(s["foot_push_peak_load_n"]) <= 3500.01 and float(s["foot_push_elapsed_seconds"]) <= 0.25 + STEP,
		"existing finite leg limits")
	return s

func _walk() -> void:
	var effort := Vector2.ZERO
	for tick in 1350:
		var p: Vector3 = _native.get_player_position()
		var v: Vector3 = _native.get_player_linear_velocity()
		var d := Vector2(-5.85 - p.x, Z - p.z)
		if _wood() and absf(d.x) < 0.08 and absf(d.y) < 0.03 and Vector2(v.x, v.z).length() < 0.12:
			_check(_native.set_move_input(0.0, 0.0), "ordinary arrival neutral")
			return
		if d.length() > 0.75:
			effort = Vector2.ZERO
		else:
			effort += 0.4 * d * STEP
			effort /= maxf(1.0, effort.length() / 0.35)
		var input := 1.8 * d - 0.28 * Vector2(v.x, v.z) + effort
		input /= maxf(1.0, input.length())
		_check(_native.set_move_input(input.x, input.y), "ordinary bounded stick")
		_tick()
	_check(false, "ordinary steel-to-timber approach finishes")

func _rail_capture() -> void:
	var body := int(_native.get_kit_body_index(1935))
	var node := _main.get_node("KitPresentation/KitBody1935") as Node3D
	var parts: PackedFloat32Array = _native.get_kit_body_parts(body)
	var p := parts.size() - 13
	_check(p >= 52 and absf(parts[p] - 0.45) < 0.0001 and absf(parts[p + 1] - 0.025) < 0.0001
		and absf(parts[p + 2] - 0.025) < 0.0001 and absf(parts[p + 3] + 5.1) < 0.0001
		and absf(parts[p + 4] - 408.3) < 0.0001 and absf(parts[p + 5] + 128.645) < 0.0001,
		"real native50mm top rail reaches bridge geometry")
	var meshes := 0
	for child in node.get_children():
		if child is MeshInstance3D and child.mesh != null and child.is_visible_in_tree():
			meshes += 1
	_check(meshes > 0, "receiving steel has actual visible geometry batches")
	for frame in 2:
		_main._render_snapshot(STEP)
		_main._camera.global_position = _native.get_player_position() + Vector3(0.0, 0.65, 0.15)
		_main._camera.fov = 70.0
		_main._camera.look_at(Vector3(-5.1, 408.3, -128.645))
		await RenderingServer.frame_post_draw
	_check(node.transform.is_equal_approx(_native.get_kit_body_render_transform(body)), "KitView follows native receiving steel")
	var image := root.get_texture().get_image()
	_check(image != null and image.get_width() == 960 and image.get_height() == 640, "actual player-scale rendered image")
	_check(image.save_png(_capture) == OK, "saved receiving rail capture")
	print("SCRAPERX_ENTRY_RAIL_VIEW " + JSON.stringify({"camera": str(_main._camera.global_position), "native_steel_pose": str(node.transform),
		"player": str(_native.get_player_position()), "view_only_camera": true, "parts": parts.size() / 13}))

func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="):
			_capture = arg.trim_prefix("--capture=")
	if not _check(DisplayServer.get_name() != "headless" and not _capture.is_empty(), "actual renderer and capture path"):
		quit(1)
		return
	root.size = Vector2i(960, 640)
	root.content_scale_size = root.size
	_main = (load("res://main.tscn") as PackedScene).instantiate()
	_main._settings_persistent = false
	root.add_child(_main)
	_main.set_process(false)
	_main._settings.apply_quality(0)
	_main._apply_settings()
	_main._sky_cycle.day_minutes = 0.0
	_main._sky_cycle.set_hour(8.0)
	_native = _main._native
	_check(_native.debug_restart_at(Vector3(-8.8, 407.9, Z)), "sole supported steel stage")
	_check(_native.set_move_input(0.0, 0.0) and _native.set_crouch_input(false) and _native.set_facing(1.0, 0.0), "ordinary standing input")
	var state := {}
	for tick in 360:
		state = _tick()
	_check(_native.is_player_grounded() and _native.get_support_entity_id() == 1935 and bool(state["checkpoint_footing_valid"]), "real firm steel entry")
	_walk()
	for tick in 360:
		state = _tick()
	_check(_wood() and Vector2(_native.get_player_linear_velocity().x, _native.get_player_linear_velocity().z).length() < 0.15
		and int(state["plank_broken_joint_mask"]) == 0, "real85kg load on intact timber end")
	_check(_native.set_facing(-1.0, 0.0) and _native.request_jump(), "ordinary wood Jump facing away")
	var begun := _tick()
	_check(bool(begun["foot_push_active"]) and float(begun["foot_push_command_work_bound_j"]) > 0.0, "Jump starts finite material leg")
	_check(_native.set_facing(1.0, -1.0) and _native.request_traversal(), "following ordinary Action faces receiving rail")
	state = _tick()
	_check(int(state["traversal_hand_constraint_count"]) == 2 and bool(state["foot_push_active"]) and bool(state["hand_foot_overlap_active"])
		and int(state["hand_foot_overlap_solved_ticks"]) > int(begun["hand_foot_overlap_solved_ticks"]), "actual simultaneous hand and foot solved entry")
	_check(float(state["hand_foot_last_hand_load_n"]) > 0.0 and float(state["hand_foot_last_foot_load_n"]) > 0.0, "actual positive shared hand and foot loads")
	_check(int(state["foot_push_start_count"]) == int(begun["foot_push_start_count"])
		and int(state["foot_push_support_entity_id"]) == int(begun["foot_push_support_entity_id"]), "original leg row and timber retained without restart")
	print("SCRAPERX_ENTRY_SHARED " + JSON.stringify(state))
	await _rail_capture()
	_check(_native.set_move_input(0.0, 0.0), "neutral compatible catch")
	for tick in 30:
		state = _tick()
	_check(not bool(state["foot_push_active"]) and int(state["foot_push_start_count"]) == int(begun["foot_push_start_count"]), "original finite leg expires without refreshed push")
	_check(float(state["foot_push_elapsed_seconds"]) <= 0.25 + STEP and float(state["foot_push_stroke_m"]) <= 0.150001
		and float(state["foot_push_command_work_bound_j"]) <= 525.00001, "actual original jump retains quarter-second150mm525J bounds")
	print("SCRAPERX_ENTRY_OVERLAP_BRIDGE %s checks=%d tick=%d state=%s" % ["FAIL" if _failed else "PASS", _checks, _native.get_tick_index(), JSON.stringify(state)])
	quit(1 if _failed else 0)
