extends "res://presentation/main_feel.gd"

# Presentation and input only. Every consequential fact below is read from the
# native ScraperX simulation; nothing here decides pose, support, traversal, or
# machine state. Where this file draws something that looks simulated, it is
# driven by an authoritative native value, so freezing that value freezes the
# effect.

# (Layers below: main_geometry, main_world, main_machines, main_context,
# main_feel. This file: start-up, the frame, input, the interface, saves.)

const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const Hud := preload("res://presentation/ui/hud.gd")
const PauseMenu := preload("res://presentation/ui/pause_menu.gd")
const TitleScreen := preload("res://presentation/ui/title_screen.gd")
# Loaded only for --uitest runs, so shipping builds never parse test code.
const UI_TEST_DRIVER_PATH := "res://presentation/ui/ui_test_driver.gd"
const Birds := preload("res://presentation/life/birds.gd")
const CrewView := preload("res://presentation/life/crew_view.gd")
const CanopyView := preload("res://presentation/canopy_view.gd")
const Works := preload("res://presentation/life/works.gd")
const CloudLayer := preload("res://presentation/cloud_layer.gd")
const SkyCycleScript := preload("res://presentation/sky_cycle.gd")
const AudioDirector := preload("res://presentation/audio/audio_director.gd")
const SlingshotView := preload("res://presentation/slingshot_view.gd")
const ThrowArcView := preload("res://presentation/throw_arc_view.gd")
const SwingView := preload("res://presentation/swing_view.gd")
const SummitView := preload("res://presentation/summit_view.gd")
const MatronView := preload("res://presentation/life/matron_view.gd")
const StockView := preload("res://presentation/stock_view.gd")
const ForkliftView := preload("res://presentation/forklift_view.gd")
const CraneView := preload("res://presentation/crane_view.gd")
const ZiplineView := preload("res://presentation/zipline_view.gd")
# A hitch -- or the first frame after Android resumes a backgrounded app --
# must not be paid for with a burst of native ticks in one frame. The excess
# of that one frame simply runs slower than wall time, which nobody can see.
const MAX_SIM_FRAME_DELTA := 0.1
# Aiming the slingshot, the view rides this far below the launch line, so the
# stretched bands, the fork and the whole predicted arc are in front of the
# rider rather than straight overhead.
const SLING_VIEW_BELOW_AIM := 0.45
# A Jump (or a swipe up) made while a mantle or vault is still under way,
# which the native takes no request in, is sent when the body is free again
# if that is within this long. Input timing assistance only (GDD 7.2).
const JUMP_BUFFER_SECONDS := 0.12
# The quickturn (Mirror's Edge's): half a turn of the view, to the right, over
# this long -- quick, but a turn, not a cut.
const QUICKTURN_SECONDS := 0.22

const PHASE_APPROACH := 0
const PHASE_PROVEN := 2

