extends SceneTree

# Diagnostic cameras only. No machine state is forced; no route/device proof.
const STEP := 1.0 / 90.0
const ENTITIES := [1978, 1979, 1902, 1903, 1904, 1905]
var _main: Node3D
var _native: Object
var _capture := ""
var _attachment_only := false
var _worksite_only := false
var _failed := false
var _checks := 0

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, message: String) -> bool:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_DRAWBRIDGE_CAPTURE FAIL " + message)
	return ok

func _view(label: String, camera: Vector3, target: Vector3) -> void:
	for frame in 2:
		_main._render_snapshot(STEP)
		if _worksite_only:
			_main._inspection_worksite_life.update_from_native(_native, STEP, false, 19.25)
		_main._camera.global_position = camera
		_main._camera.fov = 65.0
		_main._camera.look_at(target)
		await RenderingServer.frame_post_draw
	var poses := {}
	var kit := _main.get_node("KitPresentation")
	for entity in ENTITIES:
		var body := int(_native.get_kit_body_index(entity))
		var node := kit.get_node_or_null("KitBody%d" % entity) as Node3D
		if not _check(body >= 0 and node != null, "native body and view %d" % entity):
			continue
		var pose: Transform3D = _native.get_kit_body_render_transform(body)
		_check(pose.origin.is_finite() and pose.basis.is_finite(), "finite pose %d" % entity)
		_check(node.transform.is_equal_approx(pose), "view follows native %d" % entity)
		var meshes := 0
		for child in node.get_children():
			if child is MeshInstance3D and child.mesh != null and child.is_visible_in_tree():
				meshes += 1
		_check(meshes > 0, "visible native geometry %d" % entity)
		poses[str(entity)] = str(pose)
	var image := root.get_texture().get_image()
	_check(image != null and image.get_width() == 960 and image.get_height() == 640, "actual color capture dimensions")
	var min_luma := 1.0
	var max_luma := 0.0
	for y in range(0, image.get_height(), 16):
		for x in range(0, image.get_width(), 16):
			var color := image.get_pixel(x, y)
			_check(is_finite(color.r) and is_finite(color.g) and is_finite(color.b), "finite rendered color")
			var luma := color.get_luminance()
			min_luma = minf(min_luma, luma)
			max_luma = maxf(max_luma, luma)
	_check(max_luma - min_luma > 0.1, "capture has visible color range")
	_check(image.save_png(_capture + "_" + label + ".png") == OK, "saved " + label)
	print("SCRAPERX_DRAWBRIDGE_VIEW " + JSON.stringify({"view": label, "native_poses": poses, "camera": str(camera), "target": str(target), "diagnostic_camera_only": true, "route_proof": false, "color_range": [min_luma, max_luma]}))

func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--worksite-only":
			_worksite_only = true
		if arg == "--attachment-only":
			_attachment_only = true
		if arg.begins_with("--capture="):
			_capture = arg.trim_prefix("--capture=")
	if not _check(DisplayServer.get_name() != "headless" and not _capture.is_empty(), "actual renderer and output path"):
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
	_check(_main.get_node_or_null("InspectionBalanceFeedback") != null, "shipping feedback helper exists")
	var span := int(_native.get_kit_body_index(1978))
	var parts: PackedFloat32Array = _native.get_kit_body_parts(span)
	_check(parts.size() / 13 == 58, "native span has 58 real parts")
	_check(_native.set_move_input(0.0, 0.0), "neutral ordinary input")
	_check(_native.debug_restart_at(Vector3(-19.3, 474.3, -129.65)), "diagnostic observer staging")
	for tick in 90:
		_native.advance_frame(STEP)
	if _worksite_only:
		_main._sky_cycle.set_hour(19.25)
		_main._process(STEP)
		var life = _main.get_node_or_null("InspectionWorksiteLife")
		_check(life != null and life._lights.size() == 2 and life._cloth.size() == 2, "shipping Main created worksite helper")
		life.update_from_native(_native, STEP, false, 19.25)
		for light in life._lights:
			_check(light.is_visible_in_tree() and light.spot_range == 6.0 and not light.shadow_enabled and light.light_energy > 1.0, "dusk warm task light visible and bounded")
		await _view("lower_dusk_wind_a", Vector3(-17.8, 473.3, -138.0), Vector3(-20.4, 471.8, -135.4))
		var before: PackedVector3Array = life._cloth[0].mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
		for tick in 90:
			life.update_from_native(_native, STEP, false, 19.25)
		var after: PackedVector3Array = life._cloth[0].mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
		_check(before[0].is_equal_approx(after[0]) and before[1].is_equal_approx(after[1]), "telltale root stays pinned")
		_check(not before[-1].is_equal_approx(after[-1]), "actual telltale vertices move over one presentation second")
		await _view("lower_dusk_wind_b", Vector3(-17.8, 473.3, -138.0), Vector3(-20.4, 471.8, -135.4))
		await _view("upper_dusk_handoff", Vector3(13.3, 486.6, -138.6), Vector3(9.8, 484.4, -135.0))
	elif _attachment_only:
		await _view("rear_lug_cable_guide", Vector3(-17.8, 473.3, -137.0), Vector3(-19.35, 472.5, -135.25))
	else:
		await _view("raised_overview", Vector3(-31, 489, -155), Vector3(-2, 480, -134))
		await _view("counterweight_bearing_cable", Vector3(20, 487, -142), Vector3(11.5, 483.5, -134.15))
		await _view("hatch_cable_seat", Vector3(-25, 477, -142), Vector3(-18.5, 473, -134.2))
		await _view("beveled_nose_seat", Vector3(-20.7, 475.4, -137.0), Vector3(-17.5, 473.6, -134.15))
	print("SCRAPERX_DRAWBRIDGE_CAPTURE %s checks=%d diagnostic_only=1 route_proof=0 phone_evidence=0" % ["FAIL" if _failed else "PASS", _checks])
	quit(1 if _failed else 0)
