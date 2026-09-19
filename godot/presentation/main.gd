extends Node3D

# Presentation and input only. Every consequential fact below is read from the
# native ScraperX simulation; nothing here decides pose, support, traversal, or
# machine state. Where this file draws something that looks simulated -- the
# steam plume above all -- it is driven by an authoritative native value, so
# freezing that value freezes the effect.

const EYE_OFFSET := Vector3(0.0, 0.62, 0.0)
const TOUCH_RADIUS := 100.0

const TRANSLATING_SUPPORT_ENTITY_ID := 3
const MANTLE_LEDGE_ENTITY_ID := 6
const TIPPER_ENTITY_ID := 14
const LIFT_PLATFORM_ENTITY_ID := 16
const CATWALK_ENTITY_ID := 19
const TREADLE_ENTITY_ID := 24
const JIB_HOOK_ENTITY_ID := 27
const CRATE_ENTITY_ID := 28
const NEEDLE_BEAM_ENTITY_ID := 33
const SUMP_GRATE_ENTITY_ID := 34

# The stack. Mirrors the kStack* constants in simulation.cpp exactly -- these
# are the native collision sizes, so what is drawn is what you stand on.
const STACK_CENTER := Vector3(0.0, 0.0, -150.0)
const STACK_HALF_EXTENT := 26.0
const STACK_LEVEL_HEIGHT := 11.0
const STACK_LEVEL_COUNT := 14
const STACK_DECK_THICKNESS := 0.5
const STACK_DECK_BAND_DEPTH := 9.0
const STACK_COLUMN_SIZE := 1.6
const STACK_RAMP_WIDTH := 3.2
const STACK_MASS_BASE_Y := 159.0

const TRAVERSAL_NONE := 0
const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3

# Mirrors scraperx::sim::FallState.
const FALL_GROUNDED := 0
const FALL_AIRBORNE := 1
const FALL_PARACHUTING := 2

const PHASE_APPROACH := 0
const PHASE_OBSERVE := 1
const PHASE_PROVEN := 2

# The tower face sits at z = -145. Walking to z = -78 puts its lower third across
# the whole frame while the plant is still in shot to the right.
const CI_APPROACH_TARGET_Z := -66.0
const CI_APPROACH_FACING := Vector2(-0.22, -0.975)
const CI_OBSERVE_FACING := Vector2(0.36, -0.933)
const CI_HOLD_TICKS := 20

# Reference mass flow for the plume, kg/s. The native orifice peaks near this, so
# the ratio below is a real fraction of a real flow, not a tuned animation curve.
const PLUME_REFERENCE_FLOW := 0.75

# Mirrors scraperx::sim kMachineCyclePeriodSeconds. The tower-face gear motif is
# decorative -- it owns no state and is never queried -- but its rotation is a
# real function of the native machine_cycle_phase_seconds, not a free-running
# clock, so it reads as the visible face of the actual plant. Governing Law 26:
# it must never be mistaken for a second physics authority.
const KELLERWORKS_CYCLE_PERIOD_SECONDS := 26.0

var _native: Object
var _capture_path := ""
var _capture_scheduled := false
var _ci_mode := false
var _yaw := 0.0
var _pitch := -0.02
var _move_touch_index := -1
var _look_touch_index := -1
var _move_touch_origin := Vector2.ZERO
var _touch_move := Vector2.ZERO
var _viewport_size := Vector2.ZERO

var _scoop_meshes: Array[MeshInstance3D] = []
var _scoop_locals: Array[Vector3] = []
var _ballast_mesh: MeshInstance3D
var _tipper_mesh: Node3D
var _valve_mesh: Node3D
var _treadle_mesh: Node3D
var _treadle_cable_a: Node3D
var _treadle_cable_b: Node3D
var _jib_boom_mesh: Node3D
var _jib_hook_mesh: MeshInstance3D
var _jib_crate_mesh: MeshInstance3D
var _jib_capacity_load_mesh: MeshInstance3D
var _jib_hoist_cable: Node3D
var _needle_beam_mesh: MeshInstance3D
var _needle_hoist_cable: Node3D
var _sump_grate_mesh: MeshInstance3D
var _sump_grate_safe_material: Material
var _sump_grate_hazard_material: Material
var _stack_gears: Array[Node3D] = []
var _lift_mesh: MeshInstance3D
var _counterweight_mesh: MeshInstance3D
var _translating_support_mesh: MeshInstance3D
var _rotating_support_mesh: MeshInstance3D
var _moving_ledge_mesh: MeshInstance3D
var _rope_mesh: MeshInstance3D
var _plume: CPUParticles3D
var _plume_material: StandardMaterial3D
var _fire_box: OmniLight3D
var _vent_light: OmniLight3D

# Decorative dressing (WO-007). None of these carry collision, own state, or
# feed the CI proof; the gear pivot is the one exception that reads a real
# native value (see KELLERWORKS_CYCLE_PERIOD_SECONDS above).
var _gear_pivot: Node3D
var _drum_pivot: Node3D
var _crane_boom: Node3D
var _crane_hook: Node3D
var _crane_crate: MeshInstance3D
var _ambient_clock := 0.0

var _ci_phase := PHASE_APPROACH
var _ci_facing := CI_APPROACH_FACING
var _ci_proof_tick := -1
var _ci_peak_valve := 0.0
var _ci_peak_lift := 0.0
var _ci_peak_flow := 0.0
var _ci_shut_flow := 0.0
var _ci_proof_printed := false

@onready var _camera: Camera3D = $Camera
@onready var _status: Label = $HUD/TopLeft/Status
@onready var _position_value: Label = $HUD/TopLeft/Position
@onready var _velocity_value: Label = $HUD/TopLeft/Velocity
@onready var _support_value: Label = $HUD/TopLeft/Support
@onready var _traversal_value: Label = $HUD/TopLeft/Traversal
@onready var _machine_value: Label = $HUD/TopLeft/Machine
@onready var _plant_value: Label = $HUD/TopLeft/Plant
@onready var _tick_value: Label = $HUD/TopRight/Tick
@onready var _touch_knob: ColorRect = $HUD/TouchMove/Knob
@onready var _action_button: Control = $HUD/TouchAction
@onready var _release_button: Control = $HUD/TouchRelease
@onready var _parachute_button: Control = $HUD/TouchParachute
@onready var _fall_value: Label = $HUD/TopLeft/Fall
@onready var _jib_value: Label = $HUD/TopLeft/Jib
@onready var _needle_value: Label = $HUD/TopLeft/Needle
@onready var _sump_value: Label = $HUD/TopLeft/Sump
@onready var _light_rig: Node3D = $LightRig


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")

	RenderingServer.set_default_clear_color(Color("0e0d0c"))
	_build_world()
	_layout_hud()
	get_viewport().size_changed.connect(_layout_hud)

	if not ClassDB.class_exists("ScraperXSimulation"):
		_fail_native("SCRAPERX_EXTENSION_LOAD_FAILED", 20)
		return

	_native = ClassDB.instantiate("ScraperXSimulation")
	if _native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return

	if _ci_mode:
		_yaw = atan2(-CI_APPROACH_FACING.x, -CI_APPROACH_FACING.y)
		_pitch = 0.06

	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim work_order=WO-006")
	print("SCRAPERX_VIEWPORT size=%dx%d aspect=%.3f fov=%.1f far=%.0f" % [
		int(_viewport_size.x), int(_viewport_size.y),
		_viewport_size.x / maxf(1.0, _viewport_size.y), _camera.fov, _camera.far])
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

	var forward := Vector2(-sin(_yaw), -cos(_yaw))
	var right := Vector2(cos(_yaw), -sin(_yaw))
	var world_move := right * desired.x + forward * desired.y
	if not _native.set_move_input(world_move.x, world_move.y):
		_fail_native("SCRAPERX_MOVE_INPUT_REJECTED", 21)
		return
	_native.set_facing(facing.x, facing.y)

	var jib_input := _read_jib_input()
	_native.set_jib_slew_input(jib_input.x)
	_native.set_jib_hoist_input(jib_input.y)
	# WO-012 KX-NEEDLE pendant: the same Raise/Lower axis as the jib's hoist --
	# the two stations are never in range simultaneously, so reusing it needs
	# no new key binding and keeps the same Raise(+)/Lower(-) verb.
	_native.set_needle_hoist_input(jib_input.y)

	var steps_advanced := int(_native.advance_frame(delta))
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_render_snapshot()
	_ambient_clock += delta
	_update_ambient_dressing()

	if _ci_mode:
		_ci_observe()

	var proof_ready := (
		_ci_proof_tick >= 0
		and int(_native.get_tick_index()) >= _ci_proof_tick + CI_HOLD_TICKS
	)

	if proof_ready and not _capture_path.is_empty() and not _capture_scheduled:
		_capture_scheduled = true
		RenderingServer.frame_post_draw.connect(_capture_frame, CONNECT_ONE_SHOT)
	elif proof_ready and _ci_mode and _capture_path.is_empty() and not _ci_proof_printed:
		_print_runtime_proof()
		get_tree().quit(0)


# --- CI sequence: walk the approach, then watch the plant work ---------------


func _ci_movement_intent(position: Vector3) -> Vector2:
	if _ci_phase == PHASE_APPROACH:
		if position.z <= CI_APPROACH_TARGET_Z:
			_ci_phase = PHASE_OBSERVE
			_ci_facing = CI_OBSERVE_FACING
			_pitch = 0.17
			_print_ci_phase("OBSERVE")
			return Vector2.ZERO
		_ci_facing = CI_APPROACH_FACING
		return Vector2(0.0, 1.0)
	return Vector2.ZERO


func _ci_observe() -> void:
	var valve := float(_native.get_valve_open_fraction())
	var flow := float(_native.get_orifice_mass_flow_kg_per_s())
	_ci_peak_valve = maxf(_ci_peak_valve, valve)
	_ci_peak_lift = maxf(_ci_peak_lift, float(_native.get_lift_platform_position().y))
	_ci_peak_flow = maxf(_ci_peak_flow, flow)
	if valve <= 0.0:
		_ci_shut_flow = maxf(_ci_shut_flow, flow)

	if _ci_phase == PHASE_OBSERVE and _ci_peak_valve > 0.5 and _ci_peak_lift > 6.0:
		_ci_phase = PHASE_PROVEN
		_ci_proof_tick = int(_native.get_tick_index())
		_print_ci_phase("MACHINE_PROVEN")


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
		elif touch.pressed and _touch_hits(_parachute_button, touch.position):
			if _native != null:
				_native.request_parachute()
		elif touch.pressed and touch.position.x < get_viewport().get_visible_rect().size.x * 0.5:
			if _move_touch_index == -1:
				_move_touch_index = touch.index
				_move_touch_origin = touch.position
		elif touch.pressed and _look_touch_index == -1:
			_look_touch_index = touch.index
		elif not touch.pressed and touch.index == _move_touch_index:
			_move_touch_index = -1
			_touch_move = Vector2.ZERO
			_touch_knob.position = Vector2(56.0, 56.0)
		elif not touch.pressed and touch.index == _look_touch_index:
			_look_touch_index = -1
	elif event is InputEventScreenDrag:
		var drag := event as InputEventScreenDrag
		if drag.index == _move_touch_index:
			var offset := (drag.position - _move_touch_origin).limit_length(TOUCH_RADIUS)
			_touch_move = Vector2(offset.x, -offset.y) / TOUCH_RADIUS
			_touch_knob.position = Vector2(56.0, 56.0) + offset
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
		elif key.keycode == KEY_F and _native != null:
			_native.request_parachute()
		elif key.keycode == KEY_V and _native != null:
			_native.request_valve_toggle()


func _touch_hits(control: Control, at: Vector2) -> bool:
	return control != null and Rect2(control.global_position, control.size).has_point(at)


func _read_desired_movement() -> Vector2:
	var keyboard := Vector2(
		float(int(Input.is_key_pressed(KEY_D)) - int(Input.is_key_pressed(KEY_A))),
		float(int(Input.is_key_pressed(KEY_W)) - int(Input.is_key_pressed(KEY_S)))
	)
	return (keyboard + _touch_move).limit_length(1.0)


# WO-011 KX-JIB pendant: x is Drive (slew), y is Raise(+)/Lower(-). Arrow keys
# so they never collide with WASD movement; effect is native-gated to the
# station radius regardless of what this reads.
func _read_jib_input() -> Vector2:
	return Vector2(
		float(int(Input.is_key_pressed(KEY_RIGHT)) - int(Input.is_key_pressed(KEY_LEFT))),
		float(int(Input.is_key_pressed(KEY_UP)) - int(Input.is_key_pressed(KEY_DOWN)))
	)


func _apply_look_delta(delta: Vector2) -> void:
	_yaw -= delta.x * 0.003
	_pitch = clampf(_pitch - delta.y * 0.003, -1.25, 1.35)


# --- HUD sized to the bounds the device actually gives us --------------------


