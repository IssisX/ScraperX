extends Node

# Actual viewport touches -> shipping router/Main -> existing native physics.
# The fixed step is explicit; a slow host renderer cannot lengthen input time.
const STEP := 1.0 / 90.0
const Gestures := preload("res://presentation/ui/thumb_parkour_gestures.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
var main: Node
var native: Object
var checks := 0
var failures := 0
var finished := false
var capture_dir := ""


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	if "--quit-after-thumb-flow-check" in OS.get_cmdline_user_args():
		_run.call_deferred()
	else:
		var start := Button.new()
		start.name = "RunThumbFlowCheck"
		start.text = "Run ScraperX thumb-zone controls"
		start.position = Vector2(24, 80)
		start.pressed.connect(func() -> void:
			start.disabled = true
			_run.call_deferred())
		add_child(start)


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_THUMB_FLOW_INPUT FAIL " + why)


func step(count: int = 1) -> void:
	var before: int = native.get_tick_index()
	for _i in count:
		main._process(STEP)
	check(native.get_tick_index() == before + count, "native advances exactly the requested input ticks")
	check(native.get_death_count() == 0, "no death or hidden recovery")


func at() -> Vector2:
	return main._touch._safe.position + main._touch._safe.size * Vector2(0.76, 0.65)


func touch(index: int, point: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = point
	event.pressed = pressed
	get_viewport().push_input(event, true)


func drag(index: int, from: Vector2, to: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = to
	event.relative = to - from
	get_viewport().push_input(event, true)


func tap(index: int, point: Vector2) -> void:
	touch(index, point, true)
	touch(index, point, false)


func arm() -> void:
	touch(1, at(), true)
	step(15)
	check(main._touch._thumb.phase == Gestures.Phase.COMMAND, "held right thumb opens the action fan")


func stage(point: Vector3, ticks: int, yaw: float = 0.0) -> void:
	main._router.clear_held()
	main._crouch_toggled = false
	main._crouch_toggle_from_hang = false
	main._yaw = yaw
	main._pitch = 0.0
	check(native.debug_restart_at(point), "native accepts initial footing/flight at " + str(point))
	step(ticks)


func find_button(node: Node, text: String) -> Button:
	if node is Button and node.text == text:
		return node as Button
	for child in node.get_children():
		var found := find_button(child, text)
		if found != null:
			return found
	return null


func capture(name: String) -> void:
	if capture_dir.is_empty():
		return
	check(DisplayServer.get_name() != "headless", "capture uses a real renderer")
	if DisplayServer.get_name() == "headless":
		return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	var tick: int = native.get_tick_index()
	await RenderingServer.frame_post_draw
	var shot := get_viewport().get_texture().get_image()
	check(shot != null and not shot.is_empty(), "actual game capture: " + name)
	if shot != null and not shot.is_empty():
		check(shot.save_png(capture_dir.path_join(name + ".png")) == OK, "save " + name)
	check(native.get_tick_index() == tick, "capture cannot advance fixed input time")


func _run() -> void:
	get_tree().root.size = Vector2i(1280, 900)
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene, "shipping Main and real native world")
	if native == null:
		_finish()
		return
	var bank_deadline := Time.get_ticks_msec() + 30000
	while not main._audio._bank_ready and Time.get_ticks_msec() < bank_deadline:
		await get_tree().process_frame
	check(main._audio._bank_ready, "actual slide friction bank is ready")
	if not main._audio._bank_ready:
		_finish()
		return
	step(180)
	main._open_pause()
	main._pause_menu._show_page(&"settings")
	# Containers resolve their clickable rectangles on the next process frame.
	# The real touch/menu path must use those rectangles, not an unlaid-out row.
	await get_tree().process_frame
	await get_tree().process_frame
	var choice := find_button(main._pause_menu, "BUTTONS")
	check(choice != null, "shipping settings offer the touch profile")
	if choice == null:
		_finish()
		return
	# Real GUI mouse input is the desktop equivalent of the menu's emulated
	# touch click; gameplay gestures below use actual ScreenTouch events.
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.position = choice.get_global_rect().get_center()
	print("THUMB_PROFILE_CLICK rect=%s visible=%s" % [choice.get_global_rect(), choice.is_visible_in_tree()])
	click.pressed = true
	get_viewport().push_input(click, true)
	click.pressed = false
	get_viewport().push_input(click, true)
	check(main._settings.touch_profile == 1 and main._touch.control_profile == 1,
		"actual profile selector applies through shipping settings handler")
	main._pause_menu.press_resume()
	step(2)
	check(main._touch._thumb_active, "on-foot Thumb Flow is active after resume")
	if not main._touch._thumb_active:
		_finish()
		return
	# The native ExteriorGrade approach is clear; the restart-menu grade
	# point at z=-58 faces a real slingshot post and cannot be a sprint lane.
	stage(Vector3(6, 1.2, -25), 45)
	check(native.is_player_grounded(), "stance begins on actual ground")
	tap(0, main._touch.stick_home())
	step(2)
	check(native.is_player_crouched(), "left tap reaches native crouch")
	tap(0, main._touch.stick_home())
	step(2)
	check(not native.is_player_crouched(), "second left tap stands when clear")
	await capture("01-thumb-flow-idle")
	var left: Vector2 = main._touch.stick_home()
	var forward := left + Vector2(0.0, -Touch.STICK_THROW * main._touch._u * 1.4)
	touch(0, left, true)
	drag(0, left, forward)
	# Existing finite traction/work takes about1.2s to earn full sprint.
	step(120)
	print("THUMB_SPRINT position=%s velocity=%s grounded=%s sprint=%s input=%s latch=%s" % [
		native.get_player_position(), native.get_player_linear_velocity(), native.is_player_grounded(),
		native.is_player_sprinting(), main._touch.move_vector, main._touch.sprint_latched])
	check(native.is_player_sprinting() and native.get_player_linear_velocity().length() > 5.0,
		"left drag produces real sprint speed")
	# Keeping the right thumb in look while the left changes stance leaves
	# camera aiming available through the slide entry.
	touch(1, at(), true)
	drag(1, at(), at() + Vector2(30, 0))
	touch(0, forward, false)
	var was_silent: bool = main._audio._silent
	main._audio._silent = false
	tap(0, left)
	step(2)
	check(bool(native.get_landing_state().get("player_sliding", false)), "left tap at real speed enters native slide")
	check(main._audio.slide_friction.active and main._audio._slide_voice.playing,
		"actual native sliding drives its real continuous friction player")
	var slide: Dictionary = native.get_landing_state()
	check(is_equal_approx(main._audio.slide_friction.strength,
		clampf(float(slide.slide_speed_mps) / 8.0, 0.0, 1.0)),
		"friction strength follows measured support-relative native slip")
	check(main._audio._slide_voice.bus == &"Effects" and main._audio._voices.size() == 6,
		"one dedicated Effects loop retains the original fixed one-shot voice pool")
	main._audio._silent = was_silent
	touch(1, at(), false)
	main._open_pause()
	await get_tree().process_frame
	await get_tree().process_frame
	check(not main._audio.slide_friction.active and not main._audio._slide_voice.playing,
		"actual pause stops ground friction while menu audio remains available")
	main._pause_menu.press_resume()
	stage(Vector3(6, 1.2, -25), 45)
	check(not main._audio.slide_friction.active and not main._audio._slide_voice.playing,
		"restart to actual stance cannot replay the previous slide")
	touch(0, main._touch.stick_home(), true)
	drag(0, main._touch.stick_home(), main._touch.stick_home() + Vector2(0, -120) * main._touch._u)
	tap(1, at())
	step(3)
	check(not native.is_player_grounded() and native.get_player_linear_velocity().y > 1.0,
		"right tap jumps through native support while movement remains held")
	var yaw: float = main._yaw
	touch(1, at(), true)
	drag(1, at(), at() + Vector2(100, -25))
	step()
	check(absf(main._yaw - yaw) > 0.1 and main._touch.move_vector.y > 0.5,
		"turning jump preserves camera aim and left-thumb movement")
	touch(1, at(), false)
	touch(0, main._touch.stick_home(), false)

	# Same proven gantry timing as the accepted early-catch path: arm while
	# approaching, choose Reach at tick70, real native grip becomes reachable73.
	stage(Vector3(16.85, 15, -118), 55, -PI * 0.5)
	arm()
	await capture("02-thumb-flow-fan")
	var reach := at() + Vector2(0.0, -main._touch._thumb.stroke * 1.2)
	drag(1, at(), reach)
	step()
	check(bool(native.get_landing_state().get("air_catch_intent_pending", false)),
		"up stroke produces native early Reach without fake support")
	step(7)
	check(int(native.get_landing_state().traversal_hand_constraint_count) > 0,
		"held Reach acquires actual physical gantry grip")
	touch(1, reach, false)
	step()
	check(int(native.get_landing_state().traversal_hand_constraint_count) > 0,
		"lifting after acquisition preserves the real grip")
	# A finite catch may later lose its grip. The release guard cannot grant
	# infinite support. Exercise deliberate Drop from the proven cargo edge,
	# where unchanged actual footing must remain until the input commits.
	stage(Vector3(22, 12, -120.3), 180, PI * 0.5)
	check(native.is_player_grounded() and main._ctx.drop_ok, "real cargo edge offers lowering")
	arm()
	var inward := at() + Vector2(-main._touch._thumb.stroke * 1.2, 0.0)
	drag(1, at(), inward)
	step(33)
	check(native.is_player_grounded() and native.get_traversal_state() == 0,
		"even armed Drop preserves real edge footing until deliberate lift")
	touch(1, inward, false)
	step()
	check(native.get_traversal_state() == 5, "guarded release reaches native ledge lowering")

	# Existing production physical top-out, controlled by the new touch
	# profile. Native hands and receiving contact earn the higher footing.
	stage(Vector3(26.68, 87.92, -132), 45, PI * 0.5)
	arm()
	reach = at() + Vector2(0.0, -main._touch._thumb.stroke * 1.2)
	drag(1, at(), reach)
	step()
	check(int(native.get_landing_state().traversal_hand_constraint_count) == 2,
		"grounded Reach starts the existing physical two-hand top-out")
	touch(1, reach, false)
	var received := false
	for _i in 153:
		step()
		if native.is_player_grounded() and native.get_support_entity_id() == 11 \
				and native.get_player_position().y > 88.7 \
				and int(native.get_landing_state().traversal_hand_constraint_count) == 0:
			received = true
			break
	check(received, "touch-controlled physical top-out earns actual upper footing")
	step(90)
	check(native.is_player_grounded() and native.get_player_position().y > 88.7,
		"actual receiving footing remains stable after the finite push")
	var exit_start: Vector3 = native.get_player_position()
	left = main._touch.stick_home()
	touch(0, left, true)
	drag(0, left, left + Vector2(100, 0) * main._touch._u)
	step(15)
	touch(0, left, false)
	check(native.is_player_grounded() and native.get_player_position().distance_to(exit_start) > 0.03,
		"ordinary thumb movement continues on the receiving floor")

	stage(Vector3(16.85, 15, -118), 55, -PI * 0.5)
	arm()
	reach = at() + Vector2(0, -main._touch._thumb.stroke * 1.2)
	drag(1, at(), reach)
	step()
	check(bool(native.get_landing_state().get("air_catch_intent_pending", false)), "cancellation starts from actual pending Reach")
	touch(1, reach, false)
	step(7)
	check(not bool(native.get_landing_state().get("air_catch_intent_pending", true)) \
		and int(native.get_landing_state().traversal_hand_constraint_count) == 0 and native.is_grip_available(),
		"early lift cancels only the gesture's pending Reach despite a later valid grip")
	# Focus/pause cancels unfinished input instead of committing on resume.
	arm()
	main._open_pause()
	touch(1, at(), false)
	main._pause_menu.press_resume()
	step()
	check(main._touch._thumb.phase == Gestures.Phase.IDLE and main._router._verbs.is_empty(),
		"pause/resume cannot replay an armed gesture")
	_finish()


func _finish() -> void:
	finished = true
	get_tree().paused = false
	print("SCRAPERX_THUMB_FLOW_INPUT %s checks=%d native=1" % ["PASS" if failures == 0 else "FAIL", checks])
	if "--quit-after-thumb-flow-check" in OS.get_cmdline_user_args():
		get_tree().quit(0 if failures == 0 else 1)
