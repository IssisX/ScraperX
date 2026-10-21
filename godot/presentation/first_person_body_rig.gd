extends Node3D

# Presentation only. The target owner supplies actual contacts and body phase.
const Anatomy := preload("res://presentation/life/climber/climber_body_rig.gd")
const BODY_FILE := "res://presentation/life/climber/climber_body.glb.bytes"
const MODEL_SCALE := 0.88 / (Anatomy.THIGH + Anatomy.SHIN)
const THIGH := Anatomy.THIGH * MODEL_SCALE
const SHIN := Anatomy.SHIN * MODEL_SCALE
# Model faces +Z; the existing view and sole frames face -Z.
const MODEL_TURN := Basis(Vector3.UP, PI)
const SIDES := ["r", "l"]

var _model: Node3D
var _skeleton: Skeleton3D
var _bones: Dictionary = {}
var _rest: Dictionary = {}
var _foot_bounds: Array[AABB] = []
var _lean := 0.0
var _meshes: Array[MeshInstance3D] = []

func build(cloth: Material, leather: Material, rubber: Material) -> void:
	if _model != null:
		return
	var document := GLTFDocument.new()
	var data := GLTFState.new()
	var bytes := FileAccess.get_file_as_bytes(BODY_FILE)
	var error := document.append_from_buffer(bytes, "", data)
	if error != OK:
		push_error("first-person body: cannot decode %s as GLB (%s)" % [BODY_FILE, error_string(error)])
		return
	_model = document.generate_scene(data)
	if _model == null:
		push_error("first-person body: %s produced no scene" % BODY_FILE)
		return
	add_child(_model)
	var skeletons := _model.find_children("*", "Skeleton3D", true, false)
	if skeletons.is_empty():
		push_error("first-person body: imported climber has no Skeleton3D")
		return
	_skeleton = skeletons[0] as Skeleton3D
	for index in _skeleton.get_bone_count():
		var label := _skeleton.get_bone_name(index)
		_bones[label] = index
		_rest[label] = _skeleton.get_bone_global_rest(index)
	var stripe := StandardMaterial3D.new()
	stripe.albedo_color = Color("b48b32")
	stripe.roughness = 0.7
	var dress := {"trousers": cloth, "jacket": cloth, "jacket_inner": rubber,
		"hivis": stripe, "zip": rubber, "boots": leather, "soles": rubber, "shadow": cloth}
	for child in _model.find_children("*", "MeshInstance3D", true, false):
		var mesh := child as MeshInstance3D
		mesh.material_override = dress.get(String(mesh.name), cloth)
		mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY \
			if String(mesh.name) == "shadow" else GeometryInstance3D.SHADOW_CASTING_SETTING_ON
		mesh.extra_cull_margin = 1.2
		_meshes.append(mesh)
	# Exact sole vertices in each ankle's material frame, for unloaded toe roll.
	for side in 2:
		var inverse: Transform3D = (_rest["an_" + SIDES[side]] as Transform3D).affine_inverse()
		var bounds := AABB()
		var first := true
		for mesh in _meshes:
			if String(mesh.name) != "soles":
				continue
			for surface in mesh.mesh.get_surface_count():
				var vertices: PackedVector3Array = mesh.mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX]
				for vertex in vertices:
					if (vertex.x < 0.0) != (SIDES[side] == "l"):
						continue
					var local := inverse * vertex
					bounds = AABB(local, Vector3.ZERO) if first else bounds.expand(local)
					first = false
		_foot_bounds.append(bounds)

func ready_to_pose() -> bool:
	return _skeleton != null and _bones.has("an_l") and _bones.has("an_r")

func reset_pose() -> void:
	_lean = 0.0
	if _skeleton != null:
		_skeleton.reset_bone_poses()

