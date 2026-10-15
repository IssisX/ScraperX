extends SceneTree

const Director := preload("res://presentation/audio/audio_director.gd")
const Bank := preload("res://presentation/audio/sound_bank.gd")
var failures := 0
var checks := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("GRAPPLE_AUDIO FAIL " + message)

func _run() -> void:
	var audio := Director.new()
	root.add_child(audio)
	audio.set_process(false)
	if not audio.has_method("update_grapple_feedback"):
		check(false, "required read-only feedback adapter exists")
		audio.free()
		quit(1)
		return
	# The real asynchronous bank is built once, not a whole-bank test campaign.
	WorkerThreadPool.wait_for_task_completion(audio._bank_task)
	audio._bank_task = -1
	await process_frame
	var pulse := {"equipped": true, "fresh": true, "launch": true, "failed": true, "warning": true}
	audio.update_grapple_feedback(pulse, 0.016)
	check(audio.grapple_launches == 1 and audio.grapple_failures == 1 and audio.grapple_warnings == 1, "fresh native discrete receipts counted")
	pulse.fresh = false
	for i in 8:
		audio.update_grapple_feedback(pulse, 0.016)
	check(audio.grapple_launches == 1 and audio.grapple_failures == 1 and audio.grapple_warnings == 1, "stale render repeats cannot replay discrete cues")
	audio.update_grapple_feedback({"equipped": false, "fresh": true, "launch": true}, 0.016)
	check(audio.grapple_launches == 1, "stowed receipt stays quiet")
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "load": 1.0, "danger": 1.0, "auto_reel": true}, 0.016)
	check(not audio.grapple_motor_active, "tension and auto reel intention do not run motor")
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "winding": 0.4, "power": 0.0, "load": 1.0}, 0.016)
	check(audio.grapple_motor_active and audio._grapple_motor.volume_db <= -24.0 and audio._grapple_load.volume_db <= -60.0, "zero-work actual slack takeup is quiet ratchet only")
	var slack_db: float = audio._grapple_motor.volume_db
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "winding": 99.0, "power": 99.0, "load": 99.0}, 10.0)
	check(audio.grapple_motor_active and audio._grapple_motor.volume_db > slack_db, "positive physical power deepens winding")
	check(audio._grapple_motor.volume_db <= -12.0 and audio._grapple_load.volume_db <= -18.0 and audio._grapple_motor.pitch_scale >= 0.65 and audio._grapple_motor.pitch_scale <= 1.35, "motor gain and pitch remain bounded")
	check(audio._silent and not audio._grapple_motor.playing and not audio._grapple_load.playing, "Dummy counts but never starts loop playback")
	for voice in audio._voices:
		check(not voice.playing, "Dummy does not start discrete playback")
	audio.reset_grapple_feedback()
	check(not audio.grapple_motor_active and not audio._grapple_motor.playing and audio._grapple_motor.stream == null, "explicit pause death restart reset releases motor")
	audio._on_bank_ready()
	check(not audio.grapple_motor_active and not audio._grapple_motor.playing, "bank ready cannot resurrect reset motor")
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "power": 0.2}, 0.016)
	check(audio.grapple_motor_active, "measured positive work alone can run motor")
	audio.update_grapple_feedback({"equipped": true, "fresh": false, "power": 0.2}, 0.016)
	check(audio.grapple_motor_active, "zero-tick render retains the current motor target without restarting")
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "power": 0.0}, 0.016)
	check(not audio.grapple_motor_active, "next native zero-work target stops motor")
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "winding": 0.2}, 0.016)
	audio.update_grapple_feedback({"equipped": false, "fresh": true}, 0.016)
	check(not audio.grapple_motor_active and audio._grapple_load.stream == null, "stow releases both loop streams")
	# Only the five new recipes are synthesized again to reject nondeterminism.
	var other := Bank.new()
	other._build_grapple_clips()
	var preview := OS.get_environment("SCRAPERX_GRAPPLE_AUDIO_PREVIEW")
	if not preview.is_empty():
		DirAccess.make_dir_recursive_absolute(preview)
	for name in [&"grapple_launch", &"grapple_failure", &"grapple_warning", &"grapple_winding_loop", &"grapple_load_loop"]:
		var wav: AudioStreamWAV = audio._bank.clips[name][0]
		check(wav.data == other.clips[name][0].data, "fixed seed PCM " + String(name))
		var peak := 0.0
		var energy := 0.0
		for i in range(0, wav.data.size(), 2):
			var sample := float(wav.data.decode_s16(i)) / 32768.0
			peak = maxf(peak, absf(sample))
			energy += sample * sample
		check(is_finite(energy) and peak > 0.08 and peak <= 0.901 and sqrt(energy / (wav.data.size() / 2)) > 0.008, "finite bounded audible PCM " + String(name))
		if wav.loop_mode == AudioStreamWAV.LOOP_FORWARD:
			check(wav.loop_end == wav.data.size() / 2 and absf(float(wav.data.decode_s16(0) - wav.data.decode_s16(wav.data.size() - 2)) / 32768.0) < 0.08, "seamless loop boundary " + String(name))
		if not preview.is_empty():
			check(wav.save_to_wav(preview.path_join(String(name) + ".wav")) == OK, "PCM preview saved")
	audio.update_grapple_feedback({"equipped": true, "fresh": true, "winding": 0.5}, 0.016)
	check(audio.grapple_motor_active, "motor active before generic cleanup")
	audio.quiesce()
	check(not audio.grapple_motor_active, "generic quiesce clears new motor state")
	audio.free()
	print("GRAPPLE_AUDIO checks=%d failures=%d" % [checks, failures])
	quit(1 if failures else 0)
