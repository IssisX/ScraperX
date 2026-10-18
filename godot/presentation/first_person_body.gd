extends Node3D

# The climber's own body below the arms, as the player sees it looking down:
# hips, belly and chest in the work jacket, trousers, boots. It stands on the
# native capsule's soles, turns with the view, and its legs do what the
# native body is doing -- a stride paced by the ground covered, the knees
# taking a crouch, the legs trailing in the air, out in front in a slide,
# hanging under a grip. Pure presentation: nothing here is read back.
#
# The proportions are the native's (sim/corpse.hpp kPlan and the joints'
# heights over the soles), the same body a death leaves. The arms are the
# viewmodel's (first_person_arms.gd); this body has none, and no head: the
# eye is in it. The chest stops short of the shoulders, the torso is as deep
# as a body (not round), and the spine is EYE_AHEAD behind the eye, so looking
# straight down the lens is over the chest's front, never inside it, and the
# boots show past the belly.

const Gait := preload("res://presentation/gait.gd")
const HIP_Y := 0.92
const KNEE_Y := 0.50
const SPINE_Y := 1.07
const HIP_HALF_SPAN := 0.10
const THIGH := HIP_Y - KNEE_Y
const SHIN := KNEE_Y - 0.06
const ANKLE_Y := 0.06
# The chest's top, short of the shoulders (sim kShoulderY 1.43): 0.27 m
# under a standing eye.
const CHEST_TOP_Y := 1.25
# The eye sits this far ahead of the spine.
const EYE_AHEAD := 0.2
# The torso's flattening: wider than it is deep.
const CHEST_SHAPE := Vector3(1.3, 1.0, 0.78)
const BELLY_SHAPE := Vector3(1.25, 1.0, 0.82)
# The native capsule's half heights (kPlayerHalfHeight, crouched).
const STAND_HALF_HEIGHT := 0.9
const CROUCH_HALF_HEIGHT := 0.6

const TRAVERSAL_NONE := 0
const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3
const TRAVERSAL_CLIMBING := 4
const TRAVERSAL_LOWERING := 5
const TRAVERSAL_SWINGING := 6

var _root: Node3D           # at the soles, turned with the view
var _pelvis: MeshInstance3D
var _belly: MeshInstance3D
var _chest: MeshInstance3D
var _band: MeshInstance3D
var _legs := []             # per side: [thigh, shin, boot, sole]
var _stride := 0.0          # the gait's phase, in cycles of two footfalls
var _feet := [Vector3.ZERO, Vector3.ZERO]   # where each foot is, body space
var _hip_drop := 0.0        # the hips lowered (crouch), eased
var _lean := 0.0            # the torso pitched forward (+) or back (-), eased
var _sway := 0.0            # the body's own clock for hanging and kicking legs
# For a test: what the body last showed.
var shown_frames := 0
var foot_world := [Vector3.ZERO, Vector3.ZERO]
var pelvis_world := Vector3.ZERO


func build(jacket: Material, band: Material, trousers: Material, boots: Material, soles: Material) -> void:
	name = "FirstPersonBody"
	_root = Node3D.new()
	add_child(_root)
	_pelvis = _capsule(0.12, 0.34, trousers)
	_belly = _capsule(0.135, 0.30, jacket)
	_chest = _capsule(0.14, 0.30, jacket)
	# The hi-vis band round the jacket's hem.
	var hem := CylinderMesh.new()
	hem.top_radius = 0.137
	hem.bottom_radius = 0.137
	hem.height = 0.05
	hem.radial_segments = 20
	_band = MeshInstance3D.new()
	_band.mesh = hem
	_band.material_override = band
	_root.add_child(_band)
	for side in 2:
		var thigh := _capsule(0.075, THIGH + 0.06, trousers)
		var shin := _capsule(0.06, SHIN, trousers)
		# The boot: a rounded leather upper, toe to heel, on a rubber sole.
		var boot := _capsule(0.058, 0.29, boots)
		var sole_mesh := BoxMesh.new()
		sole_mesh.size = Vector3(0.105, 0.03, 0.29)
		var sole := MeshInstance3D.new()
		sole.mesh = sole_mesh
		sole.material_override = soles
		_root.add_child(sole)
		_legs.append([thigh, shin, boot, sole])
	_feet = [Vector3(-HIP_HALF_SPAN, 0.0, 0.0), Vector3(HIP_HALF_SPAN, 0.0, 0.0)]


func _capsule(radius: float, length: float, material: Material) -> MeshInstance3D:
	var shape := CapsuleMesh.new()
	shape.radius = radius
	shape.height = maxf(length, radius * 2.0)
	shape.radial_segments = 18
	shape.rings = 6
	var node := MeshInstance3D.new()
	node.mesh = shape
	node.material_override = material
	_root.add_child(node)
	return node