# The tower face sits at z = -124. Walking to z = -66 puts its lower third
# across the whole frame.
const CI_APPROACH_TARGET_Z := -66.0
# From the grade spawn (6, -25) toward the tower, east of the ground
# slingshot's timber block (x <= 1.35 between z -42.6 and -61.1): at z -61
# the walk is at x 3.1.
const CI_APPROACH_FACING := Vector2(-0.08, -0.9968)
const CI_HOLD_TICKS := 20
const SAVE_MIN_INTERVAL_MS := 2000
var _save_enabled := false
var _save_path := SAVE_PATH
var _saved_commits := -1
var _saved_deaths := -1
var _saved_at_ms := 0
var _continued_from_save := false
# The title shows once per launch: a NEW CLIMB reloads the scene without it.
static var _title_seen := false
var _title: Control
var _title_open := false
var _capture_path := ""
var _capture_scheduled := false
var _fps_label: Label
var _fps_clock := 0.0
var _touch: Control
var _throw_arc: Node3D
var _throw_path := PackedVector3Array()
var _pause_menu: Control
var _uitest: Node
var _force_touch := false
var _uitest_scenario := ""
var _paused := false
var _jump_buffer := 0.0
var _up_buffer := 0.0
var _canopy: Node3D
var _sling_was_aiming := false
# The forklift's heading last frame, as a view yaw: driving it, the view turns
# with it.
var _fork_heading := 0.0
# The quickturn's turn still to make (radians).
var _quickturn_left := 0.0
var _sling_goal_yaw := 0.0
var _sling_goal_elevation := 1.4311699866353502
var _clouds: Node3D
var _ci_phase := PHASE_APPROACH
var _ci_facing := CI_APPROACH_FACING
var _ci_proof_tick := -1
var _ci_proof_printed := false
var _sky_cycle: Node

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
	_batch_static_dressing($TowerPresentation)
	for lamp in _light_rig.get_children():
		if lamp is OmniLight3D:
			(lamp as OmniLight3D).light_cull_mask = ~BATCH_FAR_LAYER
			(lamp as OmniLight3D).distance_fade_enabled = true
			(lamp as OmniLight3D).distance_fade_begin = LAMP_REACH
			(lamp as OmniLight3D).distance_fade_length = BATCH_FADE
	_sky_cycle = SkyCycleScript.new()
	_sky_cycle.name = "SkyCycle"
	add_child(_sky_cycle)
	_sky_cycle.setup($Overcast, $SkyFill, ($Environment as WorldEnvironment).environment)
	_audio = AudioDirector.new()
	_audio.name = "AudioDirector"
	_audio.sky = _sky_cycle
	add_child(_audio)
	_build_arms()
	_build_hook_view()
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
	_birds = Birds.new()
	add_child(_birds)
	_birds.setup(_native, _audio)
	_crew = CrewView.new()
	add_child(_crew)
	_crew.setup(_native, _audio)
	_radio = preload("res://presentation/life/foreman_radio.gd").new()
	_radio.name = "ForemanRadio"
	add_child(_radio)
	_radio.setup(_audio)

	# A saved climb continues; otherwise the playtest start point (pause
	# menu), before the native clock moves.
	_save_enabled = _uitest_scenario.is_empty() and not _ci_mode
	var continued := _save_enabled and load_saved_climb()
	_continued_from_save = continued
	# The START list is a development tool: release builds always start at grade.
	if not continued and _uitest_scenario.is_empty() and _settings.start_at > 0 and OS.is_debug_build():
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
	var stock_view := StockView.new()
	add_child(stock_view)
	stock_view.setup(self, _native, _kit_root)
	for index in int(_native.get_forklift_count()):
		var view := ForkliftView.new()
		add_child(view)
		view.setup(self, _native, index)
		_forklift_views.append(view)
	_forklift_view = _forklift_views[0]
	_crane_view = CraneView.new()
	add_child(_crane_view)
	_crane_view.setup(self, _native)
	_works = Works.new()
	add_child(_works)
	_clouds = CloudLayer.new()
	add_child(_clouds)
	_works.setup(_vent_tops)
	for m in _machines.size():
		if float(_machines[m]["mass"]) >= Works.BEACON_MIN_KG:
			_works.add_beacon(_kit_bodies[int(_machines[m]["body"])], _machines[m]["top"], m)
	_throw_arc = ThrowArcView.new()
	add_child(_throw_arc)
	_throw_arc.setup()
	_slingshot_view = SlingshotView.new()
	add_child(_slingshot_view)
	_slingshot_view.setup(self)
	# The view draws the pouch as leather; the native's panels stay unseen.
	var pouch_node := _kit_root.get_node_or_null("KitBody%d" % SlingshotView.POUCH_ENTITY)
	if pouch_node != null:
		pouch_node.visible = false
	_swing_view = SwingView.new()
	add_child(_swing_view)
	_swing_view.setup()
	_zipline_view = ZiplineView.new()
	add_child(_zipline_view)
	_zipline_view.setup(_native)
	_corpse_view = CorpseView.new()
	add_child(_corpse_view)
	_corpse_view.setup(_native)
	# The summit's lamp: lit before (the profile keeps it) opens the world lit.
	if _save_enabled and FileAccess.file_exists(SUMMIT_PROFILE_PATH):
		_native.light_summit()
	_summit_view = SummitView.new()
	add_child(_summit_view)
	_summit_view.setup(_kit_root)
	_summit_view.ignited.connect(_on_summit_lit)
	_matron_view = MatronView.new()
	add_child(_matron_view)
	_matron_view.setup(_native, _audio)
	# Both views are a few metres of rope and leather: not drawn from afar.
	for view: Node in [_slingshot_view, _swing_view]:
		var pending: Array[Node] = [view]
		while not pending.is_empty():
			var node: Node = pending.pop_back()
			pending.append_array(node.get_children())
			if node is GeometryInstance3D:
				(node as GeometryInstance3D).visibility_range_end = VIEW_PROP_RANGE

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
	if not _title_seen and ((_uitest_scenario.is_empty() and not _ci_mode) or _uitest_scenario == "touch_title"):
		_show_title()