func pose_body(hip: Vector3, frame: Basis, state: Dictionary, delta: float, camera: Transform3D) -> void:
	if not ready_to_pose():
		return
	var crouch := clampf(float(state.get("crouch_visual_01", 0.0)), 0.0, 1.0)
	var sliding := bool(state.get("player_sliding", false))
	var lean_goal := -0.28 if sliding else 0.52 * crouch
	var dt := clampf(delta, 0.0, 0.12) if is_finite(delta) else 0.0
	_lean = lerpf(_lean, lean_goal, 1.0 - exp(-14.0 * dt))
	for side in SIDES:
		var bone: int = _bones["hp_" + side]
		_skeleton.set_bone_pose_position(bone, _skeleton.get_bone_rest(bone).origin)
	var rotation := frame * MODEL_TURN
	# Keep the actual garment pelvis at the existing presented hip location.
	_model.global_transform = Transform3D(rotation.scaled(Vector3.ONE * MODEL_SCALE),
		hip - rotation * (Anatomy.PELVIS * MODEL_SCALE))
	_skeleton.set_bone_pose_rotation(_bones["body"], Quaternion(Vector3.RIGHT, _lean))
	_skeleton.set_bone_pose_rotation(_bones["neck_lo"], Quaternion(Vector3.RIGHT, -_lean * 0.6))
	# Fit the upper garment beneath the actual eye. Leg scaling must not put
	# the lens inside a taller model's collar; lower-body contacts stay fixed.
	var spine: int = _bones["spine_lo"]
	var rest := _skeleton.get_bone_rest(spine).origin
	_skeleton.set_bone_pose_position(spine, rest)
	if camera.origin.is_finite():
		var neck := _model.global_transform * _global("neck_lo").origin
		var goal := camera.origin - Vector3.UP * 0.123 + frame.z * 0.225
		var shift := (goal - neck).limit_length(0.14)
		var local_shift := _global("body").basis.inverse() * (_model.global_basis.inverse() * shift)
		_skeleton.set_bone_pose_position(spine, rest + local_shift)

func hip_world(side: int) -> Vector3:
	return _model.global_transform * _global("hp_" + SIDES[side]).origin

func sole_offset(side: int, frame: Basis) -> Vector3:
	var foot := frame * MODEL_TURN * (_rest["an_" + SIDES[side]] as Transform3D).basis
	return foot * (_sole_mid(side) * MODEL_SCALE)

func unloaded_lift(side: int, frame: Basis) -> float:
	var rotation := frame * MODEL_TURN * (_rest["an_" + SIDES[side]] as Transform3D).basis
	var bounds := _foot_bounds[side]
	var mid := _sole_mid(side)
	var lowest := INF
	for corner in 8:
		var point := bounds.position + bounds.size * Vector3(
			1.0 if corner & 1 else 0.0, 1.0 if corner & 2 else 0.0, 1.0 if corner & 4 else 0.0)
		lowest = minf(lowest, (rotation * ((point - mid) * MODEL_SCALE)).y)
	return maxf(0.0, -lowest)

func pose_leg(side: int, hip: Vector3, knee: Vector3, ankle: Vector3, frame: Basis,
		bend_direction: Vector3) -> void:
	if not ready_to_pose():
		return
	var s: String = SIDES[side]
	var inverse := _model.global_transform.affine_inverse()
	var h := inverse * hip
	var k := inverse * knee
	var a := inverse * ankle
	# A bounded hip lean can stretch the garment, never move its real sole target.
	var parent := _global("body")
	_skeleton.set_bone_pose_position(_bones["hp_" + s], parent.affine_inverse() * h)
	var pole := inverse.basis * bend_direction
	_set_basis("hp_" + s, _frame(h - k, pole), "body")
	_set_basis("kn_" + s, _frame(k - a, pole), "hp_" + s + "_x")
	var world_foot := frame * MODEL_TURN * (_rest["an_" + s] as Transform3D).basis
	_set_basis("an_" + s, _model.global_basis.orthonormalized().inverse() * world_foot,
		"kn_" + s + "_x")

