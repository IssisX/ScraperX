extends Node3D

# First-person arms: the body's hands doing what the native simulation says
# the body is doing. Pure presentation. Every hand target is either fixed to
# the camera (idle, running, airborne, canopy, the crane remote) or to a
# point the native reports -- the ledge the player hangs from, the lip being
# mantled, the rail being vaulted, the edge in grab range. Nothing here is
# read back, so the arms can never grip what the native did not grip.
#
# Rig: shoulders ride a yaw-only torso frame under the eye; elbows come from
# analytic two-bone IK with a pose-dependent pole; hands are built from
# primitives (gloved palm, three-phalanx fingers, two-phalanx thumb) and curl
# per pose. Work gloves and canvas sleeves with a hazard band: this is an
# industrial climb, and bare procedural skin reads as uncanny long before
# leather and canvas do.

const UPPER_ARM := 0.36
const FOREARM := 0.33
# The first-person viewmodel convention: shoulders ride the camera, below
# and behind the eye (x mirrored per side, +z behind), so only forearms and
# hands ever enter the frame -- rising from the lower corners toward what
# they hold. Anatomical shoulders sit beside the head, and a forearm 0.25 m
# from the lens fills the screen edge as a slab.
const SHOULDER_LOCAL := Vector3(0.17, -0.30, 0.18)
# Arms raised to a grip rise from lower and nearer the centre, so the
# forearms climb out of the bottom corners instead of crossing the frame.
const SHOULDER_LOCAL_RAISED := Vector3(0.14, -0.46, 0.12)
# The native capsule holds the eye 0.41 m off a hang wall; a real body pulls
# in. The shoulder may lean this far toward an out-of-reach anchor.
const MAX_LEAN := 0.3
# A planted palm stays planted while the lean can still reach it: the push
# of a mantle ends when the shoulders have risen past the hands, not at a
# fixed fraction of the native traversal clock.
const PLANT_RELEASE_DEFICIT := 0.24
# ledge_point sits this far past the lip (kTopProbeInset, simulation.cpp);
# the grip is placed on the lip itself, not on the probe's landing spot.
const TOP_PROBE_INSET := 0.12
const GRIP_HALF_SPAN := 0.13
const PALM_HALF_THICKNESS := 0.017
# Outside a traversal a hand closes on its target no faster than a person
# reaches, so taking a load, hooking it on or letting it go is a movement,
# not a jump. The hand still rides its target's own motion. Traversal poses
# keep their rates: a mantle lifts the eye ~1.5 m in 0.3 s.
const REACH_SPEED := 4.0

const POSE_REST := 0
const POSE_RUN := 1
const POSE_AIR := 2
const POSE_REACH := 3
const POSE_GRIP := 4
const POSE_PLANT := 5
const POSE_CHUTE := 6
const POSE_REMOTE := 7
const POSE_CARRY := 8

const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3

const FINGER_LATERAL := [0.030, 0.010, -0.010, -0.028]
const FINGER_LENGTHS := [
	[0.040, 0.025, 0.020],
	[0.045, 0.028, 0.021],
	[0.042, 0.026, 0.020],
	[0.033, 0.020, 0.018],
]
# Gloved fingers: thick enough that neighbours touch, as they do in a glove.
# Radius at each phalanx's base; the last entry is the fingertip's.
const FINGER_RADII := [0.0118, 0.0108, 0.0098, 0.0088]
const THUMB_RADII := [0.0128, 0.0112, 0.0100]
# Flexion per phalanx at full curl: proximal, middle, distal.
const CURL_ANGLES := [1.25, 1.55, 1.0]