func _process(delta: float) -> void:
	if _native == null:
		return
	if _fps_label.visible:
		_fps_clock -= delta
		if _fps_clock <= 0.0:
			_fps_clock = 0.5
			_fps_label.text = "%d FPS" % int(Engine.get_frames_per_second())

	var intent: Dictionary = _router.frame(delta)
	# In the slingshot's pouch, unreleased: the look aims the shot and the
	# view follows the launch rail as it turns; the stick pulled back draws.
	var sling: Dictionary = _native.get_slingshot_state()
	var sling_seated := bool(sling["seated"])
	var aiming := sling_seated and not bool(sling["released"])
	var sling_recovering := bool(sling["recovering"])
	if aiming and not _sling_was_aiming:
		_sling_goal_yaw = float(sling["yaw_rad"])
		_sling_goal_elevation = float(sling["elevation_rad"])
	_sling_was_aiming = aiming
	if not _ci_mode:
		var look: Vector2 = intent["look"]
		if aiming:
			if not bool(sling["aim_locked"]):
				_sling_goal_yaw = clampf(_sling_goal_yaw + look.x, -0.8, 0.8)
				_sling_goal_elevation = clampf(_sling_goal_elevation - look.y, 0.35, 1.48)
			_yaw = -float(sling["yaw_rad"])
			_pitch = float(sling["elevation_rad"]) - SLING_VIEW_BELOW_AIM
		else:
			_yaw -= look.x
			_pitch = clampf(_pitch - look.y, -1.25, 1.35)
	# The quickturn, made over QUICKTURN_SECONDS. The stick is the view's, so
	# a run held on turns about with it: the feet brake it and run it back.
	if _quickturn_left > 0.0:
		var turn := minf(_quickturn_left, PI / QUICKTURN_SECONDS * delta)
		_yaw -= turn
		_quickturn_left -= turn
	# Driving the forklift, the driver turns with the truck as a seat does:
	# the view keeps its angle to the truck's nose while the truck turns, so
	# the stick's forward stays the way the driver is looking.
	var truck_heading := float(_forklift_view.heading()) if _forklift_view != null else 0.0
	if bool(_ctx.get("forklift_driving", false)):
		_yaw += wrapf(truck_heading - _fork_heading, -PI, PI)
	_fork_heading = truck_heading
	_update_view_pitch_offset(delta)

	var position: Vector3 = _native.get_player_position()
	var desired: Vector2 = intent["move"]
	var draw_effort := maxf(0.0, -desired.y) if sling_seated or sling_recovering else 0.0
	_native.set_slingshot_input(draw_effort,
		_sling_goal_yaw if aiming else float(sling["yaw_rad"]),
		_sling_goal_elevation if aiming else float(sling["elevation_rad"]))
	if sling_seated or sling_recovering:
		desired = Vector2.ZERO
	var facing := Vector2(-sin(_yaw), -cos(_yaw))

	if _ci_mode:
		desired = _ci_movement_intent(position)
		facing = _ci_facing
		_yaw = atan2(-facing.x, -facing.y)

	# While the view rides a death (on the corpse), the restored body waits.
	if _death_cam_left > 0.0:
		desired = Vector2.ZERO
	var forward := Vector2(-sin(_yaw), -cos(_yaw))
	var right := Vector2(cos(_yaw), -sin(_yaw))
	var world_move := right * desired.x + forward * desired.y
	if not _native.set_move_input(world_move.x, world_move.y):
		_fail_native("SCRAPERX_MOVE_INPUT_REJECTED", 21)
		return
	_native.set_facing(facing.x, facing.y)
	_native.set_look_pitch(_pitch)
	_dispatch(intent["verbs"], delta)
	_native.set_crouch_input(_crouch_toggled or bool(intent["crouch_held"]) or
		(bool(intent["down_held"]) and _down_hold_counts))
	# A hand line (the pitman lift's) is hauled while ACTION is held.
	_hauling = bool(intent.get("action_held", false)) and _ctx.get("action", {}).get("id", &"") == &"haul"
	_native.set_lift_haul(_hauling)
	# The forklift's forks, while driving: Jump held raises, Crouch held lowers.
	var forks := 0.0
	if bool(_ctx.get("forklift_driving", false)):
		forks = (1.0 if bool(intent.get("raise_held", false)) else 0.0) - \
			(1.0 if bool(intent.get("lower_held", false)) else 0.0)
	_native.set_forklift_lift(forks)
	# The yard crane's hoist, while it is worked: Jump held raises the hook,
	# Crouch held lowers it.
	var hoist := 0.0
	if bool(_ctx.get("crane_operating", false)):
		hoist = (1.0 if bool(intent.get("raise_held", false)) else 0.0) - \
			(1.0 if bool(intent.get("lower_held", false)) else 0.0)
	_native.set_yard_crane_hoist(hoist)
	# The hook: aimed with the view; its reel turns while the button is held.
	if bool(_ctx.get("has_hook", false)):
		_native.set_hook_aim(-_camera.global_transform.basis.z)
		_native.set_hook_reel(bool(intent.get("hook_held", false)) and int(_ctx.get("hook_state", 0)) == 2)
	_native.set_sprint_input(bool(intent["sprint_held"]))

	_launch_shot.update(delta, _native.get_slingshot_state())
	var time_scale := _launch_shot.time_scale()
	# The launch shot's slow time: fewer native ticks this frame, each the
	# same 90 Hz step. Heard too: the mix slows with it.
	AudioServer.playback_speed_scale = time_scale
	var steps_advanced := 0 if freeze_sim else int(_native.advance_frame(minf(delta * time_scale, MAX_SIM_FRAME_DELTA)))
	if steps_advanced < 0:
		_fail_native("SCRAPERX_FRAME_DELTA_REJECTED", 21)
		return

	_render_snapshot(delta)
	_ctx = _read_context()
	_save_climb()
	_arms.visible = _death_cam_left <= 0.0
	_arms.update_arms(_arms_state(intent), _camera.global_transform, delta)
	# The body under them: hidden while the view rides a death.
	_body.visible = _death_cam_left <= 0.0
	_body.update_body({
		"position": _native.get_player_render_position(),
		"velocity": _native.get_player_linear_velocity(),
		"grounded": bool(_native.is_player_grounded()),
		"crouched": bool(_native.is_player_crouched()),
		"sliding": bool(_native.is_player_sliding()),
		"traversal": int(_ctx.get("traversal", 0)),
		"on_line": bool(_native.get_zipline_state()["riding"]),
	}, _yaw, delta)
	if _canopy == null:
		_canopy = CanopyView.new()
		add_child(_canopy)
	_canopy.update(bool(_ctx.get("chute", false)), _camera.global_position, _yaw, _arms.riser_tops, delta)
	_update_feedback(delta)
	_update_gesture_hints()
	_update_throw_arc(delta)
	_touch.update_context(_ctx, delta)
	_hud.family = _router.glyph_family()
	_hud.caption = _audio.caption if _settings.captions else ""
	# The foreman's radio, from the native's state; his lines show whatever
	# the captions setting, having no voice yet.
	var climber: Vector3 = _native.get_player_position()
	_radio.update(delta, {
		"y": climber.y,
		"support": int(_native.get_support_entity_id()),
		"matron_released": bool(_native.get_matron_state()["released"]),
	})
	_hud.radio_text = _radio.text
	_hud.radio_who = _radio.who
	# The faint route hint, asked of the native only while it is on.
	_hud.route_points = _native.get_route_ahead() if _settings.route_hint else PackedVector3Array()
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
	# A preview on a machine with no GPU (the cloud dev box: its Godot tool
	# sets SCRAPERX_PREVIEW=1, and Mesa's CPU renderer is llvmpipe): the
	# lowest render, no shadows, and nothing saved, so a frame is drawn in
	# well under a second. Scripted runs keep their own settings.
	var software_gl := RenderingServer.get_video_adapter_name().containsn("llvmpipe")
	if OS.get_environment("SCRAPERX_PREVIEW") == "1" or (software_gl and _settings.persistent):
		_settings.persistent = false
		_settings.apply_quality(0)
		_settings.render_scale = SettingsStore.RENDER_SCALE_RANGE.x
		_settings.shadow_quality = 0
		_settings.quality = SettingsStore.QUALITY_CUSTOM
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
	_pause_menu.quit_requested.connect(func() -> void:
		_save_climb(true)
		get_tree().quit(0))
	_pause_menu.restart_requested.connect(func() -> void:
		discard_saved_climb()
		get_tree().paused = false
		get_tree().reload_current_scene())
	_pause_menu.settings_changed.connect(_apply_settings)
	_title = TitleScreen.new()
	_title.name = "TitleScreen"
	_title.visible = false
	pause_layer.add_child(_title)
	_title.continue_requested.connect(_close_title)
	_title.new_climb_requested.connect(func() -> void:
		if _continued_from_save:
			discard_saved_climb()
			get_tree().paused = false
			get_tree().reload_current_scene()
		else:
			_close_title())
	_title.quit_requested.connect(func() -> void: get_tree().quit(0))
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
	_touch.gestures = _settings.touch_scheme == SettingsStore.TOUCH_SCHEME_GESTURES
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
		_settings.ambience_volume, _settings.interface_volume, _settings.voice_volume)
	# DISPLAY
	_fov_base = _settings.fov
	_reduced_motion = _settings.reduced_motion
	_head_bob_on = _settings.head_bob and not _reduced_motion
	_speed_fov_on = _settings.speed_fov and not _reduced_motion
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
			_save_climb(true)
			_open_pause(true)
		NOTIFICATION_WM_CLOSE_REQUEST:
			_save_climb(true)
		NOTIFICATION_WM_GO_BACK_REQUEST:
			if _title_open:
				get_tree().quit(0)
			elif _paused:
				_pause_menu.back()
			else:
				_open_pause(true)

