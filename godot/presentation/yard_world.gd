extends Node3D

# One active machine. The native GroundStage owns every changing quantity.
const SCREW_LOW := Vector3(0.0, 0.55, -3.0)
const SCREW_HIGH := Vector3(0.0, 6.05, -12.526279)
const SCREW_LENGTH := 11.0
const SCREW_PITCH := 0.8
const CAGE_XZ := Vector2(4.0, -9.0)

var cage: AnimatableBody3D
var _bucket: Node3D
var _rotor: Node3D
var _tank_water: MeshInstance3D
var _basin_water: MeshInstance3D
var _bucket_water: MeshInstance3D
var _water_packets: Array[MeshInstance3D] = []
var _rope_nodes: Array[MeshInstance3D] = []
var _steel: StandardMaterial3D
var _dark: StandardMaterial3D
var _rust: StandardMaterial3D
var _amber: StandardMaterial3D
var _concrete: StandardMaterial3D
var _water: StandardMaterial3D
var _glass: StandardMaterial3D

func _ready() -> void:
	_make_materials()
	_build_light()
	_build_ground()
	_build_tower()
	_build_screw()
	_build_tank_and_bucket()
	_build_cage_and_dock()
	_build_rope()

func _make_materials() -> void:
	_steel = _material(Color(0.48, 0.53, 0.55), 0.72, 0.34)
	_dark = _material(Color(0.095, 0.13, 0.15), 0.45, 0.55)
	_rust = _material(Color(0.32, 0.18, 0.11), 0.52, 0.73)
	_amber = _material(Color(0.94, 0.55, 0.12), 0.24, 0.38)
	_concrete = _material(Color(0.28, 0.32, 0.33), 0.0, 0.88)
	_water = _material(Color(0.13, 0.56, 0.69, 0.78), 0.12, 0.17)
	_water.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_water.emission_enabled = true
	_water.emission = Color(0.025, 0.20, 0.25)
	_glass = _material(Color(0.42, 0.65, 0.69, 0.22), 0.14, 0.14)
	_glass.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

func _material(color: Color, metal: float, rough: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metal
	material.roughness = rough
	return material

func _build_light() -> void:
	var environment := WorldEnvironment.new()
	var settings := Environment.new()
	settings.background_mode = Environment.BG_COLOR
	settings.background_color = Color(0.34, 0.40, 0.46)
	settings.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	settings.ambient_light_color = Color(0.58, 0.65, 0.68)
	settings.ambient_light_energy = 0.62
	settings.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	environment.environment = settings
	add_child(environment)
	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-44.0, 25.0, 0.0)
	sun.light_color = Color(1.0, 0.86, 0.66)
	sun.light_energy = 1.65
	sun.shadow_enabled = true
	add_child(sun)

func _box(label: String, size: Vector3, where: Vector3, material: Material,
		solid: bool = false, host: Node3D = null) -> MeshInstance3D:
	var parent_node: Node3D = host if host != null else self
	var visual := MeshInstance3D.new()
	visual.name = label
	var box := BoxMesh.new()
	box.size = size
	visual.mesh = box
	visual.material_override = material
	visual.position = where
	parent_node.add_child(visual)
	if solid:
		var body := StaticBody3D.new()
		body.name = "%sCollision" % label
		body.position = where
		parent_node.add_child(body)
		var collision := CollisionShape3D.new()
		var shape := BoxShape3D.new()
		shape.size = size
		collision.shape = shape
		body.add_child(collision)
	return visual

func _build_ground() -> void:
	# Four slabs leave an exact 3 x 3 m cage opening. No ground collider overlaps
	# the lift floor, so the player really stands on the moving AnimatableBody3D.
	_box("YardWest", Vector3(32.5, 0.8, 40.0), Vector3(-13.75, -0.4, -5.0), _concrete, true)
	_box("YardEast", Vector3(24.5, 0.8, 40.0), Vector3(17.75, -0.4, -5.0), _concrete, true)
	_box("YardNorth", Vector3(3.0, 0.8, 14.5), Vector3(4.0, -0.4, -17.75), _concrete, true)
	_box("YardSouth", Vector3(3.0, 0.8, 22.5), Vector3(4.0, -0.4, 3.75), _concrete, true)
	_box("LiftPitBottom", Vector3(3.0, 0.3, 3.0), Vector3(4.0, -1.8, -9.0), _dark, true)
	for side in [-1.0, 1.0]:
		_box("PerimeterRail", Vector3(0.12, 1.25, 40.0),
			Vector3(side * 29.9, 0.63, -5.0), _rust, true)
	_box("YardEndRail", Vector3(60.0, 1.25, 0.12), Vector3(0.0, 0.63, 14.9), _rust, true)
	_box("PumpLaneStripe", Vector3(0.10, 0.015, 10.0),
		Vector3(2.0, 0.012, -4.5), _amber)

