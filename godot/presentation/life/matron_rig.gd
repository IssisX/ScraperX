extends RefCounted
## Her skinned body (life/matron/matron.glb, built by
## tools/body/build_matron_body.py) driven from matron_view.gd's pivots. _pose()
## sets the same rotations on the same pivots it always has; this copies them
## to the bones. Three spine bones and the pelvis share the torso pivot's
## rotation (powers of one quaternion, so the chest ends at the torso pivot's
## exact orientation); two neck bones share the neck pivot's; a leg pivot's
## scale (the Turn's long limbs) becomes a slide of the "_x" stretch bones, so
## the skin lengthens instead of the limb swelling. An arm pivot's scale
## wrenches the shoulder out of its socket instead: her arm's bones are never
## stretched (rule 33).
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
	_ankle_y = _pos("an_l").y


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
	# each neck bone half the neck's turn, halved by angle (the Turn's look
	# back over her shoulder goes past half a turn: never the short way round)
	var nq := Quaternion.from_euler((p["neck"] as Node3D).rotation * 0.5)
	_bone("neck_lo", _t("neck_lo"), Quaternion(Vector3(1.0, 0.0, 0.0), -KYPHOSIS * 0.8) * nq)
	_bone("neck_hi", _t("neck_hi"), nq)
	_bone("head", _t("head"), _q("head"))
	_bone("jaw", _t("jaw"), _q("jaw"))
	for s in ["l", "r"]:
		var arm_s := (p["sh_" + s] as Node3D).scale.y
		var leg_s := (p["hp_" + s] as Node3D).scale.y
		# the shoulder's rest frame hangs the arm just clear of her hips
		var abd := Quaternion(Vector3(0.0, 0.0, 1.0), float(by_name["sh_" + s].get("abduct", 0.0)))
		var sh_rot := abd * _q("sh_" + s)
		# The Turn's long arms (arm_s over 1): her arm's own bones keep their
		# length (rule 33: her real arms, never stretched); her shoulder is
		# wrenched out of its socket along the upper arm by what the arm
		# would have gained, so the arm still reaches that far out of her.
		var upper := _t("sh_%s_x" % s)
		var out := maxf(arm_s - 1.0, 0.0) * (upper.length() + _t("el_%s_x" % s).length())
		var socket := _t("sh_" + s)
		if out > 0.0 and upper.length() > 1.0e-4:
			socket += sh_rot * (upper.normalized() * out)
		_bone("sh_" + s, socket, sh_rot)
		_bone("sh_%s_x" % s, upper, Quaternion.IDENTITY)
		_bone("el_" + s, _t("el_" + s), _q("el_" + s))
		_bone("el_%s_x" % s, _t("el_%s_x" % s), Quaternion.IDENTITY)
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


# Her look (matron_view.gd _turn_yaw): the turn her body has not made, laid
# on her neck first and her chest the rest, each about the vertical, so her
# face keeps its pitch on her stooped trunk.
	_twist_look()


func _twist_look() -> void:
	var tw: float = float(view._twist_shown) if view != null else 0.0
	if absf(tw) < 1.0e-4:
		return
	var on_neck := clampf(tw, -LOOK_NECK_MAX, LOOK_NECK_MAX)
	if absf(tw - on_neck) > 1.0e-4:
		_turn_bone_global("chest", Quaternion(Vector3.UP, tw - on_neck))
	_turn_bone_global("neck_lo", Quaternion(Vector3.UP, on_neck))


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
const ANKLE_OVER_SOLE_M := 0.09    # her ankle over her sole, a foot set down
const HANDS_ON_RUNG_Y := 2.6       # the native's kHandsAboveCentre + kStandCentre
const RIDE_W := 7.0               # rad/s: her body's ride onto her feet, low-passed
# On all fours, the order her limbs go down in a cycle of her gait (hind
# left, fore left, hind right, fore right) and how high a limb is lifted.
const PLANT_LIMBS := [["an_l", 0.0], ["wr_l", 0.25], ["an_r", 0.5], ["wr_r", 0.75]]
const PLANT_LIFT_M := 0.12
# Her feet off her floor (the simulation's): a limb that was held to it goes
# where its phase has it under her body, eased this fast from where it was drawn.
const RELEASE_TAU := 0.05
# How fast a limb's pole turns to the side its pose bends to (rad/s).
const POLE_RAD_S := 6.0
# Her gripping arm drawn out to the held ankle, at most this many times its length.
const HOLD_STRETCH_MAX := 2.7
# Her look laid on her neck to this far (matron_view.gd NECK_TWIST_MAX), the
# rest on her chest.
const LOOK_NECK_MAX := 1.92
# and past that, her shoulder pulled out toward it at most this far
const HOLD_SHOULDER_M := 0.4
# drawn out so the arm is this far out to its length at the ankle (bent a
# little: never locked straight)
const HOLD_ARM_OUT := 0.93
# The last SOFT_REACH of a limb's length it comes out to only softly (_reach_chain).
const SOFT_REACH := 0.08
# No bone of her body turns faster than this a second (GOVERN_BURST_RAD_S in
# a held burst of the contortion layer), whatever the reaches asked: the
# last word on every frame (govern).
const GOVERN_RAD_S := 11.5
const GOVERN_BURST_RAD_S := 19.0
# and no bone of hers slides (her hips' ride, her arm drawn out, her shoulder
# pulled) faster than this a second (GOVERN_BURST_M_S in a burst)
const GOVERN_M_S := 3.0
const GOVERN_BURST_M_S := 5.0
var _ride_y := 0.0
var _ride_v := 0.0
var _ankle_y := 0.097
var _plants := {}
# Each reach's pole (frame, the unit side its middle joint stands out to).
var _pole := {}
var _governed_bones: Array[int] = []
var _govern_last: Array[Quaternion] = []
var _govern_pos: Array[Vector3] = []
var governed := 0
var _ik_frame := 0
# This frame: the limbs she put down on all fours ({limb: "an_l" / "wr_l" /
# "an_r" / "wr_r", at: the world point, surface: what it went down on}), and
# each arm's stretch.
var landed: Array[Dictionary] = []
# What this frame's contact pass drew as so (CLAIM_*), for the view's check
# against the simulation's own contacts (matron_view.gd _check_contacts).
const CLAIM_PLANTS := 1     # a limb planted on her floor (all fours)
const CLAIM_RIDE := 2       # her body ridden onto her floor
const CLAIM_HAND2 := 4      # her second hand on a second hold
const CLAIM_FEET := 8       # her body lifted onto a floor under her feet in a climb
const CLAIM_HAND := 16      # her hand reaching to a climb's hold (or its target)
const CLAIM_HANG := 32      # her body hung from her hands (rungs, pipe, line)
const CLAIM_BRACE := 64     # her feet against her pipe's riser
var claims := 0
# The limbs her gait has down this frame (bits 1 an_l, 2 wr_l, 4 an_r, 8 wr_r).
var down_mask := 0
var stretch := {"l": 1.0, "r": 1.0}
# How far the handle of a prop (the toolbox) lies past her real arm, out to
# HOLD_ARM_OUT of it (negative: within it; -1 with none): walking to it, from
# where her shoulder will be at the take; carried, from where it is now. The
# view stoops her to it (matron_view.gd _prop_lean).
var prop_short := -1.0
# Seconds until the native takes that prop at her speed (0 once carried; INF
# with none), so the view's stoop leads the take (matron_view.gd _prop_lean).
var prop_take_s := INF
# and whether that handle is above her shoulder (carried and lifted: a stoop
# only takes her shoulder further from it), and whether she carries it
var prop_over := false
var prop_carried := false
# Where the native takes a prop: her centre within this of it across the
# ground (matron.cpp kTakeM; mirrored, the native is the authority).
const PROP_TAKE_M := 0.85
# Her hand goes out to a prop's handle over this long before the take.
const PROP_HAND_LEAD_S := 0.35
# Keep the last governed arm pose while a physical prop is being approached or
# carried. Restarting the IK from unrelated walk/search poses each frame can
# pull a wrist off an otherwise stationary handle before govern clips it.
const PROP_ARM_JOINTS := ["sh_l", "sh_l_x", "el_l", "el_l_x",
	"sh_r", "sh_r_x", "el_r", "el_r_x"]
var _prop_arm_pose := {}


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


# Moves a bone by `delta` in skeleton space (through its parent's frame).
func _shift_bone_global(n: String, delta: Vector3) -> void:
	var i: int = bone_idx[n]
	var parent := skel.get_bone_parent(i)
	var pb := skel.get_bone_global_pose(parent).basis if parent >= 0 else Basis.IDENTITY
	skel.set_bone_pose_position(i, skel.get_bone_pose_position(i) + pb.inverse() * delta)


