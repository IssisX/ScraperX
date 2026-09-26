extends Node3D

# Presentation and input only. Every consequential fact below is read from the
# native ScraperX simulation; nothing here decides pose, support, traversal, or
# machine state. Where this file draws something that looks simulated, it is
# driven by an authoritative native value, so freezing that value freezes the
# effect.

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
# A Jump pressed this long before touchdown fires on the grounded tick. Input
# timing assistance only (GDD 7.2): it never jumps from anything the native
# does not report as ground at the moment it fires.
const JUMP_BUFFER_SECONDS := 0.12
# A second Jump this soon after one was sent goes straight to the native,
# which vaults if the takeoff could have (its own window is 0.30 s of ticks;
# this is a frame's slack wider so the native, not frame timing, decides).
const DOUBLE_TAP_SECONDS := 0.35
const CHECKPOINT_TOAST_RISE_METERS := 3.0

# Locomotion-feel camera response. Pure presentation, driven every frame by
# native player position/velocity/grounded state already read below -- never
# the other way around, and never touching _native itself. Every term is a
# function of CURRENT state (speed, ground contact, the last frame's own
# vertical velocity), not an accumulating drift, so it always returns to
# exactly baseline (no dip, no bob, FOV_BASE) the moment the player is
# grounded and stationary -- the CI runtime proof's own hold, where
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


# The stack. Mirrors the kStack* constants in simulation.cpp exactly -- these
# are the native collision sizes, so what is drawn is what you stand on.
const STACK_CENTER := Vector3(0.0, 0.0, -150.0)
const STACK_HALF_EXTENT := 26.0
const STACK_LEVEL_HEIGHT := 11.0
const STACK_LEVEL_COUNT := 14
const STACK_DECK_THICKNESS := 0.5
const STACK_DECK_BAND_DEPTH := 9.0
const STACK_COLUMN_SIZE := 1.6

# AS-006: the mechanism kit's entity ids (sim/mechanism_kit.hpp): band
# structure from 1000, bodies that move from 2000. What a kit body is, how
# big, what it is made of and where it is all come from the native.
const KIT_ENTITY_MIN := 1000
const KIT_DYNAMIC_ENTITY_MIN := 2000
const KIT_ENTITY_MAX := 3000
const KIT_PART_FLOATS := 11
const KIT_CABLE_SEGMENTS := 4
const KIT_CARRY_SHACKLE := 1
const KIT_CARRY_HANDLE := 2
# Rubble, the native's declared granular model: a mass in each bin, a stream
# while one pours, piles where spills land. Drawn at a loose bulk density; a
# pile is drawn as a low skirt, no taller than a step, since it has no body.
const KIT_BIN_FLOATS := 11
const KIT_POOL_FLOATS := 7
const KIT_SPOUT_FLOATS := 7
const KIT_PILE_FLOATS := 4
const KIT_MAX_PILES := 16
const RUBBLE_DENSITY := 1600.0
const RUBBLE_PILE_HEIGHT := 0.05
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
const PHASE_PROVEN := 2

# The tower face sits at z = -124. Walking to z = -66 puts its lower third
# across the whole frame.
const CI_APPROACH_TARGET_Z := -66.0
const CI_APPROACH_FACING := Vector2(-0.22, -0.975)
const CI_HOLD_TICKS := 20

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
var _settings: SettingsStore
var _uitest: Node
var _force_touch := false
var _uitest_scenario := ""
var _paused := false
var _telemetry_on := false
var _ctx := {}
var _jump_buffer := 0.0
var _since_jump_sent := INF
# The crouch toggle (C, right-stick click, the touch button). Held Ctrl
# crouches too, for as long as it is held; the native decides whether the
# body can crouch or stand.
var _crouch_toggled := false
var _fb_traversal := 0
var _fb_grounded := true
var _fb_fall_speed := 0.0
var _fb_deaths := 0
var _fb_chute := false
var _fb_warned := false
var _fb_best_checkpoint_y := 0.0
var _arms: Node3D
var _view_pitch_offset := 0.0

var _kit_root: Node3D
var _kit_bodies: Array[Node3D] = []
var _kit_dynamic: Array[bool] = []
var _kit_cables: Array = []
# Per bin: the rubble layer on its floor (null for a static bin, whose
# contents cannot be seen), that floor's box, and its stream.
var _kit_bin_layers: Array = []
var _kit_bin_floors: Array = []
var _kit_bin_streams: Array[MeshInstance3D] = []
var _kit_piles: Array[MeshInstance3D] = []
# AS-007 water: a body of water per pool, a stream per spout.
var _kit_pools: Array[MeshInstance3D] = []
var _kit_spouts: Array[MeshInstance3D] = []
var _ci_phase := PHASE_APPROACH
var _ci_facing := CI_APPROACH_FACING
var _ci_proof_tick := -1
var _ci_proof_printed := false

