extends Node3D
## The player's body, seen from outside while she has it (the fatality's
## shots): the climber's scanned figure (gore_body.gd) placed from the sim's
## position and body quaternion, lying as the sim has it. Limbs go as the sim
## says (their root bones collapsed), each stump a wound at the joint. The
## knee pin caves the chest (the chest bone crushed, rib ends out through
## the jacket); the fall ruins it.

const Kit := preload("res://presentation/life/gore/gore_kit.gd")
const Wound := preload("res://presentation/life/gore/gore_wound.gd")
const Body := preload("res://presentation/life/gore/gore_body.gd")

# The sim's position is the capsule centre; the figure's soles are 0.9 under.
const CENTRE_ABOVE_SOLES := 0.9

var body: Body
var head: Node3D               # follows the head bone (blood from the mouth)
var stumps: Array[Node3D] = []
var ribs: Array[MeshInstance3D] = []   # the crush's and the ruin's (tests)
var _crush_ribs: MeshInstance3D
var _crush_meat: MeshInstance3D
var _burst_ribs: MeshInstance3D
var _burst_meat: MeshInstance3D
var _soak: ShaderMaterial   # body_gore: wounds, runs, the strained joint
var _wound_r := [0.0, 0.0, 0.0, 0.0]
var crush := 0.0
var ruined := false
var _ruin_turns := {}
var _lost := [false, false, false, false]
var _strain := [0.0, 0.0, 0.0, 0.0]


func build() -> void:
	_soak = Kit.body_gore_material(5.0)
	body = Body.new()
	add_child(body)
	body.build(_soak)
	# Facing: the figure faces +z; the sim's body faces -z.
	body.transform = Transform3D(Basis(Vector3.UP, PI), Vector3(0, -CENTRE_ABOVE_SOLES, 0))
	head = Node3D.new()
	add_child(head)
	# The ribcage, hidden until the knee goes in: a few ribs snapped through
	# the front of the chest; at the ruin the cage burst open, the meat of
	# the chest out over it.
	var chest_frame := Transform3D(Basis(Vector3(1, 0, 0), Vector3(0, 0, 1), Vector3(0, -1, 0)), Vector3(0, 1.3, 0.12))
	_crush_ribs = _part_mesh(Kit.ribcage(31, 5, 0.45), Kit.material("bone"), chest_frame)
	_crush_meat = _part_mesh(Kit.wound_mass(0.05, 12), Kit.material("flesh"), chest_frame.translated_local(Vector3(0.02, -0.01, 0.0)))
	_burst_ribs = _part_mesh(Kit.ribcage(57, 10, 1.0), Kit.material("bone"), chest_frame.translated_local(Vector3(0, 0.01, 0)))
	_burst_meat = _part_mesh(Kit.wound_mass(0.1, 44), Kit.material("flesh"), chest_frame.translated_local(Vector3(0, -0.015, 0)))
	for k in 4:
		var w := Wound.new()
		add_child(w)
		w.setup(0.05 if k < 2 else 0.078, 100 + k, Color(0.72, 0.55, 0.47) if k < 2 else Color("24272c"),
			Color(0.72, 0.55, 0.47), 0.03, 0.08 if k < 2 else 0.1)
		w.band_on = false   # the body's own overlay shows the strain
		stumps.append(w)
	_pose_lying()


# Arms out a little, palms up, legs straight: how she lays it on the plates.
func _pose_lying() -> void:
	body.reset_pose()
	body.turn("sh_l", Quaternion(Vector3.FORWARD, -0.25))
	body.turn("sh_r", Quaternion(Vector3.FORWARD, 0.25))
	body.turn("el_l", Quaternion(Vector3.RIGHT, 0.25))
	body.turn("el_r", Quaternion(Vector3.RIGHT, 0.25))
	body.turn("neck_hi", Quaternion(Vector3.RIGHT, -0.25))


