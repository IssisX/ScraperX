extends SceneTree

# Shipping main/resources + real native bridge. Only the initial airborne
# fixture uses debug_restart_at; rebound/escape use gravity and ordinary input.
# --render or --capture=<prefix> requires a real renderer. --catch-hour=8|20.
# --profile-catch draws continuously without a fixed-FPS cap; measurements
# include this controlled harness and are host evidence, never phone evidence.
const STEP := 1.0 / 90.0
const NET := 1963
const FRAME := 2963
var _main: Node3D
var _native: Object
var _render := false
var _profile := false
var _capture := ""
var _hour := 8
var _checks := 0
var _failed := false
var _rest := PackedVector3Array()
var _cpu_usec := 0
var _tick_samples := 0
var _wall_ms: Array[float] = []
var _draw_calls: Array[float] = []
var _primitives: Array[float] = []
var _previous_usec := 0
var _rendered_poses: Array[String] = []
var _canvas_uv := PackedVector2Array()

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, reason: String) -> bool:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_CATCH_NET FAIL " + reason)
	return ok

func _tick() -> bool:
	var start := Time.get_ticks_usec()
	_native.advance_frame(STEP)
	_cpu_usec += Time.get_ticks_usec() - start
	_tick_samples += 1
	return _check(is_finite(_native.get_player_position().y), "finite native player state")

func _exit_trace(label: String, tick: int, before: Vector3, before_velocity: Vector3) -> void:
	var load: Dictionary = _native.get_catch_net_state()
	var landing: Dictionary = _native.get_landing_state()
	print("SCRAPERX_CATCH_NET_EXIT_TRACE " + JSON.stringify({"phase": label, "exit_tick": tick,
		"pre_position": str(before), "pre_velocity": str(before_velocity),
		"post_position": str(_native.get_player_position()), "post_velocity": str(_native.get_player_linear_velocity()),
		"support": _native.get_support_entity_id(), "grounded": _native.is_player_grounded(), "deaths": _native.get_death_count(),
		"catch": load, "last_impact_mps": _native.get_last_impact_speed_mps(), "traversal": _native.get_traversal_state(),
		"landing_energy_j": landing.get("landing_approach_energy_j", -1),
		"landing_normal_mps": landing.get("landing_normal_speed_mps", -1),
		"landing_support": landing.get("landing_support_entity_id", -1)}))

func _draw() -> void:
	_main._render_snapshot(STEP)
	# Camera changes presentation only; this wide view sees the west stair,
	# finite frame and loaded cloth without moving the native player.
	_main._camera.global_position = Vector3(-48, 9, -140)
	_main._camera.look_at(Vector3(-34, 3.2, -150))
	await RenderingServer.frame_post_draw
	if _profile:
		var now := Time.get_ticks_usec()
		if _previous_usec != 0:
			_wall_ms.append(float(now - _previous_usec) / 1000.0)
		_previous_usec = now
		_draw_calls.append(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME))
		_primitives.append(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME))

