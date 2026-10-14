extends Node3D
## The yard crew, drawn: what the native's workers (sim/crew.hpp) are doing,
## nothing they are not. Each worker is a figure of simple parts -- boots,
## trousers, a hi-vis vest, sleeves, hands, a head and a hard hat -- posed
## every frame from its native state: legs and arms swing with the metres it
## has walked; at a station it works (a hammering arm, head down); stepping
## aside it watches the player; startled, its arms come up and it leans back;
## shoved, its arms fly out.
##
## Every part type is one MultiMesh across the whole crew: eleven draw calls
## (four of them cast shadows) however many workers there are.

const WORKER_FLOATS := 8
const MAX_WORKERS := 8
const WALKING := 0
const WORKING := 1
const YIELDING := 2
const STARTLED := 3
const STUMBLING := 4

# Heights above the soles, metres.
const HIP_Y := 0.92
const SHOULDER_Y := 1.44
const SHOULDER_HALF := 0.2
const HIP_HALF := 0.1
const THIGH := 0.44
const SHIN := 0.44
const UPPER_ARM := 0.3
const FOREARM := 0.27
const STEP_M := 0.75   # a stride: a full leg cycle is two of them
const DRAW_RANGE_M := 160.0

var _native: Object
var _parts := {}         # name -> MultiMesh
var _clock := 0.0
# The native's capsule centre is its stand height above the soles.
const CENTRE_Y := 0.9

func setup(native: Object) -> void:
	_native = native
	var skin := _material(Color("b07a5a"), 0.75)
	var trousers := _material(Color("2a3140"), 0.9)
	var vest := _material(Color("e8611a"), 0.7, Color("e8611a"), 0.08)
	var stripe := _material(Color("d8dadc"), 0.3, Color("d8dadc"), 0.25)
	var sleeve := _material(Color("3a3d40"), 0.9)
	var boot := _material(Color("2b1f16"), 0.8)
	var hat := _material(Color("f2c418"), 0.35)
	_add_part(&"boot", _box(Vector3(0.12, 0.1, 0.28)), boot, 2, false)
	_add_part(&"shin", _cylinder(0.062, 0.07, SHIN), trousers, 2, true)
	_add_part(&"thigh", _cylinder(0.075, 0.085, THIGH), trousers, 2, true)
	_add_part(&"hips", _box(Vector3(0.34, 0.2, 0.2)), trousers, 1, true)
	_add_part(&"torso", _box(Vector3(0.4, 0.52, 0.23)), vest, 1, true)
	_add_part(&"stripe", _box(Vector3(0.405, 0.05, 0.235)), stripe, 1, false)
	_add_part(&"upper_arm", _cylinder(0.055, 0.06, UPPER_ARM), sleeve, 2, false)
	_add_part(&"forearm", _cylinder(0.048, 0.055, FOREARM), sleeve, 2, false)
	_add_part(&"hand", _box(Vector3(0.08, 0.1, 0.05)), skin, 2, false)
	_add_part(&"head", _sphere(0.105), skin, 1, false)
	_add_part(&"hat", _hat_mesh(), hat, 1, false)


