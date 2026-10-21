extends RefCounted
## Her skinned body (life/matron/matron.glb, built by
## tools/body/build_matron_body.py) driven from matron_view.gd's pivots. _pose()
## sets the same rotations on the same pivots it always has; this copies them
## to the bones. Three spine bones and the pelvis share the torso pivot's
## rotation (powers of one quaternion, so the chest ends at the torso pivot's
## exact orientation); two neck bones share the neck pivot's; a pivot's scale
## (the Turn's long limbs) becomes a slide of the "_x" stretch bones, so the
## skin lengthens instead of the limb swelling.
##
## Then what the pivots never had: her feet planted on the deck, palms on it on
## all fours, hands up to the rung on a ladder (contact); her eyes turned in
## their sockets onto the player's (aim_eyes); and her hair, hung on chains of
## bones that gravity, her own motion and her skull and back swing (step_hair).

const PELVIS_SHARE := 0.5
# Her age in her back: a fixed upper-back hunch (the dowager's hump), the neck
# bent back against it so her eyes still come level.
const KYPHOSIS := 0.24
# How far her eyes turn in their sockets, and how often they dart while she
# has not found you (locked on you, they never leave you).
const EYE_YAW_MAX := 0.6
const EYE_PITCH_MAX := 0.45
const SACCADE_RATE := 2.2
const SACCADE_YAW := 0.09
const SACCADE_PITCH := 0.05
# Her hair: verlet points down each chain, damped, pulled a little back toward
# hanging from her head (the strands' own stiffness, less toward the tips),
# kept off her skull and her upper back. Greasy, it is heavy: it swings in
# clumps and settles without bouncing (the owner, 2026-10-20).
const HAIR_DAMP := 0.84
const HAIR_GRAVITY := Vector3(0.0, -9.81, 0.0)
const HAIR_STIFFNESS := [0.22, 0.12, 0.07, 0.04, 0.02]
# Each lock its own: its weight and give varied a little, so neighbors never
# move as one piece; and the wind up on the deck in slow gusts.
const LOCK_VARY := 0.35
const WIND_MS2 := 0.7
const WIND_RATE := 0.23
const WIND_DIR := Vector3(0.93, 0.0, 0.37)
const HAIR_ITERATIONS := 3
const HAIR_JUMP_M := 1.0      # a root that moved this far in a frame (a respawn) restarts the hang
const BACK_RADIUS := 0.17

var skel: Skeleton3D
var view: Node3D
var p: Dictionary
var bones: Array = []
var by_name := {}
var bone_idx := {}
var contacts := {}
var k_height := 1.0
var stand_body_y := 1.0
var _chains: Array = []
var _skull_local := Vector3.ZERO
var _skull_radius := 0.1
var _wind_clock := 0.0


func setup(skeleton: Skeleton3D, rig: Dictionary, matron_view: Node3D) -> void:
	skel = skeleton
	view = matron_view
	p = view._p
	bones = rig["bones"]
	contacts = rig.get("contacts", {})
	for b in bones:
		by_name[b["name"]] = b
		bone_idx[b["name"]] = skel.find_bone(b["name"])
		if int(bone_idx[b["name"]]) < 0:
			push_error("matron rig: the body has no bone %s" % b["name"])
	_place_pivots()
	var hair: Dictionary = rig.get("hair", {})
	var head_at := _pos("head")
	for c in hair.get("chains", []):
		var idx: Array = []
		for n in c["bones"]:
			idx.append(int(bone_idx[n]))
		var n_chain := _chains.size()
		_chains.append({"idx": idx, "root_local": _v(c["root"]) - head_at, "seg": float(c["segment_m"]),
			"pts": [], "prev": [], "damp": HAIR_DAMP + (_hash(n_chain * 3 + 1) - 0.5) * 0.08,
			"give": 1.0 + (_hash(n_chain * 3 + 2) - 0.5) * 2.0 * LOCK_VARY, "phase": _hash(n_chain * 3 + 3) * TAU})
	if hair.has("skull"):
		_skull_local = _v(hair["skull"]) - head_at
		_skull_radius = float(hair["skull_radius"])


