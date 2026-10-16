extends SceneTree

const Router := preload("res://presentation/ui/input_router.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
const Controls := preload("res://presentation/ui/forklift_controls.gd")
const ToolPanel := preload("res://presentation/ui/forklift_panel.gd")
var failed := false

func check(ok: bool, label: String) -> void:
	if not ok:
		failed = true
		push_error("FORKLIFT_TOOL_INPUTS FAIL " + label)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var driver := Controls.new()
	driver.enter()
	var analog := {"throttle":0.0,"brake":0.0,"steer":0.0,"lift":-1.0,"tilt":1.0,"inch":0.0}
	var normal: Array = driver.sample(analog)
	check(normal[2] == -1.0 and normal[3] == 1.0 and normal[4] == 1.0,
		"normal parked hydraulics retain full signed lever demand")
	analog.inch = 1.0
	var fine: Array = driver.sample(analog)
	check(fine[2] < 0.0 and fine[3] > 0.0 and absf(fine[2]) <= 0.11 and fine[3] <= 0.11,
		"held INCH permits precise signed lift and tilt without releasing parking brake")
	analog.inch = 0.5
	var blend: Array = driver.sample(analog)
	check(absf(blend[2]) > absf(fine[2]) and absf(blend[2]) < absf(normal[2]),
		"analog fine control blends hydraulic demand continuously")
	driver.leave()
	check(driver.sample(analog) == [0.0,0.0,0.0,0.0,1.0],
		"leaving cab clears both fine hydraulic demands")
	var router := Router.new()
	root.add_child(router)
	var touch := Touch.new()
	touch.router = router
	router.touch = touch
	root.add_child(touch)
	await process_frame
	router.set_forklift_mode(true)
	var key := InputEventKey.new()
	key.physical_keycode = KEY_G
	key.pressed = true
	router._input(key)
	check(router.frame(0.01).verbs == [&"forklift_tool"], "keyboard requests only tool exchange")
	check(router.frame(0.01).verbs.is_empty(), "held key cannot repeat a transaction")
	key.echo = true
	router._input(key)
	check(router.frame(0.01).verbs.is_empty(), "key repeat cannot recouple after release")
	var pad := InputEventJoypadButton.new()
	pad.button_index = JOY_BUTTON_BACK
	pad.pressed = true
	router._input(pad)
	check(router.frame(0.01).verbs == [&"forklift_tool"], "pad View requests same named action")
	var ctx := {"operating": &"forklift", "hanging": false, "crouched": false, "slung": false,
		"jump_ok": false, "drop_ok": false, "chute_ok": false, "chute": false, "danger": 0.0,
		"action": {"id": &"done", "icon": &"done", "label": "DONE"},
		"forklift_operator": {"parking": true}, "forklift_tool_coupled": true, "forklift_tool_ready": false}
	var controls := [&"forklift_forward", &"forklift_neutral", &"forklift_reverse", &"forklift_park",
		&"forklift_inch", &"forklift_horn", &"forklift_tool", &"pendant_up", &"pendant_down",
		&"pendant_left", &"pendant_right", &"action", &"pause"]
	for size in [Vector2i(2160, 1856), Vector2i(2316, 904)]:
		root.size = size
		await process_frame
		touch._layout()
		touch.update_context(ctx, 1.0)
		var tool = touch._buttons[&"forklift_tool"]
		check(tool.shown and tool.label == "UNCOUPLE", "touch label follows actual coupled state")
		check(touch._safe.encloses(Rect2(tool.center - Vector2.ONE * tool.radius * 1.22,
			Vector2.ONE * tool.radius * 2.44)), "tool target fits phone safe area")
		for other in controls:
			if other != &"forklift_tool":
				var button = touch._buttons[other]
				check(tool.center.distance_to(button.center) > (tool.radius + button.radius) * 1.22,
					"tool target separate from " + String(other))
		var finger := InputEventScreenTouch.new()
		finger.index = 7
		finger.position = tool.center
		finger.pressed = true
		touch.handle_touch(finger)
		check(router.frame(0.01).verbs == [&"forklift_tool"], "touch requests same one-shot transaction")
		finger.pressed = false
		touch.handle_touch(finger)
		check(router.frame(0.01).verbs.is_empty(), "touch release cannot repeat exchange")
	ctx.forklift_tool_coupled = false
	ctx.forklift_tool_ready = true
	touch.update_context(ctx, 1.0)
	check(touch._buttons[&"forklift_tool"].label == "COUPLE", "released attachment offers capture")
	check(ToolPanel.tool_status({"coupled":true,"can_detach":false,"tool_refusal":10}) == "REST ON PARKING BED",
		"native unsupported refusal stays visible")
	check(ToolPanel.tool_status({"coupled":true,"can_detach":true,"tool_refusal":0}) == "READY TO UNCOUPLE",
		"only native readiness presents release ready")
	check(ToolPanel.tool_status({"coupled":false,"can_couple":true,"tool_refusal":0}) == "READY TO COUPLE",
		"actual detached capture readiness presented")
	router.set_forklift_mode(false)
	pad.pressed = true
	router._input(pad)
	check(router.frame(0.01).verbs == [&"telemetry"], "View outside cab keeps existing behavior")
	touch.free()
	router.free()
	# Queue-slot behavior observes actual bridge intent, not a synthetic coupled
	# flag. The separate public native route proves supported tool exchange.
	var main = load("res://main.tscn").instantiate()
	root.add_child(main)
	main.set_process(false)
	var native = main._native
	check(native != null, "real Main native bridge loaded")
	var generation: int = native.get_forklift_state().coupling_generation
	check(native.request_forklift_detach(), "real native slot accepts initial intent")
	main._open_pause()
	check(main._paused and paused, "actual Main pause flow opens")
	check(native.request_forklift_couple(), "pause clears old queued tool intent")
	native.cancel_forklift_tool_request()
	main._resume()
	check(native.request_forklift_detach(), "new intent can be queued after resume")
	main._end_operation()
	check(native.request_forklift_couple(), "leaving controls clears queued tool intent")
	native.cancel_forklift_tool_request()
	for i in 10:
		main._process(1.0 / 90.0)
	check(native.get_forklift_state().coupled and native.get_forklift_state().coupling_generation == generation,
		"cancelled bridge intents leave actual tool unchanged")
	main.free()
	native = null
	print("FORKLIFT_TOOL_INPUTS ", "FAIL" if failed else "PASS", " physical_exchange_test=separate")
	quit(1 if failed else 0)
