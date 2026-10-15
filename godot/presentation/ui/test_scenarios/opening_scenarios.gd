extends RefCounted

# Pipe bridge, facade, stair, ballast and braced opening traversal.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const LadderSuite := preload("res://presentation/ui/test_scenarios/ladder_scenarios.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

var h: Harness
var _ladder_suite: LadderSuite


func _init(harness: Harness, ladder: LadderSuite) -> void:
	h = harness
	_ladder_suite = ladder


func _pipe_bridge(device: int) -> bool:
	if h._main._regression_scene:
		return h._fail("continuous route selected retired regression fixtures")
	await h._seconds(0.5)
	# Brake on actual grade before turning to the rack: position-only
	# arrival drifts out of GRAB reach at the rendered 30 FPS cadence.
	for point in [Vector2(13, -75), Vector2(12.7, -78.95)]:
		if not await h._walk_to(device, point, 0.07, 20.0, true):
			return h._fail("rack approach at %s" % h._position())
	await h._face(Vector2(-1, 0))
	await h._pose("rack_ready")
	if not await h._offered(&"pick_up", "GRAB"):
		return h._fail("release grip not offered at %s" % h._position())
	h._act(device)
	await h._seconds(0.15)
	if int(h._native().get_carrying_entity_id()) != 2530:
		return h._fail("rack grip was not picked up")
	h._move(device, 0.35)
	await h._seconds(0.6)
	h._move(device, 0)
	if int(h._native().get_carrying_entity_id()) != 0:
		h._act(device)
	if not await h._wait_until(func() -> bool: return int(h._native().get_pipe_bridge_retained_pipes()) == 20, 12.0):
		return h._fail("rack did not load all twenty pipes")
	await h._pose("loaded_pan")
	for point in [Vector2(12.7, -75), Vector2(-1.4, -75), Vector2(-1.4, -85.6), Vector2(-0.5, -85.6)]:
		if not await h._walk_to(device, point, 0.07, 20.0):
			return h._fail("bridge grip approach at %s" % h._position())
	await h._face(Vector2(0, 1))
	await h._seconds(0.15)
	if not await h._offered(&"pick_up", "GRAB"):
		return h._fail("release grip not offered at %s" % h._position())
	h._act(device)
	await h._seconds(0.15)
	if int(h._native().get_carrying_entity_id()) != 2505:
		return h._fail("bridge grip was not picked up pos=%s target=%d held=%d" % [
			h._position(), h._native().get_carry_target_entity_id(), h._native().get_carrying_entity_id()])
	h._move_dir(device, Vector2(0.35, 0))
	await h._seconds(0.6)
	h._move_dir(device, Vector2.ZERO)
	if int(h._native().get_carrying_entity_id()) != 0:
		h._act(device)
	if not await h._wait_until(func() -> bool: return float(h._native().get_pipe_bridge_crush_front()) > 0.1, 15.0):
		return h._fail("pan never reached the crush receiver")
	await h._seconds(2.0)
	var tip := float(h._native().get_pipe_bridge_tip_height())
	if tip < 7.53 or tip > 8.2:
		return h._fail("bridge height %.4f" % tip)
	await h._face(Vector2(1, -1))
	await h._pose("bridge_raised")
	var tip_z := -90.0 - 20.0 * cos(asin(7.4 / 20.0))
	var crossed_bridge := false
	# Brake on the first receiving slab before turning north. Keyboard
	# momentum otherwise carries the player off its east edge during the turn.
	for point in [Vector2(-0.6, -86), Vector2(1.94, -86), Vector2(1.94, -91.1), Vector2(6, -91.1),
			Vector2(6, tip_z + 1.5), Vector2(9.06, tip_z + 1.5), Vector2(9.06, tip_z - 2), Vector2(9.06, -125.2)]:
		await h._face(point - Vector2(h._position().x, h._position().z))
		if not await h._walk_to(device, point, 0.07, 20.0, point == Vector2(9.06, tip_z + 1.5)):
			return h._fail("crossing at %s" % h._position())
		if int(h._native().get_support_entity_id()) == 2500:
			crossed_bridge = true
	if not crossed_bridge:
		return h._fail("route never used the actual moving deck")
	if int(h._native().get_support_entity_id()) not in [11, 51]:
		return h._fail("arrival is disconnected from the original tower at=%s velocity=%s grounded=%s support=%d" % [
			h._position(), h._velocity(), h._native().is_player_grounded(), h._native().get_support_entity_id()])
	await h._face(Vector2(-0.1, 1))
	await h._pose("tower_arrival")
	if h._position().y < 11.6 or int(h._native().get_death_count()) != 0:
		return h._fail("did not arrive alive at the tower ring")
	if h._main._audio.pipe_motion_frames == 0 or h._main._audio.pipe_crush_cues == 0:
		return h._fail("observed motion and crush did not produce sound cues")
	h._detail = "pipes=20 tip_y=%.4f arrival_y=%.3f native_input=1 deaths=0" % [tip, h._position().y]
	return true


func _duct_hang_can_tuck(state: Dictionary) -> bool:
	return bool(h._ctx()["hanging"]) and not h._native().is_player_grounded() \
		and int(h._native().get_traversal_support_entity_id()) == 2561 \
		and int(state.get("traversal_hand_constraint_count", 0)) == 2 \
		and bool(state.get("left_hand_attached", false)) \
		and bool(state.get("right_hand_attached", false)) \
		and int(state.get("left_hand_generation", 0)) > 0 \
		and int(state.get("right_hand_generation", 0)) > 0 \
		and bool(state.get("hanging_free_arc_active", false)) \
		and bool(state.get("swing_tuck_available", false))


