extends SceneTree

# Adapter receipts use explicit fixtures. The second half executes shipping
# Main and the real native owner at a diagnostic rack stage, not a route proof.
const Model := preload("res://presentation/grapple_feedback_model.gd")
const STEP := 1.0 / 90.0
var main: Node
var native: Object
var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("GRAPPLE_PHYSICAL_FEEDBACK FAIL " + message)

func quiet(receipt: Dictionary) -> bool:
	return not receipt.launch and not receipt.failed and not receipt.warning and receipt.wound_m == 0.0

func gun() -> Dictionary:
	return {"equipped": true, "line_attached": true, "state": 2,
		"total_launch_work_j": 400.0, "reel_work_j": 0.0,
		"paid_out_length": 10.0, "tension_n": 0.0, "last_failure": ""}

func adapter_contract() -> void:
	var model := Model.new()
	var state := gun()
	var first := model.sample(90, 0, state)
	check(first.fresh and first.reset and quiet(first) and first.power == 0.0, "first observation seeds cumulative work without firing")
	state.total_launch_work_j = 800.0
	var shot := model.sample(91, 0, state)
	check(shot.launch and not shot.reset, "positive committed launch work earns one event")
	var duplicate := model.sample(91, 0, state)
	check(not duplicate.fresh and quiet(duplicate), "duplicate native tick cannot replay event or winding delta")
	var stale := model.sample(89, 0, {"total_launch_work_j": 9000.0})
	check(not stale.fresh and quiet(stale) and stale.equipped, "older same-epoch sample ignored while continuous targets retained")
	state.cartridges_remaining = 4
	state.state = 3
	state.line_attached = false
	check(quiet(model.sample(92, 0, state)), "recover and inventory changes cannot fabricate launch")
	state.equipped = false
	check(quiet(model.sample(93, 0, state)), "ordinary stow is quiet")
	state = gun()
	var rewind := model.sample(94, 0, state)
	check(rewind.reset and quiet(rewind), "counter rewind reseeds silently")
	state.total_launch_work_j = 2000.0
	var death := model.sample(95, 1, state)
	check(death.reset and quiet(death) and death.power == 0.0, "death consumes counters without replay")
	model.reset()
	var restart := model.sample(0, 1, state)
	check(restart.reset and quiet(restart), "explicit restart accepts lower native clock without replay")
	for bad_value in [NAN, -1.0]:
		state = gun()
		state.reel_work_j = bad_value
		var invalid := model.sample(1, 1, state)
		check(invalid.reset and quiet(invalid) and invalid.power == 0.0 and not invalid.equipped, "invalid or negative counter clears effects quietly")
	model.reset()
	state = gun()
	model.sample(0, 0, state)
	state.paid_out_length = 9.95
	var slack := model.sample(9, 0, state)
	check(is_equal_approx(slack.wound_m, 0.05) and slack.winding > 0.0 and slack.power == 0.0,
		"measured slack takeup earns winding without invented command work")
	state.auto_reel = true
	state.tension_n = 3000.0
	var stationary := model.sample(18, 0, state)
	check(stationary.power == 0.0 and stationary.winding == 0.0 and stationary.wound_m == 0.0,
		"stationary tension and automatic reel intent do not earn motor work")
	state.reel_work_j = 10000.0
	state.paid_out_length = 1.0
	state.tension_n = 10000.0
	var bounded := model.sample(19, 0, state)
	check(bounded.power == 1.0 and bounded.winding == 1.0 and bounded.load == 1.0 and bounded.danger == 1.0,
		"extreme positive measured rates remain bounded")
	var stale_load := model.sample(19, 0, state)
	check(not stale_load.fresh and quiet(stale_load) and stale_load.load == 1.0 and stale_load.power == 1.0,
		"stale loaded sample retains targets but contributes no spool distance")
	for reason in ["LINE SNAGGED", "LINE BROKEN", "NO SHOT", ""]:
		model.reset()
		state = gun()
		model.sample(0, 0, state)
		state.line_attached = false
		state.state = 4
		state.last_failure = reason
		var broken := model.sample(1, 0, state)
		check(broken.failed == (reason in ["LINE SNAGGED", "LINE BROKEN"]), "failure cue requires specific native line failure " + reason)
		check(not model.sample(2, 0, state).failed, "persistent failed state never repeats " + reason)
	model.reset()
	state = gun()
	state.line_attached = false
	model.sample(0, 0, state)
	state.state = 4
	state.last_failure = "LINE BROKEN"
	check(not model.sample(1, 0, state).failed, "failed shot without previously attached line is quiet")
	model.reset()
	state = gun()
	model.sample(0, 0, state)
	state.line_attached = false
	state.state = 0
	check(not model.sample(1, 0, state).failed, "ordinary line release is not failure")
	model.reset()
	state = gun()
	model.sample(0, 0, state)
	state.tension_n = 2399.0
	check(not model.sample(1, 0, state).warning, "danger remains quiet below 80 percent")
	state.tension_n = 2400.0
	check(model.sample(2, 0, state).warning, "danger enters at 80 percent")
	state.tension_n = 2100.0
	check(not model.sample(3, 0, state).warning, "70 percent retains danger hysteresis without repeating")
	state.tension_n = 2450.0
	check(not model.sample(47, 0, state).warning, "retained danger never warns again merely as cooldown expires")
	state.tension_n = 2099.0
	model.sample(48, 0, state)
	state.tension_n = 2400.0
	check(model.sample(49, 0, state).warning, "below 70 percent clears latch and allows later entry")
	state.tension_n = 2000.0
	model.sample(50, 0, state)
	state.tension_n = 2500.0
	check(not model.sample(51, 0, state).warning, "danger reentry within 45 ticks remains quiet")
	state.tension_n = 2000.0
	model.sample(93, 0, state)
	state.tension_n = 2500.0
	check(model.sample(94, 0, state).warning, "45 native ticks permit next warning on actual reentry")

