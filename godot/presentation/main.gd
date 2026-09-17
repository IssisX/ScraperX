extends Node3D

# Presentation and input only. Every consequential fact below is read from the
# native ScraperX simulation; nothing here decides pose, support, or traversal.

const EYE_OFFSET := Vector3(0.0, 0.55, 0.0)
const TOUCH_RADIUS := 92.0

const TRANSLATING_SUPPORT_ENTITY_ID := 3
const MANTLE_LEDGE_ENTITY_ID := 6

const TRAVERSAL_NONE := 0
const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3

const PHASE_SETTLE := 0
const PHASE_JUMPED := 1
const PHASE_APPROACH := 2
const PHASE_CLOSE := 3
const PHASE_MANTLE := 4

# Standoff point in front of the native mantle ledge, in the same metres the
# native world uses, and the direction the player then faces to close on it.
const CI_LEDGE_STANDOFF := Vector2(6.6, -6.0)
const CI_LEDGE_STANDOFF_RADIUS := 0.6
const CI_LEDGE_FACING := Vector2(1.0, 0.0)
# After the mantle lands, look back across the deck so the captured frame shows
# the climbed ledge, the moving supports and the tower slice together.
const CI_PROOF_VIEW := Vector2(-0.836, 0.549)
const CI_PROOF_PITCH := -0.16
const CI_HOLD_TICKS := 24

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
var _translating_support_mesh: MeshInstance3D
var _rotating_support_mesh: MeshInstance3D
var _moving_ledge_mesh: MeshInstance3D
var _ci_phase := PHASE_SETTLE
var _ci_facing := Vector2(1.0, 0.0)
var _ci_jump_tick := -1
var _ci_mantle_tick := -1
var _ci_support_entity_before_jump := 0
var _ci_support_velocity_before_jump := Vector3.ZERO
var _ci_jump_velocity_observed := Vector3.ZERO
var _ci_inherited_motion_observed := false
var _ci_mantle_support := 0
var _ci_mantle_ledge_point := Vector3.ZERO
var _ci_proof_printed := false

@onready var _camera: Camera3D = $Camera
@onready var _status: Label = $HUD/TopLeft/Status
@onready var _position_value: Label = $HUD/TopLeft/Position
@onready var _velocity_value: Label = $HUD/TopLeft/Velocity
@onready var _support_value: Label = $HUD/TopLeft/Support
@onready var _traversal_value: Label = $HUD/TopLeft/Traversal
@onready var _tick_value: Label = $HUD/TopRight/Tick
@onready var _touch_knob: ColorRect = $HUD/TouchMove/Knob
@onready var _action_button: Control = $HUD/TouchAction
@onready var _release_button: Control = $HUD/TouchRelease


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")

	if _ci_mode:
		_pitch = -0.18

	RenderingServer.set_default_clear_color(Color("071017"))
	_build_tower_slice()

	if not ClassDB.class_exists("ScraperXSimulation"):
		_fail_native("SCRAPERX_EXTENSION_LOAD_FAILED", 20)
		return

	_native = ClassDB.instantiate("ScraperXSimulation")
	if _native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return

	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim work_order=WO-003")
	_render_snapshot()


func _process(delta: float) -> void:
	if _native == null:
		return

	var position: Vector3 = _native.get_player_position()
	var desired := _read_desired_movement()
	var facing := Vector2(-sin(_yaw), -cos(_yaw))

	if _ci_mode:
		desired = _ci_movement_intent(position)
		facing = _ci_facing
		_yaw = atan2(-facing.x, -facing.y)
		_pitch = CI_PROOF_PITCH if _ci_mantle_tick >= 0 else _pitch

	var forward := Vector2(-sin(_yaw), -cos(_yaw))
	var right := Vector2(cos(_yaw), -sin(_yaw))
	var world_move := right * desired.x + forward * desired.y
	if not _native.set_move_input(world_move.x, world_move.y):
		_fail_native("SCRAPERX_MOVE_INPUT_REJECTED", 21)
		return
	_native.set_facing(facing.x, facing.y)

	var steps_advanced := int(_native.advance_frame(delta))
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_render_snapshot()

	if _ci_mode:
		_ci_observe(int(_native.get_tick_index()))

	var proof_ready := (
		_ci_inherited_motion_observed
		and _ci_mantle_tick >= 0
		and int(_native.get_tick_index()) >= _ci_mantle_tick + CI_HOLD_TICKS
	)

	if proof_ready and not _capture_path.is_empty() and not _capture_scheduled:
		_capture_scheduled = true
		RenderingServer.frame_post_draw.connect(_capture_frame, CONNECT_ONE_SHOT)
	elif proof_ready and _ci_mode and _capture_path.is_empty() and not _ci_proof_printed:
		_print_runtime_proof()
		get_tree().quit(0)


