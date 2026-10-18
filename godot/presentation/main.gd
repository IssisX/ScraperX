extends Node3D

signal play_ready

# Mobile starts control only after real neutral90Hz physics reaches rest.
# Exported so the same production handoff can be exercised on a desktop host.
@export var prepare_initial_world := OS.has_feature("mobile")
const StartupPreparation := preload("res://presentation/ui/startup_preparation.gd")
var _startup_preparation: CanvasLayer
var _startup_previous_disable_3d := false
var _discard_startup_delta := false
var _play_ready := false
var _android_activity: Object

# Presentation and input only. Every consequential fact below is read from the
# native ScraperX simulation; nothing here decides pose, support, traversal, or
# machine state. Where this file draws something that looks simulated -- the
# steam plume above all -- it is driven by an authoritative native value, so
# freezing that value freezes the effect.

const WorldGeometry := preload("res://presentation/world_geometry.gd")
const StaticWorldBatch := preload("res://presentation/static_world_batch.gd")
var _static_world_batch: StaticWorldBatch
const StackWorldBuilder := preload("res://presentation/stack_world_builder.gd")
const AlpineWorldBuilder := preload("res://presentation/alpine_world_builder.gd")
var _geometry: WorldGeometry
var _stack_world: StackWorldBuilder
var _alpine_world: AlpineWorldBuilder
const GroundYard := preload("res://presentation/ground_yard.gd")
const ParkourYard := preload("res://presentation/parkour_yard.gd")
var _ground_yard: GroundYard
const FreightActivity := preload("res://presentation/freight_activity.gd")
var _freight_activity: Node3D
const YardAmbience := preload("res://presentation/audio/yard_ambience.gd")
var _yard_ambience: Node3D

const ForkliftView := preload("res://presentation/forklift_view.gd")
var _forklift_view: ForkliftView
const YardCraneView := preload("res://presentation/yard_crane_view.gd")
const YardCranePanel := preload("res://presentation/ui/yard_crane_panel.gd")
var _yard_crane_view: YardCraneView
const ForkliftAudio := preload("res://presentation/audio/forklift_audio.gd")
var _forklift_audio: ForkliftAudio
const YardCraneAudio := preload("res://presentation/audio/yard_crane_audio.gd")
var _yard_crane_audio: YardCraneAudio
const ForkliftControls := preload("res://presentation/ui/forklift_controls.gd")
const ForkliftPanel := preload("res://presentation/ui/forklift_panel.gd")
var _forklift_controls := ForkliftControls.new()
var _forklift_view_heading := 0.0
var _forklift_view_heading_valid := false

const WorksiteMachineView := preload("res://presentation/worksite_machine_view.gd")
var _machine_view: WorksiteMachineView

const EYE_OFFSET := Vector3(0.0, 0.62, 0.0)

# Interface: every device (touch, pad, keyboard/mouse) is read by one router
# into one verb vocabulary; the HUD, touch surface and pause menu draw from
# one per-frame read of native state (_read_context). None of it decides
# anything -- a verb becomes a native request_*, and the native says yes or no.
const UiStyle := preload("res://presentation/ui/ui_style.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const LandingCameraResponse := preload("res://presentation/landing_camera_response.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const Hud := preload("res://presentation/ui/hud.gd")
const PauseMenu := preload("res://presentation/ui/pause_menu.gd")
const PerformanceReport := preload("res://presentation/ui/performance_report.gd")
const BuildInfo := preload("res://presentation/ui/build_info.gd")
const SettingsStore := preload("res://presentation/ui/settings_store.gd")
const SlingshotView := preload("res://presentation/slingshot_view.gd")
const KitView := preload("res://presentation/kit_view.gd")
const GrapplingGunViewScript := preload("res://presentation/grappling_gun_view.gd")
const GrappleFeedbackModel := preload("res://presentation/grapple_feedback_model.gd")
const ReclaimDust := preload("res://presentation/reclaim_dust.gd")
const MovementDust := preload("res://presentation/movement_dust.gd")
var _movement_dust: MovementDust
const InspectionBalanceFeedback := preload("res://presentation/inspection_balance_feedback.gd")
var _inspection_balance_feedback: InspectionBalanceFeedback
const InspectionWorksiteLife := preload("res://presentation/inspection_worksite_life.gd")
var _inspection_worksite_life: InspectionWorksiteLife
# Loaded only for --uitest runs, so shipping builds never parse test code.
const UI_TEST_DRIVER_PATH := "res://presentation/ui/ui_test_driver.gd"
const FirstPersonArms := preload("res://presentation/first_person_arms.gd")
const FirstPersonLegs := preload("res://presentation/first_person_legs.gd")
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
# On a ladder or pipe the eye leans up toward the next holds, less than a hang
# looks up at its lip.
const VIEW_PITCH_CLIMBING := 0.18
const VIEW_PITCH_RATE := 9.0
# A hitch -- or the first frame after Android resumes a backgrounded app --
# must not be paid for with a burst of native ticks in one frame. The excess
# of that one frame simply runs slower than wall time, which nobody can see.
const MAX_SIM_FRAME_DELTA := 0.1
# Chute is offered once a fall is unmistakably a fall: a full-height jump
# lands at ~5.5 m/s, so 6.5 m/s never pops the canopy button mid-hop.
const CHUTE_OFFER_FALL_MPS := 6.5
const CHECKPOINT_TOAST_RISE_METERS := 3.0
const SLING_CHECK_SECONDS := 0.3

# Locomotion feel is presentation only: the native snapshot drives it and the
# camera never feeds back into collision or movement. Exponential responses
# follow native speed and contact without changing the authoritative pose.
const FOV_BASE := 82.0
const FOV_SPRINT_MAX_DEGREES := 4.0
const FOV_FALL_MAX_DEGREES := 3.0
const FOV_FALL_FULL_MPS := 14.0
const HEAD_BOB_CYCLES_PER_METER := 0.72
const HEAD_BOB_VERTICAL_METERS := 0.028
const HEAD_BOB_LATERAL_METERS := 0.016
# Crouched, the eye sits 0.95 m over the soles instead of 1.52: the native
# capsule is 1.2 m, not 1.8 (kPlayerCrouchHalfHeight). The native swaps the
# two in one tick with the soles fixed, so the eye is measured from the soles
# and glides between the heights; the snap never reaches the view.
const STAND_HALF_HEIGHT := 0.9
const CROUCH_HALF_HEIGHT := 0.6
const CROUCH_EYE_OVER_SOLES := 0.95
const CROUCH_EYE_SECONDS := 0.16
const HEAD_BOB_SPEED_FLOOR_MPS := 0.3
const HEAD_BOB_SPEED_FULL_MPS := 3.0
const HEAD_BOB_RESPONSE_PER_SECOND := 12.0
const FOV_RESPONSE_PER_SECOND := 9.0
const CAMERA_BANK_MAX_RADIANS := 1.5 * PI / 180.0
const CAMERA_BANK_RESPONSE_PER_SECOND := 10.0

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


# Rungs 17-20 sit 0.8 m west of the column, clear of the bascule flight's
# foot (kLegalFortySkinRungJogX); the walkway starts at rung 20's east face.
# z in [-121, -117]: 2 m south of rung 20's band, clear of the upper flight
# overhead (kLegalFortySkinWalkwayCenterZ has why).


const HOOK5_BAR_ENTITY_ID := 55
const HOOK5_BLOCK_ENTITY_ID := 56

# AS-006: the mechanism kit's entity ids (sim/mechanism_kit.hpp): band
# structure from 1000, bodies that move from 2000. What a kit body is, how
# big, what it is made of and where it is all come from the native.
const KIT_ENTITY_MIN := 1000
const KIT_ENTITY_MAX := 3000
const KIT_PART_FLOATS := 13
const KIT_CARRY_SHACKLE := 1
const KIT_CARRY_HANDLE := 2
const RIG_HOOK := 1
const RIG_UNHOOK := 2

const TRAVERSAL_NONE := 0
const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3
# Step 2 movement: on the holds of a ladder, pipe, bar or lattice; and
# lowering over an edge into a hang.
const TRAVERSAL_CLIMBING := 4
const TRAVERSAL_LOWERING := 5

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


var _native: Object
var _capture_path := ""
var _capture_scheduled := false
var _ci_mode := false
var _settings_persistent := true
var _yaw := 0.0
var _pitch := -0.02
var _landing_camera := LandingCameraResponse.new()
var _cam_bob_phase := 0.0
var _cam_bob_weight := 0.0
var _cam_fov_offset := 0.0
var _cam_bank := 0.0
var _swing_bank := 0.0
var _swing_last_velocity := Vector3.ZERO
var _grip_generation_left := 0
var _grip_generation_right := 0
var _grip_attached_left := false
var _grip_attached_right := false
var _grip_feedback_tick := -1
var _grip_feedback_deaths := -1
var _grip_feedback_cooldown := 0.0
var _catch_strain_cooldown := 0.0
var _last_hand_load := 0.0
var _crouch_eye := 0.0
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
var _slingshot_view: Node3D
var _sling_prediction := PackedVector3Array()
var _sling_preview_clock := 0.0
var _sling_was_seated := false
var _sling_goal_yaw := 0.0
var _sling_goal_elevation := 1.4311699866353502
var _settings: SettingsStore
var _uitest: Node
var _force_touch := false
var _uitest_scenario := ""
var _regression_scene := false
var _paused := false
var _performance_report := PerformanceReport.new()
var _telemetry_on := false
var _ctx := {}
# Pendant "operate" mode is presentation state only: which controls are on
# screen. The native gates every pendant axis by station radius regardless.
var _operating := &""
# The crouch toggle (C, right-stick click, the touch button). Held Ctrl
# crouches too, for as long as it is held; the native decides whether the
# body can crouch or stand.
var _crouch_toggled := false
var _crouch_toggle_from_hang := false
var _fb_traversal := 0
var _fb_swinging := false
var _catch_net_feedback_serial := -1
var _catch_net_feedback_deaths := -1
var _catch_net_feedback_cooldown := 0.0
var _fb_landing_count := -1
var _fb_landing_tick := -1
var _fb_foot_transfer_count := -1
var _fb_foot_transfer_tick := -1
var _fb_plank_fracture_count := -1
var _fb_plank_broken_mask := 0
var _fb_plank_energy_j := 0.0
var _fb_plank_tick := -1
var _fb_plank_cue_tick := -100
var _fb_fall_speed := 0.0
var _fb_deaths := 0
var _fb_chute := false
var _fb_warned := false
var _fb_best_checkpoint_y := 0.0
var _sling_check_timer := 0.0
var _sling_check_was_slung := false
var _arms: Node3D
var _legs: Node3D
var _leg_deaths := 0
var _view_pitch_offset := 0.0

var _kit_view: KitView
var _grapple_view: GrapplingGunViewScript
var _grapple_feedback := GrappleFeedbackModel.new()
var _reclaim_dust: ReclaimDust
var _reclaim_feedback_deaths := -1

# Decorative dressing (WO-007). None of these carry collision, own state, or
# feed the CI proof; the gear pivot is the one exception that reads a real
# native value (see WorksiteMachineView.KELLERWORKS_CYCLE_PERIOD_SECONDS above).
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
@onready var _hook5_value: Label = $HUD/TopLeft/Hook5
@onready var _light_rig: Node3D = $LightRig
var _sky_cycle: Node
var _audio: Node


var _export_solids_path := ""


func _ready() -> void:
	if OS.has_feature("android") and Engine.has_singleton("AndroidRuntime"):
		_android_activity = Engine.get_singleton("AndroidRuntime").getActivity()
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument == "--regression-fixtures":
			_regression_scene = true
		elif argument == "--touch":
			_force_touch = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")
		elif argument.begins_with("--uitest="):
			_uitest_scenario = argument.trim_prefix("--uitest=")
		elif argument.begins_with("--export-solids="):
			_export_solids_path = argument.trim_prefix("--export-solids=")

	if not _uitest_scenario.is_empty() and _uitest_scenario not in ["ground_foundation", "touch_causal_facade", "touch_campaign_to_121", "touch_campaign_to_143", "touch_campaign_to_165", "touch_campaign_to_198", "touch_campaign_to_231", "touch_campaign_to_253", "touch_campaign_to_286", "touch_campaign_to_308", "touch_service_lift", "touch_balance_lift", "touch_crown_gondola", "touch_crown_swing", "touch_gravity_reclaim", "touch_slab_haul_cart", "touch_ballast_mantle_feedback", "touch_taper_inspection_route", "touch_traction_tram", "touch_barrel_helix", "touch_cascade_mast", "touch_pitman_lift", "touch_north_grip_diagnostic", "touch_single_hand_catch", "touch_single_hand_catch_right", "touch_single_hand_catch_action", "touch_suspended_ladder", "touch_cargo_net", "keyboard_slingshot", "pad_slingshot", "touch_slingshot", "touch_slingshot_landing", "pipe_bridge", "touch_pipe_bridge", "keyboard_pipe_bridge", "touch_facade", "touch_stair", "touch_upper", "touch_teeter", "touch_braced_bay", "touch_north_frame", "touch_west_brace_bay"]:
		_regression_scene = true
	if _ci_mode:
		_regression_scene = true

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
	_audio.regression_machines = _regression_scene
	_audio.sky = _sky_cycle
	add_child(_audio)
	if not _regression_scene:
		_yard_ambience = YardAmbience.new()
		_yard_ambience.name = "ReceivingYardAmbience"
		_yard_ambience.configure(Vector3(-92, 2.4, -34.5), Vector3(-80, 1.6, -92))
		add_child(_yard_ambience)
	_build_arms()
	_build_legs()
	_build_interface()
	_layout_hud()
	get_viewport().size_changed.connect(_layout_hud)

	if not ClassDB.class_exists("ScraperXSimulation"):
		_fail_native("SCRAPERX_EXTENSION_LOAD_FAILED", 20)
		return

	if prepare_initial_world and not _regression_scene and _uitest_scenario.is_empty():
		_start_initial_preparation()
		return
	_native = ClassDB.instantiate("ScraperXSimulation")
	if _native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return
	_finish_native_setup()


func _start_initial_preparation() -> void:
	# _native stays null while the worker exclusively owns the simulation.
	# All ordinary input, presentation and pause getters therefore remain gated.
	_router.gameplay_active = false
	_router.clear_held()
	_hud.hide()
	_touch.hide()
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	if _startup_preparation == null:
		_startup_previous_disable_3d = get_viewport().disable_3d
	else:
		_startup_preparation.queue_free()
	get_viewport().disable_3d = true
	_startup_preparation = StartupPreparation.new()
	_startup_preparation.name = "StartupPreparation"
	_startup_preparation.prepared.connect(_on_initial_prepared)
	_startup_preparation.retry_requested.connect(_start_initial_preparation)
	_startup_preparation.cancel_requested.connect(func() -> void: get_tree().quit())
	add_child(_startup_preparation)
	# Draw the preparation panel before constructing the native world.
	_begin_initial_preparation.call_deferred(_startup_preparation)


func _begin_initial_preparation(panel: CanvasLayer) -> void:
	await RenderingServer.frame_post_draw
	if not is_inside_tree() or not is_instance_valid(panel) or panel != _startup_preparation:
		return
	var preparing_native: Object = ClassDB.instantiate("ScraperXSimulation")
	if preparing_native == null:
		_fail_native("SCRAPERX_EXTENSION_INSTANTIATION_FAILED", 20)
		return
	panel.start(preparing_native)


func _on_initial_prepared(prepared_native: Object, _steps: int) -> void:
	# The panel joins its worker BEFORE this signal. This is the single
	# owner handoff; no bridge getter or command overlaps the worker.
	_native = prepared_native
	get_viewport().disable_3d = _startup_previous_disable_3d
	_finish_native_setup()


func _finish_initial_handoff() -> void:
	# Warm the first real3D frame under the loading panel as well. Controls
	# open after resource upload/shader preparation, with a fresh frame clock.
	await RenderingServer.frame_post_draw
	if not is_inside_tree() or _startup_preparation == null:
		return
	_startup_preparation.queue_free()
	_startup_preparation = null
	_router.clear_held()
	_router.gameplay_active = true
	_hud.show()
	_on_device_changed(_router.device)
	_discard_startup_delta = true
	_performance_report.skip_next_frame()
	_play_ready = true
	play_ready.emit()


func _finish_native_setup() -> void:
	if _regression_scene and _uitest_scenario.is_empty():
		_native.configure_regression_spawn(8)

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

	_setup_kit_view()
	if not _regression_scene:
		_freight_activity = FreightActivity.new()
		_freight_activity.name = "FreightActivity"
		add_child(_freight_activity)
		_freight_activity.setup(_native)
	_yard_crane_view = YardCraneView.new()
	_yard_crane_view.name = "YardCraneCables"
	add_child(_yard_crane_view)
	_forklift_view = ForkliftView.new()
	_forklift_view.name = "ForkliftWheels"
	add_child(_forklift_view)
	_forklift_audio = ForkliftAudio.new()
	_forklift_audio.name = "ForkliftAudio"
	add_child(_forklift_audio)
	_yard_crane_audio = YardCraneAudio.new()
	_yard_crane_audio.name = "YardCraneAudio"
	add_child(_yard_crane_audio)
	_grapple_view = GrapplingGunViewScript.new()
	_grapple_view.name = "GrapplingGunPresentation"
	_grapple_view.configure_hook_geometry(_native.get_grapple_hook_geometry())
	add_child(_grapple_view)
	_grapple_view.configure(_camera)
	if bool(_native.get_slingshot_state().get("available", false)):
		_slingshot_view = SlingshotView.new()
		add_child(_slingshot_view)
		_slingshot_view.setup(self)

	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim scene=%s" %
		("regression_fixtures work_order=WO-006" if _regression_scene else ("slingshot" if bool(_native.get_slingshot_state().get("available", false)) else "pipe_bridge_fixture")))
	print("SCRAPERX_VIEWPORT size=%dx%d aspect=%.3f fov=%.1f far=%.0f" % [
		int(_viewport_size.x), int(_viewport_size.y),
		_viewport_size.x / maxf(1.0, _viewport_size.y), _camera.fov, _camera.far])
	_render_snapshot()
	_ctx = _read_context()
	_fb_traversal = int(_ctx["traversal"])
	_fb_swinging = bool(_ctx["swinging"])
	var landing: Dictionary = _native.get_landing_state()
	_reset_landing_feedback(landing, int(_native.get_tick_index()))
	_reset_foot_transfer_feedback(landing)
	_reset_plank_feedback(landing, int(_native.get_tick_index()))
	_fb_deaths = int(_ctx["deaths"])
	_fb_best_checkpoint_y = (_ctx["checkpoint"] as Vector3).y
	if not _regression_scene:
		_static_world_batch = StaticWorldBatch.new()
		_static_world_batch.build_tower($TowerPresentation)
		_static_world_batch.build_kit(_kit_view, _kit_view.static_render_roots())
		_kit_view.batch_cables()
	if _startup_preparation != null:
		_finish_initial_handoff.call_deferred()
	else:
		_play_ready = true
		play_ready.emit()