func _pose(label: String) -> bool:
	if not _render:
		return true
	await _draw()
	await _draw()
	var kit := _main.get_node("KitPresentation")
	var net := kit.get_node_or_null("NativeSoftCatchNet")
	if not _check(net != null and net.is_visible_in_tree() and net.get_child_count() == 2,
			"catch has visible native canvas and tether batches at " + label):
		return false
	var canvas := net.get_node_or_null("Canvas") as MeshInstance3D
	if not _check(canvas != null and canvas.is_visible_in_tree() and canvas.mesh is ArrayMesh
			and canvas.mesh.get_surface_count() == 1 and canvas.transform.is_equal_approx(Transform3D.IDENTITY),
			"canvas renders one native surface without a presentation offset at " + label):
		return false
	var mesh := canvas.mesh as ArrayMesh
	var arrays := mesh.surface_get_arrays(0)
	var vertices: PackedVector3Array = _native.get_catch_net_vertices()
	var indices: PackedInt32Array = _native.get_catch_net_indices()
	var rendered: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var rendered_indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var uv: PackedVector2Array = arrays[Mesh.ARRAY_TEX_UV]
	var tangents: PackedFloat32Array = arrays[Mesh.ARRAY_TANGENT]
	if not _check(mesh.surface_get_primitive_type(0) == Mesh.PRIMITIVE_TRIANGLES
			and rendered.size() == vertices.size() and rendered_indices == indices
			and normals.size() == vertices.size() and uv.size() == vertices.size()
			and tangents.size() == vertices.size() * 4,
			"canvas arrays preserve every native indexed triangle at " + label):
		return false
	var expected_normals := PackedVector3Array()
	expected_normals.resize(vertices.size())
	for i in range(0, indices.size(), 3):
		var a := indices[i]
		var b := indices[i + 1]
		var c := indices[i + 2]
		if not _check(a >= 0 and a < vertices.size() and b >= 0 and b < vertices.size()
				and c >= 0 and c < vertices.size(), "valid native triangle indices at " + label):
			return false
		var face_normal := (vertices[c] - vertices[a]).cross(vertices[b] - vertices[a])
		if not _check(face_normal.is_finite() and face_normal.length_squared() > 0.000000000001,
				"native canvas triangle remains finite and nondegenerate at " + label):
			return false
		expected_normals[a] += face_normal
		expected_normals[b] += face_normal
		expected_normals[c] += face_normal
	var bounds := AABB(vertices[0], Vector3.ZERO)
	for i in vertices.size():
		bounds = bounds.expand(vertices[i])
		var tangent := Vector3(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2])
		if not _check(rendered[i].is_finite() and rendered[i].distance_to(vertices[i]) < 0.0002,
				"canvas vertex follows interpolated native surface at " + label) \
				or not _check(normals[i].is_finite() and absf(normals[i].length() - 1.0) < 0.0002
				and normals[i].distance_to(expected_normals[i].normalized()) < 0.0002,
				"canvas normal follows deformed native triangles at " + label) \
				or not _check(uv[i].is_finite() and tangent.is_finite()
				and absf(tangent.length() - 1.0) < 0.0002 and absf(tangent.dot(normals[i])) < 0.0002
				and absf(tangents[i * 4 + 3]) == 1.0,
				"canvas has finite material coordinates and orthonormal tangents at " + label):
			return false
	if _canvas_uv.is_empty():
		_canvas_uv = uv.duplicate()
	if not _check(uv == _canvas_uv, "canvas weave coordinates stay fixed through deformation at " + label) \
			or not _check(canvas.custom_aabb.position.distance_to(bounds.position) < 0.0003
			and canvas.custom_aabb.end.distance_to(bounds.end) < 0.0003
			and mesh.get_aabb().position.distance_to(bounds.position) < 0.0003
			and mesh.get_aabb().end.distance_to(bounds.end) < 0.0003,
			"canvas culling bounds follow the actual deformed surface at " + label):
		return false
	var material := canvas.material_override as StandardMaterial3D
	if not _check(material != null and material.cull_mode == BaseMaterial3D.CULL_DISABLED
			and material.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED
			and material.albedo_color.a == 1.0 and material.albedo_texture != null
			and material.normal_enabled and material.normal_texture != null,
			"canvas has an opaque woven material visible from either side at " + label):
		return false
	var endpoints: PackedVector3Array = _native.get_catch_net_tether_vertices()
	var tether := net.get_node_or_null("Tether") as MeshInstance3D
	if not _check(endpoints.size() == 2 and endpoints[0].is_finite() and endpoints[1].is_finite()
			and endpoints[0].distance_squared_to(endpoints[1]) >= 0.000001
			and tether != null and tether.is_visible_in_tree() and tether.mesh is CylinderMesh,
			"visible shockcord uses two finite native attachment endpoints at " + label):
		return false
	var casing := tether.mesh as CylinderMesh
	var from := tether.transform * Vector3(0.0, -0.5 * casing.height, 0.0)
	var to := tether.transform * Vector3(0.0, 0.5 * casing.height, 0.0)
	if not _check(from.distance_to(endpoints[0]) < 0.0002 and to.distance_to(endpoints[1]) < 0.0002
			and absf(tether.transform.basis.x.length() * casing.top_radius - 0.01) < 0.00001
			and absf(tether.transform.basis.z.length() * casing.bottom_radius - 0.01) < 0.00001
			and absf(tether.transform.basis.x.dot(tether.transform.basis.y)) < 0.00001
			and absf(tether.transform.basis.z.dot(tether.transform.basis.y)) < 0.00001,
			"20 mm shockcord casing connects actual native endpoints without offsets at " + label):
		return false
	var frame_index := int(_native.get_kit_body_index(FRAME))
	var frame := kit.get_node("KitBody2963") as Node3D
	if not _check(frame.transform.is_equal_approx(_native.get_kit_body_render_transform(frame_index)),
			"finite frame render pose follows native body"):
		return false
	if not _capture.is_empty():
		var path := "%s_%s.png" % [_capture, label]
		if not _check(DirAccess.make_dir_recursive_absolute(path.get_base_dir()) == OK,
				"capture directory"):
			return false
		if not _check(root.get_texture().get_image().save_png(path) == OK, "capture output " + path):
			return false
		print("SCRAPERX_CATCH_NET_SCREENSHOT " + path)
	print("SCRAPERX_CATCH_NET_POSE " + JSON.stringify({"pose": label, "hour": _hour,
		"player_position": str(_native.get_player_position()),
		"player_velocity": str(_native.get_player_linear_velocity()),
		"support": _native.get_support_entity_id(), "grounded": _native.is_player_grounded(),
		"catch": _native.get_catch_net_state(), "camera_position": str(_main._camera.global_position),
		"camera_transform": str(_main._camera.global_transform)}))
	_rendered_poses.append(label)
	return true

