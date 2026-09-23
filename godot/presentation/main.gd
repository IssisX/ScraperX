extends Node3D

# Presentation and input only. Every consequential fact below is read from the
# native ScraperX simulation; nothing here decides pose, support, traversal, or
# machine state. Where this file draws something that looks simulated -- the
# steam plume above all -- it is driven by an authoritative native value, so
# freezing that value freezes the effect.

const EYE_OFFSET := Vector3(0.0, 0.62, 0.0)

# Interface: every device (touch, pad, keyboard/mouse) is read by one router
# into one verb vocabulary; the HUD, touch surface and pause menu draw from
# one per-frame read of native state (_read_context). None of it decides
# anything -- a verb becomes a native request_*, and the native says yes or no.
const UiStyle := preload("res://presentation/ui/ui_style.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const Hud := preload("res://presentation/ui/hud.gd")
const PauseMenu := preload("res://presentation/ui/pause_menu.gd")
const SettingsStore := preload("res://presentation/ui/settings_store.gd")
# Loaded only for --uitest runs, so shipping builds never parse test code.
const UI_TEST_DRIVER_PATH := "res://presentation/ui/ui_test_driver.gd"
const FirstPersonArms := preload("res://presentation/first_person_arms.gd")
const SkyCycleScript := preload("res://presentation/sky_cycle.gd")
const AudioDirector := preload("res://presentation/audio/audio_director.gd")
# Traversal head motion, added on top of the player's own pitch and never
# written into it: a hanging climber looks up at the grip (the lip sits ~46
# degrees above a level gaze, outside the frame), a mantle nods down onto the
# hands taking the push. The offset eases back to exactly zero, so a player
# who is not traversing sees the unaltered pitch -- the CI hold included.
const VIEW_PITCH_HANGING := 0.32
const VIEW_PITCH_MANTLING := -0.3
const VIEW_PITCH_VAULTING := -0.12
const VIEW_PITCH_RATE := 9.0
# A hitch -- or the first frame after Android resumes a backgrounded app --
# must not be paid for with a burst of native ticks in one frame. The excess
# of that one frame simply runs slower than wall time, which nobody can see.
const MAX_SIM_FRAME_DELTA := 0.1
# Chute is offered once a fall is unmistakably a fall: a full-height jump
# lands at ~5.5 m/s, so 6.5 m/s never pops the canopy button mid-hop.
const CHUTE_OFFER_FALL_MPS := 6.5
# A Jump pressed this long before touchdown fires on the grounded tick. Input
# timing assistance only (GDD 7.2): it never jumps from anything the native
# does not report as ground at the moment it fires.
const JUMP_BUFFER_SECONDS := 0.12
# A second Jump this soon after one was sent goes straight to the native,
# which vaults if the takeoff could have (its own window is 0.30 s of ticks;
# this is a frame's slack wider so the native, not frame timing, decides).
const DOUBLE_TAP_SECONDS := 0.35
const CHECKPOINT_TOAST_RISE_METERS := 3.0
const SLING_CHECK_SECONDS := 0.3

# Locomotion-feel camera response. Pure presentation, driven every frame by
# native player position/velocity/grounded state already read below -- never
# the other way around, and never touching _native itself. Every term is a
# function of CURRENT state (speed, ground contact, the last frame's own
# vertical velocity), not an accumulating drift, so it always returns to
# exactly baseline (no dip, no bob, FOV_BASE) the moment the player is
# grounded and stationary -- the CI runtime proof's own OBSERVE hold, where
# the player stands still, depends on that landing on the same fov=82.0 the
# scene is built with.
const FOV_BASE := 82.0
const FOV_SPRINT_MAX_DEGREES := 4.0
const FOV_FALL_MAX_DEGREES := 3.0
const FOV_FALL_FULL_MPS := 14.0
const LANDING_DIP_DURATION_SECONDS := 0.22
const LANDING_DIP_MAX_METERS := 0.16
const LANDING_DIP_MIN_IMPACT_MPS := 2.0
const LANDING_DIP_FULL_IMPACT_MPS := 9.0
const HEAD_BOB_CYCLES_PER_METER := 0.72
const HEAD_BOB_VERTICAL_METERS := 0.028
const HEAD_BOB_LATERAL_METERS := 0.016
const HEAD_BOB_SPEED_FLOOR_MPS := 0.3
const HEAD_BOB_SPEED_FULL_MPS := 3.0

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
# Stairwell cut into each deck above the flight that arrives there; mirrors
# kStackStairwellStart / kStackStairwellHalfWidth in simulation.cpp.
const STACK_STAIRWELL_START := 7.5
const STACK_STAIRWELL_HALF_WIDTH := 2.1
const STACK_FLIGHT_HALF_THICKNESS := 0.18

# AS-001 B00 intake rise. Every figure here mirrors a kIntake* constant in
# src/sim/simulation.cpp -- what is drawn and what is collided with are the
# same geometry, so a player who can see a route can walk it.
const INTAKE_BAY_FRONT_Z := -110.5
const INTAKE_BAY_BACK_Z := -122.5
const INTAKE_BAY_HALF_X := 10.0
const INTAKE_BAY_WALL_HEIGHT := 5.0
const INTAKE_BAY_WALL_THICKNESS := 0.6
const INTAKE_THROAT_HALF_WIDTH := 1.3
const INTAKE_THROAT_HEIGHT := 2.5
const INTAKE_DOG_HALF_WIDTH := 1.04
const INTAKE_DOG_HALF_HEIGHT := 1.19
const INTAKE_DOG_CENTER_Y := 1.25
const INTAKE_JIB_MAST_Z := -100.2
const INTAKE_BOOM_LENGTH := 12.0
const INTAKE_BOOM_HEIGHT := 11.5
const INTAKE_PACK_SIZE := Vector3(2.2, 1.8, 2.3)
const INTAKE_OVERWEIGHT_STAND := Vector3(7.0, 0.0, -98.0)
const INTAKE_BELT_X := 6.0
const INTAKE_BELT_TOP_Y := 1.38
const INTAKE_PENDANT_X := 3.0
const INTAKE_PENDANT_Z := -100.0
const INTAKE_STAIR_Z := -118.0
const INTAKE_STAIR_LANE_OFFSET := 2.0
const INTAKE_STAIR_FLIGHT_COUNT := 6
const INTAKE_STAIR_FLIGHT_RISE := 4.0
const INTAKE_STAIR_HALF_RUN := 7.0
const INTAKE_STAIR_WIDTH := 3.6
const INTAKE_STAIR_LANDING_X := 8.0
const INTAKE_HANDOFF_Y := 24.0
const INTAKE_HANDOFF_CENTER_X := -6.0
const INTAKE_HANDOFF_SOUTH_Z := -107.7
const INTAKE_HANDOFF_NORTH_Z := -117.5
const INTAKE_SKIN_RUNG_RISE := 1.6
const INTAKE_SKIN_RUNG_COUNT := 15
const INTAKE_SKIN_RUNG_HALF_Z := 1.0

# AS-002 Legal Forty (Ascent Atlas §6, band B00's 24-40 m leftover). Surface
# heights read off simulation.cpp's own build-time derivation (handoff
# tread-surface offset stacked with two more 8.000 m rises); geometry
# literals read off the kLegalForty*/kIntake* constants there directly.
const LEGAL_FORTY_HANDOFF_SURFACE_Y := 24.1872
const LEGAL_FORTY_MID_LANDING_SURFACE_Y := 32.1872
const LEGAL_FORTY_HALL_DECK_SURFACE_Y := 40.1872
const LEGAL_FORTY_FLIGHT_HALF_LENGTH := 8.0
const LEGAL_FORTY_FLIGHT_HALF_WIDTH := 0.90
const LEGAL_FORTY_FLIGHT_RISE := 8.0
const LEGAL_FORTY_HINGE_X := 7.856
const LEGAL_FORTY_HINGE_Z := -112.5
const LEGAL_FORTY_STOWED_THETA := 0.139626 # 8 deg
const LEGAL_FORTY_SWING_BRACKET_LOCAL_X := -7.0 # local, relative to the body centre
const LEGAL_FORTY_SWING_BRACKET_LOCAL_Y := -0.18
const LEGAL_FORTY_SWING_SHEAVE_HEIGHT := 4.0
const LEGAL_FORTY_MID_LANDING_X := 9.350
const LEGAL_FORTY_MID_LANDING_HALF_X := 1.05
const LEGAL_FORTY_MID_LANDING_Z := -115.80
const LEGAL_FORTY_MID_LANDING_HALF_Z := 5.20
const LEGAL_FORTY_UPPER_FLIGHT_X := 2.422
const LEGAL_FORTY_UPPER_FLIGHT_Z := -117.0
const LEGAL_FORTY_HALL_DECK_HALF_X := 10.0
const LEGAL_FORTY_HALL_DECK_Z := -115.1
const LEGAL_FORTY_HALL_DECK_HALF_Z := 7.40
const LEGAL_FORTY_HALL_DECK_HALF_THICKNESS := 0.20
const LEGAL_FORTY_WELL_X := -1.65
const LEGAL_FORTY_WELL_HALF_X := 3.15
const LEGAL_FORTY_WELL_Z := -117.0
const LEGAL_FORTY_WELL_HALF_Z := 2.20
const LEGAL_FORTY_SKIN_RUNG_FIRST := 16
const LEGAL_FORTY_SKIN_RUNG_LAST := 20
const LEGAL_FORTY_SKIN_RUNG_FIRST_Z := -110.0
const LEGAL_FORTY_SKIN_RUNG_STEP_Z := 2.0
# Rungs 17-20 sit 0.8 m west of the column, clear of the bascule flight's
# foot (kLegalFortySkinRungJogX); the walkway starts at rung 20's east face.
const LEGAL_FORTY_SKIN_RUNG_JOG_X := -0.8
const LEGAL_FORTY_SKIN_WALKWAY_MIN_X := -5.8
const LEGAL_FORTY_SKIN_WALKWAY_MAX_X := 8.30
# z in [-121, -117]: 2 m south of rung 20's band, clear of the upper flight
# overhead (kLegalFortySkinWalkwayCenterZ has why).
const LEGAL_FORTY_SKIN_WALKWAY_Z := -119.0
const LEGAL_FORTY_SKIN_WALKWAY_HALF_Z := 2.00
const LEGAL_FORTY_CW_CRADLE_HALF_X := 1.50
const LEGAL_FORTY_CW_CRADLE_HALF_Y := 0.90  # native: clears the B00 belt
const LEGAL_FORTY_CW_CRADLE_HALF_Z := 1.20
const LEGAL_FORTY_CW_CRADLE_BUILD_Y := 5.20 - LEGAL_FORTY_CW_CRADLE_HALF_Y  # top face at 5.20
const LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_X := 0.35
const LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Y := 4.0
const LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Z := 0.35
const LEGAL_FORTY_CW_CRADLE_BEARING := 0.80

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
var _cam_was_grounded := true
var _cam_last_velocity_y := 0.0
var _cam_landing_timer := 0.0
var _cam_landing_strength := 0.0
var _cam_bob_phase := 0.0
# DISPLAY settings; the defaults are the tuned values above.
var _fov_base := FOV_BASE
var _head_bob_on := true
var _speed_fov_on := true
var _fps_label: Label
var _fps_clock := 0.0
var _viewport_size := Vector2.ZERO

var _router: Node
var _touch: Control
var _hud: Control
var _pause_menu: Control
var _settings: SettingsStore
var _uitest: Node
var _force_touch := false
var _uitest_scenario := ""
var _paused := false
var _telemetry_on := false
var _ctx := {}
# Pendant "operate" mode is presentation state only: which controls are on
# screen. The native gates every pendant axis by station radius regardless.
var _operating := &""
var _jump_buffer := 0.0
var _since_jump_sent := INF
var _fb_traversal := 0
var _fb_grounded := true
var _fb_fall_speed := 0.0
var _fb_deaths := 0
var _fb_chute := false
var _fb_warned := false
var _fb_best_checkpoint_y := 0.0
var _sling_check_timer := 0.0
var _sling_check_was_slung := false
var _arms: Node3D
var _view_pitch_offset := 0.0

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
var _intake_boom_pivot: Node3D
var _intake_hook_mesh: MeshInstance3D
var _intake_pack_mesh: MeshInstance3D
var _intake_overweight_mesh: MeshInstance3D
var _intake_dog_pivot: Node3D
var _intake_hoist_cable: Node3D
var _intake_belt_mesh: MeshInstance3D
var _legal_forty_swing_flight_pivot: Node3D
var _legal_forty_cradle_mesh: MeshInstance3D
var _legal_forty_rope_flight: Node3D
var _legal_forty_rope_cradle: Node3D
var _legal_forty_sheave_a := Vector3.ZERO
var _legal_forty_sheave_b := Vector3.ZERO
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
@onready var _fall_value: Label = $HUD/TopLeft/Fall
@onready var _jib_value: Label = $HUD/TopLeft/Jib
@onready var _needle_value: Label = $HUD/TopLeft/Needle
@onready var _sump_value: Label = $HUD/TopLeft/Sump
@onready var _intake_value: Label = $HUD/TopLeft/Intake
@onready var _legal_forty_value: Label = $HUD/TopLeft/LegalForty
@onready var _light_rig: Node3D = $LightRig
var _sky_cycle: Node
var _audio: Node


var _export_solids_path := ""


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument == "--touch":
			_force_touch = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")
		elif argument.begins_with("--uitest="):
			_uitest_scenario = argument.trim_prefix("--uitest=")
		elif argument.begins_with("--export-solids="):
			_export_solids_path = argument.trim_prefix("--export-solids=")

	RenderingServer.set_default_clear_color(Color("0e0d0c"))
	_build_world()
	if _export_solids_path != "":
		_export_solids()
		return
	_sky_cycle = SkyCycleScript.new()
	_sky_cycle.name = "SkyCycle"
	add_child(_sky_cycle)
	_sky_cycle.setup($Overcast, $SkyFill, ($Environment as WorldEnvironment).environment)
	_audio = AudioDirector.new()
	_audio.name = "AudioDirector"
	add_child(_audio)
	_build_arms()
	_build_interface()
	_layout_hud()
	get_viewport().size_changed.connect(_layout_hud)

	if not ClassDB.class_exists("ScraperXSimulation"):
		_fail_native("SCRAPERX_EXTENSION_LOAD_FAILED", 20)
		return

	_native = ClassDB.instantiate("ScraperXSimulation")
	if _native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return

	if not _uitest_scenario.is_empty():
		_uitest = (load(UI_TEST_DRIVER_PATH) as GDScript).new()
		_uitest.name = "UiTestDriver"
		add_child(_uitest)
		if not _uitest.begin(self, _uitest_scenario, _capture_path):
			push_error("SCRAPERX_UITEST_UNKNOWN_SCENARIO %s" % _uitest_scenario)
			get_tree().quit(30)
			return

	if _ci_mode:
		_yaw = atan2(-CI_APPROACH_FACING.x, -CI_APPROACH_FACING.y)
		_pitch = 0.06

	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim work_order=WO-006")
	print("SCRAPERX_VIEWPORT size=%dx%d aspect=%.3f fov=%.1f far=%.0f" % [
		int(_viewport_size.x), int(_viewport_size.y),
		_viewport_size.x / maxf(1.0, _viewport_size.y), _camera.fov, _camera.far])
	_render_snapshot()
	_ctx = _read_context()
	_fb_traversal = int(_ctx["traversal"])
	_fb_grounded = bool(_ctx["grounded"])
	_fb_deaths = int(_ctx["deaths"])
	_fb_best_checkpoint_y = (_ctx["checkpoint"] as Vector3).y


func _process(delta: float) -> void:
	if _native == null:
		return
	if _fps_label.visible:
		_fps_clock -= delta
		if _fps_clock <= 0.0:
			_fps_clock = 0.5
			_fps_label.text = "%d FPS" % int(Engine.get_frames_per_second())

	var intent: Dictionary = _router.frame(delta)
	if not _ci_mode:
		var look: Vector2 = intent["look"]
		_yaw -= look.x
		_pitch = clampf(_pitch - look.y, -1.25, 1.35)
	_update_view_pitch_offset(delta)

	var position: Vector3 = _native.get_player_position()
	var desired: Vector2 = intent["move"]
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
	_dispatch(intent["verbs"], delta)

	var jib_input: Vector2 = intent["pendant"]
	_native.set_jib_slew_input(jib_input.x)
	_native.set_jib_hoist_input(jib_input.y)
	# The B00 yard jib shares the same pendant axes. Both are station-gated
	# natively, so whichever pendant the player is standing at is the one that
	# responds; sending to both is not sending to both machines.
	_native.set_intake_slew_input(jib_input.x)
	_native.set_intake_hoist_input(jib_input.y)
	# WO-012 KX-NEEDLE pendant: the same Raise/Lower axis as the jib's hoist --
	# the two stations are never in range simultaneously, so reusing it needs
	# no new key binding and keeps the same Raise(+)/Lower(-) verb.
	_native.set_needle_hoist_input(jib_input.y)

	var steps_advanced := int(_native.advance_frame(minf(delta, MAX_SIM_FRAME_DELTA)))
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_render_snapshot(delta)
	_ctx = _read_context()
	_arms.update_arms(_arms_state(intent), _camera.global_transform, delta)
	_update_feedback(delta)
	_touch.update_context(_ctx, delta)
	_hud.family = _router.glyph_family()
	_hud.update_hud(_ctx, delta)
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


# --- interface: devices -> verbs -> native requests ---------------------------


func _build_interface() -> void:
	_settings = SettingsStore.new()
	_settings.persistent = not _ci_mode and _uitest_scenario.is_empty()
	_settings.load_from_disk()
	var hud_layer: CanvasLayer = $HUD
	_hud = Hud.new()
	_hud.name = "Hud"
	hud_layer.add_child(_hud)
	_touch = TouchControls.new()
	_touch.name = "TouchControls"
	hud_layer.add_child(_touch)
	_fps_label = Label.new()
	_fps_label.name = "FpsReadout"
	_fps_label.add_theme_font_override("font", UiStyle.font_digits())
	_fps_label.add_theme_font_size_override("font_size", 26)
	_fps_label.add_theme_color_override("font_color", UiStyle.AMBER)
	_fps_label.set_anchors_and_offsets_preset(Control.PRESET_CENTER_TOP)
	_fps_label.position.y = 12.0
	_fps_label.visible = false
	hud_layer.add_child(_fps_label)
	_router = InputRouter.new()
	_router.name = "InputRouter"
	add_child(_router)
	_router.touch = _touch
	_touch.router = _router
	_router.enabled = not _ci_mode
	_router.capture_mouse = not OS.has_feature("mobile")
	_router.accept_emulated_touch = _force_touch and not OS.has_feature("mobile")
	var pause_layer := CanvasLayer.new()
	pause_layer.name = "PauseLayer"
	pause_layer.layer = 10
	pause_layer.process_mode = Node.PROCESS_MODE_ALWAYS
	add_child(pause_layer)
	_pause_menu = PauseMenu.new()
	_pause_menu.name = "PauseMenu"
	_pause_menu.settings = _settings
	pause_layer.add_child(_pause_menu)

	_router.device_changed.connect(_on_device_changed)
	_router.pad_disconnected.connect(_open_pause.bind(true))
	_touch.pressed_feedback.connect(_haptic.bind(&"press", 1.0))
	_pause_menu.resume_requested.connect(_resume)
	_pause_menu.quit_requested.connect(func() -> void: get_tree().quit(0))
	_pause_menu.settings_changed.connect(_apply_settings)
	# Android's system back opens the pause menu instead of ending the climb.
	get_tree().quit_on_go_back = false
	_router.device = (InputRouter.Device.TOUCH if OS.has_feature("mobile") or _force_touch
		else InputRouter.Device.KEYBOARD_MOUSE)
	_on_device_changed(_router.device)
	_apply_settings()


func _apply_settings() -> void:
	_router.look_sensitivity = _settings.look_sensitivity
	_router.stick_sensitivity = _settings.stick_sensitivity
	_router.invert_y = _settings.invert_y
	_router.gyro_aim = _settings.gyro_aim
	_router.gyro_sensitivity = _settings.gyro_sensitivity
	_touch.set_touch_scale(_settings.touch_scale)
	_set_telemetry_visible(_settings.telemetry or _ci_mode)
	# GRAPHICS
	var viewport := get_viewport()
	viewport.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
	viewport.scaling_3d_scale = _settings.render_scale
	viewport.msaa_3d = [Viewport.MSAA_DISABLED, Viewport.MSAA_2X, Viewport.MSAA_4X][_settings.msaa]
	Engine.max_fps = SettingsStore.FPS_CAPS[_settings.fps_cap]
	($Environment as WorldEnvironment).environment.glow_enabled = _settings.bloom
	_sky_cycle.exposure_scale = _settings.brightness
	_sky_cycle.set_shadow_level(_settings.shadow_quality)
	_fps_label.visible = _settings.show_fps
	# AUDIO
	_audio.set_volumes(_settings.master_volume, _settings.effects_volume,
		_settings.ambience_volume, _settings.interface_volume)
	# DISPLAY
	_fov_base = _settings.fov
	_head_bob_on = _settings.head_bob
	_speed_fov_on = _settings.speed_fov
	if _settings.time_of_day == 0:
		_sky_cycle.day_minutes = _settings.day_minutes
	else:
		_sky_cycle.day_minutes = 0.0
		_sky_cycle.set_hour(SettingsStore.TIME_OF_DAY_HOURS[_settings.time_of_day])


func _on_device_changed(device: int) -> void:
	var touch_active := device == InputRouter.Device.TOUCH
	_touch.visible = touch_active
	_hud.touch_active = touch_active
	_hud.family = _router.glyph_family()


func _set_telemetry_visible(on: bool) -> void:
	_telemetry_on = on
	($HUD/TopLeft as Control).visible = on
	($HUD/TopRight as Control).visible = on
	if on and _native != null:
		_render_snapshot()


func _notification(what: int) -> void:
	match what:
		NOTIFICATION_APPLICATION_PAUSED, NOTIFICATION_APPLICATION_FOCUS_OUT, NOTIFICATION_WM_WINDOW_FOCUS_OUT:
			_open_pause(true)
		NOTIFICATION_WM_GO_BACK_REQUEST:
			if _paused:
				_pause_menu.back()
			else:
				_open_pause(true)


# Pausing stops this node's _process, so no advance_frame call is made: the
# simulation is frozen, not slowed. System-initiated pauses (focus loss, app
# backgrounded, pad unplugged) never fire in proof runs.
func _open_pause(from_system: bool = false) -> void:
	if _paused or _ci_mode or _native == null:
		return
	if from_system and not _uitest_scenario.is_empty():
		return
	_paused = true
	_router.gameplay_active = false
	_router.clear_held()
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	var checkpoint: Vector3 = _ctx["checkpoint"]
	var position: Vector3 = _ctx["position"]
	_pause_menu.open(_router.glyph_family(), "ALTITUDE %+.1f M\nCHECKPOINT %+.1f M\nDEATHS %d" % [
		position.y, checkpoint.y, int(_ctx["deaths"])])
	get_tree().paused = true
	_audio.ui_tap()


func _resume() -> void:
	if not _paused:
		return
	_pause_menu.close()
	_audio.ui_tap(true)
	_settings.save_to_disk()
	get_tree().paused = false
	_paused = false
	_router.clear_held()
	_router.gameplay_active = true
	if _router.device == InputRouter.Device.KEYBOARD_MOUSE and _router.capture_mouse \
			and not _router.accept_emulated_touch and _uitest_scenario.is_empty():
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


# One verb, one native request. Context (last frame's native read) only
# chooses WHICH request a contextual verb means; whether it takes effect is
# always the native's decision.
func _dispatch(verbs: Array, delta: float) -> void:
	for verb in verbs:
		match verb:
			&"jump":
				if _ctx["jump_ok"] or _ctx["hanging"]:
					_native.request_jump()
					_since_jump_sent = 0.0
				else:
					if _since_jump_sent <= DOUBLE_TAP_SECONDS:
						_native.request_jump()
					_jump_buffer = JUMP_BUFFER_SECONDS
			&"action":
				_perform_action()
			&"drop":
				_native.request_release()
			&"chute":
				_native.request_parachute()
			&"back":
				if _ctx["hanging"]:
					_native.request_release()
				elif _operating != &"":
					_operating = &""
			&"alt":
				if _operating == &"intake":
					_request_sling(not bool(_ctx["slung"]))
				elif not _ctx["grounded"]:
					_native.request_parachute()
			&"sling_toggle":
				_request_sling(not bool(_ctx["slung"]))
			&"valve":
				_native.request_valve_toggle()
			&"sling_release":
				_request_sling(false)
			&"sling_attach":
				_request_sling(true)
			&"pause":
				_open_pause()
			&"telemetry":
				_settings.telemetry = not _settings.telemetry
				_set_telemetry_visible(_settings.telemetry or _ci_mode)
	_since_jump_sent += delta
	if _jump_buffer > 0.0:
		if _ctx["jump_ok"]:
			_native.request_jump()
			_since_jump_sent = 0.0
			_jump_buffer = 0.0
		else:
			_jump_buffer = maxf(0.0, _jump_buffer - delta)


# Contextual Action is a gateway (Governing Law 27): it climbs what the
# native affordance reports, opens a pendant's own controls, or works a
# valve. It never operates a machine on the player's behalf.
func _perform_action() -> void:
	var action: Dictionary = _ctx["action"]
	match action["id"]:
		&"climb_up", &"climb":
			_native.request_traversal()
		&"operate":
			_operating = _ctx["station"]
		&"done":
			_operating = &""
		&"valve":
			_native.request_valve_toggle()
		_:
			# Nothing reported in reach: ask anyway, exactly as the old E key
			# did. The native decides there is no ledge (and counts it).
			_native.request_traversal()


func _request_sling(attach: bool) -> void:
	if attach:
		_native.request_intake_sling_attach()
	else:
		_native.request_intake_sling_release()
	# Only at the pendant can the native act at all; there, a request that
	# changes nothing gets said out loud instead of silently ignored.
	if _ctx["station"] == &"intake":
		_sling_check_was_slung = bool(_ctx["slung"])
		_sling_check_timer = SLING_CHECK_SECONDS


func _read_context() -> Dictionary:
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var traversal := int(_native.get_traversal_state())
	var chute := bool(_native.is_parachute_deployed())
	var ledge := bool(_native.is_ledge_available())
	var lethal := float(_native.get_lethal_impact_speed_mps())
	var station := &""
	if bool(_native.is_intake_station_active()):
		station = &"intake"
	elif bool(_native.is_jib_station_active()):
		station = &"jib"
	elif bool(_native.is_needle_station_active()):
		station = &"needle"
	elif bool(_native.is_sump_station_active()):
		station = &"sump"
	var hanging := traversal == TRAVERSAL_HANGING
	var free := traversal == TRAVERSAL_NONE
	if _operating != &"" and (_operating != station or not grounded or not free):
		_operating = &""
	var climb_ok := grounded and free and ledge
	var action := {"id": &"", "label": "", "icon": &"climb", "detail": ""}
	if hanging:
		action = {"id": &"climb_up", "label": "CLIMB UP", "icon": &"climb", "detail": ""}
	elif not free:
		pass
	elif _operating != &"":
		action = {"id": &"done", "label": "DONE", "icon": &"done", "detail": ""}
	elif climb_ok:
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb",
			"detail": "%+.1f M" % float(_native.get_ledge_rise_meters())}
	elif grounded and station in [&"intake", &"jib", &"needle"]:
		action = {"id": &"operate", "label": "OPERATE", "icon": &"operate",
			"detail": {&"intake": "YARD JIB", &"jib": "KX-JIB", &"needle": "KX-NEEDLE"}[station]}
	elif grounded and station == &"sump":
		action = {"id": &"valve", "icon": &"valve", "detail": "SUMP",
			"label": "OPEN VALVE" if bool(_native.is_sump_isolated()) else "CLOSE VALVE"}
	return {
		"position": position,
		"velocity": velocity,
		"grounded": grounded,
		"traversal": traversal,
		"hanging": hanging,
		"chute": chute,
		"jump_ok": grounded and free,
		"climb_ok": climb_ok,
		# Airborne, is_ledge_available is the native hang probe: an edge in
		# the grab band, which engages as soon as the player pushes into it.
		"grab_hint": not grounded and free and ledge,
		"chute_ok": not grounded and free and (chute or -velocity.y > CHUTE_OFFER_FALL_MPS),
		"danger": 0.0 if grounded else clampf(-velocity.y / maxf(lethal, 0.001), 0.0, 1.0),
		"lethal": lethal,
		"station": station,
		"operating": _operating,
		"slung": bool(_native.is_legal_forty_pack_slung()),
		"action": action,
		"checkpoint": _native.get_checkpoint_position(),
		"deaths": int(_native.get_death_count()),
		"tower_height": float(_native.get_tower_height_meters()),
		"panel": _station_panel(),
	}


