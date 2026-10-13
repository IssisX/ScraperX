extends Node3D

# Read-only presentation: an indexed native membrane is canvas; the legacy
# four-corner knot/ribbon topology retains its rounded open-weave display.
# No authored surface, collider, cloth animation, or second physics authority.
const HALF_DEPTH := 0.035
const TETHER_RADIUS := 0.01 # 20 mm diameter protective woven shockcord casing.
var _native: Object
var _indices := PackedInt32Array()
var _material: StandardMaterial3D
var _knots: MultiMesh
var _strands: MultiMesh
var _links: Array[Vector2i] = []
var _cross_edges: Array[Vector4i] = []
var _last_vertices := PackedVector3Array()
var _canvas: MeshInstance3D
var _canvas_mesh: ArrayMesh
var _canvas_uv := PackedVector2Array()
var _canvas_u := Vector3.RIGHT
var _vertex_count := 0
var _tether: MeshInstance3D
var _last_tether_vertices := PackedVector3Array()


func setup(native: Object, material: StandardMaterial3D) -> void:
	_native = native
	_material = material
	visible = false
	if _native == null or not _native.has_method("get_catch_net_vertices") or not _native.has_method("get_catch_net_indices"):
		return
	render_view()


func _configure_topology(vertices: PackedVector3Array, indices: PackedInt32Array) -> bool:
	if vertices.is_empty() or indices.is_empty() or indices.size() % 3 != 0:
		return false
	for index in indices:
		if index < 0 or index >= vertices.size():
			return false
	for point in vertices:
		if not point.is_finite():
			return false
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_knots = null
	_strands = null
	_canvas = null
	_canvas_mesh = null
	_tether = null
	_last_tether_vertices.clear()
	_links.clear()
	_cross_edges.clear()
	_last_vertices.clear()
	_indices = indices
	_vertex_count = vertices.size()
	# Identify the actual legacy knot face, not a guessed membrane grid.
	var open_weave := vertices.size() % 4 == 0 and indices.size() % 6 == 0 \
		and indices.size() >= 6 and indices[0] == 0 and indices[1] == 1 \
		and indices[2] == 3 and indices[3] == 0 and indices[4] == 3 and indices[5] == 2
	if open_weave:
		_build_mesh()
	else:
		_build_canvas(vertices)
	return true


func _build_canvas(vertices: PackedVector3Array) -> void:
	_canvas_mesh = ArrayMesh.new()
	_canvas = MeshInstance3D.new()
	_canvas.name = "Canvas"
	_canvas.mesh = _canvas_mesh
	add_child(_canvas)
	var cloth := _material.duplicate() as StandardMaterial3D
	cloth.cull_mode = BaseMaterial3D.CULL_DISABLED
	cloth.metallic = 0.0
	cloth.roughness = 0.97
	cloth.uv1_triplanar = false
	cloth.uv1_scale = Vector3.ONE
	cloth.uv1_offset = Vector3.ZERO
	# Static mipmapped weave maps: no extra surface, transparency, or GPU effect.
	# Keep the shared aged-fiber tint; membrane alone gets woven surface detail.
	_canvas_textures(cloth)
	_canvas.material_override = cloth
	# UVs are material coordinates recovered from real native triangle edges.
	# Freeze them for this topology so sag, interpolation, and restore cannot swim.
	var origin := vertices[_indices[0]]
	_canvas_u = (vertices[_indices[1]] - origin).normalized()
	var normal := (vertices[_indices[2]] - origin).cross(_canvas_u).normalized()
	var v_axis := _canvas_u.cross(normal).normalized()
	_canvas_uv.resize(vertices.size())
	for i in vertices.size():
		var offset := vertices[i] - origin
		_canvas_uv[i] = Vector2(offset.dot(_canvas_u), offset.dot(v_axis))