func _touch_duct_transfer_reach() -> bool:
	# Native begin_cargo_transfer measures the actual shoulder against the
	# attached lip, with a 1.05 m limit. Passive hanging can sag beyond it.
	var before: Dictionary = h._native().get_landing_state()
	var lip: Vector3 = h._native().get_traversal_ledge_point()
	var shoulder := h._position() + Vector3(0, 0.45, 0)
	if not _duct_hang_can_tuck(before) or not lip.is_finite() \
			or lip == Vector3.ZERO or not shoulder.is_finite():
		return h._fail("duct transfer lacks actual paired airborne lip attachment")
	if shoulder.distance_to(lip) <= 1.05:
		return true
	if bool(before.get("swing_tuck_requested", true)) \
			or not h._main._touch.is_button_shown(&"crouch") \
			or h._main._touch.button_label(&"crouch") != "TUCK":
		return h._fail("duct transfer reach needs the actual touch TUCK control")
	var span_before := float(before["hanging_actual_span_m"])
	var target_before := float(before["hanging_commanded_span_m"])
	var work_before := float(before["hand_actuator_positive_work_j"])
	var tuck_tick := int(h._native().get_tick_index())
	h._tap(2, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		var actual_lip: Vector3 = h._native().get_traversal_ledge_point()
		var actual_shoulder := h._position() + Vector3(0, 0.45, 0)
		return _duct_hang_can_tuck(state) \
			and int(state["left_hand_generation"]) == int(before["left_hand_generation"]) \
			and int(state["right_hand_generation"]) == int(before["right_hand_generation"]) \
			and bool(state.get("swing_tuck_requested", false)) \
			and float(state["hanging_commanded_span_m"]) < target_before - 0.03 \
			and float(state["hanging_actual_span_m"]) < span_before - 0.03 \
			and actual_lip.is_finite() and actual_lip != Vector3.ZERO \
			and actual_shoulder.is_finite() and actual_shoulder.distance_to(actual_lip) <= 1.05, 2.0):
		return h._fail("touch TUCK did not earn actual duct transfer reach at=%s lip=%s" % [
			h._position(), h._native().get_traversal_ledge_point()])
	var tucked: Dictionary = h._native().get_landing_state()
	var seconds := float(int(h._native().get_tick_index()) - tuck_tick) * float(h._native().get_fixed_step_seconds())
	var paid_work := float(tucked["hand_actuator_positive_work_j"]) - work_before
	var left_force: Vector3 = tucked["left_hand_force_n"]
	var right_force: Vector3 = tucked["right_hand_force_n"]
	if seconds <= 0.0 or not is_finite(paid_work) or paid_work <= 0.0 \
			or paid_work > 500.0 * seconds + 0.01 \
			or not left_force.is_finite() or not right_force.is_finite() \
			or left_force.length() > 1501.0 or right_force.length() > 1501.0 \
			or float(tucked["player_gravity_factor"]) != 1.0 or h._native().is_player_crouched():
		return h._fail("duct TUCK reach lacks finite paid native body response")
	# Do not Extend before Jump: keep the earned body reach.
	# Existing Jump handling clears the posture intent.
	print("SCRAPERX_DUCT_TRANSFER touch_tuck=1 actual_span_change_m=%.6f paid_work_j=%.6f shoulder_to_lip_m=%.6f" % [
		span_before - float(tucked["hanging_actual_span_m"]), paid_work,
		(h._position() + Vector3(0, 0.45, 0)).distance_to(h._native().get_traversal_ledge_point())])
	return true


# AS-017: grade to +33 m, through the real touch controls and native world.
func _touch_facade() -> bool:
	var device := InputRouter.Device.TOUCH
	if h._scenario in ["touch_campaign_to_121", "touch_campaign_to_143", "touch_campaign_to_165", "touch_campaign_to_198", "touch_campaign_to_231", "touch_campaign_to_253", "touch_campaign_to_286", "touch_campaign_to_308", "touch_service_lift", "touch_causal_facade"]:
		if not await _ladder_suite._touch_cargo_net():
			return false
		print("SCRAPERX_CAMPAIGN cargo_net_exit at=%s support=%d" % [h._position(), h._native().get_support_entity_id()])
	elif not await _pipe_bridge(device):
		return false
	# Brake on actual footing before turning around the receiver's side rail.
	# A position-only Tower arrival carries native momentum past that rail.
	for point in [Vector2(21.2, -124.4), Vector2(21.2, -121.3), Vector2(20, -121.3)]:
		if not await h._walk_to(device, point, 0.08, 20.0, true):
			return h._fail("facade approach target=%s at=%s velocity=%s grounded=%s support=%d traversal=%d" % [
				point, h._position(), h._velocity(), h._native().is_player_grounded(),
				h._native().get_support_entity_id(), h._native().get_traversal_state()])
	await h._face(Vector2(0, -1))
	await h._seconds(0.4)
	await h._pose("facade_approach")
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("cabinet mantle not offered")
	h._act(device)
	if not await h._wait_until(func() -> bool: return h._standing_above(13.5), 4.0):
		return h._fail("cabinet mantle %s" % h._position())
	if not await h._walk_to(device, Vector2(20, -122.1), 0.05, 3.0):
		return h._fail("cabinet launch position")
	await h._face(Vector2(0, -1))
	await h._seconds(0.3)
	h._tap(1, h._center(&"jump"))
	h._move(device, 0.4)
	var caught := await h._wait_until(func() -> bool: return bool(h._ctx()["hanging"]), 2.0)
	h._move(device, 0)
	if not caught:
		return h._fail("duct hang %s" % h._position())
	await h._seconds(0.3)
	await h._pose("duct_hang")
	if not await _touch_duct_transfer_reach():
		return false
	h._tap(1, h._center(&"jump"))
	if not await h._wait_until(func() -> bool: return h._standing_above(17.0), 4.0):
		return h._fail("duct top-out %s" % h._position())
	for point in [Vector2(22.5, -123.05), Vector2(24, -123.0)]:
		if not await h._walk_to(device, point, 0.06, 6.0):
			return h._fail("duct traverse %s" % h._position())
	await h._face(Vector2(0, -1))
	if not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("vent grip not offered")
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._ctx()["climbing"]), 1.0):
		return h._fail("touch action did not take vent hold")
	h._move(device, 1)
	await h._seconds(1.0)
	await h._pose("vent_climb")
	var climbed := await h._wait_until(func() -> bool: return h._standing_above(22.5), 20.0)
	h._move(device, 0)
	if not climbed:
		return h._fail("vent top-out %s" % h._position())
	for point in [Vector2(22, -125.2), Vector2(12.5, -125.2), Vector2(12.5, -119.65)]:
		if not await h._walk_to(device, point, 0.06, 12.0):
			return h._fail("monorail %s" % h._position())
	await h._face(Vector2(0, -1))
	await h._seconds(0.3)
	if bool(h._native().is_grip_available()):
		return h._fail("ladder should require a leap")
	await h._pose("ladder_leap")
	h._tap(1, h._center(&"jump"))
	h._move(device, 1.0)
	caught = await h._wait_until(func() -> bool: return bool(h._ctx()["climbing"]), 2.0)
	h._move(device, 0)
	if not caught:
		return h._fail("ladder catch %s" % h._position())
	h._move(device, 1)
	climbed = await h._wait_until(func() -> bool: return h._standing_above(33.5), 20.0)
	h._move(device, 0)
	if not climbed:
		return h._fail("ladder top-out %s" % h._position())
	for point in [Vector2(12.5, -125), Vector2(11.2, -125.3)]:
		if not await h._walk_to(device, point, 0.1, 10.0):
			return h._fail("davit exit %s" % h._position())
	await h._seconds(0.5)
	await h._face(Vector2(1, 1))
	await h._pose("facade_arrival")
	if not h._standing_above(33.5) or int(h._native().get_support_entity_id()) != 11 \
			or int(h._native().get_death_count()) != 0:
		return h._fail("unsupported facade arrival %s" % h._position())
	h._detail = "grade_to_33m=1 normal_touch=1 deaths=0 arrival_y=%.3f" % h._position().y
	return true


