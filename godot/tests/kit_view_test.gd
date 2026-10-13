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
# One ArrayMesh upload quantizes unit normals: the captured ARM64 renderer
# changes valid authored normals by up to0.000115 in this scene. Keep vertex
# parity at0.1mm; allow0.0002 only for the normalized direction channel.
const HULL_NORMAL_TOLERANCE := 0.0002


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
			# New frame paint is checked by the catch presentation test; retain
			# independent native structural channels here.
			if String(body.name) == "KitBody2963" and String(child.name) == "CatchNetTreadEdges":
				continue
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


func _check_hull_converter(kit: Node3D) -> void:
	# An asymmetric tetrahedron makes a box proxy, mirrored winding or
	# smoothed facet normals fail independently of the shipping world's hulls.
	var triangles := PackedFloat32Array([
		0, 0, 0, 0, 3, 0, 2, 0, 0,
		0, 0, 0, 2, 0, 0, 0, 0, 4,
		0, 0, 0, 0, 0, 4, 0, 3, 0,
		2, 0, 0, 0, 3, 0, 0, 0, 4])
	var material := StandardMaterial3D.new()
	var mesh: ArrayMesh = kit._kit_hull(triangles, material, -1, -1)
	_check(mesh != null and mesh.get_surface_count() == 1, "hull converter creates one real triangle surface")
	if mesh == null or mesh.get_surface_count() != 1:
		return
	_check(mesh.surface_get_material(0) == material, "hull converter retains shared material")
	var arrays := mesh.surface_get_arrays(0)
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	_check(vertices.size() == 12 and normals.size() == 12, "hull converter preserves every cooked facet vertex")
	if vertices.size() != 12 or normals.size() != 12:
		return
	var centre := Vector3(0.5, 0.75, 1.0)
	for p in range(0, vertices.size(), 3):
		var a := vertices[p]
		var b := vertices[p + 1]
		var c := vertices[p + 2]
		var outward := (a - c).cross(b - c).normalized() * -1.0
		_check(normals[p].dot(outward) > 0.9999 and normals[p] == normals[p + 1] \
			and normals[p] == normals[p + 2], "hull facets have flat normals opposed to Godot clockwise winding")
		_check(normals[p].dot((a + b + c) / 3.0 - centre) > 0.0, "hull facet normals point outside asymmetric solid")
	_check(mesh.get_aabb().position == Vector3.ZERO and mesh.get_aabb().size == Vector3(2, 3, 4),
		"hull converter retains authoring-space bounds")


func _hull_vertex_cell(point: Vector3) -> Vector3i:
	return Vector3i(floori(point.x * 1000.0), floori(point.y * 1000.0), floori(point.z * 1000.0))


