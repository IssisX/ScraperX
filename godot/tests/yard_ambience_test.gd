extends SceneTree

var _failures := 0

func _initialize() -> void:
	_run.call_deferred()

func _check(value: bool, message: String) -> void:
	if not value:
		_failures += 1
		push_error(message)

func _frames(count: int) -> void:
	for i in count:
		await process_frame

func _run() -> void:
	# This rejects missing spatial output, a broken seam, and voices that
	# survive ground-distance culling or user mute.
	var script := load("res://presentation/audio/yard_ambience.gd")
	_check(script != null, "yard ambience implementation exists")
	if script == null:
		quit(1)
		return
	var index := AudioServer.get_bus_index(&"Ambience")
	if index < 0:
		AudioServer.add_bus()
		index = AudioServer.bus_count - 1
		AudioServer.set_bus_name(index, &"Ambience")
	var capture := AudioEffectCapture.new()
	AudioServer.add_bus_effect(index, capture)
	var world := Node3D.new()
	root.add_child(world)
	var camera := Camera3D.new()
	world.add_child(camera)
	camera.current = true
	camera.position = Vector3(-36, 2, -26)
	var yard: Node3D = script.new()
	world.add_child(yard)
	for i in 300:
		await process_frame
		if yard.get("_built"):
			break
	_check(yard.get("_built"), "bounded loop generation completes")
	var voices: Array = yard.get("_voices")
	_check(voices.size() == 2, "two bounded spatial environmental voices")
	var hashes := []
	for voice in voices:
		var wav: AudioStreamWAV = voice.stream
		_check(wav != null and wav.loop_mode == AudioStreamWAV.LOOP_FORWARD, "environment bed loops")
		if wav == null:
			continue
		hashes.append(hash(wav.data))
		var first := wav.data.decode_s16(0) / 32768.0
		var last := wav.data.decode_s16(wav.data.size() - 2) / 32768.0
		var difference_energy := 0.0
		for sample in range(1, wav.data.size() / 2):
			var difference := (wav.data.decode_s16(sample * 2) - wav.data.decode_s16((sample - 1) * 2)) / 32768.0
			difference_energy += difference * difference
		var normal_difference := sqrt(difference_energy / (wav.data.size() / 2 - 1))
		_check(absf(first - last) < normal_difference * 4.0, "loop seam stays within the texture's normal sample changes")
		_check(wav.get_length() >= 4.0, "loop has room for slow environmental movement")
	_check(hashes.size() == 2 and hashes[0] != hashes[1], "locations have distinct air textures")
	yard.update(1.0, camera.position)
	await _frames(12)
	capture.clear_buffer()
	await _frames(45)
	var peak := 0.0
	for frame in capture.get_buffer(capture.get_frames_available()):
		peak = maxf(peak, maxf(absf(frame.x), absf(frame.y)))
	if not Engine.get_write_movie_path().is_empty():
		_check(peak > 0.005, "near-shed spatial voice produces actual mixer output")
	else:
		_check(not voices[0].playing, "Dummy driver does not retain playback")
	camera.position = Vector3(36, 2, -52)
	yard.update(1.0, camera.position)
	await _frames(12)
	capture.clear_buffer()
	await _frames(45)
	var salvage_peak := 0.0
	for frame in capture.get_buffer(capture.get_frames_available()):
		salvage_peak = maxf(salvage_peak, maxf(absf(frame.x), absf(frame.y)))
	if not Engine.get_write_movie_path().is_empty():
		_check(salvage_peak > 0.005, "near-salvage spatial voice produces actual mixer output")
	yard.update(1.0, Vector3(-36, 90, -30))
	_check(not voices[0].playing and not voices[1].playing, "upper ascent culls ground voices")
	yard.update(1.0, Vector3(400, 2, 400))
	_check(not voices[0].playing and not voices[1].playing, "distant yard voices stop")
	AudioServer.set_bus_mute(index, true)
	yard.update(1.0, camera.position)
	_check(not voices[0].playing and not voices[1].playing, "user ambience mute stops voices")
	await _frames(12)
	capture.clear_buffer()
	await _frames(12)
	var mute_peak := 0.0
	for frame in capture.get_buffer(capture.get_frames_available()):
		mute_peak = maxf(mute_peak, maxf(absf(frame.x), absf(frame.y)))
	_check(mute_peak < 0.00001, "muted environmental voices leave no mixer output")
	AudioServer.set_bus_mute(index, false)
	AudioServer.set_bus_mute(0, true)
	yard.update(1.0, camera.position)
	_check(not voices[0].playing and not voices[1].playing, "master mute stops voices")
	AudioServer.set_bus_mute(0, false)
	yard.update(1.0, camera.position)
	yard.quiesce()
	_check(not voices[0].playing and not voices[1].playing, "quiesce stops voices before mixer shutdown")
	await _frames(3)
	world.queue_free()
	await _frames(2)
	print("SCRAPERX_YARD_AMBIENCE failures=%d shed_peak=%.5f salvage_peak=%.5f mute_peak=%.5f movie=%s" % [_failures, peak, salvage_peak, mute_peak, Engine.get_write_movie_path()])
	quit(0 if _failures == 0 else 1)
