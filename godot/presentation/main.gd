extends Node3D

const EYE_OFFSET := Vector3(0.0, 0.62, 0.0)
const TOUCH_RADIUS := 108.0
const FOLD6_INNER_ASPECT := 2160.0 / 1856.0
const FOLD_ASPECT_TOLERANCE := 0.08
const APPROACH_PROOF_METERS := 12.0

const TRAVERSAL_NONE := 0
const TRAVERSAL_VAULT := 1
const TRAVERSAL_MANTLE := 2
const TRAVERSAL_HANG := 3

var _native: Object
var _capture_path := ""
var _capture_scheduled := false
var _ci_mode := false
var _yaw := 0.0
var _pitch := 0.10
var _move_touch_index := -1
var _look_touch_index := -1
var _move_touch_origin := Vector2.ZERO
var _touch_move := Vector2.ZERO

var _translating_support_mesh: MeshInstance3D
var _rotating_support_mesh: MeshInstance3D
var _hopper_gate_mesh: MeshInstance3D
var _hopper_load_mesh: MeshInstance3D
var _impact_rocker_mesh: MeshInstance3D
var _jib_boom_mesh: MeshInstance3D
var _jib_hook_mesh: MeshInstance3D
var _jib_crate_mesh: MeshInstance3D
var _jib_cable_mesh: MeshInstance3D
var _needle_mesh: MeshInstance3D
var _cage_mesh: MeshInstance3D
var _cage_gate_mesh: MeshInstance3D
var _cage_lever_mesh: MeshInstance3D
var _cage_wheel_mesh: MeshInstance3D
var _cage_wheel_angle := 0.0
var _sump_water_mesh: MeshInstance3D
var _sump_valve_mesh: MeshInstance3D
var _sump_drain_mesh: MeshInstance3D
var _cloud_wisps: Array[MeshInstance3D] = []

var _pendant: Control
var _raise_held := false
var _lower_held := false
var _slew_left_held := false
var _slew_right_held := false

var _fold_layout_observed := false
var _ci_initial_player_position := Vector3.ZERO
var _ci_approach_observed := false
var _ci_machine_requested := false
var _ci_load_start := Vector3.ZERO
var _ci_load_moved_observed := false
var _ci_rocker_observed := false
var _ci_vault_observed := false
var _ci_mantle_observed := false
var _ci_phase := 0

@onready var _camera: Camera3D = $Camera
@onready var _status: Label = $HUD/TopLeft/Status
@onready var _position_value: Label = $HUD/TopLeft/Position
@onready var _velocity_value: Label = $HUD/TopLeft/Velocity
@onready var _support_value: Label = $HUD/TopLeft/Support
@onready var _machine_value: Label = $HUD/TopLeft/Machine
@onready var _tick_value: Label = $HUD/TopRight/Tick
@onready var _boundary_value: Label = $HUD/TopRight/Boundary
@onready var _touch_knob: ColorRect = $HUD/TouchMove/Knob
@onready var _action_button: Button = $HUD/ActionButton
@onready var _jump_button: Button = $HUD/JumpButton

func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")

	if _ci_mode:
		_pitch = 0.30

	RenderingServer.set_default_clear_color(Color("202326"))
	_build_exterior_world()
	_build_pendant_hud()
	_action_button.pressed.connect(_request_context_action)
	_jump_button.pressed.connect(_request_jump)
	get_viewport().size_changed.connect(_apply_viewport_composition)
	_apply_viewport_composition()

	if not ClassDB.class_exists("ScraperXSimulation"):
		_fail_native("SCRAPERX_EXTENSION_LOAD_FAILED", 20)
		return

	_native = ClassDB.instantiate("ScraperXSimulation")
	if _native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return

	_ci_initial_player_position = _native.get_player_position()
	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim checkpoint=WO-007-FIRST-PROCESS")
	_render_snapshot()

func _process(delta: float) -> void:
	if _native == null:
		return

	var world_move := Vector2.ZERO
	if _ci_mode:
		world_move = _ci_world_movement()
	else:
		var desired := _read_desired_movement()
		var forward := Vector2(-sin(_yaw), -cos(_yaw))
		var right := Vector2(cos(_yaw), -sin(_yaw))
		world_move = right * desired.x + forward * desired.y

	if not _native.set_move_input(world_move.x, world_move.y):
		_fail_native("SCRAPERX_MOVE_INPUT_REJECTED", 21)
		return

	_apply_jib_pendant_commands()

	var steps_advanced := int(_native.advance_frame(delta))
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_render_snapshot()
	_animate_weather(float(_native.get_simulation_time_seconds()))

	if _ci_mode:
		_update_ci_proof()

	var proof_ready := (
		_fold_layout_observed
		and _ci_approach_observed
		and _ci_machine_requested
		and _ci_load_moved_observed
		and _ci_rocker_observed
		and _ci_vault_observed
		and _ci_mantle_observed
	)

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
			_touch_knob.position = Vector2(64.0, 64.0)
		elif not touch.pressed and touch.index == _look_touch_index:
			_look_touch_index = -1
	elif event is InputEventScreenDrag:
		var drag := event as InputEventScreenDrag
		if drag.index == _move_touch_index:
			var offset := (drag.position - _move_touch_origin).limit_length(TOUCH_RADIUS)
			_touch_move = Vector2(offset.x, -offset.y) / TOUCH_RADIUS
			_touch_knob.position = Vector2(64.0, 64.0) + offset
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
		elif key.keycode == KEY_SPACE and key.pressed and not key.echo:
			_request_jump()
		elif key.keycode == KEY_E and key.pressed and not key.echo:
			_request_context_action()
		elif key.keycode == KEY_Q and key.pressed and not key.echo and _native != null:
			_native.request_drop_from_hang()
		elif key.keycode == KEY_R:
			_raise_held = key.pressed
		elif key.keycode == KEY_F:
			_lower_held = key.pressed
		elif key.keycode == KEY_Z:
			_slew_left_held = key.pressed
		elif key.keycode == KEY_C:
			_slew_right_held = key.pressed
		elif key.keycode == KEY_B and key.pressed and not key.echo and _native != null:
			if bool(_native.is_jib_station_occupied()):
				_native.set_jib_brake(not bool(_native.is_jib_brake_engaged()))

func _read_desired_movement() -> Vector2:
	var keyboard := Vector2(
		float(int(Input.is_key_pressed(KEY_D)) - int(Input.is_key_pressed(KEY_A))),
		float(int(Input.is_key_pressed(KEY_W)) - int(Input.is_key_pressed(KEY_S)))
	)
	return (keyboard + _touch_move).limit_length(1.0)

func _apply_look_delta(delta: Vector2) -> void:
	_yaw -= delta.x * 0.0027
	_pitch = clampf(_pitch - delta.y * 0.0027, -1.40, 1.24)

func _apply_viewport_composition() -> void:
	if _camera == null:
		return
	var size := get_viewport().get_visible_rect().size
	if size.y <= 0.0:
		return

	var aspect := size.x / size.y
	_fold_layout_observed = absf(aspect - FOLD6_INNER_ASPECT) <= FOLD_ASPECT_TOLERANCE
	if aspect < 1.35:
		_camera.fov = 96.0
	elif aspect < 1.65:
		_camera.fov = 90.0
	else:
		_camera.fov = 85.0
	_camera.near = 0.07
	_camera.far = 1600.0