# --- CI sequence: every transition is driven by native authoritative state ---


func _ci_movement_intent(position: Vector3) -> Vector2:
	if _ci_phase == PHASE_APPROACH:
		var to_standoff := Vector2(
			CI_LEDGE_STANDOFF.x - position.x, CI_LEDGE_STANDOFF.y - position.z
		)
		if to_standoff.length() <= CI_LEDGE_STANDOFF_RADIUS:
			_ci_phase = PHASE_CLOSE
			_ci_facing = CI_LEDGE_FACING
			_print_ci_phase("CLOSE")
			return Vector2(0.0, 1.0)
		# Steer by looking where we walk: the world direction becomes facing and
		# the movement command stays a plain "forward", exactly as a thumb on the
		# left pad would produce.
		_ci_facing = to_standoff.normalized()
		return Vector2(0.0, 1.0)

	if _ci_phase != PHASE_CLOSE:
		return Vector2.ZERO

	# Facing the real ledge and walking into it: the native probe decides whether
	# a traversal exists, and it keeps deciding for as long as we stand here.
	_ci_facing = CI_LEDGE_FACING
	if bool(_native.is_ledge_available()) and int(_native.get_ledge_entity_id()) == MANTLE_LEDGE_ENTITY_ID:
		if not _native.request_traversal():
			_fail_native("SCRAPERX_TRAVERSAL_REQUEST_REJECTED", 24)
			return Vector2.ZERO
		_ci_phase = PHASE_MANTLE
		_print_ci_phase("MANTLE_REQUESTED")
		return Vector2.ZERO
	return Vector2(0.0, 1.0)


func _ci_observe(tick: int) -> void:
	var grounded := bool(_native.is_player_grounded())
	var support := int(_native.get_support_entity_id())
	var support_velocity: Vector3 = _native.get_support_point_linear_velocity()
	var player_velocity: Vector3 = _native.get_player_linear_velocity()

	match _ci_phase:
		PHASE_SETTLE:
			if tick >= 55 and grounded and support == TRANSLATING_SUPPORT_ENTITY_ID and absf(support_velocity.x) > 0.5:
				_ci_support_entity_before_jump = support
				_ci_support_velocity_before_jump = support_velocity
				if not _native.request_jump():
					_fail_native("SCRAPERX_JUMP_REQUEST_REJECTED", 23)
					return
				_ci_jump_tick = tick
				_ci_phase = PHASE_JUMPED
				_print_ci_phase("JUMPED")
		PHASE_JUMPED:
			if tick > _ci_jump_tick and not grounded and not _ci_inherited_motion_observed:
				_ci_jump_velocity_observed = player_velocity
				_ci_inherited_motion_observed = (
					player_velocity.x * _ci_support_velocity_before_jump.x > 0.0
					and absf(player_velocity.x) >= absf(_ci_support_velocity_before_jump.x) * 0.45
				)
			if _ci_inherited_motion_observed and grounded:
				_ci_phase = PHASE_APPROACH
				_print_ci_phase("APPROACH")
		PHASE_MANTLE:
			if int(_native.get_traversal_state()) == TRAVERSAL_MANTLING:
				_ci_mantle_ledge_point = _native.get_traversal_ledge_point()
			if _ci_mantle_tick < 0 and grounded and support == MANTLE_LEDGE_ENTITY_ID and int(_native.get_accepted_traversal_count()) >= 1:
				_ci_mantle_tick = tick
				_ci_mantle_support = support
				_ci_facing = CI_PROOF_VIEW
				_pitch = CI_PROOF_PITCH
				_print_ci_phase("MANTLE_LANDED")
		_:
			pass


