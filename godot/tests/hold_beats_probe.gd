extends SceneTree
## Her hold's five beats, with no native and no world: open air, her hand on
## him through the grab, the drag, the heave, the hang, and the pin. Proves a
## shot per beat, real fov and vignette per beat, no jump, the let-go owning it.
##   godot --headless --path godot --script res://tests/hold_beats_probe.gd

const Cam := preload("res://presentation/life/matron_hold_camera.gd")

# [fov deg, vignette] per beat, as in matron_hold_camera.gd BEAT_SHOTS.
const SHOTS := [[2.0, 0.10], [0.0, 0.15], [4.0, 0.08], [6.0, 0.00], [-8.0, 0.35]]

var _fails := 0


func _initialize() -> void:
	var cam: RefCounted = Cam.new()
	var him := Vector3(0.0, 1.05, 0.0)
	var head := Vector3(1.35, 1.72, 0.15)
	var eye := Transform3D(Basis.IDENTITY, Vector3(0.0, 1.65, 0.0))
	var view := eye
	var worst := 0.0
	# [state, hand, held, grip, fatal_hang, release]: the hold as the sim plays
	# it (down, drag, over, hang, pin), the release, then a hang entered again.
	var hand_high := Vector3(0.28, 2.5, 0.35)
	var hand_low := Vector3(0.28, 1.2, 0.35)
	var stages := [
		[12, hand_low, true, true, false, 0],
		[13, hand_low, true, true, false, 0],
		[14, hand_high, true, true, false, 0],
		[14, hand_low, true, true, false, 0],
		[15, hand_low, true, true, false, 0],
		[16, hand_low, false, false, false, 0],
		[14, hand_low, true, true, false, 0],
	]
	for i in stages.size():
		var st: Array = stages[i]
		var beat := i if i < 5 else (-1 if i == 5 else 3)
		for _f in 72:
			var next: Transform3D = cam.frame(_hold(int(st[0]), him, st[1], bool(st[2]), bool(st[3]),
				bool(st[4]), int(st[5])), head, view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
			worst = maxf(worst, next.origin.distance_to(view.origin))
			view = next
		if i < 5:
			if int(cam.beat) != beat or not cam.third_person():
				_fail("beat %d" % i, "beat %s third %s" % [cam.beat, cam.third_person()])
			elif absf(cam.fov_offset() - float(SHOTS[i][0])) > 1.0 or absf(cam.vignette() - float(SHOTS[i][1])) > 0.08:
				_fail("beat %d shot" % i, "fov %.2f (want %.1f) vig %.2f (want %.2f)" % [
					cam.fov_offset(), float(SHOTS[i][0]), cam.vignette(), float(SHOTS[i][1])])
			else:
				print("HOLD_BEATS beat %d, fov %.1f vig %.2f: PASS" % [i, cam.fov_offset(), cam.vignette()])
		elif i == 5:
			if cam.third_person() or int(cam.beat) != -1:
				_fail("release", "third %s beat %s" % [cam.third_person(), cam.beat])
			else:
				print("HOLD_BEATS release, back in his eye: PASS")
	if int(cam.beat_cuts) != 6 or int(cam.beat_entries[3]) != 2:
		_fail("cuts", "cuts %d entries %s (want 6, hang 2)" % [cam.beat_cuts, str(cam.beat_entries)])
	else:
		print("HOLD_BEATS one cut per entry (cuts %d): PASS" % cam.beat_cuts)
	if worst < 0.45:
		print("HOLD_BEATS no jump (worst step %.3f m): PASS" % worst)
	else:
		_fail("jump", "worst step %.3f m" % worst)
	# The let-go over the drop still owns the frame: slow motion, no rip shot.
	var drop := view
	for _f in 10:
		drop = cam.frame(_hold(14, him + Vector3(0, -0.4, 0), hand_low, false, false, true, 1),
			head, drop, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	if not cam.rip_active and cam.time_scale() < 1.0:
		print("HOLD_BEATS let-go took the frame (scale %.2f): PASS" % cam.time_scale())
	else:
		_fail("let-go", "rip_active %s scale %s" % [cam.rip_active, cam.time_scale()])
	print("HOLD_BEATS %s" % ("PASS" if _fails == 0 else "FAIL"))
	quit(1 if _fails > 0 else 0)


func _hold(state: int, him: Vector3, hand: Vector3, held: bool, grip_on: bool, fatal_hang: bool,
		release: int) -> Dictionary:
	return {
		"held": held,
		"claimed": false,
		"state": state,
		"grip_on": grip_on,
		"fatal_hang": fatal_hang,
		"player_position": him,
		"body_rotation": Quaternion.IDENTITY,
		"matron_hand": hand,
		"matron_hand_present": true,
		"hand": hand,
		"held_ankle": him + Vector3(0.1, -0.8, 0.0),
		"arm_tear_count": 0,
		"severed_arm_present": false,
		"severed_arm_position": him,
		"fatal_release_beat": release,
		"fatal_edge": Vector3(2.0, 1.0, 0.0),
		"fatal_out": Vector2(1.0, 0.0),
		"fatality_beat": -1,
		"fatality_beat_edges": PackedInt64Array(),
	}


func _open(a: Vector3, b: Vector3, _skip: Vector3, _radius: float) -> float:
	return a.distance_to(b)


func _fail(where: String, detail: String) -> void:
	_fails += 1
	print("HOLD_BEATS FAIL %s: %s" % [where, detail])
