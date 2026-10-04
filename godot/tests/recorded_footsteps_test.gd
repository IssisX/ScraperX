extends SceneTree

const SoundBank := preload("res://presentation/audio/sound_bank.gd")
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
	quit(1 if _failures > 0 else 0)


func _fail(reason: String) -> void:
	_failures += 1
	push_error("RECORDED_FOOTSTEPS: " + reason)