# --- input ------------------------------------------------------------------


func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		var touch := event as InputEventScreenTouch
		if touch.pressed and _touch_hits(_action_button, touch.position):
			if _native != null:
				_native.request_traversal()
		elif touch.pressed and _touch_hits(_release_button, touch.position):
			if _native != null:
				_native.request_release()
		elif touch.pressed and touch.position.x < get_viewport().get_visible_rect().size.x * 0.5:
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
		if not key.pressed or key.echo:
			return
		if key.keycode == KEY_ESCAPE:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		elif key.keycode == KEY_SPACE and _native != null:
			_native.request_jump()
		elif key.keycode == KEY_E and _native != null:
			_native.request_traversal()
		elif key.keycode == KEY_Q and _native != null:
			_native.request_release()


func _touch_hits(control: Control, at: Vector2) -> bool:
	return control != null and Rect2(control.global_position, control.size).has_point(at)


func _read_desired_movement() -> Vector2:
	var keyboard := Vector2(
		float(int(Input.is_key_pressed(KEY_D)) - int(Input.is_key_pressed(KEY_A))),
		float(int(Input.is_key_pressed(KEY_W)) - int(Input.is_key_pressed(KEY_S)))
	)
	return (keyboard + _touch_move).limit_length(1.0)


func _apply_look_delta(delta: Vector2) -> void:
	_yaw -= delta.x * 0.003
	_pitch = clampf(_pitch - delta.y * 0.003, -1.15, 1.15)


# --- presentation mirror ----------------------------------------------------


func _render_snapshot() -> void:
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var support := int(_native.get_support_entity_id())
	var support_velocity: Vector3 = _native.get_support_point_linear_velocity()
	var traversal := int(_native.get_traversal_state())

	_camera.position = position + EYE_OFFSET
	_camera.rotation = Vector3(_pitch, _yaw, 0.0)
	_position_value.text = "POSITION  %7.2f  %6.2f  %7.2f m" % [position.x, position.y, position.z]
	_velocity_value.text = "VELOCITY  %7.2f  %6.2f  %7.2f m/s" % [velocity.x, velocity.y, velocity.z]
	_support_value.text = "SUPPORT   %s / E%04d / POINT V %5.2f %5.2f %5.2f" % [
		"GROUNDED" if grounded else "AIRBORNE",
		support,
		support_velocity.x,
		support_velocity.y,
		support_velocity.z,
	]
	_support_value.modulate = Color("62f5a8") if grounded else Color("ffc857")
	_traversal_value.text = "TRAVERSAL %s / E%04d / %3d%%   LEDGE %s" % [
		_traversal_name(traversal),
		int(_native.get_traversal_support_entity_id()),
		int(round(float(_native.get_traversal_progress()) * 100.0)),
		_ledge_affordance_text(),
	]
	_traversal_value.modulate = Color("62f5a8") if traversal != TRAVERSAL_NONE else Color("8fa3ad")
	_tick_value.text = "90 HZ NATIVE  /  TICK %08d  /  TRAVERSALS %d ACCEPTED %d REFUSED" % [
		int(_native.get_tick_index()),
		int(_native.get_accepted_traversal_count()),
		int(_native.get_rejected_traversal_count()),
	]

	if traversal == TRAVERSAL_HANGING:
		_status.text = "HANGING ON NATIVE LEDGE"
	elif traversal == TRAVERSAL_MANTLING:
		_status.text = "MANTLING REAL GEOMETRY"
	elif traversal == TRAVERSAL_VAULTING:
		_status.text = "VAULTING REAL GEOMETRY"
	elif grounded and support == TRANSLATING_SUPPORT_ENTITY_ID:
		_status.text = "NATIVE MOVING SUPPORT ONLINE"
	elif grounded:
		_status.text = "NATIVE BODY ONLINE"
	else:
		_status.text = "AIRBORNE / MOMENTUM PRESERVED"

	if _translating_support_mesh != null:
		_translating_support_mesh.position = _native.get_translating_support_position()
	if _rotating_support_mesh != null:
		_rotating_support_mesh.position = _native.get_rotating_support_position()
		_rotating_support_mesh.rotation = Vector3(0.0, float(_native.get_rotating_support_yaw_radians()), 0.0)
	if _moving_ledge_mesh != null:
		_moving_ledge_mesh.position = _native.get_moving_ledge_position()


