extends Node3D

const EYE_OFFSET := Vector3(0.0, 0.55, 0.0)
const TOUCH_RADIUS := 92.0

var _native: Object
var _capture_path := ""
var _capture_scheduled := false
var _ci_mode := false
var _yaw := 0.0
var _pitch := -0.06
var _move_touch_index := -1
var _look_touch_index := -1
var _move_touch_origin := Vector2.ZERO
var _touch_move := Vector2.ZERO

@onready var _camera: Camera3D = $Camera
@onready var _status: Label = $HUD/TopLeft/Status
@onready var _position_value: Label = $HUD/TopLeft/Position
@onready var _velocity_value: Label = $HUD/TopLeft/Velocity
@onready var _support_value: Label = $HUD/TopLeft/Support
@onready var _tick_value: Label = $HUD/TopRight/Tick
@onready var _touch_knob: ColorRect = $HUD/TouchMove/Knob


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")

	RenderingServer.set_default_clear_color(Color("071017"))
	_build_tower_slice()

	if not ClassDB.class_exists("ScraperXSimulation"):
		_fail_native("SCRAPERX_EXTENSION_LOAD_FAILED", 20)
		return

	_native = ClassDB.instantiate("ScraperXSimulation")
	if _native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return

	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim work_order=WO-001")
	_render_snapshot()


func _process(delta: float) -> void:
	if _native == null:
		return

	var desired := _read_desired_movement()
	if _ci_mode:
		desired = Vector2(0.0, 1.0)
	var forward := Vector2(-sin(_yaw), -cos(_yaw))
	var right := Vector2(cos(_yaw), -sin(_yaw))
	var world_move := right * desired.x + forward * desired.y
	if not _native.set_move_input(world_move.x, world_move.y):
		_fail_native("SCRAPERX_MOVE_INPUT_REJECTED", 21)
		return

	var steps_advanced := int(_native.advance_frame(delta))
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_render_snapshot()
	var tick := int(_native.get_tick_index())
	var position: Vector3 = _native.get_player_position()
	var proof_ready := tick >= 75 and bool(_native.is_player_grounded()) and position.z < 7.0

	if proof_ready and not _capture_path.is_empty() and not _capture_scheduled:
		_capture_scheduled = true
		RenderingServer.frame_post_draw.connect(_capture_frame, CONNECT_ONE_SHOT)
	elif proof_ready and _ci_mode and _capture_path.is_empty():
		_print_runtime_proof()
		get_tree().quit(0)


func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		var touch := event as InputEventScreenTouch
		if touch.pressed and touch.position.x < get_viewport().get_visible_rect().size.x * 0.5:
			if _move_touch_index == -1:
				_move_touch_index = touch.index
				_move_touch_origin = touch.position
		elif touch.pressed and _look_touch_index == -1:
			_look_touch_index = touch.index
		elif not touch.pressed and touch.index == _move_touch_index:
			_move_touch_index = -1
			_touch_move = Vector2.ZERO
			_touch_knob.position = Vector2(50.0, 50.0)
		elif not touch.pressed and touch.index == _look_touch_index:
			_look_touch_index = -1
	elif event is InputEventScreenDrag:
		var drag := event as InputEventScreenDrag
		if drag.index == _move_touch_index:
			var offset := (drag.position - _move_touch_origin).limit_length(TOUCH_RADIUS)
			_touch_move = Vector2(offset.x, -offset.y) / TOUCH_RADIUS
			_touch_knob.position = Vector2(50.0, 50.0) + offset
		elif drag.index == _look_touch_index:
			_apply_look_delta(drag.relative)
	elif event is InputEventMouseButton:
		var button := event as InputEventMouseButton
		if button.button_index == MOUSE_BUTTON_LEFT and button.pressed and not _ci_mode:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_apply_look_delta((event as InputEventMouseMotion).relative)
	elif event is InputEventKey:
		var key := event as InputEventKey
		if key.keycode == KEY_ESCAPE and key.pressed:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _read_desired_movement() -> Vector2:
	var keyboard := Vector2(
		float(int(Input.is_key_pressed(KEY_D)) - int(Input.is_key_pressed(KEY_A))),
		float(int(Input.is_key_pressed(KEY_W)) - int(Input.is_key_pressed(KEY_S)))
	)
	return (keyboard + _touch_move).limit_length(1.0)


func _apply_look_delta(delta: Vector2) -> void:
	_yaw -= delta.x * 0.003
	_pitch = clampf(_pitch - delta.y * 0.003, -1.15, 1.15)


