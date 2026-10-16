extends RefCounted

# Runtime rendering only. Keep authored primitive nodes/metadata for collision
# export and inspection; their render layers are replaced by spatial batches.
# Never cross a moving native body's ownership boundary.
# A cell spans one complete 52m tower cross-section plus its support frame,
# and only four 11m storeys vertically. Center it on the authored tower so
# identical corner members do not become four separate singleton batches.
const CELL_SIZE := Vector3(88.0, 44.0, 88.0)
const GRID_ORIGIN := Vector3(-44.0, 0.0, -194.0)
const DETAIL_PARTS := ["FaceBand", "FaceDuct", "FloorLight", "FloorLightHigh",
	"TimberCladding", "LaneStripe", "TreeTrunk", "TreeCanopy", "ScrubLobe"]
const SolidExport := preload("res://presentation/solid_export.gd")
const ShadowBatch := preload("res://presentation/static_shadow_batch.gd")
var _detail_batches: Array[GeometryInstance3D] = []


func build_tower(host: Node3D) -> Dictionary:
	var sources: Array[MeshInstance3D] = []
	for child in host.get_children():
		if child is MeshInstance3D:
			if child.has_meta(&"static_batch_sources"):
				continue
			var part := str(child.get_meta(&"part", child.name))
			if part not in SolidExport.MOVING:
				sources.append(child)
		elif child.get_meta(&"static_render_container", false) \
				or child.name in [&"ReceivingYard", &"WestApronParkour"] \
				or _is_static_foliage(child):
			# Repeated sibling names become @Node3D@... in Godot. Identify
			# foliage by authored part tags rather than generated parent names.
			for leaf in child.get_children():
				if leaf is MeshInstance3D:
					sources.append(leaf)
	return _build(host, sources)


func _is_static_foliage(node: Node) -> bool:
	if not node is Node3D or node.get_script() != null or node.get_child_count() == 0:
		return false
	for child in node.get_children():
		if not child is MeshInstance3D or child.get_meta(&"part", "") not in [
				"TreeTrunk", "TreeCanopy", "ScrubLobe"]:
			return false
	return true


func build_kit(host: Node3D, static_roots: Array[Node3D]) -> Dictionary:
	var sources: Array[MeshInstance3D] = []
	for body in static_roots:
		if not body.is_visible_in_tree():
			continue
		for child in body.get_children():
			# Do not enter live net/cable/pad/practical subtrees.
			if child is MeshInstance3D:
				sources.append(child)
	return _build(host, sources)


func apply_detail_distance(distance: float) -> void:
	for batch in _detail_batches:
		batch.visibility_range_end = distance
		batch.visibility_range_end_margin = 0.0
		batch.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_DISABLED


