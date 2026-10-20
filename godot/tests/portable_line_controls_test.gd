extends SceneTree

const Router := preload("res://presentation/ui/input_router.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
const LineView := preload("res://presentation/portable_traversal_line_view.gd")
const Main := preload("res://presentation/main.gd")
const Arms := preload("res://presentation/first_person_arms.gd")
# Records actual Main dispatch; it supplies no gameplay or fake success.
class CommandRecorder extends RefCounted:
	var commands: Array = []
	var inputs: Array = []
	func request_traversal_line(kind: int, target: int) -> bool:
		commands.append([kind,target])
		return true
	func set_traversal_line_reel_input(input: float) -> bool:
		inputs.append(input)
		return true

var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("PORTABLE_LINE_CONTROLS FAIL " + why)

func finger(router: Node, at: Vector2, down: bool, index: int = 7, cancelled: bool = false) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = down
	event.canceled = cancelled
	router._input(event)

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	var router := Router.new()
	var touch := Touch.new()
	root.add_child(router)
	root.add_child(touch)
	router.touch = touch
	router.capture_mouse = false
	touch.router = router
	var ctx := {"hanging": false, "jump_ok": true,
		"action": {"id": &"traversal_line_action", "icon": &"hook", "label": "INSTALL LINE", "detail": ""},
		"chute_ok": false, "chute": false, "danger": 0.0, "operating": &"",
		"slung": false, "crouched": false, "grapple_draw_available": true,
		"rigging_available": true, "traversal_line_available": true, "traversal_line_equipped": false,
		"traversal_line_gripping": false, "traversal_line_can_recover": false, "traversal": 0, "drop_ok": false}
	touch.update_context(ctx, 1.0)
	check(touch.is_button_shown(&"toggle_traversal_line"), "LINE accessible on ordinary foot context")
	finger(router, touch.button_center(&"toggle_traversal_line"), true)
	finger(router, touch.button_center(&"toggle_traversal_line"), false)
	check(router.frame(.016).verbs == [&"toggle_traversal_line"], "equip dispatches one ordered verb")
	ctx.operating = &"traversal_line"
	ctx.traversal_line_equipped = true
	touch.update_context(ctx, 1.0)
	check(not touch._movement_blocked, "carried line preserves movement stick")
	check(not touch.is_button_shown(&"pendant_up") and not touch.is_button_shown(&"pendant_down"), "no reel controls before real grip")
	finger(router, touch.button_center(&"action"), true)
	finger(router, touch.button_center(&"action"), false)
	check(router.frame(.016).verbs == [&"action"], "installation uses contextual Action")
	ctx.traversal_line_gripping = true
	ctx.traversal = 6
	ctx.drop_ok = true
	touch.update_context(ctx, 1.0)
	check(touch.is_button_shown(&"pendant_up") and touch.is_button_shown(&"pendant_down"), "real grip enables finite takeup and payout")
	check(touch.button_label(&"drop") == "LET GO" and not touch.is_button_shown(&"pack_traversal_line"), "release and recovery have distinct controls")
	finger(router, touch.button_center(&"pendant_up"), true)
	check(router.frame(.016).pendant.y == 1.0, "held up supplies positive reel command")
	finger(router, touch.stick_home(), true, 8)
	var drag := InputEventScreenDrag.new()
	drag.index = 8
	drag.position = touch.stick_home() + Vector2(0, -100)
	drag.relative = Vector2(0, -100)
	router._input(drag)
	var input := router.frame(.016)
	check(input.move.y > 0 and input.pendant.y == 1.0, "swing input and reel remain independent")
	finger(router, touch.stick_home(), false, 8)
	finger(router, touch.button_center(&"pendant_up"), false, 7, true)
	check(router.frame(.016).pendant == Vector2.ZERO, "touch cancellation clears reel")
	ctx.traversal_line_two_ended = true
	touch.update_context(ctx, 1.0)
	check(not touch._movement_blocked, "two-ended crossing retains movement stick")
	check(not touch.is_button_shown(&"pendant_up") and not touch.is_button_shown(&"pendant_down"), "fixed crossing hides inapplicable reel controls")
	ctx.traversal_line_two_ended = false
	touch.update_context(ctx, 1.0)
	finger(router, touch.button_center(&"drop"), true)
	finger(router, touch.button_center(&"drop"), false)
	check(router.frame(.016).verbs == [&"drop"], "ordinary Drop emits only release verb")
	ctx.traversal_line_gripping = false
	ctx.traversal_line_can_recover = true
	ctx.traversal = 0
	ctx.drop_ok = false
	touch.update_context(ctx, 1.0)
	check(touch.is_button_shown(&"pack_traversal_line") and not touch.is_button_shown(&"pendant_up"), "unloaded native recovery exposes PACK independently")
	finger(router, touch.button_center(&"pack_traversal_line"), true)
	finger(router, touch.button_center(&"pack_traversal_line"), false)
	check(router.frame(.016).verbs == [&"pack_traversal_line"], "PACK has its own recovery verb")
	ctx.traversal_line_gripping = true
	touch.update_context(ctx, 1.0)
	check(touch.is_button_shown(&"pack_traversal_line"), "native settled unloaded hand grip permits packing")
	ctx.traversal_line_gripping = false
	for viewport in [Vector2i(968,2376),Vector2i(2376,968),Vector2i(1856,2160),Vector2i(2160,1856)]:
		root.size = viewport
		for scale in [.8,1.0,1.3]:
			touch.set_touch_scale(scale)
			touch.update_context(ctx, 1.0)
			for id in [&"toggle_traversal_line", &"pack_traversal_line"]:
				var button = touch._buttons[id]
				var hit: float = button.radius * touch.HIT_SLOP
				check(touch._safe.encloses(Rect2(button.center - Vector2.ONE * hit, Vector2.ONE * hit * 2)), "line control stays in safe area")
				for other in touch._buttons.values():
					if other == button or not other.shown: continue
					check(button.center.distance_to(other.center) > (button.radius + other.radius) * touch.HIT_SLOP, "line control avoids " + str(other.id))
	ctx.traversal_line_gripping = true
	ctx.traversal_line_can_recover = false
	touch.update_context(ctx, 1.0)
	finger(router, touch.button_center(&"pendant_down"), true)
	check(router.frame(.016).pendant.y == -1, "held down pays out")
	ctx.traversal_line_equipped = false
	ctx.operating = &""
	touch.update_context(ctx, .016)
	check(router.frame(.016).pendant == Vector2.ZERO, "tool change cancels held reel")
	router.set_forklift_mode(true)
	router.push_verb(&"toggle_traversal_line")
	router.push_verb(&"pack_traversal_line")
	check(router.frame(.016).verbs.is_empty(), "machine input excludes line tool commands")
	var main := Main.new()
	var recorder := CommandRecorder.new()
	main._native = recorder
	main._ctx = {"traversal_line_equipped": true, "traversal_line_can_recover": false,
		"traversal_line_target": 731, "action": {"id": &"traversal_line_action"}}
	main._dispatch([&"action"], .016)
	check(recorder.commands == [[2,731]], "shipping Main Action sends exact shown target")
	recorder.commands.clear()
	main._dispatch([&"drop"], .016)
	check(recorder.commands == [[3,0]] and recorder.inputs.back() == 0.0, "shipping Drop releases grip and zeroes reel without packing")
	recorder.commands.clear()
	main._dispatch([&"pack_traversal_line"], .016)
	check(recorder.commands.is_empty(), "shipping PACK respects native recovery permission")
	main._ctx.traversal_line_can_recover = true
	main._dispatch([&"pack_traversal_line"], .016)
	check(recorder.commands == [[4,0]], "shipping PACK sends recovery only")
	main._native = null
	main.free()
	var view := LineView.new()
	root.add_child(view)
	var camera := Camera3D.new()
	root.add_child(camera)
	view.configure(camera)
	var state := {"equipped": true, "installed": false, "target_valid": true, "target_point": Vector3(1,2,3)}
	view.update_state(state, null)
	check(view._target.visible and not view._fitting.visible, "preview represents target only")
	state.available_spools = 1
	view.update_state(state, null)
	check(view._carried.visible, "actual available stock shows carried spool")
	state = {"equipped": false, "installed": true, "anchor_point": Vector3(0,2,0), "grip_point": Vector3(0,1,0),
		"fitting_rotation": Quaternion(Vector3.UP, .7), "generation": 4}
	state.path_valid = true
	state.path_points = PackedVector3Array([Vector3(0,1,0), Vector3(.2,1.5,0), Vector3(0,2,0)])
	view.update_state(state, null)
	check(view._line.get_surface_count() == 1 and not view._carried.visible, "native routed points draw one cable without duplicate carried stock")
	check(view._fitting.visible and not view._target.visible and view._fitting.position == state.grip_point, "stowed tool keeps actual installed fitting visible")
	check(view._fitting.basis.is_equal_approx(Basis(state.fitting_rotation)) and view._generation == 4, "fitting uses native rotation and generation")
	state.broken = true
	view.update_state(state, null)
	check(view._line.get_surface_count() == 0 and view._fitting.material_override == view._broken, "broken row has distinct fitting and no false cable support")
	var arms := Arms.new()
	root.add_child(arms)
	var material := StandardMaterial3D.new()
	arms.build(material,material,material,material,material,material,material,material,material,material)
	var held := {"traversal": 6, "progress": 0.0, "position": Vector3.ZERO, "render_position": Vector3.ZERO,
		"velocity": Vector3.ZERO, "grounded": false, "affordance": false, "ledge_point": Vector3(0,.8,-.25),
		"structure_normal": Vector3.FORWARD, "operating": &"traversal_line", "chute": false,
		"hand_left": Vector3(.12,.8,-.25), "hand_right": Vector3(-.12,.8,-.25),
		"landing": {"traversal_hand_constraint_count": 2, "left_hand_attached": true, "right_hand_attached": true}}
	var original := held.duplicate(true)
	for i in 60: arms.update_arms(held,Transform3D(Basis.IDENTITY,Vector3(0,.65,.3)),1.0/90)
	check(arms._hands.all(func(hand): return hand.anchored and hand.pose == Arms.POSE_GRIP), "line swing arms use supplied planted-hand pose")
	check(not arms._remote.visible and held == original, "line tool adds no machine remote or manufactured native state")
	held.landing.traversal_hand_constraint_count = 1
	held.landing.right_hand_attached = false
	arms.update_arms(held,Transform3D.IDENTITY,1.0/90)
	check(arms._hands.filter(func(hand): return hand.anchored).size() == 1, "absent native hand gets no planted pose")
	held.traversal = 0
	held.operating = &""
	arms.update_arms(held,Transform3D.IDENTITY,1.0/90)
	check(arms._hands.all(func(hand): return not hand.anchored), "release returns ordinary unanchored arms")
	arms.free()
	view.free()
	camera.free()
	touch.free()
	router.free()
	print("PORTABLE_LINE_CONTROLS checks=",checks," failures=",failures," fixture_presentation_only=1")
	quit(0 if failures == 0 else 1)
