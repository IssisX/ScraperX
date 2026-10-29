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
# How far past a posed body volume a drawn hair ribbon must sit, so a strand
# whose centre line clears a volume still does not draw through it.
const HAIR_RIBBON_M := 0.015
const HAIR_FACE_R := 0.07
const HAIR_FACE_OFF := Vector3(0.0, -0.04, 0.06)
const HAIR_NECK_R := 0.075
const HAIR_SHOULDER_R := 0.09
# Torso capsule radius: back depth, not shoulder width — a 0.17 m cylinder
# around the centreline floats the hair off her back and costs hang.
# The original 0.17 m back sphere above still covers the upper back.
const HAIR_TORSO_R := 0.13
# One verlet substep is at most this long: a slow frame is split, never
# shortened, so the hair moves all of real time instead of two-thirds of it.
const HAIR_SUBSTEP := 1.0 / 60.0

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
			"give": 1.0 + (_hash(n_chain * 3 + 2) - 0.5) * 2.0 * LOCK_VARY, "phase": _hash(n_chain * 3 + 3) * TAU,
			"side": Vector3.ZERO})
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


# `held`: her holds as the view draws them, eased onto each new one (the
# view's _ease_hands); without it, the native's own holds.
func contact(data: Dictionary, falling: bool, held := PackedVector3Array()) -> void:
	_contact(data, falling, held)
	govern()


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
	landed.clear()
	claims = 0
	_ik_frame += 1
	var state := int(data.get("state", 0))
	# Laid out on the steel, or turned over head first down her pipe: the figure
	# itself is turned, and the deck is not under her soles.
	if (state == 11 and not bool(data.get("kneeling", false))) or bool(data.get("pipe_head_down", false)):
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
		# Rung moves are not two-handed here: they keep exactly the
		# old contact behaviour, and their feet are never lifted.
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
		# On a two-handed move the left wrist goes to a second hold
		# only when the simulation has her hand on one
		# (contact_hand2_held; none of her movers holds two today);
		# otherwise to the one hold, never to a point the view
		# invented.
		if two_handed:
			var hand_l: Vector3 = hand
			var second := bool(data.get("contact_hand2_held", false))
			if not truth:
				second = bool(data.get("climb_hand2_known", false)) and data.has("climb_hand2")
			if second:
				hand_l = held[1] if held.size() >= 2 else (data["contact_hand2"] if truth else data["climb_hand2"])
				claims |= CLAIM_HAND2
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
		_plants.clear()
		down_mask = 0
		return
	if climbing or on_line:
		# her body hung from her hands only on a grip (or a line) the
		# simulation has; otherwise it is not lifted to them
		if not truth or bool(data.get("contact_hand_held", false)) or bool(data.get("contact_line", false)):
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
	if wf > 0.001:
		_plant_limbs(data, wf, feet_free)
	if down_mask != 0:
		claims |= CLAIM_PLANTS
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
# The volumes below are fitted fresh each frame from the posed bones, so
# they follow her wherever she bends: a head sphere, a face sphere
# forward and down from the skull centre (the lower face the old skull
# sphere missed — a capsule from skull to jaw would swallow the scalp
# ring the strands root in), a neck capsule, shoulder spheres, and a
# torso capsule. A strand is kept
# out of every volume as a segment, not as isolated points, with a ribbon
# margin past the surface, so a chord between two cleared points cannot
# dip through a volume between them. Each chain remembers the side it
# was pushed to, in world space, so a pitch-forward crawl cannot
# flip a strand through her head to the other side — and so a strand
# the head turns under is not held on a side that has rotated through
# her head. Her rest direction
# is world down, not head down: hanging hair falls with gravity even
# when her head is down. The rigid drawn tail past the last simulated
# point is clamped the same way, by moving the last point.
func step_hair(delta: float) -> void:
	if _chains.is_empty():
		return
	var head := bone_world("head")
	var skull := head * _skull_local
	var chest := bone_world("chest")
	var back := chest * Vector3(0.0, 0.05, -0.05)
	var volumes := _hair_volumes(head, skull, chest, back)
	# World down, not head down.
	var down := Vector3.DOWN
	var subs := clampi(ceili(maxf(delta, 0.0) / HAIR_SUBSTEP), 1, 8)
	var dt := delta / float(subs) if subs > 0 else delta
	# Verlet gravity does not split linearly: subs steps of dt/subs give
	# a total gravity coefficient of (subs+1)/(2*subs), not 1. Scale the
	# acceleration so a slow frame pulls exactly as hard as a fast one.
	var grav_scale := 1.0 if subs <= 1 else 2.0 * float(subs) / float(subs + 1)
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
			var side := (root - skull)
			ch["side"] = side.normalized() if side.length() > 1e-6 else Vector3.BACK
		pts[0] = root
		prev[0] = root
		var damp: float = ch["damp"]
		var give: float = ch["give"]
		var phase: float = ch["phase"]
		var last_push := Vector3.ZERO
		for _sub in subs:
			_wind_clock += dt
			# A gust rises and falls on its own, each lock catching it a little out of
			# step with its neighbors; the tips catch the most of it.
			var gust := maxf(0.0, sin(TAU * WIND_RATE * _wind_clock)) * (0.6 + 0.4 * sin(TAU * WIND_RATE * 3.1 * _wind_clock + phase))
			var wind := WIND_DIR * WIND_MS2 * gust
			pts[0] = root
			prev[0] = root
			for k in range(1, n + 1):
				var cur: Vector3 = pts[k]
				var vel: Vector3 = (cur - (prev[k] as Vector3)) * damp
				prev[k] = cur
				pts[k] = cur + vel + (HAIR_GRAVITY + wind * (float(k) / float(n))) * grav_scale * dt * dt
			for _it in HAIR_ITERATIONS:
				for k in range(1, n + 1):
					var above: Vector3 = pts[k - 1]
					var q: Vector3 = (pts[k] as Vector3).lerp(above + down * seg, float(HAIR_STIFFNESS[k - 1]) * give / float(HAIR_ITERATIONS))
					var d := q - above
					q = above + (d.normalized() if d.length() > 1e-6 else down) * seg
					var pushed := _hair_push(q, above, volumes, (ch["side"] as Vector3), seg)
					q = pushed[0]
					var push_dir: Vector3 = pushed[1]
					if push_dir.length() > 1e-6:
						ch["side"] = ((ch["side"] as Vector3).lerp(push_dir.normalized(), 0.3)).normalized()
					if k == n:
						last_push = push_dir
					pts[k] = q
			# A strand that ends free of every volume re-adopts the side
			# its tip now sits on, instead of staying trapped on the side
			# a violent pose change pushed it to. Without this, the
			# remembered side is a trap: the only way back is through her head.
			# Stronger still: a strand whose tip is not hanging at all
			# (folded up beside her head) has no side worth remembering,
			# so it re-adopts its actual side at once, pushed or not.
			var tip_dir2: Vector3 = (pts[n] as Vector3) - skull
			var folded := (pts[0] as Vector3).y - (pts[n] as Vector3).y < 0.1
			if folded and tip_dir2.length() > 1e-6:
				ch["side"] = tip_dir2.normalized()
			elif last_push.length() < 1e-6:
				var tip_dir: Vector3 = (pts[n] as Vector3) - skull
				if tip_dir.length() > _skull_radius + 0.05:
					ch["side"] = ((ch["side"] as Vector3).lerp(tip_dir.normalized(), 0.1)).normalized()
			# The rigid drawn tail past the last simulated point: if it would
			# draw through a volume, move the last point so it clears.
			if n >= 2:
				var tail_dir: Vector3 = (pts[n] as Vector3) - (pts[n - 1] as Vector3)
				if tail_dir.length() > 1e-6:
					tail_dir = tail_dir.normalized()
					var tail_end: Vector3 = (pts[n] as Vector3) + tail_dir * seg
					var tail_pushed := _hair_push(tail_end, pts[n], volumes, (ch["side"] as Vector3), seg)
					pts[n] = (pts[n] as Vector3) + ((tail_pushed[0] as Vector3) - tail_end)
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