# The operated machine's own readouts, straight from its native getters.
# Row tone: 0 plain, 1 safe/engaged, 2 hazard.
func _station_panel() -> Dictionary:
	match _operating:
		&"intake":
			var throat_clear := bool(_native.is_intake_throat_clear())
			var pins := bool(_native.does_intake_pack_pin_dog())
			var travel := float(_native.get_legal_forty_swing_travel_radians())
			var slung := bool(_native.is_legal_forty_pack_slung())
			return {
				"title": "YARD JIB PENDANT",
				"subtitle": "B00 INTAKE RISE  /  MOD-YARD-JIB",
				"rows": [
					["BOOM", "%+.1f DEG" % rad_to_deg(float(_native.get_intake_boom_angle_radians())), 0],
					["PACK HEIGHT", "%.2f M" % float(_native.get_intake_pack_position().y), 0],
					["DOG THROAT", "OPEN" if throat_clear else ("PINNED" if pins else "SHUT"),
						1 if throat_clear else 2],
					["LEGAL 40 FLIGHT", "%.0f DEG" % rad_to_deg(travel), 1 if travel >= 0.85 else 0],
					["SLING", "PACK SLUNG" if slung else "FREE", 0],
				],
				"verbs": [
					[&"hoist", "HOIST"], [&"slew", "SLEW"],
					[&"sling_release", "RELEASE PACK"] if slung else [&"sling_attach", "ATTACH PACK"],
					[&"leave", "DONE"],
				],
			}
		&"jib":
			var hook: Vector3 = _native.get_jib_hook_position()
			var crate: Vector3 = _native.get_jib_crate_position()
			return {
				"title": "KX-JIB PENDANT",
				"subtitle": "KERNEL  /  FIRST FREIGHT",
				"rows": [
					["BOOM", "%+.1f DEG" % rad_to_deg(float(_native.get_jib_boom_angle_radians())), 0],
					["HOOK HEIGHT", "%.2f M" % hook.y, 0],
					["CRATE HEIGHT", "%.2f M" % crate.y, 0],
				],
				"verbs": [[&"hoist", "HOIST"], [&"slew", "DRIVE"], [&"leave", "DONE"]],
			}
		&"needle":
			var seated := bool(_native.is_needle_seated())
			return {
				"title": "KX-NEEDLE PENDANT",
				"subtitle": "KERNEL  /  STRUCTURAL COUPLING",
				"rows": [
					["NEEDLE HEIGHT", "%.2f M" % float(_native.get_needle_position().y), 0],
					["SEAT", "SEATED" if seated else "UNSEATED", 1 if seated else 2],
				],
				"verbs": [[&"hoist", "RAISE / LOWER"], [&"leave", "DONE"]],
			}
	return {}