func pose_shadow(camera: Transform3D, wrists: Array) -> void:
	if not ready_to_pose():
		return
	var inverse := _model.global_transform.affine_inverse()
	var look := inverse.basis * (-camera.basis.z)
	var pitch := asin(clampf(look.normalized().y, -1.0, 1.0))
	_skeleton.set_bone_pose_rotation(_bones["neck_hi"], Quaternion(Vector3.RIGHT, -pitch * 0.4))
	_skeleton.set_bone_pose_rotation(_bones["head"], Quaternion(Vector3.RIGHT, -pitch * 0.6))
	for side in 2:
		var s: String = SIDES[side]
		var sh := _global("sh_" + s).origin
		var elbow_rest: Transform3D = _rest["el_" + s]
		var shoulder_rest: Transform3D = _rest["sh_" + s]
		var wrist_rest: Transform3D = _rest["wr_" + s]
		var upper := shoulder_rest.origin.distance_to(elbow_rest.origin)
		var lower := elbow_rest.origin.distance_to(wrist_rest.origin)
		var target := sh + Vector3(0.0, -(upper + lower) * 0.98, 0.02)
		if wrists.size() == 2 and wrists[side] is Vector3 and (wrists[side] as Vector3).is_finite() \
				and (wrists[side] as Vector3).distance_to(_model.global_transform * sh) < (upper + lower) * MODEL_SCALE + 0.15:
			target = inverse * (wrists[side] as Vector3)
		var offset := target - sh
		var reach := clampf(offset.length(), absf(upper - lower) + 0.001, upper + lower - 0.001)
		var direction := offset.normalized() if offset.length_squared() > 0.000001 else Vector3.DOWN
		var pole := Vector3(-0.2 if side == 0 else 0.2, -0.4, -0.7)
		pole = (pole - direction * pole.dot(direction)).normalized()
		var along := (upper * upper - lower * lower + reach * reach) / (2.0 * reach)
		var elbow := sh + direction * along + pole * sqrt(maxf(0.0, upper * upper - along * along))
		_set_basis("sh_" + s, _frame(sh - elbow, pole), "chest")
		_set_basis("el_" + s, _frame(elbow - (sh + direction * reach), pole), "sh_" + s + "_x")

func sole_world(side: int) -> Vector3:
	return _model.global_transform * (_global("an_" + SIDES[side]) * _sole_mid(side))

func sole_normal(side: int) -> Vector3:
	var s: String = SIDES[side]
	var rest_basis: Basis = (_rest["an_" + s] as Transform3D).basis
	var native_up := rest_basis.inverse() * Vector3.UP
	return (_model.global_basis * _global("an_" + s).basis * native_up).normalized()

func joint_world(side: int, joint: String) -> Vector3:
	return _model.global_transform * _global(joint + "_" + SIDES[side]).origin

func pelvis_world() -> Vector3:
	return _model.global_transform * _global("body").origin

func _sole_mid(side: int) -> Vector3:
	return (Anatomy.HEEL_R + Anatomy.TOE_R) * 0.5 if side == 0 \
		else (Anatomy.HEEL_L + Anatomy.TOE_L) * 0.5

func _global(label: String) -> Transform3D:
	return _skeleton.get_bone_global_pose(_bones[label])

func _set_basis(label: String, basis: Basis, parent: String) -> void:
	var previous := _global(parent).basis.orthonormalized()
	_skeleton.set_bone_pose_rotation(_bones[label],
		(previous.inverse() * basis.orthonormalized()).get_rotation_quaternion())

func _frame(y_axis: Vector3, hint: Vector3) -> Basis:
	var y := y_axis.normalized()
	var x := y.cross(hint).normalized()
	if x.length_squared() < 0.0001:
		x = y.cross(Vector3.RIGHT).normalized()
	return Basis(x, y, x.cross(y))
