extends SceneTree

# Two presentation diagnostics in shipping Main/Slingshot. Initial debug poses
# stage observation only; this does not prove ordinary input route integration.
# Run with a real renderer; --capture=<absolute prefix>, --inspection-hour=8|20.
const STEP := 1.0 / 90.0
const ENTITIES := [1973, 1974, 1978, 1979, 1997]
var _main: Node3D
var _native: Object
var _capture := ""
var _hour := 8
var _failed := false
var _checks := 0

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, message: String) -> bool:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_SPLIT_INSPECTION_CAPTURE FAIL " + message)
	return ok

func _advance(count: int) -> void:
	for tick in count:
		_native.advance_frame(STEP)
	_check(_native.get_player_position().is_finite(), "finite native player after gravity ticks")

func _draw(camera: Vector3, target: Vector3) -> void:
	_main._render_snapshot(STEP)
	_main._camera.global_position = camera
	_main._camera.fov = 70.0
	_main._camera.look_at(target)
	await RenderingServer.frame_post_draw

func _view(label: String, camera: Vector3, target: Vector3) -> void:
	await _draw(camera, target)
	await _draw(camera, target)
	var kit := _main.get_node("KitPresentation")
	var used: Array[String] = []
	for entity in ENTITIES:
		var body := int(_native.get_kit_body_index(entity))
		var node := kit.get_node_or_null("KitBody%d" % entity) as Node3D
		if not _check(body >= 0 and node != null and node.is_visible_in_tree(), "visible native body %d" % entity):
			continue
		var pose: Transform3D = _native.get_kit_body_render_transform(body)
		_check(node.transform.is_equal_approx(pose), "interpolated native pose %d" % entity)
		var meshes := 0
		for child in node.get_children():
			if not child is MeshInstance3D:
				continue
			var mesh := child as MeshInstance3D
			_check(mesh.is_visible_in_tree() and mesh.mesh != null, "visible native part batch")
			if mesh.mesh == null:
				continue
			meshes += 1
			for surface in mesh.mesh.get_surface_count():
				var material := mesh.mesh.surface_get_material(surface) as StandardMaterial3D
				for key in ["split_frame", "split_walk", "split_footing"]:
					if material == kit._junction_materials.get(key) and not used.has(key):
						used.append(key)
		_check(meshes > 0, "native geometry batches %d" % entity)
	for key in ["split_frame", "split_walk", "split_footing"]:
		var material := kit._junction_materials.get(key) as StandardMaterial3D
		_check(used.has(key) and material != null and material.roughness > 0.0,
			"cached PBR finish rendered: " + key)
	var footing := kit._junction_materials.get("split_footing") as StandardMaterial3D
	_check(footing != null and footing.normal_enabled and footing.normal_texture != null,
		"landing plate has cached tread normal texture")
	if not _capture.is_empty():
		var path := "%s_%s.png" % [_capture, label]
		if _check(DirAccess.make_dir_recursive_absolute(path.get_base_dir()) == OK, "capture directory"):
			_check(root.get_texture().get_image().save_png(path) == OK, "capture output " + path)
	print("SCRAPERX_SPLIT_INSPECTION_CAPTURE_VIEW " + JSON.stringify({"view": label,
		"hour": _hour, "player": str(_native.get_player_position()),
		"support": _native.get_support_entity_id(), "grounded": _native.is_player_grounded(),
		"hatch_native_pose": str(_native.get_kit_body_transform(_native.get_kit_body_index(1979))),
		"finishes": used, "diagnostic_staging": true}))

func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="):
			_capture = arg.trim_prefix("--capture=")
		elif arg.begins_with("--inspection-hour="):
			var hour := arg.trim_prefix("--inspection-hour=")
			if not _check(hour in ["8", "20"], "inspection hour is 8 or 20"):
				quit(1)
				return
			_hour = int(hour)
	if not _check(DisplayServer.get_name() != "headless", "requires actual renderer"):
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
	_main._sky_cycle.set_hour(float(_hour))
	_native = _main._native
	if not _check(_native != null and _native.has_method("get_kit_body_render_transform"), "shipping native bridge"):
		quit(1)
		return
	_check(_native.set_move_input(0.0, 0.0), "neutral ordinary input accepted")
	_check(_native.debug_restart_at(Vector3(-19.3, 474.3, -129.65)), "diagnostic crosshead staging")
	_advance(90)
	_check(_native.is_player_grounded() and _native.get_support_entity_id() == 1973, "actual crosshead support")
	await _view("overview", Vector3(-31, 480, -145), Vector3(-4, 476, -133.7))
	var unloaded: Transform3D = _native.get_kit_body_transform(_native.get_kit_body_index(1979))
	_check(_native.debug_restart_at(Vector3(-19.3, 474.4, -132.7)), "diagnostic hatch staging")
	_advance(120)
	var loaded: Transform3D = _native.get_kit_body_transform(_native.get_kit_body_index(1979))
	_check(_native.is_player_grounded() and _native.get_support_entity_id() == 1979, "actual player supported by hatch")
	_check(not loaded.basis.is_equal_approx(unloaded.basis), "hatch responds to actual default 85kg native player load")
	await _view("loaded_hatch", Vector3(-23, 476, -129), Vector3(-19.3, 472.9, -133.4))
	print("SCRAPERX_SPLIT_INSPECTION_CAPTURE %s checks=%d diagnostic_only=1 phone_evidence=0" % ["FAIL" if _failed else "PASS", _checks])
	quit(1 if _failed else 0)