func _layout_hud() -> void:
	_viewport_size = get_viewport().get_visible_rect().size
	var short_edge := minf(_viewport_size.x, _viewport_size.y)
	var scale := clampf(short_edge / 1100.0, 0.62, 1.7)
	var gutter := roundf(30.0 * scale)

	for label in [_status, _position_value, _velocity_value, _support_value,
			_traversal_value, _machine_value, _plant_value, _tick_value]:
		if label == null:
			continue
		var base := 28.0 if label == _status else 16.0
		label.add_theme_font_size_override("font_size", int(roundf(base * scale)))

	var top_left: Control = $HUD/TopLeft
	top_left.offset_left = gutter
	top_left.offset_top = gutter
	top_left.offset_right = gutter + _viewport_size.x * 0.52

	var top_right: Control = $HUD/TopRight
	top_right.offset_left = -_viewport_size.x * 0.44
	top_right.offset_top = gutter
	top_right.offset_right = -gutter

	var pad_size := roundf(206.0 * scale)
	var pad: Control = $HUD/TouchMove
	pad.offset_left = gutter + 12.0
	pad.offset_right = pad.offset_left + pad_size
	pad.offset_bottom = -(gutter + 12.0)
	pad.offset_top = pad.offset_bottom - pad_size

	for button in [_action_button, _release_button]:
		if button == null:
			continue
		button.offset_right = -(gutter + 12.0)
		button.offset_left = button.offset_right - roundf(184.0 * scale)


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

	_position_value.text = "POSITION  %8.2f %7.2f %8.2f m" % [position.x, position.y, position.z]
	_velocity_value.text = "VELOCITY  %8.2f %7.2f %8.2f m/s" % [velocity.x, velocity.y, velocity.z]
	_support_value.text = "SUPPORT   %s / E%04d / POINT V %5.2f %5.2f %5.2f" % [
		"GROUNDED" if grounded else "AIRBORNE", support,
		support_velocity.x, support_velocity.y, support_velocity.z]
	_support_value.modulate = Color("d9c08a") if grounded else Color("b4742c")
	_traversal_value.text = "TRAVERSAL %s / E%04d / %3d%%   LEDGE %s" % [
		_traversal_name(traversal),
		int(_native.get_traversal_support_entity_id()),
		int(round(float(_native.get_traversal_progress()) * 100.0)),
		_ledge_affordance_text()]

	var valve := float(_native.get_valve_open_fraction())
	var flow := float(_native.get_orifice_mass_flow_kg_per_s())
	_machine_value.text = "PLANT     CYCLE %5.1fs  TIPPER %+6.3f rad  VALVE %3d%%  FLOW %5.3f kg/s" % [
		float(_native.get_machine_cycle_phase_seconds()),
		float(_native.get_tipper_angle_radians()), int(round(valve * 100.0)), flow]
	_plant_value.text = "VESSEL    %5.2f bar  CYL %5.2f bar  PISTON %6.2f kN  STORE %5.2f MJ  LIFT %5.2f m" % [
		float(_native.get_vessel_pressure_pa()) / 1.0e5,
		float(_native.get_cylinder_pressure_pa()) / 1.0e5,
		float(_native.get_piston_force_n()) / 1000.0,
		float(_native.get_vessel_available_energy_j()) / 1.0e6,
		float(_native.get_lift_platform_position().y)]
	_tick_value.text = "90 HZ NATIVE  /  TICK %08d  /  TOWER %.0f m" % [
		int(_native.get_tick_index()), float(_native.get_tower_height_meters())]

	var fall_state := int(_native.get_fall_state())
	_fall_value.text = "FALL      %s  PEAK %5.1f m/s  CHUTE %s  CHECKPOINT %6.2f %5.2f %6.2f  COMMITS %d  DEATHS %d" % [
		_fall_state_name(fall_state),
		float(_native.get_fall_peak_speed_mps()),
		"DEPLOYED" if bool(_native.is_parachute_deployed()) else "stowed",
		_native.get_checkpoint_position().x, _native.get_checkpoint_position().y,
		_native.get_checkpoint_position().z,
		int(_native.get_checkpoint_commit_count()), int(_native.get_death_count())]
	_fall_value.modulate = Color("8fd9b8") if fall_state == FALL_PARACHUTING else Color("d8e0dc")

	var jib_at_station := bool(_native.is_jib_station_active())
	var jib_hook: Vector3 = _native.get_jib_hook_position()
	_jib_value.text = "JIB       %s  BOOM %+6.3f rad  HOOK %6.2f %5.2f %6.2f  CRATE %6.2f %5.2f %6.2f" % [
		"AT PENDANT" if jib_at_station else "away",
		float(_native.get_jib_boom_angle_radians()),
		jib_hook.x, jib_hook.y, jib_hook.z,
		_native.get_jib_crate_position().x, _native.get_jib_crate_position().y,
		_native.get_jib_crate_position().z]
	_jib_value.modulate = Color("e8d9a8") if jib_at_station else Color("8a8378")

	var needle_at_station := bool(_native.is_needle_station_active())
	var needle_seated := bool(_native.is_needle_seated())
	var needle_pos: Vector3 = _native.get_needle_position()
	_needle_value.text = "NEEDLE    %s  %s  POS %6.2f %5.2f %6.2f" % [
		"AT PENDANT" if needle_at_station else "away",
		"SEATED" if needle_seated else "unseated",
		needle_pos.x, needle_pos.y, needle_pos.z]
	_needle_value.modulate = Color("9ad6c4") if needle_seated else Color("8a8378")

	var sump_at_station := bool(_native.is_sump_station_active())
	var grate_safe := bool(_native.is_grate_safe())
	_sump_value.text = "SUMP      %s  VALVE %s  VOLUME %6.1f kg  GRATE %s" % [
		"AT VALVE" if sump_at_station else "away",
		"closed" if bool(_native.is_sump_isolated()) else "OPEN",
		float(_native.get_sump_volume_kg()),
		"safe" if grate_safe else "HAZARD"]
	_sump_value.modulate = Color("9ad6c4") if grate_safe else Color("d99a4a")

	if traversal == TRAVERSAL_HANGING:
		_status.text = "HANGING ON NATIVE LEDGE"
	elif traversal == TRAVERSAL_MANTLING:
		_status.text = "MANTLING REAL GEOMETRY"
	elif traversal == TRAVERSAL_VAULTING:
		_status.text = "VAULTING REAL GEOMETRY"
	elif jib_at_station:
		_status.text = "AT THE JIB PENDANT / ARROWS DRIVE-HOIST"
	elif needle_at_station:
		_status.text = "AT THE NEEDLE PENDANT / ARROWS RAISE-LOWER"
	elif sump_at_station:
		_status.text = "AT THE SUMP VALVE / V TO ISOLATE"
	elif grounded and support == CRATE_ENTITY_ID:
		_status.text = "RIDING THE CRATE"
	elif grounded and support == NEEDLE_BEAM_ENTITY_ID:
		_status.text = "ON THE SEATED NEEDLE"
	elif grounded and support == SUMP_GRATE_ENTITY_ID:
		_status.text = "ON THE DRAINED GRATE"
	elif grounded and support == LIFT_PLATFORM_ENTITY_ID:
		_status.text = "RIDING THE STEAM LIFT"
	elif grounded and support == CATWALK_ENTITY_ID:
		_status.text = "ON THE CATWALK"
	elif grounded and support == TREADLE_ENTITY_ID:
		_status.text = "ON THE TREADLE / VALVE HELD OPEN"
	elif grounded and support == TIPPER_ENTITY_ID:
		_status.text = "STANDING ON THE TIPPER"
	elif grounded and support == TRANSLATING_SUPPORT_ENTITY_ID:
		_status.text = "NATIVE MOVING SUPPORT ONLINE"
	elif grounded:
		_status.text = "AT GRADE"
	elif fall_state == FALL_PARACHUTING:
		_status.text = "PARACHUTE DEPLOYED"
	else:
		_status.text = "AIRBORNE / MOMENTUM PRESERVED"

	_mirror_machine(valve, flow)


func _mirror_machine(_valve: float, flow: float) -> void:
	if _translating_support_mesh != null:
		_translating_support_mesh.position = _native.get_translating_support_position()
	if _rotating_support_mesh != null:
		_rotating_support_mesh.position = _native.get_rotating_support_position()
		_rotating_support_mesh.rotation = Vector3(0.0, float(_native.get_rotating_support_yaw_radians()), 0.0)
	if _moving_ledge_mesh != null:
		_moving_ledge_mesh.position = _native.get_moving_ledge_position()

	var scoop_origin: Vector3 = _native.get_hoist_scoop_position()
	var scoop_tilt := float(_native.get_hoist_scoop_tilt_radians())
	var scoop_basis := Basis(Vector3(0.0, 0.0, 1.0), scoop_tilt)
	for index in _scoop_meshes.size():
		var mesh := _scoop_meshes[index]
		mesh.position = scoop_origin + scoop_basis * _scoop_locals[index]
		mesh.rotation = Vector3(0.0, 0.0, scoop_tilt)

	if _ballast_mesh != null:
		_ballast_mesh.position = _native.get_ballast_position()
	if _tipper_mesh != null:
		_tipper_mesh.position = _native.get_tipper_position()
		_tipper_mesh.rotation = Vector3(0.0, 0.0, float(_native.get_tipper_angle_radians()))
	if _valve_mesh != null:
		_valve_mesh.rotation = Vector3(0.0, 0.0, float(_native.get_valve_lever_angle_radians()))
	if _treadle_mesh != null:
		var treadle_angle := float(_native.get_treadle_angle_radians())
		_treadle_mesh.rotation = Vector3(0.0, 0.0, treadle_angle)
		# The catwalk-side cable pays out as the pedal swings, exactly as the
		# native pulley sees it.
		var cable_anchor := Vector3(15.7, 9.09, -106.0)
		cable_anchor.y -= sin(treadle_angle) * 0.70
		_span_cable(_treadle_cable_a, cable_anchor, Vector3(15.7, 11.09, -105.0))
	if _lift_mesh != null:
		_lift_mesh.position = _native.get_lift_platform_position()
	if _counterweight_mesh != null:
		_counterweight_mesh.position = _native.get_counterweight_position()

	if _jib_boom_mesh != null:
		_jib_boom_mesh.rotation = Vector3(0.0, float(_native.get_jib_boom_angle_radians()), 0.0)
	if _jib_hook_mesh != null:
		_jib_hook_mesh.position = _native.get_jib_hook_position()
	if _jib_crate_mesh != null:
		_jib_crate_mesh.position = _native.get_jib_crate_position()
	if _jib_capacity_load_mesh != null:
		_jib_capacity_load_mesh.position = _native.get_jib_capacity_stand_load_position()
	if _jib_hoist_cable != null:
		_span_cable(_jib_hoist_cable, _native.get_jib_hook_position() + Vector3(0.0, 0.15, 0.0),
			Vector3(200.0, 5.0, 0.0) + Vector3(6.0, 0.0, 0.0).rotated(
				Vector3.UP, float(_native.get_jib_boom_angle_radians())))

	if _needle_beam_mesh != null:
		_needle_beam_mesh.position = _native.get_needle_position()
	if _needle_hoist_cable != null:
		_span_cable(_needle_hoist_cable,
			_native.get_needle_position() + Vector3(0.0, 0.18, 0.0), Vector3(200.0, 7.0, -16.0))

	if _sump_grate_mesh != null:
		var grate_safe := bool(_native.is_grate_safe())
		_sump_grate_mesh.mesh.material = (
			_sump_grate_safe_material if grate_safe else _sump_grate_hazard_material)

	if _rope_mesh != null:
		var from: Vector3 = _native.get_tipper_position() + Vector3(3.0, -0.2, 0.0).rotated(
			Vector3(0.0, 0.0, 1.0), float(_native.get_tipper_angle_radians()))
		var to := Vector3(29.6, 7.2, -93.0) + Vector3(-1.6, 0.0, 0.0).rotated(
			Vector3(0.0, 0.0, 1.0), float(_native.get_valve_lever_angle_radians()))
		var span := to - from
		var length := span.length()
		if length > 0.05:
			_rope_mesh.position = from + span * 0.5
			_rope_mesh.look_at_from_position(from + span * 0.5, to, Vector3.UP, true)
			_rope_mesh.scale = Vector3(1.0, 1.0, length)

	# The plume is the only "simulated-looking" effect in this file, and it is a
	# pure function of the native orifice mass flow. Shut the valve and it stops:
	# it has no clock of its own.
	if _plume != null:
		var flow_ratio := clampf(flow / PLUME_REFERENCE_FLOW, 0.0, 1.0)
		_plume.emitting = flow > 0.0005
		_plume.initial_velocity_min = 1.5 + 6.0 * flow_ratio
		_plume.initial_velocity_max = 3.0 + 15.0 * flow_ratio
		_plume.scale_amount_min = 0.9 + 1.4 * flow_ratio
		_plume.scale_amount_max = 1.8 + 3.6 * flow_ratio
		if _plume_material != null:
			_plume_material.albedo_color = Color(0.80, 0.78, 0.74, 0.05 + 0.30 * flow_ratio)
	if _vent_light != null:
		_vent_light.light_energy = 1.2 + 9.0 * clampf(flow / PLUME_REFERENCE_FLOW, 0.0, 1.0)
	if _fire_box != null:
		var charge := clampf(float(_native.get_vessel_pressure_pa()) / 4.6e5, 0.0, 1.0)
		_fire_box.light_energy = 1.6 + 5.4 * (1.0 - charge)

	# Decorative face gear/drum: driven one-way from the real cycle phase so it
	# reads as the plant's visible mechanism. It owns no state and is never
	# read back -- freezing machine_cycle_phase_seconds freezes it too.
	var phase := float(_native.get_machine_cycle_phase_seconds())
	var gear_angle := (phase / KELLERWORKS_CYCLE_PERIOD_SECONDS) * TAU
	if _gear_pivot != null:
		_gear_pivot.rotation = Vector3(0.0, 0.0, gear_angle)
	if _drum_pivot != null:
		_drum_pivot.rotation = Vector3(gear_angle * 1.6, 0.0, 0.0)


func _fall_state_name(fall_state: int) -> String:
	match fall_state:
		FALL_PARACHUTING:
			return "PARACHUTING"
		FALL_AIRBORNE:
			return "AIRBORNE   "
		_:
			return "GROUNDED   "


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


func _update_ambient_dressing() -> void:
	# Clock-driven crane sway only. This is explicitly weather-class ambient
	# motion (GDD term for non-authoritative background movement), never
	# claimed as simulated rigging -- native-authoritative freight is a later
	# work order (TDD section 9). Nothing here is queried by any other system.
	if _crane_boom == null:
		return
	var sway := sin(_ambient_clock * 0.18) * 0.035
	_crane_boom.rotation = Vector3(0.0, 0.0, sway)
	if _crane_hook != null:
		_crane_hook.position.y = -13.0 + sin(_ambient_clock * 0.5) * 0.25
	# Stack gearing turns on the real native machine phase, like the yard gear
	# motif -- a visible face of an authoritative value, never its own clock.
	var phase := float(_native.get_machine_cycle_phase_seconds()) if _native != null else 0.0
	for index in _stack_gears.size():
		var direction := 1.0 if index % 2 == 0 else -1.0
		_stack_gears[index].rotation = Vector3(0.0, 0.0,
			direction * phase * TAU / KELLERWORKS_CYCLE_PERIOD_SECONDS)


# --- world ------------------------------------------------------------------
#
# Industrial palette: mill scale, oxidised steel, poured concrete, wet asphalt,
# galvanised mesh, faded warning yellow, chipped hazard orange. Nothing here
# emits light except a sodium fitting, a fire box, or a vent -- colour is a
# consequence of material and weather, not a shader preset.


