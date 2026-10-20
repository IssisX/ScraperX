extends RefCounted
## Drives a skinned MakeHuman-based body from matron_view.gd's pivots. The
## view's _pose() is untouched: it sets the same rotations on the same pivots;
## this copies them to bones. Three spine bones and the pelvis share the
## torso pivot's rotation (powers of one quaternion, so the chest ends at the
## torso pivot's exact orientation); two neck bones share the neck pivot's;
## a pivot's scale (the Turn's long limbs) becomes a slide of "_x" stretch
## bones, so the skin lengthens instead of the limb swelling.

const PELVIS_SHARE := 0.5
# Her age in her back: a fixed upper-back hunch (the dowager's hump), the neck
# bent back against it so her eyes still come level.
const KYPHOSIS := 0.24

var skel: Skeleton3D
var view: Node3D
var p: Dictionary
var bones: Array = []
var by_name := {}
var bone_idx := {}
var contacts := {}
var k_height := 1.0
var stand_body_y := 1.0
var debug: PackedStringArray = []


func setup(skeleton: Skeleton3D, rig: Dictionary, matron_view: Node3D) -> void:
	skel = skeleton
	view = matron_view
	p = view._p
	bones = rig["bones"]
	contacts = rig.get("contacts", {})
	for b in bones:
		by_name[b["name"]] = b
		bone_idx[b["name"]] = skel.find_bone(b["name"])
	_place_pivots()


func _v(a: Array) -> Vector3:
	return Vector3(float(a[0]), float(a[1]), float(a[2]))


func _pos(n: String) -> Vector3:
	return _v(by_name[n]["pos"])


func _t(n: String) -> Vector3:
	return _v(by_name[n]["t"])


func _euler(n: String) -> Vector3:
	return _v(by_name[n]["euler"])


# The new _build_figure's numbers: every pivot at a MakeHuman joint.
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
	# _pose's hip_y is in today's body (1.0 = standing): scale it to this one.
	k_height = stand_body_y / 1.0


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


# --- the contact pass (new, presentation only): planted feet, palms on the
# deck on all fours, hands on the rung or cable. _pose's angles stay the
# intent; this bends them just enough to touch what the native says is there.
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
	var S := _gpos("sh_" + s)
	var E := _gpos("el_" + s)
	var W := _gpos("wr_" + s)
	var l1 := S.distance_to(E)
	var l2 := E.distance_to(W)
	var to_t := target - S
	var d := clampf(to_t.length(), absf(l1 - l2) + 0.01, l1 + l2 - 0.005)
	var n := (E - S).cross(W - S)
	if n.length() < 1e-5:
		n = skel.get_bone_global_pose(bone_idx["el_" + s]).basis.x
	n = n.normalized()
	var a := acos(clampf((l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d), -1.0, 1.0))
	var dir := to_t.normalized()
	var e_new := S + dir.rotated(n, a) * l1
	if (e_new - S).cross(target - S).dot(n) < 0.0:
		e_new = S + dir.rotated(n, -a) * l1
	_turn_bone_global("sh_" + s, Quaternion((E - S).normalized(), (e_new - S).normalized()))
	var E2 := _gpos("el_" + s)
	var W2 := _gpos("wr_" + s)
	_turn_bone_global("el_" + s, Quaternion((W2 - E2).normalized(), (target - E2).normalized()))


func contact(data: Dictionary, falling: bool) -> void:
	var state := int(data.get("state", 0))
	var turn := int(data.get("turn", 0))
	var climbing := bool(data.get("climbing", false))
	var on_line := bool(data.get("on_line", false))
	var airborne := falling or float(data.get("_fall", 0.0)) > 1.0
	if climbing or on_line:
		var top := maxf(_gpos("wr_l").y, _gpos("wr_r").y)
		_lift_body(clampf(HANDS_ON_RUNG_Y - top - 0.1, 0.0, 0.8))
		return
	if airborne:
		return
	# feet on the deck: the body rides up or down to the lower foot
	_lift_body(clampf(-_lowest_foot(), -0.15, 0.3))
	var on_all_fours := (state == 3 and not bool(data.get("clinging", false))) and (float(data.get("speed", 0.0)) > 1.0 or turn == 2 or turn == 0)
	if not on_all_fours:
		return
	for s in ["l", "r"]:
		var w := _gpos("wr_" + s)
		var sh := _gpos("sh_" + s)
		if w.y < 0.8:
			# the palm goes down where the arm was swinging, not far ahead of the shoulder
			var tgt := Vector3(lerpf(sh.x, w.x, 0.6), DECK_WRIST_Y, lerpf(sh.z + 0.08, w.z, 0.5))
			_reach(s, tgt)
			debug.append("%s sh %s wr %s -> tgt %s got %s" % [s, str(sh), str(w), str(tgt), str(_gpos("wr_" + s))])
