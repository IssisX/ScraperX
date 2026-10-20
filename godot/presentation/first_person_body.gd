extends Node3D

# The climber's own body below the arms, as the player sees it looking down:
# a young man from MakeHuman's scanned body (life/climber/, built by
# tools/body/build_climber_body.py) in his work trousers, the jacket with its
# hi-vis band and zip, and ankle boots. Pure presentation: nothing here is
# read back.
#
# It hangs from the neck, NECK_BELOW_EYE under and NECK_BEHIND_EYE behind the
# eye, rather than standing on the soles, so the eye is always where a head
# would put it and looking straight down it meets the jacket's front, never
# its inside. The trunk leans from the neck (forward into a crouch, back into
# a slide), the hips go where that leaves them, and each leg reaches its foot
# by two-bone IK, the knee going forward. On the ground the feet stay where
# they are put: walking, each is down for the gait's contact time (gait.gd)
# and rolls heel to toe, then swings through ahead; standing, a foot left
# behind by a turn steps back under the body. In the air, sliding, hanging
# or climbing, the feet go where the body's state puts them. The arms are
# the viewmodel's (first_person_arms.gd); this body shows none, and no head,
# but it casts their shadow: a head that pitches with the view and arms that
# reach for wherever the drawn hands are, seen only in its shadow.

const Gait := preload("res://presentation/gait.gd")
const Rig := preload("res://presentation/life/climber/climber_body_rig.gd")
const BODY_FILE := "res://presentation/life/climber/climber_body.glb"

const NECK_BELOW_EYE := 0.123
const NECK_BEHIND_EYE := 0.225
# Where the feet stand under the hips, across and ahead (model space: +z is
# his front): standing, and crouched (the hips back over the heels).
const STANCE_X := 0.12
const CROUCH_STANCE_X := 0.15
const CROUCH_FEET_AHEAD := 0.24
# Standing, a planted foot this far from where it belongs steps back.
const RESTEP_M := 0.13
const RESTEP_SECONDS := 0.28
# The trunk's lean from the neck, radians (+ forward).
const LEAN_CROUCH := 0.68
const LEAN_SLIDE := -0.35
const LEAN_HANG := -0.05
# Heel strike (toes up) and toe-off (heel up), radians of foot pitch.
const HEEL_STRIKE := -0.22
const TOE_OFF := 0.5
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

var _root: Node3D           # at the soles, turned with the view, his front ahead
var _skel: Skeleton3D
var _bone := {}             # name -> index
var _rest_basis := {}       # name -> rest global basis (model space)
var _stride := 0.0          # the gait's phase, in cycles of two footfalls
var _lean := 0.0            # eased
var _sway := 0.0            # the body's own clock for hanging and kicking legs
# Each foot: its ground point (world), its yaw (world), and a step under way.
var _foot := [Vector3.ZERO, Vector3.ZERO]
var _foot_yaw := [0.0, 0.0]
var _planted := [false, false]
var _step_from := [Vector3.ZERO, Vector3.ZERO]
var _step_left := [0.0, 0.0]
var _roll := [0.0, 0.0]
var _ready_feet := false
var _upper_arm := 0.3
var _forearm := 0.27
# For a test: what the body last showed.
var shown_frames := 0
var foot_world := [Vector3.ZERO, Vector3.ZERO]
var pelvis_world := Vector3.ZERO


