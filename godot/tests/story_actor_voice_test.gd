extends SceneTree
const Radio := preload("res://presentation/audio/story_radio_player.gd")
var checks := 0
var failures := 0
class SpatialRadio extends Radio:
	var sample: AudioStreamWAV
	func _init() -> void:
		sample = AudioStreamWAV.new()
		sample.mix_rate = 8000
		sample.format = AudioStreamWAV.FORMAT_16_BITS
		var data := PackedByteArray()
		data.resize(16000)
		sample.data = data
	func _read_manifest(_path: String) -> Variant:
		var envelope: Array = []
		envelope.resize(30)
		envelope.fill(.4)
		return {"schema_version":1,"cues":{
			"actor":{"file":"res://audio/story_radio/actor.ogg","speaker":"ARI","text":"Ari Ward.","duration_s":1.0,"rms_30hz":envelope},
			"ro":{"file":"res://audio/story_radio/ro.ogg","speaker":"RO","text":"That's my face.","duration_s":1.0},
			"bad":{"file":"res://audio/story_radio/bad.ogg","speaker":"ARI","text":"Invalid envelope.","duration_s":1.0,"rms_30hz":[NAN]}}}
	func _load_voice_stream(_path: String) -> AudioStream:
		return sample
func _initialize() -> void:
	_run.call_deferred()
func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("STORY_ACTOR_VOICE " + why)
func _run() -> void:
	var added_bus := AudioServer.get_bus_index(&"Effects") < 0
	if added_bus:
		AudioServer.add_bus()
		AudioServer.set_bus_name(AudioServer.bus_count - 1, &"Effects")
	var radio := SpatialRadio.new()
	root.add_child(radio)
	var actor := {"transmission_id":1,"transmission_key":&"actor","speaker":"ARI","transmission":"Ari Ward."}
	radio.present(actor, 0)
	check(radio.readback().status == "scene_position_unavailable" and radio.readback().scheduled_plays == 0, "no actor location means no voice at world origin")
	radio.set_scene_voice_position(Vector3(1, 254.62, -143))
	radio.present(actor, 0)
	check(radio.readback().spatial and radio._scene_voice.stream == radio.sample and radio._voice.stream == null, "Ari recording plays from 3D actor only")
	check(radio._scene_voice.global_position == Vector3(1, 254.62, -143) and radio._scene_voice.bus == &"Effects", "actual head position and existing settings bus retained")
	check(is_equal_approx(radio.speech_level_for(&"ARI"), .4) and radio.speech_level_for(&"RO") == 0, "mouth samples active recording envelope for correct speaker")
	radio.set_scene_voice_position(Vector3(2, 254.62, -143))
	check(radio._scene_voice.global_position.x == 2, "voice follows actual actor readback")
	radio.present(actor, 0)
	check(radio.readback().scheduled_plays == 1, "duplicate cue does not restart spatial voice")
	radio.present({"transmission_id":2,"transmission_key":&"ro","speaker":"RO","transmission":"That's my face."}, 0)
	check(not radio.readback().spatial and radio._voice.stream == radio.sample and radio._scene_voice.stream == null, "Ro radio replaces actor recording without overlap")
	radio.cancel()
	check(not radio.readback().playing and radio._scene_voice.stream == null and radio._voice.stream == null and radio.speech_level_for(&"ARI") == 0, "cancel clears both emitters and mouth motion")
	check(radio.readback().missing_keys.has("bad") and not radio.cue_durations().has(&"bad"), "invalid envelope is rejected at metadata boundary")
	radio.quiesce()
	radio.present(actor, 0)
	check(not radio.readback().playing, "shutdown rejects late speech")
	radio.free()
	if added_bus:
		AudioServer.remove_bus(AudioServer.get_bus_index(&"Effects"))
	print("STORY_ACTOR_VOICE %s checks=%d failures=%d" % ["PASS" if failures == 0 else "FAIL", checks, failures])
	quit(0 if failures == 0 else 1)