func _v(a: Array) -> Vector3:
	return Vector3(float(a[0]), float(a[1]), float(a[2]))


func _pos(n: String) -> Vector3:
	return _v(by_name[n]["pos"])


func _t(n: String) -> Vector3:
	return _v(by_name[n]["t"])


func _euler(n: String) -> Vector3:
	return _v(by_name[n]["euler"])


# The pivots at her joints: the view's _pose() works on these.
func _place_pivots() -> void:
	var body: Vector3 = _pos("body")
	var waist: Vector3 = _pos("spine_lo")
	(p["body"] as Node3D).position = body
	(p["torso"] as Node3D).position = waist - body
	(p["neck"] as Node3D).position = _pos("neck_lo") - waist
	(p["head"] as Node3D).position = _pos("head") - _pos("neck_lo")
	(p["jaw"] as Node3D).position = _pos("jaw") - _pos("head") - Vector3(0.0, 0.1, 0.0)
	(p["hump"] as Node3D).position = _pos("hump") - waist
	(p["box_cord"] as Node3D).position = _pos("box") - body
	for s in ["l", "r"]:
		(p["sh_" + s] as Node3D).position = _pos("sh_" + s) - waist
		(p["el_" + s] as Node3D).position = _t("sh_%s_x" % s)
		(p["wr_" + s] as Node3D).position = _t("el_%s_x" % s)
		(p["wr_" + s] as Node3D).rotation = _euler("wr_" + s)
		(p["hp_" + s] as Node3D).position = _pos("hp_" + s) - body
		(p["kn_" + s] as Node3D).position = _t("hp_%s_x" % s)
		(p["an_" + s] as Node3D).position = _t("kn_%s_x" % s)
		(p["an_" + s] as Node3D).rotation = _euler("an_" + s)
		var chains: Array = p["fingers_" + s]
		for f in chains.size():
			var chain: Array = chains[f]
			for seg in chain.size():
				var bn := ("f%d%d_%s" % [f, seg, s]) if f < 4 else ("t%d_%s" % [seg, s])
				if not by_name.has(bn):
					continue
				var node: Node3D = chain[seg]
				node.position = _t(bn)
				node.rotation = _euler(bn)
	# Standing straight, legs plumb: how high the body pivot is over the soles.
	var hip_drop := body.y - _pos("hp_l").y
	var thigh: float = by_name["hp_l"]["length"]
	var shin: float = by_name["kn_l"]["length"]
	stand_body_y = hip_drop + thigh + shin + _pos("an_l").y
	# _pose's hip_y is in units of standing (1.0 = standing): scaled to her.
	k_height = stand_body_y


func _bone(n: String, pos: Vector3, rot: Quaternion, scl := Vector3.ONE) -> void:
	var i: int = bone_idx[n]
	if i < 0:
		return
	skel.set_bone_pose_position(i, pos)
	skel.set_bone_pose_rotation(i, rot)
	skel.set_bone_pose_scale(i, scl)


func _q(n: String) -> Quaternion:
	return (p[n] as Node3D).quaternion