func build(jacket: Material, band: Material, trousers: Material, boots: Material, soles: Material) -> void:
	name = "FirstPersonBody"
	_root = Node3D.new()
	add_child(_root)
	var doc := GLTFDocument.new()
	var state := GLTFState.new()
	var err := doc.append_from_file(BODY_FILE, state)
	var scene: Node3D = doc.generate_scene(state) as Node3D if err == OK else null
	if scene == null:
		push_error("first-person body: cannot read %s (%s)" % [BODY_FILE, error_string(err)])
		return
	_root.add_child(scene)
	var skeletons := scene.find_children("*", "Skeleton3D", true, false)
	if skeletons.is_empty():
		push_error("first-person body: %s has no skeleton" % BODY_FILE)
		return
	_skel = skeletons[0] as Skeleton3D
	var dress := {"trousers": trousers, "jacket": jacket, "jacket_inner": soles, "hivis": band, "zip": soles,
		"boots": boots, "soles": soles}
	for node in scene.find_children("*", "MeshInstance3D", true, false):
		var mesh := node as MeshInstance3D
		mesh.material_override = dress.get(String(mesh.name), jacket)
		if String(mesh.name) == "shadow":
			mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY
		# Posed far from his bind pose (a squat, a slide) he reaches past the
		# mesh's own bounds; and the lens is right over him.
		mesh.extra_cull_margin = 1.0
	for n in ["body", "spine_lo", "spine_mid", "chest", "neck_lo", "neck_hi", "head", "hp_l", "hp_r", "hp_l_x", "hp_r_x",
			"kn_l", "kn_r", "kn_l_x", "kn_r_x", "an_l", "an_r", "sh_l", "sh_r", "sh_l_x", "sh_r_x", "el_l", "el_r",
			"el_l_x", "el_r_x", "wr_l", "wr_r"]:
		var i := _skel.find_bone(n)
		if i < 0:
			push_error("first-person body: no bone %s" % n)
			_skel = null
			return
		_bone[n] = i
		_rest_basis[n] = _skel.get_bone_global_rest(i).basis
	for s in ["l", "r"]:
		var sh := _skel.get_bone_global_rest(int(_bone["sh_" + s])).origin
		var el := _skel.get_bone_global_rest(int(_bone["el_" + s])).origin
		var wr := _skel.get_bone_global_rest(int(_bone["wr_" + s])).origin
		_upper_arm = sh.distance_to(el)
		_forearm = el.distance_to(wr)


# The frame tools/body/human.py gives a limb's bone: y along it, z toward `hint`.
static func _frame(y_axis: Vector3, hint: Vector3) -> Basis:
	var y := y_axis.normalized()
	var x := y.cross(hint).normalized()
	if x.length() < 0.5:
		x = y.cross(Vector3.RIGHT).normalized()
	return Basis(x, y, x.cross(y))


func _global(n: String) -> Transform3D:
	return _skel.get_bone_global_pose(int(_bone[n]))


func _set_local(n: String, global_basis: Basis, parent: String) -> void:
	var parent_basis := _global(parent).basis.orthonormalized()
	_skel.set_bone_pose_rotation(int(_bone[n]), (parent_basis.inverse() * global_basis.orthonormalized()).get_rotation_quaternion())


