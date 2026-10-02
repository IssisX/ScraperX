extends SceneTree

# Movie Maker drives the real mixer even when the driver reports Dummy.
# Clip fixtures are isolated from gameplay. Gameplay then uses viewport
# touch input, the shipping main scene, and unmodified native readback.
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
var _main: Node3D
var _clip_player: AudioStreamPlayer
var _effects: AudioEffectCapture
var _master: AudioEffectCapture
var _effects_samples := PackedFloat32Array()
var _master_samples := PackedFloat32Array()
var _checks := 0
var _failures: Array[String] = []
var _mix_peak := 0.0
var _peak_native_speed := 0.0
var _shot_energies: Array[float] = []
var _played_gameplay_clips := {}
var _touch_events := 0


func _initialize() -> void:
	root.size = Vector2i(432, 371)
	root.content_scale_size = Vector2i(432, 371)
	root.disable_3d = true # Audio proof; no rendered-geometry claim.
	_run.call_deferred()


func _check(ok: bool, message: String) -> bool:
	_checks += 1
	if not ok:
		_failures.append(message)
		push_error("SCRAPERX_SLINGSHOT_AUDIO FAIL " + message)
	return ok


func _touch(index: int, at: Vector2, pressed: bool) -> void:
	_touch_events += 1
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	root.push_input(event, true)


func _action() -> void:
	var at: Vector2 = _main._touch.button_center(&"action")
	_touch(1, at, true)
	_touch(1, at, false)


func _move_back(held: bool) -> void:
	if not is_instance_valid(_main):
		return
	var home: Vector2 = _main._touch.stick_home()
	_touch(0, home, held)
	if held:
		var throw: float = TouchControls.STICK_THROW * float(_main._touch._u)
		for step in 3:
			_touch_events += 1
			var event := InputEventScreenDrag.new()
			event.index = 0
			event.position = home + Vector2.DOWN * throw * float(step + 1) / 3.0
			event.relative = Vector2.DOWN * throw / 3.0
			root.push_input(event, true)


func _frames(count: int) -> void:
	for i in count:
		await process_frame
		_drain()


func _drain() -> void:
	if _effects == null or _master == null:
		return
	if is_instance_valid(_main) and _main._native != null:
		_peak_native_speed = maxf(_peak_native_speed, _main._native.get_player_linear_velocity().length())
		for name in [&"band_stretch", &"sling_release", &"rubber_squeak", &"sling_wobble", &"bullet_time"]:
			for voice in _main._audio._voices + [_main._audio._bullet_time]:
				if voice.playing and voice.stream in _main._audio._bank.clips.get(name, []):
					_played_gameplay_clips[name] = true
	for frame in _effects.get_buffer(_effects.get_frames_available()):
		_effects_samples.append(frame.x)
		_effects_samples.append(frame.y)
	for frame in _master.get_buffer(_master.get_frames_available()):
		_master_samples.append(frame.x)
		_master_samples.append(frame.y)


func _flush() -> void:
	_effects.clear_buffer()
	_master.clear_buffer()
	_effects_samples = PackedFloat32Array()
	_master_samples = PackedFloat32Array()


func _seconds(seconds: float) -> void:
	var clock := 0.0
	while clock < seconds:
		await process_frame
		clock += root.get_process_delta_time()
		_drain()


func _wait(condition: Callable, seconds: float) -> bool:
	var clock := 0.0
	while not bool(condition.call()) and clock < seconds:
		await process_frame
		clock += root.get_process_delta_time()
		_drain()
	return bool(condition.call())


func _levels(samples: PackedFloat32Array) -> Vector2:
	var peak := 0.0
	var energy := 0.0
	for sample in samples:
		if not is_finite(sample):
			return Vector2(INF, INF)
		peak = maxf(peak, absf(sample))
		energy += sample * sample
	return Vector2(peak, sqrt(energy / maxf(1.0, samples.size())))


