extends RefCounted
## Production receiving yard. Tangible steel/timber is exported through
## SolidExport into native Jolt; paint, flexible pennants and dust are visual.
const FreightArrival := preload("res://presentation/freight_yard_arrival.gd")
const WorldGeometry := preload("res://presentation/world_geometry.gd")
var _g: WorldGeometry
var _root: Node3D
var _dust: CPUParticles3D
var _cloth: ShaderMaterial
var _lights: Array[SpotLight3D] = []
var _clock := 0.0

func _init(geometry: WorldGeometry) -> void:
	_g = geometry

func build() -> void:
	_root = Node3D.new()
	_root.name = "ReceivingYard"
	_g.presentation_root.add_child(_root)
	var steel := _g.material(Color("353e40"), 0.65, 0.74, Color.BLACK, 1.0, _g.bump_steel)
	var oxide := _g.material(Color("684331"), 0.32, 0.89, Color.BLACK, 1.0, _g.bump_steel)
	var blue := _g.material(Color("37555b"), 0.35, 0.78, Color.BLACK, 1.0, _g.bump_steel)
	var timber := _g.material(Color("756047"), 0.0, 0.94, Color.BLACK, 1.0, _g.bump_timber)
	_weather_surface(blue, Color("4b6060"), Color("6e4430"), 731)
	_weather_surface(oxide, Color("704c39"), Color("3a3029"), 732)
	_weather_surface(timber, Color("8a7657"), Color("493e30"), 733)
	var paint := _g.material(Color("b49c59"), 0.0, 0.93)
	var chalk := _g.material(Color("9b9e8c"), 0.0, 1.0)
	_build_shed(steel, oxide, blue, timber)
	_build_stock(steel, oxide, timber)
	# Worn traffic paint follows the clear approach. It is flush and supplies
	# no invisible kerb, route force or prescribed parkour solution.
	for z in range(-102, 10, 12):
		for x in [-7.0, 7.0]:
			_box("YardPaint", Vector3(0.14, 0.003, 4.0), Vector3(x, 0.004, float(z)), paint)
	for x in range(22, 49, 6):
		_box("YardPaint", Vector3(0.12, 0.003, 10.0), Vector3(float(x), 0.004, -38.0), chalk)
	_build_pennants()
	_build_dust()
	var arrival := FreightArrival.new(_g, _cloth)
	_lights.append_array(arrival.build())

func _build_shed(steel: Material, oxide: Material, blue: Material, timber: Material) -> void:
	# Open-front workshop: a usable sheltered volume, not a solid box with
	# painted windows. All four posts, roof, back and side sheets are real.
	for x in [-44.0, -28.0]:
		for z in [-38.0, -22.0]:
			_box("YardStructure", Vector3(0.32, 5.0, 0.32), Vector3(x, 2.5, z), steel)
	_box("YardStructure", Vector3(16.5, 0.18, 17.0), Vector3(-36.0, 5.05, -30.0), oxide)
	_box("YardStructure", Vector3(16.0, 3.8, 0.16), Vector3(-36.0, 1.9, -38.0), blue)
	_box("YardStructure", Vector3(0.16, 2.8, 16.0), Vector3(-44.0, 1.4, -30.0), blue)
	# Fabricated roof ribs make the underside readable from the walk-in bay.
	for z in [-37.0, -33.0, -29.0, -25.0, -22.0]:
		_box("YardStructure", Vector3(16.0, 0.23, 0.13), Vector3(-36.0, 4.84, z), steel)
	_box("YardStructure", Vector3(16.0, 0.6, 0.15), Vector3(-36.0, 4.5, -22.0), blue)
	# Two actual benches leave a wide through aisle and room to look around.
	for x in [-40.0, -32.0]:
		_box("YardStock", Vector3(5.0, 0.13, 1.2), Vector3(x, 1.05, -36.5), timber)
		for dx in [-2.1, 2.1]:
			_box("YardStock", Vector3(0.12, 1.0, 1.0), Vector3(x + dx, 0.5, -36.5), steel)
	for x in [-42.0, -39.5, -37.0]:
		_box("YardStock", Vector3(1.7, 0.5, 0.8), Vector3(x, 1.36, -36.5), oxide)
	# Task lamps have visible housings and bounded shadow-free pools.
	for x in [-41.0, -31.0]:
		_box("YardStructure", Vector3(1.2, 0.12, 0.3), Vector3(x, 4.68, -30.0), steel)
		var lens := _g.material(Color("d9bd81"), 0.0, 0.5, Color("ffd198"), 1.2)
		_box("YardLens", Vector3(1.0, 0.025, 0.22), Vector3(x, 4.60, -30.0), lens)
		var light := SpotLight3D.new()
		_root.add_child(light)
		light.position = Vector3(x, 4.55, -30.0)
		light.rotation.x = -PI / 2.0
		light.light_color = Color(1.0, 0.78, 0.52)
		light.light_energy = 1.2
		light.spot_range = 8.0
		light.spot_angle = 65.0
		light.shadow_enabled = false
		_lights.append(light)