# One frame: `state` holds the native body's position (its capsule's centre),
# velocity, grounded, crouched, sliding, traversal, whether it rides a line,
# and the eye (the camera); `yaw` the view's.
func update_body(state: Dictionary, yaw: float, delta: float) -> void:
	if not visible or _skel == null:
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
	var eye_world: Vector3 = state.get("eye", soles + Vector3(0.0, Rig.EYE.y, 0.0))
	# Model space: soles at the origin under the capsule, his front (+z) the view's.
	var turn := Basis(Vector3.UP, yaw + PI)
	_root.global_transform = Transform3D(turn, soles)
	var to_model := _root.global_transform.affine_inverse()
	var local_v := turn.inverse() * velocity
	var flat := Vector2(local_v.x, local_v.z)
	var speed := flat.length()
	var ease := 1.0 - exp(-10.0 * delta)
	var lean_goal := 0.0
	if sliding:
		lean_goal = LEAN_SLIDE
	elif crouched:
		lean_goal = LEAN_CROUCH
	elif grounded:
		lean_goal = clampf(speed * 0.03, 0.0, 0.2)
	elif hanging:
		lean_goal = LEAN_HANG
	_lean = lerpf(_lean, lean_goal, ease)
	var walking := grounded and not sliding and speed >= 0.2
	if walking:
		_stride += 0.5 * Gait.cadence(speed) * delta
	var stance := clampf(Gait.contact_seconds(speed) * Gait.cadence(speed) * 0.5, 0.15, 0.65)
	var step := maxf(speed * Gait.contact_seconds(speed), 0.35)
	# The pelvis's own motion in a stride: down at each footfall, over the
	# foot it stands on, turned with the leg going through.
	var gait_k := clampf(speed / 1.4, 0.0, 1.0) if walking else 0.0
	var bob := -0.018 * gait_k * (0.5 + 0.5 * cos(_stride * TAU * 2.0))
	var sway := 0.014 * gait_k * sin(_stride * TAU)
	var pelvis_yaw := 0.07 * gait_k * sin(_stride * TAU)
	# The trunk: leaning from the neck, the pelvis turned in the stride, the
	# chest turned back square under the shoulders.
	var neck_at := to_model * eye_world + Vector3(sway, -NECK_BELOW_EYE + bob, -NECK_BEHIND_EYE)
	var body_i: int = _bone["body"]
	_skel.set_bone_pose_rotation(body_i, Quaternion(Vector3.UP, pelvis_yaw) * Quaternion(Vector3.RIGHT, _lean))
	_skel.set_bone_pose_position(body_i, _skel.get_bone_rest(body_i).origin)
	_skel.set_bone_pose_rotation(int(_bone["chest"]), Quaternion(Vector3.UP, -pelvis_yaw))
	_skel.set_bone_pose_rotation(int(_bone["neck_lo"]), Quaternion(Vector3.RIGHT, -_lean * 0.6))
	var off := neck_at - _global("neck_lo").origin
	_skel.set_bone_pose_position(body_i, _skel.get_bone_rest(body_i).origin + off)
	var hips := [_global("hp_l").origin, _global("hp_r").origin]
	var hip_mid: Vector3 = ((hips[0] as Vector3) + (hips[1] as Vector3)) * 0.5
	var forward_w := turn * Vector3(0.0, 0.0, 1.0)
	for side in 2:
		var x := -1.0 if side == 0 else 1.0
		var spot := Vector3(hip_mid.x + x * (CROUCH_STANCE_X if crouched else STANCE_X), 0.0,
			hip_mid.z + (CROUCH_FEET_AHEAD if crouched else 0.0))
		var goal := spot
		var roll := 0.0
		var grounded_foot := false
		if sliding:
			# Out in front, heels down, skidding.
			goal = Vector3(hip_mid.x + x * 0.17, 0.1 + 0.03 * side, hip_mid.z + 0.75)
			roll = HEEL_STRIKE * 1.5
		elif hanging:
			# Hanging under the grip: straight down, swaying a little; on a face,
			# stepping up it in turn.
			_sway += delta
			goal = Vector3(hip_mid.x + x * 0.11, hip_mid.y - Rig.THIGH - Rig.SHIN + 0.02,
				hip_mid.z + sin(_sway * 2.1 + float(side) * 2.1) * 0.06)
			if traversal == TRAVERSAL_CLIMBING:
				goal += Vector3(0.0, 0.18 * maxf(0.0, sin(_stride * TAU + float(side) * PI)) + 0.1, 0.18)
			roll = 0.35
		elif traversal in [TRAVERSAL_MANTLING, TRAVERSAL_VAULTING]:
			goal = Vector3(hip_mid.x + x * 0.12, hip_mid.y - 0.42, hip_mid.z + 0.25)
			roll = 0.2
		elif not grounded:
			# In the air: knees a little bent, the feet trailing; falling hard,
			# kicking.
			var trail := Vector3(-local_v.x, 0.0, -local_v.z) * 0.03
			goal = Vector3(hip_mid.x + x * 0.12, hip_mid.y - (Rig.THIGH + Rig.SHIN) * 0.85, hip_mid.z - 0.08) + trail.limit_length(0.3)
			if velocity.y < -9.0:
				_sway += delta
				goal.y += 0.12 * sin(_sway * 20.0 + float(side) * PI)
			roll = 0.3
		else:
			grounded_foot = true
		if not grounded_foot or not _ready_feet:
			# Off the ground: where the state puts it, quickly but never in a frame.
			var goal_w := _root.global_transform * goal
			_foot[side] = goal_w if not _ready_feet else (_foot[side] as Vector3).lerp(goal_w, 1.0 - exp(-24.0 * delta))
			_foot_yaw[side] = yaw
			_planted[side] = false
			_step_left[side] = 0.0
			_roll[side] = lerpf(float(_roll[side]), roll, 1.0 - exp(-14.0 * delta))
		elif walking:
			# Striding: down for the stance (held where it was put, rolling
			# heel to toe), then swung through to land ahead.
			var p := fposmod(_stride + 0.5 * float(side), 1.0)
			var dir := flat / speed
			var dir_m := Vector3(dir.x, 0.0, dir.y)
			if p < stance:
				if not _planted[side]:
					_planted[side] = true
					_foot[side] = _root.global_transform * (spot + dir_m * 0.5 * step)
					_foot_yaw[side] = yaw
				var s := p / stance
				_roll[side] = HEEL_STRIKE * (1.0 - smoothstep(0.0, 0.18, s)) + TOE_OFF * smoothstep(0.6, 1.0, s)
			else:
				if _planted[side]:
					_planted[side] = false
					_step_from[side] = _foot[side]
				var s := (p - stance) / (1.0 - stance)
				var land := _root.global_transform * (spot + dir_m * 0.5 * step)
				var lift := sin(s * PI) * clampf(0.08 + 0.03 * speed, 0.08, 0.3)
				_foot[side] = (_step_from[side] as Vector3).lerp(land, smoothstep(0.0, 1.0, s)) + Vector3(0.0, lift, 0.0)
				_foot_yaw[side] = yaw
				_roll[side] = TOE_OFF * (1.0 - smoothstep(0.0, 0.4, s)) + HEEL_STRIKE * smoothstep(0.6, 1.0, s)
			_step_left[side] = 0.0
		else:
			# Standing: the foot stays where it is; one left behind by a turn
			# or a crouch steps back under the body (one foot at a time).
			_planted[side] = true
			var spot_w := _root.global_transform * spot
			var flat_off := Vector2(spot_w.x - (_foot[side] as Vector3).x, spot_w.z - (_foot[side] as Vector3).z)
			if _step_left[side] <= 0.0 and _step_left[1 - side] <= 0.0 and \
					(flat_off.length() > RESTEP_M or absf(wrapf(yaw - float(_foot_yaw[side]), -PI, PI)) > 0.6):
				_step_left[side] = RESTEP_SECONDS
				_step_from[side] = _foot[side]
			if _step_left[side] > 0.0:
				_step_left[side] = maxf(0.0, float(_step_left[side]) - delta)
				var s: float = 1.0 - float(_step_left[side]) / RESTEP_SECONDS
				_foot[side] = (_step_from[side] as Vector3).lerp(spot_w, smoothstep(0.0, 1.0, s)) + Vector3(0.0, 0.07 * sin(s * PI), 0.0)
				_foot_yaw[side] = lerp_angle(float(_foot_yaw[side]), yaw, s)
			else:
				_foot[side] = Vector3((_foot[side] as Vector3).x, soles.y, (_foot[side] as Vector3).z)
			_roll[side] = lerpf(float(_roll[side]), 0.0, 1.0 - exp(-14.0 * delta))
		_pose_leg(side, to_model * (_foot[side] as Vector3), float(_foot_yaw[side]) - yaw, float(_roll[side]))
	_ready_feet = true
	_pose_shadow(state, to_model, turn)
	pelvis_world = _root.global_transform * _global("body").origin
	for side in 2:
		foot_world[side] = _foot[side]