func _build_tower() -> void:
	_box("TowerFace", Vector3(24.0, 25.0, 2.4), Vector3(3.0, 12.5, -23.0), _dark, true)
	for x in [-7.0, 1.0, 9.0]:
		_box("TowerRib", Vector3(0.85, 25.0, 1.0), Vector3(x, 12.5, -21.45), _rust)
	for y in [7.8, 15.8, 23.8]:
		_box("TowerBand", Vector3(24.0, 0.32, 0.7), Vector3(3.0, y, -21.4), _steel)
	_box("UpperDoor", Vector3(2.1, 3.3, 0.10), Vector3(9.0, 9.65, -21.72), _amber)

func _build_screw() -> void:
	# Raised basin: its visible free surface is the level the native head law reads.
	_box("BasinBase", Vector3(2.4, 0.18, 1.4), Vector3(0.0, 0.09, -2.8), _steel, true)
	for side in [-1.0, 1.0]:
		_box("BasinSide", Vector3(0.18, 1.45, 1.4),
			Vector3(side * 1.13, 0.72, -2.8), _steel, true)
		_box("BasinEnd", Vector3(2.4, 1.45, 0.18),
			Vector3(0.0, 0.72, -2.8 + side * 0.62), _steel, true)
	_basin_water = _box("BasinWater", Vector3(2.0, 1.0, 1.0),
		Vector3(0.0, 0.6, -2.8), _water)

	var axis := Node3D.new()
	axis.name = "InclinedScrewFrame"
	axis.position = (SCREW_LOW + SCREW_HIGH) * 0.5
	add_child(axis)
	axis.look_at(SCREW_LOW, Vector3.UP)
	_box("OpenTrough", Vector3(1.52, 0.18, SCREW_LENGTH),
		Vector3(0.0, -0.53, 0.0), _dark, true, axis)
	for side in [-1.0, 1.0]:
		_box("TroughRail", Vector3(0.10, 0.66, SCREW_LENGTH),
			Vector3(side * 0.73, -0.23, 0.0), _steel, true, axis)
	_rotor = Node3D.new()
	_rotor.name = "FlightAndShaft"
	axis.add_child(_rotor)
	var shaft := MeshInstance3D.new()
	shaft.name = "Shaft"
	var shaft_mesh := CylinderMesh.new()
	shaft_mesh.top_radius = 0.125
	shaft_mesh.bottom_radius = 0.125
	shaft_mesh.height = SCREW_LENGTH
	shaft.mesh = shaft_mesh
	shaft.material_override = _rust
	shaft.rotation.x = PI * 0.5
	_rotor.add_child(shaft)
	var flight := MeshInstance3D.new()
	flight.name = "ContinuousHelicalFlight"
	flight.mesh = _flight_mesh()
	flight.material_override = _amber
	_rotor.add_child(flight)
	for i in range(12):
		var packet := MeshInstance3D.new()
		packet.name = "CarriedWater%02d" % i
		var sphere := SphereMesh.new()
		sphere.radius = 0.11
		sphere.height = 0.22
		packet.mesh = sphere
		packet.material_override = _water
		axis.add_child(packet)
		_water_packets.append(packet)
	for s in [0.7, 5.5, 10.3]:
		var local := axis.to_global(Vector3(0.0, -0.68, -SCREW_LENGTH * 0.5 + s))
		_box("ScrewSupport", Vector3(1.75, maxf(0.55, local.y), 0.32),
			Vector3(local.x, maxf(0.55, local.y) * 0.5, local.z), _rust, true)
	_box("MotorHousing", Vector3(1.55, 1.15, 1.4), Vector3(0.0, 0.58, -0.75), _dark, true)
	_box("MotorCoupler", Vector3(0.65, 0.65, 1.45), Vector3(0.0, 0.72, -1.65), _amber)
	_box("PumpPedestal", Vector3(0.85, 1.6, 0.75), Vector3(3.5, 0.8, 0.5), _rust, true)
	_box("PumpControls", Vector3(1.05, 0.42, 0.75), Vector3(3.5, 1.72, 0.5), _amber)

