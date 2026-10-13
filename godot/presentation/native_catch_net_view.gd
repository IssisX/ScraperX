extends Node3D

# Read-only rounded rope presentation of the distinct native catch cloth.
# Four corners per knot and ribbon triangles determine every display transform;
# there is no authored grid, filled face, collider or second physical authority.
# Two opaque MultiMesh draws, no additional textures, same aged-rope material.
# Catch knots/strands occupy only a few LOW-resolution pixels; eight radial
# segments retain rounded silhouettes while bounding this broad net's triangles.
const HALF_DEPTH := 0.035
var _native: Object
var _indices := PackedInt32Array()
var _material: StandardMaterial3D
var _knots: MultiMesh
var _strands: MultiMesh
var _links: Array[Vector2i] = []
var _cross_edges: Array[Vector4i] = []
var _last_vertices := PackedVector3Array()


func setup(native: Object, material: StandardMaterial3D) -> void:
	_native = native
	_material = material
	visible = false
	if _native == null or not _native.has_method("get_catch_net_vertices") or not _native.has_method("get_catch_net_indices"):
		return
	_indices = _native.get_catch_net_indices()
	var vertices: PackedVector3Array = _native.get_catch_net_vertices()
	if vertices.is_empty() or vertices.size() % 4 != 0 or _indices.is_empty() or _indices.size() % 6 != 0:
		return
	for index in _indices:
		if index < 0 or index >= vertices.size():
			return
	_build_mesh()
	render_view()


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
	# Regression fixture selection can remove the native soft body after
	# startup. Its old display must disappear with that body.
	self.visible = not vertices.is_empty()
	if vertices.is_empty() or _knots == null:
		visible = false
		return
	if vertices.size() != _knots.instance_count * 4:
		visible = false
		return
	# Read the real interpolated snapshot every frame. Resting geometry needs
	# no repeated script matrix work or instance uploads; any changed vertex
	# resumes the ordinary native-derived update, including after restoration.
	if vertices == _last_vertices:
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
