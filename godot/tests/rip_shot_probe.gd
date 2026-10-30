extends SceneTree
## The arm-rip camera, with no native and no world: open air, her hand on
## him, then the arm leaves. Proves the shot is wide, still, aimed at the
## arm, and that it does not slow the sim. The fatality let-go still owns
## the frame once he is falling.
##   godot --headless --path godot --script res://tests/rip_shot_probe.gd

const Cam := preload("res://presentation/life/matron_hold_camera.gd")

var _fails := 0


func _initialize() -> void:
	var cam: RefCounted = Cam.new()
	var him := Vector3(0.0, 1.05, 0.0)
	var head := Vector3(1.35, 1.72, 0.15)
	var hand := Vector3(0.28, 1.25, 0.35)
	var eye := Transform3D(Basis.IDENTITY, Vector3(0.0, 1.65, 0.0))
	var severed := Vector3(-0.55, 1.15, 0.9)
	var settle: Transform3D = _run(cam, _hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	if cam.third_person() and cam.time_scale() == 1.0 and int(cam.rip_holds) == 0:
		print("RIP_SHOT carry third-person, sim unslowed, no rip yet: PASS")
	else:
		_fail("carry", "third %s scale %s holds %s" % [cam.third_person(), cam.time_scale(), cam.rip_holds])
	var last := settle
	var worst := 0.0
	for _i in 72:
		var view: Transform3D = cam.frame(_hold(15, 0, him, hand, false, severed), head, last, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
		worst = maxf(worst, view.origin.distance_to(last.origin))
		last = view
	var subject: Vector3 = hand.lerp(him, 0.35)
	var aim: Vector3 = (-last.basis.z).normalized()
	var want: Vector3 = (subject - last.origin).normalized()
	var aim_deg := rad_to_deg(aim.angle_to(want))
	if cam.rip_active and cam.rip_dist >= 5.0 and last.origin.y > subject.y + 2.0 and aim_deg < 8.0 and cam.time_scale() == 1.0:
		print("RIP_SHOT wide still aim dist=%.2f up=%.2f aim_deg=%.2f: PASS" % [cam.rip_dist, last.origin.y - subject.y, aim_deg])
	else:
		_fail("armplay", "active %s dist %.2f up %.2f aim %.2f scale %s" % [cam.rip_active, cam.rip_dist, last.origin.y - subject.y, aim_deg, cam.time_scale()])
	var held_still := last
	for _i in 30:
		held_still = cam.frame(_hold(15, 0, him, hand, false, severed), head, held_still, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	var other: RefCounted = Cam.new()
	_run(other, _hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	var swung: Transform3D = _run(other, _hold(15, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	for _i in 30:
		swung = other.frame(_hold(15, 0, him, hand, false, severed), head, swung, Vector2(1.7, -0.8), 75.0, 0, 1.0 / 60.0, _open)
	# The stick must not drag the shot. Two cameras, one looked wildly.
	var stick := held_still.origin.distance_to(swung.origin)
	if stick < 0.15:
		print("RIP_SHOT stick ignored (%.3f m): PASS" % stick)
	else:
		_fail("stick", "%.3f m" % stick)
	if worst < 0.45:
		print("RIP_SHOT no jump (worst step %.3f m): PASS" % worst)
	else:
		_fail("jump", "worst step %.3f m" % worst)
	var torn: Transform3D = cam.frame(_hold(15, 1, him, hand, true, severed), head, held_still, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	var torn_subject: Vector3 = him.lerp(severed, 0.45)
	for _i in 20:
		torn = cam.frame(_hold(15, 1, him, hand, true, severed), head, torn, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	var torn_aim: Vector3 = (-torn.basis.z).normalized()
	var torn_want: Vector3 = (torn_subject - torn.origin).normalized()
	var torn_deg := rad_to_deg(torn_aim.angle_to(torn_want))
	if int(cam.rip_holds) == 1 and cam.time_scale() == 1.0 and torn_deg < 8.0 and cam.rip_dist >= 5.0:
		print("RIP_SHOT arm leaving, still unslowed, aim_deg=%.2f: PASS" % torn_deg)
	else:
		_fail("tear", "holds %s scale %s aim %.2f dist %.2f" % [cam.rip_holds, cam.time_scale(), torn_deg, cam.rip_dist])
	# Once he is falling, the let-go owns the frame. The rip shot steps aside.
	var drop: Transform3D = cam.frame(_hold(15, 1, him, hand, true, severed, false, 1), head, torn, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	for _i in 10:
		drop = cam.frame(_hold(15, 1, him + Vector3(0, -0.4, 0), hand, true, severed, false, 1), head, drop, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	if not cam.rip_active and cam.time_scale() < 1.0:
		print("RIP_SHOT let-go took the frame back (scale %.2f): PASS" % cam.time_scale())
	else:
		_fail("let-go", "rip_active %s scale %s" % [cam.rip_active, cam.time_scale()])
	print("RIP_SHOT %s" % ("PASS" if _fails == 0 else "FAIL"))
	quit(1 if _fails > 0 else 0)


func _run(cam: RefCounted, hold: Dictionary, head: Vector3, eye: Transform3D, look: Vector2, seconds: float) -> Transform3D:
	var view := eye
	var n := int(seconds * 60.0)
	for _i in n:
		view = cam.frame(hold, head, view, look, 75.0, 0, 1.0 / 60.0, _open)
	return view


func _hold(state: int, tears: int, him: Vector3, hand: Vector3, arm: bool, severed: Vector3, held := true, release := 0) -> Dictionary:
	return {
		"held": held,
		"claimed": false,
		"state": state,
		"player_position": him,
		"body_rotation": Quaternion.IDENTITY,
		"matron_hand": hand,
		"matron_hand_present": true,
		"hand": hand,
		"held_ankle": him + Vector3(0.1, -0.8, 0.0),
		"arm_tear_count": tears,
		"severed_arm_present": arm,
		"severed_arm_position": severed,
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
	print("RIP_SHOT FAIL %s: %s" % [where, detail])
