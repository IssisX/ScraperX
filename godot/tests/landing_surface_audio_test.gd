extends SceneTree
## Supplied landing receipts exercise selection; no native or heard-audio claim.

const Director := preload("res://presentation/audio/audio_director.gd")
var failures := 0
var checks := 0

class RecordedDirector extends Director:
	var sounds: Array[StringName] = []
	var levels: Array[float] = []
	var pitches: Array[float] = []
	func _play(sound: StringName, level: float, pitch: float) -> void:
		sounds.append(sound)
		levels.append(level)
		pitches.append(pitch)


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("LANDING_SURFACE_AUDIO FAIL " + why)


func receipt(count: int, speed: float, surface: Variant) -> Dictionary:
	return {"landing_count": count, "landing_normal_speed_mps": speed,
		"landing_sound_surface": surface}


func sample(audio: RecordedDirector, state: Dictionary, deaths: int = 0,
		grounded: bool = true) -> void:
	audio.sounds.clear()
	audio.levels.clear()
	audio.pitches.clear()
	var unchanged := state.duplicate(true)
	audio.update(1.0 / 90.0, Vector3(0, 9, 0), Vector3.ZERO, grounded,
		51, 0, false, deaths, false, Vector3.ZERO, state)
	check(state == unchanged, "landing metadata remains read-only")


func steel_cues(audio: RecordedDirector, body: StringName, body_db: float,
		accent_db: float, why: String) -> void:
	check(audio.sounds == [body, &"step_metal"], why + ": body then one metal accent")
	if audio.levels.size() != 2:
		return
	check(absf(audio.levels[0] - body_db) < 0.001 and
		absf(audio.levels[1] - accent_db) < 0.001 and audio.levels[1] <= -9.0,
		why + ": unchanged body gain and bounded quieter accent")
	check(audio.pitches[0] >= 0.95 and audio.pitches[0] <= 1.05 and
		audio.pitches[1] == 1.0, why + ": existing body pitch and fixed contact pitch")


func _run() -> void:
	var audio := RecordedDirector.new()
	root.add_child(audio)
	var children := audio.get_child_count()
	sample(audio, receipt(40, 9.0, 2))
	check(audio.sounds.is_empty() and audio.landings == 0, "first sample establishes quiet baseline")
	sample(audio, receipt(41, 3.0, 2))
	steel_cues(audio, &"land_soft", -10.457575, -19.457575, "soft steel impact")
	check(audio.landings == 1, "one landing event despite two selected clips")
	for frame in 5:
		sample(audio, receipt(41, 3.0, 2))
		check(audio.sounds.is_empty() and audio.landings == 1, "same count never replays")
	sample(audio, receipt(42, 9.0, 2))
	steel_cues(audio, &"land_hard", -2.498775, -11.498775, "hard steel impact")
	sample(audio, receipt(43, 24.0, 2))
	steel_cues(audio, &"land_hard", 0.0, -9.0, "saturated hard impact")
	sample(audio, receipt(44, 1.99, 2))
	check(audio.sounds.is_empty() and audio.landings == 3, "below existing impact threshold stays quiet")
	sample(audio, receipt(45, 2.0, 2))
	steel_cues(audio, &"land_soft", -10.457575, -19.457575, "existing soft threshold")
	sample(audio, receipt(46, 7.0, 2))
	steel_cues(audio, &"land_hard", -4.681664, -13.681664, "existing hard threshold")
	var count := 46
	for surface in [1, 0, -1, 3, "2", 2.0, true, null]:
		count += 1
		var state := receipt(count, 3.0, surface)
		# Current steel footing cannot turn a nonmetal impact into steel.
		state["support_sound_surface"] = 2
		sample(audio, state)
		check(audio.sounds == [&"land_soft"] and
			absf(audio.levels[0] + 10.457575) < 0.001,
			"concrete, unknown or malformed impact retains generic cue: " + str(surface))
	count += 1
	sample(audio, {"landing_count": count, "landing_normal_speed_mps": 9.0,
		"support_sound_surface": 2})
	check(audio.sounds == [&"land_hard"], "missing impact tag ignores current steel support")
	count += 1
	var delayed := receipt(count, 9.0, 2)
	delayed["support_sound_surface"] = 1
	sample(audio, delayed, 0, false)
	steel_cues(audio, &"land_hard", -2.498775, -11.498775,
		"latched steel event observed airborne after leaving its support")
	sample(audio, delayed)
	check(audio.sounds.is_empty(), "later concrete footing cannot replay observed steel event")
	count += 1
	sample(audio, receipt(count, 9.0, 2), 1)
	check(audio.sounds == [&"impact_lethal"], "death suppresses both nonlethal landing clips")
	sample(audio, receipt(count, 9.0, 2), 1)
	check(audio.sounds.is_empty(), "suppressed event cannot replay after death")
	sample(audio, receipt(0, 9.0, 2), 1)
	check(audio.sounds.is_empty(), "count rewind establishes quiet baseline")
	sample(audio, receipt(1, 3.0, 2), 1)
	steel_cues(audio, &"land_soft", -10.457575, -19.457575, "fresh event after rewind")
	audio.reset_landing_feedback(1, 1)
	sample(audio, receipt(1, 9.0, 2), 1)
	check(audio.sounds.is_empty(), "explicit feedback reset consumes existing event")
	check(audio.steps == 0 and audio.get_child_count() == children,
		"contact accent creates no ordinary stride or extra voice resources")
	audio.quiesce()
	if audio._bank_task >= 0:
		WorkerThreadPool.wait_for_task_completion(audio._bank_task)
		audio._bank_task = -1
	audio.free()
	print("LANDING_SURFACE_AUDIO %s checks=%d native_contacts_exercised=0 heard_audio=0" %
		["PASS" if failures == 0 else "FAIL", checks])
	quit(1 if failures else 0)
