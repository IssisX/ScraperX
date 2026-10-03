extends SceneTree

const InputRouter := preload("res://presentation/ui/input_router.gd")

# Reviewable captures from the real scene and native launcher. No fixture
# forces or injected launch velocity: charging and release use native input.
var _out := "/tmp/scraperx-runtime/slingshot-visual-proof"
var _main: Node3D
var _motion := false
var _size := Vector2i.ZERO
var _shadow_level := -1
var _launch_frames := 0
var _trace: FileAccess


func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--sling-probe-out="):
			_out = argument.trim_prefix("--sling-probe-out=")
		elif argument == "--sling-probe-motion":
			_motion = true
		elif argument.begins_with("--sling-probe-shadows="):
			_shadow_level = clampi(int(argument.trim_prefix("--sling-probe-shadows=")), 0, 4)
		elif argument.begins_with("--sling-probe-size="):
			var dimensions := argument.trim_prefix("--sling-probe-size=").split("x")
			if dimensions.size() == 2:
				_size = Vector2i(int(dimensions[0]), int(dimensions[1]))
	_run.call_deferred()


func _state() -> Dictionary:
	var state: Dictionary = _main._native.get_slingshot_render_state()
	state["trajectory_points"] = _main._native.get_slingshot_prediction()
	return state


func _view(delta: float) -> void:
	_main._render_snapshot(delta)
	_main._ctx = _main._read_context()
	_main._arms.update_arms(_main._arms_state({"move": Vector2.ZERO, "pendant": Vector2.ZERO}),
		_main._camera.global_transform, delta)
	_main._slingshot_view.update_view(delta, _state(), _main._native.get_player_render_position(),
		_main._native.get_player_linear_velocity(), _main._camera)


func _capture(name: String) -> void:
	await process_frame
	await RenderingServer.frame_post_draw
	var path := _out.path_join(name + ".png")
	var result := root.get_texture().get_image().save_png(path)
	if result != OK:
		push_error("SCRAPERX_SLINGSHOT_PROBE capture failed: " + path)
		quit(42)
	print("SCRAPERX_SLINGSHOT_CAPTURE=" + path)


func _launch_sample() -> void:
	var wall_step := 1.0 / 60.0
	var scale: float = _main._slingshot_view.get_simulation_scale()
	_main._native.advance_frame(wall_step * scale)
	_view(wall_step)
	var state := _state()
	var player: Vector3 = _main._native.get_player_render_position()
	var velocity: Vector3 = _main._native.get_player_linear_velocity()
	var acceleration: Vector3 = state.get("rider_specific_acceleration", Vector3.ZERO)
	var avatar: Node3D = _main._slingshot_view._avatar
	var pelvis: Node3D = avatar.get_node("HarnessPelvis")
	var contact: Vector3 = pelvis.global_transform * Vector3(0, -0.19, 0)
	_trace.store_csv_line(PackedStringArray([
		str(_launch_frames / 60.0), str(state.get("simulation_time_seconds", 0.0)), str(state.get("tick_index", 0)),
		str(scale), str(bool(state.seated)), str(player.x), str(player.y), str(player.z),
		str(velocity.x), str(velocity.y), str(velocity.z), str(acceleration.x), str(acceleration.y), str(acceleration.z),
		str(avatar.get_node("WorkJacket").rotation.x), str(avatar.get_node("ExpressiveHead").rotation.x),
		str(contact.distance_to(state.get("seat_surface_position", contact)))]))
	_trace.flush()
	if _motion:
		# Unlike checkpoint-only proof, every real physics/view sample reaches
		# the renderer. --write-movie can therefore capture actual recoil motion.
		await process_frame
		await RenderingServer.frame_post_draw
		if _launch_frames % 6 == 0:
			if root.get_texture().get_image().save_png(_out.path_join("motion-%03d.png" % _launch_frames)) != OK:
				push_error("SCRAPERX_SLINGSHOT_PROBE motion capture failed")
				quit(42)
				return
	_launch_frames += 1