# One volume per entry: a capsule from a to b with radius r (a sphere
# when a == b), fitted from posed bone transforms this frame.
func _hair_volumes(head: Transform3D, skull: Vector3, chest: Transform3D, back: Vector3) -> Array:
	var vols: Array = []
	vols.append({"a": skull, "b": skull, "r": _skull_radius, "side": true})
	vols.append({"a": head * (_skull_local + HAIR_FACE_OFF), "b": head * (_skull_local + HAIR_FACE_OFF), "r": HAIR_FACE_R, "side": true})
	if int(bone_idx.get("neck_lo", -1)) >= 0:
		vols.append({"a": bone_world("neck_lo").origin, "b": head.origin, "r": HAIR_NECK_R, "side": false})
	vols.append({"a": back, "b": back, "r": BACK_RADIUS, "side": false})
	if int(bone_idx.get("body", -1)) >= 0 and int(bone_idx.get("spine_mid", -1)) >= 0:
		# From spine_mid down, not from the chest: the chest region is
		# already covered by the back sphere and the neck capsule, and a
		# torso capsule ending at the chest would push strands above it
		# upward, a shelf for a folded strand to rest on.
		vols.append({"a": bone_world("spine_mid").origin, "b": bone_world("body").origin, "r": HAIR_TORSO_R, "side": false})
	for s in ["l", "r"]:
		if int(bone_idx.get("sh_" + s, -1)) >= 0:
			var sh: Vector3 = bone_world("sh_" + s).origin
			vols.append({"a": sh, "b": sh, "r": HAIR_SHOULDER_R, "side": false})
	return vols


