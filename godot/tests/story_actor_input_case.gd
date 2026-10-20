extends "res://tests/mounted_winch_crossing_input_case.gd"

# The player is staged on the authored receiver. All body motion, support,
# visibility, attention, speech and touch handling then use shipping owners.
# This does not claim a continuous ascent or audible device playback.
const STEP := 1.0 / 90.0
const Encounter := preload("res://presentation/story_double_encounter.gd")
var phase := ""
var done := false
var phase_tick := 0
var heard: Array[StringName] = []
var last_id := -1
var before_walk := Vector3.ZERO
var episode := -1
var scheduled := 0
var walk_tick := -1

func step(count: int = 1) -> void:
	for i in count:
		main._process(STEP)

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	var actor: Dictionary = main._native.get_story_actor_state()
	check(actor.present and int(actor.entity_id) == 63200, "normal world contains the native actor")
	check(main._native.debug_restart_at(actor.position + Vector3(0, 0, .95)), "stage only player beside actual gallery actor")
	main._yaw = PI
	main._pitch = 0
	step(100)
	actor = main._native.get_story_actor_state()
	check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == 1993, "player earns real receiver footing")
	check(actor.supported and int(actor.support_entity_id) == 1993, "actor independently earns receiver footing")
	check(bool(main._ctx.story_actor_line_of_sight), "native geometry confirms visible actor")
	check(main._story_human != null and main._story_human.visible, "shipping Main creates the real imported character")
	check(main._story_human.global_position.distance_to(actor.position - Vector3(0, .9, 0)) < .001, "visible soles follow native COM without another movement owner")
	check(not main._story_guidance._heard.has(&"double_ro_who"), "nearby actor without deliberate attention cannot trigger dialogue")
	phase = "wait_for_quiet"
	phase_tick = int(main._native.get_tick_index())
	return result()

func advance() -> Dictionary:
	step(30)
	var tick := int(main._native.get_tick_index())
	var state: Dictionary = main._story_guidance.sample(main._ctx, tick)
	var radio: Dictionary = main._story_radio.readback()
	match phase:
		"wait_for_quiet":
			if state.transmission.is_empty():
				check(not main._story_guidance._heard.has(&"double_ro_who"), "looking away through the arrival cue preserves encounter")
				var actor: Dictionary = main._native.get_story_actor_state()
				aim(actor.position + Vector3(0, .65, 0))
				phase = "conversation"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "arrival recording did not finish")
		"conversation":
			var key: StringName = state.transmission_key
			if Encounter.KEYS.has(key) and not state.transmission.is_empty() and int(state.transmission_id) != last_id:
				check(heard.size() < Encounter.KEYS.size() and key == Encounter.KEYS[heard.size()], "actual accepted dialogue keeps authored order: " + str(key))
				check(radio.active_key == key and radio.stream != null, "packaged matching recording starts: " + str(key))
				check(radio.missing_keys.is_empty() and radio.manifest_error.is_empty(), "all packaged cue resources are available")
				check(main._story_presenter._speaker.text == state.speaker, "caption identifies actual speaking character")
				if state.speaker == "ARI":
					var actor: Dictionary = main._native.get_story_actor_state()
					check(radio.spatial and main._story_radio._scene_voice.global_position.distance_to(actor.position + Vector3(0, .72, 0)) < .001, "Ari speaks from his actual head in the world")
				else:
					check(not radio.spatial, "Ro and Iona retain the radio voice path")
				heard.append(key)
				last_id = int(state.transmission_id)
				phase_tick = tick
				if heard.size() == 2:
					before_walk = main._native.get_player_position()
					walk_tick = tick
					stick(Vector2(0, -.12))
			if walk_tick >= 0 and tick > walk_tick and stick_down:
				stick(Vector2.ZERO)
				var at: Vector3 = main._native.get_player_position()
				check(Vector2(at.x - before_walk.x, at.z - before_walk.z).length() > .01, "real touch movement stays responsive during dialogue")
				check(not main._touch._movement_blocked, "encounter never owns the movement controls")
			if heard.size() == Encounter.KEYS.size() and state.transmission.is_empty():
				scheduled = int(radio.scheduled_plays)
				episode = int(main._native.get_story_actor_state().episode)
				main._restart_checkpoint()
				phase = "retry"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "next real encounter recording did not follow")
		"retry":
			var actor: Dictionary = main._native.get_story_actor_state()
			check(int(actor.episode) == episode + 1 and actor.supported, "retry restores the native actor and acquires fresh support")
			check(main._story_human.global_position.distance_to(actor.position - Vector3(0, .9, 0)) < .001, "retry presentation follows the restored body")
			check(int(radio.scheduled_plays) == scheduled, "retry does not replay already accepted conversation")
			check(main._story_guidance._double_encounter._phase == Encounter.KEYS.size(), "accepted encounter survives checkpoint restoration")
			main._audio.quiesce()
			check(not main._story_radio.readback().playing, "teardown stops actor and radio emitters")
			done = true
			phase = "complete"
	return result()

func result() -> Dictionary:
	return {"checks": checks, "failures": failures, "done": done, "phase": phase,
		"heard": heard, "actual_native": true, "touch": true, "staged_receiver_only": true,
		"tick": main._native.get_tick_index() if main != null else -1}
