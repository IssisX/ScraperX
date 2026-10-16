extends SceneTree

const Router := preload("res://presentation/ui/input_router.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
const View := preload("res://presentation/yard_crane_view.gd")
const CranePanel := preload("res://presentation/ui/yard_crane_panel.gd")
var failed := false

func check(ok: bool, label: String) -> void:
	if not ok:
		failed = true
		push_error("YARD CRANE OPERATOR: " + label)

func key(router: Node, code: Key, pressed := true) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	router._input(event)

func axis(router: Node, code: JoyAxis, value: float) -> void:
	var event := InputEventJoypadMotion.new()
	event.device = 0
	event.axis = code
	event.axis_value = value
	router._input(event)

func button(router: Node, code: JoyButton) -> void:
	var event := InputEventJoypadButton.new()
	event.device = 0
	event.button_index = code
	event.pressed = true
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
	router.set_yard_crane_mode(true)
	check(router.frame(0.01).yard_crane == Vector3.ZERO, "entry releases approach input")
	for code in [KEY_D, KEY_W, KEY_UP, KEY_SPACE, KEY_C, KEY_T, KEY_B, KEY_SHIFT, KEY_CTRL]:
		key(router, code)
	var frame: Dictionary = router.frame(0.01)
	check(frame.yard_crane == Vector3.ONE, "three independent keyboard machine axes")
	check(frame.move == Vector2.ZERO and frame.pendant == Vector2.ZERO and frame.reel == 0.0 and not frame.sprint_held and not frame.crouch_held and frame.verbs.is_empty(), "walking, grapple and jump suppressed")
	for code in [KEY_G, KEY_R, KEY_E, KEY_Q]:
		key(router, code)
	check(router.frame(0.01).verbs == [&"yard_crane_attach", &"yard_crane_detach", &"action", &"back"], "rig and leave keyboard verbs")
	router.set_yard_crane_mode(true)
	check(router.frame(0.01).yard_crane == Vector3.ONE, "unchanged mode preserves intentional held input")
	router.clear_held()
	check(router.frame(0.01).yard_crane == Vector3.ZERO, "focus clear neutralizes all axes")
	axis(router, JOY_AXIS_LEFT_X, -1.0)
	axis(router, JOY_AXIS_LEFT_Y, -1.0)
	axis(router, JOY_AXIS_TRIGGER_RIGHT, 1.0)
	axis(router, JOY_AXIS_RIGHT_X, 0.8)
	frame = router.frame(0.01)
	check(frame.yard_crane.x < -0.6 and frame.yard_crane.y > 0.6 and frame.yard_crane.z == 1.0, "radial stick and separate trigger hoist")
	check(frame.look.x > 0 and frame.move == Vector2.ZERO, "right stick freelook survives mode isolation")
	for code in [JOY_BUTTON_A, JOY_BUTTON_Y, JOY_BUTTON_X, JOY_BUTTON_B, JOY_BUTTON_RIGHT_SHOULDER]:
		button(router, code)
	check(router.frame(0.01).verbs == [&"yard_crane_attach", &"yard_crane_detach", &"action", &"back"], "pad rig / unrig / leave without tool verbs")
	router.clear_held()
	var ctx := {"operating": &"yard_crane", "hanging": false, "climbing": false, "lowering": false,
		"grounded": true, "jump_ok": true, "chute_ok": false, "danger": 0.0, "crouched": false, "chute": false, "slung": false, "carrying": 0,
		"action": {"id": &"done", "label": "DONE", "icon": &"done", "detail": ""}, "drop_ok": false}
	touch.update_context(ctx, 1.0)
	check(not touch.is_button_shown(&"yard_crane_attach") and not touch.is_button_shown(&"yard_crane_detach") and not touch.is_button_shown(&"jump"), "cab touch exposes no unreachable remote rigging")
	var home: Vector2 = touch.stick_home()
	finger(touch, 3, home)
	var drag := InputEventScreenDrag.new()
	drag.index = 3
	drag.position = home + Vector2(100, -100) * touch._u
	drag.relative = Vector2(100, -100) * touch._u
	touch.handle_touch(drag)
	frame = router.frame(0.01)
	check(frame.yard_crane.x > 0.1 and frame.yard_crane.y > 0.1 and frame.move == Vector2.ZERO, "touch stick routes both machine axes without walking")
	finger(touch, 3, home, false)
	finger(touch, 1, touch.button_center(&"pendant_up"))
	frame = router.frame(0.01)
	check(frame.yard_crane.z == 1.0 and frame.verbs.is_empty(), "actual cab touch holds hoist without remote rigging")
	finger(touch, 1, Vector2.ZERO, false)
	check(router.frame(0.01).yard_crane.z == 0.0, "hoist touch releases")
	router.gameplay_active = false
	check(router.frame(0.01).yard_crane == Vector3.ZERO, "paused gameplay emits neutral machine intent")
	router.clear_held()
	router.gameplay_active = true
	router.set_yard_crane_mode(false)
	check(router.frame(0.01).yard_crane == Vector3.ZERO, "leaving clears commands")
	key(router, KEY_W)
	check(router.frame(0.01).move.y == 1.0, "walking returns after cab exit")
	var view := View.new()
	root.add_child(view)
	check(view.get_operator_hand_targets().is_empty(), "missing native pose invents no cab")
	var state := {"available": true, "operator_left_hand": Vector3(2, 1, 3), "operator_right_hand": Vector3(2, 1, 2),
		"operator_forward": Vector3.RIGHT, "lines": [{"from": Vector3(0, 3, 0), "to": Vector3.ZERO, "attached": true}], "attached_mask": 3}
	view.render_state(state)
	check(view.get_operator_hand_targets().left_hand_world == Vector3(2, 1, 3), "native world hand anchor preserved")
	check(view._cables.multimesh.instance_count == 1 and view._knobs.multimesh.instance_count == 2, "bounded native cable and knob groups")
	state.hoist_target_mps = -0.35
	view.render_state(state)
	var grips := view.get_operator_hand_targets()
	check(grips.right_hand_world.x < state.operator_right_hand.x and grips.left_hand_world.distance_to(state.operator_left_hand) < 0.00001, "accepted hoist-up pulls only right control toward operator")
	# Dummy rendering does not retain MultiMesh instance transforms. Keep this
	# geometry readback assertion for the rendered run; target checks run in both.
	if DisplayServer.get_name() != "headless":
		check(view._knobs.multimesh.get_instance_transform(1).origin.distance_to(grips.right_hand_world) < 0.00001, "visible control and operating hand share one pose")
	state.slew_target_rad_s = 0.05
	state.trolley_target_mps = 0.25
	view.render_state(state)
	grips = view.get_operator_hand_targets()
	check(grips.left_hand_world.x > state.operator_left_hand.x and grips.left_hand_world.z > state.operator_left_hand.z and grips.left_hand_world.distance_to(state.operator_left_hand) <= 0.02, "left control shows both native axes within short physical travel")
	state.hoist_target_mps = 0.0
	state.slew_target_rad_s = 0.0
	state.trolley_target_mps = 0.0
	view.render_state(state)
	grips = view.get_operator_hand_targets()
	check(grips.left_hand_world.distance_to(state.operator_left_hand) < 0.00001 and grips.right_hand_world.distance_to(state.operator_right_hand) < 0.00001, "released native rates return hands and controls exactly to neutral")
	check(CranePanel.describe(state, 0).rows[0][1] == "2 / 4 LEGS", "rig count reflects actual mask only")
	check("X RIG" in CranePanel.describe(state, 1).help_lines[0], "pad help identifies on-foot contextual rigging")
	print("YARD_CRANE_OPERATOR_INPUTS_", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