func _receipt(name: String, audible: bool = true) -> void:
	var effects := _levels(_effects_samples)
	var master := _levels(_master_samples)
	_mix_peak = maxf(_mix_peak, master.x)
	_check(not _master_samples.is_empty(), name + " captured actual mixer samples")
	_check(is_finite(master.x) and master.x < db_to_linear(-0.5), name + " post-limiter mix does not clip")
	if audible:
		_check(effects.x > 0.001 and effects.y > 0.0001, name + " Effects output is audible")
	print("SCRAPERX_SLINGSHOT_AUDIO_PHASE name=%s effects_samples=%d effects_peak=%.6f effects_rms=%.6f master_peak_db=%.2f" % [
		name, _effects_samples.size(), effects.x, effects.y, linear_to_db(maxf(master.x, 1.0e-9))])


func _new_scene() -> bool:
	_main = load("res://main.tscn").instantiate()
	root.add_child(_main)
	_main.set_process(false)
	if not _check(_main._native != null and _main._slingshot_view != null, "shipping native launcher and scene load"):
		return false
	_main._router.capture_mouse = false
	_main._router.set_device(InputRouter.Device.TOUCH)
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	_main._settings.launch_cinematics = false
	var started := Time.get_ticks_msec()
	while not bool(_main._audio._bank_ready) and Time.get_ticks_msec() - started < 30000:
		await process_frame
	if not _check(bool(_main._audio._bank_ready) and not bool(_main._audio._silent), "bank ready with Movie Maker mixing enabled"):
		return false
	_main._audio.set_volumes(1.0, 1.0, 0.0, 1.0)
	return true


func _clip_fixtures() -> void:
	_clip_player = AudioStreamPlayer.new()
	_clip_player.bus = &"Effects"
	root.add_child(_clip_player)
	for name in [&"band_stretch", &"sling_release", &"rubber_squeak", &"sling_wobble", &"bullet_time"]:
		var variants: Array = _main._audio._bank.clips.get(name, [])
		_check(not variants.is_empty(), "bank includes " + String(name))
		for index in variants.size():
			var clip: AudioStreamWAV = variants[index]
			var decoded := PackedFloat32Array()
			for byte in range(0, clip.data.size(), 2):
				decoded.append(float(clip.data.decode_s16(byte)) / 32767.0)
			var raw := _levels(decoded)
			_check(clip.format == AudioStreamWAV.FORMAT_16_BITS and clip.mix_rate > 0 and raw.x > 0.01 and raw.x <= 0.901,
				"clip PCM is decoded, nonzero and bounded: %s_%d" % [name, index])
			_flush()
			_clip_player.stream = clip
			_clip_player.play()
			await _seconds(clip.get_length() + 0.15)
			_receipt("clip_fixture_%s_%d" % [name, index])
	_clip_player.stop()
	_clip_player.stream = null


