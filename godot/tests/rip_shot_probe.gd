extends SceneTree
## The arm-rip camera, with no native and no world: open air, her hand on
## him, then the arm leaves. Proves the shot is wide, still, aimed at the
## arm, and that it does not slow the sim. The ankle grip breaks in the same
## tick, and the shot stays. The fatality let-go still owns the frame once
## he is falling.
##   godot --headless --path godot --script res://tests/rip_shot_probe.gd

const Cam := preload("res://presentation/life/matron_hold_camera.gd")

# Analytic collision fixture behind the production cone_cast interface.
# A thin steel member blocks the next exit ray, but not the current camera.
class ExitWorld extends RefCounted:
	var center := Vector3.INF
	func get_look_target(origin: Vector3, direction: Vector3, length: float) -> Dictionary:
		if not center.is_finite():
			return {"hit": false}
		var oc := origin - center
		var b := oc.dot(direction)
		var discriminant := b * b - (oc.length_squared() - 0.005 * 0.005)
		if discriminant < 0.0:
			return {"hit": false}
		var distance := -b - sqrt(discriminant)
		if distance < 0.0 or distance > length:
			return {"hit": false}
		return {"hit": true, "distance": distance, "entity": 11, "point": origin + direction * distance}

var _exit_world := ExitWorld.new()