func _run() -> void:
	DirAccess.make_dir_recursive_absolute(_out)
	if _size.x > 0 and _size.y > 0:
		# Capture-only resolution; shipping viewport/input settings are untouched.
		root.size = _size
		root.content_scale_size = _size
	_trace = FileAccess.open(_out.path_join("launch-motion.csv"), FileAccess.WRITE)
	if _trace == null:
		push_error("SCRAPERX_SLINGSHOT_PROBE trace cannot be written")
		quit(42)
		return
	_trace.store_csv_line(PackedStringArray(["wall_s", "native_render_s", "tick", "scale", "seated", "x", "y", "z", "vx", "vy", "vz", "specific_ax", "specific_ay", "specific_az", "torso_pitch", "head_pitch", "seat_contact_error_m"]))
	_main = load("res://main.tscn").instantiate()
	root.add_child(_main)
	_main.set_process(false)
	if _main._native == null or _main._slingshot_view == null:
		push_error("SCRAPERX_SLINGSHOT_PROBE native slingshot is unavailable")
		quit(42)
		return
	_main._router.capture_mouse = false
	_main._router.device = InputRouter.Device.TOUCH
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_main._settings.head_bob = true
	_main._settings.launch_cinematics = true
	_main._settings.time_of_day = 2
	_main._sky_cycle.set_hour(12.5)
	if _shadow_level >= 0:
		# Capture-only setting for software renderers; no default-fidelity claim.
		_main._settings.shadow_quality = _shadow_level
		_main._apply_settings()
	print("SCRAPERX_SLINGSHOT_CAPTURE_SETTINGS shadows=%d viewport=%s" % [_main._settings.shadow_quality, root.size])
	_main._touch.visible = false
	_main._hud.visible = false
	_main._arms.visible = false
	var camera: Camera3D = _main._camera
	var initial: Dictionary = _main._native.get_slingshot_state()
	var station: Vector3 = initial.get("neutral_position", initial["pouch_position"])
	_view(0.0)
	camera.position = station + Vector3(14.0, 7.0, 18.0)
	camera.look_at(station + Vector3(0.0, 4.0, -4.0))
	camera.fov = 65.0
	_main._arms.visible = false
	await _capture("wooden-launcher")
	camera.position = station + Vector3(2.0, 1.65, 3.0)
	camera.look_at(station + Vector3(0.0, 0.30, 0.0))
	camera.fov = 60.0
	await _capture("leather-pouch")
	if not _main._native.debug_restart_at(station + Vector3.UP * 0.76):
		push_error("SCRAPERX_SLINGSHOT_PROBE real pouch entry was rejected")
		quit(42)
		return
	var dt: float = _main._native.get_fixed_step_seconds()
	_main._native.advance_frame(dt)
	_main._native.request_slingshot_action()
	_main._native.advance_frame(dt)
	if not bool(_main._native.get_slingshot_state()["seated"]):
		push_error("SCRAPERX_SLINGSHOT_PROBE real leather harness did not board")
		quit(42)
		return
	for i in 90:
		_main._native.advance_frame(dt)
	var loaded: Dictionary = _main._native.get_slingshot_render_state()
	if (loaded.get("leather_vertices", PackedVector3Array()) as PackedVector3Array).size() != 171 \
			or float(loaded.get("leather_deflection_m", 0.0)) >= -0.015:
		push_error("SCRAPERX_SLINGSHOT_PROBE native leather surface did not yield under rider load")
		quit(42)
		return
	_view(0.0)
	camera.position = station + Vector3(2.0, 1.65, 3.0)
	camera.look_at(station + Vector3(0.0, 0.30, 0.0))
	camera.fov = 60.0
	await _capture("leather-pouch-loaded")
	_main._slingshot_view._avatar.visible = true
	camera.position = station + Vector3(2.0, 1.25, 2.4)
	camera.look_at(station + Vector3.UP * 0.35)
	await _capture("cradled-rider-on-native-leather")
	_main._slingshot_view._avatar.visible = false
	var elevation := float(initial["elevation_rad"])
	var yaw := float(initial["yaw_rad"])
	var max_draw := float(initial.get("max_draw_m", 12.0))
	for i in 1350:
		_main._native.set_slingshot_input(1.0, yaw, elevation)
		_main._native.advance_frame(dt)
		if float(_main._native.get_slingshot_state()["draw_m"]) >= max_draw - 0.025:
			break
	_main._native.set_slingshot_input(0.0, yaw, elevation)
	for i in 36:
		_main._native.advance_frame(dt)
	var charged: Dictionary = _main._native.get_slingshot_state()
	if float(charged["draw_m"]) < max_draw - 0.04 or not bool(charged.get("release_ready", false)) \
			or float(charged["work_j"]) < float(charged["energy_j"]):
		push_error("SCRAPERX_SLINGSHOT_PROBE full charge was not funded and ready")
		quit(42)
		return
	_main._yaw = -yaw
	_main._pitch = elevation
	_view(1.0 / 60.0)
	await _capture("charged-aim-interface")
	_main._native.request_slingshot_action()
	for i in 47:
		await _launch_sample()
	await _capture("release-orbit-human-rider")
	if int(_main._native.get_slingshot_state()["launch_count"]) != int(charged["launch_count"]) + 1:
		push_error("SCRAPERX_SLINGSHOT_PROBE real restraint was not released")
		quit(42)
		return
	for i in 20:
		await _launch_sample()
	await _capture("release-orbit-front-human-rider")
	for i in 65:
		await _launch_sample()
	await _capture("accelerating-clear-orbit")
	for i in 13:
		await _launch_sample()
	await _capture("full-360-clear-finish")
	for i in 44:
		await _launch_sample()
	await _capture("return-to-true-pov")
	if _main._slingshot_view.is_cinematic_active() or _main._slingshot_view.get_simulation_scale() != 1.0:
		push_error("SCRAPERX_SLINGSHOT_PROBE cinematic did not restore real time and first person")
		quit(42)
		return
	print("SCRAPERX_SLINGSHOT_VISUAL_PROBE PASS draw_m=%.3f spring_kj=%.2f manual_kj=%.2f aim_deg=%.1f" % [
		float(charged["draw_m"]), float(charged["energy_j"]) / 1000.0,
		float(charged["work_j"]) / 1000.0, rad_to_deg(elevation)])
	_trace.close()
	quit(0)
