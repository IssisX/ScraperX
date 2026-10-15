extends SceneTree

const Router := preload("res://presentation/ui/input_router.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
const Operator := preload("res://presentation/ui/forklift_controls.gd")
var failed := false

func check(ok: bool, label: String) -> void:
	if not ok:
		failed = true
		push_error("FORKLIFT OPERATOR: " + label)

func key(router: Node, code: Key, pressed := true) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	router._input(event)

func pad_axis(router: Node, axis: JoyAxis, value: float) -> void:
	var event := InputEventJoypadMotion.new()
	event.device = 0
	event.axis = axis
	event.axis_value = value
	router._input(event)

func pad_button(router: Node, button: JoyButton, pressed := true) -> void:
	var event := InputEventJoypadButton.new()
	event.device = 0
	event.button_index = button
	event.pressed = pressed
	router._input(event)

func finger(touch: Control, index: int, at: Vector2, pressed := true) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	touch.handle_touch(event)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var router := Router.new()
	root.add_child(router)
	var touch := Touch.new()
	touch.router = router
	router.touch = touch
	root.add_child(touch)
	await process_frame
	key(router, KEY_W)
	router.set_forklift_mode(true)
	check(router.frame(0.01).forklift.throttle == 0.0, "entry clears walking throttle")
	key(router, KEY_W)
	key(router, KEY_S)
	key(router, KEY_A)
	key(router, KEY_UP)
	key(router, KEY_LEFT)
	key(router, KEY_SHIFT)
	key(router, KEY_H)
	var frame: Dictionary = router.frame(0.01)
	check(frame.forklift == {"throttle": 1.0, "brake": 1.0, "steer": -1.0, "lift": 1.0, "tilt": 1.0, "inch": 1.0, "horn": true}, "independent pedals, hydraulics and held auxiliaries")
	check(frame.move == Vector2.ZERO and frame.pendant == Vector2.ZERO and not frame.sprint_held, "cab axes do not leak into walking or pendant")
	for code in [KEY_SPACE, KEY_C, KEY_B, KEY_T, KEY_F, KEY_N, KEY_R, KEY_P]:
		key(router, code)
	check(router.frame(0.01).verbs == [&"forklift_forward", &"forklift_neutral", &"forklift_reverse", &"forklift_park"], "cab keys suppress walking and equipment verbs")
	router.set_forklift_mode(true)
	check(router.frame(0.01).forklift.throttle == 1.0, "same mode does not clear held throttle every tick")
	router.clear_held()
	check(router.frame(0.01).forklift.throttle == 0.0, "focus clear releases throttle")
	pad_axis(router, JOY_AXIS_TRIGGER_RIGHT, 1.0)
	pad_axis(router, JOY_AXIS_TRIGGER_LEFT, 0.54)
	pad_axis(router, JOY_AXIS_LEFT_X, 1.0)
	pad_button(router, JOY_BUTTON_DPAD_DOWN)
	pad_button(router, JOY_BUTTON_DPAD_RIGHT)
	pad_button(router, JOY_BUTTON_A)
	frame = router.frame(0.01)
	check(frame.forklift.throttle == 1.0 and is_equal_approx(frame.forklift.brake, 0.5), "pad triggers are separate pedals")
	check(frame.forklift.lift == -1.0 and frame.forklift.tilt == -1.0 and frame.forklift.steer == 1.0, "D-pad hydraulics independent from pedals and steering")
	check(frame.verbs.is_empty() and frame.forklift.horn, "pad A holds horn without jump")
	pad_button(router, JOY_BUTTON_RIGHT_STICK)
	frame = router.frame(0.01)
	check(frame.forklift.inch == 1.0 and frame.verbs.is_empty(), "pad right-stick hold inches without a walking verb")
	pad_button(router, JOY_BUTTON_RIGHT_STICK, false)
	check(router.frame(0.01).forklift.inch == 0.0, "pad releases inch independently of pedals")
	for button in [JOY_BUTTON_RIGHT_SHOULDER, JOY_BUTTON_LEFT_SHOULDER, JOY_BUTTON_Y, JOY_BUTTON_LEFT_STICK, JOY_BUTTON_B, JOY_BUTTON_X]:
		pad_button(router, button)
	check(router.frame(0.01).verbs == [&"forklift_forward", &"forklift_reverse", &"forklift_neutral", &"forklift_park", &"back", &"action"], "pad selector and exit verbs")
	router.clear_held()
	var input := {"throttle": 0.0, "brake": 0.0, "steer": 0.0, "lift": 0.0, "tilt": 0.0, "inch": 0.0, "horn": false}
	var operator := Operator.new()
	operator.enter()
	check(operator.sample(input) == [0.0, 0.0, 0.0, 0.0, 1.0], "enter neutral with parking brake")
	operator.handle_verb(&"forklift_forward")
	input.throttle = 1.0
	check(operator.sample(input)[0] == 0.0 and operator.ui_state().parking, "entry selection cannot lurch under held throttle")
	input.throttle = 0.0
	check(operator.sample(input)[4] == 0.0 and operator.ui_state().gear == 1, "released accelerator accepts forward and clears park")
	input.throttle = 1.0
	input.brake = 0.7
	check(operator.sample(input) == [1.0, 0.0, 0.0, 0.0, 0.7], "brake remains independent of throttle")
	operator.handle_verb(&"forklift_reverse")
	check(operator.sample(input, 1.0)[0] == 0.0 and operator.ui_state().gear == 0, "direction reversal queues neutral")
	input.throttle = 0.0
	operator.sample(input, 0.26)
	check(operator.ui_state().pending_direction == -1, "moving reversal waits for actual speed")
	operator.sample(input, 0.25)
	check(operator.ui_state().gear == -1 and operator.ui_state().pending_direction == 0, "stopped released reversal accepted")
	input.throttle = 1.0
	input.inch = 1.0
	input.brake = 0.0
	check(operator.sample(input)[0] == -0.25, "inching caps traction demand")
	input.throttle = 0.0
	check(operator.sample(input)[4] > 0.0, "inching blends brake for fine approach")
	input.inch = 0.0
	check(operator.sample(input)[4] == 0.0, "released accelerator coasts without applying service brake")
	operator.handle_verb(&"forklift_neutral")
	input.throttle = 1.0
	check(operator.sample(input)[0] == 0.0 and operator.ui_state().gear == 0, "neutral suppresses drive")
	operator.handle_verb(&"forklift_park")
	check(operator.sample(input)[4] == 1.0, "parking applies full brake")
	check(operator.sample({}) == [0.0, 0.0, 0.0, 0.0, 1.0], "invalid input brakes")
	operator.leave()
	check(operator.sample(input) == [0.0, 0.0, 0.0, 0.0, 1.0], "exit clears adapter")
	var ctx := {"hanging": false, "jump_ok": true, "action": {"id": &"done", "icon": &"done", "label": "DONE"}, "drop_ok": false, "chute_ok": false, "chute": false, "danger": 0.0, "operating": &"forklift", "crouched": false, "slung": false}
	for viewport_size in [Vector2i(2160, 1856), Vector2i(2316, 904)]:
		root.size = viewport_size
		await process_frame
		touch._layout()
		touch.update_context(ctx, 1.0)
		var controls := [&"forklift_forward", &"forklift_neutral", &"forklift_reverse", &"forklift_park", &"forklift_inch", &"forklift_horn", &"pendant_up", &"pendant_down", &"pendant_left", &"pendant_right", &"action", &"pause"]
		for id in controls:
			var button = touch._buttons[id]
			check(button.shown, "operator touch control visible " + String(id))
			check(touch._safe.encloses(Rect2(button.center - Vector2.ONE * button.radius * 1.22, Vector2.ONE * button.radius * 2.44)), "touch target inside viewport " + String(id))
			for other in controls:
				if id != other:
					check(button.center.distance_to(touch.button_center(other)) > (button.radius + touch._buttons[other].radius) * 1.22, "touch targets do not overlap " + String(id) + "/" + String(other))
		var home: Vector2 = touch.stick_home()
		finger(touch, 0, home)
		var drag := InputEventScreenDrag.new()
		drag.index = 0
		drag.position = home + Vector2(0.0, -150.0 * touch._u)
		drag.relative = Vector2(0.0, -150.0 * touch._u)
		touch.handle_touch(drag)
		check(router.frame(0.01).forklift.throttle > 0.99, "touch forward accelerates")
		drag.position = home + Vector2(0.0, 150.0 * touch._u)
		drag.relative = Vector2(0.0, 300.0 * touch._u)
		touch.handle_touch(drag)
		check(router.frame(0.01).forklift.brake > 0.99, "touch backward applies service brake")
		finger(touch, 1, touch._safe.position + touch._safe.size * Vector2(0.85, 0.3))
		drag.index = 1
		drag.position = touch._safe.position + touch._safe.size * Vector2(0.85, 0.3) + Vector2(30.0, 0.0)
		drag.relative = Vector2(30.0, 0.0)
		touch.handle_touch(drag)
		check(router.frame(0.01).look.x > 0.0, "right thumb looks independently while left brakes")
		router.clear_held()
		check(not touch.is_button_shown(&"jump") and not touch.is_button_shown(&"crouch"), "cab hides walking actions")
		finger(touch, 4, touch.button_center(&"pendant_left"))
		check(router.frame(0.01).forklift.tilt == 1.0, "touch back tilt positive")
		finger(touch, 5, touch.button_center(&"forklift_inch"))
		check(router.frame(0.01).forklift.inch == 1.0, "touch inch held independently")
		router.clear_held()
		check(router.frame(0.01).forklift.tilt == 0.0 and touch.forklift_inch == 0.0, "focus clears touch capture")
	router.set_forklift_mode(false)
	key(router, KEY_SPACE)
	key(router, KEY_W)
	frame = router.frame(0.01)
	check(frame.verbs == [&"jump"] and frame.move == Vector2(0.0, 1.0) and frame.forklift.is_empty(), "walking controls restored after cab exit")
	touch.queue_free()
	router.queue_free()
	print("SCRAPERX_FORKLIFT_OPERATOR_INPUTS ", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