func _canvas_textures(cloth: StandardMaterial3D) -> void:
	const SIZE := 256
	var albedo := Image.create(SIZE, SIZE, false, Image.FORMAT_RGB8)
	var bump := Image.create(SIZE, SIZE, false, Image.FORMAT_RGB8)
	for y in SIZE:
		for x in SIZE:
			var uv := Vector2(float(x), float(y)) / float(SIZE)
			var warp := sin(TAU * uv.x * 32.0)
			var weft := sin(TAU * uv.y * 32.0)
			# Broad woven reinforcement crossing once per metre, with irregular
			# fiber mottling beneath it; all detail is shading, never fake geometry.
			var reinforcement := 1.0 - 0.16 * (1.0 - smoothstep(0.008, 0.022,
				minf(minf(uv.x, 1.0 - uv.x), minf(uv.y, 1.0 - uv.y))))
			var age := 0.88 + 0.06 * sin(TAU * (uv.x * 3.0 + uv.y * 2.0)) \
				+ 0.025 * sin(TAU * (uv.x * 19.0 - uv.y * 13.0))
			var tone := reinforcement * (age + 0.035 * warp * weft)
			albedo.set_pixel(x, y, Color(tone, tone * 0.985, tone * 0.95))
			var normal := Vector3(-0.16 * cos(TAU * uv.x * 32.0) * weft,
				-0.16 * cos(TAU * uv.y * 32.0) * warp, 1.0).normalized()
			bump.set_pixel(x, y, Color(normal.x * 0.5 + 0.5,
				normal.y * 0.5 + 0.5, normal.z * 0.5 + 0.5))
	albedo.generate_mipmaps()
	bump.generate_mipmaps()
	cloth.albedo_texture = ImageTexture.create_from_image(albedo)
	cloth.normal_texture = ImageTexture.create_from_image(bump)
	cloth.normal_enabled = true
	cloth.normal_scale = 0.65
	cloth.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS


func _render_tether() -> void:
	# A straight reduced-order connector for the native massless tension-only
	# spring/damper. Its casing is presentation, not discretized cord collision.
	# Both ends come from the same interpolated snapshot as the canvas/frame.
	if _canvas == null or not _native.has_method("get_catch_net_tether_vertices"):
		return
	var endpoints: PackedVector3Array = _native.get_catch_net_tether_vertices()
	if endpoints.size() != 2 or not endpoints[0].is_finite() or not endpoints[1].is_finite() \
			or endpoints[0].distance_squared_to(endpoints[1]) < 0.000001:
		if _tether != null:
			_tether.visible = false
		_last_tether_vertices.clear()
		return
	if _tether == null:
		var casing := CylinderMesh.new()
		casing.top_radius = 1.0
		casing.bottom_radius = 1.0
		casing.height = 1.0
		casing.radial_segments = 12
		casing.rings = 1
		var rubber := StandardMaterial3D.new()
		rubber.albedo_color = Color(0.95, 0.24, 0.035)
		rubber.roughness = 0.88
		# Fine braided sheath detail stays on a single opaque surface.
		var weave := Image.create(64, 64, false, Image.FORMAT_RGB8)
		for y in 64:
			for x in 64:
				var crossing := cos(TAU * float(x + y) / 8.0) * cos(TAU * float(x - y) / 8.0)
				var tone := 0.87 + 0.13 * crossing
				weave.set_pixel(x, y, Color(tone, tone, tone))
		weave.generate_mipmaps()
		rubber.albedo_texture = ImageTexture.create_from_image(weave)
		rubber.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
		_tether = MeshInstance3D.new()
		_tether.name = "Tether"
		_tether.mesh = casing
		_tether.material_override = rubber
		add_child(_tether)
	_tether.visible = true
	if endpoints == _last_tether_vertices:
		return
	var delta := endpoints[1] - endpoints[0]
	var along := delta.normalized()
	var reference := Vector3.UP if absf(along.dot(Vector3.UP)) < 0.99 else Vector3.RIGHT
	var right := reference.cross(along).normalized()
	var forward := right.cross(along).normalized()
	_tether.transform = Transform3D(Basis(right * TETHER_RADIUS, delta,
		forward * TETHER_RADIUS), (endpoints[0] + endpoints[1]) * 0.5)
	_last_tether_vertices = endpoints


