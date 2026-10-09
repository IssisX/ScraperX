extends Node3D

# Read-only native mechanism presentation. Main owns startup/frame ordering
# and supplies shared appearance/sign resources. This node is KitPresentation;
# its direct children and native interpolation retain their existing paths.
const KIT_PART_FLOATS := 13
const KIT_CABLE_SEGMENTS := 4
const RefractoryMaterial := preload("res://presentation/materials/refractory_material.gd")

var _native: Object
var _create_sign: Callable
var _cargo_net_mesh: Node3D
var _cargo_net_indices := PackedInt32Array()
var _cargo_net_material: StandardMaterial3D
var _cargo_net_knots: MultiMesh
var _cargo_net_strands: MultiMesh
var _cargo_net_links: Array[Vector2i] = []
var _cargo_net_cross_edges: Array[Vector4i] = []
# Display depth, aligned with existing particle standoff; native query faces
# remain the open ribbons. This is a rounded read-only render proxy.
const CARGO_NET_HALF_DEPTH := 0.035
var _kit_bodies: Array[Node3D] = []
var _kit_dynamic: Array[bool] = []
var _kit_cables: Array = []
var _cart_paint_material: StandardMaterial3D
var _water_lift_bucket_water: MeshInstance3D


func setup(native: Object, palette: Array[Material], cable_material: Material,
		cargo_net_material: StandardMaterial3D, create_sign: Callable) -> void:
	_native = native
	_cargo_net_material = cargo_net_material
	_create_sign = create_sign
	_build_kit(palette, cable_material)


func render_view() -> void:
	_render_kit()


func _kit_tube(outer: float, inner: float, half_length: float, material: Material) -> ArrayMesh:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	surface.set_material(material)
	for segment in 32:
		var a := TAU * float(segment) / 32.0
		var b := TAU * float(segment + 1) / 32.0
		var na := Vector3(cos(a), 0, sin(a))
		var nb := Vector3(cos(b), 0, sin(b))
		for radius in [outer, inner]:
			var sign_normal := 1.0 if radius == outer else -1.0
			var corners: Array[Vector3] = [na * radius + Vector3.UP * half_length,
				nb * radius + Vector3.UP * half_length, nb * radius - Vector3.UP * half_length,
				na * radius - Vector3.UP * half_length]
			var order := [0, 2, 1, 0, 3, 2] if radius == outer else [0, 1, 2, 0, 2, 3]
			for i in order:
				surface.set_normal((na if i in [0, 3] else nb) * sign_normal)
				surface.add_vertex(corners[i])
		for side in [-1.0, 1.0]:
			var end: Vector3 = Vector3.UP * half_length * side
			var corners: Array[Vector3] = [end + na * outer, end + nb * outer,
				end + nb * inner, end + na * inner]
			var order := [0, 1, 2, 0, 2, 3] if side > 0 else [0, 2, 1, 0, 3, 2]
			for i in order:
				surface.set_normal(Vector3.UP * side)
				surface.add_vertex(corners[i])
	return surface.commit()


func _kit_hull(triangles: PackedFloat32Array, material: Material, body: int, part: int) -> ArrayMesh:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	surface.set_material(material)
	if not _append_kit_hull(surface, triangles, Transform3D.IDENTITY, body, part):
		return null
	return surface.commit()


