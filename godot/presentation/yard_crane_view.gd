extends Node3D

# Only native endpoints and cab grips are rendered. Native Kit owns the crane.
var _cables: MultiMeshInstance3D
var _knobs: MultiMeshInstance3D
var _state := {}

func _grip_target(hand: int) -> Vector3:
	var neutral: Vector3 = _state["operator_left_hand" if hand == 0 else "operator_right_hand"]
	var forward: Vector3 = _state.get("operator_forward", Vector3.RIGHT)
	forward = forward.normalized()
	var right := forward.cross(Vector3.UP).normalized()
	# Two-centimetre visual lever travel follows accepted native motor rates.
	# These are operator controls; they supply no forces or crane targets.
	var travel := Vector3.ZERO
	if hand == 0:
		travel = right * clampf(float(_state.get("slew_target_rad_s", 0.0)) / 0.05, -1.0, 1.0) * 0.014
		travel += forward * clampf(float(_state.get("trolley_target_mps", 0.0)) / 0.25, -1.0, 1.0) * 0.014
	else:
		# Hoist-up has a negative cable-length rate: pull this lever back.
		travel = forward * clampf(float(_state.get("hoist_target_mps", 0.0)) / 0.35, -1.0, 1.0) * 0.02
	if travel == Vector3.ZERO:
		return neutral
	var pivot := neutral - Vector3.UP * 0.10
	return pivot + (Vector3.UP * 0.10 + travel).normalized() * 0.10

func _group(radius: float, color: Color) -> MultiMeshInstance3D:
	var instance := MultiMeshInstance3D.new()
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = 1.0
	mesh.radial_segments = 8
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = 0.75
	mesh.material = material
	instance.multimesh = MultiMesh.new()
	instance.multimesh.transform_format = MultiMesh.TRANSFORM_3D
	instance.multimesh.mesh = mesh
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(instance)
	return instance

func render_state(state: Dictionary) -> void:
	_state = state
	visible = bool(state.get("available", false))
	if not visible:
		return
	if _cables == null:
		_cables = _group(0.018, Color("383b39"))
		_knobs = _group(0.035, Color("d6a143"))
	var lines: Array = state.get("lines", [])
	_cables.multimesh.instance_count = lines.size()
	for i in lines.size():
		var line: Dictionary = lines[i]
		var start: Vector3 = line["from"]
		var end: Vector3 = line["to"]
		var span := end - start
		var up := span.normalized() if span.length_squared() > 0.000001 else Vector3.UP
		var right := up.cross(Vector3.FORWARD if absf(up.z) < 0.9 else Vector3.RIGHT).normalized()
		_cables.multimesh.set_instance_transform(i, Transform3D(Basis(right, up * span.length(), right.cross(up)), (start + end) * 0.5))
	var has_grips := state.has("operator_left_hand") and state.has("operator_right_hand")
	_knobs.multimesh.instance_count = 2 if has_grips else 0
	if has_grips:
		for i in 2:
			var grip := _grip_target(i)
			var neutral: Vector3 = state["operator_left_hand" if i == 0 else "operator_right_hand"]
			var up := (grip - neutral + Vector3.UP * 0.10).normalized()
			var side := up.cross(Vector3.FORWARD).normalized()
			_knobs.multimesh.set_instance_transform(i, Transform3D(Basis(side, up * 0.065, side.cross(up)), grip))

func get_operator_hand_targets() -> Dictionary:
	if not bool(_state.get("available", false)) or not _state.has("operator_left_hand") or not _state.has("operator_right_hand"):
		return {}
	var forward: Vector3 = _state.get("operator_forward", Vector3.RIGHT)
	var frame := Transform3D(Basis(Vector3.UP.cross(forward).normalized(), Vector3.UP, forward), Vector3.ZERO)
	return {"left_hand_world": _grip_target(0), "right_hand_world": _grip_target(1),
		"operator_frame": _state.get("operator_frame", frame)}
