extends Node3D

# Native receipts include steering, suspension and spin on the render clock.
# CylinderMesh's Y axis is the wheel axle; no local motion is synthesized.
var _wheels: Array[MeshInstance3D] = []
var _cab: Node3D
var _steering: Node3D
var _lift: Node3D
var _tilt: Node3D
var _speed_needle: Node3D
var _left_grip: Node3D
var _right_grip: Node3D
var _tilt_grip: Node3D
var _right_on_tilt := false
var _lift_label: Label3D
var _tilt_label: Label3D
var _speed_label: Label3D


func render_state(state: Dictionary, chassis_frame: Transform3D = Transform3D.IDENTITY) -> void:
	visible = bool(state.get("available", false))
	if not visible:
		return
	# Anchors are authored by the native compartment; missing receipts never
	# create a second cab at an assumed position. Wheels remain world poses.
	if state.has("steering_mount") and state.has("lift_lever_mount") and state.has("tilt_lever_mount"):
		if not is_instance_valid(_cab):
			_build_cab()
		_cab.visible = true
		_cab.global_transform = chassis_frame
		_steering.position = state["steering_mount"]
		_steering.basis = Basis(Vector3.RIGHT, -0.68) * Basis(Vector3.UP,
			-float(state.get("steering_rad", 0.0)) / 0.9 * PI * 0.8)
		_lift.position = state["lift_lever_mount"]
		_tilt.position = state["tilt_lever_mount"]
		_lift_label.position = _lift.position + Vector3(0, .018, .045)
		_tilt_label.position = _tilt.position + Vector3(0, .018, .045)
		_speed_label.position = _steering.position + Vector3(0, -.19, .065)
		_lift.rotation.x = clampf(float(state.get("lift_target_mps", 0.0)) / 0.32, -1.0, 1.0) * -0.28
		_tilt.rotation.x = clampf(float(state.get("tilt_target_rad_s", 0.0)) / 0.1, -1.0, 1.0) * -0.28
		# A single right hand selects the hydraulic lever actually being used.
		_right_on_tilt = absf(float(state.get("tilt_target_rad_s", 0.0))) > 0.001 \
			and absf(float(state.get("lift_target_mps", 0.0))) < 0.001
		_speed_needle.position = state["steering_mount"] + Vector3(0.0, -0.15, 0.10)
		var dial: Node3D = _speed_needle.get_meta("dial")
		dial.transform = Transform3D(Basis(Vector3.RIGHT, -0.68), _speed_needle.position)
		_speed_needle.basis = Basis(Vector3.RIGHT, -0.68) * Basis(Vector3.UP,
			lerpf(-2.25, 2.25, clampf(absf(float(state.get("forward_speed_mps", 0.0))) / 3.5, 0.0, 1.0)))
	elif is_instance_valid(_cab):
		_cab.visible = false
	var wheels: Array = state.get("wheels", [])
	while _wheels.size() < wheels.size():
		var wheel := MeshInstance3D.new()
		var mesh := CylinderMesh.new()
		mesh.radial_segments = 16
		mesh.rings = 1
		wheel.mesh = mesh
		var rubber := StandardMaterial3D.new()
		rubber.albedo_color = Color("292b2c")
		rubber.roughness = 0.92
		wheel.material_override = rubber
		add_child(wheel)
		_wheels.append(wheel)
	for i in _wheels.size():
		var wheel := _wheels[i]
		wheel.visible = i < wheels.size()
		if not wheel.visible:
			continue
		var receipt: Dictionary = wheels[i]
		var mesh := wheel.mesh as CylinderMesh
		var radius := float(receipt["radius_m"])
		var width := float(receipt["width_m"])
		if mesh.top_radius != radius:
			mesh.top_radius = radius
			mesh.bottom_radius = radius
		if mesh.height != width:
			mesh.height = width
		wheel.global_transform = receipt["transform"]


# Grip points live on the rendered rim/balls, never on the camera. The
# caller supplies these world receipts to the arm presentation only.
func get_operator_hand_targets() -> Dictionary:
	if not visible or not is_instance_valid(_cab) or not _cab.visible:
		return {}
	return {"left_hand_world": _left_grip.global_position,
		"right_hand_world": (_tilt_grip if _right_on_tilt else _right_grip).global_position,
		"operator_frame": _cab.global_transform}


func _material(color: String, metal: float = 0.0) -> StandardMaterial3D:
	var mat := StandardMaterial3D.new()
	mat.albedo_color = Color(color)
	mat.metallic = metal
	mat.roughness = 0.43 if metal > 0.0 else 0.8
	return mat


func _instance(parent: Node3D, mesh: Mesh, mat: Material, frame: Transform3D = Transform3D.IDENTITY) -> MeshInstance3D:
	var part := MeshInstance3D.new()
	part.mesh = mesh
	part.material_override = mat
	part.transform = frame
	parent.add_child(part)
	return part