func _flight_mesh() -> ArrayMesh:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	var steps := 550
	for i in range(steps):
		var s0 := SCREW_LENGTH * float(i) / float(steps)
		var s1 := SCREW_LENGTH * float(i + 1) / float(steps)
		var a := _flight_point(s0, 0.13)
		var b := _flight_point(s0, 0.60)
		var c := _flight_point(s1, 0.13)
		var d := _flight_point(s1, 0.60)
		for point in [a, b, c, b, d, c]:
			surface.add_vertex(point)
	surface.generate_normals()
	return surface.commit()

func _flight_point(distance: float, radius: float) -> Vector3:
	var phase := TAU * distance / SCREW_PITCH
	return Vector3(cos(phase) * radius, sin(phase) * radius,
		-SCREW_LENGTH * 0.5 + distance)

func _build_tank_and_bucket() -> void:
	# The water's output has a real support and a rigid outlet over the bucket.
	for x in [-1.25, 1.25]:
		for z in [-14.3, -11.7]:
			_box("TankColumn", Vector3(0.2, 5.5, 0.2),
				Vector3(x, 2.75, z), _rust, true)
	_box("TankFloor", Vector3(2.5, 0.2, 2.5), Vector3(0.0, 5.4, -13.0), _steel, true)
	for side in [-1.0, 1.0]:
		_box("TankGlassX", Vector3(0.09, 0.68, 2.2),
			Vector3(side * 1.08, 5.83, -13.0), _glass)
		_box("TankGlassZ", Vector3(2.2, 0.68, 0.09),
			Vector3(0.0, 5.83, -13.0 + side * 1.08), _glass)
	_tank_water = _box("TankWater", Vector3(2.0, 1.0, 2.0),
		Vector3(0.0, 5.5, -13.0), _water)
	_box("OutletPipe", Vector3(0.32, 0.30, 2.8), Vector3(0.0, 5.55, -14.7), _steel)
	_bucket = Node3D.new()
	_bucket.name = "WaterCounterweightBucket"
	_bucket.position = Vector3(0.0, 4.5, -16.0)
	add_child(_bucket)
	_box("BucketFloor", Vector3(1.8, 0.12, 1.8), Vector3(0.0, -0.42, 0.0), _steel, false, _bucket)
	for side in [-1.0, 1.0]:
		_box("BucketWallX", Vector3(0.12, 0.9, 1.8),
			Vector3(side * 0.84, 0.0, 0.0), _rust, false, _bucket)
		_box("BucketWallZ", Vector3(1.8, 0.9, 0.12),
			Vector3(0.0, 0.0, side * 0.84), _rust, false, _bucket)
	_bucket_water = _box("BucketWater", Vector3(1.6, 1.0, 1.6),
		Vector3(0.0, -0.39, 0.0), _water, false, _bucket)
	for x in [-1.15, 1.15]:
		_box("BucketGuide", Vector3(0.16, 10.5, 0.16),
			Vector3(x, 5.25, -16.0), _steel, true)

