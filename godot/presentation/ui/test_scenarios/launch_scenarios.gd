extends RefCounted

# Slingshot boarding, launch presentation and supported roof landing.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

var h: Harness


func _init(harness: Harness) -> void:
	h = harness


func _slingshot(device: int, supported_landing: bool = false) -> bool:
	if h._main._regression_scene or not bool(h._native().get_slingshot_state().get("available", false)):
		return h._fail("slingshot scenario did not select the shipping world")
	h._main._settings.head_bob = true
	h._main._head_bob_on = true
	h._main._settings.launch_cinematics = true
	# Explicit test staging: outside the pouch at ground level, three metres
	# toward the tower. Ordinary viewport movement then walks backwards
	# along +Z onto the actual leather floor; boarding never teleports.
	var station: Vector3 = h._native().get_slingshot_state()["pouch_position"]
	if not bool(h._native().debug_restart_at(Vector3(station.x, 0.92, station.z - 3.0))):
		return h._fail("outside-pouch staging was rejected")
	h._main._yaw = 0.0 # Face -Z, so local backwards moves +Z into the pouch.
	h._main._pitch = 0.0
	await h._frames(3)
	await h._pose("outside_pouch")
	h._move(device, -1.0)
	if not await h._wait_until(func() -> bool: return bool(h._native().get_slingshot_state()["station_available"]), 4.0):
		h._move(device, 0.0)
		var missed: Dictionary = h._native().get_slingshot_state()
		return h._fail("ordinary backwards movement did not enter leather pouch at %s pouch=%s recovering=%s" % [
			h._position(), missed["pouch_position"], missed["recovering"]])
	h._move(device, 0.0)
	await h._frames(2)
	var approached: Dictionary = h._native().get_slingshot_state()
	if not approached.has("aim_locked"):
		return h._fail("native aim lock did not cross the actual Godot bridge")
	if bool(approached["seated"]) or float(approached["energy_j"]) > 1.0 or float(approached["work_j"]) > 0.01:
		return h._fail("backward touch approach seated or charged before explicit BOARD")
	if not await h._offered(&"slingshot", "ENTER POUCH"):
		return h._fail("native pouch entry was not offered at %s" % h._position())
	var before_board := h._position()
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._native().get_slingshot_state()["seated"]), 0.5):
		return h._fail("device Action did not fasten native pouch harness")
	if h._position().distance_to(before_board) > 0.12:
		return h._fail("boarding displaced rider instead of fastening present pose")
	if not await h._offered(&"slingshot", "DRAW MORE"):
		return h._fail("unfunded shot gave no draw-more explanation")
	var uncharged_launches := int(h._native().get_slingshot_state()["launch_count"])
	h._act(device)
	await h._frames(3)
	if bool(h._native().get_slingshot_state().get("release_ready", true)) \
			or int(h._native().get_slingshot_state()["launch_count"]) != uncharged_launches:
		return h._fail("unfunded Action released below the physical guide-exit energy")
	if device == InputRouter.Device.TOUCH and not await _touch_rail_aim():
		return false
	await h._pose("seated_manual_draw")
	var draw_clock := 0.0
	var peak_power := 0.0
	var parameters: Dictionary = h._native().get_slingshot_state()
	var max_draw := float(parameters.get("max_draw_m", 12.0))
	var target_draw := max_draw - 0.025 # Within the native stop's finite clearance.
	var power_limit := float(parameters.get("max_source_power_w", 200000.0))
	h._move(device, -1.0)
	while draw_clock < 15.0:
		await h.get_tree().process_frame
		draw_clock += h.get_process_delta_time()
		var charging: Dictionary = h._native().get_slingshot_state()
		peak_power = maxf(peak_power, float(charging["source_power_w"]))
		if float(charging["draw_m"]) >= target_draw:
			break
	h._move(device, 0.0)
	await h._seconds(0.4) # The real carriage coasts into its one-way catch.
	var charged: Dictionary = h._native().get_slingshot_state()
	if bool(charged["aim_locked"]):
		return h._fail("funded manual draw incorrectly locked touch aiming")
	var held_draw := float(charged["draw_m"])
	var held_work := float(charged["work_j"])
	var energy := float(charged["energy_j"])
	if not bool(charged.get("release_ready", false)):
		return h._fail("funded full draw did not unlock native release")
	if held_draw < target_draw - 0.02 or energy < 40000.0 or held_work < energy:
		return h._fail("manual draw was not physically funded: draw=%.3f energy=%.1f work=%.1f" % [held_draw, energy, held_work])
	if not is_finite(peak_power) or peak_power < power_limit * 0.25 or peak_power > power_limit * 1.15 \
			or held_work > power_limit * (draw_clock + 0.4) * 1.03:
		return h._fail("manual source power/work outside declared budget: peak=%.1f work=%.1f time=%.2f" % [peak_power, held_work, draw_clock])
	await h._seconds(0.6)
	var held: Dictionary = h._native().get_slingshot_state()
	if absf(float(held["draw_m"]) - held_draw) > 0.03 or absf(float(held["work_j"]) - held_work) > 0.10:
		return h._fail("idle ratchet lost draw or accrued manual work")
	# A charged touch gesture must change both axes through native torque,
	# retain draw, then return to the nominal roof shot for landing proof.
	if device == InputRouter.Device.TOUCH and not await _touch_rail_aim(true):
		return false
	var aimed: Dictionary = h._native().get_slingshot_state()
	var rendered_seat: Dictionary = h._native().get_slingshot_render_state()
	if not rendered_seat.has("seat_surface_position") or not rendered_seat.has("rider_specific_acceleration"):
		return h._fail("measured rider load and leather seat did not cross the shipping bridge")
	var pelvis: Node3D = h._main._slingshot_view._avatar.get_node("HarnessPelvis")
	var seat_gap := (pelvis.global_transform * Vector3(0, -0.19, 0)).distance_to(rendered_seat["seat_surface_position"])
	if seat_gap > 0.003:
		return h._fail("charged rider is not cradled on actual rendered leather: gap_m=%.4f" % seat_gap)
	await h._pose("charged_adjustable_aim")
	if not await h._offered(&"slingshot", "RELEASE"):
		return h._fail("loaded Action did not offer RELEASE")
	var launches := int(aimed["launch_count"])
	h._act(device)
	if not await h._wait_until(func() -> bool: return int(h._native().get_slingshot_state()["launch_count"]) == launches + 1, 0.5):
		return h._fail("device Action did not remove native draw restraint")
	if not bool(h._native().get_slingshot_state()["released"]) or not h._main._slingshot_view.is_cinematic_active():
		return h._fail("physical release did not start cinematic")
	if h._main._read_context()["action"]["id"] == &"slingshot":
		return h._fail("released harness still offered a charging/release action")
	var launch_time := float(h._native().get_slingshot_state()["simulation_time_seconds"])
	var launch_position := h._position()
	var recoil_peak := 0.0
	var launch_clock := 0.0
	var capture_index := 0
	var capture_times := [0.12, 0.32, 0.62]
	while launch_clock < 0.62:
		await h.get_tree().process_frame
		launch_clock += h.get_process_delta_time()
		var rider_sample: Dictionary = h._native().get_slingshot_state()
		recoil_peak = maxf(recoil_peak, (rider_sample["rider_specific_acceleration"] as Vector3).length())
		if float(rider_sample["simulation_time_seconds"]) - launch_time < 0.04 \
				and h._main._slingshot_view.get_simulation_scale() < 0.99:
			return h._fail("camera slowed the initial native recoil before it developed")
		if capture_index < capture_times.size() and launch_clock >= capture_times[capture_index]:
			print("SCRAPERX_LAUNCH_EMBODIMENT wall_s=%.3f native_s=%.3f displacement_m=%.3f speed_mps=%.3f specific_mps2=%.3f torso_rad=%s head_rad=%s scale=%.3f" % [
				launch_clock, float(rider_sample["simulation_time_seconds"]) - launch_time,
				h._position().distance_to(launch_position), h._velocity().length(),
				(rider_sample["rider_specific_acceleration"] as Vector3).length(),
				h._main._slingshot_view._avatar.get_node("WorkJacket").rotation,
				h._main._slingshot_view._avatar.get_node("ExpressiveHead").rotation,
				h._main._slingshot_view.get_simulation_scale()])
			await h._pose("launch_embodiment_%d" % capture_index)
			capture_index += 1
	if recoil_peak < 100.0 or h._position().distance_to(launch_position) < 4.0:
		return h._fail("shipping release did not expose real recoil and displacement: peak=%.2f displacement=%.2f" % [recoil_peak, h._position().distance_to(launch_position)])
	var release_speed := h._velocity().length()
	if release_speed < 8.0 or not h._main._slingshot_view._avatar.visible or h._main._arms.visible:
		return h._fail("native spring release did not accelerate visible human rider: speed=%.2f" % release_speed)
	if not h._main._slingshot_view._hud.has_thought_bubble():
		return h._fail("actual rising release did not show its brief thought reaction")
	await h._pose("release_orbit")
	if not await h._wait_until(func() -> bool: return not h._main._slingshot_view.is_cinematic_active(), 3.0):
		return h._fail("orbital camera did not return after brief sequence")
	if h._main._slingshot_view.get_simulation_scale() != 1.0 or not h._main._arms.visible \
			or h._main._slingshot_view._avatar.visible or h._main._slingshot_view._hud.has_thought_bubble():
		return h._fail("cinematic return did not restore real time and first-person body")
	await h._pose("true_first_person_return")
	if supported_landing:
		h._detail = "staging=outside_pouch_then_walk draw_m=%.3f spring_kj=%.2f manual_kj=%.2f peak_kw=%.2f release_mps=%.2f" % \
			[held_draw, energy / 1000.0, held_work / 1000.0, peak_power / 1000.0, release_speed]
		return await _touch_launch_roof_landing()
	var apex := h._position().y
	var flight_clock := 0.0
	while flight_clock < 12.0:
		await h.get_tree().process_frame
		flight_clock += h.get_process_delta_time()
		apex = maxf(apex, h._position().y)
		if h._velocity().y < -1.0:
			break
	if apex < 250.0 or bool(h._native().get_slingshot_state()["seated"]) or h._velocity().y >= -1.0:
		return h._fail("real launch did not detach and reach useful tower height: apex=%.2f" % apex)
	await h._pose("native_flight_apex")
	# A second explicit staging relocation isolates the ground retrieval UI
	# after the measured flight. All retrieval/cancel/walking below again
	# travels through ordinary viewport input; no native request_* shortcuts.
	var neutral: Vector3 = h._native().get_slingshot_state().get("neutral_position", station)
	var control: Vector3 = h._native().get_slingshot_state().get("retrieval_control_position",
		neutral + Vector3(1.5, 0.35, -1.8))
	# Native control identifies the handwheel, not the capsule's standing
	# centre. Stand clear of the real pole at grade, inside its offer radius.
	var control_standing := Vector3(control.x + 0.85, neutral.y + 0.57, control.z - 0.4)
	if not bool(h._native().debug_restart_at(control_standing)):
		return h._fail("retrieval-control staging was rejected")
	h._main._slingshot_view.cancel_cinematic()
	h._main._yaw = 0.0
	h._main._pitch = 0.0
	await h._frames(3)
	if not await h._offered(&"slingshot", "RETRIEVE POUCH"):
		return h._fail("ground control did not offer physical pouch retrieval")
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._native().get_slingshot_state()["recovering"]), 0.5):
		return h._fail("retrieval Action did not engage native tether")
	await h._seconds(0.3)
	var recovering: Dictionary = h._native().get_slingshot_state()
	if absf(float(recovering["retrieval_work_j"])) > 0.10 or absf(float(recovering["retrieval_source_power_w"])) > 0.10:
		return h._fail("idle retrieval accrued unfunded manual work")
	if device == InputRouter.Device.TOUCH and not h._main._touch.is_button_shown(&"slingshot_reel"):
		return h._fail("retrieval has no dedicated hold-to-reel touch control")
	await h._pose("retrieval_idle_cancel_available")
	match device:
		InputRouter.Device.TOUCH:
			h._tap(1, h._center(&"drop"))
		InputRouter.Device.GAMEPAD:
			h._button(JOY_BUTTON_B)
		_:
			h._key(KEY_Q, true)
			h._key(KEY_Q, false)
	if not await h._wait_until(func() -> bool: return not bool(h._native().get_slingshot_state()["recovering"]), 0.5):
		return h._fail("retrieval Drop did not release native tether")
	var after_cancel := h._position()
	h._move(device, 1.0)
	await h._seconds(0.6)
	h._move(device, 0.0)
	var walked := Vector2(h._position().x - after_cancel.x, h._position().z - after_cancel.z).length()
	if walked < 0.4:
		return h._fail("retrieval cancel kept native walking locked: %.3f m" % walked)
	h._detail = "staging=outside_pouch_then_walk draw_m=%.3f spring_kj=%.2f manual_kj=%.2f peak_kw=%.2f release_mps=%.2f apex_m=%.2f" % \
		[held_draw, energy / 1000.0, held_work / 1000.0, peak_power / 1000.0, release_speed, apex]
	h._detail += " retrieval_cancel_walk_m=%.2f" % walked
	# From here every return/reboard/recharge input is ordinary viewport
	# input. No relocation, reset, or machine-state assignment closes the loop.
	if not await h._walk_to(device, Vector2(control_standing.x, control_standing.z), 0.15):
		return h._fail("ordinary walking could not return to the wheel")
	if not await h._offered(&"slingshot", "RETRIEVE POUCH"):
		return h._fail("return wheel is not reachable from a clear standing position")
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._native().get_slingshot_state()["recovering"]), 0.5):
		return h._fail("second retrieval attempt did not engage")
	await h._seconds(0.3)
	var retrieval_clock := 0.0
	var retrieval_peak := 0.0
	h._reel(device, true)
	await h._seconds(0.7)
	h._reel(device, false)
	await h._seconds(0.15)
	var paused_work := float(h._native().get_slingshot_state()["retrieval_work_j"])
	await h._seconds(0.3)
	if absf(float(h._native().get_slingshot_state()["retrieval_work_j"]) - paused_work) > 0.1:
		return h._fail("lifting the reel finger left manual work running: delta_j=%.3f power_w=%.3f touch_reel=%.1f touch_move=%s" % [float(h._native().get_slingshot_state()["retrieval_work_j"]) - paused_work, float(h._native().get_slingshot_state()["retrieval_source_power_w"]), h._main._touch.reel_effort, str(h._main._touch.move_vector)])
	h._reel(device, true)
	while bool(h._native().get_slingshot_state()["released"]) and retrieval_clock < 15.0:
		await h.get_tree().process_frame
		retrieval_clock += h.get_process_delta_time()
		retrieval_peak = maxf(retrieval_peak, float(h._native().get_slingshot_state()["retrieval_source_power_w"]))
	h._reel(device, false)
	var returned: Dictionary = h._native().get_slingshot_state()
	if bool(returned["released"]) or bool(returned["recovering"]) or float(returned["energy_j"]) >= 1.0:
		return h._fail("held reel did not capture a slack, reusable seat: %s" % returned)
	if float(returned["retrieval_work_j"]) <= 0.0 or retrieval_peak > 22000.0:
		return h._fail("returned seat has no bounded physical work receipt")
	await h._frames(3) # Let the shipping context retire the held return control.
	if device == InputRouter.Device.TOUCH and h._main._touch.is_button_shown(&"slingshot_reel"):
		return h._fail("returned seat left the reel control active")
	await h._pose("returned_seat")
	if not await h._walk_to(device, Vector2(neutral.x, neutral.z - 2.0), 0.15):
		return h._fail("ordinary walk could not approach the returned seat")
	h._main._yaw = 0.0
	h._main._pitch = 0.0
	# Enter and stop inside the actual curved bowl before fastening. Holding
	# full walking speed through it tests a ramp hop, not a boarding attempt.
	var returned_pouch: Vector3 = h._native().get_slingshot_state()["pouch_position"]
	if not await h._walk_to(device, Vector2(returned_pouch.x, returned_pouch.z), 0.10, 4.0):
		return h._fail("ordinary walking could not enter returned leather bowl")
	if not await h._wait_until(func() -> bool: return bool(h._native().get_slingshot_state()["station_available"]), 1.0):
		return h._fail("returned leather bowl did not offer BOARD after stopping: rider=%s state=%s" % [h._position(), h._native().get_slingshot_state()])
	await h._frames(2)
	if not await h._offered(&"slingshot", "ENTER POUCH"):
		return h._fail("settled returned seat does not offer BOARD")
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._native().get_slingshot_state()["seated"]), 0.5):
		return h._fail("returned seat cannot fasten a second rider p=%s v=%s action=%s native=%s" % [h._position(), h._velocity(), h._action_label(), h._native().get_slingshot_state()])
	h._move(device, -1.0)
	var funded_again := await h._wait_until(func() -> bool:
		var reading: Dictionary = h._native().get_slingshot_state()
		return float(reading["draw_m"]) >= target_draw and bool(reading["release_ready"]), 15.0)
	h._move(device, 0.0)
	if not funded_again:
		return h._fail("returned seat cannot be manually charged again")
	await h._seconds(0.4)
	h._act(device)
	if not await h._wait_until(func() -> bool: return int(h._native().get_slingshot_state()["launch_count"]) == launches + 2, 0.5):
		return h._fail("second funded shot did not release")
	await h._seconds(0.62)
	if h._velocity().length() < 8.0 or not h._main._slingshot_view.is_cinematic_active():
		return h._fail("second shot failed to accelerate or restart its cinematic")
	await h._pose("second_release")
	h._detail += " retrieval_s=%.2f retrieval_j=%.2f retrieval_peak_kw=%.2f second_launch=1" % [
		retrieval_clock + 0.7, returned["retrieval_work_j"], retrieval_peak / 1000.0]
	return true


