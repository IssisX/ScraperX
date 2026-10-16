extends SceneTree
var failures := 0
func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("CRANE_AUDIO " + message)
func _initialize() -> void:
	_run.call_deferred()
func _run() -> void:
	var script = load("res://presentation/audio/yard_crane_audio.gd")
	if script == null:
		quit(1)
		return
	if AudioServer.get_bus_index(&"Effects") < 0:
		AudioServer.add_bus()
		AudioServer.set_bus_name(AudioServer.bus_count - 1, &"Effects")
	var audio: Node3D = script.new()
	root.add_child(audio)
	var state := {"available": true, "hoist_connected": true, "hoist_target_mps": 0.0,
		"hoist_force_n": 20000.0, "hoist_work_j": 0.0, "transport_work_j": 0.0,
		"lines": [{"from": Vector3(-61, 40, -152), "to": Vector3(-61, 10, -152)}]}
	audio.update_from_native(state, .1)
	check(audio.get_child_count() == 3, "bounded three voices")
	var clips := []
	for voice in audio.get_children():
		check(voice is AudioStreamPlayer3D and voice.bus == &"Effects", "spatial effects routing")
		check(not voice.playing, "static loaded brake silent")
		var wav: AudioStreamWAV = voice.stream
		var peak := 0
		for offset in range(0, wav.data.size(), 2):
			peak = maxi(peak, absi(wav.data.decode_s16(offset)))
		check(peak > 4000 and peak < 32767, "nonzero unclipped cached PCM")
		clips.append(wav)
	var winch: AudioStreamPlayer3D = audio.get_node("Winch")
	state.hoist_target_mps = -.5
	audio.update_from_native(state, .1)
	check(winch.playing and winch.global_position == state.lines[0].from, "signed hoist runs at actual sheave")
	var loaded := winch.volume_db
	state.hoist_force_n = 0.0
	audio.update_from_native(state, .1)
	check(winch.volume_db < loaded, "actual force raises winch effort")
	state.hoist_work_j = 5000.0
	audio.update_from_native(state, .1)
	check(winch.volume_db > loaded - 1.0, "positive work raises effort")
	state.hoist_target_mps = 0.0
	state.trolley_target_mps = .4
	state.slew_target_rad_s = -.1
	audio.update_from_native(state, .1)
	check(not winch.playing and audio.get_node("Trolley").playing and audio.get_node("Slew").playing, "independent transport sources")
	state.paused = true
	audio.update_from_native(state, .1)
	for voice in audio.get_children():
		check(not voice.playing, "pause silent")
	state.paused = false
	state.available = false
	audio.update_from_native(state, .1)
	for voice in audio.get_children():
		check(not voice.playing, "unavailable silent")
	state.available = true
	state.hoist_target_mps = INF
	state.trolley_target_mps = NAN
	state.slew_target_rad_s = NAN
	audio.update_from_native(state, NAN)
	for voice in audio.get_children():
		check(not voice.playing and is_finite(voice.volume_db), "nonfinite receipts safe")
	var other: Node3D = script.new()
	root.add_child(other)
	for i in clips.size():
		check(other.get_child(i).stream == clips[i], "PCM shared across instances")
	audio.reset()
	check(audio.get_child_count() == 3, "no per-update voice allocation")
	other.free()
	audio.free()
	print("CRANE_AUDIO failures=", failures, " driver=", AudioServer.get_driver_name(), " receipt_fixture_only=1 heard_device_audio=0")
	quit(1 if failures else 0)
