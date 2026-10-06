extends SceneTree

const Director := preload("res://presentation/audio/audio_director.gd")

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var failures := 0
	# Expected stride counts come from distance walked relative to the floor,
	# not the distance a lift/rotating support carries a planted player.
	var cases := [
		["carried", Vector3(4, 0, 0), Vector3(4, 0, 0), true, 0, 0],
		["walk_on_carrier", Vector3(6, 0, 0), Vector3(4, 0, 0), true, 5, 6],
		["rotating_contact", Vector3(0, 0, 4), Vector3(0, 0, 4), true, 0, 0],
		["fixed_floor", Vector3(2, 0, 0), Vector3.ZERO, true, 5, 6],
		["airborne", Vector3(5, 0, 0), Vector3.ZERO, false, 0, 0],
	]
	for item in cases:
		var director := Director.new()
		root.add_child(director)
		for i in 120:
			director.update(1.0 / 60.0, Vector3(0, 121.9, 0), item[1], item[3], 11, 0, false, 0, false, item[2])
		var ok: bool = director.steps >= item[4] and director.steps <= item[5]
		print("FOOTSTEP_CONTACT %s case=%s steps=%d expected=%d..%d" % ["PASS" if ok else "FAIL", item[0], director.steps, item[4], item[5]])
		if not ok:
			failures += 1
		root.remove_child(director)
		director.free()
	quit(1 if failures else 0)