func _ci_world_movement() -> Vector2:
	if _ci_phase == 0:
		if bool(_native.can_operate_hopper()):
			_ci_load_start = _native.get_hopper_load_position()
			if not bool(_native.request_hopper_release()):
				_fail_native("SCRAPERX_HOPPER_RELEASE_REJECTED_IN_RANGE", 24)
				return Vector2.ZERO
			_ci_machine_requested = true
			_ci_phase = 1
			return Vector2.ZERO
		return _movement_toward(_native.get_hopper_control_position())

	if _ci_phase == 1:
		if bool(_native.has_impact_rocker_been_struck()):
			_ci_rocker_observed = true
			_ci_phase = 2
		return Vector2.ZERO

	if _ci_phase == 2:
		if bool(_native.can_traverse()) and int(_native.get_traversal_candidate_mode()) == TRAVERSAL_VAULT:
			if not bool(_native.request_traversal()):
				_fail_native("SCRAPERX_CI_VAULT_REJECTED", 25)
				return Vector2.ZERO
			_ci_phase = 3
			return Vector2.ZERO
		return Vector2(0.0, -1.0)

	if _ci_phase == 3:
		if int(_native.get_traversal_mode()) == TRAVERSAL_VAULT:
			_ci_vault_observed = true
		if _ci_vault_observed and int(_native.get_traversal_mode()) == TRAVERSAL_NONE:
			_ci_phase = 4
		return Vector2.ZERO

	if _ci_phase == 4:
		if bool(_native.can_traverse()) and int(_native.get_traversal_candidate_mode()) == TRAVERSAL_MANTLE:
			if not bool(_native.request_traversal()):
				_fail_native("SCRAPERX_CI_MANTLE_REJECTED", 26)
				return Vector2.ZERO
			_ci_phase = 5
			return Vector2.ZERO
		return Vector2(0.0, -1.0)

	if _ci_phase == 5:
		if int(_native.get_traversal_mode()) == TRAVERSAL_MANTLE:
			_ci_mantle_observed = true
		return Vector2.ZERO

	return Vector2.ZERO

func _movement_toward(target: Vector3) -> Vector2:
	var player: Vector3 = _native.get_player_position()
	var delta := Vector2(target.x - player.x, target.z - player.z)
	if delta.length() <= 0.2:
		return Vector2.ZERO
	return delta.normalized()

func _update_ci_proof() -> void:
	var player: Vector3 = _native.get_player_position()
	var approach_distance := Vector2(
		player.x - _ci_initial_player_position.x,
		player.z - _ci_initial_player_position.z
	).length()
	if approach_distance >= APPROACH_PROOF_METERS:
		_ci_approach_observed = true

	if _ci_machine_requested:
		var load_now: Vector3 = _native.get_hopper_load_position()
		if load_now.distance_to(_ci_load_start) > 1.0 and bool(_native.has_hopper_load_moved()):
			_ci_load_moved_observed = true
		if bool(_native.has_impact_rocker_been_struck()):
			_ci_rocker_observed = true

func _request_jump() -> void:
	if _native == null:
		return
	_native.request_jump()

func _request_context_action() -> void:
	if _native == null:
		return
	var traversal_mode := int(_native.get_traversal_mode())
	if traversal_mode == TRAVERSAL_HANG:
		_native.request_drop_from_hang()
		return
	if bool(_native.can_traverse()):
		_native.request_traversal()
		return
	if bool(_native.is_jib_station_occupied()):
		_native.request_exit_jib_station()
		_raise_held = false
		_lower_held = false
		_slew_left_held = false
		_slew_right_held = false
		return
	if bool(_native.can_enter_jib_station()):
		_native.request_enter_jib_station()
		return
	if _native.has_method("can_operate_cage") and bool(_native.can_operate_cage()):
		_native.request_cage_lever()
		return
	if _native.has_method("can_operate_sump_valve") and bool(_native.can_operate_sump_valve()) and (
		not _native.has_method("is_sump_isolated") or not bool(_native.is_sump_isolated())
	):
		_native.request_sump_valve()
		return
	if _native.has_method("can_operate_sump_drain") and bool(_native.can_operate_sump_drain()) and (
		not _native.has_method("is_sump_drain_open") or not bool(_native.is_sump_drain_open())
	):
		_native.request_sump_drain()
		return
	if _native.has_method("can_operate_sump_valve") and bool(_native.can_operate_sump_valve()):
		_native.request_sump_valve()
		return
	if _native.has_method("can_operate_sump_drain") and bool(_native.can_operate_sump_drain()):
		_native.request_sump_drain()
		return
	if bool(_native.can_operate_hopper()):
		_native.request_hopper_release()
		return
	if not bool(_native.is_player_grounded()) and _native.has_method("request_parachute"):
		_native.request_parachute()

func _traversal_name(mode: int) -> String:
	match mode:
		TRAVERSAL_VAULT:
			return "VAULT"
		TRAVERSAL_MANTLE:
			return "MANTLE"
		TRAVERSAL_HANG:
			return "HANG"
		_:
			return "NONE"

