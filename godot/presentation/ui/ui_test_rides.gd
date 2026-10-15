extends "res://presentation/ui/ui_test_bands.gd"

# UI scenarios: the slingshot, the swing, C6, C7, the machines, the high ride,
# the clouds, the derrick, the incline, the stone wheel and the tram.
# (Part of the ui_test_driver.gd chain.)

const TRAVERSAL_HANGING := 1
const TRAVERSAL_SWINGING := 6
# The world's solids, the rings among them: what C6's climber stands on at the
# 264 ring.
const RING_264_ENTITY := 51
# C7 (src/sim/climb_c7.hpp) and the walkway it tops out on.
const MAST_PITMAN_WALKWAY_ENTITY := 1968
const C7_KENTLEDGE_TOP := 265.85
const C7_KENTLEDGE_NORTH := -139.0
const C7_LANE_ONE_X := -19.2
const C7_PLATFORM_ONE_TOP := 269.05
const C7_OUTRIGGER_TOP := 272.3
const C7_LANE_TWO_X := -29.3
const C7_GALLERY_TWO_FIRST_Z := -149.3
const C7_PLATFORM_TWO_TOP := 277.1
const C7_PLATFORM_THREE_TOP := 280.35
const C7_PLATFORM_FOUR_TOP := 283.6
const C7_RING_286_EDGE := -20.55
const C7_RING_286_TOP := 286.25
const C7_BLOCK_TOP := 287.85
const C7_WALKWAY_TOP := 291.25

# The ground slingshot, played as its native test plays it: round the
# pouch's east block to its low front lip and step in; ENTER POUCH; the stick
# pulled back until the pouch is drawn 10.2 m; RELEASE; over the tower and
# coming down, the chute; the stick steering onto the 220 ring, where the
# route from grade arrives.
func _slingshot(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	if not (await _go(device, Vector2(6.0, -40.0), 0.3, 25.0) and \
			await _go(device, Vector2(6.0, -57.6), 0.2, 25.0) and \
			await _go(device, Vector2(-3.0, -57.6), 0.15, 12.0) and \
			await _go(device, Vector2(-3.0, -55.0), 0.1, 6.0)):
		return _fail("the walk into the slingshot's pouch stalled at %s" % str(_position()))
	if not await _offered(&"slingshot", "ENTER POUCH"):
		return _fail("in the pouch, Action read '%s %s', not ENTER POUCH" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	await _pose("sling_pouch")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_sling()["seated"]), 1.0):
		return _fail("ENTER POUCH did not harness the rider")
	if not await _offered(&"slingshot", "PULL BACK"):
		return _fail("seated, Action read '%s', not PULL BACK" % _action_label())
	# Draw: the stick held back until the pouch is 10.2 m behind its rest.
	_move_dir(device, Vector2(0.0, -1.0))
	var drawn: bool = await _wait_until(func() -> bool: return float(_sling()["draw_m"]) >= 10.2, 8.0)
	_move_dir(device, Vector2.ZERO)
	if not drawn:
		return _fail("the stick held back drew the pouch only %.2f m" % float(_sling()["draw_m"]))
	await _pose("sling_drawn")
	var draw_m := float(_sling()["draw_m"])
	var energy_kj := float(_sling()["energy_j"]) / 1000.0
	if not await _offered(&"slingshot", "RELEASE"):
		return _fail("drawn %.2f m, Action read '%s', not RELEASE" % [draw_m, _action_label()])
	_act(device)
	# The push is never slowed: time runs at full rate until the pouch lets the
	# rider go.
	var push := {"slowed": false}
	if not await _wait_until(func() -> bool:
		push["slowed"] = bool(push["slowed"]) or (_main._launch_shot.time_scale() < 1.0 and not bool(_sling()["flight"]))
		return bool(_sling()["flight"]), 3.0):
		return _fail("RELEASE did not throw the rider off the bands")
	if bool(push["slowed"]):
		return _fail("time slowed while the bands were still pushing")
	var off_bands_mps := _velocity().length()
	# Off the bands: time slows, the eyes go back to the bands, then all is
	# normal again within the shot's 1.2 s.
	var shot_tick := int(_native().get_tick_index())
	await _seconds(0.3)
	if _main._launch_shot.time_scale() > 0.35 or _main._launch_shot.look_weight() < 0.95:
		return _fail("0.3 s off the bands: time at %.2f, eyes %.2f of the way back" % [
			_main._launch_shot.time_scale(), _main._launch_shot.look_weight()])
	var to_bands: Vector3 = (_main._launch_shot._look_back - _main._camera.global_position).normalized()
	if (-_main._camera.global_transform.basis.z).dot(to_bands) < 0.97:
		return _fail("0.3 s off the bands, the view is not on the bands")
	await _pose("launch_back")
	await _seconds(0.55)
	await _pose("launch_forward")
	await _seconds(0.45)
	var shot_ticks := int(_native().get_tick_index()) - shot_tick
	if _main._launch_shot.active() or _main._launch_shot.time_scale() != 1.0:
		return _fail("1.3 s off the bands, the launch shot still runs")
	# 1.3 s of the wall clock at full rate is 117 ticks; slowed, far fewer.
	if shot_ticks > 85:
		return _fail("the launch shot's 1.3 s ran %d native ticks: time did not slow" % shot_ticks)
	# Up the tower's face; coming down over it, the chute.
	# A lambda cannot write a local: the apex is kept in a dictionary.
	var flight := {"apex": 0.0}
	var over: bool = await _wait_until(func() -> bool:
		flight["apex"] = maxf(float(flight["apex"]), _position().y)
		return _velocity().y < 0.0 and _position().z < -118.0 and _position().y > 226.0, 20.0)
	var apex := float(flight["apex"])
	if not over:
		return _fail("the throw never came down over the tower (apex %.1f m, at %s)" % [apex, str(_position())])
	await _pose("sling_flight")
	_tap(0, _center(&"chute"))
	if not await _wait_until(func() -> bool: return bool(_native().is_parachute_deployed()), 0.5):
		return _fail("CHUTE did not open the canopy over the tower")
	# Steer onto the ring, the stick pushed toward it relative to the view.
	var target := Vector2(-3.0, -128.7)
	var waited := 0.0
	var canopy_seen := false
	var posed := false
	while waited < 20.0 and not bool(_native().is_player_grounded()):
		# Over the rider, a canopy on its lines, filled within a second;
		# looked up at (capture only) without letting go of the steering.
		if not canopy_seen and waited >= 0.9:
			if _main._canopy == null or not _main._canopy.visible or _main._canopy.open_fraction() < 1.0:
				return _fail("the chute is out but no open canopy is drawn over the rider")
			canopy_seen = true
		if canopy_seen and not posed and not _capture_prefix.is_empty():
			var pitch_before: float = _main._pitch
			_main._pitch = 1.05
			await _pose("chute_up")
			_main._pitch = pitch_before
			posed = true
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		var v := Vector2(to.dot(right), to.dot(forward))
		_move_dir(device, v.normalized() * clampf(v.length(), 0.0, 1.0) if v.length() > 0.05 else Vector2.ZERO)
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	if not canopy_seen:
		return _fail("the canopy was never seen open over the rider")
	await _seconds(0.5)
	await _pose("sling_landed")
	var at_end := _position()
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died on the way (apex %.1f m)" % apex)
	if not bool(_native().is_player_grounded()) or at_end.y < 221.0 or at_end.y > 221.3 or \
			at_end.z < -130.73 or at_end.z > -126.73:
		return _fail("the rider came down at %s, not on the 220 ring (apex %.1f m)" % [str(at_end), apex])
	_detail = "ring220_y=%.2f draw_m=%.2f energy_kJ=%.0f apex=%.1f seconds=%.1f off_bands_mps=%.1f shot_ticks=%d" % [
		at_end.y, draw_m, energy_kj, apex, float(int(_native().get_tick_index()) - started) / 90.0, off_bands_mps,
		shot_ticks]
	return true

