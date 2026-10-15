extends Node3D
## Small noncolliding worksite dressing on native fixed cable frame 1905.
## Main owns updates; this helper never changes the simulation or environment.

const FRAME_ENTITY := 1905
const CULL_DISTANCE := 40.0
const CLOTH_SEGMENTS := 6
# Source: split_inspection_route.cpp cable_frame mast and south stand leg.
# Body1905 is authored at world zero; these are its actual local steel points.
const ANCHORS := [Vector3(-21.05, 471.35, -135.5), Vector3(10.0, 484.65, -135.05)]

var _fixtures: Array[Node3D] = []
var _lights: Array[SpotLight3D] = []
var _cloth: Array[MeshInstance3D] = []
var _neutral := [true, true]
var _phase := 0.0
var _cloth_elapsed := 0.0
var _steel: StandardMaterial3D
var _bulb: StandardMaterial3D

func _ready() -> void:
	_build()

func update_from_native(native: Object, delta: float, reduced_motion: bool = false, hour: float = 10.5) -> void:
	if _fixtures.is_empty():
		_build()
	var frame := int(native.get_kit_body_index(FRAME_ENTITY))
	visible = frame >= 0
	if not visible:
		return
	global_transform = native.get_kit_body_transform(frame)
	var player: Vector3 = native.get_player_position()
	var near := false
	for index in 2:
		var fixture := _fixtures[index]
		fixture.visible = player.distance_squared_to(global_transform * ANCHORS[index]) < CULL_DISTANCE * CULL_DISTANCE
		near = near or fixture.visible
	if not near:
		return
	# Warm practical bulbs remain dim in daylight; dawn/dusk blend smoothly.
	var wrapped_hour := fposmod(hour, 24.0)
	var day := smoothstep(5.5, 8.0, wrapped_hour) * (1.0 - smoothstep(17.0, 19.5, wrapped_hour))
	for light in _lights:
		light.light_energy = lerpf(1.65, 0.22, day)
	_bulb.emission_energy_multiplier = lerpf(1.5, 0.35, day)
	if reduced_motion:
		for index in 2:
			if not _neutral[index]:
				_write_cloth(index, false)
		return
	var step := clampf(delta, 0.0, 0.1)
	_phase += step
	_cloth_elapsed += step
	if _cloth_elapsed < 1.0 / 30.0:
		return
	_cloth_elapsed = 0.0
	for index in 2:
		if _fixtures[index].visible:
			_write_cloth(index, true)

func _build() -> void:
	if not _fixtures.is_empty():
		return
	visible = false
	_steel = _material(Color(0.17, 0.19, 0.20), 0.65)
	_bulb = _material(Color(1.0, 0.71, 0.33), 0.1)
	_bulb.emission_enabled = true
	_bulb.emission = Color(1.0, 0.57, 0.20)
	var fabric := _material(Color(0.72, 0.29, 0.10), 0.0)
	fabric.cull_mode = BaseMaterial3D.CULL_DISABLED
	for index in 2:
		var fixture := Node3D.new()
		fixture.name = "LowerMastPractical" if index == 0 else "UpperStandPractical"
		add_child(fixture)
		fixture.position = ANCHORS[index]
		_fixtures.append(fixture)
		# The short bracket begins inside the existing steel face. Lower lamp
		# faces the guide/underside, not the ballast top through its solid floor.
		var outward := Vector3.RIGHT if index == 0 else Vector3.FORWARD
		_box(fixture, Vector3(0.19, 0.13, 0.20), outward * 0.04, _steel)
		_rod(fixture, Vector3.ZERO, outward * 0.26, 0.018)
		var lamp := Node3D.new()
		fixture.add_child(lamp)
		lamp.position = outward * 0.29
		var aim := Vector3(1.0, -0.35, 0.40) if index == 0 else Vector3(-0.6, -1.0, 0.7)
		lamp.basis = Basis.looking_at(aim.normalized(), Vector3.UP)
		_box(lamp, Vector3(0.24, 0.28, 0.055), Vector3.ZERO, _steel)
		_box(lamp, Vector3(0.17, 0.21, 0.025), Vector3(0.0, 0.0, -0.04), _bulb)
		# Two cage rails and three cross bars protect the recessed lens.
		for x in [-0.105, 0.105]:
			_rod(lamp, Vector3(x, -0.13, -0.075), Vector3(x, 0.13, -0.075), 0.009)
		for y in [-0.12, 0.0, 0.12]:
			_rod(lamp, Vector3(-0.105, y, -0.075), Vector3(0.105, y, -0.075), 0.009)
		var light := SpotLight3D.new()
		light.name = "WarmTaskLight"
		lamp.add_child(light)
		light.position.z = -0.09
		light.light_color = Color(1.0, 0.72, 0.43)
		light.shadow_enabled = false
		light.spot_range = 6.0
		light.spot_angle = 48.0
		light.spot_angle_attenuation = 1.5
		_lights.append(light)
		var cloth := MeshInstance3D.new()
		cloth.name = "PinnedFabricTelltale"
		fixture.add_child(cloth)
		cloth.position = Vector3(-0.095, 0.30, 0.0) if index == 0 else Vector3(0.0, 0.18, -0.105)
		# Local +X extends away from the mast/stand, outside the walking line.
		cloth.rotation.y = PI if index == 0 else PI * 0.5
		cloth.mesh = ArrayMesh.new()
		cloth.material_override = fabric
		cloth.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		_cloth.append(cloth)
		_box(fixture, Vector3(0.035, 0.16, 0.035), cloth.position, _steel)
		_write_cloth(index, false)

func _write_cloth(index: int, moving: bool) -> void:
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	var indices := PackedInt32Array()
	for segment in CLOTH_SEGMENTS + 1:
		var t := float(segment) / CLOTH_SEGMENTS
		# Shared slow gust with a small traveling ripple. Deflection is zero
		# at the complete pinned root edge, growing quadratically at the tip.
		var wave := 0.0
		if moving:
			wave = t * t * (0.055 * sin(_phase * 1.7 - t * 2.2 + index * 0.35)
				+ 0.014 * sin(_phase * 4.1 - t * 5.0))
		for edge in 2:
			vertices.append(Vector3(t * 0.48, (float(edge) - 0.5) * 0.13 - t * t * 0.08, wave))
			normals.append(Vector3(0.0, 0.0, 1.0))
		if segment < CLOTH_SEGMENTS:
			var base := segment * 2
			indices.append_array(PackedInt32Array([base, base + 2, base + 1, base + 1, base + 2, base + 3]))
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = vertices
	arrays[Mesh.ARRAY_NORMAL] = normals
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := _cloth[index].mesh as ArrayMesh
	mesh.clear_surfaces()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	_neutral[index] = not moving

func _material(color: Color, metallic: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = 0.7
	return material

func _box(parent: Node3D, size: Vector3, center: Vector3, material: Material) -> void:
	var instance := MeshInstance3D.new()
	var mesh := BoxMesh.new()
	mesh.size = size
	instance.mesh = mesh
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(instance)
	instance.position = center

func _rod(parent: Node3D, first: Vector3, last: Vector3, radius: float) -> void:
	var instance := MeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = first.distance_to(last)
	mesh.radial_segments = 6
	mesh.rings = 0
	instance.mesh = mesh
	instance.material_override = _steel
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(instance)
	instance.position = (first + last) * 0.5
	instance.quaternion = Quaternion(Vector3.UP, (last - first).normalized())
