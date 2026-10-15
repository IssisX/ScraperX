extends RefCounted

# West-brace and north-service frame handholds and diagnostics.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const OpeningSuite := preload("res://presentation/ui/test_scenarios/opening_scenarios.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

var h: Harness
var _opening_suite: OpeningSuite


func _init(harness: Harness, opening: OpeningSuite) -> void:
	h = harness
	_opening_suite = opening


func _touch_west_brace_bay() -> bool:
	const ROUTE_ENTITY := 1934
	var device := InputRouter.Device.TOUCH
	if not h._campaign_world_intact():
		return false
	if int(h._native().get_entity_body_count(ROUTE_ENTITY)) < 1:
		return h._fail("west brace bay entity1934 is absent from the normal world")
	# This is the scenario's only debug staging point. Everything after it uses
	# the ordinary viewport touch buttons and stick against native geometry.
	if not bool(h._native().debug_restart_at(Vector3(-24.70, 375.15, -139.5))):
		return h._fail("supported west brace bay lower-deck staging was rejected")
	if not await _touch_face(Vector2(1.0, 0.0)):
		return h._fail("ordinary look-thumb touch could not face the authored +X approach")
	if not await h._wait_until(func() -> bool:
		return bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == 51 \
			and absf(h._position().y - 375.15) < 0.15, 2.0):
		return h._fail("lower approach did not settle on WorldSolid51 floor374.25 at=%s support=%d" % [
			h._position(), h._native().get_support_entity_id()])
	if not await h._wait_until(func() -> bool:
		return bool(h._native().is_grip_available()) and bool(h._ctx()["grounded"]), 2.0):
		return h._fail("first diagonal hold is not physically reachable from the lower deck at=%s" % h._position())
	if not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("grounded first hold did not offer the ordinary touch CLIMB action")
	h._act(device)
	if not await h._wait_until(func() -> bool:
		return int(h._native().get_traversal_state()) == h._main.TRAVERSAL_CLIMBING \
			and int(h._native().get_traversal_support_entity_id()) == ROUTE_ENTITY \
			and int(h._native().get_landing_state()["traversal_hand_constraint_count"]) == 2, 1.0):
		return h._fail("touch CLIMB did not engage both physical hands on entity1934")
	# Facing +X, forward + screen-right moves toward +X/+Z along the first rail.
	h._move_dir(device, Vector2(0.55, 1.0))
	# Release ordinary climb input on actual receiving contact before asking
	# for settled speed, so the continued stick cannot walk off the narrow shelf.
	var first_contact := await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		return bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == ROUTE_ENTITY \
			and int(h._native().get_traversal_state()) == 0 and int(state["traversal_hand_constraint_count"]) == 0 \
			and int(state["foot_transfer_support_entity_id"]) == ROUTE_ENTITY \
			and float(state["foot_transfer_peak_hand_load_n"]) > 0.0 and absf(h._position().y - 382.8) < 0.10, 15.0)
	h._move_dir(device, Vector2.ZERO)
	var first_shelf := first_contact and await h._wait_until(func() -> bool:
		return _west_brace_shelf(381.9, -24.25, -22.70, -138.55, -136.85), 2.0)
	if not first_shelf:
		return h._fail("first diagonal did not top out on the inboard shelf381.9 at=%s support=%d traversal=%d" % [
			h._position(), h._native().get_support_entity_id(), h._native().get_traversal_state()])
	await h._seconds(0.2)
	if not _west_brace_shelf(381.9, -24.25, -22.70, -138.55, -136.85):
		return h._fail("inboard first-shelf footing was not stable after releasing the touch stick")
	# Approach the bar on the real shelf before the neutral airborne timing
	# choice; the first diagonal top-out alone leaves its jump apex out of reach.
	if not await h._walk_to(device, Vector2(-23.70, h._position().z), 0.06, 5.0, true):
		return h._fail("ordinary supported bar approach did not settle on the first shelf at=%s" % h._position())
	if not _west_brace_shelf(381.9, -24.25, -22.70, -138.55, -136.85):
		return h._fail("bar approach lost stable first-shelf footing at=%s" % h._position())
	if not await _touch_face(Vector2(-1.0, 0.0)):
		return h._fail("ordinary look-thumb touch could not turn toward the transverse bar")

	# The shelf is inboard of the first rail. Face -X, jump toward the real bar,
	# and wait for its native catch affordance before using ordinary Action.
	h._move_dir(device, Vector2(0.0, 1.0))
	h._tap(1, h._center(&"jump"))
	# A process-frame signal alone can resume before Main consumes the queued
	# Jump. Keep its ordinary direction until native physics confirms takeoff.
	if not await h._wait_until(func() -> bool:
		return not bool(h._native().is_player_grounded()) and h._velocity().y > 0.2, 0.75):
		h._move_dir(device, Vector2.ZERO)
		return h._fail("ordinary touch Jump did not produce real upward takeoff at=%s velocity=%s" % [
			h._position(), h._velocity()])
	h._move_dir(device, Vector2.ZERO)
	var apex := {"position": h._position(), "velocity": h._velocity(), "grip": false,
		"ledge": false, "action": h._ctx()["action"]}
	var catch_ready := await h._wait_until(func() -> bool:
		if h._position().y > (apex["position"] as Vector3).y:
			apex["position"] = h._position()
			apex["velocity"] = h._velocity()
			apex["grip"] = bool(h._native().is_grip_available())
			apex["ledge"] = bool(h._native().is_ledge_available())
			apex["action"] = h._ctx()["action"]
		return not bool(h._ctx()["grounded"]) and h._ctx()["action"]["id"] == &"catch" \
			and h._position().y >= 383.0 and absf(h._velocity().y) <= 1.0, 3.0)
	if not catch_ready:
		h._move_dir(device, Vector2.ZERO)
		return h._fail("airborne transverse-bar CATCH was not offered near the jump apex at=%s velocity=%s ledge=%s action=%s apex=%s" % [
			h._position(), h._velocity(), h._native().is_ledge_available(), h._ctx()["action"], apex])
	if not await h._offered(&"catch", "CATCH", "HOLD ON"):
		h._move_dir(device, Vector2.ZERO)
		return h._fail("native reachable bar affordance did not enable the touch CATCH button")
	# Forward now points toward -X and the bar. Action only requests traversal;
	# native contact, reach and load rules decide whether the catch succeeds.
	h._move_dir(device, Vector2(0.0, 1.0))
	h._act(device)
	var bar_caught := await h._wait_until(func() -> bool:
		return int(h._native().get_traversal_state()) == h._main.TRAVERSAL_CLIMBING \
			and int(h._native().get_traversal_support_entity_id()) == ROUTE_ENTITY \
			and int(h._native().get_landing_state()["traversal_hand_constraint_count"]) == 2, 0.8)
	h._move_dir(device, Vector2.ZERO)
	if not bar_caught:
		return h._fail("ordinary Jump plus apex Action failed to catch the transverse bar with real hands; traversal=%d support=%d at=%s" % [
			h._native().get_traversal_state(), h._native().get_traversal_support_entity_id(), h._position()])
	await h._pose("west_brace_bar_catch")

	# Facing -X, screen-left is world +Z. Use the native route's bounded
	# 90-tick traverse and 20-tick neutral hold; real anchors prove the crossing.
	var before_left: Vector3 = h._native().get_traversal_left_hand()
	var before_right: Vector3 = h._native().get_traversal_right_hand()
	var before_hand_z := 0.5 * (before_left.z + before_right.z)
	var traverse_start_tick := int(h._native().get_tick_index())
	h._move_dir(device, Vector2(-1.0, 0.0))
	while int(h._native().get_tick_index()) - traverse_start_tick < 90:
		await h.get_tree().process_frame
		if not _west_brace_hands():
			h._move_dir(device, Vector2.ZERO)
			_west_brace_diagnostic("bar_traverse_lost_hands")
			return h._fail("ordinary bounded bar traverse lost physical hand support")
	h._move_dir(device, Vector2.ZERO)
	var neutral_start_tick := int(h._native().get_tick_index())
	while int(h._native().get_tick_index()) - neutral_start_tick < 20:
		await h.get_tree().process_frame
		if not _west_brace_hands():
			_west_brace_diagnostic("bar_neutral_lost_hands")
			return h._fail("neutral bar hold lost physical hand support")
	var after_left: Vector3 = h._native().get_traversal_left_hand()
	var after_right: Vector3 = h._native().get_traversal_right_hand()
	var anchor_movement := 0.5 * (after_left.z + after_right.z) - before_hand_z
	_west_brace_diagnostic("bar_traverse_complete")
	if anchor_movement < 0.12 or anchor_movement > 2.2:
		return h._fail("ordinary screen-left crossing did not move real bar anchors within the native traverse bounds; delta_z=%.6f" % anchor_movement)

	# Facing -X, forward + screen-right now follows the diagonal toward world
	# -X/-Z and tops out on its outboard recovery shelf.
	var second_transfer_before := int(h._native().get_landing_state()["foot_transfer_count"])
	h._move_dir(device, Vector2(0.50, 1.0))
	var second_shelf := false
	var second_elapsed := 0.0
	while second_elapsed < 15.0:
		await h.get_tree().process_frame
		second_elapsed += h.get_process_delta_time()
		var second_state: Dictionary = h._native().get_landing_state()
		var second_position := h._position()
		if int(second_state["foot_transfer_count"]) == second_transfer_before + 1 \
				and int(second_state["foot_transfer_support_entity_id"]) == ROUTE_ENTITY \
				and float(second_state["foot_transfer_peak_hand_load_n"]) > 0.0 \
				and bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == ROUTE_ENTITY \
				and int(h._native().get_traversal_state()) == 0 and int(second_state["traversal_hand_constraint_count"]) == 0 \
				and float(second_state["player_gravity_factor"]) == 1.0 and absf(second_position.y - 390.3) < 0.10 \
				and second_position.x >= -25.35 and second_position.x <= -24.60 \
				and second_position.z >= -138.90 and second_position.z <= -137.10:
			# Completed hand support is now real footing. Release climb input
			# before demanding settled velocity, so it cannot walk off the rest.
			h._move_dir(device, Vector2.ZERO)
			_west_brace_diagnostic("second_foot_receipt")
			second_shelf = await h._wait_until(func() -> bool:
				return _west_brace_shelf(389.4, -25.35, -24.60, -138.90, -137.10), 2.0)
			break
		# Permit the real receiving contact to settle, but stop on an actual
		# release rather than continuing input through a long unsupported fall.
		var settling := bool(h._native().is_player_grounded()) \
			and int(h._native().get_support_entity_id()) == ROUTE_ENTITY \
			and int(h._native().get_traversal_state()) == 0 and absf(h._position().y - 390.3) < 0.10
		if (not _west_brace_hands() and not settling) or int(h._native().get_death_count()) != 0 \
				or float(h._native().get_landing_state()["player_gravity_factor"]) != 1.0:
			h._move_dir(device, Vector2.ZERO)
			_west_brace_diagnostic("reverse_ascent_lost_support")
			return h._fail("reverse ascent lost real hands before the physical389.4 shelf")
	h._move_dir(device, Vector2.ZERO)
	if not second_shelf:
		return h._fail("reverse diagonal did not top out on the outboard shelf389.4 at=%s support=%d traversal=%d" % [
			h._position(), h._native().get_support_entity_id(), h._native().get_traversal_state()])
	await h._seconds(0.2)
	if not _west_brace_shelf(389.4, -25.35, -24.60, -138.90, -137.10):
		return h._fail("outboard second-shelf footing was not stable after releasing the touch stick")
	# The final line now starts at z=-137.70, so move across the shelf toward
	# that reachable hold while still facing -X (screen-left is world +Z).
	# Brake through ordinary touch input before stopping on this narrow rest.
	var final_grip_approach := await h._walk_to(device, Vector2(h._position().x, -138.0), 0.06, 5.0, true)
	if not final_grip_approach:
		_west_brace_diagnostic("final_grip_approach_failed")
		return h._fail("ordinary supported touch braking did not reach the final-grip approach at z=-138.0; at=%s" % h._position())
	await h._seconds(0.2)
	if not _west_brace_shelf(389.4, -25.35, -24.60, -138.25, -137.75):
		return h._fail("final-grip approach did not settle on stable support1934 footing near z=-138.0 at=%s" % h._position())
	if not await _touch_face(Vector2(1.0, 0.0)):
		return h._fail("ordinary look-thumb touch could not face the final diagonal")
	if not await h._wait_until(func() -> bool:
		return bool(h._native().is_grip_available()) and bool(h._ctx()["grounded"]), 2.0):
		return h._fail("final diagonal handhold was not natively reachable from the outboard shelf at=%s" % h._position())
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("reachable final diagonal did not expose the ordinary touch CLIMB action")
	h._act(device)
	if not await h._wait_until(func() -> bool:
		return int(h._native().get_traversal_state()) == h._main.TRAVERSAL_CLIMBING \
			and int(h._native().get_traversal_support_entity_id()) == ROUTE_ENTITY \
			and int(h._native().get_landing_state()["traversal_hand_constraint_count"]) == 2, 1.0):
		return h._fail("touch Action did not engage the final diagonal's reachable real handholds")

	var accepted_before_final := int(h._native().get_accepted_traversal_count())
	var transfer_before_final: Dictionary = h._native().get_landing_state()
	var transfer_count_before_final := int(transfer_before_final["foot_transfer_count"])
	var final_start_tick := int(h._native().get_tick_index())
	var arrived := false
	var elapsed := 0.0
	h._move_dir(device, Vector2(-0.45, 1.0))
	while elapsed < 15.0:
		await h.get_tree().process_frame
		elapsed += h.get_process_delta_time()
		var transfer_state: Dictionary = h._native().get_landing_state()
		var traversal := int(h._native().get_traversal_state())
		if float(transfer_state["player_gravity_factor"]) != 1.0 \
				or traversal in [h._main.TRAVERSAL_MANTLING, h._main.TRAVERSAL_VAULTING] \
				or (traversal == h._main.TRAVERSAL_CLIMBING \
					and int(transfer_state["traversal_hand_constraint_count"]) != 2):
			h._move_dir(device, Vector2.ZERO)
			return h._fail("final transfer lost gravity-on physical hands at=%s traversal=%d native=%s" % [
				h._position(), traversal, transfer_state])
		if int(transfer_state["foot_transfer_count"]) == transfer_count_before_final + 1 \
				and int(transfer_state["foot_transfer_support_entity_id"]) == 51 \
				and bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == 51 \
				and traversal == 0 and int(transfer_state["traversal_hand_constraint_count"]) == 0 \
				and absf(h._position().y - 397.15) < 0.15:
			h._move_dir(device, Vector2.ZERO)
			_west_brace_diagnostic("upper_foot_receipt")
			arrived = await h._wait_until(func() -> bool:
				return _west_brace_stable(51, 397.15), 2.0)
			break
	h._move_dir(device, Vector2.ZERO)
	var final_transfer: Dictionary = h._native().get_landing_state()
	var final_peak_load := float(final_transfer["foot_transfer_peak_hand_load_n"])
	if not arrived \
			or int(final_transfer["foot_transfer_count"]) != transfer_count_before_final + 1 \
			or int(final_transfer["foot_transfer_support_entity_id"]) != 51 \
			or int(final_transfer["foot_transfer_tick"]) <= final_start_tick \
			or not is_finite(final_peak_load) or final_peak_load <= 0.0 \
			or int(h._native().get_accepted_traversal_count()) <= accepted_before_final:
		return h._fail("final hand-to-foot transfer did not complete onto WorldSolid51 floor396.25 at=%s support=%d traversal=%d native=%s" % [
			h._position(), h._native().get_support_entity_id(), h._native().get_traversal_state(), final_transfer])
	var upper_hold_start := int(h._native().get_tick_index())
	while int(h._native().get_tick_index()) - upper_hold_start < 270:
		await h.get_tree().process_frame
		if not _west_brace_stable(51, 397.15) or int(h._native().get_death_count()) != 0:
			return h._fail("upper WorldSolid51 footing lost during three-second neutral hold at=%s velocity=%s support=%d" % [
				h._position(), h._velocity(), h._native().get_support_entity_id()])
	await h._pose("west_brace_existing_floor396_25")
	var exit := h._position()
	if not await h._walk_to(device, Vector2(-22.5, -142.0), 0.14, 8.0, true) \
			or not _west_brace_stable(51, 397.15) or h._position().distance_to(exit) < 1.0 \
			or not h._campaign_world_intact():
		return h._fail("upper WorldSolid51 floor lacks stable ordinary onward walking at=%s support=%d" % [
			h._position(), h._native().get_support_entity_id()])
	h._detail = "lower374_25=1 first_diagonal_shelf381_9=1 jump_apex_action_catch=1 transverse_under_cap=1 reverse_shelf389_4=1 final_hand_transfer396_25=1 foot_transfer=1 upper_hold_ticks=270 gravity=1 onward_walk_m=%.2f touch=1 support=51 deaths=0" % h._position().distance_to(exit)
	return true