@onready var _camera: Camera3D = $Camera
@onready var _status: Label = $HUD/TopLeft/Status
@onready var _position_value: Label = $HUD/TopLeft/Position
@onready var _velocity_value: Label = $HUD/TopLeft/Velocity
@onready var _support_value: Label = $HUD/TopLeft/Support
@onready var _traversal_value: Label = $HUD/TopLeft/Traversal
@onready var _tick_value: Label = $HUD/TopRight/Tick
@onready var _fall_value: Label = $HUD/TopLeft/Fall
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
	_audio.sky = _sky_cycle
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

	# The playtest start point (pause menu), before the native clock moves.
	if _uitest_scenario.is_empty() and _settings.start_at > 0:
		var spawn := int(_settings.START_SPAWNS[_settings.start_at])
		if spawn >= 0:
			_native.configure_initial_spawn(spawn)

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

	_build_kit()

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
	_native.set_crouch_input(_crouch_toggled or bool(intent["crouch_held"]))
	_native.set_sprint_input(bool(intent["sprint_held"]))


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


# --- CI sequence: walk the approach, stop, hold, prove ------------------------


func _ci_movement_intent(position: Vector3) -> Vector2:
	if _ci_phase == PHASE_APPROACH:
		if position.z <= CI_APPROACH_TARGET_Z:
			_ci_phase = PHASE_PROVEN
			_ci_proof_tick = int(_native.get_tick_index())
			_print_ci_phase("ARRIVED")
			return Vector2.ZERO
		_ci_facing = CI_APPROACH_FACING
		return Vector2(0.0, 1.0)
	return Vector2.ZERO


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
	_pause_menu.restart_requested.connect(func() -> void:
		get_tree().paused = false
		get_tree().reload_current_scene())
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
				# A jump stands a crouched body first (natively), and ends the toggle.
				_crouch_toggled = false
				if _ctx["jump_ok"] or _ctx["hanging"] or _ctx["climbing"]:
					_native.request_jump()
					_since_jump_sent = 0.0
				else:
					if _since_jump_sent <= DOUBLE_TAP_SECONDS:
						_native.request_jump()
					_jump_buffer = JUMP_BUFFER_SECONDS
			&"action":
				_crouch_toggled = false
				_perform_action()
			&"crouch":
				# Does what the button says: crouched, it asks to stand.
				_crouch_toggled = not bool(_ctx["crouched"])
			&"drop":
				# Let go of whatever the hands are on: a ledge, a climb's
				# holds, or a load. On the ground with an edge behind, the
				# native lowers the body over it into a hang.
				if int(_ctx["carrying"]) != 0:
					_native.request_set_down()
				else:
					_native.request_release()
			&"chute":
				_native.request_parachute()
			&"back":
				if _ctx["hanging"] or _ctx["climbing"]:
					_native.request_release()
				elif int(_ctx["carrying"]) != 0:
					_native.request_set_down()
				elif _ctx["drop_ok"]:
					_native.request_release()
			&"alt":
				if not _ctx["grounded"]:
					_native.request_parachute()
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
# native affordance reports, takes hold of what is in reach, or hooks what is
# held. It never operates a machine on the player's behalf.
func _perform_action() -> void:
	var action: Dictionary = _ctx["action"]
	match action["id"]:
		&"climb_up", &"climb":
			_native.request_traversal()
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