const HAPTICS := {
	&"press": [12, 0.3],
	&"tick": [16, 0.35],
	&"grab": [30, 0.6],
	&"land": [34, 0.85],
	&"chute": [45, 0.7],
	&"warn": [80, 0.9],
	&"death": [150, 1.0],
}


func _haptic(kind: StringName, strength: float = 1.0) -> void:
	if _ci_mode or not _settings.vibration or not HAPTICS.has(kind):
		return
	var spec: Array = HAPTICS[kind]
	var duration_ms := int(float(spec[0]) * lerpf(0.6, 1.0, strength))
	var amplitude := clampf(float(spec[1]) * strength, 0.05, 1.0)
	match _router.device:
		InputRouter.Device.TOUCH:
			if OS.has_feature("mobile"):
				Input.vibrate_handheld(duration_ms, amplitude)
		InputRouter.Device.GAMEPAD:
			if _router.active_pad >= 0:
				var heavy := kind in [&"land", &"death", &"warn"]
				Input.start_joy_vibration(_router.active_pad, amplitude * 0.8,
					amplitude if heavy else amplitude * 0.25, float(duration_ms) / 1000.0)


# State transitions become feedback: a grab, a landing, a canopy, a lethal
# fall warning, a restore, a higher checkpoint. Each reads native state only.
func _update_feedback(delta: float) -> void:
	var traversal: int = _ctx["traversal"]
	if traversal != _fb_traversal:
		if traversal == TRAVERSAL_HANGING:
			_haptic(&"grab")
		elif traversal != TRAVERSAL_NONE:
			_haptic(&"tick")
		_fb_traversal = traversal
	var grounded: bool = _ctx["grounded"]
	var velocity: Vector3 = _ctx["velocity"]
	var lethal: float = _ctx["lethal"]
	# The native velocity read on the last airborne frame. Not the native's
	# last_impact_speed: after a restore the player re-settles within a tick
	# and that value is overwritten with the settle before any HUD sees it.
	var fall_speed_before := _fb_fall_speed
	if grounded and not _fb_grounded and fall_speed_before > 5.0:
		_haptic(&"land", clampf(fall_speed_before / maxf(lethal, 0.001), 0.25, 1.0))
	_fb_grounded = grounded
	_fb_fall_speed = 0.0 if grounded else maxf(0.0, -velocity.y)

	var deaths: int = _ctx["deaths"]
	if deaths > _fb_deaths:
		_fb_deaths = deaths
		_operating = &""
		var restored: Vector3 = _ctx["checkpoint"]
		_hud.toast("LETHAL IMPACT", "FELL AT %.1f M/S  /  RESTORED TO CHECKPOINT %+.1f M" % [
			fall_speed_before, restored.y], UiStyle.HAZARD, 3.4)
		_hud.flash(UiStyle.HAZARD)
		_haptic(&"death")

	var chute: bool = _ctx["chute"]
	if chute and not _fb_chute:
		_haptic(&"chute")
	_fb_chute = chute
	if grounded:
		_fb_warned = false
	elif not chute and float(_ctx["danger"]) >= 0.75 and not _fb_warned:
		_fb_warned = true
		_haptic(&"warn")

	var checkpoint_y: float = (_ctx["checkpoint"] as Vector3).y
	if grounded and checkpoint_y > _fb_best_checkpoint_y + CHECKPOINT_TOAST_RISE_METERS:
		_fb_best_checkpoint_y = checkpoint_y
		_hud.toast("CHECKPOINT", "%+.1f M  SECURED" % checkpoint_y, UiStyle.SAFE)
		_hud.show_altimeter(4.0)

	if _sling_check_timer > 0.0:
		_sling_check_timer -= delta
		if _sling_check_timer <= 0.0 and bool(_ctx["slung"]) == _sling_check_was_slung:
			if _sling_check_was_slung:
				_hud.toast("RELEASE REFUSED", "PACK NOT SEATED AND SETTLED ON THE CRADLE",
					UiStyle.AMBER, 2.6)
			else:
				_hud.toast("ATTACH REFUSED", "HOOK NOT SETTLED ON THE PADEYE", UiStyle.AMBER, 2.6)


