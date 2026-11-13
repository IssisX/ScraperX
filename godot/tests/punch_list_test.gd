extends SceneTree
## J-131: the punch list window (presentation/ui/punch_list.gd) -- the lines
## the pause card and the HUD draw from the native done flags. Titles are
## the B3 mission names in B3 order; the window holds at most three lines,
## the current job first among the open ones, the just-done line kept in
## view; exactly one line is current unless every line is done.
##
##   tools/godot/godot --headless --path godot --script res://tests/punch_list_test.gd

const PunchList := preload("res://presentation/ui/punch_list.gd")

var _fails := 0


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	# None done: both lines, the first is the current job.
	var fresh := PunchList.lines([false, false])
	_check(fresh.size() == 2, "fresh list has both lines")
	_check(_title(fresh, 0) == "CLOCK IN" and _current(fresh, 0) and not _done(fresh, 0),
		"fresh: first line is the current job")
	_check(_title(fresh, 1) == "BRING THE PLANT UP" and not _current(fresh, 1) and not _done(fresh, 1),
		"fresh: second line is open, not current")
	_check(_current_count(fresh) == 1, "fresh: exactly one current line")
	# First done: it stays on the list as done, the second is current.
	var first_done := PunchList.lines([true, false])
	_check(first_done.size() == 2, "first-done list has both lines")
	_check(_done(first_done, 0) and not _current(first_done, 0), "first-done: first line is done")
	_check(_current(first_done, 1) and not _done(first_done, 1), "first-done: second line is current")
	_check(_current_count(first_done) == 1, "first-done: exactly one current line")
	# Second done alone: the first is still the current job.
	var second_done := PunchList.lines([false, true])
	_check(_current(second_done, 0) and not _done(second_done, 0), "second-done: first line is current")
	_check(_done(second_done, 1) and not _current(second_done, 1), "second-done: second line is done")
	_check(_current_count(second_done) == 1, "second-done: exactly one current line")
	# All done: both lines shown as done, no current job.
	var all_done := PunchList.lines([true, true])
	_check(all_done.size() == 2, "all-done list has both lines")
	_check(_done(all_done, 0) and _done(all_done, 1), "all-done: both lines are done")
	_check(_current_count(all_done) == 0, "all-done: no current line")
	# A short read counts a missing entry as open, so a job is never hidden.
	var short := PunchList.lines([])
	_check(short.size() == 2 and _current(short, 0), "short read: both jobs open, first current")
	# The list never exceeds three lines.
	_check(fresh.size() <= PunchList.MAX_LINES and all_done.size() <= PunchList.MAX_LINES,
		"list holds at most three lines")
	print("PUNCH_LIST %s fails=%d" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(0 if _fails == 0 else 1)


func _title(list: Array, i: int) -> String:
	return String((list[i] as Dictionary).get("title", ""))


func _done(list: Array, i: int) -> bool:
	return bool((list[i] as Dictionary).get("done", false))


func _current(list: Array, i: int) -> bool:
	return bool((list[i] as Dictionary).get("current", false))


func _current_count(list: Array) -> int:
	var count := 0
	for i in list.size():
		if _current(list, i):
			count += 1
	return count


func _check(ok: bool, what: String) -> void:
	if not ok:
		_fails += 1
		print("PUNCH_LIST FAIL: " + what)
