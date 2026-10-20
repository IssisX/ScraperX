extends "res://tests/mounted_winch_crossing_input_case.gd"

# Stage the player once on the real receiver, then use shipping Main, camera,
# native support, touch input, Guide and packaged recordings. No fake contacts.
# This does not claim continuous ascent, phone audibility or human face fitting.
const STEP := 1.0 / 90.0
const Record := preload("res://presentation/story_handover_record.gd")
const Dialogue := preload("res://presentation/story_handover_dialogue.gd")
const STANDING := Vector3(12, 110.9, -181.95)
var phase := ""
var done := false
var phase_tick := 0
var heard: Array[StringName] = []
var last_id := -1
var replay_id := -1
var scheduled := 0
var before_walk := Vector3.ZERO
var walk_tick := -1
var walked := false

func step(count: int = 1) -> void:
	for i in count:
		main._process(STEP)

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	check(main._native.debug_restart_at(STANDING), "stage only player at authored handover viewing passage")
	main._yaw = PI
	main._pitch = 0
	step(100)
	check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == Record.RECEIVER_ID, "native solver earns actual terminal receiver footing")
	check(main._native.get_player_position().distance_to(STANDING) < .15, "staged player settles on authored deck without being displaced")
	check(main._story_record.identity_store == main._player_identity, "archived Ro and Main share the persistent work-ID owner")
	check(main._story_record.get_children().all(func(child: Node) -> bool: return not child is CollisionObject3D), "terminal adds no competing physics body")
	check(not main._story_guidance._heard.has(Dialogue.KEYS[0]), "arrival while looking away does not activate handover")
	phase = "wait_for_quiet"
	phase_tick = int(main._native.get_tick_index())
	return result()

func aim_pixel(y: float) -> void:
	aim(Record.SCREEN_POSITION + Vector3(0, (.5 - y / Record.PIXELS.y) * Record.SCREEN_SIZE.y, 0))

func read_cue(state: Dictionary, radio: Dictionary) -> void:
	var key: StringName = state.transmission_key
	check(radio.active_key == key and radio.stream != null, "matching packaged recording starts: " + str(key))
	check(radio.missing_keys.is_empty() and radio.manifest_error.is_empty(), "all packaged recordings validate")
	check(state.transmission == Dialogue.CUES[key] and state.speaker == Dialogue.SPEAKERS[key], "heard recording and caption use authored copy: " + str(key))
	check(main._story_presenter._speaker.text == state.speaker, "visible caption names actual speaker")
	if state.speaker in ["RECORD", "SOURCE"]:
		check(radio.spatial and main._story_radio._scene_voice.global_position.distance_to(Record.SCREEN_POSITION) < .001, "recorded voice is spatial at the physical terminal")
		main._story_radio.set_scene_voice_position(Vector3(6,254,-150))
		check(main._story_radio._scene_voice.global_position.distance_to(Record.SCREEN_POSITION) < .001, "Ari head-position updates cannot move terminal playback")
	else:
		check(not radio.spatial, "Ro and Iona retain headset playback")
	check(not main._touch._movement_blocked and main._router.gameplay_active, "dialogue keeps normal movement ownership")

func advance() -> Dictionary:
	step(30)
	var tick := int(main._native.get_tick_index())
	var state: Dictionary = main._story_guidance.sample(main._ctx, tick)
	var radio: Dictionary = main._story_radio.readback()
	match phase:
		"wait_for_quiet":
			if state.transmission.is_empty():
				aim_pixel(480)
				phase = "conversation"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "arrival recording did not finish")
		"conversation":
			var key: StringName = state.transmission_key
			if Dialogue.KEYS.has(key) and not state.transmission.is_empty() and int(state.transmission_id) != last_id:
				check(heard.size() < Dialogue.KEYS.size() and key == Dialogue.KEYS[heard.size()], "five actual recordings keep authored sequence: " + str(key))
				read_cue(state, radio)
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
				check(Vector2(at.x - before_walk.x, at.z - before_walk.z).length() > .01, "ordinary touch walking moves native player during speech")
				check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == Record.RECEIVER_ID, "walking retains solved deck support")
				walked = true
			if heard.size() == Dialogue.KEYS.size() and state.transmission.is_empty():
				check(walked and main._story_guidance._record_phase == Dialogue.KEYS.size(), "entire exchange finishes without taking movement controls")
				check(state.objective_id == &"crown" and "+253 M" in state.detail and "ARI WARD" in state.detail, "onward witness detail preserves Crown objective")
				check(not main._story_guidance._heard.has(&"double_ro_who"), "terminal does not consume or gate Ari encounter")
				scheduled = int(radio.scheduled_plays)
				main._yaw = PI
				main._pitch = 0
				phase = "source_aim"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "next handover recording did not follow")
		"source_aim":
			aim_pixel(840)
			phase = "source"
			phase_tick = tick
		"source":
			if state.transmission_key == &"handover_source_survey" and not state.transmission.is_empty():
				read_cue(state, radio)
				check(int(state.transmission_id) > last_id and int(radio.scheduled_plays) == scheduled + 1, "source tab schedules exactly one fresh recording")
				check(main._story_record._revision.text.contains("REV 03") and main._story_record._copy.text.contains("Occupancy not assessed."), "terminal displays original qualification during source playback")
				replay_id = int(state.transmission_id)
				phase = "source_finish"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "physical source-tab attention did not play source")
		"source_finish":
			if state.transmission.is_empty():
				check(int(radio.scheduled_plays) == scheduled + 1, "holding source gaze cannot spam playback")
				aim_pixel(730)
				phase = "replay"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "source recording did not finish")
		"replay":
			if state.transmission_key == &"handover_record_assurance" and int(state.transmission_id) > replay_id and not state.transmission.is_empty():
				read_cue(state, radio)
				check(int(radio.scheduled_plays) == scheduled + 2, "physical handover tab schedules only the requested replay")
				check(main._story_record._revision.text.contains("REV 04") and main._story_record._copy.text == Dialogue.CUES[&"handover_record_assurance"], "terminal restores assurance wording for direct comparison")
				phase = "replay_finish"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "physical handover-tab attention did not replay recording")
		"replay_finish":
			if state.transmission.is_empty():
				scheduled = int(radio.scheduled_plays)
				main._restart_checkpoint()
				phase = "retry"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "handover replay did not finish")
		"retry":
			check(main._story_guidance._record_phase == Dialogue.KEYS.size() and main._story_guidance._heard.has(&"handover_iona_gallery"), "checkpoint restart preserves accepted handover progress")
			check(int(radio.scheduled_plays) == scheduled, "checkpoint restart does not replay accepted conversation")
			main._audio.quiesce()
			check(not main._story_radio.readback().playing, "teardown stops both recorded and headset speech")
			done = true
			phase = "complete"
	return result()

func result() -> Dictionary:
	return {"checks": checks, "failures": failures, "done": done, "phase": phase,
		"heard": heard, "actual_native": true, "touch": true, "staged_receiver_only": true,
		"tick": main._native.get_tick_index() if main != null else -1}