func _check_native_hulls(kit: Node3D, native: Object) -> void:
	# Compare actual batched render triangles with the native cooked hulls,
	# including part rotation/offset. Existing worlds need not contain hulls.
	for body in int(native.get_kit_body_count()):
		var parts: PackedFloat32Array = native.get_kit_body_parts(body)
		var hull_parts: Array[int] = []
		for p in range(0, parts.size() - 12, 13):
			if int(parts[p + 11]) == 3:
				hull_parts.append(p)
		if hull_parts.is_empty():
			continue
		_check(native.has_method("get_kit_body_part_mesh"), "native cooked hull mesh getter exists")
		if not native.has_method("get_kit_body_part_mesh"):
			continue
		var node := kit.get_node_or_null("KitBody%d" % int(native.get_kit_body_entity_id(body))) as Node3D
		_check(node != null, "native hull body has render owner")
		if node == null:
			continue
		var rendered: Dictionary = {}
		for child in node.get_children():
			if not child is MeshInstance3D:
				continue
			var instance := child as MeshInstance3D
			if instance.mesh == null:
				continue
			for surface in instance.mesh.get_surface_count():
				var arrays: Array = instance.mesh.surface_get_arrays(surface)
				var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
				var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
				var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX] if arrays[Mesh.ARRAY_INDEX] != null else PackedInt32Array()
				var count := indices.size() if not indices.is_empty() else vertices.size()
				for t in range(0, count - 2, 3):
					var triangle: Array = []
					for corner in 3:
						var i: int = indices[t + corner] if not indices.is_empty() else t + corner
						triangle.append(instance.transform * vertices[i])
						triangle.append((instance.transform.basis * normals[i]).normalized() if i < normals.size() else Vector3.ZERO)
					var cell := _hull_vertex_cell(triangle[0])
					if not rendered.has(cell):
						rendered[cell] = []
					rendered[cell].append(triangle)
		for p in hull_parts:
			var triangles: PackedFloat32Array = native.get_kit_body_part_mesh(body, p / 13)
			var valid := not triangles.is_empty() and triangles.size() % 9 == 0
			for value in triangles:
				valid = valid and is_finite(value)
			_check(valid, "native cooked hull has complete finite triangles body=%d part=%d" % [body, p / 13])
			if not valid:
				continue
			var local := Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
				Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			for t in range(0, triangles.size(), 9):
				var a := Vector3(triangles[t], triangles[t + 1], triangles[t + 2])
				var b := Vector3(triangles[t + 3], triangles[t + 4], triangles[t + 5])
				var c := Vector3(triangles[t + 6], triangles[t + 7], triangles[t + 8])
				var cross := (b - a).cross(c - a)
				_check(cross.is_finite() and cross.length_squared() > 0.000000000000000001,
					"native cooked hull facet is nondegenerate body=%d part=%d triangle=%d" % [body, p / 13, t / 9])
				if not cross.is_finite() or cross.length_squared() <= 0.000000000000000001:
					continue
				var normal := (local.basis * cross.normalized()).normalized()
				var expected: Array[Vector3] = [local * a, local * c, local * b]
				var cell := _hull_vertex_cell(expected[0])
				var found := false
				# Neighbouring cells tolerate ARM/x86 rounding at bin boundaries.
				for x in range(-1, 2):
					for y in range(-1, 2):
						for z in range(-1, 2):
							for candidate in rendered.get(cell + Vector3i(x, y, z), []):
								var matches := true
								for corner in 3:
									matches = matches and expected[corner].distance_to(candidate[corner * 2]) < 0.0001 \
										and normal.distance_to(candidate[corner * 2 + 1]) < HULL_NORMAL_TOLERANCE
								found = found or matches
				_check(found, "render matches cooked hull facet/pose/winding/normal body=%d part=%d triangle=%d" % [body, p / 13, t / 9])


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


# Native construction in simulation.cpp appends only these normal-world bodies:
# west brace, inspection frame/seats/recovery/hanger, twelve physical timber
# cells, north transfer and pipe service. 1962 is logical metadata, not a body.
const UPPER_BODIES := ["KitBody1934", "KitBody1935", "KitBody1936", "KitBody1937", "KitBody1938", "KitBody1939", "KitBody2880", "KitBody2881", "KitBody2882", "KitBody2883", "KitBody2884", "KitBody2885", "KitBody2886", "KitBody2887", "KitBody2888", "KitBody2889", "KitBody2890", "KitBody2891", "KitBody2935"]


func _check_upper_inventory(names: Array[String]) -> void:
	var sorted := names.duplicate()
	sorted.sort()
	_check(sorted == UPPER_BODIES, "all nineteen appended upper bodies render exactly once")


func _check_mesh_children(node: Node3D, context: String) -> void:
	# Batched vertices already contain each native part's local transform.
	# Numeric mesh records alone cannot reject a displaced or hidden draw node.
	for child in node.get_children():
		if child is MeshInstance3D:
			_check(child.transform.is_equal_approx(Transform3D.IDENTITY),
				context + " mesh child retains identity transform")
			_check(child.visible and child.is_visible_in_tree(),
				context + " mesh child visible in tree")