# --- first-person arms ----------------------------------------------------------


func _update_view_pitch_offset(delta: float) -> void:
	var target := 0.0
	match int(_ctx.get("traversal", TRAVERSAL_NONE)):
		TRAVERSAL_HANGING:
			target = VIEW_PITCH_HANGING
		TRAVERSAL_MANTLING:
			target = VIEW_PITCH_MANTLING
		TRAVERSAL_VAULTING:
			target = VIEW_PITCH_VAULTING
	_view_pitch_offset = lerpf(_view_pitch_offset, target, 1.0 - exp(-VIEW_PITCH_RATE * delta))
	if absf(_view_pitch_offset - target) < 0.0005:
		_view_pitch_offset = target


func _arm_material(color: Color, roughness: float, bump: NoiseTexture2D, grain: float,
		emission_energy: float = 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	if emission_energy > 0.0:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = emission_energy
	if bump != null:
		# The world's own procedural normal noise, at weave/grain scale.
		material.normal_enabled = true
		material.normal_texture = bump
		material.uv1_triplanar = true
		material.uv1_scale = Vector3(grain, grain, grain)
	return material


func _build_arms() -> void:
	_arms = FirstPersonArms.new()
	_arms.name = "FirstPersonArms"
	add_child(_arms)
	_arms.build(
		# Albedos sit dark on purpose: the yard's exposure lifts anything
		# this close to the lens toward pastel.
		_arm_material(Color("1f231c"), 0.93, _bump_concrete, 9.0),
		_arm_material(Color("b8611d"), 0.55, null, 1.0, 0.1),
		_arm_material(Color("3b2616"), 0.72, _bump_steel, 14.0),
		_arm_material(Color("22170e"), 0.82, _bump_steel, 14.0),
		_arm_material(Color("7d5806"), 0.5, null, 1.0),
		_arm_material(Color("0c0c0d"), 0.62, null, 1.0),
		_arm_material(Color("303033"), 0.5, null, 1.0),
		_arm_material(Color("b8281b"), 0.45, null, 1.0),
		_arm_material(Color("59e08a"), 0.4, null, 1.0, 2.2),
		_arm_material(Color("d7d2c4"), 0.72, null, 1.0))


# Everything the arms may know, all of it native or already-presented state.
func _arms_state(intent: Dictionary) -> Dictionary:
	return {
		"traversal": int(_ctx["traversal"]),
		"progress": float(_native.get_traversal_progress()),
		"ledge_point": _native.get_traversal_ledge_point(),
		"affordance": bool(_native.is_ledge_available()),
		"affordance_point": _native.get_ledge_point(),
		"position": _ctx["position"],
		"velocity": _ctx["velocity"],
		"grounded": bool(_ctx["grounded"]),
		"chute": bool(_ctx["chute"]),
		"operating": _operating,
		"pendant": intent["pendant"],
		"move": intent["move"],
		"bob_phase": _cam_bob_phase,
	}


# --- telemetry overlay, sized to the bounds the device actually gives us ------


func _layout_hud() -> void:
	_viewport_size = get_viewport().get_visible_rect().size
	var short_edge := minf(_viewport_size.x, _viewport_size.y)
	var scale := clampf(short_edge / 1100.0, 0.62, 1.7)
	var gutter := roundf(30.0 * scale)

	# Every readout, not just the first eight: the machine lines were left out
	# of the scaling pass, so at Fold scale they kept their authored size while
	# the VBox stayed at its authored height, and the box squeezed thirteen
	# children into room for eight. They overlapped into an unreadable stack.
	var readouts := [_status, _position_value, _velocity_value, _support_value,
			_traversal_value, _machine_value, _plant_value, _tick_value,
			_fall_value, _jib_value, _needle_value, _sump_value, _intake_value,
			_legal_forty_value]
	for label in readouts:
		if label == null:
			continue
		var base := 24.0 if label == _status else 15.0
		label.add_theme_font_size_override("font_size", int(roundf(base * scale)))

	# Below the touch pause button, which owns the top-left corner.
	var top_left: Control = $HUD/TopLeft
	top_left.offset_left = gutter
	top_left.offset_top = gutter + roundf(150.0 * short_edge / 1856.0)
	top_left.offset_right = gutter + _viewport_size.x * 0.52
	# Size the box to what it actually has to hold, so the container never
	# compresses a line out of legibility.
	var line_height := roundf(15.0 * scale) + 9.0
	top_left.offset_bottom = top_left.offset_top + roundf(24.0 * scale) + 9.0 \
		+ line_height * float(readouts.size())

	# Top-centre: the top-right corner belongs to the altimeter.
	var top_right: Control = $HUD/TopRight
	top_right.anchor_left = 0.5
	top_right.anchor_right = 0.5
	top_right.offset_left = -_viewport_size.x * 0.2
	top_right.offset_right = _viewport_size.x * 0.2
	top_right.offset_top = gutter
	for child in top_right.get_children():
		if child is Label:
			(child as Label).horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER


# --- presentation mirror ----------------------------------------------------


# Every term is a pure function of THIS frame's native state (or a bounded
# transient timer that provably reaches exactly zero), so a stationary,
# grounded player always reads back exactly EYE_OFFSET / FOV_BASE -- see this
# file's own header comment on the constants block above for why that must
# hold for the CI runtime proof.
func _apply_camera_feel(position: Vector3, velocity: Vector3, grounded: bool,
		delta: float) -> void:
	if grounded and not _cam_was_grounded:
		var impact := absf(_cam_last_velocity_y)
		_cam_landing_timer = LANDING_DIP_DURATION_SECONDS
		_cam_landing_strength = smoothstep(LANDING_DIP_MIN_IMPACT_MPS,
			LANDING_DIP_FULL_IMPACT_MPS, impact)
	_cam_was_grounded = grounded
	_cam_last_velocity_y = velocity.y

	var dip := 0.0
	if _cam_landing_timer > 0.0:
		_cam_landing_timer = maxf(0.0, _cam_landing_timer - delta)
		var t := 1.0 - _cam_landing_timer / LANDING_DIP_DURATION_SECONDS
		dip = -_cam_landing_strength * LANDING_DIP_MAX_METERS * sin(PI * t)

	var horizontal_speed := Vector2(velocity.x, velocity.z).length()
	# 5.5 mirrors kPlayerMaximumRelativeSpeed (src/sim/simulation.cpp) -- a
	# curve-shape input, not a gameplay bound, so the native constant is not
	# exposed through the bridge just for this.
	var bob_fade := 0.0
	if grounded:
		bob_fade = smoothstep(HEAD_BOB_SPEED_FLOOR_MPS, HEAD_BOB_SPEED_FULL_MPS, horizontal_speed)
		_cam_bob_phase += horizontal_speed * HEAD_BOB_CYCLES_PER_METER * TAU * delta
	if not _head_bob_on:
		bob_fade = 0.0
	var vertical_bob := HEAD_BOB_VERTICAL_METERS * sin(_cam_bob_phase) * bob_fade
	var lateral_bob := HEAD_BOB_LATERAL_METERS * sin(_cam_bob_phase * 0.5) * bob_fade
	var right_vector := Vector3(cos(_yaw), 0.0, -sin(_yaw))

	_camera.position = position + EYE_OFFSET + Vector3(0.0, dip + vertical_bob, 0.0) + \
		right_vector * lateral_bob
	_camera.rotation = Vector3(_pitch + _view_pitch_offset, _yaw, 0.0)

	var fov_ground := FOV_SPRINT_MAX_DEGREES * smoothstep(0.0, 5.5, horizontal_speed)
	var fov_fall := 0.0
	if not grounded and velocity.y < 0.0:
		fov_fall = FOV_FALL_MAX_DEGREES * smoothstep(0.0, FOV_FALL_FULL_MPS, -velocity.y)
	if not _speed_fov_on:
		fov_ground = 0.0
		fov_fall = 0.0
	_camera.fov = _fov_base + fov_ground + fov_fall


func _render_snapshot(delta: float = 0.0) -> void:
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())

	_apply_camera_feel(position, velocity, grounded, delta)
	if delta > 0.0:
		_audio.update(delta, position, velocity, grounded, int(_native.get_support_entity_id()),
			int(_native.get_traversal_state()), bool(_native.is_parachute_deployed()),
			int(_native.get_death_count()))
	# The developer telemetry overlay costs a dozen string formats a frame;
	# it is only paid for while the overlay is actually on screen.
	if _telemetry_on:
		_write_telemetry(position, velocity, grounded)
	_mirror_machine(float(_native.get_valve_open_fraction()),
		float(_native.get_orifice_mass_flow_kg_per_s()))