func apply() -> void:
	var tq := _q("torso")
	var pel := Quaternion.IDENTITY.slerp(tq, PELVIS_SHARE)
	var bp: Vector3 = (p["body"] as Node3D).position
	_bone("body", Vector3(bp.x, bp.y * k_height, bp.z), pel)
	for n in ["spine_lo", "spine_mid", "chest"]:
		var hunch := Quaternion(Vector3(1.0, 0.0, 0.0), KYPHOSIS * (0.6 if n == "chest" else 0.4 if n == "spine_mid" else 0.0))
		_bone(n, _t(n), Quaternion.IDENTITY.slerp(tq, float(by_name[n]["share"])) * hunch)
	_bone("hump", _t("hump"), Quaternion.IDENTITY, (p["hump"] as Node3D).scale)
	var nq := _q("neck")
	_bone("neck_lo", _t("neck_lo"), Quaternion(Vector3(1.0, 0.0, 0.0), -KYPHOSIS * 0.8) * Quaternion.IDENTITY.slerp(nq, 0.5))
	_bone("neck_hi", _t("neck_hi"), Quaternion.IDENTITY.slerp(nq, 0.5))
	_bone("head", _t("head"), _q("head"))
	_bone("jaw", _t("jaw"), _q("jaw"))
	for s in ["l", "r"]:
		var arm_s := (p["sh_" + s] as Node3D).scale.y
		var leg_s := (p["hp_" + s] as Node3D).scale.y
		# the shoulder's rest frame hangs the arm just clear of her hips
		var abd := Quaternion(Vector3(0.0, 0.0, 1.0), float(by_name["sh_" + s].get("abduct", 0.0)))
		_bone("sh_" + s, _t("sh_" + s), abd * _q("sh_" + s))
		_bone("sh_%s_x" % s, _t("sh_%s_x" % s) * arm_s, Quaternion.IDENTITY)
		_bone("el_" + s, _t("el_" + s), _q("el_" + s))
		_bone("el_%s_x" % s, _t("el_%s_x" % s) * arm_s, Quaternion.IDENTITY)
		_bone("wr_" + s, _t("wr_" + s), _q("wr_" + s))
		var chains: Array = p["fingers_" + s]
		for f in chains.size():
			var chain: Array = chains[f]
			for seg in chain.size():
				var bn := ("f%d%d_%s" % [f, seg, s]) if f < 4 else ("t%d_%s" % [seg, s])
				if by_name.has(bn):
					_bone(bn, _t(bn), (chain[seg] as Node3D).quaternion)
		# the thighs keep the angle _pose gave them, whatever the pelvis did
		_bone("hp_" + s, _t("hp_" + s), pel.inverse() * _q("hp_" + s))
		_bone("hp_%s_x" % s, _t("hp_%s_x" % s) * leg_s, Quaternion.IDENTITY)
		_bone("kn_" + s, _t("kn_" + s), _q("kn_" + s))
		_bone("kn_%s_x" % s, _t("kn_%s_x" % s) * leg_s, Quaternion.IDENTITY)
		_bone("an_" + s, _t("an_" + s), _q("an_" + s))
	_bone("skirt", _t("skirt"), Quaternion.IDENTITY.slerp(_q("rags"), 0.6))
	_bone("box", _t("box"), pel.inverse() * _q("box_cord"))


func point(bone: String, key: String) -> Vector3:
	var i: int = bone_idx[bone]
	var local := _v(contacts[bone][key])
	return skel.global_transform * (skel.get_bone_global_pose(i) * local)


func bone_world(bone: String) -> Transform3D:
	return skel.global_transform * skel.get_bone_global_pose(int(bone_idx[bone]))


# --- the contact pass (presentation only): planted feet, palms on the deck on
# all fours, hands on the rung or cable. _pose's angles stay the intent; this
# bends them just enough to touch what the native says is there.
const DECK_WRIST_Y := 0.075        # a planted palm's wrist over the deck
const HANDS_ON_RUNG_Y := 2.6       # the native's kHandsAboveCentre + kStandCentre


func _gpos(n: String) -> Vector3:
	return skel.get_bone_global_pose(bone_idx[n]).origin


func _lowest_foot() -> float:
	var y := INF
	for s in ["l", "r"]:
		for key in ["heel", "toe"]:
			var i: int = bone_idx["an_" + s]
			y = minf(y, (skel.get_bone_global_pose(i) * _v(contacts["an_" + s][key])).y)
	return y


func _lift_body(dy: float) -> void:
	var i: int = bone_idx["body"]
	skel.set_bone_pose_position(i, skel.get_bone_pose_position(i) + Vector3(0.0, dy, 0.0))


func _turn_bone_global(n: String, q_world: Quaternion) -> void:
	var i: int = bone_idx[n]
	var parent := skel.get_bone_parent(i)
	var g := skel.get_bone_global_pose(i).basis
	var ng := Basis(q_world) * g
	var pg := skel.get_bone_global_pose(parent).basis if parent >= 0 else Basis()
	skel.set_bone_pose_rotation(i, (pg.inverse() * ng).get_rotation_quaternion())