func _touch_launch_roof_landing() -> bool:
	var deaths := int(h._native().get_death_count())
	var landings := int(h._native().get_landing_state()["landing_count"])
	# Guidance uses the interpolated visible rider, which can trail a physics tick.
	if not await h._wait_until(func() -> bool: return h._native().get_player_render_position().y >= 354.0 and h._velocity().y > 0.0, 12.0):
		return h._fail("launched rider did not reach the manual roof-braking window")
	if not h._main._slingshot_view._hud.landing_hint.contains("TAP CHUTE NOW") or bool(h._native().is_parachute_deployed()):
		return h._fail("measured braking window has no manual canopy guidance, or guidance deployed it automatically: native_y=%.4f render_y=%.4f vy=%.4f hint=%s deployed=%s flight=%s" % [
			h._position().y, h._native().get_player_render_position().y, h._velocity().y,
			h._main._slingshot_view._hud.landing_hint, h._native().is_parachute_deployed(),
			h._main._slingshot_view._landing_flight_active])
	if not bool(h._ctx()["chute_ok"]) or not h._main._touch.is_button_shown(&"chute"):
		return h._fail("touch interface did not offer its manual upward-flight brake")
	var brake_height := h._position().y
	h._tap(1, h._center(&"chute"))
	if not await h._wait_until(func() -> bool: return bool(h._native().is_parachute_deployed()), 0.4):
		return h._fail("real touch canopy press did not engage native drag")
	if not h._main._slingshot_view._hud.landing_hint.contains("CHUTE OPEN"):
		return h._fail("landing guidance did not reflect the real deployed canopy")
	await h._pose("manual_airbrake_354m")
	var apex := h._position().y
	var deepest_dip := 0.0
	var clock := 0.0
	while clock < 10.0:
		await h.get_tree().process_frame
		clock += h.get_process_delta_time()
		apex = maxf(apex, h._position().y)
		deepest_dip = minf(deepest_dip, h._main._landing_camera.translation.y)
		if int(h._native().get_death_count()) != deaths:
			return h._fail("manual touch roof arrival caused death")
		if bool(h._native().is_player_grounded()) and h._position().y > 350.0:
			break
	if not bool(h._native().is_player_grounded()) or h._position().y < 350.0:
		return h._fail("manual touch brake did not produce supported +352 m roof arrival: %s" % h._position())
	var landing: Dictionary = h._native().get_landing_state()
	if int(landing["landing_count"]) <= landings or float(landing["landing_approach_energy_j"]) <= 0.0 \
			or int(landing["landing_support_entity_id"]) <= 0:
		return h._fail("supported roof arrival emitted no real contact-energy witness")
	if deepest_dip >= -0.0001 or deepest_dip < -0.25:
		return h._fail("actual roof contact produced no bounded knee camera response: %.5f" % deepest_dip)
	await h._seconds(0.35)
	var look_start := h._viewport_size() * Vector2(0.78, 0.28)
	h._touch(7, look_start, true)
	h._drag(7, look_start + Vector2(0.0, 650.0), Vector2(0.0, 650.0))
	await h._frames(3)
	h._touch(7, look_start + Vector2(0.0, 650.0), false)
	if not h._main._slingshot_view._hud.landing_hint.is_empty():
		return h._fail("landing guidance stayed on-screen after native supported arrival")
	await h._pose("supported_352m_roof")
	var stood := h._position()
	h._move(InputRouter.Device.TOUCH, 1.0)
	await h._seconds(0.6)
	h._move(InputRouter.Device.TOUCH, 0.0)
	var walked := Vector2(h._position().x - stood.x, h._position().z - stood.z).length()
	if walked < 0.4 or not bool(h._native().is_player_grounded()) or h._position().y < 350.0 \
			or int(h._native().get_death_count()) != deaths:
		return h._fail("roof arrival did not permit continued ordinary touch walking")
	await h._pose("roof_touch_walk")
	h._detail += " brake_height_m=%.2f braked_apex_m=%.2f supported_height_m=%.2f deaths=%d roof_walk_m=%.2f contact_j=%.1f camera_dip_m=%.4f" % [
		brake_height, apex, h._position().y - 0.9, int(h._native().get_death_count()) - deaths,
		walked, float(landing["landing_approach_energy_j"]), deepest_dip]
	return true