func _turn_bone_global(n: String, q_world: Quaternion) -> void:
	var i: int = bone_idx[n]
	var parent := skel.get_bone_parent(i)
	var g := skel.get_bone_global_pose(i).basis
	var ng := Basis(q_world) * g
	var pg := skel.get_bone_global_pose(parent).basis if parent >= 0 else Basis()
	skel.set_bone_pose_rotation(i, (pg.inverse() * ng).get_rotation_quaternion())


# Two-bone reach: the wrist to `target`, the elbow kept in the plane it bent in.
func _reach(s: String, target: Vector3, w := 1.0, key := "") -> void:
	_reach_chain("sh_" + s, "el_" + s, "wr_" + s, target, w, key)


# The same for any two-bone limb: `c` (wrist, ankle) to `target` (from where
# the pose has it, as far as `w`). The middle joint (elbow, knee) stands out
# toward the limb's pole: the side it stood out to last frame, turned toward
# the pose's own side no faster than POLE_RAD_S (the pose's side the first
# time). No choice between two ways to bend, so it never flips round.
func _reach_chain(a: String, b: String, c: String, target: Vector3, w := 1.0, key := "") -> void:
	if key == "":
		key = c
	var sh := _gpos(a)
	var el := _gpos(b)
	var wr := _gpos(c)
	var want := wr.lerp(target, clampf(w, 0.0, 1.0))
	var l1 := sh.distance_to(el)
	var l2 := el.distance_to(wr)
	var to_t := want - sh
	if to_t.length() < 1.0e-4 or l1 < 1.0e-4 or l2 < 1.0e-4:
		return
	var dir := to_t.normalized()
	# a soft end to the reach: the limb comes out toward straight ever more
	# slowly and never quite gets there, so a target at the end of its reach
	# never spins the middle joint round
	var reach := l1 + l2
	var soft := SOFT_REACH * reach
	var d := to_t.length()
	if d > reach - soft:
		d = reach - soft + soft * (1.0 - exp(-(d - reach + soft) / soft))
	d = maxf(d, absf(l1 - l2) + 0.01)
	var pose_side := (el - sh) - dir * (el - sh).dot(dir)
	var last: Array = _pole.get(key, [])
	var q: Vector3
	if last.is_empty() or int(last[0]) < _ik_frame - 1:
		q = pose_side
	else:
		q = last[1]
		q -= dir * q.dot(dir)
		if pose_side.length() > 0.02 * l1 and q.length() > 1.0e-6:
			var to := pose_side.normalized()
			var qn := q.normalized()
			var turn := atan2(dir.dot(qn.cross(to)), qn.dot(to))
			var dt: float = float(view._dt) if view != null else 0.0
			q = qn.rotated(dir, clampf(turn, -POLE_RAD_S * dt, POLE_RAD_S * dt))
	if q.length() < 1.0e-6:
		# no side to stand out to (a limb straight at its target): any
		# square to it
		q = dir.cross(Vector3.UP if absf(dir.y) < 0.9 else Vector3.RIGHT)
	q = (q - dir * q.dot(dir)).normalized()
	_pole[key] = [_ik_frame, q]
	var ang := acos(clampf((l1 * l1 + d * d - l2 * l2) / (2.0 * l1 * d), -1.0, 1.0))
	var e_new := sh + (dir * cos(ang) + q * sin(ang)) * l1
	_turn_bone_global(a, Quaternion((el - sh).normalized(), (e_new - sh).normalized()))
	var el2 := _gpos(b)
	var wr2 := _gpos(c)
	_turn_bone_global(b, Quaternion((wr2 - el2).normalized(), (want - el2).normalized()))


# Her body's ride onto what is under her (her lower foot on the deck, her
# hands up to the rung), low-passed (RIDE_W): her hips carried smoothly over
# her feet, never jumped a frame to whichever foot is lowest.
func _ride(dy: float) -> void:
	var dt: float = float(view._dt) if view != null else 0.0
	if view == null or bool(view.fresh_frame):
		_ride_y = dy
		_ride_v = 0.0
	elif dt <= 0.0:
		pass   # a frame of no time: held
	else:
		var r: Array = view.spring(_ride_y, _ride_v, dy, RIDE_W, dt)
		_ride_y = float(r[0])
		_ride_v = float(r[1])
	_lift_body(_ride_y)


# On all fours, her four limbs walk (matron_view.gd's gait phase and cycle):
# each one planted on the deck and held where it was put (world-locked) while
# her body goes over it, lifted when the pose is behind it, and swung along a
# low arc to where it goes down next, ahead. As much as she is on all fours
# (`w`): from the pose's own limb toward the plant. A limb put down is
# reported in `landed`, for her sound. `free`: the simulation has nothing
# under her feet, so no limb is held to a floor: from that frame each one its
# phase has down goes where that phase has it under her body (eased from where
# it was drawn, RELEASE_TAU), and is held to the floor again, from there, once
# her feet are on one.
func _plant_limbs(data: Dictionary, w: float, free := false) -> void:
	var phase: float = view._gait_phase
	var cycle: float = view._gait_cycle_m
	var dt: float = view._dt
	var speed := float(data.get("speed", 0.0))
	var xf := skel.global_transform
	var inv := xf.affine_inverse()
	var fresh: bool = bool(view.fresh_frame) or _plants.is_empty()
	var gait_mps: float = float(view._gait_mps)
	var lift_h := PLANT_LIFT_M * clampf(gait_mps / 1.5, 0.0, 1.0)
	down_mask = 0
	var bit := 1
	var surface := String(data.get("surface", "steel"))
	for limb in PLANT_LIMBS:
		var bone: String = limb[0]
		var hand := bone.begins_with("wr")
		var s := bone.substr(3)
		var root_b := ("sh_" if hand else "hp_") + s
		var mid_b := ("el_" if hand else "kn_") + s
		var root := _gpos(root_b)
		var mid := _gpos(mid_b)
		var tip := _gpos(bone)
		var length := root.distance_to(mid) + mid.distance_to(tip)
		var floor_y := DECK_WRIST_Y if hand else _ankle_y
		var neutral := Vector3(root.x * 1.1, floor_y, root.z + (0.1 if hand else -0.02))
		var reach_h := sqrt(maxf(0.0, pow(0.92 * length, 2.0) - pow(maxf(0.0, root.y - floor_y), 2.0)))
		var s_half := clampf(0.8 * reach_h, 0.05, 0.45)
		var duty := clampf(2.0 * s_half / maxf(cycle, 0.1), 0.25, 0.7)
		var u := fposmod(phase / TAU + float(limb[1]), 1.0)
		var now := u < duty
		var down := neutral + Vector3(0.0, 0.0, s_half)
		var st: Dictionary = _plants.get(bone, {})
		var target: Vector3
		if fresh or st.is_empty():
			# no walk before: each limb where its phase has it
			var x := s_half - 2.0 * s_half * (u / duty) if now else -s_half + 2.0 * s_half * ((u - duty) / (1.0 - duty))
			target = neutral + Vector3(0.0, 0.0, x)
			st = {"stance": now, "lock": xf * target, "from": target, "at": target}
		elif now and free:
			var x := s_half - 2.0 * s_half * (u / duty)
			target = (st["at"] as Vector3).lerp(neutral + Vector3(0.0, 0.0, x), 1.0 - exp(-dt / RELEASE_TAU))
			st["lock"] = xf * target
		elif now:
			if not bool(st["stance"]):
				# down: where the swing has it now (at its end, unless her gait
				# changed under it)
				st["lock"] = xf * (st["at"] as Vector3)
				if w > 0.5 and gait_mps > 0.8:
					landed.append({"limb": bone, "at": st["lock"], "surface": surface})
			target = inv * (st["lock"] as Vector3)
			# dragged too far from under her (a turn, a stop): it slides back
			if Vector2(target.x - neutral.x, target.z - neutral.z).length() > 1.6 * s_half + 0.1:
				st["lock"] = (st["lock"] as Vector3).lerp(xf * neutral, 1.0 - exp(-6.0 * dt))
				target = inv * (st["lock"] as Vector3)
		else:
			if bool(st["stance"]):
				st["from"] = inv * (st["lock"] as Vector3)
			var sw := clampf((u - duty) / (1.0 - duty), 0.0, 1.0)
			var e := sw * sw * (3.0 - 2.0 * sw)
			target = (st["from"] as Vector3).lerp(down, e) + Vector3(0.0, lift_h * sin(PI * sw), 0.0)
		st["stance"] = now
		if now and not free:
			down_mask |= bit
		bit *= 2
		st["at"] = target
		_plants[bone] = st
		_reach_chain(root_b, mid_b, bone, target, w, "plant_" + bone)