func _swing_state() -> Dictionary:
	return _native().get_swing_state()

# AS-012, the swing, played as its native test plays it: west along the 220
# ring to the gangway, out to its end and west into the seat; STRAP IN; facing
# the tower, KICK THE TRIP; the ram comes down and strikes, the seat goes up
# its arc and the rack holds it at the 242 ring's edge; UNBUCKLE and step off
# north onto the ring.
func _swing(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	if not (await _go(device, Vector2(-13.35, -127.5), 0.2, 20.0) and \
			await _go(device, Vector2(-13.35, -98.8), 0.15, 25.0) and \
			await _go(device, Vector2(-15.0, -98.85), 0.1, 6.0)):
		return _fail("the walk out the gangway into the swing's seat stalled at %s" % str(_position()))
	if not await _offered(&"swing", "STRAP IN"):
		return _fail("in the seat, Action read '%s %s', not STRAP IN" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_swing_state()["seated"]), 1.0):
		return _fail("STRAP IN did not harness the rider")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"swing", "KICK THE TRIP"):
		return _fail("strapped in, Action read '%s', not KICK THE TRIP" % _action_label())
	await _pose("swing_seat")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_swing_state()["tripped"]), 1.0):
		return _fail("KICK THE TRIP did not let the ram go (kick bar %.3f m)" % float(_swing_state()["kick_travel_m"]))
	if not await _wait_until(func() -> bool: return float(_swing_state()["ram_angle_rad"]) > -0.6, 5.0):
		return _fail("the ram never came down (at %.2f rad)" % float(_swing_state()["ram_angle_rad"]))
	await _pose("swing_ram")
	if not await _wait_until(func() -> bool: return float(_swing_state()["peak_buffer_force_n"]) > 0.0, 3.0):
		return _fail("the ram never struck the seat's buffer")
	if not await _wait_until(func() -> bool:
			var state := _swing_state()
			return bool(state["held_at_top"]) and float(state["seat_speed_mps"]) < 0.3 and \
				float(state["seat_angle_rad"]) > 1.2, 10.0):
		return _fail("the seat never came to rest on the rack (at %.3f rad, floor %.2f m, tooth %d)" % [
			float(_swing_state()["seat_angle_rad"]), float(_swing_state()["seat_floor_y"]), int(_swing_state()["tooth"])])
	await _seconds(0.5)
	await _pose("swing_top")
	var state := _swing_state()
	if not await _offered(&"swing", "UNBUCKLE"):
		return _fail("held at the top, Action read '%s', not UNBUCKLE" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return not bool(_swing_state()["seated"]), 1.0):
		return _fail("UNBUCKLE did not let the rider out of the harness")
	if not await _go(device, Vector2(-15.0, -129.3), 0.15, 6.0):
		return _fail("the step off the seat onto the 242 ring stalled at %s (on %d)" % [str(_position()),
			int(_native().get_support_entity_id())])
	await _seconds(0.5)
	await _pose("swing_242")
	var at_end := _position()
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died on the swing")
	if not bool(_native().is_player_grounded()) or at_end.y < 242.9 or at_end.y > 243.4 or \
			at_end.z < -131.64 or at_end.z > -127.64:
		return _fail("the rider stands at %s, not on the 242 ring" % str(at_end))
	_detail = "ring242_y=%.2f apex_floor=%.2f held_floor=%.2f peak_g=%.1f seconds=%.1f" % [at_end.y,
		float(state["apex_floor_y"]), float(state["seat_floor_y"]), float(state["peak_seat_accel_mps2"]) / 9.81,
		float(int(_native().get_tick_index()) - started) / 90.0]
	return true