# Writes the committed checkpoint when it has changed since the last write:
# at most every SAVE_MIN_INTERVAL_MS while playing, at once when forced (the
# app leaving the foreground, quit). Written whole to a temporary file and
# renamed over the save, so a kill mid-write leaves the previous save intact.
func _save_climb(force := false) -> void:
	if not _save_enabled or _native == null:
		return
	var commits := int(_native.get_checkpoint_commit_count())
	var deaths := int(_native.get_death_count())
	if commits == _saved_commits and deaths == _saved_deaths:
		return
	var now := Time.get_ticks_msec()
	if not force and _saved_commits >= 0 and now - _saved_at_ms < SAVE_MIN_INTERVAL_MS:
		return
	if write_saved_climb():
		_saved_commits = commits
		_saved_deaths = deaths
		_saved_at_ms = now

func write_saved_climb() -> bool:
	var bytes: PackedByteArray = _native.get_save_game()
	var temporary := _save_path + ".tmp"
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		push_warning("SCRAPERX_SAVE_OPEN_FAILED path=%s error=%d" % [temporary, FileAccess.get_open_error()])
		return false
	file.store_buffer(bytes)
	var error := file.get_error()
	file.close()
	if error != OK:
		push_warning("SCRAPERX_SAVE_WRITE_FAILED path=%s error=%d" % [temporary, error])
		return false
	error = DirAccess.rename_absolute(temporary, _save_path)
	if error != OK:
		push_warning("SCRAPERX_SAVE_RENAME_FAILED path=%s error=%d" % [_save_path, error])
		return false
	return true

