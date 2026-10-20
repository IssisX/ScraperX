extends Node3D

# Prospective geometry is drawn separately from the installed native cables.
var _preview := ImmediateMesh.new()
var _marker: MeshInstance3D
var _material := StandardMaterial3D.new()
var _serial := -1
var _installed := 0

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

func update_state(state: Dictionary, audio: Node) -> void:
	_marker.visible = bool(state.get("equipped", false)) and int(state.get("target", 0)) != 0
	if _marker.visible:
		_marker.global_position = state["target_point"]
	_preview.clear_surfaces()
	var points: PackedVector3Array = state.get("preview", PackedVector3Array())
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
