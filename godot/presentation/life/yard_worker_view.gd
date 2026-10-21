extends Node3D

# Native state owns every contact, activity and position. This is a garment rig.
const BodyRig := preload("res://presentation/first_person_body_rig.gd")
var _body: Node3D
var _hood: Node3D
var _rod: MeshInstance3D
var _arc: MeshInstance3D
var _sparks: Array[MeshInstance3D] = []
var _working := false
var _tip := Vector3.ZERO
var _episode := -1
var _tick := -1
var _phase := 0.0
var _footstep_serial := 0
var _last_half_step := 0

func _material(color: Color, metal := 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metal
	material.roughness = 0.74
	return material

func _box(parent: Node3D, label: String, size: Vector3, point: Vector3, material: Material) -> MeshInstance3D:
	var node := MeshInstance3D.new()
	node.name = label
	var mesh := BoxMesh.new()
	mesh.size = size
	node.mesh = mesh
	node.material_override = material
	parent.add_child(node)
	node.position = point
	return node

func _build() -> void:
	if _body != null:
		return
	_body = BodyRig.new()
	_body.name = "WorkerGarment"
	add_child(_body)
	_body.build(_material(Color("585645")), _material(Color("302c26")), _material(Color("171b1c")))
	for mesh in _body._meshes:
		if String(mesh.name) == "shadow":
			mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
			mesh.material_override = _material(Color("292d2b"))
	_hood = Node3D.new()
	_hood.name = "WeldingHood"
	add_child(_hood)
	var shell := _material(Color("3b4142"), 0.25)
	# A full enclosed shield conceals the source head rather than drawing a second face.
	_box(_hood, "Shield", Vector3(0.265, 0.32, 0.24), Vector3(0, 0.065, 0.045), shell)
	_box(_hood, "LensFrame", Vector3(0.196, 0.112, 0.024), Vector3(0, 0.097, 0.172), _material(Color("111718")))
	_box(_hood, "WeldingFilter", Vector3(0.154, 0.070, 0.009), Vector3(0, 0.100, 0.188), _material(Color("223e39"), 0.6))
	_rod = _box(self, "Electrode", Vector3(0.009, 0.009, 0.26), Vector3.ZERO, _material(Color("8d8c7d"), 0.7))
	var glow := _material(Color("bad9e5"))
	glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_arc = _box(self, "SeamArc", Vector3(0.018, 0.014, 0.018), Vector3.ZERO, glow)
	var spark_material := _material(Color("df9d53"))
	spark_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	for index in 10:
		_sparks.append(_box(self, "SeamSpark%d" % index, Vector3(0.003, 0.003, 0.018), Vector3.ZERO, spark_material))
	_effects(false)

func reset() -> void:
	_working = false
	_phase = 0.0
	_last_half_step = 0
	_footstep_serial = 0
	_episode = -1
	_tick = -1
	if _body != null:
		_body.reset_pose()
	_effects(false)
	visible = false

func _effects(active: bool) -> void:
	if _arc == null:
		return
	_arc.visible = active
	_rod.visible = active
	for spark in _sparks:
		spark.visible = active

func welding_active() -> bool:
	return _working

func weld_tip_world() -> Vector3:
	return _tip

func footstep_serial() -> int:
	return _footstep_serial

func present(state: Dictionary, delta: float) -> void:
	_build()
	var point: Vector3 = state.get("position", Vector3.ZERO)
	var velocity: Vector3 = state.get("velocity", Vector3.ZERO)
	var heading := float(state.get("heading_radians", 0.0))
	var episode := int(state.get("episode", 0))
	var tick := int(state.get("native_tick", 0))
	if episode != _episode or tick < _tick:
		reset()
	_episode = episode
	_tick = tick
	visible = bool(state.get("present", false)) and point.is_finite() and velocity.is_finite() and is_finite(heading)
	_working = false
	if not visible or not _body.ready_to_pose():
		_tip = Vector3.ZERO
		_effects(false)
		return
	var supported := bool(state.get("supported", false))
	var activity := int(state.get("activity", 0))
	var seam: Vector3 = state.get("work_point", point)
	var guide: Vector3 = state.get("guide_point", point)
	var look: Vector3 = state.get("look_target", point + Vector3.FORWARD)
	var speed := Vector2(velocity.x, velocity.z).length()
	var dt := clampf(delta, 0.0, 0.1) if is_finite(delta) else 0.0
	var frame := Basis(Vector3.UP, heading)
	var working_pose := supported and activity == 0 and speed < 0.3 and seam.is_finite()
	_body.pose_body(point + Vector3.UP * (0.10 - (0.075 if working_pose else 0.0)), frame,
		{"crouch_visual_01": 0.32 if working_pose else 0.0}, dt,
		Transform3D(Basis.IDENTITY, Vector3(INF, INF, INF)))
	if supported and speed > 0.12:
		_phase += minf(speed, 3.0) * dt * 7.0
		var half_step := int(_phase / PI)
		if half_step != _last_half_step:
			_footstep_serial += 1
		_last_half_step = half_step
	for side in 2:
		var hip: Vector3 = _body.hip_world(side)
		var stride := sin(_phase + side * PI) if supported and speed > 0.12 else 0.0
		var sole := point + frame * Vector3(-0.17 if side == 0 else 0.17, -0.9, stride * 0.20)
		sole.y += maxf(0.0, stride) * 0.065 if supported else 0.12
		var ankle: Vector3 = sole - _body.sole_offset(side, frame)
		var offset := ankle - hip
		var length := clampf(offset.length(), 0.04, BodyRig.THIGH + BodyRig.SHIN - 0.002)
		var direction := offset.normalized()
		var pole := -frame.z
		pole = (pole - direction * pole.dot(direction)).normalized()
		var along := (BodyRig.THIGH * BodyRig.THIGH - BodyRig.SHIN * BodyRig.SHIN + length * length) / (2.0 * length)
		var knee := hip + direction * along + pole * sqrt(maxf(0.0, BodyRig.THIGH * BodyRig.THIGH - along * along))
		_body.pose_leg(side, hip, knee, hip + direction * length, frame, pole)
	var wrists: Array = []
	if working_pose:
		wrists = [seam + frame * Vector3(-0.05, 0.09, 0.14), seam + frame * Vector3(0.15, 0.06, 0.14)]
	elif supported and activity == 2 and guide.is_finite():
		var shoulder: Vector3 = _body.joint_world(0, "sh")
		var reach := (guide - shoulder).normalized()
		wrists = [shoulder + reach * 0.57, _body.joint_world(1, "sh") + Vector3.DOWN * 0.52]
	var gaze := frame
	var head_origin: Vector3 = _body._model.global_transform * _body._global("head").origin
	var gaze_target := seam if working_pose else look
	if gaze_target.is_finite() and gaze_target.distance_squared_to(head_origin) > 0.001:
		var sight := (gaze_target - head_origin).normalized()
		gaze = Basis.looking_at(sight, Vector3.UP) if absf(sight.y) < 0.99 else frame
	_body.pose_shadow(Transform3D(gaze, point), wrists)
	var head: Transform3D = _body._global("head")
	_hood.global_transform = Transform3D(_body._model.global_basis.orthonormalized() * head.basis.orthonormalized(), _body._model.global_transform * head.origin)
	_tip = seam if seam.is_finite() else point
	# Never show an arc disconnected from the actual posed electrode hand.
	var hand: Vector3 = _body.joint_world(0, "wr")
	_working = working_pose and hand.distance_to(_tip) <= 0.38
	_effects(_working)
	if _working:
		_rod.global_position = (hand + _tip) * 0.5
		if hand.distance_squared_to(_tip) > 0.0001:
			_rod.look_at(_tip, Vector3.UP)
		_rod.scale.z = maxf(0.05, hand.distance_to(_tip)) / 0.26
		_arc.global_position = _tip
		_arc.scale = Vector3.ONE * (0.72 + 0.24 * sin(float(tick) * 2.7))
		for index in _sparks.size():
			var age := fposmod(float(tick) / 90.0 * 3.0 + index * 0.137, 1.0) * 0.16
			var angle := index * 2.399
			_sparks[index].global_position = _tip + Vector3(cos(angle) * age * 0.8, age * 0.6 - 4.9 * age * age, sin(angle) * age * 0.8)
