extends SceneTree

# Shipping controls/native owner, with explicit diagnostic staging for each
# UI condition. This does not claim a continuous route or inventory restore.
const STEP := 1.0 / 90.0
const UiStyle := preload("res://presentation/ui/ui_style.gd")
var main: Node
var native: Object
var failed := false
var checks := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failed = true
		push_error("GRAPPLE_EQUIPMENT_ACCESS FAIL " + message)

func step(count: int = 1, delta: float = STEP) -> void:
	for i in count:
		await process_frame
		main._process(delta)

func key(code: Key, echo: bool = false) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = true
	event.echo = echo
	root.push_input(event, true)
	if not echo:
		event.pressed = false
		root.push_input(event, true)

func pad(button: JoyButton) -> void:
	var event := InputEventJoypadButton.new()
	event.device = 0
	event.button_index = button
	event.pressed = true
	root.push_input(event, true)
	event.pressed = false
	root.push_input(event, true)

func touch_button(id: StringName) -> void:
	check(main._touch.is_button_shown(id), "actual touch button shown " + String(id))
	var event := InputEventScreenTouch.new()
	event.index = 8
	event.position = main._touch.button_center(id)
	event.pressed = true
	root.push_input(event, true)
	event.pressed = false
	root.push_input(event, true)

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	main = load("res://main.tscn").instantiate()
	main._settings_persistent = false
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene and not main._ci_mode, "shipping scene and native world")
	check(UiStyle.BINDINGS[&"toggle_grapple"][1] == "T" and UiStyle.BINDINGS[&"auto_reel"][1] == "B",
		"controls table names actual keyboard mappings")
	check(not bool(main._ctx["grapple_draw_available"]), "unacquired tool is unavailable away from rack")
	key(KEY_T)
	await step()
	check(not bool(native.get_grapple_gun_state()["acquired"]), "tool key grants no remote equipment")
	check(native.debug_restart_at(Vector3(12, 485.15, -135.5)), "diagnostic actual rack stage")
	await step(45)
	check(bool(native.get_grapple_gun_state()["station_available"]) and bool(main._ctx["grapple_draw_available"]),
		"native rack permits initial physical acquisition")
	key(KEY_T)
	var tick: int = native.get_tick_index()
	await step(2, 0.0)
	check(native.get_tick_index() == tick and not bool(native.get_grapple_gun_state()["equipped"]),
		"dedicated draw waits through zero-tick frames for native acknowledgement")
	await step(2)
	check(bool(native.get_grapple_gun_state()["acquired"]) and bool(native.get_grapple_gun_state()["equipped"]),
		"keyboard dedicated draw acquires and equips at real rack")
	key(KEY_T, true)
	await step()
	check(bool(native.get_grapple_gun_state()["equipped"]), "keyboard echo never toggles equipment")
	main._pitch = 1.56
	await step()
	check(main._pitch > 1.55 and main._pitch < PI * 0.5,
		"equipped gun permits a physically pointed near-vertical shot without crossing the camera pole")
	var stock: int = native.get_grapple_gun_state()["cartridges_remaining"]
	main._pitch = 1.49
	key(KEY_E)
	await step(2)
	check(bool(native.get_grapple_gun_state()["line_attached"]) and int(native.get_grapple_gun_state()["cartridges_remaining"]) == stock - 1,
		"ordinary Action launches real finite hook after dedicated draw")
	key(KEY_B)
	await step()
	check(bool(native.get_grapple_gun_state()["auto_reel"]), "keyboard B enables native automatic reel")
	var gun_prompts: Array = main._hud._build_prompts(main._ctx)
	check(gun_prompts.any(func(prompt: Dictionary) -> bool: return prompt["verb"] == &"auto_reel" and prompt["text"] == "STOP REEL"),
		"running native auto reel exposes its actual Stop binding")
	pad(JOY_BUTTON_RIGHT_SHOULDER)
	await step()
	check(not bool(native.get_grapple_gun_state()["auto_reel"]), "pad right shoulder stops automatic reel")
	pad(JOY_BUTTON_LEFT_SHOULDER)
	await step(2)
	check(not bool(native.get_grapple_gun_state()["equipped"]) and not bool(native.get_grapple_gun_state()["line_attached"])
		and int(native.get_grapple_gun_state()["cartridges_remaining"]) == stock - 1, "pad dedicated stow releases line without stock refund")
	# Separate UI fixture: a real ledge produces a competing contextual Action.
	# Debug relocation clears transients by its existing contract; no route or
	# cross-restart stock preservation is claimed by this staging.
	check(native.debug_restart_at(Vector3(26.68, 87.92, -132)), "diagnostic actual Tower88 approach")
	main._yaw = PI * 0.5
	main._pitch = 0.0
	await step(45)
	check(main._ctx["action"]["id"] == &"climb" and bool(main._ctx["grapple_draw_available"]),
		"actual ledge retains Climb Action while acquired gun remains drawable")
	var ledge_prompts: Array = main._hud._build_prompts(main._ctx)
	check(ledge_prompts.any(func(prompt: Dictionary) -> bool: return prompt["verb"] == &"action")
		and ledge_prompts.any(func(prompt: Dictionary) -> bool: return prompt["verb"] == &"toggle_grapple"),
		"keyboard/pad HUD shows both ledge Action and dedicated equipment binding")
	touch_button(&"grapple")
	await step(2)
	check(bool(native.get_grapple_gun_state()["equipped"]) and int(native.get_traversal_state()) == 0,
		"dedicated touch draw works beside actual ledge without starting climb")
	check(main._ctx["action"]["id"] == &"grapple_action", "native-equipped gun owns its Action after acknowledgement")
	touch_button(&"grapple")
	await step(2)
	check(not bool(native.get_grapple_gun_state()["equipped"]) and main._ctx["action"]["id"] == &"climb",
		"touch stow restores unchanged native ledge Action")
	main._router.gameplay_active = false
	key(KEY_T)
	await step()
	check(not bool(native.get_grapple_gun_state()["equipped"]), "paused router suppresses keyboard equipment")
	check(native.get_death_count() == 0, "UI control checks cause no death")
	print("GRAPPLE_EQUIPMENT_ACCESS %s checks=%d shipping_main=1 native_ack=1 ledge_conflict=1 keyboard_pad_touch=1 phone_play=0" % ["FAIL" if failed else "PASS", checks])
	quit(1 if failed else 0)
