extends SceneTree

# Real shipping scene and native bridge, with an actual renderer. The stored
# numeric baseline describes rigid meshes/materials/signs before extraction.
# Floating channels allow 0.1 mm/1e-4 rounding differences across ARM/x86;
# channel presence, topology, materials and signs still have to match. Native
# snapshots independently check moving poses, cable endpoints and net motion.
var _main: Node3D
var _failures: Array[String] = []
var _checks := 0
var _regression_fixtures := false


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, reason: String) -> void:
	_checks += 1
	if not ok and not _failures.has(reason):
		_failures.append(reason)
		push_error("SCRAPERX_KIT_VIEW FAIL " + reason)


func _material_record(material: Material) -> Array:
	if material is StandardMaterial3D:
		var m := material as StandardMaterial3D
		return [m.albedo_color.to_html(true), m.metallic, m.roughness,
			m.cull_mode, m.transparency, m.normal_enabled]
	return []


func _channel_record(channel: Variant) -> Variant:
	if channel == null:
		return null
	var values: Array = []
	for value in channel:
		match typeof(value):
			TYPE_VECTOR2:
				values.append([value.x, value.y])
			TYPE_VECTOR3:
				values.append([value.x, value.y, value.z])
			TYPE_VECTOR4:
				values.append([value.x, value.y, value.z, value.w])
			TYPE_COLOR:
				values.append([value.r, value.g, value.b, value.a])
			_:
				values.append(value)
	return [typeof(channel), values]


func _first_difference(actual: Variant, expected: Variant, path: String = "rigid") -> String:
	if (actual is float or actual is int) and (expected is float or expected is int):
		if is_finite(float(actual)) and is_finite(float(expected)) \
				and absf(float(actual) - float(expected)) <= 0.0001:
			return ""
	elif actual is Array and expected is Array:
		if actual.size() != expected.size():
			return "%s length actual=%d expected=%d" % [path, actual.size(), expected.size()]
		for i in actual.size():
			var difference := _first_difference(actual[i], expected[i], "%s/%d" % [path, i])
			if not difference.is_empty():
				return difference
		return ""
	elif actual == expected:
		return ""
	return "%s actual=%s expected=%s" % [path, str(actual), str(expected)]


func _rigid_record(kit: Node3D) -> Array:
	var result: Array = []
	for body in kit.get_children():
		if not String(body.name).begins_with("KitBody"):
			continue
		var children: Array = []
		for child in body.get_children():
			if child is MeshInstance3D:
				var mesh := (child as MeshInstance3D).mesh
				var surfaces: Array = []
				for surface in mesh.get_surface_count():
					# Compare original channels numerically, rather than hashing
					# platform-dependent serialized floating-point bits.
					var channels: Array = []
					for channel in mesh.surface_get_arrays(surface):
						channels.append(_channel_record(channel))
					surfaces.append([channels, _material_record(mesh.surface_get_material(surface))])
				children.append(["mesh", surfaces])
			elif child is Label3D:
				var label := child as Label3D
				var frame := label.transform
				var pose := [frame.basis.x.x, frame.basis.x.y, frame.basis.x.z,
					frame.basis.y.x, frame.basis.y.y, frame.basis.y.z,
					frame.basis.z.x, frame.basis.z.y, frame.basis.z.z,
					frame.origin.x, frame.origin.y, frame.origin.z]
				children.append(["sign", label.text, pose, label.font_size,
					label.pixel_size, label.modulate.to_html(true), label.outline_size,
					label.double_sided])
		result.append([String(body.name), children])
	return result