func _render_snapshot() -> void:
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var support := int(_native.get_support_entity_id())
	var support_velocity: Vector3 = _native.get_support_point_linear_velocity()
	var traversal_mode := int(_native.get_traversal_mode())
	var traversal_candidate := int(_native.get_traversal_candidate_mode())
	var traversal_available := bool(_native.can_traverse())
	var hopper_available := bool(_native.can_operate_hopper())
	var hopper_started := bool(_native.has_hopper_release_started())
	var hopper_open := bool(_native.is_hopper_gate_open())
	var hopper_moved := bool(_native.has_hopper_load_moved())
	var load_velocity: Vector3 = _native.get_hopper_load_linear_velocity()
	var rocker_struck := bool(_native.has_impact_rocker_been_struck())
	var rocker_omega: Vector3 = _native.get_impact_rocker_angular_velocity()
	var jib_occupied := bool(_native.is_jib_station_occupied())
	var jib_enter := bool(_native.can_enter_jib_station())
	var jib_brake := bool(_native.is_jib_brake_engaged())
	var jib_stall := bool(_native.is_jib_stalled())
	var jib_limit := bool(_native.is_jib_at_hoist_limit())
	var winch := float(_native.get_jib_winch_length_meters())
	var crate_mass := float(_native.get_jib_crate_mass_kg())
	var needle_seated := _native.has_method("is_needle_seated") and bool(_native.is_needle_seated())
	var hook_load := int(_native.get_jib_hook_load()) if _native.has_method("get_jib_hook_load") else 1
	var cage_lever := _native.has_method("can_operate_cage") and bool(_native.can_operate_cage())
	var cage_stall := _native.has_method("is_cage_stalled") and bool(_native.is_cage_stalled())
	var cage_brake := _native.has_method("is_cage_brake_engaged") and bool(_native.is_cage_brake_engaged())
	var cage_limit := _native.has_method("is_cage_at_limit") and bool(_native.is_cage_at_limit())
	var cage_command := float(_native.get_cage_command()) if _native.has_method("get_cage_command") else 0.0
	var sump_valve := _native.has_method("can_operate_sump_valve") and bool(_native.can_operate_sump_valve())
	var sump_drain := _native.has_method("can_operate_sump_drain") and bool(_native.can_operate_sump_drain())
	var sump_isolated := _native.has_method("is_sump_isolated") and bool(_native.is_sump_isolated())
	var sump_drain_open := _native.has_method("is_sump_drain_open") and bool(_native.is_sump_drain_open())
	var sump_safe := _native.has_method("is_sump_grate_safe") and bool(_native.is_sump_grate_safe())
	var sump_inventory := float(_native.get_sump_inventory()) if _native.has_method("get_sump_inventory") else 1.0

	_camera.position = position + EYE_OFFSET
	_camera.rotation = Vector3(_pitch, _yaw, 0.0)
	_position_value.text = "POS  %7.2f  %6.2f  %7.2f m" % [position.x, position.y, position.z]
	_velocity_value.text = "VEL  %7.2f  %6.2f  %7.2f m/s" % [velocity.x, velocity.y, velocity.z]
	_support_value.text = "SUPPORT  %s  E%02d  /  TRAVERSE %s" % [
		"GROUNDED" if grounded else "AIRBORNE",
		support,
		_traversal_name(traversal_mode),
	]
	_machine_value.text = "SUMP  %s  CAGE %s  NEEDLE %s" % [
		"SAFE" if sump_safe else ("ISO" if sump_isolated else ("DRAIN" if sump_drain_open else "WET")),
		"STALL" if cage_stall else ("LIMIT" if cage_limit else ("HOLD" if cage_brake else "LIVE")),
		"SEATED" if needle_seated else "FREE",
	]
	_tick_value.text = "90 HZ NATIVE  /  %08d" % int(_native.get_tick_index())
	_boundary_value.text = "KX-SUMP / GRATE HAZARD / LOCAL VALVE"

	var context_visible := false
	if traversal_mode == TRAVERSAL_HANG:
		_action_button.text = "DROP"
		context_visible = true
	elif traversal_available:
		_action_button.text = _traversal_name(traversal_candidate)
		context_visible = true
	elif jib_occupied:
		_action_button.text = "EXIT JIB"
		context_visible = true
	elif jib_enter:
		_action_button.text = "ENTER JIB"
		context_visible = true
	elif cage_lever:
		_action_button.text = "PULL LEVER"
		context_visible = true
	elif sump_valve and not sump_isolated:
		_action_button.text = "ISOLATE"
		context_visible = true
	elif sump_drain and not sump_drain_open:
		_action_button.text = "OPEN DRAIN"
		context_visible = true
	elif sump_valve:
		_action_button.text = "OPEN LINE"
		context_visible = true
	elif hopper_available and not hopper_started:
		_action_button.text = "RELEASE HOPPER"
		context_visible = true
	elif not grounded and _native.has_method("is_parachute_allowed") and bool(_native.is_parachute_allowed()):
		_action_button.text = "CHUTE"
		context_visible = true
	_action_button.visible = context_visible
	_action_button.disabled = not context_visible
	_jump_button.text = "CLIMB" if traversal_mode == TRAVERSAL_HANG else "JUMP"

	if traversal_mode == TRAVERSAL_HANG:
		_status.text = "LEDGE HELD / JUMP TO CLIMB / ACTION TO DROP"
	elif traversal_available:
		_status.text = "%s AVAILABLE / REAL GEOMETRY IN REACH" % _traversal_name(traversal_candidate)
	elif jib_occupied:
		if jib_stall:
			_status.text = "PENDANT LIVE / ACTUATOR STALLED / BRAKE OR SWL"
		elif jib_brake:
			_status.text = "PENDANT LIVE / BRAKE HOLDING / RELEASE TO WORK"
		else:
			_status.text = "PENDANT LIVE / RAISE LOWER SLEW / BOUNDED WORK"
	elif jib_enter:
		_status.text = "KX-JIB PENDANT IN RANGE / ACTION ENTERS STATION"
	elif cage_stall:
		_status.text = "CAGE INTERLOCK / NEEDLE MUST SEAT BEFORE RAISE"
	elif cage_lever and absf(cage_command) > 0.05:
		_status.text = "CAGE TRAVEL / PULL LEVER TO BRAKE"
	elif cage_lever:
		_status.text = "KX-CAGE LEVER IN REACH / PULL TO WORK THE DRUM"
	elif sump_safe:
		_status.text = "KX-GRATE DRY / ISOLATED VOLUME / WALK THE BARS"
	elif sump_isolated and sump_drain_open:
		_status.text = "SUMP DUMPING / INVENTORY FALLING / GRATE STILL A HOLE"
	elif sump_valve and not sump_isolated:
		_status.text = "ISOLATION WHEEL IN REACH / CLOSE THE FILL LINE"
	elif sump_drain:
		_status.text = "DRAIN COCK IN REACH / OPENS ONLY THE DUMP"
	elif needle_seated:
		_status.text = "KX-NEEDLE SEATED / SPAN IS SUPPORT / WALK THE BAY"
	elif hopper_available:
		_status.text = "LOADING BAY CONTROL IN RANGE"
	elif rocker_struck:
		_status.text = "LOAD IMPACT PROPAGATED / ROCKER NOW PHYSICAL"
	elif hopper_moved:
		_status.text = "HOPPER DISCHARGED / FOLLOW THE MASS"
	elif grounded:
		_status.text = "EXTERIOR GRADE / MACHINE-TOWER AHEAD"
	elif not grounded:
		if _native.has_method("is_parachute_deployed") and bool(_native.is_parachute_deployed()):
			_status.text = "CHUTE OPEN / SINKING / STEER WITH MOVE"
		elif _native.has_method("get_fall_severity") and int(_native.get_fall_severity()) >= 2:
			_status.text = "LONG FALL / CHUTE IF YOU HAVE CLEARANCE"
		else:
			_status.text = "AIRBORNE / MOMENTUM PRESERVED"

	if _translating_support_mesh != null:
		_translating_support_mesh.position = _native.get_translating_support_position()
	if _rotating_support_mesh != null:
		_rotating_support_mesh.position = _native.get_rotating_support_position()
		_rotating_support_mesh.rotation = Vector3(0.0, float(_native.get_rotating_support_yaw_radians()), 0.0)
	if _hopper_gate_mesh != null:
		_hopper_gate_mesh.position = _native.get_hopper_gate_position()
	if _hopper_load_mesh != null:
		_hopper_load_mesh.position = _native.get_hopper_load_position()
	if _impact_rocker_mesh != null:
		_impact_rocker_mesh.position = _native.get_impact_rocker_position()
		_impact_rocker_mesh.rotation = Vector3(float(_native.get_impact_rocker_angle_radians()), 0.0, 0.0)
	_sync_jib_meshes()
	_sync_needle_mesh()
	_sync_cage_meshes()
	_sync_sump_meshes(sump_isolated, sump_drain_open, sump_inventory)
	if _pendant != null:
		_pendant.visible = jib_occupied

