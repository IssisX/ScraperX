extends SceneTree
## Her wake and his breath are real recordings (issue #416, rule 41): the
## bank holds four recorded wake takes and three recorded halves of each of
## his breaths, all loaded from files and none generated in code; the
## director's voice list holds no dead names; her takes choose the wake the
## way they choose every other family of hers.
##
##   tools/godot/godot --headless --path godot --script res://tests/sound_bank_test.gd

const SoundBank := preload("res://presentation/audio/sound_bank.gd")
const Director := preload("res://presentation/audio/audio_director.gd")
const HerTakes := preload("res://presentation/audio/her_takes.gd")

var _fails := 0


func _init() -> void:
	_bank()
	_source()
	_director()
	print("SOUND_BANK %s fails=%d" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(0 if _fails == 0 else 1)


func _check(ok: bool, what: String) -> void:
	if not ok:
		_fails += 1
		if _fails <= 20:
			print("SOUND_BANK FAIL %s" % what)


func _takes(bank: RefCounted, name: StringName, count: int) -> Array:
	_check(bank.clips.has(name), "%s missing from the bank" % name)
	if not bank.clips.has(name):
		return []
	var takes: Array = bank.clips[name]
	_check(takes.size() == count, "%s has %d takes, want %d" % [name, takes.size(), count])
	return takes


func _recorded(takes: Array, name: StringName) -> void:
	for take in takes:
		_check(take is AudioStreamOggVorbis, "%s take is %s, not a recording" % [name, take.get_class()])
	for i in takes.size():
		for j in range(i + 1, takes.size()):
			_check(takes[i] != takes[j], "%s takes %d and %d are the same stream" % [name, i, j])


# The bank, built the way the director builds it: the world's bank, then
# hers on a second instance.
func _bank() -> void:
	var world := SoundBank.new()
	world.build()
	var her := SoundBank.new()
	her.build_her()
	var breath_in := _takes(world, &"breath_in", 3)
	var breath_out := _takes(world, &"breath_out", 3)
	_recorded(breath_in, &"breath_in")
	_recorded(breath_out, &"breath_out")
	for take in breath_in:
		_check(absf(take.get_length() - 0.34) < 0.02, "breath_in take is %.2f s, want 0.34" % take.get_length())
	for take in breath_out:
		_check(absf(take.get_length() - 0.42) < 0.02, "breath_out take is %.2f s, want 0.42" % take.get_length())
	_check(not world.clips.has(&"matron_wake"), "matron_wake is still in the world's bank")
	_check(not her.clips.has(&"breath_in") and not her.clips.has(&"breath_out"),
		"his breath is in her bank")
	var wake := _takes(her, &"matron_wake", 4)
	_recorded(wake, &"matron_wake")
	for take in wake:
		_check(absf(take.get_length() - 3.4) < 0.02, "matron_wake take is %.2f s, want 3.40" % take.get_length())


# No human sound is generated in code: the two generators are gone from
# the bank's source, and the wake is still named there as a recording.
func _source() -> void:
	var source := FileAccess.get_file_as_string("res://presentation/audio/sound_bank.gd")
	_check(not source.is_empty(), "sound_bank.gd did not read")
	_check(not "func _matron_wake(" in source, "sound_bank.gd still generates matron_wake")
	_check(not "func _breath(" in source, "sound_bank.gd still generates breath")
	_check("&\"matron_wake\": 4" in source, "matron_wake is not a recorded family")


# The director's voice list holds no dead names, and her takes choose the
# wake: no take twice running, none twice in her ring.
func _director() -> void:
	_check(not (&"matron_croak" in Director.HER_VOICE_CLIPS), "HER_VOICE_CLIPS still names matron_croak")
	_check(not (&"matron_cluck" in Director.HER_VOICE_CLIPS), "HER_VOICE_CLIPS still names matron_cluck")
	_check(HerTakes.FAMILIES.has(&"matron_wake"), "matron_wake has no takes family")
	var takes := HerTakes.new(7)
	var clock := 0.0
	var last := -1
	for i in 24:
		clock += 3.61
		var k := takes.choose(&"matron_wake", 4, clock)
		_check(k >= 0 and k < 4, "matron_wake chose %d of 4" % k)
		_check(k != last, "matron_wake took %d twice running" % k)
		last = k
		takes.commit(&"matron_wake", k, clock, 0)