# One frame: `state` holds the native body's position (its capsule's
# centre), velocity, grounded, crouched, sliding, traversal and whether it
# rides a line; `yaw` the view's.
func update_body(state: Dictionary, yaw: float, delta: float) -> void:
	if not visible:
		return
	shown_frames += 1
	var centre: Vector3 = state["position"]
	var velocity: Vector3 = state["velocity"]
	var crouched := bool(state["crouched"])
	var grounded := bool(state["grounded"])
	var sliding := bool(state["sliding"])
	var traversal := int(state["traversal"])
	var on_line := bool(state.get("on_line", false))
	var hanging := on_line or traversal in [TRAVERSAL_HANGING, TRAVERSAL_SWINGING, TRAVERSAL_LOWERING, TRAVERSAL_CLIMBING]
	var soles := centre - Vector3(0.0, CROUCH_HALF_HEIGHT if crouched else STAND_HALF_HEIGHT, 0.0)
	var basis := Basis(Vector3.UP, yaw)
	# The spine a hand behind the eye.
	var back := basis * Vector3(0.0, 0.0, EYE_AHEAD)
	_root.global_transform = Transform3D(basis, soles + back)
	# Velocity in the body's frame: x right, z back.
	var local_v := basis.inverse() * velocity
	var flat := Vector2(local_v.x, local_v.z)
	var speed := flat.length()
	var ease := 1.0 - exp(-10.0 * delta)
	# Crouched the eye is 0.95 m up: a deep squat, the hips down by the
	# knees and the chest under the eye, not in it.
	_hip_drop = lerpf(_hip_drop, (0.55 if crouched else 0.0) + (0.12 if sliding else 0.0), ease)
	var lean_goal := 0.0
	if sliding:
		lean_goal = -0.35
	elif crouched:
		lean_goal = 0.45
	elif grounded:
		lean_goal = clampf(speed * 0.03, 0.0, 0.25)
	elif hanging:
		lean_goal = -0.05
	_lean = lerpf(_lean, lean_goal, ease)
	var hip := Vector3(0.0, HIP_Y - _hip_drop, 0.0)
	# The gait (presentation/gait.gd): two footfalls a cycle at the cadence
	# the speed asks. Each foot is down for the gait's contact time, sliding
	# back under the hip exactly as far as the body passes over it, and the
	# rest of the cycle swings through ahead.
	if grounded and not sliding and speed >= 0.2:
		_stride += 0.5 * Gait.cadence(speed) * delta
	var phase := _stride
	var stance := clampf(Gait.contact_seconds(speed) * Gait.cadence(speed) * 0.5, 0.15, 0.65)
	var step := maxf(speed * Gait.contact_seconds(speed), 0.35)
	for side in 2:
		var x := -HIP_HALF_SPAN if side == 0 else HIP_HALF_SPAN
		var goal := Vector3(x, 0.0, 0.0)
		if sliding:
			# Out in front, heels down, skidding.
			goal = Vector3(x * 1.4, 0.12 + 0.03 * side, -0.75)
		elif hanging:
			# Hanging under the grip: straight down, swaying a little.
			_sway += delta
			var sway := sin(_sway * 2.1 + float(side) * 2.1) * 0.06
			goal = Vector3(x, hip.y - THIGH - SHIN + 0.06, sway - (0.18 if traversal == TRAVERSAL_CLIMBING else 0.0))
			if traversal == TRAVERSAL_CLIMBING:
				# Feet on the face climbed, stepping up it in turn.
				goal.y += 0.18 * maxf(0.0, sin(phase * TAU + float(side) * PI)) + 0.1
		elif traversal in [TRAVERSAL_MANTLING, TRAVERSAL_VAULTING]:
			# Knees up over the lip or the rail.
			goal = Vector3(x, hip.y - 0.42, -0.25)
		elif not grounded:
			# In the air: knees a little bent, feet trailing the way the body
			# goes; falling hard, kicking.
			var trail := Vector3(-local_v.x, 0.0, -local_v.z) * 0.03
			goal = Vector3(x, hip.y - THIGH - SHIN * 0.8, 0.08) + trail.limit_length(0.3)
			if velocity.y < -9.0:
				_sway += delta
				goal += Vector3(0.0, 0.12 * sin(_sway * 20.0 + float(side) * PI), 0.0)
		else:
			# Planted, or striding: in stance the foot keeps to the ground and
			# slides back under the hip as the body passes over it; in the
			# swing it lifts and comes through ahead.
			var p := fposmod(phase + 0.5 * float(side), 1.0)
			# The way the body is going, in its own frame (-z its front).
			var dir := Vector2(0.0, -1.0) if speed < 0.2 else flat / speed
			var along := 0.0
			var lift := 0.0
			if speed >= 0.2:
				if p < stance:
					along = lerpf(0.5, -0.5, p / stance) * step
				else:
					var s := (p - stance) / (1.0 - stance)
					along = lerpf(-0.5, 0.5, s) * step
					lift = sin(s * PI) * clampf(0.08 + 0.03 * speed, 0.08, 0.3)
			# A crouched foot sits a little wider, and both forward under the
			# knees, the hips back over the heels.
			goal = Vector3(x * (1.25 if crouched else 1.0) + dir.x * along, lift,
				(-0.28 if crouched else 0.0) + dir.y * along)
		# Feet move like feet: quickly, never in one frame.
		_feet[side] = (_feet[side] as Vector3).lerp(goal, 1.0 - exp(-24.0 * delta))
		_pose_leg(side, Vector3(x, hip.y, 0.0), _feet[side])
	# The torso: pelvis across the hips, the belly and chest up the spine,
	# leaning from the hips.
	var tilt := Basis(Vector3.RIGHT, -_lean)
	_pelvis.position = hip
	_pelvis.basis = Basis(Vector3.FORWARD, PI * 0.5) * Basis.from_scale(Vector3(1.0, 1.0, 0.85))
	var spine_up := tilt * Vector3.UP
	_belly.position = hip + spine_up * (SPINE_Y - HIP_Y - 0.03)
	_belly.basis = tilt * Basis.from_scale(BELLY_SHAPE)
	_chest.position = hip + spine_up * (CHEST_TOP_Y - HIP_Y - 0.09)
	_chest.basis = tilt * Basis.from_scale(CHEST_SHAPE)
	_band.position = hip + spine_up * 0.06
	_band.basis = tilt * Basis.from_scale(BELLY_SHAPE)
	pelvis_world = _root.global_transform * hip
	for side in 2:
		foot_world[side] = _root.global_transform * (_feet[side] as Vector3)


