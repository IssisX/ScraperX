extends SceneTree

const Director := preload("res://presentation/audio/audio_director.gd")
const Reactions := preload("res://presentation/audio/fall_reactions.gd")

func _initialize() -> void:
	root.size = Vector2i(128, 128)
	root.content_scale_size = Vector2i(128, 128)
	root.disable_3d = true
	_run.call_deferred()

func _run() -> void:
	if Engine.get_write_movie_path().is_empty():
		push_error("[DEBUG-audio-gate] actual mixer required")
		quit(1)
		return
	var director := Director.new()
	root.add_child(director)
	director.set_volumes(1.0, 1.0, 0.0, 0.0)
	var effects := AudioEffectCapture.new()
	var master := AudioEffectCapture.new()
	effects.buffer_length = 1.0
	master.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Effects"), effects)
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Master"), master)
	var voice: AudioStreamPlayer = director.fall_reactions._voice
	print("[DEBUG-audio-gate] gain_db=%.2f master_effects=%d" % [voice.volume_db, AudioServer.get_bus_effect_count(0)])
	for clip in Reactions.ALARM + Reactions.PANIC:
		voice.stop()
		for i in 3:
			await process_frame
		effects.clear_buffer()
		master.clear_buffer()
		voice.stream = clip
		voice.play()
		var effects_peak := 0.0
		var master_peak := 0.0
		var samples := 0
		for i in ceili((clip.get_length() + 0.08) * 60.0):
			await process_frame
			for sample in effects.get_buffer(effects.get_frames_available()):
				effects_peak = maxf(effects_peak, maxf(absf(sample.x), absf(sample.y)))
			for sample in master.get_buffer(master.get_frames_available()):
				master_peak = maxf(master_peak, maxf(absf(sample.x), absf(sample.y)))
				samples += 1
		print("[DEBUG-audio-gate] clip=%s effects_peak=%.9f master_peak=%.9f master_db=%.3f samples=%d" % [clip.resource_path.get_file(), effects_peak, master_peak, linear_to_db(maxf(master_peak, 0.000001)), samples])
	director.quiesce()
	for i in 4:
		await process_frame
	director.queue_free()
	await process_frame
	quit(0)