func _read_context() -> Dictionary:
	var position: Vector3 = _native.get_player_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var traversal := int(_native.get_traversal_state())
	var chute := bool(_native.is_parachute_deployed())
	var ledge := bool(_native.is_ledge_available())
	var lethal := float(_native.get_lethal_impact_speed_mps())
	var hanging := traversal == TRAVERSAL_HANGING
	var climbing := traversal == TRAVERSAL_CLIMBING
	var free := traversal == TRAVERSAL_NONE
	var grip := bool(_native.is_grip_available())
	var edge_drop := bool(_native.is_edge_drop_available())
	var climb_ok := grounded and free and ledge
	# What is on the carry point, and what a pick-up would take -- both the
	# native's own reading, never guessed here.
	var carrying := int(_native.get_carrying_entity_id())
	var carry_target := int(_native.get_carry_target_entity_id())
	# AS-006: the native's own rig reading -- hook what is carried onto the
	# anchor in reach, or take a slack hooked end off -- and what kind of
	# thing a pick-up would take.
	var rig := int(_native.get_rig_action())
	var rig_target := int(_native.get_rig_target_entity_id())
	var action := {"id": &"", "label": "", "icon": &"climb", "detail": ""}
	if hanging or climbing:
		action = {"id": &"climb_up", "label": "CLIMB UP", "icon": &"climb", "detail": ""}
	elif not free:
		pass
	elif carrying != 0 and rig == RIG_HOOK:
		action = {"id": &"hook", "label": "HOOK", "icon": &"hook",
			"detail": "ONTO " + _kit_anchor_name(rig_target)}
	elif carrying != 0:
		# Both hands are on it: letting go is the only thing Action can do.
		action = {"id": &"set_down", "label": "LET GO", "icon": &"set_down",
			"detail": _carry_name(carrying)}
	elif grounded and carry_target != 0:
		# Ahead of CLIMB: whatever a load rests on may itself be a mantle
		# ledge, and whoever faces the load means the load.
		var kind := int(_native.get_carry_target_kind())
		var verb := "GRAB" if kind == KIT_CARRY_HANDLE else ("TAKE" if kind == KIT_CARRY_SHACKLE else "PICK UP")
		action = {"id": &"pick_up", "label": verb, "icon": &"pick_up",
			"detail": _carry_name(carry_target)}
	elif grounded and rig == RIG_UNHOOK:
		action = {"id": &"unhook", "label": "UNHOOK", "icon": &"hook", "detail": "ROPE END"}
	elif climb_ok:
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb",
			"detail": "%+.1f M" % float(_native.get_ledge_rise_meters())}
	elif grounded and free and grip:
		# A hold at hand height: a rung, a pipe, a scaffold bar.
		action = {"id": &"climb", "label": "CLIMB", "icon": &"climb", "detail": "HOLD"}
	return {
		"position": position,
		"velocity": velocity,
		"grounded": grounded,
		"crouched": bool(_native.is_player_crouched()),
		"traversal": traversal,
		"hanging": hanging,
		"climbing": climbing,
		# Drop at an edge behind lowers into a hang; on holds it lets go.
		"drop_ok": hanging or climbing or (grounded and free and edge_drop and carrying == 0),
		"edge_drop": edge_drop,
		"sprinting": bool(_native.is_player_sprinting()),
		"balancing": bool(_native.is_player_balancing()),
		"chute": chute,
		"jump_ok": grounded and free,
		"climb_ok": climb_ok,
		# Airborne, is_ledge_available is the native hang probe: an edge in
		# the grab band, which engages as soon as the player pushes into it.
		"grab_hint": not grounded and free and ledge,
		"chute_ok": not grounded and free and (chute or -velocity.y > CHUTE_OFFER_FALL_MPS),
		"danger": 0.0 if grounded else clampf(-velocity.y / maxf(lethal, 0.001), 0.0, 1.0),
		"lethal": lethal,
		"carrying": carrying,
		"action": action,
		"checkpoint": _native.get_checkpoint_position(),
		"deaths": int(_native.get_death_count()),
		"tower_height": float(_native.get_tower_height_meters()),
	}


func _carry_center(entity: int) -> Vector3:
	if _is_kit(entity):
		var body := int(_native.get_kit_body_index(entity))
		if body >= 0:
			return (_native.get_kit_body_transform(body) as Transform3D).origin
	return Vector3.ZERO


func _carry_half(entity: int) -> float:
	if _is_kit(entity):
		var body := int(_native.get_kit_body_index(entity))
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body) if body >= 0 \
			else PackedFloat32Array()
		if parts.size() >= KIT_PART_FLOATS:
			return clampf(parts[0], 0.08, 0.30)
	return 0.18


func _carry_name(entity: int) -> String:
	match entity:
		2002, 2012:
			return "ROPE SHACKLE"
		2004, 2014:
			return "TRIP HANDLE"
		2006:
			return "TIP-OUT HANDLE"
		2024:
			return "REBAR LINE"
		2026:
			return "LATCH HANDLE"
		2031:
			return "PIPE SPOOL"
		2033:
			return "FILL VALVE"
		2035:
			return "DRAIN HANDLE"
		2042:
			return "DOOR"
		2043:
			return "DOOR LATCH"
		2045:
			return "CHILLER TRIP"
		2052:
			return "HOSE"
		2054:
			return "STOP VALVE"
		2061:
			return "HEADER DUMP"
		2072:
			return "PROP PIN"
		2073, 2086, 2095:
			return "LANYARD"
		2074, 2096:
			return "ROPE SHACKLE"
		2084:
			return "TAIL PIN"
		2093:
			return "DOMINO PIN"
		2102:
			return "RAIL JOINT"
		2104, 2113, 2125:
			return "LANYARD"
		2112:
			return "PENDANT PIN"
		2114:
			return "ROPE SHACKLE"
		2124:
			return "DROP PIN"
		2127:
			return "CLUTCH HANDLE"
		2204:
			return "TRIP HANDLE"
		2205:
			return "FILL CHAIN"
	return "ROPE END" if _is_kit(entity) else ""