func _append_kit_hull(surface: SurfaceTool, triangles: PackedFloat32Array,
		local: Transform3D, body: int, part: int) -> bool:
	# The bridge exports cooked Jolt faces in part-local authoring space,
	# without the part offset/rotation. Validate the whole hull before adding
	# anything to a shared surface; invalid hulls never become partial solids.
	if triangles.is_empty() or triangles.size() % 9 != 0:
		push_error("KitView invalid cooked hull body=%d part=%d floats=%d" % [body, part, triangles.size()])
		return false
	for value in triangles:
		if not is_finite(value):
			push_error("KitView nonfinite cooked hull body=%d part=%d" % [body, part])
			return false
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	for p in range(0, triangles.size(), 9):
		var a := Vector3(triangles[p], triangles[p + 1], triangles[p + 2])
		var b := Vector3(triangles[p + 3], triangles[p + 4], triangles[p + 5])
		var c := Vector3(triangles[p + 6], triangles[p + 7], triangles[p + 8])
		var cross := (b - a).cross(c - a)
		if not cross.is_finite() or not is_finite(cross.length_squared()) \
				or cross.length_squared() <= 0.000000000000000001:
			push_error("KitView degenerate cooked hull body=%d part=%d triangle=%d" % [body, part, p / 9])
			return false
		# Jolt's outward CCW face order becomes Godot's clockwise front face.
		# Transform the native outward normal once, before the sole mesh commit
		# packs it. An intermediate committed mesh would quantize it twice.
		var normal := (local.basis * cross.normalized()).normalized()
		if not normal.is_finite() or normal.length_squared() <= 0.000000000000000001:
			push_error("KitView invalid cooked hull normal transform body=%d part=%d triangle=%d" % [body, part, p / 9])
			return false
		normals.append(normal)
		for vertex in [a, c, b]:
			var transformed: Vector3 = local * vertex
			if not transformed.is_finite():
				push_error("KitView nonfinite cooked hull transform body=%d part=%d triangle=%d" % [body, part, p / 9])
				return false
			vertices.append(transformed)
	for face in normals.size():
		surface.set_normal(normals[face])
		for corner in 3:
			surface.add_vertex(vertices[face * 3 + corner])
	return true