func _process(delta: float) -> void:
	if _native == null or not _play_ready:
		return
	# Android's notification shade can take window focus without pausing the
	# Activity. Check before consuming input or advancing the native world.
	if _android_activity != null and not bool(_android_activity.hasWindowFocus()):
		_open_pause(true)
		if _paused:
			return
	if _discard_startup_delta:
		# Preparation and view construction precede player control. They
		# must not become a catch-up burst on the first controllable frame.
		delta = 0.0
		_discard_startup_delta = false
	if _fps_label.visible:
		_fps_clock -= delta
		if _fps_clock <= 0.0:
			_fps_clock = 0.5
			_fps_label.text = "%d FPS" % int(Engine.get_frames_per_second())

	var launcher: Dictionary = _native.get_slingshot_state()
	var sling_seated := bool(launcher.get("seated", false))
	var aiming := sling_seated and not bool(launcher.get("released", false))
	var sling_recovering := bool(launcher.get("recovering", false))
	var aim_locked := bool(launcher.get("aim_locked", true))
	var intent: Dictionary = _router.frame(delta)
	_update_forklift_view_heading()
	if aiming and not _sling_was_seated:
		_sling_goal_yaw = float(launcher["yaw_rad"])
		_sling_goal_elevation = float(launcher["elevation_rad"])
	_sling_was_seated = aiming
	if not _ci_mode:
		var look: Vector2 = intent["look"]
		if aiming:
			if not aim_locked:
				_sling_goal_yaw = clampf(_sling_goal_yaw + look.x, -0.8, 0.8)
				_sling_goal_elevation = clampf(_sling_goal_elevation - look.y, 0.35, 1.48)
			else:
				_sling_goal_yaw = float(launcher["yaw_rad"])
				_sling_goal_elevation = float(launcher["elevation_rad"])
			# Retain the requested angle while the real, finite-torque rail
			# settles. The lens and reticle continue to show its actual axis.
			_yaw = -float(launcher["yaw_rad"])
			_pitch = float(launcher["elevation_rad"])
		else:
			_yaw -= look.x
			# The equipped overhead tool needs an honest high shot. Other movement
			# keeps its established look range; aim excludes all cosmetic head motion.
			_pitch = clampf(_pitch - look.y, -1.25, PI * 0.5 - 0.001 if bool(_ctx.get("grapple_equipped", false)) else 1.35)
	_update_view_pitch_offset(delta)

	var position: Vector3 = _native.get_player_position()
	var desired: Vector2 = intent["move"]
	var sling_yaw := _sling_goal_yaw if aiming else float(launcher.get("yaw_rad", 0.0))
	var sling_elevation := _sling_goal_elevation if aiming else float(launcher.get("elevation_rad", 1.4311699866))
	var sling_effort := maxf(0.0, -desired.y) if sling_seated or sling_recovering else 0.0
	if sling_recovering:
		sling_effort = maxf(sling_effort, float(intent.get("reel", 0.0)))
	_native.set_slingshot_input(sling_effort, sling_yaw, sling_elevation)
	var facing := Vector2(-sin(_yaw), -cos(_yaw))

	if _ci_mode:
		desired = _ci_movement_intent(position)
		facing = _ci_facing
		_yaw = atan2(-facing.x, -facing.y)

	var forward := Vector2(-sin(_yaw), -cos(_yaw))
	var right := Vector2(cos(_yaw), -sin(_yaw))
	var world_move := right * desired.x + forward * desired.y
	if sling_recovering or _operating in [&"forklift", &"yard_crane"]:
		world_move = Vector2.ZERO
	if not _native.set_move_input(world_move.x, world_move.y):
		_fail_native("SCRAPERX_MOVE_INPUT_REJECTED", 21)
		return
	_native.set_facing(facing.x, facing.y)
	_native.set_grapple_aim(-sin(_yaw) * cos(_pitch), sin(_pitch), -cos(_yaw) * cos(_pitch))
	_dispatch(intent["verbs"], delta)
	_native.set_crouch_input(_crouch_toggled or bool(intent["crouch_held"]))
	_native.set_sprint_input(bool(intent["sprint_held"]))

	_deliver_forklift_input(intent)
	_deliver_yard_crane_input(intent)
	var pendant_input: Vector2 = intent["pendant"]
	_native.set_grapple_reel_input(pendant_input.y if _operating == &"grappling_gun" else 0.0)
	var jib_input := Vector2.ZERO if _operating in [&"grappling_gun", &"forklift", &"yard_crane"] else pendant_input
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
	# Operation is a UI mode; native station reach and brake state own motion.
	_native.set_service_lift_input(jib_input.y if _operating == &"service_lift" else 0.0)
	var balance: Dictionary = _native.get_supplied_machine_state()
	_native.set_supplied_machine_input(jib_input.y if _operating == &"supplied_machine" \
		and int(balance["station"]) != 0 else 0.0)

	var time_scale := float(_slingshot_view.get_simulation_scale()) if _slingshot_view != null else 1.0
	var native_started_usec := Time.get_ticks_usec()
	var steps_advanced := int(_native.advance_frame(minf(delta, MAX_SIM_FRAME_DELTA) * time_scale))
	var native_wall_usec := Time.get_ticks_usec() - native_started_usec
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_performance_report.record_frame(delta, native_wall_usec, steps_advanced,
		int(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)),
		int(Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME)),
		float(_native.get_player_position().y), rad_to_deg(_pitch), -1,
		int(_native.get_last_advance_process_cpu_usec()))
	_render_snapshot(delta)
	_ctx = _read_context()
	# Attachment ownership can end natively without a presentation release verb.
	# Clear only the posture toggle; held Ctrl and ordinary ground crouch retain
	# their own intent. Temporary posture ineligibility with hands stays intact.
	if _crouch_toggle_from_hang and (not bool(_ctx["climbing"]) and not bool(_ctx["hanging"]) \
			or int(_native.get_landing_state().get("traversal_hand_constraint_count", 0)) == 0):
		_crouch_toggled = false
		_crouch_toggle_from_hang = false
		_native.set_crouch_input(bool(intent["crouch_held"]))
	var body_state := _arms_state(intent)
	_arms.update_arms(body_state, _camera.global_transform, delta)
	_update_legs(body_state, delta)
	if _slingshot_view != null:
		var sling: Dictionary = _native.get_slingshot_render_state()
		_audio.update_slingshot(sling, _native.get_player_linear_velocity())
		sling["reduced_motion"] = not _settings.launch_cinematics
		sling["player_grounded"] = _native.is_player_grounded()
		sling["chute_deployed"] = _native.is_parachute_deployed()
		_sling_preview_clock -= delta
		if bool(sling["seated"]) and not bool(sling["released"]) and _sling_preview_clock <= 0.0:
			_sling_preview_clock = 0.2
			_sling_prediction = _native.get_slingshot_prediction()
		sling["trajectory_points"] = _sling_prediction
		_slingshot_view.update_view(delta, sling, _native.get_player_render_position(), _native.get_player_linear_velocity(), _camera)
		_audio.update_launch_cinematic(_slingshot_view.get_cinematic_state())
	_update_feedback(delta)
	_touch.update_context(_ctx, delta)
	_hud.family = _router.glyph_family()
	_hud.update_hud(_ctx, delta)
	_ambient_clock += delta
	_machine_view.update_ambient_dressing(_native, _ambient_clock)
	if _inspection_worksite_life != null:
		_inspection_worksite_life.update_from_native(_native, delta,
			not _settings.launch_cinematics, _sky_cycle.hour if _sky_cycle != null else 10.5)

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
	_settings.persistent = _settings_persistent and not _ci_mode and _uitest_scenario.is_empty()
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
	_pause_menu.restart_requested.connect(_restart_at)
	_pause_menu.checkpoint_restart_requested.connect(_restart_checkpoint)
	_pause_menu.lift_attempt_restart_requested.connect(_restart_lift_attempt)
	_pause_menu.reclaim_attempt_restart_requested.connect(_restart_reclaim_attempt)
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
	_touch.set_control_profile(_settings.touch_profile)
	_set_telemetry_visible(_settings.telemetry or _ci_mode)
	# GRAPHICS
	var viewport := get_viewport()
	viewport.scaling_3d_mode = Viewport.SCALING_3D_MODE_BILINEAR
	viewport.scaling_3d_scale = _settings.render_scale
	viewport.msaa_3d = [Viewport.MSAA_DISABLED, Viewport.MSAA_2X, Viewport.MSAA_4X][_settings.msaa]
	Engine.max_fps = SettingsStore.FPS_CAPS[_settings.fps_cap]
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_ENABLED if _settings.vsync == 0
		else DisplayServer.VSYNC_DISABLED)
	_camera.far = _settings.view_distance
	_apply_visual_detail($TowerPresentation, SettingsStore.DETAIL_DISTANCES[_settings.detail_distance])
	if _static_world_batch != null:
		_static_world_batch.apply_detail_distance(SettingsStore.DETAIL_DISTANCES[_settings.detail_distance])
	($Environment as WorldEnvironment).environment.glow_enabled = _settings.bloom
	_sky_cycle.exposure_scale = _settings.brightness
	_sky_cycle.set_shadow_level(_settings.shadow_quality)
	_fps_label.visible = _settings.show_fps
	# AUDIO
	_audio.set_volumes(0.0 if _settings.audio_muted else _settings.master_volume, _settings.effects_volume,
		_settings.ambience_volume, _settings.interface_volume)
	if _inspection_balance_feedback != null:
		_inspection_balance_feedback.set_volumes(0.0 if _settings.audio_muted else _settings.master_volume, _settings.effects_volume)
	# DISPLAY
	_fov_base = _settings.fov
	_head_bob_on = _settings.head_bob
	_speed_fov_on = _settings.speed_fov
	if not OS.has_feature("mobile") and DisplayServer.get_name() != "headless":
		var target_mode := DisplayServer.WINDOW_MODE_FULLSCREEN if _settings.window_mode == 1 \
			else DisplayServer.WINDOW_MODE_WINDOWED
		if DisplayServer.window_get_mode() != target_mode:
			DisplayServer.window_set_mode(target_mode)
	if _settings.time_of_day == 0:
		_sky_cycle.day_minutes = _settings.day_minutes
	else:
		_sky_cycle.day_minutes = 0.0
		_sky_cycle.set_hour(SettingsStore.TIME_OF_DAY_HOURS[_settings.time_of_day])