# The leg to its foot: the foot laid at its yaw and roll with its ground point
# where it belongs (the heel the pivot when the toes are up, the toe when the
# heel is up), then thigh and shin by two-bone IK, the knee forward.
func _pose_leg(side: int, ground: Vector3, foot_yaw: float, roll: float) -> void:
	var s := "l" if side == 0 else "r"
	var heel: Vector3 = Rig.HEEL_L if side == 0 else Rig.HEEL_R
	var toe: Vector3 = Rig.TOE_L if side == 0 else Rig.TOE_R
	var rest: Basis = _rest_basis["an_" + s]
	var flat_basis := Basis(Vector3.UP, foot_yaw) * rest
	var foot_basis := Basis(Vector3.UP, foot_yaw) * Basis(Vector3.RIGHT, roll) * rest
	var mid := (heel + toe) * 0.5
	var pivot := heel if roll < 0.0 else toe if roll > 0.0 else mid
	# The pivot's ground point: where it lies with the foot flat on `ground`.
	var pivot_ground := ground + flat_basis * (pivot - mid)
	pivot_ground.y = ground.y + (flat_basis * pivot).y - (flat_basis * mid).y
	var ankle := pivot_ground - foot_basis * pivot
	var hip := _global("hp_" + s).origin
	var to := ankle - hip
	var thigh: float = Rig.THIGH
	var shin: float = Rig.SHIN
	var reach := clampf(to.length(), absf(thigh - shin) + 0.01, thigh + shin - 0.002)
	var dir := to.normalized() if to.length() > 1.0e-4 else Vector3.DOWN
	var knee_way := Basis(Vector3.UP, foot_yaw) * Vector3(0.12 * (1.0 if side == 1 else -1.0), 0.0, 1.0)
	var axis := dir.cross(knee_way)
	if axis.length() < 1.0e-4:
		axis = Vector3.RIGHT
	axis = axis.normalized()
	var a := acos(clampf((thigh * thigh + reach * reach - shin * shin) / (2.0 * thigh * reach), -1.0, 1.0))
	var knee := hip + dir.rotated(axis, a) * thigh
	var end := hip + dir * reach
	_set_local("hp_" + s, _frame(hip - knee, knee_way), "body")
	_set_local("kn_" + s, _frame(knee - end, knee_way), "hp_%s_x" % s)
	_set_local("an_" + s, foot_basis, "kn_%s_x" % s)


