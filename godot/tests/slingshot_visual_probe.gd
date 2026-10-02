extends SceneTree

const InputRouter := preload("res://presentation/ui/input_router.gd")

# Reviewable captures from the real scene and native launcher. No fixture
# forces or injected launch velocity: charging and release use native input.
var _out := "/tmp/scraperx-runtime/slingshot-visual-proof"
var _main: Node3D


func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--sling-probe-out="):
			_out = argument.trim_prefix("--sling-probe-out=")
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


func _run() -> void:
	DirAccess.make_dir_recursive_absolute(_out)
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
		_main._native.advance_frame((1.0 / 60.0) * _main._slingshot_view.get_simulation_scale())
		_view(1.0 / 60.0)
	await _capture("release-orbit-human-rider")
	if int(_main._native.get_slingshot_state()["launch_count"]) != int(charged["launch_count"]) + 1:
		push_error("SCRAPERX_SLINGSHOT_PROBE real restraint was not released")
		quit(42)
		return
	for i in 20:
		_main._native.advance_frame((1.0 / 60.0) * _main._slingshot_view.get_simulation_scale())
		_view(1.0 / 60.0)
	await _capture("release-orbit-front-human-rider")
	for i in 65:
		_main._native.advance_frame((1.0 / 60.0) * _main._slingshot_view.get_simulation_scale())
		_view(1.0 / 60.0)
	await _capture("accelerating-clear-orbit")
	for i in 13:
		_main._native.advance_frame((1.0 / 60.0) * _main._slingshot_view.get_simulation_scale())
		_view(1.0 / 60.0)
	await _capture("full-360-clear-finish")
	for i in 44:
		_main._native.advance_frame((1.0 / 60.0) * _main._slingshot_view.get_simulation_scale())
		_view(1.0 / 60.0)
	await _capture("return-to-true-pov")
	if _main._slingshot_view.is_cinematic_active() or _main._slingshot_view.get_simulation_scale() != 1.0:
		push_error("SCRAPERX_SLINGSHOT_PROBE cinematic did not restore real time and first person")
		quit(42)
		return
	print("SCRAPERX_SLINGSHOT_VISUAL_PROBE PASS draw_m=%.3f spring_kj=%.2f manual_kj=%.2f aim_deg=%.1f" % [
		float(charged["draw_m"]), float(charged["energy_j"]) / 1000.0,
		float(charged["work_j"]) / 1000.0, rad_to_deg(elevation)])
	quit(0)