func update(delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	if _native == null:
		return
	_clock += delta
	var data: PackedFloat32Array = _native.get_workers()
	var count := mini(data.size() / WORKER_FLOATS, MAX_WORKERS)
	for w in MAX_WORKERS:
		if w >= count:
			_hide(w)
			continue
		var b := w * WORKER_FLOATS
		var centre := Vector3(data[b], data[b + 1], data[b + 2])
		if centre.distance_to(camera_position) > DRAW_RANGE_M:
			_hide(w)
			continue
		_pose(w, centre - Vector3(0.0, CENTRE_Y, 0.0), data[b + 3], int(data[b + 4]), data[b + 5],
			data[b + 6], player_position)


# Every figure in the crew, for a test: [feet, yaw, state] each.
func figures() -> Array:
	var out := []
	if _native == null:
		return out
	var data: PackedFloat32Array = _native.get_workers()
	for w in mini(data.size() / WORKER_FLOATS, MAX_WORKERS):
		var b := w * WORKER_FLOATS
		out.append([Vector3(data[b], data[b + 1] - CENTRE_Y, data[b + 2]), data[b + 3], int(data[b + 4])])
	return out


func _pose(w: int, feet: Vector3, yaw: float, state: int, speed: float, stride: float,
		player: Vector3) -> void:
	var body := Transform3D(Basis(Vector3.UP, yaw), feet)
	var gait := clampf(speed / 1.3, 0.0, 1.0) if state != WORKING else 0.0
	var phase := stride / STEP_M * PI
	var lean := 0.0          # forward (+) / back (-), radians
	var side_lean := 0.0
	var head_pitch := 0.0
	var arm := [Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0)]   # per side: pitch, roll, elbow
	var personal := float(w) * 1.7
	match state:
		WORKING:
			lean = 0.18
			head_pitch = 0.35
			# The right arm works -- a hammer's rise and strike -- the left holds.
			var beat := sin(_clock * 5.5 + personal)
			arm[0] = Vector3(-0.9 - 0.55 * maxf(beat, 0.0), 0.15, 1.2 - 0.5 * beat)
			arm[1] = Vector3(-0.7, -0.1, 1.0)
		YIELDING:
			arm[0] = Vector3(0.1, 0.12, 0.25)
			arm[1] = Vector3(0.1, -0.12, 0.25)
		STARTLED:
			lean = -0.22
			head_pitch = -0.1
			arm[0] = Vector3(-1.9, 0.35, 1.5)
			arm[1] = Vector3(-1.9, -0.35, 1.5)
		STUMBLING:
			lean = 0.3 * sin(personal)
			side_lean = 0.25 * cos(personal)
			arm[0] = Vector3(-0.4, 1.3, 0.3)
			arm[1] = Vector3(-0.6, -1.2, 0.4)
		_:
			# Walking: arms swing against the legs.
			arm[0] = Vector3(0.38 * gait * sin(phase), 0.08, 0.35)
			arm[1] = Vector3(-0.38 * gait * sin(phase), -0.08, 0.35)
			lean = 0.06 * gait
	# A slight bob with each step.
	var bob := absf(sin(phase)) * 0.025 * gait
	var hip_pos := Vector3(0.0, HIP_Y + bob, 0.0)
	var pelvis := Basis(Vector3.RIGHT, -lean * 0.3) * Basis(Vector3.BACK, side_lean * 0.3)
	_place(&"hips", w, 0, body * Transform3D(pelvis, hip_pos + Vector3(0.0, 0.03, 0.0)))
	# Legs: thigh from the hip, shin from the knee, boot at the ankle.
	for side in 2:
		var s := 1.0 if side == 0 else -1.0
		var swing := 0.42 * gait * sin(phase + (0.0 if side == 0 else PI))
		var knee_bend := 0.7 * gait * maxf(0.0, sin(phase + (0.0 if side == 0 else PI) + 1.2))
		if state == STARTLED:
			swing = -0.15 if side == 0 else 0.1
		var hip_joint := hip_pos + Vector3(-HIP_HALF * s, -0.04, 0.0)
		var thigh_basis := pelvis * Basis(Vector3.RIGHT, -swing)
		var knee := hip_joint + thigh_basis * Vector3(0.0, -THIGH, 0.0)
		var shin_basis := thigh_basis * Basis(Vector3.RIGHT, knee_bend)
		var ankle := knee + shin_basis * Vector3(0.0, -SHIN, 0.0)
		# Keep the standing foot on the ground (the walk's lowest ankle).
		_place(&"thigh", w, side, body * _segment(hip_joint, thigh_basis, THIGH))
		_place(&"shin", w, side, body * _segment(knee, shin_basis, SHIN))
		var foot_basis := Basis(Vector3.RIGHT, -swing * 0.3)
		_place(&"boot", w, side, body * Transform3D(foot_basis, ankle + Vector3(0.0, -0.03, 0.06)))
	# Torso on the pelvis, leaning.
	var spine := pelvis * Basis(Vector3.RIGHT, lean) * Basis(Vector3.BACK, side_lean)
	var chest := hip_pos + spine * Vector3(0.0, 0.32, 0.0)
	_place(&"torso", w, 0, body * Transform3D(spine, chest))
	_place(&"stripe", w, 0, body * Transform3D(spine, chest + spine * Vector3(0.0, -0.06, 0.0)))
	var neck := hip_pos + spine * Vector3(0.0, SHOULDER_Y - HIP_Y + 0.08, 0.0)
	# The head looks at the player when it has reason to.
	var head_basis := spine * Basis(Vector3.RIGHT, head_pitch)
	if state == YIELDING or state == STARTLED:
		var to := body.affine_inverse() * player - neck
		var look_yaw := clampf(atan2(to.x, to.z), -0.9, 0.9)
		head_basis = spine * Basis(Vector3.UP, look_yaw) * Basis(Vector3.RIGHT, head_pitch)
	var head := neck + head_basis * Vector3(0.0, 0.14, 0.0)
	_place(&"head", w, 0, body * Transform3D(head_basis, head))
	_place(&"hat", w, 0, body * Transform3D(head_basis, head + head_basis * Vector3(0.0, 0.06, 0.0)))
	# Arms: from each shoulder, pitch forward (-) / back (+), roll out, elbow.
	for side in 2:
		var s := 1.0 if side == 0 else -1.0
		var a: Vector3 = arm[side]
		var shoulder := hip_pos + spine * Vector3(-SHOULDER_HALF * s, SHOULDER_Y - HIP_Y, 0.0)
		var upper_basis := spine * Basis(Vector3.RIGHT, a.x) * Basis(Vector3.BACK, -a.y * s)
		var elbow := shoulder + upper_basis * Vector3(0.0, -UPPER_ARM, 0.0)
		var fore_basis := upper_basis * Basis(Vector3.RIGHT, -a.z)
		var wrist := elbow + fore_basis * Vector3(0.0, -FOREARM, 0.0)
		_place(&"upper_arm", w, side, body * _segment(shoulder, upper_basis, UPPER_ARM))
		_place(&"forearm", w, side, body * _segment(elbow, fore_basis, FOREARM))
		_place(&"hand", w, side, body * Transform3D(fore_basis, wrist + fore_basis * Vector3(0.0, -0.05, 0.0)))


