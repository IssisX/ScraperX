extends SceneTree
## Her spoken lines (presentation/audio/matron_speech.gd, wired in matron_sound.gd): every one of the 43
## lines of 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md has three clips on disk and a caption; a moment
## never says the line it just said, and a line never plays the clip it just played (rule 42: while she
## holds him no sound of hers plays twice running); and her sounds, driven the way the game drives them
## (hunting by ear, the sight of him, a hold with a carry, a dangle and an arm torn, the apron wiped, a hold
## he breaks), say each moment's line, never twice running in a hold or out of it, and end on "Waste not."
##
##   tools/godot/godot --headless --path godot --script res://tests/matron_speech_test.gd

const MatronSpeech := preload("res://presentation/audio/matron_speech.gd")
const MatronSound := preload("res://presentation/audio/matron_sound.gd")
const Table := preload("res://presentation/audio/matron_voice_lines.gd")

var _fails := 0


# What the game's audio director is to her sounds: the same speech choice, and nothing played.
class StubAudio:
	extends RefCounted
	var matron_encounter := 0
	var speech := MatronSpeech.new(11)
	var said: Array = []

	func matron_say(moment: StringName, _at: Vector3, _volume_db: float = 0.0) -> int:
		var choice := speech.choose(moment, matron_encounter)
		if choice.is_empty():
			return 0
		said.append(choice)
		return int(choice["line"])

	func matron_dread(_at: Vector3, _distance: float, _stage: int = 0) -> void:
		pass

	func matron_cue(_name: StringName, _at: Vector3, _volume_db: float, _pitch: float = 1.0) -> bool:
		return true

	func matron_cue_one_of(_names: Array, _at: Vector3, _volume_db: float) -> StringName:
		return &""

	func matron_strain(_at: Vector3, _volume_db: float, _pitch: float) -> void:
		pass

	func arm_break(_at: Vector3) -> void:
		pass

	func matron_layers(_delta: float, _data: Dictionary, _out: bool, _head: Vector3, _distance: float,
			_listener_y: float) -> void:
		pass


func _init() -> void:
	_bank()
	_choosing()
	_wired()
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


# Her sounds, driven the way the game does it, every frame at 60 Hz.
func _wired() -> void:
	var audio := StubAudio.new()
	var sound := MatronSound.new(audio)
	var data := {"state": 3, "position": Vector3(0, 154, 0), "speed": 1.0, "turn": 0, "stage": 0, "sees_player": false,
		"stride": 0.0, "alive": true}
	var hold := {"state": 0, "carries": 0, "dangles": 0, "armplay_beat": 0, "arm_tear_count": 0, "leg_tear_count": 0,
		"fatal_hang": false, "fatal_release_beat": 0, "releases": 0, "claims": 0, "waste_not": 0}
	var feet := Vector3(0, 154, 0)
	var head := Vector3(0, 155.6, 0)
	var player := Vector3(6, 154, 0)
	var frame := func(seconds: float) -> void:
		for i in int(seconds * 60.0):
			sound.update(data, 1.0 / 60.0, true, feet, head, player)
			sound.hear_hold(hold)
	frame.call(1.0)
	# Hunting by ear: she talks to him now and then.
	frame.call(70.0)
	var after_hunt: int = audio.said.size()
	_check(after_hunt >= 3, "hunting by ear said %d lines in 70 s" % after_hunt)
	_check(audio.said.all(func(s): return s["moment"] == &"hunting"), "something else said while hunting by ear")
	# She sees him.
	data["sees_player"] = true
	frame.call(2.0)
	_check(audio.said.any(func(s): return s["moment"] == &"sees"), "the sight of him said nothing")
	# A hold: the grab, the carry, the dangle with her other hand at his arm torn, then the apron.
	for hold_number in 2:
		hold["state"] = 12
		frame.call(1.0)
		hold["state"] = 13
		hold["carries"] = int(hold["carries"]) + 1
		frame.call(8.0)
		hold["state"] = 14
		hold["dangles"] = int(hold["dangles"]) + 1
		frame.call(20.0)
		if hold_number == 0:
			hold["arm_tear_count"] = int(hold["arm_tear_count"]) + 1
			frame.call(6.0)
			# Claimed: the apron wiped, then the hold over (she claims him).
			hold["state"] = 18
			hold["claims"] = int(hold["claims"]) + 1
			hold["waste_not"] = int(hold["waste_not"]) + 1
			frame.call(1.0)
			hold["state"] = 0
			frame.call(4.0)
		else:
			# He breaks free: the hold ends with nothing of hers let go.
			hold["state"] = 0
			frame.call(4.0)
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
	print("MATRON_SPEECH lines said %d: %s" % [audio.said.size(), moments])