func _build_world() -> void:
	_build_bump_textures()

	# Palette: oxidised iron and rust carry the structure, weathered timber
	# softens it, mill scale is the dark shadow value, crane yellow and brass
	# lamplight are the warm accents -- and now verdigris copper, painted
	# machinery blue, and lichen staining break the rust/iron monochrome, the
	# way a real decades-old industrial site actually weathers.
	var asphalt := _material(Color("17150f"), 0.06, 0.4)
	var concrete := _material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _bump_concrete)
	# A real value ladder: near-black iron in shadow, mid rust for the frame,
	# brighter oxide only where light catches an edge. Bump-mapped: these
	# cover most of the structure's surface area, so this is where per-pixel
	# normal detail matters most.
	var mill_scale := _material(Color("1d1a17"), 0.72, 0.6, Color.BLACK, 1.0, _bump_steel)
	var oxidised := _material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _bump_steel)
	var rust_deep := _material(Color("3b1f13"), 0.28, 0.95, Color.BLACK, 1.0, _bump_steel)
	var rust_bright := _material(Color("9a5326"), 0.34, 0.82, Color.BLACK, 1.0, _bump_steel)
	var galvanised := _material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _bump_steel)
	var faded_yellow := _material(Color("b08a22"), 0.16, 0.68)
	var hazard := _material(Color("a04d16"), 0.18, 0.76)
	var tar := _material(Color("0e0f11"), 0.05, 0.62)
	var timber := _material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _bump_timber)
	# New accents: living colour against the rust.
	var verdigris := _material(Color("3f6b5c"), 0.42, 0.68, Color.BLACK, 1.0, _bump_steel)
	var machine_blue := _material(Color("29455c"), 0.22, 0.6, Color.BLACK, 1.0, _bump_steel)
	var lichen := _material(Color("57642e"), 0.0, 0.96)

	# Grade and the tower's upper mass: sizes mirror the native Jolt bodies.
	_add_box("Grade", Vector3(480.0, 1.0, 480.0), Vector3(0.0, -0.5, -60.0), asphalt)
	var mass_half := 800.0 - STACK_MASS_BASE_Y * 0.5
	_add_box("TowerMass", Vector3(92.0, mass_half * 2.0, 80.0),
		Vector3(-30.0, STACK_MASS_BASE_Y + mass_half, -330.0), concrete)

	_build_stack(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded_yellow, timber)
	_build_stack_accents(verdigris, machine_blue, lichen)
	_build_tower_skin(mill_scale, oxidised, galvanised, faded_yellow, timber)
	_build_yard(concrete, mill_scale, faded_yellow, tar)
	_build_legacy_fixtures(mill_scale, galvanised, hazard, faded_yellow)
	_build_plant(mill_scale, oxidised, galvanised, hazard, faded_yellow)
	_build_mountains_and_waterfall()
	_build_kellerworks_signage(timber, faded_yellow)
	_build_gear_motif(mill_scale, oxidised)
	_build_crane(mill_scale, hazard)
	_build_foliage()
	_build_sky_shear()
	_build_lighting()


# The stack: the tower's climbable lower section. Deck rings, columns and
# stair flights mirror real native collision one-for-one; bracing, rails,
# steps, pipework and lamps are dressing hung on that frame. The player is
# inside this structure, so it is built to be seen from within as well as
# from the yard.
func _build_stack(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var band_center := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH * 0.5
	var inner_half := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z

	for level in range(1, STACK_LEVEL_COUNT + 1):
		var deck_y := float(level) * STACK_LEVEL_HEIGHT
		var slab_y := deck_y - STACK_DECK_THICKNESS * 0.5
		var deck_material: Material = galvanised if level % 2 == 1 else mill_scale

		for sz in [1.0, -1.0]:
			_add_box("StackDeck", Vector3(STACK_HALF_EXTENT * 2.0, STACK_DECK_THICKNESS,
				STACK_DECK_BAND_DEPTH), Vector3(cx, slab_y, cz + sz * band_center), deck_material)
		for sx in [1.0, -1.0]:
			_add_box("StackDeck", Vector3(STACK_DECK_BAND_DEPTH, STACK_DECK_THICKNESS,
				inner_half * 2.0), Vector3(cx + sx * band_center, slab_y, cz), deck_material)

		# Edge beams around the shaft and the outer face: the structure reads
		# as fabricated plate girders, not floating slabs.
		for sz in [1.0, -1.0]:
			_add_box("ShaftEdgeBeam", Vector3(inner_half * 2.0, 0.9, 0.5),
				Vector3(cx, deck_y - 0.55, cz + sz * inner_half), oxidised)
			_add_box("OuterEdgeBeam", Vector3(STACK_HALF_EXTENT * 2.0, 1.1, 0.6),
				Vector3(cx, deck_y - 0.7, cz + sz * STACK_HALF_EXTENT), rust_deep)
		for sx in [1.0, -1.0]:
			_add_box("ShaftEdgeBeam", Vector3(0.5, 0.9, inner_half * 2.0),
				Vector3(cx + sx * inner_half, deck_y - 0.55, cz), oxidised)
			_add_box("OuterEdgeBeam", Vector3(0.6, 1.1, STACK_HALF_EXTENT * 2.0),
				Vector3(cx + sx * STACK_HALF_EXTENT, deck_y - 0.7, cz), rust_deep)

		# Handrails around the open shaft -- the safety line you walk beside.
		for sz in [1.0, -1.0]:
			_add_box("ShaftRail", Vector3(inner_half * 2.0, 0.08, 0.08),
				Vector3(cx, deck_y + 1.05, cz + sz * inner_half), galvanised)
		for sx in [1.0, -1.0]:
			_add_box("ShaftRail", Vector3(0.08, 0.08, inner_half * 2.0),
				Vector3(cx + sx * inner_half, deck_y + 1.05, cz), galvanised)
		for post_x in [-inner_half, -inner_half * 0.5, 0.0, inner_half * 0.5, inner_half]:
			for sz in [1.0, -1.0]:
				_add_box("ShaftPost", Vector3(0.09, 1.1, 0.09),
					Vector3(cx + post_x, deck_y + 0.55, cz + sz * inner_half), galvanised)

		# Timber decking planks laid over the walking band, warm against iron.
		for plank in range(-2, 3):
			for sz in [1.0, -1.0]:
				_add_box("DeckPlank", Vector3(STACK_HALF_EXTENT * 2.0 - 2.0, 0.08, 1.1),
					Vector3(cx, deck_y + 0.05, cz + sz * (band_center + float(plank) * 1.35)),
					timber)

	# Columns, and the diagonal bracing that makes a frame a frame.
	for level in range(0, STACK_LEVEL_COUNT):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5
		var brace_length := sqrt(pow(STACK_LEVEL_HEIGHT, 2.0) + pow(STACK_HALF_EXTENT, 2.0))
		var brace_pitch := atan2(STACK_LEVEL_HEIGHT, STACK_HALF_EXTENT)

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y,
					cz + sz * STACK_HALF_EXTENT), rust_deep)
			_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y, cz), rust_deep)
			_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx, mid_y, cz + sx * STACK_HALF_EXTENT), rust_deep)

		# Cross bracing on all four outer faces.
		for sz in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace := _add_box("StackBrace", Vector3(brace_length, 0.45, 0.45),
					Vector3(cx + direction * STACK_HALF_EXTENT * 0.5, mid_y,
						cz + sz * STACK_HALF_EXTENT), oxidised)
				brace.rotation = Vector3(0.0, 0.0, direction * brace_pitch)
		for sx in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace_z := _add_box("StackBrace", Vector3(0.45, 0.45, brace_length),
					Vector3(cx + sx * STACK_HALF_EXTENT, mid_y,
						cz + direction * STACK_HALF_EXTENT * 0.5), oxidised)
				brace_z.rotation = Vector3(-direction * brace_pitch, 0.0, 0.0)

	# Stair flights: the inclined slab is the native collision, the treads and
	# stringers are drawn on top of it so the two agree.
	for level in range(0, STACK_LEVEL_COUNT):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var run := STACK_HALF_EXTENT * 2.0 - STACK_DECK_BAND_DEPTH * 2.0
		var rise := STACK_LEVEL_HEIGHT
		var length := sqrt(run * run + rise * rise)
		var pitch := atan2(rise, run)
		var side := 1.0 if level % 2 == 0 else -1.0
		var flight_origin := Vector3(cx, base_y + rise * 0.5, cz + side * band_center)

		var flight := _add_box("StairFlight", Vector3(length, 0.36, STACK_RAMP_WIDTH),
			flight_origin, mill_scale)
		flight.rotation = Vector3(0.0, 0.0, side * pitch)

		var tread_count := 14
		for step in range(tread_count):
			var t := (float(step) + 0.5) / float(tread_count) - 0.5
			var along := t * length
			var step_position := flight_origin + Vector3(
				along * cos(side * pitch), along * sin(side * pitch), 0.0)
			_add_box("StairTread", Vector3(length / float(tread_count) * 0.86, 0.1,
				STACK_RAMP_WIDTH * 0.94), step_position + Vector3(0.0, 0.26, 0.0), galvanised)
		for rail_side in [1.0, -1.0]:
			var stringer := _add_box("StairStringer", Vector3(length, 0.9, 0.12),
				flight_origin + Vector3(0.0, 0.5, rail_side * STACK_RAMP_WIDTH * 0.5),
				rust_bright)
			stringer.rotation = Vector3(0.0, 0.0, side * pitch)

	_build_stack_dressing(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded)
	_build_stack_megastructure(mill_scale, oxidised, rust_deep, rust_bright, galvanised,
		faded, timber)


