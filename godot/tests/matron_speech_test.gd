extends SceneTree
## Her spoken lines (presentation/audio/matron_speech.gd, wired in matron_sound.gd, said by audio_director.gd
## matron_say): every one of the 43 lines of 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md has three clips
## on disk and a caption; a moment never says the line it just said, and a line never plays the clip it just
## played (rule 42: while she holds him no sound of hers plays twice running); her sounds, driven the way the
## game drives them (hunting by ear, the sight of him, a hold with a carry, a dangle and an arm torn, the apron
## wiped, a hold he breaks, holds her hand ends), say each moment's line as soon as her mouth is free, never
## twice running in a hold or out of it, never a word to his back after a hold her hand ended, and "Waste
## not." at the wipe even over a line in her mouth; and in the director her one mouth: a line waits out her
## shriek, a shriek cuts her line, her growl is silent and her laugh does not start while she speaks, and her
## last word cuts a line or a shriek and nothing cuts it.
##
##   tools/godot/godot --headless --path godot --script res://tests/matron_speech_test.gd

const MatronSpeech := preload("res://presentation/audio/matron_speech.gd")
const MatronSound := preload("res://presentation/audio/matron_sound.gd")
const Table := preload("res://presentation/audio/matron_voice_lines.gd")
const Director := preload("res://presentation/audio/audio_director.gd")

const FRAME_S := 1.0 / 60.0

var _fails := 0