func _build_cage_and_dock() -> void:
	cage = AnimatableBody3D.new()
	cage.name = "PlayerLiftCage"
	cage.position = Vector3(4.0, -0.08, -9.0)
	add_child(cage)
	var floor_collision := CollisionShape3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(3.0, 0.16, 3.0)
	floor_collision.shape = floor_shape
	cage.add_child(floor_collision)
	_box("CageFloor", Vector3(3.0, 0.16, 3.0), Vector3.ZERO, _steel, false, cage)
	for x in [-1.35, 1.35]:
		for z in [-1.35, 1.35]:
			_box("CagePost", Vector3(0.12, 2.6, 0.12),
				Vector3(x, 1.3, z), _amber, false, cage)
	for side in [-1.0, 1.0]:
		_box("CageSideRail", Vector3(0.10, 0.12, 3.0),
			Vector3(side * 1.43, 1.12, 0.0), _amber, false, cage)
	_box("CageRelease", Vector3(0.42, 0.55, 0.25),
		Vector3(0.85, 1.0, -1.27), _amber, false, cage)
	for x in [2.35, 5.65]:
		for z in [-10.65, -7.35]:
			_box("LiftGuide", Vector3(0.2, 10.8, 0.2),
				Vector3(x, 5.4, z), _rust, true)
	_box("HeadFrame", Vector3(3.8, 0.26, 3.8), Vector3(4.0, 11.0, -9.0), _rust, true)
	_box("UpperLanding", Vector3(5.5, 0.24, 4.0), Vector3(8.25, 7.88, -9.0), _steel, true)
	for side in [-1.0, 1.0]:
		_box("DockRail", Vector3(5.5, 1.1, 0.14),
			Vector3(8.25, 8.55, -9.0 + side * 1.93), _amber, true)
	_box("DockEndRail", Vector3(0.14, 1.1, 4.0),
		Vector3(10.92, 8.55, -9.0), _amber, true)
	_box("ResetPedestal", Vector3(0.5, 1.45, 0.5),
		Vector3(8.0, 8.73, -9.0), _rust, true)
	_box("ResetButton", Vector3(0.65, 0.22, 0.65),
		Vector3(8.0, 9.52, -9.0), _amber)

func _build_rope() -> void:
	for i in range(3):
		var line := MeshInstance3D.new()
		line.name = "LoadRope%d" % i
		var cylinder := CylinderMesh.new()
		cylinder.top_radius = 0.035
		cylinder.bottom_radius = 0.035
		cylinder.height = 1.0
		line.mesh = cylinder
		line.material_override = _steel
		add_child(line)
		_rope_nodes.append(line)
	_box("SheaveBeam", Vector3(5.0, 0.3, 0.3), Vector3(2.0, 10.45, -12.5), _rust)

func _set_rope(line: MeshInstance3D, start: Vector3, end: Vector3) -> void:
	var vector := end - start
	var length := maxf(vector.length(), 0.01)
	line.position = (start + end) * 0.5
	line.quaternion = Quaternion(Vector3.UP, vector / length)
	var cylinder := line.mesh as CylinderMesh
	cylinder.height = length

func update_stage(stage: Object) -> void:
	var q := float(stage.call("get_cage_travel"))
	var angle := float(stage.call("get_shaft_angle"))
	cage.position.y = q - 0.08
	_bucket.position.y = 4.5 - q * 0.5
	_rotor.rotation.z = angle
	var basin_depth := clampf(float(stage.call("get_basin_water")) / 2.0, 0.0, 1.25)
	_basin_water.visible = basin_depth > 0.005
	_basin_water.scale.y = maxf(0.001, basin_depth)
	_basin_water.position.y = basin_depth * 0.5
	var tank_depth := clampf(float(stage.call("get_tank_water")) / 4.0, 0.0, 0.5)
	_tank_water.visible = tank_depth > 0.005
	_tank_water.scale.y = maxf(0.001, tank_depth)
	_tank_water.position.y = 5.5 + tank_depth * 0.5
	var bucket_depth := clampf(float(stage.call("get_bucket_water")) / 2.56, 0.0, 0.8)
	_bucket_water.visible = bucket_depth > 0.005
	_bucket_water.scale.y = maxf(0.001, bucket_depth)
	_bucket_water.position.y = -0.39 + bucket_depth * 0.5
	var flow := float(stage.call("get_screw_flow"))
	for i in range(_water_packets.size()):
		var packet := _water_packets[i]
		packet.visible = absf(flow) > 0.004
		var s := fposmod(float(i) * SCREW_LENGTH / float(_water_packets.size()) +
			angle / TAU * SCREW_PITCH, SCREW_LENGTH)
		packet.position = _flight_point(s, 0.43)
	_set_rope(_rope_nodes[0], Vector3(0.0, 10.3, -16.0),
		Vector3(0.0, _bucket.position.y + 0.5, -16.0))
	_set_rope(_rope_nodes[1], Vector3(3.3, 10.3, -9.0),
		Vector3(3.3, cage.position.y + 2.6, -9.0))
	_set_rope(_rope_nodes[2], Vector3(4.7, 10.3, -9.0),
		Vector3(4.7, cage.position.y + 2.6, -9.0))