func _is_kit(entity: int) -> bool:
	return entity >= KIT_ENTITY_MIN and entity < KIT_ENTITY_MAX


func _kit_anchor_name(entity: int) -> String:
	match entity:
		1000:
			return "BOLLARD"
		1001:
			return "CLEAT"
		2000, 2010:
			return "CAGE EYE"
		2050:
			return "RAM INLET"
		2070:
			return "PLATFORM EYE"
		2090, 2110:
			return "CAGE EYE"
		1007:
			return "CLEAT"
	return "ANCHOR"


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
		if traversal == TRAVERSAL_HANGING or traversal == TRAVERSAL_CLIMBING:
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
		"hand_left": _native.get_traversal_left_hand(),
		"hand_right": _native.get_traversal_right_hand(),
		"structure_normal": _native.get_traversal_normal(),
		"sprinting": bool(_ctx["sprinting"]),
		"balancing": bool(_ctx["balancing"]),
		"affordance": bool(_native.is_ledge_available()),
		"affordance_point": _native.get_ledge_point(),
		"position": _ctx["position"],
		"velocity": _ctx["velocity"],
		"grounded": bool(_ctx["grounded"]),
		"chute": bool(_ctx["chute"]),
		"carrying": int(_ctx["carrying"]),
		"carry_center": _carry_center(int(_ctx["carrying"])),
		"carry_half": _carry_half(int(_ctx["carrying"])),
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
			_traversal_value, _tick_value, _fall_value]
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
func _apply_camera_feel(position: Vector3, velocity: Vector3, grounded: bool, crouched: bool,
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

	var eye := position + EYE_OFFSET
	_crouch_eye = move_toward(_crouch_eye, 1.0 if crouched else 0.0, delta / CROUCH_EYE_SECONDS)
	if crouched or _crouch_eye > 0.0:
		var soles_y := position.y - (CROUCH_HALF_HEIGHT if crouched else STAND_HALF_HEIGHT)
		eye.y = soles_y + lerpf(STAND_HALF_HEIGHT + EYE_OFFSET.y, CROUCH_EYE_OVER_SOLES,
			smoothstep(0.0, 1.0, _crouch_eye))
	_camera.position = eye + Vector3(0.0, dip + vertical_bob, 0.0) + right_vector * lateral_bob
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
	var crouched := bool(_native.is_player_crouched())

	_apply_camera_feel(position, velocity, grounded, crouched, delta)
	if delta > 0.0:
		_audio.update(delta, position, velocity, grounded, int(_native.get_support_entity_id()),
			int(_native.get_traversal_state()), bool(_native.is_parachute_deployed()),
			int(_native.get_death_count()), crouched)
	# The developer telemetry overlay costs a dozen string formats a frame;
	# it is only paid for while the overlay is actually on screen.
	if _telemetry_on:
		_write_telemetry(position, velocity, grounded)
	_render_kit()


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

	if traversal == TRAVERSAL_HANGING:
		_status.text = "HANGING ON NATIVE LEDGE"
	elif traversal == TRAVERSAL_MANTLING:
		_status.text = "MANTLING REAL GEOMETRY"
	elif traversal == TRAVERSAL_VAULTING:
		_status.text = "VAULTING REAL GEOMETRY"
	elif traversal == TRAVERSAL_CLIMBING:
		_status.text = "CLIMBING"
	elif grounded and _is_kit(support) and support >= KIT_DYNAMIC_ENTITY_MIN:
		_status.text = "RIDING A MACHINE"
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
	var lichen := _material(Color("57642e"), 0.0, 0.96)

	# Grade and the tower's upper mass: sizes mirror the native Jolt bodies.
	_add_box("Grade", Vector3(480.0, 1.0, 480.0), Vector3(0.0, -0.5, -60.0), asphalt)
	# The neighbouring shaft stands on the ground: 1.6 km from grade up.
	_add_box("TowerMass", Vector3(92.0, 1600.0, 80.0), Vector3(-30.0, 800.0, -330.0), concrete)

	_build_stack(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded_yellow, timber)
	_build_stack_accents(verdigris, lichen)
	_build_tower_skin(mill_scale, oxidised, galvanised, faded_yellow, timber)
	_build_yard(concrete, faded_yellow, tar)
	_build_mountains_and_waterfall()
	_build_kellerworks_signage(timber, faded_yellow)
	_build_foliage()
	_build_lighting()



# The stack: the tower's lower section. Deck rings and columns mirror real
# native collision one-for-one; bracing, rails, pipework and lamps are
# dressing hung on that frame. There is no stair: a level is gained by a
# machine or a climb. The player is inside this structure, so it is built to
# be seen from within as well as from the yard.
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
				var plank_z := band_center + float(plank) * 1.35
				_add_box("DeckPlank", Vector3(STACK_HALF_EXTENT * 2.0 - 2.0, 0.08, 1.1),
					Vector3(cx, deck_y + 0.05, cz + sz * plank_z), timber)

	# Columns, and the diagonal bracing that makes a frame a frame.
	for level in range(0, STACK_LEVEL_COUNT):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y,
					cz + sz * STACK_HALF_EXTENT), rust_deep)
			_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y, cz), rust_deep)
			_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx, mid_y, cz + sx * STACK_HALF_EXTENT), rust_deep)


	# The outer faces' bracing: a diagrid of two-storey diagonals, 22 m up over
	# 13 m along the face, meeting at the corner and mid-span columns and at
	# the edge beams half way between. At 59 degrees a brace's top is too steep
	# to stand on (the native's support limit is 56), so no brace is a ramp
	# from one deck to the next.
	var tier_height := STACK_LEVEL_HEIGHT * 2.0
	var half_bay := STACK_HALF_EXTENT * 0.5
	for tier in range(0, int(STACK_LEVEL_COUNT / 2.0)):
		var base_y := float(tier) * tier_height
		for side in [1.0, -1.0]:
			var outer := [0.0, side * half_bay, side * STACK_HALF_EXTENT]
			var legs: Array = []
			if tier % 2 == 0:
				legs = [[outer[0], outer[1]], [outer[2], outer[1]]]
			else:
				legs = [[outer[1], outer[0]], [outer[1], outer[2]]]
			for leg in legs:
				var along0: float = leg[0]
				var along1: float = leg[1]
				var length := sqrt(pow(along1 - along0, 2.0) + pow(tier_height, 2.0))
				var pitch := atan2(tier_height, along1 - along0)
				var mid_along := 0.5 * (along0 + along1)
				var mid_y := base_y + tier_height * 0.5
				for face in [1.0, -1.0]:
					var brace := _add_box("StackBrace", Vector3(length, 0.45, 0.45),
						Vector3(cx + mid_along, mid_y, cz + face * STACK_HALF_EXTENT), oxidised)
					brace.rotation = Vector3(0.0, 0.0, pitch)
					var brace_z := _add_box("StackBrace", Vector3(0.45, 0.45, length),
						Vector3(cx + face * STACK_HALF_EXTENT, mid_y, cz + mid_along), oxidised)
					brace_z.rotation = Vector3(-pitch, 0.0, 0.0)

	_build_stack_dressing(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded)
	_build_stack_megastructure(mill_scale, oxidised, rust_deep, rust_bright, galvanised,
		faded, timber)


