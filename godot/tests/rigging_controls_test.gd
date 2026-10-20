extends SceneTree

const Router := preload("res://presentation/ui/input_router.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
const Hud := preload("res://presentation/ui/hud.gd")
var failures: Array[String] = []

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, why: String) -> void:
	if not value:
		failures.append(why)
		push_error("RIGGING_CONTROLS FAIL " + why)

func finger(router: Node, at: Vector2, pressed: bool, cancelled: bool = false, index: int = 7) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	event.canceled = cancelled
	router._input(event)

func context() -> Dictionary:
	return {"hanging": false, "jump_ok": true, "action": {"id": &"rigging", "icon": &"hook", "label": "INSTALL", "detail": ""},
		"chute_ok": false, "chute": false, "danger": 0.0, "operating": &"rigging", "slung": false,
		"crouched": false, "grapple_draw_available": true, "grapple_equipped": false,
		"rigging_available": true, "rigging_equipped": true, "rigging_draft": true,
		"rigging_selected": 123, "rigging_tension_n": 100.0, "rigging_cut_progress": .5,
		"grab_hint": false, "traversal": 0}

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	var router := Router.new()
	var touch := Touch.new()
	root.add_child(router)
	root.add_child(touch)
	router.touch = touch
	router.capture_mouse = false
	touch.router = router
	router.set_rigging_mode(true)
	var ctx := context()
	touch.update_context(ctx, 1.0)
	check(touch.is_button_shown(&"toggle_rigging") and touch.is_button_shown(&"rigging_cut"), "acknowledged rig exposes equipment and contextual controls")
	check(not touch._movement_blocked and not touch.is_button_shown(&"pendant_up") and not touch.is_button_shown(&"pendant_down"), "rigging station retains movement without machine pendant")
	check(touch.is_button_shown(&"grapple") and touch.is_button_shown(&"action"), "grapple and installation remain independently accessible")
	for entry in [[&"toggle_rigging", &"toggle_rigging"], [&"rigging_undo", &"rigging_undo"], [&"rigging_unclip", &"rigging_unclip"], [&"grapple", &"toggle_grapple"]]:
		var at: Vector2 = touch.button_center(entry[0])
		finger(router, at, true)
		finger(router, at, false)
		check(router.frame(.016).verbs == [entry[1]], "touch discrete verb " + str(entry[1]))
	var cut: Vector2 = touch.button_center(&"rigging_cut")
	finger(router, cut, true)
	check(router.frame(.016).verbs == [&"rigging_cut_start"], "touch cut press dispatches start only")
	check(router.frame(.5).verbs.is_empty(), "held touch cut never repeats or invents completion")
	finger(router, touch.stick_home(), true, false, 8)
	var move_drag := InputEventScreenDrag.new()
	move_drag.index = 8
	move_drag.position = touch.stick_home() + Vector2(0,-100)
	move_drag.relative = Vector2(0,-100)
	router._input(move_drag)
	finger(router, Vector2(1200,900), true, false, 9)
	var look_drag := InputEventScreenDrag.new()
	look_drag.index = 9
	look_drag.position = Vector2(1240,920)
	look_drag.relative = Vector2(40,20)
	router._input(look_drag)
	var touch_frame := router.frame(.016)
	check(touch_frame.move.y > 0 and touch_frame.look.length() > 0 and touch_frame.verbs.is_empty(), "independent touch thumbs walk and look while cut held")
	finger(router, touch.stick_home(), false, false, 8)
	finger(router, Vector2(1240,920), false, false, 9)
	finger(router, cut, false, true)
	check(router.frame(.016).verbs == [&"rigging_cut_stop"], "canceled touch stops cut")
	finger(router, cut, true)
	router.frame(.016)
	var drag := InputEventScreenDrag.new()
	drag.index = 7
	drag.position = cut + Vector2(300,0)
	drag.relative = Vector2(300,0)
	router._input(drag)
	check(router.frame(.016).verbs == [&"rigging_cut_stop"], "drag off cut target cancels without look or install")
	finger(router, cut, false)
	finger(router, cut, true)
	router.frame(.016)
	router.clear_held()
	check(router.frame(.016).verbs == [&"rigging_cut_stop"], "focus/reset clears held cut")
	finger(router, cut, true)
	router.frame(.016)
	ctx.rigging_equipped = false
	router.set_rigging_mode(false)
	touch.update_context(ctx, .016)
	check(router.frame(.016).verbs == [&"rigging_cut_stop"] and not touch.is_button_shown(&"rigging_cut"), "unequip cancels and hides cut")
	router.set_forklift_mode(true)
	ctx.operating = &"forklift"
	touch.update_context(ctx, 1.0)
	check(not touch.is_button_shown(&"toggle_rigging") and not touch.is_button_shown(&"rigging_cut"), "machine station hides rig tool targets")
	router.set_forklift_mode(false)
	ctx.operating = &"rigging"
	router.set_rigging_mode(true)
	ctx.rigging_equipped = true
	var layouts := 0
	for viewport in [Vector2i(968,2376),Vector2i(2376,968),Vector2i(1856,2160),Vector2i(2160,1856)]:
		root.size = viewport
		for scale in [.8,1.0,1.3]:
			touch.set_touch_scale(scale)
			touch._layout()
			touch.update_context(ctx, 1.0)
			for id in [&"toggle_rigging", &"rigging_undo", &"rigging_unclip", &"rigging_cut"]:
				var button = touch._buttons[id]
				var hit: float = button.radius * touch.HIT_SLOP
				check(touch._safe.encloses(Rect2(button.center-Vector2.ONE*hit, Vector2.ONE*hit*2)), "rig hit target within safe area " + str(id))
				for other in touch._buttons.values():
					if other == button or not other.shown: continue
					check(button.center.distance_to(other.center) > (button.radius+other.radius)*touch.HIT_SLOP, "rig hit target avoids " + str(other.id))
			layouts += 1
	touch.set_control_profile(1)
	touch.update_context(ctx, 1.0)
	check(touch.is_button_shown(&"rigging_cut") and touch.is_button_shown(&"toggle_rigging"), "thumb-flow profile retains separate rig controls")
	check(touch._thumb_active and not touch._movement_blocked, "acknowledged rigging station preserves thumb-flow locomotion")
	ctx.operating = &""
	touch.update_context(ctx, .016)
	check(touch.is_button_shown(&"rigging_cut") and not touch._movement_blocked, "empty station context also retains rig controls")
	ctx.operating = &"rigging"
	var hud := Hud.new()
	var prompts: Array = hud._build_prompts(ctx)
	check(prompts.any(func(p): return p.verb == &"rigging_cut_start"), "HUD exposes deliberate cut binding")
	hud.free()
	touch.queue_free()
	router.queue_free()
	if failures.is_empty(): print("RIGGING_CONTROLS PASS touch edges, cut cancellation, walking, machine visibility, ", layouts, " safe layouts")
	quit(0 if failures.is_empty() else 1)
