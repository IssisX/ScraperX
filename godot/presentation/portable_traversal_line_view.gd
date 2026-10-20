extends Node3D

# Endpoints and fitting pose come from the native interpolation snapshot.
# The cyan marker is only a proposed support; it never represents a grip.
var _line := ImmediateMesh.new()
var _fitting: MeshInstance3D
var _target: MeshInstance3D
var _steel := StandardMaterial3D.new()
var _cable := StandardMaterial3D.new()
var _preview := StandardMaterial3D.new()
var _broken := StandardMaterial3D.new()
var _serial := -1
var _installed := false
var _gripping := false
var _generation := 0
var _loaded := false
var _carried: Node3D

func configure(camera: Camera3D) -> void:
	# A carried tool is presentation, not a new physical support. The stock
	# count below hides it as soon as the native fitting is installed.
	_carried = Node3D.new()
	_carried.name = "CarriedLineSpool"
	camera.add_child(_carried)
	_carried.position = Vector3(0.24, -0.25, -0.48)
	_carried.rotation = Vector3(0.12, -0.22, 0.35)
	for y in [-0.045, 0.045]:
		var disc := CylinderMesh.new()
		disc.top_radius = 0.105
		disc.bottom_radius = 0.105
		disc.height = 0.012
		disc.radial_segments = 16
		_add_carried_part(disc, _steel, Vector3(0, y, 0))
	var coil := CylinderMesh.new()
	coil.top_radius = 0.087
	coil.bottom_radius = 0.087
	coil.height = 0.08
	coil.radial_segments = 16
	_add_carried_part(coil, _cable, Vector3.ZERO)
	var handle := BoxMesh.new()
	handle.size = Vector3(0.22, 0.035, 0.045)
	_add_carried_part(handle, _steel, Vector3(0, -0.10, 0))
	_carried.visible = false

func _add_carried_part(mesh: Mesh, material: Material, at: Vector3) -> void:
	var part := MeshInstance3D.new()
	part.mesh = mesh
	part.material_override = material
	part.position = at
	part.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_carried.add_child(part)

func _ready() -> void:
	_steel.albedo_color = Color("777b79")
	_steel.metallic = 0.85
	_steel.roughness = 0.55
	_cable.albedo_color = Color("b69c5a")
	_cable.roughness = 0.9
	_preview.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_preview.albedo_color = Color("48c4d2")
	_broken.albedo_color = Color("a64c36")
	var cable := MeshInstance3D.new()
	cable.mesh = _line
	cable.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(cable)
	_fitting = MeshInstance3D.new()
	var bar := BoxMesh.new()
	bar.size = Vector3(0.28, 0.04, 0.05) # Matches the native collision box.
	_fitting.mesh = bar
	_fitting.material_override = _steel
	_fitting.visible = false
	add_child(_fitting)
	_target = MeshInstance3D.new()
	var marker := SphereMesh.new()
	marker.radius = 0.035
	marker.height = 0.07
	_target.mesh = marker
	_target.material_override = _preview
	_target.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_target.visible = false
	add_child(_target)

func reset_feedback() -> void:
	_serial = -1
	_generation = 0
	_installed = false
	_gripping = false
	_loaded = false

func update_state(state: Dictionary, audio: Node) -> void:
	var installed := bool(state.get("installed", false))
	var broken := bool(state.get("broken", false))
	var gripping := bool(state.get("gripping", false))
	var generation := int(state.get("generation", 0))
	var grip: Vector3 = state.get("grip_point", Vector3.ZERO)
	if is_instance_valid(_carried):
		_carried.visible = bool(state.get("equipped", false)) and not installed and int(state.get("available_spools", 0)) > 0
	_fitting.visible = installed
	_line.clear_surfaces()
	if installed:
		var rotation: Quaternion = state.get("fitting_rotation", Quaternion.IDENTITY)
		_fitting.global_transform = Transform3D(Basis(rotation), grip)
		_fitting.material_override = _broken if broken else _steel
		# A broken row carries no load and earns no connecting cable visual.
		var points: PackedVector3Array = state.get("path_points", PackedVector3Array())
		if not broken and bool(state.get("path_valid", false)) and points.size() > 1:
			_line.surface_begin(Mesh.PRIMITIVE_LINES, _cable)
			for i in range(1, points.size()):
				_line.surface_add_vertex(points[i - 1])
				_line.surface_add_vertex(points[i])
			_line.surface_end()
	_target.visible = bool(state.get("equipped", false)) and not installed and bool(state.get("target_valid", false))
	if _target.visible:
		_target.global_position = state.get("target_point", Vector3.ZERO)
	var serial := int(state.get("event_serial", 0))
	if audio != null and _serial >= 0 and serial != _serial:
		if int(state.get("failure", 0)) != 0:
			audio.ui_tap(true)
		elif gripping and (not _gripping or generation != _generation):
			audio.climb_regrip(grip, true)
		elif installed != _installed:
			audio.ui_tap()
	_serial = serial
	var tension := float(state.get("tension_n", 0.0))
	if not broken and gripping and tension >= 400.0 and not _loaded:
		_loaded = true
		if audio != null:
			audio.inspection_span_strain(grip, clampf(tension / 3000.0, 0.0, 1.0))
	elif not gripping or tension < 150.0:
		_loaded = false
	_installed = installed
	_gripping = gripping
	_generation = generation

func _exit_tree() -> void:
	if is_instance_valid(_carried):
		_carried.queue_free()