# Continue the same ordinary touch route onto the moving stair and the +44 m deck.
func _touch_stair() -> bool:
	var device := InputRouter.Device.TOUCH
	if not await _touch_facade():
		return false
	if int(h._native().get_entity_body_count(2600)) != 1:
		return h._fail("swinging stair is absent from the normal scene")
	# The actual hand point is 1.05 m above the standing capsule centre. Stop
	# inside the normal 1.20 m reach, clear of the landing's south rail.
	for point in [Vector2(-17.5, -125.5), Vector2(-17.5, -121.35)]:
		if not await h._walk_to(device, point, 0.08, 20.0):
			return h._fail("stair handle approach %s" % h._position())
	await h._face(Vector2(0, 1))
	h._main._pitch = -0.75
	await h._seconds(0.5)
	await h._pose("stair_handle_ready")
	if not await h._offered(&"pick_up", "GRAB") or int(h._native().get_carry_target_entity_id()) != 2602:
		return h._fail("stair chain handle was not offered: target=%d grounded=%s traversal=%d action=%s" % [
			int(h._native().get_carry_target_entity_id()),
			str(h._native().is_player_grounded()), int(h._native().get_traversal_state()), str(h._ctx()["action"])])
	h._act(device)
	await h._seconds(0.3)
	if int(h._native().get_carrying_entity_id()) != 2602:
		return h._fail("stair handle was not held")
	h._move(device, -0.35)
	await h._seconds(0.6)
	h._move(device, 0)
	if int(h._native().get_carrying_entity_id()) == 2602:
		h._act(device)
	await h._seconds(0.1)
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.4
	await h._pose("stair_released")
	if not await h._walk_to(device, Vector2(-16.4, -122.1), 0.1, 8.0):
		return h._fail("stair foot %s" % h._position())
	var used_flight := false
	for x in [-15.2, -14.0, -12.0, -10.0, -8.0, -5.0, -2.7]:
		if not await h._walk_to(device, Vector2(x, -122.1), 0.1, 25.0):
			var flight := int(h._native().get_kit_body_index(2600))
			var flight_pose: Transform3D = h._native().get_kit_body_transform(flight)
			var sample_tick := int(h._native().get_tick_index())
			await h._frames(2)
			var sample_seconds := float(int(h._native().get_tick_index()) - sample_tick) * float(h._native().get_fixed_step_seconds())
			var angular_z := angle_difference(flight_pose.basis.get_euler().z, h._teeter_angle(flight)) / maxf(sample_seconds, 0.000001)
			return h._fail("stair climb at %.1f: %s support=%d grounded=%s traversal=%d velocity=%s flight_origin=%s flight_angle_z=%.6f flight_velocity=%s measured_angular_z_rad_s=%.6f grip=%s landing=%s deaths=%d" % [
				x, h._position(), h._native().get_support_entity_id(), h._ctx()["grounded"], h._ctx()["traversal"], h._velocity(),
				flight_pose.origin, flight_pose.basis.get_euler().z, h._native().get_kit_body_linear_velocity(flight), angular_z,
				h._native().is_grip_available(), h._native().get_landing_state(), h._native().get_death_count()])
		if int(h._native().get_support_entity_id()) == 2600:
			used_flight = true
		if x == -10.0:
			h._main._pitch = -0.55
			await h._pose("stair_climb")
	if not used_flight:
		return h._fail("player never stood on the moving flight")
	await h._seconds(0.3)
	var stair_body := int(h._native().get_kit_body_index(2600))
	var stair_pose: Transform3D = h._native().get_kit_body_transform(stair_body)
	var exit_local := Vector3(0.25 + 43.0 * 0.25 / tan(0.70860367) + 0.45, 11.3, 0.0)
	var exit_x := (stair_pose * exit_local).x
	if not await h._walk_to(device, Vector2(exit_x, -122.4), 0.1, 4.0):
		return h._fail("stair landing exit %s" % h._position())
	h._main._pitch = -0.4
	await h._pose("stair_exit_ready")
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(exit_x, -125.5), 0.1, 6.0):
		return h._fail("stair deck jump %s" % h._position())
	await h._seconds(0.5)
	await h._face(Vector2(0, 1))
	h._main._pitch = -0.4
	await h._pose("stair_arrival")
	if not h._standing_above(44.5) or int(h._native().get_support_entity_id()) != 11 \
			or int(h._native().get_death_count()) != 0:
		return h._fail("unsupported +44 m arrival %s" % h._position())
	h._detail = "grade_to_44m=1 moving_stair=1 normal_touch=1 deaths=0 arrival_y=%.3f" % h._position().y
	return true