var _fails := 0
var _block_center := Vector3.INF
var _block_radius := 0.05
var _dest_blocked := false


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
	# tear_grip: the ankle grip is gone in the same tick the arm leaves, and
	# she stays in ArmPlay. The shot must not fall back into his eye.
	var loose := Cam.new()
	var wild := Cam.new()
	var loose_view := _run(loose, _hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	var wild_view := _run(wild, _hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	for _i in 60:
		loose_view = loose.frame(_hold(15, 1, him, hand, true, severed, false), head, loose_view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
		wild_view = wild.frame(_hold(15, 1, him, hand, true, severed, false), head, wild_view, Vector2(1.7, -0.8), 75.0, 0, 1.0 / 60.0, _open)
	var loose_subject: Vector3 = him.lerp(severed, 0.45)
	var loose_aim: Vector3 = (-loose_view.basis.z).normalized()
	var loose_want: Vector3 = (loose_subject - loose_view.origin).normalized()
	var loose_deg := rad_to_deg(loose_aim.angle_to(loose_want))
	var loose_stick := loose_view.origin.distance_to(wild_view.origin)
	if loose.third_person() and loose.rip_active and loose.time_scale() == 1.0 and loose.rip_dist >= 5.0 and loose_deg < 8.0 and int(loose.rip_holds) == 1 and loose_stick < 0.15:
		print("RIP_SHOT grip broken, shot held (aim %.2f stick %.3f): PASS" % [loose_deg, loose_stick])
	else:
		_fail("grip-broken", "third %s active %s scale %s dist %.2f aim %.2f holds %s stick %.3f" % [loose.third_person(), loose.rip_active, loose.time_scale(), loose.rip_dist, loose_deg, loose.rip_holds, loose_stick])
	# ArmPlay ends inside the 1.4 s. The shot still holds.
	for _i in 20:
		loose_view = loose.frame(_hold(12, 1, him, hand, true, severed, false), head, loose_view, Vector2(1.2, 0.4), 75.0, 0, 1.0 / 60.0, _open)
		wild_view = wild.frame(_hold(12, 1, him, hand, true, severed, false), head, wild_view, Vector2(-1.1, 0.6), 75.0, 0, 1.0 / 60.0, _open)
	if loose.third_person() and loose.rip_active and loose.time_scale() == 1.0 and loose_view.origin.distance_to(wild_view.origin) < 0.15:
		print("RIP_SHOT after ArmPlay, still inside 1.4 s: PASS")
	else:
		_fail("after-armplay", "third %s active %s scale %s stick %.3f" % [loose.third_person(), loose.rip_active, loose.time_scale(), loose_view.origin.distance_to(wild_view.origin)])
	# Past 1.4 s, and no longer in ArmPlay, the wide shot ends. The sim stays at full rate.
	for _i in 40:
		loose_view = loose.frame(_hold(12, 1, him, hand, true, severed, false), head, loose_view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	if not loose.rip_active and loose.time_scale() == 1.0:
		print("RIP_SHOT hold ended after 1.4 s, sim still full rate: PASS")
	else:
		_fail("hold-end", "active %s scale %s" % [loose.rip_active, loose.time_scale()])
	# The return itself, with no look input, so the ordinary frame has not
	# been orbited away from the shot. Every frame of the ease stays under
	# the same step bound. A cut back onto the hold camera fails it.
	var leave := Cam.new()
	var leave_view := _run(leave, _hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	for _i in 60:
		leave_view = leave.frame(_hold(15, 1, him, hand, true, severed, false), head, leave_view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	var leave_worst := 0.0
	for _i in 90:
		var leave_next: Transform3D = leave.frame(_hold(12, 1, him, hand, true, severed, false), head, leave_view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
		leave_worst = maxf(leave_worst, leave_next.origin.distance_to(leave_view.origin))
		leave_view = leave_next
	if leave_worst < 0.45 and not leave.rip_active and leave.time_scale() == 1.0:
		print("RIP_SHOT exit eases, no cut (%.3f m): PASS" % leave_worst)
	else:
		_fail("exit-cut", "worst %.3f active %s scale %s" % [leave_worst, leave.rip_active, leave.time_scale()])
	_check_blocked_exit(him, head, hand, eye, severed, false)
	_check_blocked_exit(him, head, hand, eye, severed, true)
	_check_blocked_exit(him, head, hand, eye, head + Vector3(0.0, -0.75, 0.0), false)
	_check_blocked_exit(him, head, hand, eye, head + Vector3(0.0, -0.75, 0.0), true)
	# Blocked blend: the destination ray is clear, but a steel protrusion
	# sits on the blended ray close to the pivot. The wide shot must not
	# be claimed for a camera that steel has pulled back to the pivot.
	var blocked := Cam.new()
	var blocked_view := _run(blocked, _hold(15, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 0.25)
	var her_chest := head + Vector3(0.0, -0.75, 0.0)
	var pivot_est := her_chest.lerp(him, 0.55)
	var blend_dir := (blocked_view.origin - pivot_est).normalized()
	_block_center = pivot_est + blend_dir * 0.6
	blocked_view = blocked.frame(_hold(15, 0, him, hand, false, severed), head, blocked_view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _blocked)
	if not blocked.rip_active and blocked.rip_dist == 0.0 and blocked.clearance < 1.0:
		print("RIP_SHOT blocked blend, shot not claimed (clearance %.2f): PASS" % blocked.clearance)
	else:
		_fail("blocked-blend", "active %s dist %.2f clearance %.2f" % [blocked.rip_active, blocked.rip_dist, blocked.clearance])
	# The beam clears. One frame must be one eased step, not the whole shot.
	# The shot comes back over the ease, still unslowed.
	_block_center = Vector3.INF
	var opened: Transform3D = blocked.frame(_hold(15, 0, him, hand, false, severed), head, blocked_view, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	var snap := opened.origin.distance_to(blocked_view.origin)
	for _i in 90:
		opened = blocked.frame(_hold(15, 0, him, hand, false, severed), head, opened, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	if snap < 0.45 and blocked.rip_active and blocked.rip_dist >= 5.0 and blocked.time_scale() == 1.0:
		print("RIP_SHOT blocked blend clears without a snap (%.3f m): PASS" % snap)
	else:
		_fail("blocked-blend-return", "snap %.3f active %s dist %.2f" % [snap, blocked.rip_active, blocked.rip_dist])
	# Thirty frames with the wide place closed, then one open frame. The
	# weight must not have reached 1 while blocked.
	_dest_blocked = true
	var gate := Cam.new()
	var closed := _run(gate, _hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 1.2)
	for _i in 30:
		closed = gate.frame(_hold(15, 0, him, hand, false, severed), head, closed, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	_dest_blocked = false
	var first: Transform3D = gate.frame(_hold(15, 0, him, hand, false, severed), head, closed, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _open)
	var jump := first.origin.distance_to(closed.origin)
	if jump < 0.45 and gate.time_scale() == 1.0:
		print("RIP_SHOT blocked then clear, no snap (%.3f m): PASS" % jump)
	else:
		_fail("blocked-clear", "jump %.3f scale %s active %s" % [jump, gate.time_scale(), gate.rip_active])
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


func _exit_cast(a: Vector3, b: Vector3, skip: Vector3, radius: float) -> float:
	return Cam.cone_cast(_exit_world, a, b, radius, skip)


func _check_blocked_exit(him: Vector3, head: Vector3, hand: Vector3, eye: Transform3D, severed: Vector3, still_held: bool) -> void:
	_exit_world.center = Vector3.INF
	var camera := Cam.new()
	var view := eye
	for i in 72:
		view = camera.frame(_hold(13, 0, him, hand, false, severed), head, eye, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _exit_cast)
	for i in 72:
		view = camera.frame(_hold(15, 1, him, hand, true, severed), head, eye, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _exit_cast)
	var yaw: float = camera.get("_side_yaw")
	var ordinary: Vector3 = camera.pivot + camera.call("_orbit", yaw, Cam.BASE_PITCH) * float(camera.get("_dist"))
	var wide: Vector3 = camera.pivot + camera.call("_orbit", yaw, Cam.RIP_PITCH) * Cam.RIP_DIST
	var next: Vector3 = ordinary.lerp(wide, 1.0 - Cam.RIP_EASE / 60.0)
	_exit_world.center = camera.pivot + (next - camera.pivot) * 0.85
	var span := view.origin.distance_to(camera.pivot)
	var old_free := _exit_cast(camera.pivot, view.origin + (view.origin - camera.pivot).normalized() * Cam.CAM_RADIUS, Vector3.INF, Cam.CAM_RADIUS)
	var next_free := _exit_cast(camera.pivot, next, Vector3.INF, Cam.CAM_RADIUS)
	if old_free < span + Cam.CAM_RADIUS - 0.001 or next_free >= next.distance_to(camera.pivot) - 0.001:
		_fail("exit-fixture", "current %.3f/%.3f next %.3f/%.3f" % [old_free, span, next_free, next.distance_to(camera.pivot)])
	var worst := 0.0
	var worst_turn := 0.0
	var crossed := false
	for i in 300:
		var after: Transform3D = camera.frame(_hold(12, 1, him, hand, true, severed, still_held), head, eye, Vector2.ZERO, 75.0, 0, 1.0 / 60.0, _exit_cast)
		var travel := after.origin - view.origin
		if travel.length() > 0.001:
			crossed = crossed or _exit_cast(view.origin, after.origin, Vector3.INF, 0.0) < travel.length() - 0.001
		worst = maxf(worst, travel.length())
		worst_turn = maxf(worst_turn, rad_to_deg(view.basis.get_rotation_quaternion().angle_to(after.basis.get_rotation_quaternion())))
		view = after
	var returned := not camera.rip_active and float(camera.get("_rip")) <= 0.001
	returned = returned and camera.third_person() == still_held
	if not still_held:
		returned = returned and view.origin.distance_to(eye.origin) < 0.001
	if returned and worst < 0.45 and worst_turn < 10.0 and not crossed and camera.time_scale() == 1.0:
		print("RIP_SHOT blocked exit returned held=%s, no cut (%.3f m, %.2f deg): PASS" % [still_held, worst, worst_turn])
	else:
		_fail("blocked-exit", "held %s returned %s worst %.3f turn %.2f crossed %s" % [still_held, returned, worst, worst_turn, crossed])
	_exit_world.center = Vector3.INF


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
	var open_d := a.distance_to(b)
	if not _dest_blocked or open_d < 1.0e-4:
		return open_d
	var dir := (b - a) / open_d
	var pitch := asin(clampf(dir.y, -1.0, 1.0))
	# The wide destination cast only. Everything else stays open.
	if absf(open_d - 6.9) < 0.08 and pitch > 0.55:
		return 2.3
	return open_d


func _blocked(a: Vector3, b: Vector3, _skip: Vector3, _radius: float) -> float:
	var open_d := a.distance_to(b)
	if _block_center == Vector3.INF or open_d < 1.0e-6:
		return open_d
	var d := (b - a).normalized()
	var oc := _block_center - a
	var t := oc.dot(d)
	if t < 0.0 or t > open_d:
		return open_d
	var closest := a + d * t
	var perp := closest.distance_to(_block_center)
	if perp >= _block_radius:
		return open_d
	return maxf(0.0, t - sqrt(_block_radius * _block_radius - perp * perp))


func _fail(where: String, detail: String) -> void:
	_fails += 1
	print("RIP_SHOT FAIL %s: %s" % [where, detail])
