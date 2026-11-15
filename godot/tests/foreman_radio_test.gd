extends SceneTree
## J-130: Brenner's job lines on the foreman radio (text first).
## Missions 1 "Clock in" and 7 "Bring the plant up": one line when the job
## is given (a place cue, the same as every other beat), one line when it is
## done (J-129's done flag, read from ctx as `objectives_done`, objective
## index -> bool, the same numbering as Objectives::Id: 0 is mission 1
## "Clock in", 1 is mission 7 "Bring the plant up"). A done line shows only
## to true during this launch: a flag already true at launch was earned (and
## said) in an earlier launch, so it is never shown twice for one done flag.
## The beam seated keeps the last word: no job line queues over his admission.
## Every line fits the Fold folded: no line is wider than the longest line
## already shipping (last_beam), measured at the radio's own size.
##
##   tools/godot/godot --headless --path godot --script res://tests/foreman_radio_test.gd

const ForemanRadio := preload("res://presentation/life/foreman_radio.gd")
const UiStyle := preload("res://presentation/ui/ui_style.gd")

const FRAME_S := 1.0 / 60.0
const S1_CAGE_ENTITY := 2200
const TOWER_ENTITY := 11

var _fails := 0


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	_silence_before_the_cage()
	_mission_1_given_then_done()
	_done_at_launch_is_not_said_again()
	_mission_7_given_then_done()
	_beam_seated_keeps_the_last_word()
	_lines_fit_the_fold()
	print("FOREMAN_RADIO %s fails=%d" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(0 if _fails == 0 else 1)


func _check(ok: bool, what: String) -> void:
	if not ok:
		_fails += 1
		if _fails <= 20:
			print("FOREMAN_RADIO FAIL %s" % what)


func _ctx(y: float, support: int, released: bool, beam_seated: bool, done: Dictionary) -> Dictionary:
	return {"y": y, "support": support, "matron_released": released,
		"beam_seated": beam_seated, "objectives_done": done}


func _yard() -> Dictionary:
	return _ctx(0.0, 0, false, false, {})


func _step(radio: ForemanRadio, seconds: float, ctx: Dictionary) -> void:
	for i in maxi(1, roundi(seconds * 60.0)):
		radio.update(FRAME_S, ctx)


func _wait_text(radio: ForemanRadio, needle: String, seconds: float, ctx: Dictionary) -> bool:
	for i in maxi(1, roundi(seconds * 60.0)):
		radio.update(FRAME_S, ctx)
		if needle in String(radio.text):
			return true
	return false


func _wait_quiet(radio: ForemanRadio, seconds: float, ctx: Dictionary) -> bool:
	for i in maxi(1, roundi(seconds * 60.0)):
		radio.update(FRAME_S, ctx)
		if String(radio.text).is_empty():
			return true
	return false


# Nothing is said before a machine is touched: the yard is silent.
func _silence_before_the_cage() -> void:
	var radio := ForemanRadio.new()
	_step(radio, 2.0, _yard())
	_check(int(radio.lines_said) == 0, "the foreman spoke before the climber touched a machine")
	_check(String(radio.text).is_empty(), "a radio line showed in the yard")
	radio.free()


func _mission_1_given_then_done() -> void:
	var radio := ForemanRadio.new()
	_step(radio, 1.0, _yard())
	var cage := _ctx(0.0, S1_CAGE_ENTITY, false, false, {})
	_check(_wait_text(radio, "Saturday", 2.0, cage), "stepping into S1's cage did not bring his first line")
	_check(int(radio.lines_said) == 1, "the cage step said more than his first line")
	_check(_wait_quiet(radio, 12.0, cage), "his first line never closed")
	_check(_wait_text(radio, "Clock in", 5.0, cage), "the cage step did not bring the Clock in job line")
	_check(int(radio.lines_said) == 2, "the job line did not follow his first line")
	_check(_wait_quiet(radio, 12.0, cage), "the Clock in line never closed")
	_step(radio, 3.0, cage)
	_check(int(radio.lines_said) == 2, "a beat repeated on stepping into the cage again")
	var done := _ctx(0.0, 0, false, false, {0: true})
	_check(_wait_text(radio, "One line off the list", 5.0, done),
		"the done flag did not bring the Clock in done line")
	_check(int(radio.lines_said) == 3, "the done line did not follow the job line")
	_step(radio, 10.0, done)
	_check(int(radio.lines_said) == 3, "the done line repeated while its flag stayed true")
	radio.free()


# A flag already true at launch was earned in an earlier launch: it is
# never said again for that done flag.
func _done_at_launch_is_not_said_again() -> void:
	var radio := ForemanRadio.new()
	var done := _ctx(0.0, 0, false, false, {0: true, 1: true})
	_step(radio, 10.0, done)
	_check(int(radio.lines_said) == 0, "a done flag true at launch was said again")
	_check(String(radio.text).is_empty(), "a done-at-launch line showed")
	radio.free()


func _mission_7_given_then_done() -> void:
	var radio := ForemanRadio.new()
	_step(radio, 1.0, _yard())
	var ring := _ctx(221.0, TOWER_ENTITY, true, false, {})
	_check(_wait_text(radio, "What was that", 2.0, ring), "her locker bursting did not bring his locker line")
	_check(int(radio.lines_said) == 1, "the locker line did not come first at the ring")
	_check(_wait_quiet(radio, 12.0, ring), "his locker line never closed")
	_check(_wait_text(radio, "Don't come down", 5.0, ring), "the ring did not bring his lifeline")
	_check(int(radio.lines_said) == 2, "the lifeline did not follow the locker line")
	_check(_wait_quiet(radio, 12.0, ring), "his lifeline never closed")
	_check(_wait_text(radio, "Bring the plant up", 5.0, ring),
		"the ring did not bring the Bring the plant up job line")
	_check(int(radio.lines_said) == 3, "the job line did not follow the lifeline")
	_check(_wait_quiet(radio, 12.0, ring), "the Bring the plant up line never closed")
	var done := _ctx(221.0, TOWER_ENTITY, true, false, {1: true})
	_check(_wait_text(radio, "breathing again", 5.0, done),
		"the done flag did not bring the Bring the plant up done line")
	_check(int(radio.lines_said) == 4, "the done line did not follow the job line")
	_step(radio, 10.0, done)
	_check(int(radio.lines_said) == 4, "the done line repeated while its flag stayed true")
	radio.free()


# The beam seated keeps the last word: a done flag flipping while the beam
# is seated brings no job line over his admission.
func _beam_seated_keeps_the_last_word() -> void:
	var radio := ForemanRadio.new()
	_step(radio, 1.0, _yard())
	var seated := _ctx(0.0, 0, false, true, {0: true})
	_check(_wait_text(radio, "Your turn", 2.0, seated), "seating the beam did not bring his admission")
	_check(int(radio.lines_said) == 1, "a job line queued over his admission")
	_step(radio, 10.0, seated)
	_check(int(radio.lines_said) == 1, "his admission repeated, or a job line followed it")
	radio.free()


# No line is wider than the longest line already shipping (last_beam),
# measured with the radio's own font at the radio's own size: width scales
# with the layout unit, so the bound holds at any width, folded included.
func _lines_fit_the_fold() -> void:
	for beat in [&"mission_1_given", &"mission_1_done", &"mission_7_given", &"mission_7_done"]:
		_check(ForemanRadio.LINES.has(beat), "the radio has no %s job line" % String(beat))
	var font := UiStyle.font_label()
	var cap := UiStyle.text_width(font, String(ForemanRadio.LINES[&"last_beam"]), 32)
	_check(cap > 0.0, "the radio font measured zero width headless (the fit check is vacuous)")
	var widest := 0.0
	var widest_beat := &""
	for beat in ForemanRadio.LINES:
		var w := UiStyle.text_width(font, String(ForemanRadio.LINES[beat]), 32)
		if w > widest:
			widest = w
			widest_beat = beat
	_check(widest <= cap + 0.01,
		"a radio line (%s, %.1f) is wider than the longest line already shipping (last_beam, %.1f)"
		% [String(widest_beat), widest, cap])