# Everything that makes the frame read as one vast working plant rather than
# a repeated scaffold: splayed footings, clad machine halls with lit windows,
# exposed gearing, lift cages on their guide rails, jib cranes with loads
# hanging off them, company signage, and walkways striking out into the air.
# None of it is collision or authority -- it is filler in the honest sense,
# structure whose job is scale and density.
func _build_stack_megastructure(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var banner_cloth := _material(Color("5e2220"), 0.0, 0.95)
	var crane_yellow := _material(Color("b8862a"), 0.26, 0.58)
	var sign_plate := _material(Color("46423b"), 0.2, 0.86)
	var window_lit := _material(Color("2a2118"), 0.1, 0.7, Color("ffb45c"), 2.8)
	var cage_yellow := _material(Color("94701f"), 0.3, 0.6)

	_build_stack_footings(rust_deep, oxidised, mill_scale)
	_build_stack_halls(timber, mill_scale, rust_deep, window_lit)
	_build_stack_gearworks(rust_deep, oxidised, mill_scale)
	_build_stack_lifts(cage_yellow, galvanised, mill_scale, window_lit)
	_build_stack_jibs(crane_yellow, mill_scale, timber, galvanised)
	_build_stack_signage(banner_cloth, sign_plate)
	_build_stack_bridges(galvanised, faded, rust_deep, mill_scale)


# Splayed footings. The references all plant their towers on legs that kick
# out well past the shaft, which is what gives them their sense of weight.
func _build_stack_footings(rust_deep: Material, oxidised: Material, mill_scale: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var reach := STACK_HALF_EXTENT + 13.0
	var meet_y := STACK_LEVEL_HEIGHT * 3.0

	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			var foot := Vector3(cx + sx * reach, 0.0, cz + sz * reach)
			var head := Vector3(cx + sx * STACK_HALF_EXTENT, meet_y, cz + sz * STACK_HALF_EXTENT)
			_add_strut("Buttress", foot, head, 2.4, rust_deep)
			_add_box("ButtressFoot", Vector3(6.0, 2.6, 6.0),
				foot + Vector3(0.0, 1.3, 0.0), mill_scale)
			# Secondary tie back into the frame, one storey down.
			var tie_foot := foot.lerp(head, 0.42)
			var tie_head := Vector3(cx + sx * STACK_HALF_EXTENT, STACK_LEVEL_HEIGHT,
				cz + sz * STACK_HALF_EXTENT)
			_add_strut("ButtressTie", tie_foot, tie_head, 1.1, oxidised)


# Clad machine halls bolted onto the frame: solid volumes with lit windows,
# so the tower is not uniformly see-through and has interior worth reading.
func _build_stack_halls(timber: Material, mill_scale: Material, rust_deep: Material,
		window_lit: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var halls := [
		{"level": 3, "sx": -1.0, "storeys": 2.0, "depth": 13.0},
		{"level": 7, "sx": 1.0, "storeys": 3.0, "depth": 11.0},
		{"level": 11, "sx": -1.0, "storeys": 2.0, "depth": 12.0},
	]
	for hall in halls:
		var level: float = hall["level"]
		var sx: float = hall["sx"]
		var storeys: float = hall["storeys"]
		var depth: float = hall["depth"]
		var height := storeys * STACK_LEVEL_HEIGHT
		var width := 9.0
		var centre := Vector3(cx + sx * (STACK_HALF_EXTENT + width * 0.5 - 1.0),
			level * STACK_LEVEL_HEIGHT + height * 0.5 - 1.0, cz)

		_add_box("MachineHall", Vector3(width, height, depth), centre, timber)
		_add_box("HallCapping", Vector3(width + 1.2, 0.9, depth + 1.2),
			centre + Vector3(0.0, height * 0.5 + 0.3, 0.0), rust_deep)
		_add_box("HallSill", Vector3(width + 1.0, 0.8, depth + 1.0),
			centre - Vector3(0.0, height * 0.5 + 0.2, 0.0), mill_scale)
		# Window grid on the outward face and the front face.
		for row in range(int(storeys) * 2):
			for column in range(3):
				var wy := centre.y - height * 0.5 + 2.6 + float(row) * 4.4
				var wz := cz - depth * 0.5 + 2.6 + float(column) * (depth - 5.2) * 0.5
				_add_box("HallWindow", Vector3(0.4, 1.9, 1.5),
					Vector3(centre.x + sx * (width * 0.5 + 0.1), wy, wz), window_lit)
			_add_box("HallWindowFront", Vector3(2.2, 1.9, 0.4),
				Vector3(centre.x, centre.y - height * 0.5 + 3.4 + float(row) * 4.4,
					cz + depth * 0.5 + 0.1), window_lit)
		# Ribs, so the cladding reads as boards on a frame.
		for rib in range(5):
			_add_box("HallRib", Vector3(0.5, height, 0.5),
				Vector3(centre.x - width * 0.5 + 0.5 + float(rib) * (width - 1.0) * 0.25,
					centre.y, cz + depth * 0.5 + 0.2), rust_deep)


# Exposed gearing on the front face, big enough to read from the yard.
func _build_stack_gearworks(rust_deep: Material, oxidised: Material, mill_scale: Material) -> void:
	var cx := STACK_CENTER.x
	var front_z := STACK_CENTER.z + STACK_HALF_EXTENT
	var specs = [
		{"at": Vector3(cx - 10.0, STACK_LEVEL_HEIGHT * 2.4, front_z + 1.4), "r": 7.4, "t": 22},
		{"at": Vector3(cx + 2.0, STACK_LEVEL_HEIGHT * 3.6, front_z + 1.0), "r": 4.6, "t": 16},
		{"at": Vector3(cx - 6.0, STACK_LEVEL_HEIGHT * 6.3, front_z + 1.4), "r": 6.2, "t": 20},
		{"at": Vector3(cx + 8.0, STACK_LEVEL_HEIGHT * 9.4, front_z + 1.2), "r": 5.4, "t": 18},
	]
	for index in specs.size():
		var spec = specs[index]
		var gear_at: Vector3 = spec["at"]
		var gear_radius: float = spec["r"]
		var gear_teeth: int = spec["t"]
		var gear := _add_gear("StackGearwheel", gear_at, gear_radius, gear_teeth,
			rust_deep, oxidised)
		_stack_gears.append(gear)
		# The shaft it turns on, driven back into the frame.
		var shaft := _add_cylinder("GearShaft", gear_radius * 0.16, 3.2,
			gear_at - Vector3(0.0, 0.0, 1.6), mill_scale)
		shaft.rotation = Vector3(PI * 0.5, 0.0, 0.0)

	# Winch drums with cable wound on them, paired with the gearing.
	for drum in [Vector3(cx + 9.0, STACK_LEVEL_HEIGHT * 4.5, front_z - 1.0),
			Vector3(cx - 11.0, STACK_LEVEL_HEIGHT * 8.4, front_z - 1.0)]:
		var barrel := _add_cylinder("WinchDrum", 1.9, 7.0, drum, mill_scale)
		barrel.rotation = Vector3(0.0, 0.0, PI * 0.5)
		for band in range(7):
			var ring := _add_cylinder("DrumCable", 2.05, 0.5,
				drum + Vector3(-2.6 + float(band) * 0.9, 0.0, 0.0), rust_deep)
			ring.rotation = Vector3(0.0, 0.0, PI * 0.5)
		_add_box("DrumHousing", Vector3(1.4, 3.4, 3.4), drum + Vector3(4.4, 0.0, 0.0), rust_deep)


# Lift cages running in guide rails up the front of the shaft.
func _build_stack_lifts(cage_yellow: Material, galvanised: Material, mill_scale: Material,
		window_lit: Material) -> void:
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT
	var top_y := STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT)

	for shaft_index in range(2):
		var lift_x := STACK_CENTER.x + (6.5 if shaft_index == 0 else -14.5)
		# Paired guide rails, full height.
		for rail in [-1.6, 1.6]:
			_add_box("LiftGuide", Vector3(0.45, top_y, 0.45),
				Vector3(lift_x + rail, top_y * 0.5, front_z + 1.1), galvanised)
		_add_box("LiftHead", Vector3(5.4, 2.2, 3.2), Vector3(lift_x, top_y + 1.0, front_z + 1.1),
			mill_scale)

		var cage_y: float = STACK_LEVEL_HEIGHT * (4.5 if shaft_index == 0 else 8.5)
		# Hoist rope from the head down to the cage.
		_add_box("LiftRope", Vector3(0.12, top_y - cage_y, 0.12),
			Vector3(lift_x, (top_y + cage_y) * 0.5, front_z + 1.1), mill_scale)

		var cage := Node3D.new()
		cage.name = "LiftCage"
		cage.position = Vector3(lift_x, cage_y, front_z + 1.1)
		$TowerPresentation.add_child(cage)
		_add_box_to("CageFloor", Vector3(4.0, 0.3, 3.0), Vector3(0.0, -1.7, 0.0), mill_scale, cage)
		_add_box_to("CageRoof", Vector3(4.0, 0.3, 3.0), Vector3(0.0, 1.7, 0.0), cage_yellow, cage)
		for corner_x in [-1.85, 1.85]:
			for corner_z in [-1.35, 1.35]:
				_add_box_to("CagePost", Vector3(0.24, 3.4, 0.24),
					Vector3(corner_x, 0.0, corner_z), cage_yellow, cage)
		_add_box_to("CageBack", Vector3(4.0, 3.0, 0.16), Vector3(0.0, 0.0, -1.4),
			galvanised, cage)
		_add_box_to("CageLamp", Vector3(1.4, 0.3, 1.0), Vector3(0.0, 1.35, 0.0), window_lit, cage)
		_add_sign_text(str(shaft_index + 3), Vector3(0.0, 0.4, 1.45), 0.0, 1.1,
			Color("f2e6cf"), cage)

		var cage_light := OmniLight3D.new()
		cage_light.name = "CageLight"
		cage_light.position = cage.position
		cage_light.light_color = Color(1.0, 0.74, 0.42)
		cage_light.light_energy = 3.0
		cage_light.omni_range = 13.0
		_light_rig.add_child(cage_light)


# Jib cranes reaching off the tower with loads on the hook, at three heights.
func _build_stack_jibs(crane_yellow: Material, mill_scale: Material, timber: Material,
		galvanised: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var jibs = [
		{"y": STACK_LEVEL_HEIGHT * 5.0, "sx": 1.0, "reach": 26.0, "drop": 13.0},
		{"y": STACK_LEVEL_HEIGHT * 9.0, "sx": -1.0, "reach": 22.0, "drop": 17.0},
		{"y": STACK_LEVEL_HEIGHT * 12.5, "sx": 1.0, "reach": 19.0, "drop": 11.0},
	]
	for jib in jibs:
		var y: float = jib["y"]
		var sx: float = jib["sx"]
		var reach: float = jib["reach"]
		var drop: float = jib["drop"]
		var root := Vector3(cx + sx * (STACK_HALF_EXTENT - 1.0), y, cz + 4.0)
		var tip := Vector3(cx + sx * (STACK_HALF_EXTENT + reach), y + reach * 0.42, cz + 9.0)
		var mast_top := root + Vector3(0.0, 11.0, 0.0)

		# Boom as a shallow lattice: two chords and the zigzag between them.
		_add_strut("JibChord", root, tip, 1.05, crane_yellow)
		var chord_offset := Vector3(0.0, 1.5, 0.0)
		_add_strut("JibChord", root + chord_offset, tip + chord_offset, 0.8, crane_yellow)
		for web in range(7):
			var a := float(web) / 7.0
			var b := (float(web) + 1.0) / 7.0
			_add_strut("JibWeb", root.lerp(tip, a) + chord_offset, root.lerp(tip, b), 0.4,
				crane_yellow)
		# A-frame mast and the tie back to the boom tip.
		_add_strut("JibMast", root, mast_top, 1.2, crane_yellow)
		_add_strut("JibStay", mast_top, tip, 0.35, mill_scale)
		_add_strut("JibBackStay", mast_top,
			Vector3(cx - sx * (STACK_HALF_EXTENT - 2.0), y + 2.0, cz), 0.35, mill_scale)

		# Hook rope and the crate hanging on it.
		var hook := tip - Vector3(0.0, drop, 0.0)
		_add_box("JibRope", Vector3(0.14, drop, 0.14), (tip + hook) * 0.5, mill_scale)
		_add_box("JibBlock", Vector3(1.0, 0.9, 1.0), hook + Vector3(0.0, 0.5, 0.0), mill_scale)
		var crate_size := 3.6
		_add_box("JibLoad", Vector3(crate_size, crate_size, crate_size),
			hook - Vector3(0.0, crate_size * 0.5, 0.0), timber)
		for edge in [-1.0, 1.0]:
			_add_box("JibLoadBand", Vector3(crate_size + 0.2, 0.34, 0.34),
				hook + Vector3(0.0, -crate_size * 0.5, edge * crate_size * 0.5), galvanised)
		# Slings from the block out to the crate corners.
		for corner_x in [-1.0, 1.0]:
			for corner_z in [-1.0, 1.0]:
				_add_strut("JibSling", hook + Vector3(0.0, 0.5, 0.0),
					hook + Vector3(corner_x * crate_size * 0.5, 0.0, corner_z * crate_size * 0.5),
					0.1, mill_scale)


# Company signage: hanging cloth banners and painted plate on the structure.
func _build_stack_signage(banner_cloth: Material, sign_plate: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT
	var emblem := _material(Color("ddd2c0"), 0.0, 0.8)

	var banners = [
		{"x": cx - 15.0, "level": 9.0, "lines": "KELLERWORKS", "sub": "MATERIALS\nMOVE\nCIVILIZATION\nRISES"},
		{"x": cx + 14.0, "level": 5.0, "lines": "", "sub": "PEOPLE\nPOWER\nPROGRESS"},
		{"x": cx - 4.0, "level": 12.0, "lines": "", "sub": "A HIGHER\nWORLD\nTOGETHER"},
	]
	for banner in banners:
		var bx: float = banner["x"]
		var top := float(banner["level"]) * STACK_LEVEL_HEIGHT - 0.8
		var height := 13.0
		var width := 5.0
		var centre := Vector3(bx, top - height * 0.5, front_z + 0.5)
		_add_box("Banner", Vector3(width, height, 0.12), centre, banner_cloth)
		_add_box("BannerRod", Vector3(width + 0.8, 0.22, 0.22),
			centre + Vector3(0.0, height * 0.5 + 0.2, 0.0), sign_plate)
		# Emblem: a canted bar cluster standing in for the company mark.
		for bar in range(2):
			var mark := _add_box("BannerMark", Vector3(2.4, 0.5, 0.06),
				centre + Vector3(0.0, height * 0.5 - 2.0, 0.09), emblem)
			mark.rotation = Vector3(0.0, 0.0, (0.7 if bar == 0 else -0.7))
		if String(banner["lines"]) != "":
			_add_sign_text(String(banner["lines"]),
				centre + Vector3(0.0, height * 0.5 - 4.1, 0.12), 0.0, 0.62, Color("efe5d4"))
		_add_sign_text(String(banner["sub"]),
			centre + Vector3(0.0, height * 0.5 - 7.4, 0.12), 0.0, 0.78, Color("e4d8c4"))

	# Painted plate high on the shaft, the biggest piece of lettering here.
	var plate_centre := Vector3(cx + 6.0, STACK_LEVEL_HEIGHT * 7.6, front_z + 0.45)
	_add_box("SignPlate", Vector3(11.0, 11.0, 0.3), plate_centre, sign_plate)
	for bar in range(2):
		var plate_mark := _add_box("SignPlateMark", Vector3(4.6, 0.9, 0.08),
			plate_centre + Vector3(0.0, 3.4, 0.2), emblem)
		plate_mark.rotation = Vector3(0.0, 0.0, (0.7 if bar == 0 else -0.7))
	_add_sign_text("HIGHER\nSTRONGER\nFURTHER", plate_centre + Vector3(0.0, -1.6, 0.25),
		0.0, 1.5, Color("ded2bd"))

	# Bay lettering down at the loading level, where the player starts.
	var bay_centre := Vector3(cx - 12.0, 6.4, front_z + 0.45)
	_add_box("BayPlate", Vector3(8.0, 9.0, 0.3), bay_centre, sign_plate)
	_add_sign_text("LIFT A", bay_centre + Vector3(0.0, 2.2, 0.25), 0.0, 2.1, Color("e8dcc6"))
	_add_sign_text("TO A HIGHER\nTOMORROW", bay_centre + Vector3(0.0, -1.8, 0.25), 0.0, 0.8,
		Color("cbbfa8"))


# Walkways striking out from the tower toward structures off in the weather.
func _build_stack_bridges(galvanised: Material, faded: Material, rust_deep: Material,
		mill_scale: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var spans = [
		{"y": STACK_LEVEL_HEIGHT * 6.0, "sx": 1.0, "length": 54.0},
		{"y": STACK_LEVEL_HEIGHT * 10.0, "sx": -1.0, "length": 46.0},
	]
	for span in spans:
		var y: float = span["y"]
		var sx: float = span["sx"]
		var length: float = span["length"]
		var from := Vector3(cx + sx * STACK_HALF_EXTENT, y, cz - 3.0)
		var to := from + Vector3(sx * length, -3.0, 0.0)

		_add_box("BridgeDeck", Vector3(length, 0.4, 4.4), (from + to) * 0.5, mill_scale)
		for rail_z in [-2.1, 2.1]:
			_add_box("BridgeRail", Vector3(length, 0.1, 0.1),
				(from + to) * 0.5 + Vector3(0.0, 1.15, rail_z), faded)
			for post in range(int(length / 4.0)):
				_add_box("BridgePost", Vector3(0.12, 1.2, 0.12),
					from + Vector3(sx * (2.0 + float(post) * 4.0), 0.6, rail_z), galvanised)
		# Under-truss, so the span looks like it could carry itself.
		for web in range(int(length / 6.0)):
			var a := float(web) / (length / 6.0)
			var b := (float(web) + 1.0) / (length / 6.0)
			_add_strut("BridgeWeb", from.lerp(to, a) - Vector3(0.0, 0.2, 0.0),
				from.lerp(to, b) - Vector3(0.0, 2.6, 0.0), 0.3, rust_deep)
		_add_strut("BridgeChord", from - Vector3(0.0, 2.6, 0.0), to - Vector3(0.0, 2.6, 0.0),
			0.42, rust_deep)
		# A pylon out at the far end, implying the span lands somewhere.
		_add_box("BridgePylon", Vector3(3.0, y * 0.94, 3.0),
			to + Vector3(sx * 2.0, -y * 0.5, 0.0), rust_deep)


# Colour that isn't rust: verdigris copper pipe runs, blue-painted machinery
# boxes (the ordinary paint colour for real industrial valve gear), and
# lichen staining low on the columns where damp and shade let something
# green actually take hold. A decades-old working plant is never one colour.
func _build_stack_accents(verdigris: Material, machine_blue: Material,
		lichen: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT

	# A verdigris pipe run climbing the front face, distinct from the oxidised
	# risers in the shaft -- copper service lines age to blue-green, not rust.
	for sx in [1.0]:
		_add_cylinder("VerdigrisPipe", 0.4, STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT),
			Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5),
				STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT) * 0.5, front_z + 0.9), verdigris)
		for level in range(1, STACK_LEVEL_COUNT + 1):
			_add_box("VerdigrisFlange", Vector3(1.0, 0.3, 1.0),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5), float(level) * STACK_LEVEL_HEIGHT - 1.2,
					front_z + 0.9), verdigris)

	# Painted machine-blue valve boxes and gauge housings at working levels.
	for level in [2, 5, 8, 11]:
		var y := float(level) * STACK_LEVEL_HEIGHT + 2.0
		var box := _add_box("ValveHousing", Vector3(1.8, 1.4, 1.2),
			Vector3(cx - STACK_HALF_EXTENT + 3.5, y, front_z - 2.0), machine_blue)
		_add_cylinder("ValveWheel", 0.55, 0.22,
			box.position + Vector3(0.0, 0.0, 0.75), machine_blue).rotation = Vector3(PI * 0.5, 0.0, 0.0)

	# Lichen staining low on every column, on the shaded (south) face, and
	# streaking down from every deck's drip line -- damp industrial concrete
	# and iron are never actually clean at the base.
	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			_add_box("ColumnLichen", Vector3(STACK_COLUMN_SIZE + 0.1, 3.5, STACK_COLUMN_SIZE + 0.1),
				Vector3(cx + sx * STACK_HALF_EXTENT, 1.75, cz + sz * STACK_HALF_EXTENT), lichen)
	for level in range(1, 5):
		var y := float(level) * STACK_LEVEL_HEIGHT
		for streak in range(-3, 4):
			_add_box("DeckStreak", Vector3(0.35, 2.4, 0.1),
				Vector3(cx + float(streak) * 4.0, y - 1.2, cz - STACK_HALF_EXTENT - 0.15), lichen)