func _west_brace_hands() -> bool:
	var state: Dictionary = h._native().get_landing_state()
	return int(h._native().get_traversal_state()) == h._main.TRAVERSAL_CLIMBING \
		and int(h._native().get_traversal_support_entity_id()) == 1934 \
		and int(state["traversal_hand_constraint_count"]) == 2 \
		and float(state["player_gravity_factor"]) == 1.0 and int(h._native().get_death_count()) == 0 \
		and h._position().is_finite() and h._velocity().is_finite()


func _west_brace_diagnostic(phase: String) -> void:
	print("SCRAPERX_WEST_BRACE phase=%s tick=%d position=%s velocity=%s support=%d traversal=%d traversal_support=%d left_hand=%s right_hand=%s yaw=%.6f touch_move=%s deaths=%d landing=%s" % [
		phase, h._native().get_tick_index(), h._position(), h._velocity(), h._native().get_support_entity_id(),
		h._native().get_traversal_state(), h._native().get_traversal_support_entity_id(),
		h._native().get_traversal_left_hand(), h._native().get_traversal_right_hand(), h._main._yaw,
		h._main._touch.move_vector, h._native().get_death_count(), h._native().get_landing_state()])


func _west_brace_shelf(top_y: float, min_x: float, max_x: float,
		min_z: float, max_z: float) -> bool:
	var relative: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
	var state: Dictionary = h._native().get_landing_state()
	var half_height := 0.6 if bool(h._native().is_player_crouched()) else 0.9
	var position := h._position()
	return bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == 1934 \
		and int(h._native().get_traversal_state()) == 0 and absf(position.y - (top_y + half_height)) < 0.10 \
		and int(state["traversal_hand_constraint_count"]) == 0 and float(state["player_gravity_factor"]) == 1.0 \
		and position.x >= min_x and position.x <= max_x and position.z >= min_z and position.z <= max_z \
		and absf(relative.y) < 0.2 and Vector2(relative.x, relative.z).length() < 0.3