func _sample(kit: Node3D, native: Object) -> void:
	_check(kit.transform == Transform3D.IDENTITY, "kit root transform")
	for body in int(native.get_kit_body_count()):
		var node := kit.get_node_or_null("KitBody%d" % int(native.get_kit_body_entity_id(body))) as Node3D
		_check(node != null, "native body missing %d" % body)
		if node == null:
			continue
		if bool(native.is_kit_body_dynamic(body)):
			var enabled := bool(native.is_kit_body_enabled(body))
			_check(node.visible == enabled and node.is_visible_in_tree() == enabled, "dynamic/tree visibility %d" % body)
			if enabled:
				if int(native.get_kit_body_entity_id(body)) == 2900:
					# SlingshotView replaces this rigid proxy with native leather.
					var view := _main._slingshot_view as Node3D
					var leather := view.get_node_or_null("LeatherCradle/NativeLeatherSurface") as MeshInstance3D if view != null else null
					_check(leather != null and leather.mesh != null and leather.is_visible_in_tree(),
						"enabled pouch has visible native leather")
					if leather != null and leather.mesh != null:
						_check(leather.mesh.get_surface_count() == 1 and leather.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX].size() == 864,
							"pouch draws complete native leather surface")
						var state: Dictionary = native.get_slingshot_render_state()
						_check(state.has("pouch_position") and (leather.get_parent() as Node3D).global_position.distance_to(state["pouch_position"]) < 0.0001,
							"leather pouch follows native render position")
				else:
					var drawable_meshes := node.find_children("*", "MeshInstance3D", true, false)
					var has_drawable_mesh := false
					for mesh_node in drawable_meshes:
						if (mesh_node as MeshInstance3D).mesh != null and mesh_node.is_visible_in_tree():
							has_drawable_mesh = true
					_check(has_drawable_mesh, "enabled body has a visible mesh %d" % body)
				var expected: Transform3D = native.get_kit_body_render_transform(body)
				_check(node.transform.is_equal_approx(expected), "interpolated body pose %d" % body)
	for cable in int(native.get_kit_cable_count()):
		var points: PackedVector3Array = native.get_kit_cable_render_points(cable)
		for segment in 4:
			var node := kit.get_node_or_null("KitCable%d_%d" % [cable, segment]) as MeshInstance3D
			_check(node != null, "cable segment missing")
			if node == null:
				continue
			var used := segment + 1 < points.size()
			if used:
				used = points[segment].distance_to(points[segment + 1]) >= 0.001
			_check(node.visible == used and node.is_visible_in_tree() == used, "cable/tree visibility")
			_check(not used or node.mesh != null, "visible cable has a mesh")
			if used:
				_check((node.transform * Vector3(0, 0, -0.5)).distance_to(points[segment]) < 0.0001,
					"cable start follows native")
				_check((node.transform * Vector3(0, 0, 0.5)).distance_to(points[segment + 1]) < 0.0001,
					"cable end follows native")
	var vertices: PackedVector3Array = native.get_cargo_net_vertices()
	var indices: PackedInt32Array = native.get_cargo_net_indices()
	var net := kit.get_node_or_null("NativeSoftCargoNet") as Node3D
	if vertices.is_empty():
		_check(_regression_fixtures, "normal world native cargo net is empty")
		_check(indices.is_empty(), "empty native net retains triangle indices")
		_check(net == null or not net.visible, "empty native net display is hidden")
		return
	_check(vertices.size() == 828, "native net has all 207 four-corner knots")
	_check(indices.size() == 3534, "native net has complete woven faces")
	_check(net != null and net.visible and net.is_visible_in_tree(), "nonempty native net display is visible in tree")
	if net == null:
		return
	_check(net.global_transform == Transform3D.IDENTITY, "net world vertex frame")
	var strands := net.get_node_or_null("Strands") as MultiMeshInstance3D
	var knots := net.get_node_or_null("Knots") as MultiMeshInstance3D
	_check(strands != null and knots != null and strands.is_visible_in_tree() and knots.is_visible_in_tree(),
		"rounded net batches missing or hidden in tree")
	if strands == null or knots == null:
		return
	_check(strands.multimesh != null and knots.multimesh != null, "net multimesh resources exist")
	if strands.multimesh == null or knots.multimesh == null:
		return
	_check(strands.multimesh.mesh != null and knots.multimesh.mesh != null, "net source meshes exist")
	if strands.multimesh.mesh == null or knots.multimesh.mesh == null:
		return
	_check(knots.multimesh.instance_count == 207 and strands.multimesh.instance_count == 382,
		"open native weave display")
	_check(knots.multimesh.custom_aabb.size.is_finite() and strands.multimesh.custom_aabb.size.is_finite(),
		"net batch bounds are finite")
	for i in knots.multimesh.instance_count:
		var p := i * 4
		var centre := Vector3.ZERO
		for corner in 4:
			centre += vertices[p + corner] * 0.25
		var right := (vertices[p + 1] + vertices[p + 3] - vertices[p] - vertices[p + 2]) * 0.25
		var up := (vertices[p + 2] + vertices[p + 3] - vertices[p] - vertices[p + 1]) * 0.25
		var normal := right.cross(up)
		var pose := knots.multimesh.get_instance_transform(i)
		_check(pose.is_finite() and pose.origin.distance_to(centre) < 0.0001,
			"net knot follows interpolated native mean")
		if right.length_squared() > 0.000000000001 and up.length_squared() > 0.000000000001 \
			and normal.length_squared() > 0.000000000001:
			var expected_x := right * sqrt(2.0)
			var expected_y := up * sqrt(2.0)
			var expected_z := normal.normalized() * 0.035
			_check(pose.basis.x.distance_to(expected_x) < 0.0001
				and pose.basis.y.distance_to(expected_y) < 0.0001
				and pose.basis.z.distance_to(expected_z) < 0.0001,
				"net knot orientation and profile follow native corners")
		else:
			_check(pose.basis.x.length_squared() < 0.000000000001
				and pose.basis.y.length_squared() < 0.000000000001
				and pose.basis.z.length_squared() < 0.000000000001,
				"degenerate native knot frame is suppressed")
		var knot_bounds := pose * knots.multimesh.mesh.get_aabb()
		_check(knots.multimesh.custom_aabb.encloses(knot_bounds),
			"net knot batch bounds contain transformed geometry")
		for corner in 4:
			_check(knot_bounds.grow(0.0001).has_point(vertices[p + corner]),
				"rounded knot bounds cover native corner")
	for i in strands.multimesh.instance_count:
		var pose := strands.multimesh.get_instance_transform(i)
		_check(pose.is_finite() and absf(pose.basis.z.length() * 2.0 - 0.07) < 0.0001,
			"net rounded world depth 70 mm")
		_check(absf(pose.basis.x.length() * 2.0 - 0.08) < 0.01,
			"unloaded net retains native ribbon width")
		_check(strands.multimesh.custom_aabb.encloses(pose * strands.multimesh.mesh.get_aabb()),
			"net rope bounds contain transformed geometry")
	var tick := int(native.get_tick_index())
	kit.render_view()
	_check(int(native.get_tick_index()) == tick and native.get_cargo_net_vertices() == vertices,
		"net presentation is read-only")