# Procedural meshes have no generated mesh LOD. These explicitly decorative
# parts therefore use distance culling instead. Native Kit parts and the
# structural route never enter this list; collision/simulation stays intact.
func _apply_visual_detail(node: Node, distance: float) -> void:
	if node is GeometryInstance3D and node.get_meta(&"part", "") in [
			"FaceBand", "FaceDuct", "FloorLight", "FloorLightHigh", "TimberCladding",
			"LaneStripe", "TreeTrunk", "TreeCanopy", "ScrubLobe"]:
		var geometry := node as GeometryInstance3D
		geometry.visibility_range_end = distance
		geometry.visibility_range_end_margin = 0.0
		geometry.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_DISABLED
	for child in node.get_children():
		_apply_visual_detail(child, distance)


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


func _exit_tree() -> void:
	_android_activity = null
	if _startup_preparation != null:
		get_viewport().disable_3d = _startup_previous_disable_3d


func _notification(what: int) -> void:
	match what:
		NOTIFICATION_APPLICATION_PAUSED, NOTIFICATION_APPLICATION_FOCUS_OUT, NOTIFICATION_WM_WINDOW_FOCUS_OUT:
			_open_pause(true)
		NOTIFICATION_WM_GO_BACK_REQUEST:
			if _startup_preparation != null:
				get_tree().quit()
			elif _paused:
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
	_performance_report.set_paused(true)
	if _freight_activity != null:
		_freight_activity.reset()
	_native.cancel_forklift_tool_request()
	_zero_yard_crane_input()
	_native.set_forklift_input(0.0, 0.0, 0.0, 0.0, 1.0)
	if _operating == &"forklift":
		_forklift_controls.enter()
	if _forklift_audio != null:
		_forklift_audio.reset()
	if _yard_crane_audio != null:
		_yard_crane_audio.reset()
	_reset_grapple_feedback()
	if _slingshot_view != null:
		_slingshot_view.cancel_cinematic()
	_audio.cancel_launch_cinematic()
	_router.gameplay_active = false
	_router.clear_held()
	_native.set_supplied_machine_input(0.0)
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	var checkpoint: Vector3 = _ctx["checkpoint"]
	var position: Vector3 = _ctx["position"]
	var in_reclaim_bay := position.y > 303.0 and position.y < 334.0 \
		and Vector2(position.x + 38.0, position.z + 158.0).length() < 25.0
	_pause_menu.set_restart_context(position, bool(_native.get_service_lift_state()["retry_available"]),
		in_reclaim_bay and bool(_native.can_restart_gravity_reclaim_attempt()))
	_pause_menu.performance_report = _performance_report.summary()
	_pause_menu.performance_report_text = _performance_report.copy_text(BuildInfo.read_identity())
	_pause_menu.open(_router.glyph_family(), "ALTITUDE %+.1f M\nCHECKPOINT %+.1f M\nDEATHS %d" % [
		position.y, checkpoint.y, int(_ctx["deaths"])])
	get_tree().paused = true
	_audio.ui_tap()


func _restart_at(position: Vector3) -> void:
	var accepted := bool(_native.debug_restart_at(position))
	_pause_menu.set_restart_result(accepted, "That position intersects the structure or is outside the world.")
	if accepted:
		_after_restart()


func _restart_checkpoint() -> void:
	if bool(_native.restart_checkpoint()):
		_after_restart()


func _restart_lift_attempt() -> void:
	var accepted := bool(_native.restart_service_lift_attempt())
	_pause_menu.set_restart_result(accepted, "" if accepted else "No saved lift attempt is available.")
	if accepted:
		_after_restart()


func _restart_reclaim_attempt() -> void:
	var accepted := bool(_native.restart_gravity_reclaim_attempt())
	_pause_menu.set_restart_result(accepted, "" if accepted else "Reach the reclaim bay's feeding control first.")
	if accepted:
		_after_restart()


func _after_restart() -> void:
	if _freight_activity != null:
		_freight_activity.reset()
	_reset_grapple_feedback()
	_landing_camera.reset()
	if _movement_dust != null:
		_movement_dust.reset()
	if _reclaim_dust != null:
		_reclaim_dust.reset()
	if _slingshot_view != null:
		_slingshot_view.cancel_cinematic(true)
	_audio.cancel_launch_cinematic()
	_sling_was_seated = false
	_sling_preview_clock = 0.0
	_sling_prediction = PackedVector3Array()
	_end_operation()
	_crouch_toggled = false
	_crouch_toggle_from_hang = false
	_ctx = _read_context()
	var landing: Dictionary = _native.get_landing_state()
	_reset_landing_feedback(landing, int(_native.get_tick_index()))
	_reset_foot_transfer_feedback(landing)
	_reset_plank_feedback(landing, int(_native.get_tick_index()))
	_fb_deaths = int(_ctx["deaths"])
	_audio.reset_landing_feedback(_fb_landing_count, _fb_deaths, landing)
	_fb_swinging = bool(_ctx["swinging"])
	_grip_generation_left = int(landing.get("left_hand_generation", 0))
	_grip_generation_right = int(landing.get("right_hand_generation", 0))
	_grip_attached_left = bool(landing.get("left_hand_attached", false))
	_grip_attached_right = bool(landing.get("right_hand_attached", false))
	_grip_feedback_tick = int(_native.get_tick_index())
	_grip_feedback_deaths = _fb_deaths
	_catch_strain_cooldown = 0.0
	_last_hand_load = 0.0
	_render_snapshot()
	_resume()


func _resume() -> void:
	if not _paused:
		return
	_pause_menu.close()
	_audio.ui_tap(true)
	_settings.save_to_disk()
	get_tree().paused = false
	_paused = false
	_performance_report.set_paused(false)
	_router.clear_held()
	_router.gameplay_active = true
	if _router.device == InputRouter.Device.KEYBOARD_MOUSE and _router.capture_mouse \
			and not _router.accept_emulated_touch and _uitest_scenario.is_empty():
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


# One fresh verb, one native request. The native 90 Hz owner decides Jump's
# contextual intent, buffer and vault timing. Last-frame context only selects
# which request the other contextual verbs mean; native authority accepts it.
func _dispatch(verbs: Array, _delta: float) -> void:
	for verb in verbs:
		if _operating == &"yard_crane":
			if verb in [&"action", &"back", &"drop"]:
				_end_operation()
			elif verb == &"yard_crane_attach":
				_native.request_yard_crane_attach()
			elif verb == &"yard_crane_detach":
				_native.request_yard_crane_detach()
			elif verb == &"pause":
				_open_pause()
			elif verb == &"telemetry":
				_settings.telemetry = not _settings.telemetry
				_set_telemetry_visible(_settings.telemetry or _ci_mode)
			continue
		if bool(_ctx.get("yard_crane_rigging_reachable", false)) and verb in [&"sling_attach", &"sling_release", &"alt"]:
			if verb == &"sling_attach":
				_native.request_yard_crane_attach()
			else:
				_native.request_yard_crane_detach()
			continue
		if _operating == &"forklift" and verb in [&"drop", &"back"]:
			_end_operation()
			continue
		if _operating == &"forklift" and verb == &"forklift_tool":
			var tool: Dictionary = _native.get_forklift_state()
			if bool(tool.get("coupled", false)):
				_native.request_forklift_detach()
			else:
				_native.request_forklift_couple()
			continue
		if _operating == &"forklift" and String(verb).begins_with("forklift_"):
			_forklift_controls.handle_verb(verb)
			continue
		match verb:
			&"jump":
				# A jump stands a crouched body first (natively), and ends the toggle.
				_crouch_toggled = false
				_crouch_toggle_from_hang = false
				if _operating == &"forklift":
					_end_operation()
				_native.request_jump()
			&"action":
				_crouch_toggled = false
				_crouch_toggle_from_hang = false
				_perform_action()
			&"crouch":
				# Native attachment conditions select hanging posture or ground crouch.
				_crouch_toggled = not bool(_ctx.get("swing_tuck_requested", false)) \
					if bool(_ctx.get("swing_tuck_available", false)) else not bool(_ctx["crouched"])
				_crouch_toggle_from_hang = _crouch_toggled and bool(_ctx.get("swing_tuck_available", false))
			&"drop":
				# Let go of whatever the hands are on: a ledge, a climb's
				# holds, or a load. On the ground with an edge behind, the
				# native lowers the body over it into a hang.
				if _crouch_toggle_from_hang:
					_crouch_toggled = false
					_crouch_toggle_from_hang = false
				if bool(_ctx.get("grapple_equipped", false)):
					if bool(_ctx.get("grapple_line_attached", false)):
						_native.request_grapple_release()
					else:
						_end_operation()
				elif bool(_ctx.get("slingshot_seated", false)) or bool(_ctx.get("slingshot_recovering", false)):
					_native.request_slingshot_drop()
				elif int(_ctx["carrying"]) != 0:
					_native.request_set_down()
				else:
					_native.request_release()
			&"auto_reel":
				var gun: Dictionary = _native.get_grapple_gun_state()
				if bool(gun.get("equipped", false)) and bool(gun.get("line_attached", false)):
					_native.set_grapple_auto_reel(not bool(gun.get("auto_reel", false)))
			&"toggle_grapple":
				# Equipment has its own verb: a nearby grip, load or machine
				# must not hide a carried tool. Native hand/ownership rules
				# still accept or reject drawing on the next simulation tick.
				var gun: Dictionary = _native.get_grapple_gun_state()
				if bool(gun.get("equipped", false)):
					_native.set_grapple_equipped(false)
					if _operating == &"grappling_gun":
						_operating = &""
				elif bool(_ctx.get("grapple_draw_available", false)):
					_native.set_grapple_equipped(true)
			&"chute":
				_native.request_parachute()
			&"back":
				if _crouch_toggle_from_hang:
					_crouch_toggled = false
					_crouch_toggle_from_hang = false
				if bool(_ctx.get("grapple_line_attached", false)):
					_native.request_grapple_release()
				elif bool(_ctx.get("slingshot_seated", false)) or bool(_ctx.get("slingshot_recovering", false)):
					_native.request_slingshot_drop()
				elif _ctx["hanging"] or _ctx["climbing"] or _ctx["lowering"]:
					_native.request_release()
				elif int(_ctx["carrying"]) != 0:
					_native.request_set_down()
				elif _operating != &"":
					_end_operation()
				elif _ctx["drop_ok"]:
					_native.request_release()
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


