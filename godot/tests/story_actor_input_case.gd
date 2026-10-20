extends "res://tests/mounted_winch_crossing_input_case.gd"

# The player is staged on the authored receiver. All body motion, support,
# visibility, attention, speech and touch handling then use shipping owners.
# This does not claim a continuous ascent or audible device playback.
const STEP := 1.0 / 90.0
const Encounter := preload("res://presentation/story_double_encounter.gd")
const IdentityStore := preload("res://presentation/identity/player_identity_store.gd")
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
	check_identity()
	phase = "wait_for_quiet"
	phase_tick = int(main._native.get_tick_index())
	return result()

func check_identity() -> void:
	# Refuse an existing profile. This case owns only the files it creates,
	# including when somebody runs the check outside a clean CI fixture.
	if DirAccess.dir_exists_absolute(IdentityStore.DIRECTORY):
		check(false, "identity integration requires a fixture without an existing profile")
		return
	main._open_pause()
	main._pause_menu._show_page(main._pause_menu.PAGE_WORK_ID)
	var page: Control = main._pause_menu._identity_page
	check(page.store == main._player_identity and page._intake.store == main._player_identity, "photo picker, work ID and Main share the real persistent owner")
	var position: Vector3 = main._native.get_player_position()
	var checkpoint: Vector3 = main._ctx.checkpoint
	var serial: int = main._story_guidance._transmission_id
	# Select a bundled fictional portrait through the actual intake callback.
	# This exercises decoding/storage/signals; it does not claim an Android
	# OS picker was operated by a person on a phone.
	page._intake.request_photo()
	page._intake._on_file_selected("res://assets/story/iona_dispatch_portrait.png", page._intake._request_id)
	check(main._player_identity.has_photo() and main._story_identity_dirty, "real intake stores the selected image and invalidates the visible identity")
	var points := PackedVector2Array([Vector2(.391,.387), Vector2(.574,.354), Vector2(.504,.578)])
	page._save_alignment(points)
	check(IdentityStore.new().face_landmarks() == points, "a fresh owner reloads alignment bound to the saved portrait")
	main._update_story_human(int(main._native.get_tick_index()))
	var fitted := false
	var normalized: Image = main._player_identity.photo_texture().get_image()
	for node: MeshInstance3D in main._story_human._original_skin:
		check(node.mesh != main._story_human._original_skin[node], "live actor uses a fitted facial mesh")
		for surface in node.mesh.get_surface_count():
			var material: Material = node.mesh.surface_get_material(surface)
			if material is ShaderMaterial and material.resource_name == "PortraitSkin":
				var texture: Texture2D = material.get_shader_parameter("portrait_texture")
				fitted = texture != null and texture.get_image().get_data() == normalized.get_data()
	check(fitted and main._story_human.last_error.is_empty(), "live facial shader receives the exact locally normalized portrait pixels")
	page._use_default()
	main._update_story_human(int(main._native.get_tick_index()))
	for node: MeshInstance3D in main._story_human._original_skin:
		check(node.mesh == main._story_human._original_skin[node], "removing the photo restores the original live character skin")
	check(not IdentityStore.new().has_photo() and not FileAccess.file_exists(IdentityStore.FIT_PATH), "removal clears owned photo and calibration data")
	check(main._native.get_player_position() == position and main._ctx.checkpoint == checkpoint and main._story_guidance._transmission_id == serial, "personalization and removal preserve physical position, checkpoint and story progress")
	check(DirAccess.remove_absolute(IdentityStore.DIRECTORY) == OK, "remove the now-empty test-owned profile directory")
	main._resume()
	main.set_process(false)

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