# Pipework, gearing, vents and lamps hung on the frame. None of this is
# collision or authority -- it is what makes the frame read as a working
# plant rather than a jungle gym.
func _build_stack_dressing(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var inner_half := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH

	# Riser pipes running the full height in the corners of the shaft.
	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			var pipe_material: Material = oxidised if sx * sz > 0.0 else rust_bright
			_add_box("Riser", Vector3(0.7, STACK_LEVEL_HEIGHT * STACK_LEVEL_COUNT, 0.7),
				Vector3(cx + sx * (inner_half - 1.2),
					STACK_LEVEL_HEIGHT * STACK_LEVEL_COUNT * 0.5,
					cz + sz * (inner_half - 1.2)), pipe_material)
			for level in range(1, STACK_LEVEL_COUNT + 1):
				_add_box("RiserFlange", Vector3(1.1, 0.35, 1.1),
					Vector3(cx + sx * (inner_half - 1.2), float(level) * STACK_LEVEL_HEIGHT - 1.4,
						cz + sz * (inner_half - 1.2)), mill_scale)

	# Drive gearing on alternating levels, big enough to read from the yard.
	for level in range(1, STACK_LEVEL_COUNT):
		if level % 2 == 0:
			continue
		var gear_y := float(level) * STACK_LEVEL_HEIGHT + 3.4
		var gear := Node3D.new()
		gear.name = "StackGear"
		gear.position = Vector3(cx - inner_half + 1.0, gear_y, cz - STACK_HALF_EXTENT + 1.2)
		$TowerPresentation.add_child(gear)
		_add_box_to("GearHub", Vector3(1.0, 1.0, 0.8), Vector3.ZERO, rust_deep, gear)
		for tooth in range(12):
			var angle := TAU * float(tooth) / 12.0
			_add_box_to("GearTooth", Vector3(0.7, 0.7, 0.7),
				Vector3(cos(angle) * 2.6, sin(angle) * 2.6, 0.0), oxidised, gear)
		_stack_gears.append(gear)

	# Vent stacks that the plume system can sit on later, plus lamp fittings
	# throwing warm light into the frame.
	var lamp_material := _material(Color("4a3a24"), 0.3, 0.7, Color("ffb04d"), 3.0)
	for level in range(1, STACK_LEVEL_COUNT + 1):
		var y := float(level) * STACK_LEVEL_HEIGHT
		if level % 2 == 0:
			continue
		for sx in [1.0, -1.0]:
			_add_box("VentStack", Vector3(0.8, 2.4, 0.8),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 2.2), y + 1.2,
					cz - STACK_HALF_EXTENT + 2.2), mill_scale)
			_add_box("LampFitting", Vector3(0.5, 0.3, 0.7),
				Vector3(cx + sx * (inner_half - 0.6), y + 2.6, cz), lamp_material)
			var lamp := OmniLight3D.new()
			lamp.name = "StackLamp"
			lamp.position = Vector3(cx + sx * (inner_half - 0.6), y + 2.4, cz)
			lamp.light_color = Color(1.0, 0.72, 0.38)
			lamp.light_energy = 3.2
			lamp.omni_range = 16.0
			lamp.omni_attenuation = 1.5
			_light_rig.add_child(lamp)

	# Hazard striping on the outer edge beams at the lower, most-seen levels.
	for level in range(1, 4):
		var y := float(level) * STACK_LEVEL_HEIGHT
		for stripe in range(-7, 8):
			_add_box("EdgeStripe", Vector3(1.1, 0.5, 0.12),
				Vector3(cx + float(stripe) * 2.3, y - 0.7, cz + STACK_HALF_EXTENT + 0.32),
				faded if stripe % 2 == 0 else mill_scale)


