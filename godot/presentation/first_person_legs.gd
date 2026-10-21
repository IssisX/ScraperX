extends Node3D

const BodyRig := preload("res://presentation/first_person_body_rig.gd")

# Read-only body presentation. A sole is planted only at a supplied native
# material point. Leg parity is artistic; native foot motors have no side.
const THIGH := BodyRig.THIGH
const SHIN := BodyRig.SHIN
const MAX_HIP_LEAN := 0.18
const RETURN_TIME := 0.12
const HIP_BEHIND_EYES := 0.16

class Leg:
	var side := 1.0
	var point := Vector3.ZERO
	var frame := Basis.IDENTITY
	var initialized := false
	var planted := false
	var returning := false
	var return_elapsed := 0.0
	var return_point := Vector3.ZERO
	var return_rotation := Quaternion.IDENTITY

var _legs: Array[Leg] = []
var _last_tick := -1
var _planted_index := -1
var _body: BodyRig

func build(sleeve: Material, boot: Material, sole: Material) -> void:
	if _body != null:
		return
	_body = BodyRig.new()
	add_child(_body)
	_body.build(sleeve, boot, sole)
	for side in [-1.0, 1.0]:
		var leg := Leg.new()
		leg.side = side
		_legs.append(leg)
	reset()

func reset() -> void:
	_last_tick = -1
	_planted_index = -1
	visible = false
	if _body != null:
		_body.reset_pose()
	for leg in _legs:
		leg.initialized = false
		leg.planted = false
		leg.returning = false
		leg.return_elapsed = 0.0
		leg.point = Vector3.ZERO
		leg.frame = Basis.IDENTITY
		leg.return_point = Vector3.ZERO
		leg.return_rotation = Quaternion.IDENTITY