func _traversal_name(traversal: int) -> String:
	match traversal:
		TRAVERSAL_HANGING:
			return "HANG   "
		TRAVERSAL_MANTLING:
			return "MANTLE "
		TRAVERSAL_VAULTING:
			return "VAULT  "
		_:
			return "NONE   "


func _ledge_affordance_text() -> String:
	if not bool(_native.is_ledge_available()):
		return "  --"
	return "E%04d +%4.2fm" % [int(_native.get_ledge_entity_id()), float(_native.get_ledge_rise_meters())]


# --- world -------------------------------------------------------------------


func _build_tower_slice() -> void:
	var steel := _material(Color("25343d"), 0.72, 0.28)
	var dark_steel := _material(Color("101b22"), 0.8, 0.32)
	var deck := _material(Color("34444d"), 0.65, 0.45)
	var hazard := _material(Color("e9a62f"), 0.35, 0.5)
	var signal_material := _material(Color("33d996"), 0.2, 0.34, Color("0b3d2d"))
	var climbable := _material(Color("3c5764"), 0.55, 0.42, Color("102c26"))
	# Set dressing is deliberately dimmer than native-authoritative surfaces:
	# it carries no collision in scraperx_sim and is never traversable.
	var backdrop := _material(Color("16222a"), 0.35, 0.68)

	# --- native-authoritative surfaces: sizes mirror the Jolt bodies exactly ---
	_add_box("AuthorityDeck", Vector3(32.0, 1.0, 32.0), Vector3(0.0, -0.5, 0.0), deck)
	_translating_support_mesh = _add_box("NativeTranslatingSupport", Vector3(5.5, 0.5, 5.5), Vector3(0.0, 0.25, 8.0), hazard)
	_rotating_support_mesh = _add_box("NativeRotatingSupport", Vector3(6.0, 0.5, 6.0), Vector3(-8.0, 0.25, 0.0), signal_material)
	_add_box("NativeVaultRail", Vector3(0.44, 0.95, 5.0), Vector3(5.0, 0.475, -6.0), hazard)
	_add_box("NativeMantleLedge", Vector3(4.0, 1.55, 4.0), Vector3(11.0, 0.775, -6.0), climbable)
	_add_box("NativeHangLedge", Vector3(5.0, 3.6, 5.0), Vector3(11.0, 1.8, 4.0), climbable)
	_moving_ledge_mesh = _add_box("NativeMovingLedge", Vector3(4.0, 3.6, 4.0), Vector3(9.0, 1.8, 12.5), climbable)
	_add_box("NativeBlockedLedge", Vector3(3.0, 1.55, 3.0), Vector3(-6.0, 0.775, -8.0), climbable)
	_add_box("NativeBlockedCanopy", Vector3(4.4, 0.3, 4.4), Vector3(-6.0, 2.7, -8.0), hazard)

	# --- non-authoritative set dressing ---------------------------------------
	for lane_x in [-8.0, 0.0, 8.0]:
		_add_box("DeckLane", Vector3(0.10, 0.025, 31.0), Vector3(lane_x, 0.015, 0.0), hazard)
	for lane_z in [-8.0, 0.0, 8.0]:
		_add_box("DeckCrossline", Vector3(31.0, 0.026, 0.10), Vector3(0.0, 0.016, lane_z), dark_steel)

	for x in [-14.0, 14.0]:
		for z in [-14.0, 14.0]:
			_add_box("TowerColumn", Vector3(0.9, 64.0, 0.9), Vector3(x, 31.5, z), backdrop)

	for level in range(4, 61, 6):
		for z in [-14.0, 14.0]:
			_add_box("CrossBeamX", Vector3(28.9, 0.55, 0.55), Vector3(0.0, float(level), z), backdrop)
		for x in [-14.0, 14.0]:
			_add_box("CrossBeamZ", Vector3(0.55, 0.55, 28.9), Vector3(x, float(level), 0.0), backdrop)

	_add_box("LockedMechanismHeader", Vector3(18.0, 1.0, 1.0), Vector3(0.0, 8.0, -13.0), backdrop)
	for marker_x in [-10.5, -3.5, 3.5, 10.5]:
		_add_box("SignalMarker", Vector3(0.22, 0.22, 0.22), Vector3(marker_x, 1.25, -13.5), signal_material)
	_add_box("SuspendedGuide", Vector3(0.45, 18.0, 0.45), Vector3(-7.5, 15.0, -11.5), backdrop)
	_add_box("Counterweight", Vector3(2.4, 5.0, 2.4), Vector3(-7.5, 12.0, -11.5), backdrop)