func _touch_upper() -> bool:
	var device := InputRouter.Device.TOUCH
	if not await _touch_stair():
		return false
	if int(h._native().get_entity_body_count(2700)) != 1:
		return h._fail("upper lift absent from the normal scene")
	for point in [Vector2(4.0, -125.5), Vector2(4.0, -116.7)]:
		if not await h._walk_to(device, point, 0.12, 12.0):
			return h._fail("upper lift boarding %s" % h._position())
	if int(h._native().get_support_entity_id()) != 2700:
		return h._fail("upper lift did not support boarding %s" % h._position())
	await h._face(Vector2(0, 1))
	h._main._pitch = -0.4
	await h._seconds(0.4)
	await h._pose("upper_handle_ready")
	if not await h._offered(&"pick_up", "GRAB") or int(h._native().get_carry_target_entity_id()) != 2703:
		return h._fail("upper handle not offered at %s target=%d" % [
			h._position(), int(h._native().get_carry_target_entity_id())])
	h._act(device)
	await h._seconds(0.3)
	if int(h._native().get_carrying_entity_id()) != 2703:
		return h._fail("upper handle was not held")
	var lever := int(h._native().get_kit_body_index(2702))
	if lever < 0:
		h._act(device)
		return h._fail("upper lift lever2702 absent")
	# Let finite loaded motion reach the real trip before releasing the handle.
	h._move(device, -0.45)
	var pull_finished := await h._wait_until(func() -> bool:
		if int(h._native().get_carrying_entity_id()) != 2703:
			return true
		var pose: Transform3D = h._native().get_kit_body_transform(lever)
		var q := pose.basis.get_rotation_quaternion()
		return 2.0 * atan2(q.z, q.w) >= 0.18, 3.0)
	h._move(device, 0.0)
	var held := int(h._native().get_carrying_entity_id())
	var lever_pose: Transform3D = h._native().get_kit_body_transform(lever)
	var lever_q := lever_pose.basis.get_rotation_quaternion()
	var trip_angle := 2.0 * atan2(lever_q.z, lever_q.w)
	if held == 2703:
		h._act(device)
	if not pull_finished or held != 2703 or trip_angle < 0.18:
		return h._fail("upper lift pull did not trip lever2702 within3s while holding2703 angle=%.6f carrying=%d at=%s" % [
			trip_angle, held, h._position()])
	if not await h._wait_until(func() -> bool: return h._standing_above(54.5) and \
			int(h._native().get_support_entity_id()) == 2700, 20.0):
		return h._fail("counterweight never raised the rider %s" % h._position())
	await h._pose("upper_lift_rising")
	await h._seconds(3.0)
	if not h._standing_above(55.4) or int(h._native().get_support_entity_id()) != 2700:
		return h._fail("upper lift did not settle as support %s" % h._position())
	await h._pose("upper_lift_seated")
	for point in [Vector2(4.0, -122.0), Vector2(4.0, -125.5)]:
		if not await h._walk_to(device, point, 0.12, 10.0):
			return h._fail("upper lift exit %s" % h._position())
	if not h._standing_above(55.5) or int(h._native().get_support_entity_id()) != 11:
		return h._fail("unsupported upper lift exit %s" % h._position())
	await h._pose("deck_55m")
	# Cross the receiver lip with ordinary forward input before turning.
	# Brake on its flat yellow step; this approach does not settle on the incline.
	if not await h._walk_to(device, Vector2(4.0, -122.0), 0.12, 8.0) \
			or not h._standing_above(55.4) or int(h._native().get_support_entity_id()) != 1700:
		return h._fail("upper receiver crossing lacks actual55m footing at=%s" % h._position())
	var step := Vector2(2.6, -122.0)
	if not await h._walk_to(device, step, 0.12, 8.0, true):
		# Walking catches the raised step's rounded front lip. Jump only from
		# its real receiver contact, then brake onto the actual flat top.
		var lip := h._position()
		if absf(lip.x - step.x) > 0.15 or lip.z < -121.55 or lip.z > -120.85:
			return h._fail("upper step approach missed the real front lip at=%s" % lip)
		await h._face(Vector2(0, -1))
		if not await h._wait_until(func() -> bool:
			return h._standing_above(55.4) and int(h._native().get_support_entity_id()) == 1700, 1.0):
			return h._fail("upper step Jump lacks actual receiver1700 footing at=%s" % h._position())
		h._touch(1, h._center(&"jump"), true)
		var took_off := await h._wait_until(func() -> bool:
			return not h._native().is_player_grounded() and h._velocity().y > 0.2, 0.5)
		h._touch(1, h._center(&"jump"), false)
		if not took_off or not await h._walk_to(device, step, 0.12, 8.0, true):
			return h._fail("upper step ordinary Jump did not land and settle at=%s" % h._position())
	if not await h._walk_to(device, Vector2(2.4, -122.0), 0.12, 8.0, true):
		return h._fail("upper cabinet approach did not settle at=%s velocity=%s" % [h._position(), h._velocity()])
	var step_contact: Vector3 = h._native().get_support_contact_point()
	if not h._standing_above(55.85) or int(h._native().get_support_entity_id()) != 1700 \
			or step_contact.y < 55.03 or step_contact.x < 2.2 or step_contact.x > 3.0 \
			or step_contact.z < -122.5 or step_contact.z > -121.5:
		return h._fail("upper cabinet approach lacks actual yellow-step footing at=%s contact=%s" % [h._position(), step_contact])
	await h._face(Vector2(-1, 0))
	await h._seconds(0.4)
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("upper cabinet mantle not offered at %s" % h._position())
	h._act(device)
	if not await h._wait_until(func() -> bool: return h._standing_above(57.0), 2.5):
		return h._fail("upper cabinet mantle %s" % h._position())
	if not await h._walk_to(device, Vector2(1.4, -122.1), 0.08, 4.0):
		return h._fail("upper cabinet launch %s" % h._position())
	await h._face(Vector2(0, -1))
	await h._seconds(0.3)
	h._tap(1, h._center(&"jump"))
	h._move(device, 0.4)
	var caught := await h._wait_until(func() -> bool: return bool(h._ctx()["hanging"]), 2.5)
	h._move(device, 0.0)
	if not caught:
		return h._fail("upper duct hang %s" % h._position())
	await h._pose("upper_duct_hang")
	h._tap(1, h._center(&"jump"))
	if not await h._wait_until(func() -> bool: return h._standing_above(60.5), 2.5):
		return h._fail("upper duct top-out %s" % h._position())
	# Brake before the duct's real outer edge instead of accepting a moving
	# horizontal arrival that can carry the body off support1701.
	if not await h._walk_to(device, Vector2(5.4, -123.0), 0.08, 10.0, true) \
			or not h._standing_above(60.9) or int(h._native().get_support_entity_id()) != 1701 \
			or h._native().get_support_contact_point().y < 60.28:
		return h._fail("upper vent approach lacks settled duct1701 top at=%s velocity=%s grounded=%s support=%d contact=%s" % [
			h._position(), h._velocity(), h._native().is_player_grounded(), h._native().get_support_entity_id(), h._native().get_support_contact_point()])
	await h._face(Vector2(0, -1))
	await h._pose("upper_vent_ready")
	if not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("upper vent grip not offered at=%s velocity=%s grounded=%s support=%d" % [
			h._position(), h._velocity(), h._native().is_player_grounded(), h._native().get_support_entity_id()])
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._ctx()["climbing"]), 1.0):
		return h._fail("upper vent did not take a hold")
	h._move(device, 1.0)
	await h._seconds(1.0)
	await h._pose("upper_vent_climb")
	var climbed := await h._wait_until(func() -> bool: return h._standing_above(66.5), 20.0)
	h._move(device, 0.0)
	if not climbed:
		return h._fail("upper vent top-out %s" % h._position())
	if not await h._walk_to(device, Vector2(5.4, -125.4), 0.12, 6.0, true):
		return h._fail("upper deck exit %s" % h._position())
	await h._seconds(0.5)
	await h._face(Vector2(0, 1))
	await h._pose("upper_arrival")
	if not h._standing_above(66.5) or int(h._native().get_support_entity_id()) != 11 or \
			int(h._native().get_death_count()) != 0:
		return h._fail("unsupported +66 m arrival %s" % h._position())
	h._detail = "grade_to_66m=1 counterweight_lift=1 parkour=1 normal_touch=1 deaths=0 arrival_y=%.3f" % h._position().y
	return true

