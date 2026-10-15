extends SceneTree

const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
var failures: Array[String] = []

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	if not ok:
		failures.append(message)
		push_error("GRAPPLE_EQUIPMENT_TOUCH FAIL " + message)

func context() -> Dictionary:
	return {"hanging": false, "jump_ok": true, "action": {"id": &"climb", "icon": &"climb", "label": "CLIMB"},
		"chute_ok": false, "chute": false, "danger": 0.0, "operating": &"", "slung": false,
		"crouched": false, "grapple_draw_available": true, "grapple_equipped": false}

func touch(router: Node, index: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	router._input(event)

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	var controls := TouchControls.new()
	var router := InputRouter.new()
	root.add_child(controls)
	root.add_child(router)
	router.capture_mouse = false
	router.touch = controls
	controls.router = router
	var ctx := context()
	controls.update_context(ctx, 1.0)
	check(controls._buttons.has(&"grapple"), "dedicated grapple control exists alongside contextual Action")
	if not controls._buttons.has(&"grapple"):
		quit(1)
		return
	var button = controls._buttons[&"grapple"]
	check(button.shown and button.enabled and button.label == "DRAW", "draw availability shown")
	touch(router, 0, button.center, true)
	check(router.frame(0.016)["verbs"] == [&"toggle_grapple"], "competing climb Action does not mask grapple draw")
	for i in range(10):
		controls.update_context(ctx, 0.016)
		check(router.frame(0.016)["verbs"].is_empty(), "held draw never repeats")
	check(button.label == "DRAW" and not ctx["grapple_equipped"], "draw press waits for native acknowledgement")
	touch(router, 0, button.center, false)
	touch(router, 1, controls.button_center(&"action"), true)
	check(router.frame(0.016)["verbs"] == [&"action"], "contextual Action remains separate")
	touch(router, 1, controls.button_center(&"action"), false)
	ctx["grapple_equipped"] = true
	ctx["grapple_draw_available"] = false
	ctx["grapple_line_attached"] = true
	ctx["drop_ok"] = true
	controls.update_context(ctx, 1.0)
	check(button.shown and button.enabled and button.label == "STOW", "native equipped state exposes deliberate stow")
	check(controls.button_label(&"drop") == "RELEASE", "attached Drop still releases line")
	touch(router, 2, button.center, true)
	check(router.frame(0.016)["verbs"] == [&"toggle_grapple"], "stow uses equipment verb even with attached line")
	ctx["grapple_equipped"] = false
	controls.update_context(ctx, 1.0)
	check(not button.shown and not button.enabled and button.index == -1, "native stow acknowledgement clears unavailable held capture")
	touch(router, 3, button.center, true)
	check(router.frame(0.016)["verbs"].is_empty() and button.index == -1, "hidden grapple cannot capture or dispatch")
	router.clear_held()
	ctx["grapple_draw_available"] = true
	controls.update_context(ctx, 1.0)
	button.enabled = false
	touch(router, 4, button.center, true)
	check(router.frame(0.016)["verbs"].is_empty() and button.index == -1, "disabled grapple cannot capture or dispatch")
	router.clear_held()
	controls.update_context(ctx, 1.0)
	touch(router, 5, button.center, true)
	router.frame(0.016)
	button.enabled = false
	# update_context's generic cleanup must release disabled controls too.
	controls._buttons[&"action"].index = 6
	ctx["action"]["id"] = &""
	controls.update_context(ctx, 0.016)
	check(controls._buttons[&"action"].index == -1, "disabled contextual capture is released")
	router.clear_held()
	router.gameplay_active = false
	touch(router, 7, button.center, true)
	check(router.frame(0.016)["verbs"].is_empty() and button.index == -1, "paused router suppresses draw")
	router.gameplay_active = true
	# Check actual expanded hit circles at both Fold panels/orientations and
	# every supported slider step. Existing controls keep their own layout.
	var layouts := 0
	for viewport in [Vector2i(968, 2376), Vector2i(2376, 968), Vector2i(1856, 2160), Vector2i(2160, 1856)]:
		root.size = viewport
		controls._layout()
		for step in range(11):
			controls.set_touch_scale(0.8 + step * 0.05)
			for station in [&"", &"grappling_gun", &"intake", &"jib", &"service_lift"]:
				ctx["operating"] = station
				ctx["grapple_equipped"] = true
				ctx["chute_ok"] = true
				controls.update_context(ctx, 1.0)
				var hit: float = button.radius * controls.HIT_SLOP
				check(controls._safe.has_point(button.center - Vector2.ONE * hit) and controls._safe.has_point(button.center + Vector2.ONE * hit), "grapple hit circle stays within safe area")
				# HUD altimeter is scaled independently of the touch-size setting.
				var hud_u: float = controls.UiStyle.unit(controls)
				var hud_reserved := Rect2(Vector2(controls._safe.end.x - 394.0 * hud_u,
					controls._safe.position.y + 48.0 * hud_u), Vector2(356.0, 166.0) * hud_u)
				var hit_bounds := Rect2(button.center - Vector2.ONE * hit, Vector2.ONE * hit * 2.0)
				check(not hit_bounds.intersects(hud_reserved), "grapple expanded hit avoids HUD altimeter")
				var font: Font = controls.UiStyle.font_label()
				var font_size := int(roundf(25.0 * controls._u))
				var outline := int(roundf(6.0 * controls._u))
				for label in ["DRAW", "STOW"]:
					var width := font.get_string_size(label, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
					var height := font.get_ascent(font_size) + font.get_descent(font_size)
					var label_center: Vector2 = button.center + Vector2(0.0, button.radius + 30.0 * controls._u)
					var label_bounds := Rect2(label_center - Vector2(width, height) * 0.5, Vector2(width, height)).grow(outline)
					check(not label_bounds.intersects(hud_reserved), "grapple " + label + " label avoids HUD altimeter")
					check(controls._safe.encloses(label_bounds), "grapple label stays within safe area")
				for other in controls._buttons.values():
					if other == button or not other.shown:
						continue
					check(button.center.distance_to(other.center) > (button.radius + other.radius) * controls.HIT_SLOP, "grapple hit circle avoids " + str(other.id) + " at " + str(viewport))
				layouts += 1
	controls.queue_free()
	router.queue_free()
	if failures.is_empty():
		print("GRAPPLE_EQUIPMENT_TOUCH PASS real touch/router, contextual independence, native acknowledgement, capture cleanup, pause; ", layouts, " viewport/scale/station layouts")
	quit(0 if failures.is_empty() else 1)
