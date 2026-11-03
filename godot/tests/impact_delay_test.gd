extends SceneTree

## Far hits arrive as sound does (audio_director.gd update_impacts): a hit beyond
## IMPACT_FAR_M is started after distance / 343 s, a near one at once, one beyond
## IMPACT_REACH_M never, at most IMPACT_PENDING_MAX in flight, and a restore
## (the hit sequence going back) drops those in flight.
## Run:
## tools/godot/godot --headless --path godot --script res://tests/impact_delay_test.gd
## Proved by the director's started-hit counter, not by listening: the VM has no audio device.

const Director := preload("res://presentation/audio/audio_director.gd")

const DT := 1.0 / 60.0

var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("IMPACT_DELAY %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _hit(sequence: int, at: Vector3) -> Dictionary:
	return {"sequence": sequence, "sound": 0, "at": at, "energy_j": 500.0, "mass_kg": 160.0}


func _frames(director: Node, seconds: float) -> void:
	for i in int(round(seconds / DT)):
		director.update(DT, Vector3.ZERO, Vector3.ZERO, true, 0, 0, false, 0, false)


func _fresh() -> Node:
	var director := Director.new()
	root.add_child(director)
	return director


func _drop(director: Node) -> void:
	root.remove_child(director)
	director.free()


func _run() -> void:
	_check(Director.impact_delay_s(0.0) == 0.0 and Director.impact_delay_s(Director.IMPACT_FAR_M) == 0.0,
		"delay_none_up_to_far", "0 m and %.0f m" % Director.IMPACT_FAR_M)
	var d120 := Director.impact_delay_s(120.0)
	_check(absf(d120 - 120.0 / 343.0) < 1e-6, "delay_is_distance_over_343", "120 m -> %.4f s" % d120)

	# A hit 120 m away: not started at once, not before 0.35 s, started by 0.40 s.
	var director := _fresh()
	director.update_impacts([_hit(1, Vector3(120.0, 0.0, 0.0))], Vector3.ZERO)
	_check(director.impacts_started == 0, "far_not_at_once", "started=%d" % director.impacts_started)
	_frames(director, 0.30)
	_check(director.impacts_started == 0, "far_not_before_travel_time", "at 0.30 s started=%d" % director.impacts_started)
	_frames(director, 0.10)
	_check(director.impacts_started == 1, "far_after_travel_time", "at 0.40 s started=%d" % director.impacts_started)
	_check(int(director.impacts_heard.get(0, 0)) == 1, "far_counted_as_heard_at_once", "heard=%s" % str(director.impacts_heard))
	_drop(director)

	# A near hit (10 m) starts at once; one past the reach (200 m) never does.
	director = _fresh()
	director.update_impacts([_hit(1, Vector3(10.0, 0.0, 0.0)), _hit(2, Vector3(200.0, 0.0, 0.0))], Vector3.ZERO)
	_check(director.impacts_started == 1, "near_at_once_beyond_reach_never", "started=%d" % director.impacts_started)
	_frames(director, 2.0)
	_check(director.impacts_started == 1, "beyond_reach_stays_silent", "started=%d" % director.impacts_started)
	_drop(director)

	# More than IMPACT_PENDING_MAX in flight: the oldest are dropped, the newest kept.
	director = _fresh()
	var many: Array = []
	for i in 30:
		many.append(_hit(i + 1, Vector3(100.0, 0.0, 0.0)))
	director.update_impacts(many, Vector3.ZERO)
	_frames(director, 1.0)
	_check(director.impacts_started == Director.IMPACT_PENDING_MAX, "pending_is_bounded",
		"30 far hits -> started=%d (cap %d)" % [director.impacts_started, Director.IMPACT_PENDING_MAX])
	_drop(director)

	# A restore (the sequence goes back): hits in flight never happened.
	director = _fresh()
	director.update_impacts([_hit(5, Vector3(120.0, 0.0, 0.0))], Vector3.ZERO)
	director.update_impacts([_hit(1, Vector3(300.0, 0.0, 0.0))], Vector3.ZERO)
	_frames(director, 1.0)
	_check(director.impacts_started == 0, "restore_drops_hits_in_flight", "started=%d" % director.impacts_started)
	_drop(director)

	quit(1 if _failures else 0)