func _touch_rail_aim(charged: bool = false) -> bool:
	var initial: Dictionary = h._native().get_slingshot_state()
	if bool(initial.get("aim_locked", true)) or (not charged and float(initial["energy_j"]) > 1.0):
		return h._fail("ordinary boarding preloaded bands and blocked unloaded aiming")
	var initial_goal := float(h._main._sling_goal_yaw)
	var initial_pitch := float(h._main._sling_goal_elevation)
	var gesture := Vector2(45.0, 70.0) if charged else Vector2(24.0, 0.0)
	var at := h._viewport_size() * Vector2(0.80, 0.30)
	h._touch(7, at, true)
	h._drag(7, at + gesture, gesture)
	await h._frames(3)
	h._touch(7, at + gesture, false)
	await h._frames(2)
	var requested := float(h._main._sling_goal_yaw)
	if absf(requested - initial_goal) < 0.001:
		return h._fail("one-shot touch look did not request a new rail angle")
	await h._seconds(0.25)
	if absf(float(h._main._sling_goal_yaw) - requested) > 0.000001:
		return h._fail("touch rail target was forgotten after finger lift")
	if not await h._wait_until(func() -> bool:
			var rail: Dictionary = h._native().get_slingshot_state()
			return bool(rail.get("aim_ready", false)) and absf(float(rail["yaw_rad"]) - requested) < 0.012 \
				and absf(float(rail["elevation_rad"]) - float(h._main._sling_goal_elevation)) < 0.012, 5.0):
		return h._fail("finite native rail did not settle near the retained touch target")
	if charged:
		var turned: Dictionary = h._native().get_slingshot_state()
		if absf(float(turned["elevation_rad"]) - initial_pitch) < 0.06 \
				or absf(float(turned["draw_m"]) - float(initial["draw_m"])) > 0.03 \
				or float(turned["work_j"]) <= float(initial["work_j"]):
			return h._fail("charged touch pitch did not move through paid torque with retained draw")
		await h._pose("charged_pitch_yaw_settled")
	else:
		await h._pose("manual_aim_settled")
	h._touch(7, at, true)
	h._drag(7, at - gesture, -gesture)
	await h._frames(3)
	h._touch(7, at - gesture, false)
	if not await h._wait_until(func() -> bool:
			var rail: Dictionary = h._native().get_slingshot_state()
			return bool(rail.get("aim_ready", false)) and absf(float(rail["yaw_rad"]) - initial_goal) < 0.012 \
				and absf(float(rail["elevation_rad"]) - initial_pitch) < 0.012, 5.0):
		return h._fail("reverse touch look did not return the physical rail to its initial goal")
	await h._seconds(0.4)
	return true