func _build_exterior_world() -> void:
	var wet_asphalt := _material(Color("202223"), 0.02, 0.34)
	var poured_concrete := _material(Color("62625d"), 0.0, 0.88)
	var dark_concrete := _material(Color("343633"), 0.0, 0.93)
	var mill_scale := _material(Color("303438"), 0.68, 0.58)
	var oxidized_steel := _material(Color("59433a"), 0.52, 0.74)
	var galvanized := _material(Color("777b78"), 0.58, 0.52)
	var tar := _material(Color("151616"), 0.0, 0.20)
	var faded_yellow := _material(Color("9d7c31"), 0.18, 0.70)
	var chipped_orange := _material(Color("8c4f2d"), 0.22, 0.72)
	var ballast := _material(Color("74452f"), 0.48, 0.66)
	var weathered_timber := _material(Color("604f3c"), 0.0, 0.82)
	var cloud := _transparent_material(Color(0.50, 0.52, 0.53, 0.36), 1.0)
	var plume := _transparent_material(Color(0.58, 0.58, 0.56, 0.20), 1.0)

	_add_box("WetAsphaltGrade", Vector3(140.0, 0.30, 180.0), Vector3(0.0, -0.16, 18.0), wet_asphalt)
	_add_box("ConcreteApron", Vector3(48.0, 0.08, 34.0), Vector3(0.0, 0.02, 31.0), poured_concrete)
	for x in [-18.0, -6.0, 6.0, 18.0]:
		_add_box("DrainStripe", Vector3(0.14, 0.035, 58.0), Vector3(x, 0.05, 42.0), tar)

	# Monumental lower mass: piers, then stacked machine bays that ARE the tower.
	_add_box("TowerBasePierL", Vector3(20.0, 30.0, 6.0), Vector3(-17.0, 15.0, 10.0), dark_concrete)
	_add_box("TowerBasePierR", Vector3(20.0, 30.0, 6.0), Vector3(17.0, 15.0, 10.0), dark_concrete)
	_add_box("LoadingBayHeader", Vector3(14.0, 3.0, 6.0), Vector3(0.0, 27.5, 10.0), oxidized_steel)
	_build_stacked_machine_tower(mill_scale, oxidized_steel, dark_concrete, weathered_timber, galvanized, faded_yellow)

	# Native hopper and downstream impact rocker.
	_add_box("HopperFrameL", Vector3(0.28, 1.6, 3.6), Vector3(5.05, 5.85, 33.0), mill_scale)
	_add_box("HopperFrameR", Vector3(0.28, 1.6, 3.6), Vector3(8.95, 5.85, 33.0), mill_scale)
	_add_box("HopperCrown", Vector3(4.4, 0.45, 4.3), Vector3(7.0, 7.15, 33.0), oxidized_steel)
	var chute := _add_box("ReceivingChute", Vector3(4.0, 0.32, 12.0), Vector3(7.0, 2.65, 28.8), galvanized)
	chute.rotation = Vector3(-0.20, 0.0, 0.0)
	_hopper_gate_mesh = _add_box("NativeHopperGate", Vector3(3.6, 0.30, 3.6), Vector3(7.0, 4.8, 33.0), faded_yellow)
	_hopper_load_mesh = _add_sphere("NativeHopperLoad", 0.65, Vector3(7.0, 5.60, 33.0), ballast)
	_impact_rocker_mesh = _add_box("NativeImpactRocker", Vector3(4.0, 3.2, 0.44), Vector3(7.0, 1.65, 21.55), oxidized_steel)
	_add_box("RockerBearingL", Vector3(0.45, 0.45, 1.0), Vector3(4.7, 1.65, 21.55), mill_scale)
	_add_box("RockerBearingR", Vector3(0.45, 0.45, 1.0), Vector3(9.3, 1.65, 21.55), mill_scale)

	var control_position := Vector3(2.5, 0.0, 40.0)
	_add_box("HopperControlPedestal", Vector3(1.1, 1.7, 1.0), control_position + Vector3(0.0, 0.85, 0.0), mill_scale)
	_add_box("HopperControlFace", Vector3(0.82, 0.42, 0.08), control_position + Vector3(0.0, 1.35, -0.54), chipped_orange)

	# Actual native traversal geometry is mirrored at the exact authored coordinates.
	_add_box("NativeVaultBarrier", Vector3(4.0, 0.90, 0.70), Vector3(2.5, 0.45, 35.5), faded_yellow)
	_add_box("NativeMantleBlock", Vector3(4.4, 2.30, 2.80), Vector3(2.5, 1.15, 31.5), dark_concrete)
	_add_box("NativeHangCatwalk", Vector3(5.6, 0.50, 3.20), Vector3(2.5, 4.10, 25.4), galvanized)
	_add_box("CatwalkRailL", Vector3(0.12, 1.3, 3.2), Vector3(-0.25, 4.85, 25.4), faded_yellow)
	_add_box("CatwalkRailR", Vector3(0.12, 1.3, 3.2), Vector3(5.25, 4.85, 25.4), faded_yellow)

	_translating_support_mesh = _add_box("NativeTransferDeck", Vector3(5.5, 0.5, 5.5), Vector3(0.0, 0.25, 8.0), faded_yellow)
	_rotating_support_mesh = _add_box("NativeRotaryTable", Vector3(6.0, 0.5, 6.0), Vector3(-8.0, 0.25, 0.0), mill_scale)

	var jib_mast := Vector3(-12.0, 0.0, 48.0)
	_add_box("KxJibMast", Vector3(0.76, 9.1, 0.76), jib_mast + Vector3(0.0, 4.55, 0.0), oxidized_steel)
	_add_box("KxJibBase", Vector3(2.2, 0.35, 2.2), jib_mast + Vector3(0.0, 0.18, 0.0), mill_scale)
	_jib_boom_mesh = _add_box("KxJibBoom", Vector3(8.0, 0.36, 0.36), Vector3(-8.0, 9.0, 48.0), mill_scale)
	_jib_hook_mesh = _add_sphere("KxJibHook", 0.16, Vector3(-4.0, 0.85, 48.0), faded_yellow)
	_jib_crate_mesh = _add_box("KxCrate", Vector3(1.40, 0.32, 1.40), Vector3(-4.0, 0.16, 48.0), chipped_orange)
	_add_box("KxCrateMark", Vector3(1.42, 0.04, 1.42), Vector3(-4.0, 0.30, 48.0), faded_yellow)
	_jib_cable_mesh = _add_cylinder("KxJibCable", 0.035, 1.0, Vector3(-4.0, 5.0, 48.0), galvanized)
	var pendant := Vector3(-7.0, 0.0, 49.0)
	_add_box("KxPendantPedestal", Vector3(0.7, 1.35, 0.55), pendant + Vector3(0.0, 0.68, 0.0), mill_scale)
	_add_box("KxPendantFace", Vector3(0.52, 0.38, 0.08), pendant + Vector3(0.0, 1.18, -0.30), chipped_orange)

	_build_needle_bay(mill_scale, oxidized_steel, weathered_timber, galvanized, faded_yellow, chipped_orange)
	_build_cage_house(mill_scale, oxidized_steel, galvanized, faded_yellow, chipped_orange, weathered_timber)
	_build_sump_bay(mill_scale, oxidized_steel, dark_concrete, weathered_timber, galvanized, faded_yellow, chipped_orange)

	# Readable idle machinery integrated into the lower bays — not a dead lift car in a blank shaft.
	_add_machine_wheel(Vector3(-17.0, 43.0, 14.0), 6.5, 1.4, mill_scale, oxidized_steel)
	_add_machine_wheel(Vector3(-18.0, 22.0, 14.0), 4.8, 1.2, oxidized_steel, mill_scale)
	_add_hoist_drum(Vector3(-14.0, 28.0, 12.0), 3.4, 7.5, mill_scale, galvanized)

	var crane_mast := _add_box("CraneMast", Vector3(4.0, 24.0, 4.0), Vector3(18.0, 44.0, 13.0), oxidized_steel)
	var crane_boom := _add_box("CraneBoom", Vector3(2.3, 2.0, 38.0), Vector3(18.0, 57.0, 29.0), mill_scale)
	crane_boom.rotation.x = -0.34
	_add_box("CraneBoomSpine", Vector3(0.7, 0.7, 39.0), crane_boom.position + Vector3(-1.5, 0.8, 0.0), galvanized).rotation.x = -0.34
	crane_mast.rotation.y = 0.0

	for pipe_x in [-13.0, -10.5]:
		_add_box("SteamLine", Vector3(0.55, 0.55, 54.0), Vector3(pipe_x, 4.0, 34.0), galvanized)
		_add_box("SteamRiser", Vector3(0.55, 26.0, 0.55), Vector3(pipe_x, 16.5, 8.0), oxidized_steel)
	_add_box("Stack", Vector3(5.0, 210.0, 5.0), Vector3(23.0, 105.0, 4.0), oxidized_steel)
	_add_box("StackBrace", Vector3(13.0, 1.0, 1.0), Vector3(17.0, 84.0, 4.0), mill_scale)

	_add_floodlight(Vector3(-8.0, 8.5, 31.0), Vector3(0.0, -0.35, -1.0))
	_add_floodlight(Vector3(9.0, 9.0, 30.0), Vector3(-0.25, -0.35, -1.0))
	_add_floodlight(Vector3(0.0, 12.0, 14.5), Vector3(0.0, -0.28, 1.0))
	_add_floodlight(Vector3(-18.0, 31.0, 15.0), Vector3(0.1, -0.2, 1.0))

	for cloud_spec in [
		[Vector3(-24.0, 350.0, 4.0), Vector3(38.0, 9.0, 28.0)],
		[Vector3(24.0, 382.0, -8.0), Vector3(42.0, 11.0, 31.0)],
		[Vector3(-5.0, 420.0, 13.0), Vector3(52.0, 12.0, 35.0)],
		[Vector3(20.0, 465.0, 0.0), Vector3(48.0, 11.0, 38.0)],
		[Vector3(-20.0, 510.0, -4.0), Vector3(56.0, 14.0, 42.0)],
	]:
		_cloud_wisps.append(_add_cloud_wisp(cloud_spec[0], cloud_spec[1], cloud))

	for index in range(11):
		var plume_position := Vector3(23.0 + sin(float(index) * 0.72) * 3.0, 216.0 + float(index) * 18.0, 4.0 + cos(float(index) * 0.55) * 2.5)
		_cloud_wisps.append(_add_cloud_wisp(plume_position, Vector3(5.0 + index * 0.7, 7.0, 5.0 + index * 0.5), plume))

