extends SceneTree

# Controlled bridge contract: input safety and native wheel pose consumption.
# Does not establish native support gating, traction, cargo motion, or device play.
const Main := preload("res://presentation/main.gd")
const Wheels := preload("res://presentation/forklift_view.gd")
const Router := preload("res://presentation/ui/input_router.gd")
const Touch := preload("res://presentation/ui/touch_controls.gd")
var failed := false

class ReceiptBridge extends RefCounted:
	var reachable := true
	var input: Array = []
	var release_calls := 0
	var tool_cancel_calls := 0
	func cancel_forklift_tool_request() -> void:
		tool_cancel_calls += 1
	func get_forklift_state() -> Dictionary:
		return {"available": true, "controls_reachable": reachable}
	func set_forklift_input(drive: float, steer: float, lift: float, tilt: float, brake: float) -> bool:
		input = [drive, steer, lift, tilt, brake]
		return true
	func request_release() -> bool:
		release_calls += 1
		return true

func check(ok: bool, label: String) -> void:
	if not ok:
		failed = true
		push_error("FORKLIFT CONTROLS: " + label)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var main := Main.new()
	var bridge := ReceiptBridge.new()
	main._native = bridge
	var idle := {"throttle": 0.0, "brake": 0.0, "steer": 0.0, "lift": 0.0, "tilt": 0.0, "inch": 0.0, "horn": false}
	var controls: Dictionary = idle.duplicate()
	controls.merge({"throttle": 0.75, "steer": 0.5, "lift": 1.0, "tilt": 0.5}, true)
	var intent := {"forklift": controls}
	var main_router := Router.new()
	root.add_child(main_router)
	main._router = main_router
	main._forklift_controls.enter()
	main._forklift_controls.handle_verb(&"forklift_forward")
	main._operating = &"forklift"
	main._deliver_forklift_input({"forklift": idle})
	main._deliver_forklift_input(intent)
	check(bridge.input == [0.75, 0.5, 1.0, 0.5, 0.0], "raw local axes and back tilt")
	main._deliver_forklift_input({"forklift": idle})
	check(bridge.input == [0.0, 0.0, 0.0, 0.0, 0.0], "released pedals allow native coasting")
	main._paused = true
	main._deliver_forklift_input(intent)
	check(bridge.input == [0.0, 0.0, 0.0, 0.0, 1.0], "pause clears held throttle")
	main._paused = false
	bridge.reachable = false
	main._deliver_forklift_input(intent)
	check(bridge.input == [0.0, 0.0, 0.0, 0.0, 1.0], "lost native reach clears throttle")
	bridge.reachable = true
	main._dispatch([&"drop"], 1.0 / 90.0)
	check(main._operating == &"" and bridge.input == [0.0, 0.0, 0.0, 0.0, 1.0], "leaving operation brakes immediately")
	check(bridge.tool_cancel_calls == 1, "leaving operation cancels one queued tool intent")
	check(bridge.release_calls == 0, "forklift Q leaves controls without requesting a ledge drop")
	main.free()
	main_router.queue_free()
	var view := Wheels.new()
	root.add_child(view)
	var pose := Transform3D(Basis(Vector3.FORWARD, 0.3), Vector3(3.0, 2.0, -1.0))
	var receipt := {"transform": pose, "radius_m": 0.35, "width_m": 0.2}
	view.render_state({"available": true, "wheels": [receipt, receipt, receipt]})
	check(view.get_child_count() == 3, "three native wheel visuals only")
	check(view.get_child(0).global_transform.is_equal_approx(pose), "wheel pose uses native render transform")
	check(is_equal_approx(view.get_child(0).mesh.height, 0.2), "cylinder Y axle uses native wheel width")
	view.render_state({"available": false})
	check(not view.visible, "unavailable native truck hides wheels")
	view.queue_free()
	var router := Router.new()
	root.add_child(router)
	var touch := Touch.new()
	touch.router = router
	router.touch = touch
	root.add_child(touch)
	await process_frame
	touch._layout()
	var ctx := {"hanging": false, "climbing": false, "jump_ok": true,
		"action": {"id": &"done", "icon": &"done", "label": "DONE"},
		"drop_ok": false, "chute_ok": false, "chute": false, "danger": 0.0,
		"operating": &"forklift", "crouched": false, "slung": false}
	router.set_forklift_mode(true)
	touch.update_context(ctx, 1.0)
	check(not touch._movement_blocked, "forklift touch movement stick remains usable")
	for id in [&"pendant_up", &"pendant_down", &"pendant_left", &"pendant_right"]:
		check(touch._buttons[id].shown, "touch offers forklift axis " + String(id))
	var finger := InputEventScreenTouch.new()
	finger.index = 4
	finger.position = touch.button_center(&"pendant_left")
	finger.pressed = true
	touch.handle_touch(finger)
	check(router.frame(1.0 / 90.0)["forklift"]["tilt"] == 1.0, "touch tilt-back uses dedicated positive axis")
	finger.pressed = false
	touch.handle_touch(finger)
	check(router.frame(1.0 / 90.0)["forklift"]["tilt"] == 0.0, "touch release clears tilt")
	var keyboard := InputEventKey.new()
	keyboard.physical_keycode = KEY_W
	keyboard.pressed = true
	router._input(keyboard)
	check(router.frame(1.0 / 90.0)["forklift"]["throttle"] == 1.0, "keyboard feeds independent accelerator")
	router.clear_held()
	var pad := InputEventJoypadMotion.new()
	pad.device = 0
	pad.axis = JOY_AXIS_LEFT_X
	pad.axis_value = 1.0
	router._input(pad)
	check(router.frame(1.0 / 90.0)["forklift"]["steer"] == 1.0, "gamepad feeds local steering axis")
	router.clear_held()
	touch.queue_free()
	router.queue_free()
	print("SCRAPERX_FORKLIFT_CONTROLS ", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