func _shot(name: String, fraction: float, cinematic: bool = false) -> bool:
	_main._settings.launch_cinematics = cinematic
	_main._settings.head_bob = true
	_main.set_process(true)
	var native: Object = _main._native
	var station: Vector3 = native.get_slingshot_state()["neutral_position"]
	_check(native.debug_restart_at(Vector3(station.x, 0.92, station.z - 3.0)), name + " explicit outside-pouch test staging accepted")
	_main._yaw = 0.0
	_main._pitch = 0.0
	await _frames(3)
	_move_back(true)
	var arrived := await _wait(func() -> bool: return bool(native.get_slingshot_state()["station_available"]), 4.0)
	_move_back(false)
	if not _check(arrived, name + " ordinary backwards input reaches native pouch"):
		return false
	await _frames(3)
	_action()
	if not _check(await _wait(func() -> bool: return bool(native.get_slingshot_state()["seated"]), 1.0), name + " viewport Action boards real harness"):
		return false
	await _seconds(0.6)
	var draw_before: int = _main._audio.sling_draw_cues
	var releases_before: int = _main._audio.sling_releases
	var comedy_before: int = _main._audio.sling_comedy_cues
	var launches_before := int(native.get_slingshot_state()["launch_count"])
	_action()
	await _seconds(0.25)
	_check(int(native.get_slingshot_state()["launch_count"]) == launches_before and bool(native.get_slingshot_state()["seated"]),
		name + " weak-draw touch Action keeps the native harness seated")
	_check(_main._audio.sling_releases == releases_before and _main._audio.sling_comedy_cues == comedy_before,
		name + " rejected weak-draw Action produces no release or comedy cue")
	var max_draw := float(native.get_slingshot_state()["max_draw_m"])
	var target := minf(max_draw * fraction, max_draw - 0.025) # Native stop's finite contact clearance.
	_flush()
	_played_gameplay_clips.clear()
	_move_back(true)
	var charged := await _wait(func() -> bool:
		var reading: Dictionary = native.get_slingshot_state()
		return float(reading["draw_m"]) >= target and bool(reading.get("release_ready", false)), 15.0)
	_move_back(false)
	await _seconds(0.3)
	var held_draw_cues: int = _main._audio.sling_draw_cues
	var held_comedy_cues: int = _main._audio.sling_comedy_cues
	await _seconds(0.35)
	_check(_main._audio.sling_draw_cues == held_draw_cues and _main._audio.sling_comedy_cues == held_comedy_cues,
		name + " holding funded draw does not repeat stretch or comedy cues")
	var state: Dictionary = native.get_slingshot_state()
	_check(_main._router.device == InputRouter.Device.TOUCH, name + " actual viewport input remains on the touch device")
	_check(charged and float(state["energy_j"]) > 1.0 and float(state["work_j"]) > 1.0, name + " manual input produces real native stretch/work")
	_check(bool(state.get("release_ready", false)), name + " native energy/track-exit gate permits release")
	if fraction < 0.5:
		_check(float(state["draw_m"]) < max_draw - 0.25, "partial-draw shot remains below full native stop")
	_check(_main._audio.sling_draw_cues > draw_before and _main._audio.sling_releases == releases_before,
		name + " draw cues fire before any native release")
	_check(bool(_played_gameplay_clips.get(&"band_stretch", false)), name + " native draw plays the real stretch clip")
	var expected_draw_comedy := 1 if float(state["draw_m"]) > max_draw * 0.70 else 0
	_check(_main._audio.sling_comedy_cues == comedy_before + expected_draw_comedy,
		name + " rubber squeak fires once at the actual 70-percent native draw crossing")
	_check(bool(_played_gameplay_clips.get(&"rubber_squeak", false)) == (expected_draw_comedy == 1),
		name + " native threshold selects the actual rubber-squeak clip")
	_receipt(name + "_native_draw")
	var draw := float(state["draw_m"])
	var energy := float(state["energy_j"])
	_shot_energies.append(energy)
	_flush()
	_peak_native_speed = 0.0
	_action()
	_check(await _wait(func() -> bool: return int(native.get_slingshot_state()["launch_count"]) == launches_before + 1, 1.0),
		name + " viewport Action releases native spring")
	await _seconds(0.9)
	state = native.get_slingshot_state()
	_check(_main._audio.sling_releases == releases_before + 1, name + " one native launch produces one release cue")
	_check(_main._audio.sling_comedy_cues == comedy_before + expected_draw_comedy + 1,
		name + " one real native launch produces one mechanical wobble cue")
	if fraction > 0.5:
		_check(_main._audio.sling_comedy_cues - comedy_before == 2, "full touch draw and launch produce exactly two comedy cues")
	_check(bool(_played_gameplay_clips.get(&"sling_release", false)), name + " native release plays actual release clip")
	_check(bool(_played_gameplay_clips.get(&"sling_wobble", false)), name + " native release plays actual mechanical-wobble clip")
	_check(_peak_native_speed > 3.0, name + " physical spring release accelerates native rider")
	if fraction > 0.5:
		_check(bool(state["released"]) and native.get_player_linear_velocity().length() > 12.0, name + " funded high draw produces sustained native flight")
	_receipt(name + "_native_release")
	if cinematic:
		_check(_main._audio.sling_cinematic_cues == 1 and _main._audio._bullet_time.playing,
			"real native release starts the reserved cinematic sound once")
		_main._audio._voices.map(func(voice: AudioStreamPlayer) -> void: voice.stop())
		_flush()
		await _seconds(0.35)
		_receipt("native_cinematic_sound_isolation")
		_check(_main._audio._bullet_time.playing, "cinematic cue survives ordinary voice-pool reuse")
		var cues_before := int(_main._audio.sling_cinematic_cues)
		var pause_at: Vector2 = _main._touch.button_center(&"pause")
		_touch(9, pause_at, true)
		_touch(9, pause_at, false)
		await _frames(3)
		var paused_tick := int(native.get_tick_index())
		_check(paused and not _main._audio._bullet_time.playing and _main._slingshot_view.get_simulation_scale() == 1.0,
			"mid-shot viewport Pause stops cinema sound and restores the native time request")
		await _frames(3)
		_check(int(native.get_tick_index()) == paused_tick, "mid-shot pause freezes actual native stepping")
		var resume_at: Vector2 = _main._pause_menu.resume_button_center()
		_touch(9, resume_at, true)
		_touch(9, resume_at, false)
		await _frames(3)
		_check(not paused and not _main._audio._bullet_time.playing and int(_main._audio.sling_cinematic_cues) == cues_before,
			"viewport Resume cannot replay the same physical launch's sound")
		_main._settings.launch_cinematics = false
		await _frames(2)
		_check(not _main._audio._bullet_time.playing and _main._audio._cinematic_air_duck_db == 0.0,
			"comfort cancellation stops cinematic audio and restores the air mix")
	_main._audio.set_volumes(1.0, 0.0, 1.0, 1.0)
	_flush()
	await _seconds(0.45)
	var flight_mix := _levels(_master_samples)
	if fraction > 0.5:
		_check(_main._audio._sling_air_speed > 12.0 and _main._audio._rush.volume_db > -30.0,
			name + " authoritative release velocity opens speed-rush playback")
	var air_state: Dictionary = native.get_slingshot_state()
	var expected_air_speed: float = native.get_player_linear_velocity().length() if bool(air_state["released"]) else 0.0
	_check(absf(float(_main._audio._sling_air_speed) - expected_air_speed) < 0.001,
		name + " rush input matches genuine native release velocity")
	_check(flight_mix.y > 0.001, name + " speed-rush and air are genuinely mixed")
	_receipt(name + "_native_flight", false)
	if fraction > 0.5:
		# Isolate the actual rush voice after the native flight set its gain.
		# Stop scene processing and other ambience; never invent velocity/state.
		_main.set_process(false)
		for child in _main._audio.get_children():
			if (child is AudioStreamPlayer or child is AudioStreamPlayer3D) and child.bus == &"Ambience" and child != _main._audio._rush:
				child.stop()
		_flush()
		await _seconds(0.3)
		var rush := _levels(_master_samples)
		_check(_main._audio._rush.playing and rush.y > 0.001, "real native-speed rush voice produces isolated mixer output")
		_receipt("native_speed_rush_isolation", false)
	print("SCRAPERX_SLINGSHOT_AUDIO_NATIVE shot=%s draw_m=%.4f energy_j=%.1f source_limit_w=%.1f draw_cues=%d release_cues=%d comedy_cues=%d launches=%d speed_mps=%.3f peak_release_speed_mps=%.3f rush_db=%.2f touch_device=%s" % [
		name, draw, energy, state["max_source_power_w"], _main._audio.sling_draw_cues - draw_before,
		_main._audio.sling_releases - releases_before, _main._audio.sling_comedy_cues - comedy_before,
		state["launch_count"], native.get_player_linear_velocity().length(), _peak_native_speed,
		_main._audio._rush.volume_db, _main._router.device == InputRouter.Device.TOUCH])
	return true