# Focused normal-world mantle: supported entry staging is the only relocation.
# Movement and Action then use the same viewport touch path as ordinary play.
func _touch_ballast_mantle_feedback() -> bool:
	var device := InputRouter.Device.TOUCH
	if not h._campaign_world_intact() or not h._native().debug_restart_at(Vector3(25.3, 66.9, -139.0)):
		return h._fail("ballast mantle supported entry staging rejected")
	await h._seconds(0.5)
	if not _ballast_mantle_footing(11):
		return h._fail("ballast mantle entry " + _ballast_mantle_state())
	for point in [Vector2(27.2, -139.0), Vector2(27.8, -138.58),
			Vector2(30.5, -138.58), Vector2(29.7, -139.45)]:
		if not await h._walk_to(device, point, 0.12, 15.0, true) or not _ballast_mantle_footing(2800):
			return h._fail("ballast mantle approach target=%s %s" % [point, _ballast_mantle_state()])
	h._move(device, 0.0)
	await h._face(Vector2(-1.0, 0.0))
	await h._seconds(0.1)
	if not await h._offered(&"climb", "CLIMB") or int(h._native().get_ledge_entity_id()) != 2801:
		return h._fail("ballast roof mantle not offered " + _ballast_mantle_state())
	var before: Dictionary = h._native().get_landing_state()
	for field in ["foot_transfer_count", "foot_transfer_tick", "foot_transfer_support_entity_id",
			"foot_transfer_peak_hand_load_n", "player_gravity_factor", "traversal_hand_constraint_count",
			"traversal_command_work_bound_j"]:
		if not before.has(field):
			return h._fail("ballast mantle native readback missing " + field)
	var count := int(before["foot_transfer_count"])
	var start_tick := int(h._native().get_tick_index())
	var accepted := int(h._native().get_accepted_traversal_count())
	var plants := int(h._main._audio.foot_plants)
	var command := float(before["traversal_command_work_bound_j"])
	var positive := float(before["hand_actuator_positive_work_j"])
	var last_tick := start_tick
	var loaded := false
	var both_hands := false
	var completed := false
	var peak_load := 0.0
	h._act(device)
	var waited := 0.0
	while waited < 2.5:
		await h.get_tree().process_frame
		waited += h.get_process_delta_time()
		var state: Dictionary = h._native().get_landing_state()
		var tick := int(h._native().get_tick_index())
		var left: Vector3 = state["left_hand_force_n"]
		var right: Vector3 = state["right_hand_force_n"]
		var new_command := float(state["traversal_command_work_bound_j"])
		var new_positive := float(state["hand_actuator_positive_work_j"])
		var budget := 6600.0 * float(tick - last_tick) / 90.0 + 0.001
		if int(h._native().get_death_count()) != 0 or float(state["player_gravity_factor"]) != 1.0 \
				or not left.is_finite() or not right.is_finite() or left.length() > 1501.0 or right.length() > 1501.0 \
				or not is_finite(new_command) or not is_finite(new_positive) \
				or new_command - command < -0.000001 or new_positive - positive < -0.000001 \
				or new_command - command > budget or new_positive - positive > budget:
			return h._fail("ballast mantle force/work/gravity " + _ballast_mantle_state())
		command = new_command
		positive = new_positive
		last_tick = tick
		peak_load = maxf(peak_load, (left + right).length())
		loaded = loaded or left.length() > 0.01 or right.length() > 0.01
		if int(state["foot_transfer_count"]) == count + 1:
			completed = _ballast_mantle_footing(2801)
			break
		if int(h._native().get_traversal_state()) != h._main.TRAVERSAL_CLIMBING \
				or int(h._native().get_traversal_support_entity_id()) != 2801 \
				or int(state["traversal_hand_constraint_count"]) != 2:
			return h._fail("ballast mantle lost native two-hand transfer " + _ballast_mantle_state())
		both_hands = true
	var receipt: Dictionary = h._native().get_landing_state()
	var receipt_tick := int(receipt["foot_transfer_tick"])
	var receipt_peak := float(receipt["foot_transfer_peak_hand_load_n"])
	if not completed or not loaded or not both_hands \
			or int(h._native().get_accepted_traversal_count()) != accepted + 1 \
			or command <= float(before["traversal_command_work_bound_j"]) \
			or positive <= float(before["hand_actuator_positive_work_j"]) \
			or receipt_tick <= start_tick or receipt_tick > last_tick or receipt_tick - start_tick > 153 \
			or int(receipt["foot_transfer_support_entity_id"]) != 2801 \
			or not is_finite(receipt_peak) or receipt_peak <= 0.0 or receipt_peak > 3001.0:
		return h._fail("ballast mantle completion " + _ballast_mantle_state())
	await h._frames(2)
	if int(h._main._audio.foot_plants) != plants + 1 or int(h._main._fb_foot_transfer_count) != count + 1 \
			or int(h._main._fb_foot_transfer_tick) != receipt_tick:
		return h._fail("ballast mantle receipt consumers plants=%d expected=%d %s" % [
			h._main._audio.foot_plants, plants + 1, _ballast_mantle_state()])
	waited = 0.0
	while waited < 3.0:
		await h.get_tree().process_frame
		waited += h.get_process_delta_time()
		if not _ballast_mantle_footing(2801, false) or int(h._native().get_landing_state()["foot_transfer_count"]) != count + 1 \
				or int(h._main._audio.foot_plants) != plants + 1:
			return h._fail("ballast mantle stable roof/replay " + _ballast_mantle_state())
	if not _ballast_mantle_footing(2801):
		return h._fail("ballast mantle roof end slip " + _ballast_mantle_state())
	var roof_support_velocity: Vector3 = h._native().get_support_point_linear_velocity()
	var roof_relative_velocity: Vector3 = h._velocity() - roof_support_velocity
	for point in [Vector2(27.8, -139.45), Vector2(25.3, -139.0)]:
		var support := 2800 if point.x > 27.0 else 11
		if not await h._walk_to(device, point, 0.12, 15.0, true) or not _ballast_mantle_footing(support):
			return h._fail("ballast mantle return target=%s %s" % [point, _ballast_mantle_state()])
	if not h._campaign_world_intact() or int(h._native().get_landing_state()["foot_transfer_count"]) != count + 1 \
			or int(h._main._audio.foot_plants) != plants + 1 or int(h._main._fb_foot_transfer_count) != count + 1:
		return h._fail("ballast mantle return receipt/death " + _ballast_mantle_state())
	h._detail = "staging=supported_teeter_entry shipping_touch=1 real_hands=2 gravity=1 command_bound_j=%.3f positive_work_j=%.3f sampled_peak_n=%.3f receipt_peak_n=%.3f foot_transfer_tick=%d foot_plants=1 stable_roof_seconds=3 roof_support_velocity=%s roof_relative_velocity=%s return_beam=1 return_tower=1 deaths=0" % [
		command - float(before["traversal_command_work_bound_j"]), positive - float(before["hand_actuator_positive_work_j"]),
		peak_load, receipt_peak, receipt_tick, roof_support_velocity, roof_relative_velocity]
	return true