# Contextual Action is a gateway (Governing Law 27): it climbs what the
# native affordance reports, opens a pendant's own controls, or works a
# valve. It never operates a machine on the player's behalf.
func _perform_action() -> void:
	var action: Dictionary = _ctx["action"]
	match action["id"]:
		&"equip_grapple":
			_native.set_grapple_equipped(true)
		&"grapple_action":
			_native.request_grapple_action()
		&"slingshot":
			if bool(_native.get_slingshot_state().get("recovering", false)):
				_native.request_slingshot_drop()
			else:
				_native.request_slingshot_action()
		&"release":
			_native.request_release()
		&"climb_up", &"climb", &"catch", &"reach_hand":
			_native.request_traversal()
		&"operate":
			_operating = _ctx["station"]
			if _operating == &"yard_crane":
				_router.set_yard_crane_mode(true)
				_crouch_toggled = false
				_crouch_toggle_from_hang = false
				var crane := _yard_crane_state()
				var front: Vector3 = crane.get("operator_forward", Vector3.RIGHT)
				_yaw = atan2(-front.x, -front.z)
				_pitch = 0.0
			if _operating == &"forklift":
				_forklift_controls.enter()
				_router.set_forklift_mode(true)
				_forklift_view_heading_valid = false
				_update_forklift_view_heading()
		&"yard_crane_attach":
			_native.request_yard_crane_attach()
		&"yard_crane_detach":
			_native.request_yard_crane_detach()
		&"done":
			_end_operation()
		&"valve":
			_native.request_valve_toggle()
		&"screw_toggle":
			_native.request_water_screw_toggle()
		&"water_lift_valve":
			_native.request_water_lift_valve_toggle()
		&"water_lift_release":
			_native.request_water_lift_release()
		&"water_lift_reset":
			_native.request_water_lift_reset()
		&"pick_up":
			_native.request_pick_up()
		&"set_down":
			_native.request_set_down()
		&"hook", &"unhook":
			_native.request_rig()
		_:
			# Nothing reported in reach: ask anyway, exactly as the old E key
			# did. The native decides there is no ledge (and counts it).
			_native.request_traversal()


func _yard_crane_state() -> Dictionary:
	return _native.get_yard_crane_state() if _native.has_method("get_yard_crane_state") else {}


func _zero_yard_crane_input() -> void:
	if _native != null and _native.has_method("set_yard_crane_input"):
		_native.set_yard_crane_input(0.0, 0.0, 0.0)


func _deliver_yard_crane_input(intent: Dictionary) -> void:
	if not _native.has_method("set_yard_crane_input"):
		return
	var crane := _yard_crane_state()
	if _paused or _operating != &"yard_crane" or not bool(crane.get("controls_reachable", false)):
		_zero_yard_crane_input()
		return
	var axes: Vector3 = intent.get("yard_crane", Vector3.ZERO)
	if not axes.is_finite():
		axes = Vector3.ZERO
	_native.set_yard_crane_input(axes.x, axes.y, axes.z)


func _deliver_forklift_input(intent: Dictionary) -> void:
	var state: Dictionary = _native.get_forklift_state()
	if _paused or _operating != &"forklift" or not bool(state.get("controls_reachable", false)):
		_native.set_forklift_input(0.0, 0.0, 0.0, 0.0, 1.0)
		return
	var input: Array = _forklift_controls.sample(intent.get("forklift", {}),
		float(state.get("forward_speed_mps", 0.0)))
	# The native owner still requires current real operator-floor contact.
	# Looking around cannot change the truck-space steering or pedal axes.
	_native.set_forklift_input(input[0], input[1], input[2], input[3], input[4])


func _forklift_chassis_frame() -> Transform3D:
	var index := int(_native.get_kit_body_index(2440))
	return _native.get_kit_body_render_transform(index) if index >= 0 else Transform3D.IDENTITY


func _update_forklift_view_heading() -> void:
	if _operating != &"forklift":
		_forklift_view_heading_valid = false
		return
	var front := _forklift_chassis_frame().basis.z
	if Vector2(front.x, front.z).length_squared() < 0.001:
		return
	var heading := atan2(-front.x, -front.z)
	if _forklift_view_heading_valid:
		_yaw += wrapf(heading - _forklift_view_heading, -PI, PI)
	else:
		_yaw = heading
		_pitch = -0.12
	_forklift_view_heading = heading
	_forklift_view_heading_valid = true


func _end_operation() -> void:
	_native.cancel_forklift_tool_request()
	if _operating == &"yard_crane":
		_zero_yard_crane_input()
		_router.set_yard_crane_mode(false)
	if _operating == &"forklift":
		_native.set_forklift_input(0.0, 0.0, 0.0, 0.0, 1.0)
		_forklift_controls.leave()
		_router.set_forklift_mode(false)
		_forklift_view_heading_valid = false
	if _operating == &"grappling_gun":
		_native.set_grapple_reel_input(0.0)
		_native.set_grapple_equipped(false)
	if _operating == &"supplied_machine":
		_native.set_supplied_machine_input(0.0)
		_router.clear_held()
	_operating = &""


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
	var sling: Dictionary = _native.get_slingshot_state()
	var sling_seated := bool(sling.get("seated", false))
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var traversal := int(_native.get_traversal_state())
	var landing: Dictionary = _native.get_landing_state()
	var propping := bool(landing.get("palm_left_active", false)) or bool(landing.get("palm_right_active", false))
	var chute := bool(_native.is_parachute_deployed())
	var ledge := bool(_native.is_ledge_available())
	var lethal := float(_native.get_lethal_impact_speed_mps())
	var station := &""
	var grapple: Dictionary = _native.get_grapple_gun_state()
	var service_lift: Dictionary = _native.get_service_lift_state()
	var balance: Dictionary = _native.get_supplied_machine_state()
	var crane := _yard_crane_state()
	var forklift: Dictionary = _native.get_forklift_state()
	var water_lift_valve_station := bool(_native.is_water_lift_valve_station_active())
	var water_lift_release_station := bool(_native.is_water_lift_release_station_active())
	var water_lift_reset_station := bool(_native.is_water_lift_reset_station_active())
	if bool(grapple.get("equipped", false)):
		station = &"grappling_gun"
		# Native acknowledgment owns acquisition. A rendering frame before the
		# next90Hz tick cannot cancel a still-pending equip command.
		_operating = &"grappling_gun"
	elif bool(crane.get("controls_reachable", false)):
		station = &"yard_crane"
	elif bool(forklift.get("controls_reachable", false)):
		station = &"forklift"
	elif int(service_lift["station"]) != 0 or (_operating == &"service_lift" and int(service_lift["reachable_station"]) != 0):
		station = &"service_lift"
	elif int(balance["station"]) != 0 or (_operating == &"supplied_machine" and int(balance["reachable_station"]) != 0):
		station = &"supplied_machine"
	elif water_lift_valve_station or water_lift_release_station or water_lift_reset_station:
		station = &"water_lift"
	elif bool(_native.is_water_screw_station_active()):
		station = &"water_screw"
	elif bool(_native.is_intake_station_active()):
		station = &"intake"
	elif bool(_native.is_jib_station_active()):
		station = &"jib"
	elif bool(_native.is_needle_station_active()):
		station = &"needle"
	elif bool(_native.is_sump_station_active()):
		station = &"sump"
	var hanging := traversal == TRAVERSAL_HANGING
	var climbing := traversal == TRAVERSAL_CLIMBING
	var single_hand_hold := (hanging or climbing) \
		and int(landing.get("traversal_hand_constraint_count", 0)) == 1 \
		and bool(landing.get("left_hand_attached", false)) != bool(landing.get("right_hand_attached", false))
	var lowering := traversal == TRAVERSAL_LOWERING
	var free := traversal == TRAVERSAL_NONE
	var grip := bool(_native.is_grip_available())
	var edge_drop := bool(_native.is_edge_drop_available())
	# Reach preserves the pendant through brief unweighting; only native
	# supported footing authorizes drive effort on each physics tick.
	var lift_reachable := ((_operating == &"service_lift" and int(service_lift["reachable_station"]) != 0) \
		or (_operating == &"supplied_machine" and int(balance["reachable_station"]) != 0) \
		or (_operating == &"forklift" and bool(forklift.get("controls_reachable", false))) \
		or (_operating == &"yard_crane" and bool(crane.get("controls_reachable", false)))) \
		and free and int(_native.get_carrying_entity_id()) == 0
	var tool_active := station == &"grappling_gun" and free
	if _operating != &"" and (_operating != station or (not grounded and not lift_reachable and not tool_active) or not free \
			or (_operating in [&"service_lift", &"supplied_machine", &"forklift", &"yard_crane"] and not lift_reachable)):
		_end_operation()
	var climb_ok := grounded and free and ledge
	# AS-003: what is on the carry point, and what a pick-up would take --
	# both the native's own reading, never guessed here.
	var carrying := int(_native.get_carrying_entity_id())
	var carry_target := int(_native.get_carry_target_entity_id())
	var grapple_draw_available := free and carrying == 0 and not sling_seated \
		and not bool(sling.get("recovering", false)) \
		and (bool(grapple.get("acquired", false)) or bool(grapple.get("station_available", false)))
	# AS-006: the native's own rig reading -- hook what is carried onto the
	# anchor in reach, or take a slack hooked end off -- and what kind of
	# thing a pick-up would take.
	var rig := int(_native.get_rig_action())
	var rig_target := int(_native.get_rig_target_entity_id())
	var action := {"id": &"", "label": "", "icon": &"climb", "detail": ""}
	if bool(sling.get("can_retrieve", false)) or bool(sling.get("recovering", false)):
		action = {"id": &"slingshot", "label": "STOP RETRIEVAL" if bool(sling.get("recovering", false)) else "RETRIEVE POUCH", "icon": &"operate",
			"detail": "HOLD THE REEL CONTROL" if bool(sling.get("recovering", false)) else "RETURN THE SEAT"}
	elif (sling_seated or bool(sling.get("station_available", false))) and not bool(sling.get("released", false)):
		action = {"id": &"slingshot", "label": ("RELEASE" if bool(sling.get("release_ready", false)) else "DRAW MORE") if sling_seated else "ENTER POUCH", "icon": &"operate",
			"detail": "PULL BACK TO STRETCH" if sling_seated else "WOODEN SLINGSHOT"}
	elif hanging or climbing:
		if single_hand_hold:
			# Action asks for the missing hand even when its target is blocked.
			# Only Jump/Drop release; native reach/contact rules accept the hand.
			action = {"id": &"reach_hand", "label": "REACH HAND", "icon": &"climb",
				"detail": "" if bool(landing.get("missing_hand_grip_available", false)) else "NO CLEAR GRIP"}
		else:
			# Swing Action reaches a verified next bar. Jump/Drop release;
			# ordinary climbing still requests its contextual top-out.
			var swinging := bool(landing.get("player_swinging", false))
			action = {"id": &"climb_up", "label": "REACH BAR" if swinging else "CLIMB UP",
				"icon": &"climb", "detail": "LEAN TOWARD THE NEXT BAR" if swinging else ""}
	elif not free:
		pass
	elif carrying != 0 and rig == RIG_HOOK:
		action = {"id": &"hook", "label": "HOOK", "icon": &"hook",
			"detail": "ONTO " + _kit_anchor_name(rig_target)}
	elif carrying != 0:
		# Both hands are on it: letting go is the only thing Action can do.
		action = {"id": &"set_down", "label": "LET GO" if _is_kit(carrying) else "SET DOWN",
			"icon": &"set_down", "detail": _carry_name(carrying)}
	elif tool_active:
		var line_state := int(grapple.get("state", 0))
		action = {"id": &"grapple_action", "label": "LOCK LINE" if line_state == 1 else ("RECOVER HOOK" if bool(grapple.get("action_recovers_hook", line_state == 2)) else "FIRE HOOK"),
			"icon": &"hook", "detail": "TIGHTEN AT LIP, THEN STEER TOWARD LANDING" if line_state == 2 else "ARC THE HOOK OVER THE CROSSBAR"}
	elif _operating != &"":
		action = {"id": &"done", "label": "DONE", "icon": &"done", "detail": ""}
	elif grounded and free and bool(crane.get("rigging_reachable", false)):
		var leg := int(crane.get("rigging_leg", -1))
		var attached := leg >= 0 and (int(crane.get("attached_mask", 0)) & (1 << leg)) != 0
		action = {"id": &"yard_crane_detach" if attached else &"yard_crane_attach", "label": "UNRIG LEG" if attached else "RIG LEG", "icon": &"hook", "detail": "TRUCK EYE %d / 4" % (leg + 1)}
	elif grounded and carry_target != 0:
		# Ahead of CLIMB: the rack under the block is itself a mantle ledge,
		# and whoever faces the block means the block.
		var kind := int(_native.get_carry_target_kind())
		var verb := "GRAB" if kind == KIT_CARRY_HANDLE else ("TAKE" if kind == KIT_CARRY_SHACKLE else "PICK UP")
		action = {"id": &"pick_up", "label": verb, "icon": &"pick_up",
			"detail": _carry_name(carry_target)}
	elif grounded and rig == RIG_UNHOOK:
		action = {"id": &"unhook", "label": "UNHOOK", "icon": &"hook", "detail": "ROPE END"}
	elif grounded and station == &"water_lift":
		var bucket_water := float(_native.get_water_lift_bucket_water_m3())
		var fill_open := bool(_native.is_water_lift_valve_open())
		var bucket_caught := bool(_native.is_water_lift_bucket_catch_latched())
		var upper_caught := bool(_native.is_water_lift_upper_catch_latched())
		if water_lift_reset_station and upper_caught and bucket_water < 0.02:
			action = {"id": &"water_lift_reset", "label": "RESET LIFT", "icon": &"operate",
				"detail": "RETURN CAGE / RAISE EMPTY BUCKET"}
		elif water_lift_valve_station and (fill_open or bucket_water < 1.99):
			action = {"id": &"water_lift_valve",
				"label": "CLOSE FILL" if fill_open else "OPEN FILL",
				"icon": &"valve",
				"detail": "BUCKET %.2f / 2.00 M3" % bucket_water}
		elif water_lift_release_station and bucket_caught:
			action = {"id": &"water_lift_release", "label": "RELEASE LIFT", "icon": &"operate",
				"detail": "BUCKET %.0f KG" % float(_native.get_water_lift_bucket_mass_kg())}
	elif grounded and station == &"water_screw":
		var running := bool(_native.is_water_screw_motor_enabled())
		action = {"id": &"screw_toggle", "label": "STOP SCREW" if running else "START SCREW",
			"icon": &"operate", "detail": "UPPER TANK %.2f / 2.00 M3" % float(_native.get_water_screw_tank_volume_m3())}
	elif grounded and station == &"yard_crane":
		action = {"id": &"operate", "label": "ENTER CAB", "icon": &"operate", "detail": "YARD CRANE"}
	elif grounded and station == &"forklift":
		action = {"id": &"operate", "label": "OPERATE", "icon": &"operate", "detail": "FORKLIFT"}
	elif grounded and station == &"service_lift":
		action = {"id": &"operate", "label": "OPERATE", "icon": &"operate", "detail": "SERVICE LIFT"}
	elif grounded and station == &"supplied_machine":
		action = {"id": &"operate", "label": "OPERATE", "icon": &"operate", "detail": String(balance["name"])}
	elif not grounded and free and carrying == 0 and not chute and not bool(_native.is_player_crouched()):
		# Reach stays pressable before a grip appears. Only the native's short
		# intent window and current physical grip query can acquire a hand.
		action = {"id": &"catch", "label": "CATCH" if ledge or grip else "REACH", "icon": &"climb",
			"detail": "REACHING" if bool(landing.get("air_catch_intent_pending", false)) else ("HOLD ON" if ledge or grip else "TAP NEAR A GRIP")}
	elif climb_ok:
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb",
			"detail": "%+.1f M" % float(_native.get_ledge_rise_meters())}
	elif grounded and free and grip:
		# A hold at hand height: a rung, a pipe, a scaffold bar.
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb", "detail": "HOLD"}
	elif grounded and station in [&"intake", &"jib", &"needle"]:
		action = {"id": &"operate", "label": "OPERATE", "icon": &"operate",
			"detail": {&"intake": "YARD JIB", &"jib": "KX-JIB", &"needle": "KX-NEEDLE"}[station]}
	elif grounded and station == &"sump":
		action = {"id": &"valve", "icon": &"valve", "detail": "SUMP",
			"label": "OPEN VALVE" if bool(_native.is_sump_isolated()) else "CLOSE VALVE"}
	elif free and (bool(grapple.get("station_available", false)) or bool(grapple.get("acquired", false))):
		action = {"id": &"equip_grapple", "label": "DRAW GRAPPLE GUN", "icon": &"hook", "detail": ""}
	return {
		"position": position,
		"velocity": velocity,
		"grounded": grounded,
		"support_entity": int(_native.get_support_entity_id()),
		"crouched": bool(_native.is_player_crouched()),
		"traversal": traversal,
		"hanging": hanging,
		"single_hand_hold": single_hand_hold,
		"left_hand_generation": int(landing.get("left_hand_generation", 0)),
		"right_hand_generation": int(landing.get("right_hand_generation", 0)),
		"missing_hand_grip_available": bool(landing.get("missing_hand_grip_available", false)),
		"swinging": bool(landing.get("player_swinging", false)),
		"swing_tuck_available": bool(landing.get("swing_tuck_available", false)),
		"swing_tuck_requested": bool(landing.get("swing_tuck_requested", false)),
		"climbing": climbing,
		"lowering": lowering,
		# Drop at an edge behind lowers into a hang; on holds it lets go.
		"slingshot_seated": sling_seated,
		"slingshot_recovering": bool(sling.get("recovering", false)),
		"grapple_equipped": bool(grapple.get("equipped", false)),
		"grapple_draw_available": grapple_draw_available,
		"grapple_line_attached": bool(grapple.get("line_attached", false)),
		"grapple_auto_reel": bool(grapple.get("auto_reel", false)),
		"air_catch_intent_pending": bool(landing.get("air_catch_intent_pending", false)),
		"drop_ok": bool(landing.get("air_catch_intent_pending", false)) or bool(grapple.get("equipped", false)) or sling_seated or bool(sling.get("recovering", false)) or hanging or climbing or lowering or propping or (grounded and free and edge_drop and carrying == 0),
		"edge_drop": edge_drop,
		"sprinting": bool(_native.is_player_sprinting()),
		"balancing": bool(_native.is_player_balancing()),
		"chute": chute,
		"jump_ok": grounded and free and not sling_seated,
		"climb_ok": climb_ok,
		# Airborne, is_ledge_available is the native hang probe: an edge in
		# the grab band, which engages as soon as the player pushes into it.
		"grab_hint": not grounded and free and ledge,
		"chute_ok": not grounded and free and (chute or -velocity.y > CHUTE_OFFER_FALL_MPS \
			or (bool(sling.get("released", false)) and not sling_seated)),
		"danger": 0.0 if grounded else clampf(-velocity.y / maxf(lethal, 0.001), 0.0, 1.0),
		"lethal": lethal,
		"station": station,
		"operating": _operating,
		"forklift_operator": _forklift_controls.ui_state(),
		"forklift_tool_coupled": bool(forklift.get("coupled", false)),
		"forklift_tool_ready": bool(forklift.get("can_detach", false)) if bool(forklift.get("coupled", false)) else bool(forklift.get("can_couple", false)),
		"yard_crane_rigging_reachable": bool(crane.get("rigging_reachable", false)),
		"slung": bool(_native.is_legal_forty_pack_slung()),
		"carrying": carrying,
		"action": action,
		"checkpoint": _native.get_checkpoint_position(),
		"deaths": int(_native.get_death_count()),
		"tower_height": float(_native.get_tower_height_meters()),
		"panel": _station_panel(),
	}