func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("SCRAPERX_KIT_VIEW FAIL actual renderer required")
		quit(1)
		return
	root.size = Vector2i(192, 164)
	root.content_scale_size = root.size
	_main = (load("res://main.tscn") as PackedScene).instantiate()
	root.add_child(_main)
	# Rendering quality is a test cost setting, not an exported project edit.
	_main._settings.apply_quality(0)
	_main._apply_settings()
	await process_frame
	var kit := _main.get_node("KitPresentation") as Node3D
	var record := _rigid_record(kit)
	var fixture := "--regression-fixtures" in OS.get_cmdline_user_args()
	_regression_fixtures = fixture
	var baseline := "res://tests/fixtures/kit-fixture-numeric.json.gz" if fixture else "res://tests/fixtures/kit-normal-numeric.json.gz"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--kit-record="):
			var file := FileAccess.open(arg.trim_prefix("--kit-record="), FileAccess.WRITE)
			file.store_string(JSON.stringify(record) + "\n")
		if arg.begins_with("--kit-baseline="):
			baseline = arg.trim_prefix("--kit-baseline=")
	var baseline_bytes := FileAccess.get_file_as_bytes(baseline)
	if baseline.ends_with(".gz"):
		baseline_bytes = baseline_bytes.decompress_dynamic(32 * 1024 * 1024, FileAccess.COMPRESSION_GZIP)
	var expected: Variant = JSON.parse_string(baseline_bytes.get_string_from_utf8())
	_check(expected is Array, "pre-extraction numeric baseline loads")
	if expected is Array:
		if not fixture:
			# Preserve the entire original oracle for every unaffected body. The
			# explicit cargo migration is derived from that oracle, never captured
			# from the current renderer and blessed as its own expectation.
			var actual_original: Array = []
			var expected_original: Array = []
			var actual_cargo: Array = []
			for body in record:
				if body[0] in ["KitBody1952", "KitBody2952", "KitBody2954"]:
					actual_cargo.append(body)
				else:
					actual_original.append(body)
			for body in expected:
				if body[0] != "KitBody1952":
					expected_original.append(body)
			var cargo_bytes := FileAccess.get_file_as_bytes("res://tests/fixtures/kit-causal-cargo-numeric.json.gz")
			var expected_cargo: Variant = JSON.parse_string(cargo_bytes.decompress_dynamic(
				32 * 1024 * 1024, FileAccess.COMPRESSION_GZIP).get_string_from_utf8())
			_check(expected_cargo is Array, "derived cargo migration oracle loads")
			var cargo_difference := _first_difference(actual_cargo, expected_cargo, "cargo")
			_check(cargo_difference.is_empty(), "explicit causal cargo mesh migration " + cargo_difference)
			record = actual_original
			expected = expected_original
		var difference := _first_difference(record, expected)
		_check(difference.is_empty(), "rigid meshes/materials/signs equal pre-extraction baseline " + difference)
	var script: Script = kit.get_script()
	_check(script != null and script.resource_path == "res://presentation/kit_view.gd",
		"KitPresentation owns extracted rendering")
	for frame in 12:
		await process_frame
		# Main has updated native and rendered this snapshot by this point.
		await RenderingServer.frame_post_draw
		_sample(kit, _main._native)
	print("SCRAPERX_KIT_VIEW %s checks=%d rigid_bodies=%d renderer=%s" % [
		"PASS" if _failures.is_empty() else "FAIL", _checks,
		int(_main._native.get_kit_body_count()), DisplayServer.get_name()])
	quit(0 if _failures.is_empty() else 1)