# Loads the save into the native world. A save this build can't read (cut
# short, or from a build whose layout differs) is set aside as .bad, not
# deleted, and the climb starts fresh.
func load_saved_climb() -> bool:
	if not FileAccess.file_exists(_save_path):
		return false
	var bytes := FileAccess.get_file_as_bytes(_save_path)
	if not bytes.is_empty() and bool(_native.load_save_game(bytes)):
		_saved_commits = int(_native.get_checkpoint_commit_count())
		_saved_deaths = int(_native.get_death_count())
		_saved_at_ms = Time.get_ticks_msec()
		var checkpoint: Vector3 = _native.get_checkpoint_position()
		print("SCRAPERX_CONTINUE path=%s bytes=%d checkpoint_y=%.2f tick=%d" % [
			_save_path, bytes.size(), checkpoint.y, int(_native.get_tick_index())])
		_hud.toast("CONTINUE", "%+.1f M" % checkpoint.y, UiStyle.SAFE)
		return true
	push_warning("SCRAPERX_SAVE_REJECTED path=%s bytes=%d" % [_save_path, bytes.size()])
	DirAccess.rename_absolute(_save_path, _save_path + ".bad")
	_hud.toast("SAVE UNREADABLE", "STARTING A NEW CLIMB", UiStyle.HAZARD)
	return false

