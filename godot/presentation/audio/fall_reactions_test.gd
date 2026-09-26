extends SceneTree
## Run with --headless --script res://presentation/audio/fall_reactions_test.gd.
## --write-movie also exercises decoding and the engine's actual audio mix.
const Reactions := preload("res://presentation/audio/fall_reactions.gd")
var failures := 0


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("FALL_VOICE_FAIL " + message)


func _run() -> void:
	AudioServer.add_bus()
	AudioServer.set_bus_name(AudioServer.bus_count - 1, &"Effects")
	var voice := Reactions.new()
	root.add_child(voice)
	# Controlled readback sequences isolate cancellation, repetition and thresholds.
	for i in 180:
		var t := float(i) / 90.0
		voice.update(1.0 / 90.0, Vector3(0, 1 + maxf(0, 5 * t - 4.905 * t * t), 0),
			Vector3(0, 5 - 9.81 * t, 0), t > 1.02, 0, false, 0)
	check(voice.reactions == 0, "ordinary jump produced a yell")
	var previous := ""
	for episode in 4:
		voice.update(4.0, Vector3(0, 100, 0), Vector3.ZERO, true, 0, false, 0)
		voice.update(0.01, Vector3(0, 100, 0), Vector3.ZERO, false, 0, false, 0)
		voice.update(0.6, Vector3(0, 94, 0), Vector3(0, -12, 0), false, 0, false, 0)
		check(voice.speaking(), "dangerous fall did not start a voice")
		check(voice.last_clip != previous, "immediately repeated alarm take")
		previous = voice.last_clip
		if episode == 0:
			paused = true
			await process_frame
			await process_frame
			check(not voice.speaking(), "pause did not stop voice")
			paused = false
			var count := voice.reactions
			voice.update(0.1, Vector3(0, 79, 0), Vector3(0, -20, 0), false, 0, false, 0)
			check(voice.reactions == count, "resume replayed first reaction")
		voice.update(0.01, Vector3(0, 94, 0), Vector3.ZERO,
			episode == 0, 1 if episode == 1 else 0, episode == 2, 1 if episode == 3 else 0)
		check(not voice.speaking(), "recovery did not cut voice")
	voice.update(4, Vector3(0, 300, 0), Vector3.ZERO, true, 0, false, 1)
	voice.update(0.01, Vector3(0, 300, 0), Vector3.ZERO, false, 0, false, 1)
	var count := voice.reactions
	voice.update(1, Vector3(0, 292, 0), Vector3(0, -13, 0), false, 0, false, 1)
	voice.update(4, Vector3(0, 220, 0), Vector3(0, -30, 0), false, 0, false, 1)
	voice.update(4, Vector3(0, 100, 0), Vector3(0, -40, 0), false, 0, false, 1)
	check(voice.reactions == count + 2, "long fall must have exactly two reactions")
	voice.quiesce()
	voice.queue_free()
	await process_frame
	# Use an actual native free fall through lethal impact and restoration.
	var native: RefCounted = ClassDB.instantiate("ScraperXSimulation")
	native.configure_regression_spawn(11)
	voice = Reactions.new()
	root.add_child(voice)
	var capture := AudioEffectCapture.new()
	capture.buffer_length = 10.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Effects"), capture)
	var peak := 0.0
	for frame in 300:
		native.advance_frame(1.0 / 60.0)
		voice.update(1.0 / 60.0, native.get_player_position(), native.get_player_linear_velocity(),
			native.is_player_grounded(), native.get_traversal_state(), native.is_parachute_deployed(),
			native.get_death_count())
		await process_frame
		var samples := capture.get_buffer(capture.get_frames_available())
		for sample in samples:
			peak = maxf(peak, maxf(absf(sample.x), absf(sample.y)))
	check(voice.reactions >= 1 and voice.reactions <= 2, "native high fall reaction count %d" % voice.reactions)
	check(not voice.speaking(), "death/landing left voice playing")
	if not Engine.get_write_movie_path().is_empty():
		check(peak > 0.01 and peak < 1.0, "decoded voice mix peak %f" % peak)
	var fall_reactions := voice.reactions
	voice.quiesce()
	voice.queue_free()
	await process_frame
	native = ClassDB.instantiate("ScraperXSimulation")
	native.configure_regression_spawn(11)
	voice = Reactions.new()
	root.add_child(voice)
	var deployed := false
	for frame in 150:
		native.advance_frame(1.0 / 60.0)
		voice.update(1.0 / 60.0, native.get_player_position(), native.get_player_linear_velocity(),
			native.is_player_grounded(), native.get_traversal_state(), native.is_parachute_deployed(), 0)
		if voice.reactions > 0 and not deployed:
			native.request_parachute()
			deployed = true
	check(deployed and native.is_parachute_deployed() and not voice.speaking(), "native canopy failed to cancel reaction")
	voice.quiesce()
	for i in 5:
		await process_frame
	native = null
	print("SCRAPERX_FALL_VOICE reactions=%d mix_peak=%.6f failures=%d" % [fall_reactions, peak, failures])
	quit(0 if failures == 0 else 1)