func _dispose_scene() -> void:
	_move_back(false)
	if _main != null:
		_main.set_process(false)
		_main._audio.quiesce()
		await _frames(4) # Release playbacks while Movie Maker still mixes.
		_main.queue_free()
		await _frames(2)
		_main = null


func _run() -> void:
	if not _check(not Engine.get_write_movie_path().is_empty(), "run under --write-movie for actual mixer proof"):
		quit(31)
		return
	if not await _new_scene():
		await _dispose_scene()
		quit(31)
		return
	_effects = AudioEffectCapture.new()
	_master = AudioEffectCapture.new()
	_effects.buffer_length = 1.0
	_master.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Effects"), _effects)
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Master"), _master)
	await _clip_fixtures()
	await _shot("low_draw", 0.25)
	await _dispose_scene()
	if await _new_scene():
		await _shot("full_draw", 1.0)
	await _dispose_scene()
	if await _new_scene():
		await _shot("cinematic_draw", 1.0, true)
	await _dispose_scene()
	if await _new_scene():
		await _fear_mix()
	await _dispose_scene()
	_check(_shot_energies.size() == 3 and _shot_energies[1] > _shot_energies[0], "full native draw funds more energy than partial draw")
	_clip_player.queue_free()
	await _frames(3)
	if _failures.is_empty():
		print("SCRAPERX_SLINGSHOT_AUDIO PASS checks=%d mix_peak_db=%.2f driver=%s touch_events=%d movie=%s" % [
			_checks, linear_to_db(maxf(_mix_peak, 1.0e-9)), AudioServer.get_driver_name(), _touch_events, Engine.get_write_movie_path()])
	else:
		print("SCRAPERX_SLINGSHOT_AUDIO FAIL checks=%d failures=%d" % [_checks, _failures.size()])
	quit(0 if _failures.is_empty() else 31)