func _write_telemetry(position: Vector3, velocity: Vector3, grounded: bool) -> void:
	var support := int(_native.get_support_entity_id())
	var support_velocity: Vector3 = _native.get_support_point_linear_velocity()
	var traversal := int(_native.get_traversal_state())

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

	# AS-001 B00. pins/clear are predicates derived from real poses -- the HUD
	# reports them, nothing in the world obeys them.
	var intake_at_station := bool(_native.is_intake_station_active())
	var throat_clear := bool(_native.is_intake_throat_clear())
	var pack_pins := bool(_native.does_intake_pack_pin_dog())
	_intake_value.text = "INTAKE    %s  PACK %5.1f m  DOG %5.2f rad  THROAT %s" % [
		"AT PENDANT" if intake_at_station else "away",
		float(_native.get_intake_pack_position().y),
		float(_native.get_intake_dog_angle_radians()),
		"OPEN" if throat_clear else ("pinned" if pack_pins else "shut")]
	_intake_value.modulate = Color("9ad6c4") if throat_clear else Color("d99a4a")

	# AS-002 Legal Forty. travel is the same raw 0 (stowed) .. ~0.9076
	# (deployed) radians the native falsifier itself asserts against; R/G
	# fire the same tolerance-gated commands the pendant test drives.
	var legal_forty_slung := bool(_native.is_legal_forty_pack_slung())
	var legal_forty_travel := float(_native.get_legal_forty_swing_travel_radians())
	_legal_forty_value.text = "LEGAL 40  %s  FLIGHT %5.2f rad  SLUNG %s  CRADLE %5.2f m" % [
		"AT PENDANT" if intake_at_station else "away",
		legal_forty_travel,
		"yes" if legal_forty_slung else "NO",
		float(_native.get_legal_forty_cradle_position().y)]
	_legal_forty_value.modulate = Color("9ad6c4") if legal_forty_travel >= 0.85 else Color("d99a4a")

	if traversal == TRAVERSAL_HANGING:
		_status.text = "HANGING ON NATIVE LEDGE"
	elif traversal == TRAVERSAL_MANTLING:
		_status.text = "MANTLING REAL GEOMETRY"
	elif traversal == TRAVERSAL_VAULTING:
		_status.text = "VAULTING REAL GEOMETRY"
	elif intake_at_station:
		_status.text = "AT THE YARD JIB PENDANT / ARROWS SLEW-HOIST"
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

	if _intake_boom_pivot != null:
		_intake_boom_pivot.rotation = Vector3(
			0.0, float(_native.get_intake_boom_angle_radians()), 0.0)
	if _intake_hook_mesh != null:
		_intake_hook_mesh.position = _native.get_intake_hook_position()
	if _intake_pack_mesh != null:
		_intake_pack_mesh.position = _native.get_intake_pack_position()
	if _intake_overweight_mesh != null:
		_intake_overweight_mesh.position = _native.get_intake_overweight_pack_position()
	if _intake_dog_pivot != null:
		_intake_dog_pivot.rotation = Vector3(
			0.0, float(_native.get_intake_dog_angle_radians()), 0.0)
	if _intake_hoist_cable != null:
		var boom_yaw := float(_native.get_intake_boom_angle_radians())
		var boom_tip := Vector3(0.0, INTAKE_BOOM_HEIGHT, INTAKE_JIB_MAST_Z) + Vector3(
			0.0, 0.0, -INTAKE_BOOM_LENGTH).rotated(Vector3.UP, boom_yaw)
		_span_cable(_intake_hoist_cable,
			_native.get_intake_hook_position() + Vector3(0.0, 0.2, 0.0), boom_tip)
	if _intake_belt_mesh != null:
		# The belt is native-kinematic; its stroke is authoritative, so the
		# mesh follows the body rather than running its own animation.
		var belt_phase: float = sin(0.40 * float(_native.get_simulation_time_seconds()))
		_intake_belt_mesh.position = Vector3(INTAKE_BELT_X, INTAKE_BELT_TOP_Y - 0.18,
			-96.0 + 9.0 * belt_phase)

	# AS-002 Legal Forty. The pivot sits at the hinge itself (built once,
	# above); only its own rotation is native-driven, exactly like the dog
	# plate and the yard jib boom. travel runs 0 (stowed) .. ~0.9076
	# (deployed); simulation.cpp's own build comment: the body is built at
	# stowed_phi = pi/2 - theta_stowed, and departing the stowed limit is a
	# NEGATIVE rotation about +Z, so the live angle is stowed_phi - travel.
	if _legal_forty_swing_flight_pivot != null:
		var legal_forty_travel := float(_native.get_legal_forty_swing_travel_radians())
		var stowed_phi := PI * 0.5 - LEGAL_FORTY_STOWED_THETA
		_legal_forty_swing_flight_pivot.rotation = Vector3(0.0, 0.0, stowed_phi - legal_forty_travel)
	if _legal_forty_cradle_mesh != null:
		_legal_forty_cradle_mesh.position = _native.get_legal_forty_cradle_position()
	if _legal_forty_rope_flight != null and _legal_forty_swing_flight_pivot != null:
		var bracket_local := Vector3(
			LEGAL_FORTY_SWING_BRACKET_LOCAL_X - LEGAL_FORTY_FLIGHT_HALF_LENGTH,
			LEGAL_FORTY_SWING_BRACKET_LOCAL_Y, 0.0)
		var bracket_world: Vector3 = (
			_legal_forty_swing_flight_pivot.global_transform * bracket_local)
		_span_cable(_legal_forty_rope_flight, bracket_world, _legal_forty_sheave_a)
	if _legal_forty_rope_cradle != null:
		var cradle_top: Vector3 = _native.get_legal_forty_cradle_position() + Vector3(
			0.0, LEGAL_FORTY_CW_CRADLE_HALF_Y, 0.0)
		_span_cable(_legal_forty_rope_cradle, cradle_top, _legal_forty_sheave_b)

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
	# The neighbouring shaft stands on the ground: 1.6 km from grade up.
	_add_box("TowerMass", Vector3(92.0, 1600.0, 80.0), Vector3(-30.0, 800.0, -330.0), concrete)

	_build_stack(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded_yellow, timber)
	_build_stack_accents(verdigris, machine_blue, lichen)
	_build_tower_skin(mill_scale, oxidised, galvanised, faded_yellow, timber)
	_build_intake_rise(concrete, mill_scale, oxidised, rust_deep, rust_bright,
		galvanised, faded_yellow, hazard, timber)
	_build_legal_forty(concrete, mill_scale, oxidised, rust_deep, rust_bright,
		galvanised, faded_yellow, hazard, timber)
	_build_yard(concrete, mill_scale, faded_yellow, tar)
	_build_legacy_fixtures(mill_scale, galvanised, hazard, faded_yellow)
	_build_plant(mill_scale, oxidised, galvanised, hazard, faded_yellow)
	_build_mountains_and_waterfall()
	_build_kellerworks_signage(timber, faded_yellow)
	_build_gear_motif(mill_scale, oxidised)
	_build_crane(mill_scale, hazard)
	_build_foliage()
	_build_lighting()



