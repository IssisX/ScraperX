extends RefCounted

# A composed entrance to the real truck/crane yard. All structural/stock boxes
# use the existing SolidExport classes; paint, lenses and cloth are presentation.
const WorldGeometry := preload("res://presentation/world_geometry.gd")
var _g: WorldGeometry
var _cloth: ShaderMaterial
var _root: Node3D

func _init(geometry: WorldGeometry, cloth: ShaderMaterial) -> void:
	_g = geometry
	_cloth = cloth

func build() -> Array[SpotLight3D]:
	_root = Node3D.new()
	_root.name = "FreightArrival"
	_root.set_meta(&"static_render_container", true)
	_g.presentation_root.add_child(_root)
	var steel := _g.material(Color("38454a"), 0.62, 0.73, Color.BLACK, 1.0, _g.bump_steel)
	var oxide := _g.material(Color("754d36"), 0.24, 0.9, Color.BLACK, 1.0, _g.bump_steel)
	var blue := _g.material(Color("29494f"), 0.4, 0.76, Color.BLACK, 1.0, _g.bump_steel)
	var timber := _g.material(Color("837054"), 0.0, 0.94, Color.BLACK, 1.0, _g.bump_timber)
	var concrete := _g.material(Color("65645c"), 0.0, 0.96, Color.BLACK, 1.0, _g.bump_concrete)
	var yellow := _g.material(Color("b7a15d"), 0.05, 0.86)
	# A 28m clear-span open canopy frames both machines without placing a
	# post in the truck's proven turning arc (-102..-86 X, -58..-47.5 Z).
	for x in [-106.0, -78.0]:
		for z in [-41.0, -28.0]:
			_box("YardStructure", Vector3(0.30, 4.6, 0.30), Vector3(x, 2.3, z), steel)
			_box("YardStructure", Vector3(0.9, 0.3, 0.9), Vector3(x, 0.15, z), concrete)
	_box("YardStructure", Vector3(28.6, 0.16, 13.8), Vector3(-92, 4.78, -34.5), oxide)
	for z in [-41.0, -34.5, -28.0]:
		_box("YardStructure", Vector3(28.0, 0.24, 0.18), Vector3(-92, 4.54, z), steel)
	_box("YardStructure", Vector3(27.6, 0.62, 0.12), Vector3(-92, 4.15, -41), blue)
	# Usable side work surfaces leave the centre and all freight approaches clear.
	for z in [-31.0, -37.0]:
		_box("YardStock", Vector3(2.0, 0.14, 4.4), Vector3(-104, 0.92, z), timber)
		for dz in [-1.8, 1.8]:
			_box("YardStructure", Vector3(1.8, 0.85, 0.14), Vector3(-104, 0.425, z + dz), steel)
		for layer in 3:
			_box("YardStock", Vector3(1.4, 0.16, 3.1), Vector3(-104, 1.12 + layer * 0.19, z), oxide)
	for x in [-82.0, -79.5]:
		_box("YardStock", Vector3(1.6, 0.32, 3.2), Vector3(x, 0.16, -36), timber)
		_box("YardStock", Vector3(1.35, 1.25, 1.9), Vector3(x, 0.945, -36), blue)
		_box("YardStock", Vector3(1.5, 0.08, 2.05), Vector3(x, 1.61, -36), steel)
	# Forward stores frame the clear cab approach without entering the truck's
	# proven turn/exchange corridor. Their bearers and steel are tangible stock.
	for x in [-108.0, -82.0]:
		for dz in [-1.6, 1.6]:
			_box("YardStock", Vector3(2.2, 0.22, 0.28), Vector3(x, 0.11, -48 + dz), timber)
		for layer in 4:
			_box("YardStock", Vector3(1.7, 0.20, 4.4), Vector3(x, 0.34 + layer * 0.23, -48), oxide)
		for dz in [-2.5, 2.5]:
			_box("YardStructure", Vector3(0.22, 1.1, 0.22), Vector3(x + 1.3, 0.55, -48 + dz), yellow)
	# Stock streets frame the opening at human scale while the working middle
	# remains open. The west stack is outside the truck/cargo lane; the east
	# stores stop north of the crane's 35m operating radius (pivot Z=-150).
	_container(Vector3(-112, 0, -72), 12.2, oxide, steel)
	_container(Vector3(-112, 2.6, -72), 12.2, blue, steel)
	_container(Vector3(-75, 0, -69), 12.2, blue, steel)
	_container(Vector3(-75, 0, -94), 6.1, oxide, steel)
	_beam_stock(Vector3(-111, 0, -94), steel, oxide, timber)
	_plate_stock(Vector3(-75, 0, -108), steel, oxide, timber)
	# Flush paint reserves a 4m walking/service strip on the east side of
	# the open truck aisle, then ends at the existing crane approach pad.
	# Paint does not pretend to supply collision or native route authority.
	for z in range(-58, -138, -4):
		for x in [-85.0, -81.0]:
			_box("YardPaint", Vector3(0.10, 0.003, 2.4), Vector3(x, 0.005, z), yellow)
	# Raised hoses/handrails are real steel; no decorative invisible obstacles.
	_box("YardStructure", Vector3(0.16, 1.1, 4.6), Vector3(-77.8, 0.55, -35), steel)
	for z in [-37.1, -32.9]:
		_box("YardStructure", Vector3(0.16, 1.4, 0.16), Vector3(-77.8, 0.7, z), yellow)
	# Existing warehouse lane boundaries leave the actual vehicle aisle clear.
	for z in range(-62, -38, 4):
		for x in [-103.2, -96.8]:
			_box("YardPaint", Vector3(0.12, 0.003, 2.4), Vector3(x, 0.005, z), yellow)
	for x in [-105.7, -78.3]:
		var flag := MeshInstance3D.new()
		flag.name = "FreightPennant"
		var plane := PlaneMesh.new()
		plane.size = Vector2(0.65, 1.2)
		plane.subdivide_width = 4
		plane.subdivide_depth = 8
		flag.mesh = plane
		flag.material_override = _cloth
		flag.position = Vector3(x, 3.2, -40.8)
		flag.rotation.x = PI / 2
		flag.visibility_range_end = 90
		_root.add_child(flag)
	var lights: Array[SpotLight3D] = []
	var lens := _g.material(Color("dac390"), 0.0, 0.5, Color("f9cc86"), 1.5)
	for x in [-101.0, -83.0]:
		_box("YardStructure", Vector3(1.4, 0.12, 0.35), Vector3(x, 4.5, -36), steel)
		_box("YardLens", Vector3(1.15, 0.025, 0.24), Vector3(x, 4.43, -36), lens)
		var light := SpotLight3D.new()
		light.position = Vector3(x, 4.4, -36)
		light.rotation.x = -PI / 2
		light.light_color = Color(1.0, 0.8, 0.55)
		light.spot_range = 10
		light.spot_angle = 65
		light.light_energy = 1.8
		light.shadow_enabled = false
		_root.add_child(light)
		lights.append(light)
	return lights