func _ballast_mantle_footing(support: int, require_low_slip: bool = true) -> bool:
	var state: Dictionary = h._native().get_landing_state()
	var slip: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
	return bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == support \
		and int(h._native().get_traversal_state()) == 0 and int(h._native().get_death_count()) == 0 \
		and int(state.get("traversal_hand_constraint_count", -1)) == 0 \
		and float(state.get("player_gravity_factor", -1.0)) == 1.0 \
		and (not require_low_slip or Vector2(slip.x, slip.z).length() < 0.2)


func _ballast_mantle_state() -> String:
	return "tick=%d at=%s velocity=%s support_velocity=%s relative_velocity=%s grounded=%s support=%d traversal=%d traversal_support=%d accepted=%d receipt=%s" % [
		h._native().get_tick_index(), h._position(), h._velocity(), h._native().get_support_point_linear_velocity(),
		h._velocity() - h._native().get_support_point_linear_velocity(), h._native().is_player_grounded(),
		h._native().get_support_entity_id(), h._native().get_traversal_state(),
		h._native().get_traversal_support_entity_id(), h._native().get_accepted_traversal_count(), h._native().get_landing_state()]


func _touch_teeter() -> bool:
	if not await _touch_upper():
		return false
	var device := InputRouter.Device.TOUCH
	if int(h._native().get_entity_body_count(2800)) != 1:
		return h._fail("teeter absent from the normal route")
	var beam := int(h._native().get_kit_body_index(2800))
	var ballast := int(h._native().get_kit_body_index(2801))
	# Brake through ordinary input before each turn; finite stopping distance
	# can otherwise carry the player off the upper ring after arrival.
	for point in [Vector2(24.8, -125.4), Vector2(24.8, -139.0), Vector2(25.3, -139.0)]:
		var arrived := await h._walk_to(device, point, 0.14, 20.0, true)
		var at := h._position()
		var support := int(h._native().get_support_entity_id())
		var grounded := bool(h._native().is_player_grounded())
		var slip: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
		var planar_slip := Vector2(slip.x, slip.z).length()
		if not arrived or not grounded or support != 11 or at.y <= 66.5 \
				or planar_slip >= 0.15 or (point - Vector2(at.x, at.z)).length() > 0.14:
			return h._fail("teeter ring approach target=%s at=%s support=%d grounded=%s slip=%.6f arrived=%s" % [
				point, at, support, grounded, planar_slip, arrived])
	await _face_teeter_pivot(beam)
	await h._pose("teeter_entry")
	if not h._standing_above(66.5) or int(h._native().get_support_entity_id()) != 11:
		return h._fail("teeter entry lost the +66 m ring %s" % h._position())
	for point in [Vector2(27.2, -139.0), Vector2(27.8, -139.45)]:
		if not await h._walk_to(device, point, 0.14, 12.0):
			return h._fail("teeter near-side loading %s" % h._position())
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.08
	await h._pose("teeter_ballast_near")
	if _teeter_ballast_x(beam, ballast) > 1.0:
		return h._fail("ballast moved before contact %.3f" % _teeter_ballast_x(beam, ballast))
	for point in [Vector2(29.8, -139.45), Vector2(31.95, -139.45)]:
		if not await h._walk_to(device, point, 0.14, 12.0):
			return h._fail("teeter ballast push %s position=%.3f" % [
				h._position(), _teeter_ballast_x(beam, ballast)])
	await h._seconds(0.5)
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.08
	await h._pose("teeter_ballast_far")
	if int(h._native().get_support_entity_id()) != 2800 or \
			_teeter_ballast_x(beam, ballast) < 4.15:
		return h._fail("ballast did not reach the far stop %s position=%.3f" % [
			h._position(), _teeter_ballast_x(beam, ballast)])
	# Clear the rail beside the ballast with a lateral leg before advancing.
	var lateral_x := h._position().x
	var lateral_arrived := await h._walk_to(device, Vector2(lateral_x, -138.58), 0.14, 12.0)
	var lateral_grounded := bool(h._native().is_player_grounded())
	var lateral_support := int(h._native().get_support_entity_id())
	if not lateral_arrived or not lateral_grounded or lateral_support != 2800:
		return h._fail("teeter far-side lateral target=%s at=%s velocity=%s support=%d grounded=%s arrived=%s" % [
			Vector2(lateral_x, -138.58), h._position(), h._velocity(), lateral_support, lateral_grounded, lateral_arrived])
	if not await h._walk_to(device, Vector2(32.8, -138.58), 0.14, 12.0):
		return h._fail("teeter far-side loading %s" % h._position())
	await h._seconds(3.0)
	await _face_teeter_pivot(beam)
	await h._pose("teeter_loaded")
	if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2800 or \
			h._teeter_angle(beam) > -0.4:
		return h._fail("player contact did not lower the far tip %s angle=%.3f" % [
			h._position(), h._teeter_angle(beam)])
	if not await h._walk_to(device, Vector2(34.4, -139.0), 0.14, 12.0):
		return h._fail("teeter receiving shelf %s" % h._position())
	await h._seconds(3.0)
	await h._face(Vector2(-1, 0))
	h._main._pitch = -0.16
	await h._pose("teeter_shelf")
	if not h._standing_above(64.0) or int(h._native().get_support_entity_id()) != 1800 or \
			h._teeter_angle(beam) < 0.07:
		return h._fail("teeter failed to unload onto shelf %s angle=%.3f" % [
			h._position(), h._teeter_angle(beam)])
	if not await h._walk_to(device, Vector2(34.72, -139.45), 0.14, 6.0):
		return h._fail("teeter grip approach %s" % h._position())
	await h._face(Vector2(0, -1))
	if not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("teeter upper grip not offered %s" % h._position())
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._ctx()["climbing"]), 1.0):
		return h._fail("teeter upper grip not taken %s" % h._position())
	h._move(device, 1.0)
	await h._seconds(6.0)
	await h._pose("teeter_climb")
	var topped_out := await h._wait_until(func() -> bool: return h._standing_above(77.5), 20.0)
	h._move(device, 0.0)
	if not topped_out:
		return h._fail("teeter upper climb did not top out %s" % h._position())
	if not await h._walk_to(device, Vector2(25.1, -140.8), 0.14, 20.0):
		return h._fail("teeter +77 m catwalk exit %s" % h._position())
	await h._seconds(0.5)
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.25
	await h._pose("teeter_arrival")
	if not h._standing_above(77.5) or int(h._native().get_support_entity_id()) != 11 or \
			int(h._native().get_death_count()) != 0:
		return h._fail("teeter +77 m ring unsupported %s" % h._position())
	# AS-021 approach framing from the earned ring. This capture proves only
	# the visible layout; the full +88 m touch route is a separate handoff gate.
	if not await h._walk_to(device, Vector2(25.1, -131.0), 0.14, 12.0):
		return h._fail("braced bay approach along +77 m ring %s" % h._position())
	var bay_target := Vector3(31.0, 83.0, -137.0)
	var to_bay: Vector3 = bay_target - h._main._camera.global_position
	await h._face(Vector2(to_bay.x, to_bay.z))
	h._main._pitch = atan2(to_bay.y, Vector2(to_bay.x, to_bay.z).length())
	await h._pose("braced_bay_preview")
	h._detail = "grade_to_77m=1 ballast_pushed=1 player_loaded_teeter=1 physical_reset=1 normal_touch=1 deaths=0 arrival_y=%.3f" % h._position().y
	return true


