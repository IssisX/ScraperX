extends SceneTree

# Exercise shipping touch capture, router vocabulary and Main dispatch without
# starting a world. The double records requests; it cannot prove motor physics.
const Main := preload("res://presentation/main.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

class GunRequests extends RefCounted:
	var gun := {"equipped": true, "line_attached": true, "state": 2,
		"auto_reel": false, "action_recovers_hook": false}
	var reel_requests: Array[bool] = []
	var action_requests := 0
	var releases := 0

	func get_grapple_gun_state() -> Dictionary:
		return gun.duplicate()

	func set_grapple_auto_reel(enabled: bool) -> void:
		reel_requests.append(enabled)

	func request_grapple_action() -> void:
		action_requests += 1

	func request_grapple_release() -> void:
		releases += 1

var failures: Array[String] = []
var checks := 0
var main: Node
var native := GunRequests.new()
var controls: Control
var router: Node

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures.append(message)
		push_error("GRAPPLE_PRIMARY_TOUCH FAIL " + message)

func refresh() -> void:
	main._ctx = {"hanging": false, "jump_ok": true,
		"action": Main._grapple_primary_action(native.gun),
		"chute_ok": false, "chute": false, "danger": 0.0,
		"operating": &"grappling_gun", "slung": false, "crouched": false,
		"grapple_equipped": native.gun["equipped"], "grapple_draw_available": false,
		"grapple_line_attached": native.gun["line_attached"],
		"grapple_auto_reel": native.gun["auto_reel"], "drop_ok": true}
	controls.update_context(main._ctx, 1.0)

func touch(id: StringName, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = 1
	event.position = controls.button_center(id)
	event.pressed = pressed
	router._input(event)
	main._dispatch(router.frame(0.016)["verbs"], 0.016)

func tap(id: StringName) -> void:
	touch(id, true)
	touch(id, false)

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	main = Main.new()
	main._native = native
	main._operating = &"grappling_gun"
	controls = TouchControls.new()
	router = InputRouter.new()
	root.add_child(controls)
	root.add_child(router)
	router.capture_mouse = false
	router.touch = controls
	controls.router = router
	refresh()
	check(controls.button_label(&"action") == "REEL IN", "locked distant hook offers primary Reel In")
	check(main._ctx["action"]["id"] == &"grapple_reel", "reel is a separate UI verb")
	touch(&"action", true)
	check(native.reel_requests == [true] and native.action_requests == 0,
		"actual primary touch dispatch requests reel rather than hook recovery")
	check(not native.gun["auto_reel"] and controls.button_label(&"action") == "REEL IN",
		"touch waits for native acknowledgement")
	for i in 10:
		main._dispatch(router.frame(0.016)["verbs"], 0.016)
	check(native.reel_requests.size() == 1, "held primary touch never repeats toggle")
	touch(&"action", false)
	native.gun["auto_reel"] = true
	refresh()
	check(controls.button_label(&"action") == "STOP REEL", "native running reel offers primary Stop Reel")
	tap(&"action")
	check(native.reel_requests == [true, false] and native.action_requests == 0,
		"actual primary touch dispatch stops the existing reel")
	check(controls.button_label(&"drop") == "RELEASE", "Drop remains deliberate line release")
	tap(&"drop")
	check(native.releases == 1 and native.action_requests == 0, "Drop dispatch remains native release")
	# A close hook recovers only with the native affordance, including a
	# locked hook whose geometry has moved back within the player's reach.
	native.gun["action_recovers_hook"] = true
	refresh()
	check(controls.button_label(&"action") == "RECOVER HOOK", "native nearby hook offers recovery")
	tap(&"action")
	check(native.action_requests == 1 and native.reel_requests.size() == 2, "nearby primary dispatch remains recovery")
	native.gun.erase("action_recovers_hook")
	refresh()
	check(controls.button_label(&"action") == "STOP REEL", "absent native recovery flag never assumes remote retrieval")
	native.gun["state"] = 1
	refresh()
	check(controls.button_label(&"action") == "LOCK LINE", "flying hook retains Lock Line")
	tap(&"action")
	check(native.action_requests == 2, "Lock Line still uses native action")
	native.gun["state"] = 0
	native.gun["line_attached"] = false
	native.gun["auto_reel"] = false
	refresh()
	check(controls.button_label(&"action") == "FIRE HOOK", "ready tool retains Fire Hook")
	tap(&"action")
	check(native.action_requests == 3, "Fire Hook still uses native action")
	main._ctx["action"] = {"id": &"grapple_reel"}
	main._perform_action()
	check(native.reel_requests.size() == 2, "stale primary reel cannot enable a detached line")
	main.free()
	controls.queue_free()
	router.queue_free()
	print("GRAPPLE_PRIMARY_TOUCH %s checks=%d touch_router_main=1 native_requests=1 world_physics=0" % ["PASS" if failures.is_empty() else "FAIL", checks])
	quit(0 if failures.is_empty() else 1)