# A part hanging from `joint` along the basis's -y, `length` long.
func _segment(joint: Vector3, basis: Basis, length: float) -> Transform3D:
	return Transform3D(basis, joint + basis * Vector3(0.0, -length * 0.5, 0.0))


func _place(part: StringName, w: int, side: int, transform: Transform3D) -> void:
	var mm: MultiMesh = _parts[part]
	var per := mm.instance_count / MAX_WORKERS
	mm.set_instance_transform(w * per + side, transform)


func _hide(w: int) -> void:
	for part in _parts:
		var mm: MultiMesh = _parts[part]
		var per := mm.instance_count / MAX_WORKERS
		for side in per:
			mm.set_instance_transform(w * per + side, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))


func _add_part(part: StringName, mesh: Mesh, material: Material, per_worker: int, shadows: bool) -> void:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	mm.instance_count = MAX_WORKERS * per_worker
	for i in mm.instance_count:
		mm.set_instance_transform(i, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))
	var instance := MultiMeshInstance3D.new()
	instance.name = "Crew_%s" % part
	instance.multimesh = mm
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if shadows else \
		GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# The crew works over the yard; its parts' bounds are the yard.
	instance.custom_aabb = AABB(Vector3(-80, -2, -200), Vector3(160, 10, 260))
	add_child(instance)
	_parts[part] = mm


func _material(color: Color, roughness: float, emission: Color = Color.BLACK,
		emission_energy: float = 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	if emission_energy > 0.0:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
	return material


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


func _cylinder(top: float, bottom: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top
	mesh.bottom_radius = bottom
	mesh.height = height
	mesh.radial_segments = 8
	mesh.rings = 1
	return mesh


func _sphere(radius: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.15
	mesh.radial_segments = 12
	mesh.rings = 6
	return mesh


# A hard hat: a dome with a peak to the front.
func _hat_mesh() -> ArrayMesh:
	var dome := SphereMesh.new()
	dome.radius = 0.125
	dome.height = 0.16
	dome.is_hemisphere = true
	dome.radial_segments = 12
	dome.rings = 4
	var st := SurfaceTool.new()
	st.append_from(dome, 0, Transform3D(Basis(), Vector3(0.0, -0.02, 0.0)))
	var brim := CylinderMesh.new()
	brim.top_radius = 0.15
	brim.bottom_radius = 0.15
	brim.height = 0.012
	brim.radial_segments = 12
	st.append_from(brim, 0, Transform3D(Basis().scaled(Vector3(1.0, 1.0, 1.15)), Vector3(0.0, -0.02, 0.02)))
	return st.commit()