func _closest_on_segment(p: Vector3, a: Vector3, b: Vector3) -> Vector3:
	var ab := b - a
	var denom := ab.length_squared()
	if denom < 1e-12:
		return a
	var t := clampf((p - a).dot(ab) / denom, 0.0, 1.0)
	return a + ab * t


# Closest pair between segments p1-q1 and p2-q2, as [point_on_first, point_on_second].
func _seg_seg_closest(p1: Vector3, q1: Vector3, p2: Vector3, q2: Vector3) -> Array:
	var d1 := q1 - p1
	var d2 := q2 - p2
	var r := p1 - p2
	var a := d1.dot(d1)
	var e := d2.dot(d2)
	var f := d2.dot(r)
	if a < 1e-12 and e < 1e-12:
		return [p1, p2]
	if a < 1e-12:
		return [p1, _closest_on_segment(p1, p2, q2)]
	if e < 1e-12:
		return [_closest_on_segment(p2, p1, q1), p2]
	var b := d1.dot(d2)
	var c := d1.dot(r)
	var denom := a * e - b * b
	var s := clampf((b * f - c * e) / denom, 0.0, 1.0) if absf(denom) > 1e-12 else 0.0
	var t := (b * s + f) / e
	if t < 0.0:
		t = 0.0
		s = clampf(-c / a, 0.0, 1.0)
	elif t > 1.0:
		t = 1.0
		s = clampf((b - c) / a, 0.0, 1.0)
	return [p1 + d1 * s, p2 + d2 * t]


# Push q out of every volume, first as a point (capsule test), then as
# the segment from above to q, so a chord cannot dip through between
# two cleared points. Returns [new_q, first_push_direction_or_zero].
# The remembered side applies only to head/face volumes, where a flip
# means crossing through her head; for torso/shoulder volumes a strand
# must be free to slide past, and replacing its push direction with
# the side would hold it at that height. Even for head/face, the side
# only blends into the push (never replaces it), so the outward and
# downward components that let a draped strand slide off survive.
# Passes repeat until nothing moves, because a push out of one volume
# can land inside another. The result is clamped back to about one
# segment length from above: a push that stretches a segment lets the
# strand fold or stretch over following frames.
func _hair_push(q: Vector3, above: Vector3, volumes: Array, side_world: Vector3, seg: float) -> Array:
	var out := q
	var push_dir := Vector3.ZERO
	for _pass in 3:
		var moved := false
		for vol in volumes:
			var a: Vector3 = vol["a"]
			var b: Vector3 = vol["b"]
			var r: float = vol["r"] + HAIR_RIBBON_M
			var use_side: bool = bool(vol.get("side", false)) and side_world.length() > 1e-6
			var closest: Vector3 = _closest_on_segment(out, a, b)
			var d := out - closest
			if d.length() < r:
				var dir := d.normalized() if d.length() > 1e-6 else (out - a).normalized() if (out - a).length() > 1e-6 else Vector3.UP
				if use_side and dir.dot(side_world) < 0.0:
					dir = (dir + side_world.normalized()).normalized()
				out = closest + dir * r
				moved = true
				if push_dir.length() < 1e-6:
					push_dir = dir
			var pair := _seg_seg_closest(above, out, a, b)
			var seg_pt: Vector3 = pair[0]
			var axis_pt: Vector3 = pair[1]
			var sd := seg_pt - axis_pt
			if sd.length() < r:
				var dir := sd.normalized() if sd.length() > 1e-6 else (out - a).normalized() if (out - a).length() > 1e-6 else Vector3.UP
				if use_side and dir.dot(side_world) < 0.0:
					dir = (dir + side_world.normalized()).normalized()
				out += dir * (r - sd.length())
				moved = true
				if push_dir.length() < 1e-6:
					push_dir = dir
		var dd := out - above
		if dd.length() > seg * 1.05:
			out = above + dd.normalized() * seg * 1.05
			moved = true
		if not moved:
			break
	return [out, push_dir]


func _outside(q: Vector3, centre: Vector3, radius: float) -> Vector3:
	var d := q - centre
	var l := d.length()
	if l >= radius or l < 1e-6:
		return q
	return centre + d / l * radius
