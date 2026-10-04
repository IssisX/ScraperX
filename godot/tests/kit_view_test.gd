extends SceneTree

# Real shipping scene and native bridge, with an actual renderer. The stored
# baseline describes rigid meshes/materials/signs before extraction. Native
# snapshots independently check moving poses, cable endpoints and net motion.
var _main: Node3D
var _failures: Array[String] = []
var _checks := 0


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
					# Binary mesh channels catch omitted bores, UVs, batching and
					# part transforms without rebuilding expected geometry here.
					surfaces.append([var_to_bytes(mesh.surface_get_arrays(surface)).hex_encode().sha256_text(),
						_material_record(mesh.surface_get_material(surface))])
				children.append(["mesh", surfaces])
			elif child is Label3D:
				var label := child as Label3D
				children.append(["sign", label.text, str(label.transform), label.font_size,
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
			_check(node.visible == bool(native.is_kit_body_enabled(body)), "dynamic visibility %d" % body)
			if node.visible:
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
			_check(node.visible == used, "cable visibility")
			if used:
				_check((node.transform * Vector3(0, 0, -0.5)).distance_to(points[segment]) < 0.0001,
					"cable start follows native")
				_check((node.transform * Vector3(0, 0, 0.5)).distance_to(points[segment + 1]) < 0.0001,
					"cable end follows native")
	var vertices: PackedVector3Array = native.get_cargo_net_vertices()
	if not vertices.is_empty():
		var net := kit.get_node_or_null("NativeSoftCargoNet") as Node3D
		_check(net != null, "native net display missing")
		if net == null:
			return
		_check(net.global_transform == Transform3D.IDENTITY, "net world vertex frame")
		var strands := net.get_node_or_null("Strands") as MultiMeshInstance3D
		var knots := net.get_node_or_null("Knots") as MultiMeshInstance3D
		_check(strands != null and knots != null, "rounded net batches missing")
		if strands == null or knots == null:
			return
		_check(knots.multimesh.instance_count == 207 and strands.multimesh.instance_count == 382,
			"open native weave display")
		for i in knots.multimesh.instance_count:
			var centre := Vector3.ZERO
			for corner in 4:
				centre += vertices[i * 4 + corner] * 0.25
			_check(knots.multimesh.get_instance_transform(i).origin.distance_to(centre) < 0.0001,
				"net knot follows interpolated native mean")
		for i in strands.multimesh.instance_count:
			var pose := strands.multimesh.get_instance_transform(i)
			_check(pose.is_finite() and absf(pose.basis.z.length() * 2.0 - 0.07) < 0.0001,
				"net rounded world depth 70 mm")
			_check(absf(pose.basis.x.length() * 2.0 - 0.08) < 0.01,
				"unloaded net retains native ribbon width")
			_check(strands.multimesh.custom_aabb.encloses(pose * strands.multimesh.mesh.get_aabb()),
				"net rope bounds contain geometry")
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
	var record := JSON.stringify(_rigid_record(kit))
	var fixture := "--regression-fixtures" in OS.get_cmdline_user_args()
	var baseline := "res://tests/fixtures/kit-fixture.json" if fixture else "res://tests/fixtures/kit-normal.json"
	_check(record == FileAccess.get_file_as_string(baseline).strip_edges(),
		"rigid meshes/materials/signs equal pre-extraction baseline")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--kit-record="):
			var file := FileAccess.open(arg.trim_prefix("--kit-record="), FileAccess.WRITE)
			file.store_string(record + "\n")
		if arg.begins_with("--kit-baseline="):
			_check(record == FileAccess.get_file_as_string(arg.trim_prefix("--kit-baseline=")).strip_edges(),
				"rigid meshes/materials/signs equal pre-extraction baseline")
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