# Hung from her hand (the hold, seen from outside): the held leg's ankle
# where her fingers have it (the native's held_ankle, the point her drawn hand
# is put on: matron_view.gd), the other leg and the arms hanging under
# gravity as far as their joints let them, swaying; in the rip, the arm she
# pulls reaching for her other hand. Out of her hand (the let-go's fall) the
# legs hang straight from the body.
const FREE_LEG_MAX := 1.2      # the free leg off its line, toward world down (rad)
const SWAY := 0.18
const GRIP_TRUST_M := 0.5      # her drawn hand this near the native ankle: his ankle goes into it
var hanging := false
var held_side := -1            # the leg her fingers have: 2 left, 3 right
var _clock := 0.0


func hang(hold: Dictionary, lost: Array, delta: float, grip_at := Vector3.INF) -> void:
	hanging = true
	_clock += delta
	var skel := body.skel
	for n in ["hp_l", "hp_r", "kn_l", "kn_r", "sh_l", "sh_r", "el_l", "el_r", "neck_hi"]:
		var i := int(body.bone[n])
		skel.set_bone_pose_rotation(i, skel.get_bone_rest(i).basis.get_rotation_quaternion())
	var q: Quaternion = hold.get("body_rotation", Quaternion.IDENTITY)
	if not q.is_normalized():
		q = q.normalized() if q.length() > 1.0e-6 else Quaternion.IDENTITY
	var body_down := q * Vector3.DOWN
	var ankle: Vector3 = hold.get("held_ankle", Vector3.ZERO)
	var gripped := bool(hold.get("held", false)) and ankle.length_squared() > 1.0e-6
	held_side = -1
	if gripped:
		held_side = 2 if not bool(lost[2]) else (3 if not bool(lost[3]) else -1)
	for k in [2, 3]:
		if bool(lost[k]):
			continue
		var s := "l" if k == 2 else "r"
		if k == held_side:
			# Into her drawn hand where it is on him (it is put on this same
			# native point, drawn by her own reach), else the native point.
			var at := ankle
			if grip_at.is_finite() and grip_at.distance_to(ankle) < GRIP_TRUST_M:
				at = grip_at
			_leg_to(s, at, q * Vector3.FORWARD)
			continue
		var sway := Vector3(sin(_clock * 2.3 + float(k)), 0.0, cos(_clock * 1.9 + float(k))) * SWAY
		var dir := body_down
		if gripped:
			# Hung by the other ankle: this leg falls toward world down, as far
			# as a hip opens.
			var to_down := Vector3.DOWN + sway
			var a := minf(body_down.angle_to(to_down), FREE_LEG_MAX)
			var axis := body_down.cross(to_down)
			if axis.length() > 1.0e-4:
				dir = body_down.rotated(axis.normalized(), a)
		_aim_dir("hp_" + s, "an_" + s, dir)
	for k in [0, 1]:
		if bool(lost[k]):
			continue
		var s := "l" if k == 0 else "r"
		var hand2: Vector3 = hold.get("matron_hand2", Vector3.ZERO)
		if int(hold.get("wrist_side", -1)) == k and hand2.length_squared() > 1.0e-6:
			_aim_at("sh_" + s, "wr_" + s, hand2)
			continue
		# Arms hang (past his head, hung head down), a little out, swaying.
		var out := q * Vector3(0.35 if k == 1 else -0.35, 0.0, 0.0)
		var flail := Vector3(sin(_clock * 3.1 + 1.7 * float(k)), 0.0, cos(_clock * 2.7 + 0.9 * float(k))) * SWAY * 1.5
		_aim_dir("sh_" + s, "wr_" + s, Vector3.DOWN + out + flail)


func unhang() -> void:
	hanging = false
	held_side = -1
	_pose_lying()


## His held ankle as drawn (the leg her fingers have), for a test.
func held_ankle_drawn() -> Vector3:
	if held_side < 2:
		return Vector3.INF
	return body.world_bone("an_l" if held_side == 2 else "an_r").origin