func _carry_center(entity: int) -> Vector3:
	if entity == HOOK5_BLOCK_ENTITY_ID:
		return _native.get_hook5_block_position()
	if entity == HOOK5_BAR_ENTITY_ID:
		return _native.get_hook5_bar_position()
	if _is_kit(entity):
		var body := int(_native.get_kit_body_index(entity))
		if body >= 0:
			return _native.get_kit_carry_grip_position(body)
	return Vector3.ZERO


func _carry_half(entity: int) -> float:
	if entity == HOOK5_BLOCK_ENTITY_ID:
		return WorksiteMachineView.HOOK5_BLOCK_HALF
	if _is_kit(entity):
		var body := int(_native.get_kit_body_index(entity))
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body) if body >= 0 \
			else PackedFloat32Array()
		if parts.size() >= KIT_PART_FLOATS:
			return clampf(parts[0], 0.08, 0.30)
	return 0.18


func _carry_name(entity: int) -> String:
	if entity == HOOK5_BLOCK_ENTITY_ID:
		return "HOOK BLOCK"
	if entity == HOOK5_BAR_ENTITY_ID:
		return "DOOR BAR"
	match entity:
		1902:
			return "BALANCE BLOCK · 25 kg"
		2505:
			return "BRIDGE RELEASE"
		2530:
			return "PIPE RACK RELEASE"
		2602:
			return "STAIR CHAIN HANDLE"
		2002:
			return "ROPE SHACKLE"
		2004:
			return "TRIP HANDLE"
		2327:
			return "MAINTENANCE BLOCK"
	return "ROPE END" if _is_kit(entity) else ""


func _is_kit(entity: int) -> bool:
	return entity >= KIT_ENTITY_MIN and entity < KIT_ENTITY_MAX


func _kit_anchor_name(entity: int) -> String:
	match entity:
		1000:
			return "BOLLARD"
		2000:
			return "CAGE EYE"
	return "ANCHOR"


# The operated machine's own readouts, straight from its native getters.
# Row tone: 0 plain, 1 safe/engaged, 2 hazard.
func _station_panel() -> Dictionary:
	match _operating:
		&"yard_crane":
			return YardCranePanel.describe(_yard_crane_state(), _router.device)
		&"forklift":
			return ForkliftPanel.describe(_native.get_forklift_state(), _forklift_controls.ui_state(), _router.device)
		&"grappling_gun":
			var gun: Dictionary = _native.get_grapple_gun_state()
			var drop_label := "RELEASE" if bool(gun.get("line_attached", false)) else "STOW"
			var line_state := int(gun.get("state", 0))
			var action_label := "LOCK LINE" if line_state == 1 else ("RECOVER HOOK" if bool(gun.get("action_recovers_hook", false)) else "FIRE HOOK")
			var failure := str(gun.get("last_failure", ""))
			var status := failure if not failure.is_empty() else str(gun.get("state_name", "READY"))
			var verbs := [[&"action", action_label], [&"hoist", "TIGHTEN / PAY OUT"]]
			if bool(gun.get("line_attached", false)):
				verbs.append([&"auto_reel", "STOP REEL" if bool(gun.get("auto_reel", false)) else "AUTO REEL"])
				verbs.append([&"drop", "RELEASE"])
			verbs.append([&"toggle_grapple", "STOW"])
			return {"title": "GRAPPLING GUN", "subtitle": "UP: TIGHTEN / DOWN: PAY OUT / DROP: " + drop_label,
				"rows": [["SHOTS", str(int(gun.get("cartridges_remaining", 0))), 0],
					["LINE", "%.1f M" % float(gun.get("paid_out_length", 0.0)), 0],
					["TENSION", "%d N" % int(gun.get("tension_n", 0.0)), 2 if float(gun.get("tension_n", 0.0)) >= 2500.0 else 0],
					["STATUS", status, 2 if not failure.is_empty() else 0]],
				"verbs": verbs}
		&"supplied_machine":
			var balance: Dictionary = _native.get_supplied_machine_state()
			var cutoff := bool(balance["energy_cutoff"])
			if int(balance["index"]) == 7:
				return {
					"title": "SLAB HAUL / INSPECTION BAY" if int(balance["station"]) == 4 else "SLAB HAUL CART",
					"subtitle": "UP: RELEASE / DOWN: RETURN / LET GO: BRAKE",
					"rows": [
						["CART", "%+.1f M" % float(balance["surface_y"]), 0],
						["BRAKE", "HELD" if bool(balance["braking"]) else "RELEASED", 0],
						["RETURN RESERVE", "EMPTY" if cutoff else "%.0f%%" % (100.0 * float(balance["energy_j"]) / maxf(1.0, float(balance["capacity_j"]))), 2 if cutoff else 0],
					],
					"verbs": [[&"hoist", "RELEASE / RETURN"], [&"leave", "DONE"]],
				}
			if int(balance["index"]) == 6:
				var feeding := int(balance["station"]) == 2
				return {
					"title": "REFRACTORY RECLAIM",
					"subtitle": "UP: FEED / RELEASE: CLOSE" if feeding else "UP: RELEASE BRAKE / RELEASE: HOLD",
					"rows": [
						["HOPPER", "%.0f KG LEFT" % float(balance["hopper_mass_kg"]), 0],
						["CABIN", "%+.1f M" % float(balance["surface_y"]), 0],
						["BRAKE", "HELD" if bool(balance["braking"]) else "RELEASED", 0],
						["GATE / DOORS", "POWER EMPTY" if cutoff else "%.0f KJ" % (float(balance["energy_j"]) / 1000.0), 2 if cutoff else 0],
					],
					"verbs": [[&"hoist", "FEED / DUMP" if feeding else "RELEASE / DUMP"], [&"leave", "DONE"]],
				}
			return {
				"title": String(balance["name"]),
				"subtitle": "HOLD UP / DOWN / RELEASE TO BRAKE",
				"rows": [
					["DECK", "%+.1f M" % float(balance["surface_y"]), 0],
					["ENERGY", "%.0f / %.0f KJ" % [float(balance["energy_j"]) / 1000.0, float(balance["capacity_j"]) / 1000.0], 2 if cutoff else 0],
					["BRAKE", "APPLIED" if bool(balance["braking"]) else "RELEASED", 0],
					["POWER", "ENERGY CUTOFF" if cutoff else "%.1f KW" % (float(balance["power_w"]) / 1000.0), 2 if cutoff else 0],
				],
				"verbs": [[&"hoist", "UP / DOWN"], [&"leave", "DONE"]],
			}
		&"service_lift":
			var lift: Dictionary = _native.get_service_lift_state()
			var cutoff := bool(lift["energy_cutoff"])
			return {
				"title": "SERVICE LIFT",
				"subtitle": "HOLD UP / DOWN  /  RELEASE TO BRAKE",
				"rows": [
					["DECK", "%+.1f M" % float(lift["surface_y"]), 0],
					["ENERGY", "%.0f / %.0f KJ" % [float(lift["energy_j"]) / 1000.0, float(lift["capacity_j"]) / 1000.0], 2 if cutoff else 0],
					["BRAKE", "APPLIED" if bool(lift["braking"]) else "RELEASED", 0],
					["POWER", "ENERGY CUTOFF" if cutoff else "%.1f KW" % (float(lift["power_w"]) / 1000.0), 2 if cutoff else 0],
				],
				"verbs": [[&"hoist", "RAISE / LOWER"], [&"leave", "DONE"]],
			}
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
	&"release": [18, 0.4],
	&"strain": [20, 0.35],
	&"footplant": [18, 0.25],
	&"land": [34, 0.85],
	&"stumble": [28, 0.5],
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
				var heavy := kind in [&"land", &"stumble", &"death", &"warn"]
				Input.start_joy_vibration(_router.active_pad, amplitude * 0.8,
					amplitude if heavy else amplitude * 0.25, float(duration_ms) / 1000.0)


