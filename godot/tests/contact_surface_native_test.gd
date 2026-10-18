extends Node

# Initial footing is staged. Subsequent walking, Jump, sound selection and
# support loss run through the shipping Main/input/native path.
const STEP := 1.0 / 90.0
var main: Node
var native: Object
var audio: Node
var capture_dir := ""
var checks := 0
var failures := 0
var finished := false
var observed_steps := 0
var observed_landings := 0


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	if "--quit-after-contact-sound-check" in OS.get_cmdline_user_args():
		_run.call_deferred()
	else:
		var start := Button.new()
		start.name = "RunContactSoundCheck"
		start.text = "Run ScraperX contact-linked footsteps"
		start.position = Vector2(24, 80)
		start.pressed.connect(func() -> void:
			start.hide()
			_run.call_deferred())
		add_child(start)


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_CONTACT_SOUND FAIL " + why)


func key(code: Key, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.pressed = pressed
	get_viewport().push_input(event, true)


func step(count: int, expected_sound: StringName = &"", expected_landing_surface: int = -1) -> void:
	var before_tick: int = native.get_tick_index()
	for _i in count:
		var before_steps: int = audio.steps
		var voice_index: int = audio._next_voice
		var before_landing: Dictionary = native.get_landing_state()
		main._process(STEP)
		if audio.steps > before_steps and not expected_sound.is_empty():
			observed_steps += 1
			var voice: AudioStreamPlayer = audio._voices[voice_index]
			var clips: Array = audio._bank.clips[expected_sound]
			check(voice.stream in clips and voice.playing,
				"real ordinary footstep plays recorded " + str(expected_sound))
		var after_landing: Dictionary = native.get_landing_state()
		if expected_landing_surface >= 0 and int(after_landing.landing_count) > int(before_landing.landing_count):
			observed_landings += 1
			check(int(after_landing.landing_sound_surface) == expected_landing_surface,
				"actual rigid impact latches its own surface")
			check(float(after_landing.landing_normal_speed_mps) >= 2.0,
				"real Jump return exceeds the existing audible impact boundary")
			var body: AudioStreamPlayer = audio._voices[voice_index]
			var body_key := &"land_hard" if float(after_landing.landing_normal_speed_mps) >= 7.0 else &"land_soft"
			check(body.stream in audio._bank.clips[body_key] and body.playing,
				"actual landing plays the existing severity-driven body sound")
			var expected_voices := 1
			if expected_landing_surface == 2:
				expected_voices = 2
				var accent: AudioStreamPlayer = audio._voices[(voice_index + 1) % audio._voices.size()]
				check(accent.stream in audio._bank.clips[&"step_metal"] and accent.playing,
					"real steel landing adds the recorded metal contact")
				check(is_equal_approx(accent.volume_db, body.volume_db - 9.0),
					"metal contact stays below the body impact")
			check(audio._next_voice == (voice_index + expected_voices) % audio._voices.size(),
				"one actual event starts only its bounded landing voices")
	check(native.get_tick_index() == before_tick + count, "exact native input time")
	check(native.get_death_count() == 0, "no hidden death or reset")


func capture(label: String) -> void:
	if capture_dir.is_empty():
		return
	check(DisplayServer.get_name() != "headless", "capture uses a renderer")
	if DisplayServer.get_name() == "headless":
		return
	DirAccess.make_dir_recursive_absolute(capture_dir)
	var tick: int = native.get_tick_index()
	await RenderingServer.frame_post_draw
	var picture := get_viewport().get_texture().get_image()
	check(picture != null and not picture.is_empty(), "actual viewport: " + label)
	if picture != null and not picture.is_empty():
		check(picture.save_png(capture_dir.path_join(label + ".png")) == OK, "save " + label)
	check(native.get_tick_index() == tick, "rendering does not advance input time")


func _run() -> void:
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	main._yaw = 0.0
	main._pitch = -0.4
	native = main._native
	audio = main._audio
	check(native != null and not main._regression_scene, "shipping Main and normal native world")
	if native == null:
		_finish()
		return
	# Wait for the real bank's completion, not a fabricated audio-ready flag.
	var bank_deadline := Time.get_ticks_msec() + 30000
	while not audio._bank_ready and Time.get_ticks_msec() < bank_deadline:
		await get_tree().process_frame
	check(audio._bank_ready, "actual recorded sound bank ready")
	if not audio._bank_ready:
		_finish()
		return
	# Dummy still drives actual players/streams; it cannot prove heard sound.
	audio._silent = false
	step(45)
	var arrival: Vector3 = native.get_player_position()
	check(native.get_landing_state().has("support_sound_surface"), "native bridge exposes current support sound")
	check(native.get_landing_state().has("landing_sound_surface"), "native bridge exposes latched impact sound")
	for example in [
		[Vector3(-72.0, 3.3, -62.0), 2, &"step_metal", "steel-gallery"],
		[Vector3(-66.0, 2.1, -90.0), 1, &"step_concrete", "concrete-platform"],
	]:
		main._router.clear_held()
		check(native.debug_restart_at(example[0]), "stage initial " + str(example[3]) + " footing")
		check(int(native.get_landing_state().support_sound_surface) == 0,
			"restart cannot retain the previous contact's sound tag")
		step(45)
		check(native.is_player_grounded() and native.get_support_entity_id() == 51,
			"actual world-solid footing on " + str(example[3]))
		check(int(native.get_landing_state().support_sound_surface) == example[1],
			"two supports sharing entity51 retain their distinct explicit material")
		var before_steps: int = observed_steps
		key(KEY_W, true)
		step(40, example[2])
		key(KEY_W, false)
		check(observed_steps > before_steps, "ordinary movement emits an actual selected footstep")
		step(30)
		await capture(str(example[3]))
		if example[1] == 1:
			key(KEY_SPACE, true)
			key(KEY_SPACE, false)
			step(8)
			step(100, &"", 1)
			check(native.is_player_grounded(), "concrete Jump returns to the real platform")
	# The same high-line perch keeps steel identity through loss/reacquisition.
	check(native.debug_restart_at(Vector3(-69.8, 3.95, -67.5)), "stage real upper-gallery perch")
	step(45)
	check(native.is_player_grounded() and int(native.get_landing_state().support_sound_surface) == 2,
		"supported high-line perch carries actual steel sound")
	key(KEY_SPACE, true)
	key(KEY_SPACE, false)
	step(8)
	check(not native.is_player_grounded() and int(native.get_landing_state().support_sound_surface) == 0,
		"airborne body has no remembered steel contact")
	var airborne_steps: int = audio.steps
	step(8)
	check(audio.steps == airborne_steps, "no ordinary footsteps in free flight")
	step(100, &"", 2)
	check(native.is_player_grounded() and int(native.get_landing_state().support_sound_surface) == 2,
		"ordinary gravity reacquires the actual steel perch")
	var impact_count: int = native.get_landing_state().landing_count
	key(KEY_SPACE, true)
	key(KEY_SPACE, false)
	step(8)
	var airborne_receipt: Dictionary = native.get_landing_state()
	check(not native.is_player_grounded() and int(airborne_receipt.support_sound_surface) == 0 and
		int(airborne_receipt.landing_sound_surface) == 2 and int(airborne_receipt.landing_count) == impact_count,
		"leaving steel clears current footing while preserving the exact earlier impact receipt")
	check(observed_landings == 2, "both real concrete and steel Jump landings were observed")
	# An unannotated real arrival support remains Unknown for legacy fallback.
	# Debug relocation requires2cm clearance; the solved standing capsule
	# touches the floor. Reacquire that same real surface from5cm above it.
	check(native.debug_restart_at(arrival + Vector3.UP * 0.05), "return above actual unannotated arrival footing")
	step(45)
	check(native.is_player_grounded() and int(native.get_landing_state().support_sound_surface) == 0,
		"untagged native floor retains fallback without a guessed surface")
	_finish()


func _finish() -> void:
	finished = true
	if audio != null:
		audio.quiesce()
	print("SCRAPERX_CONTACT_SOUND %s checks=%d failures=%d actual_steps=%d actual_landings=%d staged_footing=1 heard_audio=0" % [
		"PASS" if failures == 0 else "FAIL", checks, failures, observed_steps, observed_landings])
	if "--quit-after-contact-sound-check" in OS.get_cmdline_user_args():
		get_tree().quit(1 if failures else 0)