func _render_canvas(vertices: PackedVector3Array) -> void:
	var normals := PackedVector3Array()
	normals.resize(vertices.size())
	var u_tangents := PackedVector3Array()
	var v_tangents := PackedVector3Array()
	u_tangents.resize(vertices.size())
	v_tangents.resize(vertices.size())
	var bounds := AABB(vertices[0], Vector3.ZERO)
	for point in vertices:
		bounds = bounds.expand(point)
	for i in range(0, _indices.size(), 3):
		var a := _indices[i]
		var b := _indices[i + 1]
		var c := _indices[i + 2]
		# Godot clockwise fronts; area-weighted normals preserve native faces.
		var normal := (vertices[c] - vertices[a]).cross(vertices[b] - vertices[a])
		normals[a] += normal
		normals[b] += normal
		normals[c] += normal
		var uv_b := _canvas_uv[b] - _canvas_uv[a]
		var uv_c := _canvas_uv[c] - _canvas_uv[a]
		var determinant := uv_b.x * uv_c.y - uv_b.y * uv_c.x
		if absf(determinant) > 0.000000000001:
			var edge_b := vertices[b] - vertices[a]
			var edge_c := vertices[c] - vertices[a]
			var u := (edge_b * uv_c.y - edge_c * uv_b.y) / determinant
			var v := (edge_c * uv_b.x - edge_b * uv_c.x) / determinant
			for vertex in [a, b, c]:
				u_tangents[vertex] += u
				v_tangents[vertex] += v
	var tangents := PackedFloat32Array()
	tangents.resize(vertices.size() * 4)
	for i in normals.size():
		normals[i] = normals[i].normalized()
		var tangent := (u_tangents[i] - normals[i] * u_tangents[i].dot(normals[i])).normalized()
		tangents[i * 4] = tangent.x
		tangents[i * 4 + 1] = tangent.y
		tangents[i * 4 + 2] = tangent.z
		tangents[i * 4 + 3] = -1.0 if normals[i].cross(tangent).dot(v_tangents[i]) < 0.0 else 1.0
	var arrays: Array = []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_TANGENT] = tangents
	arrays[Mesh.ARRAY_TEX_UV] = _canvas_uv
	arrays[Mesh.ARRAY_INDEX] = _indices
	_canvas_mesh.clear_surfaces()
	_canvas_mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	_canvas.custom_aabb = bounds.grow(0.0001)
	_last_vertices = vertices


func _build_mesh() -> void:
	var vertices: PackedVector3Array = _native.get_catch_net_vertices()
	for i in range(0, _indices.size(), 6):
		var a := _indices[i]
		var b := _indices[i + 1]
		var c := _indices[i + 2]
		var d := _indices[i + 5]
		var node_a := a / 4
		var node_b := b / 4
		var node_c := c / 4
		if node_a == node_b and node_a == node_c:
			continue # Knot face; its rounded display follows all four corners.
		if node_a != node_b:
			_links.append(Vector2i(node_a, node_b))
			_cross_edges.append(Vector4i(a, d, b, c))
		else:
			_links.append(Vector2i(node_a, node_c))
			_cross_edges.append(Vector4i(a, b, d, c))
	var rope := CylinderMesh.new()
	rope.top_radius = 1.0
	rope.bottom_radius = 1.0
	rope.height = 1.0
	rope.radial_segments = 8
	rope.rings = 1
	rope.material = _material
	_strands = _net_instances("Strands", rope, _links.size())
	var knot := SphereMesh.new()
	knot.radius = 1.0
	knot.height = 2.0
	knot.radial_segments = 8
	knot.rings = 3
	knot.material = _material
	_knots = _net_instances("Knots", knot, vertices.size() / 4)