# Everything that makes the frame read as one vast building rather than a
# repeated scaffold: splayed footings, clad machine halls with lit windows,
# company signage, and walkways striking out into the air. No machine is
# dressing: a machine here is a native one that works.
func _build_stack_megastructure(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var banner_cloth := _material(Color("5e2220"), 0.0, 0.95)
	var sign_plate := _material(Color("46423b"), 0.2, 0.86)
	var window_lit := _material(Color("2a2118"), 0.1, 0.7, Color("ffb45c"), 2.8)

	_build_stack_footings(rust_deep, oxidised, mill_scale)
	_build_stack_halls(timber, mill_scale, rust_deep, window_lit)
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
	_add_sign_text("STACK 1", bay_centre + Vector3(0.0, 2.2, 0.25), 0.0, 2.1, Color("e8dcc6"))
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


# Colour that isn't rust: verdigris copper pipe runs and lichen staining low on the columns where damp and shade let something
# green actually take hold. A decades-old working plant is never one colour.
func _build_stack_accents(verdigris: Material, lichen: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT

	# A verdigris pipe run climbing the front face, distinct from the oxidised
	# risers in the shaft -- copper service lines age to blue-green, not rust.
	# West of the centre: the east half of the face carries S1 and C1.
	for sx in [-1.0]:
		_add_cylinder("VerdigrisPipe", 0.4, STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT),
			Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5),
				STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT) * 0.5, front_z + 0.9), verdigris)
		for level in range(1, STACK_LEVEL_COUNT + 1):
			_add_box("VerdigrisFlange", Vector3(1.0, 0.3, 1.0),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5), float(level) * STACK_LEVEL_HEIGHT - 1.2,
					front_z + 0.9), verdigris)

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

	# Vent stacks, and lamp fittings throwing warm light into the frame.
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


