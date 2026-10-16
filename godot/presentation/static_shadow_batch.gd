extends RefCounted

# Opaque static casters need their triangles and culling state, not their
# visible paint/normal-map materials. Share depth-only geometry by spatial
# cell while retaining all source triangles, visibility ranges and layers.
# Native collision and authored inspection nodes never enter this owner.
func build(host: Node3D, sources: Array[MeshInstance3D], cell_size: Vector3,
		grid_origin: Vector3, detail_parts: Array) -> Dictionary:
	var groups: Dictionary = {}
	for source in sources:
		var finish := source.get_active_material(0) as StandardMaterial3D
		if finish == null or source.cast_shadow != GeometryInstance3D.SHADOW_CASTING_SETTING_ON \
				or finish.fixed_size or finish.use_point_size \
				or finish.depth_draw_mode != BaseMaterial3D.DEPTH_DRAW_OPAQUE_ONLY:
			continue
		var local := host.global_transform.affine_inverse() * source.global_transform
		var grid := (local * source.mesh.get_aabb().get_center() - grid_origin) / cell_size
		var cell := Vector3i(floori(grid.x), floori(grid.y), floori(grid.z))
		var channels := 0
		var arrays := source.mesh.surface_get_arrays(0)
		for channel in arrays.size():
			if arrays[channel] != null:
				channels |= 1 << channel
		if arrays[Mesh.ARRAY_NORMAL] == null:
			continue # Preserve the original owner when a mesh supplies no normal data.
		var detail: bool = source.get_meta(&"part", "") in detail_parts
		var key: Array = [cell, source.layers, source.cast_shadow, finish.cull_mode,
			source.gi_mode, source.ignore_occlusion_culling,
			source.visibility_range_begin, source.visibility_range_end,
			source.visibility_range_begin_margin, source.visibility_range_end_margin,
			source.visibility_range_fade_mode, source.extra_cull_margin, channels, detail]
		if not groups.has(key):
			groups[key] = []
		groups[key].append([source, local])
	var handled: Dictionary = {}
	var detail_batches: Array[GeometryInstance3D] = []
	var count := 0
	for key in groups:
		var entries: Array = groups[key]
		# Singleton geometry has nothing to gain from a second render resource.
		if entries.size() < 2:
			continue
		var origin: Vector3 = grid_origin + Vector3(key[0]) * cell_size
		var material := StandardMaterial3D.new()
		material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		material.cull_mode = key[3]
		var surface := SurfaceTool.new()
		surface.begin(Mesh.PRIMITIVE_TRIANGLES)
		surface.set_material(material)
		for entry in entries:
			var local: Transform3D = entry[1]
			local.origin -= origin
			_append(surface, (entry[0] as MeshInstance3D).mesh, local)
		var mesh := surface.commit()
		if mesh == null:
			continue
		var first: MeshInstance3D = entries[0][0]
		var batch := MeshInstance3D.new()
		batch.name = "StaticShadowBatch%d" % count
		batch.mesh = mesh
		batch.position = origin
		batch.layers = first.layers
		batch.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
		batch.gi_mode = GeometryInstance3D.GI_MODE_DISABLED # Visible originals own GI; avoid a duplicate contributor.
		batch.ignore_occlusion_culling = first.ignore_occlusion_culling
		batch.visibility_range_begin = first.visibility_range_begin
		batch.visibility_range_end = first.visibility_range_end
		batch.visibility_range_begin_margin = first.visibility_range_begin_margin
		batch.visibility_range_end_margin = first.visibility_range_end_margin
		batch.visibility_range_fade_mode = first.visibility_range_fade_mode
		batch.extra_cull_margin = first.extra_cull_margin
		batch.set_meta(&"static_batch_sources", entries.size())
		host.add_child(batch)
		if key.back():
			detail_batches.append(batch)
		for entry in entries:
			handled[(entry[0] as MeshInstance3D).get_instance_id()] = true
		count += 1
	return {"handled": handled, "detail_batches": detail_batches, "batches": count}


func _append(surface: SurfaceTool, mesh: Mesh, local: Transform3D) -> void:
	if local.basis.is_conformal():
		surface.append_from(mesh, 0, local)
		return
	# SurfaceTool's append transform uses the basis for normals. Nonuniform
	# scale/shear needs inverse-transpose to retain the original shadow bias.
	# Bake only this uncommon case first, then append with an identity pose.
	var arrays := mesh.surface_get_arrays(0)
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX].duplicate()
	var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL].duplicate()
	var normal_basis := local.basis.inverse().transposed()
	for i in vertices.size():
		vertices[i] = local * vertices[i]
		normals[i] = (normal_basis * normals[i]).normalized()
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	if arrays[Mesh.ARRAY_TANGENT] != null:
		var tangents: PackedFloat32Array = arrays[Mesh.ARRAY_TANGENT].duplicate()
		for i in range(0, tangents.size(), 4):
			var tangent := (local.basis * Vector3(tangents[i], tangents[i+1], tangents[i+2])).normalized()
			tangents[i] = tangent.x
			tangents[i+1] = tangent.y
			tangents[i+2] = tangent.z
		arrays[Mesh.ARRAY_TANGENT] = tangents
	var baked := ArrayMesh.new()
	baked.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	surface.append_from(baked, 0, Transform3D.IDENTITY)
