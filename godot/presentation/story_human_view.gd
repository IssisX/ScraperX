extends Node3D

# Presentation for the native story actor. The supplied transform is at its
# soles, in world meters. This node never creates collision or locomotion.
const MODEL_PATH := "res://assets/characters/ro_human.glb"
const FaceFit := preload("res://presentation/identity/portrait_face_fit.gd")
const HumanGait := preload("res://presentation/story_human_gait.gd")
var _gait := HumanGait.new()
var _model: Node3D
var _skeleton: Skeleton3D
var _meshes: Array[MeshInstance3D] = []
var _original_skin: Dictionary = {}
var _rest: Array[Transform3D] = []
var _head := -1
var _spine := -1
var _arms: Array[int] = []
var _last_tick := -1
var last_error := ""

func _ready() -> void:
	_build()

func _build() -> bool:
	if _model != null:
		return true
	var scene := load(MODEL_PATH) as PackedScene
	if scene == null:
		last_error = "Cannot load story human model: " + MODEL_PATH
		push_error(last_error)
		return false
	_model = scene.instantiate()
	add_child(_model)
	_model.position.y = .002609
	var skeletons := _model.find_children("*", "Skeleton3D", true, false)
	if skeletons.size() != 1:
		last_error = "Story human requires one imported skeleton"
		push_error(last_error)
		_model.queue_free()
		_model = null
		return false
	_skeleton = skeletons[0]
	if not _gait.configure(_skeleton):
		last_error = _gait.last_error
		push_error(last_error)
		_model.queue_free()
		_model = null
		_skeleton = null
		return false
	for i in _skeleton.get_bone_count():
		_rest.append(_skeleton.get_bone_rest(i))
	_head = _skeleton.find_bone("head")
	_spine = _skeleton.find_bone("spine02")
	for name in ["upperarm01.L", "upperarm01.R"]:
		_arms.append(_skeleton.find_bone(name))
	for node in _model.find_children("*", "MeshInstance3D", true, false):
		_meshes.append(node)
		if node.mesh is ArrayMesh:
			for surface in node.mesh.get_surface_count():
				var material: Material = node.mesh.surface_get_material(surface)
				if material != null and material.resource_name == FaceFit.SKIN_MATERIAL:
					_original_skin[node] = node.mesh
	return true

func apply_portrait(photo: Texture2D, landmarks: PackedVector2Array) -> bool:
	if not _build():
		return false
	last_error = ""
	if photo == null:
		for node in _original_skin:
			node.mesh = _original_skin[node]
		return true
	var replacements: Dictionary = {}
	var fitter := FaceFit.new()
	for node in _original_skin:
		var fitted: ArrayMesh = fitter.fit_mesh(_original_skin[node], photo, landmarks)
		if fitted == null:
			last_error = fitter.last_error
			return false
		replacements[node] = fitted
	if replacements.is_empty():
		last_error = "Imported story human has no accepted facial skin surface"
		return false
	# Apply together: a rejected fit keeps the prior visible identity intact.
	for node in replacements:
		node.mesh = replacements[node]
	return true

func present(soles_transform: Transform3D, tick: int, voice_level: float, gaze_world: Vector3, supported: bool = false, episode: int = 0) -> void:
	if not _build():
		return
	if not soles_transform.is_finite() or not gaze_world.is_finite() or tick < 0 or not is_finite(voice_level):
		visible = false
		return
	visible = true
	global_transform = soles_transform
	_last_tick = tick
	# Native tick drives presentation; repeated samples do not advance a timer.
	var seconds := float(tick) / 90.0
	for bone in [_head, _spine] + _arms:
		if bone >= 0:
			_skeleton.set_bone_pose(bone, _rest[bone])
	if _spine >= 0:
		var rest := _rest[_spine]
		rest.basis *= Basis(Vector3.RIGHT, sin(seconds * 1.6) * .003)
		_skeleton.set_bone_pose(_spine, rest)
	# Lower the imported A-pose into a relaxed working stance. This affects
	# bones only; native actor support/position remain the caller's facts.
	for side in _arms.size():
		var bone := _arms[side]
		if bone < 0:
			continue
		var pose := _skeleton.get_bone_global_pose(bone)
		var from := _skeleton.get_bone_global_rest(bone).basis.y.normalized()
		var toward := Vector3(.10 if side == 0 else -.10, -1, .05).normalized()
		pose.basis = Basis(Quaternion(from, toward)) * _skeleton.get_bone_global_rest(bone).basis
		_skeleton.set_bone_global_pose(bone, pose)
	if _head >= 0:
		var head_pose := _skeleton.get_bone_global_pose(_head)
		var local_target := _skeleton.to_local(gaze_world) - head_pose.origin
		if local_target.length_squared() > .0001:
			var yaw := clampf(atan2(local_target.x, local_target.z), -.6, .6)
			var pitch := clampf(-atan2(local_target.y, Vector2(local_target.x, local_target.z).length()), -.28, .28)
			head_pose.basis = Basis(Vector3.UP, yaw) * Basis(Vector3.RIGHT, pitch) * _skeleton.get_bone_global_rest(_head).basis
			_skeleton.set_bone_global_pose(_head, head_pose)
	var blink_phase := tick % 347
	var blink := maxf(0, 1.0 - absf(float(blink_phase - 9)) / 7.0) if blink_phase < 16 else 0.0
	var speech := clampf(voice_level, 0, 1)
	_set_expression("eyeBlinkLeft", blink)
	_set_expression("eyeBlinkRight", blink)
	_set_expression("jawOpen", speech * .45)
	_set_expression("browInnerUp", speech * .08)
	_gait.apply(soles_transform.origin, soles_transform.basis, supported, tick, episode)

func _set_expression(expression: String, amount: float) -> void:
	for node in _meshes:
		var shape := node.find_blend_shape_by_name(expression)
		if shape >= 0:
			node.set_blend_shape_value(shape, amount)