func _walk_grade_to(point: Vector2, deaths: int, tick_limit: int) -> bool:
	for i in tick_limit:
		var position: Vector3 = _native.get_player_position()
		var delta := point - Vector2(position.x, position.z)
		if delta.length() < 0.25:
			_native.set_move_input(0.0, 0.0)
			for settle in 90:
				if not _tick():
					return false
			return _check(int(_native.get_death_count()) == deaths and _native.is_player_grounded()
				and bool(_native.get_landing_state()["checkpoint_footing_valid"]), "onward waypoint has actual firm grade footing")
		var move := delta.normalized() * minf(1.0, delta.length() / 0.8)
		_native.set_facing(move.x, move.y)
		_native.set_move_input(move.x, move.y)
		if not _tick() or not _check(int(_native.get_death_count()) == deaths, "ordinary onward grade walk survives"):
			return false
	return _check(false, "ordinary onward grade waypoint timed out at " + str(_native.get_player_position()))

func _resource_bounds() -> bool:
	var kit := _main.get_node("KitPresentation")
	var submissions := 0
	for path in ["NativeSoftCatchNet", "KitBody2963", "KitBody1964"]:
		var owner := kit.get_node_or_null(path)
		if not _check(owner != null, "catch native presentation owner " + path):
			return false
		for child in owner.get_children():
			if child is MeshInstance3D and child.mesh != null:
				submissions += child.mesh.get_surface_count()
			elif child is MultiMeshInstance3D and child.multimesh != null and child.multimesh.instance_count > 0:
				submissions += child.multimesh.mesh.get_surface_count()
	var frame := kit.get_node("KitBody2963")
	var marks := frame.get_node_or_null("CatchNetTreadEdges") as MeshInstance3D
	if not _check(marks != null and marks.mesh != null
			and marks.mesh.get_surface_count() == 1
			and marks.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX].size() == 16 * 6,
			"all16 actual tread nosings share one paint batch"):
		return false
	var lamps := frame.get_node_or_null("CatchNetLampFaces") as MultiMeshInstance3D
	if not _check(lamps != null and lamps.multimesh != null and lamps.multimesh.instance_count == 2,
			"both mounted practical lamp faces share one batch"):
		return false
	var lights := 0
	for child in frame.get_children():
		if child is OmniLight3D:
			lights += 1
			if not _check(not child.shadow_enabled and child.omni_range <= 7.0
					and child.distance_fade_enabled and child.distance_fade_begin == 25.0
					and child.distance_fade_length == 10.0, "catch practical light bounds"):
				return false
	return _check(submissions <= 8 and lights == 2, "catch source draw budget <=8 and exactly2 practical lights")