# AS-001: MOD-APRON, MOD-INTAKE-BELT, CAP-PENDANT, MOD-YARD-JIB, the 4 t pack,
# MOD-DOG-A, MOD-STAIR-A, the +24 m handoff, and MOD-SKIN-LADDER-S. This is the
# bottom of the real ascent: the player starts on the apron and the first 24 m
# of the climb is won by moving freight, not by finding a ladder.
#
# Bodies that the native simulation owns get a mesh here and nothing else --
# their transforms come from _render_snapshot(). Everything static mirrors the
# native half-extents doubled.
func _build_intake_rise(concrete: Material, mill_scale: Material, oxidised: Material,
		rust_deep: Material, rust_bright: Material, galvanised: Material,
		faded: Material, hazard: Material, timber: Material) -> void:
	var bay := Node3D.new()
	bay.name = "IntakeBay"
	$TowerPresentation.add_child(bay)

	var bay_depth := INTAKE_BAY_FRONT_Z - INTAKE_BAY_BACK_Z
	var bay_center_z := (INTAKE_BAY_FRONT_Z + INTAKE_BAY_BACK_Z) * 0.5
	var wall_mid_y := INTAKE_BAY_WALL_HEIGHT * 0.5

	_add_box_to("BayBack", Vector3(INTAKE_BAY_HALF_X * 2.0, INTAKE_BAY_WALL_HEIGHT,
		INTAKE_BAY_WALL_THICKNESS), Vector3(0.0, wall_mid_y, INTAKE_BAY_BACK_Z),
		concrete, bay)
	for sx in [1.0, -1.0]:
		_add_box_to("BaySide%d" % int(sx), Vector3(INTAKE_BAY_WALL_THICKNESS,
			INTAKE_BAY_WALL_HEIGHT, bay_depth),
			Vector3(sx * INTAKE_BAY_HALF_X, wall_mid_y, bay_center_z), concrete, bay)

	var jamb_width := INTAKE_BAY_HALF_X - INTAKE_THROAT_HALF_WIDTH
	for sx in [1.0, -1.0]:
		_add_box_to("BayJamb%d" % int(sx), Vector3(jamb_width, INTAKE_BAY_WALL_HEIGHT,
			INTAKE_BAY_WALL_THICKNESS),
			Vector3(sx * (INTAKE_THROAT_HALF_WIDTH + jamb_width * 0.5), wall_mid_y,
				INTAKE_BAY_FRONT_Z), concrete, bay)
	var lintel_height := INTAKE_BAY_WALL_HEIGHT - INTAKE_THROAT_HEIGHT
	_add_box_to("BayLintel", Vector3(INTAKE_THROAT_HALF_WIDTH * 2.0, lintel_height,
		INTAKE_BAY_WALL_THICKNESS),
		Vector3(0.0, INTAKE_THROAT_HEIGHT + lintel_height * 0.5, INTAKE_BAY_FRONT_Z),
		concrete, bay)
	_add_sign_text("STAIR A", Vector3(0.0, 4.1, INTAKE_BAY_FRONT_Z + 0.4), 0.0, 0.5,
		Color("d8b45a"), bay)

	# MOD-DOG-A. The pivot sits on the hinge line so the plate swings exactly
	# as the native hinge does; _render_snapshot drives its yaw.
	_intake_dog_pivot = Node3D.new()
	_intake_dog_pivot.name = "IntakeDog"
	_intake_dog_pivot.position = Vector3(-INTAKE_DOG_HALF_WIDTH, INTAKE_DOG_CENTER_Y,
		INTAKE_BAY_FRONT_Z)
	bay.add_child(_intake_dog_pivot)
	_add_box_to("DogPlate", Vector3(INTAKE_DOG_HALF_WIDTH * 2.0,
		INTAKE_DOG_HALF_HEIGHT * 2.0, 0.4),
		Vector3(INTAKE_DOG_HALF_WIDTH, 0.0, 0.0), hazard, _intake_dog_pivot)
	for rib in range(3):
		_add_box_to("DogRib%d" % rib, Vector3(0.14, INTAKE_DOG_HALF_HEIGHT * 2.0, 0.5),
			Vector3(0.5 + float(rib) * 0.7, 0.0, 0.0), mill_scale, _intake_dog_pivot)

	# MOD-INTAKE-BELT and the CAP-PENDANT catwalk.
	_intake_belt_mesh = _add_box_to("IntakeBelt", Vector3(4.0, 0.36, 12.0),
		Vector3(INTAKE_BELT_X, INTAKE_BELT_TOP_Y - 0.18, -96.0), galvanised, bay)
	for slat in range(9):
		_add_box_to("BeltSlat%d" % slat, Vector3(4.2, 0.1, 0.5),
			Vector3(0.0, 0.22, -5.0 + float(slat) * 1.25), mill_scale, _intake_belt_mesh)
	_add_box_to("PendantCatwalk", Vector3(3.2, 0.36, 6.0),
		Vector3(INTAKE_PENDANT_X, INTAKE_BELT_TOP_Y - 0.18, INTAKE_PENDANT_Z),
		galvanised, bay)
	var ramp := _add_box_to("PendantRamp", Vector3(3.2, 0.3, 4.24),
		Vector3(INTAKE_PENDANT_X, INTAKE_BELT_TOP_Y * 0.5, INTAKE_PENDANT_Z + 5.0),
		galvanised, bay)
	ramp.rotation = Vector3(atan2(INTAKE_BELT_TOP_Y, 4.0), 0.0, 0.0)
	_add_box_to("PendantStand", Vector3(0.4, 1.1, 0.4),
		Vector3(INTAKE_PENDANT_X - 1.0, INTAKE_BELT_TOP_Y + 0.55, INTAKE_PENDANT_Z),
		mill_scale, bay)
	_add_box_to("PendantBox", Vector3(0.5, 0.6, 0.3),
		Vector3(INTAKE_PENDANT_X - 1.0, INTAKE_BELT_TOP_Y + 1.35, INTAKE_PENDANT_Z),
		faded, bay)

	# MOD-YARD-JIB: 12 m boom, 11.5 m under the boom, 5 t SWL.
	_add_box_to("YardJibMast", Vector3(0.9, INTAKE_BOOM_HEIGHT, 0.9),
		Vector3(0.0, INTAKE_BOOM_HEIGHT * 0.5, INTAKE_JIB_MAST_Z), oxidised, bay)
	for brace in range(4):
		var brace_y := 2.0 + float(brace) * 2.6
		_add_strut("MastBrace%d" % brace,
			Vector3(0.0, brace_y, INTAKE_JIB_MAST_Z),
			Vector3(2.4, brace_y - 2.2, INTAKE_JIB_MAST_Z + 2.4), 0.16, rust_deep, bay)
	_intake_boom_pivot = Node3D.new()
	_intake_boom_pivot.name = "YardJibBoom"
	_intake_boom_pivot.position = Vector3(0.0, INTAKE_BOOM_HEIGHT, INTAKE_JIB_MAST_Z)
	bay.add_child(_intake_boom_pivot)
	_add_box_to("BoomSpine", Vector3(0.44, 0.44, INTAKE_BOOM_LENGTH),
		Vector3(0.0, 0.0, -INTAKE_BOOM_LENGTH * 0.5), oxidised, _intake_boom_pivot)
	for lattice in range(7):
		var lz := -1.0 - float(lattice) * 1.6
		_add_strut("BoomLattice%d" % lattice, Vector3(0.0, 0.28, lz),
			Vector3(0.0, -0.28, lz - 1.6), 0.09, mill_scale, _intake_boom_pivot)
	_add_box_to("BoomTail", Vector3(0.5, 0.5, 3.0), Vector3(0.0, 0.0, 2.0),
		rust_bright, _intake_boom_pivot)
	_add_box_to("BoomBallast", Vector3(1.4, 1.0, 1.4), Vector3(0.0, -0.2, 3.4),
		mill_scale, _intake_boom_pivot)

	_intake_hoist_cable = Node3D.new()
	_intake_hoist_cable.name = "IntakeHoistCable"
	bay.add_child(_intake_hoist_cable)
	var cable := BoxMesh.new()
	cable.size = Vector3(0.07, 1.0, 0.07)
	cable.material = mill_scale
	var cable_mesh := MeshInstance3D.new()
	cable_mesh.name = "Span"
	cable_mesh.set_meta(&"part", "Span")
	cable_mesh.mesh = cable
	_intake_hoist_cable.add_child(cable_mesh)

	_intake_hook_mesh = _add_box_to("IntakeHook", Vector3(0.4, 0.4, 0.4),
		Vector3(0.0, 2.0, -112.2), galvanised, bay)
	_intake_pack_mesh = _add_box_to("IntakePack", INTAKE_PACK_SIZE,
		Vector3(0.0, 0.9, -112.2), timber, bay)
	_add_box_to("PackBand", Vector3(2.3, 0.14, 2.4), Vector3(0.0, 0.5, 0.0),
		mill_scale, _intake_pack_mesh)
	_add_box_to("PackBandUpper", Vector3(2.3, 0.14, 2.4), Vector3(0.0, -0.5, 0.0),
		mill_scale, _intake_pack_mesh)

	# The 9 t proof load on its own stand: rated force, permanently commanded
	# up, permanently stalled.
	_add_box_to("OverweightMast", Vector3(0.7, 9.0, 0.7),
		INTAKE_OVERWEIGHT_STAND + Vector3(0.0, 4.5, 0.0), oxidised, bay)
	_intake_overweight_mesh = _add_box_to("OverweightPack", INTAKE_PACK_SIZE,
		INTAKE_OVERWEIGHT_STAND + Vector3(0.0, 0.9, 0.0), rust_deep, bay)

	# MOD-STAIR-A: six flights in two lanes, landings at each turn.
	var pitch := atan2(INTAKE_STAIR_FLIGHT_RISE, INTAKE_STAIR_HALF_RUN * 2.0)
	var flight_length := sqrt(pow(INTAKE_STAIR_HALF_RUN * 2.0, 2.0)
		+ pow(INTAKE_STAIR_FLIGHT_RISE, 2.0))
	var tread_surface := 0.18 / cos(pitch)
	for flight in range(INTAKE_STAIR_FLIGHT_COUNT):
		var base_y := float(flight) * INTAKE_STAIR_FLIGHT_RISE
		var side := 1.0 if flight % 2 == 0 else -1.0
		var lane_z := INTAKE_STAIR_Z + side * INTAKE_STAIR_LANE_OFFSET
		var slab := _add_box_to("StairFlight%d" % flight,
			Vector3(flight_length, 0.36, INTAKE_STAIR_WIDTH),
			Vector3(0.0, base_y + INTAKE_STAIR_FLIGHT_RISE * 0.5, lane_z),
			mill_scale, bay, side * pitch)
		# Treads drawn on the slab, so what you see and what you stand on agree.
		for tread in range(11):
			var along := -flight_length * 0.5 + 0.7 + float(tread) * 1.3
			_add_box_to("Tread%d" % tread, Vector3(1.0, 0.1, INTAKE_STAIR_WIDTH - 0.2),
				Vector3(along, 0.22, 0.0), galvanised, slab)
		for rail in [1.0, -1.0]:
			_add_box_to("StairRail%d" % int(rail), Vector3(flight_length, 0.08, 0.08),
				Vector3(0.0, 1.05, rail * (INTAKE_STAIR_WIDTH * 0.5 - 0.1)), faded, slab)
	for flight in range(INTAKE_STAIR_FLIGHT_COUNT + 1):
		var base_y := float(flight) * INTAKE_STAIR_FLIGHT_RISE
		var side := 1.0 if flight % 2 == 0 else -1.0
		_add_box_to("StairLanding%d" % flight, Vector3(2.0, 0.36,
			(INTAKE_STAIR_LANE_OFFSET + INTAKE_STAIR_WIDTH * 0.5) * 2.0),
			Vector3(-side * INTAKE_STAIR_LANDING_X, base_y + tread_surface - 0.18,
				INTAKE_STAIR_Z), galvanised, bay)

	# The +24 m handoff. Both braids arrive here.
	var handoff_depth := INTAKE_HANDOFF_SOUTH_Z - INTAKE_HANDOFF_NORTH_Z
	_add_box_to("IntakeHandoff", Vector3(8.0, 0.4, handoff_depth),
		Vector3(INTAKE_HANDOFF_CENTER_X, INTAKE_HANDOFF_Y + tread_surface - 0.2,
			(INTAKE_HANDOFF_SOUTH_Z + INTAKE_HANDOFF_NORTH_Z) * 0.5), timber, bay)
	_add_sign_text("+24", Vector3(INTAKE_HANDOFF_CENTER_X, INTAKE_HANDOFF_Y + 1.6,
		INTAKE_HANDOFF_SOUTH_Z - 0.2), 0.0, 0.9, Color("c8a04a"), bay)

	# MOD-SKIN-LADDER-S: the exposed bypass, stepping north up the apron.
	var head_z := INTAKE_HANDOFF_SOUTH_Z + INTAKE_SKIN_RUNG_HALF_Z
	for rung in range(1, INTAKE_SKIN_RUNG_COUNT + 1):
		var top_y := float(rung) * INTAKE_SKIN_RUNG_RISE
		var rung_z := head_z + 2.0 * INTAKE_SKIN_RUNG_HALF_Z * float(
			INTAKE_SKIN_RUNG_COUNT - rung)
		_add_box_to("SkinRung%d" % rung, Vector3(2.0, 1.0, INTAKE_SKIN_RUNG_HALF_Z * 2.0),
			Vector3(INTAKE_HANDOFF_CENTER_X, top_y - 0.5, rung_z), rust_deep, bay)
		_add_box_to("SkinRungPlate%d" % rung, Vector3(2.1, 0.08, 1.9),
			Vector3(INTAKE_HANDOFF_CENTER_X, top_y + 0.02, rung_z), galvanised, bay)
		if rung % 3 == 0:
			_add_strut("SkinStay%d" % rung,
				Vector3(INTAKE_HANDOFF_CENTER_X - 1.0, top_y, rung_z),
				Vector3(INTAKE_HANDOFF_CENTER_X - 2.6, top_y - 3.0, rung_z + 1.2),
				0.12, oxidised, bay)