# The leg's ankle to `world_at` by two-bone reach: the knee bent toward
# `front` (his front, world) as much as the hip-to-ankle span is short of the
# leg's length.
func _leg_to(s: String, world_at: Vector3, front: Vector3) -> void:
	var hip := body.world_bone("hp_" + s).origin
	var knee := body.world_bone("kn_" + s).origin
	var foot := body.world_bone("an_" + s).origin
	var thigh := hip.distance_to(knee)
	var shin := knee.distance_to(foot)
	var to := world_at - hip
	var d := clampf(to.length(), absf(thigh - shin) + 0.01, thigh + shin - 0.001)
	var dir := to.normalized() if to.length() > 1.0e-4 else front
	var axis := dir.cross(front)
	if axis.length() < 1.0e-4:
		axis = dir.cross(Vector3.UP if absf(dir.y) < 0.9 else Vector3.RIGHT)
	axis = axis.normalized()
	var a := acos(clampf((thigh * thigh + d * d - shin * shin) / (2.0 * thigh * d), -1.0, 1.0))
	var knee_at := hip + dir.rotated(axis, a) * thigh
	_aim_at("hp_" + s, "kn_" + s, knee_at)
	_aim_at("kn_" + s, "an_" + s, world_at)


# Turn the bone `root` so the limb's `tip` lies toward `world_dir` from it.
func _aim_dir(root: String, tip: String, world_dir: Vector3) -> void:
	var skel := body.skel
	var want := (skel.global_transform.basis.inverse() * world_dir)
	if want.length() < 1.0e-5:
		return
	_turn_to(root, tip, want.normalized())


# Turn the bone `root` so the limb's `tip` goes to `world_at`.
func _aim_at(root: String, tip: String, world_at: Vector3) -> void:
	var skel := body.skel
	var at := skel.global_transform.affine_inverse() * world_at
	var from := skel.get_bone_global_pose(int(body.bone[root])).origin
	if (at - from).length() < 1.0e-4:
		return
	_turn_to(root, tip, (at - from).normalized())


func _turn_to(root: String, tip: String, want: Vector3) -> void:
	var skel := body.skel
	var ri := int(body.bone[root])
	var rg := skel.get_bone_global_pose(ri)
	var cur := skel.get_bone_global_pose(int(body.bone[tip])).origin - rg.origin
	if cur.length() < 1.0e-4:
		return
	cur = cur.normalized()
	var arc: Quaternion
	if cur.dot(want) < -0.9999:
		var axis := cur.cross(Vector3.RIGHT)
		if axis.length() < 1.0e-3:
			axis = cur.cross(Vector3.FORWARD)
		arc = Quaternion(axis.normalized(), PI)
	else:
		arc = Quaternion(cur, want)
	var new_basis := Basis(arc) * rg.basis.orthonormalized()
	var parent := skel.get_bone_parent(ri)
	var pb := skel.get_bone_global_pose(parent).basis.orthonormalized() if parent >= 0 else Basis()
	skel.set_bone_pose_rotation(ri, (pb.inverse() * new_basis).get_rotation_quaternion())