# Upright feet, world-locked while they stand (M6 foot-drag fix): the plants
# above only run on all fours, so upright a stance foot rode the body and
# the probe measured it travelling ~0.037 m/frame median. A foot down on the
# deck stays where it was put while her body goes over it, and steps when it
# must. The pose's swing still decides when a foot comes down (its sole
# within UPLANT_M of the deck) and when it steps (lifted, or dragged past
# its leg's reach); this holds only the stance foot's place, never her
# gait's timing or step length. A stepping foot is never handed back to the
# pose mid-swing at deck height, where it would count as planted while it
# travels: it eases off on a low arc like the fours' swing (PLANT_LIFT_M),
# then tracks the pose's ankle held above the deck until the pose's foot
# comes down again, and plants there. Silent: no claim, no sound.
const UPLANT_M := 0.03
const UPLANT_RELEASE_S := 0.15
const UPLANT_LIFT_M := 0.12
const UPLANT_TRACK_LIFT_M := 0.15
# A release lifts before it travels: the sole must be off the deck before
# the foot moves sideways, or the deck counts the travel as a slide. The
# lift needs a few frames to bend a near-straight leg past govern's cap,
# so sideways travel waits until this much of the release has passed.
const UPLANT_TRAVEL_START := 0.4
# ... and the travel only advances while the sole is actually up: the
# lift alone does not hold it there while the leg reaches forward (the
# leg stretches and the sole drops back to the deck, act 0.111 m at the
# unrig), so the travel waits on the drawn sole's height above its lock,
# not on the clock. A frame old it lags the drop by a frame, so the bar
# sits above the plant band by that drop.
const UPLANT_TRAVEL_H_M := 0.10
# The proof pin only holds a foot while the body is nearly still — the
# check's own plant bar (STILL_MPS). A stepping body must be free to step:
# pinning a foot mid-stride holds it off the deck (figure carry) and starves
# the planted-pair count (studio shuffle).
const UPLANT_STILL_MPS := 0.15
# ... and while the foot itself is nearly still in the world (its pose-sole
# travel this frame, body-corrected — the same dev the slow test measures).
# A foot travelling fast is mid-step or mid-transition, not settling: the
# pin's anchor is the drawn sole a frame old, and a foot moving 0.10+ m in
# that frame strands the anchor — and the lock after it — far behind the
# stance (figure carry: the lock landed 0.63 m behind the foot, the leg
# could not reach the deck, lowest sole 0.048 m). A settling foot in the
# grab travels 0.024–0.034 m/frame; a pose transition travels 0.10+; the
# bar sits between them, at the slow test's own floor allowance.
const UPLANT_PIN_DEV_M := 0.05
var _uplants := {}
var _uprev := {}
# The soles as drawn last frame (after govern), world points: the upright
# plant's proof anchors to where the foot actually is, not where the pose
# has it — the two differ by ~0.18 m in a grab settle, and a pose anchor
# yanks the foot across the deck (v2: 0.090 m at frame 33).
var _udrawn := {}


func _sole_offset(s: String) -> Vector3:
	# The sole's midpoint, in the ankle bone's own space: the probe and the
	# test measure the sole, not the ankle, and the foot's pitch moves the
	# sole even when the ankle is held. Holding the sole itself removes
	# that pitch travel from the measured slide.
	return (_v(contacts["an_" + s]["heel"]) + _v(contacts["an_" + s]["toe"])) * 0.5


func _reach_sole(hip: String, knee: String, ankle: String, sole_target: Vector3, w: float, key: String, s: String) -> void:
	# Reach the ankle so the SOLE lands on sole_target. One pass: the
	# ankle's rotation moves the sole as it pitches, so a second pass
	# does not converge it (govern clips the extra change instead, and
	# the hold degrades at speed), and the landing below spreads the
	# descent so the single pass lands cleanly.
	var basis := skel.get_bone_global_pose(bone_idx[ankle]).basis
	_reach_chain(hip, knee, ankle, sole_target - basis * _sole_offset(s), w, key)


func _sole_skel(s: String) -> Dictionary:
	var i: int = bone_idx["an_" + s]
	var heel: Vector3 = skel.get_bone_global_pose(i) * _v(contacts["an_" + s]["heel"])
	var toe: Vector3 = skel.get_bone_global_pose(i) * _v(contacts["an_" + s]["toe"])
	return {"y": minf(heel.y, toe.y),
		"mid": Vector3((heel.x + toe.x) * 0.5, 0.0, (heel.z + toe.z) * 0.5)}