func _apply_jib_pendant_commands() -> void:
	if _native == null or not bool(_native.is_jib_station_occupied()):
		return
	var hoist := 0.0
	if _raise_held:
		hoist += 1.0
	if _lower_held:
		hoist -= 1.0
	var slew := 0.0
	if _slew_left_held:
		slew -= 1.0
	if _slew_right_held:
		slew += 1.0
	_native.set_jib_hoist_input(hoist)
	_native.set_jib_slew_input(slew)

func _hold_button(name: String, label: String, on_down: Callable, on_up: Callable) -> Button:
	var button := Button.new()
	button.name = name
	button.text = label
	button.custom_minimum_size = Vector2(118, 56)
	button.button_down.connect(on_down)
	button.button_up.connect(on_up)
	return button

func _build_pendant_hud() -> void:
	_pendant = Control.new()
	_pendant.name = "JibPendant"
	_pendant.visible = false
	_pendant.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	_pendant.offset_left = 250.0
	_pendant.offset_right = -300.0
	_pendant.offset_top = -228.0
	_pendant.offset_bottom = -16.0
	$HUD.add_child(_pendant)

	var grid := GridContainer.new()
	grid.columns = 3
	grid.add_theme_constant_override("h_separation", 8)
	grid.add_theme_constant_override("v_separation", 8)
	_pendant.add_child(grid)

	grid.add_child(_hold_button("Raise", "RAISE", func() -> void: _raise_held = true, func() -> void: _raise_held = false))
	grid.add_child(_hold_button("Lower", "LOWER", func() -> void: _lower_held = true, func() -> void: _lower_held = false))
	var brake := Button.new()
	brake.text = "BRAKE"
	brake.custom_minimum_size = Vector2(118, 56)
	brake.pressed.connect(func() -> void:
		if _native != null and bool(_native.is_jib_station_occupied()):
			_native.set_jib_brake(not bool(_native.is_jib_brake_engaged()))
	)
	grid.add_child(brake)
	grid.add_child(_hold_button("SlewL", "SLEW L", func() -> void: _slew_left_held = true, func() -> void: _slew_left_held = false))
	grid.add_child(_hold_button("SlewR", "SLEW R", func() -> void: _slew_right_held = true, func() -> void: _slew_right_held = false))
	var exit_btn := Button.new()
	exit_btn.text = "EXIT"
	exit_btn.custom_minimum_size = Vector2(118, 56)
	exit_btn.pressed.connect(func() -> void:
		if _native != null:
			_native.request_exit_jib_station()
			_raise_held = false
			_lower_held = false
			_slew_left_held = false
			_slew_right_held = false
	)
	grid.add_child(exit_btn)

func _sync_jib_meshes() -> void:
	if _native == null:
		return
	var tip: Vector3 = _native.get_jib_boom_tip_position()
	var hook: Vector3 = _native.get_jib_hook_position()
	var crate: Vector3 = _native.get_jib_crate_position()
	var mast := Vector3(-12.0, 9.0, 48.0)
	var slew := float(_native.get_jib_slew_radians())
	if _jib_boom_mesh != null:
		_jib_boom_mesh.position = (mast + tip) * 0.5
		_jib_boom_mesh.rotation = Vector3(0.0, slew, 0.0)
	if _jib_hook_mesh != null:
		_jib_hook_mesh.position = hook
	if _jib_crate_mesh != null:
		_jib_crate_mesh.position = crate
	if _jib_cable_mesh != null:
		var mid := (tip + hook) * 0.5
		var length := tip.distance_to(hook)
		_jib_cable_mesh.position = mid
		_jib_cable_mesh.scale = Vector3(1.0, maxf(length, 0.05), 1.0)
		if length > 0.001:
			_jib_cable_mesh.look_at(tip, Vector3.RIGHT)
			_jib_cable_mesh.rotate_object_local(Vector3.RIGHT, PI * 0.5)

func _sync_needle_mesh() -> void:
	if _native == null or _needle_mesh == null or not _native.has_method("get_needle_position"):
		return
	_needle_mesh.position = _native.get_needle_position()
	_needle_mesh.rotation = Vector3(0.0, float(_native.get_needle_yaw_radians()), 0.0)

func _build_needle_bay(mill_scale: Material, oxidized_steel: Material, weathered_timber: Material, galvanized: Material, faded_yellow: Material, chipped_orange: Material) -> void:
	_add_box("KxPocketWest", Vector3(0.72, 0.36, 0.72), Vector3(-8.75, 4.86, 43.90), mill_scale)
	_add_box("KxPocketEast", Vector3(0.72, 0.36, 0.72), Vector3(-1.55, 4.86, 43.90), mill_scale)
	_add_box("KxPocketCheekW", Vector3(0.18, 0.70, 0.80), Vector3(-8.75, 5.20, 43.40), oxidized_steel)
	_add_box("KxPocketCheekE", Vector3(0.18, 0.70, 0.80), Vector3(-1.55, 5.20, 43.40), oxidized_steel)
	_needle_mesh = _add_box("KxNeedle", Vector3(7.20, 0.36, 0.56), Vector3(-18.0, 0.18, 40.0), faded_yellow)
	_add_box("KxNearLanding", Vector3(4.40, 0.40, 3.00), Vector3(-10.60, 5.20, 43.90), weathered_timber)
	_add_box("KxFarLanding", Vector3(5.00, 0.40, 3.00), Vector3(0.60, 5.20, 43.90), weathered_timber)
	_add_box("KxBayFloor", Vector3(12.00, 0.40, 10.00), Vector3(9.50, 8.50, 40.50), mill_scale)
	_add_box("KxBayFloorEdge", Vector3(12.2, 0.08, 0.18), Vector3(9.50, 8.72, 45.40), faded_yellow)
	for i in range(15):
		var y_top := 0.36 * float(i + 1)
		var z := 53.20 - 0.60 * float(i)
		_add_box("KxWestTread", Vector3(2.30, 0.20, 0.64), Vector3(-10.60, y_top - 0.10, z), galvanized)
	for i in range(9):
		var y_top := 5.40 + 0.367 * float(i + 1)
		var x := 2.40 + 0.68 * float(i)
		_add_box("KxEastTread", Vector3(0.84, 0.20, 2.40), Vector3(x, y_top - 0.10, 43.90), galvanized)
	_add_box("KxNeedleRailN", Vector3(4.8, 0.08, 0.08), Vector3(-5.15, 6.15, 42.55), faded_yellow)
	_add_box("KxBayHeader", Vector3(8.0, 1.4, 1.6), Vector3(9.5, 14.2, 45.2), oxidized_steel)
	_add_box("KxBayColumnL", Vector3(1.4, 16.0, 1.4), Vector3(4.2, 8.0, 45.4), mill_scale)
	_add_box("KxBayColumnR", Vector3(1.4, 16.0, 1.4), Vector3(14.8, 8.0, 45.4), mill_scale)
	_add_machine_wheel(Vector3(9.5, 12.4, 44.6), 2.6, 0.7, mill_scale, oxidized_steel)
	_add_box("KxStagingRack", Vector3(7.6, 0.22, 0.7), Vector3(-18.0, 0.20, 40.0), mill_scale)
	_add_box("KxApronBrace", Vector3(0.55, 5.4, 0.55), Vector3(-13.4, 2.7, 44.2), oxidized_steel)
	_add_box("KxApronBrace2", Vector3(0.55, 5.4, 0.55), Vector3(-7.8, 2.7, 44.2), oxidized_steel)
	_add_box("KxPocketPaint", Vector3(0.20, 0.12, 0.46), Vector3(-8.75, 5.28, 43.90), chipped_orange)