func _west_brace_stable(support: int, standing_y: float) -> bool:
	var relative: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
	var state: Dictionary = h._native().get_landing_state()
	return bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == support \
		and int(h._native().get_traversal_state()) == 0 and absf(h._position().y - standing_y) < 0.15 \
		and int(state["traversal_hand_constraint_count"]) == 0 and float(state["player_gravity_factor"]) == 1.0 \
		and absf(relative.y) < 0.15 and Vector2(relative.x, relative.z).length() < 0.18


# Uses the ordinary second-thumb look area to face the route; unlike `_face`,
# this exercises the same viewport touch events as the player's camera input.
func _touch_face(direction: Vector2) -> bool:
	var goal := atan2(-direction.x, -direction.y)
	var radians_per_pixel := InputRouter.LOOK_RADIANS_PER_UNIT * float(h._main._router.look_sensitivity)
	if absf(radians_per_pixel) < 0.0001:
		return false
	var look_at := Vector2(h._viewport_size().x * 0.78, h._viewport_size().y * 0.45)
	var look_position := look_at
	var attempts := 0
	while attempts < 64:
		var remaining := wrapf(float(h._main._yaw) - goal, -PI, PI)
		if absf(remaining) <= 0.02:
			return true
		var delta_x := clampf(remaining / radians_per_pixel, -48.0, 48.0)
		h._touch(1, look_position, true)
		await h._frames(1)
		look_position += Vector2(delta_x, 0.0)
		h._drag(1, look_position, Vector2(delta_x, 0.0))
		await h._frames(1)
		h._touch(1, look_position, false)
		await h._frames(1)
		look_position = look_at
		attempts += 1
	return absf(wrapf(float(h._main._yaw) - goal, -PI, PI)) <= 0.02