func _build(host: Node3D, sources: Array[MeshInstance3D]) -> Dictionary:
	var groups: Dictionary = {}
	var signatures: Dictionary = {}
	var foliage_finishes: Dictionary = {}
	var considered := 0
	var eligible: Array[MeshInstance3D] = []
	for source in sources:
		var mesh := source.mesh
		if mesh == null or mesh.get_surface_count() != 1 or source.layers == 0 \
				or not source.is_visible_in_tree() or source.material_overlay != null \
				or source.transparency != 0.0 or not source.visibility_parent.is_empty() \
				or source.get_blend_shape_count() != 0 or source.skin != null:
			continue
		var material := source.get_active_material(0)
		# Animated shaders, translucent ordering, local projection and
		# next-pass effects need their original object coordinates/identity.
		if not material is StandardMaterial3D:
			continue
		var standard := material as StandardMaterial3D
		if standard.transparency != BaseMaterial3D.TRANSPARENCY_DISABLED \
				or standard.billboard_mode != BaseMaterial3D.BILLBOARD_DISABLED \
				or standard.next_pass != null or standard.grow \
				or standard.proximity_fade_enabled or standard.distance_fade_mode != 0:
			continue
		if mesh is ArrayMesh and mesh.surface_get_primitive_type(0) != Mesh.PRIMITIVE_TRIANGLES:
			continue
		var local := host.global_transform.affine_inverse() * source.global_transform
		if local.basis.determinant() <= 0.0:
			continue
		var center := local * mesh.get_aabb().get_center()
		var grid := (center - GRID_ORIGIN) / CELL_SIZE
		var cell := Vector3i(floori(grid.x), floori(grid.y), floori(grid.z))
		var tint := Color.WHITE
		var colored := false
		var foliage := _foliage_primitive(source, standard)
		if not foliage.is_empty():
			colored = true
			mesh = foliage["mesh"]
			local.basis = local.basis * Basis.from_scale(foliage["scale"])
			tint = standard.albedo_color
			# Compatibility works in sRGB; the linear renderers need linear
			# instance color to match their converted albedo uniform.
			if RenderingServer.get_current_rendering_method() != "gl_compatibility":
				tint = tint.srgb_to_linear()
			var original_id := standard.get_instance_id()
			if not foliage_finishes.has(original_id):
				var finish := standard.duplicate() as StandardMaterial3D
				finish.albedo_color = Color.WHITE
				finish.vertex_color_use_as_albedo = true
				finish.vertex_color_is_srgb = false
				foliage_finishes[original_id] = finish
			material = foliage_finishes[original_id]
		var material_id := material.get_instance_id()
		if not signatures.has(material_id):
			signatures[material_id] = _material_signature(material)
		var channels := 0
		var arrays := mesh.surface_get_arrays(0)
		for channel in arrays.size():
			if arrays[channel] != null:
				channels |= 1 << channel
		var detail: bool = source.get_meta(&"part", "") in DETAIL_PARTS
		# Local triplanar projection must keep the original vertex space.
		# Instance identical geometry rather than baking its object transform.
		var instanced := colored or (standard.uv1_triplanar and not standard.uv1_world_triplanar) \
			or (standard.uv2_triplanar and not standard.uv2_world_triplanar)
		var key: Array = [cell, signatures[material_id], channels, source.layers,
			source.cast_shadow, source.gi_mode, source.ignore_occlusion_culling,
			source.visibility_range_begin, source.visibility_range_end,
			source.visibility_range_begin_margin, source.visibility_range_end_margin,
			source.visibility_range_fade_mode, source.extra_cull_margin, detail,
			arrays if instanced else [], colored]
		if not groups.has(key):
			groups[key] = []
		groups[key].append([source, local, mesh, material, tint])
		considered += 1
		eligible.append(source)
	var shadows := ShadowBatch.new().build(host, eligible, CELL_SIZE, GRID_ORIGIN, DETAIL_PARTS)
	_detail_batches.append_array(shadows.detail_batches)
	var replaced := 0
	var created := 0
	for key in groups:
		var entries: Array = groups[key]
		if entries.size() < 2:
			continue
		var first: MeshInstance3D = entries[0][0]
		# Keep vertices close to their original magnitude for precision and
		# ordinary per-cell frustum/shadow culling, including tower end views.
		var origin := GRID_ORIGIN + Vector3(key[0]) * CELL_SIZE
		var batch: GeometryInstance3D
		if not (key[-2] as Array).is_empty():
			var instances := MultiMesh.new()
			instances.transform_format = MultiMesh.TRANSFORM_3D
			instances.use_colors = key.back()
			instances.mesh = entries[0][2]
			instances.instance_count = entries.size()
			for index in entries.size():
				var local: Transform3D = entries[index][1]
				local.origin -= origin
				instances.set_instance_transform(index, local)
				if instances.use_colors:
					instances.set_instance_color(index, entries[index][4])
			var multi := MultiMeshInstance3D.new()
			multi.multimesh = instances
			multi.material_override = entries[0][3]
			batch = multi
		else:
			var surface := SurfaceTool.new()
			surface.begin(Mesh.PRIMITIVE_TRIANGLES)
			surface.set_material(first.get_active_material(0))
			for entry in entries:
				var local: Transform3D = entry[1]
				local.origin -= origin
				surface.append_from((entry[0] as MeshInstance3D).mesh, 0, local)
			var mesh := surface.commit()
			if mesh == null:
				continue
			var merged := MeshInstance3D.new()
			merged.mesh = mesh
			batch = merged
		batch.name = "StaticBatch%d" % created
		batch.position = origin
		batch.layers = first.layers
		batch.cast_shadow = first.cast_shadow
		var shared_shadow := true
		for entry in entries:
			shared_shadow = shared_shadow and shadows.handled.has((entry[0] as MeshInstance3D).get_instance_id())
		if shared_shadow:
			batch.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		batch.gi_mode = first.gi_mode
		batch.ignore_occlusion_culling = first.ignore_occlusion_culling
		batch.visibility_range_begin = first.visibility_range_begin
		batch.visibility_range_end = first.visibility_range_end
		batch.visibility_range_begin_margin = first.visibility_range_begin_margin
		batch.visibility_range_end_margin = first.visibility_range_end_margin
		batch.visibility_range_fade_mode = first.visibility_range_fade_mode
		batch.extra_cull_margin = first.extra_cull_margin
		batch.set_meta(&"static_batch_sources", entries.size())
		host.add_child(batch)
		if key[-3]:
			_detail_batches.append(batch)
		for entry in entries:
			(entry[0] as MeshInstance3D).layers = 0
		replaced += entries.size()
		created += 1
	for source in eligible:
		if shadows.handled.has(source.get_instance_id()):
			source.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return {"eligible_sources": considered, "replaced_sources": replaced,
		"batches": created, "saved_submissions": replaced - created,
		"shadow_batches": shadows.batches, "shadow_sources": shadows.handled.size()}