func _plant_upright_feet(wf: float, release_only := false) -> void:
	var w := 1.0 - wf
	if w <= 0.001 or view == null:
		return
	var dt: float = float(view._dt)
	var xf := skel.global_transform
	var inv := xf.affine_inverse()
	if bool(view.fresh_frame):
		_uplants.clear()
		_uprev.clear()
		_udrawn.clear()
	# A plant the fallthrough skipped a frame for (a climb, a leap) is
	# stale: the foot has gone elsewhere since, so it is dropped, never
	# dragged back.
	for s in ["l", "r"]:
		var old: Dictionary = _uplants.get(s, {})
		if not old.is_empty() and _ik_frame - int(old.get("frame", 0)) > 1:
			_uplants.erase(s)
	for s in ["l", "r"]:
		var sole := _sole_skel(s)
		var down := float(sole["y"]) > -UPLANT_M and float(sole["y"]) < UPLANT_M
		# The pose's own ankle, before this foot is reached for: apply()
		# reset the bones to the pose's intent this frame, so this is where
		# the pose has the foot, not where the last plant put it.
		var pose_ankle := _gpos("an_" + s)
		var st: Dictionary = _uplants.get(s, {})
		if release_only and not st.is_empty() and not bool(st.get("releasing", false)):
			# Taking off with a plant held (or a foot mid-landing, or
			# tracking): ease it back to the pose over the release,
			# from where the foot is drawn — never snap it to the pose.
			# The release branch below runs this same frame (the trigger
			# frame blends at k = 0, so the foot stays where it is). The
			# lock is held in skeleton space (body-relative): the world
			# point it was planted on is behind her now, and chasing it
			# as the body falls yanks the foot up.
			var at: Vector3
			var drawn_r: Dictionary = _udrawn.get(s, {})
			if not drawn_r.is_empty() and int(drawn_r.get("frame", -1)) == _ik_frame - 1:
				at = drawn_r["at"] as Vector3
			elif st.has("lock"):
				at = st["lock"] as Vector3
			else:
				var mid_r: Vector3 = sole["mid"] as Vector3
				at = xf * Vector3(mid_r.x, 0.0, mid_r.z)
			st = {"lock": at, "lock_skel": inv * at, "releasing": true, "rel": -dt / UPLANT_RELEASE_S, "frame": _ik_frame}
			_uplants[s] = st
		# A hold that ends this frame starts its release blend NOW, in
		# this frame's releasing branch below — never a frame later. The
		# old code set releasing and reached nothing that frame, so the
		# foot snapped from its lock to the pose's foot in one frame
		# (a 0.19 m jump at the end of a turn, where the crouch into
		# the next pose takes the sole out of the band). A hold ends
		# when the sole leaves the band, or when the body drags the lock
		# past the leg's reach. The trigger frame itself blends at
		# k = 0, so the foot stays at its lock for that frame instead
		# of snapping — and the release then runs the old frame count
		# exactly, so every later touchdown lands on the old beat (the
		# plant state machine is pose-driven; shifting the release by
		# even one frame moves later touchdowns by several, which is
		# what broke the figure test's carry pose under the first cut
		# of this fix).
		if st.has("lock") and not bool(st.get("tracking", false)) and not bool(st.get("releasing", false)) and int(st.get("landing", 0)) == 0:
			var dragged := false
			if down:
				var hip := _gpos("hp_" + s)
				var leg := hip.distance_to(_gpos("kn_" + s)) + _gpos("kn_" + s).distance_to(_gpos("an_" + s))
				var ls: Vector3 = inv * (st["lock"] as Vector3)
				dragged = Vector2(ls.x - hip.x, ls.z - hip.z).length() > 0.92 * leg
			if not down or dragged:
				st["releasing"] = true
				st["rel"] = -dt / UPLANT_RELEASE_S
				st["frame"] = _ik_frame
				st.erase("proof")
				_uplants[s] = st
		if bool(st.get("tracking", false)):
			# Touchdown needs three frames of sole on the deck, not one,
			# and a foot that has nearly stopped in the world, not one
			# sweeping low through the band: a sole grazing the deck
			# mid-swing must not plant, or the hold fights the swing and
			# the foot oscillates. While waiting, the foot keeps
			# hovering, so no counted frame travels. The pose's sole is
			# the signal, not its ankle: a dragging toe touches down
			# while its ankle is still high.
			var df := int(st.get("down_frames", 0)) + 1 if down else 0
			st["down_frames"] = df
			# The foot's world speed this frame, from its skeleton-space
			# travel plus the body's own advance (skeleton forward is
			# +Z, so stance travel is -body_step in Z and the deviation
			# from that is the world travel).
			var slow := false
			var dev := -1.0
			if _uprev.has(s):
				var pv: Vector2 = _uprev[s] as Vector2
				var mv: Vector3 = sole["mid"] as Vector3
				var body_step := float(view._gait_mps) * dt
				dev = Vector2(mv.x - pv.x, mv.z - pv.y + body_step).length()
				var allow := maxf(2.5 * float(view._gait_mps) * dt, 0.05)
				slow = dev < 0.5 * allow
			if down and df >= 3 and slow:
				var mid: Vector3 = sole["mid"] as Vector3
				# The lock is where the foot was HELD. When the proof pin
				# held it, the foot IS at the proof anchor — as drawn, the
				# two agree within millimetres — so the lock is the anchor
				# and the landing is vertical (v3: the grab plants, peak
				# 0.046 m). When the pin never held it, the foot is where
				# the pose has it now, and the lock is the pose sole this
				# frame (the base lock): the drawn sole a frame old is
				# already stale at speed — 0.05 m at 3 m/s — and a stale
				# lock starves the planted-pair count and drifts the
				# landing (drawn-sole lock: shuffle_fast pairs 25/37,
				# median 0.020 m; proof-anchor-always: pairs 5, and the
				# release then slides, act 0.103 m).
				var lock_at: Vector3 = xf * Vector3(mid.x, 0.0, mid.z)
				if st.has("proof"):
					lock_at = st["proof"] as Vector3
				# The landing starts from where the foot is drawn, not
				# from the tracking height: a foot the proof pin already
				# seated is at the deck, and lifting it to 0.15 m to land
				# it again is a visible jump. The drawn sole a frame old
				# is the foot's height; without a fresh one the old
				# tracking height stands.
				var land_from := UPLANT_TRACK_LIFT_M
				var drawn0: Dictionary = _udrawn.get(s, {})
				if not drawn0.is_empty() and int(drawn0.get("frame", -1)) == _ik_frame - 1:
					land_from = clampf((inv * (drawn0["at"] as Vector3)).y, 0.0, UPLANT_TRACK_LIFT_M)
				st = {"lock": lock_at, "frame": _ik_frame, "landing": 5, "land_from": land_from}
				_uplants[s] = st
				# Fall through to the landing branch below, not to the
				# hold: the foot comes down over five frames, it does
				# not drop the hover height in one.
			else:
				# Still swinging, or still proving the touchdown. From the
				# first down frame, hold the sole where it went down — but
				# only while the body is nearly still (UPLANT_STILL_MPS)
				# and the foot itself is nearly still in the world
				# (dev < UPLANT_PIN_DEV_M): a travelling foot is
				# mid-step or mid-transition, and the pin's anchor — the
				# drawn sole a frame old — is already far behind it.
				# The check already counts it planted, and the pose's settle
				# would drag it across the deck for the whole proof (0.066 m
				# in the grab). The anchor is the sole as DRAWN last frame,
				# not the pose's sole: in a grab settle the pose's sole sits
				# ~0.18 m from the drawn one, so a pose anchor yanks the
				# foot across the deck instead of holding it. The first frame
				# anchors the XZ and starts down at the landing's rate
				# (0.03 m a frame); from the second frame the hold resists
				# the pose, so a one-frame graze is never fought. The anchor
				# is a world point, so a moving body does not drag it. A drawn
				# sole from any frame but the last is stale (a climb, a leap)
				# and is never used.
				var drawn: Dictionary = _udrawn.get(s, {})
				var pin_gates := down and not drawn.is_empty() and int(drawn.get("frame", -1)) == _ik_frame - 1 and float(view._gait_mps) <= UPLANT_STILL_MPS and dev >= 0.0 and dev < UPLANT_PIN_DEV_M
				if pin_gates:
					if not st.has("proof"):
						# The pin eases in from where the foot is drawn:
						# the blend weight starts at the drawn height, so
						# the first frame only anchors (the sole is already
						# there) and the descent to the deck takes five
						# frames at the landing's own rate.
						var sk: Vector3 = inv * (drawn["at"] as Vector3)
						st["proof"] = xf * Vector3(sk.x, 0.0, sk.z)
						st["pin_k"] = clampf(1.0 - sk.y / UPLANT_TRACK_LIFT_M, 0.0, 1.0)
					var k_in: float = minf(float(st.get("pin_k", 1.0)) + 0.2, 1.0)
					st["pin_k"] = k_in
					var tgt: Vector3 = inv * (st["proof"] as Vector3)
					tgt.y = UPLANT_TRACK_LIFT_M * (1.0 - k_in)
					_reach_sole("hp_" + s, "kn_" + s, "an_" + s, tgt, w, "uplant_" + s, s)
				elif st.has("proof"):
					# The pin eases out the way it came in: five frames
					# from the deck back to the tracking height, at the
					# anchor, before the foot is handed back to the pose
					# — never a one-frame snap either way. A flicker
					# re-engages from the faded weight, so the blend
					# never restarts from the top.
					var k_out: float = maxf(float(st.get("pin_k", 1.0)) - 0.2, 0.0)
					st["pin_k"] = k_out
					if k_out <= 0.0:
						st.erase("proof")
						st.erase("pin_k")
						_reach_chain("hp_" + s, "kn_" + s, "an_" + s, pose_ankle + Vector3(0.0, UPLANT_TRACK_LIFT_M, 0.0), w, "uplant_" + s)
					else:
						var tgt2: Vector3 = inv * (st["proof"] as Vector3)
						tgt2.y = UPLANT_TRACK_LIFT_M * (1.0 - k_out)
						_reach_sole("hp_" + s, "kn_" + s, "an_" + s, tgt2, w, "uplant_" + s, s)
				else:
					# Still swinging, or still proving the touchdown: track
					# the pose's ankle held above the deck, never handed back
					# low and fast.
					st.erase("proof")
					_reach_chain("hp_" + s, "kn_" + s, "an_" + s, pose_ankle + Vector3(0.0, UPLANT_TRACK_LIFT_M, 0.0), w, "uplant_" + s)
				st["frame"] = _ik_frame
				_uplants[s] = st
				_uprev[s] = Vector2((sole["mid"] as Vector3).x, (sole["mid"] as Vector3).z)
				continue
		elif bool(st.get("releasing", false)):
			var k := clampf(float(st.get("rel", 0.0)) + dt / UPLANT_RELEASE_S, 0.0, 1.0)
			if k < 1.0:
				# A takeoff release holds its lock in skeleton space
				# (body-relative): the world point it was planted on is
				# behind her now, and chasing it as the body falls yanks
				# the foot up. A normal release holds its lock in the
				# world — the foot was planted there.
				var plant_tgt: Vector3
				if st.has("lock_skel"):
					plant_tgt = (st["lock_skel"] as Vector3) - skel.get_bone_global_pose(bone_idx["an_" + s]).basis * _sole_offset(s)
				else:
					var lock: Vector3 = st["lock"] as Vector3
					plant_tgt = inv * lock - skel.get_bone_global_pose(bone_idx["an_" + s]).basis * _sole_offset(s)
				# The foot lifts before it travels: sideways travel
				# waits until UPLANT_TRAVEL_START of the release has
				# passed, so the sole is off the deck (and no longer
				# counted as planted) before it moves sideways. The
				# old code travelled from the first frame, while the
				# near-straight leg could not bend fast enough past
				# govern's cap to lift the sole — so the sole dragged
				# along the deck to its new stance.
				var travel_tgt := clampf((k - UPLANT_TRAVEL_START) / (1.0 - UPLANT_TRAVEL_START), 0.0, 1.0)
				# ... and the travel only advances while the sole is
				# actually up: the lift alone does not hold it there
				# while the leg reaches forward (the leg stretches and
				# the sole drops back to the deck, act 0.111 m at the
				# unrig), so the travel waits on the drawn sole's height
				# above its lock. A low foot holds the travel (the target
				# stays, the lift bends the knee) until it rises; the
				# release still ends on its clock, and the tracking after
				# it carries any travel left over.
				var travel := float(st.get("travel", 0.0))
				var drawn_h := 1.0
				var drawn_r: Dictionary = _udrawn.get(s, {})
				if not drawn_r.is_empty() and int(drawn_r.get("frame", -1)) == _ik_frame - 1 and st.has("lock"):
					drawn_h = float((drawn_r["at"] as Vector3).y - (st["lock"] as Vector3).y)
				if drawn_h >= UPLANT_TRAVEL_H_M:
					travel = minf(travel_tgt, travel + dt / UPLANT_RELEASE_S / (1.0 - UPLANT_TRAVEL_START))
				st["travel"] = travel
				var tgt := plant_tgt.lerp(pose_ankle, travel)
				# The arc stays up: its lift eases from the arc's height
				# into the tracking height, never back down to a low fast
				# pose ankle at the tail, so the handoff to tracking is
				# smooth and the foot never snaps forward to a plant. On
				# a takeoff release the tail eases to the pose instead:
				# she is airborne, and the foot dangles with her body.
				if k < 0.7:
					tgt.y += UPLANT_LIFT_M * sin(PI * k)
				else:
					var tail_h := 0.0 if release_only else UPLANT_TRACK_LIFT_M
					tgt.y += lerpf(UPLANT_LIFT_M * sin(PI * 0.7), tail_h, (k - 0.7) / 0.3)
				_reach_chain("hp_" + s, "kn_" + s, "an_" + s, tgt, w, "uplant_" + s)
				st["rel"] = k
				st["frame"] = _ik_frame
				_uplants[s] = st
				_uprev[s] = Vector2((sole["mid"] as Vector3).x, (sole["mid"] as Vector3).z)
				continue
			if release_only:
				# The takeoff release ends at the pose, not at the
				# tracking height: the plant is gone, and the foot
				# follows the pose from here.
				_uplants.erase(s)
				_reach_chain("hp_" + s, "kn_" + s, "an_" + s, pose_ankle, w, "uplant_" + s)
			else:
				st["tracking"] = true
				st["frame"] = _ik_frame
				_uplants[s] = st
				_reach_chain("hp_" + s, "kn_" + s, "an_" + s, pose_ankle + Vector3(0.0, UPLANT_TRACK_LIFT_M, 0.0), w, "uplant_" + s)
			_uprev[s] = Vector2((sole["mid"] as Vector3).x, (sole["mid"] as Vector3).z)
			continue
		if int(st.get("landing", 0)) > 0:
			# Touchdown from the hover: the sole comes down over five
			# frames at the lock's XZ, so the descent is vertical only
			# and no counted frame travels. A fast drop needs a joint
			# change at govern's cap, which clips and swings the foot
			# sideways; five frames keep each step well under it. The
			# descent starts from where the foot is drawn (land_from),
			# never from the tracking height when the pin seated it.
			var n := int(st["landing"])
			var lock: Vector3 = st["lock"] as Vector3
			var sole_tgt := inv * lock
			sole_tgt.y = float(st.get("land_from", UPLANT_TRACK_LIFT_M)) * n / 5.0
			_reach_sole("hp_" + s, "kn_" + s, "an_" + s, sole_tgt, w, "uplant_" + s, s)
			st["landing"] = n - 1
			st["frame"] = _ik_frame
			_uplants[s] = st
			_uprev[s] = Vector2((sole["mid"] as Vector3).x, (sole["mid"] as Vector3).z)
			continue
		if down and not release_only:
			if st.is_empty():
				var mid: Vector3 = sole["mid"] as Vector3
				st = {"lock": xf * Vector3(mid.x, 0.0, mid.z)}
			# A hold dragged past its leg's reach is already releasing:
			# the trigger above converts it before this branch runs.
			var lock: Vector3 = st["lock"] as Vector3
			_reach_sole("hp_" + s, "kn_" + s, "an_" + s, inv * lock, w, "uplant_" + s, s)
			st["frame"] = _ik_frame
			_uplants[s] = st
		# No final elif: a lifted foot with a state is always a hold,
		# and the trigger above turns every held foot whose sole left
		# the band into a release before this point — so there is no
		# lifted, held, unreleased foot left to handle here. (And the
		# old branch only updated _uprev, which the line below does
		# unconditionally anyway.)
		_uprev[s] = Vector2((sole["mid"] as Vector3).x, (sole["mid"] as Vector3).z)