func _upper_finish(entity: int, index: int, half: Vector3, local: Transform3D) -> Array:
	# Independent presentation contract, keyed by the real native material
	# and physical function. Never call KitView's finish/mesh builders here.
	const PALETTE := {
		0: ["687174ff", 0.35, 0.78, false], 1: ["86472dff", 0.12, 0.92, false],
		2: ["4a3420ff", 0.02, 0.9, true], 3: ["4e4841ff", 0.0, 0.94, true],
		4: ["a04d16ff", 0.18, 0.76, false], 5: ["879394ff", 0.45, 0.68, false],
		6: ["5b544bff", 0.0, 0.98, true], 7: ["c19a2aff", 0.2, 0.62, false]}
	const FINISHES := {
		"frame": ["3f555cff", 0.12, 0.86, false], "oxide": ["89664bff", 0.0, 0.93, false],
		"footing": ["b2beb9ff", 1.0, 0.68, true], "recovery": ["7e948fff", 1.0, 0.80, true],
		"return": ["9caeaaff", 0.12, 0.81, false], "grip": ["b1bcb6ff", 0.0, 0.62, false],
		"bearing": ["829390ff", 1.0, 0.48, false], "warning": ["cba347ff", 0.0, 0.76, false],
		"transfer_frame": ["526572ff", 0.08, 0.87, false], "transfer_footing": ["c3c0adff", 0.12, 0.72, true],
		"service_frame": ["69746cff", 0.08, 0.83, false], "service_pipe": ["a17b59ff", 0.0, 0.88, false],
		"service_footing": ["afb6afff", 0.12, 0.72, true]}
	var key := ""
	if entity == 2935 or entity == 1937:
		key = "grip" if index == 0 else ("warning" if index == 7 else "bearing")
	elif entity == 1939:
		key = {0: "service_frame", 1: "service_pipe", 5: "service_footing", 7: "warning"}.get(index, "")
	elif entity == 1938:
		key = {0: "transfer_frame", 1: "oxide", 5: "transfer_footing", 7: "warning"}.get(index, "")
		if index in [0, 5] and half.x < 0.08 and half.y < 0.08:
			key = "grip"
		elif index == 0 and half.x > 1.5 and half.y < 0.17 and half.z > 0.6:
			key = "warning"
	elif entity in [1935, 1936]:
		key = {0: "frame", 1: "oxide", 5: "footing" if entity == 1935 else "recovery", 7: "warning"}.get(index, "")
		if index == 0 and entity == 1936 and half.x > 3.0 and local.basis.x.y > 0.3:
			key = "return"
		elif index == 0 and entity == 1935 and half.y < 0.025 and half.x < 0.07:
			key = "grip"
	var finish: Array = FINISHES[key] if not key.is_empty() else PALETTE.get(index, [])
	if finish.is_empty():
		return []
	return [finish[0], finish[1], finish[2], 0, 0, finish[3]]


func _upper_signs(entity: int) -> Array:
	# Complete stencil records specified from authored locations/text, rather
	# than accepting whatever labels the shipping renderer produces.
	var signs: Array = []
	match entity:
		1935:
			signs = [["INSPECTION\n407", Vector3(-22, 407.0015, -127.7), 0.007, "253c40ff"],
				["NORTH FRAME\n418", Vector3(0.2, 407.0015, -127), 0.0045, "253c40ff"]]
		1936:
			signs = [["INSPECTION RETURN\n403.5", Vector3(-16.5, 403.5015, -128.25), 0.006, "263e3eff"]]
		2935:
			signs = [["J-407", Vector3(0, -0.7, 0.2015), 0.002, "3d3526ff"]]
		1938:
			signs = [["SERVICE HEADER\nN-429", Vector3(-21.1, 429.0015, -128), 0.0055, "303d43ff"],
				["NORTH FRAME\n440", Vector3(-0.1, 429.0015, -128), 0.0045, "303d43ff"]]
		1939:
			signs = [["RETURN HEADER\nN-451", Vector3(-20.8, 451.0015, -130), 0.0055, "293b35ff"],
				["FRAME SERVICE\n462", Vector3(-0.1, 451.0015, -129.3), 0.0045, "293b35ff"]]
	var records: Array = []
	for sign in signs:
		var at: Vector3 = sign[1]
		var axes := [1, 0, 0, 0, 1, 0, 0, 0, 1] if entity == 2935 else [1, 0, 0, 0, 0, -1, 0, 1, 0]
		records.append(["sign", sign[0], axes + [at.x, at.y, at.z], 40, sign[2], sign[3], 0, true])
	return records