func _touch_braced_bay() -> bool:
	if not await _touch_teeter():
		return false
	var device := InputRouter.Device.TOUCH
	if int(h._native().get_entity_body_count(1900)) != 1:
		return h._fail("braced bay absent from normal play")
	# Settle aligned before the narrow girder; finite braking can overshoot its entry.
	if not await h._walk_to(device, Vector2(28.0, -131.0), 0.14, 12.0, true):
		return h._fail("braced bay flat entry %s" % h._position())
	await h._seconds(0.5)
	var entry_at := h._position()
	var entry_grounded := bool(h._native().is_player_grounded())
	var entry_support := int(h._native().get_support_entity_id())
	var entry_slip: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
	var entry_planar_slip := Vector2(entry_slip.x, entry_slip.z).length()
	var entry_distance := (Vector2(28.0, -131.0) - Vector2(entry_at.x, entry_at.z)).length()
	if not entry_grounded or entry_support != 1900 or absf(entry_at.y - 77.9) > 0.1 \
			or entry_distance > 0.14 or entry_planar_slip >= 0.15:
		return h._fail("braced bay flat entry unsettled target=%s at=%s grounded=%s support=%d height_error=%.6f distance=%.6f slip=%.6f" % [
			Vector2(28.0, -131.0), entry_at, entry_grounded, entry_support,
			absf(entry_at.y - 77.9), entry_distance, entry_planar_slip])
	await h._face(Vector2(0, -1))
	h._main._pitch = 0.11
	await h._pose("braced_bay_entry")
	if not await h._walk_to(device, Vector2(28.0, -144.0), 0.14, 16.0):
		return h._fail("first inclined girder %s" % h._position())
	await h._seconds(0.4)
	if not h._standing_above(82.6) or int(h._native().get_support_entity_id()) != 1900:
		return h._fail("first braced bay landing unsupported %s" % h._position())
	if not await h._walk_to(device, Vector2(29.0, -144.0), 0.14, 4.0):
		return h._fail("gap run-up %s" % h._position())
	await h._seconds(0.4)
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.08
	await h._pose("braced_bay_transfer")
	h._move_dir(device, Vector2(0, 1))
	var at_takeoff := await h._wait_until(
		func() -> bool: return h._position().x >= 30.0 or not bool(h._ctx()["grounded"]), 1.0)
	if not at_takeoff or not bool(h._ctx()["grounded"]):
		h._move_dir(device, Vector2.ZERO)
		return h._fail("gap takeoff lost its supported run-up %s" % h._position())
	h._tap(1, h._center(&"jump"))
	var airborne := false
	var landed := false
	var elapsed := 0.0
	while elapsed < 3.0:
		var at := h._position()
		var to := Vector2(33.7 - at.x, -144.0 - at.z)
		var world_command := (to / 1.4).limit_length(1.0)
		var yaw := float(h._main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		h._move_dir(device, Vector2(world_command.dot(right), world_command.dot(forward)))
		await h.get_tree().process_frame
		elapsed += h.get_process_delta_time()
		airborne = airborne or not bool(h._ctx()["grounded"])
		if airborne and bool(h._ctx()["grounded"]) and h._position().x > 33.2 and absf(h._position().y - 82.9) < 0.12:
			landed = true
			break
	h._move_dir(device, Vector2.ZERO)
	await h._seconds(0.4)
	if not landed or int(h._native().get_support_entity_id()) != 1900:
		return h._fail("gap jump missed far landing %s" % h._position())
	if not await h._walk_to(device, Vector2(34.0, -144.0), 0.14, 4.0):
		return h._fail("second girder alignment %s" % h._position())
	await h._seconds(0.4)
	await h._face(Vector2(0, 1))
	if not await h._walk_to(device, Vector2(34.0, -139.4), 0.14, 8.0):
		return h._fail("second girder approach %s" % h._position())
	var walked_through := await h._walk_to(device, Vector2(34.0, -136.7), 0.14, 3.0)
	if walked_through or h._position().z > -137.8:
		return h._fail("standing capsule passed through low member %s" % h._position())
	await h._face(Vector2(0, 1))
	h._main._pitch = 0.16
	await h._pose("braced_bay_low_member")
	h._tap(1, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool: return bool(h._native().is_player_crouched()), 0.3):
		return h._fail("crouch input failed at braced bay")
	if not await h._walk_to(device, Vector2(34.0, -136.7), 0.14, 8.0):
		return h._fail("crouched passage blocked %s" % h._position())
	h._tap(1, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool: return not bool(h._native().is_player_crouched()), 0.3):
		return h._fail("standing clearance missing beyond low member")
	if not await h._walk_to(device, Vector2(34.0, -131.5), 0.14, 10.0):
		return h._fail("upper braced bay junction %s" % h._position())
	await h._seconds(0.4)
	if not h._standing_above(87.6) or int(h._native().get_support_entity_id()) != 1900:
		return h._fail("upper braced bay junction unsupported %s" % h._position())
	await h._face(Vector2(-1, 0))
	h._main._pitch = -0.12
	await h._pose("braced_bay_upper_junction")
	if not await h._walk_to(device, Vector2(34.0, -132.0), 0.14, 4.0):
		return h._fail("return girder turn %s" % h._position())
	await h._seconds(0.3)
	if not await h._walk_to(device, Vector2(26.55, -132.0), 0.14, 13.0):
		return h._fail("return girder balance %s" % h._position())
	await h._face(Vector2(-1, 0))
	await h._seconds(0.3)
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("tower +88 m mantle unavailable %s" % h._position())
	h._act(device)
	await h._seconds(1.0)
	if not await h._walk_to(device, Vector2(24.8, -132.0), 0.14, 5.0):
		return h._fail("braced bay ring exit %s" % h._position())
	await h._seconds(0.4)
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.12
	await h._pose("braced_bay_arrival")
	if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 11 or \
			absf(h._position().y - 88.9) > 0.1 or int(h._native().get_death_count()) != 0:
		return h._fail("braced bay +88 m ring unsupported %s" % h._position())
	h._detail = "grade_to_88m=1 gap_jump=1 crouched_portal=1 mantle=1 support=11 deaths=0 arrival_y=%.3f" % h._position().y
	return true


func _face_teeter_pivot(body: int) -> void:
	var pose: Transform3D = h._native().get_kit_body_transform(body)
	var at := h._position()
	await h._face(Vector2(pose.origin.x - at.x, pose.origin.z - at.z))
	var to_pivot: Vector3 = pose.origin - h._main._camera.global_position
	h._main._pitch = atan2(to_pivot.y, Vector2(to_pivot.x, to_pivot.z).length())


func _teeter_ballast_x(beam: int, ballast: int) -> float:
	var beam_pose: Transform3D = h._native().get_kit_body_transform(beam)
	var carriage_pose: Transform3D = h._native().get_kit_body_transform(ballast)
	return (beam_pose.affine_inverse() * carriage_pose.origin).x