func discard_saved_climb() -> void:
	_save_enabled = false
	if FileAccess.file_exists(_save_path):
		DirAccess.remove_absolute(_save_path)

# Pausing stops this node's _process, so no advance_frame call is made: the
# simulation is frozen, not slowed. System-initiated pauses (focus loss, app
# backgrounded, pad unplugged) never fire in proof runs.
func _open_pause(from_system: bool = false) -> void:
	if _paused or _title_open or _ci_mode or _native == null:
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

# The title over the live tower, the simulation frozen behind it.
func _show_title() -> void:
	_title_seen = true
	_title_open = true
	_router.gameplay_active = false
	_router.clear_held()
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_title.saved_altitude = (_native.get_checkpoint_position() as Vector3).y if _continued_from_save else NAN
	_title.camera = _camera
	_title.base_yaw = _yaw
	_title.open()
	get_tree().paused = true

func _close_title() -> void:
	if not _title_open:
		return
	_title.close()
	_title_open = false
	_audio.ui_tap(true)
	get_tree().paused = false
	_router.clear_held()
	_router.gameplay_active = true

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
		# At the forklift's wheel Jump and Crouch are its forks' buttons, held
		# (_process): pressed, they neither jump nor crouch.
		if (bool(_ctx.get("forklift_driving", false)) or bool(_ctx.get("crane_operating", false))) and \
				verb in [&"jump", &"crouch"]:
			continue
		match verb:
			&"jump":
				# A jump stands a crouched body first (natively), and ends the toggle.
				# Sent whatever the body is doing: the native jumps it, kicks it off
				# the wall it runs, vaults the takeoff, or holds it for the landing.
				_crouch_toggled = false
				_down_until = DOWN_UNTIL_UP
				_native.request_jump()
				if _committed():
					_jump_buffer = JUMP_BUFFER_SECONDS
			&"up":
				# Sliding down a climb's holds, up takes hold again; otherwise it is
				# the native's move from where the body is.
				var holding_on: bool = _ctx["climbing"] and _crouch_toggled
				_crouch_toggled = false
				_down_until = DOWN_UNTIL_UP
				_down_hold_counts = false
				if not holding_on and not _native.request_up():
					_up_buffer = JUMP_BUFFER_SECONDS
			&"down":
				_down()
			&"action":
				_crouch_toggled = false
				_perform_action()
			&"turn":
				if _quickturn_left <= 0.0:
					_quickturn_left = PI
			&"crouch":
				# Does what the button says: crouched, it asks to stand. On the
				# trapeze it pulls up on the bar, or lets down. On a climb's
				# holds it slides down them, or holds on.
				if bool(_ctx["trapeze_hanging"]):
					_crouch_toggled = not bool(_ctx["trapeze_pulled"])
				elif bool(_ctx["climbing"]):
					_crouch_toggled = not _crouch_toggled
					_down_until = DOWN_CLIMB
				else:
					_crouch_toggled = not bool(_ctx["crouched"])
			&"drop":
				# Let go of whatever the hands are on: a ledge, a climb's
				# holds, or a load. On the ground with an edge behind, the
				# native lowers the body over it into a hang. In the
				# slingshot's pouch, out of it.
				if int(_ctx.get("hook_state", 0)) != 0:
					_native.request_hook_release()
				elif bool(_ctx["sling_seated"]) or bool(_ctx["sling_recovering"]):
					_native.request_slingshot_drop()
				elif bool(_ctx["swing_seated"]):
					_native.request_swing_drop()
				elif bool(_ctx["trapeze_hanging"]):
					_native.request_trapeze_drop()
				elif int(_ctx["carrying"]) != 0:
					_native.request_set_down()
				else:
					_native.request_release()
			&"chute":
				_native.request_parachute()
			&"crane":
				_native.request_yard_crane_operate()
			&"hook":
				if bool(_ctx.get("has_hook", false)) and int(_ctx.get("hook_state", 0)) == 0:
					_native.set_hook_aim(-_camera.global_transform.basis.z)
					_native.request_hook_fire()
			&"back":
				if bool(_ctx["sling_seated"]) or bool(_ctx["sling_recovering"]):
					_native.request_slingshot_drop()
				elif bool(_ctx["swing_seated"]):
					_native.request_swing_drop()
				elif bool(_ctx["trapeze_hanging"]):
					_native.request_trapeze_drop()
				elif _ctx["hanging"] or _ctx["climbing"] or _ctx["swinging"]:
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
	if _jump_buffer > 0.0:
		if _ctx["jump_ok"]:
			_native.request_jump()
			_jump_buffer = 0.0
		else:
			_jump_buffer = maxf(0.0, _jump_buffer - delta)
	if _up_buffer > 0.0:
		if _native.request_up():
			_up_buffer = 0.0
		else:
			_up_buffer = maxf(0.0, _up_buffer - delta)