func _north_climb(device: int, target_centre_y: float, label: String) -> bool:
	if not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("%s grip not offered at=%s support=%d grounded=%s crouched=%s traversal=%d grip=%s grip_point=%s ledge=%s carrying=%d action=%s view_yaw_rad=%.6f" % [
			label, h._position(), h._native().get_support_entity_id(), h._native().is_player_grounded(),
			h._native().is_player_crouched(), h._native().get_traversal_state(), h._native().is_grip_available(),
			h._native().get_grip_point(), h._native().is_ledge_available(), h._native().get_carrying_entity_id(), h._ctx()["action"], h._main._yaw])
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._ctx()["climbing"]), 1.0):
		return h._fail("%s grip not taken %s" % [label, h._position()])
	h._move(device, 1.0)
	var topped_out := await h._wait_until(func() -> bool: return h._standing_above(target_centre_y), 15.0)
	h._move(device, 0.0)
	if not topped_out or int(h._native().get_support_entity_id()) != 1901:
		return h._fail("%s top-out unsupported %s" % [label, h._position()])
	return true


func _touch_north_grip_diagnostic() -> bool:
	# Explicit supported staging at the recorded failed stance. This bounded
	# reach probe is not evidence of continuous ascent or an authored route.
	if not h._campaign_world_intact():
		return false
	if not h._native().debug_restart_at(Vector3(16.00084, 88.9, -179.4486)):
		return h._fail("north grip diagnostic supported staging rejected")
	await h._face(Vector2(0, -1))
	await h._seconds(0.5)
	print("SCRAPERX_NORTH_GRIP_DIAGNOSTIC before at=%s support=%d grounded=%s crouched=%s traversal=%d grip=%s grip_point=%s action=%s view_yaw_rad=%.6f" % [
		h._position(), h._native().get_support_entity_id(), h._native().is_player_grounded(),
		h._native().is_player_crouched(), h._native().get_traversal_state(), h._native().is_grip_available(),
		h._native().get_grip_point(), h._ctx()["action"], h._main._yaw])
	if h._native().is_grip_available():
		return h._fail("recorded north stance already offers native grip; reach hypothesis rejected")
	if not await h._walk_to(InputRouter.Device.TOUCH, Vector2(16, -179.55), 0.04, 2.0):
		return h._fail("north grip diagnostic closer stance unavailable")
	await h._seconds(0.5)
	print("SCRAPERX_NORTH_GRIP_DIAGNOSTIC after at=%s support=%d grounded=%s grip=%s grip_point=%s action=%s" % [
		h._position(), h._native().get_support_entity_id(), h._native().is_player_grounded(),
		h._native().is_grip_available(), h._native().get_grip_point(), h._ctx()["action"]])
	if not h._native().is_grip_available() or not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("closer supported stance does not expose native grip; reach hypothesis rejected")
	if not await _north_climb(InputRouter.Device.TOUCH, 94.7, "closer first north climb"):
		return false
	h._detail = "staging=supported_recorded_88m_stance continuous_ascent=0 native_grip_before=0 native_grip_after=1 closer_viewport_touch=1 supported_first_topout=1"
	return true


