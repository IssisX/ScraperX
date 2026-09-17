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
var _cloud_wisps: Array[MeshInstance3D] = []

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
	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim checkpoint=CP-004-TRAVERSAL-IMPACT")
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
	if bool(_native.can_operate_hopper()):
		_native.request_hopper_release()

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

	_camera.position = position + EYE_OFFSET
	_camera.rotation = Vector3(_pitch, _yaw, 0.0)
	_position_value.text = "POS  %7.2f  %6.2f  %7.2f m" % [position.x, position.y, position.z]
	_velocity_value.text = "VEL  %7.2f  %6.2f  %7.2f m/s" % [velocity.x, velocity.y, velocity.z]
	_support_value.text = "SUPPORT  %s  E%02d  /  TRAVERSE %s" % [
		"GROUNDED" if grounded else "AIRBORNE",
		support,
		_traversal_name(traversal_mode),
	]
	_machine_value.text = "CHAIN  HOPPER %s  /  ROCKER %s  ωX %5.2f" % [
		"MOVED" if hopper_moved else ("OPEN" if hopper_open else ("CYCLING" if hopper_started else "READY")),
		"STRUCK" if rocker_struck else "IDLE",
		rocker_omega.x,
	]
	_tick_value.text = "90 HZ NATIVE  /  %08d" % int(_native.get_tick_index())
	_boundary_value.text = "EXTERIOR MACHINE / PHYSICAL TRAVERSAL"

	var context_visible := false
	if traversal_mode == TRAVERSAL_HANG:
		_action_button.text = "DROP"
		context_visible = true
	elif traversal_available:
		_action_button.text = _traversal_name(traversal_candidate)
		context_visible = true
	elif hopper_available and not hopper_started:
		_action_button.text = "RELEASE HOPPER"
		context_visible = true
	_action_button.visible = context_visible
	_action_button.disabled = not context_visible
	_jump_button.text = "CLIMB" if traversal_mode == TRAVERSAL_HANG else "JUMP"

	if traversal_mode == TRAVERSAL_HANG:
		_status.text = "LEDGE HELD / JUMP TO CLIMB / ACTION TO DROP"
	elif traversal_available:
		_status.text = "%s AVAILABLE / REAL GEOMETRY IN REACH" % _traversal_name(traversal_candidate)
	elif hopper_available:
		_status.text = "LOADING BAY CONTROL IN RANGE"
	elif rocker_struck:
		_status.text = "LOAD IMPACT PROPAGATED / ROCKER NOW PHYSICAL"
	elif hopper_moved:
		_status.text = "HOPPER DISCHARGED / FOLLOW THE MASS"
	elif grounded:
		_status.text = "EXTERIOR GRADE / MACHINE-TOWER AHEAD"
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

	# Monumental lower mass: concrete piers plus steel machine bays, not a thin scaffold cage.
	_add_box("TowerBasePierL", Vector3(20.0, 30.0, 6.0), Vector3(-17.0, 15.0, 10.0), dark_concrete)
	_add_box("TowerBasePierR", Vector3(20.0, 30.0, 6.0), Vector3(17.0, 15.0, 10.0), dark_concrete)
	_add_box("LoadingBayHeader", Vector3(14.0, 3.0, 6.0), Vector3(0.0, 27.5, 10.0), oxidized_steel)
	_add_box("TowerCoreSpine", Vector3(24.0, 900.0, 30.0), Vector3(0.0, 470.0, -5.0), dark_concrete)
	_add_box("MachineBayMassL", Vector3(13.0, 610.0, 24.0), Vector3(-19.0, 320.0, -1.0), mill_scale)
	_add_box("MachineBayMassR", Vector3(13.0, 610.0, 24.0), Vector3(19.0, 320.0, -1.0), mill_scale)

	for x in [-31.0, 31.0]:
		for z in [-27.0, 25.0]:
			_add_box("MegaColumn", Vector3(2.8, 650.0, 2.8), Vector3(x, 325.0, z), mill_scale)
	for level in range(36, 325, 18):
		var y := float(level)
		for z in [-27.0, 25.0]:
			_add_box("ExteriorBeamX", Vector3(64.0, 1.3, 1.3), Vector3(0.0, y, z), oxidized_steel)
		for x in [-31.0, 31.0]:
			_add_box("ExteriorBeamZ", Vector3(1.3, 1.3, 54.0), Vector3(x, y, -1.0), oxidized_steel)

	# Timber infill breaks the concrete/steel mass with a construction language seen in the identity reference.
	for y in [44.0, 68.0, 92.0, 116.0]:
		_add_box("TimberInfillL", Vector3(8.0, 14.0, 0.55), Vector3(-18.5, y, 12.2), weathered_timber)
		_add_box("TimberInfillR", Vector3(8.0, 14.0, 0.55), Vector3(18.5, y + 8.0, 12.2), weathered_timber)

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

	# Machine-tower identity: large readable idle infrastructure, not fake animated authority.
	_add_machine_wheel(Vector3(-17.0, 43.0, 14.0), 6.5, 1.4, mill_scale, oxidized_steel)
	_add_machine_wheel(Vector3(-18.0, 22.0, 14.0), 4.8, 1.2, oxidized_steel, mill_scale)
	_add_hoist_drum(Vector3(12.0, 24.0, 14.0), 3.4, 7.5, mill_scale, galvanized)
	_add_box("LiftRailL", Vector3(0.55, 120.0, 0.55), Vector3(-8.0, 60.0, 15.0), galvanized)
	_add_box("LiftRailR", Vector3(0.55, 120.0, 0.55), Vector3(-4.8, 60.0, 15.0), galvanized)
	_add_box("IdleLiftCar", Vector3(3.0, 5.0, 2.6), Vector3(-6.4, 14.0, 15.0), oxidized_steel)

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
