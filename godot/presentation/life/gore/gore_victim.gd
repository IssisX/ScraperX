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
var ribs: Array[MeshInstance3D] = []
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
	var rng := RandomNumberGenerator.new()
	rng.seed = 77
	for i in 6:
		var rib := MeshInstance3D.new()
		var c := CylinderMesh.new()
		c.top_radius = 0.003
		c.bottom_radius = 0.011
		c.height = rng.randf_range(0.08, 0.14)
		rib.mesh = c
		rib.material_override = Kit.material("rib")
		rib.visible = false
		add_child(rib)
		rib.set_meta("at", Vector3(rng.randf_range(-0.12, 0.12), rng.randf_range(1.2, 1.36), 0.14))
		rib.set_meta("tilt", Vector3(PI * 0.5 + rng.randf_range(-0.5, 0.5), 0.0, rng.randf_range(-0.6, 0.6)))
		ribs.append(rib)
	for k in 4:
		var w := Wound.new()
		add_child(w)
		w.setup(0.05 if k < 2 else 0.078, 100 + k, Color(0.72, 0.55, 0.47) if k < 2 else Color("24272c"),
			Color(0.72, 0.55, 0.47), 0.05, 0.08 if k < 2 else 0.1)
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
	for rib in ribs:
		rib.visible = crush > 0.55 or ruined
		if rib.visible:
			var at: Vector3 = rib.get_meta("at")
			var tilt: Vector3 = rib.get_meta("tilt")
			rib.global_transform = Transform3D(body.global_transform.basis * Basis.from_euler(tilt) *
				Basis.from_scale(Vector3(1, 1.8 if ruined else 1.0, 1)), body.global_transform * (at + Vector3(0, 0, -0.02 * crush)))


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


func head_position() -> Vector3:
	return head.global_position