# Two-bone IK, thigh then shin, the knee bending forward.
func _pose_leg(side: int, hip: Vector3, foot: Vector3) -> void:
	var leg: Array = _legs[side]
	var ankle := foot + Vector3(0.0, ANKLE_Y, 0.0)
	var to_ankle := ankle - hip
	var reach := clampf(to_ankle.length(), 0.05, THIGH + SHIN - 0.001)
	var dir := to_ankle.normalized() if to_ankle.length() > 1.0e-4 else Vector3.DOWN
	# Angle at the hip between the leg's line and the thigh (law of cosines).
	var cos_hip := clampf((THIGH * THIGH + reach * reach - SHIN * SHIN) / (2.0 * THIGH * reach), -1.0, 1.0)
	var bend := acos(cos_hip)
	# The knee bends toward the body's front (-z).
	var pole := Vector3(0.0, 0.0, -1.0)
	var axis := dir.cross(pole)
	if axis.length() < 1.0e-4:
		axis = Vector3.RIGHT
	axis = axis.normalized()
	var thigh_dir := dir.rotated(axis, -bend)
	var knee := hip + thigh_dir * THIGH
	var shin_dir := (ankle - knee).normalized()
	_place(leg[0], hip, knee)
	_place(leg[1], knee, ankle)
	var boot: MeshInstance3D = leg[2]
	var sole: MeshInstance3D = leg[3]
	var heading := Vector3(shin_dir.x, 0.0, shin_dir.z)
	var toward := Basis.IDENTITY
	if heading.length() > 0.2:
		toward = Basis(Vector3.UP, atan2(-heading.x, -heading.z) * 0.3)
	# The upper lies along the foot (the capsule's axis turned to -z), a
	# little flattened, its heel under the ankle.
	boot.position = foot + toward * Vector3(0.0, 0.075, -0.06)
	boot.basis = toward * Basis(Vector3.RIGHT, -PI * 0.5) * Basis.from_scale(Vector3(0.95, 1.0, 0.8))
	sole.position = foot + toward * Vector3(0.0, 0.015, -0.06)
	sole.basis = toward


# A capsule from a to b.
func _place(part: MeshInstance3D, a: Vector3, b: Vector3) -> void:
	var span := b - a
	part.position = (a + b) * 0.5
	var up := span.normalized() if span.length() > 1.0e-4 else Vector3.UP
	var side := up.cross(Vector3.FORWARD)
	if side.length() < 1.0e-3:
		side = up.cross(Vector3.RIGHT)
	side = side.normalized()
	part.basis = Basis(side, up, side.cross(up))


# For a test: the nearest the body comes to `point` (the lens), roughly: the
# distance to each part's centre less its radius.
func nearest_to(point: Vector3) -> float:
	var nearest := INF
	for part in [_pelvis, _belly, _chest, _band]:
		nearest = minf(nearest, (part as MeshInstance3D).global_position.distance_to(point) - 0.16)
	for leg in _legs:
		for part in leg:
			nearest = minf(nearest, (part as MeshInstance3D).global_position.distance_to(point) - 0.1)
	return nearest