func _check_upper_body(kit: Node3D, body: Array) -> void:
	var name: String = body[0]
	var entity := int(name.trim_prefix("KitBody"))
	var native: Object = _main._native
	var index := int(native.get_kit_body_index(entity))
	_check(index >= 0 and int(native.get_entity_body_count(entity)) == 1,
		"upper body exists exactly once in native Kit " + name)
	if index < 0:
		return
	var dynamic := entity in [2935, 2963] or entity in range(2880, 2892)
	_check(bool(native.is_kit_body_dynamic(index)) == dynamic, "upper static/dynamic ownership " + name)
	var node := kit.get_node_or_null(name) as Node3D
	_check(node != null, "upper body has render owner " + name)
	if node == null:
		return
	_check(node.visible and node.is_visible_in_tree(), "upper body visible " + name)
	_check(node.transform.is_equal_approx(native.get_kit_body_render_transform(index)), "upper native render pose " + name)
	_check_mesh_children(node, name)
	var parts: PackedFloat32Array = native.get_kit_body_parts(index)
	_check(not parts.is_empty() and parts.size() % 13 == 0, "upper complete native parts " + name)
	# These specific constructors use boxes and solid cylinders only. Future
	# shapes need an explicit migration; never silently fall back to a box.
	var batches: Dictionary = {}
	for p in range(0, parts.size() - 12, 13):
		var half := Vector3(parts[p], parts[p + 1], parts[p + 2])
		var local := Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
			Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
		var material_index := int(parts[p + 10])
		var finish := _upper_finish(entity, material_index, half, local)
		_check(not finish.is_empty(), "upper native material is authorized " + name)
		var shape := int(parts[p + 11])
		_check(shape in [0, 1] and parts[p + 12] == 0.0, "upper native primitive is authorized " + name)
		if finish.is_empty() or shape not in [0, 1] or parts[p + 12] != 0.0:
			continue
		var primitive: PrimitiveMesh
		if shape == 0:
			var box := BoxMesh.new()
			box.size = half * 2.0
			primitive = box
		else:
			var cylinder := CylinderMesh.new()
			cylinder.top_radius = half.x
			cylinder.bottom_radius = half.x
			cylinder.height = half.y * 2.0
			primitive = cylinder
		var mesh: Mesh = primitive
		var timber := entity in range(2880, 2892)
		if timber:
			_check(material_index == 2 and shape == 0, "plank cells are real native timber boxes " + name)
			var arrays := primitive.surface_get_arrays(0)
			var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
			var uv := PackedVector2Array()
			var uv2 := PackedVector2Array()
			var cell := entity - 2880
			for v in vertices.size():
				uv.append(Vector2(vertices[v].z, vertices[v].x + (float(cell) + 0.5) * 0.2))
				var face := 0.0
				if normals[v].x < -0.5:
					face = -1.0 if cell == 0 else float(cell)
				elif normals[v].x > 0.5:
					face = -1.0 if cell == 11 else float(cell + 1)
				uv2.append(Vector2(vertices[v].y, face))
			arrays[Mesh.ARRAY_TEX_UV] = uv
			arrays[Mesh.ARRAY_TEX_UV2] = uv2
			var grain := ArrayMesh.new()
			grain.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
			mesh = grain
		var key := str(material_index) + "/" + str(finish)
		if not batches.has(key):
			var surface := SurfaceTool.new()
			surface.begin(Mesh.PRIMITIVE_TRIANGLES)
			batches[key] = [surface, [] if timber else finish]
		(batches[key][0] as SurfaceTool).append_from(mesh, 0, local)
	var expected: Array = []
	for key in batches:
		var mesh := (batches[key][0] as SurfaceTool).commit()
		var channels: Array = []
		for channel in mesh.surface_get_arrays(0):
			channels.append(_channel_record(channel))
		expected.append(["mesh", [[channels, batches[key][1]]]])
	expected.append_array(_upper_signs(entity))
	var difference := _first_difference(body[1], expected, name)
	_check(difference.is_empty(), "upper native mesh/material/sign migration " + difference)
	if entity in range(2880, 2892):
		for child in node.get_children():
			if child is MeshInstance3D:
				var material := (child as MeshInstance3D).mesh.surface_get_material(0) as ShaderMaterial
				_check(material != null and material.shader != null and material.shader.resource_path == "res://presentation/plank_wood.gdshader",
					"plank retains native timber shader " + name)
				if material != null:
					_check(material.get_shader_parameter("timber_light") == Color("b9a077") and material.get_shader_parameter("timber_dark") == Color("65503a"),
						"plank authored timber finish " + name)