func _stats(values: Array[float]) -> Dictionary:
	if values.is_empty():
		return {"samples": 0}
	var sorted := values.duplicate()
	sorted.sort()
	return {"samples": sorted.size(), "median": sorted[sorted.size() / 2],
		"p95": sorted[mini(sorted.size() - 1, int(ceil(sorted.size() * 0.95)) - 1)], "max": sorted.back()}

func _report_profile() -> void:
	if not _profile:
		return
	var canvas := _main.get_node("KitPresentation/NativeSoftCatchNet/Canvas") as MeshInstance3D
	var arrays := canvas.mesh.surface_get_arrays(0)
	# The renderer rebuilds this complete array payload when its native snapshot
	# changes. Driver packing/transfer and update frequency are not measured here.
	var array_bytes: int = arrays[Mesh.ARRAY_VERTEX].size() * 3 * 4 \
		+ arrays[Mesh.ARRAY_NORMAL].size() * 3 * 4 \
		+ arrays[Mesh.ARRAY_TANGENT].size() * 4 \
		+ arrays[Mesh.ARRAY_TEX_UV].size() * 2 * 4 \
		+ arrays[Mesh.ARRAY_INDEX].size() * 4
	var report := {"scope": "controlled native catch fall/rebound/escape harness; host, not phone",
		"renderer": RenderingServer.get_current_rendering_method(), "display_server": DisplayServer.get_name(),
		"adapter": RenderingServer.get_video_adapter_name(), "resolution": [root.size.x, root.size.y],
		"preset": "LOW", "hour": _hour, "capture_enabled": not _capture.is_empty(),
		"fps_cap": Engine.max_fps, "frame_wall_ms": _stats(_wall_ms), "draw_calls": _stats(_draw_calls),
		"primitives": _stats(_primitives), "native_advance_cpu_usec": _cpu_usec, "native_ticks_sampled": _tick_samples,
		"catch_mesh_vertices": arrays[Mesh.ARRAY_VERTEX].size(),
		"catch_mesh_triangles": arrays[Mesh.ARRAY_INDEX].size() / 3,
		"catch_mesh_array_bytes_per_rebuild": array_bytes,
		"upload_bytes_kind": "actual surface array payload, not measured GPU transfer"}
	print("SCRAPERX_CATCH_NET_PROFILE " + JSON.stringify(report))