# His shadow's head and arms (the part only the shadow shows): the head
# pitched as the view is; each arm reaching from its shoulder for the wrist
# the view draws (first_person_arms.gd: right hand first), the elbow down and
# out, or hanging when there is no drawn hand.
func _pose_shadow(state: Dictionary, to_model: Transform3D, turn: Basis) -> void:
	var view: Basis = state.get("view", Basis())
	var look := turn.inverse() * (-view.z)
	var pitch := asin(clampf(look.y, -1.0, 1.0))
	_skel.set_bone_pose_rotation(int(_bone["neck_hi"]), Quaternion(Vector3.RIGHT, -pitch * 0.4))
	_skel.set_bone_pose_rotation(int(_bone["head"]), Quaternion(Vector3.RIGHT, -pitch * 0.6))
	var wrists: Array = state.get("wrists", [])
	for side in 2:
		var s := "l" if side == 0 else "r"
		var sh := _global("sh_" + s).origin
		var target: Vector3
		if wrists.size() == 2:
			target = to_model * (wrists[side] as Vector3)
		else:
			target = sh + Vector3(0.0, -(_upper_arm + _forearm) * 0.98, 0.02)
		var out := -1.0 if side == 0 else 1.0
		var pole := Vector3(0.2 * out, -1.0, -0.7).normalized()
		var to := target - sh
		var reach := clampf(to.length(), absf(_upper_arm - _forearm) + 0.01, _upper_arm + _forearm - 0.002)
		var dir := to.normalized() if to.length() > 1.0e-4 else Vector3.DOWN
		var bend := (pole - dir * pole.dot(dir))
		bend = bend.normalized() if bend.length() > 1.0e-4 else Vector3(out, 0.0, 0.0)
		var cos_a := clampf((_upper_arm * _upper_arm + reach * reach - _forearm * _forearm) / (2.0 * _upper_arm * reach), -1.0, 1.0)
		var elbow := sh + dir * (_upper_arm * cos_a) + bend * (_upper_arm * sqrt(maxf(0.0, 1.0 - cos_a * cos_a)))
		var wrist := sh + dir * reach
		_set_local("sh_" + s, _frame(sh - elbow, bend), "chest")
		_set_local("el_" + s, _frame(elbow - wrist, bend), "sh_%s_x" % s)


# For a test: the nearest the body comes to `point` (the lens), roughly: the
# distance to each joint less the body's girth there.
func nearest_to(point: Vector3) -> float:
	if _skel == null:
		return INF
	var nearest := INF
	var radii := {"neck_lo": 0.07, "neck_hi": 0.06, "chest": 0.16, "spine_mid": 0.15, "body": 0.16, "hp_l": 0.09,
		"hp_r": 0.09, "kn_l": 0.07, "kn_r": 0.07, "an_l": 0.07, "an_r": 0.07}
	for n in radii:
		var at := _root.global_transform * _global(n).origin
		nearest = minf(nearest, at.distance_to(point) - float(radii[n]))
	return nearest