# Two-bone reach: the wrist to `target`, the elbow kept in the plane it bent in.
func _reach(s: String, target: Vector3) -> void:
	var sh := _gpos("sh_" + s)
	var el := _gpos("el_" + s)
	var wr := _gpos("wr_" + s)
	var l1 := sh.distance_to(el)
	var l2 := el.distance_to(wr)
	var to_t := target - sh
	var d := clampf(to_t.length(), absf(l1 - l2) + 0.01, l1 + l2 - 0.005)
	var n := (el - sh).cross(wr - sh)
	if n.length() < 1e-5:
		n = skel.get_bone_global_pose(bone_idx["el_" + s]).basis.x
	n = n.normalized()
	var a := acos(clampf((l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d), -1.0, 1.0))
	var dir := to_t.normalized()
	var e_new := sh + dir.rotated(n, a) * l1
	if (e_new - sh).cross(target - sh).dot(n) < 0.0:
		e_new = sh + dir.rotated(n, -a) * l1
	_turn_bone_global("sh_" + s, Quaternion((el - sh).normalized(), (e_new - sh).normalized()))
	var el2 := _gpos("el_" + s)
	var wr2 := _gpos("wr_" + s)
	_turn_bone_global("el_" + s, Quaternion((wr2 - el2).normalized(), (target - el2).normalized()))


func contact(data: Dictionary, falling: bool) -> void:
	var state := int(data.get("state", 0))
	var turn := int(data.get("turn", 0))
	# Laid out on the steel, or turned over head first down her pipe: the figure
	# itself is turned, and the deck is not under her soles.
	if (state == 11 and not bool(data.get("kneeling", false))) or bool(data.get("pipe_head_down", false)):
		return
	var climbing := bool(data.get("climbing", false)) or bool(data.get("on_pipe", false))
	var on_line := bool(data.get("on_line", false))
	var airborne := falling or float(view._fall_speed) > 1.0
	if climbing or on_line:
		var top := maxf(_gpos("wr_l").y, _gpos("wr_r").y)
		_lift_body(clampf(HANDS_ON_RUNG_Y - top - 0.1, 0.0, 0.8))
		return
	if airborne:
		return
	# feet on the deck: the body rides up or down to the lower foot
	_lift_body(clampf(-_lowest_foot(), -0.15, 0.3))
	if int(data.get("capture_phase", 0)) in [1, 2]:
		# Mirror the native ankle attachment; the rig never pulls the body.
		var ankle: Vector3 = data["capture_ankle"]
		_reach("r", skel.global_transform.affine_inverse() * ankle)
		return
	# (A pest hurries after you upright: only the dangerous and the changed run
	# on all fours, and the Turn's lunge.)
	var on_all_fours := state == 3 and not bool(data.get("clinging", false)) and \
		((float(data.get("speed", 0.0)) > 1.0 and int(data.get("stage", 0)) > 0) or turn == 2)
	if not on_all_fours:
		return
	for s in ["l", "r"]:
		var w := _gpos("wr_" + s)
		var sh := _gpos("sh_" + s)
		if w.y < 0.8:
			# the palm goes down where the arm was swinging, not far ahead of the shoulder
			_reach(s, Vector3(lerpf(sh.x, w.x, 0.6), DECK_WRIST_Y, lerpf(sh.z + 0.08, w.z, 0.5)))


# --- her eyes: each turned in its socket onto `target` (the player's eye), as
# far as a socket lets an eye turn. Until she has found you they dart, a few
# times a second; once she has, they hold on you.
func aim_eyes(target: Vector3, locked: bool, clock: float) -> void:
	var head := bone_world("head")
	var dart := Vector2.ZERO
	if not locked:
		var beat := int(floorf(clock * SACCADE_RATE))
		dart = Vector2((_hash(beat * 2 + 1) - 0.5) * 2.0 * SACCADE_YAW, (_hash(beat * 2 + 2) - 0.5) * 2.0 * SACCADE_PITCH)
	for s in ["l", "r"]:
		var i: int = bone_idx.get("eye_" + s, -1)
		if i < 0:
			continue
		var at := (skel.global_transform * skel.get_bone_global_pose(i)).origin
		var to := target - at
		if to.length() < 0.05:
			continue
		var d := (head.basis.inverse() * to).normalized()
		var yaw := clampf(atan2(d.x, d.z) + dart.x, -EYE_YAW_MAX, EYE_YAW_MAX)
		var pitch := clampf(asin(clampf(d.y, -1.0, 1.0)) + dart.y, -EYE_PITCH_MAX, EYE_PITCH_MAX)
		skel.set_bone_pose_rotation(i, Quaternion.from_euler(Vector3(-pitch, yaw, 0.0)))