func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg == "--render":
			_render = true
		elif arg == "--profile-catch":
			_profile = true
			_render = true
		elif arg.begins_with("--capture="):
			_capture = arg.trim_prefix("--capture=")
			_render = true
		elif arg.begins_with("--catch-hour="):
			var value := arg.trim_prefix("--catch-hour=")
			if not _check(value in ["8", "20"], "--catch-hour must be 8 or 20"):
				quit(1)
				return
			_hour = int(value)
	if not _check(not _render or DisplayServer.get_name() != "headless", "render/capture requires actual renderer"):
		quit(1)
		return
	if _profile:
		Engine.max_fps = 0
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	root.size = Vector2i(432, 371)
	root.content_scale_size = root.size
	root.disable_3d = not _render
	_main = (load("res://main.tscn") as PackedScene).instantiate()
	_main._settings_persistent = false
	root.add_child(_main)
	_main.set_process(false)
	_main._settings.apply_quality(0)
	_main._apply_settings()
	if _profile:
		# LOW may impose its own cap; profiling explicitly removes it afterward.
		Engine.max_fps = 0
		DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	_main._sky_cycle.day_minutes = 0.0
	_main._sky_cycle.set_hour(float(_hour))
	_native = _main._native
	if not _check(_native != null and _native.has_method("get_catch_net_state")
			and _native.has_method("get_catch_net_tether_vertices")
			and not _main._settings.persistent, "real shipping bridge and temporary LOW settings"):
		quit(1)
		return
	var state: Dictionary = _native.get_catch_net_state()
	for key in ["minimum_y_m", "sag_m", "total_specific_load_mps2", "peak_total_specific_load_mps2", "excess_load_dose_g_seconds", "approach_speed_mps"]:
		if not _check(state.has(key) and is_finite(float(state.get(key, INF))), "finite native SI telemetry " + key):
			quit(1)
			return
	var columns := int(state.get("columns", 0))
	var rows := int(state.get("rows", 0))
	if not _check(bool(state.get("installed", false)) and columns >= 2 and rows >= 2,
			"distinct installed catch membrane grid"):
		quit(1)
		return
	_rest = _native.get_catch_net_vertices()
	if not _check(_rest.size() == columns * rows
			and _native.get_catch_net_indices().size() == (columns - 1) * (rows - 1) * 6,
			"actual continuous catch membrane topology") or not _check(_native.get_cargo_net_vertices().size() == 828
			and _native.get_cargo_net_indices().size() == 3534, "original cargo topology retained"):
		quit(1)
		return
	if not _check(_native.get_kit_body_index(FRAME) >= 0 and _native.get_kit_body_index(1964) >= 0,
			"real finite frame and grounded foundations"):
		quit(1)
		return
	if not _resource_bounds():
		quit(1)
		return
	var deaths := int(_native.get_death_count())
	if not _check(_native.debug_restart_at(Vector3(-34, 36, -150)), "clear initial airborne fall fixture"):
		quit(1)
		return
	_native.set_move_input(0.0, 0.0)
	if not await _pose("catch_entry"):
		quit(1)
		return
	var contact := false
	var bounced := false
	var separated := false
	var recontact := false
	var settled_ticks := 0
	var loaded_capture := false
	var max_sag := 0.0
	var peak_upward := 0.0
	for i in 3600:
		if not _tick() or not _check(int(_native.get_death_count()) == deaths, "neutral fall survives actual compliant contact"):
			quit(1)
			return
		state = _native.get_catch_net_state()
		var support := int(_native.get_support_entity_id())
		var vy := float(_native.get_player_linear_velocity().y)
		max_sag = maxf(max_sag, float(state["sag_m"]))
		if support == NET:
			contact = true
			if separated:
				recontact = true
			if not _check(not bool(_native.get_landing_state()["checkpoint_footing_valid"]), "cloth never becomes firm checkpoint footing"):
				quit(1)
				return
		if contact:
			peak_upward = maxf(peak_upward, vy)
		if not loaded_capture and max_sag > 0.25 and contact:
			loaded_capture = true
			if not await _pose("catch_loaded"):
				quit(1)
				return
		if contact and vy > 2.0:
			bounced = true
		if bounced and not separated and not _native.is_player_grounded() and support != NET:
			separated = true
			if not await _pose("catch_rebound"):
				quit(1)
				return
		if recontact and support == NET and _native.is_player_grounded() and absf(vy - _native.get_support_point_linear_velocity().y) < 0.35:
			settled_ticks += 1
		else:
			settled_ticks = 0
		if settled_ticks >= 90:
			break
		if i % 180 == 0:
			print("SCRAPERX_CATCH_NET_TRACE " + JSON.stringify({"tick": i, "position": str(_native.get_player_position()),
				"vy": vy, "support": support, "sag_m": state["sag_m"], "contact_serial": state["contact_serial"]}))
		if _profile:
			await _draw()
	if not _check(contact and bounced and separated and recontact and settled_ticks >= 90 and max_sag > 0.25,
			"actual fall deforms, rebounds above2m/s, separates, recontacts and settles"):
		quit(1)
		return
	if not await _pose("catch_settled"):
		quit(1)
		return
	var saw_frame := false
	var grade := false
	var jumped := false
	for i in 1350:
		var position: Vector3 = _native.get_player_position()
		var before_velocity: Vector3 = _native.get_player_linear_velocity()
		_native.set_facing(-1.0, 0.0)
		# Careful ordinary analog footwork on the loaded cloth; full-speed
		# rim approach remains a separately preserved rejecting diagnostic.
		_native.set_move_input(-0.4, clampf((-150.0 - position.z) * 0.5, -0.3, 0.3))
		# Ordinary Jump helps clear the real rim; ordinary Climb may catch it.
		if not jumped and position.x < -38.5 and int(_native.get_support_entity_id()) == NET and _native.is_player_grounded():
			jumped = true
			_native.request_jump()
		if position.x < -39.0 and position.x > -41.0 and i % 30 == 0:
			_native.request_traversal()
		var tick_ok := _tick()
		if i % 15 == 0 or int(_native.get_death_count()) != deaths:
			_exit_trace("careful_analog_exit", i, position, before_velocity)
		if not tick_ok or not _check(int(_native.get_death_count()) == deaths, "ordinary frame/stair exit survives"):
			quit(1)
			return
		if int(_native.get_support_entity_id()) == FRAME:
			saw_frame = true
		position = _native.get_player_position()
		if position.x < -46.7 and position.y < 1.4 and _native.is_player_grounded() and int(_native.get_support_entity_id()) != NET:
			grade = true
			break
		if _profile:
			await _draw()
	_native.set_move_input(0.0, 0.0)
	if not _check(saw_frame and grade, "ordinary input reaches real frame, descends stairs and reaches grade"):
		print("SCRAPERX_CATCH_NET_ESCAPE position=%s support=%d frame=%s" % [_native.get_player_position(), _native.get_support_entity_id(), saw_frame])
		quit(1)
		return
	for i in 180:
		if not _tick():
			quit(1)
			return
	if not _check(bool(_native.get_landing_state()["checkpoint_footing_valid"]), "grade restores firm checkpoint footing"):
		quit(1)
		return
	if not await _pose("catch_exit"):
		quit(1)
		return
	# Continue around the tower front on the existing real grade, without
	# fixture relocation or added bypass geometry, to the retained cargo entry.
	# The front crossing clears the real buttress feet spanning z[-114,-108].
	for waypoint in [Vector2(-48.0, -104.0), Vector2(20.0, -104.0), Vector2(20.0, -114.0)]:
		if not await _walk_grade_to(waypoint, deaths, 2250):
			quit(1)
			return
	if not _check(float(_native.get_slingshot_state()["work_j"]) == 0.0,
			"catch exit reaches retained cargo approach without launcher work"):
		quit(1)
		return
	print("SCRAPERX_CATCH_NET_ONWARD ordinary_grade_walk=1 cargo_approach=" + str(_native.get_player_position()))
	# One outside-footprint miss checks bridge death/restart, leaving the
	# consequential rim/underside/spill/overload variants to the native suite.
	if not _check(_native.debug_restart_at(Vector3(-50, 36, -154)), "clear outside-footprint miss fixture"):
		quit(1)
		return
	# Debug initial-state placement saves the current physical retry state.
	# Compare restoration with that exact checkpoint, not the older startup mesh.
	_rest = _native.get_catch_net_vertices()
	for i in 900:
		if not _tick():
			quit(1)
			return
		if int(_native.get_death_count()) > deaths:
			break
	if not _check(int(_native.get_death_count()) == deaths + 1, "actual miss dies on rigid grade") \
			or not _check(_native.restart_checkpoint(), "ordinary restart accepted"):
		quit(1)
		return
	var restored: PackedVector3Array = _native.get_catch_net_vertices()
	var restore_error := 0.0
	if restored.size() == _rest.size():
		for i in restored.size():
			restore_error = maxf(restore_error, restored[i].distance_to(_rest[i]))
	else:
		restore_error = INF
	state = _native.get_catch_net_state()
	if not _check(restore_error < 0.001 and not bool(state["current_contact"]) and int(_native.get_support_entity_id()) != NET,
			"restart restores native catch/interpolation and clears support episode") \
			or not _check(_native.get_cargo_net_vertices().size() == 828 and _native.get_cargo_net_indices().size() == 3534,
				"restart retains original cargo topology"):
		quit(1)
		return
	if _failed:
		quit(1)
		return
	_report_profile()
	print("SCRAPERX_CATCH_NET PASS checks=%d max_sag_m=%.6f peak_upward_mps=%.6f frame_exit=%s grade=%s rendered_poses=%s" % [
		_checks, max_sag, peak_upward, saw_frame, grade, str(_rendered_poses)])
	quit(0)
