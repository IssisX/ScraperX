extends "res://tests/mounted_winch_crossing_input_case.gd"

# Staged actual-gallery integration; does not claim the approach/ascent route.
const STEP := 1.0 / 90.0
var phase := ""
var done := false
var started_at := 0
var before_walk := Vector3.ZERO
var plays_after_exchange := 0

func step(count: int = 1) -> void:
	for i in count:
		main._process(STEP)

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	check(main._native.debug_restart_at(Vector3(-101.5, 6.8, -95)), "staged gallery start on existing physical deck")
	step(100)
	check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == 1440, "actual native gallery footing")
	check(not main._story_discovery._read and int(main._story_radio.readback().scheduled_plays) == 0, "arrival without inspection does not play the exchange")
	var direction: Vector3 = main._story_discovery.CARD_POSITION - main._camera.global_position
	main._yaw = atan2(-direction.x, -direction.z)
	main._pitch = atan2(direction.y, Vector2(direction.x, direction.z).length())
	phase = "inspect"
	return result()

func advance() -> Dictionary:
	step(90)
	var state: Dictionary = main._story_guidance.sample(main._ctx, int(main._native.get_tick_index()))
	var radio: Dictionary = main._story_radio.readback()
	match phase:
		"inspect":
			check(main._story_discovery._read, "camera attention reads the actual card from solved gallery footing")
			check(state.transmission_key == &"vale_contact_ro" and state.speaker == "RO", "authored question reaches shipping story state")
			check(radio.active_key == &"vale_contact_ro" and radio.stream != null and int(radio.scheduled_plays) == 1, "distinct Ro recording actually starts")
			check(main._story_presenter._speaker.text == "RO", "question has matching accessible speaker caption")
			started_at = int(main._native.get_tick_index())
			phase = "reply"
		"reply":
			if state.transmission_key == &"vale_contact_iona":
				check(state.speaker == "IONA" and "fucking recommendation" in state.transmission, "adult reply retains exact authored wording")
				check(radio.active_key == &"vale_contact_iona" and radio.stream != null and int(radio.scheduled_plays) == 2, "Iona's matching recording starts once after Ro")
				check(state.objective_id == &"freight_one", "personal scene does not manufacture ascent progress")
				started_at = int(main._native.get_tick_index())
				phase = "finish"
			elif int(main._native.get_tick_index()) - started_at > 540:
				check(false, "reply did not follow the completed question")
		"finish":
			if state.transmission.is_empty():
				plays_after_exchange = int(radio.scheduled_plays)
				before_walk = main._native.get_player_position()
				stick(Vector2.RIGHT)
				phase = "onward"
			elif int(main._native.get_tick_index()) - started_at > 540:
				check(false, "exchange did not release the ordinary HUD")
		"onward":
			stick(Vector2.ZERO)
			var at: Vector3 = main._native.get_player_position()
			check(Vector2(at.x - before_walk.x, at.z - before_walk.z).length() > .25, "touch movement continues after the discovery")
			check(int(radio.scheduled_plays) == plays_after_exchange, "moving away never repeats the personal exchange")
			check(int(main._native.get_death_count()) == 0, "same-life discovery and onward movement")
			done = true
			phase = "complete"
	return result()

func result() -> Dictionary:
	return {"checks": checks, "failures": failures, "done": done, "phase": phase,
		"actual_native": true, "touch": true, "staged_gallery_only": true,
		"tick": main._native.get_tick_index() if main != null else -1}