func apply(lost: Array, strain: Array, crush_amount: float, soak: float) -> void:
	crush = clampf(crush_amount, 0.0, 1.0)
	var worst := -1
	for k in 4:
		_lost[k] = bool(lost[k])
		_strain[k] = float(strain[k])
		body.sever(k, _lost[k])
		if _lost[k]:
			stumps[k].tear()
			# The blood round the stump spreads (the owner calls this every frame).
			_wound_r[k] = minf(_wound_r[k] + 0.0025, 0.2)
		else:
			stumps[k].heal()
			_wound_r[k] = 0.0
			if worst < 0 or _strain[k] > _strain[worst]:
				worst = k
		# Pulled: the limb drawn out of its socket a few centimetres, the
		# skin stretched over the gap by the skinning.
		if not _lost[k]:
			var root: String = Body.LIMB_ROOT[k]
			var i := int(body.bone[root])
			var rest: Vector3 = body.skel.get_bone_rest(i).origin
			var out: Vector3 = Vector3(signf(rest.x), 0.0, 0.0) if k < 2 else Vector3.DOWN
			body.skel.set_bone_pose_position(i, rest + out * 0.07 * _strain[k] * _strain[k])
	var chest_s := Vector3(1.0 + 0.14 * crush, 1.0 - 0.08 * crush, 1.0 - 0.45 * crush)
	if ruined:
		chest_s = Vector3(1.3, 0.75, 0.42)
	body.scale_bone("chest", chest_s)
	_place_wounds()
	_soak.set_shader_parameter("spatter", clampf(soak * 2.5, 0.0, 1.0))
	var keys := ["wound_a", "wound_b", "wound_c", "wound_d"]
	for k in 4:
		var c: Vector3 = stumps[k].global_position
		_soak.set_shader_parameter(keys[k], Vector4(c.x, c.y, c.z, _wound_r[k]))
	if worst >= 0 and _strain[worst] > 0.02:
		var j: Transform3D = stumps[worst].global_transform
		_soak.set_shader_parameter("joint", j.origin)
		_soak.set_shader_parameter("joint_axis", j.basis.y)
		_soak.set_shader_parameter("strain", clampf(_strain[worst], 0.0, 1.0))
		_soak.set_shader_parameter("split", clampf((_strain[worst] - 0.75) / 0.25, 0.0, 1.0))
	else:
		_soak.set_shader_parameter("strain", 0.0)
		_soak.set_shader_parameter("split", 0.0)


func _place_wounds() -> void:
	for k in 4:
		var root: String = Body.LIMB_ROOT[k]
		var tip: String = ["el_l", "el_r", "kn_l", "kn_r"][k]
		var r := body.world_bone(root)
		# The limb's rest direction out of the joint, in the parent's frame
		# (its own transform may be collapsed).
		var d := (body.global_transform.basis * (body.skel.get_bone_global_rest(int(body.bone[tip])).origin
			- body.skel.get_bone_global_rest(int(body.bone[root])).origin)).normalized()
		stumps[k].global_transform = Transform3D(Basis(Quaternion(Vector3.UP, d)), r.origin + d * 0.02)
	head.global_position = body.world_bone("head").origin + body.global_transform.basis * Vector3(0, 0.02, 0.09)
	_crush_ribs.visible = crush > 0.55 and not ruined
	_crush_meat.visible = _crush_ribs.visible
	_burst_ribs.visible = ruined
	_burst_meat.visible = ruined


## Hit the bottom: flattened, twisted, what is left broken the wrong way.
func ruin() -> void:
	ruined = true
	var rng := RandomNumberGenerator.new()
	rng.seed = 4242
	for n in ["el_l", "el_r", "kn_l", "kn_r", "neck_lo", "spine_mid", "sh_l", "sh_r", "hp_l", "hp_r", "wr_l", "wr_r"]:
		body.turn(n, Quaternion.from_euler(Vector3(rng.randf_range(-1.7, 1.7), rng.randf_range(-1.2, 1.2),
			rng.randf_range(-1.5, 1.5))))
	body.scale_bone("head", Vector3(1.15, 0.7, 1.2))
	body.scale_bone("spine_lo", Vector3(1.2, 0.8, 0.7))


func unruin() -> void:
	ruined = false
	_pose_lying()


# A mesh on the figure, in its model space (follows the body's root).
func _part_mesh(mesh: Mesh, mat: Material, at: Transform3D) -> MeshInstance3D:
	var m := MeshInstance3D.new()
	m.mesh = mesh
	m.material_override = mat
	m.transform = at
	m.visible = false
	body.add_child(m)
	ribs.append(m)
	return m


func head_position() -> Vector3:
	return head.global_position