func _net_instances(node_name: String, mesh: Mesh, count: int) -> MultiMesh:
	var instances := MultiMesh.new()
	instances.transform_format = MultiMesh.TRANSFORM_3D
	instances.mesh = mesh
	instances.instance_count = count
	var node := MultiMeshInstance3D.new()
	node.name = node_name
	node.multimesh = instances
	self.add_child(node)
	return instances


func render_view() -> void:
	if _native == null or not _native.has_method("get_catch_net_vertices"):
		visible = false
		return
	var vertices: PackedVector3Array = _native.get_catch_net_vertices()
	# Removed/uninitialized native bodies have no stale display. Reconfigure
	# from actual topology when a fixture installs a different native surface.
	if vertices.is_empty():
		visible = false
		return
	var indices: PackedInt32Array = _native.get_catch_net_indices()
	if vertices.size() != _vertex_count or indices != _indices:
		if not _configure_topology(vertices, indices):
			visible = false
			return
	visible = true
	_render_tether()
	# Immutable interpolated snapshots require no uploads while unchanged.
	if vertices == _last_vertices:
		return
	for point in vertices:
		if not point.is_finite():
			visible = false
			return
	if _canvas != null:
		_render_canvas(vertices)
		return
	if _knots == null or vertices.size() != _knots.instance_count * 4:
		visible = false
		return
	var centres := PackedVector3Array()
	centres.resize(_knots.instance_count)
	var bounds := AABB(vertices[0], Vector3.ZERO)
	for i in centres.size():
		var p := i * 4
		var centre := (vertices[p] + vertices[p + 1] + vertices[p + 2] + vertices[p + 3]) * 0.25
		var right := (vertices[p + 1] + vertices[p + 3] - vertices[p] - vertices[p + 2]) * 0.25
		var up := (vertices[p + 2] + vertices[p + 3] - vertices[p] - vertices[p + 1]) * 0.25
		var normal := right.cross(up)
		centres[i] = centre
		# Affine rounded proxy follows average stretch/shear/orientation.
		# sqrt(2) spans planar parallelogram corners, not arbitrary warp.
		var basis := Basis(Vector3.ZERO, Vector3.ZERO, Vector3.ZERO)
		if normal.length_squared() > 0.000000000001:
			basis = Basis(right * sqrt(2.0), up * sqrt(2.0), normal.normalized() * HALF_DEPTH)
		var pose := Transform3D(basis, centre)
		_knots.set_instance_transform(i, pose)
		bounds = bounds.merge(pose * _knots.mesh.get_aabb())
	for i in _links.size():
		var link := _links[i]
		var from := centres[link.x]
		var to := centres[link.y]
		var delta := to - from
		var length := delta.length()
		var edge := _cross_edges[i]
		# The in-plane width follows the native ribbon, retaining its 80 mm
		# unloaded width. Display depth supplies a rounded cross-section.
		var cross_edge := (vertices[edge.y] - vertices[edge.x] + vertices[edge.w] - vertices[edge.z]) * 0.25
		var basis := Basis(Vector3.ZERO, Vector3.ZERO, Vector3.ZERO)
		if length >= 0.001:
			var along := delta / length
			var right := cross_edge - along * cross_edge.dot(along)
			if right.length_squared() > 0.000000000001:
				var normal := right.normalized().cross(along)
				basis = Basis(right, delta, normal * HALF_DEPTH)
		var pose := Transform3D(basis, (from + to) * 0.5)
		_strands.set_instance_transform(i, pose)
		bounds = bounds.merge(pose * _strands.mesh.get_aabb())
	# One shared geometry batch per primitive; bounds include live deformation.
	# AABB stores size and reconstructs the endpoint; roundoff can otherwise
	# place a transformed knot one float ULP beyond the exact merged bounds.
	bounds = bounds.grow(0.0001)
	_knots.custom_aabb = bounds
	_strands.custom_aabb = bounds
	_last_vertices = vertices