func _cylinder(radius: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 20
	mesh.rings = 1
	return mesh


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


# Combine components sharing a material into a single surface/draw.
func _append(surface: SurfaceTool, mesh: Mesh, frame: Transform3D) -> void:
	surface.append_from(mesh, 0, frame)


func _build_cab() -> void:
	_cab = Node3D.new()
	_cab.name = "NativeCabControls"
	add_child(_cab)
	var rubber := _material("252a2c")
	var steel := _material("8f999e", 0.75)
	var amber := _material("e6b449")
	_steering = Node3D.new()
	_steering.name = "SteeringWheel"
	_cab.add_child(_steering)
	var rim := TorusMesh.new()
	rim.inner_radius = 0.166
	rim.outer_radius = 0.194
	rim.rings = 32
	rim.ring_segments = 10
	_instance(_steering, rim, rubber)
	var spokes := SurfaceTool.new()
	spokes.begin(Mesh.PRIMITIVE_TRIANGLES)
	_append(spokes, _cylinder(0.046, 0.037), Transform3D.IDENTITY)
	for i in 3:
		var angle := TAU * float(i) / 3.0
		var basis := Basis(Vector3.UP, angle)
		_append(spokes, _box(Vector3(.025, .016, .14)),
			Transform3D(basis, basis * Vector3(0, 0, .10)))
	_instance(_steering, spokes.commit(), steel)
	_left_grip = Node3D.new()
	_left_grip.position = Vector3(.18, .025, 0)
	_steering.add_child(_left_grip)
	_lift = _build_lever("LiftLever", steel, rubber)
	_tilt = _build_lever("TiltLever", steel, rubber)
	_right_grip = _lift.get_node("Grip")
	_tilt_grip = _tilt.get_node("Grip")
	# The instrument face and ticks sit just ahead/below the rim. Native
	# speed drives its needle; no fabricated battery/load readout is shown.
	var dial := Node3D.new()
	dial.name = "TravelSpeedDial"
	_cab.add_child(dial)
	# Position follows the same anchor as the wheel in render_state.
	_speed_needle = Node3D.new()
	_speed_needle.name = "NativeTravelSpeed"
	_cab.add_child(_speed_needle)
	var face := _instance(dial, _cylinder(.066, .009), rubber)
	face.position.y = -.007
	var ticks := SurfaceTool.new()
	ticks.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in 9:
		var angle := lerpf(-2.25, 2.25, float(i) / 8.0)
		var basis := Basis(Vector3.UP, angle)
		_append(ticks, _box(Vector3(.004, .003, .012)),
			Transform3D(basis, basis * Vector3(0, .002, .053)))
	# Face/ticks retain their calibration independent of current needle yaw.
	_instance(dial, ticks.commit(), steel)
	_instance(_speed_needle, _box(Vector3(.004, .004, .048)), amber,
		Transform3D(Basis.IDENTITY, Vector3(0, .005, .021)))
	# Keep the fixed dial assembly mounted without a per-frame callback.
	_speed_needle.set_meta("dial", dial)
	_lift_label = _label("LIFT")
	_tilt_label = _label("TILT")
	_speed_label = _label("m/s   0 — 3.5")


func _build_lever(label: String, steel: Material, rubber: Material) -> Node3D:
	var lever := Node3D.new()
	lever.name = label
	_cab.add_child(lever)
	var stem := SurfaceTool.new()
	stem.begin(Mesh.PRIMITIVE_TRIANGLES)
	_append(stem, _cylinder(.009, .17), Transform3D(Basis.IDENTITY, Vector3(0, .085, 0)))
	_append(stem, _cylinder(.027, .016), Transform3D.IDENTITY)
	_instance(lever, stem.commit(), steel)
	var ball := SphereMesh.new()
	ball.radius = .029
	ball.height = .058
	ball.radial_segments = 16
	ball.rings = 8
	_instance(lever, ball, rubber, Transform3D(Basis.IDENTITY, Vector3(0, .185, 0)))
	var grip := Node3D.new()
	grip.name = "Grip"
	# Wrist just behind and below the ball; curled fingers surround it.
	grip.position = Vector3(0, .13, -.025)
	lever.add_child(grip)
	return lever


func _label(text: String) -> Label3D:
	var label := Label3D.new()
	label.text = text
	label.font_size = 32
	label.pixel_size = .00065
	label.modulate = Color("e1dfca")
	label.outline_size = 0
	label.basis = Basis(Vector3.LEFT, Vector3(0, .629, .777), Vector3(0, .777, -.629)).orthonormalized()
	_cab.add_child(label)
	return label