class Hand:
	var side := 1.0
	var root: Node3D
	var upper: MeshInstance3D
	var elbow: MeshInstance3D
	var fore: MeshInstance3D
	var cuff: MeshInstance3D
	var band: MeshInstance3D
	var fingers: Array = []
	var thumb: Array = []
	var thumb_root: Node3D
	var thumb_open := Quaternion.IDENTITY
	var thumb_closed := Quaternion.IDENTITY
	var local_wrist := Vector3.ZERO
	var local_rotation := Quaternion.IDENTITY
	var world_wrist := Vector3.ZERO
	var world_rotation := Quaternion.IDENTITY
	var local_valid := false
	var anchor := Vector3.ZERO
	var anchored := false
	var curl := 0.3
	var thumb_curl := 0.3
	var initialized := false
	var planted := false
	var released := false
	var pose := POSE_REST


var _hands: Array[Hand] = []
var _remote: Node3D
var _remote_buttons := {}
var _remote_led: MeshInstance3D
var _risers: Array[MeshInstance3D] = []
var _grip_forward := Vector3.FORWARD
var _last_traversal := 0
var _clock := 0.0


func build(sleeve: Material, band: Material, glove: Material, glove_dark: Material,
		remote_body: Material, remote_face: Material, remote_button: Material,
		stop_red: Material, led: Material, riser: Material) -> void:
	for side in [1.0, -1.0]:
		var hand := Hand.new()
		hand.side = side
		hand.upper = _mesh(_cylinder(0.044, 0.052, UPPER_ARM), sleeve)
		hand.elbow = _mesh(_sphere(0.045), sleeve)
		hand.fore = _mesh(_cylinder(0.035, 0.043, FOREARM), sleeve)
		hand.band = _mesh(_cylinder(0.0385, 0.0405, 0.035), band)
		# Gauntlet cuff: narrow at the wrist, flaring toward the elbow.
		hand.cuff = _mesh(_cylinder(0.037, 0.041, 0.055), glove_dark)
		hand.root = Node3D.new()
		add_child(hand.root)
		_build_hand(hand, glove)
		_hands.append(hand)
	_build_remote(remote_body, remote_face, remote_button, stop_red, led)
	for i in 2:
		var line := _mesh(_cylinder(0.0035, 0.0035, 1.0), riser)
		line.visible = false
		_risers.append(line)


