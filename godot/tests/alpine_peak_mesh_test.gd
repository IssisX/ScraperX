extends SceneTree

const Peak := preload("res://presentation/alpine_peak_mesh.gd")
const Builder := preload("res://presentation/alpine_world_builder.gd")
const Geometry := preload("res://presentation/world_geometry.gd")
const Export := preload("res://presentation/solid_export.gd")
const Batch := preload("res://presentation/static_world_batch.gd")
var _failures := 0


func _initialize() -> void:
	call_deferred("_run")


func _check(condition: bool, label: String) -> void:
	if not condition:
		_failures += 1
		push_error(label)


func _run() -> void:
	var vertex_count := 0
	var index_count := 0
	for variant in Peak.VARIANTS:
		var mesh := Peak.peak(variant)
		_check(mesh == Peak.peak(variant + Peak.VARIANTS), "variants reuse shared mesh resources")
		var arrays := mesh.surface_get_arrays(0)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		vertex_count += vertices.size()
		index_count += indices.size()
		var carved := 0
		for index in vertices.size():
			var p := vertices[index]
			var n := normals[index]
			_check(p.is_finite() and n.is_finite() and absf(n.length() - 1.0) < 0.002,
				"finite vertices and unit normals variant%d vertex%d" % [variant, index])
			_check(n.y > 0.0, "height-field normals face upward")
			# All12 native hull halfspaces, not just a circular envelope.
			var remaining := 1.0
			for face in Export.CYLINDER_SEGMENTS:
				var angle := TAU * (float(face) + 0.5) / float(Export.CYLINDER_SEGMENTS)
				var dot := Vector2(p.x, p.z).dot(Vector2(cos(angle), sin(angle)))
				remaining = minf(remaining, 1.0 - dot / cos(PI / float(Export.CYLINDER_SEGMENTS)))
			_check(p.y >= -0.00001 and p.y <= remaining + 0.00002, "carved vertices stay inside native cone hull")
			if remaining <= Peak.FOOT_TRUE + 0.00001:
				_check(absf(p.y - remaining) < 0.00002, "reachable lower polygonal faces match collision")
			if remaining - p.y > 0.07:
				carved += 1
		_check(carved > 150, "substantial relief instead of a smooth cone")
		for index in range(0, indices.size(), 3):
			var a := vertices[indices[index]]
			var b := vertices[indices[index + 1]]
			var c := vertices[indices[index + 2]]
			_check((c - a).cross(b - a).y > 0.0, "nondegenerate clockwise triangles")
			# For any point within12m of an original triangle, every vertex
			# lies inside the shader's full-restoration radius by the triangle
			# inequality. Bound the real generated cell edges, not a guessed grid.
			var originals: Array[Vector3] = []
			for point in [a, b, c]:
				var y := 1.0
				for face in Export.CYLINDER_SEGMENTS:
					var angle := TAU * (float(face) + 0.5) / float(Export.CYLINDER_SEGMENTS)
					y = minf(y, 1.0 - Vector2(point.x, point.z).dot(Vector2(cos(angle), sin(angle))) / cos(PI / float(Export.CYLINDER_SEGMENTS)))
				originals.append(Vector3(point.x, y, point.z))
			for edge in 3:
				_check(originals[edge].distance_to(originals[(edge + 1) % 3]) <= Peak.RESTORE_CELL_BOUND,
					"near-surface restoration covers every triangle vertex before interaction reach")

	var host := Node3D.new()
	root.add_child(host)
	Builder.new(Geometry.new(host)).build_backdrop()
	var before := Export.collect(host)
	_check(before.unknown.is_empty(), "all original parts retain export classification")
	var batches := 0
	var instances := 0
	var sources := 0
	var far_sources := 0
	var source_snapshot: Array = []
	var native_caps := 0
	for child in host.get_children():
		if child is MultiMeshInstance3D and child.has_meta(&"native_snow_cap_instances"):
			native_caps += child.multimesh.instance_count
			_check(child.multimesh.mesh.radial_segments == Export.CYLINDER_SEGMENTS, "near snow proxies use actual native polygon")
		if child is MultiMeshInstance3D and child.has_meta(&"alpine_peak_instances"):
			batches += 1
			instances += child.multimesh.instance_count
			_check(child.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF, "peaks do not add shadow passes")
			for index in child.multimesh.instance_count:
				var transform: Transform3D = child.multimesh.get_instance_transform(index)
				_check(child.custom_aabb.has_point(transform.origin + Vector3.UP * transform.basis.y.y), "culling bounds include the fully restored native summit")
				_check(transform.basis.x.z == 0.0 and transform.basis.z.x == 0.0, "native polygon orientation retained")
				_check(transform.basis.x.x > 0.0 and transform.basis.y.y > 0.0, "positive exact dimension transform")
		if child is MeshInstance3D and child.has_meta(&"alpine_render_replaced"):
			sources += 1
			_check(child.layers == 0 and child.visible and child.mesh is CylinderMesh, "original export primitive retained without duplicate rendering")
			source_snapshot.append([child, child.mesh, child.transform, child.get_meta(&"part")])
			if child.get_meta(&"part") == "MountainRidgeFar":
				far_sources += 1
				_check(child.mesh.bottom_radius == 630.0 and child.mesh.height == 900.0, "far source aspect .70 retained")
	_check(instances == 54 and batches == 4 and far_sources == 4, "54 original peaks use exactly four shared draws")
	_check(native_caps == sources - instances, "every protruding native snow cap has a near-contact proxy")
	_check(sources > 64, "original main peaks and snow cap solid sources remain")
	Batch.new().build_tower(host)
	_check(before == Export.collect(host), "static rendering representation preserves every exported collision entry")
	for entry in source_snapshot:
		var source: MeshInstance3D = entry[0]
		_check(source.mesh == entry[1] and source.transform == entry[2] and source.get_meta(&"part") == entry[3]
			and source.layers == 0, "batching preserves exact authored source identity")
	# Analytical memory is vertex position+normal+color and32-bit indices;
	# renderer storage/compression overhead remains runtime evidence.
	print("ALPINE_MESH variants=%d vertices=%d indices=%d triangles_per_peak=%d approximate_array_bytes=%d instances=%d draws=%d" % [
		Peak.VARIANTS, vertex_count, index_count, index_count / (3 * Peak.VARIANTS),
		vertex_count * 40 + index_count * 4, instances, batches])
	host.free()
	print("ALPINE_PEAK_MESH_TEST failures=%d" % _failures)
	quit(1 if _failures > 0 else 0)
