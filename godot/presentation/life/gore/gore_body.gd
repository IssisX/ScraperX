extends Node3D
## The climber's scanned body (life/climber/climber_body.glb: MakeHuman, 56
## bones; the "shadow" mesh is the whole skinned figure, head and hands
## included) dressed for being seen from outside, with bone-driven damage:
## a limb gone is its root bone collapsed (`sever`), or the opposite, only
## that limb kept (`keep_only`), for the severed piece itself.
## Model space: soles on y = 0, facing +z.

const BODY_FILE := "res://presentation/life/climber/climber_body.glb"
const COLLAPSE := 0.002

var skel: Skeleton3D
var meshes := {}   # name -> MeshInstance3D
var bone := {}
var skin_mat: StandardMaterial3D
var jacket_mat: StandardMaterial3D
var trouser_mat: StandardMaterial3D
var boot_mat: StandardMaterial3D
var _soak_overlay: ShaderMaterial

const LIMB_ROOT := ["sh_l", "sh_r", "hp_l", "hp_r"]
const LIMB_TIP := ["wr_l", "wr_r", "an_l", "an_r"]


func build(soak: ShaderMaterial = null) -> bool:
	var doc := GLTFDocument.new()
	var state := GLTFState.new()
	if doc.append_from_file(BODY_FILE, state) != OK:
		push_error("gore body: cannot read %s" % BODY_FILE)
		return false
	var scene: Node3D = doc.generate_scene(state) as Node3D
	add_child(scene)
	skel = scene.find_children("*", "Skeleton3D", true, false)[0] as Skeleton3D
	for i in skel.get_bone_count():
		bone[skel.get_bone_name(i)] = i
	skin_mat = StandardMaterial3D.new()
	skin_mat.albedo_color = Color(0.72, 0.55, 0.47)
	skin_mat.roughness = 0.5
	skin_mat.rim_enabled = true
	skin_mat.rim = 0.15
	jacket_mat = StandardMaterial3D.new()
	jacket_mat.albedo_color = Color("2a3026")
	jacket_mat.roughness = 0.9
	trouser_mat = StandardMaterial3D.new()
	trouser_mat.albedo_color = Color("24272c")
	trouser_mat.roughness = 0.88
	boot_mat = StandardMaterial3D.new()
	boot_mat.albedo_color = Color("1c130b")
	boot_mat.roughness = 0.8
	var hivis := StandardMaterial3D.new()
	hivis.albedo_color = Color("b8611d")
	var dark := StandardMaterial3D.new()
	dark.albedo_color = Color("0d0d0c")
	var dress := {"shadow": skin_mat, "jacket": jacket_mat, "jacket_inner": dark, "hivis": hivis, "zip": dark,
		"trousers": trouser_mat, "boots": boot_mat, "soles": dark}
	_soak_overlay = soak
	for node in scene.find_children("*", "MeshInstance3D", true, false):
		var m := node as MeshInstance3D
		meshes[String(m.name)] = m
		m.material_override = dress.get(String(m.name), jacket_mat)
		m.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
		m.extra_cull_margin = 2.0
		if soak != null and String(m.name) in ["shadow", "jacket", "trousers"]:
			m.material_overlay = soak
	return true


func reset_pose() -> void:
	for i in skel.get_bone_count():
		skel.reset_bone_pose(i)


## Collapse a limb (0 left arm, 1 right arm, 2 left leg, 3 right leg).
func sever(k: int, gone: bool) -> void:
	skel.set_bone_pose_scale(int(bone[LIMB_ROOT[k]]), Vector3.ONE * (COLLAPSE if gone else 1.0))


## Only limb k shows: everything collapses into the body's root but it.
func keep_only(k: int) -> void:
	var b := int(bone["body"])
	skel.set_bone_pose_scale(b, Vector3.ONE * COLLAPSE)
	# Undo the collapse down the limb's chain at its root.
	var root := int(bone[LIMB_ROOT[k]])
	var chain_scale := 1.0 / COLLAPSE
	skel.set_bone_pose_scale(root, Vector3.ONE * chain_scale)
	for n in ["jacket", "jacket_inner", "hivis", "zip"]:
		if meshes.has(n):
			meshes[n].visible = false
	if k < 2:
		for n in ["trousers", "boots", "soles"]:
			if meshes.has(n):
				meshes[n].visible = false


## A bone's rest-pose origin, model space.
func rest_origin(n: String) -> Vector3:
	return skel.get_bone_global_rest(int(bone[n])).origin


## A bone's posed global transform (world).
func world_bone(n: String) -> Transform3D:
	return skel.global_transform * skel.get_bone_global_pose(int(bone[n]))


## Turn a bone (local rotation on top of its rest).
func turn(n: String, q: Quaternion) -> void:
	var i := int(bone[n])
	skel.set_bone_pose_rotation(i, skel.get_bone_rest(i).basis.get_rotation_quaternion() * q)


func scale_bone(n: String, s: Vector3) -> void:
	skel.set_bone_pose_scale(int(bone[n]), s)
