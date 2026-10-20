extends SceneTree
# The director's cost per frame (no cue fired: the common case), 20,000 frames.
const Director := preload("res://fear_director.gd")

func _init() -> void:
	var dir = Director.new()
	var her := {"state": 3, "stage": 1, "released": true, "alive": true, "sees_player": false,
		"position": Vector3(5, 155.05, -128.5), "rage": 0.3, "shoves": 0, "catches": 0, "climbing": false, "speed": 6.8}
	var ctx := {"player": Vector3(0, 155, -128.5), "velocity": Vector3(5, 0, 0),
		"camera": Transform3D(Basis.IDENTITY, Vector3(0, 155.6, -128.5)), "radio": "", "deaths": 0,
		"fear_clip": "", "fear_line": "", "climbing": false, "hour": 15.0}
	var n := 20000
	var t0 := Time.get_ticks_usec()
	for i in n:
		her["position"] = Vector3(5.0 + 0.0001 * i, 155.05, -128.5)
		dir.update(1.0 / 60.0, her, ctx)
	var us := float(Time.get_ticks_usec() - t0) / float(n)
	print("FEAR_BENCH frames=%d us_per_frame=%.2f cues=%d" % [n, us, dir.cues.size()])
	quit()