# Her hand on what she holds (the hold: his ankle, and in the rip his arm):
# the wrist from where her pose has it toward `at_world` as far as `w`, the
# arm drawn out (upper arm and forearm both, their "_x" bones slid) as long as
# that reach needs, never past HOLD_STRETCH_MAX its own length.
func hold_reach(s: String, at_world: Vector3, w: float) -> void:
	var target := skel.global_transform.affine_inverse() * at_world
	var sh := _gpos("sh_" + s)
	var el := _gpos("el_" + s)
	var wr := _gpos("wr_" + s)
	var arm := sh.distance_to(el) + el.distance_to(wr)
	var base_s := (p["sh_" + s] as Node3D).scale.y if p.has("sh_" + s) else 1.0
	var k_max := HOLD_STRETCH_MAX / maxf(base_s, 0.01)
	var want := wr.lerp(target, w)
	var need := sh.distance_to(want)
	var k := clampf(need / maxf(HOLD_ARM_OUT * arm, 0.01), 1.0, k_max)
	var beyond := need - HOLD_ARM_OUT * arm * k_max
	if beyond > 0.0:
		# the arm out as far as it goes: her shoulder pulled up out of her
		# toward it
		_shift_bone_global("sh_" + s, (want - sh).normalized() * minf(beyond, HOLD_SHOULDER_M))
	if k > 1.0001:
		for bn in ["sh_%s_x" % s, "el_%s_x" % s]:
			var i: int = bone_idx[bn]
			skel.set_bone_pose_position(i, skel.get_bone_pose_position(i) * k)
	stretch[s] = base_s * k
	_reach(s, want, 1.0, "hold_" + s)


# Her hand toward a prop's handle `at_world` as far as `w`, onto the nearest
# point her real arm has toward it (_in_arm): walking to it, out from
# HOLD_ARM_OUT of the arm to its whole length as the take comes; carried, its
# whole length (the native has it in her hand while she slides the last of
# the way to it).
func _prop_hand(s: String, at_world: Vector3, w: float, carried: bool) -> void:
	hold_reach(s, _in_arm(s, at_world, 1.0 if carried else lerpf(HOLD_ARM_OUT, 1.0, w)), w)


# The nearest point to `at_world` her real arm (its bones at rest) has from
# its shoulder, out to `out` of its length.
func _in_arm(s: String, at_world: Vector3, out: float) -> Vector3:
	var sh := bone_world("sh_" + s).origin
	var arm := _t("sh_%s_x" % s).length() + _t("el_%s_x" % s).length()
	return sh + (at_world - sh).limit_length(out * arm)


# Her feet on her pipe's rail's top, perched over it (the simulation's
# contact_feet there): her soles either side of where it has them.
func _pipe_feet(data: Dictionary, w: float) -> void:
	if not bool(data.get("contact_supported", false)):
		return
	var inv := skel.global_transform.affine_inverse()
	var feet: Vector3 = data["contact_feet"] + Vector3(0.0, ANKLE_OVER_SOLE_M, 0.0)
	var along := Vector3(1.0, 0.0, 0.0)
	_reach_chain("hp_r", "kn_r", "an_r", inv * (feet + along * 0.1), w, "perch_r")
	_reach_chain("hp_l", "kn_l", "an_l", inv * (feet - along * 0.1), w, "perch_l")
	claims |= CLAIM_FEET