func _reset_foot_transfer_feedback(state: Dictionary) -> void:
	_fb_foot_transfer_count = int(state.get("foot_transfer_count", -1))
	_fb_foot_transfer_tick = int(state.get("foot_transfer_tick", -1))


func _consume_foot_transfer(state: Dictionary, deaths: int) -> float:
	var count := int(state.get("foot_transfer_count", -1))
	var tick := int(state.get("foot_transfer_tick", -1))
	var support := int(state.get("foot_transfer_support_entity_id", 0))
	var load := float(state.get("foot_transfer_peak_hand_load_n", 0.0))
	var new_transfer := _fb_foot_transfer_count >= 0 and count > _fb_foot_transfer_count \
		and tick > _fb_foot_transfer_tick and tick > 0 and support > 0 \
		and deaths == _fb_deaths and is_finite(load) and load >= 0.0
	# Startup, rewind, disabled vibration and restarts consume receipts quietly.
	# A skipped render sample emits the latest receipt once, never a catch-up burst.
	_fb_foot_transfer_count = count
	_fb_foot_transfer_tick = tick
	return clampf(load / 2400.0, 0.25, 1.0) if new_transfer else 0.0


func _reset_plank_feedback(state: Dictionary, tick: int) -> void:
	_fb_plank_fracture_count = int(state.get("plank_fracture_count", -1))
	_fb_plank_broken_mask = int(state.get("plank_broken_joint_mask", 0)) & 0x7ff
	var energy := float(state.get("plank_discarded_strain_energy_j", 0.0))
	_fb_plank_energy_j = maxf(energy, 0.0) if is_finite(energy) else 0.0
	_fb_plank_tick = tick
	_fb_plank_cue_tick = -100


func _consume_plank_fracture(state: Dictionary, deaths: int, tick: int) -> Dictionary:
	var count := int(state.get("plank_fracture_count", -1))
	var mask := int(state.get("plank_broken_joint_mask", 0)) & 0x7ff
	var energy := float(state.get("plank_discarded_strain_energy_j", 0.0))
	# A native rollback establishes a new cooldown timeline. Equal render
	# samples stay coalesced; the restored receipt itself still stays quiet.
	if tick < _fb_plank_tick or count < _fb_plank_fracture_count \
			or (_fb_plank_broken_mask & ~mask) != 0 or deaths != _fb_deaths:
		_fb_plank_cue_tick = -100
	var new_bits := mask & ~_fb_plank_broken_mask & 0x7ff
	var energy_delta := energy - _fb_plank_energy_j
	var new_break := _fb_plank_fracture_count >= 0 and count > _fb_plank_fracture_count \
		and new_bits != 0 and (_fb_plank_broken_mask & ~mask) == 0 \
		and tick > _fb_plank_tick and tick > 0 and deaths == _fb_deaths \
		and is_finite(energy) and energy >= _fb_plank_energy_j
	# Receipts are consumed even while sound/vibration is disabled. Restored
	# damage, rewind and missing state cannot replay a snap; a render gap
	# coalesces its new broken interfaces instead of firing count-delta voices.
	_fb_plank_fracture_count = count
	_fb_plank_broken_mask = mask
	_fb_plank_energy_j = maxf(energy, 0.0) if is_finite(energy) else 0.0
	_fb_plank_tick = tick
	if not new_break or tick - _fb_plank_cue_tick < 9 or _native == null:
		return {}
	var at := Vector3.ZERO
	var interfaces := 0
	for joint in 11:
		if (new_bits & (1 << joint)) == 0:
			continue
		var left := int(_native.get_kit_body_index(2880 + joint))
		var right := int(_native.get_kit_body_index(2881 + joint))
		if left < 0 or right < 0:
			continue
		var a: Transform3D = _native.get_kit_body_transform(left)
		var b: Transform3D = _native.get_kit_body_transform(right)
		# Current actual material endpoints, not a claimed historical rupture
		# position. This presentation lookup applies no force or body motion.
		var point := ((a * Vector3(0.1, 0.019, 0.0)) \
			+ (b * Vector3(-0.1, 0.019, 0.0))) * 0.5
		if point.is_finite():
			at += point
			interfaces += 1
	if interfaces == 0:
		return {}
	_fb_plank_cue_tick = tick
	# An authored restrained response to estimated discarded spring storage,
	# not acoustic-energy calibration or complete fracture-energy accounting.
	var strength := clampf(sqrt(maxf(energy_delta, 0.0) / 200.0), 0.35, 0.85)
	return {"at": at / float(interfaces), "strength": strength, "new_bits": new_bits}


func _reset_landing_feedback(state: Dictionary, tick: int) -> void:
	_fb_landing_count = int(state.get("landing_count", -1))
	_fb_landing_tick = tick


func _consume_landing_feedback(state: Dictionary, deaths: int, tick: int,
		lethal_speed: float) -> Dictionary:
	var count := int(state.get("landing_count", -1))
	var new_landing := _fb_landing_count >= 0 and count > _fb_landing_count \
		and tick > _fb_landing_tick and tick > 0 and deaths == _fb_deaths
	# Consume quiet, rewind and skipped samples too: only the latest native
	# receipt can earn one cue, and enabling vibration never replays it.
	_reset_landing_feedback(state, tick)
	if not new_landing:
		return {}
	var normal_speed := float(state.get("landing_normal_speed_mps", 0.0))
	if not is_finite(normal_speed):
		return {}
	normal_speed = maxf(normal_speed, 0.0)
	if normal_speed > 5.0:
		return {"kind": &"land", "strength": clampf(normal_speed / maxf(lethal_speed, 0.001), 0.25, 1.0)}
	var tangent_energy := float(state.get("landing_tangent_energy_j", 0.0))
	# Native sideways recovery can be substantial despite a gentle normal
	# approach. It earns a small body pulse, never a second impact cue.
	if int(state.get("landing_response", 0)) == 3 \
			and bool(state.get("landing_recovering", false)) \
			and int(state.get("landing_support_entity_id", 0)) > 0 \
			and is_finite(tangent_energy) and tangent_energy > 0.0:
		return {"kind": &"stumble", "strength": clampf(sqrt(tangent_energy / 3442.5), 0.25, 0.85)}
	return {}


# Shared selection lets the earlier audio update suppress its generic grab
# only when this same native sample will earn a positional contact cue.
func _grip_feedback_hand(state: Dictionary, traversal: int, tick: int, deaths: int,
		delta: float) -> int:
	if traversal not in [TRAVERSAL_HANGING, TRAVERSAL_CLIMBING] \
			or tick <= _grip_feedback_tick or deaths != _grip_feedback_deaths \
			or _grip_feedback_cooldown - delta > 0.0:
		return 0
	var left_generation := int(state.get("left_hand_generation", 0))
	var right_generation := int(state.get("right_hand_generation", 0))
	if bool(state.get("left_hand_attached", false)) and left_generation > 0 \
			and (not _grip_attached_left or left_generation != _grip_generation_left):
		return -1
	if bool(state.get("right_hand_attached", false)) and right_generation > 0 \
			and (not _grip_attached_right or right_generation != _grip_generation_right):
		return 1
	return 0


# State transitions become feedback: a grab, a landing, a canopy, a lethal
# fall warning, a restore, a higher checkpoint. Each reads native state only.
func _update_feedback(delta: float) -> void:
	var traversal: int = _ctx["traversal"]
	var entered_grip := traversal != _fb_traversal \
		and traversal in [TRAVERSAL_HANGING, TRAVERSAL_CLIMBING]
	var swinging := bool(_ctx["swinging"])
	if swinging and not _fb_swinging:
		_hud.toast("SWING", "LEAN WITH STICK · JUMP LETS GO", UiStyle.SAFE, 2.8)
	_fb_swinging = swinging
	if traversal != _fb_traversal:
		if traversal == TRAVERSAL_HANGING or traversal == TRAVERSAL_CLIMBING:
			_haptic(&"grab")
		elif traversal != TRAVERSAL_NONE:
			_haptic(&"tick")
		elif _fb_traversal == TRAVERSAL_CLIMBING and traversal == TRAVERSAL_NONE and (_ctx["velocity"] as Vector3).y > 1.0:
			_haptic(&"release")
		_fb_traversal = traversal
	# Native generations identify actual grasp/regrip events. Body swing,
	# moving supports and deforming grips cannot create contact sounds.
	var landing: Dictionary = _native.get_landing_state()
	var deaths: int = _ctx["deaths"]
	var grip_tick := int(_native.get_tick_index())
	var left_generation := int(landing.get("left_hand_generation", 0))
	var right_generation := int(landing.get("right_hand_generation", 0))
	var left_attached := bool(landing.get("left_hand_attached", false))
	var right_attached := bool(landing.get("right_hand_attached", false))
	var contact_hand := _grip_feedback_hand(landing, traversal, grip_tick, deaths, delta)
	_grip_feedback_cooldown = maxf(0.0, _grip_feedback_cooldown - delta)
	if contact_hand != 0:
		var at: Vector3 = _native.get_traversal_left_hand() if contact_hand < 0 \
			else _native.get_traversal_right_hand()
		if at.is_finite():
			if not entered_grip:
				_haptic(&"tick")
			_audio.climb_regrip(at, entered_grip)
			_grip_feedback_cooldown = 0.18
	# Consume quiet/cooldown/release/restore samples too; never replay them.
	_grip_generation_left = left_generation
	_grip_generation_right = right_generation
	_grip_attached_left = left_attached
	_grip_attached_right = right_attached
	_grip_feedback_tick = grip_tick
	_grip_feedback_deaths = deaths
	# Contact generations consume quiet/death/rewind samples too. A steady
	# loaded mesh cannot retrigger the short rope response every render frame.
	_catch_net_feedback_cooldown = maxf(0.0, _catch_net_feedback_cooldown - delta)
	var catch_state: Dictionary = _native.get_catch_net_state()
	var catch_serial := int(catch_state.get("contact_serial", 0))
	if _catch_net_feedback_serial >= 0 and catch_serial > _catch_net_feedback_serial \
			and deaths == _catch_net_feedback_deaths and _catch_net_feedback_cooldown <= 0.0:
		var approach := float(catch_state.get("approach_speed_mps", 0.0))
		if approach > 5.0 and is_finite(approach) and bool(catch_state.get("installed", false)):
			_audio.catch_net_strain(_native.get_player_position() - Vector3.UP * STAND_HALF_HEIGHT, approach)
			_catch_net_feedback_cooldown = 0.8
	_catch_net_feedback_serial = catch_serial
	_catch_net_feedback_deaths = deaths
	var grounded: bool = _ctx["grounded"]
	var velocity: Vector3 = _ctx["velocity"]
	var lethal: float = _ctx["lethal"]
	var foot_strength := _consume_foot_transfer(landing, deaths)
	if foot_strength > 0.0:
		_haptic(&"footplant", foot_strength)
	_catch_strain_cooldown = maxf(0.0, _catch_strain_cooldown - delta)
	var left_force: Vector3 = landing.get("left_hand_force_n", Vector3.ZERO)
	var right_force: Vector3 = landing.get("right_hand_force_n", Vector3.ZERO)
	var hand_load := (left_force + right_force).length() \
		if traversal in [TRAVERSAL_HANGING, TRAVERSAL_CLIMBING] else 0.0
	# A finite catch's force rise earns one short pulse. Steady body weight
	# stays quiet, and the initial grip already has its stronger grab cue.
	if not entered_grip and deaths == _fb_deaths and _catch_strain_cooldown <= 0.0 \
			and hand_load > 1200.0 and _last_hand_load <= 1200.0:
		_haptic(&"strain", clampf(hand_load / 2400.0, 0.5, 1.0))
		_catch_strain_cooldown = 0.3
	_last_hand_load = hand_load
	# The receipt survives a missed airborne/contact render frame and measures
	# approach relative to the actual support, including inclined moving decks.
	# Restores consume the receipt silently; death has its own feedback below.
	var feedback_tick := int(_native.get_tick_index())
	var fracture_cue := _consume_plank_fracture(landing, deaths, feedback_tick)
	if not fracture_cue.is_empty():
		_audio.timber_fracture(fracture_cue["at"], float(fracture_cue["strength"]))
	var landing_cue := _consume_landing_feedback(landing, deaths, feedback_tick, lethal)
	if not landing_cue.is_empty():
		_haptic(landing_cue["kind"], float(landing_cue["strength"]))
	elif not fracture_cue.is_empty() and not entered_grip and foot_strength <= 0.0:
		var fracture_at: Vector3 = fracture_cue["at"]
		if fracture_at.distance_to(_ctx["position"] as Vector3) <= 3.5:
			_haptic(&"stumble", float(fracture_cue["strength"]))
	# The native velocity read on the last airborne frame. Not the native's
	# last_impact_speed: after a restore the player re-settles within a tick
	# and that value is overwritten with the settle before any HUD sees it.
	# Retained only for the lethal-impact toast, not landing intensity.
	var fall_speed_before := _fb_fall_speed
	_fb_fall_speed = 0.0 if grounded else maxf(0.0, -velocity.y)

	if deaths > _fb_deaths:
		_fb_deaths = deaths
		_end_operation()
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
		TRAVERSAL_HANGING, TRAVERSAL_LOWERING:
			target = VIEW_PITCH_HANGING
		TRAVERSAL_CLIMBING:
			target = VIEW_PITCH_CLIMBING
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
		_arm_material(Color("1f231c"), 0.93, _geometry.bump_concrete, 9.0),
		_arm_material(Color("b8611d"), 0.55, null, 1.0, 0.1),
		_arm_material(Color("3b2616"), 0.72, _geometry.bump_steel, 14.0),
		_arm_material(Color("22170e"), 0.82, _geometry.bump_steel, 14.0),
		_arm_material(Color("7d5806"), 0.5, null, 1.0),
		_arm_material(Color("0c0c0d"), 0.62, null, 1.0),
		_arm_material(Color("303033"), 0.5, null, 1.0),
		_arm_material(Color("b8281b"), 0.45, null, 1.0),
		_arm_material(Color("59e08a"), 0.4, null, 1.0, 2.2),
		_arm_material(Color("d7d2c4"), 0.72, null, 1.0))