func _phone_rms(samples: PackedFloat32Array) -> float:
	var low := 0.0
	var energy := 0.0
	var alpha := 1.0 - exp(-TAU * 300.0 / AudioServer.get_mix_rate())
	# Samples are interleaved stereo; their channels carry the same centered
	# voice. Use one channel for a consistent phone-band high-pass measure.
	for i in range(0, samples.size(), 2):
		low += alpha * (samples[i] - low)
		energy += pow(samples[i] - low, 2)
	return sqrt(energy / maxf(1.0, samples.size() / 2.0))


func _fear_mix() -> void:
	_main._audio.set_volumes(1.0, 1.0, 1.0, 1.0)
	var ambience := AudioEffectCapture.new()
	ambience.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Ambience"), ambience)
	_check(_main._native.debug_restart_at(Vector3(70, 1000, -25)), "fear mix stages a clear native gravity fall")
	_main.set_process(true)
	var voice_samples := PackedFloat32Array()
	var bed_samples := PackedFloat32Array()
	var peak := 0.0
	for i in 210:
		await process_frame
		var effects := _effects.get_buffer(_effects.get_frames_available())
		var bed := ambience.get_buffer(ambience.get_frames_available())
		for frame in _master.get_buffer(_master.get_frames_available()):
			peak = maxf(peak, maxf(absf(frame.x), absf(frame.y)))
		if _main._audio.fall_reactions.speaking():
			for frame in effects:
				voice_samples.append(frame.x)
				voice_samples.append(frame.y)
			for frame in bed:
				bed_samples.append(frame.x)
				bed_samples.append(frame.y)
	var voice_rms := _phone_rms(voice_samples)
	var bed_rms := _phone_rms(bed_samples)
	_check(_main._audio.fall_reactions.reactions >= 1 and voice_rms > 0.01,
		"actual gravity fall plays a decoded human reaction with phone-band presence")
	_check(voice_rms > bed_rms * 2.0, "human fear voice stands at least 6 dB above the ducked air bed")
	_check(peak < db_to_linear(-0.5), "louder fear mix stays below the master limiter ceiling")
	print("SCRAPERX_FEAR_MIX voice_phone_db=%.2f bed_phone_db=%.2f peak_db=%.2f" % [
		linear_to_db(maxf(voice_rms, 0.000001)), linear_to_db(maxf(bed_rms, 0.000001)), linear_to_db(maxf(peak, 0.000001))])
	AudioServer.remove_bus_effect(AudioServer.get_bus_index(&"Ambience"), AudioServer.get_bus_effect_count(AudioServer.get_bus_index(&"Ambience")) - 1)