func _build_cage_house(mill_scale: Material, oxidized_steel: Material, galvanized: Material, faded_yellow: Material, chipped_orange: Material, weathered_timber: Material) -> void:
	# Open well: rails and guides, not a 280 m concrete plug.
	_add_box("CageRailL", Vector3(0.22, 48.0, 0.22), Vector3(7.75, 24.0, 34.20), galvanized)
	_add_box("CageRailR", Vector3(0.22, 48.0, 0.22), Vector3(11.25, 24.0, 34.20), galvanized)
	_add_box("CageGuideN", Vector3(0.16, 48.0, 0.16), Vector3(9.50, 24.0, 35.85), mill_scale)
	_add_box("CageGuideS", Vector3(0.16, 48.0, 0.16), Vector3(9.50, 24.0, 32.55), mill_scale)
	_add_box("CageHeadgear", Vector3(8.4, 1.8, 6.2), Vector3(9.50, 24.8, 34.20), oxidized_steel)
	_add_hoist_drum(Vector3(9.50, 26.4, 31.4), 1.6, 4.8, mill_scale, galvanized)
	_add_machine_wheel(Vector3(14.6, 12.2, 34.0), 3.4, 0.9, mill_scale, oxidized_steel)
	_add_machine_wheel(Vector3(4.4, 11.6, 33.4), 2.6, 0.7, oxidized_steel, mill_scale)
	_add_box("CageHousePostL", Vector3(1.2, 16.0, 1.2), Vector3(5.6, 8.0, 32.2), mill_scale)
	_add_box("CageHousePostR", Vector3(1.2, 16.0, 1.2), Vector3(13.4, 8.0, 32.2), mill_scale)
	_add_box("CageHouseBeam", Vector3(9.2, 0.7, 1.1), Vector3(9.5, 16.2, 32.2), oxidized_steel)
	_add_box("CageHouseTimber", Vector3(6.5, 8.0, 0.4), Vector3(9.5, 10.0, 31.4), weathered_timber)
	_add_box("KxUpperLanding", Vector3(6.40, 0.36, 4.40), Vector3(9.50, 21.82, 37.80), mill_scale)
	_add_box("KxUpperEdge", Vector3(6.5, 0.08, 0.14), Vector3(9.50, 22.02, 39.90), faded_yellow)
	_add_box("KxUpperRailL", Vector3(0.10, 1.1, 4.2), Vector3(6.40, 22.55, 37.80), faded_yellow)
	_add_box("KxUpperRailR", Vector3(0.10, 1.1, 4.2), Vector3(12.60, 22.55, 37.80), faded_yellow)
	_add_wrapping_stairs(Vector3(15.6, 0.0, 34.2), 8.7, 1, galvanized, faded_yellow)

	_cage_mesh = _add_box("KxCageDeck", Vector3(3.10, 0.36, 3.10), Vector3(9.50, 8.52, 34.20), faded_yellow)
	_cage_gate_mesh = _add_box("KxCageGate", Vector3(2.6, 2.4, 0.12), Vector3(9.50, 9.85, 32.72), oxidized_steel)
	_cage_lever_mesh = _add_box("KxCageLever", Vector3(0.12, 1.15, 0.12), Vector3(10.20, 9.47, 33.85), chipped_orange)
	_cage_lever_mesh.rotation.z = 0.45
	_add_box("KxCageLeverBase", Vector3(0.45, 0.22, 0.45), Vector3(10.20, 8.82, 33.85), mill_scale)
	_cage_wheel_mesh = _add_cylinder("KxCageHandwheel", 0.85, 0.12, Vector3(8.35, 9.55, 33.40), oxidized_steel)
	_cage_wheel_mesh.rotation.x = PI * 0.5
	_add_floodlight(Vector3(9.5, 16.8, 32.0), Vector3(0.0, -0.45, 1.0))
	_add_floodlight(Vector3(7.2, 10.4, 36.0), Vector3(0.2, -0.25, -0.4))

func _build_sump_bay(mill_scale: Material, oxidized_steel: Material, dark_concrete: Material, weathered_timber: Material, galvanized: Material, faded_yellow: Material, chipped_orange: Material) -> void:
	# Torn +22 m floor: the grate is a real hole while the sump is wet.
	_add_box("SumpPitWest", Vector3(0.44, 4.40, 8.40), Vector3(5.95, 19.40, 42.80), oxidized_steel)
	_add_box("SumpPitEast", Vector3(0.44, 4.40, 8.40), Vector3(13.05, 19.40, 42.80), oxidized_steel)
	_add_box("SumpPitSouthLip", Vector3(6.8, 0.22, 0.55), Vector3(9.50, 21.62, 40.05), mill_scale)
	_add_box("SumpPitNorthLip", Vector3(6.8, 0.22, 0.55), Vector3(9.50, 21.62, 45.10), mill_scale)
	_add_box("SumpFloor", Vector3(6.80, 0.36, 8.40), Vector3(9.50, 17.20, 42.80), dark_concrete)
	_add_box("SumpSludge", Vector3(6.2, 0.10, 7.6), Vector3(9.50, 17.42, 42.80), oxidized_steel)
	_add_box("SumpFarLanding", Vector3(6.00, 0.36, 4.80), Vector3(9.50, 21.82, 47.50), mill_scale)
	_add_box("SumpFarEdge", Vector3(6.1, 0.08, 0.14), Vector3(9.50, 22.02, 45.20), faded_yellow)
	_add_box("SumpFarRailL", Vector3(0.10, 1.05, 4.4), Vector3(6.55, 22.52, 47.50), faded_yellow)
	_add_box("SumpFarRailR", Vector3(0.10, 1.05, 4.4), Vector3(12.45, 22.52, 47.50), faded_yellow)
	_add_box("BrokenSlabL", Vector3(1.8, 0.16, 1.1), Vector3(6.70, 21.70, 40.90), mill_scale).rotation.z = 0.18
	_add_box("BrokenSlabR", Vector3(1.6, 0.14, 0.9), Vector3(12.20, 21.68, 40.70), oxidized_steel).rotation.z = -0.22
	_add_box("HangingPlate", Vector3(2.4, 0.08, 1.6), Vector3(11.6, 20.4, 43.6), mill_scale).rotation = Vector3(0.35, 0.2, -0.4)
	_add_box("TornRebar", Vector3(0.08, 1.8, 0.08), Vector3(7.1, 20.9, 40.4), galvanized).rotation.z = 0.55
	_add_box("SumpDowncomer", Vector3(0.55, 8.5, 0.55), Vector3(12.15, 17.8, 40.6), galvanized)
	_add_box("SumpFillRiser", Vector3(0.48, 9.2, 0.48), Vector3(6.85, 18.2, 40.2), oxidized_steel)
	_add_box("SumpHeaderPipe", Vector3(6.6, 0.42, 0.42), Vector3(9.50, 23.55, 40.15), galvanized)

	# Visual grate bars only — collision is native and wet means you fall through.
	for i in range(9):
		var z := 40.15 + 0.55 * float(i)
		_add_box("KxGrateBar", Vector3(4.80, 0.07, 0.12), Vector3(9.50, 21.84, z), oxidized_steel)
	for i in range(5):
		var x := 7.30 + 1.10 * float(i)
		_add_box("KxGrateTie", Vector3(0.10, 0.05, 5.10), Vector3(x, 21.80, 42.55), mill_scale)

	var dirty_water := _transparent_material(Color(0.22, 0.28, 0.24, 0.55), 0.18)
	_sump_water_mesh = _add_box("KxSumpWater", Vector3(6.4, 0.22, 8.0), Vector3(9.50, 21.20, 42.80), dirty_water)

	_add_box("KxValvePedestal", Vector3(0.70, 1.10, 0.70), Vector3(6.85, 22.45, 38.20), mill_scale)
	_add_box("KxValveStem", Vector3(0.12, 0.70, 0.12), Vector3(6.85, 23.10, 38.20), galvanized)
	_sump_valve_mesh = _add_cylinder("KxValveWheel", 0.62, 0.10, Vector3(6.85, 22.70, 38.20), chipped_orange)
	_sump_valve_mesh.rotation.x = PI * 0.5
	_add_box("KxValveTag", Vector3(0.28, 0.08, 0.02), Vector3(6.85, 22.18, 37.82), faded_yellow)

	_add_box("KxDrainPedestal", Vector3(0.55, 0.90, 0.55), Vector3(12.15, 22.35, 38.40), mill_scale)
	_sump_drain_mesh = _add_box("KxDrainCock", Vector3(0.16, 0.55, 0.16), Vector3(12.15, 22.55, 38.40), faded_yellow)
	_sump_drain_mesh.rotation.x = 0.20
	_add_box("KxDrainPipe", Vector3(0.22, 0.22, 2.4), Vector3(12.15, 22.05, 39.50), galvanized)
	_add_box("KxDrainTag", Vector3(0.28, 0.08, 0.02), Vector3(12.15, 22.05, 38.05), chipped_orange)

	_add_box("SumpBayPostL", Vector3(1.1, 10.0, 1.1), Vector3(5.4, 19.0, 47.8), mill_scale)
	_add_box("SumpBayPostR", Vector3(1.1, 10.0, 1.1), Vector3(13.6, 19.0, 47.8), mill_scale)
	_add_box("SumpBayHeader", Vector3(9.4, 1.1, 1.4), Vector3(9.50, 24.4, 47.6), oxidized_steel)
	_add_box("TimberInfillSump", Vector3(4.8, 6.4, 0.38), Vector3(15.6, 20.6, 46.8), weathered_timber)
	_add_floodlight(Vector3(9.5, 24.6, 40.8), Vector3(0.0, -0.55, 0.35))
	_add_floodlight(Vector3(6.2, 23.4, 38.0), Vector3(0.35, -0.4, 0.2))

