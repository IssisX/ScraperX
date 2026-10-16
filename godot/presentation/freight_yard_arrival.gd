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
	var white := _g.material(Color("c6c9b9"), 0.0, 0.94)
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
	_sign("FREIGHT  /  RECEIVING 01", Vector3(-92, 4.15, -40.91), 0.005, Color("e2cf8c"))
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
	# Raised hoses/handrails are real steel; no decorative invisible obstacles.
	_box("YardStructure", Vector3(0.16, 1.1, 4.6), Vector3(-77.8, 0.55, -35), steel)
	for z in [-37.1, -32.9]:
		_box("YardStructure", Vector3(0.16, 1.4, 0.16), Vector3(-77.8, 0.7, z), yellow)
	# Flush lane paint points toward the existing truck and then the crane aisle.
	for z in range(-62, -38, 4):
		for x in [-103.2, -96.8]:
			_box("YardPaint", Vector3(0.12, 0.003, 2.4), Vector3(x, 0.005, z), yellow)
	for z in [-38.0, -41.0, -44.0]:
		var at := Vector3(-93.4, 0.008, z)
		_box("YardPaint", Vector3(0.13, 0.003, 1.0), at + Vector3(-0.28, 0, 0), white, -0.55)
		_box("YardPaint", Vector3(0.13, 0.003, 1.0), at + Vector3(0.28, 0, 0), white, 0.55)
	_sign("MATERIAL HANDLING", Vector3(-100, 0.012, -43), 0.004, Color("b7a15d"), true)
	_sign("FORKLIFT 01", Vector3(-101.8, 2.6, -41.2), 0.0035, Color("e0d6b5"))
	_sign("CRANE YARD  /  TOWER ACCESS", Vector3(-83, 2.6, -41.2), 0.003, Color("e0d6b5"))
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

func _sign(text: String, at: Vector3, pixel: float, color: Color, floor_text := false) -> void:
	var sign := Label3D.new()
	sign.text = text
	sign.font_size = 64
	sign.pixel_size = pixel
	sign.modulate = color
	sign.outline_size = 0
	sign.position = at
	sign.visibility_range_end = 90
	if floor_text:
		sign.rotation.x = -PI / 2
	_root.add_child(sign)