func _box(part: String, size: Vector3, at: Vector3, material: Material, yaw := 0.0) -> void:
	var mesh := _g.add_box_to(part, size, at, material, _root, yaw)
	mesh.visibility_range_end = 180

# Closed freight containers, built from actual skin, corner posts, edge
# rails, corrugation and locking bars. Every visible rib exports as stock;
# roofs have bearing along the rails and the stacked unit bears at corners.
func _container(at: Vector3, length: float, paint: Material, steel: Material) -> void:
	for y in [0.06, 2.54]:
		_box("YardStock", Vector3(2.44, 0.12, length), at + Vector3(0, y, 0), paint)
	for side in [-1.0, 1.0]:
		_box("YardStock", Vector3(0.08, 2.36, length), at + Vector3(side * 1.18, 1.3, 0), paint)
		for y in [0.10, 2.50]:
			_box("YardStructure", Vector3(0.14, 0.20, length), at + Vector3(side * 1.22, y, 0), steel)
		for end in [-1.0, 1.0]:
			_box("YardStructure", Vector3(0.14, 2.6, 0.14), at + Vector3(side * 1.22, 1.3, end * (length * 0.5 - 0.07)), steel)
		# Widely spaced real pressed ribs provide depth without new textures.
		var ribs := int(length / 0.75)
		for rib in ribs:
			var z := -length * 0.5 + 0.45 + rib * 0.75
			_box("YardStock", Vector3(0.06, 2.16, 0.08), at + Vector3(side * 1.25, 1.3, z), paint)
	for end in [-1.0, 1.0]:
		_box("YardStock", Vector3(2.30, 2.36, 0.08), at + Vector3(0, 1.3, end * (length * 0.5 - 0.04)), paint)
		for y in [0.10, 2.50]:
			_box("YardStructure", Vector3(2.44, 0.20, 0.14), at + Vector3(0, y, end * (length * 0.5 - 0.07)), steel)
	# Twin door seam and four locking bars face the arrival. Handles are
	# actual short bars joined to their rods, with no hovering sign backing.
	_box("YardStructure", Vector3(0.035, 2.25, 0.035), at + Vector3(0, 1.3, length * 0.5 + 0.015), steel)
	for x in [-0.88, -0.34, 0.34, 0.88]:
		_box("YardStructure", Vector3(0.045, 2.2, 0.045), at + Vector3(x, 1.3, length * 0.5 + 0.02), steel)
		_box("YardStructure", Vector3(0.18, 0.05, 0.05), at + Vector3(x + 0.065, 0.9, length * 0.5 + 0.04), steel)

# Nine recognizable I beams on three tiers of cross-sleepers. Each next
# tier bears on the preceding top flange; no floating decorative bundles.
func _beam_stock(at: Vector3, steel: Material, oxide: Material, timber: Material) -> void:
	for layer in 3:
		var base := layer * 0.46
		for z in [-2.9, 2.9]:
			_box("YardStock", Vector3(2.8, 0.14, 0.24), at + Vector3(0, base + 0.07, z), timber)
		for column in 3:
			var x := (column - 1) * 0.8
			var surface := oxide if column == 1 else steel
			for y in [0.17, 0.43]:
				_box("YardStock", Vector3(0.38, 0.06, 8.0), at + Vector3(x, base + y, 0), surface)
			_box("YardStock", Vector3(0.06, 0.20, 8.0), at + Vector3(x, base + 0.30, 0), surface)

func _plate_stock(at: Vector3, steel: Material, oxide: Material, timber: Material) -> void:
	for z in [-2.1, 2.1]:
		_box("YardStock", Vector3(3.4, 0.24, 0.32), at + Vector3(0, 0.12, z), timber)
	for layer in 5:
		_box("YardStock", Vector3(3.0, 0.16, 6.4), at + Vector3(0, 0.32 + layer * 0.16, 0), oxide if layer % 2 == 0 else steel)