# A mantle, vault or lowering under way: the native takes no jump in it.
func _committed() -> bool:
	var traversal := int(_ctx["traversal"])
	return traversal == TRAVERSAL_MANTLING or traversal == TRAVERSAL_VAULTING or \
		traversal == TRAVERSAL_LOWERING


# What a swipe down does from here, in the order the hands let go: off the
# hook's line, the trapeze's pull (held), off a ledge or a bar, down a climb's
# holds, a load put down; on the ground with an edge close behind and the
# body nearly still, over it into a hang; already crouched, nothing (a swipe
# up stands); running, a slide; else a crouch; in the air, the legs drawn up
# (a tuck, or a braced landing).
func _down_route() -> StringName:
	if int(_ctx.get("hook_state", 0)) != 0:
		return &"hook_release"
	if bool(_ctx["trapeze_hanging"]):
		return &"pull"
	if _ctx["hanging"] or _ctx["swinging"]:
		return &"let_go"
	if _ctx["climbing"]:
		return &"slide_down"
	if int(_ctx["traversal"]) != TRAVERSAL_NONE:
		return &""
	if int(_ctx["carrying"]) != 0:
		return &"set_down"
	if not _ctx["grounded"]:
		return &"tuck"
	# Over what the feet stand on: on a moving deck, standing still is still.
	var velocity: Vector3 = _ctx["velocity"] - _native.get_support_point_linear_velocity()
	var speed := Vector2(velocity.x, velocity.z).length()
	if _ctx["edge_drop"] and speed < EDGE_DROP_MAX_MPS:
		return &"drop_down"
	if _ctx["crouched"]:
		return &""
	return &"slide" if speed >= SLIDE_MPS else &"crouch"


func _down() -> void:
	var route := _down_route()
	# The thumb kept on the glass keeps the body down only where down is a
	# crouch, a pull or a slide down the holds, never after letting go.
	_down_hold_counts = route in [&"crouch", &"slide", &"tuck", &"slide_down", &"pull"]
	match route:
		&"hook_release":
			_native.request_hook_release()
		&"let_go", &"drop_down":
			_native.request_release()
		&"set_down":
			_native.request_set_down()
		&"slide_down":
			_crouch_toggled = true
			_down_until = DOWN_CLIMB
		&"crouch", &"slide":
			_crouch_toggled = true
			_down_until = DOWN_GROUND
			_down_frames = 0
		&"tuck":
			_crouch_toggled = true
			_down_until = DOWN_AIR


