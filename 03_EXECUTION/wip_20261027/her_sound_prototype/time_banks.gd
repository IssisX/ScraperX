extends SceneTree
const DIR := "/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/her_sound/"
const SoundBank := preload("res://presentation/audio/sound_bank.gd")

func _initialize() -> void:
	for round in 3:
		var old = SoundBank.new()
		old.build()
		var trimmed = (load(DIR + "world_bank_trimmed.gd") as GDScript).new()
		trimmed.build()
		var her = (load(DIR + "her_bank_proto.gd") as GDScript).new()
		her.build_her()
		var s2 := 0.0
		for k in her.cost:
			if String(k).begins_with("slice2"):
				s2 += float(her.cost[k])
		print("TIMES round=%d world_now=%d world_trimmed=%d her_slice1=%d her_slice2=%.0f clips_now=%d clips_trimmed=%d" % [
			round, old.build_msec, trimmed.build_msec, her.build_msec, s2, old.clips.size(), trimmed.clips.size()])
	quit(0)
