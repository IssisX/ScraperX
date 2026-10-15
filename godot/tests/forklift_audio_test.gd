extends SceneTree

var failures := 0

func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("FORKLIFT_AUDIO " + message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var script = load("res://presentation/audio/forklift_audio.gd")
	check(script != null, "native-driven forklift sound implementation exists")
	if script == null:
		quit(1)
		return
	var bus := AudioServer.get_bus_index(&"Effects")
	if bus < 0:
		AudioServer.add_bus()
		bus = AudioServer.bus_count - 1
		AudioServer.set_bus_name(bus, &"Effects")
	var audio: Node3D = script.new()
	root.add_child(audio)
	var state := {"available": true, "forward_speed_mps": 0.0, "applied_drive": 0.0,
		"lift_target_mps": 0.0, "tilt_target_rad_s": 0.0, "lift_force_n": 0.0,
		"tilt_torque_nm": 0.0, "positive_work_j": 0.0}
	var at := Vector3(3, 1, 8)
	audio.update_from_native(state, at, false, 1.0 / 60.0)
	check(audio.get_child_count() == 4, "bounded four spatial sources")
	var clips := []
	for voice in audio.get_children():
		check(voice is AudioStreamPlayer3D and voice.bus == &"Effects", "machine source respects effects bus")
		check(not voice.playing, "idle truck has no fabricated motor or warning")
		check(voice.attenuation_model == AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE and voice.max_distance <= 64, "yard falloff is bounded")
		var wav: AudioStreamWAV = voice.stream
		check(wav != null and not wav.stereo and wav.loop_mode == AudioStreamWAV.LOOP_FORWARD, "cached mono PCM loop")
		var peak := 0
		for offset in range(0, wav.data.size(), 2):
			peak = maxi(peak, absi(wav.data.decode_s16(offset)))
		check(peak > 4000 and peak < 32767, "generated source has nonzero unclipped PCM")
		clips.append(wav)
	var drive: AudioStreamPlayer3D = audio.get_node("Drive")
	var hydraulic: AudioStreamPlayer3D = audio.get_node("Hydraulics")
	var reverse: AudioStreamPlayer3D = audio.get_node("ReverseWarning")
	var horn: AudioStreamPlayer3D = audio.get_node("Horn")
	audio.update_from_native(state, at, true)
	check(horn.playing and horn.volume_db > -20, "held horn starts actual player with usable gain")
	audio.update_from_native(state, at, false)
	check(not horn.playing, "released horn stops immediately")
	state.forward_speed_mps = 1.0
	audio.update_from_native(state, at, false)
	var slow_pitch := drive.pitch_scale
	check(drive.playing and not reverse.playing, "native forward motion produces drive sound only")
	state.forward_speed_mps = 3.0
	audio.update_from_native(state, at + Vector3.RIGHT, false)
	check(drive.pitch_scale > slow_pitch and audio.global_position.is_equal_approx(at + Vector3.RIGHT), "actual speed raises whine and source follows chassis")
	state.forward_speed_mps = -0.2
	audio.update_from_native(state, at, false)
	check(reverse.playing, "actual reverse motion enables warning")
	state.forward_speed_mps = 0.0
	state.applied_drive = -0.5
	audio.update_from_native(state, at, false)
	check(reverse.playing and drive.playing, "native reverse command warns before movement")
	state.applied_drive = 0.0
	state.lift_target_mps = .32
	audio.update_from_native(state, at, false, .1)
	check(hydraulic.playing and not reverse.playing, "native hydraulic command runs pump without reverse warning")
	var unloaded := hydraulic.volume_db
	state.lift_force_n = 18000.0
	state.positive_work_j = 400.0
	audio.update_from_native(state, at, false, .1)
	check(hydraulic.volume_db > unloaded, "actual hydraulic load/work raises bounded pump effort")
	state.lift_target_mps = 0.0
	state.tilt_target_rad_s = -.1
	audio.update_from_native(state, at, false, .1)
	check(hydraulic.playing, "tilt in either direction drives pump")
	state.tilt_target_rad_s = 0.0
	audio.update_from_native(state, at, false)
	check(not hydraulic.playing, "static holding force never fabricates active pumping")
	state.forward_speed_mps = NAN
	state.applied_drive = INF
	state.lift_force_n = NAN
	state.lift_target_mps = INF
	audio.update_from_native(state, at, false, NAN)
	for voice in audio.get_children():
		check(is_finite(voice.pitch_scale) and is_finite(voice.volume_db), "nonfinite receipts cannot poison audio mixer")
	state.available = false
	audio.update_from_native(state, at, true)
	for voice in audio.get_children():
		check(not voice.playing, "unavailable truck silences even held horn")
	state.available = true
	audio.update_from_native(state, at, true)
	audio.reset()
	for voice in audio.get_children():
		check(not voice.playing and voice.volume_db <= -79, "pause/reset silences every allocated source")
	var other: Node3D = script.new()
	root.add_child(other)
	for i in clips.size():
		check(other.get_child(i).stream == clips[i], "streams are cached once across updates and instances")
	check(audio.get_child_count() == 4, "updates never allocate extra voices")
	other.free()
	audio.free()
	print("FORKLIFT_AUDIO failures=", failures, " driver=", AudioServer.get_driver_name(), " control_pcm_fixture_only=1 heard_device_audio=0")
	quit(1 if failures else 0)