func _build_stock(steel: Material, oxide: Material, timber: Material) -> void:
	# Receiving stacks have lengths, gaps and timber bearers, rather than
	# randomly scattered cubes. Both sides leave the established route clear.
	for bay in 3:
		var z := -49.0 - float(bay) * 13.0
		for x in [27.0, 35.0, 43.0]:
			for dz in [-2.8, 2.8]:
				_box("YardStock", Vector3(1.6, 0.22, 0.26), Vector3(x, 0.11, z + dz), timber)
			for layer in 3:
				_box("YardStock", Vector3(1.35, 0.18, 7.5), Vector3(x, 0.34 + float(layer) * 0.22, z), oxide if bay == 1 else steel)
	# A separate timber repair pile beside the open workshop.
	for layer in 4:
		for x in [-44.0, -42.7, -41.4]:
			_box("YardStock", Vector3(0.9, 0.22, 6.0), Vector3(x, 0.12 + float(layer) * 0.25, -49.0), timber)
	# Low steel cable trays tie the stores back to the worksite visually.
	for z in [-72.0, -85.0, -98.0]:
		_box("YardStock", Vector3(6.5, 0.55, 1.8), Vector3(-38.0, 0.275, z), oxide)

func _build_pennants() -> void:
	# Flexible cloth is not a rigid support. Shader displacement is bounded
	# to 9cm and its fixed hem stays on an existing lamp mast at x=-10,z=-24.
	_cloth = ShaderMaterial.new()
	_cloth.shader = preload("res://presentation/yard_cloth.gdshader")
	for i in 3:
		var mesh := PlaneMesh.new()
		mesh.size = Vector2(0.65, 1.2)
		mesh.subdivide_width = 4
		mesh.subdivide_depth = 8
		var flag := MeshInstance3D.new()
		flag.name = "YardPennant"
		flag.set_meta(&"part", "YardPennant")
		flag.mesh = mesh
		flag.material_override = _cloth
		flag.rotation.x = PI / 2.0
		flag.position = Vector3(-9.76, 6.8 + float(i) * 1.3, -23.98)
		flag.visibility_range_end = 100.0
		_root.add_child(flag)

func _build_dust() -> void:
	_dust = CPUParticles3D.new()
	_dust.name = "YardAirborneDust"
	_dust.amount = 48
	_dust.lifetime = 7.0
	_dust.preprocess = 3.0
	_dust.fixed_fps = 20
	_dust.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	_dust.emission_box_extents = Vector3(12.0, 1.8, 9.0)
	_dust.direction = Vector3(1.0, 0.05, 0.25)
	_dust.spread = 18.0
	_dust.gravity = Vector3.ZERO
	_dust.initial_velocity_min = 0.15
	_dust.initial_velocity_max = 0.45
	_dust.scale_amount_min = 0.015
	_dust.scale_amount_max = 0.04
	var quad := QuadMesh.new()
	quad.size = Vector2.ONE
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	material.albedo_color = Color(0.7, 0.66, 0.54, 0.16)
	quad.material = material
	_dust.mesh = quad
	_root.add_child(_dust)
	_dust.position = Vector3(-34.0, 2.0, -26.0)
	_dust.visibility_aabb = AABB(Vector3(-16.0, -4.0, -12.0), Vector3(35.0, 10.0, 26.0))

func update(player: Vector3, delta: float, reduced_motion: bool, hour: float) -> void:
	_clock += clampf(delta, 0.0, 0.1)
	_cloth.set_shader_parameter("phase", _clock)
	_cloth.set_shader_parameter("motion", 0.0 if reduced_motion else 1.0)
	var near := player.distance_squared_to(Vector3(-34.0, 2.0, -26.0)) < 10000.0
	_dust.emitting = near and not reduced_motion
	_dust.visible = near and not reduced_motion
	var daylight := smoothstep(5.5, 8.0, hour) * (1.0 - smoothstep(17.0, 19.5, hour))
	for light in _lights:
		light.light_energy = lerpf(1.8, 0.7, daylight)

func _box(part: String, size: Vector3, at: Vector3, material: Material) -> void:
	var mesh := _g.add_box_to(part, size, at, material, _root)
	mesh.visibility_range_end = 180.0
	mesh.visibility_range_end_margin = 12.0

func _weather_surface(material: StandardMaterial3D, base: Color, stain: Color, seed_value: int) -> void:
	# One shared seeded albedo map per surface, not a texture per stock item.
	# Existing normal maps retain the steel pits and timber grain at hand range.
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.frequency = 0.035
	noise.fractal_octaves = 3
	var ramp := Gradient.new()
	ramp.offsets = PackedFloat32Array([0.0, 0.38, 0.70, 0.84, 1.0])
	ramp.colors = PackedColorArray([base.darkened(0.3), base, base.lightened(0.08), stain, stain.darkened(0.35)])
	var texture := NoiseTexture2D.new()
	texture.width = 256
	texture.height = 256
	texture.seamless = true
	texture.noise = noise
	texture.color_ramp = ramp
	material.albedo_color = Color.WHITE
	material.albedo_texture = texture