func _build_legs() -> void:
	_legs = FirstPersonLegs.new()
	_legs.name = "FirstPersonLegs"
	add_child(_legs)
	_legs.build(_arm_material(Color("1f231c"), 0.93, _geometry.bump_concrete, 9.0),
		_arm_material(Color("34271e"), 0.86, _geometry.bump_steel, 14.0),
		_arm_material(Color("101311"), 0.96, null, 1.0))


func _update_legs(body_state: Dictionary, delta: float) -> void:
	# Reuse this frame's native landing receipt and the existing body pose.
	# This dictionary is presentation input only; nothing feeds native motion.
	var state: Dictionary = body_state["landing"]
	var deaths := int(_ctx["deaths"])
	state["native_tick"] = int(_native.get_tick_index())
	state["render_position"] = body_state["render_position"]
	state["facing"] = Vector3.FORWARD.rotated(Vector3.UP, _yaw)
	state["player_grounded"] = body_state["grounded"]
	state["player_crouched"] = _ctx["crouched"]
	state["operating"] = body_state["operating"]
	state["dead"] = deaths != _leg_deaths
	state["velocity"] = body_state["velocity"]
	state["bob_phase"] = body_state["bob_phase"]
	state["crouch_visual_01"] = _crouch_eye
	_leg_deaths = deaths
	_legs.update_legs(state, _camera.global_transform, delta)


# Everything the arms may know, all of it native or already-presented state.
func _arms_state(intent: Dictionary) -> Dictionary:
	var velocity: Vector3 = _ctx["velocity"]
	if bool(_ctx["grounded"]):
		velocity -= _native.get_support_point_linear_velocity()
	var forklift_hands: Dictionary = _forklift_view.get_operator_hand_targets() if _forklift_view != null else {}
	var crane_hands: Dictionary = _yard_crane_view.get_operator_hand_targets() if _yard_crane_view != null else {}
	return {
		"yard_crane_active": _operating == &"yard_crane" and not crane_hands.is_empty(),
		"yard_crane_left_hand_world": crane_hands.get("left_hand_world", Vector3.ZERO),
		"yard_crane_right_hand_world": crane_hands.get("right_hand_world", Vector3.ZERO),
		"yard_crane_frame": crane_hands.get("operator_frame", Transform3D.IDENTITY),
		"traversal": int(_ctx["traversal"]),
		"progress": float(_native.get_traversal_progress()),
		"ledge_point": _native.get_traversal_ledge_point(),
		"render_position": _native.get_player_render_position(),
		"hand_left": _native.get_traversal_left_hand_render_position(),
		"hand_right": _native.get_traversal_right_hand_render_position(),
		"structure_normal": _native.get_traversal_normal(),
		"sprinting": bool(_ctx["sprinting"]),
		"balancing": bool(_ctx["balancing"]),
		"affordance": bool(_native.is_ledge_available()),
		"affordance_point": _native.get_ledge_point(),
		"position": _ctx["position"],
		"velocity": velocity,
		"landing": _native.get_landing_state(),
		"grounded": bool(_ctx["grounded"]),
		"chute": bool(_ctx["chute"]),
		"operating": _operating,
		"forklift_active": _operating == &"forklift",
		"forklift_left_hand_world": forklift_hands.get("left_hand_world", Vector3.ZERO),
		"forklift_right_hand_world": forklift_hands.get("right_hand_world", Vector3.ZERO),
		"forklift_frame": forklift_hands.get("operator_frame", Transform3D.IDENTITY),
		"carrying": int(_ctx["carrying"]),
		"grapple_equipped": bool(_ctx.get("grapple_equipped", false)),
		"grapple_frame": _grapple_view._tool.global_transform if _grapple_view != null else Transform3D.IDENTITY,
		"grapple_receiver_offset": _grapple_view.RECEIVER_OFFSET if _grapple_view != null else Vector3.ZERO,
		"carry_center": _carry_center(int(_ctx["carrying"])),
		"carry_half": _carry_half(int(_ctx["carrying"])),
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
			_legal_forty_value, _hook5_value]
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


# Bounded camera responses settle exactly to zero at rest. The physical player
# position is never filtered here: native simulation supplies the render pose.
func _apply_camera_feel(position: Vector3, velocity: Vector3, grounded: bool, crouched: bool,
		delta: float) -> void:
	var landing: Dictionary = _native.get_landing_state() if _native.has_method("get_landing_state") else {}
	var sliding := grounded and bool(landing.get("player_sliding", false))
	# A stationary rider is carried by the support without walking. Match the
	# footstep cadence's native point velocity, which also includes rotation.
	var locomotion_velocity: Vector3 = velocity
	if grounded:
		locomotion_velocity -= _native.get_support_point_linear_velocity()
	var horizontal_speed := Vector2(locomotion_velocity.x, locomotion_velocity.z).length()
	# 5.5 mirrors kPlayerMaximumRelativeSpeed (src/sim/simulation.cpp) -- a
	# curve-shape input, not a gameplay bound, so the native constant is not
	# exposed through the bridge just for this.
	var bob_target := 0.0
	if grounded and not sliding and int(_native.get_traversal_state()) == 0:
		bob_target = smoothstep(HEAD_BOB_SPEED_FLOOR_MPS, HEAD_BOB_SPEED_FULL_MPS, horizontal_speed)
		_cam_bob_phase += horizontal_speed * HEAD_BOB_CYCLES_PER_METER * TAU * delta
	if not _head_bob_on:
		_cam_bob_weight = 0.0
	else:
		_cam_bob_weight = lerpf(_cam_bob_weight, bob_target,
			1.0 - exp(-HEAD_BOB_RESPONSE_PER_SECOND * delta))
		if absf(_cam_bob_weight - bob_target) < 0.001:
			_cam_bob_weight = bob_target
	var vertical_bob := HEAD_BOB_VERTICAL_METERS * sin(_cam_bob_phase) * _cam_bob_weight
	var lateral_bob := HEAD_BOB_LATERAL_METERS * sin(_cam_bob_phase * 0.5) * _cam_bob_weight
	var right_vector := Vector3(cos(_yaw), 0.0, -sin(_yaw))
	var forward_vector := Vector3(-sin(_yaw), 0.0, -cos(_yaw))
	_landing_camera.update(landing, delta, _head_bob_on, right_vector, forward_vector,
		int(_native.get_death_count()))
	if _movement_dust != null:
		_movement_dust.update_from_native(landing, _native.get_support_contact_point(),
			locomotion_velocity, grounded, int(_native.get_support_entity_id()), delta,
			int(_native.get_death_count()), _settings.launch_cinematics)
	if not _head_bob_on:
		_cam_bank = 0.0
	else:
		var lateral_speed := locomotion_velocity.dot(right_vector)
		var bank_target := -CAMERA_BANK_MAX_RADIANS * clampf(lateral_speed / 5.5, -1.0, 1.0)
		_cam_bank = lerpf(_cam_bank, bank_target,
			1.0 - exp(-CAMERA_BANK_RESPONSE_PER_SECOND * delta))
		if absf(_cam_bank - bank_target) < 0.0001:
			_cam_bank = bank_target

	var eye := position + EYE_OFFSET
	_crouch_eye = move_toward(_crouch_eye, 1.0 if crouched else 0.0, delta / CROUCH_EYE_SECONDS)
	if crouched or _crouch_eye > 0.0:
		var soles_y := position.y - (CROUCH_HALF_HEIGHT if crouched else STAND_HALF_HEIGHT)
		eye.y = soles_y + lerpf(STAND_HALF_HEIGHT + EYE_OFFSET.y, CROUCH_EYE_OVER_SOLES,
			smoothstep(0.0, 1.0, _crouch_eye))
	_camera.position = eye + Vector3.UP * (vertical_bob + _landing_camera.translation.y + _landing_camera.foot_settle.x) \
		+ right_vector * (lateral_bob + _landing_camera.translation.x) \
		+ forward_vector * _landing_camera.translation.z
	var swing_target := 0.0
	var ladder := int(_native.get_kit_body_index(2960))
	if _head_bob_on and ladder >= 0 and int(_native.get_traversal_support_entity_id()) == 2960:
		var frame: Transform3D = _native.get_kit_body_render_transform(ladder)
		swing_target = clampf(frame.basis.get_euler().z * 0.30, -0.052, 0.052)
	_swing_bank = lerpf(_swing_bank, swing_target, 1.0 - exp(-8.0 * delta)) if _head_bob_on else 0.0
	_camera.rotation = Vector3(_pitch + _view_pitch_offset + _landing_camera.rotation.x + _landing_camera.foot_settle.y,
		_yaw, _cam_bank + _swing_bank + _landing_camera.rotation.y)

	var fov_ground := FOV_SPRINT_MAX_DEGREES * smoothstep(0.0, 5.5, horizontal_speed)
	var fov_fall := 0.0
	if not grounded and velocity.y < 0.0:
		fov_fall = FOV_FALL_MAX_DEGREES * smoothstep(0.0, FOV_FALL_FULL_MPS, -velocity.y)
	if not _speed_fov_on:
		_cam_fov_offset = 0.0
	else:
		var fov_target := fov_ground + fov_fall
		_cam_fov_offset = lerpf(_cam_fov_offset, fov_target,
			1.0 - exp(-FOV_RESPONSE_PER_SECOND * delta))
		if absf(_cam_fov_offset - fov_target) < 0.005:
			_cam_fov_offset = fov_target
	_camera.fov = _fov_base + _cam_fov_offset


func _render_snapshot(delta: float = 0.0) -> void:
	var position: Vector3 = _native.get_player_position()
	var render_position: Vector3 = _native.get_player_render_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var crouched := bool(_native.is_player_crouched())

	_update_forklift_view_heading()
	_apply_camera_feel(render_position, velocity, grounded, crouched, delta)
	if _ground_yard != null and _sky_cycle != null:
		_ground_yard.update(position, delta, not _head_bob_on, _sky_cycle.hour)
	if _yard_ambience != null:
		_yard_ambience.update(delta, position)
	if delta > 0.0:
		var ladder := int(_native.get_kit_body_index(2960))
		if ladder >= 0:
			var swing_velocity: Vector3 = _native.get_kit_body_linear_velocity(ladder)
			_audio.update_suspended_ladder(delta, swing_velocity, int(_native.get_traversal_support_entity_id()) == 2960)
			if int(_native.get_traversal_support_entity_id()) == 2960 and (_swing_last_velocity - swing_velocity).length() > 0.30:
				_haptic(&"strain", clampf((_swing_last_velocity - swing_velocity).length(), 0.3, 1.0))
			_swing_last_velocity = swing_velocity
		var reclaim_state: Dictionary = _native.get_supplied_machine_state()
		var deaths := int(_native.get_death_count())
		if deaths != _reclaim_feedback_deaths:
			_audio.reset_reclaim_feedback()
			if _reclaim_dust != null:
				_reclaim_dust.reset()
			_reclaim_feedback_deaths = deaths
		_audio.update_reclaim_impacts(reclaim_state, delta)
		if _reclaim_dust != null:
			reclaim_state["player_position"] = position
			_reclaim_dust.set_reduced_motion(not _settings.launch_cinematics)
			_reclaim_dust.update_from_native(reclaim_state, delta)
		var audio_landing: Dictionary = _native.get_landing_state()
		var audio_tick := int(_native.get_tick_index())
		_audio.update_ground_swing(_native, position, audio_tick, deaths)
		_audio.update(delta, position, velocity, grounded, int(_native.get_support_entity_id()),
			int(_native.get_traversal_state()), bool(_native.is_parachute_deployed()),
			int(_native.get_death_count()), crouched, _native.get_support_point_linear_velocity(),
			audio_landing,
			_grip_feedback_hand(audio_landing, int(_native.get_traversal_state()),
				audio_tick, int(_native.get_death_count()), delta) != 0, audio_tick)
		if _regression_scene:
			_audio.update_machines(delta, float(_native.get_orifice_mass_flow_kg_per_s()),
				_native.get_hoist_scoop_position(), _native.get_ballast_position(),
				_native.get_ballast_linear_velocity(), _native.get_tipper_position(),
				float(_native.get_tipper_angle_radians()), _native.get_lift_platform_position(),
				_native.get_lift_platform_linear_velocity())
			_audio.update_water_screw(float(_native.get_water_screw_rpm()),
				float(_native.get_water_screw_motor_torque_nm()))
			_audio.update_water_lift(float(_native.get_water_lift_valve_flow_m3_s()),
				float(_native.get_water_lift_rope_tension_n()))
		else:
			_audio.update_pipe_bridge(_native.get_pipe_bridge_audio_state(), int(_native.get_death_count()))
	# The developer telemetry overlay costs a dozen string formats a frame;
	# it is only paid for while the overlay is actually on screen.
	if _telemetry_on:
		_write_telemetry(position, velocity, grounded)
	if _regression_scene:
		_machine_view.mirror(_native, float(_native.get_valve_open_fraction()),
			float(_native.get_orifice_mass_flow_kg_per_s()))
		_machine_view.mirror_water_screw(_native)
	_kit_view.render_view()
	if _freight_activity != null:
		_freight_activity.update_from_native(delta, position, not _settings.launch_cinematics)
	var crane_state := _yard_crane_state()
	_yard_crane_view.render_state(crane_state)
	_yard_crane_audio.update_from_native(crane_state, delta)
	var forklift_frame := _forklift_chassis_frame()
	_forklift_view.render_state(_native.get_forklift_render_state(), forklift_frame)
	var forklift_state: Dictionary = _native.get_forklift_state()
	_forklift_audio.update_from_native(forklift_state, forklift_frame.origin,
		_operating == &"forklift" and bool(forklift_state.get("controls_reachable", false)) \
		and bool(_forklift_controls.ui_state().get("horn", false)), delta)
	if _inspection_balance_feedback != null:
		_inspection_balance_feedback.update_from_native(_native, delta, not _settings.launch_cinematics)
	if _grapple_view != null:
		_update_grapple_feedback(delta)
		_grapple_view.update_state(_native.get_grapple_gun_render_state())