# `held`: her holds as the view draws them, eased onto each new one (the
# view's _ease_hands); without it, the native's own holds.
func contact(data: Dictionary, falling: bool, held := PackedVector3Array()) -> void:
	var prop_intent := int(data.get("carrying", 0)) != 0 or int(data.get("stock_seeking", 0)) != 0
	var carrying := int(data.get("carrying", 0)) != 0 and data.has("stock_handle")
	# Preserve the preceding frame's governed wrist positions before govern()
	# updates them. The contact correction must not spend a second motion budget.
	var wrist_before := {}
	if carrying:
		for side in ["r", "l"]:
			var idx: int = _governed_bones.find(int(bone_idx["wr_" + side]))
			if idx >= 0 and idx < _govern_pos.size():
				wrist_before[side] = _govern_pos[idx]
	if prop_intent and not _prop_arm_pose.is_empty() and not bool(view.fresh_frame):
		for bone in PROP_ARM_JOINTS:
			var i: int = bone_idx[bone]
			var prior: Array = _prop_arm_pose[bone]
			skel.set_bone_pose_rotation(i, prior[0])
			# Previous forearm length eases back toward rest at the existing
			# position-rate limit rather than collapsing when her state shifts.
			var nominal := skel.get_bone_pose_position(i)
			var last: Vector3 = prior[1]
			var cap := GOVERN_M_S * maxf(float(view._dt), 0.0)
			skel.set_bone_pose_position(i, last.move_toward(nominal, cap))
	_contact(data, falling, held)
	govern()
	# Only the physical constraint owns attachment. The native handle is the
	# contact target, not a visual approximation. Finish the wrists AFTER the
	# joint governor, but clamp each wrist to its original per-frame position
	# budget from the saved previous pose. No second budget is added.
	if carrying and wrist_before.has("r"):
		var handle: Vector3 = data["stock_handle"]
		var size: Vector3 = data.get("stock_size", Vector3.ZERO)
		var two_hands := maxf(size.x, size.z) >= 0.38
		var across := Vector3.ZERO
		if two_hands:
			var rotation: Quaternion = data.get("stock_rotation", Quaternion.IDENTITY)
			across = (rotation * Vector3.RIGHT) * minf(0.02, size.x * 0.1)
		_finish_prop_wrist("r", _in_arm("r", handle + across, 1.0), wrist_before["r"])
		if two_hands and wrist_before.has("l"):
			_finish_prop_wrist("l", _in_arm("l", handle - across, 1.0), wrist_before["l"])
	_prop_arm_pose.clear()
	if prop_intent:
		for bone in PROP_ARM_JOINTS:
			var i: int = bone_idx[bone]
			_prop_arm_pose[bone] = [skel.get_bone_pose_rotation(i), skel.get_bone_pose_position(i)]
	# The soles as drawn this frame (after govern), for the upright plant's
	# proof anchor next frame: where the foot actually is, not the pose.
	for s in ["l", "r"]:
		var heel: Vector3 = point("an_" + s, "heel")
		var toe: Vector3 = point("an_" + s, "toe")
		_udrawn[s] = {"at": (heel + toe) * 0.5, "frame": _ik_frame}


# An actual grasp's terminal wrist contact, corrected only within the
# existing per-frame GOVERN_M_S budget. The underlying arm's two-bone IK
# and physics handle remain authoritative; only the drawn end joint shifts.
func _finish_prop_wrist(side: String, at_world: Vector3, prior: Vector3) -> void:
	var bone := "wr_" + side
	var id: int = bone_idx[bone]
	var idx: int = _governed_bones.find(id)
	if idx < 0:
		return
	var target: Vector3 = skel.global_transform.affine_inverse() * at_world
	var offset: Vector3 = target - _gpos(bone)
	if offset.length() <= 0.02:
		return
	var parent: int = skel.get_bone_parent(id)
	var parent_basis: Basis = skel.get_bone_global_pose(parent).basis if parent >= 0 else Basis.IDENTITY
	var desired: Vector3 = skel.get_bone_pose_position(id) + parent_basis.inverse() * offset
	var speed: float = GOVERN_BURST_M_S if bool(view.burst_active) else GOVERN_M_S
	var limit: float = speed * maxf(float(view._dt), 0.0)
	var placed: Vector3 = prior + (desired - prior).limit_length(limit)
	skel.set_bone_pose_position(id, placed)
	_govern_pos[idx] = placed
	if placed.distance_to(desired) > 0.0001:
		governed += 1


# Every bone of her body (not her hair, eyes or fingers) turned this frame
# no further than GOVERN_RAD_S allows from where it was drawn last frame. The
# springs, weights and poles keep her well inside it; this is the ceiling
# that holds whatever they missed. `governed`: the bones it held back.
func govern() -> void:
	if _governed_bones.is_empty():
		for bn in bone_idx.keys():
			var name := String(bn)
			if name.begins_with("hair") or name.begins_with("eye") or (name.length() >= 2 and (name[0] == "f" or name[0] == "t") and name[1].is_valid_int()):
				continue
			if int(bone_idx[bn]) >= 0:
				_governed_bones.append(int(bone_idx[bn]))
	governed = 0
	var dt: float = float(view._dt) if view != null else 0.0
	var reset: bool = view == null or bool(view.fresh_frame) or _govern_last.size() != _governed_bones.size()
	if reset:
		_govern_last.resize(_governed_bones.size())
		_govern_pos.resize(_governed_bones.size())
		for j in _governed_bones.size():
			_govern_last[j] = skel.get_bone_pose_rotation(_governed_bones[j])
			_govern_pos[j] = skel.get_bone_pose_position(_governed_bones[j])
		return
	# a frame of no time holds her as she was
	var burst := bool(view.burst_active)
	var cap := (GOVERN_BURST_RAD_S if burst else GOVERN_RAD_S) * maxf(dt, 0.0)
	var cap_m := (GOVERN_BURST_M_S if burst else GOVERN_M_S) * maxf(dt, 0.0)
	for j in _governed_bones.size():
		var i: int = _governed_bones[j]
		var q := skel.get_bone_pose_rotation(i)
		var was: Quaternion = _govern_last[j]
		var ang := was.angle_to(q)
		if ang > cap:
			q = was.slerp(q, cap / ang) if ang > 1.0e-6 else was
			skel.set_bone_pose_rotation(i, q)
			governed += 1
		_govern_last[j] = q
		var at := skel.get_bone_pose_position(i)
		var was_at: Vector3 = _govern_pos[j]
		if at.distance_to(was_at) > cap_m:
			at = was_at + (at - was_at).limit_length(cap_m)
			skel.set_bone_pose_position(i, at)
			governed += 1
		_govern_pos[j] = at