func _sync_sump_meshes(isolated: bool, drain_open: bool, inventory: float) -> void:
	if _sump_water_mesh != null:
		var water_y := 17.42 + clampf(inventory, 0.0, 1.0) * 4.20
		_sump_water_mesh.position = Vector3(9.50, water_y, 42.80)
		_sump_water_mesh.visible = inventory > 0.03
	if _sump_valve_mesh != null:
		_sump_valve_mesh.rotation = Vector3(PI * 0.5, 1.35 if isolated else 0.20, 0.0)
	if _sump_drain_mesh != null:
		_sump_drain_mesh.rotation.x = 1.15 if drain_open else 0.20

func _add_wrapping_stairs(origin: Vector3, rise: float, flights: int, tread_material: Material, rail_material: Material) -> void:
	for flight in range(flights):
		var base_y := origin.y + rise * float(flight)
		for i in range(12):
			var t := float(i)
			var y := base_y + 0.36 * (t + 1.0)
			var z := origin.z - 0.42 * t
			_add_box("WrapTread", Vector3(1.6, 0.14, 0.46), Vector3(origin.x, y - 0.07, z), tread_material)
		_add_box("WrapRail", Vector3(0.08, rise * 0.9, 0.08), Vector3(origin.x + 0.85, base_y + rise * 0.5, origin.z - 2.4), rail_material)

func _sync_cage_meshes() -> void:
	if _native == null or _cage_mesh == null or not _native.has_method("get_cage_position"):
		return
	var cage: Vector3 = _native.get_cage_position()
	var lever: Vector3 = _native.get_cage_lever_position()
	var velocity: Vector3 = _native.get_cage_linear_velocity()
	_cage_mesh.position = cage
	if _cage_gate_mesh != null:
		_cage_gate_mesh.position = cage + Vector3(0.0, 1.33, -1.48)
	if _cage_lever_mesh != null:
		_cage_lever_mesh.position = lever
		var pulled := not bool(_native.is_cage_brake_engaged())
		_cage_lever_mesh.rotation.z = 0.95 if pulled else 0.45
	if _cage_wheel_mesh != null:
		_cage_wheel_angle += velocity.y * 0.55
		_cage_wheel_mesh.position = cage + Vector3(-1.15, 1.03, -0.80)
		_cage_wheel_mesh.rotation = Vector3(PI * 0.5, 0.0, _cage_wheel_angle)

