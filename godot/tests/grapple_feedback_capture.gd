extends SceneTree
# Actual shipping tool/native projectile and reel at a diagnostic rack stage.
# This capture does not claim retained anchor, route completion or device play.
const STEP := 1.0 / 90.0
var main: Node
var native: Object
var prefix := ""
var failed := false

func _initialize() -> void:
	_run.call_deferred()

func require(ok: bool, label: String) -> bool:
	if not ok:
		failed = true
		push_error("GRAPPLE_FEEDBACK_CAPTURE " + label)
	return ok

func tick(count := 1) -> void:
	for i in count:
		native.advance_frame(STEP)
		main._update_grapple_feedback(STEP)

func capture(label: String) -> void:
	main._process(0.0)
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	require(image != null and image.get_width() == 1280 and image.get_height() == 900, "actual renderer image")
	require(image.save_png(prefix + "_" + label + ".png") == OK, "save " + label)
	print("GRAPPLE_FEEDBACK_VIEW " + JSON.stringify({"view": label, "native_tick": native.get_tick_index(),
		"launch_cues": main._audio.grapple_launches, "recoil": main._grapple_view.recoil_amount,
		"spool_phase": main._grapple_view.spool_phase, "motor": main._audio.grapple_motor_active,
		"native_reel_work_j": native.get_grapple_gun_state().reel_work_j,
		"native_line_length": native.get_grapple_gun_state().paid_out_length,
		"diagnostic_stage": true, "capture_claim": false, "phone_play": false}))

func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="):
			prefix = arg.trim_prefix("--capture=")
	if not require(DisplayServer.get_name() != "headless" and not prefix.is_empty(), "renderer and output required"):
		quit(1)
		return
	root.size = Vector2i(1280, 900)
	root.content_scale_size = root.size
	main = load("res://main.tscn").instantiate()
	main._settings_persistent = false
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	require(native.debug_restart_at(Vector3(12, 485.15, -135.5)), "actual rack stage")
	main._after_restart()
	tick(90)
	require(native.set_grapple_equipped(true), "real rack equip request")
	tick(2)
	require(native.get_grapple_gun_state().equipped, "native equip acknowledgement")
	main._yaw = -PI * 0.5
	main._pitch = 0.15
	if main._sky_cycle != null:
		main._sky_cycle.set_hour(17.0)
	main._process(0.0)
	await capture("ready")
	var launches: int = main._audio.grapple_launches
	require(native.request_grapple_action(), "ordinary finite shot")
	tick()
	require(main._audio.grapple_launches == launches + 1 and main._grapple_view.recoil_amount > 0.0, "actual launch drives report and tool recoil")
	await capture("launch")
	tick(16)
	require(native.set_grapple_auto_reel(true), "ordinary auto reel")
	var winding := false
	for i in 90:
		tick()
		if main._audio.grapple_motor_active and native.get_grapple_gun_state().reel_work_j > 0.0:
			winding = true
			break
	require(winding, "actual finite reel work drives spool and motor")
	await capture("winding")
	main._audio.quiesce()
	print("GRAPPLE_FEEDBACK_CAPTURE " + ("FAIL" if failed else "PASS") + " diagnostic_stage=1 retained_anchor_claim=0")
	quit(1 if failed else 0)