# Where an eye looks, in the world (its bone's +z): for a test.
func eye_forward(s: String) -> Vector3:
	return bone_world("eye_" + s).basis.z.normalized()


func eye_position(s: String) -> Vector3:
	return bone_world("eye_" + s).origin


func _hash(n: int) -> float:
	var v := sin(float(n) * 12.9898) * 43758.5453
	return v - floorf(v)


# --- her hair, after the bones above it are posed.
func step_hair(delta: float) -> void:
	if _chains.is_empty():
		return
	var dt := clampf(delta, 1.0 / 240.0, 1.0 / 30.0)
	_wind_clock += dt
	var head := bone_world("head")
	var skull := head * _skull_local
	var chest := bone_world("chest")
	var back := chest * Vector3(0.0, 0.05, -0.05)
	var down := head.basis * Vector3.DOWN
	for ch in _chains:
		var idx: Array = ch["idx"]
		var n := idx.size()
		var seg: float = ch["seg"]
		var root: Vector3 = head * (ch["root_local"] as Vector3)
		var pts: Array = ch["pts"]
		var prev: Array = ch["prev"]
		if pts.size() != n + 1 or (pts[0] as Vector3).distance_to(root) > HAIR_JUMP_M:
			pts.clear()
			prev.clear()
			for k in n + 1:
				pts.append(root + down * seg * float(k))
				prev.append(root + down * seg * float(k))
		pts[0] = root
		prev[0] = root
		var damp: float = ch["damp"]
		var give: float = ch["give"]
		# A gust rises and falls on its own, each lock catching it a little out of
		# step with its neighbors; the tips catch the most of it.
		var phase: float = ch["phase"]
		var gust := maxf(0.0, sin(TAU * WIND_RATE * _wind_clock)) * (0.6 + 0.4 * sin(TAU * WIND_RATE * 3.1 * _wind_clock + phase))
		var wind := WIND_DIR * WIND_MS2 * gust
		for k in range(1, n + 1):
			var cur: Vector3 = pts[k]
			var vel: Vector3 = (cur - (prev[k] as Vector3)) * damp
			prev[k] = cur
			pts[k] = cur + vel + (HAIR_GRAVITY + wind * (float(k) / float(n))) * dt * dt
		for _it in HAIR_ITERATIONS:
			for k in range(1, n + 1):
				var above: Vector3 = pts[k - 1]
				var q: Vector3 = (pts[k] as Vector3).lerp(above + down * seg, float(HAIR_STIFFNESS[k - 1]) * give / float(HAIR_ITERATIONS))
				var d := q - above
				q = above + (d.normalized() if d.length() > 1e-6 else down) * seg
				q = _outside(q, skull, _skull_radius)
				q = _outside(q, back, BACK_RADIUS)
				pts[k] = q
		# The bones from the points: each turned from hanging (its rest, down its
		# parent) to the way its strand now runs.
		var parent_basis := head.basis
		for k in n:
			var bi: int = idx[k]
			var dir: Vector3 = ((pts[k + 1] as Vector3) - (pts[k] as Vector3)).normalized()
			var local_dir := (parent_basis.inverse() * dir).normalized()
			var rot := Quaternion(Vector3.DOWN, local_dir) if local_dir.dot(Vector3.DOWN) > -0.999 else Quaternion(Vector3.RIGHT, PI)
			skel.set_bone_pose_rotation(bi, rot)
			parent_basis = parent_basis * Basis(rot)


func _outside(q: Vector3, centre: Vector3, radius: float) -> Vector3:
	var d := q - centre
	var l := d.length()
	if l >= radius or l < 1e-6:
		return q
	return centre + d / l * radius
