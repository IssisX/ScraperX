extends "res://tests/mounted_winch_crossing_input_case.gd"

# Stage the player once on the real receiver, then use shipping Main, camera,
# native support, touch input, Guide and packaged recordings. No fake contacts.
# This does not claim continuous ascent, phone audibility or human face fitting.
const STEP := 1.0 / 90.0
const Record := preload("res://presentation/story_handover_record.gd")
const Dialogue := preload("res://presentation/story_handover_dialogue.gd")
const Encounter := preload("res://presentation/story_double_encounter.gd")
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
var requested_disposition := &"sent_to_dev"
var decision_heard: Array[StringName] = []
var ari_heard: Array[StringName] = []
var highlight_tick := -1

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

func aim_pixel(y: float, x: float = 320) -> void:
	aim(Record.SCREEN_POSITION + Vector3((x / Record.PIXELS.x - .5) * Record.SCREEN_SIZE.x, (.5 - y / Record.PIXELS.y) * Record.SCREEN_SIZE.y, 0))

func read_cue(state: Dictionary, radio: Dictionary) -> void:
	var key: StringName = state.transmission_key
	check(radio.active_key == key and radio.stream != null, "matching packaged recording starts: " + str(key))
	check(radio.missing_keys.is_empty() and radio.manifest_error.is_empty(), "all packaged recordings validate")
	var cues: Dictionary = Dialogue.CUES if Dialogue.CUES.has(key) else Encounter.CUES
	var speakers: Dictionary = Dialogue.SPEAKERS if Dialogue.CUES.has(key) else Encounter.SPEAKERS
	check(state.transmission == cues[key] and state.speaker == speakers[key], "heard recording and caption use authored copy: " + str(key))
	check(main._story_presenter._speaker.text == state.speaker, "visible caption names actual speaker")
	if state.speaker in ["RECORD", "SOURCE"]:
		check(radio.spatial and main._story_radio._scene_voice.global_position.distance_to(Record.SCREEN_POSITION) < .001, "recorded voice is spatial at the physical terminal")
		main._story_radio.set_scene_voice_position(Vector3(6,254,-150))
		check(main._story_radio._scene_voice.global_position.distance_to(Record.SCREEN_POSITION) < .001, "Ari head-position updates cannot move terminal playback")
	elif state.speaker == "ARI":
		var actor: Dictionary = main._native.get_story_actor_state()
		check(radio.spatial and main._story_radio._scene_voice.global_position.distance_to(actor.position + Vector3(0, .72, 0)) < .001, "Ari responds from the actual actor head")
	else:
		check(not radio.spatial, "remote and player voices retain headset playback")
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
				aim_pixel(638, 168 if requested_disposition == &"sent_to_dev" else 472)
				phase = "choice_highlight"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "handover replay did not finish")
		"choice_highlight":
			check(state.handover_disposition == &"undecided", "looking at a choice never submits evidence")
			if main._story_record.choice_offer() == requested_disposition:
				if highlight_tick < 0:
					highlight_tick = tick
				if tick - highlight_tick >= 90:
					check(main._ctx.action.id == &"handover_decision" and main._ctx.action.label == ("SEND" if requested_disposition == &"sent_to_dev" else "KEEP"), "existing touch Action names the aimed story decision")
					check(main._story_record._send_choice.visible and main._story_record._ari_choice.visible, "both choices remain on the real terminal screen")
					tap(&"action")
					phase = "decision_dialogue"
					phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "terminal choice did not become available through real attention")
		"decision_dialogue":
			var expected: Array = Dialogue.SEND_KEYS if requested_disposition == &"sent_to_dev" else Dialogue.ARI_KEYS
			var key: StringName = state.transmission_key
			check(state.handover_disposition == requested_disposition, "real touch press commits and retains the selected knowledge state")
			if expected.has(key) and not state.transmission.is_empty() and int(state.transmission_id) != last_id:
				check(decision_heard.size() < expected.size() and key == expected[decision_heard.size()], "chosen decision plays its own recorded response")
				read_cue(state, radio)
				decision_heard.append(key)
				last_id = int(state.transmission_id)
				phase_tick = tick
			if decision_heard.size() == expected.size() and state.transmission.is_empty():
				check(not state.handover_receipt.is_empty() and main._story_record._hint.text == state.handover_receipt, "terminal displays the chosen dispatch or retained-copy receipt")
				scheduled = int(radio.scheduled_plays)
				main._restart_checkpoint()
				phase = "retry"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "decision acknowledgement did not complete")
		"retry":
			check(main._story_guidance._record_phase == Dialogue.KEYS.size() and main._story_guidance._heard.has(&"handover_iona_gallery"), "checkpoint restart preserves accepted handover progress")
			check(state.handover_disposition == requested_disposition, "checkpoint retry preserves the accepted dispatch decision")
			check(int(radio.scheduled_plays) == scheduled, "checkpoint restart does not replay accepted conversation")
			var actor: Dictionary = main._native.get_story_actor_state()
			check(actor.present and int(actor.entity_id) == Encounter.ACTOR_ID, "normal world contains the independent Ari actor")
			# This second explicit observation stage exercises the consequence;
			# it is not a claimed continuous110→253m ascent.
			check(main._native.debug_restart_at(actor.position + Vector3(0, 0, .95)), "stage player only at actual Ari receiver, preserving the accepted story state")
			main._yaw = PI
			main._pitch = 0
			phase = "ari_settle"
			phase_tick = tick
		"ari_settle":
			if tick - phase_tick >= 90:
				var actor: Dictionary = main._native.get_story_actor_state()
				check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == Encounter.RECEIVER_ID, "player earns actual Ari receiver footing")
				check(actor.supported and int(actor.support_entity_id) == Encounter.RECEIVER_ID and bool(main._ctx.story_actor_line_of_sight), "Ari independently has actual footing and native visibility")
				aim(actor.position + Vector3(0, .65, 0))
				phase = "ari_consequence"
				phase_tick = tick
		"ari_consequence":
			var key: StringName = state.transmission_key
			var expected: Array = Encounter.KEYS.slice(0, 4) + Encounter.BRANCH_KEYS[requested_disposition]
			if expected.has(key) and not state.transmission.is_empty() and int(state.transmission_id) != last_id:
				check(ari_heard.size() < expected.size() and key == expected[ari_heard.size()], "accepted archive choice changes the real Ari conversation")
				read_cue(state, radio)
				ari_heard.append(key)
				last_id = int(state.transmission_id)
				phase_tick = tick
			if ari_heard.size() == expected.size():
				check(main._story_guidance._double_encounter._branch_disposition == requested_disposition, "Ari freezes knowledge only after accepting the branch")
				before_walk = main._native.get_player_position()
				stick(Vector2(0, -.12))
				phase = "ari_onward"
				phase_tick = tick
			elif tick - phase_tick > 1800:
				check(false, "Ari's chosen response did not play")
		"ari_onward":
			stick(Vector2.ZERO)
			var at: Vector3 = main._native.get_player_position()
			check(Vector2(at.x - before_walk.x, at.z - before_walk.z).length() > .01, "ordinary touch walking remains live during the consequential reply")
			check(bool(main._ctx.checkpoint_footing_valid), "onward movement retains actual support")
			main._audio.quiesce()
			check(not main._story_radio.readback().playing, "teardown stops both recorded and headset speech")
			done = true
			phase = "complete"
	return result()

func result() -> Dictionary:
	return {"checks": checks, "failures": failures, "done": done, "phase": phase,
		"heard": heard, "decision_heard": decision_heard, "ari_heard": ari_heard,
		"disposition": requested_disposition, "actual_native": true, "touch": true, "two_explicit_receiver_stages": true,
		"tick": main._native.get_tick_index() if main != null else -1}
