extends SceneTree

const Touch := preload("res://presentation/ui/touch_controls.gd")
const Router := preload("res://presentation/ui/input_router.gd")
const Gestures := preload("res://presentation/ui/thumb_parkour_gestures.gd")

var controls: Control
var router: Node
var checks := 0
var failures := 0


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_THUMB_CONTROLS FAIL " + why)


func context(extra: Dictionary = {}) -> void:
	var ctx := {"hanging": false, "climbing": false, "jump_ok": true,
		"grounded": true, "action": {"id": &"climb", "icon": &"climb", "label": "CLIMB"},
		"chute_ok": false, "chute": false, "danger": 0.0, "operating": &"",
		"crouched": false, "slung": false}
	ctx.merge(extra, true)
	controls.update_context(ctx, 0.0)


func down(index: int, at: Vector2) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = true
	root.push_input(event, true)


func up(index: int, at: Vector2, cancelled: bool = false) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.canceled = cancelled
	root.push_input(event, true)


func drag(index: int, from: Vector2, to: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = to
	event.relative = to - from
	root.push_input(event, true)


func frame(delta: float = 1.0 / 90.0) -> Dictionary:
	return router.frame(delta)


func arm(at: Vector2) -> void:
	down(1, at)
	frame(Gestures.ARM_SECONDS + 0.001)
	check(controls._thumb.phase == Gestures.Phase.COMMAND, "hold arms command mode")


func _run() -> void:
	root.size = Vector2i(1280, 900)
	controls = Touch.new()
	root.add_child(controls)
	router = Router.new()
	router.capture_mouse = false
	root.add_child(router)
	router.touch = controls
	controls.router = router
	context()
	var jump_at: Vector2 = controls.button_center(&"jump")
	down(1, jump_at)
	check(frame().verbs == [&"jump"], "Buttons preserves immediate Jump on press")
	up(1, jump_at)
	check(frame().verbs.is_empty(), "button lift never repeats Jump")
	controls.set_control_profile(1)
	check(not controls.is_button_shown(&"jump"), "profile switches hit areas immediately without a game tick")
	context()
	check(controls._thumb_active, "selected profile enters the on-foot surface")
	for id in [&"jump", &"action", &"crouch", &"drop"]:
		check(not controls.is_button_shown(id), "parkour buttons replaced in profile: " + String(id))
	check(controls.is_button_shown(&"pause"), "Pause remains accessible")
	var at: Vector2 = controls._safe.position + controls._safe.size * Vector2(0.75, 0.65)
	down(1, at)
	check(frame(0.04).verbs.is_empty(), "press begins candidate without guessing Jump")
	up(1, at)
	check(frame().verbs == [&"jump"], "short stationary right tap sends one Jump")
	check(frame().verbs.is_empty(), "tap is consumed once")
	down(1, at)
	var offset := Vector2(70, -30)
	drag(1, at, at + offset)
	var look := frame(0.03)
	check(look.verbs.is_empty() and look.look.is_equal_approx(offset * Router.LOOK_RADIANS_PER_UNIT),
		"look starts with full drag, without a command stroke or lost pixels")
	frame(1.0)
	drag(1, at + offset, at + Vector2(-180, 160))
	check(frame().verbs.is_empty() and controls._thumb.phase == Gestures.Phase.LOOK,
		"pausing during look never arms Jump, Crouch or Drop")
	up(1, at)
	check(frame().verbs.is_empty(), "look lift never jumps")
	arm(at)
	var reach_at := at + Vector2(0.0, -controls._thumb.stroke * 1.2)
	drag(1, at, reach_at)
	var reach := frame()
	check(reach.verbs == [&"action"] and reach.look == Vector2.ZERO, "up stroke sends Action without camera snap")
	drag(1, reach_at, reach_at + Vector2(30, 0))
	check(frame().look.x > 0.0, "same thumb resumes look after its command")
	up(1, reach_at)
	check(frame().verbs.is_empty(), "Action lift does not add Jump")
	arm(at)
	drag(1, at, at + Vector2(0, controls._thumb.stroke * 1.2))
	check(frame().verbs == [&"crouch"], "fan down sends stance once")
	up(1, at)
	frame()
	var left: Vector2 = controls.stick_home()
	down(0, left)
	frame(0.03)
	up(0, left)
	check(frame().verbs == [&"crouch"], "left stationary tap gives immediate stance without the fan")
	down(0, left)
	var forward := left + Vector2(0.0, -Touch.STICK_THROW * controls._u * 1.4)
	drag(0, left, forward)
	var movement := frame()
	check(movement.move.y > 0.99 and movement.sprint_held, "movement and overshoot sprint preserved")
	down(1, at)
	up(1, at)
	var moving_jump := frame()
	check(moving_jump.move.y > 0.99 and moving_jump.verbs == [&"jump"], "right tap preserves held left movement")
	drag(0, forward, left)
	up(0, left)
	check(frame().verbs.is_empty() and controls.move_vector == Vector2.ZERO,
		"returning a moved stick to rest and lifting never toggles stance")
	context({"grounded": false, "hanging": true, "single_hand_hold": true,
		"missing_hand_grip_available": true, "drop_ok": true,
		"action": {"id": &"reach_hand", "icon": &"climb", "label": "REACH HAND"}})
	down(1, at)
	up(1, at)
	check(frame().verbs == [&"action"], "solo-hand tap requests next grip instead of releasing")
	context({"grounded": false, "hanging": true, "swinging": true, "drop_ok": true})
	down(1, at)
	up(1, at)
	check(frame().verbs == [&"action"], "swing tap continues grip instead of releasing")
	arm(at)
	var inward := at + Vector2(-controls._thumb.stroke * 1.2, 0.0)
	drag(1, at, inward)
	frame(Gestures.DROP_HOLD_SECONDS * 0.5)
	up(1, inward)
	check(frame().verbs.is_empty(), "short inward stroke cannot Drop")
	arm(at)
	drag(1, at, inward)
	check(frame(Gestures.DROP_HOLD_SECONDS + 0.001).verbs.is_empty(), "ready Drop still waits for deliberate lift")
	up(1, inward)
	check(frame().verbs == [&"drop"], "guarded inward hold then lift releases exactly once")
	arm(at)
	drag(1, at, inward)
	frame(Gestures.DROP_HOLD_SECONDS + 0.001)
	drag(1, inward, at)
	up(1, at)
	check(frame().verbs.is_empty(), "moving away cancels ready Drop")
	arm(at)
	drag(1, at, inward)
	frame(Gestures.DROP_HOLD_SECONDS + 0.001)
	context({"grounded": false, "hanging": true, "drop_ok": true, "left_hand_generation": 8})
	up(1, inward)
	check(frame().verbs.is_empty(), "new grip invalidates a Drop armed on another contact")
	context({"drop_ok": true, "support_entity": 11})
	arm(at)
	drag(1, at, inward)
	frame(Gestures.DROP_HOLD_SECONDS + 0.001)
	context({"drop_ok": true, "support_entity": 1460})
	up(1, inward)
	check(frame().verbs.is_empty(), "stepping onto another support invalidates an old Drop gesture")
	context({"grounded": false, "hanging": true, "drop_ok": true})
	arm(at)
	drag(1, at, inward)
	frame(Gestures.DROP_HOLD_SECONDS + 0.001)
	up(1, inward, true)
	check(frame().verbs.is_empty(), "OS touch cancellation cannot commit ready Drop")
	arm(at)
	drag(1, at, inward)
	frame(Gestures.DROP_HOLD_SECONDS + 0.001)
	down(1, at)
	check(frame().verbs.is_empty(), "reused finger ID cancels prior ready Drop")
	up(1, at, true)
	frame()
	arm(at)
	drag(1, at, at + Vector2(controls._thumb.stroke * 1.2, 0))
	up(1, at)
	check(frame().verbs.is_empty(), "outward stroke cancels the fan without a tap")
	arm(at)
	router.clear_held()
	up(1, at)
	check(frame().verbs.is_empty() and controls._thumb.phase == Gestures.Phase.IDLE,
		"pause/focus reset forgets all pending gestures")
	down(1, at)
	controls.set_control_profile(0)
	context()
	up(1, at)
	check(frame().verbs.is_empty() and not controls._thumb_active and controls.is_button_shown(&"jump"),
		"profile switch cancels contact and restores Buttons")
	controls.set_control_profile(1)
	context({"operating": &"forklift"})
	check(not controls._thumb_active and controls.is_button_shown(&"forklift_forward"),
		"forklift keeps its dedicated control deck")
	context({"operating": &"grappling_gun", "grapple_equipped": true})
	check(not controls._thumb_active and controls.is_button_shown(&"action"),
		"equipped grapple keeps its explicit controls")
	context({"grounded": false, "chute_ok": true})
	check(controls._thumb_active and controls.is_button_shown(&"chute"), "free fall retains explicit Chute")
	down(1, at)
	controls._layout_in_safe_rect(Rect2(20, 10, 1180, 800))
	up(1, at)
	check(frame().verbs.is_empty(), "safe-area resize cancels contacts without a spurious tap")
	_rate_equivalence()
	print("SCRAPERX_THUMB_CONTROLS %s checks=%d" % ["PASS" if failures == 0 else "FAIL", checks])
	quit(0 if failures == 0 else 1)


func _rate_equivalence() -> void:
	for rate in [60, 90, 360]:
		var gesture := Gestures.new()
		gesture.begin(1, Vector2.ZERO, 1.0)
		for _i in int(ceil(0.2 * rate)):
			gesture.advance(1.0 / rate)
		gesture.drag(Vector2(-180, 0), Vector2(-180, 0))
		for _i in int(ceil(0.4 * rate)):
			gesture.advance(1.0 / rate)
		gesture.release(false)
		check(gesture.take_commands() == [&"drop"], "same timed gesture at %d caller FPS" % rate)
