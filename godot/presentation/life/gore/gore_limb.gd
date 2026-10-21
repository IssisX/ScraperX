extends Node3D
## A severed limb, placed from the sim's severed body (position = its
## capsule's centre, rotation: the capsule along local y). The climber's own
## figure with everything but that limb collapsed away (gore_body.gd), soaked
## from the torn end, the torn end itself (gore_wound.gd) at its root.

const Kit := preload("res://presentation/life/gore/gore_kit.gd")
const Wound := preload("res://presentation/life/gore/gore_wound.gd")
const Body := preload("res://presentation/life/gore/gore_body.gd")

var wound: Node3D
var is_leg := false
var length := 0.62
var _body: Body
var _holder: Node3D
var _k := 0
var _blood: ShaderMaterial


## k: 0 left arm, 1 right arm, 2 left leg, 3 right leg.
func build_limb(k: int, seed: int) -> void:
	_k = k
	is_leg = k >= 2
	_holder = Node3D.new()
	add_child(_holder)
	_body = Body.new()
	_holder.add_child(_body)
	_body.build(null)
	_body.keep_only(k)
	var root: String = Body.LIMB_ROOT[k]
	var tip: String = Body.LIMB_TIP[k]
	var a: Vector3 = _body.rest_origin(root)
	var b: Vector3 = _body.rest_origin(tip)
	length = a.distance_to(b) + (0.12 if not is_leg else 0.1)
	var blood := Kit.body_gore_material(float(seed))
	blood.set_shader_parameter("spatter", 0.7)
	_blood = blood
	for n in ["shadow", "trousers", "boots"]:
		if _body.meshes.has(n):
			_body.meshes[n].material_overlay = blood
	wound = Wound.new()
	add_child(wound)
	wound.setup(0.05 if not is_leg else 0.078, seed, Color(0.72, 0.55, 0.47) if not is_leg else Color("24272c"),
		Color(0.72, 0.55, 0.47), 0.035, 0.08 if not is_leg else 0.1)
	wound.tear()


## Place from the sim: `centre` the capsule centre, `basis` its frame (the
## capsule along y; the root end up +y).
func place(centre: Vector3, basis: Basis) -> void:
	# The limb's own axis (rest, model space): root -> tip. Align it with -y of
	# the sim's frame, the root half a length up.
	var root: String = Body.LIMB_ROOT[_k]
	var tip: String = Body.LIMB_TIP[_k]
	var root_world_now := _body.world_bone(root).origin
	var tip_world_now := _body.world_bone(tip).origin
	var holder_xf := _holder.global_transform
	var local_root := holder_xf.affine_inverse() * root_world_now
	var local_tip := holder_xf.affine_inverse() * tip_world_now
	var axis := (local_tip - local_root).normalized()
	var align := Basis(Quaternion(axis, Vector3.DOWN))
	var root_at := centre + basis.y.normalized() * (length * 0.5)
	var xf_basis := basis.orthonormalized() * align
	_holder.global_transform = Transform3D(xf_basis, root_at - xf_basis * local_root)
	wound.global_transform = Transform3D(Basis(Quaternion(Vector3.UP, basis.y.normalized())), root_at)
	_blood.set_shader_parameter("wound_a", Vector4(root_at.x, root_at.y, root_at.z, 0.13 if not is_leg else 0.17))


func root_transform() -> Transform3D:
	return wound.global_transform
