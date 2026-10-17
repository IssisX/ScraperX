extends SceneTree

const Bank := preload("res://presentation/audio/sound_bank.gd")
const Air := preload("res://presentation/audio/yard_ambience.gd")
const Forklift := preload("res://presentation/audio/forklift_audio.gd")
const Crane := preload("res://presentation/audio/yard_crane_audio.gd")

var failures := 0
var checks := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("PCM_LOOP " + message)

# Exercise the real WAV decoder at one input frame per output frame. Starting
# two frames before the boundary forces a wrap inside the resampler's block.
func check_loop(wav: AudioStreamWAV, label: String) -> void:
	var data := wav.data
	check(wav.loop_end > 0 and data.size() == (wav.loop_end + 1) * 2, label + " has exactly one readable guard")
	if data.size() != (wav.loop_end + 1) * 2:
		return
	check(data.decode_s16(wav.loop_end * 2) == data.decode_s16(0), label + " guard equals loop head")
	var playback := wav.instantiate_playback()
	playback.start(float(wav.loop_end - 2) / wav.mix_rate)
	var rate := AudioServer.get_mix_rate() / float(wav.mix_rate)
	var frames := playback.mix_audio(rate, 24)
	check(frames.size() == 24, label + " remains active across wrap")
	# Cubic resampling has two frames of history before the requested position.
	for i in range(2, frames.size()):
		var source := (wav.loop_end - 2 + i - 2) % wav.loop_end
		var expected := data.decode_s16(source * 2) / 32767.0
		check(absf(frames[i].x - expected) < 0.00002 and absf(frames[i].y - expected) < 0.00002, label + " preserves sample order %d" % i)
	playback.stop()

func _run() -> void:
	var bank := Bank.new()
	var samples := PackedFloat32Array([0.7, 0.5, 0.3, 0.1])
	var wav := bank._wav(samples, samples.size())
	check_loop(wav, "short sentinel loop")
	var whole := wav.instantiate_playback()
	whole.start()
	var rate := AudioServer.get_mix_rate() / float(wav.mix_rate)
	var expected := whole.mix_audio(rate, 128)
	for i in range(2, expected.size()):
		var source := (i - 2) % samples.size()
		var value := wav.data.decode_s16(source * 2) / 32767.0
		check(absf(expected[i].x - value) < 0.00002 and absf(expected[i].y - value) < 0.00002, "sentinel repeats every four frames at %d" % i)
	var split := wav.instantiate_playback()
	split.start()
	var actual := PackedVector2Array()
	for size in [7, 17, 104]:
		actual.append_array(split.mix_audio(rate, size))
	check(actual == expected, "mix block splits preserve the same loop sequence")
	whole.stop()
	split.stop()
	var one_shot := bank._wav(samples.duplicate())
	check(one_shot.data.size() == samples.size() * 2 and one_shot.loop_mode == AudioStreamWAV.LOOP_DISABLED, "one shots retain their original extent")
	check_loop(bank._loop(bank._wind(4.5), 0.5), "startup wind")
	var air := Air.new()
	for kind in 2:
		check_loop(air._air_loop(kind), "yard air %d" % kind)
	air.free()
	for kind in 4:
		check_loop(Forklift._make_clip(kind), "forklift %d" % kind)
	for kind in 3:
		check_loop(Crane._make_clip(kind), "crane %d" % kind)
	print("PCM_LOOP checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