func _build_tower_skin(mill_scale: Material, oxidised: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	# Relief on the neighbouring mass, now set well back across the yard. It is
	# a second Kellerworks shaft receding into the weather, not a lid over the
	# frame the player climbs.
	var mass_x := -30.0
	var face_z := -290.0
	var lit_band := _material(Color("2a2521"), 0.1, 0.8, Color("e0a040"), 0.9)
	var lit_band_dim := _material(Color("242019"), 0.1, 0.8, Color("a86c22"), 0.5)
	for offset in [-38.0, -19.0, 0.0, 19.0, 38.0]:
		_add_box("FacePier", Vector3(6.0, 460.0, 3.0),
			Vector3(mass_x + offset, 230.0, face_z), oxidised)
	for level in range(40, 520, 16):
		_add_box("FaceBand", Vector3(90.0, 1.4, 2.0),
			Vector3(mass_x, float(level), face_z - 0.4), mill_scale)
	for offset in [-30.0, -10.0, 10.0, 30.0]:
		_add_box("FaceDuct", Vector3(3.0, 380.0, 3.0),
			Vector3(mass_x + offset, 200.0, face_z + 1.6), galvanised)
	for level in range(30, 360, 12):
		var band: Material = lit_band if (level / 12) % 3 != 0 else lit_band_dim
		_add_box("FloorLight", Vector3(80.0, 1.0, 0.6),
			Vector3(mass_x, float(level), face_z - 1.3), band)
	for level in range(380, 900, 34):
		_add_box("FloorLightHigh", Vector3(74.0, 0.9, 0.6),
			Vector3(mass_x, float(level), face_z - 1.3), lit_band_dim)
	for offset in [-28.0, -9.0, 9.0, 28.0]:
		_add_box("TimberCladding", Vector3(8.0, 44.0, 1.2),
			Vector3(mass_x + offset, 62.0, face_z + 0.8), timber)

	_build_stack_continuation(oxidised, rust_deepen(oxidised), mill_scale, galvanised, faded)


# The frame does not stop where the player's reach does. Above the climbable
# stack the same columns, decks and bracing carry on for a few hundred metres,
# thinning out as they go, and the environment fog takes them the rest of the
# way. This is what makes the structure read as a skyscraper rather than a
# gantry: there is always more of it above you.
func _build_stack_continuation(oxidised: Material, rust_deep: Material, mill_scale: Material,
		galvanised: Material, faded: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var base_level := STACK_LEVEL_COUNT
	var top_level := STACK_LEVEL_COUNT + 24

	for level in range(base_level, top_level):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5
		# The shaft steps in as it rises, so the silhouette tapers.
		var shrink := 1.0 - float(level - base_level) / float(top_level - base_level) * 0.42
		var half := STACK_HALF_EXTENT * shrink

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_add_box("UpperColumn", Vector3(STACK_COLUMN_SIZE * shrink, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE * shrink),
					Vector3(cx + sx * half, mid_y, cz + sz * half), oxidised)
		# A deck band every other level, and bracing on the faces between.
		if level % 2 == 0:
			for sz in [1.0, -1.0]:
				_add_box("UpperDeck", Vector3(half * 2.0, 0.5, 4.0),
					Vector3(cx, base_y, cz + sz * (half - 2.0)), mill_scale)
			for sx in [1.0, -1.0]:
				_add_box("UpperDeck", Vector3(4.0, 0.5, half * 2.0 - 8.0),
					Vector3(cx + sx * (half - 2.0), base_y, cz), mill_scale)
		var brace_length := sqrt(pow(STACK_LEVEL_HEIGHT, 2.0) + pow(half, 2.0))
		var brace_pitch := atan2(STACK_LEVEL_HEIGHT, half)
		for sz in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace := _add_box("UpperBrace", Vector3(brace_length, 0.4, 0.4),
					Vector3(cx + direction * half * 0.5, mid_y, cz + sz * half), rust_deep)
				brace.rotation = Vector3(0.0, 0.0, direction * brace_pitch)
		if level % 3 == 0:
			_add_box("UpperLightBand", Vector3(half * 2.0, 0.7, 0.4),
				Vector3(cx, base_y + 1.2, cz + half + 0.3), faded)


# Slightly darker variant of a base material, for members that should sit back
# a value step without defining a whole new palette entry.
func rust_deepen(source: Material) -> Material:
	var base := source as StandardMaterial3D
	if base == null:
		return source
	return _material(base.albedo_color.darkened(0.35), base.metallic, base.roughness)


func _build_yard(concrete: Material, mill_scale: Material,
		faded: Material, tar: Material) -> void:
	for z in range(-130, 10, 14):
		_add_box("LaneStripe", Vector3(0.22, 0.02, 7.0), Vector3(-2.0, 0.012, float(z)), faded)
	for z in [-118.0, -60.0, -16.0]:
		_add_box("KerbRun", Vector3(86.0, 0.28, 0.5), Vector3(-6.0, 0.14, z), concrete)
	for x in [-44.0, -20.0]:
		for z in [-124.0, -86.0, -40.0]:
			_add_box("YardCrate", Vector3(4.4, 3.0, 4.4), Vector3(x, 1.5, z), mill_scale)
	_add_box("StandingWater", Vector3(26.0, 0.02, 18.0), Vector3(-24.0, 0.021, -70.0), tar)
	_add_box("StandingWaterTwo", Vector3(18.0, 0.02, 12.0), Vector3(14.0, 0.021, -36.0), tar)


func _build_legacy_fixtures(mill_scale: Material, galvanised: Material,
		hazard: Material, faded: Material) -> void:
	# The WO-001..003 traversal fixtures, re-sited as the loading dock the player
	# starts beside. Native sizes, unchanged.
	_translating_support_mesh = _add_box("NativeTranslatingSupport", Vector3(5.5, 0.5, 5.5), Vector3(0.0, 0.25, 8.0), hazard)
	_rotating_support_mesh = _add_box("NativeRotatingSupport", Vector3(6.0, 0.5, 6.0), Vector3(-8.0, 0.25, 0.0), galvanised)
	_add_box("NativeVaultRail", Vector3(0.44, 0.95, 5.0), Vector3(5.0, 0.475, -6.0), faded)
	_add_box("NativeMantleLedge", Vector3(4.0, 1.55, 4.0), Vector3(11.0, 0.775, -6.0), mill_scale)
	_add_box("NativeHangLedge", Vector3(5.0, 3.6, 5.0), Vector3(11.0, 1.8, 4.0), mill_scale)
	_moving_ledge_mesh = _add_box("NativeMovingLedge", Vector3(4.0, 3.6, 4.0), Vector3(9.0, 1.8, 12.5), galvanised)
	_add_box("NativeBlockedLedge", Vector3(3.0, 1.55, 3.0), Vector3(-6.0, 0.775, -8.0), mill_scale)
	_add_box("NativeBlockedCanopy", Vector3(4.4, 0.3, 4.4), Vector3(-6.0, 2.7, -8.0), faded)


func _build_plant(mill_scale: Material, oxidised: Material, galvanised: Material,
		hazard: Material, faded: Material) -> void:
	# Static plant structure, mirroring the native bodies.
	_add_box("TipperPylon", Vector3(1.1, 2.9, 2.8), Vector3(29.0, 1.45, -96.0), mill_scale)
	_add_box("ValvePylon", Vector3(0.6, 7.2, 0.6), Vector3(29.6, 3.6, -92.25), galvanised)
	_add_box("HoistMast", Vector3(0.8, 12.4, 0.8), Vector3(35.9, 6.2, -96.0), mill_scale)
	_add_box("Vessel", Vector3(3.4, 4.6, 3.4), Vector3(30.5, 2.3, -101.5), oxidised)
	_add_box("LiftMast", Vector3(0.8, 11.2, 0.8), Vector3(13.0, 5.6, -100.0), mill_scale)
	_add_box("Catwalk", Vector3(5.2, 0.28, 18.0), Vector3(16.4, 8.55, -112.0), galvanised)
	_add_box("AccessStepOne", Vector3(1.8, 1.25, 1.6), Vector3(27.0, 0.625, -94.05), mill_scale)
	_add_box("AccessStepTwo", Vector3(1.8, 2.5, 1.6), Vector3(28.3, 1.25, -94.05), mill_scale)
	_add_box("AccessLanding", Vector3(2.0, 0.3, 1.6), Vector3(29.5, 3.65, -94.05), galvanised)
	_add_box("ReturnBasin", Vector3(2.4, 0.28, 3.0), Vector3(31.82, 1.84, -96.0), oxidised, -0.20)
	for guard_z in [-97.55, -94.45]:
		_add_box("BasinGuard", Vector3(2.8, 0.9, 0.24), Vector3(31.82, 2.20, guard_z), faded)

	# Catwalk handrail and sheave head: dressing, deliberately dimmer.
	for rail_x in [13.9, 18.9]:
		_add_box("CatwalkRail", Vector3(0.1, 1.1, 18.0), Vector3(rail_x, 9.2, -112.0), galvanised)
	_add_box("SheaveHead", Vector3(6.4, 0.5, 1.2), Vector3(15.0, 10.4, -100.0), mill_scale)

	# Moving machine parts, each mirroring one authoritative native body.
	_scoop_locals = [
		Vector3(0.0, -0.03, 0.0), Vector3(1.42, 0.70, 0.0),
		Vector3(0.0, 0.70, -1.42), Vector3(0.0, 0.70, 1.42)]
	var scoop_sizes := [
		Vector3(2.6, 0.06, 2.6), Vector3(0.24, 1.4, 2.6),
		Vector3(2.6, 1.4, 0.24), Vector3(2.6, 1.4, 0.24)]
	for index in 4:
		_scoop_meshes.append(_add_box("HoistScoop", scoop_sizes[index],
			Vector3(34.0, 0.03, -96.0) + _scoop_locals[index], oxidised))

	_ballast_mesh = _add_box("Ballast", Vector3(0.84, 0.84, 0.84), Vector3(34.6, 1.0, -96.0), mill_scale)

	_tipper_mesh = Node3D.new()
	_tipper_mesh.name = "Tipper"
	$TowerPresentation.add_child(_tipper_mesh)
	_add_box_to("TipperBeam", Vector3(7.2, 0.4, 2.2), Vector3.ZERO, hazard, _tipper_mesh)
	_add_box_to("TipperBallast", Vector3(1.0, 1.0, 1.0), Vector3(-3.95, -0.30, 0.0), mill_scale, _tipper_mesh)

	_valve_mesh = Node3D.new()
	_valve_mesh.name = "ValveLever"
	_valve_mesh.position = Vector3(29.6, 7.2, -93.0)
	$TowerPresentation.add_child(_valve_mesh)
	_add_box_to("ValveArm", Vector3(1.6, 0.26, 0.4), Vector3(-0.80, 0.0, 0.0), galvanised, _valve_mesh)
	_add_box_to("ValveWeight", Vector3(0.68, 0.68, 0.68), Vector3(0.62, 0.0, 0.0), mill_scale, _valve_mesh)

	_lift_mesh = _add_box("LiftPlatform", Vector3(4.6, 0.32, 4.6), Vector3(16.4, 1.2, -100.0), galvanised)
	_counterweight_mesh = _add_box("Counterweight", Vector3(1.0, 1.8, 1.0), Vector3(11.6, 7.4, -100.0), mill_scale)

	var rope_material := _material(Color("2b2621"), 0.5, 0.7)
	_rope_mesh = _add_box("Rope", Vector3(0.09, 0.09, 1.0), Vector3(30.0, 5.0, -95.0), rope_material)

	_build_treadle(galvanised, mill_scale, hazard, rope_material)
	_build_kernel_jib(galvanised, mill_scale, hazard)
	_build_kernel_needle(galvanised, mill_scale)
	_build_kernel_sump(mill_scale)
	_build_plume()


# WO-010. The plant's human-scale control and the cable run that proves what it
# is wired to. The cable is drawn between the same two sheave points the native
# PulleyConstraint uses, so what the player sees spanning the yard is the actual
# linkage, not a decorative wire.
func _build_treadle(galvanised: StandardMaterial3D, mill_scale: StandardMaterial3D,
		hazard: StandardMaterial3D, cable_material: StandardMaterial3D) -> void:
	_add_box("TreadlePylon", Vector3(0.48, 0.31, 0.68), Vector3(16.4, 8.845, -106.0), mill_scale)

	_treadle_mesh = Node3D.new()
	_treadle_mesh.name = "Treadle"
	_treadle_mesh.position = Vector3(16.4, 9.09, -106.0)
	$TowerPresentation.add_child(_treadle_mesh)
	_add_box_to("TreadlePlate", Vector3(1.5, 0.08, 1.1), Vector3(-0.75, 0.0, 0.0), hazard,
		_treadle_mesh)
	_add_box_to("TreadleWeight", Vector3(0.64, 0.64, 0.64), Vector3(0.55, 0.42, 0.0), mill_scale,
		_treadle_mesh)

	_add_box("TreadleSheaveMast", Vector3(0.2, 2.0, 0.2), Vector3(15.7, 10.3, -105.0), galvanised)
	_add_box("ValveSheaveMast", Vector3(0.2, 2.4, 0.2), Vector3(29.85, 8.4, -92.0), galvanised)

	# Two cable segments, matching the native pulley's two runs.
	_treadle_cable_a = _add_box("TreadleCableA", Vector3(0.06, 0.06, 1.0), Vector3.ZERO,
		cable_material)
	_treadle_cable_b = _add_box("TreadleCableB", Vector3(0.06, 0.06, 1.0), Vector3.ZERO,
		cable_material)
	_span_cable(_treadle_cable_a, Vector3(15.7, 9.09, -106.0), Vector3(15.7, 11.09, -105.0))
	_span_cable(_treadle_cable_b, Vector3(29.85, 7.4, -93.0), Vector3(29.85, 9.6, -92.0))


func _span_cable(node: Node3D, from: Vector3, to: Vector3) -> void:
	if node == null:
		return
	var delta := to - from
	var length := delta.length()
	if length < 0.001:
		return
	node.position = from + delta * 0.5
	node.scale = Vector3(1.0, 1.0, length)
	node.look_at(to, Vector3.UP if absf(delta.normalized().y) < 0.99 else Vector3.RIGHT)


# WO-011. Ascent Atlas v1.0 kernel: KX-JIB + KX-CRATE. Sited well clear of the
# Kellerworks yard -- the atlas kernel is its own bounded proof volume (atlas
# section 9), not band content, so it is not staged inside the tower approach.
func _build_kernel_jib(galvanised: StandardMaterial3D, mill_scale: StandardMaterial3D,
		hazard: StandardMaterial3D) -> void:
	var kernel_deck := _material(Color("55524a"), 0.05, 0.9)
	_add_box("KernelDeck", Vector3(20.0, 0.6, 20.0), Vector3(200.0, -0.3, 0.0), kernel_deck)
	_add_box("JibMast", Vector3(0.7, 5.0, 0.7), Vector3(200.0, 2.5, 0.0), mill_scale)

	_jib_boom_mesh = Node3D.new()
	_jib_boom_mesh.name = "JibBoom"
	_jib_boom_mesh.position = Vector3(200.0, 5.0, 0.0)
	$TowerPresentation.add_child(_jib_boom_mesh)
	_add_box_to("JibBoomBeam", Vector3(6.0, 0.3, 0.3), Vector3(3.0, 0.0, 0.0), galvanised,
		_jib_boom_mesh)

	_jib_hook_mesh = _add_box("JibHook", Vector3(0.3, 0.3, 0.3), Vector3(206.0, 1.65, 0.0), hazard)
	_jib_crate_mesh = _add_box("KernelCrate", Vector3(1.5, 1.5, 1.5), Vector3(206.0, 0.75, 0.0),
		mill_scale)
	_jib_hoist_cable = _add_box("JibHoistCable", Vector3(0.05, 0.05, 1.0), Vector3.ZERO,
		_material(Color("2b2621"), 0.5, 0.7))

	# Capacity-proving stand: fixed, no slew, permanently overweight. A real,
	# visible part of the yard, not a hidden test fixture -- it proves the
	# rated winch force is real whether or not anyone is watching.
	_add_box("CapacityStandMast", Vector3(0.6, 4.0, 0.6), Vector3(208.0, 2.0, 6.0), mill_scale)
	_jib_capacity_load_mesh = _add_box("CapacityStandLoad", Vector3(1.2, 1.2, 1.2),
		Vector3(208.0, 0.6, 6.0), hazard)


# WO-012. Ascent Atlas v1.0 kernel: KX-NEEDLE + KX-POCKETS. Each pier is a
# main block plus a lower notch: the seated beam's top sits flush with the
# pier top (a real recessed pocket, not a shelf the beam sits proud on --
# this locomotion has no step-up assist, confirmed by direct observation),
# so its underside needs somewhere to go that is not solid pier.
func _build_kernel_needle(galvanised: StandardMaterial3D, mill_scale: StandardMaterial3D) -> void:
	_add_box("NeedlePierApproachMain", Vector3(3.9, 4.0, 3.2), Vector3(195.35, 2.0, -16.0),
		mill_scale)
	_add_box("NeedlePierApproachNotch", Vector3(1.1, 3.64, 3.2), Vector3(197.85, 1.82, -16.0),
		mill_scale)
	_add_box("NeedlePierFarMain", Vector3(3.9, 4.0, 3.2), Vector3(204.65, 2.0, -16.0), mill_scale)
	_add_box("NeedlePierFarNotch", Vector3(1.1, 3.64, 3.2), Vector3(202.15, 1.82, -16.0), mill_scale)

	_add_box("NeedleHoistMast", Vector3(0.7, 7.0, 0.7), Vector3(200.0, 3.5, -13.4), mill_scale)
	_add_box("NeedleHoistHead", Vector3(0.8, 0.4, 0.8), Vector3(200.0, 7.0, -16.0), mill_scale)

	_needle_beam_mesh = _add_box("NeedleBeam", Vector3(5.0, 0.36, 1.0), Vector3(200.0, 6.2, -16.0),
		galvanised)
	_needle_hoist_cable = _add_box("NeedleHoistCable", Vector3(0.05, 0.05, 1.0), Vector3.ZERO,
		_material(Color("2b2621"), 0.5, 0.7))


# WO-013. Ascent Atlas v1.0 kernel: KX-SUMP + KX-GRATE. Fixed decking flanks
# one grate panel; the panel's own tint amplifies the real derived predicate
# (hazard amber wet, mill-scale grey safe) read back from native every frame
# -- it never decides the predicate, only displays it.
func _build_kernel_sump(mill_scale: StandardMaterial3D) -> void:
	_add_box("SumpApproachDeck", Vector3(3.0, 0.3, 3.0), Vector3(197.0, 2.85, 16.0), mill_scale)
	_add_box("SumpFarDeck", Vector3(3.0, 0.3, 3.0), Vector3(203.0, 2.85, 16.0), mill_scale)
	_add_box("SumpApproachLeg", Vector3(0.5, 3.0, 0.5), Vector3(197.0, 1.5, 16.0), mill_scale)
	_add_box("SumpFarLeg", Vector3(0.5, 3.0, 0.5), Vector3(203.0, 1.5, 16.0), mill_scale)

	_sump_grate_safe_material = mill_scale
	_sump_grate_hazard_material = _material(Color("6b4a1c"), 0.2, 0.75, Color("c98a2c"), 0.8)
	_sump_grate_mesh = _add_box("SumpGrate", Vector3(3.0, 0.3, 3.0), Vector3(200.0, 2.85, 16.0),
		_sump_grate_hazard_material)


# A soft radial falloff for every billboard particle in the scene. Without
# one, an untextured quad renders as a hard-edged square, and a plume reads as
# a drift of grey boxes rather than steam.
func _smoke_texture() -> GradientTexture2D:
	var gradient := Gradient.new()
	gradient.set_color(0, Color(1.0, 1.0, 1.0, 1.0))
	gradient.set_color(1, Color(1.0, 1.0, 1.0, 0.0))
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.width = 64
	texture.height = 64
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	return texture


func _build_plume() -> void:
	_plume = CPUParticles3D.new()
	_plume.name = "VentPlume"
	_plume.position = Vector3(30.5, 5.1, -101.5)
	# Kept deliberately sparse and thin: at the old 160 x 2.2 m billboards this
	# vent stacked into an opaque white wall whenever it sat between the eye
	# and the tower, swallowing the whole frame behind it.
	_plume.amount = 60
	_plume.lifetime = 2.6
	_plume.direction = Vector3(0.15, 1.0, 0.0)
	_plume.spread = 16.0
	_plume.gravity = Vector3(0.6, 1.1, 0.0)
	_plume.damping_min = 0.5
	_plume.damping_max = 1.4
	_plume.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	_plume.emission_sphere_radius = 0.45
	_plume.scale_amount_min = 0.5
	_plume.scale_amount_max = 1.1
	_plume.emitting = false

	var quad := QuadMesh.new()
	quad.size = Vector2(1.2, 1.2)
	_plume_material = StandardMaterial3D.new()
	_plume_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_plume_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	_plume_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	_plume_material.albedo_color = Color(0.82, 0.80, 0.76, 0.10)
	_plume_material.albedo_texture = _smoke_texture()
	_plume_material.roughness = 1.0
	_plume_material.disable_receive_shadows = false
	quad.material = _plume_material
	_plume.mesh = quad
	$TowerPresentation.add_child(_plume)

	# Stack plumes on the tower itself, high enough to shear the mass before the
	# crown. These are weather, not simulation, and are not claimed otherwise.
	# High stack plume, kept well above the climbable frame and much smaller
	# than before: at the old scale these billboards swallowed the structure.
	for stack in [Vector3(-34.0, 320.0, -168.0), Vector3(22.0, 392.0, -172.0)]:
		var haze := CPUParticles3D.new()
		haze.name = "StackPlume"
		haze.position = stack
		haze.amount = 26
		haze.lifetime = 20.0
		haze.direction = Vector3(0.8, 0.6, 0.0)
		haze.spread = 22.0
		haze.gravity = Vector3(3.2, 1.6, 0.0)
		haze.scale_amount_min = 8.0
		haze.scale_amount_max = 20.0
		haze.emitting = true
		var stack_quad := QuadMesh.new()
		stack_quad.size = Vector2(2.0, 2.0)
		var stack_material := StandardMaterial3D.new()
		stack_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		stack_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		stack_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		stack_material.albedo_color = Color(0.66, 0.64, 0.62, 0.16)
		stack_material.albedo_texture = _smoke_texture()
		stack_quad.material = stack_material
		haze.mesh = stack_quad
		$TowerPresentation.add_child(haze)

	# Working steam venting from the frame itself, at a human scale -- the
	# references are full of small, sharp vents, not fog banks.
	for level in range(2, STACK_LEVEL_COUNT, 3):
		var vent_y := float(level) * STACK_LEVEL_HEIGHT + 1.6
		var vent := CPUParticles3D.new()
		vent.name = "FrameVent"
		vent.position = Vector3(STACK_CENTER.x + (STACK_HALF_EXTENT - 2.2) * (1.0 if level % 2 == 0 else -1.0),
			vent_y, STACK_CENTER.z - STACK_HALF_EXTENT + 2.2)
		vent.amount = 10
		vent.lifetime = 2.2
		vent.direction = Vector3(0.3, 1.0, 0.0)
		vent.spread = 14.0
		vent.gravity = Vector3(0.4, 1.6, 0.0)
		vent.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
		vent.emission_sphere_radius = 0.3
		vent.scale_amount_min = 0.8
		vent.scale_amount_max = 2.6
		vent.emitting = true
		var vent_quad := QuadMesh.new()
		vent_quad.size = Vector2(1.6, 1.6)
		var vent_material := StandardMaterial3D.new()
		vent_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		vent_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		vent_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		vent_material.albedo_color = Color(0.90, 0.89, 0.86, 0.22)
		vent_material.albedo_texture = _smoke_texture()
		vent_quad.material = vent_material
		vent.mesh = vent_quad
		$TowerPresentation.add_child(vent)


func _build_sky_shear() -> void:
	# Thin high cloud that cuts the tower before the eye can finish it.
	var shear := StandardMaterial3D.new()
	shear.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	shear.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	shear.albedo_color = Color(0.30, 0.31, 0.33, 0.16)
	shear.cull_mode = BaseMaterial3D.CULL_DISABLED
	for level in [236.0, 318.0, 402.0, 520.0]:
		var plane := PlaneMesh.new()
		plane.size = Vector2(1400.0, 900.0)
		plane.material = shear
		var instance := MeshInstance3D.new()
		instance.name = "SkyShear"
		instance.mesh = plane
		instance.position = Vector3(0.0, level, -170.0)
		$TowerPresentation.add_child(instance)


func _build_lighting() -> void:
	# Sodium vapour: warm, low, and sourced from actual fittings on masts.
	for mast in [Vector3(-10.0, 0.0, -24.0), Vector3(-10.0, 0.0, -62.0),
			Vector3(-10.0, 0.0, -100.0), Vector3(22.0, 0.0, -44.0),
			Vector3(22.0, 0.0, -82.0), Vector3(40.0, 0.0, -104.0)]:
		_add_box("LampMast", Vector3(0.38, 11.0, 0.38), mast + Vector3(0.0, 5.5, 0.0),
			_material(Color("3a3835"), 0.7, 0.62))
		_add_box("LampHead", Vector3(1.5, 0.4, 0.8), mast + Vector3(0.6, 10.9, 0.0),
			_material(Color("5a4b2c"), 0.4, 0.7, Color("b06a1c"), 1.4))
		var lamp := OmniLight3D.new()
		lamp.name = "SodiumFlood"
		lamp.position = mast + Vector3(0.6, 10.6, 0.0)
		lamp.light_color = Color(1.0, 0.585, 0.225)
		lamp.light_energy = 3.5
		lamp.omni_range = 26.0
		lamp.omni_attenuation = 1.6
		_light_rig.add_child(lamp)

	for base_x in [-46.0, -16.0, 16.0, 46.0]:
		var base_flood := OmniLight3D.new()
		base_flood.name = "TowerFootFlood"
		base_flood.position = Vector3(base_x, 15.0, -132.0)
		base_flood.light_color = Color(1.0, 0.7, 0.42)
		base_flood.light_energy = 4.0
		base_flood.omni_range = 48.0
		base_flood.omni_attenuation = 1.3
		_light_rig.add_child(base_flood)

	_fire_box = OmniLight3D.new()
	_fire_box.name = "FireBox"
	_fire_box.position = Vector3(30.5, 1.2, -100.0)
	_fire_box.light_color = Color(1.0, 0.42, 0.14)
	_fire_box.light_energy = 2.4
	_fire_box.omni_range = 16.0
	_light_rig.add_child(_fire_box)

	_vent_light = OmniLight3D.new()
	_vent_light.name = "VentGlow"
	_vent_light.position = Vector3(30.5, 5.3, -101.5)
	_vent_light.light_color = Color(0.86, 0.82, 0.76)
	_vent_light.light_energy = 1.2
	_vent_light.omni_range = 22.0
	_light_rig.add_child(_vent_light)


func _build_mountains_and_waterfall() -> void:
	# Non-collidable alpine backdrop (identity doc item 2). Peaks are cheap
	# cones -- a CylinderMesh with top_radius 0 -- with a lighter snow-cap cone
	# nested at the tip. Distance fade comes from the environment's aerial
	# perspective fog, not from manual colour tuning per peak.
	# Kept dark and desaturated: these are distant backdrop mass, and at the
	# old values they read as bright white blobs competing with the tower.
	var rock := _material(Color("2f353d"), 0.04, 0.92)
	var rock_far := _material(Color("414a55"), 0.02, 0.95)
	var snow := _material(Color("b9c2cc"), 0.0, 0.78)

	var near_peaks := [
		Vector3(-380.0, 0.0, -560.0), Vector3(-160.0, 0.0, -610.0),
		Vector3(110.0, 0.0, -630.0), Vector3(180.0, 0.0, -540.0),
		Vector3(380.0, 0.0, -580.0), Vector3(580.0, 0.0, -680.0),
	]
	var near_sizes := [520.0, 640.0, 700.0, 600.0, 560.0, 480.0]
	for index in near_peaks.size():
		var base: Vector3 = near_peaks[index]
		var peak_height: float = near_sizes[index]
		var radius := peak_height * 0.62
		_add_cone("MountainPeak", radius, peak_height, base + Vector3(0.0, peak_height * 0.5, 0.0), rock)
		_add_cone("MountainSnowCap", radius * 0.34, peak_height * 0.3,
			base + Vector3(0.0, peak_height * 0.92, 0.0), snow)

	var far_peaks := [
		Vector3(-640.0, 0.0, -880.0), Vector3(-240.0, 0.0, -950.0),
		Vector3(260.0, 0.0, -930.0), Vector3(680.0, 0.0, -900.0),
	]
	for base in far_peaks:
		var peak_height := 900.0
		var radius := peak_height * 0.7
		_add_cone("MountainRidgeFar", radius, peak_height, base + Vector3(0.0, peak_height * 0.5, 0.0), rock_far)
		_add_cone("MountainRidgeFarSnow", radius * 0.4, peak_height * 0.32,
			base + Vector3(0.0, peak_height * 0.9, 0.0), snow)

	_build_waterfall(Vector3(200.0, 0.0, -560.0))
	_build_foothill_forest()


# A treeline at the base of the near peaks, mixed in with the rock cones
# already there -- the mountains stop being bare geometry and start being a
# real slope something grows on.
func _build_foothill_forest() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 4021
	var bands := [
		{"x": -380.0, "z": -520.0, "spread": 140.0},
		{"x": -160.0, "z": -560.0, "spread": 150.0},
		{"x": 140.0, "z": -570.0, "spread": 160.0},
		{"x": 420.0, "z": -540.0, "spread": 130.0},
	]
	for band in bands:
		var bx: float = band["x"]
		var bz: float = band["z"]
		var spread: float = band["spread"]
		for _tree in range(14):
			var tx := bx + rng.randf_range(-spread, spread)
			var tz := bz + rng.randf_range(-spread * 0.5, spread * 0.5)
			_add_tree(Vector3(tx, 0.0, tz), rng.randf_range(7.0, 13.0), rng)


# One stylised conifer: a trunk and three descending, widening canopy tiers.
# Cheap enough to scatter by the dozen, varied enough per-instance (scale,
# yaw, a hue jitter across the greens) that a cluster doesn't read as one
# mesh copy-pasted.
func _add_tree(at: Vector3, height: float, rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.03, 0.03)
	var canopy := _material(Color(0.16 + hue_jitter, 0.28 + hue_jitter, 0.14, 1.0), 0.0, 0.92)
	var trunk_material := _material(Color("362316"), 0.0, 0.9)

	var tree := Node3D.new()
	tree.name = "Conifer"
	tree.position = at
	tree.rotation.y = rng.randf_range(0.0, TAU)
	tree.scale = Vector3.ONE * rng.randf_range(0.85, 1.25)
	$TowerPresentation.add_child(tree)

	var trunk_height := height * 0.32
	_add_cylinder("TreeTrunk", height * 0.045, trunk_height,
		Vector3(0.0, trunk_height * 0.5, 0.0), trunk_material, tree)
	for tier in range(3):
		var t := float(tier) / 2.0
		var tier_radius := lerpf(height * 0.34, height * 0.11, t)
		var tier_height := height * 0.4
		var tier_y := trunk_height + t * height * 0.5
		_add_cone("TreeCanopy", tier_radius, tier_height,
			Vector3(0.0, tier_y + tier_height * 0.5, 0.0), canopy, tree)


# Low scrub and ground bushes: irregular clusters of squashed spheres, no
# trunk, filling the gap between bare grade and full trees.
func _add_bush(at: Vector3, spread: float, rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.04, 0.04)
	var bush_material := _material(Color(0.2 + hue_jitter, 0.3 + hue_jitter, 0.15, 1.0), 0.0, 0.94)
	var bush := Node3D.new()
	bush.name = "Scrub"
	bush.position = at
	$TowerPresentation.add_child(bush)
	for _lobe in range(rng.randi_range(3, 5)):
		var lobe_at := Vector3(rng.randf_range(-spread, spread), rng.randf_range(0.1, spread * 0.5),
			rng.randf_range(-spread, spread))
		var lobe := _add_sphere("ScrubLobe", rng.randf_range(spread * 0.45, spread * 0.75),
			lobe_at, bush_material, bush)
		lobe.scale.y = 0.72


func _add_sphere(node_name: String, radius: float, at: Vector3,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 10
	sphere.rings = 6
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = sphere
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(instance)
	return instance


# Foliage scattered through the yard itself: away from every kernel/stack/
# plant footprint, so it reads as the site being slowly reclaimed rather than
# clipping through a wall. Two open bands exist by construction -- west of
# the tower and plant, and along the north/entrance edge -- and this stays
# inside them.
func _build_foliage() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 7733

	for _tree in range(22):
		var tx := rng.randf_range(-225.0, -95.0)
		var tz := rng.randf_range(-260.0, 30.0)
		_add_tree(Vector3(tx, 0.0, tz), rng.randf_range(6.0, 11.0), rng)
	for _tree in range(10):
		var tx := rng.randf_range(-190.0, 190.0)
		var tz := rng.randf_range(110.0, 165.0)
		_add_tree(Vector3(tx, 0.0, tz), rng.randf_range(6.0, 10.0), rng)

	for _bush in range(26):
		var bx := rng.randf_range(-225.0, -90.0)
		var bz := rng.randf_range(-260.0, 40.0)
		_add_bush(Vector3(bx, 0.0, bz), rng.randf_range(1.1, 2.4), rng)
	for _bush in range(14):
		var bx := rng.randf_range(-190.0, 190.0)
		var bz := rng.randf_range(100.0, 170.0)
		_add_bush(Vector3(bx, 0.0, bz), rng.randf_range(1.0, 2.0), rng)


func _build_waterfall(at: Vector3) -> void:
	var falls := StandardMaterial3D.new()
	falls.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	falls.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	falls.albedo_color = Color(0.86, 0.9, 0.94, 0.55)
	falls.cull_mode = BaseMaterial3D.CULL_DISABLED
	var quad := QuadMesh.new()
	quad.size = Vector2(26.0, 420.0)
	quad.material = falls
	var instance := MeshInstance3D.new()
	instance.name = "Waterfall"
	instance.mesh = quad
	instance.position = at + Vector3(0.0, 210.0, 0.0)
	$TowerPresentation.add_child(instance)

	var mist := CPUParticles3D.new()
	mist.name = "WaterfallMist"
	mist.position = at + Vector3(0.0, 8.0, 8.0)
	mist.amount = 50
	mist.lifetime = 6.0
	mist.direction = Vector3(0.0, 1.0, 0.4)
	mist.spread = 40.0
	mist.gravity = Vector3(0.0, 1.4, 1.6)
	mist.scale_amount_min = 8.0
	mist.scale_amount_max = 20.0
	mist.emitting = true
	var mist_quad := QuadMesh.new()
	mist_quad.size = Vector2(2.0, 2.0)
	var mist_material := StandardMaterial3D.new()
	mist_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mist_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mist_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	mist_material.albedo_color = Color(0.88, 0.91, 0.94, 0.16)
	mist_quad.material = mist_material
	mist.mesh = mist_quad
	$TowerPresentation.add_child(mist)


func _build_kellerworks_signage(timber: Material, faded: Material) -> void:
	# Painted banners (identity doc items 1 and 7): real Label3D copy over a
	# backing plane, plus an original chevron-K mark. Non-collidable.
	var backing := _material(Color("4a231c"), 0.05, 0.85)
	var slogans := [
		["MATERIALS MOVE", "CIVILIZATION RISES"],
		["HIGHER  STRONGER", "FURTHER"],
		["PEOPLE  POWER", "PROGRESS"],
	]
	var banner_spots := [Vector3(-9.4, 0.0, -22.0), Vector3(21.4, 0.0, -42.0), Vector3(26.5, 0.0, -90.5)]
	for index in banner_spots.size():
		_add_banner(banner_spots[index], slogans[index], backing)

	# Tower-face wordmark: bigger backing, bigger text, high enough to be read
	# from the approach.
	_add_box("WordmarkBacking", Vector3(20.0, 5.0, 0.4), Vector3(0.0, 46.0, -142.3), backing)
	var wordmark := Label3D.new()
	wordmark.name = "KellerworksWordmark"
	wordmark.text = "KELLERWORKS"
	wordmark.font_size = 96
	wordmark.pixel_size = 0.018
	wordmark.modulate = Color(0.93, 0.89, 0.82)
	wordmark.billboard = BaseMaterial3D.BILLBOARD_DISABLED
	wordmark.position = Vector3(0.0, 46.0, -142.05)
	$TowerPresentation.add_child(wordmark)
	_add_chevron_mark(Vector3(0.0, 52.6, -142.1), 1.4, faded)

	# Lift signage near the platform (identity doc item 7): a display
	# designation distinct from the internal native entity ID (16).
	_add_box("LiftSignBacking", Vector3(1.6, 1.0, 0.12), Vector3(13.0, 8.7, -99.1), backing)
	var lift_sign := Label3D.new()
	lift_sign.name = "LiftSign"
	lift_sign.text = "LIFT A\nCAGE 3"
	lift_sign.font_size = 44
	lift_sign.pixel_size = 0.012
	lift_sign.modulate = Color(0.95, 0.92, 0.86)
	lift_sign.billboard = BaseMaterial3D.BILLBOARD_DISABLED
	lift_sign.position = Vector3(13.0, 8.7, -99.02)
	$TowerPresentation.add_child(lift_sign)


func _add_banner(at: Vector3, lines: Array, backing: Material) -> void:
	_add_box("BannerBacking", Vector3(3.2, 4.6, 0.12), at + Vector3(0.0, 4.6, 0.0), backing)
	var label := Label3D.new()
	label.name = "BannerCopy"
	label.text = "\n".join(lines)
	label.font_size = 40
	label.pixel_size = 0.0095
	label.modulate = Color(0.92, 0.88, 0.8)
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.position = at + Vector3(0.0, 4.6, 0.07)
	$TowerPresentation.add_child(label)
	_add_chevron_mark(at + Vector3(0.0, 6.7, 0.07), 0.55, backing)


func _add_chevron_mark(at: Vector3, scale: float, accent: Material) -> void:
	# Original angular chevron-K mark, built from three thin boxes. Not a copy
	# of any existing logotype.
	var mark := Node3D.new()
	mark.name = "ChevronMark"
	mark.position = at
	$TowerPresentation.add_child(mark)
	_add_box_to("ChevronSpine", Vector3(0.14, 0.9, 0.05) * scale, Vector3(-0.32, 0.0, 0.0) * scale, accent, mark)
	_add_box_to("ChevronUpper", Vector3(0.62, 0.14, 0.05) * scale, Vector3(0.05, 0.24, 0.0) * scale, accent, mark, -0.55)
	_add_box_to("ChevronLower", Vector3(0.62, 0.14, 0.05) * scale, Vector3(0.05, -0.24, 0.0) * scale, accent, mark, 0.55)


func _build_gear_motif(mill_scale: Material, oxidised: Material) -> void:
	# Decorative exposed gear + wound drum on the tower face (identity doc item
	# 5). Rotation is applied in _mirror_machine from the real machine phase.
	_gear_pivot = Node3D.new()
	_gear_pivot.name = "FaceGearPivot"
	_gear_pivot.position = Vector3(-30.0, 58.0, -142.4)
	$TowerPresentation.add_child(_gear_pivot)
	var hub := CylinderMesh.new()
	hub.top_radius = 3.6
	hub.bottom_radius = 3.6
	hub.height = 0.7
	hub.radial_segments = 20
	var hub_instance := MeshInstance3D.new()
	hub_instance.mesh = hub
	hub_instance.material_override = mill_scale
	hub_instance.rotation = Vector3(deg_to_rad(90.0), 0.0, 0.0)
	_gear_pivot.add_child(hub_instance)
	for tooth_index in 12:
		var angle := float(tooth_index) / 12.0 * TAU
		var tooth := _add_box_to("GearTooth", Vector3(0.75, 0.75, 0.7), Vector3.ZERO, mill_scale, _gear_pivot)
		tooth.position = Vector3(cos(angle), sin(angle), 0.0) * 3.9
		tooth.rotation = Vector3(0.0, 0.0, angle)

	_drum_pivot = Node3D.new()
	_drum_pivot.name = "FaceDrumPivot"
	_drum_pivot.position = Vector3(-19.0, 58.0, -142.4)
	$TowerPresentation.add_child(_drum_pivot)
	var drum := CylinderMesh.new()
	drum.top_radius = 1.9
	drum.bottom_radius = 1.9
	drum.height = 3.2
	drum.radial_segments = 14
	var drum_instance := MeshInstance3D.new()
	drum_instance.mesh = drum
	drum_instance.material_override = oxidised
	drum_instance.rotation = Vector3(0.0, 0.0, deg_to_rad(90.0))
	_drum_pivot.add_child(drum_instance)


func _build_crane(mill_scale: Material, hazard: Material) -> void:
	# Scale-telegraphing crane (identity doc item 6). Sway is applied in
	# _update_ambient_dressing from a wall-clock, never from native state --
	# it is explicitly not simulated rigging.
	var mast_base := Vector3(58.0, 0.0, -108.0)
	_add_box("CraneMast", Vector3(1.1, 26.0, 1.1), mast_base + Vector3(0.0, 13.0, 0.0), hazard)

	_crane_boom = Node3D.new()
	_crane_boom.name = "CraneBoom"
	_crane_boom.position = mast_base + Vector3(0.0, 25.0, 0.0)
	$TowerPresentation.add_child(_crane_boom)
	_add_box_to("CraneBoomArm", Vector3(24.0, 0.9, 0.9), Vector3(-11.0, 0.6, 0.0), mill_scale, _crane_boom)
	_add_box_to("CraneCounterArm", Vector3(6.0, 0.9, 0.9), Vector3(4.0, 0.6, 0.0), mill_scale, _crane_boom)
	_add_box_to("CraneCounterweight", Vector3(2.4, 2.0, 2.4), Vector3(7.4, -0.4, 0.0), mill_scale, _crane_boom)

	_crane_hook = Node3D.new()
	_crane_hook.name = "CraneHook"
	_crane_hook.position = Vector3(-20.0, -13.0, 0.0)
	_crane_boom.add_child(_crane_hook)
	_add_box_to("CraneCable", Vector3(0.08, 12.0, 0.08), Vector3(0.0, 6.0, 0.0), hazard, _crane_hook)
	_crane_crate = _add_box_to("CraneCrate", Vector3(2.6, 2.0, 2.6), Vector3.ZERO, mill_scale, _crane_hook)


func _add_cone(node_name: String, radius: float, height: float, at: Vector3, material: Material,
		parent: Node3D = null) -> MeshInstance3D:
	var cone := CylinderMesh.new()
	cone.top_radius = 0.0
	cone.bottom_radius = radius
	cone.height = height
	cone.radial_segments = 9
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = cone
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(instance)
	return instance


# A member spanning two points. Almost every diagonal in a steel frame is one
# of these -- braces, buttress legs, jib booms, tie rods -- and computing the
# pose from the endpoints is far less error-prone than hand-solving rotations.
func _add_strut(node_name: String, from: Vector3, to: Vector3, thickness: float,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var delta := to - from
	var length := delta.length()
	if length < 0.01:
		return null
	var host: Node3D = parent if parent != null else $TowerPresentation
	var instance := _add_box_to(node_name, Vector3(thickness, thickness, length),
		from + delta * 0.5, material, host)
	var up := Vector3.UP if absf(delta.normalized().y) < 0.99 else Vector3.RIGHT
	instance.look_at(to, up)
	return instance


func _add_cylinder(node_name: String, radius: float, height: float, at: Vector3,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var cylinder := CylinderMesh.new()
	cylinder.top_radius = radius
	cylinder.bottom_radius = radius
	cylinder.height = height
	cylinder.radial_segments = 12
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = cylinder
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(instance)
	return instance


# An exposed gearwheel: hub, spokes and a toothed rim, built in the local XY
# plane so the returned node can be rotated to face any direction and spun on
# its own Z axis. The references lean on these hard -- they are the single
# clearest signal that the building is a machine.
func _add_gear(node_name: String, at: Vector3, radius: float, teeth: int,
		hub_material: Material, rim_material: Material) -> Node3D:
	var gear := Node3D.new()
	gear.name = node_name
	gear.position = at
	$TowerPresentation.add_child(gear)

	var depth := maxf(0.5, radius * 0.16)
	_add_box_to("GearHub", Vector3(radius * 0.38, radius * 0.38, depth * 1.5),
		Vector3.ZERO, hub_material, gear)
	for spoke in range(6):
		var spoke_mesh := _add_box_to("GearSpoke",
			Vector3(radius * 1.7, radius * 0.11, depth * 0.8), Vector3.ZERO, hub_material, gear)
		spoke_mesh.rotation = Vector3(0.0, 0.0, PI * float(spoke) / 6.0)
	# Rim built from short chords, with a tooth standing proud of each joint.
	var rim_step := TAU / float(teeth)
	for index in range(teeth):
		var angle := rim_step * float(index)
		var chord := 2.0 * radius * tan(rim_step * 0.5) * 1.06
		var rim := _add_box_to("GearRim", Vector3(chord, radius * 0.13, depth),
			Vector3(cos(angle) * radius, sin(angle) * radius, 0.0), rim_material, gear)
		rim.rotation = Vector3(0.0, 0.0, angle + PI * 0.5)
		var tooth_radius := radius * 1.075
		var tooth := _add_box_to("GearTooth",
			Vector3(chord * 0.5, radius * 0.11, depth * 0.92),
			Vector3(cos(angle) * tooth_radius, sin(angle) * tooth_radius, 0.0), rim_material, gear)
		tooth.rotation = Vector3(0.0, 0.0, angle + PI * 0.5)
	return gear


# Painted text on the structure. Label3D keeps this readable at distance
# without needing an atlas, which is what sells the company's presence in the
# references -- the building talks at you.
func _add_sign_text(text: String, at: Vector3, yaw: float, height_meters: float,
		color: Color, parent: Node3D = null) -> Label3D:
	var label := Label3D.new()
	label.text = text
	label.font_size = 64
	label.pixel_size = height_meters / 64.0
	label.modulate = color
	label.double_sided = true
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.position = at
	label.rotation = Vector3(0.0, yaw, 0.0)
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(label)
	return label


# Real bump mapping, not flat-shaded boxes. gl_compatibility (what CI renders
# with) has no SSAO/SSIL/SDFGI -- those are Forward+-only -- so per-pixel
# normal perturbation is the actual lever available here for surface detail,
# and it is a basic, renderer-agnostic feature confirmed working by direct
# probe (NoiseTexture2D.as_normal_map). Three shared textures (steel, timber,
# concrete), not one per material instance, since the grain frequency is what
# distinguishes them and dozens of unique noise textures would cost more than
# they are worth.
var _bump_steel: NoiseTexture2D
var _bump_timber: NoiseTexture2D
var _bump_concrete: NoiseTexture2D


func _build_bump_textures() -> void:
	_bump_steel = _make_bump_texture(1, 0.5, 1.8, false)
	_bump_timber = _make_bump_texture(2, 0.7, 2.6, true)
	_bump_concrete = _make_bump_texture(3, 0.7, 1.3, false)


func _make_bump_texture(seed_value: int, frequency: float, strength: float,
		directional: bool) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.frequency = frequency
	noise.fractal_octaves = 4
	noise.fractal_lacunarity = 2.1
	if directional:
		# Wood grain: stretched noise reads as fibrous rather than pitted.
		noise.frequency = frequency * 0.2
		noise.fractal_type = FastNoiseLite.FRACTAL_RIDGED
	var tex := NoiseTexture2D.new()
	tex.width = 256
	tex.height = 256
	tex.seamless = true
	tex.generate_mipmaps = true
	tex.as_normal_map = true
	tex.bump_strength = strength
	tex.noise = noise
	return tex


func _material(color: Color, metallic: float, roughness: float,
		emission: Color = Color.BLACK, emission_energy: float = 1.0,
		bump: NoiseTexture2D = null) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
	if bump != null:
		material.normal_enabled = true
		material.normal_texture = bump
		material.uv1_triplanar = true
		material.uv1_triplanar_sharpness = 1.0
		material.uv1_scale = Vector3(0.22, 0.22, 0.22)
	return material


func _add_box(node_name: String, size: Vector3, at: Vector3, material: Material,
		roll: float = 0.0) -> MeshInstance3D:
	return _add_box_to(node_name, size, at, material, $TowerPresentation, roll)


func _add_box_to(node_name: String, size: Vector3, at: Vector3, material: Material,
		parent: Node3D, roll: float = 0.0) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.mesh = mesh
	instance.position = at
	if not is_zero_approx(roll):
		instance.rotation = Vector3(0.0, 0.0, roll)
	parent.add_child(instance)
	return instance


func _fail_native(reason: String, exit_code: int) -> void:
	_status.text = "NATIVE AUTHORITY FAILURE"
	_status.modulate = Color("c8503a")
	push_error(reason)
	get_tree().quit(exit_code)


func _print_ci_phase(label: String) -> void:
	var position: Vector3 = _native.get_player_position()
	print("SCRAPERX_CI_PHASE %s tick=%d position=(%.2f,%.2f,%.2f) valve=%.2f lift=%.2f" % [
		label, _native.get_tick_index(), position.x, position.y, position.z,
		float(_native.get_valve_open_fraction()),
		float(_native.get_lift_platform_position().y)])


func _print_runtime_proof() -> void:
	_ci_proof_printed = true
	var position: Vector3 = _native.get_player_position()
	print("SCRAPERX_WO004_VIEWPORT_PROOF width=%d height=%d aspect=%.3f fov=%.1f far=%.0f stretch=expand" % [
		int(_viewport_size.x), int(_viewport_size.y),
		_viewport_size.x / maxf(1.0, _viewport_size.y), _camera.fov, _camera.far])
	print("SCRAPERX_WO005_APPROACH_PROOF spawn_grade=1 position=(%.2f,%.2f,%.2f) tower_face_distance=%.1f tower_height=%.0f" % [
		position.x, position.y, position.z, absf(-145.0 - position.z),
		float(_native.get_tower_height_meters())])
	print("SCRAPERX_WO006_MACHINE_PROOF ticks=%d peak_valve=%.2f peak_lift=%.2f peak_flow=%.3f shut_flow=%.5f vessel_bar=%.2f vented=%.2f" % [
		_native.get_tick_index(), _ci_peak_valve, _ci_peak_lift, _ci_peak_flow,
		_ci_shut_flow, float(_native.get_vessel_pressure_pa()) / 1.0e5,
		float(_native.get_vented_mass_kg())])


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