# What a swipe would do, drawn where the thumb swipes (touch_controls).
func _update_gesture_hints() -> void:
	var up := int(_native.get_up_move())
	_touch.up_hint = UP_HINTS[up] if up < UP_HINTS.size() else ""
	_touch.up_hint_special = not up in [UP_NONE, UP_JUMP, UP_PRECISION]
	if _ctx["climbing"] and _crouch_toggled:
		_touch.up_hint = "HOLD ON"
	var route := _down_route()
	_touch.down_hint = DOWN_HINTS.get(route, "")
	_touch.down_hint_special = route in [&"hook_release", &"pull", &"let_go", &"slide_down", &"set_down",
		&"drop_down"]
	# What lets go of a hold or jumps off it is made only by a flick that lifts;
	# so is a throw, its path drawn while the thumb is still on the glass.
	_touch.up_on_lift = up in [UP_JUMP_OFF, UP_LAUNCH, UP_LET_GO, UP_THROW]
	_touch.down_on_lift = route in [&"hook_release", &"let_go", &"slide_down"]

# The throw's path, while the thumb swipes up with a throwable in the hands
# (made as it lifts): faint, and gone with the thumb.
func _update_throw_arc(delta: float) -> void:
	if _throw_arc == null:
		return
	var aiming := bool(_touch.pending_verb() == &"up") and int(_native.get_up_move()) == UP_THROW
	if aiming:
		_throw_path = _native.get_throw_prediction()
	_throw_arc.update_view(_throw_path, aiming, delta)

# Contextual Action is a gateway (Governing Law 27): it climbs what the
# native affordance reports, takes hold of what is in reach, or hooks what is
# held. It never operates a machine on the player's behalf.
func _perform_action() -> void:
	var action: Dictionary = _ctx["action"]
	match action["id"]:
		&"climb_up", &"climb":
			_native.request_traversal()
		&"let_go":
			_native.request_release()
		&"hook_take":
			_native.request_hook_take()
		&"haul":
			pass   # held, not tapped: see _process
		&"pick_up":
			_native.request_pick_up()
		&"set_down":
			_native.request_set_down()
		&"hook", &"unhook":
			_native.request_rig()
		&"slingshot":
			if bool(_ctx["sling_recovering"]):
				_native.request_slingshot_drop()
			else:
				_native.request_slingshot_action()
		&"swing":
			_native.request_swing_action()
		&"trapeze":
			if bool(_ctx["trapeze_hanging"]):
				_native.request_trapeze_drop()
			else:
				_native.request_trapeze_action()
		&"lift":
			_native.request_lift_action()
		&"drive":
			_native.request_forklift_action()
		&"crane":
			_native.request_yard_crane_operate()
		&"crane_device":
			_native.request_yard_crane_device()
		&"crane_take":
			_native.request_yard_crane_take()
		&"s1", &"incline", &"tram", &"derrick":
			# S1 and the slab incline are worked by standing on their plates;
			# Action only reads them out.
			pass
		_:
			# Nothing reported in reach: ask anyway, exactly as the old E key
			# did. The native decides there is no ledge (and counts it).
			_native.request_traversal()
const LAMP_REACH := 100.0
# The searchlight has just been lit by the climber: the first time, ever, is
# kept in the profile so every later climb starts under its beam.
const SUMMIT_PROFILE_PATH := "user://summit.lit"

func _on_summit_lit() -> void:
	_hud.toast("THE LIGHT IS ON", "SEEN FROM THE GROUND. NOW GET DOWN.", UiStyle.AMBER, 7.0)
	_hud.flash(UiStyle.AMBER)
	_haptic(&"press", 1.0)
	if _save_enabled:
		var file := FileAccess.open(SUMMIT_PROFILE_PATH, FileAccess.WRITE)
		if file != null:
			file.store_string("lit\n")
	print("SCRAPERX_SUMMIT_LIT")

const VIEW_PROP_RANGE := 250.0

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