# What the game's audio director is to her sounds, her one mouth as matron_say keeps it: the same speech
# choice, a line taking 0.4 s a word (as with no device), and while it runs another refused (-1, her mouth
# full) unless said with priority (her last word), which cuts it; her shriek (the Turn, a shove, her dread
# fully changed) cuts a line, fills her mouth for SHRIEK_S and refuses lines meanwhile; nothing cuts her
# last word. Nothing is played.
class StubAudio:
	extends RefCounted
	const SHRIEK_S := 1.2
	var matron_encounter := 0
	var speech := MatronSpeech.new(11)
	var said: Array = []        # each line said: its choice, with "t" when and "s" how long
	var shrieks: Array = []     # when each shriek started
	var refused := 0
	var cut := 0
	var clock := 0.0
	var _mouth_left := 0.0
	var _mouth_rank := 0        # 2 a line, 3 a shriek, 4 her last word

	func matron_say(moment: StringName, _at: Vector3, _volume_db: float = 0.0, priority: bool = false) -> int:
		var rank := 4 if priority else 2
		if _mouth_left > 0.0 and _mouth_rank >= rank:
			refused += 1
			return -1
		var choice := speech.choose(moment, matron_encounter)
		if choice.is_empty():
			return 0
		if _mouth_left > 0.0:
			cut += 1
		_mouth_left = 0.4 * float(speech.caption(int(choice["line"])).split(" ", false).size())
		_mouth_rank = rank
		choice["t"] = clock
		choice["s"] = _mouth_left
		said.append(choice)
		return int(choice["line"])

	func speaking() -> bool:
		return _mouth_left > 0.0 and _mouth_rank == 2

	func _shriek() -> void:
		if _mouth_left > 0.0 and _mouth_rank > 3:
			return
		if _mouth_left > 0.0 and _mouth_rank == 2:
			cut += 1
		_mouth_left = SHRIEK_S
		_mouth_rank = 3
		shrieks.append(clock)

	func matron_dread(_at: Vector3, _distance: float, stage: int = 0) -> void:
		if stage >= 2:
			_shriek()

	func matron_cue(name: StringName, _at: Vector3, _volume_db: float, _pitch: float = 1.0) -> bool:
		if name == &"matron_shriek":
			_shriek()
		return true

	func matron_cue_one_of(_names: Array, _at: Vector3, _volume_db: float) -> StringName:
		return &""

	func matron_strain(_at: Vector3, _volume_db: float, _pitch: float) -> void:
		pass

	func arm_break(_at: Vector3) -> void:
		pass

	func matron_layers(delta: float, _data: Dictionary, _out: bool, _head: Vector3, _distance: float,
			_listener_y: float) -> void:
		clock += delta
		_mouth_left = maxf(0.0, _mouth_left - delta)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	_bank()
	_choosing()
	_wired()
	await _director()
	print("MATRON_SPEECH %s fails=%d" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(0 if _fails == 0 else 1)


func _check(ok: bool, what: String) -> void:
	if not ok:
		_fails += 1
		if _fails <= 20:
			print("MATRON_SPEECH FAIL %s" % what)


# All 43 lines, three clips each on disk, each with its words; her last line is 27, "Waste not."
func _bank() -> void:
	var seen := {}
	for moment in Table.MOMENTS:
		for number in Table.MOMENTS[moment]:
			seen[number] = true
			var clips: Array = Table.MOMENTS[moment][number]
			_check(clips.size() >= 3, "line %d has %d clips" % [number, clips.size()])
			for clip in clips:
				_check(FileAccess.file_exists(Table.DIR + String(clip)), "clip %s is not on disk" % clip)
			_check(not String(Table.CAPTIONS.get(number, "")).is_empty(), "line %d has no caption" % number)
	_check(seen.size() == 43, "%d lines in the bank, not 43" % seen.size())
	_check(Table.LAST_LINE == 27 and Table.MOMENTS[&"last"].keys() == [27] and Table.CAPTIONS[27] == "Waste not.",
		"her last word is not 27, Waste not.")


# A moment alone, said 5,000 times: never the line just said, never the clip just played, and every line
# and every clip of it comes round.
func _choosing() -> void:
	for moment in Table.MOMENTS:
		var speech := MatronSpeech.new(3)
		var lines: Dictionary = Table.MOMENTS[moment]
		var last_line := 0
		var last_clip := ""
		var counts := {}
		for i in 5000:
			var said := speech.choose(moment, 1)
			var line: int = said["line"]
			var clip: String = said["clip"]
			_check(lines.has(line) and clip in lines[line], "%s said a line or clip it does not own" % moment)
			if lines.size() > 1:
				_check(line != last_line, "%s said line %d twice running" % [moment, line])
			_check(clip != last_clip, "%s played %s twice running" % [moment, clip])
			last_line = line
			last_clip = clip
			counts[clip] = int(counts.get(clip, 0)) + 1
		var clips := 0
		for number in lines:
			clips += (lines[number] as Array).size()
		_check(counts.size() == clips, "%s played %d of its %d clips" % [moment, counts.size(), clips])
	_check(MatronSpeech.new(1).choose(&"nothing", 0).is_empty(), "a moment with no lines said one")


# Her sounds, driven the way the game does it, every frame at 60 Hz, her clocks (randf_range) the same
# every run.
func _wired() -> void:
	seed(0x4D415452)
	var audio := StubAudio.new()
	var sound := MatronSound.new(audio)
	var data := {"state": 3, "position": Vector3(0, 154, 0), "speed": 1.0, "turn": 0, "stage": 0, "sees_player": false,
		"stride": 0.0, "alive": true}
	var hold := {"state": 0, "carries": 0, "dangles": 0, "armplay_beat": 0, "arm_tear_count": 0, "leg_tear_count": 0,
		"fatal_hang": false, "fatal_release_beat": 0, "releases": 0, "spares": 0, "claims": 0, "waste_not": 0}
	var feet := Vector3(0, 154, 0)
	var head := Vector3(0, 155.6, 0)
	var player := Vector3(6, 154, 0)
	var frame := func(seconds: float) -> void:
		for i in roundi(seconds * 60.0):
			sound.update(data, FRAME_S, true, feet, head, player)
			sound.hear_hold(hold)
	var bump := func(key: String) -> void:
		hold[key] = int(hold[key]) + 1
	# Her hands on him: the grab, the carry, the dangle.
	var take := func(carry_s: float, dangle_s: float) -> void:
		hold["state"] = 12
		frame.call(1.0)
		hold["state"] = 13
		bump.call("carries")
		frame.call(carry_s)
		hold["state"] = 14
		bump.call("dangles")
		frame.call(dangle_s)
	frame.call(1.0)
	# Hunting by ear: she talks to him now and then.
	frame.call(70.0)
	var after_hunt: int = audio.said.size()
	_check(after_hunt >= 3, "hunting by ear said %d lines in 70 s" % after_hunt)
	_check(audio.said.all(func(s): return s["moment"] == &"hunting"), "something else said while hunting by ear")
	# She sees him: her line for it, as soon as her mouth is free (it waits out the gap after her last).
	data["sees_player"] = true
	var seen_t: float = audio.clock
	frame.call(8.0)
	_check(audio.said.any(func(s): return s["moment"] == &"sees"), "the sight of him said nothing")
	_check_prompt(audio, &"sees", seen_t + FRAME_S, 0.9)
	# A hold: the grab, the carry, the dangle with her other hand at his arm torn, then claimed and the
	# apron wiped while the rip's line is still in her mouth: "Waste not." at once, over it.
	take.call(8.0, 20.0)
	var tear_t: float = audio.clock
	bump.call("arm_tear_count")
	for i in 600:
		if audio.said.any(func(s): return s["moment"] == &"rip"):
			break
		frame.call(FRAME_S)
	_check_prompt(audio, &"rip", tear_t + FRAME_S, 0.4)
	frame.call(0.3)
	_check(audio.speaking(), "no line in her mouth at the apron wipe (the test's own setting)")
	hold["state"] = 18
	bump.call("claims")
	bump.call("waste_not")
	var wipe_t: float = audio.clock
	var cut_before := audio.cut
	frame.call(1.0)
	var lasts := audio.said.filter(func(s): return s["moment"] == &"last")
	_check(lasts.size() == 1 and float(lasts[0]["t"]) - wipe_t <= 3.0 * FRAME_S and audio.cut > cut_before,
		"the apron wipe did not say Waste not. at once over the line in her mouth")
	hold["state"] = 0
	var end_t: float = audio.clock
	frame.call(8.0)
	_check(_said_between(audio, &"free", end_t, audio.clock) == 0, "a word to his back after she claimed him")
	# He breaks free: the hold ends with nothing her hand did; a word to his back, as soon as she can.
	take.call(8.0, 20.0)
	hold["state"] = 0
	end_t = audio.clock
	frame.call(8.0)
	_check(_said_between(audio, &"free", end_t, audio.clock) == 1, "no word to his back as he broke free")
	_check_prompt(audio, &"free", end_t + FRAME_S, 0.5)
	# Let go over the drop (her fatality's release): his fall watched (her Release, still in the hold's
	# range) and then the hold over; no word to his back, however long after the let-go it ends.
	take.call(3.0, 3.0)
	bump.call("fatal_release_beat")
	hold["state"] = 16
	frame.call(8.0)
	hold["state"] = 0
	end_t = audio.clock
	frame.call(8.0)
	_check(_said_between(audio, &"free", end_t, audio.clock) == 0,
		"a word to his back after she let him go over the drop")
	# An arm torn off and its play going on (the hold's count rose seconds before it ends): no word to his back.
	take.call(3.0, 3.0)
	hold["state"] = 15
	bump.call("arm_tear_count")
	frame.call(10.0)
	hold["state"] = 0
	end_t = audio.clock
	frame.call(8.0)
	_check(_said_between(audio, &"free", end_t, audio.clock) == 0, "a word to his back after she tore off his arm")
	# Set down on her deck (Spare): no word to his back.
	take.call(3.0, 3.0)
	hold["state"] = 17
	bump.call("spares")
	frame.call(3.0)
	hold["state"] = 0
	end_t = audio.clock
	frame.call(8.0)
	_check(_said_between(audio, &"free", end_t, audio.clock) == 0, "a word to his back after she set him down")
	# Lost, then seen again fully changed: her dread is her shriek, and her line for the sight of him waits
	# for it to end (never over it, never lost to it).
	data["sees_player"] = false
	frame.call(8.0)
	data["stage"] = 2
	data["sees_player"] = true
	seen_t = audio.clock
	frame.call(8.0)
	var shrieked := audio.shrieks.filter(func(t): return float(t) >= seen_t)
	var seen_again := audio.said.filter(func(s): return float(s["t"]) >= seen_t and s["moment"] == &"sees")
	_check(shrieked.size() == 1 and seen_again.size() == 1
		and float(seen_again[0]["t"]) >= float(shrieked[0]) + StubAudio.SHRIEK_S - FRAME_S,
		"the sight of him fully changed said nothing after her shriek, or said it over it (%s %s)"
		% [shrieked, seen_again])
	var moments := {}
	for s in audio.said:
		moments[s["moment"]] = int(moments.get(s["moment"], 0)) + 1
	for moment in [&"hunting", &"sees", &"grab", &"drop", &"rip", &"last", &"free"]:
		_check(moments.has(moment), "no line for the moment %s (said: %s)" % [moment, moments])
	# Her voice twice running: never a line, never a clip, in a hold or out of it.
	for i in range(1, audio.said.size()):
		var a: Dictionary = audio.said[i - 1]
		var b: Dictionary = audio.said[i]
		_check(a["clip"] != b["clip"], "clip %s twice running" % a["clip"])
		_check(a["line"] != b["line"] or a["moment"] == &"last", "line %d twice running" % a["line"])
	# While she holds him, several different lines.
	var in_hold := audio.said.filter(func(s): return int(s["encounter"]) == 1 and s["moment"] == &"drop")
	var distinct := {}
	for s in in_hold:
		distinct[s["line"]] = true
	_check(distinct.size() >= 3, "%d different lines in the first hold's dangle" % distinct.size())
	# The apron: line 27 is the one said at the wipe.
	var last := audio.said.filter(func(s): return s["moment"] == &"last")
	_check(last.size() == 1 and last[0]["line"] == 27, "the apron wipe did not say Waste not.")
	print("MATRON_SPEECH lines said %d (refused %d, cut %d): %s" % [audio.said.size(), audio.refused, audio.cut,
		moments])


# The first line of `moment` said from `since` on came `after` seconds after it, or as soon as her mouth
# let it: when the gap after the line before it (MatronSound.SAY_GAP_S from its start) and that line had
# ended. Two frames of slack.
func _check_prompt(audio: StubAudio, moment: StringName, since: float, after: float) -> void:
	for i in audio.said.size():
		var s: Dictionary = audio.said[i]
		if float(s["t"]) < since or s["moment"] != moment:
			continue
		var due := since + after
		if i > 0:
			var before: Dictionary = audio.said[i - 1]
			due = maxf(due, float(before["t"]) + maxf(MatronSound.SAY_GAP_S, float(before["s"])))
		_check(float(s["t"]) <= due + 2.0 * FRAME_S,
			"%s said at %.2f s, due by %.2f s (as soon as her mouth was free)" % [moment, float(s["t"]), due])
		return
	_check(false, "no line for %s from %.2f s" % [moment, since])


func _said_between(audio: StubAudio, moment: StringName, from: float, to: float) -> int:
	var n := 0
	for s in audio.said:
		if s["moment"] == moment and float(s["t"]) >= from and float(s["t"]) <= to:
			n += 1
	return n


# Her one mouth in the game's director (audio_director.gd), with no audio device: each sound timed as if
# it played (her own bank's lengths, a line 0.4 s a word). A line asked for over her shriek is refused and
# said once the shriek has ended; while she says it her growl is held silent and her laugh (optional)
# does not start; a shriek cuts it; her last word takes her mouth from a shriek or a line, and neither a
# line nor a shriek cuts it.
func _director() -> void:
	var audio := Director.new()
	root.add_child(audio)
	for i in 3600:
		if audio._bank_ready and audio._her_ready:
			break
		await process_frame
	_check(audio._bank_ready and audio._her_ready, "the director's banks were not ready in 3600 frames")
	var head := Vector3(0, 155.6, 0)
	# Hunting him, dangerous (stage 1): her growl is up.
	var data := {"state": 3, "stage": 1, "turn": 0, "speed": 1.0, "rage": 0.6, "floor_y": 154.0}
	var tick := func(seconds: float) -> void:
		for i in maxi(1, roundi(seconds * 60.0)):
			audio.matron_layers(FRAME_S, data, true, head, 6.0, 154.0)
	tick.call(1.0)
	_check(audio.matron_growl_db > Director.HER_OFF_DB, "no growl hunting dangerous (the test's own setting)")
	_check(audio.matron_cue(&"matron_shriek", head, -3.0, 1.0), "her shriek did not start")
	_check(audio.matron_say(&"hunting", head) == Director.MOUTH_FULL and audio.matron_caption.is_empty(),
		"a line of hers was said over her shriek")
	tick.call(2.0)
	var line := audio.matron_say(&"hunting", head)
	_check(line > 0 and audio.matron_caption == String(Table.CAPTIONS.get(line, "-")),
		"her line was not said once her shriek had ended")
	tick.call(FRAME_S)
	_check(audio.matron_growl_db <= Director.HER_OFF_DB, "her growl was wanted while she said a line")
	_check(not audio.matron_cue(&"matron_laugh", head, 0.0, 1.0), "her laugh started over her line")
	# Her shriek again (its own rest, 4 s from the first, her_takes.gd, is over), the line still in her
	# mouth (it runs at least 3.2 s): it cuts the line.
	tick.call(2.1)
	_check(not audio.matron_caption.is_empty(), "her line ended before her shriek came (the test's own setting)")
	_check(audio.matron_cue(&"matron_shriek", head, -3.0, 1.0) and audio.matron_caption.is_empty()
		and audio.matron_lines_cut == 1, "her shriek sounded over her line")
	tick.call(FRAME_S)
	_check(audio.matron_growl_db > Director.HER_OFF_DB, "her growl stayed silent with no line in her mouth")
	# Her last word over her shriek; nothing said over it.
	_check(audio.matron_say(&"last", head, 0.0, true) == Table.LAST_LINE and audio.matron_caption == "Waste not.",
		"her last word did not take her mouth from her shriek")
	_check(audio.matron_say(&"hunting", head) == Director.MOUTH_FULL and audio.matron_caption == "Waste not.",
		"a line was said over her last word")
	# Her last word over a line, and her shriek (rested) not over it.
	tick.call(4.5)
	_check(audio.matron_say(&"hunting", head) > 0, "no line once her last word had ended")
	_check(audio.matron_say(&"last", head, 0.0, true) == Table.LAST_LINE and audio.matron_lines_cut == 2,
		"her last word did not cut the line in her mouth")
	_check(not audio.matron_cue(&"matron_shriek", head, -3.0, 1.0) and audio.matron_caption == "Waste not.",
		"her shriek cut her last word")
	print("MATRON_SPEECH director lines refused %d, cut %d, said %d" % [audio.matron_lines_suppressed,
		audio.matron_lines_cut, audio.matron_speech.log.size()])
	root.remove_child(audio)
	audio.free()
