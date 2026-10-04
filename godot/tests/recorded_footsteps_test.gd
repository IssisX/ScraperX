extends SceneTree

const SoundBank := preload("res://presentation/audio/sound_bank.gd")
const Director := preload("res://presentation/audio/audio_director.gd")
const STEP_KEYS := [&"step_concrete", &"step_metal", &"step_earth"]
const RECORDED_ROOT := "res://assets/audio/footsteps/"

var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	var bank = SoundBank.new()
	bank.build()
	for key in STEP_KEYS:
		var variants: Array = bank.clips.get(key, [])
		if variants.size() != 4:
			_fail("%s exposes %d variants; expected four recorded steps" % [key, variants.size()])
			continue
		var seen_data: Array[PackedByteArray] = []
		for index in variants.size():
			var stream := variants[index] as AudioStreamWAV
			if stream == null:
				_fail("%s variant %d is not an AudioStreamWAV" % [key, index])
				continue
			if not stream.resource_path.begins_with(RECORDED_ROOT):
				_fail("%s variant %d is not loaded from the recorded footsteps assets: %s" % [key, index, stream.resource_path])
			if stream.data.is_empty():
				_fail("%s variant %d has no PCM data" % [key, index])
			if stream.format != AudioStreamWAV.FORMAT_16_BITS:
				_fail("%s variant %d is not 16-bit PCM" % [key, index])
			if stream.mix_rate < 16000 or stream.mix_rate > 48000:
				_fail("%s variant %d has an unsupported sample rate: %d" % [key, index, stream.mix_rate])
			var duration := stream.get_length()
			if duration < 0.035 or duration > 0.4:
				_fail("%s variant %d has an implausible single-step duration: %.3f s" % [key, index, duration])
			for prior_data in seen_data:
				if prior_data == stream.data:
					_fail("%s contains duplicate recorded variants" % key)
					break
			seen_data.append(stream.data)
	if _failures == 0:
		print("RECORDED_FOOTSTEPS PASS surfaces=3 variants=12 pcm=16-bit distinct=true")
	else:
		print("RECORDED_FOOTSTEPS FAIL failures=%d" % _failures)
	# Resource checks also run with Dummy audio. Movie Maker additionally
	# exercises every take through the production footstep controller, pool,
	# buses and Master limiter; the shipping scene's audio_mix remains a
	# separate native-input/machine-ambience integration gate.
	if _failures == 0 and not Engine.get_write_movie_path().is_empty():
		await _mix_all_variants()
	quit(1 if _failures > 0 else 0)


func _mix_all_variants() -> void:
	var audio := Director.new()
	root.add_child(audio)
	audio.set_volumes(0.8, 1.0, 0.8, 0.7)
	var started := Time.get_ticks_msec()
	while not audio._bank_ready:
		await process_frame
		if Time.get_ticks_msec() - started > 15000:
			_fail("mix sound bank did not finish building")
			audio.quiesce()
			audio.queue_free()
			return
	# Bus effects precede the Master fader. Capture is post-limiter and
	# pre-fader; default Master 0.8 lowers final output by 1.94 dB.
	# Prominence is unchanged and the captured peak gate is conservative.
	var capture := AudioEffectCapture.new()
	capture.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Master"), capture)
	await _capture_mix(audio, capture, 2.5, Vector3.ZERO, 1, 0.9)
	var standing := await _capture_mix(audio, capture, 1.0, Vector3.ZERO, 1, 0.9)
	var bed_db := _phone_band(standing)
	if bed_db < -40.0:
		_fail("mix ambience above 300 Hz is inaudible: %.2f dBFS" % bed_db)
	var cases := 0
	for surface in [[&"step_concrete", 1, 0.9], [&"step_metal", 11, 11.9], [&"step_earth", 51, 0.4]]:
		var key: StringName = surface[0]
		var variants: Array = audio._bank.clips[key]
		for index in variants.size():
			# Controlled selection is confined to this test. Playback and pitch
			# still follow the actual controller; no gameplay state is authored.
			audio._bank.clips[key] = [variants[index]]
			for phase in 3:
				seed(0xC04E + index * 3 + phase)
				await _capture_mix(audio, capture, 0.37 + phase * 0.04, Vector3.ZERO, int(surface[1]), float(surface[2]))
				var steps_before: int = audio.steps
				var walking := await _capture_mix(audio, capture, 0.6, Vector3(5.0, 0, 0), int(surface[1]), float(surface[2]))
				var steps: int = audio.steps - steps_before
				var body_db := _phone_band(walking, true)
				var peak := 0.0
				for sample in walking:
					peak = maxf(peak, absf(sample))
				var peak_db := linear_to_db(maxf(peak, 1.0e-9))
				cases += 1
				print("RECORDED_FOOTSTEPS_MIX case=%s/%d/%d bed_db=%.2f steps_db=%.2f peak_db=%.2f steps=%d" % [key, index, phase, bed_db, body_db, peak_db, steps])
				if steps < 3 or body_db < bed_db + 4.0 or peak_db > -0.5:
					_fail("mix %s/%d/%d lacks prominence or headroom: separation=%.2f dB peak=%.2f dBFS steps=%d" % [key, index, phase, body_db - bed_db, peak_db, steps])
		audio._bank.clips[key] = variants
	audio.quiesce()
	for _frame in 4:
		await process_frame
	AudioServer.remove_bus_effect(AudioServer.get_bus_index(&"Master"), AudioServer.get_bus_effect_count(AudioServer.get_bus_index(&"Master")) - 1)
	audio.queue_free()
	print("RECORDED_FOOTSTEPS_MIX %s cases=%d" % ["PASS" if _failures == 0 else "FAIL", cases])


func _capture_mix(audio: Node, capture: AudioEffectCapture, seconds: float,
		velocity: Vector3, support: int, height: float) -> PackedFloat32Array:
	var mono := PackedFloat32Array()
	var elapsed := 0.0
	while elapsed < seconds:
		var delta := root.get_process_delta_time()
		audio.update(delta, Vector3(0, height, -8), velocity, true, support, 0, false, 0, false)
		await process_frame
		elapsed += delta
		for frame in capture.get_buffer(capture.get_frames_available()):
			mono.append((frame.x + frame.y) * 0.5)
	return mono


func _phone_band(mono: PackedFloat32Array, loudest := false) -> float:
	# The shipping gate's two 300 Hz high-passes and 50 ms RMS windows.
	var a := exp(-TAU * 300.0 / AudioServer.get_mix_rate())
	var x1 := 0.0
	var y1 := 0.0
	var x2 := 0.0
	var y2 := 0.0
	var window := int(AudioServer.get_mix_rate() * 0.05)
	var count := 0
	var energy := 0.0
	var levels: Array[float] = []
	for sample in mono:
		var h1 := a * (y1 + sample - x1)
		x1 = sample
		y1 = h1
		var h2 := a * (y2 + h1 - x2)
		x2 = h1
		y2 = h2
		energy += h2 * h2
		count += 1
		if count == window:
			levels.append(linear_to_db(maxf(sqrt(energy / window), 1.0e-9)))
			count = 0
			energy = 0.0
	if levels.is_empty():
		return -200.0
	levels.sort()
	return levels[-1] if loudest else levels[levels.size() / 2]


func _fail(reason: String) -> void:
	_failures += 1
	push_error("RECORDED_FOOTSTEPS: " + reason)
