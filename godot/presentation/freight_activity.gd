extends Node3D

# Two opaque instanced warning fittings. Motion is sampled from authoritative
# native tick poses, never interpolated render displacement or input commands.
# They indicate movement only, with no readiness/contact/physics authority.
const HOLD_SECONDS := 1.6
const MOTION_MPS := 0.05
const NATIVE_HZ := 90.0
const RANGE_METERS := 160.0
const SPECS := [
	{"mount": 2440, "point": Vector3(0.48, 1.97, -1.55), "motion": [2440, 2441, 2442]},
	{"mount": 2452, "point": Vector3(0.0, 0.085, -0.25), "motion": [2451, 2452, 2453]},
]
var _native: Object
var _slots: Array[Dictionary] = []
var _previous: Dictionary = {}
var _last_tick := -1
var _last_deaths := -1
var _clock := 0.0
var _bases: MultiMesh
var _lenses: MultiMesh

func setup(native: Object) -> void:
	_native = native
	for spec in SPECS:
		var mount := int(native.get_kit_body_index(spec.mount))
		if mount < 0:
			continue
		var motion: Array[int] = []
		for entity in spec.motion:
			var index := int(native.get_kit_body_index(entity))
			if index >= 0:
				motion.append(index)
		_slots.append({"mount": mount, "point": spec.point, "motion": motion, "hold": 0.0})
	var base := CylinderMesh.new()
	base.top_radius = 0.12
	base.bottom_radius = 0.14
	base.height = 0.05
	base.radial_segments = 12
	var mounting := StandardMaterial3D.new()
	mounting.albedo_color = Color("252b2c")
	mounting.metallic = 0.65
	mounting.roughness = 0.55
	_bases = _batch("BeaconMounts", base, mounting)
	var dome := SphereMesh.new()
	dome.radius = 0.105
	dome.height = 0.16
	dome.radial_segments = 16
	dome.rings = 6
	var lens := ShaderMaterial.new()
	lens.shader = preload("res://presentation/materials/freight_beacon.gdshader")
	_lenses = _batch("MotionLenses", dome, lens)

func _batch(label: String, mesh: Mesh, material: Material) -> MultiMesh:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_custom_data = true
	mm.mesh = mesh
	mm.instance_count = _slots.size()
	var instance := MultiMeshInstance3D.new()
	instance.name = label
	instance.multimesh = mm
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# Fixed whole-yard bounds; per-instance range is checked before drawing.
	instance.custom_aabb = AABB(Vector3(-250, -5, -300), Vector3(500, 100, 500))
	add_child(instance)
	return mm

func reset() -> void:
	_previous.clear()
	_last_tick = -1
	_last_deaths = -1
	_clock = 0.0
	for slot in _slots:
		slot.hold = 0.0
	if _lenses != null:
		for i in _slots.size():
			_lenses.set_instance_custom_data(i, Color(0, 0, 0, 0))

func active_count() -> int:
	var count := 0
	for slot in _slots:
		if float(slot.hold) > 0.0:
			count += 1
	return count

func update_from_native(delta: float, eye: Vector3, reduced_motion: bool) -> void:
	if _native == null or _bases == null:
		return
	var tick := int(_native.get_tick_index())
	var deaths := int(_native.get_death_count())
	var discontinuity := _last_tick < 0 or tick < _last_tick or deaths != _last_deaths
	# Restore/restart samples must seed a new interval; a teleported body is
	# not a machine that swept through the space between its old/new poses.
	if discontinuity:
		reset()
	var tick_delta := tick - _last_tick
	var advanced := not discontinuity and tick_delta > 0
	var interval := float(tick_delta) / NATIVE_HZ if advanced else 0.0
	var poses: Dictionary = {}
	for slot in _slots:
		for body in slot.motion:
			if not poses.has(body):
				poses[body] = _native.get_kit_body_transform(body)
		var moving := false
		if advanced:
			for body in slot.motion:
				if not _previous.has(body):
					continue
				var now: Transform3D = poses[body]
				var before: Transform3D = _previous[body]
				# The origin plus a one-metre arm detects both translation and
				# rotation. Each is an observation of the same actual body.
				var displacement := maxf(now.origin.distance_to(before.origin),
					(now * Vector3.FORWARD).distance_to(before * Vector3.FORWARD))
				if displacement / interval > MOTION_MPS:
					moving = true
		var age := maxf(delta, 0.0) if is_finite(delta) else 0.0
		slot.hold = HOLD_SECONDS if moving else maxf(0.0, float(slot.hold) - age)
	if discontinuity or advanced:
		_previous = poses
		_last_tick = tick
		_last_deaths = deaths
	_clock += clampf(delta, 0.0, 0.1) if is_finite(delta) else 0.0
	for i in _slots.size():
		var slot := _slots[i]
		var frame: Transform3D = _native.get_kit_body_render_transform(slot.mount)
		var point: Vector3 = frame * slot.point
		var shown := bool(_native.is_kit_body_enabled(slot.mount)) and point.distance_squared_to(eye) < RANGE_METERS * RANGE_METERS
		var basis := frame.basis if shown else Basis().scaled(Vector3.ZERO)
		_bases.set_instance_transform(i, Transform3D(basis, point))
		_lenses.set_instance_transform(i, Transform3D(basis, point + frame.basis.y * 0.09))
		# A gentle one-Hz warning cycle; reduced motion keeps a steady light.
		var brightness := (1.0 if reduced_motion else 0.55 + 0.45 * sin(_clock * TAU)) if float(slot.hold) > 0.0 else 0.0
		_lenses.set_instance_custom_data(i, Color(brightness, 0, 0, 0))
