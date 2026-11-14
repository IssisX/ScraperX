# The punch list (J-131, B5 item 3, B3 table): the mission lines as drawn.
# Titles are in B3 order, indexed by the native Objectives::Id number
# (J-129: 0 Mission 1 "Clock in", 1 Mission 7 "Bring the plant up").
# lines(done) is pure: given one done boolean per id, it returns the
# window the pause card and the HUD draw -- at most three lines, the
# current job first among the open ones, the just-done line kept in view.
# The done flags come from the bridge getter is_objective_done(id), which
# lands with the per-tick hook after J-116's F8 bump; until then every
# read is false and the list shows both jobs open. No script decides an
# outcome: this only windows flags the native owns.

const TITLES := ["CLOCK IN", "BRING THE PLANT UP"]
const MAX_LINES := 3


# One line per entry: {"title", "done", "current"}. A missing done entry
# counts as open, so a short read never hides a job.
static func lines(done: Array) -> Array:
	var count := TITLES.size()
	var is_done: Array[bool] = []
	for i in count:
		is_done.append(i < done.size() and bool(done[i]))
	var current := count
	for i in count:
		if not is_done[i]:
			current = i
			break
	var start := 0
	if current >= count:
		start = maxi(0, count - MAX_LINES)
	elif current > 0:
		start = current - 1
	var out: Array = []
	for i in range(start, mini(start + MAX_LINES, count)):
		out.append({"title": TITLES[i], "done": is_done[i], "current": i == current})
	return out