# C6, the west band, the way its native climb takes it: from the 242 ring onto
# the kentledge, a run along it and a leap from its striped end over the gap,
# caught by the hands at the platform's lip, west along the girder over the
# void on the balance, a hang up onto the outrigger's end and back east along
# it, north up the rising girder on the balance, a hang up onto the
# crossbeam's end and east along it into the tower, up onto the block, and
# from it onto the 264 ring.
func _c6(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not (await _go(device, Vector2(-17.0, -130.8), 0.15, 8.0) and \
			await _go(device, Vector2(-20.9, -133.0), 0.15, 8.0) and \
			await _go(device, Vector2(-20.9, -141.2), 0.06, 6.0)):
		return _fail("C6: the walk to the kentledge stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), 244.3, "the kentledge"):
		return false
	# A run along its top and the leap from its striped end, pushing on until
	# the hands have the platform's lip.
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.2)
	_move(device, 1.0)
	if not await _wait_until(func() -> bool: return _position().z <= -144.4, 2.0):
		_move(device, 0.0)
		return _fail("C6: the run along the kentledge stalled at %s" % str(_position()))
	await _pose("c6_takeoff")
	_jump(device)
	var caught: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 2.0)
	_move(device, 0.0)
	if not caught:
		return _fail("C6: the leap off the kentledge never caught the platform's lip (at %s)" % str(_position()))
	await _pose("c6_caught")
	if not await _climb_up(device):
		return _fail("C6: CLIMB UP not offered hanging from the platform's lip")
	if not await _wait_until(func() -> bool: return _standing_above(247.8), 2.5):
		return _fail("C6: the climb up onto the platform did not land (at %s)" % str(_position()))
	await _seconds(0.4)
	# West along the girder over the void, on the balance, to its landing.
	if not (await _go(device, Vector2(-21.6, -151.6), 0.08, 4.0) and \
			await _go(device, Vector2(-23.5, -151.6), 0.1, 4.0)):
		return _fail("C6: the step onto the girder out stalled at %s" % str(_position()))
	if not bool(_ctx()["balancing"]):
		return _fail("C6: not balancing on the girder out at %s" % str(_position()))
	await _pose("c6_girder_out")
	if not (await _go(device, Vector2(-29.6, -151.6), 0.1, 8.0) and \
			await _go(device, Vector2(-30.1, -151.6), 0.08, 3.0)):
		return _fail("C6: the walk out along the girder stalled at %s" % str(_position()))
	# Up onto the outrigger's end, and back east along it.
	await _go(device, Vector2(-29.3, -151.6), 0.05, 2.0)
	if not await _c4_hang_up(device, Vector2(1.0, 0.0), 251.1, "the outrigger"):
		return false
	if not await _go(device, Vector2(-21.2, -151.6), 0.1, 8.0):
		return _fail("C6: the walk east along the outrigger stalled at %s" % str(_position()))
	# North up the rising girder, on the balance, to the platform at its head.
	if not await _go(device, Vector2(-21.2, -153.5), 0.1, 4.0):
		return _fail("C6: the step onto the rising girder stalled at %s" % str(_position()))
	if not bool(_ctx()["balancing"]):
		return _fail("C6: not balancing on the rising girder at %s" % str(_position()))
	await _pose("c6_girder_up")
	if not await _go(device, Vector2(-21.2, -162.6), 0.1, 10.0):
		return _fail("C6: the walk up the rising girder stalled at %s" % str(_position()))
	# Up onto the crossbeam's end, and east along it into the tower.
	if not await _go(device, Vector2(-19.75, -163.0), 0.06, 4.0):
		return _fail("C6: the step to the crossbeam's end stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(1.0, 0.0), 258.4, "the crossbeam"):
		return false
	if not (await _go(device, Vector2(-16.4, -163.0), 0.1, 6.0) and \
			await _go(device, Vector2(-16.4, -162.8), 0.05, 2.0)):
		return _fail("C6: the walk along the crossbeam stalled at %s" % str(_position()))
	# Up onto the block, and from its west edge onto the 264 ring.
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), 261.4, "the block"):
		return false
	if not await _go(device, Vector2(-16.9, -164.5), 0.05, 3.0):
		return _fail("C6: the walk to the block's west edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(-1.0, 0.0), 264.7, "the 264 ring"):
		return false
	if not await _go(device, Vector2(-19.5, -164.5), 0.1, 3.0):
		return _fail("C6: the walk onto the 264 ring stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("c6_264")
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	if int(_native().get_support_entity_id()) != RING_264_ENTITY or not _standing_above(264.9):
		return _fail("C6: not standing on the 264 ring (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	_detail = "ring264_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true

# C7, the way its native climb takes it: from the 264 ring's west band onto
# the kentledge, a run and a leap from its striped end onto gallery one,
# swung pipe to pipe to platform one's lip; up onto the outrigger, out west,
# a standing leap to gallery two and up it to platform two; hang-ups onto
# platforms three and four and the 286 ring; along the band to the block, up
# its step, and a last hang-up onto the walkway. The mast never moves.
func _c7(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not (await _go(device, Vector2(-19.2, -155.0), 0.15, 8.0) and \
			await _go(device, Vector2(-17.6, -150.0), 0.15, 4.0) and \
			await _go(device, Vector2(-17.6, -133.4), 0.15, 12.0) and \
			await _go(device, Vector2(-19.2, -133.6), 0.06, 4.0)):
		return _fail("C7: the walk to the kentledge stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), C7_KENTLEDGE_TOP + 0.45, "the kentledge"):
		return false
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.2)
	_move(device, 1.0)
	if not await _wait_until(func() -> bool: return _position().z <= C7_KENTLEDGE_NORTH + 0.5, 2.0):
		_move(device, 0.0)
		return _fail("C7: the run along the kentledge stalled at %s" % str(_position()))
	await _pose("c7_takeoff")
	_jump(device)
	if not await _c7_gallery(device, "gallery one"):
		return false
	if not await _climb_up(device):
		return _fail("C7: CLIMB UP not offered on platform one's lip")
	if not await _wait_until(func() -> bool: return _standing_above(C7_PLATFORM_ONE_TOP + 0.4), 3.0):
		return _fail("C7: the climb up onto platform one did not land (at %s)" % str(_position()))
	await _seconds(0.3)
	if not await _go(device, Vector2(C7_LANE_ONE_X, -147.4), 0.06, 4.0):
		return _fail("C7: the walk to platform one's north edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), C7_OUTRIGGER_TOP + 0.4, "the outrigger"):
		return false
	if not (await _go(device, Vector2(-24.0, -149.1), 0.15, 6.0) and \
			await _go(device, Vector2(C7_LANE_TWO_X, C7_GALLERY_TWO_FIRST_Z), 0.06, 6.0)):
		return _fail("C7: the walk out along the outrigger stalled at %s" % str(_position()))
	await _pose("c7_outrigger")
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	_jump(device)
	if not await _c7_gallery(device, "gallery two"):
		return false
	if not await _climb_up(device):
		return _fail("C7: CLIMB UP not offered on platform two's lip")
	if not await _wait_until(func() -> bool: return _standing_above(C7_PLATFORM_TWO_TOP + 0.4), 3.0):
		return _fail("C7: the climb up onto platform two did not land (at %s)" % str(_position()))
	await _seconds(0.3)
	if not await _go(device, Vector2(-28.4, -158.4), 0.06, 4.0):
		return _fail("C7: the walk to platform two's east edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(1.0, 0.0), C7_PLATFORM_THREE_TOP + 0.4, "platform three"):
		return false
	if not await _go(device, Vector2(-23.5, -159.7), 0.06, 6.0):
		return _fail("C7: the walk to platform three's north edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), C7_PLATFORM_FOUR_TOP + 0.4, "platform four"):
		return false
	if not await _go(device, Vector2(C7_RING_286_EDGE - 0.6, -161.9), 0.06, 6.0):
		return _fail("C7: the walk to platform four's east edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(1.0, 0.0), C7_RING_286_TOP + 0.4, "the 286 ring"):
		return false
	await _pose("c7_286")
	if not (await _go(device, Vector2(-17.5, -161.9), 0.15, 4.0) and \
			await _go(device, Vector2(-17.5, -143.6), 0.15, 12.0) and \
			await _go(device, Vector2(-19.0, -143.6), 0.06, 4.0)):
		return _fail("C7: the walk along the 286 ring to the block stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), C7_BLOCK_TOP + 0.45, "the block"):
		return false
	if not await _go(device, Vector2(-19.0, -147.4), 0.06, 4.0):
		return _fail("C7: the walk up the block's step stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), C7_WALKWAY_TOP + 0.4, "the walkway"):
		return false
	await _seconds(0.5)
	await _pose("c7_walkway")
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	if int(_native().get_support_entity_id()) != MAST_PITMAN_WALKWAY_ENTITY or not _standing_above(291.9):
		return _fail("C7: not standing on the walkway (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	if absf(float(_native().get_lift_state()["travels"][0])) >= 0.005:
		return _fail("C7: the cascade mast moved (travel %.3f)" % float(_native().get_lift_state()["travels"][0]))
	_detail = "walkway_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true

# From a leap at a gallery's first pipe: forward held, the hands swing pipe to
# pipe until they take the platform's lip.
func _c7_gallery(device: int, what: String) -> bool:
	_move(device, 1.0)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_SWINGING, 1.0):
		_move(device, 0.0)
		return _fail("C7: the leap never caught %s's first pipe (at %s)" % [what, str(_position())])
	await _pose("c7_" + what.replace(" ", "_"))
	var lip: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 14.0)
	_move(device, 0.0)
	if not lip:
		return _fail("C7: swung along %s, the hands never took the platform's lip (at %s)" % [what,
			str(_position())])
	return true

# One of the owner's vertical machines: from its lower receiver at `entry`
# onto its deck at `deck`, RAISE, up to its upper receiver, and off onto it at
# `exit` (the entry's spot, unless given).
func _ride_machine(device: int, index: int, name: String, entry: Vector2, deck: Vector2,
		exit := Vector2.INF) -> bool:
	if exit == Vector2.INF:
		exit = entry
	if not (await _go(device, entry, 0.15, 8.0) and await _go(device, deck, 0.15, 6.0)):
		return _fail("%s: the walk onto the deck stalled at %s" % [name, str(_position())])
	# The pitman lift is hauled by hand: ACTION held, not RAISE.
	var hauled: bool = int(_native().get_lift_state()["pitman"]) == index
	if hauled:
		if not await _offered(&"haul", "HAUL"):
			return _fail("%s: HAUL not offered on the deck (offered %s)" % [name, _action_label()])
		_hold_act(device, true)
		await _seconds(5.0)
		if int(_main._arms.hand_poses()[0]) != _main.FirstPersonArms.POSE_LINE:
			return _fail("%s: hauling, the right hand is not on the line (pose %d)" % [name,
				int(_main._arms.hand_poses()[0])])
		await _pose("machine_%d_haul" % index)
		if _felt(&"strain") < 1:
			return _fail("%s: hauling, the hands felt no strain (haptics %s)" % [name, str(_main.haptic_counts)])
	else:
		if not await _offered(&"lift", "RAISE", name):
			return _fail("%s: RAISE not offered on the deck (offered %s)" % [name, _action_label()])
		_act(device)
	var arrived: bool = await _wait_until(func() -> bool:
			var lift: Dictionary = _native().get_lift_state()
			return absf(float(lift["travels"][index]) - 1.0) < 0.01 and (hauled or not bool(lift["moving"])) and \
				bool(_ctx()["grounded"]), 90.0)
	if hauled:
		_hold_act(device, false)
		await _seconds(0.3)
	if not arrived:
		return _fail("%s: the deck never reached its upper receiver (at %s, lift %s, grounded %s)" % [name,
			str(_position()), str(_native().get_lift_state()), str(_ctx()["grounded"])])
	await _pose("machine_%d_top" % index)
	if not await _go(device, exit, 0.15, 8.0):
		return _fail("%s: the step off onto the upper receiver stalled at %s" % [name, str(_position())])
	await _seconds(0.5)
	if int(_native().get_lift_state()["role"]) != 3:
		return _fail("%s: not standing on the upper receiver (y %.2f)" % [name, _position().y])
	return true

func _machines(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not await _go(device, Vector2(-19.5, -150.0), 0.15, 12.0):
		return _fail("machines: the walk along the 264 ring stalled at %s" % str(_position()))
	if not await _ride_machine(device, 0, "CASCADE MAST", Vector2(-16.0, -150.0), Vector2(-10.65, -150.0)):
		return false
	var mast_y := _position().y
	if not await _go(device, Vector2(-19.5, -149.25), 0.15, 6.0):
		return _fail("machines: the walkway out the west face stalled at %s" % str(_position()))
	if not await _ride_machine(device, 1, "PITMAN LIFT", Vector2(-23.5, -149.0), Vector2(-23.5, -154.3)):
		return false
	var pitman_y := _position().y
	if not await _ride_machine(device, 2, "BARREL HELIX", Vector2(-27.0, -149.0), Vector2(-30.8, -149.0)):
		return false
	var helix_y := _position().y
	if not await _go(device, Vector2(-15.2, -149.0), 0.1, 10.0):
		return _fail("machines: the walkway east to TP-340 stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(1.0, 0.0), 340.6, "TP-340"):
		return false
	await _go(device, Vector2(-12.0, -149.0), 0.15, 4.0)
	await _seconds(1.0)
	await _pose("machines_tp340")
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	if not _standing_above(341.0):
		return _fail("machines: not standing on TP-340 (y %.2f)" % _position().y)
	_detail = "mast_y=%.2f pitman_y=%.2f helix_y=%.2f tp340_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		mast_y, pitman_y, helix_y, _position().y, float(int(_native().get_tick_index()) - started) / 90.0,
		worst_step, float(body["lift"])]
	return true

func _high(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not await _high_legs(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	if not _standing_above(803.3):
		return _fail("high: not standing on the derrick's upper receiver (y %.2f)" % _position().y)
	if not bool(_native().get_landing_state()["summit_reached"]) or not bool(_main._summit_announced):
		return _fail("high: the top of the route was not marked (native %s, announced %s)" % [
			str(_native().get_landing_state()["summit_reached"]), str(_main._summit_announced)])
	await _pose("summit")
	_detail = "crown_y=%.2f top_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_high_crown_y, _position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step,
		float(body["lift"])]
	return true

var _high_crown_y := 0.0

# The derrick's cradle and its six iron pigs (entities after its boom).
const DERRICK_CRADLE_ENTITY := 2957
const DERRICK_IRON_FIRST := 2958

func _kit_origin(entity: int) -> Vector3:
	return _native().get_kit_body_transform(_native().get_kit_body_index(entity)).origin

# The luffing derrick as a player works it: onto the cradle (TOO HEAVY with
# its six iron pigs), four pigs picked up and let go over its open west side,
# and with two left the counterweight luffs it up; at the top, off onto the
# upper receiver.
# The renderer's load at the extremes of the tower: from the 750 deck and
# from grade, the view turned through five pitches and two headings. Looking
# straight down from the top or up from the ground once drew ~9,700 objects a
# frame (every dressing part its own draw); the world is now drawn in batches
# (main.gd, _batch_static_dressing). Fails past PERF_DRAW_BUDGET at the
# shadow quality a phone on LOW runs, so the regression cannot come back
# unseen; the HIGH figures are reported alongside.
const PERF_DRAW_BUDGET := 700

func _clouds() -> bool:
	await _seconds(1.0)
	var clouds: Node3D = _main._clouds
	if clouds == null or int(clouds.puff_count()) < 50:
		return _fail("no cloud layer")
	# Round the climb at its own height: a puff within 200 m of the tower's
	# axis at 480 m, so the climber passes them.
	var near := float(clouds.nearest(Vector3(0.0, 480.0, -150.0)))
	if near > 200.0:
		return _fail("the nearest cloud to the climb at 480 m is %.0f m off" % near)
	for i in 4:
		_main._yaw = float(i) * PI * 0.5
		_main._pitch = 0.75
		await _seconds(0.4)
		await _pose("clouds_%d" % i)
	_detail = "puffs=%d nearest_to_climb_m=%.0f" % [int(clouds.puff_count()), near]
	return true

func _perf_probe() -> bool:
	await _seconds(2.0)
	var worst := {}
	for level in [3, 1]:
		_main._sky_cycle.set_shadow_level(level)
		var top := 0
		for pitch in [-1.25, -0.6, 0.0, 0.6, 1.3]:
			_main._pitch = pitch
			for yaw in [0.0, PI]:
				_main._yaw = yaw
				await _seconds(0.5)
				if level == 3 and yaw == 0.0 and pitch in [-1.25, 0.0, 0.6]:
					await _pose("perf_p%d" % int(round(pitch * 100.0)))
				top = maxi(top, int(Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)))
		worst[level] = top
	_main._sky_cycle.set_shadow_level(_main._settings.shadow_quality)
	_detail = "worst_draws_high=%d worst_draws_low=%d %s" % [worst[3], worst[1], _main._batch_report]
	if worst[1] > PERF_DRAW_BUDGET:
		return _fail("the renderer drew %d objects in a frame at LOW shadows (budget %d)" % [worst[1], PERF_DRAW_BUDGET])
	return true

func _ride_derrick(device: int) -> bool:
	if not (await _go(device, Vector2(13.5, -156.5), 0.15, 6.0) and \
			await _go(device, Vector2(12.3, -157.5), 0.15, 4.0) and \
			await _go(device, Vector2(12.3, -161.0), 0.15, 4.0)):
		return _fail("derrick: the walk onto the cradle stalled at %s" % str(_position()))
	if not await _offered(&"derrick", "TOO HEAVY", "IRON ABOARD: 6"):
		return _fail("derrick: the cradle did not read TOO HEAVY with six pigs (read '%s %s')" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	for pig in [DERRICK_IRON_FIRST + 2, DERRICK_IRON_FIRST + 5, DERRICK_IRON_FIRST + 1, DERRICK_IRON_FIRST + 4]:
		var at := _kit_origin(pig)
		if not await _go(device, Vector2(at.x, at.z + 0.6), 0.1, 4.0):
			return _fail("derrick: the walk to iron %d stalled at %s" % [pig, str(_position())])
		await _face(Vector2(0.0, -1.0))
		if not await _offered(&"pick_up", "PICK UP", "IRON PIG"):
			return _fail("derrick: Action read '%s %s' facing iron %d, not PICK UP IRON PIG" % [_action_label(),
				String(_ctx()["action"]["detail"]), pig])
		_act(device)
		if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == pig, 0.5):
			return _fail("derrick: PICK UP did not lift iron %d" % pig)
		var cradle := _kit_origin(DERRICK_CRADLE_ENTITY)
		if not await _go(device, Vector2(cradle.x - 1.6, cradle.z + 1.0), 0.1, 4.0):
			return _fail("derrick: carrying iron %d to the edge stalled at %s" % [pig, str(_position())])
		await _face(Vector2(-1.0, 0.0))
		await _seconds(0.6)
		if not await _let_go_if_held(device):
			return _fail("derrick: LET GO did not drop iron %d over the side" % pig)
		await _seconds(1.0)
	await _wait_until(func() -> bool: return int(_native().get_lift_state()["derrick_iron_aboard"]) == 2, 3.0)
	_derrick_iron_left = int(_native().get_lift_state()["derrick_iron_aboard"])
	if _derrick_iron_left != 2:
		return _fail("derrick: %d pigs left aboard, not 2" % _derrick_iron_left)
	var waited := 0.0
	while waited < 90.0:
		var lift: Dictionary = _native().get_lift_state()
		if absf(float(lift["travels"][4]) - 1.0) < 0.01 and not bool(lift["moving"]) and bool(_ctx()["grounded"]):
			break
		var cradle := _kit_origin(DERRICK_CRADLE_ENTITY)
		_steer_on_deck(device, Vector2(cradle.x - 1.2, cradle.z))
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	if absf(float(_native().get_lift_state()["travels"][4]) - 1.0) >= 0.01:
		return _fail("derrick: the lightened cradle never reached the top (lift %s)" % str(_native().get_lift_state()))
	await _pose("machine_4_top")
	if not await _go(device, Vector2(13.5, -184.3), 0.15, 8.0):
		return _fail("derrick: the step off onto the upper receiver stalled at %s" % str(_position()))
	await _seconds(0.5)
	if int(_native().get_lift_state()["role"]) != 3:
		return _fail("derrick: not standing on the upper receiver (y %.2f)" % _position().y)
	return true

var _derrick_iron_left := 0

# From the 750 deck: the vault over its east parapet, the crown gondola and the
# luffing derrick to its upper receiver (802.64 m). Played alone by touch_high
# from a 750 deck spawn, and by touch_stack on from the gravel wheel's arrival.
func _high_legs(device: int) -> bool:
	if not (await _go(device, Vector2(10.8, -152.0), 0.15, 10.0) and \
			await _go(device, Vector2(11.3, -152.0), 0.06, 3.0)):
		return _fail("high: the walk to the east parapet stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("high: Action read '%s' facing the parapet, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 3, 0.3):
		return _fail("high: Action did not vault the parapet (state %d)" % int(_native().get_traversal_state()))
	if not await _wait_until(func() -> bool: return _standing_above(750.6) and _position().x > 12.4, 2.0):
		return _fail("high: the vault over the parapet did not land (at %s)" % str(_position()))
	await _pose("high_vault")
	if not await _ride_machine(device, 3, "CROWN GONDOLA", Vector2(13.5, -152.0), Vector2(17.3, -152.0),
			Vector2(13.5, -151.5)):
		return false
	_high_crown_y = _position().y
	if not await _ride_derrick(device):
		return false
	await _pose("high_top")
	return true

func _incline(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not (await _go(device, Vector2(6.0, -70.0), 0.2, 20.0) and \
			await _go(device, Vector2(-13.4, -86.0), 0.2, 20.0)):
		return _fail("incline: the walk across the yard stalled at %s" % str(_position()))
	await _pose("incline_foot")
	if not await _ride_incline(device):
		return false
	if not await _go(device, Vector2(-13.4, -126.0), 0.15, 8.0):
		return _fail("incline: the step onto deck 3 stalled at %s" % str(_position()))
	await _seconds(1.0)
	await _pose("incline_deck3")
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	if not _standing_above(33.7):
		return _fail("incline: not standing on deck 3 (y %.2f)" % _position().y)
	_detail = "deck3_y=%.2f peak_mps=%.2f arrival_mps=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, _incline_peak, _incline_arrival, float(int(_native().get_tick_index()) - started) / 90.0,
		worst_step, float(body["lift"])]
	return true
const INCLINE_TRIP_ENTITY := INCLINE_DECK_ENTITY + 1
const INCLINE_BRAKE_ENTITY := INCLINE_DECK_ENTITY + 2
# Off the brake: this far west of the treadle, clear of it on the deck. The
# speed the trolley gains in the time a step back onto it takes.
const INCLINE_OFF_BRAKE_M := 0.8
const INCLINE_STEP_GAIN_MPS := 1.4

func _incline_plate(entity: int) -> Vector3:
	return _native().get_kit_body_transform(_native().get_kit_body_index(entity)).origin

# One frame of the stick toward a point that moves with the trolley.
func _steer_on_deck(device: int, target: Vector2) -> void:
	var at := _position()
	var to := target - Vector2(at.x, at.z)
	var yaw := float(_main._yaw)
	var forward := Vector2(-sin(yaw), -cos(yaw))
	var right := Vector2(cos(yaw), -sin(yaw))
	var v := Vector2.ZERO
	if to.length() > 0.05:
		v = Vector2(to.dot(right), to.dot(forward)).normalized() * clampf(to.length() / 0.3, 0.25, 1.0)
	_move_dir(device, v)

# Up the slab incline as a player works it: onto the trolley, stand on the
# yellow plate (the slab's pin comes out), then ride the brake treadle: on it
# whenever the trolley runs faster than the head allows, off it otherwise.
# At the head, off onto the receiver.
func _ride_incline(device: int) -> bool:
	if not (await _go(device, Vector2(-13.4, -103.0), 0.15, 12.0) and \
			await _go(device, Vector2(-10.0, -103.08), 0.15, 6.0)):
		return _fail("incline: the walk onto the trolley stalled at %s" % str(_position()))
	if not await _offered(&"incline", "STAND ON", "THE YELLOW PLATE"):
		return _fail("incline: the deck did not read STAND ON THE YELLOW PLATE (read '%s')" % _action_label())
	var trip := _incline_plate(INCLINE_TRIP_ENTITY)
	if not await _go(device, Vector2(trip.x, trip.z), 0.15, 4.0) or \
			not await _wait_until(func() -> bool: return int(_native().get_lift_state()["incline_state"]) == 1, 2.0):
		return _fail("incline: standing on the yellow plate did not pull the pin (lift %s)" % str(
			_native().get_lift_state()))
	var peak := 0.0
	var arrival := 0.0
	var braked := false
	var waited := 0.0
	while waited < 40.0:
		var lift: Dictionary = _native().get_lift_state()
		var travel := float(lift["travels"][LIFT_INCLINE_INDEX])
		var speed := float(lift["incline_speed_mps"])
		peak = maxf(peak, speed)
		if travel > 0.995 and speed < 0.05:
			break
		arrival = speed
		braked = braked or float(lift["incline_brake_n"]) > 20000.0
		# What the head allows here (stopping on the brake at 1.2 m/s2), less
		# what the trolley gains while the feet step back onto the treadle;
		# nearly stopped, step off and let it creep.
		var remaining := (1.0 - travel) * INCLINE_LENGTH_M
		var allowed := minf(3.5, sqrt(2.0 * 1.2 * maxf(0.0, remaining - 0.5)) + 0.2)
		var on_brake := speed > 0.3 and speed + INCLINE_STEP_GAIN_MPS > allowed
		var brake := _incline_plate(INCLINE_BRAKE_ENTITY)
		_steer_on_deck(device, Vector2(brake.x - (0.0 if on_brake else INCLINE_OFF_BRAKE_M), brake.z))
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	if float(_native().get_lift_state()["travels"][LIFT_INCLINE_INDEX]) < 0.995:
		return _fail("incline: the trolley never reached the head (lift %s)" % str(_native().get_lift_state()))
	if not braked or peak > 5.0 or arrival > 2.0:
		return _fail("incline: ridden on the brake it must arrive slowly (braked %s, peak %.2f, arrival %.2f m/s)" % [
			str(braked), peak, arrival])
	_incline_peak = peak
	_incline_arrival = arrival
	await _pose("machine_5_top")
	if not await _go(device, Vector2(-13.4, -120.4), 0.15, 8.0):
		return _fail("incline: the step off onto the head's receiver stalled at %s" % str(_position()))
	await _seconds(0.5)
	if int(_native().get_lift_state()["role"]) != 3:
		return _fail("incline: not standing on the head's receiver (y %.2f)" % _position().y)
	return true

const LIFT_INCLINE_INDEX := 5
const INCLINE_LENGTH_M := 34.641
var _incline_peak := 0.0
var _incline_arrival := 0.0

func _wheel(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	# Down the ramp onto the lower receiver. Nothing to press: Action offers
	# nothing here.
	if not (await _go(device, Vector2(14.19, -125.0), 0.15, 10.0) and \
			await _go(device, Vector2(14.19, -113.3), 0.15, 12.0)):
		return _fail("wheel: the walk down to the lower receiver stalled at %s" % str(_position()))
	await _seconds(0.5)
	if _action_label() == "RAISE" or _action_label() == "CALL":
		return _fail("wheel: Action offered %s; the wheel is worked by the plank" % _action_label())
	# Out along the feed plank over the drop: it tips under the player, the rope
	# opens the hopper, stone pours into the top bucket, and the wheel is held.
	if not await _go(device, Vector2(20.2, -113.3), 0.1, 8.0):
		return _fail("wheel: the walk out the feed plank stalled at %s" % str(_position()))
	await _pose("wheel_plank")
	await _seconds(4.0)
	# Look back up at the hopper pouring into the top bucket.
	await _face(Vector2(-1.0, 0.0))
	var level_pitch: float = _main._pitch
	_main._pitch = 1.1
	await _pose("wheel_pour")
	_main._pitch = level_pitch
	# The pour is heard: a rattle of stone from the middle of its fall.
	if int(_main._audio.pours_heard) == 0:
		return _fail("wheel: stone poured but nothing was heard")
	await _seconds(4.0)
	# Back in off the plank, which lets the wheel go, and into the bucket.
	if not (await _go(device, Vector2(14.19, -113.3), 0.1, 8.0) and \
			await _go(device, Vector2(14.19, -110.4), 0.1, 6.0)):
		return _fail("wheel: back in and into the bucket stalled at %s" % str(_position()))
	if not await _wait_until(func() -> bool:
			return _standing_above(46.0) and bool(_ctx()["grounded"]) and \
				Vector2(_native().get_player_linear_velocity().x, _native().get_player_linear_velocity().y).length() < 0.05, 120.0):
		return _fail("wheel: the stone never carried the bucket over the top (at %s)" % str(_position()))
	await _pose("wheel_top")
	if not await _go(device, Vector2(17.81, -113.3), 0.15, 8.0):
		return _fail("wheel: the step off onto the upper receiver stalled at %s" % str(_position()))
	# The reload: the skip's line hangs at the receiver's north-east corner.
	# Taken and pulled back until taut, the skip pours into the hopper; let
	# go, it stops.
	var hopper0 := float(_native().get_lift_state()["wheel_hopper_kg"])
	if not await _go(device, Vector2(19.0, -112.9), 0.08, 4.0):
		return _fail("wheel reload: the walk to the skip's line stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB", "SKIP LINE"):
		return _fail("wheel reload: GRAB SKIP LINE not offered (offered %s)" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2979, 1.0):
		return _fail("wheel reload: the line was not taken")
	_move(device, -0.35)
	await _wait_until(func() -> bool: return _position().z <= -113.2, 2.0)
	_move(device, 0.0)
	await _pose("wheel_reload")
	await _seconds(6.0)
	var poured := float(_native().get_lift_state()["wheel_hopper_kg"]) - hopper0
	if poured < 500.0:
		return _fail("wheel reload: pulled, the skip poured only %.0f kg into the hopper" % poured)
	if not await _offered(&"set_down", "LET GO", "SKIP LINE"):
		return _fail("wheel reload: LET GO not offered on the line (offered %s)" % _action_label())
	_act(device)
	await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 1.0)
	if not await _go(device, Vector2(17.81, -113.3), 0.15, 6.0):
		return _fail("wheel reload: back onto the receiver stalled at %s" % str(_position()))
	if not await _go(device, Vector2(17.81, -125.5), 0.15, 12.0):
		return _fail("wheel: the ramp down onto deck 4 stalled at %s" % str(_position()))
	await _seconds(1.0)
	await _pose("wheel_deck4")
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	if not _standing_above(44.7):
		return _fail("wheel: not standing on deck 4 (y %.2f)" % _position().y)
	_detail = "deck4_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true

func _tram(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not (await _go(device, Vector2(18.5, -128.0), 0.15, 12.0) and \
			await _go(device, Vector2(18.5, -153.0), 0.15, 12.0) and \
			await _go(device, Vector2(25.5, -153.0), 0.15, 6.0) and \
			await _go(device, Vector2(79.5, -153.0), 0.15, 25.0) and \
			await _go(device, Vector2(81.2, -153.0), 0.15, 4.0) and \
			await _go(device, Vector2(81.1, -157.5), 0.15, 6.0)):
		return _fail("tram: the walk out the east bridge stalled at %s (support %d, traversal %d, crouched %s)" % [
			str(_position()), int(_native().get_support_entity_id()), int(_native().get_traversal_state()),
			str(_native().is_player_crouched())])
	await _pose("tram_gangway")
	if not await _ride_tram(device):
		return false
	if not await _go(device, Vector2(23.5, -161.0), 0.15, 6.0):
		return _fail("tram: the walk onto deck 8 stalled at %s" % str(_position()))
	await _seconds(1.5)
	await _pose("tram_deck8")
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	if not _standing_above(88.7):
		return _fail("tram: not standing on deck 8 (y %.2f)" % _position().y)
	_detail = "deck8_y=%.2f head_wait_s=%.1f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, _tram_head_wait, float(int(_native().get_tick_index()) - started) / 90.0, worst_step,
		float(body["lift"])]
	return true

# The traction tram shuttles on its own. From its lower receiver: wait until
# it stands at the foot reading BOARD with time to walk on, walk on, ride it
# up, and step off in its wait at the head (STEP OFF), before it leaves.
func _ride_tram(device: int) -> bool:
	if not await _go(device, Vector2(80.6, -161.0), 0.15, 8.0):
		return _fail("tram: the walk onto its lower receiver stalled at %s" % str(_position()))
	if not await _wait_until(func() -> bool:
			return _ctx()["action"]["id"] == &"tram" and _action_label() == "BOARD" and \
				float(_native().get_lift_state()["tram_departs_s"]) >= 3.0, 120.0):
		return _fail("tram: never offered BOARD with time to walk on (read '%s', lift %s)" % [_action_label(),
			str(_native().get_lift_state())])
	if not await _go(device, Vector2(74.0, -161.0), 0.15, 6.0) or int(_native().get_lift_state()["role"]) != 1:
		return _fail("tram: the walk on board stalled at %s" % str(_position()))
	if not await _wait_until(func() -> bool: return _action_label() == "STEP OFF" and bool(_ctx()["grounded"]), 90.0):
		return _fail("tram: never read STEP OFF at the head (read '%s', lift %s)" % [_action_label(),
			str(_native().get_lift_state())])
	_tram_head_wait = float(_native().get_lift_state()["tram_departs_s"])
	await _pose("machine_7_top")
	if not await _go(device, Vector2(29.9, -161.0), 0.15, 8.0):
		return _fail("tram: the step off onto the upper receiver stalled at %s" % str(_position()))
	await _seconds(0.5)
	if int(_native().get_lift_state()["role"]) != 3:
		return _fail("tram: not standing on the upper receiver (y %.2f)" % _position().y)
	return true

var _tram_head_wait := 0.0

# Turns to a ledge ahead and mantles it on Action.
func _c4_mantle(device: int, facing: Vector2, top: float, what: String) -> bool:
	await _face(facing)
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s %s' facing %s, not CLIMB" % [_action_label(),
			String(_ctx()["action"]["detail"]), what])
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(top), 2.0):
		return _fail("the mantle onto %s did not land (at %s)" % [what, str(_position())])
	return true

# Turns to a lip 3.2 m up ahead, jumps for it pushing in, and climbs up.
func _c4_hang_up(device: int, facing: Vector2, top: float, what: String) -> bool:
	await _face(facing)
	await _seconds(0.3)
	_jump(device)
	_move(device, 0.4)
	var hung: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 2.0)
	_move(device, 0.0)
	if not hung:
		return _fail("the jump for %s's lip never hung (at %s)" % [what, str(_position())])
	if not await _climb_up(device):
		return _fail("CLIMB UP not offered hanging from %s" % what)
	if not await _wait_until(func() -> bool: return _standing_above(top), 2.5):
		return _fail("the climb up onto %s did not land (at %s)" % [what, str(_position())])
	return true
