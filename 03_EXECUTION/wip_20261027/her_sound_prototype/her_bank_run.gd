extends SceneTree
# Times the prototype of her bank and writes each clip's first variant (and
# every variant of the old clips it replaces) as WAV for the analysis.

const PROTO := "/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/her_sound/her_bank_proto.gd"
const SoundBank := preload("res://presentation/audio/sound_bank.gd")
const OUT := "/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/her_sound/wav"


func _initialize() -> void:
	DirAccess.make_dir_recursive_absolute(OUT)
	var her = (load(PROTO) as GDScript).new()
	her.build_her()
	var total := 0
	var streams := 0
	for name in her.clips:
		var k := 0
		for wav in her.clips[name]:
			total += (wav as AudioStreamWAV).data.size()
			streams += 1
			(wav as AudioStreamWAV).save_to_wav("%s/new_%s_%d.wav" % [OUT, name, k])
			k += 1
	print("HER clips=%d streams=%d bytes=%d build_msec=%d" % [her.clips.size(), streams, total, her.build_msec])
	for name in her.cost:
		print("HERCOST %s %.1f" % [name, her.cost[name]])
	var old = SoundBank.new()
	old.build()
	for name in [&"matron_breath", &"matron_step", &"matron_shriek", &"bone_crack", &"matron_wake", &"hit_heavy",
			&"step_metal", &"clang", &"scrape", &"locker_burst", &"wind_loop", &"drone_loop", &"matron_lullaby_loop"]:
		var k := 0
		for wav in old.clips[name]:
			(wav as AudioStreamWAV).save_to_wav("%s/old_%s_%d.wav" % [OUT, name, k])
			k += 1
	quit(0)
