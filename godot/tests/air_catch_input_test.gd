extends Node

# Shipping input router -> Main -> actual native catch -> phone feedback.
# Native time is advanced explicitly; desktop frame timing cannot change
# the early input window. The initial stage is the existing shipping gantry.
const STEP := 1.0 / 90.0
const FIRST_REACH_TICK := 73
var main: Node
var native: Object
var checks := 0
var failures := 0
var finished := false
var result_label: Label

func _ready() -> void:
	if "--quit-after-air-catch-check" in OS.get_cmdline_user_args():
		_run.call_deferred()
	else:
		# Let the runtime bridge connect before the synchronous fixed-tick
		# check starts. Starting a check must not block its own MCP handshake.
		var start := Button.new()
		start.name = "RunInputCheck"
		start.text = "Run ScraperX early-catch input check"
		start.position = Vector2(24, 80)
		start.pressed.connect(func() -> void:
			start.disabled = true
			_run.call_deferred())
		add_child(start)

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_AIR_CATCH_INPUT FAIL " + why)

func step(count: int = 1) -> void:
	for i in count:
		var before: int = native.get_tick_index()
		main._process(STEP)
		check(native.get_tick_index() == before + 1, "one exact native input tick")
		check(native.get_death_count() == 0, "no death or hidden recovery")

func stage() -> void:
	main._router.clear_held()
	main._crouch_toggled = false
	main._yaw = -PI * 0.5
	main._pitch = 0.0
	check(native.debug_restart_at(Vector3(16.85, 15, -118)), "initial airborne gantry stage")
	step(FIRST_REACH_TICK - 3)
	check(not native.is_grip_available() and not native.is_player_grounded(),
		"input begins before actual grip reach")
	check(main._ctx.action.id == &"catch" and main._ctx.action.label == "REACH",
		"shipping context offers an early reach without claiming a grip")
	check(main._touch.is_button_shown(&"action") and main._touch._buttons[&"action"].enabled,
		"phone Action is genuinely pressable before hand reach")

func key(code: Key) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = true
	get_viewport().push_input(event, true)
	event.pressed = false
	get_viewport().push_input(event, true)

func touch(id: StringName) -> void:
	check(main._touch.is_button_shown(id) and main._touch._buttons[id].enabled,
		"actual phone button enabled: " + String(id))
	if id == &"drop":
		print("AIR_CATCH_CANCEL fade=%.6f native_pending=%s" % [
			main._touch._buttons[id].appear, native.get_landing_state().air_catch_intent_pending])
	var event := InputEventScreenTouch.new()
	event.index = 4
	event.position = main._touch.button_center(id)
	event.pressed = true
	get_viewport().push_input(event, true)
	event.pressed = false
	get_viewport().push_input(event, true)
	check(main._router._verbs.has(id), "actual phone press queues its verb: " + String(id))

func pending() -> void:
	var state: Dictionary = native.get_landing_state()
	check(bool(state.get("air_catch_intent_pending", false)) and
		int(state.traversal_hand_constraint_count) == 0,
		"native pending intent reaches presentation without a physical hand")
	check(main._ctx.action.detail == "REACHING" and main._touch.is_button_shown(&"drop")
		and main._touch.button_label(&"drop") == "CANCEL",
		"phone exposes honest waiting feedback and a cancellation control")

func caught() -> void:
	var state: Dictionary = native.get_landing_state()
	check(int(state.traversal_hand_constraint_count) > 0 and
		native.get_traversal_support_entity_id() == 2952 and
		float(state.actual_hand_load_n) > 50,
		"shipping input produces an actual loaded gantry catch")
	check(not bool(state.get("air_catch_intent_pending", true)) and
		main._touch.button_label(&"drop") != "CANCEL",
		"successful acquisition consumes input and restores ordinary release feedback")

func _run() -> void:
	get_tree().root.size = Vector2i(1280, 900)
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene, "shipping Main and native normal world")
	step(180)
	stage()
	touch(&"action")
	step()
	pending()
	step(7)
	caught()

	stage()
	touch(&"action")
	step()
	pending()
	touch(&"drop")
	step(5)
	var cancelled: Dictionary = native.get_landing_state()
	check(not bool(cancelled.get("air_catch_intent_pending", true)) and
		int(cancelled.traversal_hand_constraint_count) == 0 and native.is_grip_available(),
		"phone Cancel prevents a later catch despite a real available grip")
	check(not main._touch.is_button_shown(&"drop"), "cancelled empty hands remove Cancel")
	touch(&"action")
	step()
	caught()

	stage()
	key(KEY_E)
	step()
	pending()
	step(7)
	caught()
	key(KEY_Q)
	step()
	check(int(native.get_landing_state().traversal_hand_constraint_count) == 0,
		"keyboard Drop releases the actual caught hands")
	finished = true
	var verdict := "SCRAPERX_AIR_CATCH_INPUT %s checks=%d" % ["PASS" if failures == 0 else "FAIL", checks]
	print(verdict)
	var layer := CanvasLayer.new()
	layer.layer = 200
	add_child(layer)
	result_label = Label.new()
	result_label.name = "AirCatchResult"
	result_label.text = verdict
	result_label.position = Vector2(24, 24)
	layer.add_child(result_label)
	if "--quit-after-air-catch-check" in OS.get_cmdline_user_args():
		get_tree().quit(0 if failures == 0 else 1)