func _build_yard(concrete: Material, faded: Material, tar: Material) -> void:
	for z in range(-130, 10, 14):
		_add_box("LaneStripe", Vector3(0.22, 0.02, 7.0), Vector3(-2.0, 0.012, float(z)), faded)
	for z in [-118.0, -60.0, -16.0]:
		_add_box("KerbRun", Vector3(86.0, 0.28, 0.5), Vector3(-6.0, 0.14, z), concrete)
	_add_box("StandingWater", Vector3(26.0, 0.02, 18.0), Vector3(-24.0, 0.021, -70.0), tar)
	_add_box("StandingWaterTwo", Vector3(18.0, 0.02, 12.0), Vector3(14.0, 0.021, -36.0), tar)


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


# AS-006: the mechanism kit. Every kit body is declared by the native -- its
# boxes, each box's material class, its pose -- and drawn here from that
# declaration, so what the player sees is exactly what collides; nothing is
# authored twice. Kit bodies live outside $TowerPresentation: they are native
# collision already, never part of the exported solid dressing.
func _build_kit() -> void:
	_kit_root = Node3D.new()
	_kit_root.name = "KitPresentation"
	add_child(_kit_root)
	# Indexed by the native's Material enum: steel, rust, timber, concrete,
	# hazard, galvanised, rubble, yellow.
	var palette: Array[Material] = [
		_material(Color("2a2723"), 0.7, 0.55, Color.BLACK, 1.0, _bump_steel),
		_material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _bump_steel),
		_material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _bump_timber),
		_material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _bump_concrete),
		_material(Color("a04d16"), 0.18, 0.76),
		_material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _bump_steel),
		_material(Color("5b544b"), 0.0, 0.98, Color.BLACK, 1.0, _bump_concrete),
		_material(Color("c19a2a"), 0.2, 0.62),
	]
	for body in int(_native.get_kit_body_count()):
		var node := Node3D.new()
		node.name = "KitBody%d" % int(_native.get_kit_body_entity_id(body))
		node.transform = _native.get_kit_body_transform(body)
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body)
		for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
			var mesh := BoxMesh.new()
			mesh.size = Vector3(parts[p], parts[p + 1], parts[p + 2]) * 2.0
			mesh.material = palette[clampi(int(parts[p + 10]), 0, palette.size() - 1)]
			var instance := MeshInstance3D.new()
			instance.mesh = mesh
			instance.transform = Transform3D(
				Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
				Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			node.add_child(instance)
		_kit_root.add_child(node)
		_kit_bodies.append(node)
		_kit_dynamic.append(bool(_native.is_kit_body_dynamic(body)))
	var cable := BoxMesh.new()
	cable.size = Vector3(0.035, 0.035, 1.0)
	cable.material = _material(Color("1b1916"), 0.6, 0.5)
	for index in int(_native.get_kit_cable_count()):
		var segments: Array[MeshInstance3D] = []
		for segment in KIT_CABLE_SEGMENTS:
			var instance := MeshInstance3D.new()
			instance.name = "KitCable%d_%d" % [index, segment]
			instance.mesh = cable
			instance.visible = false
			_kit_root.add_child(instance)
			segments.append(instance)
		_kit_cables.append(segments)
	var stream_mesh := BoxMesh.new()
	stream_mesh.size = Vector3(0.3, 0.3, 1.0)
	stream_mesh.material = palette[6]
	var water := StandardMaterial3D.new()
	water.albedo_color = Color(0.16, 0.30, 0.33, 0.74)
	water.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	water.metallic = 0.1
	water.roughness = 0.08
	water.cull_mode = BaseMaterial3D.CULL_DISABLED
	var water_stream := BoxMesh.new()
	water_stream.size = Vector3(0.22, 0.22, 1.0)
	water_stream.material = water
	var bins: PackedFloat32Array = _native.get_kit_bins()
	for b in range(0, bins.size() - KIT_BIN_FLOATS + 1, KIT_BIN_FLOATS):
		var index := _kit_bin_streams.size()
		var body := int(bins[b])
		var is_water := bins[b + 10] > 0.5
		var layer: MeshInstance3D = null
		var floor_box := PackedFloat32Array()
		var moving := body >= 0 and body < _kit_dynamic.size() and _kit_dynamic[body]
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body) if moving \
			else PackedFloat32Array()
		if parts.size() >= KIT_PART_FLOATS:
			# The rubble lies on the bin's floor, its first part.
			floor_box = parts.slice(0, KIT_PART_FLOATS)
			var layer_mesh := BoxMesh.new()
			layer_mesh.material = water if is_water else palette[6]
			layer = MeshInstance3D.new()
			layer.name = "KitRubble%d" % index
			layer.mesh = layer_mesh
			layer.visible = false
			_kit_bodies[body].add_child(layer)
		_kit_bin_layers.append(layer)
		_kit_bin_floors.append(floor_box)
		var stream := MeshInstance3D.new()
		stream.name = "KitStream%d" % index
		stream.mesh = water_stream if is_water else stream_mesh
		stream.visible = false
		_kit_root.add_child(stream)
		_kit_bin_streams.append(stream)
	var pile_mesh := CylinderMesh.new()
	pile_mesh.top_radius = 0.6
	pile_mesh.bottom_radius = 1.0
	pile_mesh.height = 1.0
	pile_mesh.radial_segments = 20
	pile_mesh.rings = 1
	pile_mesh.material = palette[6]
	for index in KIT_MAX_PILES:
		var pile := MeshInstance3D.new()
		pile.name = "KitPile%d" % index
		pile.mesh = pile_mesh
		pile.visible = false
		_kit_root.add_child(pile)
		_kit_piles.append(pile)
	var pools: PackedFloat32Array = _native.get_kit_pools()
	for p in range(0, pools.size() - KIT_POOL_FLOATS + 1, KIT_POOL_FLOATS):
		var body_mesh := BoxMesh.new()
		body_mesh.size = Vector3.ONE
		body_mesh.material = water
		var pool := MeshInstance3D.new()
		pool.name = "KitPool%d" % _kit_pools.size()
		pool.mesh = body_mesh
		pool.visible = false
		_kit_root.add_child(pool)
		_kit_pools.append(pool)
	var spouts: PackedFloat32Array = _native.get_kit_spouts()
	for p in range(0, spouts.size() - KIT_SPOUT_FLOATS + 1, KIT_SPOUT_FLOATS):
		var spout := MeshInstance3D.new()
		spout.name = "KitSpout%d" % _kit_spouts.size()
		spout.mesh = water_stream
		spout.visible = false
		_kit_root.add_child(spout)
		_kit_spouts.append(spout)
	_render_kit()