func _contact(data: Dictionary, falling: bool, held: PackedVector3Array) -> void:
	stretch["l"] = 1.0
	stretch["r"] = 1.0
	prop_short = -1.0
	prop_take_s = INF
	prop_over = false
	prop_carried = false
	landed.clear()
	claims = 0
	_ik_frame += 1
	var state := int(data.get("state", 0))
	# Laid out on the steel, or turned over head first down her pipe: the figure
	# itself is turned, and the deck is not under her soles.
	if ((state == 11 or state == 6) and not bool(data.get("kneeling", false))) or bool(data.get("pipe_head_down", false)):
		_plants.clear()
		down_mask = 0
		return
	var climbing := bool(data.get("climbing", false)) or bool(data.get("on_pipe", false))
	var on_line := bool(data.get("on_line", false))
	var airborne := falling or float(view._fall_speed) > 1.0 or bool(data.get("climb_airborne", false))
	# What the simulation itself has her standing on and holding (its
	# contact_* facts, matron_contact.cpp): a limb is planted, and her body
	# ridden or lifted onto a floor, only on those, and on the frame her feet
	# have nothing under them every plant lets go. A native without them (or
	# contact_truth off): the old inferences.
	var truth := view != null and bool(view.contact_truth) and data.has("contact_supported")
	var supported := bool(data.get("contact_supported", false))
	var feet_free := truth and not supported
	# On all fours (only the dangerous and the changed, and the Turn's lunge:
	# a pest hurries after you upright), as much as the view has her down;
	# whatever she goes on to (a climb, a leap), her limbs leave their plants
	# as that weight goes; and from the frame her feet leave her floor no limb
	# is held to it (each eased back under her body, _plant_limbs).
	var wf := float(view._w_fours) if view != null else 0.0
	if wf <= 0.001:
		_plants.clear()
		down_mask = 0
	if bool(data.get("climbing", false)) and data.has("climb_hand"):
		var hand: Vector3 = held[0] if held.size() >= 1 else data["climb_hand"]
		# Two-handed moves (mantle on the lip, jump_catch on the bar,
		# pole with hands close together, hang_drop from the lip).
		# Rung moves are not two-handed for their feet: their feet
		# are never lifted, but their left wrist now reaches a held
		# second grip (M5e); without one they keep exactly the old
		# contact behaviour.
		var two_handed := bool(data.get("climb_move_active", false)) and String(data.get("climb_move", "")) in ["mantle", "jump_catch", "pole_up", "pole_down", "hang_drop"]
		# Feet first, so the hand reach is solved from where the body
		# ends up: on a two-handed move, when the simulation has her
		# feet on a floor (contact_feet) within reach of the feet, the
		# body rides just enough for the lowest foot to meet it. A
		# floor farther than that (feet in mid-air under a lip) lifts
		# nothing, so the figure never floats toward a deck it has
		# not reached. With nothing under her feet the feet pose is
		# untouched.
		var lift := 0.0
		var feet_known := supported
		if not truth:
			feet_known = bool(data.get("climb_feet_support_known", false)) and data.has("climb_feet_support")
		if two_handed and feet_known:
			var at: Vector3 = data["contact_feet"] if truth else data["climb_feet_support"]
			var sup: Vector3 = skel.global_transform.affine_inverse() * at
			var dy := sup.y - _lowest_foot()
			if absf(dy) <= 0.3:
				lift = dy
				claims |= CLAIM_FEET
		# her body's ride on its spring here too: onto the support, and off
		# the deck's ride as the climb starts, never in a frame
		_ride(lift)
		if wf > 0.001:
			_plant_limbs(data, wf, feet_free)
		var wc := float(view._w_climb) if view != null else 1.0
		_reach("r", skel.global_transform.affine_inverse() * hand, wc)
		claims |= CLAIM_HAND
		# The left wrist goes to a second hold only when the
		# simulation has her hand on one (contact_hand2_held under
		# truth, climb_hand2_known otherwise): on a two-handed move
		# it falls back to the one hold, on a rung move to nothing
		# at all (the old rung behaviour), never to a point the
		# view invented. Climb-branch movers hold two on rung
		# climbs now (M5b/M5d); the guard is unchanged, the hold is
		# made valid by gripping.
		var rung_move := String(data.get("climb_move", "")) in ["rung_up", "rung_down"]
		if two_handed or rung_move:
			var hand_l: Vector3 = hand
			var second := bool(data.get("contact_hand2_held", false))
			if not truth:
				second = bool(data.get("climb_hand2_known", false)) and data.has("climb_hand2")
			if second:
				hand_l = held[1] if held.size() >= 2 else (data["contact_hand2"] if truth else data["climb_hand2"])
				claims |= CLAIM_HAND2
				_reach("l", skel.global_transform.affine_inverse() * hand_l, wc)
			elif two_handed:
				_reach("l", skel.global_transform.affine_inverse() * hand_l, wc)
		if down_mask != 0:
			claims |= CLAIM_PLANTS
		return
	if truth and bool(data.get("on_pipe", false)) and bool(data.get("contact_hand_held", false)) \
			and not bool(data.get("pipe_sliding", false)) and not bool(data.get("pipe_stunned", false)):
		# On her pipe: her hands round its face at her grip, her feet against
		# it where her legs press (contact_brace), her body where the
		# simulation has it: laid back between them, never hung in the air.
		var wp := float(view._w_climb) if view != null else 1.0
		var inv := skel.global_transform.affine_inverse()
		var grip: Vector3 = data["contact_hand"]
		var into := grip - skel.global_position
		into.y = 0.0
		var side := Vector3.UP.cross(into.normalized()) if into.length() > 0.01 else Vector3.RIGHT
		_ride(0.0)
		_reach("r", inv * (grip + side * 0.08), wp)
		_reach("l", inv * (grip - side * 0.08), wp)
		claims |= CLAIM_HAND | CLAIM_HANG
		if bool(data.get("contact_braced", false)):
			var brace: Vector3 = data["contact_brace"]
			_reach_chain("hp_r", "kn_r", "an_r", inv * (brace + side * 0.12), wp)
			_reach_chain("hp_l", "kn_l", "an_l", inv * (brace - side * 0.12), wp)
			claims |= CLAIM_BRACE
		_pipe_feet(data, wp)
		_plants.clear()
		down_mask = 0
		return
	if truth and bool(data.get("on_pipe", false)) and bool(data.get("contact_supported", false)) \
			and not bool(data.get("pipe_sliding", false)) and not bool(data.get("pipe_stunned", false)):
		# Perched on her pipe's rail, her hands off it: her feet on its top.
		_ride(0.0)
		_pipe_feet(data, 1.0)
		_plants.clear()
		down_mask = 0
		return
	if climbing or on_line:
		# her body hung from her hands only on a grip (or a line) the
		# simulation has; otherwise it is not lifted to them. With
		# both grips held the two fixed wrists already place her
		# body, so the 2.6 m ride retires there (M5e); anywhere else
		# it stays exactly as before.
		var both := truth and bool(data.get("contact_hand_held", false)) and bool(data.get("contact_hand2_held", false))
		if both:
			_ride(0.0)
			claims |= CLAIM_HANG
		elif not truth or bool(data.get("contact_hand_held", false)) or bool(data.get("contact_line", false)):
			var top := maxf(_gpos("wr_l").y, _gpos("wr_r").y)
			_ride(clampf(HANDS_ON_RUNG_Y - top - 0.1, 0.0, 0.8))
			claims |= CLAIM_HANG
		else:
			_ride(0.0)
		if wf > 0.001:
			_plant_limbs(data, wf, feet_free)
		if down_mask != 0:
			claims |= CLAIM_PLANTS
		return
	if airborne or feet_free:
		_ride(0.0)
	else:
		# feet on her floor: the body rides up or down to the lower foot
		_ride(clampf(-_lowest_foot(), -0.15, 0.3))
		claims |= CLAIM_RIDE
	if view == null:
		return
	# Upright feet plant with the weight the fours do not take: at wf = 0
	# they hold alone; as she goes down, the fours' plants take over.
	if not airborne and not feet_free:
		_plant_upright_feet(wf)
	elif not _uplants.is_empty():
		# Taking off, or with nothing under her feet: the plants do not
		# vanish — they release over the release blend, from where the
		# feet are drawn, so a foot never jumps to the pose in one frame
		# at takeoff. The call repeats while a release is still running;
		# each finished release erases its own plant.
		_plant_upright_feet(wf, true)
	if wf > 0.001:
		_plant_limbs(data, wf, feet_free)
	if down_mask != 0:
		claims |= CLAIM_PLANTS
	# The native knows which handle she is walking toward before taking it.
	# Put her hands on that moving physical point during the approach, so the
	# first constrained frame does not start with a half-meter visual reach.
	# A far-off target is never a claim of contact: reach begins only when the
	# existing physical handle has come within the drawn arms' range.
	var carrying := int(data.get("carrying", 0)) != 0 and data.has("stock_handle")
	var seeking := not carrying and int(data.get("stock_seeking", 0)) != 0 and data.has("stock_seek_handle")
	if carrying or seeking:
		var handle: Vector3 = data["stock_handle"] if carrying else data["stock_seek_handle"]
		var size: Vector3 = data.get("stock_size", Vector3.ZERO) if carrying else data.get("stock_seek_size", Vector3.ZERO)
		var rotation: Quaternion = data.get("stock_rotation", Quaternion.IDENTITY) if carrying else data.get("stock_seek_rotation", Quaternion.IDENTITY)
		var shoulder: Vector3 = bone_world("sh_r").origin
		var two_hands := maxf(size.x, size.z) >= 0.38
		var across := (rotation * Vector3.RIGHT) * minf(0.02, size.x * 0.1) if two_hands else Vector3.ZERO
		var reach := HOLD_ARM_OUT * (_t("sh_r_x").length() + _t("el_r_x").length())
		var at_take := shoulder
		prop_take_s = 0.0
		if seeking:
			# walking to it: where her shoulder will be when the native takes
			# it (her centre PROP_TAKE_M across from it), her pose as now
			var root: Vector3 = data.get("render_position", skel.global_position)
			var to := Vector2(handle.x - root.x, handle.z - root.z)
			var ahead := maxf(0.0, to.length() - PROP_TAKE_M)
			if to.length() > 0.001:
				var d := to.normalized() * ahead
				at_take += Vector3(d.x, 0.0, d.y)
			prop_take_s = ahead / maxf(float(data.get("speed", 0.0)), 0.01)
		# what her real arm (its bones at rest) falls short of there
		prop_short = at_take.distance_to(handle + across) - reach
		prop_over = handle.y > shoulder.y
		prop_carried = carrying
		# walking to it, her hand goes out over the last PROP_HAND_LEAD_S
		# before the take, along her real arm toward the handle and never past
		# it, so the handle comes into her hand; carried, the native has it in
		# her hand
		var w := 1.0
		if seeking:
			w = clampf(1.0 - prop_take_s / PROP_HAND_LEAD_S, 0.0, 1.0)
		if w > 0.001:
			_prop_hand("r", handle + across, w, carrying)
			if two_hands:
				_prop_hand("l", handle - across, w, carrying)
	# Her hand on the climber's ankle, and in the rip her other on his arm
	# (the view keeps the last of each while her hand comes off it).
	if float(view._w_hold) > 0.001:
		hold_reach("r", view._hold_at, float(view._w_hold))
	if float(view._w_hold2) > 0.001:
		hold_reach("l", view._hold_at2, float(view._w_hold2))


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
	var belt := bone_world("spine_mid")
	var neck := bone_world("neck_lo")
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
				# The point can sit outside the skull and the back while the ribbon
				# between this point and the one above still cuts through them.
				q = _segment_outside(above, q, seg, skull, _skull_radius)
				q = _segment_outside(above, q, seg, back, BACK_RADIUS)
				# Collar and belt are rings the two spheres do not cover. Walk the
				# segment out sideways and down. A lift along the chest axis spends
				# the hang. Measured radii, not a knob: collar 0.138 on chest
				# local Y 0.091..0.154, buckle 0.192 on spine_mid local Y -0.001..0.042.
				q = _clear_ring(above, q, seg, chest, 0.091, 0.154, 0.138)
				q = _clear_ring(above, q, seg, belt, -0.001, 0.042, 0.192)
				# The neck is a cylinder the collar ring does not cover. Measured on
				# the posed cloth: max radial 0.089, so 0.095 sits the strand just
				# outside it. Y -0.01..0.105 is the neck on neck_lo.
				q = _clear_ring(above, q, seg, neck, -0.01, 0.105, 0.095)
				# The chest front is a slope, not a wider cylinder. A point can be
				# outside the collar radius and still be in front of the cloth.
				# Walk it back in the horizontal, never up: aiming the whole lock
				# straight backward dropped the hang under the gate.
				q = _clear_front(above, q, seg, chest)
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