func _touch_north_service_frame() -> bool:
	if not await _opening_suite._touch_braced_bay():
		return false
	var device := InputRouter.Device.TOUCH
	if int(h._native().get_entity_body_count(1901)) != 1:
		return h._fail("north service frame absent from normal play")
	for point in [Vector2(24.8, -174.5), Vector2(16.0, -174.5)]:
		if not await h._walk_to(device, point, 0.14, 20.0):
			return h._fail("north frame +88 m entry %s" % h._position())
	# Settle at the intended rung stance so approach momentum does not
	# carry the player under the receiver fascia before the first climb.
	if not await h._walk_to(device, Vector2(16.0, -179.55), 0.04, 20.0, true):
		return h._fail("north frame +88 m grip stance %s" % h._position())
	await h._face(Vector2(0, -1))
	h._main._pitch = 0.24
	await h._pose("north_frame_entry")
	if not await _north_climb(device, 94.7, "first north climb"):
		return false
	await h._face(Vector2(-1, 0))
	h._main._pitch = -0.14
	await h._pose("north_frame_first_rest")
	for point in [Vector2(16.0, -181.2), Vector2(10.0, -181.2), Vector2(10.0, -179.55)]:
		if not await h._walk_to(device, point, 0.14, 9.0):
			return h._fail("north frame lateral transfer %s" % h._position())
	await h._face(Vector2(0, 1))
	if not await _north_climb(device, 98.2, "second north climb"):
		return false
	if not await h._walk_to(device, Vector2(10.0, -177.55), 0.14, 6.0):
		return h._fail("north frame +99 m mantle approach %s" % h._position())
	await h._face(Vector2(0, 1))
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("+99 m mantle not offered %s" % h._position())
	h._act(device)
	await h._seconds(1.0)
	if not await h._walk_to(device, Vector2(10.0, -174.5), 0.14, 5.0):
		return h._fail("north frame +99 m ring exit %s" % h._position())
	await h._seconds(0.4)
	if not h._standing_above(99.6) or int(h._native().get_support_entity_id()) != 11:
		return h._fail("north frame +99 m ring unsupported %s" % h._position())
	await h._face(Vector2(1, 0))
	h._main._pitch = -0.14
	await h._pose("north_frame_99m")
	# The former 0.14m stop could leave the hand search just outside the
	# rung. Walk into the intended stance; native reach and CLIMB stay exact.
	for point in [Vector2(16.0, -174.5), Vector2(16.0, -179.55)]:
		if not await h._walk_to(device, point, 0.04, 9.0):
			return h._fail("north frame upper entry %s" % h._position())
	await h._face(Vector2(0, -1))
	if not await _north_climb(device, 104.2, "upper north climb"):
		return false
	await h._face(Vector2(-1, 0))
	h._main._pitch = -0.14
	await h._pose("north_frame_upper_rest")
	if not await h._walk_to(device, Vector2(15.1, -180.6), 0.14, 5.0):
		return h._fail("raised lip takeoff %s" % h._position())
	await h._face(Vector2(-1, 0))
	await h._seconds(0.3)
	h._tap(1, h._center(&"jump"))
	h._move(device, 0.5)
	var caught := await h._wait_until(func() -> bool: return bool(h._ctx()["hanging"]), 2.0)
	h._move(device, 0.0)
	if not caught:
		return h._fail("raised north lip not caught %s" % h._position())
	h._main._pitch = 0.08
	await h._pose("north_frame_hang")
	# Facing west (yaw +PI/2), view-local right is world north (-Z).
	h._move_dir(device, Vector2(1.0, 0.0))
	var crossed := await h._wait_until(func() -> bool: return h._position().z < -181.85, 5.0)
	h._move_dir(device, Vector2.ZERO)
	if not crossed or not bool(h._ctx()["hanging"]):
		return h._fail("canopy shimmy missed clear pocket %s" % h._position())
	await h._pose("north_frame_shimmy")
	h._tap(1, h._center(&"jump"))
	if not await h._wait_until(func() -> bool: return h._standing_above(106.7), 2.0):
		return h._fail("north frame hanging top-out %s" % h._position())
	await h._face(Vector2(0, -1))
	h._main._pitch = -0.14
	await h._pose("north_frame_106m")
	if not await h._walk_to(device, Vector2(13.0, -182.75), 0.14, 5.0):
		return h._fail("final north ladder approach %s" % h._position())
	await h._face(Vector2(0, -1))
	if not await _north_climb(device, 109.2, "final north climb"):
		return false
	for point in [Vector2(15.5, -183.8), Vector2(15.5, -177.55)]:
		if not await h._walk_to(device, point, 0.14, 9.0, true):
			return h._fail("high catwalk to tower %s" % h._position())
	await h._face(Vector2(0, 1))
	h._main._pitch = -0.14
	await h._pose("north_frame_upper_exit")
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("+110 m mantle not offered %s" % h._position())
	h._act(device)
	await h._seconds(1.0)
	if not await h._walk_to(device, Vector2(15.5, -174.5), 0.14, 5.0):
		return h._fail("north frame +110 m ring exit %s" % h._position())
	await h._seconds(0.4)
	await h._face(Vector2(0, -1))
	h._main._pitch = -0.22
	await h._pose("north_frame_110m")
	if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 11 or \
			absf(h._position().y - 110.9) > 0.1 or int(h._native().get_death_count()) != 0:
		return h._fail("+110 m tower ring unsupported %s" % h._position())
	h._detail = "grade_to_110m=1 north_frame=1 hanging_shimmy=1 support=11 deaths=0 arrival_y=%.3f" % h._position().y
	return true