# Poses every moving kit body and lays every cable through its points, from
# this frame's native state. A body the native has taken out of the world (a
# shackle hooked onto an anchor) is hidden, not moved.
func _render_kit() -> void:
	if _kit_root == null:
		return
	for body in _kit_bodies.size():
		if not _kit_dynamic[body]:
			continue
		var node: Node3D = _kit_bodies[body]
		node.visible = bool(_native.is_kit_body_enabled(body))
		if node.visible:
			node.transform = _native.get_kit_body_transform(body)
	for index in _kit_cables.size():
		var points: PackedVector3Array = _native.get_kit_cable_points(index)
		var segments: Array = _kit_cables[index]
		for segment in segments.size():
			var instance: MeshInstance3D = segments[segment]
			if segment + 1 < points.size():
				_lay_segment(instance, points[segment], points[segment + 1])
			else:
				instance.visible = false
	_render_rubble()


# Rubble from the native's bins and piles: a layer on each moving bin's floor
# as deep as its contents would lie, a stream while a bin pours, and a low
# skirt of spilled rubble where each pile landed.
func _render_rubble() -> void:
	var bins: PackedFloat32Array = _native.get_kit_bins()
	for index in _kit_bin_streams.size():
		var b := index * KIT_BIN_FLOATS
		if b + KIT_BIN_FLOATS > bins.size():
			break
		var layer: MeshInstance3D = _kit_bin_layers[index]
		if layer != null:
			var floor_box: PackedFloat32Array = _kit_bin_floors[index]
			var half_x := maxf(floor_box[0] - 0.06, 0.05)
			var half_z := maxf(floor_box[2] - 0.06, 0.05)
			var density := 1000.0 if bins[b + 10] > 0.5 else RUBBLE_DENSITY
			var depth := bins[b + 1] / density / (4.0 * half_x * half_z)
			layer.visible = bins[b + 1] >= 1.0
			if layer.visible:
				var rotation := Basis(Quaternion(floor_box[6], floor_box[7], floor_box[8],
					floor_box[9]))
				var on_floor := Vector3(floor_box[3], floor_box[4], floor_box[5]) \
					+ rotation * Vector3(0.0, floor_box[1] + depth * 0.5, 0.0)
				layer.transform = Transform3D(
					rotation * Basis.from_scale(Vector3(half_x * 2.0, depth, half_z * 2.0)),
					on_floor)
		var stream: MeshInstance3D = _kit_bin_streams[index]
		if bins[b + 3] > 0.5:
			_lay_segment(stream, Vector3(bins[b + 4], bins[b + 5], bins[b + 6]),
				Vector3(bins[b + 7], bins[b + 8], bins[b + 9]))
		else:
			stream.visible = false
	var piles: PackedFloat32Array = _native.get_kit_piles()
	for index in _kit_piles.size():
		var pile: MeshInstance3D = _kit_piles[index]
		var p := index * KIT_PILE_FLOATS
		pile.visible = p + KIT_PILE_FLOATS <= piles.size() and piles[p + 3] >= 1.0
		if pile.visible:
			# A frustum of this height and base radius r holds about
			# 0.65 * PI * r^2 * height.
			var volume := piles[p + 3] / RUBBLE_DENSITY
			var radius := sqrt(volume / (0.65 * PI * RUBBLE_PILE_HEIGHT))
			pile.transform = Transform3D(
				Basis.from_scale(Vector3(radius, RUBBLE_PILE_HEIGHT, radius)),
				Vector3(piles[p], piles[p + 1] + RUBBLE_PILE_HEIGHT * 0.5, piles[p + 2]))
	_render_water()