func _material(color: Color, metallic: float, roughness: float, emission: Color = Color.BLACK) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = 2.4
	return material


func _add_box(node_name: String, size: Vector3, at: Vector3, material: Material) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = mesh
	instance.position = at
	$TowerPresentation.add_child(instance)
	return instance


func _fail_native(reason: String, exit_code: int) -> void:
	_status.text = "NATIVE AUTHORITY FAILURE"
	_status.modulate = Color("ef5b5b")
	push_error(reason)
	get_tree().quit(exit_code)


func _print_ci_phase(label: String) -> void:
	var position: Vector3 = _native.get_player_position()
	print("SCRAPERX_CI_PHASE %s tick=%d position=(%.2f,%.2f,%.2f) grounded=%d support=%d ledge=%d/%d" % [
		label,
		_native.get_tick_index(),
		position.x,
		position.y,
		position.z,
		int(_native.is_player_grounded()),
		int(_native.get_support_entity_id()),
		int(_native.is_ledge_available()),
		int(_native.get_ledge_entity_id()),
	])


func _print_runtime_proof() -> void:
	_ci_proof_printed = true
	var position: Vector3 = _native.get_player_position()
	var translating_position: Vector3 = _native.get_translating_support_position()
	print("SCRAPERX_WO002_RUNTIME_PROOF ticks=%d position=(%.3f,%.3f,%.3f) support_before_jump=%d support_v=(%.3f,%.3f,%.3f) jump_v=(%.3f,%.3f,%.3f) inherited=%d translating_support=(%.3f,%.3f,%.3f) rotating_yaw=%.3f" % [
		_native.get_tick_index(),
		position.x,
		position.y,
		position.z,
		_ci_support_entity_before_jump,
		_ci_support_velocity_before_jump.x,
		_ci_support_velocity_before_jump.y,
		_ci_support_velocity_before_jump.z,
		_ci_jump_velocity_observed.x,
		_ci_jump_velocity_observed.y,
		_ci_jump_velocity_observed.z,
		int(_ci_inherited_motion_observed),
		translating_position.x,
		translating_position.y,
		translating_position.z,
		_native.get_rotating_support_yaw_radians(),
	])
	print("SCRAPERX_WO003_RUNTIME_PROOF ticks=%d mantle_support=%d position=(%.3f,%.3f,%.3f) grounded=%d accepted=%d refused=%d aborted=%d ledge_point=(%.3f,%.3f,%.3f)" % [
		_native.get_tick_index(),
		_ci_mantle_support,
		position.x,
		position.y,
		position.z,
		int(_native.is_player_grounded()),
		int(_native.get_accepted_traversal_count()),
		int(_native.get_rejected_traversal_count()),
		int(_native.get_aborted_traversal_count()),
		_ci_mantle_ledge_point.x,
		_ci_mantle_ledge_point.y,
		_ci_mantle_ledge_point.z,
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