func _mesh(mesh: Mesh, material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	# A disembodied pair of arms must not throw a shadow onto the deck.
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(instance)
	return instance


func _cylinder(top: float, bottom: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top
	mesh.bottom_radius = bottom
	mesh.height = height
	mesh.radial_segments = 14
	mesh.rings = 1
	return mesh


func _sphere(radius: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 14
	mesh.rings = 8
	return mesh


func _capsule(radius: float, length: float) -> CapsuleMesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = length + radius * 2.0
	mesh.radial_segments = 10
	mesh.rings = 3
	return mesh


# Hand frame: origin at the wrist, +Y toward the fingertips, -Z the palm
# face, +X = Y x Z. The thumb sits at -X on the right hand, +X on the left.
func _build_hand(hand: Hand, glove: Material) -> void:
	var thumb_side := -hand.side
	# A flattened pill with heel and knuckle pads: an ellipsoid tapers to a
	# point at the wrist (a leaf), a box shows its corners (a mitt).
	var palm_core := MeshInstance3D.new()
	palm_core.mesh = _capsule(0.041, 0.018)
	palm_core.material_override = glove
	palm_core.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	palm_core.scale = Vector3(1.0, 1.0, 0.38)
	palm_core.position = Vector3(thumb_side * 0.002, 0.052, 0.0)
	hand.root.add_child(palm_core)
	_hand_part(hand.root, _capsule(0.016, 0.052), glove,
		Vector3(thumb_side * 0.004, 0.018, -0.001), Vector3(0.0, 0.0, PI * 0.5))
	# Knuckle line: flattened like the palm, so it rounds the palm's top edge
	# instead of standing proud of it as a tube.
	var knuckles := MeshInstance3D.new()
	knuckles.mesh = _capsule(0.0150, 0.058)
	knuckles.material_override = glove
	knuckles.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	knuckles.rotation = Vector3(0.0, 0.0, PI * 0.5)
	knuckles.scale = Vector3(1.0, 1.0, 0.9)
	knuckles.position = Vector3(thumb_side * 0.001, 0.094, 0.001)
	hand.root.add_child(knuckles)
	for f in 4:
		var chain: Array = []
		var parent := hand.root
		var base := Vector3(thumb_side * FINGER_LATERAL[f], 0.100, 0.0)
		for p in 3:
			var joint := Node3D.new()
			joint.position = base if p == 0 else Vector3(0.0, FINGER_LENGTHS[f][p - 1], 0.0)
			parent.add_child(joint)
			_digit_segment(joint, FINGER_LENGTHS[f][p], FINGER_RADII[p], FINGER_RADII[p + 1],
				glove, p == 2)
			chain.append(joint)
			parent = joint
		hand.fingers.append(chain)
	# Open, the thumb leaves the palm diagonally toward the fingertips and
	# outward; closed, it lies across the front of the curled fingers.
	hand.thumb_open = _basis_from_y(Vector3(thumb_side * 0.80, 0.52, -0.30),
		Vector3(0.0, 0.0, 1.0)).get_rotation_quaternion()
	hand.thumb_closed = _basis_from_y(Vector3(-thumb_side * 0.50, 0.40, -0.76),
		Vector3(0.0, 0.0, 1.0)).get_rotation_quaternion()
	hand.thumb_root = Node3D.new()
	hand.thumb_root.position = Vector3(thumb_side * 0.036, 0.028, -0.012)
	hand.root.add_child(hand.thumb_root)
	var thumb_parent := hand.thumb_root
	for p in 2:
		var length := 0.038 if p == 0 else 0.030
		var joint := Node3D.new()
		joint.position = Vector3.ZERO if p == 0 else Vector3(0.0, 0.038, 0.0)
		thumb_parent.add_child(joint)
		_digit_segment(joint, length, THUMB_RADII[p], THUMB_RADII[p + 1], glove, p == 1)
		hand.thumb.append(joint)
		thumb_parent = joint


# One phalanx: a tube tapering from `r_base` at its joint to `r_end`, with a
# ball of exactly `r_base` at the joint. The ball is the same radius as both
# tubes meeting there, so a bent joint reads as one continuous rounded
# finger, never a bead; the tip closes with a ball of `r_end`.
func _digit_segment(joint: Node3D, length: float, r_base: float, r_end: float,
		material: Material, is_tip: bool) -> void:
	_hand_part(joint, _cylinder(r_end, r_base, length), material,
		Vector3(0.0, length * 0.5, 0.0), Vector3.ZERO)
	_hand_part(joint, _sphere(r_base), material, Vector3.ZERO, Vector3.ZERO)
	if is_tip:
		_hand_part(joint, _sphere(r_end), material, Vector3(0.0, length, 0.0), Vector3.ZERO)


func _hand_part(parent: Node3D, mesh: Mesh, material: Material, at: Vector3,
		rotation_euler: Vector3) -> void:
	var part := MeshInstance3D.new()
	part.mesh = mesh
	part.material_override = material
	part.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	part.position = at
	part.rotation = rotation_euler
	parent.add_child(part)


# A handheld radio crane remote: the native pendant stations accept input
# anywhere inside their radius, which a fixed panel within arm's reach could
# not honour -- a belly-box remote can.
func _build_remote(body: Material, face: Material, button: Material, stop_red: Material,
		led: Material) -> void:
	_remote = Node3D.new()
	add_child(_remote)
	_remote.visible = false
	var parts := [
		[_box(Vector3(0.094, 0.165, 0.046)), body, Vector3.ZERO],
		[_box(Vector3(0.080, 0.118, 0.006)), face, Vector3(0.0, -0.008, 0.024)],
		[_cylinder(0.0035, 0.0035, 0.085), face, Vector3(0.036, 0.118, -0.006)],
		[_cylinder(0.013, 0.015, 0.012), stop_red, Vector3(0.0, 0.088, 0.0)],
	]
	for part in parts:
		var instance := MeshInstance3D.new()
		instance.mesh = part[0]
		instance.material_override = part[1]
		instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		instance.position = part[2]
		_remote.add_child(instance)
	for entry in [[&"up", Vector3(0.022, 0.022, 0.028)], [&"down", Vector3(0.022, -0.018, 0.028)],
			[&"left", Vector3(-0.034, 0.002, 0.028)], [&"right", Vector3(-0.012, 0.002, 0.028)]]:
		var knob := MeshInstance3D.new()
		knob.mesh = _cylinder(0.0085, 0.0085, 0.008)
		knob.material_override = button
		knob.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		knob.rotation = Vector3(PI * 0.5, 0.0, 0.0)
		knob.position = entry[1]
		_remote.add_child(knob)
		_remote_buttons[entry[0]] = knob
	_remote_led = MeshInstance3D.new()
	_remote_led.mesh = _sphere(0.0045)
	_remote_led.material_override = led
	_remote_led.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_remote_led.position = Vector3(-0.024, 0.042, 0.027)
	_remote_led.visible = false
	_remote.add_child(_remote_led)


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


# Y along `y`, X = hint x Y, Z = X x Y: a proper rotation for any hint not
# parallel to y.
static func _basis_from_y(y: Vector3, hint: Vector3) -> Basis:
	var yy := y.normalized()
	var x := hint.cross(yy)
	if x.length_squared() < 1.0e-8:
		x = Vector3.RIGHT.cross(yy)
		if x.length_squared() < 1.0e-8:
			x = Vector3.FORWARD.cross(yy)
	x = x.normalized()
	return Basis(x, yy, x.cross(yy).normalized())


# Hand basis from where the fingers point and where the back of the hand faces.
static func _hand_basis(fingers: Vector3, dorsal: Vector3) -> Basis:
	var y := fingers.normalized()
	var z := dorsal - y * dorsal.dot(y)
	if z.length_squared() < 1.0e-8:
		z = _basis_from_y(y, Vector3.UP).z
	z = z.normalized()
	return Basis(y.cross(z), y, z)


# state: see main.gd _arms_state(). camera: the final camera transform this
# frame (after landing dip and head bob), so camera-held poses ride with it.
func update_arms(state: Dictionary, camera: Transform3D, delta: float) -> void:
	_clock += delta
	var traversal: int = state["traversal"]
	var progress: float = state["progress"]
	var position: Vector3 = state["position"]
	var velocity: Vector3 = state["velocity"]
	var grounded: bool = state["grounded"]
	var ledge: Vector3 = state["ledge_point"]
	var forward := -camera.basis.z
	forward.y = 0.0
	forward = forward.normalized() if forward.length_squared() > 1.0e-6 else Vector3.FORWARD
	# The wall a traversal is against does not turn when the player looks
	# around mid-hang: the grip frame is fixed when the traversal begins.
	if traversal != 0 and _last_traversal == 0:
		var to_ledge := ledge - position
		to_ledge.y = 0.0
		_grip_forward = to_ledge.normalized() if to_ledge.length_squared() > 1.0e-4 else forward
	_last_traversal = traversal
	var torso_right := forward.cross(Vector3.UP).normalized()
	var speed := Vector2(velocity.x, velocity.z).length()
	var operating: StringName = state["operating"]
	var chute: bool = state["chute"]

	var remote_frame := Transform3D()
	if operating != &"":
		remote_frame = _remote_transform(camera)
	_remote.visible = operating != &""
	if _remote.visible:
		_remote.global_transform = remote_frame
		_update_remote_buttons(state["pendant"])

	for hand in _hands:
		var shoulder_local := SHOULDER_LOCAL_RAISED if traversal == TRAVERSAL_HANGING else SHOULDER_LOCAL
		var shoulder := camera * Vector3(shoulder_local.x * hand.side, shoulder_local.y, shoulder_local.z)
		var pose := POSE_REST
		var reachable := true
		if traversal == TRAVERSAL_HANGING:
			pose = POSE_GRIP
		elif traversal == TRAVERSAL_MANTLING or (traversal == TRAVERSAL_VAULTING and hand.side < 0.0):
			# One-hand speed vault: only the left palm takes the rail.
			var plant: Vector3 = _pose_target(hand, POSE_PLANT, state, camera, forward, torso_right,
				remote_frame)[0]
			# A hand reaches for the lip until it lands there, then stays until
			# the rising body carries the shoulder out of reach of it. A lip the
			# native path never brings within reach is reached for, never
			# touched: a palm on air would be a lie about the geometry.
			reachable = plant.distance_to(shoulder) - (UPPER_ARM + FOREARM) < PLANT_RELEASE_DEFICIT
			if hand.planted and not reachable:
				hand.released = true
			pose = POSE_PLANT
			if hand.released:
				pose = POSE_AIR if traversal == TRAVERSAL_VAULTING else POSE_REST
		elif traversal == TRAVERSAL_VAULTING:
			pose = POSE_AIR
		elif operating != &"":
			pose = POSE_REMOTE
		elif int(state.get("carrying", 0)) != 0:
			pose = POSE_CARRY
		elif chute:
			pose = POSE_CHUTE
		elif not grounded:
			pose = POSE_REACH if bool(state["affordance"]) else POSE_AIR
		elif speed > 0.8:
			pose = POSE_RUN
		hand.pose = pose

		var target := _pose_target(hand, pose, state, camera, forward, torso_right, remote_frame)
		var world_wrist: Vector3 = target[0]
		var world_basis: Basis = target[1]
		var anchored: bool = target[2]
		var rate: float = target[3]
		hand.anchored = anchored
		hand.anchor = world_wrist
		hand.curl = lerpf(hand.curl, target[4], 1.0 - exp(-14.0 * delta))
		hand.thumb_curl = lerpf(hand.thumb_curl, target[5], 1.0 - exp(-18.0 * delta))

		# A held pose converges in camera space, so a turning head carries it;
		# an anchored pose converges in world space, fixed to the ledge while
		# the camera sweeps past -- a mantle lifts the eye ~1.5 m in 0.3 s,
		# and a hand chasing that in camera space never lands. Once it lands
		# it snaps exactly onto the anchor, so a grip never slides.
		var inverse := camera.affine_inverse()
		var target_rotation_world := world_basis.get_rotation_quaternion()
		if not hand.initialized:
			hand.world_wrist = world_wrist
			hand.world_rotation = target_rotation_world
			hand.initialized = true
		var blend := 1.0 - exp(-rate * delta)
		var reach_step := REACH_SPEED * delta if traversal == 0 else INF
		if anchored:
			hand.world_wrist = world_wrist + _close(hand.world_wrist - world_wrist, blend, reach_step)
			hand.world_rotation = hand.world_rotation.slerp(target_rotation_world, blend)
			if hand.world_wrist.distance_to(world_wrist) < 0.012:
				hand.world_wrist = world_wrist
				hand.world_rotation = target_rotation_world
				if pose == POSE_PLANT and reachable:
					hand.planted = true
		else:
			var local_target := inverse * world_wrist
			var local_rotation_target := (inverse.basis * world_basis).get_rotation_quaternion()
			var carried := inverse * hand.world_wrist if not hand.local_valid else hand.local_wrist
			var carried_rotation := (inverse.basis * Basis(hand.world_rotation)).get_rotation_quaternion() \
				if not hand.local_valid else hand.local_rotation
			hand.local_wrist = local_target + _close(carried - local_target, blend, reach_step)
			hand.local_rotation = carried_rotation.slerp(local_rotation_target, blend)
			hand.world_wrist = camera * hand.local_wrist
			hand.world_rotation = (camera.basis * Basis(hand.local_rotation)).get_rotation_quaternion()
		# Camera-held state is only carried while the pose stays camera-held;
		# after an anchored stretch it is re-derived from where the hand is.
		hand.local_valid = not anchored
		var wrist := hand.world_wrist
		var hand_basis := Basis(hand.world_rotation)

		if traversal == 0:
			hand.released = false
			hand.planted = false
		var reach := wrist - shoulder
		var deficit := reach.length() - (UPPER_ARM + FOREARM - 0.015)
		if deficit > 0.0:
			shoulder += reach.normalized() * minf(deficit, MAX_LEAN)
		_solve_arm(hand, shoulder, wrist, hand_basis, _pole(pose, hand.side, forward, torso_right))
		_pose_fingers(hand)

	_update_risers(chute and operating == &"" and traversal == 0)


# [wrist, basis, anchored, smoothing rate, curl, thumb curl] in world space.
func _pose_target(hand: Hand, pose: int, state: Dictionary, camera: Transform3D, forward: Vector3,
		torso_right: Vector3, remote_frame: Transform3D) -> Array:
	var side := hand.side
	var cam := camera.basis
	var right := cam.x
	var up := cam.y
	var back := cam.z
	match pose:
		POSE_GRIP, POSE_PLANT:
			var ledge: Vector3 = state["ledge_point"]
			var along := _grip_forward.cross(Vector3.UP).normalized()
			var lip := ledge - _grip_forward * TOP_PROBE_INSET
			if pose == POSE_GRIP:
				# Hook grip: palm to the lip, wrist under it, fingers curled
				# over onto the top -- from below, knuckles on the edge.
				var hook := lip - _grip_forward * 0.034 + Vector3.DOWN * 0.074 \
					+ along * (GRIP_HALF_SPAN * side)
				var hook_basis := _hand_basis(Vector3.UP * 0.95 + _grip_forward * 0.3, -_grip_forward)
				return [hook, hook_basis, true, 30.0, 0.8, 0.35]
			var span := GRIP_HALF_SPAN * 0.9
			var inset := 0.06
			if int(state["traversal"]) == TRAVERSAL_VAULTING:
				span = 0.08
				inset = 0.04
			var wrist := lip + _grip_forward * inset + Vector3.UP * PALM_HALF_THICKNESS \
				+ along * (span * side)
			# Palm flat on the top, fingers pointing into the ledge: the heel
			# of the hand takes the push as the body rises past it.
			var basis := _hand_basis(_grip_forward, Vector3.UP)
			return [wrist, basis, true, 30.0, 0.1, 0.15]
		POSE_REACH:
			var target: Vector3 = state["affordance_point"]
			var along := forward.cross(Vector3.UP).normalized()
			var wrist := target - forward * (TOP_PROBE_INSET + 0.06) + Vector3.UP * 0.02 \
				+ along * (GRIP_HALF_SPAN * side)
			var basis := _hand_basis(Vector3.UP * 0.75 + forward * 0.65, -forward)
			return [wrist, basis, false, 16.0, 0.12, 0.1]
		POSE_CHUTE:
			# Brake toggles at head height; a steering pull hauls one down.
			var pull := maxf(0.0, -float((state["move"] as Vector2).x) * side) * 0.12
			var wrist := camera.origin + right * (0.33 * side) + up * (0.10 - pull) - back * 0.46
			var basis := _hand_basis(up * 0.5 - back * 0.8, right * side)
			return [wrist, basis, false, 12.0, 1.0, 0.8]
		POSE_REMOTE:
			# Wrist behind the box's midplane: fingers wrap the back, the
			# thumb comes round onto the face -- a book held by its edge.
			var local_wrist := Vector3(0.060 * side, -0.066, -0.030)
			var wrist := remote_frame * local_wrist
			var rb := remote_frame.basis
			var basis := _hand_basis(rb.y * 0.9 - rb.z * 0.35, rb.x * side)
			var pendant: Vector2 = state["pendant"]
			var pressing := absf(pendant.y) > 0.1 if side > 0.0 else absf(pendant.x) > 0.1
			return [wrist, basis, false, 18.0, 0.7, 0.42 if pressing else 0.3]
		POSE_CARRY:
			# Both hands on the load, one either side of it, gripping its
			# flanks: the native carry point is between them, so what the arms
			# hold is what the hands hold. World-anchored, so the grip rides the
			# load as it swings.
			var centre: Vector3 = state["carry_center"]
			var half: float = state["carry_half"]
			var wrist := centre + torso_right * ((half + 0.035) * side) + Vector3.UP * 0.04
			var basis := _hand_basis(Vector3.DOWN * 0.6 + forward * 0.5, torso_right * side)
			return [wrist, basis, true, 22.0, 0.75, 0.5]
		POSE_AIR:
			# Arms thrown up for balance: palms turned in and down, fingers
			# loose -- not a reach, which only an edge in range earns.
			var sway := sin(_clock * 5.3 + side) * 0.012
			var wrist := camera.origin + right * (0.27 * side) + up * (-0.34 + sway) - back * 0.45
			var basis := _hand_basis(-back * 0.85 - up * 0.2 - right * (0.25 * side), right * side + up * 0.8)
			return [wrist, basis, false, 11.0, 0.45, 0.35]
		POSE_RUN:
			# Arm pump locked to the head-bob stride, one arm against the other.
			var velocity: Vector3 = state["velocity"]
			var speed_fraction := clampf(Vector2(velocity.x, velocity.z).length() / 5.5, 0.0, 1.0)
			var swing := sin(float(state["bob_phase"]) * 0.5 + (0.0 if side > 0.0 else PI)) * speed_fraction
			var wrist := camera.origin + right * (0.24 * side) + up * (-0.39 + 0.05 * swing) \
				- back * (0.31 + 0.10 * swing)
			var basis := _hand_basis(-up * 0.35 - back * 0.9, right * side)
			return [wrist, basis, false, 20.0, 0.72, 0.6]
	var rest := camera.origin + right * (0.23 * side) + up * -0.44 - back * 0.22
	return [rest, _hand_basis(-up * 0.7 - back * 0.7, right * side), false, 9.0, 0.35, 0.3]


# Held at the belly, its face turned up toward the eye.
func _remote_transform(camera: Transform3D) -> Transform3D:
	var cam := camera.basis
	var center := camera.origin + cam.y * -0.215 - cam.z * 0.34
	var face := (camera.origin - center).normalized()
	var up := (cam.y - face * cam.y.dot(face)).normalized()
	return Transform3D(Basis(up.cross(face), up, face), center)


func _update_remote_buttons(pendant: Vector2) -> void:
	var pressed := {
		&"up": pendant.y > 0.1, &"down": pendant.y < -0.1,
		&"left": pendant.x < -0.1, &"right": pendant.x > 0.1,
	}
	for key in _remote_buttons:
		var knob: MeshInstance3D = _remote_buttons[key]
		knob.position.z = 0.0245 if pressed[key] else 0.028
	_remote_led.visible = pendant.length_squared() > 0.01


func _pole(pose: int, side: float, forward: Vector3, right: Vector3) -> Vector3:
	match pose:
		POSE_GRIP, POSE_PLANT, POSE_REACH:
			# Elbows below and behind the grip, a little out: the forearm
			# climbs into frame from the lower corner.
			return right * side * 0.35 - forward * 0.6 + Vector3.DOWN * 1.0
		POSE_REMOTE, POSE_CHUTE:
			return right * side * 0.9 + Vector3.DOWN * 1.0
	return right * side * 0.5 + Vector3.DOWN * 1.0 - forward * 0.4


func _solve_arm(hand: Hand, shoulder: Vector3, wrist_target: Vector3, hand_basis: Basis,
		pole: Vector3) -> void:
	var to_target := wrist_target - shoulder
	var reach := clampf(to_target.length(), absf(UPPER_ARM - FOREARM) + 0.01,
		UPPER_ARM + FOREARM - 0.004)
	var direction := to_target.normalized() if to_target.length_squared() > 1.0e-8 else Vector3.DOWN
	var cos_alpha := clampf((UPPER_ARM * UPPER_ARM + reach * reach - FOREARM * FOREARM)
		/ (2.0 * UPPER_ARM * reach), -1.0, 1.0)
	var bend := pole - direction * pole.dot(direction)
	if bend.length_squared() < 1.0e-8:
		bend = _basis_from_y(direction, Vector3.UP).x
	bend = bend.normalized()
	var elbow := shoulder + direction * (UPPER_ARM * cos_alpha) \
		+ bend * (UPPER_ARM * sqrt(maxf(0.0, 1.0 - cos_alpha * cos_alpha)))
	var wrist := shoulder + direction * reach
	# Out of reach even after the lean: the hand stays on its line, never
	# detached from the forearm.
	_place_segment(hand.upper, shoulder, elbow, bend)
	hand.elbow.global_position = elbow
	_place_segment(hand.fore, elbow, wrist, bend)
	var fore_dir := (wrist - elbow).normalized()
	hand.band.global_transform = Transform3D(_basis_from_y(fore_dir, bend), wrist - fore_dir * 0.075)
	hand.cuff.global_transform = Transform3D(_basis_from_y(fore_dir, bend), wrist - fore_dir * 0.018)
	hand.root.global_transform = Transform3D(hand_basis, wrist)


func _place_segment(segment: MeshInstance3D, from: Vector3, to: Vector3, hint: Vector3) -> void:
	var span := to - from
	if span.length_squared() < 1.0e-8:
		return
	segment.global_transform = Transform3D(_basis_from_y(span, hint), (from + to) * 0.5)


func _pose_fingers(hand: Hand) -> void:
	for f in hand.fingers.size():
		var chain: Array = hand.fingers[f]
		# The little finger closes a touch further than the index, as hands do.
		var finger_curl := clampf(hand.curl * (1.0 + 0.06 * float(f)), 0.0, 1.0)
		for p in chain.size():
			(chain[p] as Node3D).rotation = Vector3(-CURL_ANGLES[p] * finger_curl, 0.0, 0.0)
	hand.thumb_root.basis = Basis(hand.thumb_open.slerp(hand.thumb_closed, hand.thumb_curl))
	(hand.thumb[1] as Node3D).rotation = Vector3(-0.7 * hand.thumb_curl, 0.0, 0.0)


# Canopy risers from each toggle up and out of frame; the canopy itself is
# above the view, where a real one would be.
func _update_risers(show: bool) -> void:
	for i in _risers.size():
		var line := _risers[i]
		line.visible = show
		if not show:
			continue
		var fist := _hands[i].root.global_transform * Vector3(0.0, 0.07, 0.0)
		var outward := _hands[i].root.global_position - _hands[1 - i].root.global_position
		outward.y = 0.0
		var top := fist + Vector3.UP * 2.6
		if outward.length_squared() > 1.0e-6:
			top += outward.normalized() * 0.35
		var span := top - fist
		var basis := _basis_from_y(span, Vector3.RIGHT)
		line.global_transform = Transform3D(Basis(basis.x, basis.y * span.length(), basis.z),
			(fist + top) * 0.5)


# --- test hooks for the scripted --uitest proof --------------------------------


# An offset from a target after one frame of closing on it: `blend` of it
# taken off, but never more than `limit` metres.
func _close(offset: Vector3, blend: float, limit: float) -> Vector3:
	return offset - (offset * blend).limit_length(limit)


func hand_poses() -> Array:
	return _hands.map(func(hand: Hand) -> int: return hand.pose)


# Where each rendered wrist is in the world this frame, for proofs that a
# hand moves between poses instead of jumping.
func wrist_positions() -> Array:
	return _hands.map(func(hand: Hand) -> Vector3: return hand.world_wrist)


# Worst distance between a rendered wrist and the point the native ledge
# implies for it, over hands currently anchored; -1 when none is anchored.
func anchored_error() -> float:
	var worst := -1.0
	for hand in _hands:
		if hand.anchored:
			worst = maxf(worst, hand.root.global_position.distance_to(hand.anchor))
	return worst


func planted_count() -> int:
	var count := 0
	for hand in _hands:
		if hand.planted and not hand.released:
			count += 1
	return count


func remote_visible() -> bool:
	return _remote.visible


func risers_visible() -> bool:
	return _risers[0].visible and _risers[1].visible