# AS-002 Legal Forty: the counterweighted-bascule flight and cradle that
# open band B00's own 24-40 m leftover, plus the static run above it --
# mid-landing, upper flight, MOD-HALL-DECK and its well, and the SKIN
# ladder's own continuation. Geometry mirrors simulation.cpp's own
# build_legal_forty one-for-one; this function only draws it.
func _build_legal_forty(concrete: Material, mill_scale: Material, oxidised: Material,
		rust_deep: Material, rust_bright: Material, galvanised: Material,
		faded: Material, hazard: Material, timber: Material) -> void:
	var forty := Node3D.new()
	forty.name = "LegalForty"
	$TowerPresentation.add_child(forty)

	# ---- MOD-STAIR-A-SWING: the dynamic bascule flight, its rotation driven
	# by _mirror_machine every frame -- the pivot sits at the hinge itself,
	# exactly like IntakeDog and YardJibBoom above, so only its own rotation
	# needs updating; the slab (and everything hung on it) follows for free.
	_legal_forty_swing_flight_pivot = Node3D.new()
	_legal_forty_swing_flight_pivot.name = "SwingFlightPivot"
	_legal_forty_swing_flight_pivot.position = Vector3(
		LEGAL_FORTY_HINGE_X, LEGAL_FORTY_MID_LANDING_SURFACE_Y, LEGAL_FORTY_HINGE_Z)
	forty.add_child(_legal_forty_swing_flight_pivot)
	var flight_length := LEGAL_FORTY_FLIGHT_HALF_LENGTH * 2.0
	var flight_slab := _add_box_to("SwingFlightSlab",
		Vector3(flight_length, 0.36, LEGAL_FORTY_FLIGHT_HALF_WIDTH * 2.0),
		Vector3(-LEGAL_FORTY_FLIGHT_HALF_LENGTH, 0.0, 0.0), mill_scale,
		_legal_forty_swing_flight_pivot)
	for tread in range(6):
		var along := -flight_length + 1.4 + float(tread) * 2.4
		_add_box_to("SwingTread%d" % tread,
			Vector3(1.0, 0.1, LEGAL_FORTY_FLIGHT_HALF_WIDTH * 2.0 - 0.2),
			Vector3(along, 0.22, 0.0), galvanised, flight_slab)
	for rail in [1.0, -1.0]:
		_add_box_to("SwingRail%d" % int(rail), Vector3(flight_length, 0.08, 0.08),
			Vector3(0.0, 1.05, rail * (LEGAL_FORTY_FLIGHT_HALF_WIDTH - 0.1)), faded, flight_slab)

	# ---- MOD-CW-CRADLE: dynamic car on a free vertical slider, plus its
	# guide mast -- cradle_x/z read off the same yard-jib bearing formula
	# simulation.cpp uses (same boom, same mast, MOD-YARD-JIB's own).
	# kIntakeJibMastX is kIntakeBayCenterX (0.0) natively -- the yard jib's
	# own mast sits on the bay's own centreline.
	var cradle_x := 0.0 + INTAKE_BOOM_LENGTH * sin(LEGAL_FORTY_CW_CRADLE_BEARING)
	var cradle_z := INTAKE_JIB_MAST_Z - INTAKE_BOOM_LENGTH * cos(LEGAL_FORTY_CW_CRADLE_BEARING)
	var guide_mast_z := cradle_z + 2.0
	_add_box_to("CradleGuideMast",
		Vector3(LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_X * 2.0, LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Y * 2.0,
			LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Z * 2.0),
		Vector3(cradle_x, LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Y, guide_mast_z), oxidised, forty)

	# The car: a solid block, exactly the native body. The pack rides on its
	# top face. _render_snapshot drives it from the live cradle every frame.
	_legal_forty_cradle_mesh = _add_box_to("CwCradleCar",
		Vector3(LEGAL_FORTY_CW_CRADLE_HALF_X * 2.0, LEGAL_FORTY_CW_CRADLE_HALF_Y * 2.0,
			LEGAL_FORTY_CW_CRADLE_HALF_Z * 2.0),
		Vector3(cradle_x, LEGAL_FORTY_CW_CRADLE_BUILD_Y, cradle_z), rust_deep, forty)

	# ---- The rope: two segments over two fixed sheaves, matching the
	# native pulley's own two runs (exactly like the treadle's own cable
	# above) -- _mirror_machine spans both every frame from the live flight
	# bracket and cradle top.
	_legal_forty_sheave_a = Vector3(LEGAL_FORTY_HINGE_X,
		LEGAL_FORTY_MID_LANDING_SURFACE_Y + LEGAL_FORTY_SWING_SHEAVE_HEIGHT, LEGAL_FORTY_HINGE_Z)
	_legal_forty_sheave_b = Vector3(cradle_x,
		LEGAL_FORTY_MID_LANDING_SURFACE_Y + LEGAL_FORTY_SWING_SHEAVE_HEIGHT, cradle_z)
	_add_box_to("SheaveA", Vector3(0.3, 0.3, 0.3), _legal_forty_sheave_a, oxidised, forty)
	_add_box_to("SheaveB", Vector3(0.3, 0.3, 0.3), _legal_forty_sheave_b, oxidised, forty)
	_legal_forty_rope_flight = _add_box_to("RopeFlightSide", Vector3(0.05, 0.05, 1.0),
		Vector3.ZERO, mill_scale, forty)
	_legal_forty_rope_cradle = _add_box_to("RopeCradleSide", Vector3(0.05, 0.05, 1.0),
		Vector3.ZERO, mill_scale, forty)

	# ---- MOD-STAIR-A upper flight (static) + mid-landing --------------------
	var upper_flight_pitch := asin(LEGAL_FORTY_FLIGHT_RISE / flight_length)
	_add_box_to("UpperFlightSlab",
		Vector3(flight_length, 0.36, LEGAL_FORTY_FLIGHT_HALF_WIDTH * 2.0),
		Vector3(LEGAL_FORTY_UPPER_FLIGHT_X,
			LEGAL_FORTY_MID_LANDING_SURFACE_Y + LEGAL_FORTY_FLIGHT_RISE * 0.5,
			LEGAL_FORTY_UPPER_FLIGHT_Z),
		mill_scale, forty, -upper_flight_pitch)
	_add_box_to("MidLanding",
		Vector3(LEGAL_FORTY_MID_LANDING_HALF_X * 2.0, 0.36, LEGAL_FORTY_MID_LANDING_HALF_Z * 2.0),
		Vector3(LEGAL_FORTY_MID_LANDING_X, LEGAL_FORTY_MID_LANDING_SURFACE_Y - 0.18,
			LEGAL_FORTY_MID_LANDING_Z),
		galvanised, forty)
	_add_sign_text("+32", Vector3(LEGAL_FORTY_MID_LANDING_X, LEGAL_FORTY_MID_LANDING_SURFACE_Y + 1.6,
		LEGAL_FORTY_MID_LANDING_Z + LEGAL_FORTY_MID_LANDING_HALF_Z - 0.2),
		0.0, 0.9, Color("c8a04a"), forty)

	# ---- MOD-HALL-DECK, four strips tiling the deck minus its well ----------
	var well_min_x := LEGAL_FORTY_WELL_X - LEGAL_FORTY_WELL_HALF_X
	var well_max_x := LEGAL_FORTY_WELL_X + LEGAL_FORTY_WELL_HALF_X
	var well_min_z := LEGAL_FORTY_WELL_Z - LEGAL_FORTY_WELL_HALF_Z
	var well_max_z := LEGAL_FORTY_WELL_Z + LEGAL_FORTY_WELL_HALF_Z
	var deck_min_x := -LEGAL_FORTY_HALL_DECK_HALF_X
	var deck_max_x := LEGAL_FORTY_HALL_DECK_HALF_X
	var deck_min_z := LEGAL_FORTY_HALL_DECK_Z - LEGAL_FORTY_HALL_DECK_HALF_Z
	var deck_max_z := LEGAL_FORTY_HALL_DECK_Z + LEGAL_FORTY_HALL_DECK_HALF_Z
	var deck_center_y := LEGAL_FORTY_HALL_DECK_SURFACE_Y - LEGAL_FORTY_HALL_DECK_HALF_THICKNESS
	var south_cz := (well_max_z + deck_max_z) * 0.5
	var south_hz := (deck_max_z - well_max_z) * 0.5
	_add_box_to("HallDeckSouth", Vector3(LEGAL_FORTY_HALL_DECK_HALF_X * 2.0,
		LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0, south_hz * 2.0),
		Vector3(0.0, deck_center_y, south_cz), timber, forty)
	var north_cz := (deck_min_z + well_min_z) * 0.5
	var north_hz := (well_min_z - deck_min_z) * 0.5
	_add_box_to("HallDeckNorth", Vector3(LEGAL_FORTY_HALL_DECK_HALF_X * 2.0,
		LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0, north_hz * 2.0),
		Vector3(0.0, deck_center_y, north_cz), timber, forty)
	var west_cx := (deck_min_x + well_min_x) * 0.5
	var west_hx := (well_min_x - deck_min_x) * 0.5
	_add_box_to("HallDeckWest", Vector3(west_hx * 2.0, LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0,
		LEGAL_FORTY_WELL_HALF_Z * 2.0),
		Vector3(west_cx, deck_center_y, LEGAL_FORTY_WELL_Z), timber, forty)
	var east_cx := (well_max_x + deck_max_x) * 0.5
	var east_hx := (deck_max_x - well_max_x) * 0.5
	_add_box_to("HallDeckEast", Vector3(east_hx * 2.0, LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0,
		LEGAL_FORTY_WELL_HALF_Z * 2.0),
		Vector3(east_cx, deck_center_y, LEGAL_FORTY_WELL_Z), timber, forty)
	_add_sign_text("+40", Vector3(0.0, LEGAL_FORTY_HALL_DECK_SURFACE_Y + 1.6, deck_max_z - 0.2),
		0.0, 0.9, Color("c8a04a"), forty)

	# ---- MOD-SKIN-LADDER-S continuation, rungs 16-20, and its walkway -------
	for rung in range(LEGAL_FORTY_SKIN_RUNG_FIRST, LEGAL_FORTY_SKIN_RUNG_LAST + 1):
		var top_y := float(rung) * INTAKE_SKIN_RUNG_RISE
		var rung_z := LEGAL_FORTY_SKIN_RUNG_FIRST_Z - LEGAL_FORTY_SKIN_RUNG_STEP_Z * float(
			rung - LEGAL_FORTY_SKIN_RUNG_FIRST)
		var rung_x := INTAKE_HANDOFF_CENTER_X + (LEGAL_FORTY_SKIN_RUNG_JOG_X
			if rung > LEGAL_FORTY_SKIN_RUNG_FIRST else 0.0)
		_add_box_to("SkinRung%d" % rung, Vector3(2.0, 1.0, INTAKE_SKIN_RUNG_HALF_Z * 2.0),
			Vector3(rung_x, top_y - 0.5, rung_z), rust_deep, forty)
		_add_box_to("SkinRungPlate%d" % rung, Vector3(2.1, 0.08, 1.9),
			Vector3(rung_x, top_y + 0.02, rung_z), galvanised, forty)
		if rung % 3 == 0:
			_add_strut("SkinStay%d" % rung,
				Vector3(rung_x - 1.0, top_y, rung_z),
				Vector3(rung_x - 2.6, top_y - 3.0, rung_z + 1.2),
				0.12, oxidised, forty)
	var walk_cx := (LEGAL_FORTY_SKIN_WALKWAY_MIN_X + LEGAL_FORTY_SKIN_WALKWAY_MAX_X) * 0.5
	var walk_hx := (LEGAL_FORTY_SKIN_WALKWAY_MAX_X - LEGAL_FORTY_SKIN_WALKWAY_MIN_X) * 0.5
	_add_box_to("SkinWalkway", Vector3(walk_hx * 2.0, 0.36, LEGAL_FORTY_SKIN_WALKWAY_HALF_Z * 2.0),
		Vector3(walk_cx, LEGAL_FORTY_MID_LANDING_SURFACE_Y - 0.18, LEGAL_FORTY_SKIN_WALKWAY_Z),
		mill_scale, forty)


