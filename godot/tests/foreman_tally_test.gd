extends SceneTree

## The foreman's count of falls and throws (presentation/life/foreman_radio.gd _tally):
## a count is news only when it steps up by one while the radio watches, once a
## launch. The first read, a jump (a saved climb loaded), a drop (a restore) and
## the seated beam's final line are all silent for the tally.
## Run:
## tools/godot/godot --headless --path godot --script res://tests/foreman_tally_test.gd

const ForemanRadio := preload("res://presentation/life/foreman_radio.gd")

const DT := 0.1

var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("FOREMAN_TALLY %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _ctx(deaths: int, throws: int, beam: bool = false) -> Dictionary:
	return {"y": 5.0, "support": 0, "matron_released": false, "beam_seated": beam,
		"deaths": deaths, "throws": throws}


# Frames with the same counts: long enough for any line to show and close.
func _hold(radio: Node, deaths: int, throws: int, seconds: float, beam: bool = false) -> void:
	for i in int(round(seconds / DT)):
		radio.update(DT, _ctx(deaths, throws, beam))


func _run() -> void:
	# 1. The first read is history: a continued save at 6 falls says nothing, 7 is not a milestone.
	var radio := ForemanRadio.new()
	_hold(radio, 6, 3, 1.0)
	_hold(radio, 7, 3, 1.0)
	_check(radio.lines_said == 0, "first_read_is_history", "lines=%d" % radio.lines_said)
	radio.free()

	# 2. Stepping up onto 1, 5 and 12 says each line, once, and nothing else.
	radio = ForemanRadio.new()
	_hold(radio, 0, 0, 1.0)
	_check(radio.lines_said == 0, "nothing_at_zero", "lines=%d" % radio.lines_said)
	radio.update(DT, _ctx(1, 0))
	_check(radio.text == radio.LINES[&"fall_1"], "fall_1_said", "text='%s'" % radio.text)
	_hold(radio, 1, 0, 12.0)
	for n in [2, 3, 4]:
		_hold(radio, n, 0, 0.2)
	_check(radio.lines_said == 1, "between_milestones_silent", "lines=%d" % radio.lines_said)
	radio.update(DT, _ctx(5, 0))
	_check(radio.text == radio.LINES[&"fall_5"], "fall_5_said", "text='%s'" % radio.text)
	_hold(radio, 5, 0, 12.0)
	for n in [6, 7, 8, 9, 10, 11]:
		_hold(radio, n, 0, 0.2)
	radio.update(DT, _ctx(12, 0))
	_check(radio.text == radio.LINES[&"fall_12"], "fall_12_said", "text='%s'" % radio.text)
	_hold(radio, 12, 0, 30.0)
	_check(radio.lines_said == 3, "each_once_no_repeat", "lines=%d" % radio.lines_said)
	radio.free()

	# 3. A jump (a saved climb loaded after the first frame) is not news.
	radio = ForemanRadio.new()
	_hold(radio, 0, 0, 0.5)
	_hold(radio, 5, 0, 1.0)
	_check(radio.lines_said == 0, "jump_is_history", "lines=%d" % radio.lines_said)
	radio.free()

	# 4. A restore (the count drops) then the climb up again: once a launch only.
	radio = ForemanRadio.new()
	_hold(radio, 0, 0, 0.2)
	radio.update(DT, _ctx(1, 0))
	_hold(radio, 1, 0, 12.0)
	_hold(radio, 0, 0, 0.5)
	radio.update(DT, _ctx(1, 0))
	_hold(radio, 1, 0, 12.0)
	_check(radio.lines_said == 1, "once_a_launch", "lines=%d" % radio.lines_said)
	radio.free()

	# 5. The twelfth throw.
	radio = ForemanRadio.new()
	_hold(radio, 0, 11, 0.3)
	radio.update(DT, _ctx(0, 12))
	_check(radio.text == radio.LINES[&"throws_12"], "throws_12_said", "text='%s'" % radio.text)
	radio.free()
	radio = ForemanRadio.new()
	_hold(radio, 0, 0, 0.3)
	_hold(radio, 0, 12, 1.0)
	_check(radio.lines_said == 0, "throw_jump_is_history", "lines=%d" % radio.lines_said)
	radio.free()

	# 6. The seated beam's admission is never over-talked by the count.
	radio = ForemanRadio.new()
	_hold(radio, 4, 0, 0.3)
	radio.update(DT, _ctx(4, 0, true))
	radio.update(DT, _ctx(5, 0, true))
	_hold(radio, 5, 0, 12.0, true)
	_check(radio.lines_said == 1 and radio.LINES[&"last_beam"] != radio.LINES[&"fall_5"],
		"beam_line_not_over_talked", "lines=%d" % radio.lines_said)
	radio.free()

	quit(1 if _failures else 0)