func update_legs(state: Dictionary, camera: Transform3D, delta: float) -> void:
	if _body == null or not _body.ready_to_pose():
		return
	var tick := int(state.get("native_tick", -1))
	var rewound := _last_tick >= 0 and tick < _last_tick
	var operating := StringName(state.get("operating", &""))
	if rewound or bool(state.get("dead", false)) or (
			operating != &"" and operating != &"grappling_gun"):
		reset()
		_last_tick = tick
		return
	var com: Vector3 = state.get("render_position", Vector3.INF)
	var facing: Vector3 = state.get("facing", Vector3.FORWARD)
	if not com.is_finite() or not facing.is_finite() or tick < 0:
		reset()
		return
	facing.y = 0.0
	if not is_finite(facing.length_squared()) or facing.length_squared() < 0.0001:
		reset()
		return
	facing = facing.normalized()
	var body := _sole_frame(Vector3.UP, facing)
	var crouched := bool(state.get("player_crouched", false))
	var half_height := 0.6 if crouched else 0.9
	var anchor: Vector3 = state.get("foot_push_world_anchor", Vector3.INF)
	var normal: Vector3 = state.get("foot_push_world_normal", Vector3.ZERO)
	var contact_tick := int(state.get("foot_push_source_contact_tick", -1))
	var generation := int(state.get("foot_push_start_count", 0))
	var valid := bool(state.get("foot_push_active", false)) \
		and int(state.get("foot_push_support_entity_id", -1)) > 0 \
		and contact_tick > 0 and contact_tick <= tick \
		and anchor.is_finite() and normal.is_finite() \
		and is_finite(normal.length_squared()) and normal.length_squared() > 0.0001
	var selected := generation % 2 if valid else -1
	_planted_index = -1
	visible = true
	_last_tick = tick
	var dt := clampf(delta, 0.0, RETURN_TIME) if is_finite(delta) else 0.0
	var velocity: Vector3 = state.get("velocity", Vector3.ZERO)
	velocity.y = 0.0
	var speed := velocity.length() if velocity.is_finite() else 0.0
	var travel := velocity / speed if speed > 0.001 else facing
	var stride := clampf(speed / 3.5, 0.0, 1.0)
	var phase := float(state.get("bob_phase", 0.0)) * 0.5
	if not is_finite(phase):
		phase = 0.0
	var grounded := bool(state.get("player_grounded", false))
	# Share Main's already-presented crouch blend. Feet keep their real floor
	# height while the hip and camera lower together, without another clock.
	var crouch_blend := float(state.get("crouch_visual_01", 1.0 if crouched else 0.0))
	if not is_finite(crouch_blend):
		crouch_blend = 1.0 if crouched else 0.0
	var hip_height := lerpf(0.915, 0.57, smoothstep(0.0, 1.0, crouch_blend))
	# The camera sits over the capsule centre. Present the hip joints behind
	# the eyes so the waist does not cover the knees in the look-down view.
	var hip_centre := Vector3(com.x, com.y - half_height + hip_height, com.z) \
		- facing * HIP_BEHIND_EYES
	_body.pose_body(hip_centre, body, state, dt, camera)
	for i in _legs.size():
		var leg := _legs[i]
		var neutral := com + body.x * leg.side * 0.115 + facing * 0.035 - Vector3.UP * half_height
		if grounded:
			# Supplied body phase drives both feet; there is no animation clock.
			var cycle := phase + float(i) * PI
			var crouch_scale := 0.6 if crouched else 1.0
			neutral += travel * sin(cycle) * 0.13 * stride * crouch_scale
			neutral += Vector3.UP * maxf(0.0, cos(cycle)) * 0.055 * stride * crouch_scale
		else:
			# Unloaded feet tuck under the body, without asserting a contact.
			neutral += Vector3.UP * 0.17 + facing * 0.09
		var target := neutral
		var frame := body
		var planting := i == selected
		if grounded and not planting and stride > 0.0:
			# The same supplied stride phase rolls the unloaded boot through
			# heel/toe motion. It never rotates a native planted contact.
			var cycle := phase + float(i) * PI
			var toe_roll := sin(cycle) * 0.15 * stride * (0.6 if crouched else 1.0)
			frame = body * Basis(Vector3.RIGHT, toe_roll)
			# Keep the unloaded silhouette above the floor while it rolls.
			# A real planted sole takes its native point/frame below instead.
			target.y += _body.unloaded_lift(i, frame)
		if planting:
			target = anchor
			frame = _sole_frame(normal.normalized(), facing)
		elif leg.planted:
			leg.returning = true
			leg.return_elapsed = 0.0
			leg.return_point = leg.point
			leg.return_rotation = leg.frame.get_rotation_quaternion()
		leg.planted = false
		if planting:
			leg.returning = false
		elif leg.returning:
			leg.return_elapsed = minf(RETURN_TIME, leg.return_elapsed + dt)
			var weight := smoothstep(0.0, RETURN_TIME, leg.return_elapsed)
			target = leg.return_point.lerp(neutral, weight)
			frame = Basis(leg.return_rotation.slerp(body.get_rotation_quaternion(), weight))
			leg.returning = leg.return_elapsed < RETURN_TIME
		var hip: Vector3 = _body.hip_world(i)
		var ankle: Vector3 = target - _body.sole_offset(i, frame)
		var distance := hip.distance_to(ankle)
		# Lean only the presented hip, never move the sole to fit the leg.
		if planting and distance > THIGH + SHIN - 0.002:
			hip += hip.direction_to(ankle) * minf(MAX_HIP_LEAN, distance - (THIGH + SHIN - 0.002))
			distance = hip.distance_to(ankle)
		if distance > THIGH + SHIN - 0.001 or distance < 0.04:
			# A rejected visual reach cannot invent contact or stretch a leg.
			planting = false
			leg.returning = false
			target = neutral
			frame = body
			hip = _body.hip_world(i)
			ankle = target - _body.sole_offset(i, frame)
			distance = hip.distance_to(ankle)
		leg.point = target
		leg.frame = frame
		leg.initialized = true
		leg.planted = planting
		if planting:
			_planted_index = i
		var down := (ankle - hip) / distance
		# A wall plant folds the knee away from the wall, keeping the boot
		# visible below the body rather than burying it behind the shin.
		var bend_direction := normal.normalized() if planting and absf(normal.y) < 0.2 else facing
		var pole := bend_direction - down * bend_direction.dot(down)
		if pole.length_squared() < 0.0001:
			pole = body.x * leg.side
		pole = pole.normalized()
		var along := (THIGH * THIGH - SHIN * SHIN + distance * distance) / (2.0 * distance)
		var bend := sqrt(maxf(0.0, THIGH * THIGH - along * along))
		var knee := hip + down * along + pole * bend
		_body.pose_leg(i, hip, knee, ankle, frame, pole)
	_body.pose_shadow(camera, state.get("wrist_positions", []))

# Diagnostics expose the actual drawn sole bottom, not a second target cache.
func planted_foot_index() -> int:
	return _planted_index

func planted_foot_position() -> Vector3:
	if _planted_index < 0:
		return Vector3.INF
	return _body.sole_world(_planted_index)

func planted_foot_normal() -> Vector3:
	if _planted_index < 0:
		return Vector3.ZERO
	return _body.sole_normal(_planted_index)

func _sole_frame(normal: Vector3, facing: Vector3) -> Basis:
	var toe := facing - normal * facing.dot(normal)
	if toe.length_squared() < 0.0001:
		# On a head-on wall, point the toe upward along that wall.
		toe = Vector3.UP - normal * normal.y
	if toe.length_squared() < 0.0001:
		toe = Vector3.FORWARD - normal * normal.dot(Vector3.FORWARD)
	toe = toe.normalized()
	return Basis(toe.cross(normal).normalized(), normal, -toe)
