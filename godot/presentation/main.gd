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
	elif grounded and support == CRATE_ENTITY_ID:
		_status.text = "RIDING THE CRATE"
	elif grounded and support == NEEDLE_BEAM_ENTITY_ID:
		_status.text = "ON THE SEATED NEEDLE"
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


# --- world ------------------------------------------------------------------
#
# Industrial palette: mill scale, oxidised steel, poured concrete, wet asphalt,
# galvanised mesh, faded warning yellow, chipped hazard orange. Nothing here
# emits light except a sodium fitting, a fire box, or a vent -- colour is a
# consequence of material and weather, not a shader preset.


func _build_world() -> void:
	var asphalt := _material(Color("18191b"), 0.06, 0.34)
	var concrete := _material(Color("6a635b"), 0.0, 0.93)
	var mill_scale := _material(Color("33322f"), 0.82, 0.58)
	var oxidised := _material(Color("6b3a22"), 0.22, 0.88)
	var galvanised := _material(Color("8b8e91"), 0.72, 0.42)
	var faded_yellow := _material(Color("8f7a2e"), 0.12, 0.76)
	var hazard := _material(Color("8a4620"), 0.14, 0.8)
	var tar := _material(Color("131417"), 0.05, 0.62)

	# Grade and tower: sizes mirror the native Jolt bodies exactly.
	_add_box("Grade", Vector3(480.0, 1.0, 480.0), Vector3(0.0, -0.5, -60.0), asphalt)
	_add_box("Tower", Vector3(120.0, 1600.0, 90.0), Vector3(0.0, 800.0, -190.0), concrete)

	var timber := _material(Color("4a3524"), 0.02, 0.86)

	_build_tower_skin(mill_scale, oxidised, galvanised, faded_yellow, timber)
	_build_yard(concrete, mill_scale, faded_yellow, tar)
	_build_legacy_fixtures(mill_scale, galvanised, hazard, faded_yellow)
	_build_plant(mill_scale, oxidised, galvanised, hazard, faded_yellow)
	_build_mountains_and_waterfall()
	_build_kellerworks_signage(timber, faded_yellow)
	_build_gear_motif(mill_scale, oxidised)
	_build_crane(mill_scale, hazard)
	_build_sky_shear()
	_build_lighting()


func _build_tower_skin(mill_scale: Material, oxidised: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	# Structural relief on the approach face so the lower third reads as a wall of
	# structure rather than a flat slab. Non-authoritative set dressing: it sits
	# proud of the native collision box by design.
	for x in [-52.0, -26.0, 0.0, 26.0, 52.0]:
		_add_box("FacePier", Vector3(7.0, 240.0, 3.0), Vector3(x, 120.0, -143.0), mill_scale)
	for level in range(14, 240, 16):
		_add_box("FaceBand", Vector3(118.0, 1.4, 2.0), Vector3(0.0, float(level), -143.4), oxidised)
	for x in [-40.0, -13.0, 13.0, 40.0]:
		_add_box("FaceDuct", Vector3(3.2, 190.0, 3.2), Vector3(x, 96.0, -141.0), galvanised)
	var lit_band := _material(Color("2a2521"), 0.1, 0.8, Color("c07a24"), 0.4)
	var lit_band_dim := _material(Color("242019"), 0.1, 0.8, Color("6e4a1c"), 0.3)
	for level in range(22, 320, 12):
		var band: Material = lit_band if (level / 12) % 3 != 0 else lit_band_dim
		_add_box("FloorLight", Vector3(104.0, 1.0, 0.6), Vector3(0.0, float(level), -144.3), band)
	for level in range(340, 900, 34):
		_add_box("FloorLightHigh", Vector3(96.0, 0.9, 0.6), Vector3(0.0, float(level), -144.3), lit_band_dim)
	_add_box("LoadingHeader", Vector3(46.0, 4.0, 3.0), Vector3(0.0, 13.0, -141.0), faded)
	for x in [-18.0, -6.0, 6.0, 18.0]:
		_add_box("BayDoor", Vector3(9.0, 11.0, 1.2), Vector3(x, 5.5, -141.2), mill_scale)
	# Timber cladding accent (identity doc item 4): secondary material, not a
	# replacement for the steel piers/bands above.
	for x in [-39.0, -14.0, 14.0, 39.0]:
		_add_box("TimberCladding", Vector3(9.0, 34.0, 1.2), Vector3(x, 30.0, -142.6), timber)


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


func _build_plume() -> void:
	_plume = CPUParticles3D.new()
	_plume.name = "VentPlume"
	_plume.position = Vector3(30.5, 5.1, -101.5)
	_plume.amount = 160
	_plume.lifetime = 3.4
	_plume.direction = Vector3(0.15, 1.0, 0.0)
	_plume.spread = 16.0
	_plume.gravity = Vector3(0.6, 1.1, 0.0)
	_plume.damping_min = 0.5
	_plume.damping_max = 1.4
	_plume.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	_plume.emission_sphere_radius = 0.45
	_plume.scale_amount_min = 0.9
	_plume.scale_amount_max = 1.8
	_plume.emitting = false

	var quad := QuadMesh.new()
	quad.size = Vector2(2.2, 2.2)
	_plume_material = StandardMaterial3D.new()
	_plume_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_plume_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	_plume_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	_plume_material.albedo_color = Color(0.80, 0.78, 0.74, 0.05)
	_plume_material.roughness = 1.0
	_plume_material.disable_receive_shadows = false
	quad.material = _plume_material
	_plume.mesh = quad
	$TowerPresentation.add_child(_plume)

	# Stack plumes on the tower itself, high enough to shear the mass before the
	# crown. These are weather, not simulation, and are not claimed otherwise.
	for stack in [Vector3(-34.0, 210.0, -150.0), Vector3(22.0, 268.0, -152.0)]:
		var haze := CPUParticles3D.new()
		haze.name = "StackPlume"
		haze.position = stack
		haze.amount = 40
		haze.lifetime = 22.0
		haze.direction = Vector3(0.8, 0.6, 0.0)
		haze.spread = 24.0
		haze.gravity = Vector3(3.2, 1.6, 0.0)
		haze.scale_amount_min = 26.0
		haze.scale_amount_max = 64.0
		haze.emitting = true
		var stack_quad := QuadMesh.new()
		stack_quad.size = Vector2(2.0, 2.0)
		var stack_material := StandardMaterial3D.new()
		stack_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		stack_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		stack_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		stack_material.albedo_color = Color(0.30, 0.29, 0.28, 0.10)
		stack_quad.material = stack_material
		haze.mesh = stack_quad
		$TowerPresentation.add_child(haze)


func _build_sky_shear() -> void:
	# Thin high cloud that cuts the tower before the eye can finish it.
	var shear := StandardMaterial3D.new()
	shear.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	shear.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	shear.albedo_color = Color(0.23, 0.225, 0.215, 0.36)
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
	var rock := _material(Color("3c434c"), 0.04, 0.9)
	var rock_far := _material(Color("57616c"), 0.02, 0.94)
	var snow := _material(Color("f4f7fa"), 0.0, 0.7)

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


func _add_cone(node_name: String, radius: float, height: float, at: Vector3, material: Material) -> MeshInstance3D:
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
	$TowerPresentation.add_child(instance)
	return instance


func _material(color: Color, metallic: float, roughness: float,
		emission: Color = Color.BLACK, emission_energy: float = 1.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
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
