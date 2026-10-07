extends SceneTree
## Run with --headless --script res://presentation/audio/fall_reactions_test.gd.
## --write-movie also exercises decoding and the engine's actual audio mix.
const Reactions := preload("res://presentation/audio/fall_reactions.gd")
const Director := preload("res://presentation/audio/audio_director.gd")
var failures := 0


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("FALL_VOICE_FAIL " + message)


func _run() -> void:
	var director := Director.new()
	root.add_child(director)
	# Keep the production buses/limiter; isolate recordings from synthesized ambience.
	director.quiesce()
	var mixing := AudioServer.get_driver_name() != "Dummy" or not Engine.get_write_movie_path().is_empty()
	var voice := Reactions.new()
	root.add_child(voice)
	# Reject repeats within complete cycles and at cycle boundaries in both pools.
	for clips in [Reactions.ALARM, Reactions.PANIC]:
		var bag: Array[int] = []
		var last := -1
		for cycle in 8:
			var seen: Array[int] = []
			for take in clips.size():
				var chosen: int = voice._choose(clips, bag, last)
				check(chosen != last, "repeated take at bag boundary")
				check(not seen.has(chosen), "repeated take before exhausting pool")
				seen.append(chosen)
				last = chosen
			check(seen.size() == clips.size() and bag.is_empty(), "incomplete recording cycle")
	voice.quiesce()
	voice.queue_free()
	await process_frame
	voice = Reactions.new()
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
	var master := AudioServer.get_bus_index(&"Master")
	var capture_index := AudioServer.get_bus_effect_count(master)
	# Appended after the Director's limiter: measure the output a player hears.
	AudioServer.add_bus_effect(master, capture)
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
	if mixing:
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
	voice.queue_free()
	for i in 5:
		await process_frame
	native = null
	# Isolated decoded-clip coverage, separate from the native behavioral proof above.
	# Use the real reaction player so its authored gain and Effects routing apply.
	var recordings := 0
	var recording_peak := 0.0
	if mixing:
		var recording_voice := Reactions.new()
		root.add_child(recording_voice)
		var player: AudioStreamPlayer = recording_voice._voice
		var clips: Array[AudioStream] = Reactions.ALARM + Reactions.PANIC
		for clip in clips:
			capture.clear_buffer()
			player.stream = clip
			player.play()
			var clip_peak := 0.0
			var elapsed := 0.0
			while elapsed < clip.get_length() + 0.15:
				await process_frame
				elapsed += root.get_process_delta_time()
				for sample in capture.get_buffer(capture.get_frames_available()):
					clip_peak = maxf(clip_peak, maxf(absf(sample.x), absf(sample.y)))
			check(clip_peak > 0.01 and clip_peak < 1.0,
				"decoded recording %s Master peak %f" % [clip.resource_path.get_file(), clip_peak])
			recordings += 1
			recording_peak = maxf(recording_peak, clip_peak)
			print("SCRAPERX_FALL_CLIP clip=%s mix_peak=%.6f" % [clip.resource_path.get_file(), clip_peak])
		check(recordings == clips.size(), "decoded every reaction recording")
		recording_voice.quiesce()
		recording_voice.queue_free()
	director.queue_free()
	for i in 5:
		await process_frame # Release stopped playbacks while Movie Maker still mixes.
	AudioServer.remove_bus_effect(master, capture_index)
	print("SCRAPERX_FALL_VOICE reactions=%d mix_peak=%.6f recordings=%d recording_peak=%.6f failures=%d" % [
		fall_reactions, peak, recordings, recording_peak, failures])
	quit(0 if failures == 0 else 1)
