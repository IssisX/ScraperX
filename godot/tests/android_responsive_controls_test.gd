extends SceneTree

const Touch := preload("res://presentation/ui/touch_controls.gd")
const Router := preload("res://presentation/ui/input_router.gd")
const Hud := preload("res://presentation/ui/hud.gd")
const ForkliftPanel := preload("res://presentation/ui/forklift_panel.gd")
const UiStyle := preload("res://presentation/ui/ui_style.gd")
var failures := 0
var layouts := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		if failures <= 30:
			push_error("ANDROID_RESPONSIVE " + message)

func context(station: StringName) -> Dictionary:
	return {"hanging": false, "jump_ok": true,
		"action": {"id": &"done", "icon": &"done", "label": "DONE"},
		"drop_ok": true, "chute_ok": true, "chute": false, "danger": 0.0,
		"operating": &"" if station == &"recovery" else station, "slung": false, "crouched": false,
		"grapple_equipped": station != &"recovery", "grapple_line_attached": station != &"recovery",
		"slingshot_recovering": station == &"recovery"}

func finger(controls: Control, index: int, at: Vector2, pressed := true) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	controls.handle_touch(event)

func drag(controls: Control, index: int, origin: Vector2, delta: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = origin + delta
	event.relative = delta
	controls.handle_touch(event)

func _run() -> void:
	var controls := Touch.new()
	var router := Router.new()
	var hud := Hud.new()
	root.add_child(controls)
	root.add_child(router)
	root.add_child(hud)
	controls.router = router
	router.touch = controls
	router.capture_mouse = false
	# Window sizes include compact/tall phones, tablet and both Fold panels.
	for portrait in [Vector2i(360, 640), Vector2i(390, 844), Vector2i(800, 1280),
			Vector2i(1080, 2400), Vector2i(968, 2376), Vector2i(1856, 2160)]:
		for viewport in [portrait, Vector2i(portrait.y, portrait.x)]:
			root.size = viewport
			await process_frame
			for scale in [0.8, 1.0, 1.3]:
				controls.set_touch_scale(scale)
				for inset in [0.0, 0.06]:
					for station in [&"", &"grappling_gun", &"forklift", &"yard_crane", &"intake", &"jib", &"service_lift", &"recovery"]:
						controls.update_context(context(station), 1.0)
						# The shipping canvas expands from its design resolution; window
						# pixels are not canvas coordinates. Match UiStyle.safe_rect's
						# coordinate space before applying simulated display insets.
						var full := controls.get_viewport_rect()
						var safe := full.grow_individual(-full.size.x * inset, -full.size.y * inset, -full.size.x * inset, -full.size.y * inset)
						controls._layout_in_safe_rect(safe)
						var label := "window=%s canvas=%s scale=%s inset=%s station=%s" % [viewport, full.size, scale, inset, station]
						var active: Array = controls._buttons.values().filter(func(b): return b.shown and b.enabled)
						var sightline := Rect2(safe.position + Vector2(0.0, safe.size.y * 0.22), Vector2(safe.size.x, safe.size.y * 0.40))
						for i in range(active.size()):
							var button = active[i]
							var hit: float = button.radius * Touch.HIT_SLOP
							check(controls._safe.encloses(Rect2(button.center - Vector2.ONE * hit, Vector2.ONE * hit * 2.0)), label + " outside " + str(button.id))
							if station in [&"forklift", &"yard_crane"]:
								check(not sightline.intersects(Rect2(button.center - Vector2.ONE * hit, Vector2.ONE * hit * 2.0)), label + " obstructs forward view " + str(button.id))
							for j in range(i):
								var other = active[j]
								check(button.center.distance_to(other.center) > (button.radius + other.radius) * Touch.HIT_SLOP, label + " overlap " + str(button.id) + "/" + str(other.id))
							controls.reset_touches()
							router.frame(0.016)
							finger(controls, 2, button.center)
							check(button.index == 2, label + " captures " + str(button.id))
							if button.id == &"tools":
								check(controls._tool_tray_open, label + " TOOLS opens actual equipment tray")
								check(router.frame(0.016).verbs.is_empty(), label + " equipment menu sends no gameplay command")
							elif button.hold:
								check(controls._held(button.id) == 1.0, label + " hold " + str(button.id))
							else:
								check(router.frame(0.016).verbs == [controls._verb_for(button.id)], label + " verb " + str(button.id))
						controls.reset_touches()
						if station == &"forklift":
							for caption in controls._forklift_stick_captions():
								var font := UiStyle.font_label()
								var pixels := int(roundf(30.0 * controls._u))
								var text_size := Vector2(UiStyle.text_width(font, caption[0], pixels), font.get_height(pixels))
								check(safe.encloses(Rect2(caption[1] - text_size * 0.5, text_size).grow(5.0 * controls._u)), label + " clipped stick caption " + caption[0])
							hud._safe = safe
							hud._panel = ForkliftPanel.describe({}, {}, 2)
							check(safe.encloses(hud.compact_station_rect()) and not sightline.intersects(hud.compact_station_rect()), label + " instrument strip preserves sightline")
						if not controls._movement_blocked:
							var home: Vector2 = controls.stick_home()
							finger(controls, 0, home)
							drag(controls, 0, home, Vector2(0.0, -Touch.STICK_THROW * controls._u))
							check(controls.move_vector.y > 0.99, label + " movement")
						var look := controls._safe.position + controls._safe.size * Vector2(0.85, 0.30)
						finger(controls, 1, look)
						var relative := Vector2(20.0, 10.0)
						drag(controls, 1, look, relative)
						check(controls.take_look_delta() == relative, label + " independent look")
						controls.reset_touches()
						layouts += 1
	controls.free()
	router.free()
	hud.free()
	print("ANDROID_RESPONSIVE layouts=", layouts, " failures=", failures)
	quit(0 if failures == 0 else 1)