func step(count: int = 1, delta: float = STEP) -> void:
	for i in count:
		await process_frame
		main._process(delta)

func verb(value: StringName) -> void:
	main._router.push_verb(value)

func _run() -> void:
	adapter_contract()
	root.size = Vector2i(2160, 1856)
	main = load("res://main.tscn").instantiate()
	main._settings_persistent = false
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene and not main._ci_mode, "actual shipping Main/native world")
	var view: Node = main._grapple_view
	var audio: Node = main._audio
	check(audio.has_method("update_grapple_feedback") and view.has_method("present_feedback"), "production feedback consumers wired")
	check(native.debug_restart_at(Vector3(12, 485.15, -135.5)), "diagnostic rack pose accepted")
	await step(45)
	verb(&"toggle_grapple")
	await step(2)
	check(bool(native.get_grapple_gun_state().equipped) and view._tool.visible, "router draw acknowledged by native and tool visibility")
	check(not main._arms._remote.visible, "drawing gun never presents the crane radio pendant")
	var geometry: Array = native.get_grapple_hook_geometry()
	var stem_found := false
	for part: Dictionary in geometry:
		var lo := INF
		var hi := -INF
		for point: Vector3 in part.polygon:
			lo = minf(lo, point.y)
			hi = maxf(hi, point.y)
		if absf(hi - lo - 0.175) < 0.00001:
			stem_found = true
	check(stem_found and view._hook_geometry == geometry, "view consumes real native 175 mm stem geometry")
	var launches: int = audio.grapple_launches
	var receipts: int = view.launch_receipts
	var work: float = native.get_grapple_gun_state().total_launch_work_j
	main._pitch = 1.49
	verb(&"action")
	await step(2)
	check(float(native.get_grapple_gun_state().total_launch_work_j) > work, "ordinary Action commits native launch work")
	check(audio.grapple_launches == launches + 1 and view.launch_receipts == receipts + 1, "one committed native shot earns one sound and view receipt")
	await step(5, 0.0)
	check(audio.grapple_launches == launches + 1 and view.launch_receipts == receipts + 1, "duplicate render samples never replay sound or recoil receipt")
	var camera_pose: Transform3D = main._camera.global_transform
	var player_pose: Vector3 = native.get_player_position()
	var yaw: float = main._yaw
	var pitch: float = main._pitch
	var render_state: Dictionary = native.get_grapple_gun_render_state()
	var visual_pulse := {"fresh": true, "launch": true, "equipped": true, "wound_m": 0.035, "power": 1.0, "danger": 1.0}
	var phase: float = view.spool_phase
	view.present_feedback(visual_pulse, 0.0, false)
	view.update_state(render_state)
	check(view.recoil_amount > 0.0 and (view._tool.global_transform * view.MUZZLE_LOCAL).distance_to(render_state.launch_origin) > 0.001, "launch feedback moves only decorative held tool")
	check(main._camera.global_transform == camera_pose and native.get_player_position() == player_pose and main._yaw == yaw and main._pitch == pitch,
		"view-only recoil cannot alter camera aim or native player pose")
	check(is_equal_approx(view.spool_phase, fposmod(phase + 1.0, TAU)), "decorative spool follows measured takeup distance")
	var phase_after: float = view.spool_phase
	view.present_feedback({"fresh": false, "equipped": true, "wound_m": 0.035, "launch": true}, 0.0, false)
	check(view.spool_phase == phase_after, "stale render cannot turn spool again")
	view.present_feedback(visual_pulse, 0.0, true)
	view.update_state(render_state)
	check(view.recoil_amount == 0.0 and (view._tool.global_transform * view.MUZZLE_LOCAL).distance_to(render_state.launch_origin) < 0.00002, "reduced motion keeps tool at native launch origin")
	launches = audio.grapple_launches
	receipts = view.launch_receipts
	verb(&"toggle_grapple")
	await step(2)
	check(not bool(native.get_grapple_gun_state().equipped) and not view._tool.visible and view.recoil_amount == 0.0 and not audio.grapple_motor_active, "native stow clears tool recoil and motor")
	check(audio.grapple_launches == launches and view.launch_receipts == receipts, "stow never manufactures launch receipt")
	verb(&"toggle_grapple")
	await step(2)
	check(audio.grapple_launches == launches and view.launch_receipts == receipts, "redraw with unchanged work counters does not replay shot")
	view.present_feedback(visual_pulse, 0.0, false)
	main._open_pause()
	check(view.recoil_amount == 0.0 and not audio.grapple_motor_active, "shipping pause clears effects immediately")
	main._resume()
	receipts = view.launch_receipts
	await step(2, 0.0)
	check(audio.grapple_launches == launches and view.launch_receipts == receipts, "resume reseeds without old shot replay")
	view.present_feedback(visual_pulse, 0.0, false)
	main._restart_at(Vector3(12, 485.15, -135.5))
	check(view.recoil_amount == 0.0 and not audio.grapple_motor_active, "shipping restart clears effects immediately")
	receipts = view.launch_receipts
	await step(2)
	check(audio.grapple_launches == launches and view.launch_receipts == receipts, "restart counter epoch seeds quietly")
	check(native.get_death_count() == 0, "diagnostic feedback scenario causes no deaths")
	print("GRAPPLE_PHYSICAL_FEEDBACK %s checks=%d shipping_main=1 native_ack=1 fixture_adapter=1 phone_play=0" % ["FAIL" if failures else "PASS", checks])
	quit(1 if failures else 0)