# Child of a fixed-length segment, swung the smallest amount that puts the
# whole segment outside the sphere. Length stays exact. No extra radius.
func _segment_outside(a: Vector3, b: Vector3, length: float, centre: Vector3, radius: float) -> Vector3:
	if (a - centre).length() < radius:
		var pushed := _outside(b, centre, radius)
		var pd := pushed - a
		if pd.length() < 1e-6:
			pd = Vector3.DOWN
		return a + pd.normalized() * length
	var dir := b - a
	if dir.length() < 1e-6:
		dir = Vector3.DOWN
	var d := dir.normalized()
	if not _segment_hits(a, d, length, centre, radius):
		return a + d * length
	var toward := centre - a
	var axis := toward.cross(d)
	if axis.length() < 1e-6:
		axis = toward.cross(Vector3.UP)
		if axis.length() < 1e-6:
			axis = toward.cross(Vector3.RIGHT)
	axis = axis.normalized()
	var lo := 0.0
	var hi := PI
	for _i in 16:
		var mid := (lo + hi) * 0.5
		var trial := (Basis(axis, mid) * d).normalized()
		if _segment_hits(a, trial, length, centre, radius):
			lo = mid
		else:
			hi = mid
	return a + (Basis(axis, hi) * d).normalized() * length


func _segment_hits(a: Vector3, d_unit: Vector3, length: float, centre: Vector3, radius: float) -> bool:
	var oc := a - centre
	var bb := 2.0 * oc.dot(d_unit)
	var cc := oc.dot(oc) - radius * radius
	var disc := bb * bb - 4.0 * cc
	if disc < 0.0:
		return false
	var s := sqrt(disc)
	var t1 := (-bb - s) * 0.5
	var t2 := (-bb + s) * 0.5
	return t1 < length and t2 > 0.0


# The clothing ring is a slice of a cylinder on a posed bone. If the segment
# enters it, tilt the child toward the horizontal way out, still downward.
func _clear_ring(a: Vector3, b: Vector3, length: float, bone: Transform3D, y0: float, y1: float, radius: float) -> Vector3:
	var origin := bone.origin
	var axis := bone.basis.y.normalized()
	for _guard in 6:
		if _ring_depth(a, b, origin, axis, y0, y1, radius) <= 0.001:
			return b
		var horiz := _ring_out(a, b, origin, axis, y0, y1)
		horiz = Vector3(horiz.x, 0.0, horiz.z)
		if horiz.length() < 1e-4:
			horiz = Vector3(0.0, 0.0, -1.0)
		horiz = horiz.normalized()
		var d := b - a
		if d.length() < 1e-6:
			d = Vector3.DOWN
		d = d.normalized()
		var trial := d + horiz * 0.45
		trial.y = minf(trial.y, -0.15)
		if trial.length() < 1e-6:
			trial = Vector3.DOWN
		b = a + trial.normalized() * length
	return b


func _ring_depth(a: Vector3, b: Vector3, origin: Vector3, axis: Vector3, y0: float, y1: float, radius: float) -> float:
	var worst := 0.0
	for s in 8:
		var p: Vector3 = a.lerp(b, float(s) / 7.0)
		var along := (p - origin).dot(axis)
		if along < y0 - 0.01 or along > y1 + 0.01:
			continue
		var radial := (p - origin) - axis * along
		worst = maxf(worst, radius - radial.length())
	return worst


func _ring_out(a: Vector3, b: Vector3, origin: Vector3, axis: Vector3, y0: float, y1: float) -> Vector3:
	var best := 1.0e9
	var out := Vector3.ZERO
	for s in 8:
		var p: Vector3 = a.lerp(b, float(s) / 7.0)
		var rel := p - origin
		var along := rel.dot(axis)
		if along < y0 - 0.01 or along > y1 + 0.01:
			continue
		var radial := rel - axis * along
		var rl := radial.length()
		if rl < best:
			best = rl
			out = radial
	if out.length() < 1e-4:
		out = Vector3(a.x - origin.x, 0.0, a.z - origin.z)
	return out


# Front of the chest cloth in chest-local Y, the larger of the guard and the
# head-down pose. Chest-weighted verts only, so the arms are not in it.
func _chest_front_z(y: float) -> float:
	if y <= -0.25:
		return 0.249
	if y <= -0.15:
		return lerpf(0.249, 0.228, (y + 0.25) / 0.10)
	if y <= -0.05:
		return lerpf(0.228, 0.179, (y + 0.15) / 0.10)
	if y <= 0.05:
		return lerpf(0.179, 0.173, (y + 0.05) / 0.10)
	if y <= 0.12:
		return lerpf(0.173, 0.116, (y - 0.05) / 0.07)
	if y <= 0.18:
		return lerpf(0.116, 0.098, (y - 0.12) / 0.06)
	return 0.098


# If the segment is in front of that slope, and still over the torso (|local x|
# under 0.16, so a shoulder is not the torso), step the child backward in the
# horizontal. Never add an upward step.
func _clear_front(a: Vector3, b: Vector3, length: float, bone: Transform3D) -> Vector3:
	var origin := bone.origin
	var axis := bone.basis.y.normalized()
	var fwd := bone.basis.z.normalized()
	var side := bone.basis.x.normalized()
	for _guard in 8:
		var worst := 0.0
		var sx := 0.0
		for s in 8:
			var p: Vector3 = a.lerp(b, float(s) / 7.0)
			var rel := p - origin
			var x := rel.dot(side)
			var y := rel.dot(axis)
			var z := rel.dot(fwd)
			if absf(x) > 0.16 or y < -0.25 or y > 0.18:
				continue
			var ex := z - _chest_front_z(y)
			if ex > worst:
				worst = ex
				sx = x
		if worst <= 0.001:
			return b
		var back := -fwd
		back.y = minf(back.y, 0.0)
		if back.length() < 0.35:
			back = side * (1.0 if sx >= 0.0 else -1.0)
			back.y = 0.0
		if back.length() < 1e-6:
			back = Vector3(0.0, 0.0, -1.0)
		back = back.normalized()
		var d := b - a
		if d.length() < 1e-6:
			d = Vector3.DOWN
		else:
			d = d.normalized()
		var gain := minf(0.70, worst / length)
		var trial := d + back * gain
		trial.y = minf(trial.y, -0.15)
		if trial.length() < 1e-6:
			trial = Vector3.DOWN
		b = a + trial.normalized() * length
	return b