# The deck band a flight arrives through, as Rect2 pieces in (x, z) about the
# stack centre with z measured outward along the band's own side: full depth
# before and after the well along x, two strips beside it.
func _stairwell_band_pieces(well_side: float, band_center: float, flight_head: float) -> Array:
	var band_in := band_center - STACK_DECK_BAND_DEPTH * 0.5
	var band_out := band_center + STACK_DECK_BAND_DEPTH * 0.5
	var well_in := band_center - STACK_STAIRWELL_HALF_WIDTH
	var well_out := band_center + STACK_STAIRWELL_HALF_WIDTH
	var spans := [
		[-STACK_HALF_EXTENT, STACK_STAIRWELL_START, band_in, band_out],
		[flight_head, STACK_HALF_EXTENT, band_in, band_out],
		[STACK_STAIRWELL_START, flight_head, band_in, well_in],
		[STACK_STAIRWELL_START, flight_head, well_out, band_out],
	]
	var pieces := []
	for span in spans:
		var x0: float = well_side * span[0]
		var x1: float = well_side * span[1]
		pieces.append(Rect2(minf(x0, x1), span[2], absf(x1 - x0), span[3] - span[2]))
	return pieces


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

	var flight_head := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH
	for level in range(1, STACK_LEVEL_COUNT + 1):
		var deck_y := float(level) * STACK_LEVEL_HEIGHT
		var slab_y := deck_y - STACK_DECK_THICKNESS * 0.5
		var deck_material: Material = galvanised if level % 2 == 1 else mill_scale
		var well_side := 1.0 if (level - 1) % 2 == 0 else -1.0

		for sz in [1.0, -1.0]:
			if sz != well_side:
				_add_box("StackDeck", Vector3(STACK_HALF_EXTENT * 2.0, STACK_DECK_THICKNESS,
					STACK_DECK_BAND_DEPTH), Vector3(cx, slab_y, cz + sz * band_center), deck_material)
				continue
			for piece in _stairwell_band_pieces(well_side, band_center, flight_head):
				_add_box("StackDeck", Vector3(piece.size.x, STACK_DECK_THICKNESS, piece.size.y),
					Vector3(cx + piece.get_center().x, slab_y, cz + sz * piece.get_center().y),
					deck_material)
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

		# Timber decking planks laid over the walking band, warm against iron;
		# broken where the stairwell opens.
		for plank in range(-2, 3):
			for sz in [1.0, -1.0]:
				var plank_z := band_center + float(plank) * 1.35
				var over_well: bool = sz == well_side \
					and absf(plank_z - band_center) - 0.55 < STACK_STAIRWELL_HALF_WIDTH
				if not over_well:
					_add_box("DeckPlank", Vector3(STACK_HALF_EXTENT * 2.0 - 2.0, 0.08, 1.1),
						Vector3(cx, deck_y + 0.05, cz + sz * plank_z), timber)
					continue
				for run in [[-STACK_HALF_EXTENT + 1.0, STACK_STAIRWELL_START],
						[flight_head, STACK_HALF_EXTENT - 1.0]]:
					var x0: float = well_side * run[0]
					var x1: float = well_side * run[1]
					_add_box("DeckPlank", Vector3(absf(x1 - x0), 0.08, 1.1),
						Vector3(cx + (x0 + x1) * 0.5, deck_y + 0.05, cz + sz * plank_z), timber)

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
		# Set down by its half-thickness along its normal so the walking
		# surface meets both floors flush (mirrors the native flight).
		var flight_origin := Vector3(cx + side * STACK_FLIGHT_HALF_THICKNESS * sin(pitch),
			base_y + rise * 0.5 - STACK_FLIGHT_HALF_THICKNESS * cos(pitch), cz + side * band_center)

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
		gear.set_meta(&"solid_disc", Vector2(2.6 + 0.5, 0.8))
		gear.set_meta(&"part", "StackGear")
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
		# A deck band every other level, and bracing on the faces between. Not
		# at the base level: the stack's own top deck (native) is there, and
		# a second slab 0.25 m proud of it is a floor on a floor.
		if level % 2 == 0 and level > base_level:
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
	# Starts at the second step's east face; over the step it left 1.0 m of headroom.
	_add_box("AccessLanding", Vector3(2.0, 0.3, 1.6), Vector3(30.2, 3.65, -94.05), galvanised)
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
	_build_basin()


# The yard sits in an alpine basin, not on a slab in a void. The valley floor
# runs out under the rim, 0.3 m below grade (a step the feet walk, down and
# back up). The rim is a closed ring of steep ridges: 44 cones on an 1100 m
# circle, spaced closer than the smallest cone's base radius, so where two
# meet the crease between them never flattens below ~54 degrees until it is
# half their height up -- a wall of rock, not an invisible one, and the
# same solid collision as everything else (solid_export.gd).
const BASIN_CENTER := Vector3(0.0, 0.0, -100.0)
const BASIN_RIM_RADIUS := 1100.0
const BASIN_RIM_RIDGES := 44
const VALLEY_FLOOR_TOP := -0.3


func _build_basin() -> void:
	var meadow := _material(Color("3f4634"), 0.0, 0.96, Color.BLACK, 1.0, _bump_concrete)
	# A frame around the grade (x in [-240, 240], z in [-300, 180]), not a
	# sheet under it: two surfaces 0.3 m apart z-fight from any distance.
	var floor_y := VALLEY_FLOOR_TOP - 0.5
	for piece in [
			[Vector3(5200.0, 1.0, 2400.0), Vector3(0.0, floor_y, -1500.0)],
			[Vector3(5200.0, 1.0, 2320.0), Vector3(0.0, floor_y, 1340.0)],
			[Vector3(2360.0, 1.0, 480.0), Vector3(-1420.0, floor_y, -60.0)],
			[Vector3(2360.0, 1.0, 480.0), Vector3(1420.0, floor_y, -60.0)]]:
		_add_box("ValleyFloor", piece[0], piece[1], meadow)

	var rock := _material(Color("353b41"), 0.03, 0.93)
	var rock_warm := _material(Color("3d3934"), 0.03, 0.93)
	var snow := _material(Color("b9c2cc"), 0.0, 0.78)
	var rng := RandomNumberGenerator.new()
	rng.seed = 1100
	for index in BASIN_RIM_RIDGES:
		var angle := TAU * float(index) / float(BASIN_RIM_RIDGES) + rng.randf_range(-0.01, 0.01)
		var reach := BASIN_RIM_RADIUS + rng.randf_range(-15.0, 15.0)
		var height := rng.randf_range(280.0, 470.0)
		var radius := height * 0.62
		var base := BASIN_CENTER + Vector3(sin(angle) * reach, 0.0, cos(angle) * reach)
		_add_cone("RimRidge", radius, height, base + Vector3(0.0, height * 0.5, 0.0),
			rock if index % 3 != 0 else rock_warm)
		if height > 400.0:
			_add_cone("RimRidgeSnow", radius * 0.3, height * 0.28,
				base + Vector3(0.0, height * 0.93, 0.0), snow)

	# Conifers scattered over the meadow, clear of the yard and the tower.
	var placed := 0
	while placed < 48:
		var angle := rng.randf_range(0.0, TAU)
		var reach := rng.randf_range(330.0, BASIN_RIM_RADIUS - 260.0)
		var at := BASIN_CENTER + Vector3(sin(angle) * reach, VALLEY_FLOOR_TOP, cos(angle) * reach)
		if at.z < -420.0 and absf(at.x) < 700.0:
			continue  # the near peaks' own foothill forest already covers the north
		_add_tree(at, rng.randf_range(8.0, 15.0), rng)
		placed += 1


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
	instance.set_meta(&"part", node_name)
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
	instance.set_meta(&"part", "Waterfall")
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
	_gear_pivot.set_meta(&"solid_disc", Vector2(3.9 + 0.53, 0.7))
	_gear_pivot.set_meta(&"part", "FaceGear")
	_gear_pivot.position = Vector3(-30.0, 58.0, -142.4)
	$TowerPresentation.add_child(_gear_pivot)
	var hub := CylinderMesh.new()
	hub.top_radius = 3.6
	hub.bottom_radius = 3.6
	hub.height = 0.7
	hub.radial_segments = 20
	var hub_instance := MeshInstance3D.new()
	hub_instance.set_meta(&"part", "GearMotifHub")
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
	drum_instance.set_meta(&"part", "FaceDrum")
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
	instance.set_meta(&"part", node_name)
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
	instance.set_meta(&"part", node_name)
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
	# It turns, so its collider is the volume it sweeps: rim and teeth out to
	# 1.13 r, hub depth through.
	gear.set_meta(&"solid_disc", Vector2(radius * 1.13, depth * 1.5))
	gear.set_meta(&"part", node_name)
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
	instance.set_meta(&"part", node_name)
	instance.mesh = mesh
	instance.position = at
	if not is_zero_approx(roll):
		instance.rotation = Vector3(0.0, 0.0, roll)
	parent.add_child(instance)
	return instance


# Tool mode: write the world's solid dressing for the native build and quit.
func _export_solids() -> void:
	var SolidExport: GDScript = load("res://presentation/solid_export.gd")
	var collected: Dictionary = SolidExport.collect($TowerPresentation)
	var unknown: Array = collected["unknown"]
	if not unknown.is_empty():
		push_error("SCRAPERX_SOLIDS_UNCLASSIFIED %s" % ", ".join(unknown))
		get_tree().quit(41)
		return
	var error: Error = SolidExport.write_cpp(collected, _export_solids_path)
	if error != OK:
		push_error("SCRAPERX_SOLIDS_WRITE_FAILED code=%d path=%s" % [error, _export_solids_path])
		get_tree().quit(42)
		return
	print("SCRAPERX_SOLIDS boxes=%d hulls=%d counts=%s" % [collected["boxes"].size(),
		collected["hulls"].size(), str(collected["counts"])])
	get_tree().quit(0)


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
