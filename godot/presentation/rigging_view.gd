extends Node3D

# Prospective geometry is drawn separately from the installed native cables.
var _preview := ImmediateMesh.new()
var _marker: MeshInstance3D
var _material := StandardMaterial3D.new()
var _serial := -1
var _installed := 0
var _winch: AudioStreamPlayer3D

func _ready() -> void:
	_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_material.albedo_color = Color(0.2, 0.9, 1.0)
	var line := MeshInstance3D.new()
	line.mesh = _preview
	line.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(line)
	_marker = MeshInstance3D.new()
	var shape := SphereMesh.new()
	shape.radius = 0.045
	shape.height = 0.09
	_marker.mesh = shape
	_marker.material_override = _material
	_marker.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_marker.visible = false
	add_child(_marker)
	var sign := Label3D.new()
	sign.text = "BUILD YOUR WAY UP\nRIG: PLATFORM / GUIDE / TRAY\nADD THREE WEIGHTS / BOARD / PULL RELEASE"
	sign.font_size = 40
	sign.pixel_size = 0.007
	sign.position = Vector3(-120, 2.8, -39.7)
	sign.rotation.y = PI
	sign.modulate = Color(0.9, 0.78, 0.4)
	sign.visibility_range_end = 24.0
	add_child(sign)
	var winch_sign := Label3D.new()
	winch_sign.text = "LIFT THE LOOSE BEAM\nRIG: WINCH / BEAM EYE\nHOLD UP / DOWN — RELEASE TO BRAKE"
	winch_sign.font_size = 36
	winch_sign.pixel_size = 0.006
	winch_sign.position = Vector3(-108, 2.85, -37.4)
	winch_sign.rotation.y = PI
	winch_sign.modulate = Color(0.9, 0.78, 0.4)
	winch_sign.visibility_range_end = 22.0
	add_child(winch_sign)
	_winch = AudioStreamPlayer3D.new()
	_winch.bus = &"Effects"
	_winch.unit_size = 4.0
	_winch.max_distance = 30.0
	_winch.volume_db = -80.0
	add_child(_winch)

func stop_power_feedback() -> void:
	if is_instance_valid(_winch):
		_winch.stop()
		_winch.volume_db = -80.0

func update_state(state: Dictionary, audio: Node) -> void:
	_marker.visible = bool(state.get("equipped", false)) and int(state.get("target", 0)) != 0
	if _marker.visible:
		_marker.global_position = state["target_point"]
	_preview.clear_surfaces()
	var points: PackedVector3Array = state.get("preview", PackedVector3Array())
	# Reuse the existing grounded gearbox loop. Native commanded speed gates
	# motion sound; measured cable tension colours its load, never body motion.
	if bool(state.get("power_selected", false)) and absf(float(state.get("target_speed_mps", 0.0))) > 0.001 and audio._bank_ready:
		_winch.global_position = state.get("power_point", _winch.global_position)
		if _winch.stream == null:
			_winch.stream = audio._bank.clips[&"motor_loop"][0]
		var load_fraction := clampf(float(state.get("tension_n", 0.0)) / 6000.0, 0.0, 1.0)
		_winch.volume_db = -26.0 + 6.0 * load_fraction
		_winch.pitch_scale = 1.0 - 0.15 * load_fraction
		if not _winch.playing:
			_winch.play()
	else:
		stop_power_feedback()
	if points.size() > 1:
		_preview.surface_begin(Mesh.PRIMITIVE_LINES, _material)
		for i in range(1, points.size()):
			_preview.surface_add_vertex(points[i - 1])
			_preview.surface_add_vertex(points[i])
		_preview.surface_end()
	var serial := int(state.get("event_serial", 0))
	var installed := int(state.get("installed_count", 0))
	if _serial >= 0 and serial != _serial and bool(state.get("equipped", false)):
		if int(state.get("failure", 0)) != 0:
			audio.ui_tap(true)
		elif installed > _installed:
			audio.climb_regrip(state.get("target_point", global_position), true)
		else:
			audio.ui_tap()
	_serial = serial
	_installed = installed