func _build_stacked_machine_tower(mill_scale: Material, oxidized_steel: Material, dark_concrete: Material, weathered_timber: Material, galvanized: Material, faded_yellow: Material) -> void:
	# Machine bays ARE the tower. Corner posts only span their bay. The well stays open.
	var elevations := [8.0, 22.0, 36.0, 50.0, 66.0, 82.0, 100.0, 118.0, 138.0, 158.0, 180.0, 204.0, 230.0, 258.0, 288.0, 320.0]
	for index in range(elevations.size()):
		var y: float = elevations[index]
		var next_y: float = elevations[index + 1] if index + 1 < elevations.size() else y + 28.0
		var bay_h: float = next_y - y
		var variant := index % 4
		var dense := index < 5
		var post_h: float = bay_h
		var post_y: float = y + bay_h * 0.5
		var post_size := Vector3(2.4 if dense else 1.8, post_h, 2.4 if dense else 1.8)
		for corner in [Vector3(-20.0, post_y, 18.0), Vector3(20.0, post_y, 18.0), Vector3(-20.0, post_y, -16.0), Vector3(20.0, post_y, -16.0)]:
			_add_box("BayPost", post_size, corner, mill_scale if dense else oxidized_steel)
		_add_box("BayFloorRingN", Vector3(44.0 if dense else 36.0, 1.2 if dense else 0.8, 2.8), Vector3(0.0, y, 19.5), oxidized_steel)
		_add_box("BayFloorRingS", Vector3(44.0 if dense else 36.0, 1.2 if dense else 0.8, 2.8), Vector3(0.0, y, -16.5), dark_concrete)
		_add_box("BayFloorRingE", Vector3(2.8, 1.2 if dense else 0.8, 34.0), Vector3(20.0, y, 1.5), mill_scale)
		_add_box("BayFloorRingW", Vector3(2.8, 1.2 if dense else 0.8, 34.0), Vector3(-20.0, y, 1.5), mill_scale)
		_add_box("BayHeaderN", Vector3(32.0, 2.0 if dense else 1.3, 2.2), Vector3(0.0, y + bay_h * 0.72, 19.0), mill_scale)
		_add_box("ChevronL", Vector3(1.2, bay_h * 0.85, 1.2), Vector3(-13.0, y + bay_h * 0.42, 18.4), oxidized_steel).rotation.z = 0.48
		_add_box("ChevronR", Vector3(1.2, bay_h * 0.85, 1.2), Vector3(13.0, y + bay_h * 0.42, 18.4), oxidized_steel).rotation.z = -0.48
		_add_box("CatwalkN", Vector3(24.0, 0.22, 2.2), Vector3(0.0, y + 3.4, 22.2), galvanized)
		_add_box("CatwalkRailN", Vector3(24.0, 0.08, 0.08), Vector3(0.0, y + 4.3, 23.2), faded_yellow)
		if dense:
			_add_wrapping_stairs(Vector3(-16.5 + float(index % 2) * 33.0, y, 21.5), minf(bay_h - 1.0, 12.0), 1, galvanized, faded_yellow)
		if variant == 0:
			if dense:
				_add_machine_wheel(Vector3(-14.0, y + 7.2, 16.2), 4.2, 1.15, mill_scale, oxidized_steel)
				_add_hoist_drum(Vector3(13.0, y + 6.2, 16.0), 2.5, 6.0, mill_scale, galvanized)
			else:
				_add_cylinder("BaySilhouetteWheel", 3.6, 0.9, Vector3(-14.0, y + 7.0, 16.0), mill_scale).rotation.x = PI * 0.5
		elif variant == 1:
			_add_hoist_drum(Vector3(-12.0, y + 6.8, 15.2), 2.8 if dense else 2.0, 6.4, oxidized_steel, mill_scale)
			if dense:
				_add_machine_wheel(Vector3(14.0, y + 7.6, 15.8), 3.4, 0.95, oxidized_steel, mill_scale)
			_add_box("TimberInfill", Vector3(7.2, 8.5 if dense else 5.5, 0.45), Vector3(-18.0, y + 5.5, 20.8), weathered_timber)
		elif variant == 2:
			_add_box("IdleCraneMast", Vector3(2.4, 12.0 if dense else 8.0, 2.4), Vector3(16.0, y + (7.5 if dense else 5.0), 13.5), oxidized_steel)
			_add_box("IdleCraneBoom", Vector3(1.5, 1.3, 18.0 if dense else 12.0), Vector3(16.0, y + (13.5 if dense else 9.0), 22.0), mill_scale).rotation.x = -0.22
			if dense:
				_add_machine_wheel(Vector3(-16.0, y + 6.6, 15.5), 3.1, 0.85, mill_scale, oxidized_steel)
		else:
			_add_box("PipeRack", Vector3(20.0, 0.65, 0.65), Vector3(0.0, y + 8.8, 17.2), galvanized)
			_add_box("Counterweight", Vector3(4.0, 5.4 if dense else 3.6, 2.8), Vector3(17.5, y + 3.8, 14.0), mill_scale)
			_add_box("TimberInfill", Vector3(7.6, 9.0 if dense else 6.0, 0.45), Vector3(18.0, y + 6.4, 20.8), weathered_timber)
		if index % 3 == 0:
			_add_floodlight(Vector3(-10.0, y + 10.0, 20.6), Vector3(0.15, -0.35, 1.0))
		if index >= 8:
			# Upper tower continues as overlapping machine silhouette, crown hidden in weather.
			_add_box("HighSilhouette", Vector3(28.0, 0.7, 1.6), Vector3(0.0, y + bay_h * 0.55, 17.5), mill_scale)

func _material(color: Color, metallic: float, roughness: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	return material

func _transparent_material(color: Color, roughness: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.albedo_color = color
	material.metallic = 0.0
	material.roughness = roughness
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	return material

func _lamp_material() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("a86e2b")
	material.metallic = 0.12
	material.roughness = 0.48
	material.emission_enabled = true
	material.emission = Color("d99038")
	material.emission_energy_multiplier = 2.1
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

func _add_sphere(node_name: String, radius: float, at: Vector3, material: Material) -> MeshInstance3D:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 18
	mesh.rings = 10
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = mesh
	instance.position = at
	$TowerPresentation.add_child(instance)
	return instance

func _add_cylinder(node_name: String, radius: float, height: float, at: Vector3, material: Material) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 20
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = mesh
	instance.position = at
	$TowerPresentation.add_child(instance)
	return instance

func _add_machine_wheel(center: Vector3, radius: float, depth: float, material: Material, tooth_material: Material) -> void:
	var rim := _add_cylinder("MachineWheelRim", radius, depth, center, material)
	rim.rotation.x = PI * 0.5
	var hub := _add_cylinder("MachineWheelHub", radius * 0.22, depth * 1.35, center, tooth_material)
	hub.rotation.x = PI * 0.5
	for index in range(16):
		var angle := TAU * float(index) / 16.0
		var radial := Vector3(cos(angle), sin(angle), 0.0)
		var tooth := _add_box("MachineWheelTooth", Vector3(radius * 0.22, radius * 0.55, depth * 1.25), center + radial * radius * 0.88, tooth_material)
		tooth.rotation.z = angle

func _add_hoist_drum(center: Vector3, radius: float, width: float, material: Material, flange_material: Material) -> void:
	var drum := _add_cylinder("HoistDrum", radius, width, center, material)
	drum.rotation.z = PI * 0.5
	for side in [-1.0, 1.0]:
		var flange := _add_cylinder("HoistFlange", radius * 1.18, 0.28, center + Vector3(side * width * 0.5, 0.0, 0.0), flange_material)
		flange.rotation.z = PI * 0.5

func _add_cloud_wisp(at: Vector3, scale_value: Vector3, material: Material) -> MeshInstance3D:
	var wisp := _add_sphere("WeatherWisp", 1.0, at, material)
	wisp.scale = scale_value
	wisp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return wisp

func _add_floodlight(at: Vector3, direction: Vector3) -> void:
	var housing := _add_box("FloodLampHousing", Vector3(0.8, 0.45, 0.55), at, _material(Color("3a3a36"), 0.58, 0.64))
	var face := _add_box("FloodLampFace", Vector3(0.62, 0.30, 0.04), at + direction.normalized() * 0.30, _lamp_material())
	housing.look_at(at + direction, Vector3.UP)
	face.look_at(at + direction, Vector3.UP)
	var light := OmniLight3D.new()
	light.name = "SodiumFlood"
	light.position = at + direction.normalized() * 0.35
	light.light_color = Color("d99038")
	light.light_energy = 5.2
	light.omni_range = 26.0
	light.shadow_enabled = true
	$TowerPresentation.add_child(light)

func _animate_weather(simulation_time: float) -> void:
	for index in range(_cloud_wisps.size()):
		var wisp := _cloud_wisps[index]
		var base_speed := 0.004 + float(index % 5) * 0.0008
		wisp.position.x += sin(simulation_time * base_speed + float(index)) * 0.002
		wisp.position.z += cos(simulation_time * base_speed * 0.7 + float(index)) * 0.0015

func _fail_native(reason: String, exit_code: int) -> void:
	_status.text = "NATIVE AUTHORITY FAILURE"
	_status.modulate = Color("b45143")
	push_error(reason)
	get_tree().quit(exit_code)

func _print_runtime_proof() -> void:
	var position: Vector3 = _native.get_player_position()
	var load_position: Vector3 = _native.get_hopper_load_position()
	var rocker_omega: Vector3 = _native.get_impact_rocker_angular_velocity()
	var viewport := get_viewport().get_visible_rect().size
	var aspect := viewport.x / viewport.y if viewport.y > 0.0 else 0.0
	print("SCRAPERX_CP004_RUNTIME_PROOF ticks=%d viewport=(%.0f,%.0f) aspect=%.4f fold_layout=%d approach=%d interaction=%d load_moved=%d rocker=%d vault=%d mantle=%d player=(%.3f,%.3f,%.3f) load=(%.3f,%.3f,%.3f) rocker_omega_x=%.3f" % [
		_native.get_tick_index(), viewport.x, viewport.y, aspect, int(_fold_layout_observed), int(_ci_approach_observed), int(_ci_machine_requested), int(_ci_load_moved_observed), int(_ci_rocker_observed), int(_ci_vault_observed), int(_ci_mantle_observed), position.x, position.y, position.z, load_position.x, load_position.y, load_position.z, rocker_omega.x,
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