func _migrate_north_frame(frozen: Array) -> Array:
	# Pinned source-before/source-after reconstruction must first account for
	# EVERY frozen channel. Keep all materials/signs and the unaffected deck
	# batch byte-for-byte; only the declared steel/rust/yellow channels change.
	var authored = preload("res://tests/fixtures/kit-north-frame-migration.gd")
	var before: Array = authored.channels(false)
	var after: Array = authored.channels(true)
	var migrated := frozen.duplicate(true)
	_check(frozen[0] == "KitBody1901" and frozen[1].size() == 4,
		"north migration has exactly four frozen material batches and no signs")
	if frozen[1].size() != 4:
		return migrated
	for batch in 4:
		var old_channels: Array = []
		var new_channels: Array = []
		for channel in before[batch]:
			old_channels.append(_channel_record(channel))
		for channel in after[batch]:
			new_channels.append(_channel_record(channel))
		var child: Array = frozen[1][batch]
		_check(child[0] == "mesh" and child[1].size() == 1,
			"north frozen single surface batch %d" % batch)
		var difference := _first_difference(old_channels, child[1][0][0], "north-before/%d" % batch)
		_check(difference.is_empty(), "north source-before matches immutable channels " + difference)
		if batch == 0:
			difference = _first_difference(new_channels, old_channels, "north-decks")
			_check(difference.is_empty(), "north galvanized deck channels remain unchanged " + difference)
		else:
			migrated[1][batch][1][0][0] = new_channels
	# Collision parity is additional evidence, independent of the appearance
	# oracle above. Native parts never supply the expected appearance channels.
	var native: Object = _main._native
	var index := int(native.get_kit_body_index(1901))
	_check(index >= 0 and int(native.get_entity_body_count(1901)) == 1,
		"north frame exists exactly once in native Kit")
	if index >= 0:
		var expected_parts: Array = []
		for part in authored.parts(true):
			var half: Vector3 = part[0]
			var at: Vector3 = part[1]
			expected_parts.append_array([half.x, half.y, half.z, at.x, at.y, at.z, 0, 0, 0, 1, part[2], 0, 0])
		var difference := _first_difference(Array(native.get_kit_body_parts(index)), expected_parts, "north-native")
		_check(difference.is_empty(), "north source-after collision parity " + difference)
		_check(not bool(native.is_kit_body_dynamic(index)), "north frame retains static ownership")
		var node := _main.get_node("KitPresentation/KitBody1901") as Node3D
		_check(node.visible and node.is_visible_in_tree(), "north frame visible")
		_check(node.transform.is_equal_approx(native.get_kit_body_render_transform(index)), "north frame native render pose")
		_check_mesh_children(node, "north frame")
	return migrated


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
			# explicit cargo/facade migrations are derived from that oracle, never captured
			# from the current renderer and blessed as its own expectation.
			var actual_original: Array = []
			var expected_original: Array = []
			var actual_cargo: Array = []
			var actual_facade: Array = []
			var added_lift: Array[String] = []
			var added_supplied: Array[String] = []
			var added_reclaim: Array[String] = []
			var added_cart: Array[String] = []
			var added_taper: Array[String] = []
			var added_upper: Array[String] = []
			var added_catch: Array[String] = []
			var expected_reclaim: Array[String] = []
			# The new wheel is an explicit append-only migration. Preserve the
			# old oracle and require every reserved body exactly once; do not
			# regenerate a baseline from the changed shipping scene.
			for entity in range(1940, 1946):
				expected_reclaim.append("KitBody%d" % entity)
			for entity in range(2200, 2303):
				expected_reclaim.append("KitBody%d" % entity)
			const SUPPLIED_BODIES := ["KitBody1920", "KitBody1921", "KitBody1922", "KitBody1930", "KitBody1931", "KitBody1980", "KitBody1981", "KitBody1982", "KitBody1983", "KitBody1984", "KitBody1985", "KitBody1986", "KitBody1987", "KitBody1988", "KitBody1989", "KitBody1990", "KitBody1991", "KitBody1992", "KitBody1993", "KitBody1994", "KitBody1995", "KitBody1996", "KitBody2920", "KitBody2921", "KitBody2922", "KitBody2930", "KitBody2980", "KitBody2981", "KitBody2982", "KitBody2983", "KitBody2984", "KitBody2985", "KitBody2986", "KitBody2987", "KitBody2988", "KitBody2989", "KitBody2990", "KitBody2991", "KitBody2992"]
			const LIFT_BODIES := ["KitBody1970", "KitBody1971", "KitBody1972", "KitBody2970", "KitBody2971", "KitBody2972", "KitBody2973", "KitBody2974", "KitBody2975", "KitBody2976", "KitBody2977", "KitBody2978"]
			const CART_BODIES := ["KitBody1954", "KitBody1955", "KitBody1956", "KitBody1957", "KitBody1958", "KitBody1959", "KitBody2320", "KitBody2321", "KitBody2322", "KitBody2323", "KitBody2324", "KitBody2325", "KitBody2326", "KitBody2327"]
			const TAPER_BODIES := ["KitBody1932", "KitBody1933"]
			for body in record:
				# Appended AS-027 bodies have no pre-extraction counterpart.
				# Keep every prior body in its original oracle; native pose/visibility
				# checks below also cover each new moving assembly.
				if body[0] in ["KitBody1964", "KitBody2963"]:
					added_catch.append(body[0])
					_check_upper_body(kit, body)
				elif body[0] in UPPER_BODIES:
					added_upper.append(body[0])
					_check_upper_body(kit, body)
				elif body[0] in CART_BODIES:
					added_cart.append(body[0])
					_check(not body[1].is_empty(), "cart assembly has native-derived draw geometry " + body[0])
					var entity := int(String(body[0]).trim_prefix("KitBody"))
					var native_body := int(_main._native.get_kit_body_index(entity))
					_check(native_body >= 0, "cart body exists in native Kit " + body[0])
					if native_body >= 0:
						_check(bool(_main._native.is_kit_body_dynamic(native_body)) == (entity >= 2320),
							"cart static/dynamic ownership " + body[0])
						_check(not _main._native.get_kit_body_parts(native_body).is_empty(),
							"cart body has real native parts " + body[0])
						var node := kit.get_node(body[0]) as Node3D
						_check(node.transform.is_equal_approx(_main._native.get_kit_body_render_transform(native_body)),
							"cart initial native render pose " + body[0])
				elif body[0] in TAPER_BODIES:
					# These two static route bodies append to the normal world only.
					# Keep all preceding geometry in its existing numeric oracle.
					added_taper.append(body[0])
					_check(not body[1].is_empty(), "taper body has native-derived draw geometry " + body[0])
					var entity := int(String(body[0]).trim_prefix("KitBody"))
					var native_body := int(_main._native.get_kit_body_index(entity))
					_check(native_body >= 0 and int(_main._native.get_entity_body_count(entity)) == 1,
						"taper body exists exactly once in native Kit " + body[0])
					if native_body >= 0:
						_check(not bool(_main._native.is_kit_body_dynamic(native_body)),
							"taper body retains static ownership " + body[0])
						_check(not _main._native.get_kit_body_parts(native_body).is_empty(),
							"taper body has real native parts " + body[0])
						var node := kit.get_node(body[0]) as Node3D
						_check(node.visible and node.is_visible_in_tree(), "taper body is visible " + body[0])
						_check(node.transform.is_equal_approx(_main._native.get_kit_body_render_transform(native_body)),
							"taper native render pose " + body[0])
				elif body[0] in expected_reclaim:
					added_reclaim.append(body[0])
					_check(not body[1].is_empty(), "reclaim assembly has native-derived draw geometry " + body[0])
				elif body[0] in SUPPLIED_BODIES:
					added_supplied.append(body[0])
					_check(not body[1].is_empty(), "supplied assembly has collision-derived draw geometry " + body[0])
				elif body[0] in LIFT_BODIES:
					added_lift.append(body[0])
					_check(not body[1].is_empty(), "lift assembly has drawable geometry " + body[0])
				elif body[0] in ["KitBody1952", "KitBody2952", "KitBody2954"]:
					actual_cargo.append(body)
				elif body[0] in ["KitBody1600", "KitBody2560", "KitBody2561", "KitBody2562", "KitBody2563", "KitBody2564", "KitBody2565", "KitBody2566"]:
					actual_facade.append(body)
				else:
					actual_original.append(body)
			_check_upper_inventory(added_upper)
			added_catch.sort()
			_check(added_catch == ["KitBody1964", "KitBody2963"], "both native catch foundations/frame render exactly once")
			added_lift.sort()
			_check(added_lift == LIFT_BODIES, "all twelve added lift assemblies render exactly once")
			added_supplied.sort()
			_check(added_supplied == SUPPLIED_BODIES, "all added supplied assemblies render exactly once")
			added_reclaim.sort()
			expected_reclaim.sort()
			_check(added_reclaim == expected_reclaim, "all109 gravity-reclaim bodies render exactly once")
			added_cart.sort()
			_check(added_cart == CART_BODIES, "all fourteen slab-haul cart bodies render exactly once")
			added_taper.sort()
			_check(added_taper == TAPER_BODIES, "both static taper bodies render exactly once")
			var north_oracles := 0
			for body in expected:
				if body[0] == "KitBody1901":
					north_oracles += 1
					expected_original.append(_migrate_north_frame(body))
				elif body[0] not in ["KitBody1952", "KitBody1600"]:
					expected_original.append(body)
			_check(north_oracles == 1, "exactly one immutable north frame oracle")
			var cargo_bytes := FileAccess.get_file_as_bytes("res://tests/fixtures/kit-causal-cargo-numeric.json.gz")
			var expected_cargo: Variant = JSON.parse_string(cargo_bytes.decompress_dynamic(
				32 * 1024 * 1024, FileAccess.COMPRESSION_GZIP).get_string_from_utf8())
			_check(expected_cargo is Array, "derived cargo migration oracle loads")
			var cargo_difference := _first_difference(actual_cargo, expected_cargo, "cargo")
			_check(cargo_difference.is_empty(), "explicit causal cargo mesh migration " + cargo_difference)
			var facade_bytes := FileAccess.get_file_as_bytes("res://tests/fixtures/kit-causal-facade-numeric.json.gz")
			var expected_facade: Variant = JSON.parse_string(facade_bytes.decompress_dynamic(
				32 * 1024 * 1024, FileAccess.COMPRESSION_GZIP).get_string_from_utf8())
			_check(expected_facade is Array, "derived facade partition oracle loads")
			# The two route-guidance labels are deliberate additions. Specify their
			# complete records independently; retain every original geometry channel.
			for body in expected_facade:
				if body[0] == "KitBody2560":
					body[1].push_front(["sign", "UP · JUMP TO DUCT\nCLIMB CABINET FIRST",
						[1, 0, 0, 0, 1, 0, 0, 0, 1, 20.0, 23.5, -121.76], 64,
						0.10 / 64.0, "e7c67cff", 12, true])
				elif body[0] == "KitBody2562":
					body[1].push_front(["sign", "UP · VENT TO NEXT DECK\nCLIMB · HOLD FORWARD",
						[1, 0, 0, 0, 1, 0, 0, 0, 1, 24.0, 28.2, -123.34], 64,
						0.10 / 64.0, "e7c67cff", 12, true])
			var facade_difference := _first_difference(actual_facade, expected_facade, "facade")
			_check(facade_difference.is_empty(), "explicit causal facade ownership partition " + facade_difference)
			record = actual_original
			expected = expected_original
		var difference := _first_difference(record, expected)
		_check(difference.is_empty(), "rigid meshes/materials/signs equal pre-extraction baseline " + difference)
	var script: Script = kit.get_script()
	_check(script != null and script.resource_path == "res://presentation/kit_view.gd",
		"KitPresentation owns extracted rendering")
	_check_hull_converter(kit)
	_check_native_hulls(kit, _main._native)
	for frame in 12:
		await process_frame
		# Main has updated native and rendered this snapshot by this point.
		await RenderingServer.frame_post_draw
		_sample(kit, _main._native)
	print("SCRAPERX_KIT_VIEW %s checks=%d rigid_bodies=%d renderer=%s" % [
		"PASS" if _failures.is_empty() else "FAIL", _checks,
		int(_main._native.get_kit_body_count()), DisplayServer.get_name()])
	quit(0 if _failures.is_empty() else 1)