func _reset_grapple_feedback() -> void:
	_grapple_feedback.reset()
	if _grapple_view != null:
		_grapple_view.reset_feedback()
	if _audio != null:
		_audio.reset_grapple_feedback()


func _update_grapple_feedback(delta: float) -> void:
	# Consume native receipts once per90Hz tick; interpolated hook poses never
	# create sound, motor work, launch recoil or a claim of physical capture.
	var feedback := _grapple_feedback.sample(int(_native.get_tick_index()),
		int(_native.get_death_count()), _native.get_grapple_gun_state())
	if bool(feedback.get("reset", false)):
		_grapple_view.reset_feedback()
		_audio.reset_grapple_feedback()
	_grapple_view.present_feedback(feedback, delta, not _settings.launch_cinematics)
	_audio.update_grapple_feedback(feedback, delta)


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

	# AS-003. CAP-HOOK5 is the block's pose read back (hook_in_rack false),
	# never a stored bit; the door angle is the hinge's own reading.
	var carrying := int(_native.get_carrying_entity_id())
	var hook_in_rack := bool(_native.is_hook_in_rack())
	var door := float(_native.get_hook5_door_angle_radians())
	_hook5_value.text = "HOOK 5  DOOR %4.2f rad  %s  HANDS %s" % [door,
		"BLOCK IN RACK" if hook_in_rack else "CAP-HOOK5 (BLOCK OFF RACK)",
		{0: "free", HOOK5_BAR_ENTITY_ID: "BAR", HOOK5_BLOCK_ENTITY_ID: "HOOK BLOCK"}.get(
			carrying, "?")]
	_hook5_value.modulate = Color("9ad6c4") if not hook_in_rack else Color("d99a4a")

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


# --- world ------------------------------------------------------------------
#
# Industrial palette: mill scale, oxidised steel, poured concrete, wet asphalt,
# galvanised mesh, faded warning yellow, chipped hazard orange. Nothing here
# emits light except a sodium fitting, a fire box, or a vent -- colour is a
# consequence of material and weather, not a shader preset.


func _build_world() -> void:
	_geometry = WorldGeometry.new($TowerPresentation)
	_stack_world = StackWorldBuilder.new(_geometry, _light_rig, _regression_scene)
	_alpine_world = AlpineWorldBuilder.new(_geometry)
	_machine_view = WorksiteMachineView.new(_geometry, _light_rig, _stack_world, _regression_scene, self)
	_geometry.build_bump_textures()

	# Palette: oxidised iron and rust carry the structure, weathered timber
	# softens it, mill scale is the dark shadow value, crane yellow and brass
	# lamplight are the warm accents -- and now verdigris copper, painted
	# machinery blue, and lichen staining break the rust/iron monochrome, the
	# way a real decades-old industrial site actually weathers.
	var asphalt := ShaderMaterial.new()
	asphalt.shader = preload("res://presentation/yard_asphalt.gdshader")
	var concrete := _geometry.material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _geometry.bump_concrete)
	# A real value ladder: near-black iron in shadow, mid rust for the frame,
	# brighter oxide only where light catches an edge. Bump-mapped: these
	# cover most of the structure's surface area, so this is where per-pixel
	# normal detail matters most.
	var mill_scale := _geometry.material(Color("1d1a17"), 0.72, 0.6, Color.BLACK, 1.0, _geometry.bump_steel)
	var oxidised := _geometry.material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _geometry.bump_steel)
	var rust_deep := _geometry.material(Color("3b1f13"), 0.28, 0.95, Color.BLACK, 1.0, _geometry.bump_steel)
	var rust_bright := _geometry.material(Color("9a5326"), 0.34, 0.82, Color.BLACK, 1.0, _geometry.bump_steel)
	var galvanised := _geometry.material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _geometry.bump_steel)
	var faded_yellow := _geometry.material(Color("b08a22"), 0.16, 0.68)
	var hazard := _geometry.material(Color("a04d16"), 0.18, 0.76)
	var tar := _geometry.material(Color("0e0f11"), 0.05, 0.62)
	var timber := _geometry.material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _geometry.bump_timber)
	# New accents: living colour against the rust.
	var verdigris := _geometry.material(Color("3f6b5c"), 0.42, 0.68, Color.BLACK, 1.0, _geometry.bump_steel)
	var machine_blue := _geometry.material(Color("29455c"), 0.22, 0.6, Color.BLACK, 1.0, _geometry.bump_steel)
	var lichen := _geometry.material(Color("57642e"), 0.0, 0.96)

	# Grade and the tower's upper mass: sizes mirror the native Jolt bodies.
	_geometry.add_box("Grade", Vector3(480.0, 1.0, 480.0), Vector3(0.0, -0.5, -60.0), asphalt)
	# The neighbouring shaft stands on the ground: 1.6 km from grade up.
	var shaft_concrete := ShaderMaterial.new()
	shaft_concrete.shader = preload("res://presentation/materials/shaft_concrete.gdshader")
	_geometry.add_box("TowerMass", Vector3(92.0, 1600.0, 80.0), Vector3(-30.0, 800.0, -330.0), shaft_concrete)

	_stack_world.build(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded_yellow, timber)
	if _regression_scene:
		_stack_world.build_accents(verdigris, machine_blue, lichen)
		_stack_world.build_tower_skin(mill_scale, oxidised, galvanised, faded_yellow, timber)
		_machine_view.build_intake_rise(concrete, mill_scale, oxidised, rust_deep, rust_bright,
			galvanised, faded_yellow, hazard, timber)
		_machine_view.build_water_screw(concrete, mill_scale, oxidised, galvanised, faded_yellow, hazard)
		_machine_view.build_legal_forty(concrete, mill_scale, oxidised, rust_deep, rust_bright,
			galvanised, faded_yellow, hazard, timber)
		_machine_view.build_hook5_rack(concrete, mill_scale, oxidised, galvanised, faded_yellow, hazard)
		_machine_view.build_yard(concrete, mill_scale, faded_yellow, tar)
		_machine_view.build_legacy_fixtures(mill_scale, galvanised, hazard, faded_yellow)
		_machine_view.build_plant(mill_scale, oxidised, galvanised, hazard, faded_yellow)
	else:
		_stack_world.build_continuation(oxidised, rust_deep, mill_scale, galvanised, faded_yellow)
	if not _regression_scene:
		_ground_yard = GroundYard.new(_geometry)
		_ground_yard.build()
	_alpine_world.build_backdrop()
	_build_kellerworks_signage(timber, faded_yellow)
	if _regression_scene:
		_machine_view.build_gear_motif(mill_scale, oxidised)
		_machine_view.build_crane(mill_scale, hazard)
	_alpine_world.build_foliage()
	_machine_view.build_lighting()
	if not _regression_scene:
		# Append the practice yard after existing exported geometry so its
		# arrival does not reorder established box/hull entries.
		ParkourYard.new(_geometry).build()
# Slightly darker variant of a base material, for members that should sit back
# a value step without defining a whole new palette entry.
func rust_deepen(source: Material) -> Material:
	return _stack_world.rust_deepen(source)


func _build_kellerworks_signage(timber: Material, faded: Material) -> void:
	# Painted banners (identity doc items 1 and 7): real Label3D copy over a
	# backing plane, plus an original chevron-K mark. Non-collidable.
	var backing := _geometry.material(Color("4a231c"), 0.05, 0.85)
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
	_geometry.add_box("WordmarkBacking", Vector3(20.0, 5.0, 0.4), Vector3(0.0, 46.0, -142.3), backing)
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

	if not _regression_scene:
		return
	# Lift signage near the platform (identity doc item 7): a display
	# designation distinct from the internal native entity ID (16).
	_geometry.add_box("LiftSignBacking", Vector3(1.6, 1.0, 0.12), Vector3(13.0, 8.7, -99.1), backing)
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
	_geometry.add_box("BannerBacking", Vector3(3.2, 4.6, 0.12), at + Vector3(0.0, 4.6, 0.0), backing)
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
	_geometry.add_box_to("ChevronSpine", Vector3(0.14, 0.9, 0.05) * scale, Vector3(-0.32, 0.0, 0.0) * scale, accent, mark)
	_geometry.add_box_to("ChevronUpper", Vector3(0.62, 0.14, 0.05) * scale, Vector3(0.05, 0.24, 0.0) * scale, accent, mark, -0.55)
	_geometry.add_box_to("ChevronLower", Vector3(0.62, 0.14, 0.05) * scale, Vector3(0.05, -0.24, 0.0) * scale, accent, mark, 0.55)


# AS-006: the mechanism kit. Every kit body is declared by the native -- its
# boxes, each box's material class, its pose -- and drawn here from that
# declaration, so what the player sees is exactly what collides; nothing is
# authored twice. Kit bodies live outside $TowerPresentation: they are native
# collision already, never part of the exported solid dressing.
func _on_inspection_span_strain(at: Vector3, strength: float) -> void:
	if _audio != null:
		_audio.inspection_span_strain(at, strength)


func _setup_kit_view() -> void:
	_kit_view = KitView.new()
	_inspection_worksite_life = InspectionWorksiteLife.new()
	_inspection_worksite_life.name = "InspectionWorksiteLife"
	add_child(_inspection_worksite_life)
	_inspection_balance_feedback = InspectionBalanceFeedback.new()
	_inspection_balance_feedback.name = "InspectionBalanceFeedback"
	_inspection_balance_feedback.span_strain.connect(_on_inspection_span_strain)
	add_child(_inspection_balance_feedback)
	_kit_view.name = "KitPresentation"
	add_child(_kit_view)
	# Indexed by the native's Material enum: steel, rust, timber, concrete,
	# hazard, galvanised, rubble, yellow, refractory brick.
	var palette: Array[Material] = [
		_geometry.material(Color("2a2723"), 0.7, 0.55, Color.BLACK, 1.0, _geometry.bump_steel),
		_geometry.material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _geometry.bump_steel),
		_geometry.material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _geometry.bump_timber),
		_geometry.material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _geometry.bump_concrete),
		_geometry.material(Color("a04d16"), 0.18, 0.76),
		_geometry.material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _geometry.bump_steel),
		_geometry.material(Color("5b544b"), 0.0, 0.98, Color.BLACK, 1.0, _geometry.bump_concrete),
		_geometry.material(Color("c19a2a"), 0.2, 0.62),
		preload("res://presentation/materials/refractory_material.gd").build(),
	]
	if not _regression_scene:
		# Broad painted/cast surfaces remain readable at normal play distance.
		palette[0] = _geometry.material(Color("687174"), 0.35, 0.78)
		palette[1] = _geometry.material(Color("86472d"), 0.12, 0.92)
		palette[5] = _geometry.material(Color("879394"), 0.45, 0.68)
	var cargo_material := _geometry.material(Color("dfb981"), 0.0, 0.97)
	cargo_material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var cable_material := _geometry.material(Color("1b1916") if _regression_scene else Color("b2a383"), 0.6, 0.5)
	_kit_view.setup(_native, palette, cable_material, cargo_material, _geometry.add_sign_text)
	if not _regression_scene:
		_movement_dust = MovementDust.new()
		_movement_dust.name = "MovementDust"
		add_child(_movement_dust)
		_reclaim_dust = ReclaimDust.new()
		_reclaim_dust.name = "ReclaimDust"
		add_child(_reclaim_dust)
		_reclaim_dust.setup()


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