func _render_snapshot() -> void:
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var support := int(_native.get_support_entity_id())

	_camera.position = position + EYE_OFFSET
	_camera.rotation = Vector3(_pitch, _yaw, 0.0)
	_position_value.text = "POSITION  %7.2f  %6.2f  %7.2f m" % [position.x, position.y, position.z]
	_velocity_value.text = "VELOCITY  %7.2f  %6.2f  %7.2f m/s" % [velocity.x, velocity.y, velocity.z]
	_support_value.text = "SUPPORT   %s  /  ENTITY %04d" % ["GROUNDED" if grounded else "AIRBORNE", support]
	_support_value.modulate = Color("62f5a8") if grounded else Color("ffc857")
	_tick_value.text = "90 HZ NATIVE  /  TICK %08d" % int(_native.get_tick_index())
	_status.text = "NATIVE BODY ONLINE" if grounded else "ACQUIRING STATIC SUPPORT"


func _build_tower_slice() -> void:
	var steel := _material(Color("25343d"), 0.72, 0.28)
	var dark_steel := _material(Color("101b22"), 0.8, 0.32)
	var deck := _material(Color("34444d"), 0.65, 0.45)
	var hazard := _material(Color("e9a62f"), 0.35, 0.5)
	var signal := _material(Color("33d996"), 0.2, 0.34, Color("0b3d2d"))

	_add_box("AuthorityDeck", Vector3(32.0, 1.0, 32.0), Vector3(0.0, -0.5, 0.0), deck)
	for lane_x in [-8.0, 0.0, 8.0]:
		_add_box("DeckLane", Vector3(0.10, 0.025, 31.0), Vector3(lane_x, 0.015, 0.0), hazard)
	for lane_z in [-8.0, 0.0, 8.0]:
		_add_box("DeckCrossline", Vector3(31.0, 0.026, 0.10), Vector3(0.0, 0.016, lane_z), dark_steel)

	for x in [-14.0, 14.0]:
		for z in [-14.0, 14.0]:
			_add_box("TowerColumn", Vector3(0.9, 64.0, 0.9), Vector3(x, 31.5, z), steel)

	for level in range(4, 61, 6):
		for z in [-14.0, 14.0]:
			_add_box("CrossBeamX", Vector3(28.9, 0.55, 0.55), Vector3(0.0, float(level), z), steel)
		for x in [-14.0, 14.0]:
			_add_box("CrossBeamZ", Vector3(0.55, 0.55, 28.9), Vector3(x, float(level), 0.0), steel)

	for side in [-1.0, 1.0]:
		_add_box("FreightRail", Vector3(0.28, 0.28, 29.0), Vector3(side * 7.5, 5.8, 0.0), hazard)
		_add_box("SuspendedGuide", Vector3(0.45, 18.0, 0.45), Vector3(side * 7.5, 15.0, -11.5), dark_steel)
		_add_box("Counterweight", Vector3(2.4, 5.0, 2.4), Vector3(side * 7.5, 12.0, -11.5), steel)

	_add_box("LockedMechanismHeader", Vector3(18.0, 1.0, 1.0), Vector3(0.0, 8.0, -13.0), hazard)
	for marker_x in [-10.5, -3.5, 3.5, 10.5]:
		_add_box("SignalMarker", Vector3(0.22, 0.22, 0.22), Vector3(marker_x, 1.25, -13.5), signal)


func _material(color: Color, metallic: float, roughness: float, emission := Color.BLACK) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = 2.4
	return material


func _add_box(node_name: String, size: Vector3, at: Vector3, material: Material) -> void:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = mesh
	instance.position = at
	$TowerPresentation.add_child(instance)


func _fail_native(reason: String, exit_code: int) -> void:
	_status.text = "NATIVE AUTHORITY FAILURE"
	_status.modulate = Color("ef5b5b")
	push_error(reason)
	get_tree().quit(exit_code)


func _print_runtime_proof() -> void:
	var position: Vector3 = _native.get_player_position()
	print("SCRAPERX_WO001_RUNTIME_PROOF ticks=%d position=(%.3f,%.3f,%.3f) grounded=%d support=%d" % [
		_native.get_tick_index(),
		position.x,
		position.y,
		position.z,
		int(_native.is_player_grounded()),
		_native.get_support_entity_id(),
	])


func _capture_frame() -> void:
	DirAccess.make_dir_recursive_absolute(_capture_path.get_base_dir())
	var image := get_viewport().get_texture().get_image()
	var error := image.save_png(_capture_path)
	if error != OK:
		push_error("SCRAPERX_SCREENSHOT_FAILED code=%d path=%s" % [error, _capture_path])
		get_tree().quit(22)
		return

	print("SCRAPERX_SCREENSHOT_SAVED=%s" % _capture_path)
	_print_runtime_proof()
	_capture_path = ""
	get_tree().quit(0)
