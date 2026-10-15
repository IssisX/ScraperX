extends RefCounted

# Crown gondola, swing transfer, independent hand catches and recovery.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

var h: Harness


func _init(harness: Harness) -> void:
	h = harness


func _touch_crown_gondola(stage_at_165: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_165:
		if not h._native().debug_restart_at(Vector3(-24.7, 165.9, -159.8)):
			return h._fail("crown gondola +165m staging rejected")
		await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(165.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("crown gondola approach requires supported +165m Tower11")
	for point in [Vector2(-24.7, -160.45), Vector2(-40.5, -160.45)]:
		if not await h._walk_to(device, point, 0.12, 12.0):
			return h._fail("crown gondola lower landing approach at=%s" % h._position())
	var lower: Dictionary = h._native().get_supplied_machine_state()
	if String(lower["name"]) != "CROWN GONDOLA" or int(lower["station"]) != 2:
		return h._fail("crown gondola lower station is not reachable")
	if not await h._walk_to(device, Vector2(-44, -159), 0.08, 6.0):
		return h._fail("crown gondola boarding takeoff at=%s" % h._position())
	await h._face(Vector2(0, 1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(-44, -156.1), 0.10, 8.0, true):
		return h._fail("crown gondola physical boarding at=%s" % h._position())
	var cabin := int(h._native().get_kit_body_index(2983))
	if cabin < 0:
		return h._fail("crown gondola cabin2983 body unavailable")
	if not await _wait_crown_boarded(cabin):
		_crown_ride_diagnostic(cabin, "boarding_floor_unstable")
		return h._fail("crown gondola boarding lacks stable cabin floor contact at=%s" % h._position())
	if String(h._native().get_supplied_machine_state()["name"]) != "CROWN GONDOLA" \
			or int(h._native().get_supplied_machine_state()["station"]) != 1 \
			or not await h._offered(&"operate", "OPERATE", "CROWN GONDOLA"):
		return h._fail("crown gondola cabin control unavailable at=%s" % h._position())
	_crown_ride_diagnostic(cabin, "before_operate")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("crown gondola touch pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), true)
	var ride_tick := int(h._native().get_tick_index())
	var sample_tick := ride_tick - 15
	var ride_seconds := 0.0
	while ride_seconds < 2.0:
		var tick := int(h._native().get_tick_index())
		if tick - sample_tick >= 15:
			_crown_ride_diagnostic(cabin, "up")
			sample_tick = tick
		var state: Dictionary = h._native().get_supplied_machine_state()
		if h._main._operating != &"supplied_machine" or (not h._native().is_player_grounded() \
				and int(h._native().get_support_entity_id()) != 2983 \
				and h._position().y < float(state["surface_y"]) - 0.5):
			_crown_ride_diagnostic(cabin, "lost_operation_or_below_cabin")
			h._touch(1, h._center(&"pendant_up"), false)
			return h._fail("crown gondola lost cabin operation/footing during initial UP")
		await h.get_tree().process_frame
		ride_seconds = float(int(h._native().get_tick_index()) - ride_tick) \
			* float(h._native().get_fixed_step_seconds())
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 197.97, maxf(0.1, 90.0 - ride_seconds))
	h._touch(1, h._center(&"pendant_up"), false)
	if not reached:
		return h._fail("crown gondola touch ride failed at=%s station=%s" % [
			h._position(), h._native().get_supplied_machine_state()])
	# The wheel brake does not freeze its passive cabin hinge. Wait for
	# sustained native contact, low cabin speed and an upright walking floor.
	if not await _wait_crown_settled(cabin):
		return h._fail("crown gondola upper cabin did not settle at=%s" % h._position())
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if String(arrival["name"]) != "CROWN GONDOLA" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return h._fail("crown gondola arrival violates finite energy/brake state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("crown gondola DONE did not release operation")
	var pose: Transform3D = h._native().get_kit_body_transform(cabin)
	if not await h._walk_to(device, Vector2(pose.origin.x, pose.origin.z - 1.5), 0.08, 7.0):
		return h._fail("crown gondola upper exit takeoff at=%s" % h._position())
	await h._face(Vector2(0, -1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(h._position().x, -159.8), 0.10, 7.0):
		return h._fail("crown gondola upper receiver transfer at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 1985, 3.0):
		return h._fail("crown gondola upper transfer missed the actual start platform1985")
	if stage_at_165 and not await _touch_crown_swing_recovery():
		return false
	if not await _touch_crown_swing_exit():
		return false
	if stage_at_165 and not await _touch_crown_upper_recall():
		return false
	for point in [Vector2(-24.7, -160.45), Vector2(-24.7, -159.8)]:
		if not await h._walk_to(device, point, 0.12, 12.0):
			return h._fail("crown gondola +198m Tower exit at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return h._standing_above(198.7) and absf(h._position().y - 198.9) <= 0.15 \
			and int(h._native().get_support_entity_id()) == 11, 3.0) or not h._campaign_world_intact():
		return h._fail("crown gondola +198m arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=%s shipping_touch=1 crown_gondola=1 passive_cabin_settled=1 hanging_swing=1 earned_release=1 finite_energy=1 missed_release_recovery=%d upper_recall=%d supported_height_m=198 support=11 deaths=0" % [
		"supported_165m" if stage_at_165 else "ordinary_grade_spawn", int(stage_at_165), int(stage_at_165)]
	return true


func _touch_crown_swing() -> bool:
	if not h._native().debug_restart_at(Vector3(-41.0759, 198.9, -160.45)):
		return h._fail("crown swing +198m start platform staging rejected")
	await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(198.7) \
			or int(h._native().get_support_entity_id()) != 1985:
		return h._fail("crown swing staging lacks supported start platform1985")
	if not await _touch_crown_swing_recovery() or not await _touch_crown_swing_exit():
		return false
	for point in [Vector2(-24.7, -160.45), Vector2(-24.7, -159.8)]:
		if not await h._walk_to(InputRouter.Device.TOUCH, point, 0.12, 12.0):
			return h._fail("crown swing Tower exit at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return h._standing_above(198.7) and absf(h._position().y - 198.9) <= 0.15 \
			and int(h._native().get_support_entity_id()) == 11, 3.0) or not h._campaign_world_intact():
		return h._fail("crown swing arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=supported_198m_start_platform shipping_touch=1 missed_release_recovery=1 hanging_swing=1 earned_release=1 supported_height_m=198 support=11 deaths=0"
	return true


func _crown_swing_catch_diagnostic(phase: String) -> void:
	var bar := int(h._native().get_kit_body_index(2930))
	var bar_pose: Variant = h._native().get_kit_body_transform(bar) if bar >= 0 else "unavailable"
	var bar_grip: Variant = bar_pose * Vector3(0, -7, 0) if bar_pose is Transform3D else "unavailable"
	var landing: Dictionary = h._native().get_landing_state()
	var move: Vector2 = h._main._touch.move_vector
	var facing := Vector2(-sin(h._main._yaw), -cos(h._main._yaw))
	var right := Vector2(cos(h._main._yaw), -sin(h._main._yaw))
	print("SCRAPERX_CROWN_CATCH phase=%s tick=%d player=%s velocity=%s grounded=%s crouched=%s support=%d traversal=%d traversal_support=%d left_hand=%s right_hand=%s hands=%s player_swinging=%s facing_from_yaw=%s touch_move=%s world_move_from_touch=%s bar_transform=%s bar_grip=%s" % [
		phase, h._native().get_tick_index(), h._position(), h._velocity(), h._native().is_player_grounded(),
		h._native().is_player_crouched(), h._native().get_support_entity_id(), h._native().get_traversal_state(),
		h._native().get_traversal_support_entity_id(), h._native().get_traversal_left_hand(),
		h._native().get_traversal_right_hand(), landing.get("traversal_hand_constraint_count", "not_exposed"),
		landing["player_swinging"], facing, move, right * move.x + facing * move.y, bar_pose, bar_grip])


func _touch_crown_swing_catch() -> bool:
	var device := InputRouter.Device.TOUCH
	for point in [Vector2(-41.0759, -160.45), Vector2(-38.1, -160.45)]:
		if not await h._walk_to(device, point, 0.08, 7.0, true):
			return h._fail("crown hanging arm takeoff approach at=%s" % h._position())
	await h._face(Vector2(1, 0))
	var takeoff_slip: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
	if not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 1985 \
			or Vector2(takeoff_slip.x, takeoff_slip.z).length() >= 0.15:
		_crown_swing_catch_diagnostic("takeoff_unsettled")
		return h._fail("crown hanging arm Jump requires settled start platform1985 at=%s" % h._position())
	# A retry leaves the real bar oscillating. Time the ordinary jump while its
	# physical crossbar is ahead and moving away, rather than jumping into a
	# behind-facing grip. A stationary first approach retains its proven stance.
	var bar := int(h._native().get_kit_body_index(2930))
	var phase_start := int(h._native().get_tick_index())
	var previous_tick := phase_start
	var previous_grip: Vector3 = h._native().get_kit_body_transform(bar) * Vector3(0, -7, 0)
	var catch_phase := false
	while float(int(h._native().get_tick_index()) - phase_start) * float(h._native().get_fixed_step_seconds()) < 12.0:
		await h.get_tree().process_frame
		var tick := int(h._native().get_tick_index())
		if tick == previous_tick:
			continue
		var grip: Vector3 = h._native().get_kit_body_transform(bar) * Vector3(0, -7, 0)
		var grip_vx := (grip.x - previous_grip.x) / (float(tick - previous_tick) * float(h._native().get_fixed_step_seconds()))
		var ahead := grip.x - h._position().x
		previous_grip = grip
		previous_tick = tick
		if ahead >= 0.30 and ahead <= 1.25 and grip.y <= 200.60 \
				and (grip_vx >= 0.15 or (absf(grip_vx) <= 0.01 and ahead >= 0.90)):
			catch_phase = true
			_crown_swing_catch_diagnostic("timed_takeoff")
			break
	if not catch_phase:
		return h._fail("hanging swing never presented a reachable forward catch phase")
	var catch_tick := int(h._native().get_tick_index())
	var sample_tick := catch_tick
	h._move(device, 1.0)
	_crown_swing_catch_diagnostic("start")
	h._tap(1, h._center(&"jump"))
	while int(h._native().get_tick_index()) - catch_tick < 100 \
			and not bool(h._native().get_landing_state()["player_swinging"]):
		var tick := int(h._native().get_tick_index())
		if tick - sample_tick >= 15:
			_crown_swing_catch_diagnostic("approach")
			sample_tick = tick
		await h.get_tree().process_frame
	_crown_swing_catch_diagnostic("finish")
	h._move(device, 0.0)
	if not _crown_swing_attached():
		return h._fail("crown ordinary jump did not catch arm2930 at=%s" % h._position())
	await h._seconds(2.0)
	if not _crown_swing_attached():
		return h._fail("crown hanging arm lost its native neutral hold")
	return true


func _touch_crown_swing_exit() -> bool:
	if not await _touch_crown_swing_catch():
		return false
	if not await _touch_hanging_posture():
		return false
	var device := InputRouter.Device.TOUCH
	var pump_tick := int(h._native().get_tick_index())
	var released := false
	var saw_tuck := false
	var saw_extension := false
	while float(int(h._native().get_tick_index()) - pump_tick) * float(h._native().get_fixed_step_seconds()) < 180.0:
		if not _crown_swing_attached() or int(h._native().get_death_count()) != 0:
			h._move(device, 0.0)
			return h._fail("crown deliberate lean lost its native hanging support at=%s" % h._position())
		var velocity := h._velocity()
		var posture: Dictionary = h._native().get_landing_state()
		var wants_tuck := velocity.x > 0.0 and h._position().x > -37.7
		if wants_tuck != bool(posture.get("swing_tuck_requested", false)):
			h._tap(2, h._center(&"crouch"))
			await h.get_tree().process_frame
			posture = h._native().get_landing_state()
		saw_tuck = saw_tuck or bool(posture.get("swing_tuck_requested", false))
		saw_extension = saw_extension or not bool(posture.get("swing_tuck_requested", false))
		if h._position().x > -32.4 and velocity.x > 2.5 and velocity.y > 1.5 \
				and bool(posture.get("swing_tuck_requested", false)) \
				and float(posture.get("swing_commanded_body_below_grip_m", 1.0)) < 0.8:
			h._move(device, 0.0)
			h._tap(1, h._center(&"jump"))
			released = true
			break
		# Phase choice from observed hand position and motion; this proves the
		# physical route, without establishing human usability.
		# Native hands bound lean to +/-0.15m and stroke to 0.6m/s. The 500W
		# budget meters positive 1/2*k*extension^2 target work; positive/absorbed
		# receipts are distinct from Fmax-distance and full energy closure.
		var hand: Vector3 = h._native().get_traversal_left_hand()
		h._move(device, 1.0 if velocity.x - 1.2 * (hand.x + 36.95) > 0.0 else -1.0)
		await h.get_tree().process_frame
	h._move(device, 0.0)
	if not released or not saw_tuck or not saw_extension:
		return h._fail("hanging swing did not earn a tucked release using deliberate tuck and extension")
	if not await h._wait_until(func() -> bool:
		return not bool(h._native().get_landing_state()["player_swinging"]), 0.5):
		return h._fail("crown Jump did not release the hanging arm")
	var detached: Dictionary = h._native().get_landing_state()
	if bool(detached.get("swing_tuck_available", true)) or bool(detached.get("swing_tuck_requested", true)):
		return h._fail("hanging posture readback survived release")
	# Coast with neutral input. A real landing brace can lower the capsule to
	# 198.59m; grounded contact on the narrow receiver establishes this arrival.
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 1985 \
			and h._position().x > -30.05 and h._position().y > 198.3, 3.0):
		return h._fail("crown passive release missed the narrow receiver at=%s" % h._position())
	return h._campaign_world_intact()


# One initial normal-world reach probe; every subsequent transition is real touch.
# This proves the bounded catch/posture/release slice, not continuous ascent.
func _touch_single_hand_catch(right_hand: bool = false, release_with_action: bool = false) -> bool:
	if not h._campaign_world_intact():
		return false
	if not await h._wait_until(func() -> bool: return int(h._native().get_tick_index()) >= 180, 3.0):
		return h._fail("single-hand normal world did not settle for 180 native ticks")
	if not h._native().debug_restart_at(Vector3(23.15 if right_hand else 16.85, 11.5, -118.0)):
		return h._fail("single-hand initial reach staging rejected")
	h._main._yaw = PI * 0.5 if right_hand else -PI * 0.5
	h._main._pitch = 0.0
	var staged_tick := int(h._native().get_tick_index())
	if not await h._wait_until(func() -> bool: return int(h._native().get_tick_index()) > staged_tick, 0.5):
		return h._fail("single-hand approach did not advance a native tick")
	if not bool(h._native().is_grip_available()) or not h._main._touch.is_button_shown(&"action") \
			or int(h._native().get_entity_body_count(2952)) == 0 \
			or h._native().get_grip_point().distance_to(Vector3(22.6001 if right_hand else 17.39992523, 11.91257095, -118.16998291)) > 0.08:
		return h._fail("single-hand blocked-other guardrail approach has no actual touch Action grip")
	print("SCRAPERX_SINGLE_HAND approach tick=%d position=%s cue=%s action=%s" % [
		h._native().get_tick_index(), h._position(), h._native().get_grip_point(), h._ctx()["action"]])
	var audio_before: Dictionary = h._main._audio.get_grip_feedback_stats()
	h._act(InputRouter.Device.TOUCH)
	if not await h._wait_until(func() -> bool: return _single_hand_attached(right_hand), 0.5):
		return h._fail("single-hand touch Action did not attach only the requested actual hand")
	await h._frames(2)
	var generation_key := "right_hand_generation" if right_hand else "left_hand_generation"
	var held_generation := int(h._native().get_landing_state().get(generation_key, 0))
	var audio_caught: Dictionary = h._main._audio.get_grip_feedback_stats()
	var contact_count := int(audio_caught["climb_regrips"])
	var contact_playbacks := int(audio_caught["grab_playbacks"])
	var caught_point: Vector3 = h._native().get_traversal_right_hand() if right_hand else h._native().get_traversal_left_hand()
	var cue_position: Vector3 = audio_caught["last_climb_regrip_position"]
	if held_generation <= 0 or contact_count != int(audio_before["climb_regrips"]) + 1 \
			or int(audio_caught["grabs"]) != int(audio_before["grabs"]) + 1 \
			or contact_playbacks != int(audio_before["grab_playbacks"]) + 1 \
			or cue_position.distance_to(caught_point) > 0.05:
		return h._fail("single actual catch lacks one sound at its attached native hand: stats=%s point=%s" % [audio_caught, caught_point])
	await h._seconds(0.35)
	if not _single_hand_attached(right_hand) or not _single_hand_arms_honest(right_hand) \
			or not _single_hand_feedback_quiet(right_hand, held_generation, contact_count, contact_playbacks):
		return h._fail("single-hand neutral native hold or arm presentation is dishonest")
	if not _single_hand_controls_honest("TUCK"):
		return h._fail("solo hold exposes unsupported controls: action=%s jump=%s drop=%s" % [
			h._ctx()["action"], h._main._touch.button_label(&"jump"), h._main._touch.button_label(&"drop")])
	var before: Dictionary = h._native().get_landing_state()
	if not bool(before.get("swing_tuck_available", false)) \
			or bool(before.get("swing_tuck_requested", true)) \
			or not h._main._touch.is_button_shown(&"crouch") \
			or h._main._touch.button_label(&"crouch") != "TUCK":
		return h._fail("single actual hand did not expose touch TUCK")
	var target_before := float(before["hanging_commanded_span_m"])
	var span_before := float(before["hanging_actual_span_m"])
	var work_before := float(before["hand_actuator_positive_work_j"])
	var tuck_tick := int(h._native().get_tick_index())
	h._tap(2, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		return bool(state.get("swing_tuck_requested", false)) \
			and float(state.get("hanging_commanded_span_m", target_before)) < target_before - 0.20 \
			and float(state.get("hanging_actual_span_m", span_before)) < span_before - 0.10, 4.0):
		return h._fail("single-hand touch TUCK did not shorten actual span and paid target")
	var tucked: Dictionary = h._native().get_landing_state()
	var target_tucked := float(tucked["hanging_commanded_span_m"])
	var actual_hold: Vector3 = h._native().get_traversal_right_hand() if right_hand else h._native().get_traversal_left_hand()
	var actual_span := actual_hold.distance_to(h._position())
	var tuck_seconds := float(int(h._native().get_tick_index()) - tuck_tick) * float(h._native().get_fixed_step_seconds())
	var paid_work := float(tucked["hand_actuator_positive_work_j"]) - work_before
	if not _single_hand_attached(right_hand) or bool(h._native().is_player_crouched()) \
			or absf(actual_span - float(tucked["hanging_actual_span_m"])) > 0.001 \
			or target_before - target_tucked > 0.251 \
			or not is_finite(paid_work) or paid_work <= 0.0 or paid_work > 500.0 * tuck_seconds + 0.01 \
			or h._main._touch.button_label(&"crouch") != "EXTEND" \
			or not _single_hand_controls_honest("EXTEND") \
			or not _single_hand_arms_honest(right_hand) \
			or not _single_hand_feedback_quiet(right_hand, held_generation, contact_count, contact_playbacks):
		return h._fail("single-hand TUCK lacks honest finite native hands, paid work or EXTEND")
	h._tap(2, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		return not bool(state.get("swing_tuck_requested", true)) \
			and float(state.get("hanging_commanded_span_m", target_tucked)) > target_tucked + 0.20, 4.0):
		return h._fail("single-hand touch EXTEND did not restore native target")
	if not _single_hand_attached(right_hand) or h._main._touch.button_label(&"crouch") != "TUCK" \
			or not _single_hand_controls_honest("TUCK") \
			or not _single_hand_feedback_quiet(right_hand, held_generation, contact_count, contact_playbacks):
		return h._fail("single-hand extension lost the hand or touch toggle label")
	var release_y := h._position().y
	if release_with_action:
		# The retained scenario ID now probes Reach Hand, then explicitly drops.
		# Action may add a valid partner; it must never release the planted hand.
		var action_tick := int(h._native().get_tick_index())
		h._act(InputRouter.Device.KEYBOARD_MOUSE)
		if not await h._wait_until(func() -> bool: return int(h._native().get_tick_index()) > action_tick, 0.5):
			return h._fail("single-hand Reach Action did not reach a native tick")
		var reached: Dictionary = h._native().get_landing_state()
		if (int(reached.get("traversal_hand_constraint_count", 0)) < 1
				or int(reached.get(generation_key, 0)) != held_generation):
			return h._fail("Reach Hand Action released the actual planted hand")
		var after_reach: Dictionary = h._main._audio.get_grip_feedback_stats()
		contact_count = int(after_reach["climb_regrips"])
		contact_playbacks = int(after_reach["grab_playbacks"])
		h._tap(1, h._center(&"drop"))
	else:
		h._tap(1, h._center(&"drop" if right_hand else &"jump"))
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		return int(h._native().get_traversal_state()) == 0 \
			and int(state.get("traversal_hand_constraint_count", -1)) == 0 \
			and not bool(state.get("left_hand_attached", true)) \
			and not bool(state.get("right_hand_attached", true)) \
			and not bool(state.get("swing_tuck_available", true)) \
			and not bool(state.get("swing_tuck_requested", true)), 0.5):
		return h._fail("single-hand touch release did not clear actual hand and posture")
	if not await h._wait_until(func() -> bool: return h._position().y < release_y - 0.10, 0.75):
		return h._fail("single-hand release did not begin a real fall")
	for hand in h._main._arms._hands:
		if hand.anchored or hand.root.global_position.distance_to(h._native().get_player_render_position()) > 1.8:
			return h._fail("single-hand released arm retained a fictitious grip")
	if float(h._native().get_landing_state().get("player_gravity_factor", -1.0)) != 1.0:
		return h._fail("single-hand release changed native gravity")
	if int(h._main._audio.get_grip_feedback_stats()["climb_regrips"]) != contact_count \
			or int(h._main._audio.get_grip_feedback_stats()["grab_playbacks"]) != contact_playbacks:
		return h._fail("single-hand release invented a contact sound")
	h._detail = "normal_world=1 initial_stages=1 viewport_action_crouch_release=1 solo_controls_honest=1 release_control=%s actual_hands=%s initial_contact_cues=1 initial_grab_playbacks=1 passive_extra_cues=0 actual_span_shortening_m=%.4f target_shortening_m=%.4f paid_work_j=%.4f honest_arms=1 passive_release=1" % [
		"keyboard_reach_then_touch_drop" if release_with_action else ("touch_drop" if right_hand else "touch_jump"),
		"right_only" if right_hand else "left_only", span_before - float(tucked["hanging_actual_span_m"]), target_before - target_tucked,
		float(tucked["hand_actuator_positive_work_j"]) - work_before]
	return h._campaign_world_intact()


func _single_hand_attached(right_hand: bool = false) -> bool:
	var state: Dictionary = h._native().get_landing_state()
	var left_force: Vector3 = state.get("left_hand_force_n", Vector3(INF, INF, INF))
	var right_force: Vector3 = state.get("right_hand_force_n", Vector3(INF, INF, INF))
	var held_force := right_force if right_hand else left_force
	var absent_force := left_force if right_hand else right_force
	var absent_point: Vector3 = h._native().get_traversal_left_hand() if right_hand else h._native().get_traversal_right_hand()
	return int(h._native().get_traversal_state()) in [1, 4] \
		and int(h._native().get_traversal_support_entity_id()) == 2952 \
		and int(state.get("traversal_hand_constraint_count", -1)) == 1 \
		and bool(state.get("right_hand_attached" if right_hand else "left_hand_attached", false)) \
		and not bool(state.get("left_hand_attached" if right_hand else "right_hand_attached", true)) \
		and bool(state.get("hanging_free_arc_active", false)) \
		and float(state.get("player_gravity_factor", -1.0)) == 1.0 \
		and held_force.is_finite() and held_force.length() <= 1501.0 \
		and absent_force.is_finite() and absent_force.length() < 0.001 \
		and absent_point == Vector3.ZERO


func _single_hand_arms_honest(right_hand: bool = false) -> bool:
	var arms = h._main._arms
	var camera: Transform3D = h.get_viewport().get_camera_3d().global_transform
	for hand in arms._hands:
		if (hand.side < 0.0) == right_hand:
			var shoulder: Vector3 = 2.0 * hand.upper.global_position - hand.elbow.global_position
			var ordinary: Vector3 = camera * Vector3(arms.SHOULDER_LOCAL.x * hand.side, arms.SHOULDER_LOCAL.y, arms.SHOULDER_LOCAL.z)
			if hand.anchored or hand.pose != arms.POSE_AIR \
					or shoulder.distance_to(ordinary) > arms.MAX_LEAN + 0.002 \
					or hand.root.global_position.distance_to(camera.origin) > 1.2:
				return false
		else:
			var climbing := int(h._native().get_traversal_state()) == 4
			var hold: Vector3 = h._native().get_traversal_right_hand_render_position() if right_hand else h._native().get_traversal_left_hand_render_position()
			var expected: Vector3 = hold - arms._grip_forward * (0.045 if climbing else arms.TOP_PROBE_INSET + 0.034) \
				+ Vector3.DOWN * (0.035 if climbing else 0.074)
			if not hand.anchored or hand.pose != (arms.POSE_CLIMB if climbing else arms.POSE_GRIP) \
					or hand.anchor.distance_to(expected) > 0.002 \
					or hand.world_wrist.distance_to(expected) > 0.04:
				return false
			# Check rendered IK against its actual reach clamp, not just the target.
			var shoulder: Vector3 = 2.0 * hand.upper.global_position - hand.elbow.global_position
			var to_wrist: Vector3 = hand.world_wrist - shoulder
			var reach := clampf(to_wrist.length(), absf(arms.UPPER_ARM - arms.FOREARM) + 0.01,
				arms.UPPER_ARM + arms.FOREARM - 0.004)
			var rendered_expected := shoulder + to_wrist.normalized() * reach
			if hand.root.global_position.distance_to(rendered_expected) > 0.002:
				return false
	return true


# Read the real held context through both presentation paths. Switching the
# HUD renderer synchronously does not alter input or native physical state.
func _single_hand_controls_honest(posture_label: String) -> bool:
	var ctx := h._ctx()
	if (not bool(ctx.get("single_hand_hold", false)) or ctx["action"]["id"] != &"reach_hand"
			or ctx["action"]["label"] != "REACH HAND"
			or not h._main._touch.is_button_shown(&"action")
			or bool(h._main._touch._buttons[&"action"].enabled) != bool(ctx.get("missing_hand_grip_available", false))
			or not h._main._touch.is_button_shown(&"jump")
			or not h._main._touch.is_button_shown(&"drop")
			or h._main._touch.button_label(&"jump") != "LET GO"
			or h._main._touch.button_label(&"drop") != "LET GO"
			or h._main._touch.button_label(&"crouch") != posture_label):
		return false
	var hud = h._main._hud
	var was_touch: bool = hud.touch_active
	hud.touch_active = false
	var prompts: Array = hud._build_prompts(ctx)
	hud.touch_active = was_touch
	var release_prompt := false
	var posture_prompt := false
	var reach_prompt := false
	for prompt in prompts:
		var wording := String(prompt["text"]) + " " + String(prompt["detail"])
		if "CLIMB UP" in wording or "JUMP OFF" in wording or "MOVE TO LEAN" in wording:
			return false
		reach_prompt = reach_prompt or (prompt["verb"] == &"action" and prompt["text"] == "REACH HAND")
		release_prompt = release_prompt or (prompt["verb"] in [&"jump", &"drop"] and prompt["text"] == "LET GO")
		posture_prompt = posture_prompt or (prompt["verb"] == &"crouch" and prompt["text"] == posture_label)
	return release_prompt and posture_prompt and reach_prompt


func _single_hand_feedback_quiet(right_hand: bool, generation: int, count: int, playbacks: int) -> bool:
	var key := "right_hand_generation" if right_hand else "left_hand_generation"
	return int(h._native().get_landing_state().get(key, 0)) == generation \
		and int(h._main._audio.get_grip_feedback_stats()["climb_regrips"]) == count \
		and int(h._main._audio.get_grip_feedback_stats()["grab_playbacks"]) == playbacks


func _hanging_posture_hud_matches(label: String) -> bool:
	var hud = h._main._hud
	var was_touch: bool = hud.touch_active
	hud.touch_active = false
	var prompts: Array = hud._build_prompts(h._ctx())
	hud.touch_active = was_touch
	for prompt in prompts:
		if prompt["verb"] == &"crouch" and prompt["text"] == label:
			return true
	return false


func _touch_hanging_posture() -> bool:
	var before: Dictionary = h._native().get_landing_state()
	if not bool(before.get("swing_tuck_available", false)) \
			or bool(before.get("swing_tuck_requested", true)) \
			or not h._main._touch.is_button_shown(&"crouch") \
			or h._main._touch.button_label(&"crouch") != "TUCK" \
			or not _hanging_posture_hud_matches("TUCK"):
		return h._fail("actual hanging hands did not expose the posture control")
	# A free arc changes world-Y projection without changing arm span. Measure
	# the real native radial distances when active, retaining literal Y readback.
	var radial := bool(before.get("hanging_free_arc_active", false))
	if not radial:
		return h._fail("neutral hanging hands did not enter the shared passive arc")
	var target_key := "hanging_commanded_span_m" if radial else "swing_commanded_body_below_grip_m"
	var body_key := "hanging_actual_span_m" if radial else "swing_body_below_grip_m"
	var target_before := float(before[target_key])
	var body_before := float(before[body_key])
	var work_before := float(before["hand_actuator_positive_work_j"])
	h._tap(2, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		return bool(state.get("swing_tuck_requested", false)) \
			and float(state.get(target_key, target_before)) < target_before - 0.20 \
			and float(state.get(body_key, body_before)) < body_before - 0.10, 4.0):
		return h._fail("touch TUCK did not move the actual body and bounded hand target")
	var tucked: Dictionary = h._native().get_landing_state()
	var left: Vector3 = h._native().get_traversal_left_hand()
	var right: Vector3 = h._native().get_traversal_right_hand()
	var measured_below := 0.5 * (left.y + right.y) - h._position().y
	var measured_span := 0.5 * (left.distance_to(h._position()) + right.distance_to(h._position()))
	var left_force: Vector3 = tucked["left_hand_force_n"]
	var right_force: Vector3 = tucked["right_hand_force_n"]
	if absf(measured_below - float(tucked["swing_body_below_grip_m"])) > 0.001 \
			or (radial and absf(measured_span - float(tucked[body_key])) > 0.001) \
			or float(tucked["hand_actuator_positive_work_j"]) <= work_before \
			or left_force.length() > 1501.0 or right_force.length() > 1501.0 \
			or int(tucked["traversal_hand_constraint_count"]) != 2 \
			or float(tucked["player_gravity_factor"]) != 1.0 or bool(h._native().is_player_crouched()):
		return h._fail("touch TUCK lacks real finite hands, paid work, or actual COM readback")
	var target_tucked := float(tucked[target_key])
	if h._main._touch.button_label(&"crouch") != "EXTEND" or not _hanging_posture_hud_matches("EXTEND"):
		return h._fail("native tuck request did not update the EXTEND control")
	h._tap(2, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		return not bool(state.get("swing_tuck_requested", true)) \
			and float(state.get(target_key, target_tucked)) > target_tucked + 0.20, 4.0):
		return h._fail("touch EXTEND did not restore the native hand target")
	if not _hanging_posture_hud_matches("TUCK"):
		return h._fail("actual hanging extension did not restore the HUD TUCK prompt")
	print("SCRAPERX_HANGING_POSTURE touch_tuck_extend=1 free_arc=%s actual_posture_change_m=%f commanded_change_m=%f positive_target_work_j=%f" % [
		radial, body_before - float(tucked[body_key]), target_before - target_tucked,
		float(tucked["hand_actuator_positive_work_j"]) - work_before])
	return true


func _touch_crown_swing_recovery() -> bool:
	if not await _touch_crown_swing_catch():
		return false
	var device := InputRouter.Device.TOUCH
	h._tap(2, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool:
		return bool(h._native().get_landing_state().get("swing_tuck_requested", false)), 0.2):
		return h._fail("recovery did not acquire the touch posture toggle")
	var miss_tick := int(h._native().get_tick_index())
	var miss_available := false
	while float(int(h._native().get_tick_index()) - miss_tick) * float(h._native().get_fixed_step_seconds()) < 10.0:
		if not _crown_swing_attached():
			return h._fail("crown recovery miss lost its hanging support before release")
		var velocity := h._velocity()
		if h._position().x > -36.4 and velocity.x > 0.0 and velocity.x < 2.5:
			miss_available = true
			break
		await h.get_tree().process_frame
	if not miss_available:
		return h._fail("crown recovery miss never reached its low-speed release window")
	# Release over the gap with insufficient forward speed to reach the receiver.
	h._move(device, 0.0)
	# End real attachment ownership without going through presentation's Jump
	# or Drop cleanup. The same owner transition occurs on native auto-detach.
	h._native().request_release()
	if not await h._wait_until(func() -> bool:
		return not _crown_swing_attached() and not h._main._crouch_toggled \
			and not h._main._crouch_toggle_from_hang, 0.5):
		return h._fail("native detach retained the presentation posture toggle")
	if not await h._wait_until(func() -> bool:
		return not bool(h._native().get_landing_state()["player_swinging"]) \
			and bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 1931, 5.0):
		return h._fail("crown missed release did not reach the real recovery tray at=%s" % h._position())
	# A real impact may briefly compact the body for native brace/roll recovery.
	# Cleared input must permit ordinary standing once that recovery ends;
	# first supported contact can precede the next posture update by one tick.
	var recovery_start_tick := int(h._native().get_tick_index())
	while bool(h._native().is_player_crouched()) or float(h._native().get_landing_state().get("landing_recovery_seconds", 0.0)) > 0.0:
		if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 1931 \
				or _crown_swing_attached() or h._main._crouch_toggled or h._main._crouch_toggle_from_hang \
				or int(h._native().get_landing_state().get("traversal_hand_constraint_count", -1)) != 0:
			return h._fail("released posture recovery lost footing or retained hanging input")
		if float(int(h._native().get_tick_index()) - recovery_start_tick) * float(h._native().get_fixed_step_seconds()) > 2.5:
			return h._fail("released posture remained crouched after native impact recovery")
		await h.get_tree().process_frame
	if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 1931 \
			or _crown_swing_attached() or h._main._crouch_toggled or h._main._crouch_toggle_from_hang \
			or int(h._native().get_landing_state().get("traversal_hand_constraint_count", -1)) != 0:
		return h._fail("released posture recovery ended without clear input and real footing")
	print("SCRAPERX_HANGING_POSTURE native_detach_toggle_clear=1 recovery_support=1931 ground_crouch=0")
	if not await h._walk_to(device, Vector2(-37.0, -159.8), 0.08, 7.0):
		return h._fail("crown recovery ladder approach at=%s" % h._position())
	await h._face(Vector2(-1, 0))
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("crown recovery ladder CLIMB was not offered")
	h._act(device)
	await h._frames(2)
	if int(h._native().get_traversal_state()) != h._main.TRAVERSAL_CLIMBING \
			or int(h._native().get_traversal_support_entity_id()) != 1931:
		return h._fail("crown recovery ladder did not acquire native climbing hands")
	h._move(device, 1.0)
	var recovered := await h._wait_until(func() -> bool:
		return h._standing_above(198.7) and int(h._native().get_support_entity_id()) == 1985 \
			and not bool(h._native().get_landing_state()["player_swinging"]), 35.0)
	h._move(device, 0.0)
	if not recovered or not h._campaign_world_intact():
		return h._fail("crown physical recovery climb did not reach the start platform at=%s" % h._position())
	await h._seconds(1.0)
	if not await h._walk_to(device, Vector2(-41.0759, -160.45), 0.08, 7.0):
		return h._fail("crown recovered retry approach at=%s" % h._position())
	return true


func _crown_swing_attached() -> bool:
	return bool(h._native().get_landing_state()["player_swinging"]) \
		and int(h._native().get_traversal_state()) == h._main.TRAVERSAL_CLIMBING \
		and int(h._native().get_traversal_support_entity_id()) == 2930


func _touch_crown_upper_recall() -> bool:
	if not await h._walk_to(InputRouter.Device.TOUCH, Vector2(-26.7379, -160.1), 0.08, 7.0):
		return h._fail("crown far-side upper call approach at=%s" % h._position())
	var before: Dictionary = h._native().get_supplied_machine_state()
	if int(before["index"]) != 1 or int(before["station"]) != 3 \
			or not await h._offered(&"operate", "OPERATE", "CROWN GONDOLA"):
		return h._fail("crown far-side upper call station is not reachable")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_down"):
		return h._fail("crown upper call touch pendant did not open")
	h._touch(1, h._center(&"pendant_down"), true)
	var lowered := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) < 197.9, 12.0)
	h._touch(1, h._center(&"pendant_down"), false)
	await h._seconds(0.5)
	var after: Dictionary = h._native().get_supplied_machine_state()
	if not lowered or int(after["index"]) != 1 or not bool(after["braking"]) \
			or float(after["energy_j"]) < 0.0 or float(after["energy_j"]) >= float(before["energy_j"]) \
			or int(h._native().get_support_entity_id()) != 1985 or not bool(h._ctx()["grounded"]):
		return h._fail("crown far-side recall did not move and brake from fixed receiver footing")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("crown upper call DONE did not release operation")
	return true


func _wait_crown_boarded(cabin: int) -> bool:
	var start_tick := int(h._native().get_tick_index())
	var stable_tick := -1
	var step := float(h._native().get_fixed_step_seconds())
	while float(int(h._native().get_tick_index()) - start_tick) * step < 3.0:
		var tick := int(h._native().get_tick_index())
		var pose: Transform3D = h._native().get_kit_body_transform(cabin)
		var slip: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
		# The control panel belongs to cabin2983 too. Its top can provide
		# transient support; require the actual floor height and settled feet.
		var ready := bool(h._native().is_player_grounded()) \
			and int(h._native().get_support_entity_id()) == 2983 \
			and absf(h._position().y - (pose.origin.y + 1.15)) <= 0.15 \
			and Vector2(slip.x, slip.z).length() < 0.15
		if ready:
			if stable_tick < 0:
				stable_tick = tick
			if float(tick - stable_tick) * step >= 0.3:
				return true
		else:
			stable_tick = -1
		await h.get_tree().process_frame
	return false


func _crown_ride_diagnostic(cabin: int, phase: String) -> void:
	var pose: Transform3D = h._native().get_kit_body_transform(cabin)
	var landing: Dictionary = h._native().get_landing_state()
	var support_velocity: Vector3 = h._native().get_support_point_linear_velocity()
	var move: Vector2 = h._main._touch.move_vector
	var forward := Vector2(-sin(h._main._yaw), -cos(h._main._yaw))
	var right := Vector2(cos(h._main._yaw), -sin(h._main._yaw))
	print("SCRAPERX_CROWN_RIDE phase=%s tick=%d player=%s velocity=%s grounded=%s crouched=%s support=%d support_velocity=%s slip=%s traversal=%d traversal_support=%d left_hand=%s right_hand=%s gravity=%s hands=%s landing=%s cabin_origin=%s cabin_rotation=%s cabin_velocity=%s touch_move=%s world_move=%s pendant=%s operating=%s machine=%s" % [
		phase, h._native().get_tick_index(), h._position(), h._velocity(), h._native().is_player_grounded(),
		h._native().is_player_crouched(), h._native().get_support_entity_id(), support_velocity,
		h._velocity() - support_velocity, h._native().get_traversal_state(),
		h._native().get_traversal_support_entity_id(), h._native().get_traversal_left_hand(),
		h._native().get_traversal_right_hand(), landing.get("player_gravity_factor", "not_exposed"),
		landing.get("traversal_hand_constraint_count", "not_exposed"), landing, pose.origin,
		pose.basis.get_euler(), h._native().get_kit_body_linear_velocity(cabin), move,
		right * move.x + forward * move.y, h._main._touch.pendant_axes, h._main._operating,
		h._native().get_supplied_machine_state()])


func _wait_crown_settled(cabin: int) -> bool:
	var waited := 0.0
	var settled := 0.0
	while waited < 20.0:
		var pose: Transform3D = h._native().get_kit_body_transform(cabin)
		var state: Dictionary = h._native().get_supplied_machine_state()
		var ready := bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 2983 \
			and bool(state["braking"]) and float(state["surface_y"]) >= 197.97 \
			and (h._native().get_kit_body_linear_velocity(cabin) as Vector3).length() < 0.15 \
			and pose.basis.y.dot(Vector3.UP) > 0.995
		settled = settled + h.get_process_delta_time() if ready else 0.0
		if settled >= 0.5:
			return true
		await h.get_tree().process_frame
		waited += h.get_process_delta_time()
	return false