func _build_kit(palette: Array[Material], cable_material: Material) -> void:
	for body in int(_native.get_kit_body_count()):
		var node := Node3D.new()
		node.name = "KitBody%d" % int(_native.get_kit_body_entity_id(body))
		node.transform = _native.get_kit_body_transform(body)
		if int(_native.get_kit_body_entity_id(body)) == 1960:
			_create_sign.call("SERVICE +121\nSWING TRANSFER", Vector3(11.6, 112.1, -180.42), 0.0, 0.22, Color("e7c67c"), node)
			_create_sign.call("+121\nUP", Vector3(9, 121.8, -177.74), PI, 0.26, Color("e7c67c"), node)
		if int(_native.get_kit_body_entity_id(body)) == 2560:
			_create_sign.call("UP · JUMP TO DUCT\nCLIMB CABINET FIRST", Vector3(20.0, 23.5, -121.76), 0.0, 0.10, Color("e7c67c"), node)
		if int(_native.get_kit_body_entity_id(body)) == 2562:
			_create_sign.call("UP · VENT TO NEXT DECK\nCLIMB · HOLD FORWARD", Vector3(24.0, 28.2, -123.34), 0.0, 0.10, Color("e7c67c"), node)
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body)
		# Parts on one rigid body share a pose. Batch their triangles by
		# material once, instead of submitting every tread/rung separately.
		# This retains the native shapes, local poses, UVs and tube bores.
		var surfaces: Dictionary = {}
		for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
			var mesh: Mesh
			var material_index := clampi(int(parts[p + 10]), 0, palette.size() - 1)
			var material: Material = palette[material_index]
			var local := Transform3D(
				Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
				Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			if int(parts[p + 11]) == 1:
				if parts[p + 12] > 0.0:
					mesh = _kit_tube(parts[p], parts[p + 12], parts[p + 1], material)
				else:
					var cylinder := CylinderMesh.new()
					cylinder.top_radius = parts[p]
					cylinder.bottom_radius = parts[p]
					cylinder.height = parts[p + 1] * 2.0
					cylinder.material = material
					mesh = cylinder
			elif int(parts[p + 11]) == 2:
				var capsule := CapsuleMesh.new()
				capsule.radius = parts[p]
				capsule.height = (parts[p + 1] + parts[p]) * 2.0
				capsule.radial_segments = 20
				capsule.rings = 8
				capsule.material = material
				mesh = capsule
			elif int(parts[p + 11]) == 3:
				if not _native.has_method("get_kit_body_part_mesh"):
					push_error("KitView cooked hull bridge unavailable body=%d part=%d" % [body, p / KIT_PART_FLOATS])
					continue
				var triangles: PackedFloat32Array = _native.get_kit_body_part_mesh(body, p / KIT_PART_FLOATS)
				# Hulls carry unindexed vertices and flat normals. Append directly
				# to their final material batch, keeping indexed boxes separate.
				var channels := (1 << Mesh.ARRAY_VERTEX) | (1 << Mesh.ARRAY_NORMAL)
				var key := "%d/%d" % [material_index, channels]
				var surface: SurfaceTool = surfaces.get(key)
				if surface == null:
					surface = SurfaceTool.new()
					surface.begin(Mesh.PRIMITIVE_TRIANGLES)
					surface.set_material(material)
				if _append_kit_hull(surface, triangles, local, body, p / KIT_PART_FLOATS):
					surfaces[key] = surface
				continue
			else:
				var box := BoxMesh.new()
				box.size = Vector3(parts[p], parts[p + 1], parts[p + 2]) * 2.0
				box.material = material
				mesh = box
			for surface_index in mesh.get_surface_count():
				# Indexed boxes and the unindexed tube bore need separate
				# surfaces; mixing them would omit the unindexed triangles.
				var channels := 0
				var arrays := mesh.surface_get_arrays(surface_index)
				for channel in arrays.size():
					if arrays[channel] != null:
						channels |= 1 << channel
				var key := "%d/%d" % [material_index, channels]
				if not surfaces.has(key):
					var surface := SurfaceTool.new()
					surface.begin(Mesh.PRIMITIVE_TRIANGLES)
					surface.set_material(material)
					surfaces[key] = surface
				surfaces[key].append_from(mesh, surface_index, local)
		for key in surfaces:
			var surface: SurfaceTool = surfaces[key]
			var instance := MeshInstance3D.new()
			instance.mesh = surface.commit()
			if int(String(key).get_slice("/", 0)) == RefractoryMaterial.NATIVE_MATERIAL_INDEX:
				RefractoryMaterial.apply_to(instance, int(_native.get_kit_body_material_key(body)))
			node.add_child(instance)
		var entity := int(_native.get_kit_body_entity_id(body))
		# Signs stay on the native landing/deck bodies, including the moving
		# pendant. They label the real stations without owning machine state.
		if entity == 1981:
			_balance_sign(node, "GRAVITY BALANCE · +143\nOPERATE · HOLD UP TO RISE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1982:
			_balance_sign(node, "EXIT · +165\nHOLD DOWN TO RESET", Vector3(0.0, 1.8, 0.0))
		elif entity == 2980:
			_balance_sign(node, "GRAVITY BALANCE\nUP: RISE · DOWN: RESET\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1984:
			_balance_sign(node, "CROWN GONDOLA · +165\nOPERATE · HOLD UP TO RISE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1985:
			_balance_sign(node, "EXIT · +198\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
			_balance_sign(node, "CROWN RECALL\nHOLD DOWN TO RETURN", Vector3(-14.337928, 1.8, -0.55))
		elif entity == 2983:
			_balance_sign(node, "CROWN GONDOLA\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1987:
			_balance_sign(node, "TRACTION TRAM · +198\nOPERATE · HOLD UP TO CLIMB", Vector3(0.0, 1.8, 0.0))
		elif entity == 1988:
			_balance_sign(node, "EXIT · +231\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2984:
			_balance_sign(node, "TRACTION TRAM\nUP: CLIMB · DOWN: RETURN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1992:
			_balance_sign(node, "BARREL HELIX · +231\nOPERATE · ROLLER RIDES CAM", Vector3(0.0, 1.8, 0.0))
		elif entity == 1993:
			_balance_sign(node, "EXIT · +253\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2988:
			_balance_sign(node, "BARREL HELIX\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1995:
			_balance_sign(node, "CASCADE MAST · +253\nOPERATE · 1:2:3 CABLE CASCADE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1996:
			_balance_sign(node, "EXIT · +286\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2992:
			_balance_sign(node, "CASCADE MAST\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1921:
			_balance_sign(node, "PITMAN LIFT · +286\nOPERATE · CRANK DRIVES ROD", Vector3(0.0, 1.8, 0.0))
		elif entity == 1922:
			_balance_sign(node, "EXIT · +308\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2921:
			_balance_sign(node, "PITMAN LIFT\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		if entity == 1941:
			_balance_sign(node, "REFRACTORY RECLAIM · +308\nFEED WHILE HELD · BOARD TO RELEASE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1942:
			_balance_sign(node, "RECLAIM EXIT · +330\nEMPTY THE SCOOPS BEFORE RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 1945:
			_balance_sign(node, "MAINTENANCE CROSSING · +319\nDUMP TO RETURN · LOWER FEED BELOW", Vector3(3.5, -0.5, 5.75))
		elif entity == 2201:
			_balance_sign(node, "RECLAIM CABIN\nUP: RELEASE BRAKE · DOWN: DUMP\nRELEASE TO HOLD", Vector3(0.0, -0.7, 0.0))
		# Factory static parts share (-41.5,327.25,-142); the cart control
		# moves with its deck. Each sign is local to its actual native owner.
		if entity == 1955:
			_balance_sign(node, "SLAB HAUL · +330\nFALLING SLAB LIFTS CART\nDOWN: PAID RETURN", Vector3(15.6, 3.85, 2.5))
		elif entity == 1956:
			_balance_sign(node, "INSPECTION · +341\nJUMP ASHORE · DOWN TO RECOVER\nREBOARD TO CONTINUE", Vector3(15.6, 14.85, 2.5))
		elif entity == 1957:
			_balance_sign(node, "EXIT · +352\nJUMP NORTH LANE TO TOWER\nDOWN: RAISE SLAB / RETURN CART", Vector3(16.4, 25.85, 2.5))
		elif entity == 2320:
			_balance_sign(node, "SLAB HAUL CART\nUP: RELEASE · NEUTRAL: BRAKE\n341 INSPECTION · 352 JUMP", Vector3(-1.15, 3.85, -0.45))
		# Paint identifies a maintenance transfer, not a scripted cargo objective.
		# Every mark lies on an existing native surface and follows that owner.
		if entity == 1957:
			_cart_storage_marks(node, Vector3(15.7, 24.751, 2.35))
			var stores := Label3D.new()
			stores.text = "MAINTENANCE STORES"
			stores.position = Vector3(15.7, 24.752, 2.65)
			stores.rotation.x = -PI / 2.0
			stores.font_size = 32
			stores.pixel_size = 0.0025
			stores.modulate = Color("d7b54f")
			node.add_child(stores)
		elif entity == 2327:
			_cart_paint(node, Vector3(0.25, 0.001, 0.045), Vector3(0, 0.1306, 0))
		elif entity == 2326:
			_cart_paint(node, Vector3(0.08, 0.30, 0.001),
				Vector3(0.1797, -0.10375, 0.1806), Vector3(0, 0, PI / 3.0))
		elif entity == 1958:
			# Slab-centre witness bands for cart330/341/352; no implied powered ascent.
			for slab_y in [351.721281, 339.019575, 326.317869]:
				_cart_paint(node, Vector3(0.001, 0.045, 0.17),
					Vector3(-2.0985, slab_y - 327.25, 0.7))
		if entity == 2952:
			var sign := Label3D.new()
			sign.name = "CargoNetSign"
			sign.text = "EASY WAY UP · FIRST DECK +11 M\nTAP CLIMB · HOLD FORWARD"
			sign.position = Vector3(20.0, 2.8, -117.65)
			sign.font_size = 44
			sign.pixel_size = 0.006
			sign.modulate = Color("fff0c4")
			sign.outline_size = 12
			node.add_child(sign)
		if entity == 2011:
			var water_mesh := BoxMesh.new()
			water_mesh.size = Vector3(1.54, 1.0, 1.54)
			var water_mat := StandardMaterial3D.new()
			water_mat.albedo_color = Color(0.08, 0.30, 0.38, 0.72)
			water_mat.roughness = 0.16
			water_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			water_mesh.material = water_mat
			_water_lift_bucket_water = MeshInstance3D.new()
			_water_lift_bucket_water.name = "BucketWater"
			_water_lift_bucket_water.mesh = water_mesh
			_water_lift_bucket_water.visible = false
			node.add_child(_water_lift_bucket_water)
		add_child(node)
		_kit_bodies.append(node)
		_kit_dynamic.append(bool(_native.is_kit_body_dynamic(body)))
	_cargo_net_indices = _native.get_cargo_net_indices()
	if not _cargo_net_indices.is_empty():
		_build_cargo_net()
	var cable := BoxMesh.new()
	cable.size = Vector3(0.035, 0.035, 1.0)
	cable.material = cable_material
	for index in int(_native.get_kit_cable_count()):
		var segments: Array[MeshInstance3D] = []
		for segment in KIT_CABLE_SEGMENTS:
			var instance := MeshInstance3D.new()
			instance.name = "KitCable%d_%d" % [index, segment]
			instance.mesh = cable
			instance.visible = false
			add_child(instance)
			segments.append(instance)
		_kit_cables.append(segments)
	_render_kit()


func _cart_paint(parent: Node3D, size: Vector3, at: Vector3,
		rotation: Vector3 = Vector3.ZERO) -> void:
	if _cart_paint_material == null:
		_cart_paint_material = StandardMaterial3D.new()
		_cart_paint_material.albedo_color = Color("bca04c")
		_cart_paint_material.roughness = 0.94
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = _cart_paint_material
	var marking := MeshInstance3D.new()
	marking.name = "MaintenancePaint"
	marking.mesh = mesh
	marking.position = at
	marking.rotation = rotation
	marking.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(marking)


func _cart_storage_marks(parent: Node3D, centre: Vector3) -> void:
	# A .36m painted footprint suits the real .26m block without claiming
	# a new structural surface, delivery reward or hidden cargo constraint.
	for side in [-1.0, 1.0]:
		_cart_paint(parent, Vector3(0.36, 0.001, 0.012), centre + Vector3(0, 0, side * 0.18))
		_cart_paint(parent, Vector3(0.012, 0.001, 0.36), centre + Vector3(side * 0.18, 0, 0))


func _balance_sign(parent: Node3D, text: String, at: Vector3) -> void:
	var plate := MeshInstance3D.new()
	plate.name = "GravityBalanceControlSign"
	var mesh := BoxMesh.new()
	mesh.size = Vector3(3.8, 0.84, 0.035)
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("252a2d")
	material.roughness = 0.85
	mesh.material = material
	plate.mesh = mesh
	plate.position = at
	parent.add_child(plate)
	for face in [-1.0, 1.0]:
		var label: Label3D = _create_sign.call(text, at + Vector3(0.0, 0.0, face * 0.025),
			0.0 if face > 0.0 else PI, 0.16, Color("fff0c4"), parent)
		label.name = "GravityBalanceInstructions"
		label.outline_size = 8


# Native CargoNet exposes four corners per knot and two triangles per woven
# quad. Derive links from that topology, without authoring another grid.
func _build_cargo_net() -> void:
	_cargo_net_mesh = Node3D.new()
	_cargo_net_mesh.name = "NativeSoftCargoNet"
	add_child(_cargo_net_mesh)
	var vertices: PackedVector3Array = _native.get_cargo_net_vertices()
	for i in range(0, _cargo_net_indices.size(), 6):
		var a := _cargo_net_indices[i]
		var b := _cargo_net_indices[i + 1]
		var c := _cargo_net_indices[i + 2]
		var d := _cargo_net_indices[i + 5]
		var node_a := a / 4
		var node_b := b / 4
		var node_c := c / 4
		if node_a == node_b and node_a == node_c:
			continue # Knot face; its rounded display follows all four corners.
		if node_a != node_b:
			_cargo_net_links.append(Vector2i(node_a, node_b))
			_cargo_net_cross_edges.append(Vector4i(a, d, b, c))
		else:
			_cargo_net_links.append(Vector2i(node_a, node_c))
			_cargo_net_cross_edges.append(Vector4i(a, b, d, c))
	var rope := CylinderMesh.new()
	rope.top_radius = 1.0
	rope.bottom_radius = 1.0
	rope.height = 1.0
	rope.radial_segments = 16
	rope.rings = 1
	rope.material = _cargo_net_material
	_cargo_net_strands = _net_instances("Strands", rope, _cargo_net_links.size())
	var knot := SphereMesh.new()
	knot.radius = 1.0
	knot.height = 2.0
	knot.radial_segments = 16
	knot.rings = 7
	knot.material = _cargo_net_material
	_cargo_net_knots = _net_instances("Knots", knot, vertices.size() / 4)


func _net_instances(node_name: String, mesh: Mesh, count: int) -> MultiMesh:
	var instances := MultiMesh.new()
	instances.transform_format = MultiMesh.TRANSFORM_3D
	instances.mesh = mesh
	instances.instance_count = count
	var node := MultiMeshInstance3D.new()
	node.name = node_name
	node.multimesh = instances
	_cargo_net_mesh.add_child(node)
	return instances


func _update_cargo_net() -> void:
	if _cargo_net_mesh == null or DisplayServer.get_name() == "headless":
		return
	var vertices: PackedVector3Array = _native.get_cargo_net_vertices()
	# Regression fixture selection can remove the native soft body after
	# startup. Its old display must disappear with that body.
	_cargo_net_mesh.visible = not vertices.is_empty()
	if vertices.is_empty():
		return
	var centres := PackedVector3Array()
	centres.resize(_cargo_net_knots.instance_count)
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
			basis = Basis(right * sqrt(2.0), up * sqrt(2.0), normal.normalized() * CARGO_NET_HALF_DEPTH)
		var pose := Transform3D(basis, centre)
		_cargo_net_knots.set_instance_transform(i, pose)
		bounds = bounds.merge(pose * _cargo_net_knots.mesh.get_aabb())
	for i in _cargo_net_links.size():
		var link := _cargo_net_links[i]
		var from := centres[link.x]
		var to := centres[link.y]
		var delta := to - from
		var length := delta.length()
		var edge := _cargo_net_cross_edges[i]
		# The in-plane width follows the native ribbon, retaining its 80 mm
		# unloaded width. Display depth supplies a rounded cross-section.
		var cross_edge := (vertices[edge.y] - vertices[edge.x] + vertices[edge.w] - vertices[edge.z]) * 0.25
		var basis := Basis(Vector3.ZERO, Vector3.ZERO, Vector3.ZERO)
		if length >= 0.001:
			var along := delta / length
			var right := cross_edge - along * cross_edge.dot(along)
			if right.length_squared() > 0.000000000001:
				var normal := right.normalized().cross(along)
				basis = Basis(right, delta, normal * CARGO_NET_HALF_DEPTH)
		var pose := Transform3D(basis, (from + to) * 0.5)
		_cargo_net_strands.set_instance_transform(i, pose)
		bounds = bounds.merge(pose * _cargo_net_strands.mesh.get_aabb())
	# One shared geometry batch per primitive; bounds include live deformation.
	# AABB stores size and reconstructs the endpoint; roundoff can otherwise
	# place a transformed knot one float ULP beyond the exact merged bounds.
	bounds = bounds.grow(0.0001)
	_cargo_net_knots.custom_aabb = bounds
	_cargo_net_strands.custom_aabb = bounds


# Poses moving kit bodies and cables from this frame's native snapshot.
# A disabled native body is hidden without replacing its physical state.
func _render_kit() -> void:
	_update_cargo_net()
	if _native == null:
		return
	for body in _kit_bodies.size():
		if not _kit_dynamic[body]:
			continue
		var node: Node3D = _kit_bodies[body]
		node.visible = bool(_native.is_kit_body_enabled(body))
		if node.visible:
			node.transform = _native.get_kit_body_render_transform(body)
	if _water_lift_bucket_water != null:
		var volume := clampf(float(_native.get_water_lift_bucket_water_m3()), 0.0, 2.0)
		var depth := clampf(volume / (1.54 * 1.54), 0.0, 0.94)
		_water_lift_bucket_water.visible = depth > 0.002
		if _water_lift_bucket_water.visible:
			var water_mesh := _water_lift_bucket_water.mesh as BoxMesh
			water_mesh.size = Vector3(1.54, depth, 1.54)
			_water_lift_bucket_water.position = Vector3(0.0, -0.29 + depth * 0.5, 0.0)
	for index in _kit_cables.size():
		var points: PackedVector3Array = _native.get_kit_cable_render_points(index)
		var segments: Array = _kit_cables[index]
		for segment in segments.size():
			var instance: MeshInstance3D = segments[segment]
			if segment + 1 < points.size():
				_lay_segment(instance, points[segment], points[segment + 1])
			else:
				instance.visible = false


func _lay_segment(instance: MeshInstance3D, from: Vector3, to: Vector3) -> void:
	var delta := to - from
	var length := delta.length()
	if length < 0.001:
		instance.visible = false
		return
	var along := delta / length
	var up := Vector3.UP if absf(along.y) < 0.99 else Vector3.RIGHT
	var side := up.cross(along).normalized()
	instance.transform = Transform3D(Basis(side, along.cross(side), along * length),
		from + delta * 0.5)
	instance.visible = true