# Water (the native's declared model): each pool drawn from its floor to its
# level, and a stream from each spout pouring this frame to where it lands.
func _render_water() -> void:
	var pools: PackedFloat32Array = _native.get_kit_pools()
	for index in _kit_pools.size():
		var p := index * KIT_POOL_FLOATS
		var pool: MeshInstance3D = _kit_pools[index]
		if p + KIT_POOL_FLOATS > pools.size():
			pool.visible = false
			continue
		var low := Vector3(pools[p], pools[p + 1], pools[p + 2])
		var high := Vector3(pools[p + 3], pools[p + 6], pools[p + 5])
		pool.visible = high.y - low.y > 0.02
		if pool.visible:
			pool.transform = Transform3D(Basis.from_scale(high - low), (low + high) * 0.5)
	var spouts: PackedFloat32Array = _native.get_kit_spouts()
	for index in _kit_spouts.size():
		var p := index * KIT_SPOUT_FLOATS
		var spout: MeshInstance3D = _kit_spouts[index]
		if p + KIT_SPOUT_FLOATS <= spouts.size() and spouts[p] > 0.5:
			_lay_segment(spout, Vector3(spouts[p + 1], spouts[p + 2], spouts[p + 3]),
				Vector3(spouts[p + 4], spouts[p + 5], spouts[p + 6]))
		else:
			spout.visible = false


func _lay_segment(instance: MeshInstance3D, from: Vector3, to: Vector3) -> void:
	var delta := to - from
	var length := delta.length()
	if length < 0.001:
		instance.visible = false
		return
	var along := delta / length
	var up := Vector3.UP if absf(along.y) < 0.99 else Vector3.RIGHT
	var side := up.cross(along).normalized()
	instance.transform = Transform3D(Basis(side, along.cross(side), along * length),
		from + delta * 0.5)
	instance.visible = true


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
	print("SCRAPERX_CI_PHASE %s tick=%d position=(%.2f,%.2f,%.2f)" % [
		label, _native.get_tick_index(), position.x, position.y, position.z])


func _print_runtime_proof() -> void:
	_ci_proof_printed = true
	var position: Vector3 = _native.get_player_position()
	print("SCRAPERX_WO004_VIEWPORT_PROOF width=%d height=%d aspect=%.3f fov=%.1f far=%.0f stretch=expand" % [
		int(_viewport_size.x), int(_viewport_size.y),
		_viewport_size.x / maxf(1.0, _viewport_size.y), _camera.fov, _camera.far])
	print("SCRAPERX_WO005_APPROACH_PROOF spawn_grade=1 position=(%.2f,%.2f,%.2f) tower_face_distance=%.1f tower_height=%.0f" % [
		position.x, position.y, position.z, absf(-145.0 - position.z),
		float(_native.get_tower_height_meters())])


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