func _foliage_primitive(source: MeshInstance3D, material: StandardMaterial3D) -> Dictionary:
	# These authored primitives differ only in dimensions and foliage tint.
	# Move dimensions/color into instance data; retain every original segment,
	# cap, UV, normal and placement. No palette quantization or mesh reduction.
	if source.get_meta(&"part", "") not in ["TreeTrunk", "TreeCanopy", "ScrubLobe"] \
			or material.albedo_texture != null or material.normal_enabled \
			or material.emission_enabled or material.vertex_color_use_as_albedo \
			or material.uv1_triplanar or material.uv2_triplanar:
		return {}
	if source.mesh is CylinderMesh:
		var original := source.mesh as CylinderMesh
		if original.bottom_radius <= 0.0 or original.height <= 0.0:
			return {}
		var unit := original.duplicate() as CylinderMesh
		unit.top_radius = original.top_radius / original.bottom_radius
		unit.bottom_radius = 1.0
		unit.height = 1.0
		return {"mesh": unit, "scale": Vector3(original.bottom_radius, original.height, original.bottom_radius)}
	if source.mesh is SphereMesh:
		var original := source.mesh as SphereMesh
		if original.radius <= 0.0 or original.height <= 0.0:
			return {}
		var unit := original.duplicate() as SphereMesh
		unit.radius = 1.0
		unit.height = 2.0
		return {"mesh": unit, "scale": Vector3(original.radius, original.height * 0.5, original.radius)}
	return {}


func _material_signature(material: StandardMaterial3D) -> Array:
	# Compare full stored render properties, not rounded colors or a small
	# palette whitelist. Equal finishes can share despite distinct resources.
	var result: Array = []
	for property in material.get_property_list():
		var name: StringName = property["name"]
		if int(property["usage"]) & PROPERTY_USAGE_STORAGE \
				and name not in [&"resource_name", &"resource_local_to_scene", &"resource_path"]:
			result.append([name, material.get(name)])
	return result
