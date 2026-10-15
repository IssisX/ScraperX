extends SceneTree

# One diagnostic approach stage, then ordinary router movement and Jump.
# This proves bridge/arms alternation only, not the full native topout route.
const STEP := 1.0 / 90.0
var main: Node
var native: Object
var checks := 0
var failures := 0
var capture_dir := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("ALTERNATING_SHIMMY_BRIDGE FAIL " + message)

func axis(value: float, index: JoyAxis = JOY_AXIS_LEFT_Y) -> void:
	var event := InputEventJoypadMotion.new()
	event.device = 0
	event.axis = index
	event.axis_value = value
	root.push_input(event, true)

func step(count: int = 1) -> void:
	for i in count:
		main._process(STEP)

func hands() -> Dictionary:
	return {"left": native.get_traversal_left_hand(),
		"right": native.get_traversal_right_hand(),
		"landing": native.get_landing_state()}

func wrists_reach() -> bool:
	var arms: Node = main._arms
	var state: Dictionary = main._arms_state({"move": Vector2.ZERO, "pendant": Vector2.ZERO})
	for hand in arms._hands:
		var hold: Vector3 = state.hand_left if hand.side < 0.0 else state.hand_right
		var expected: Vector3 = hold - arms._grip_forward * 0.154 + Vector3.DOWN * 0.074
		if hand.root.global_position.distance_to(expected) > 0.025:
			return false
	return true

func capture(label: String) -> void:
	if capture_dir.is_empty():
		return
	await process_frame
	await RenderingServer.frame_post_draw
	var path := capture_dir.path_join(label + ".png")
	check(root.get_texture().get_image().save_png(path) == OK, "actual renderer capture " + label)

func _run() -> void:
	if not capture_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(capture_dir)
	root.size = Vector2i(864, 742)
	main = load("res://main.tscn").instantiate()
	main._settings_persistent = false
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene and not main._ci_mode, "shipping Main/native world")
	check(native.debug_restart_at(Vector3(15.1, 104.4, -180.6)), "single existing 1901 approach stage")
	step(45)
	check(native.is_player_grounded() and int(native.get_support_entity_id()) == 1901, "native actual shelf footing")
	main._yaw = PI * 0.5
	axis(-0.57)
	main._router.push_verb(&"jump")
	var caught := false
	for i in 180:
		step()
		if int(native.get_traversal_state()) == 1:
			caught = true
			break
	check(caught, "ordinary Jump and forward input catch real lip")
	if not caught:
		print("ALTERNATING_SHIMMY_BRIDGE FAIL no native hang")
		quit(1)
		return
	axis(0.0)
	step(36)
	var initial := hands()
	check(int(initial.landing.traversal_hand_constraint_count) == 2 and not bool(initial.landing.foot_push_active), "actual two-hand hang without foot")
	check(wrists_reach(), "both rendered wrist meshes reach their native hang anchors")
	var arms: Node = main._arms
	var shoulder_axis: Vector3 = arms._hang_shoulder_right
	var before_span: Vector3 = initial.right - initial.left
	await capture("before-crossing")
	axis(1.0, JOY_AXIS_LEFT_X)
	var crossed := false
	var alternating := true
	var planted := true
	var shoulder_preserved := true
	var changes := 0
	for i in 90:
		var before := hands()
		step()
		var after := hands()
		var left_changed := int(before.landing.left_hand_generation) != int(after.landing.left_hand_generation)
		var right_changed := int(before.landing.right_hand_generation) != int(after.landing.right_hand_generation)
		if left_changed or right_changed:
			changes += 1
			alternating = alternating and left_changed != right_changed
			if not left_changed:
				planted = planted and (after.left as Vector3).distance_to(before.left) < 0.00001
			if not right_changed:
				planted = planted and (after.right as Vector3).distance_to(before.right) < 0.00001
			print("SHIMMY_RENDER_REGRIP tick=", native.get_tick_index(), " left=", left_changed, " right=", right_changed)
		shoulder_preserved = shoulder_preserved and arms._hang_shoulder_right.dot(shoulder_axis) > 0.99
		if (after.right - after.left).dot(before_span) < 0.0:
			crossed = true
			break
	axis(0.0, JOY_AXIS_LEFT_X)
	step(12)
	check(changes > 0 and alternating, "bridge reports one hand generation change at each observed transfer")
	check(planted, "nonmoving hand retains exact planted native world anchor")
	check(crossed, "ordinary lateral input crosses native grip order")
	check(shoulder_preserved and arms._hang_shoulder_right.dot(shoulder_axis) > 0.99, "native grip crossing preserves rendered shoulder handedness")
	check(wrists_reach(), "crossed rendered wrists still reach their own native anchors")
	var wrist_before: Array = arms._hands.map(func(hand) -> Vector3: return hand.root.global_position)
	var anchors_before := hands()
	main._yaw += 1.2
	main._pitch = 0.35
	main._process(0.0)
	check(arms._hang_shoulder_right.dot(shoulder_axis) > 0.99, "head turn cannot exchange shoulders")
	var head_quiet := true
	for i in 2:
		head_quiet = head_quiet and arms._hands[i].root.global_position.distance_to(wrist_before[i]) < 0.00001
	var anchors_after := hands()
	check(head_quiet and anchors_after.left == anchors_before.left and anchors_after.right == anchors_before.right,
		"head-only zero-tick turn leaves actual and rendered grip anchors fixed")
	main._yaw = PI * 0.5
	main._pitch = 0.12
	main._process(0.0)
	await capture("after-crossing")
	check(int(native.get_traversal_state()) == 1 and native.get_death_count() == 0, "presentation observation remains live native hang without recovery")
	print("ALTERNATING_SHIMMY_BRIDGE %s checks=%d native_tick=%d diagnostic_stage=1 phone_play=0" % ["FAIL" if failures else "PASS", checks, native.get_tick_index()])
	quit(1 if failures else 0)
