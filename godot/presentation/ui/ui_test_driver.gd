extends Node

# Scripted proof that every control path reaches the native authority. Each
# scenario injects events through the real viewport input pipeline -- the
# same _input the devices feed -- and asserts a NATIVE state change (tick,
# traversal, velocity, station state), never a presentation variable alone.
#
# One scenario per process, because the native spawn is fixed at tick 0:
#   godot --headless --fixed-fps 60 --path godot -- --uitest=<scenario>
# Prints "SCRAPERX_UITEST PASS|FAIL <scenario> ..." and exits 0 or 31. With
# --capture=<prefix> under a real renderer, each scenario also saves the
# screen at its most telling moment as <prefix>_<pose>.png.

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")

const SCENARIOS := {
	"ground_foundation": 8,
	"touch_cargo_net": 8,
	"touch_campaign_to_121": 8,
	"touch_campaign_to_143": 8,
	"touch_campaign_to_165": 8,
	"touch_campaign_to_198": 8,
	"touch_campaign_to_231": 8,
	"touch_campaign_to_253": 8,
	"touch_campaign_to_286": 8,
	"touch_campaign_to_308": 8,
	"touch_service_lift": 8,
	"touch_balance_lift": 8,
	"touch_crown_gondola": 8,
	"touch_crown_swing": 8,
	"touch_traction_tram": 8,
	"touch_barrel_helix": 8,
	"touch_cascade_mast": 8,
	"touch_pitman_lift": 8,
	"touch_gravity_reclaim": 8,
	"touch_slab_haul_cart": 8,
	"touch_taper_inspection_route": 8,
	"touch_causal_facade": 8,
	"touch_north_grip_diagnostic": 8,
	"touch_suspended_ladder": 8,
	"keyboard_slingshot": 8,
	"pad_slingshot": 8,
	"touch_slingshot": 8,
	"touch_slingshot_landing": 8,
	"pipe_bridge": 8,
	"touch_pipe_bridge": 8,
	"keyboard_pipe_bridge": 8,
	"touch_facade": 8,
	"touch_stair": 8,
	"touch_upper": 8,
	"touch_teeter": 8,
	"touch_ballast_mantle_feedback": 8,
	"touch_braced_bay": 8,
	"touch_north_frame": 8,
	"touch_jump": 8,
	"touch_move_look": 8,
	"touch_gyro_aim": 8,
	"touch_climb": 4,
	"touch_vault": 3,
	"touch_double_tap_vault": 3,
	"touch_crouch": 0,
	"touch_hang_drop": 5,
	"touch_hang_climb": 5,
	"touch_chute": 11,
	"touch_lethal_feedback": 11,
	"touch_sump": 17,
	"touch_pendant": 18,
	"touch_carry": 22,
	"touch_water_screw": 25,
	"touch_water_lift": 26,
	"touch_pause": 8,
	"pad_core": 8,
	"pad_pendant": 14,
	"keyboard_core": 8,
	# AS-006 Stage A, rigged and ridden on each device from the stair's top deck.
	"touch_rig": 24,
	"pad_rig": 24,
	"keyboard_rig": 24,
	# Needs a mixing audio driver: run under --write-movie (see _audio_mix).
	"audio_mix": 8,
}

var _main: Node
var _scenario := ""
var _capture_prefix := ""
var _detail := ""


func begin(main: Node, scenario: String, capture_prefix: String) -> bool:
	if not SCENARIOS.has(scenario):
		return false
	_main = main
	_scenario = scenario
	_capture_prefix = capture_prefix
	process_mode = Node.PROCESS_MODE_ALWAYS
	# The removed opening survives only as an explicitly selected regression.
	if scenario in ["pipe_bridge", "touch_pipe_bridge", "keyboard_pipe_bridge", "touch_facade", "touch_stair", "touch_upper", "touch_teeter", "touch_braced_bay", "touch_north_frame"]:
		if not bool(main._native.configure_pipe_bridge_fixture()):
			return false
	if scenario not in ["ground_foundation", "touch_suspended_ladder", "touch_cargo_net", "touch_campaign_to_121", "touch_campaign_to_143", "touch_campaign_to_165", "touch_campaign_to_198", "touch_campaign_to_231", "touch_campaign_to_253", "touch_campaign_to_286", "touch_campaign_to_308", "touch_service_lift", "touch_balance_lift", "touch_crown_gondola", "touch_crown_swing", "touch_traction_tram", "touch_barrel_helix", "touch_cascade_mast", "touch_pitman_lift", "touch_gravity_reclaim", "touch_slab_haul_cart", "touch_taper_inspection_route", "touch_causal_facade", "touch_north_grip_diagnostic", "keyboard_slingshot", "pad_slingshot", "touch_slingshot", "touch_slingshot_landing", "pipe_bridge", "touch_pipe_bridge", "keyboard_pipe_bridge", "touch_facade", "touch_stair", "touch_upper", "touch_teeter", "touch_braced_bay", "touch_north_frame", "touch_ballast_mantle_feedback"] and not bool(main._native.configure_regression_spawn(int(SCENARIOS[scenario]))):
		return false
	# The traversal kernels are authored facing +x (native tests do the same).
	if scenario in ["touch_climb", "touch_vault", "touch_double_tap_vault", "touch_hang_drop",
			"touch_hang_climb"]:
		main._yaw = -PI * 0.5
		main._pitch = 0.0
	# HangApproach starts mid-air: the push toward the wall has to be held
	# from the first tick, exactly as the native falsifier holds it.
	if scenario.begins_with("touch_hang"):
		_stick_push(Vector2(0.0, -1.0))
	# From the static-deck spawn (0, -8) straight down the crawl beam's lane,
	# toward (-2, -15).
	if scenario == "touch_crouch":
		main._yaw = atan2(2.0, 7.0)
		main._pitch = 0.0
	_run.call_deferred()
	return true


func _run() -> void:
	await _frames(2)
	# Touch scenarios start the way a thumb does: the first contact switches
	# the interface to touch (a tap in the open look area moves nothing).
	if _scenario.begins_with("touch_") and not _scenario.begins_with("touch_hang"):
		_tap(7, Vector2(_viewport_size().x * 0.7, _viewport_size().y * 0.35))
		await _frames(2)
	var ok := false
	match _scenario:
		"ground_foundation":
			ok = await _ground_foundation()
		"touch_suspended_ladder":
			ok = await _touch_suspended_ladder()
		"touch_cargo_net":
			ok = await _touch_cargo_net()
		"touch_campaign_to_121":
			ok = await _touch_campaign_to_121()
		"touch_campaign_to_143":
			ok = await _touch_campaign_to_143()
		"touch_campaign_to_165":
			ok = await _touch_campaign_to_165()
		"touch_campaign_to_198":
			ok = await _touch_campaign_to_198()
		"touch_campaign_to_231":
			ok = await _touch_campaign_to_231()
		"touch_campaign_to_253":
			ok = await _touch_campaign_to_253()
		"touch_campaign_to_286":
			ok = await _touch_campaign_to_286()
		"touch_campaign_to_308":
			ok = await _touch_campaign_to_308()
		"touch_service_lift":
			ok = await _touch_service_lift(true)
		"touch_balance_lift":
			ok = await _touch_balance_lift(true)
		"touch_crown_gondola":
			ok = await _touch_crown_gondola(true)
		"touch_crown_swing":
			ok = await _touch_crown_swing()
		"touch_traction_tram":
			ok = await _touch_traction_tram(true)
		"touch_barrel_helix":
			ok = await _touch_barrel_helix(true)
		"touch_cascade_mast":
			ok = await _touch_cascade_mast(true)
		"touch_pitman_lift":
			ok = await _touch_pitman_lift(true)
		"touch_gravity_reclaim":
			ok = await _touch_gravity_reclaim()
		"touch_slab_haul_cart":
			ok = await _touch_slab_haul_cart()
		"touch_taper_inspection_route":
			ok = await _touch_taper_inspection_route()
		"touch_north_grip_diagnostic":
			ok = await _touch_north_grip_diagnostic()
		"keyboard_slingshot":
			ok = await _slingshot(InputRouter.Device.KEYBOARD_MOUSE)
		"pad_slingshot":
			ok = await _slingshot(InputRouter.Device.GAMEPAD)
		"touch_slingshot":
			ok = await _slingshot(InputRouter.Device.TOUCH)
		"touch_slingshot_landing":
			ok = await _slingshot(InputRouter.Device.TOUCH, true)
		"pipe_bridge":
			ok = await _pipe_bridge(InputRouter.Device.GAMEPAD)
		"touch_pipe_bridge":
			ok = await _pipe_bridge(InputRouter.Device.TOUCH)
		"keyboard_pipe_bridge":
			ok = await _pipe_bridge(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_facade", "touch_causal_facade":
			ok = await _touch_facade()
		"touch_stair":
			ok = await _touch_stair()
		"touch_upper":
			ok = await _touch_upper()
		"touch_teeter":
			ok = await _touch_teeter()
		"touch_ballast_mantle_feedback":
			ok = await _touch_ballast_mantle_feedback()
		"touch_braced_bay":
			ok = await _touch_braced_bay()
		"touch_north_frame":
			ok = await _touch_north_service_frame()
		"touch_jump":
			ok = await _touch_jump()
		"touch_move_look":
			ok = await _touch_move_look()
		"touch_gyro_aim":
			ok = await _touch_gyro_aim()
		"touch_climb":
			ok = await _touch_climb(&"climb", 2)
		"touch_vault":
			ok = await _touch_vault()
		"touch_double_tap_vault":
			ok = await _touch_double_tap_vault()
		"touch_crouch":
			ok = await _touch_crouch()
		"touch_hang_drop":
			ok = await _touch_hang(false)
		"touch_hang_climb":
			ok = await _touch_hang(true)
		"touch_chute":
			ok = await _touch_chute()
		"touch_lethal_feedback":
			ok = await _touch_lethal_feedback()
		"touch_sump":
			ok = await _touch_sump()
		"touch_pendant":
			ok = await _touch_pendant()
		"touch_carry":
			ok = await _touch_carry()
		"touch_water_screw":
			ok = await _touch_water_screw()
		"touch_water_lift":
			ok = await _touch_water_lift()
		"touch_pause":
			ok = await _touch_pause()
		"pad_core":
			ok = await _pad_core()
		"pad_pendant":
			ok = await _pad_pendant()
		"keyboard_core":
			ok = await _keyboard_core()
		"touch_rig":
			ok = await _rig(InputRouter.Device.TOUCH)
		"pad_rig":
			ok = await _rig(InputRouter.Device.GAMEPAD)
		"keyboard_rig":
			ok = await _rig(InputRouter.Device.KEYBOARD_MOUSE)
		"audio_mix":
			ok = await _audio_mix()
	print("SCRAPERX_UITEST %s %s %s" % ["PASS" if ok else "FAIL", _scenario, _detail])
	get_tree().paused = false
	get_tree().quit(0 if ok else 31)


func _fail(reason: String) -> bool:
	_detail = "reason=" + reason
	return false


# --- scenarios -----------------------------------------------------------------

func _slingshot(device: int, supported_landing: bool = false) -> bool:
	if _main._regression_scene or not bool(_native().get_slingshot_state().get("available", false)):
		return _fail("slingshot scenario did not select the shipping world")
	_main._settings.head_bob = true
	_main._head_bob_on = true
	_main._settings.launch_cinematics = true
	# Explicit test staging: outside the pouch at ground level, three metres
	# toward the tower. Ordinary viewport movement then walks backwards
	# along +Z onto the actual leather floor; boarding never teleports.
	var station: Vector3 = _native().get_slingshot_state()["pouch_position"]
	if not bool(_native().debug_restart_at(Vector3(station.x, 0.92, station.z - 3.0))):
		return _fail("outside-pouch staging was rejected")
	_main._yaw = 0.0 # Face -Z, so local backwards moves +Z into the pouch.
	_main._pitch = 0.0
	await _frames(3)
	await _pose("outside_pouch")
	_move(device, -1.0)
	if not await _wait_until(func() -> bool: return bool(_native().get_slingshot_state()["station_available"]), 4.0):
		_move(device, 0.0)
		var missed: Dictionary = _native().get_slingshot_state()
		return _fail("ordinary backwards movement did not enter leather pouch at %s pouch=%s recovering=%s" % [
			_position(), missed["pouch_position"], missed["recovering"]])
	_move(device, 0.0)
	await _frames(2)
	var approached: Dictionary = _native().get_slingshot_state()
	if not approached.has("aim_locked"):
		return _fail("native aim lock did not cross the actual Godot bridge")
	if bool(approached["seated"]) or float(approached["energy_j"]) > 1.0 or float(approached["work_j"]) > 0.01:
		return _fail("backward touch approach seated or charged before explicit BOARD")
	if not await _offered(&"slingshot", "ENTER POUCH"):
		return _fail("native pouch entry was not offered at %s" % _position())
	var before_board := _position()
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_slingshot_state()["seated"]), 0.5):
		return _fail("device Action did not fasten native pouch harness")
	if _position().distance_to(before_board) > 0.12:
		return _fail("boarding displaced rider instead of fastening present pose")
	if not await _offered(&"slingshot", "DRAW MORE"):
		return _fail("unfunded shot gave no draw-more explanation")
	var uncharged_launches := int(_native().get_slingshot_state()["launch_count"])
	_act(device)
	await _frames(3)
	if bool(_native().get_slingshot_state().get("release_ready", true)) \
			or int(_native().get_slingshot_state()["launch_count"]) != uncharged_launches:
		return _fail("unfunded Action released below the physical guide-exit energy")
	if device == InputRouter.Device.TOUCH and not await _touch_rail_aim():
		return false
	await _pose("seated_manual_draw")
	var draw_clock := 0.0
	var peak_power := 0.0
	var parameters: Dictionary = _native().get_slingshot_state()
	var max_draw := float(parameters.get("max_draw_m", 12.0))
	var target_draw := max_draw - 0.025 # Within the native stop's finite clearance.
	var power_limit := float(parameters.get("max_source_power_w", 200000.0))
	_move(device, -1.0)
	while draw_clock < 15.0:
		await get_tree().process_frame
		draw_clock += get_process_delta_time()
		var charging: Dictionary = _native().get_slingshot_state()
		peak_power = maxf(peak_power, float(charging["source_power_w"]))
		if float(charging["draw_m"]) >= target_draw:
			break
	_move(device, 0.0)
	await _seconds(0.4) # The real carriage coasts into its one-way catch.
	var charged: Dictionary = _native().get_slingshot_state()
	if bool(charged["aim_locked"]):
		return _fail("funded manual draw incorrectly locked touch aiming")
	var held_draw := float(charged["draw_m"])
	var held_work := float(charged["work_j"])
	var energy := float(charged["energy_j"])
	if not bool(charged.get("release_ready", false)):
		return _fail("funded full draw did not unlock native release")
	if held_draw < target_draw - 0.02 or energy < 40000.0 or held_work < energy:
		return _fail("manual draw was not physically funded: draw=%.3f energy=%.1f work=%.1f" % [held_draw, energy, held_work])
	if not is_finite(peak_power) or peak_power < power_limit * 0.25 or peak_power > power_limit * 1.15 \
			or held_work > power_limit * (draw_clock + 0.4) * 1.03:
		return _fail("manual source power/work outside declared budget: peak=%.1f work=%.1f time=%.2f" % [peak_power, held_work, draw_clock])
	await _seconds(0.6)
	var held: Dictionary = _native().get_slingshot_state()
	if absf(float(held["draw_m"]) - held_draw) > 0.03 or absf(float(held["work_j"]) - held_work) > 0.10:
		return _fail("idle ratchet lost draw or accrued manual work")
	# A charged touch gesture must change both axes through native torque,
	# retain draw, then return to the nominal roof shot for landing proof.
	if device == InputRouter.Device.TOUCH and not await _touch_rail_aim(true):
		return false
	var aimed: Dictionary = _native().get_slingshot_state()
	var rendered_seat: Dictionary = _native().get_slingshot_render_state()
	if not rendered_seat.has("seat_surface_position") or not rendered_seat.has("rider_specific_acceleration"):
		return _fail("measured rider load and leather seat did not cross the shipping bridge")
	var pelvis: Node3D = _main._slingshot_view._avatar.get_node("HarnessPelvis")
	var seat_gap := (pelvis.global_transform * Vector3(0, -0.19, 0)).distance_to(rendered_seat["seat_surface_position"])
	if seat_gap > 0.003:
		return _fail("charged rider is not cradled on actual rendered leather: gap_m=%.4f" % seat_gap)
	await _pose("charged_adjustable_aim")
	if not await _offered(&"slingshot", "RELEASE"):
		return _fail("loaded Action did not offer RELEASE")
	var launches := int(aimed["launch_count"])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_slingshot_state()["launch_count"]) == launches + 1, 0.5):
		return _fail("device Action did not remove native draw restraint")
	if not bool(_native().get_slingshot_state()["released"]) or not _main._slingshot_view.is_cinematic_active():
		return _fail("physical release did not start cinematic")
	if _main._read_context()["action"]["id"] == &"slingshot":
		return _fail("released harness still offered a charging/release action")
	var launch_time := float(_native().get_slingshot_state()["simulation_time_seconds"])
	var launch_position := _position()
	var recoil_peak := 0.0
	var launch_clock := 0.0
	var capture_index := 0
	var capture_times := [0.12, 0.32, 0.62]
	while launch_clock < 0.62:
		await get_tree().process_frame
		launch_clock += get_process_delta_time()
		var rider_sample: Dictionary = _native().get_slingshot_state()
		recoil_peak = maxf(recoil_peak, (rider_sample["rider_specific_acceleration"] as Vector3).length())
		if float(rider_sample["simulation_time_seconds"]) - launch_time < 0.04 \
				and _main._slingshot_view.get_simulation_scale() < 0.99:
			return _fail("camera slowed the initial native recoil before it developed")
		if capture_index < capture_times.size() and launch_clock >= capture_times[capture_index]:
			print("SCRAPERX_LAUNCH_EMBODIMENT wall_s=%.3f native_s=%.3f displacement_m=%.3f speed_mps=%.3f specific_mps2=%.3f torso_rad=%s head_rad=%s scale=%.3f" % [
				launch_clock, float(rider_sample["simulation_time_seconds"]) - launch_time,
				_position().distance_to(launch_position), _velocity().length(),
				(rider_sample["rider_specific_acceleration"] as Vector3).length(),
				_main._slingshot_view._avatar.get_node("WorkJacket").rotation,
				_main._slingshot_view._avatar.get_node("ExpressiveHead").rotation,
				_main._slingshot_view.get_simulation_scale()])
			await _pose("launch_embodiment_%d" % capture_index)
			capture_index += 1
	if recoil_peak < 100.0 or _position().distance_to(launch_position) < 4.0:
		return _fail("shipping release did not expose real recoil and displacement: peak=%.2f displacement=%.2f" % [recoil_peak, _position().distance_to(launch_position)])
	var release_speed := _velocity().length()
	if release_speed < 8.0 or not _main._slingshot_view._avatar.visible or _main._arms.visible:
		return _fail("native spring release did not accelerate visible human rider: speed=%.2f" % release_speed)
	if not _main._slingshot_view._hud.has_thought_bubble():
		return _fail("actual rising release did not show its brief thought reaction")
	await _pose("release_orbit")
	if not await _wait_until(func() -> bool: return not _main._slingshot_view.is_cinematic_active(), 3.0):
		return _fail("orbital camera did not return after brief sequence")
	if _main._slingshot_view.get_simulation_scale() != 1.0 or not _main._arms.visible \
			or _main._slingshot_view._avatar.visible or _main._slingshot_view._hud.has_thought_bubble():
		return _fail("cinematic return did not restore real time and first-person body")
	await _pose("true_first_person_return")
	if supported_landing:
		_detail = "staging=outside_pouch_then_walk draw_m=%.3f spring_kj=%.2f manual_kj=%.2f peak_kw=%.2f release_mps=%.2f" % \
			[held_draw, energy / 1000.0, held_work / 1000.0, peak_power / 1000.0, release_speed]
		return await _touch_launch_roof_landing()
	var apex := _position().y
	var flight_clock := 0.0
	while flight_clock < 12.0:
		await get_tree().process_frame
		flight_clock += get_process_delta_time()
		apex = maxf(apex, _position().y)
		if _velocity().y < -1.0:
			break
	if apex < 250.0 or bool(_native().get_slingshot_state()["seated"]) or _velocity().y >= -1.0:
		return _fail("real launch did not detach and reach useful tower height: apex=%.2f" % apex)
	await _pose("native_flight_apex")
	# A second explicit staging relocation isolates the ground retrieval UI
	# after the measured flight. All retrieval/cancel/walking below again
	# travels through ordinary viewport input; no native request_* shortcuts.
	var neutral: Vector3 = _native().get_slingshot_state().get("neutral_position", station)
	var control: Vector3 = _native().get_slingshot_state().get("retrieval_control_position",
		neutral + Vector3(1.5, 0.35, -1.8))
	# Native control identifies the handwheel, not the capsule's standing
	# centre. Stand clear of the real pole at grade, inside its offer radius.
	var control_standing := Vector3(control.x + 0.85, neutral.y + 0.57, control.z - 0.4)
	if not bool(_native().debug_restart_at(control_standing)):
		return _fail("retrieval-control staging was rejected")
	_main._slingshot_view.cancel_cinematic()
	_main._yaw = 0.0
	_main._pitch = 0.0
	await _frames(3)
	if not await _offered(&"slingshot", "RETRIEVE POUCH"):
		return _fail("ground control did not offer physical pouch retrieval")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_slingshot_state()["recovering"]), 0.5):
		return _fail("retrieval Action did not engage native tether")
	await _seconds(0.3)
	var recovering: Dictionary = _native().get_slingshot_state()
	if absf(float(recovering["retrieval_work_j"])) > 0.10 or absf(float(recovering["retrieval_source_power_w"])) > 0.10:
		return _fail("idle retrieval accrued unfunded manual work")
	if device == InputRouter.Device.TOUCH and not _main._touch.is_button_shown(&"slingshot_reel"):
		return _fail("retrieval has no dedicated hold-to-reel touch control")
	await _pose("retrieval_idle_cancel_available")
	match device:
		InputRouter.Device.TOUCH:
			_tap(1, _center(&"drop"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_B)
		_:
			_key(KEY_Q, true)
			_key(KEY_Q, false)
	if not await _wait_until(func() -> bool: return not bool(_native().get_slingshot_state()["recovering"]), 0.5):
		return _fail("retrieval Drop did not release native tether")
	var after_cancel := _position()
	_move(device, 1.0)
	await _seconds(0.6)
	_move(device, 0.0)
	var walked := Vector2(_position().x - after_cancel.x, _position().z - after_cancel.z).length()
	if walked < 0.4:
		return _fail("retrieval cancel kept native walking locked: %.3f m" % walked)
	_detail = "staging=outside_pouch_then_walk draw_m=%.3f spring_kj=%.2f manual_kj=%.2f peak_kw=%.2f release_mps=%.2f apex_m=%.2f" % \
		[held_draw, energy / 1000.0, held_work / 1000.0, peak_power / 1000.0, release_speed, apex]
	_detail += " retrieval_cancel_walk_m=%.2f" % walked
	# From here every return/reboard/recharge input is ordinary viewport
	# input. No relocation, reset, or machine-state assignment closes the loop.
	if not await _walk_to(device, Vector2(control_standing.x, control_standing.z), 0.15):
		return _fail("ordinary walking could not return to the wheel")
	if not await _offered(&"slingshot", "RETRIEVE POUCH"):
		return _fail("return wheel is not reachable from a clear standing position")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_slingshot_state()["recovering"]), 0.5):
		return _fail("second retrieval attempt did not engage")
	await _seconds(0.3)
	var retrieval_clock := 0.0
	var retrieval_peak := 0.0
	_reel(device, true)
	await _seconds(0.7)
	_reel(device, false)
	await _seconds(0.15)
	var paused_work := float(_native().get_slingshot_state()["retrieval_work_j"])
	await _seconds(0.3)
	if absf(float(_native().get_slingshot_state()["retrieval_work_j"]) - paused_work) > 0.1:
		return _fail("lifting the reel finger left manual work running: delta_j=%.3f power_w=%.3f touch_reel=%.1f touch_move=%s" % [float(_native().get_slingshot_state()["retrieval_work_j"]) - paused_work, float(_native().get_slingshot_state()["retrieval_source_power_w"]), _main._touch.reel_effort, str(_main._touch.move_vector)])
	_reel(device, true)
	while bool(_native().get_slingshot_state()["released"]) and retrieval_clock < 15.0:
		await get_tree().process_frame
		retrieval_clock += get_process_delta_time()
		retrieval_peak = maxf(retrieval_peak, float(_native().get_slingshot_state()["retrieval_source_power_w"]))
	_reel(device, false)
	var returned: Dictionary = _native().get_slingshot_state()
	if bool(returned["released"]) or bool(returned["recovering"]) or float(returned["energy_j"]) >= 1.0:
		return _fail("held reel did not capture a slack, reusable seat: %s" % returned)
	if float(returned["retrieval_work_j"]) <= 0.0 or retrieval_peak > 22000.0:
		return _fail("returned seat has no bounded physical work receipt")
	await _frames(3) # Let the shipping context retire the held return control.
	if device == InputRouter.Device.TOUCH and _main._touch.is_button_shown(&"slingshot_reel"):
		return _fail("returned seat left the reel control active")
	await _pose("returned_seat")
	if not await _walk_to(device, Vector2(neutral.x, neutral.z - 2.0), 0.15):
		return _fail("ordinary walk could not approach the returned seat")
	_main._yaw = 0.0
	_main._pitch = 0.0
	# Enter and stop inside the actual curved bowl before fastening. Holding
	# full walking speed through it tests a ramp hop, not a boarding attempt.
	var returned_pouch: Vector3 = _native().get_slingshot_state()["pouch_position"]
	if not await _walk_to(device, Vector2(returned_pouch.x, returned_pouch.z), 0.10, 4.0):
		return _fail("ordinary walking could not enter returned leather bowl")
	if not await _wait_until(func() -> bool: return bool(_native().get_slingshot_state()["station_available"]), 1.0):
		return _fail("returned leather bowl did not offer BOARD after stopping: rider=%s state=%s" % [_position(), _native().get_slingshot_state()])
	await _frames(2)
	if not await _offered(&"slingshot", "ENTER POUCH"):
		return _fail("settled returned seat does not offer BOARD")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_slingshot_state()["seated"]), 0.5):
		return _fail("returned seat cannot fasten a second rider p=%s v=%s action=%s native=%s" % [_position(), _velocity(), _action_label(), _native().get_slingshot_state()])
	_move(device, -1.0)
	var funded_again := await _wait_until(func() -> bool:
		var reading: Dictionary = _native().get_slingshot_state()
		return float(reading["draw_m"]) >= target_draw and bool(reading["release_ready"]), 15.0)
	_move(device, 0.0)
	if not funded_again:
		return _fail("returned seat cannot be manually charged again")
	await _seconds(0.4)
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_slingshot_state()["launch_count"]) == launches + 2, 0.5):
		return _fail("second funded shot did not release")
	await _seconds(0.62)
	if _velocity().length() < 8.0 or not _main._slingshot_view.is_cinematic_active():
		return _fail("second shot failed to accelerate or restart its cinematic")
	await _pose("second_release")
	_detail += " retrieval_s=%.2f retrieval_j=%.2f retrieval_peak_kw=%.2f second_launch=1" % [
		retrieval_clock + 0.7, returned["retrieval_work_j"], retrieval_peak / 1000.0]
	return true


func _touch_launch_roof_landing() -> bool:
	var deaths := int(_native().get_death_count())
	var landings := int(_native().get_landing_state()["landing_count"])
	# Guidance uses the interpolated visible rider, which can trail a physics tick.
	if not await _wait_until(func() -> bool: return _native().get_player_render_position().y >= 354.0 and _velocity().y > 0.0, 12.0):
		return _fail("launched rider did not reach the manual roof-braking window")
	if not _main._slingshot_view._hud.landing_hint.contains("TAP CHUTE NOW") or bool(_native().is_parachute_deployed()):
		return _fail("measured braking window has no manual canopy guidance, or guidance deployed it automatically: native_y=%.4f render_y=%.4f vy=%.4f hint=%s deployed=%s flight=%s" % [
			_position().y, _native().get_player_render_position().y, _velocity().y,
			_main._slingshot_view._hud.landing_hint, _native().is_parachute_deployed(),
			_main._slingshot_view._landing_flight_active])
	if not bool(_ctx()["chute_ok"]) or not _main._touch.is_button_shown(&"chute"):
		return _fail("touch interface did not offer its manual upward-flight brake")
	var brake_height := _position().y
	_tap(1, _center(&"chute"))
	if not await _wait_until(func() -> bool: return bool(_native().is_parachute_deployed()), 0.4):
		return _fail("real touch canopy press did not engage native drag")
	if not _main._slingshot_view._hud.landing_hint.contains("CHUTE OPEN"):
		return _fail("landing guidance did not reflect the real deployed canopy")
	await _pose("manual_airbrake_354m")
	var apex := _position().y
	var deepest_dip := 0.0
	var clock := 0.0
	while clock < 10.0:
		await get_tree().process_frame
		clock += get_process_delta_time()
		apex = maxf(apex, _position().y)
		deepest_dip = minf(deepest_dip, _main._landing_camera.translation.y)
		if int(_native().get_death_count()) != deaths:
			return _fail("manual touch roof arrival caused death")
		if bool(_native().is_player_grounded()) and _position().y > 350.0:
			break
	if not bool(_native().is_player_grounded()) or _position().y < 350.0:
		return _fail("manual touch brake did not produce supported +352 m roof arrival: %s" % _position())
	var landing: Dictionary = _native().get_landing_state()
	if int(landing["landing_count"]) <= landings or float(landing["landing_approach_energy_j"]) <= 0.0 \
			or int(landing["landing_support_entity_id"]) <= 0:
		return _fail("supported roof arrival emitted no real contact-energy witness")
	if deepest_dip >= -0.0001 or deepest_dip < -0.25:
		return _fail("actual roof contact produced no bounded knee camera response: %.5f" % deepest_dip)
	await _seconds(0.35)
	var look_start := _viewport_size() * Vector2(0.78, 0.28)
	_touch(7, look_start, true)
	_drag(7, look_start + Vector2(0.0, 650.0), Vector2(0.0, 650.0))
	await _frames(3)
	_touch(7, look_start + Vector2(0.0, 650.0), false)
	if not _main._slingshot_view._hud.landing_hint.is_empty():
		return _fail("landing guidance stayed on-screen after native supported arrival")
	await _pose("supported_352m_roof")
	var stood := _position()
	_move(InputRouter.Device.TOUCH, 1.0)
	await _seconds(0.6)
	_move(InputRouter.Device.TOUCH, 0.0)
	var walked := Vector2(_position().x - stood.x, _position().z - stood.z).length()
	if walked < 0.4 or not bool(_native().is_player_grounded()) or _position().y < 350.0 \
			or int(_native().get_death_count()) != deaths:
		return _fail("roof arrival did not permit continued ordinary touch walking")
	await _pose("roof_touch_walk")
	_detail += " brake_height_m=%.2f braked_apex_m=%.2f supported_height_m=%.2f deaths=%d roof_walk_m=%.2f contact_j=%.1f camera_dip_m=%.4f" % [
		brake_height, apex, _position().y - 0.9, int(_native().get_death_count()) - deaths,
		walked, float(landing["landing_approach_energy_j"]), deepest_dip]
	return true


func _touch_rail_aim(charged: bool = false) -> bool:
	var initial: Dictionary = _native().get_slingshot_state()
	if bool(initial.get("aim_locked", true)) or (not charged and float(initial["energy_j"]) > 1.0):
		return _fail("ordinary boarding preloaded bands and blocked unloaded aiming")
	var initial_goal := float(_main._sling_goal_yaw)
	var initial_pitch := float(_main._sling_goal_elevation)
	var gesture := Vector2(45.0, 70.0) if charged else Vector2(24.0, 0.0)
	var at := _viewport_size() * Vector2(0.80, 0.30)
	_touch(7, at, true)
	_drag(7, at + gesture, gesture)
	await _frames(3)
	_touch(7, at + gesture, false)
	await _frames(2)
	var requested := float(_main._sling_goal_yaw)
	if absf(requested - initial_goal) < 0.001:
		return _fail("one-shot touch look did not request a new rail angle")
	await _seconds(0.25)
	if absf(float(_main._sling_goal_yaw) - requested) > 0.000001:
		return _fail("touch rail target was forgotten after finger lift")
	if not await _wait_until(func() -> bool:
			var rail: Dictionary = _native().get_slingshot_state()
			return bool(rail.get("aim_ready", false)) and absf(float(rail["yaw_rad"]) - requested) < 0.012 \
				and absf(float(rail["elevation_rad"]) - float(_main._sling_goal_elevation)) < 0.012, 5.0):
		return _fail("finite native rail did not settle near the retained touch target")
	if charged:
		var turned: Dictionary = _native().get_slingshot_state()
		if absf(float(turned["elevation_rad"]) - initial_pitch) < 0.06 \
				or absf(float(turned["draw_m"]) - float(initial["draw_m"])) > 0.03 \
				or float(turned["work_j"]) <= float(initial["work_j"]):
			return _fail("charged touch pitch did not move through paid torque with retained draw")
		await _pose("charged_pitch_yaw_settled")
	else:
		await _pose("manual_aim_settled")
	_touch(7, at, true)
	_drag(7, at - gesture, -gesture)
	await _frames(3)
	_touch(7, at - gesture, false)
	if not await _wait_until(func() -> bool:
			var rail: Dictionary = _native().get_slingshot_state()
			return bool(rail.get("aim_ready", false)) and absf(float(rail["yaw_rad"]) - initial_goal) < 0.012 \
				and absf(float(rail["elevation_rad"]) - initial_pitch) < 0.012, 5.0):
		return _fail("reverse touch look did not return the physical rail to its initial goal")
	await _seconds(0.4)
	return true


func _pipe_bridge(device: int) -> bool:
	if _main._regression_scene:
		return _fail("continuous route selected retired regression fixtures")
	await _seconds(0.5)
	for point in [Vector2(13, -75), Vector2(12.7, -78.95)]:
		if not await _walk_to(device, point, 0.07, 20.0):
			return _fail("rack approach at %s" % _position())
	await _face(Vector2(-1, 0))
	await _pose("rack_ready")
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("release grip not offered at %s" % _position())
	_act(device)
	await _seconds(0.15)
	if int(_native().get_carrying_entity_id()) != 2530:
		return _fail("rack grip was not picked up")
	_move(device, 0.35)
	await _seconds(0.6)
	_move(device, 0)
	if int(_native().get_carrying_entity_id()) != 0:
		_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_pipe_bridge_retained_pipes()) == 20, 12.0):
		return _fail("rack did not load all twenty pipes")
	await _pose("loaded_pan")
	for point in [Vector2(12.7, -75), Vector2(-1.4, -75), Vector2(-1.4, -85.6), Vector2(-0.5, -85.6)]:
		if not await _walk_to(device, point, 0.07, 20.0):
			return _fail("bridge grip approach at %s" % _position())
	await _face(Vector2(0, 1))
	await _seconds(0.15)
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("release grip not offered at %s" % _position())
	_act(device)
	await _seconds(0.15)
	if int(_native().get_carrying_entity_id()) != 2505:
		return _fail("bridge grip was not picked up pos=%s target=%d held=%d" % [
			_position(), _native().get_carry_target_entity_id(), _native().get_carrying_entity_id()])
	_move_dir(device, Vector2(0.35, 0))
	await _seconds(0.6)
	_move_dir(device, Vector2.ZERO)
	if int(_native().get_carrying_entity_id()) != 0:
		_act(device)
	if not await _wait_until(func() -> bool: return float(_native().get_pipe_bridge_crush_front()) > 0.1, 15.0):
		return _fail("pan never reached the crush receiver")
	await _seconds(2.0)
	var tip := float(_native().get_pipe_bridge_tip_height())
	if tip < 7.53 or tip > 8.2:
		return _fail("bridge height %.4f" % tip)
	await _face(Vector2(1, -1))
	await _pose("bridge_raised")
	var tip_z := -90.0 - 20.0 * cos(asin(7.4 / 20.0))
	var crossed_bridge := false
	for point in [Vector2(-0.6, -86), Vector2(1.94, -86), Vector2(1.94, -91.1), Vector2(6, -91.1),
			Vector2(6, tip_z + 1.5), Vector2(9.06, tip_z + 1.5), Vector2(9.06, tip_z - 2), Vector2(9.06, -125.2)]:
		await _face(point - Vector2(_position().x, _position().z))
		if not await _walk_to(device, point, 0.07, 20.0):
			return _fail("crossing at %s" % _position())
		if int(_native().get_support_entity_id()) == 2500:
			crossed_bridge = true
	if not crossed_bridge:
		return _fail("route never used the actual moving deck")
	if int(_native().get_support_entity_id()) not in [11, 51]:
		return _fail("arrival is disconnected from the original tower")
	await _face(Vector2(-0.1, 1))
	await _pose("tower_arrival")
	if _position().y < 11.6 or int(_native().get_death_count()) != 0:
		return _fail("did not arrive alive at the tower ring")
	if _main._audio.pipe_motion_frames == 0 or _main._audio.pipe_crush_cues == 0:
		return _fail("observed motion and crush did not produce sound cues")
	_detail = "pipes=20 tip_y=%.4f arrival_y=%.3f native_input=1 deaths=0" % [tip, _position().y]
	return true


# AS-017: grade to +33 m, through the real touch controls and native world.
func _touch_facade() -> bool:
	var device := InputRouter.Device.TOUCH
	if _scenario in ["touch_campaign_to_121", "touch_campaign_to_143", "touch_campaign_to_165", "touch_campaign_to_198", "touch_campaign_to_231", "touch_campaign_to_253", "touch_campaign_to_286", "touch_campaign_to_308", "touch_service_lift", "touch_causal_facade"]:
		if not await _touch_cargo_net():
			return false
		print("SCRAPERX_CAMPAIGN cargo_net_exit at=%s support=%d" % [_position(), _native().get_support_entity_id()])
	elif not await _pipe_bridge(device):
		return false
	for point in [Vector2(21.2, -124.4), Vector2(21.2, -121.3), Vector2(20, -121.3)]:
		if not await _walk_to(device, point, 0.08, 20.0):
			return _fail("facade approach %s" % _position())
	await _face(Vector2(0, -1))
	await _seconds(0.4)
	await _pose("facade_approach")
	if not await _offered(&"climb", "CLIMB"):
		return _fail("cabinet mantle not offered")
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(13.5), 4.0):
		return _fail("cabinet mantle %s" % _position())
	if not await _walk_to(device, Vector2(20, -122.1), 0.05, 3.0):
		return _fail("cabinet launch position")
	await _face(Vector2(0, -1))
	await _seconds(0.3)
	_tap(1, _center(&"jump"))
	_move(device, 0.4)
	var caught := await _wait_until(func() -> bool: return bool(_ctx()["hanging"]), 2.0)
	_move(device, 0)
	if not caught:
		return _fail("duct hang %s" % _position())
	await _seconds(0.3)
	await _pose("duct_hang")
	_tap(1, _center(&"jump"))
	if not await _wait_until(func() -> bool: return _standing_above(17.0), 4.0):
		return _fail("duct top-out %s" % _position())
	for point in [Vector2(22.5, -123.05), Vector2(24, -123.0)]:
		if not await _walk_to(device, point, 0.06, 6.0):
			return _fail("duct traverse %s" % _position())
	await _face(Vector2(0, -1))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("vent grip not offered")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_ctx()["climbing"]), 1.0):
		return _fail("touch action did not take vent hold")
	_move(device, 1)
	await _seconds(1.0)
	await _pose("vent_climb")
	var climbed := await _wait_until(func() -> bool: return _standing_above(22.5), 20.0)
	_move(device, 0)
	if not climbed:
		return _fail("vent top-out %s" % _position())
	for point in [Vector2(22, -125.2), Vector2(12.5, -125.2), Vector2(12.5, -119.65)]:
		if not await _walk_to(device, point, 0.06, 12.0):
			return _fail("monorail %s" % _position())
	await _face(Vector2(0, -1))
	await _seconds(0.3)
	if bool(_native().is_grip_available()):
		return _fail("ladder should require a leap")
	await _pose("ladder_leap")
	_tap(1, _center(&"jump"))
	_move(device, 1.0)
	caught = await _wait_until(func() -> bool: return bool(_ctx()["climbing"]), 2.0)
	_move(device, 0)
	if not caught:
		return _fail("ladder catch %s" % _position())
	_move(device, 1)
	climbed = await _wait_until(func() -> bool: return _standing_above(33.5), 20.0)
	_move(device, 0)
	if not climbed:
		return _fail("ladder top-out %s" % _position())
	for point in [Vector2(12.5, -125), Vector2(11.2, -125.3)]:
		if not await _walk_to(device, point, 0.1, 10.0):
			return _fail("davit exit %s" % _position())
	await _seconds(0.5)
	await _face(Vector2(1, 1))
	await _pose("facade_arrival")
	if not _standing_above(33.5) or int(_native().get_support_entity_id()) != 11 \
			or int(_native().get_death_count()) != 0:
		return _fail("unsupported facade arrival %s" % _position())
	_detail = "grade_to_33m=1 normal_touch=1 deaths=0 arrival_y=%.3f" % _position().y
	return true


# Continue the same ordinary touch route onto the moving stair and the +44 m deck.
func _touch_stair() -> bool:
	var device := InputRouter.Device.TOUCH
	if not await _touch_facade():
		return false
	if int(_native().get_entity_body_count(2600)) != 1:
		return _fail("swinging stair is absent from the normal scene")
	# The actual hand point is 1.05 m above the standing capsule centre. Stop
	# inside the normal 1.20 m reach, clear of the landing's south rail.
	for point in [Vector2(-17.5, -125.5), Vector2(-17.5, -121.35)]:
		if not await _walk_to(device, point, 0.08, 20.0):
			return _fail("stair handle approach %s" % _position())
	await _face(Vector2(0, 1))
	_main._pitch = -0.75
	await _seconds(0.5)
	await _pose("stair_handle_ready")
	if not await _offered(&"pick_up", "GRAB") or int(_native().get_carry_target_entity_id()) != 2602:
		return _fail("stair chain handle was not offered: target=%d grounded=%s traversal=%d action=%s" % [
			int(_native().get_carry_target_entity_id()),
			str(_native().is_player_grounded()), int(_native().get_traversal_state()), str(_ctx()["action"])])
	_act(device)
	await _seconds(0.3)
	if int(_native().get_carrying_entity_id()) != 2602:
		return _fail("stair handle was not held")
	_move(device, -0.35)
	await _seconds(0.6)
	_move(device, 0)
	if int(_native().get_carrying_entity_id()) == 2602:
		_act(device)
	await _seconds(0.1)
	await _face(Vector2(1, 0))
	_main._pitch = -0.4
	await _pose("stair_released")
	if not await _walk_to(device, Vector2(-16.4, -122.1), 0.1, 8.0):
		return _fail("stair foot %s" % _position())
	var used_flight := false
	for x in [-15.2, -14.0, -12.0, -10.0, -8.0, -5.0, -2.7]:
		if not await _walk_to(device, Vector2(x, -122.1), 0.1, 25.0):
			var flight := int(_native().get_kit_body_index(2600))
			var flight_pose: Transform3D = _native().get_kit_body_transform(flight)
			var sample_tick := int(_native().get_tick_index())
			await _frames(2)
			var sample_seconds := float(int(_native().get_tick_index()) - sample_tick) * float(_native().get_fixed_step_seconds())
			var angular_z := angle_difference(flight_pose.basis.get_euler().z, _teeter_angle(flight)) / maxf(sample_seconds, 0.000001)
			return _fail("stair climb at %.1f: %s support=%d grounded=%s traversal=%d velocity=%s flight_origin=%s flight_angle_z=%.6f flight_velocity=%s measured_angular_z_rad_s=%.6f grip=%s landing=%s deaths=%d" % [
				x, _position(), _native().get_support_entity_id(), _ctx()["grounded"], _ctx()["traversal"], _velocity(),
				flight_pose.origin, flight_pose.basis.get_euler().z, _native().get_kit_body_linear_velocity(flight), angular_z,
				_native().is_grip_available(), _native().get_landing_state(), _native().get_death_count()])
		if int(_native().get_support_entity_id()) == 2600:
			used_flight = true
		if x == -10.0:
			_main._pitch = -0.55
			await _pose("stair_climb")
	if not used_flight:
		return _fail("player never stood on the moving flight")
	await _seconds(0.3)
	var stair_body := int(_native().get_kit_body_index(2600))
	var stair_pose: Transform3D = _native().get_kit_body_transform(stair_body)
	var exit_local := Vector3(0.25 + 43.0 * 0.25 / tan(0.70860367) + 0.45, 11.3, 0.0)
	var exit_x := (stair_pose * exit_local).x
	if not await _walk_to(device, Vector2(exit_x, -122.4), 0.1, 4.0):
		return _fail("stair landing exit %s" % _position())
	_main._pitch = -0.4
	await _pose("stair_exit_ready")
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(exit_x, -125.5), 0.1, 6.0):
		return _fail("stair deck jump %s" % _position())
	await _seconds(0.5)
	await _face(Vector2(0, 1))
	_main._pitch = -0.4
	await _pose("stair_arrival")
	if not _standing_above(44.5) or int(_native().get_support_entity_id()) != 11 \
			or int(_native().get_death_count()) != 0:
		return _fail("unsupported +44 m arrival %s" % _position())
	_detail = "grade_to_44m=1 moving_stair=1 normal_touch=1 deaths=0 arrival_y=%.3f" % _position().y
	return true


func _touch_upper() -> bool:
	var device := InputRouter.Device.TOUCH
	if not await _touch_stair():
		return false
	if int(_native().get_entity_body_count(2700)) != 1:
		return _fail("upper lift absent from the normal scene")
	for point in [Vector2(4.0, -125.5), Vector2(4.0, -116.7)]:
		if not await _walk_to(device, point, 0.12, 12.0):
			return _fail("upper lift boarding %s" % _position())
	if int(_native().get_support_entity_id()) != 2700:
		return _fail("upper lift did not support boarding %s" % _position())
	await _face(Vector2(0, 1))
	_main._pitch = -0.4
	await _seconds(0.4)
	await _pose("upper_handle_ready")
	if not await _offered(&"pick_up", "GRAB") or int(_native().get_carry_target_entity_id()) != 2703:
		return _fail("upper handle not offered at %s target=%d" % [
			_position(), int(_native().get_carry_target_entity_id())])
	_act(device)
	await _seconds(0.3)
	if int(_native().get_carrying_entity_id()) != 2703:
		return _fail("upper handle was not held")
	_move(device, -0.45)
	await _seconds(0.35)
	_move(device, 0.0)
	if int(_native().get_carrying_entity_id()) == 2703:
		_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(54.5) and \
			int(_native().get_support_entity_id()) == 2700, 20.0):
		return _fail("counterweight never raised the rider %s" % _position())
	await _pose("upper_lift_rising")
	await _seconds(3.0)
	if not _standing_above(55.4) or int(_native().get_support_entity_id()) != 2700:
		return _fail("upper lift did not settle as support %s" % _position())
	await _pose("upper_lift_seated")
	for point in [Vector2(4.0, -122.0), Vector2(4.0, -125.5)]:
		if not await _walk_to(device, point, 0.12, 10.0):
			return _fail("upper lift exit %s" % _position())
	if not _standing_above(55.5) or int(_native().get_support_entity_id()) != 11:
		return _fail("unsupported upper lift exit %s" % _position())
	await _pose("deck_55m")
	for point in [Vector2(4.0, -122.0), Vector2(2.4, -122.0)]:
		if not await _walk_to(device, point, 0.12, 8.0):
			return _fail("upper cabinet approach %s" % _position())
	await _face(Vector2(-1, 0))
	await _seconds(0.4)
	if not await _offered(&"climb", "CLIMB"):
		return _fail("upper cabinet mantle not offered at %s" % _position())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(57.0), 2.5):
		return _fail("upper cabinet mantle %s" % _position())
	if not await _walk_to(device, Vector2(1.4, -122.1), 0.08, 4.0):
		return _fail("upper cabinet launch %s" % _position())
	await _face(Vector2(0, -1))
	await _seconds(0.3)
	_tap(1, _center(&"jump"))
	_move(device, 0.4)
	var caught := await _wait_until(func() -> bool: return bool(_ctx()["hanging"]), 2.5)
	_move(device, 0.0)
	if not caught:
		return _fail("upper duct hang %s" % _position())
	await _pose("upper_duct_hang")
	_tap(1, _center(&"jump"))
	if not await _wait_until(func() -> bool: return _standing_above(60.5), 2.5):
		return _fail("upper duct top-out %s" % _position())
	if not await _walk_to(device, Vector2(5.4, -123.0), 0.08, 10.0):
		return _fail("upper vent approach %s" % _position())
	await _face(Vector2(0, -1))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("upper vent grip not offered at %s" % _position())
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_ctx()["climbing"]), 1.0):
		return _fail("upper vent did not take a hold")
	_move(device, 1.0)
	await _seconds(1.0)
	await _pose("upper_vent_climb")
	var climbed := await _wait_until(func() -> bool: return _standing_above(66.5), 20.0)
	_move(device, 0.0)
	if not climbed:
		return _fail("upper vent top-out %s" % _position())
	if not await _walk_to(device, Vector2(5.4, -125.4), 0.12, 6.0):
		return _fail("upper deck exit %s" % _position())
	await _seconds(0.5)
	await _face(Vector2(0, 1))
	await _pose("upper_arrival")
	if not _standing_above(66.5) or int(_native().get_support_entity_id()) != 11 or \
			int(_native().get_death_count()) != 0:
		return _fail("unsupported +66 m arrival %s" % _position())
	_detail = "grade_to_66m=1 counterweight_lift=1 parkour=1 normal_touch=1 deaths=0 arrival_y=%.3f" % _position().y
	return true

# Focused normal-world mantle: supported entry staging is the only relocation.
# Movement and Action then use the same viewport touch path as ordinary play.
func _touch_ballast_mantle_feedback() -> bool:
	var device := InputRouter.Device.TOUCH
	if not _campaign_world_intact() or not _native().debug_restart_at(Vector3(25.3, 66.9, -139.0)):
		return _fail("ballast mantle supported entry staging rejected")
	await _seconds(0.5)
	if not _ballast_mantle_footing(11):
		return _fail("ballast mantle entry " + _ballast_mantle_state())
	for point in [Vector2(27.2, -139.0), Vector2(27.8, -138.58),
			Vector2(30.5, -138.58), Vector2(29.7, -139.45)]:
		if not await _walk_to(device, point, 0.12, 15.0, true) or not _ballast_mantle_footing(2800):
			return _fail("ballast mantle approach target=%s %s" % [point, _ballast_mantle_state()])
	_move(device, 0.0)
	await _face(Vector2(-1.0, 0.0))
	await _seconds(0.1)
	if not await _offered(&"climb", "CLIMB") or int(_native().get_ledge_entity_id()) != 2801:
		return _fail("ballast roof mantle not offered " + _ballast_mantle_state())
	var before: Dictionary = _native().get_landing_state()
	for field in ["foot_transfer_count", "foot_transfer_tick", "foot_transfer_support_entity_id",
			"foot_transfer_peak_hand_load_n", "player_gravity_factor", "traversal_hand_constraint_count",
			"traversal_command_work_bound_j"]:
		if not before.has(field):
			return _fail("ballast mantle native readback missing " + field)
	var count := int(before["foot_transfer_count"])
	var start_tick := int(_native().get_tick_index())
	var accepted := int(_native().get_accepted_traversal_count())
	var plants := int(_main._audio.foot_plants)
	var command := float(before["traversal_command_work_bound_j"])
	var positive := float(before["hand_actuator_positive_work_j"])
	var last_tick := start_tick
	var loaded := false
	var both_hands := false
	var completed := false
	var peak_load := 0.0
	_act(device)
	var waited := 0.0
	while waited < 2.5:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var state: Dictionary = _native().get_landing_state()
		var tick := int(_native().get_tick_index())
		var left: Vector3 = state["left_hand_force_n"]
		var right: Vector3 = state["right_hand_force_n"]
		var new_command := float(state["traversal_command_work_bound_j"])
		var new_positive := float(state["hand_actuator_positive_work_j"])
		var budget := 6600.0 * float(tick - last_tick) / 90.0 + 0.001
		if int(_native().get_death_count()) != 0 or float(state["player_gravity_factor"]) != 1.0 \
				or not left.is_finite() or not right.is_finite() or left.length() > 1501.0 or right.length() > 1501.0 \
				or not is_finite(new_command) or not is_finite(new_positive) \
				or new_command - command < -0.000001 or new_positive - positive < -0.000001 \
				or new_command - command > budget or new_positive - positive > budget:
			return _fail("ballast mantle force/work/gravity " + _ballast_mantle_state())
		command = new_command
		positive = new_positive
		last_tick = tick
		peak_load = maxf(peak_load, (left + right).length())
		loaded = loaded or left.length() > 0.01 or right.length() > 0.01
		if int(state["foot_transfer_count"]) == count + 1:
			completed = _ballast_mantle_footing(2801)
			break
		if int(_native().get_traversal_state()) != _main.TRAVERSAL_CLIMBING \
				or int(_native().get_traversal_support_entity_id()) != 2801 \
				or int(state["traversal_hand_constraint_count"]) != 2:
			return _fail("ballast mantle lost native two-hand transfer " + _ballast_mantle_state())
		both_hands = true
	var receipt: Dictionary = _native().get_landing_state()
	var receipt_tick := int(receipt["foot_transfer_tick"])
	var receipt_peak := float(receipt["foot_transfer_peak_hand_load_n"])
	if not completed or not loaded or not both_hands \
			or int(_native().get_accepted_traversal_count()) != accepted + 1 \
			or command <= float(before["traversal_command_work_bound_j"]) \
			or positive <= float(before["hand_actuator_positive_work_j"]) \
			or receipt_tick <= start_tick or receipt_tick > last_tick or receipt_tick - start_tick > 153 \
			or int(receipt["foot_transfer_support_entity_id"]) != 2801 \
			or not is_finite(receipt_peak) or receipt_peak <= 0.0 or receipt_peak > 3001.0:
		return _fail("ballast mantle completion " + _ballast_mantle_state())
	await _frames(2)
	if int(_main._audio.foot_plants) != plants + 1 or int(_main._fb_foot_transfer_count) != count + 1 \
			or int(_main._fb_foot_transfer_tick) != receipt_tick:
		return _fail("ballast mantle receipt consumers plants=%d expected=%d %s" % [
			_main._audio.foot_plants, plants + 1, _ballast_mantle_state()])
	waited = 0.0
	while waited < 3.0:
		await get_tree().process_frame
		waited += get_process_delta_time()
		if not _ballast_mantle_footing(2801, false) or int(_native().get_landing_state()["foot_transfer_count"]) != count + 1 \
				or int(_main._audio.foot_plants) != plants + 1:
			return _fail("ballast mantle stable roof/replay " + _ballast_mantle_state())
	if not _ballast_mantle_footing(2801):
		return _fail("ballast mantle roof end slip " + _ballast_mantle_state())
	var roof_support_velocity: Vector3 = _native().get_support_point_linear_velocity()
	var roof_relative_velocity: Vector3 = _velocity() - roof_support_velocity
	for point in [Vector2(27.8, -139.45), Vector2(25.3, -139.0)]:
		var support := 2800 if point.x > 27.0 else 11
		if not await _walk_to(device, point, 0.12, 15.0, true) or not _ballast_mantle_footing(support):
			return _fail("ballast mantle return target=%s %s" % [point, _ballast_mantle_state()])
	if not _campaign_world_intact() or int(_native().get_landing_state()["foot_transfer_count"]) != count + 1 \
			or int(_main._audio.foot_plants) != plants + 1 or int(_main._fb_foot_transfer_count) != count + 1:
		return _fail("ballast mantle return receipt/death " + _ballast_mantle_state())
	_detail = "staging=supported_teeter_entry shipping_touch=1 real_hands=2 gravity=1 command_bound_j=%.3f positive_work_j=%.3f sampled_peak_n=%.3f receipt_peak_n=%.3f foot_transfer_tick=%d foot_plants=1 stable_roof_seconds=3 roof_support_velocity=%s roof_relative_velocity=%s return_beam=1 return_tower=1 deaths=0" % [
		command - float(before["traversal_command_work_bound_j"]), positive - float(before["hand_actuator_positive_work_j"]),
		peak_load, receipt_peak, receipt_tick, roof_support_velocity, roof_relative_velocity]
	return true


func _ballast_mantle_footing(support: int, require_low_slip: bool = true) -> bool:
	var state: Dictionary = _native().get_landing_state()
	var slip: Vector3 = _velocity() - _native().get_support_point_linear_velocity()
	return bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == support \
		and int(_native().get_traversal_state()) == 0 and int(_native().get_death_count()) == 0 \
		and int(state.get("traversal_hand_constraint_count", -1)) == 0 \
		and float(state.get("player_gravity_factor", -1.0)) == 1.0 \
		and (not require_low_slip or Vector2(slip.x, slip.z).length() < 0.2)


func _ballast_mantle_state() -> String:
	return "tick=%d at=%s velocity=%s support_velocity=%s relative_velocity=%s grounded=%s support=%d traversal=%d traversal_support=%d accepted=%d receipt=%s" % [
		_native().get_tick_index(), _position(), _velocity(), _native().get_support_point_linear_velocity(),
		_velocity() - _native().get_support_point_linear_velocity(), _native().is_player_grounded(),
		_native().get_support_entity_id(), _native().get_traversal_state(),
		_native().get_traversal_support_entity_id(), _native().get_accepted_traversal_count(), _native().get_landing_state()]


func _touch_teeter() -> bool:
	if not await _touch_upper():
		return false
	var device := InputRouter.Device.TOUCH
	if int(_native().get_entity_body_count(2800)) != 1:
		return _fail("teeter absent from the normal route")
	var beam := int(_native().get_kit_body_index(2800))
	var ballast := int(_native().get_kit_body_index(2801))
	for point in [Vector2(24.8, -125.4), Vector2(24.8, -139.0), Vector2(25.3, -139.0)]:
		if not await _walk_to(device, point, 0.14, 20.0):
			return _fail("teeter ring approach %s" % _position())
	await _face_teeter_pivot(beam)
	await _pose("teeter_entry")
	if not _standing_above(66.5) or int(_native().get_support_entity_id()) != 11:
		return _fail("teeter entry lost the +66 m ring %s" % _position())
	for point in [Vector2(27.2, -139.0), Vector2(27.8, -139.45)]:
		if not await _walk_to(device, point, 0.14, 12.0):
			return _fail("teeter near-side loading %s" % _position())
	await _face(Vector2(1, 0))
	_main._pitch = -0.08
	await _pose("teeter_ballast_near")
	if _teeter_ballast_x(beam, ballast) > 1.0:
		return _fail("ballast moved before contact %.3f" % _teeter_ballast_x(beam, ballast))
	for point in [Vector2(29.8, -139.45), Vector2(31.95, -139.45)]:
		if not await _walk_to(device, point, 0.14, 12.0):
			return _fail("teeter ballast push %s position=%.3f" % [
				_position(), _teeter_ballast_x(beam, ballast)])
	await _seconds(0.5)
	await _face(Vector2(1, 0))
	_main._pitch = -0.08
	await _pose("teeter_ballast_far")
	if int(_native().get_support_entity_id()) != 2800 or \
			_teeter_ballast_x(beam, ballast) < 4.15:
		return _fail("ballast did not reach the far stop %s position=%.3f" % [
			_position(), _teeter_ballast_x(beam, ballast)])
	if not await _walk_to(device, Vector2(32.8, -138.58), 0.14, 12.0):
		return _fail("teeter far-side loading %s" % _position())
	await _seconds(3.0)
	await _face_teeter_pivot(beam)
	await _pose("teeter_loaded")
	if not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2800 or \
			_teeter_angle(beam) > -0.4:
		return _fail("player contact did not lower the far tip %s angle=%.3f" % [
			_position(), _teeter_angle(beam)])
	if not await _walk_to(device, Vector2(34.4, -139.0), 0.14, 12.0):
		return _fail("teeter receiving shelf %s" % _position())
	await _seconds(3.0)
	await _face(Vector2(-1, 0))
	_main._pitch = -0.16
	await _pose("teeter_shelf")
	if not _standing_above(64.0) or int(_native().get_support_entity_id()) != 1800 or \
			_teeter_angle(beam) < 0.07:
		return _fail("teeter failed to unload onto shelf %s angle=%.3f" % [
			_position(), _teeter_angle(beam)])
	if not await _walk_to(device, Vector2(34.72, -139.45), 0.14, 6.0):
		return _fail("teeter grip approach %s" % _position())
	await _face(Vector2(0, -1))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("teeter upper grip not offered %s" % _position())
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_ctx()["climbing"]), 1.0):
		return _fail("teeter upper grip not taken %s" % _position())
	_move(device, 1.0)
	await _seconds(6.0)
	await _pose("teeter_climb")
	var topped_out := await _wait_until(func() -> bool: return _standing_above(77.5), 20.0)
	_move(device, 0.0)
	if not topped_out:
		return _fail("teeter upper climb did not top out %s" % _position())
	if not await _walk_to(device, Vector2(25.1, -140.8), 0.14, 20.0):
		return _fail("teeter +77 m catwalk exit %s" % _position())
	await _seconds(0.5)
	await _face(Vector2(1, 0))
	_main._pitch = -0.25
	await _pose("teeter_arrival")
	if not _standing_above(77.5) or int(_native().get_support_entity_id()) != 11 or \
			int(_native().get_death_count()) != 0:
		return _fail("teeter +77 m ring unsupported %s" % _position())
	# AS-021 approach framing from the earned ring. This capture proves only
	# the visible layout; the full +88 m touch route is a separate handoff gate.
	if not await _walk_to(device, Vector2(25.1, -131.0), 0.14, 12.0):
		return _fail("braced bay approach along +77 m ring %s" % _position())
	var bay_target := Vector3(31.0, 83.0, -137.0)
	var to_bay: Vector3 = bay_target - _main._camera.global_position
	await _face(Vector2(to_bay.x, to_bay.z))
	_main._pitch = atan2(to_bay.y, Vector2(to_bay.x, to_bay.z).length())
	await _pose("braced_bay_preview")
	_detail = "grade_to_77m=1 ballast_pushed=1 player_loaded_teeter=1 physical_reset=1 normal_touch=1 deaths=0 arrival_y=%.3f" % _position().y
	return true


func _touch_braced_bay() -> bool:
	if not await _touch_teeter():
		return false
	var device := InputRouter.Device.TOUCH
	if int(_native().get_entity_body_count(1900)) != 1:
		return _fail("braced bay absent from normal play")
	if not await _walk_to(device, Vector2(28.0, -131.0), 0.14, 12.0):
		return _fail("braced bay flat entry %s" % _position())
	await _seconds(0.5)
	await _face(Vector2(0, -1))
	_main._pitch = 0.11
	await _pose("braced_bay_entry")
	if not await _walk_to(device, Vector2(28.0, -144.0), 0.14, 16.0):
		return _fail("first inclined girder %s" % _position())
	await _seconds(0.4)
	if not _standing_above(82.6) or int(_native().get_support_entity_id()) != 1900:
		return _fail("first braced bay landing unsupported %s" % _position())
	if not await _walk_to(device, Vector2(29.0, -144.0), 0.14, 4.0):
		return _fail("gap run-up %s" % _position())
	await _seconds(0.4)
	await _face(Vector2(1, 0))
	_main._pitch = -0.08
	await _pose("braced_bay_transfer")
	_move_dir(device, Vector2(0, 1))
	var at_takeoff := await _wait_until(
		func() -> bool: return _position().x >= 30.0 or not bool(_ctx()["grounded"]), 1.0)
	if not at_takeoff or not bool(_ctx()["grounded"]):
		_move_dir(device, Vector2.ZERO)
		return _fail("gap takeoff lost its supported run-up %s" % _position())
	_tap(1, _center(&"jump"))
	var airborne := false
	var landed := false
	var elapsed := 0.0
	while elapsed < 3.0:
		var at := _position()
		var to := Vector2(33.7 - at.x, -144.0 - at.z)
		var world_command := (to / 1.4).limit_length(1.0)
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		_move_dir(device, Vector2(world_command.dot(right), world_command.dot(forward)))
		await get_tree().process_frame
		elapsed += get_process_delta_time()
		airborne = airborne or not bool(_ctx()["grounded"])
		if airborne and bool(_ctx()["grounded"]) and _position().x > 33.2 and absf(_position().y - 82.9) < 0.12:
			landed = true
			break
	_move_dir(device, Vector2.ZERO)
	await _seconds(0.4)
	if not landed or int(_native().get_support_entity_id()) != 1900:
		return _fail("gap jump missed far landing %s" % _position())
	if not await _walk_to(device, Vector2(34.0, -144.0), 0.14, 4.0):
		return _fail("second girder alignment %s" % _position())
	await _seconds(0.4)
	await _face(Vector2(0, 1))
	if not await _walk_to(device, Vector2(34.0, -139.4), 0.14, 8.0):
		return _fail("second girder approach %s" % _position())
	var walked_through := await _walk_to(device, Vector2(34.0, -136.7), 0.14, 3.0)
	if walked_through or _position().z > -137.8:
		return _fail("standing capsule passed through low member %s" % _position())
	await _face(Vector2(0, 1))
	_main._pitch = 0.16
	await _pose("braced_bay_low_member")
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return bool(_native().is_player_crouched()), 0.3):
		return _fail("crouch input failed at braced bay")
	if not await _walk_to(device, Vector2(34.0, -136.7), 0.14, 8.0):
		return _fail("crouched passage blocked %s" % _position())
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return not bool(_native().is_player_crouched()), 0.3):
		return _fail("standing clearance missing beyond low member")
	if not await _walk_to(device, Vector2(34.0, -131.5), 0.14, 10.0):
		return _fail("upper braced bay junction %s" % _position())
	await _seconds(0.4)
	if not _standing_above(87.6) or int(_native().get_support_entity_id()) != 1900:
		return _fail("upper braced bay junction unsupported %s" % _position())
	await _face(Vector2(-1, 0))
	_main._pitch = -0.12
	await _pose("braced_bay_upper_junction")
	if not await _walk_to(device, Vector2(34.0, -132.0), 0.14, 4.0):
		return _fail("return girder turn %s" % _position())
	await _seconds(0.3)
	if not await _walk_to(device, Vector2(26.55, -132.0), 0.14, 13.0):
		return _fail("return girder balance %s" % _position())
	await _face(Vector2(-1, 0))
	await _seconds(0.3)
	if not await _offered(&"climb", "CLIMB"):
		return _fail("tower +88 m mantle unavailable %s" % _position())
	_act(device)
	await _seconds(1.0)
	if not await _walk_to(device, Vector2(24.8, -132.0), 0.14, 5.0):
		return _fail("braced bay ring exit %s" % _position())
	await _seconds(0.4)
	await _face(Vector2(1, 0))
	_main._pitch = -0.12
	await _pose("braced_bay_arrival")
	if not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 11 or \
			absf(_position().y - 88.9) > 0.1 or int(_native().get_death_count()) != 0:
		return _fail("braced bay +88 m ring unsupported %s" % _position())
	_detail = "grade_to_88m=1 gap_jump=1 crouched_portal=1 mantle=1 support=11 deaths=0 arrival_y=%.3f" % _position().y
	return true


func _north_climb(device: int, target_centre_y: float, label: String) -> bool:
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("%s grip not offered at=%s support=%d grounded=%s crouched=%s traversal=%d grip=%s grip_point=%s ledge=%s carrying=%d action=%s view_yaw_rad=%.6f" % [
			label, _position(), _native().get_support_entity_id(), _native().is_player_grounded(),
			_native().is_player_crouched(), _native().get_traversal_state(), _native().is_grip_available(),
			_native().get_grip_point(), _native().is_ledge_available(), _native().get_carrying_entity_id(), _ctx()["action"], _main._yaw])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_ctx()["climbing"]), 1.0):
		return _fail("%s grip not taken %s" % [label, _position()])
	_move(device, 1.0)
	var topped_out := await _wait_until(func() -> bool: return _standing_above(target_centre_y), 15.0)
	_move(device, 0.0)
	if not topped_out or int(_native().get_support_entity_id()) != 1901:
		return _fail("%s top-out unsupported %s" % [label, _position()])
	return true


func _touch_north_grip_diagnostic() -> bool:
	# Explicit supported staging at the recorded failed stance. This bounded
	# reach probe is not evidence of continuous ascent or an authored route.
	if not _campaign_world_intact():
		return false
	if not _native().debug_restart_at(Vector3(16.00084, 88.9, -179.4486)):
		return _fail("north grip diagnostic supported staging rejected")
	await _face(Vector2(0, -1))
	await _seconds(0.5)
	print("SCRAPERX_NORTH_GRIP_DIAGNOSTIC before at=%s support=%d grounded=%s crouched=%s traversal=%d grip=%s grip_point=%s action=%s view_yaw_rad=%.6f" % [
		_position(), _native().get_support_entity_id(), _native().is_player_grounded(),
		_native().is_player_crouched(), _native().get_traversal_state(), _native().is_grip_available(),
		_native().get_grip_point(), _ctx()["action"], _main._yaw])
	if _native().is_grip_available():
		return _fail("recorded north stance already offers native grip; reach hypothesis rejected")
	if not await _walk_to(InputRouter.Device.TOUCH, Vector2(16, -179.55), 0.04, 2.0):
		return _fail("north grip diagnostic closer stance unavailable")
	await _seconds(0.5)
	print("SCRAPERX_NORTH_GRIP_DIAGNOSTIC after at=%s support=%d grounded=%s grip=%s grip_point=%s action=%s" % [
		_position(), _native().get_support_entity_id(), _native().is_player_grounded(),
		_native().is_grip_available(), _native().get_grip_point(), _ctx()["action"]])
	if not _native().is_grip_available() or not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("closer supported stance does not expose native grip; reach hypothesis rejected")
	if not await _north_climb(InputRouter.Device.TOUCH, 94.7, "closer first north climb"):
		return false
	_detail = "staging=supported_recorded_88m_stance continuous_ascent=0 native_grip_before=0 native_grip_after=1 closer_viewport_touch=1 supported_first_topout=1"
	return true


func _touch_north_service_frame() -> bool:
	if not await _touch_braced_bay():
		return false
	var device := InputRouter.Device.TOUCH
	if int(_native().get_entity_body_count(1901)) != 1:
		return _fail("north service frame absent from normal play")
	for point in [Vector2(24.8, -174.5), Vector2(16.0, -174.5)]:
		if not await _walk_to(device, point, 0.14, 20.0):
			return _fail("north frame +88 m entry %s" % _position())
	# Stop within the actual rung search instead of accepting a stance
	# outside native reach. The approach target and grip checks are unchanged.
	if not await _walk_to(device, Vector2(16.0, -179.55), 0.04, 20.0):
		return _fail("north frame +88 m grip stance %s" % _position())
	await _face(Vector2(0, -1))
	_main._pitch = 0.24
	await _pose("north_frame_entry")
	if not await _north_climb(device, 94.7, "first north climb"):
		return false
	await _face(Vector2(-1, 0))
	_main._pitch = -0.14
	await _pose("north_frame_first_rest")
	for point in [Vector2(16.0, -181.2), Vector2(10.0, -181.2), Vector2(10.0, -179.55)]:
		if not await _walk_to(device, point, 0.14, 9.0):
			return _fail("north frame lateral transfer %s" % _position())
	await _face(Vector2(0, 1))
	if not await _north_climb(device, 98.2, "second north climb"):
		return false
	if not await _walk_to(device, Vector2(10.0, -177.55), 0.14, 6.0):
		return _fail("north frame +99 m mantle approach %s" % _position())
	await _face(Vector2(0, 1))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("+99 m mantle not offered %s" % _position())
	_act(device)
	await _seconds(1.0)
	if not await _walk_to(device, Vector2(10.0, -174.5), 0.14, 5.0):
		return _fail("north frame +99 m ring exit %s" % _position())
	await _seconds(0.4)
	if not _standing_above(99.6) or int(_native().get_support_entity_id()) != 11:
		return _fail("north frame +99 m ring unsupported %s" % _position())
	await _face(Vector2(1, 0))
	_main._pitch = -0.14
	await _pose("north_frame_99m")
	# The former 0.14m stop could leave the hand search just outside the
	# rung. Walk into the intended stance; native reach and CLIMB stay exact.
	for point in [Vector2(16.0, -174.5), Vector2(16.0, -179.55)]:
		if not await _walk_to(device, point, 0.04, 9.0):
			return _fail("north frame upper entry %s" % _position())
	await _face(Vector2(0, -1))
	if not await _north_climb(device, 104.2, "upper north climb"):
		return false
	await _face(Vector2(-1, 0))
	_main._pitch = -0.14
	await _pose("north_frame_upper_rest")
	if not await _walk_to(device, Vector2(15.1, -180.6), 0.14, 5.0):
		return _fail("raised lip takeoff %s" % _position())
	await _face(Vector2(-1, 0))
	await _seconds(0.3)
	_tap(1, _center(&"jump"))
	_move(device, 0.5)
	var caught := await _wait_until(func() -> bool: return bool(_ctx()["hanging"]), 2.0)
	_move(device, 0.0)
	if not caught:
		return _fail("raised north lip not caught %s" % _position())
	_main._pitch = 0.08
	await _pose("north_frame_hang")
	# Facing west (yaw +PI/2), view-local right is world north (-Z).
	_move_dir(device, Vector2(1.0, 0.0))
	var crossed := await _wait_until(func() -> bool: return _position().z < -181.85, 5.0)
	_move_dir(device, Vector2.ZERO)
	if not crossed or not bool(_ctx()["hanging"]):
		return _fail("canopy shimmy missed clear pocket %s" % _position())
	await _pose("north_frame_shimmy")
	_tap(1, _center(&"jump"))
	if not await _wait_until(func() -> bool: return _standing_above(106.7), 2.0):
		return _fail("north frame hanging top-out %s" % _position())
	await _face(Vector2(0, -1))
	_main._pitch = -0.14
	await _pose("north_frame_106m")
	if not await _walk_to(device, Vector2(13.0, -182.75), 0.14, 5.0):
		return _fail("final north ladder approach %s" % _position())
	await _face(Vector2(0, -1))
	if not await _north_climb(device, 109.2, "final north climb"):
		return false
	for point in [Vector2(15.5, -183.8), Vector2(15.5, -177.55)]:
		if not await _walk_to(device, point, 0.14, 9.0):
			return _fail("high catwalk to tower %s" % _position())
	await _face(Vector2(0, 1))
	_main._pitch = -0.14
	await _pose("north_frame_upper_exit")
	if not await _offered(&"climb", "CLIMB"):
		return _fail("+110 m mantle not offered %s" % _position())
	_act(device)
	await _seconds(1.0)
	if not await _walk_to(device, Vector2(15.5, -174.5), 0.14, 5.0):
		return _fail("north frame +110 m ring exit %s" % _position())
	await _seconds(0.4)
	await _face(Vector2(0, -1))
	_main._pitch = -0.22
	await _pose("north_frame_110m")
	if not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 11 or \
			absf(_position().y - 110.9) > 0.1 or int(_native().get_death_count()) != 0:
		return _fail("+110 m tower ring unsupported %s" % _position())
	_detail = "grade_to_110m=1 north_frame=1 hanging_shimmy=1 support=11 deaths=0 arrival_y=%.3f" % _position().y
	return true


func _face_teeter_pivot(body: int) -> void:
	var pose: Transform3D = _native().get_kit_body_transform(body)
	var at := _position()
	await _face(Vector2(pose.origin.x - at.x, pose.origin.z - at.z))
	var to_pivot: Vector3 = pose.origin - _main._camera.global_position
	_main._pitch = atan2(to_pivot.y, Vector2(to_pivot.x, to_pivot.z).length())


func _teeter_angle(body: int) -> float:
	var pose: Transform3D = _native().get_kit_body_transform(body)
	return pose.basis.get_euler().z


func _teeter_ballast_x(beam: int, ballast: int) -> float:
	var beam_pose: Transform3D = _native().get_kit_body_transform(beam)
	var carriage_pose: Transform3D = _native().get_kit_body_transform(ballast)
	return (beam_pose.affine_inverse() * carriage_pose.origin).x


func _standing_above(height: float) -> bool:
	return bool(_ctx()["grounded"]) and int(_ctx()["traversal"]) == 0 and _position().y > height

func _campaign_world_intact() -> bool:
	# The native launcher and absent retired bridge identify the normal world;
	# presentation's regression flag alone cannot establish native identity.
	if _main._regression_scene or not bool(_native().get_slingshot_state().get("available", false)) \
			or int(_native().get_entity_body_count(2500)) != 0:
		return _fail("campaign selected a fixture world")
	if int(_native().get_death_count()) != 0 or float(_native().get_slingshot_state()["work_j"]) != 0.0:
		return _fail("campaign used death recovery or launcher work at %s" % _position())
	return true


func _touch_campaign_to_121() -> bool:
	if not _campaign_world_intact():
		return false
	# Existing upper-route helpers use the cargo-net opener only for this
	# scenario. No fixture selection, restart or player relocation occurs.
	if not await _touch_north_service_frame():
		return false
	if not _campaign_world_intact():
		return false
	print("SCRAPERX_CAMPAIGN north_frame_exit at=%s support=%d" % [_position(), _native().get_support_entity_id()])
	if not await _touch_suspended_ladder(false):
		return false
	if not _campaign_world_intact():
		return false
	if not _standing_above(121.7) \
			or absf(_position().y - 121.9) > 0.15 or int(_native().get_support_entity_id()) != 11:
		return _fail("campaign +121m arrival unsupported at=%s" % _position())
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 supported_height_m=121 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_campaign_to_143() -> bool:
	if not await _touch_campaign_to_121():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=121 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_service_lift(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 supported_height_m=143 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_service_lift(stage_at_121: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_121:
		if not _native().debug_restart_at(Vector3(-25, 121.9, -162)):
			return _fail("service lift lower-ring staging rejected")
		await _seconds(0.5)
	else:
		for point in [Vector2(-22, -174.5), Vector2(-22, -162), Vector2(-25, -162)]:
			if not await _walk_to(device, point, 0.12, 10.0):
				return _fail("121m ring approach to service lift failed at=%s" % _position())
	if not await _walk_to(device, Vector2(-27.4, -162), 0.12, 6.0):
		return _fail("lower landing approach failed")
	if int(_native().get_service_lift_state()["station"]) != 2:
		return _fail("lower call station is not reachable")
	if not await _walk_to(device, Vector2(-30.8, -162), 0.12, 6.0):
		return _fail("physical lift boarding failed")
	await _face(Vector2(1.1, -0.75))
	if int(_native().get_support_entity_id()) != 2973 or not await _offered(&"operate", "OPERATE", "SERVICE LIFT"):
		return _fail("deck station does not offer OPERATE at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"service_lift" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("service lift pendant did not open")
	var before: Dictionary = _native().get_service_lift_state()
	_touch(1, _center(&"pendant_up"), true)
	await _seconds(3.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var paused: Dictionary = _native().get_service_lift_state()
	if float(paused["surface_y"]) < float(before["surface_y"]) + 0.5 or not bool(paused["braking"]):
		return _fail("touch hold/release did not drive and brake native lift")
	var stopped_height := float(paused["surface_y"])
	await _seconds(0.5)
	if absf(float(_native().get_service_lift_state()["surface_y"]) - stopped_height) > 0.02:
		return _fail("released lift does not hold its height")
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_service_lift_state()["surface_y"]) >= 142.98, 65.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	if not reached or not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2973:
		return _fail("touch ride failed before143m receiver at=%s station=%s" % [_position(), _native().get_service_lift_state()])
	var arrival: Dictionary = _native().get_service_lift_state()
	if not bool(arrival["braking"]) or float(arrival["energy_j"]) >= float(before["energy_j"]) or float(arrival["energy_j"]) <= 0:
		return _fail("lift arrival is not a finite-energy braked state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("DONE did not release lift operation")
	if not await _walk_to(device, Vector2(-24.7, -162), 0.12, 8.0):
		return _fail("physical143m exit failed at=%s" % _position())
	if not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 11 or absf(_position().y - 143.9) > 0.15:
		return _fail("143m arrival is not supported by Tower11")
	_detail = "staging=supported_121m shipping_touch=1 hold_release=1 finite_energy=1 supported_height_m=143 support=11 deaths=%d" % int(_native().get_death_count())
	return true


func _touch_campaign_to_165() -> bool:
	if not await _touch_campaign_to_143():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=143 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_balance_lift(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 hold_release=1 supported_height_m=165 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_balance_lift(stage_at_143: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	# Only the focused slice uses explicit supported staging. From here onward
	# both scenarios walk, jump and operate through ordinary viewport touch.
	if stage_at_143:
		if not _native().debug_restart_at(Vector3(-24.7, 143.9, -162)):
			return _fail("gravity balance +143m staging rejected")
		await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(143.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("gravity balance approach requires supported +143m Tower11")
	for point in [Vector2(-24.7, -142), Vector2(-27.5, -142.65)]:
		if not await _walk_to(device, point, 0.12, 10.0):
			return _fail("gravity balance lower landing approach at=%s" % _position())
	if int(_native().get_supplied_machine_state()["station"]) != 2:
		return _fail("gravity balance lower call station is not reachable")
	# The landing and moving deck have a real 0.3m gap. Jump from the
	# supported landing; the native body/contact owner decides the transfer.
	if not await _walk_to(device, Vector2(-28.45, -142.6), 0.08, 5.0):
		return _fail("gravity balance boarding takeoff at=%s" % _position())
	await _face(Vector2(-1, 0))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(-32.8, -142.5), 0.10, 8.0):
		return _fail("gravity balance physical boarding at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 2980, 2.0):
		return _fail("gravity balance boarding is not supported by deck2980")
	await _face(Vector2(0, -1))
	if int(_native().get_supplied_machine_state()["station"]) != 1 \
			or not await _offered(&"operate", "OPERATE", "GRAVITY BALANCE"):
		return _fail("gravity balance deck control unavailable at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("gravity balance touch pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), true)
	var moved := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= float(before["surface_y"]) + 0.8, 12.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var stopped: Dictionary = _native().get_supplied_machine_state()
	if not moved or not bool(stopped["braking"]) or int(_native().get_support_entity_id()) != 2980:
		return _fail("gravity balance UP/release did not move and brake the ridden deck")
	var stopped_height := float(stopped["surface_y"])
	await _seconds(0.5)
	if absf(float(_native().get_supplied_machine_state()["surface_y"]) - stopped_height) > 0.02:
		return _fail("gravity balance released brake does not hold height")
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 164.98, 65.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not reached or not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2980:
		return _fail("gravity balance touch ride failed at=%s station=%s" % [_position(), arrival])
	if not bool(arrival["braking"]) or float(arrival["energy_j"]) < 0.0 \
			or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) > float(before["energy_j"]):
		return _fail("gravity balance arrival violates finite energy/brake state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity balance DONE did not release operation")
	# Jump the same native gap onto the upper receiver, then walk into Tower11.
	if not await _walk_to(device, Vector2(-29.8, -142.65), 0.08, 7.0):
		return _fail("gravity balance upper exit takeoff at=%s" % _position())
	await _face(Vector2(1, 0))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(-27.5, -142.65), 0.10, 7.0):
		return _fail("gravity balance upper receiver transfer at=%s" % _position())
	if stage_at_143:
		if not await _touch_balance_upper_recall():
			return false
	if not await _walk_to(device, Vector2(-24.7, -142), 0.12, 7.0):
		return _fail("gravity balance +165m Tower exit at=%s" % _position())
	if not _standing_above(165.7) or absf(_position().y - 165.9) > 0.15 \
			or int(_native().get_support_entity_id()) != 11 or not _campaign_world_intact():
		return _fail("gravity balance +165m arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=%s shipping_touch=1 gravity_balance=1 hold_release=1 finite_energy=1 upper_recall=%d supported_height_m=165 support=11 deaths=0" % [
		"supported_143m" if stage_at_143 else "ordinary_grade_spawn", int(stage_at_143)]
	return true


func _touch_balance_upper_recall() -> bool:
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_supplied_machine_state()["station"]) == 3, 2.0) \
			or not await _offered(&"operate", "OPERATE", "GRAVITY BALANCE"):
		return _fail("gravity balance upper call station unavailable at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_down"):
		return _fail("gravity balance upper call pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_down"), true)
	var lowered := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) <= 163.8, 12.0)
	_touch(1, _center(&"pendant_down"), false)
	await _seconds(0.5)
	if not lowered or not bool(_native().get_supplied_machine_state()["braking"]) \
			or int(_native().get_support_entity_id()) not in [11, 1982] \
			or not bool(_ctx()["grounded"]):
		return _fail("gravity balance upper station return did not brake from fixed footing")
	_touch(1, _center(&"pendant_up"), true)
	var recalled := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 164.98, 12.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var after: Dictionary = _native().get_supplied_machine_state()
	if not recalled or not bool(after["braking"]) or float(after["energy_j"]) < 0.0 \
			or float(after["energy_j"]) >= float(before["energy_j"]):
		return _fail("gravity balance upper recall did not spend finite reset energy")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity balance upper call DONE did not release operation")
	return true


func _touch_campaign_to_198() -> bool:
	if not await _touch_campaign_to_165():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=165 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_crown_gondola(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 supported_height_m=198 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_crown_gondola(stage_at_165: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_165:
		if not _native().debug_restart_at(Vector3(-24.7, 165.9, -159.8)):
			return _fail("crown gondola +165m staging rejected")
		await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(165.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("crown gondola approach requires supported +165m Tower11")
	for point in [Vector2(-24.7, -160.45), Vector2(-40.5, -160.45)]:
		if not await _walk_to(device, point, 0.12, 12.0):
			return _fail("crown gondola lower landing approach at=%s" % _position())
	var lower: Dictionary = _native().get_supplied_machine_state()
	if String(lower["name"]) != "CROWN GONDOLA" or int(lower["station"]) != 2:
		return _fail("crown gondola lower station is not reachable")
	if not await _walk_to(device, Vector2(-44, -159), 0.08, 6.0):
		return _fail("crown gondola boarding takeoff at=%s" % _position())
	await _face(Vector2(0, 1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(-44, -156.1), 0.10, 8.0, true):
		return _fail("crown gondola physical boarding at=%s" % _position())
	var cabin := int(_native().get_kit_body_index(2983))
	if cabin < 0:
		return _fail("crown gondola cabin2983 body unavailable")
	if not await _wait_crown_boarded(cabin):
		_crown_ride_diagnostic(cabin, "boarding_floor_unstable")
		return _fail("crown gondola boarding lacks stable cabin floor contact at=%s" % _position())
	if String(_native().get_supplied_machine_state()["name"]) != "CROWN GONDOLA" \
			or int(_native().get_supplied_machine_state()["station"]) != 1 \
			or not await _offered(&"operate", "OPERATE", "CROWN GONDOLA"):
		return _fail("crown gondola cabin control unavailable at=%s" % _position())
	_crown_ride_diagnostic(cabin, "before_operate")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("crown gondola touch pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), true)
	var ride_tick := int(_native().get_tick_index())
	var sample_tick := ride_tick - 15
	var ride_seconds := 0.0
	while ride_seconds < 2.0:
		var tick := int(_native().get_tick_index())
		if tick - sample_tick >= 15:
			_crown_ride_diagnostic(cabin, "up")
			sample_tick = tick
		var state: Dictionary = _native().get_supplied_machine_state()
		if _main._operating != &"supplied_machine" or (not _native().is_player_grounded() \
				and int(_native().get_support_entity_id()) != 2983 \
				and _position().y < float(state["surface_y"]) - 0.5):
			_crown_ride_diagnostic(cabin, "lost_operation_or_below_cabin")
			_touch(1, _center(&"pendant_up"), false)
			return _fail("crown gondola lost cabin operation/footing during initial UP")
		await get_tree().process_frame
		ride_seconds = float(int(_native().get_tick_index()) - ride_tick) \
			* float(_native().get_fixed_step_seconds())
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 197.97, maxf(0.1, 90.0 - ride_seconds))
	_touch(1, _center(&"pendant_up"), false)
	if not reached:
		return _fail("crown gondola touch ride failed at=%s station=%s" % [
			_position(), _native().get_supplied_machine_state()])
	# The wheel brake does not freeze its passive cabin hinge. Wait for
	# sustained native contact, low cabin speed and an upright walking floor.
	if not await _wait_crown_settled(cabin):
		return _fail("crown gondola upper cabin did not settle at=%s" % _position())
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if String(arrival["name"]) != "CROWN GONDOLA" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return _fail("crown gondola arrival violates finite energy/brake state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("crown gondola DONE did not release operation")
	var pose: Transform3D = _native().get_kit_body_transform(cabin)
	if not await _walk_to(device, Vector2(pose.origin.x, pose.origin.z - 1.5), 0.08, 7.0):
		return _fail("crown gondola upper exit takeoff at=%s" % _position())
	await _face(Vector2(0, -1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(_position().x, -159.8), 0.10, 7.0):
		return _fail("crown gondola upper receiver transfer at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 1985, 3.0):
		return _fail("crown gondola upper transfer missed the actual start platform1985")
	if stage_at_165 and not await _touch_crown_swing_recovery():
		return false
	if not await _touch_crown_swing_exit():
		return false
	if stage_at_165 and not await _touch_crown_upper_recall():
		return false
	for point in [Vector2(-24.7, -160.45), Vector2(-24.7, -159.8)]:
		if not await _walk_to(device, point, 0.12, 12.0):
			return _fail("crown gondola +198m Tower exit at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return _standing_above(198.7) and absf(_position().y - 198.9) <= 0.15 \
			and int(_native().get_support_entity_id()) == 11, 3.0) or not _campaign_world_intact():
		return _fail("crown gondola +198m arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=%s shipping_touch=1 crown_gondola=1 passive_cabin_settled=1 hanging_swing=1 earned_release=1 finite_energy=1 missed_release_recovery=%d upper_recall=%d supported_height_m=198 support=11 deaths=0" % [
		"supported_165m" if stage_at_165 else "ordinary_grade_spawn", int(stage_at_165), int(stage_at_165)]
	return true


func _touch_crown_swing() -> bool:
	if not _native().debug_restart_at(Vector3(-41.0759, 198.9, -160.45)):
		return _fail("crown swing +198m start platform staging rejected")
	await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(198.7) \
			or int(_native().get_support_entity_id()) != 1985:
		return _fail("crown swing staging lacks supported start platform1985")
	if not await _touch_crown_swing_recovery() or not await _touch_crown_swing_exit():
		return false
	for point in [Vector2(-24.7, -160.45), Vector2(-24.7, -159.8)]:
		if not await _walk_to(InputRouter.Device.TOUCH, point, 0.12, 12.0):
			return _fail("crown swing Tower exit at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return _standing_above(198.7) and absf(_position().y - 198.9) <= 0.15 \
			and int(_native().get_support_entity_id()) == 11, 3.0) or not _campaign_world_intact():
		return _fail("crown swing arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=supported_198m_start_platform shipping_touch=1 missed_release_recovery=1 hanging_swing=1 earned_release=1 supported_height_m=198 support=11 deaths=0"
	return true


func _crown_swing_catch_diagnostic(phase: String) -> void:
	var bar := int(_native().get_kit_body_index(2930))
	var bar_pose: Variant = _native().get_kit_body_transform(bar) if bar >= 0 else "unavailable"
	var landing: Dictionary = _native().get_landing_state()
	var move: Vector2 = _main._touch.move_vector
	var facing := Vector2(-sin(_main._yaw), -cos(_main._yaw))
	var right := Vector2(cos(_main._yaw), -sin(_main._yaw))
	print("SCRAPERX_CROWN_CATCH phase=%s tick=%d player=%s velocity=%s grounded=%s crouched=%s support=%d traversal=%d traversal_support=%d left_hand=%s right_hand=%s hands=%s player_swinging=%s facing_from_yaw=%s touch_move=%s world_move_from_touch=%s bar_transform=%s" % [
		phase, _native().get_tick_index(), _position(), _velocity(), _native().is_player_grounded(),
		_native().is_player_crouched(), _native().get_support_entity_id(), _native().get_traversal_state(),
		_native().get_traversal_support_entity_id(), _native().get_traversal_left_hand(),
		_native().get_traversal_right_hand(), landing.get("traversal_hand_constraint_count", "not_exposed"),
		landing["player_swinging"], facing, move, right * move.x + facing * move.y, bar_pose])


func _touch_crown_swing_catch() -> bool:
	var device := InputRouter.Device.TOUCH
	for point in [Vector2(-41.0759, -160.45), Vector2(-38.1, -160.45)]:
		if not await _walk_to(device, point, 0.08, 7.0, true):
			return _fail("crown hanging arm takeoff approach at=%s" % _position())
	await _face(Vector2(1, 0))
	var takeoff_slip: Vector3 = _velocity() - _native().get_support_point_linear_velocity()
	if not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 1985 \
			or Vector2(takeoff_slip.x, takeoff_slip.z).length() >= 0.15:
		_crown_swing_catch_diagnostic("takeoff_unsettled")
		return _fail("crown hanging arm Jump requires settled start platform1985 at=%s" % _position())
	var catch_tick := int(_native().get_tick_index())
	var sample_tick := catch_tick
	_move(device, 1.0)
	_crown_swing_catch_diagnostic("start")
	_tap(1, _center(&"jump"))
	while int(_native().get_tick_index()) - catch_tick < 100 \
			and not bool(_native().get_landing_state()["player_swinging"]):
		var tick := int(_native().get_tick_index())
		if tick - sample_tick >= 15:
			_crown_swing_catch_diagnostic("approach")
			sample_tick = tick
		await get_tree().process_frame
	_crown_swing_catch_diagnostic("finish")
	_move(device, 0.0)
	if not _crown_swing_attached():
		return _fail("crown ordinary jump did not catch arm2930 at=%s" % _position())
	await _seconds(2.0)
	if not _crown_swing_attached():
		return _fail("crown hanging arm lost its native neutral hold")
	return true


func _touch_crown_swing_exit() -> bool:
	if not await _touch_crown_swing_catch():
		return false
	var device := InputRouter.Device.TOUCH
	var pump_tick := int(_native().get_tick_index())
	var released := false
	while float(int(_native().get_tick_index()) - pump_tick) * float(_native().get_fixed_step_seconds()) < 180.0:
		if not _crown_swing_attached() or int(_native().get_death_count()) != 0:
			_move(device, 0.0)
			return _fail("crown deliberate lean lost its native hanging support at=%s" % _position())
		var velocity := _velocity()
		if _position().x > -32.4 and velocity.x > 2.5 and velocity.y > 1.5:
			_move(device, 0.0)
			_tap(1, _center(&"jump"))
			released = true
			break
		# Phase choice from observed hand position and motion; this proves the
		# physical route, without establishing human usability.
		# Native hands bound lean to +/-0.15m and stroke to 0.6m/s. The 500W
		# budget meters positive 1/2*k*extension^2 target work; positive/absorbed
		# receipts are distinct from Fmax-distance and full energy closure.
		var hand: Vector3 = _native().get_traversal_left_hand()
		_move(device, 1.0 if velocity.x - 1.2 * (hand.x + 36.95) > 0.0 else -1.0)
		await get_tree().process_frame
	_move(device, 0.0)
	if not released:
		return _fail("crown deliberate lean never earned the rising release window")
	if not await _wait_until(func() -> bool:
		return not bool(_native().get_landing_state()["player_swinging"]), 0.5):
		return _fail("crown Jump did not release the hanging arm")
	# Coast with neutral input. A real landing brace can lower the capsule to
	# 198.59m; grounded contact on the narrow receiver establishes this arrival.
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 1985 \
			and _position().x > -30.05 and _position().y > 198.3, 3.0):
		return _fail("crown passive release missed the narrow receiver at=%s" % _position())
	return _campaign_world_intact()


func _touch_crown_swing_recovery() -> bool:
	if not await _touch_crown_swing_catch():
		return false
	var device := InputRouter.Device.TOUCH
	var miss_tick := int(_native().get_tick_index())
	var miss_available := false
	while float(int(_native().get_tick_index()) - miss_tick) * float(_native().get_fixed_step_seconds()) < 10.0:
		if not _crown_swing_attached():
			return _fail("crown recovery miss lost its hanging support before release")
		var velocity := _velocity()
		if _position().x > -36.4 and velocity.x > 0.0 and velocity.x < 2.5:
			miss_available = true
			break
		await get_tree().process_frame
	if not miss_available:
		return _fail("crown recovery miss never reached its low-speed release window")
	# Release over the gap with insufficient forward speed to reach the receiver.
	_move(device, 0.0)
	_tap(1, _center(&"jump"))
	if not await _wait_until(func() -> bool:
		return not bool(_native().get_landing_state()["player_swinging"]) \
			and bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 1931, 5.0):
		return _fail("crown missed release did not reach the real recovery tray at=%s" % _position())
	if not await _walk_to(device, Vector2(-37.0, -159.8), 0.08, 7.0):
		return _fail("crown recovery ladder approach at=%s" % _position())
	await _face(Vector2(-1, 0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("crown recovery ladder CLIMB was not offered")
	_act(device)
	await _frames(2)
	if int(_native().get_traversal_state()) != _main.TRAVERSAL_CLIMBING \
			or int(_native().get_traversal_support_entity_id()) != 1931:
		return _fail("crown recovery ladder did not acquire native climbing hands")
	_move(device, 1.0)
	var recovered := await _wait_until(func() -> bool:
		return _standing_above(198.7) and int(_native().get_support_entity_id()) == 1985 \
			and not bool(_native().get_landing_state()["player_swinging"]), 35.0)
	_move(device, 0.0)
	if not recovered or not _campaign_world_intact():
		return _fail("crown physical recovery climb did not reach the start platform at=%s" % _position())
	await _seconds(1.0)
	if not await _walk_to(device, Vector2(-41.0759, -160.45), 0.08, 7.0):
		return _fail("crown recovered retry approach at=%s" % _position())
	return true


func _crown_swing_attached() -> bool:
	return bool(_native().get_landing_state()["player_swinging"]) \
		and int(_native().get_traversal_state()) == _main.TRAVERSAL_CLIMBING \
		and int(_native().get_traversal_support_entity_id()) == 2930


func _touch_crown_upper_recall() -> bool:
	if not await _walk_to(InputRouter.Device.TOUCH, Vector2(-26.7379, -160.1), 0.08, 7.0):
		return _fail("crown far-side upper call approach at=%s" % _position())
	var before: Dictionary = _native().get_supplied_machine_state()
	if int(before["index"]) != 1 or int(before["station"]) != 3 \
			or not await _offered(&"operate", "OPERATE", "CROWN GONDOLA"):
		return _fail("crown far-side upper call station is not reachable")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_down"):
		return _fail("crown upper call touch pendant did not open")
	_touch(1, _center(&"pendant_down"), true)
	var lowered := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) < 197.9, 12.0)
	_touch(1, _center(&"pendant_down"), false)
	await _seconds(0.5)
	var after: Dictionary = _native().get_supplied_machine_state()
	if not lowered or int(after["index"]) != 1 or not bool(after["braking"]) \
			or float(after["energy_j"]) < 0.0 or float(after["energy_j"]) >= float(before["energy_j"]) \
			or int(_native().get_support_entity_id()) != 1985 or not bool(_ctx()["grounded"]):
		return _fail("crown far-side recall did not move and brake from fixed receiver footing")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("crown upper call DONE did not release operation")
	return true


func _wait_crown_boarded(cabin: int) -> bool:
	var start_tick := int(_native().get_tick_index())
	var stable_tick := -1
	var step := float(_native().get_fixed_step_seconds())
	while float(int(_native().get_tick_index()) - start_tick) * step < 3.0:
		var tick := int(_native().get_tick_index())
		var pose: Transform3D = _native().get_kit_body_transform(cabin)
		var slip: Vector3 = _velocity() - _native().get_support_point_linear_velocity()
		# The control panel belongs to cabin2983 too. Its top can provide
		# transient support; require the actual floor height and settled feet.
		var ready := bool(_native().is_player_grounded()) \
			and int(_native().get_support_entity_id()) == 2983 \
			and absf(_position().y - (pose.origin.y + 1.15)) <= 0.15 \
			and Vector2(slip.x, slip.z).length() < 0.15
		if ready:
			if stable_tick < 0:
				stable_tick = tick
			if float(tick - stable_tick) * step >= 0.3:
				return true
		else:
			stable_tick = -1
		await get_tree().process_frame
	return false


func _crown_ride_diagnostic(cabin: int, phase: String) -> void:
	var pose: Transform3D = _native().get_kit_body_transform(cabin)
	var landing: Dictionary = _native().get_landing_state()
	var support_velocity: Vector3 = _native().get_support_point_linear_velocity()
	var move: Vector2 = _main._touch.move_vector
	var forward := Vector2(-sin(_main._yaw), -cos(_main._yaw))
	var right := Vector2(cos(_main._yaw), -sin(_main._yaw))
	print("SCRAPERX_CROWN_RIDE phase=%s tick=%d player=%s velocity=%s grounded=%s crouched=%s support=%d support_velocity=%s slip=%s traversal=%d traversal_support=%d left_hand=%s right_hand=%s gravity=%s hands=%s landing=%s cabin_origin=%s cabin_rotation=%s cabin_velocity=%s touch_move=%s world_move=%s pendant=%s operating=%s machine=%s" % [
		phase, _native().get_tick_index(), _position(), _velocity(), _native().is_player_grounded(),
		_native().is_player_crouched(), _native().get_support_entity_id(), support_velocity,
		_velocity() - support_velocity, _native().get_traversal_state(),
		_native().get_traversal_support_entity_id(), _native().get_traversal_left_hand(),
		_native().get_traversal_right_hand(), landing.get("player_gravity_factor", "not_exposed"),
		landing.get("traversal_hand_constraint_count", "not_exposed"), landing, pose.origin,
		pose.basis.get_euler(), _native().get_kit_body_linear_velocity(cabin), move,
		right * move.x + forward * move.y, _main._touch.pendant_axes, _main._operating,
		_native().get_supplied_machine_state()])


func _wait_crown_settled(cabin: int) -> bool:
	var waited := 0.0
	var settled := 0.0
	while waited < 20.0:
		var pose: Transform3D = _native().get_kit_body_transform(cabin)
		var state: Dictionary = _native().get_supplied_machine_state()
		var ready := bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 2983 \
			and bool(state["braking"]) and float(state["surface_y"]) >= 197.97 \
			and (_native().get_kit_body_linear_velocity(cabin) as Vector3).length() < 0.15 \
			and pose.basis.y.dot(Vector3.UP) > 0.995
		settled = settled + get_process_delta_time() if ready else 0.0
		if settled >= 0.5:
			return true
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


func _touch_campaign_to_231() -> bool:
	if not await _touch_campaign_to_198():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=198 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_traction_tram(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 supported_height_m=231 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_traction_tram(stage_at_198: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	# The focused slice stages only its initial Tower footing. The campaign
	# continues from Crown; both use ordinary touch for the entire tram route.
	if stage_at_198:
		if not _native().debug_restart_at(Vector3(-24.7, 198.9, -159.8)):
			return _fail("traction tram +198m staging rejected")
		await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(198.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("traction tram approach requires supported +198m Tower11")
	# Step around the landing panel post before approaching its deck side.
	for point in [Vector2(-22.5, -159.8), Vector2(-22.5, -125), Vector2(-23.15, -125),
			Vector2(-23.15, -122.75), Vector2(-22.5, -122.75)]:
		if not await _walk_to(device, point, 0.10, 15.0):
			return _fail("traction tram lower landing approach at=%s" % _position())
	var lower: Dictionary = _native().get_supplied_machine_state()
	if String(lower["name"]) != "TRACTION TRAM" or int(lower["station"]) != 2:
		return _fail("traction tram lower call station is not reachable at=%s state=%s support=%s" % [_position(), lower, _native().get_support_entity_id()])
	for point in [Vector2(-23.15, -119.1), Vector2(-22.5, -119.1)]:
		if not await _walk_to(device, point, 0.10, 8.0):
			return _fail("traction tram physical boarding at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 2984, 2.0):
		return _fail("traction tram boarding is not supported by deck2984")
	await _face(Vector2(0, -1))
	var deck := int(_native().get_kit_body_index(2984))
	if deck < 0 or String(_native().get_supplied_machine_state()["name"]) != "TRACTION TRAM" \
			or int(_native().get_supplied_machine_state()["station"]) != 1 \
			or not await _offered(&"operate", "OPERATE", "TRACTION TRAM"):
		return _fail("traction tram deck control unavailable at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("traction tram touch pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 230.98, 90.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not reached or not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2984:
		return _fail("traction tram touch ride failed at=%s station=%s" % [_position(), arrival])
	if String(arrival["name"]) != "TRACTION TRAM" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return _fail("traction tram arrival violates finite energy/brake state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("traction tram DONE did not release operation")
	var pose: Transform3D = _native().get_kit_body_transform(deck)
	for point in [Vector2(pose.origin.x - 0.65, -119.1), Vector2(pose.origin.x - 0.65, -122.75)]:
		if not await _walk_to(device, point, 0.10, 8.0):
			return _fail("traction tram upper receiver transfer at=%s" % _position())
	if not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 1988:
		return _fail("traction tram upper transfer missed the actual receiver1988")
	for point in [Vector2(pose.origin.x - 0.65, -127), Vector2(pose.origin.x, -127)]:
		if not await _walk_to(device, point, 0.10, 8.0):
			return _fail("traction tram +231m Tower exit at=%s" % _position())
	if not _standing_above(231.7) or absf(_position().y - 231.9) > 0.15 \
			or int(_native().get_support_entity_id()) != 11 or not _campaign_world_intact():
		return _fail("traction tram +231m arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=%s shipping_touch=1 traction_tram=1 finite_energy=1 supported_height_m=231 support=11 deaths=0" % [
		"supported_198m" if stage_at_198 else "ordinary_grade_spawn"]
	return true


func _touch_campaign_to_253() -> bool:
	if not await _touch_campaign_to_231():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=231 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_barrel_helix(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 barrel_helix=1 supported_height_m=253 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_barrel_helix(stage_at_231: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_231:
		if not _native().debug_restart_at(Vector3(24.63, 231.9, -127)):
			return _fail("barrel helix +231m staging rejected")
		await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(231.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("barrel helix approach requires supported +231m Tower11")
	for point in [Vector2(23, -127), Vector2(-22.5, -127), Vector2(-22.5, -142),
			Vector2(-24.7, -142.65), Vector2(-27.5, -142.65)]:
		if not await _walk_to(device, point, 0.10, 15.0):
			return _fail("barrel helix lower landing approach at=%s" % _position())
	var lower: Dictionary = _native().get_supplied_machine_state()
	if String(lower["name"]) != "BARREL HELIX" or int(lower["station"]) != 2:
		return _fail("barrel helix lower call station is not reachable")
	if not await _walk_to(device, Vector2(-31.3, -142.5), 0.10, 8.0):
		return _fail("barrel helix physical boarding at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 2988, 2.0):
		return _fail("barrel helix boarding is not supported by deck2988")
	await _face(Vector2(0, -1))
	if String(_native().get_supplied_machine_state()["name"]) != "BARREL HELIX" \
			or int(_native().get_supplied_machine_state()["station"]) != 1 \
			or not await _offered(&"operate", "OPERATE", "BARREL HELIX"):
		return _fail("barrel helix deck control unavailable at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("barrel helix touch pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 252.98, 65.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not reached or not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2988:
		return _fail("barrel helix touch ride failed at=%s station=%s" % [_position(), arrival])
	if String(arrival["name"]) != "BARREL HELIX" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return _fail("barrel helix arrival violates finite energy/brake state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("barrel helix DONE did not release operation")
	for point in [Vector2(-27.5, -142.65), Vector2(-24.7, -142.65)]:
		if not await _walk_to(device, point, 0.10, 8.0):
			return _fail("barrel helix +253m receiver/Tower exit at=%s" % _position())
	if not _standing_above(253.7) or absf(_position().y - 253.9) > 0.15 \
			or int(_native().get_support_entity_id()) != 11 or not _campaign_world_intact():
		return _fail("barrel helix +253m arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=%s shipping_touch=1 barrel_helix=1 finite_energy=1 supported_height_m=253 support=11 deaths=0" % [
		"supported_231m" if stage_at_231 else "ordinary_grade_spawn"]
	return true


func _touch_campaign_to_286() -> bool:
	if not await _touch_campaign_to_253():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=253 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_cascade_mast(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 barrel_helix=1 cascade_mast=1 supported_height_m=286 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_cascade_mast(stage_at_253: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_253:
		if not _native().debug_restart_at(Vector3(-24.7, 253.9, -142.65)):
			return _fail("cascade mast +253m staging rejected")
		await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(253.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("cascade mast approach requires supported +253m Tower11")
	for point in [Vector2(-24.7, -161.10), Vector2(-27.5, -161.10)]:
		if not await _walk_to(device, point, 0.10, 10.0):
			return _fail("cascade mast lower landing approach at=%s" % _position())
	var lower: Dictionary = _native().get_supplied_machine_state()
	if String(lower["name"]) != "CASCADE MAST" or int(lower["station"]) != 2:
		return _fail("cascade mast lower call station is not reachable")
	if not await _walk_to(device, Vector2(-32.8, -160.95), 0.10, 8.0):
		return _fail("cascade mast physical boarding at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 2992, 2.0):
		return _fail("cascade mast boarding is not supported by deck2992")
	await _face(Vector2(0, -1))
	if String(_native().get_supplied_machine_state()["name"]) != "CASCADE MAST" \
			or int(_native().get_supplied_machine_state()["station"]) != 1 \
			or not await _offered(&"operate", "OPERATE", "CASCADE MAST"):
		return _fail("cascade mast deck control unavailable at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("cascade mast touch pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 285.98, 65.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not reached or not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2992:
		return _fail("cascade mast touch ride failed at=%s station=%s" % [_position(), arrival])
	if String(arrival["name"]) != "CASCADE MAST" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return _fail("cascade mast arrival violates finite energy/brake state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("cascade mast DONE did not release operation")
	for point in [Vector2(-27.5, -161.10), Vector2(-24.7, -161.10)]:
		if not await _walk_to(device, point, 0.10, 8.0):
			return _fail("cascade mast +286m receiver/Tower exit at=%s" % _position())
	if not _standing_above(286.7) or absf(_position().y - 286.9) > 0.15 \
			or int(_native().get_support_entity_id()) != 11 or not _campaign_world_intact():
		return _fail("cascade mast +286m arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=%s shipping_touch=1 cascade_mast=1 finite_energy=1 supported_height_m=286 support=11 deaths=0" % [
		"supported_253m" if stage_at_253 else "ordinary_grade_spawn"]
	return true


func _touch_campaign_to_308() -> bool:
	if not await _touch_campaign_to_286():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=286 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_pitman_lift(false) or not _campaign_world_intact():
		return false
	_detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 barrel_helix=1 cascade_mast=1 pitman_lift=1 supported_height_m=308 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_pitman_lift(stage_at_286: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_286:
		if not _native().debug_restart_at(Vector3(-24.7, 286.9, -161.10)):
			return _fail("pitman lift +286m staging rejected")
		await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(286.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("pitman lift approach requires supported +286m Tower11")
	for point in [Vector2(-24.7, -142.65), Vector2(-32.25, -142.65)]:
		if not await _walk_to(device, point, 0.10, 10.0):
			return _fail("pitman lift lower landing approach at=%s" % _position())
	var lower: Dictionary = _native().get_supplied_machine_state()
	if String(lower["name"]) != "PITMAN LIFT" or int(lower["station"]) != 2:
		return _fail("pitman lift lower call station is not reachable")
	if not await _walk_to(device, Vector2(-42.5, -142.5), 0.10, 10.0):
		return _fail("pitman lift physical boarding at=%s" % _position())
	if not await _wait_until(func() -> bool:
		return bool(_ctx()["grounded"]) and int(_native().get_support_entity_id()) == 2921, 2.0):
		return _fail("pitman lift boarding is not supported by deck2921")
	await _face(Vector2(0, -1))
	if String(_native().get_supplied_machine_state()["name"]) != "PITMAN LIFT" \
			or int(_native().get_supplied_machine_state()["station"]) != 1 \
			or not await _offered(&"operate", "OPERATE", "PITMAN LIFT"):
		return _fail("pitman lift deck control unavailable at=%s" % _position())
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("pitman lift touch pendant did not open")
	var before: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 307.98, 65.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(0.5)
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not reached or not bool(_ctx()["grounded"]) or int(_native().get_support_entity_id()) != 2921:
		return _fail("pitman lift touch ride failed at=%s station=%s" % [_position(), arrival])
	if String(arrival["name"]) != "PITMAN LIFT" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return _fail("pitman lift arrival violates finite energy/brake state")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("pitman lift DONE did not release operation")
	for point in [Vector2(-32.25, -142.65), Vector2(-24.7, -142.65)]:
		if not await _walk_to(device, point, 0.10, 10.0):
			return _fail("pitman lift +308m receiver/Tower exit at=%s" % _position())
	if not _standing_above(308.7) or absf(_position().y - 308.9) > 0.15 \
			or int(_native().get_support_entity_id()) != 11 or not _campaign_world_intact():
		return _fail("pitman lift +308m arrival is not supported by Tower11 at=%s" % _position())
	_detail = "staging=%s shipping_touch=1 pitman_lift=1 finite_energy=1 supported_height_m=308 support=11 deaths=0" % [
		"supported_286m" if stage_at_286 else "ordinary_grade_spawn"]
	return true


# Focused normal-world route: only the initial supported Tower330 staging
# relocates the player. Every transfer uses the shipping viewport controls.
# An actual failed jump/recovery branch remains pending a feasible observed
# failure; the inspection stop below proves voluntary escape and reboarding.
func _touch_slab_haul_cart() -> bool:
	var device := InputRouter.Device.TOUCH
	if not _native().debug_restart_at(Vector3(-24.7, 330.9, -158.25)):
		return _fail("slab haul +330m staging rejected")
	await _seconds(0.5)
	if not _campaign_world_intact() or not _reclaim_tower_arrived():
		return _fail("slab haul approach requires supported +330m Tower11")
	for point in [Vector2(-24.7, -140.15), Vector2(-25.05, -140.15)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("slab haul lower control approach at=%s" % _position())
	if not _cart_station(2) or not await _offered(&"operate", "OPERATE", "SLAB HAUL CART"):
		return _fail("slab haul actual lower station2 unavailable")
	var deck := int(_native().get_kit_body_index(2320))
	var slab := int(_native().get_kit_body_index(2325))
	if deck < 0 or slab < 0:
		return _fail("slab haul native deck2320/slab2325 missing")
	for point in [Vector2(-26.5, -140.25), Vector2(-41.8, -140.25)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("slab haul loading floor approach at=%s" % _position())
	if int(_native().get_support_entity_id()) != 1955:
		return _fail("slab haul loading Jump lacks actual receiver1955")
	await _face(Vector2(0, -1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(-41.8, -141.6), 0.10, 8.0, true) \
			or not await _wait_cart_footing(deck, 330.0, 8.0):
		return _fail("slab haul south Jump missed stable cart2320 at=%s" % _position())
	if not await _cart_operate(1):
		return false
	var loaded: Dictionary = _native().get_supplied_machine_state()
	var slab_loaded: Transform3D = _native().get_kit_body_transform(slab)
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 340.985, 90.0)
	_touch(1, _center(&"pendant_up"), false)
	if not reached or not await _wait_cart_footing(deck, 341.0, 12.0):
		return _fail("slab haul neutral341 lacks stable cart2320 footing")
	var inspection: Dictionary = _native().get_supplied_machine_state()
	var slab_inspection: Transform3D = _native().get_kit_body_transform(slab)
	if not _cart_station(1) or not bool(inspection["braking"]) or not _cart_bank(inspection) \
			or slab_inspection.origin.y >= slab_loaded.origin.y - 5.0 \
			or float(inspection["energy_j"]) > float(loaded["energy_j"]) + 0.01:
		return _fail("slab haul inspection lacks falling slab/neutral brake/finite energy")
	if not await _cart_done():
		return false
	var pose: Transform3D = _native().get_kit_body_transform(deck)
	var floor_point := pose * Vector3(-0.30, 2.75, 0)
	if not await _walk_to(device, Vector2(floor_point.x, -141.6), 0.10, 8.0, true):
		return _fail("slab haul inspection takeoff unavailable")
	await _face(Vector2(0, 1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(floor_point.x, -140.25), 0.10, 8.0, true) \
			or int(_native().get_support_entity_id()) != 1956:
		return _fail("slab haul341 side Jump missed actual inspection tongue1956")
	if not await _walk_to(device, Vector2(-26.1, -140.15), 0.10, 12.0, true):
		return _fail("slab haul inspection fixed post approach at=%s" % _position())
	if not _cart_station(4) or not await _offered(&"operate", "OPERATE", "SLAB HAUL CART") \
			or not _standing_above(341.7) or int(_native().get_support_entity_id()) != 1956:
		return _fail("slab haul actual inspection station4 unavailable")
	for point in [Vector2(-26.5, -140.25), Vector2(floor_point.x, -140.25)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("slab haul inspection reboarding approach at=%s" % _position())
	await _face(Vector2(0, -1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(floor_point.x, -141.6), 0.10, 8.0, true) \
			or not await _wait_cart_footing(deck, 341.0, 8.0):
		return _fail("slab haul341 reboard Jump missed actual cart2320")
	if not await _cart_operate(1):
		return false
	_touch(1, _center(&"pendant_up"), true)
	reached = await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 351.985, 90.0)
	_touch(1, _center(&"pendant_up"), false)
	if not reached or not await _wait_cart_footing(deck, 352.0, 12.0):
		return _fail("slab haul upper seat lacks stable352 cart2320")
	if not await _cart_done():
		return false
	# A missed commitment has a real inspection apron eleven metres below.
	# Walk off without Jump, then steer in flight; no relocation or catch grant.
	print("SCRAPERX_CART_MISS before at=%s support=%d grounded=%s deck=%s" % [
		_position(), _native().get_support_entity_id(), _native().is_player_grounded(),
		_native().get_supplied_machine_state()["surface_y"]])
	await _face(Vector2(1, 0))
	_move(device, 1.0)
	var left_deck := await _wait_until(func() -> bool:
		return not bool(_native().is_player_grounded()), 5.0)
	_move(device, 0.0)
	print("SCRAPERX_CART_MISS departure=%s at=%s support=%d grounded=%s" % [
		left_deck, _position(), _native().get_support_entity_id(), _native().is_player_grounded()])
	if not left_deck:
		return _fail("slab haul miss did not leave cart at=%s support=%d" % [_position(), _native().get_support_entity_id()])
	# Aim below the centre of the real gap, not back onto the cart above.
	# The same supported-arrival gate then requires the recovery floor.
	var recovered := await _walk_to(device, Vector2(-26.9, -141.6), 0.10, 10.0, true)
	if not recovered or int(_native().get_support_entity_id()) != 1956:
		return _fail("slab haul missed exit lacks341 apron arrived=%s at=%s support=%d grounded=%s deaths=%d" % [
			recovered, _position(), _native().get_support_entity_id(), _native().is_player_grounded(), _native().get_death_count()])
	# The real heavy landing temporarily compacts the capsule. Require its
	# native recovery and clearance to restore standing before the next action.
	var recovered_stance := await _wait_until(func() -> bool:
		return _standing_above(341.7) and int(_native().get_support_entity_id()) == 1956, 3.0)
	if not recovered_stance:
		return _fail("slab haul341 landing did not recover standing at=%s landing=%s" % [
			_position(), _native().get_landing_state()])
	for point in [Vector2(-28.0, -140.15), Vector2(-26.1, -140.15)]:
		if not await _walk_to(device, point, 0.10, 8.0, true):
			return _fail("slab haul recovery post approach failed at=%s" % _position())
	if not await _cart_operate(4):
		return false
	var before_recall: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_down"), true)
	var recalled := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) <= 341.015, 90.0)
	_touch(1, _center(&"pendant_down"), false)
	await _seconds(0.5)
	var after_recall: Dictionary = _native().get_supplied_machine_state()
	if not recalled or absf(float(after_recall["surface_y"]) - 341.0) > 0.10 \
			or not _cart_station(4) or not _cart_bank(after_recall) \
			or float(after_recall["energy_j"]) >= float(before_recall["energy_j"]) \
			or int(_native().get_support_entity_id()) != 1956:
		return _fail("slab haul paid341 recall lacks real returned deck/source work/ashore footing")
	if not await _cart_done():
		return false
	if not await _walk_to(device, Vector2(-35.45, -140.25), 0.10, 12.0, true):
		return _fail("slab haul recovery reboarding approach failed")
	await _face(Vector2(0, -1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(-35.45, -141.6), 0.10, 8.0, true) \
			or not await _wait_cart_footing(deck, 341.0, 8.0) or not await _cart_operate(1):
		return _fail("slab haul recovery reboard missed actual cart")
	_touch(1, _center(&"pendant_up"), true)
	var retried := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 351.985, 90.0)
	_touch(1, _center(&"pendant_up"), false)
	if not retried or not await _wait_cart_footing(deck, 352.0, 12.0) or not await _cart_done():
		return _fail("slab haul recovery retry lacks actual352 footing")
	if not await _walk_to(device, Vector2(-28.10, -141.6), 0.10, 8.0, true) \
			or int(_native().get_support_entity_id()) != 2320:
		return _fail("slab haul352 north lane takeoff lacks actual cart footing")
	await _face(Vector2(1, 0))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(-25.35, -141.6), 0.10, 8.0, true) \
			or not _cart_tower_arrived():
		return _fail("slab haul352 real north lane Jump missed Tower11 at=%s" % _position())
	await _seconds(0.5)
	if not _cart_tower_arrived():
		return _fail("slab haul352 Tower11 footing did not remain stable")
	if not await _walk_to(device, Vector2(-25.05, -140.15), 0.10, 8.0, true) \
			or not await _cart_operate(3):
		return _fail("slab haul upper DOWN control approach failed")
	var before_return: Dictionary = _native().get_supplied_machine_state()
	var slab_before_return: Transform3D = _native().get_kit_body_transform(slab)
	var ashore := _position()
	var ashore_support := int(_native().get_support_entity_id())
	if ashore_support not in [11, 1957] or not _standing_above(352.7):
		return _fail("slab haul upper return has no actual ashore footing")
	_touch(1, _center(&"pendant_down"), true)
	var returned := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) <= 330.05, 120.0)
	_touch(1, _center(&"pendant_down"), false)
	await _seconds(0.5)
	var after: Dictionary = _native().get_supplied_machine_state()
	var deck_after: Transform3D = _native().get_kit_body_transform(deck)
	var slab_after: Transform3D = _native().get_kit_body_transform(slab)
	var floor_after := deck_after * Vector3(-0.30, 2.75, 0)
	if not returned or not _cart_station(3) or not bool(after["braking"]) or not _cart_bank(after) \
			or absf(floor_after.y - 330.0) > 0.10 or absf(floor_after.x + 41.8) > 0.15 \
			or slab_after.origin.y <= slab_before_return.origin.y + 20.0 \
			or float(after["positive_work_j"]) <= float(before_return["positive_work_j"]) \
			or float(after["energy_j"]) >= float(before_return["energy_j"]) \
			or not _standing_above(352.7) or int(_native().get_support_entity_id()) != ashore_support \
			or _position().distance_to(ashore) > 0.15:
		return _fail("slab haul paid return lacks real raised slab/cart330/unchanged ashore footing state=%s" % after)
	if not await _cart_done():
		return false
	if not await _walk_to(device, Vector2(-25.35, -141.6), 0.10, 8.0, true) \
			or not _cart_tower_arrived() or not _campaign_world_intact():
		return _fail("slab haul final352 proof lacks stable death-free Tower11")
	_detail = "staging=supported_330m shipping_touch=1 slab_haul_cart=1 inspection341=1 reboard=1 north_lane_jump=1 paid_return=1 finite_energy=1 supported_height_m=352 support=11 deaths=0 failure_recovery=upper_miss341_paid_recall_reboard_retry"
	return true


func _cart_station(station: int) -> bool:
	var state: Dictionary = _native().get_supplied_machine_state()
	return int(state["index"]) == 7 and int(state["station"]) == station \
		and String(state["name"]) == "SLAB HAUL CART"


func _cart_bank(state: Dictionary) -> bool:
	return is_finite(float(state["energy_j"])) and is_finite(float(state["positive_work_j"])) \
		and float(state["energy_j"]) >= 0.0 and float(state["energy_j"]) <= float(state["capacity_j"]) \
		and float(state["positive_work_j"]) >= 0.0 and float(state["energy_overdraft_j"]) == 0.0 \
		and not bool(state["energy_cutoff"])


func _cart_tower_arrived() -> bool:
	return _standing_above(352.7) and absf(_position().y - 352.9) <= 0.15 \
		and int(_native().get_support_entity_id()) == 11


func _cart_operate(station: int) -> bool:
	if not _cart_station(station) or not await _offered(&"operate", "OPERATE", "SLAB HAUL CART"):
		return _fail("slab haul actual station%d OPERATE unavailable at=%s" % [station, _position()])
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up") \
			or not _main._touch.is_button_shown(&"pendant_down"):
		return _fail("slab haul Action did not open ordinary pendant")
	return true


func _cart_done() -> bool:
	_tap(0, _center(&"action"))
	await _frames(3)
	return _main._operating == &"" or _fail("slab haul DONE did not release operation")


func _wait_cart_footing(deck: int, surface: float, timeout: float) -> bool:
	var stable_ticks := 0
	var previous_tick := int(_native().get_tick_index())
	var waited := 0.0
	while waited < timeout:
		var pose: Transform3D = _native().get_kit_body_transform(deck)
		var floor_point := pose * Vector3(-0.30, 2.75, 0)
		var tick := int(_native().get_tick_index())
		var settled: bool = _standing_above(surface + 0.7) and int(_native().get_support_entity_id()) == 2320 \
			and absf(floor_point.y - surface) <= 0.10 \
			and absf(_position().y - surface - 0.9) <= 0.15 \
			and _native().get_kit_body_linear_velocity(deck).length() < 0.10 \
			and (_velocity() - _native().get_support_point_linear_velocity()).length() < 0.15
		stable_ticks = stable_ticks + maxi(0, tick - previous_tick) if settled else 0
		previous_tick = tick
		if stable_ticks >= 18:
			return true
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


func _touch_taper_inspection_route() -> bool:
	var device := InputRouter.Device.TOUCH
	# The sole staging operation is the measured, supported cart Tower exit.
	# Everything after it uses viewport touch movement, Jump and crouch.
	if not _campaign_world_intact() or not _native().debug_restart_at(Vector3(-25.4203, 352.9, -141.599)):
		return _fail("taper supported cart-exit staging rejected")
	_move(device, 0.0)
	await _seconds(2.0)
	if not await _taper_stable(11, 352.9) or int(_native().get_entity_body_count(1932)) != 1 \
			or int(_native().get_entity_body_count(1933)) != 1:
		return _fail("taper requires normal-world cart footing and actual inspection/recovery bodies")
	var lower_work := float(_native().get_landing_state()["landing_recovery_work_j"])
	for point in [Vector2(-24.12, -138.6), Vector2(-24.0, -139.5)]:
		if not await _walk_to(device, point, 0.08, 12.0, true) or not _taper_ordinary():
			return _fail("taper lower toe approach target=%s at=%s" % [point, _position()])
	if not bool(_native().is_player_grounded()) or _position().y >= 353.3:
		return _fail("taper lower girder toe has no actual352 footing at=%s" % _position())
	if not await _walk_to(device, Vector2(-24.0, -164.0), 0.08, 45.0, true) \
			or not await _taper_stable(1932, 363.9) \
			or float(_native().get_landing_state()["landing_recovery_work_j"]) <= lower_work:
		return _fail("taper lower girder did not earn supported landingA363 at=%s" % _position())
	print("SCRAPERX_TAPER landing_a at=%s support=%d" % [_position(), _native().get_support_entity_id()])
	if not await _walk_to(device, Vector2(-25.9, -164.0), 0.08, 10.0, true):
		return _fail("taper A run-up unavailable")
	await _face(Vector2(1, 0))
	_move(device, 1.0)
	var run_up := await _wait_until(func() -> bool: return _position().x >= -23.95, 2.0)
	var launch := _position()
	var launch_work := float(_native().get_landing_state()["landing_jump_work_j"])
	if not run_up or not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1932 \
			or launch.x < -24.02 or launch.x >= -23.7 or _velocity().x <= 2.0 \
			or not _main._touch.is_button_shown(&"jump"):
		_move(device, 0.0)
		return _fail("taper gap Jump lacks supported running stance at=%s velocity=%s" % [launch, _velocity()])
	var gap := {"running": true, "rise": false, "middle": false, "fall": false, "invalid": false, "peak": launch.y}
	_taper_sample_flight(gap)
	_tap(1, _center(&"jump"))
	var crossed := await _walk_to(device, Vector2(-20.15, -164.0), 0.08, 8.0, true)
	gap["running"] = false
	var jump_work := float(_native().get_landing_state()["landing_jump_work_j"]) - launch_work
	if not crossed or not bool(gap["rise"]) or not bool(gap["middle"]) or bool(gap["invalid"]) \
			or float(gap["peak"]) <= launch.y + 0.8 or jump_work <= 0.0 or jump_work > 1285.725 \
			or not await _taper_stable(1932, 363.9):
		return _fail("taper real2m Jump did not reach stable landingB at=%s flight=%s work_j=%.3f" % [_position(), gap, jump_work])
	print("SCRAPERX_TAPER gap_b at=%s peak=%.3f jump_work_j=%.3f" % [_position(), gap["peak"], jump_work])
	# Walk back into the open gap without Jump. This connected middle miss
	# must be caught by the real apron, then use the longer crouched bypass.
	var before_miss := _position()
	var before_landing: Dictionary = _native().get_landing_state()
	var miss := {"running": true, "rise": false, "middle": false, "fall": false, "invalid": false, "peak": before_miss.y}
	_taper_sample_flight(miss)
	var caught := await _walk_to(device, Vector2(-22.4, -164.0), 0.08, 12.0, true)
	miss["running"] = false
	var landing: Dictionary = _native().get_landing_state()
	if not caught or not bool(miss["fall"]) or bool(miss["invalid"]) \
			or int(landing["landing_count"]) <= int(before_landing["landing_count"]) \
			or int(landing["landing_support_entity_id"]) != 1933 \
			or float(landing["landing_jump_work_j"]) != float(before_landing["landing_jump_work_j"]) \
			or _position().y >= before_miss.y - 2.0 or not await _taper_stable(1933, 361.4):
		return _fail("taper walking miss lacks actual360.5 apron recovery at=%s flight=%s landing=%s" % [_position(), miss, landing])
	print("SCRAPERX_TAPER middle_apron at=%s support=%d" % [_position(), _native().get_support_entity_id()])
	for point in [Vector2(-22.4, -157.7), Vector2(-19.55, -157.7), Vector2(-19.55, -157.55)]:
		if not await _walk_to(device, point, 0.08, 15.0, true) or not _taper_ordinary():
			return _fail("taper service toe approach target=%s at=%s" % [point, _position()])
	var stood_through := await _walk_to(device, Vector2(-19.55, -160.2), 0.08, 3.0, true)
	if stood_through or not _taper_ordinary() or not bool(_native().is_player_grounded()) \
			or bool(_native().is_player_crouched()) or _position().z <= -159.5 or _position().z >= -157.6:
		return _fail("taper real bearing header did not block standing at=%s" % _position())
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return bool(_native().is_player_crouched()), 0.4):
		return _fail("taper touch crouch input did not reach native posture")
	# Hold deliberate forward effort through the loaded incline. Proportional
	# easing far from the target supplies too little uphill crouched effort;
	# the player can hold the ordinary stick, then brake beyond the header.
	await _face(Vector2(0, -1))
	_move(device, 1.0)
	var cleared_header := await _wait_until(func() -> bool: return _position().z < -160.3, 6.0)
	_move(device, 0.0)
	if not cleared_header or not await _walk_to(device, Vector2(-19.55, -160.2), 0.08, 12.0, true) \
			or not bool(_native().is_player_crouched()) or not bool(_native().is_player_grounded()) \
			or int(_native().get_support_entity_id()) != 1932 or not _taper_ordinary():
		return _fail("taper touch crouch failed actual inclined header passage at=%s" % _position())
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return not bool(_native().is_player_crouched()), 0.4) \
			or _position().z >= -159.8:
		return _fail("taper touch Stand lacks clearance beyond real header at=%s" % _position())
	if not await _walk_to(device, Vector2(-19.55, -164.0), 0.08, 15.0, true) \
			or not await _taper_stable(1932, 363.9):
		return _fail("taper service bypass did not rejoin supported landingB at=%s" % _position())
	print("SCRAPERX_TAPER service_rejoin at=%s support=%d" % [_position(), _native().get_support_entity_id()])
	if not await _walk_to(device, Vector2(-20.35, -164.0), 0.08, 10.0, true) \
			or not await _walk_to(device, Vector2(-20.35, -139.05), 0.08, 45.0, true) \
			or not await _taper_stable(1932, 374.9):
		return _fail("taper upper girder did not reach actual turning platform374 at=%s" % _position())
	for point in [Vector2(-20.35, -139.5), Vector2(-22.5, -139.5)]:
		if not await _walk_to(device, point, 0.08, 15.0, true):
			return _fail("taper final return beam approach target=%s at=%s" % [point, _position()])
	if not await _taper_stable(51, 375.15):
		return _fail("taper return did not step onto existing WorldSolid51 floor374.25 at=%s" % _position())
	var exit := _position()
	if not await _walk_to(device, Vector2(-22.5, -143.0), 0.08, 15.0, true) \
			or not await _taper_stable(51, 375.15) or _position().distance_to(exit) <= 3.0 \
			or _main._router.device != InputRouter.Device.TOUCH or not _campaign_world_intact():
		return _fail("taper existing upper floor lacks stable ordinary onward walking at=%s" % _position())
	await _pose("taper_existing_floor374_25")
	_detail = "staging=supported_cart_exit352 shipping_touch=1 lower_girder=1 ordinary2m_gap_jump=1 middle_miss_apron360_5=1 standing_header_blocked=1 crouched_service_bypass=1 upper_girder=1 existing_floor374_25=1 onward_m=%.3f support=51 deaths=0 gravity=1 upper_miss=native_only jump_work_j=%.3f" % [_position().distance_to(exit), jump_work]
	return true


func _taper_ordinary() -> bool:
	var state: Dictionary = _native().get_landing_state()
	return int(_native().get_death_count()) == 0 and int(_native().get_traversal_state()) == 0 \
		and int(_native().get_accepted_traversal_count()) == 0 and not bool(_native().is_parachute_deployed()) \
		and not bool(_native().is_player_sprinting()) and _position().is_finite() and _velocity().is_finite() \
		and float(state.get("player_gravity_factor", -1.0)) == 1.0 \
		and int(state.get("traversal_hand_constraint_count", -1)) == 0 \
		and is_finite(float(state["landing_recovery_work_j"])) and is_finite(float(state["landing_jump_work_j"]))


func _taper_stable(support: int, standing_y: float) -> bool:
	_move(InputRouter.Device.TOUCH, 0.0)
	var footing := func() -> bool:
		return _taper_ordinary() and bool(_native().is_player_grounded()) \
			and int(_native().get_support_entity_id()) == support and not bool(_native().is_player_crouched()) \
			and absf(_position().y - standing_y) < 0.12 and absf(_velocity().y) < 0.12 \
			and Vector2(_velocity().x, _velocity().z).length() < 0.18
	if not await _wait_until(footing, 3.0):
		return false
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 90:
		await get_tree().process_frame
		if not footing.call():
			return false
	return true


# Runs beside the actual touch walk helper, reading native flight each frame.
func _taper_sample_flight(watch: Dictionary) -> void:
	var previous_airborne := false
	var previous_vy := _velocity().y
	while bool(watch["running"]):
		var at := _position()
		var velocity := _velocity()
		var airborne := not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0
		watch["invalid"] = bool(watch["invalid"]) or not _taper_ordinary()
		watch["peak"] = maxf(float(watch["peak"]), at.y)
		watch["rise"] = bool(watch["rise"]) or (airborne and velocity.y > 0.5)
		watch["fall"] = bool(watch["fall"]) or (airborne and velocity.y < -3.0)
		if airborne and previous_airborne and at.x > -23.0 and at.x < -21.8 and at.y > 364.1:
			watch["middle"] = true
			watch["invalid"] = bool(watch["invalid"]) or velocity.y > previous_vy + 0.01
		previous_airborne = airborne
		previous_vy = velocity.y
		await get_tree().process_frame


func _touch_gravity_reclaim() -> bool:
	var device := InputRouter.Device.TOUCH
	if not _native().debug_restart_at(Vector3(-24.7, 308.9, -142.65)):
		return _fail("gravity reclaim +308m staging rejected")
	await _seconds(0.5)
	if not _campaign_world_intact() or not _standing_above(308.7) \
			or int(_native().get_support_entity_id()) != 11:
		return _fail("gravity reclaim approach requires supported +308m Tower11")
	# Approach alongside the physical landing post, within ordinary reach.
	for point in [Vector2(-24.7, -158.75), Vector2(-33.4, -158.75), Vector2(-33.4, -158.25)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("gravity reclaim feed approach at=%s" % _position())
	var before: Dictionary = _native().get_supplied_machine_state()
	if not _reclaim_station(2) or not await _offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return _fail("gravity reclaim lower feed control unavailable")
	if float(before["hopper_mass_kg"]) <= 0.0:
		return _fail("gravity reclaim has no finite starting hopper material")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("gravity reclaim feed pendant did not open")
	_touch(1, _center(&"pendant_up"), true)
	await _seconds(18.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(4.0)
	var loaded: Dictionary = _native().get_supplied_machine_state()
	if not _reclaim_station(2) or not bool(loaded["braking"]) or not _reclaim_bank(loaded) \
			or float(loaded["hopper_mass_kg"]) < 0.0 \
			or float(loaded["hopper_mass_kg"]) >= float(before["hopper_mass_kg"]) \
			or float(loaded["energy_j"]) >= float(before["energy_j"]):
		return _fail("gravity reclaim touch feed did not draw from finite material/power")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity reclaim feed DONE did not release operation")
	var cabin := int(_native().get_kit_body_index(2201))
	if cabin < 0:
		return _fail("gravity reclaim cabin2201 body unavailable")
	var boarding_pose: Transform3D = _native().get_kit_body_transform(cabin)
	var boarding_x := boarding_pose.origin.x - 0.65
	if not await _walk_to(device, Vector2(boarding_x, -158.5), 0.10, 8.0, true):
		return _fail("gravity reclaim boarding tongue approach at=%s" % _position())
	await _face(Vector2(0, -1))
	if not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 1941:
		return _fail("gravity reclaim boarding Jump lacks lower receiver footing")
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(boarding_x, -160.10), 0.10, 8.0, true):
		return _fail("gravity reclaim physical cabin boarding at=%s" % _position())
	# Off-centre rider weight tilts the passive hanger. Walk behind the post
	# to its actual transformed floor centre before asking the brake to release.
	boarding_pose = _native().get_kit_body_transform(cabin)
	var floor_centre := boarding_pose * Vector3(0, -2.5, -0.3)
	if not await _walk_to(device, Vector2(floor_centre.x, floor_centre.z), 0.10, 8.0, true):
		return _fail("gravity reclaim actual cabin floor centre approach at=%s" % _position())
	if not await _wait_reclaim_cabin(cabin, 307.97, 8.0):
		return _fail("gravity reclaim boarding lacks stable actual cabin2201 floor")
	if not _reclaim_station(1) or not await _offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return _fail("gravity reclaim cabin brake control unavailable")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("gravity reclaim cabin pendant did not open")
	_touch(1, _center(&"pendant_up"), true)
	var moved := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 314.0, 60.0)
	var rising: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), false)
	if not _reclaim_station(1) or bool(rising["braking"]) or bool(rising["wheel_motor_enabled"]):
		return _fail("gravity reclaim cabin UP did not release a passive, unpowered wheel")
	if not moved or not await _wait_reclaim_cabin(cabin, 313.9, 12.0):
		return _fail("gravity reclaim gravity ride did not stop on supported cabin")
	var paused: Dictionary = _native().get_supplied_machine_state()
	var paused_height := float(paused["surface_y"])
	await _seconds(0.5)
	if absf(float(_native().get_supplied_machine_state()["surface_y"]) - paused_height) > 0.10 \
			or not _reclaim_bank(paused):
		return _fail("gravity reclaim released brake did not retain its physical stop")
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 329.97, 90.0)
	var resumed: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), false)
	if bool(resumed["braking"]) or bool(resumed["wheel_motor_enabled"]):
		return _fail("gravity reclaim resumed ascent violated passive brake-release semantics")
	if not reached or not await _wait_reclaim_cabin(cabin, 329.9, 15.0):
		return _fail("gravity reclaim loaded ascent did not settle at +330m")
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not _reclaim_station(1) or not _reclaim_bank(arrival) \
			or float(arrival["energy_j"]) > float(loaded["energy_j"]) + 0.01 \
			or float(arrival["hopper_mass_kg"]) > float(loaded["hopper_mass_kg"]) + 0.01 \
			or absf(float(arrival["wheel_angle_rad"]) - float(loaded["wheel_angle_rad"])) < 0.1:
		return _fail("gravity reclaim ascent violates finite material/bank or actual wheel motion")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity reclaim cabin DONE did not release operation")
	var pose: Transform3D = _native().get_kit_body_transform(cabin)
	var exit_floor := pose * Vector3(-0.65, -2.5, 0)
	var exit_x := exit_floor.x
	for point in [Vector2(exit_x, exit_floor.z), Vector2(exit_x, -159.65)]:
		if not await _walk_to(device, point, 0.10, 7.0, true):
			return _fail("gravity reclaim upper cabin takeoff at=%s" % _position())
	await _face(Vector2(0, 1))
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(exit_x, -158.5), 0.10, 8.0, true) \
			or int(_native().get_support_entity_id()) != 1942:
		return _fail("gravity reclaim upper transfer missed actual receiver1942")
	for point in [Vector2(exit_x, -158.75), Vector2(-24.7, -158.75), Vector2(-24.7, -158.25)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("gravity reclaim +330m Tower exit at=%s" % _position())
	if not _reclaim_tower_arrived():
		return _fail("gravity reclaim +330m arrival lacks actual Tower11 footing")
	# The far-side panel opens the real scoop doors and releases the wheel
	# brake. Empty return remains gravity-driven while the player stays ashore.
	for point in [Vector2(-24.7, -158.75), Vector2(-33.4, -158.75), Vector2(-33.4, -158.25)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("gravity reclaim upper discharge approach at=%s" % _position())
	if not _reclaim_station(3) or not await _offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return _fail("gravity reclaim upper discharge control unavailable")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_down"):
		return _fail("gravity reclaim discharge pendant did not open")
	_touch(1, _center(&"pendant_down"), true)
	var returned := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) <= 308.05, 120.0)
	_touch(1, _center(&"pendant_down"), false)
	await _seconds(0.5)
	var after: Dictionary = _native().get_supplied_machine_state()
	if not returned or not _reclaim_station(3) or not bool(after["braking"]) \
			or not _reclaim_bank(after) or float(after["energy_j"]) >= float(arrival["energy_j"]) \
			or float(after["hopper_mass_kg"]) > float(loaded["hopper_mass_kg"]) + 0.01 \
			or not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 1942:
		return _fail("gravity reclaim discharge/empty return did not preserve upper footing and finite supply")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity reclaim discharge DONE did not release operation")
	for point in [Vector2(-33.4, -158.75), Vector2(-24.7, -158.75), Vector2(-24.7, -158.25)]:
		if not await _walk_to(device, point, 0.10, 12.0, true):
			return _fail("gravity reclaim final Tower footing at=%s" % _position())
	if not _reclaim_tower_arrived() or not _campaign_world_intact():
		return _fail("gravity reclaim final +330m proof lacks Tower11/death-free world")
	if not await _touch_reclaim_retry(before):
		return false
	_detail = "staging=supported_308m shipping_touch=1 finite_hopper=1 gravity_reclaim=1 stop_resume=1 upper_discharge=1 empty_gravity_return=1 finite_energy=1 first_supported_height_m=330 first_support=11 retry_from_feeding=1 second_supported_height_m=%.2f second_support=2201 deaths=0" % float(_native().get_supplied_machine_state()["surface_y"])
	return true


func _touch_reclaim_retry(initial: Dictionary) -> bool:
	var device := InputRouter.Device.TOUCH
	var deaths := int(_native().get_death_count())
	_tap(0, _center(&"pause"))
	await _frames(2)
	if not get_tree().paused:
		return _fail("gravity reclaim retry PAUSE did not open")
	_click(_main._pause_menu.side_button_center(&"restart"))
	await _frames(2)
	var retry: Button = _main._pause_menu._reclaim_retry_button
	if _main._pause_menu.current_page() != &"restart" or not is_instance_valid(retry) \
			or not retry.is_visible_in_tree() or retry.disabled or retry.text != "RETRY FROM FEEDING":
		return _fail("gravity reclaim retry button unavailable on actual RESTART page")
	# The same emulated GUI twin used by the ordinary touch pause-page tests.
	_click(retry.get_global_rect().get_center())
	await _frames(3)
	if get_tree().paused:
		return _fail("gravity reclaim retry did not restore and resume the world")
	await _seconds(0.5)
	var restored: Dictionary = _native().get_supplied_machine_state()
	if not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 1941 \
			or absf(_position().y - 308.9) > 0.15 or int(_native().get_death_count()) != deaths \
			or not _reclaim_station(2) or not _reclaim_bank(restored) \
			or float(restored["hopper_mass_kg"]) <= 0.0 \
			or float(restored["hopper_mass_kg"]) <= float(initial["hopper_mass_kg"]) - 100.0 \
			or float(restored["hopper_mass_kg"]) > float(initial["hopper_mass_kg"]) + 0.01 \
			or absf(float(restored["surface_y"]) - 308.0) >= 0.05:
		return _fail("gravity reclaim retry lacks actual safe1941 footing/finite restored stock at=%s state=%s" % [_position(), restored])
	if not await _walk_to(device, Vector2(-33.4, -158.25), 0.10, 8.0, true) \
			or not _reclaim_station(2) or not await _offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return _fail("gravity reclaim retry ordinary feeding approach failed")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("gravity reclaim retry feed pendant did not open")
	_touch(1, _center(&"pendant_up"), true)
	await _seconds(18.0)
	_touch(1, _center(&"pendant_up"), false)
	await _seconds(4.0)
	var loaded: Dictionary = _native().get_supplied_machine_state()
	if not _reclaim_station(2) or not bool(loaded["braking"]) or not _reclaim_bank(loaded) \
			or float(loaded["hopper_mass_kg"]) < 0.0 \
			or float(loaded["hopper_mass_kg"]) >= float(restored["hopper_mass_kg"]) \
			or float(loaded["energy_j"]) >= float(restored["energy_j"]):
		return _fail("gravity reclaim retry did not feed actual restored finite material/power")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity reclaim retry feed DONE did not release operation")
	var cabin := int(_native().get_kit_body_index(2201))
	if cabin < 0:
		return _fail("gravity reclaim retry cabin2201 unavailable")
	var pose: Transform3D = _native().get_kit_body_transform(cabin)
	var boarding_x := pose.origin.x - 0.65
	if not await _walk_to(device, Vector2(boarding_x, -158.5), 0.10, 8.0, true):
		return _fail("gravity reclaim retry boarding tongue approach failed")
	await _face(Vector2(0, -1))
	if not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 1941:
		return _fail("gravity reclaim retry Jump lacks lower receiver footing")
	_tap(1, _center(&"jump"))
	if not await _walk_to(device, Vector2(boarding_x, -160.10), 0.10, 8.0, true):
		return _fail("gravity reclaim retry physical cabin reboarding at=%s" % _position())
	pose = _native().get_kit_body_transform(cabin)
	var floor_centre := pose * Vector3(0, -2.5, -0.3)
	if not await _walk_to(device, Vector2(floor_centre.x, floor_centre.z), 0.10, 8.0, true) \
			or not await _wait_reclaim_cabin(cabin, 307.97, 8.0):
		return _fail("gravity reclaim retry ordinary reboarding lacks stable cabin2201 floor")
	if not _reclaim_station(1) or not await _offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return _fail("gravity reclaim retry cabin brake control unavailable")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"supplied_machine" or not _main._touch.is_button_shown(&"pendant_up"):
		return _fail("gravity reclaim retry cabin pendant did not open")
	_touch(1, _center(&"pendant_up"), true)
	var reached := await _wait_until(func() -> bool:
		return float(_native().get_supplied_machine_state()["surface_y"]) >= 311.0 \
			or not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 2201, 60.0)
	var rising: Dictionary = _native().get_supplied_machine_state()
	_touch(1, _center(&"pendant_up"), false)
	if not reached or float(rising["surface_y"]) < 311.0 or not _reclaim_station(1) \
			or not _native().is_player_grounded() or int(_native().get_support_entity_id()) != 2201 \
			or bool(rising["braking"]) or bool(rising["wheel_motor_enabled"]) \
			or not await _wait_reclaim_cabin(cabin, 311.0, 12.0):
		return _fail("gravity reclaim retry finite load did not earn supported passive +311m ascent")
	var arrival: Dictionary = _native().get_supplied_machine_state()
	if not _reclaim_bank(arrival) or float(arrival["energy_j"]) > float(loaded["energy_j"]) + 0.01 \
			or float(arrival["hopper_mass_kg"]) > float(loaded["hopper_mass_kg"]) + 0.01 \
			or int(_native().get_death_count()) != deaths or not _campaign_world_intact():
		return _fail("gravity reclaim retry ascent violated finite supply/death-free world")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"":
		return _fail("gravity reclaim retry cabin DONE did not release operation")
	return true


func _reclaim_station(station: int) -> bool:
	var state: Dictionary = _native().get_supplied_machine_state()
	return int(state["index"]) == 6 and int(state["station"]) == station \
		and String(state["name"]) == "REFRACTORY RECLAIM"


func _reclaim_bank(state: Dictionary) -> bool:
	return is_finite(float(state["energy_j"])) and float(state["energy_j"]) >= 0.0 \
		and float(state["energy_j"]) <= float(state["capacity_j"]) and not bool(state["energy_cutoff"])


func _reclaim_tower_arrived() -> bool:
	return _standing_above(330.7) and absf(_position().y - 330.9) <= 0.15 \
		and int(_native().get_support_entity_id()) == 11


func _wait_reclaim_cabin(cabin: int, minimum_surface: float, timeout: float) -> bool:
	var waited := 0.0
	var stable_ticks := 0
	var previous_tick := int(_native().get_tick_index())
	var previous_angle := float(_native().get_supplied_machine_state()["wheel_angle_rad"])
	var previous_floor := float(_native().get_supplied_machine_state()["surface_y"])
	var angular_speed := INF
	var floor_speed := INF
	var step := float(_native().get_fixed_step_seconds())
	while waited < timeout:
		var pose: Transform3D = _native().get_kit_body_transform(cabin)
		var state: Dictionary = _native().get_supplied_machine_state()
		var tick := int(_native().get_tick_index())
		var elapsed_ticks := tick - previous_tick
		if elapsed_ticks > 0:
			angular_speed = absf(angle_difference(previous_angle, float(state["wheel_angle_rad"]))) \
				/ (float(elapsed_ticks) * step)
			floor_speed = absf(float(state["surface_y"]) - previous_floor) / (float(elapsed_ticks) * step)
			previous_tick = tick
			previous_angle = float(state["wheel_angle_rad"])
			previous_floor = float(state["surface_y"])
		var floor_at := pose * Vector3(0, -2.5, 0)
		var slip: Vector3 = _velocity() - _native().get_support_point_linear_velocity()
		var ready: bool = _native().is_player_grounded() and int(_native().get_support_entity_id()) == 2201 \
			and bool(state["braking"]) and float(state["surface_y"]) >= minimum_surface \
			and absf(_position().y - (floor_at.y + 0.9)) <= 0.15 \
			and Vector2(slip.x, slip.z).length() < 0.15 and pose.basis.y.dot(Vector3.UP) > 0.995 \
			and (_native().get_kit_body_linear_velocity(cabin) as Vector3).length() < 0.15 \
			and angular_speed < 0.0045 and floor_speed < 0.018
		# A finite brake first decelerates the wheel. Require 30 actual native
		# ticks of low wheel/floor speed and floor contact; suspension can keep
		# moving after the wheel stops.
		stable_ticks = stable_ticks + elapsed_ticks if ready else 0
		if stable_ticks >= 30:
			return true
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


func _swing_catch_sample(body: int, previous: Dictionary) -> void:
	var tick := int(_native().get_tick_index())
	if tick == int(previous["tick"]):
		return
	var pose: Transform3D = _native().get_kit_body_transform(body)
	var angle := pose.basis.get_euler().z
	# Track the same real rung midpoint, not a changing nearest point.
	var midpoint := pose * Vector3(0, -7.82, 0)
	var sample_seconds := float(tick - int(previous["tick"])) * float(_native().get_fixed_step_seconds())
	var point_velocity := Vector3.ZERO
	var angular_z := 0.0
	if int(previous["tick"]) >= 0 and sample_seconds > 0.0:
		point_velocity = (midpoint - (previous["midpoint"] as Vector3)) / sample_seconds
		angular_z = angle_difference(float(previous["angle"]), angle) / sample_seconds
	var facing := Vector3(-sin(float(_main._yaw)), 0, -cos(float(_main._yaw)))
	var hand_aim := _position() + Vector3(0, 0.45, 0) + facing * 0.4
	var aim_local := pose.affine_inverse() * hand_aim
	var nearest := Vector3.ZERO
	var distance := INF
	for rung in 24:
		var point := pose * Vector3(clampf(aim_local.x, -0.85, 0.85), -8.9 + 0.36 * rung, 0)
		if point.distance_to(hand_aim) < distance:
			nearest = point
			distance = point.distance_to(hand_aim)
	print("SCRAPERX_SWING_CATCH tick=%d player=%s player_velocity=%s grounded=%s crouched=%s traversal=%d view_yaw_rad=%.6f grip=%s grip_point=%s angle_z=%.6f origin=%s body_velocity=%s observed_rung_midpoint_velocity=%s observed_angular_z=%.6f nearest_rung=%s hand_distance=%.6f faced_depth=%.6f" % [
		tick, _position(), _velocity(), _native().is_player_grounded(), _native().is_player_crouched(),
		_native().get_traversal_state(), _main._yaw, _native().is_grip_available(), _native().get_grip_point(),
		angle, pose.origin, _native().get_kit_body_linear_velocity(body), point_velocity, angular_z, nearest,
		distance, (nearest - _position()).dot(facing)])
	previous["tick"] = tick
	previous["midpoint"] = midpoint
	previous["angle"] = angle


func _touch_suspended_ladder(stage_at_110: bool = true) -> bool:
	var device := InputRouter.Device.TOUCH
	# Explicit normal-world supported staging; not full campaign ascent proof.
	if stage_at_110:
		if not _native().debug_restart_at(Vector3(10, 110.9, -174)):
			return _fail("+110m supported staging rejected")
	else:
		# Walk the real +110m ring from the north-frame exit to the AS-026
		# approach; retain the airborne catch and loaded moving climb below.
		if not await _walk_to(device, Vector2(10, -174.5), 0.12, 5.0):
			return _fail("north frame to suspended ladder ring transfer %s" % _position())
	await _seconds(0.5)
	if not await _walk_to(device, Vector2(10, -179.60), 0.12, 5.0):
		return _fail("fixed ladder approach %s" % _position())
	await _face(Vector2(0, -1))
	await _pose("swing_approach")
	if not await _offered(&"climb", "CLIMB"):
		return _fail("fixed ladder grip unavailable")
	_act(device)
	await _seconds(0.2)
	_move(device, 1.0)
	var reached := await _wait_until(func() -> bool: return _standing_above(114.7), 8.0)
	_move(device, 0.0)
	if not reached:
		return _fail("launch shelf not reached %s" % _position())
	# Stand near the shelf's front edge before the north-facing side jump.
	# The old loose stop left z=-180.31: the descending capsule crossed the
	# rung plane before its x coordinate entered the unchanged hand query.
	if not await _walk_to(device, Vector2(9.15, -180.10), 0.04, 4.0):
		return _fail("launch edge not reached")
	if not _standing_above(114.7):
		return _fail("launch edge lost supported footing %s" % _position())
	await _face(Vector2(-1, -0.25))
	_main._pitch = -0.55
	await _pose("swing_launch_view")
	# Catch from the front. Native acquisition retains this normal for hand
	# loading and jump-off; turning the camera later cannot change that stance.
	await _face(Vector2(0, -1))
	_main._pitch = 0.0
	var body := int(_native().get_kit_body_index(2960))
	var catch_sample := {"tick": -1, "midpoint": Vector3.ZERO, "angle": 0.0}
	_swing_catch_sample(body, catch_sample)
	_move_dir(device, Vector2(-1, 0.35))
	_tap(1, _center(&"jump"))
	var caught := await _wait_until(func() -> bool:
		_swing_catch_sample(body, catch_sample)
		return int(_native().get_traversal_support_entity_id()) == 2960, 1.5)
	_move(device, 0.0)
	if not caught:
		return _fail("touch jump missed moving grip %s" % _position())
	await _seconds(0.5)
	await _pose("swing_caught")
	var first_angle := _teeter_angle(body)
	var swing_span := [0.0]
	_move(device, 1.0)
	reached = await _wait_until(func() -> bool:
		swing_span[0] = maxf(swing_span[0], absf(_teeter_angle(body) - first_angle))
		return _position().y > 119.8, 8.0)
	_move(device, 0.0)
	if not reached or swing_span[0] < 0.002:
		return _fail("loaded swing/climb not demonstrated reached=%s span=%.4f at=%s" % [reached, swing_span[0], _position()])
	var phase := {"min_vx": INF, "max_vx": -INF, "min_angle": INF, "max_angle": -INF, "tick": -1}
	print("SCRAPERX_SWING_RELEASE_START player=%s normal=%s loaded_span=%.6f" % [_position(), _native().get_traversal_normal(), swing_span[0]])
	var rightward := await _wait_until(func() -> bool:
		var velocity: Vector3 = _native().get_kit_body_linear_velocity(body)
		var angle := _teeter_angle(body)
		phase["min_vx"] = minf(phase["min_vx"], velocity.x)
		phase["max_vx"] = maxf(phase["max_vx"], velocity.x)
		phase["min_angle"] = minf(phase["min_angle"], angle)
		phase["max_angle"] = maxf(phase["max_angle"], angle)
		if int(_native().get_tick_index()) - int(phase["tick"]) >= 45:
			phase["tick"] = int(_native().get_tick_index())
			print("SCRAPERX_SWING_RELEASE_PHASE tick=%d player=%s velocity=%s angle=%.6f normal=%s support=%d" % [phase["tick"], _position(), velocity, angle, _native().get_traversal_normal(), _native().get_traversal_support_entity_id()])
		return velocity.x > 0.12, 6.7)
	if not rightward:
		return _fail("rightward release phase unavailable phase=%s at=%s normal=%s" % [phase, _position(), _native().get_traversal_normal()])
	await _pose("swing_release_height")
	_move_dir(device, Vector2(0.7, -0.35))
	_tap(1, _center(&"jump"))
	reached = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 1.7)
	_move(device, 0.0)
	await _seconds(0.3)
	if not reached or not _standing_above(119.7) or int(_native().get_support_entity_id()) != 1960:
		return _fail("swing release missed offset receiver reached=%s at=%s" % [reached, _position()])
	await _face(Vector2(0, 1))
	await _pose("swing_landing")
	if not await _walk_to(device, Vector2(9, -178.45), 0.12, 4.0):
		return _fail("receiver exit climb approach failed")
	await _face(Vector2(0, 1))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("upper climb unavailable %s" % _position())
	_act(device)
	await _seconds(0.2)
	_move(device, 1.0)
	reached = await _wait_until(func() -> bool: return _standing_above(121.7), 5.0)
	_move(device, 0.0)
	if not reached or not await _walk_to(device, Vector2(9, -174.5), 0.12, 4.0):
		return _fail("+121m tower exit failed %s" % _position())
	await _seconds(0.3)
	await _pose("swing_tower_exit")
	if int(_native().get_support_entity_id()) != 11 or int(_native().get_death_count()) != 0:
		return _fail("+121m exit is unsupported or deaths occurred")
	if _main._audio._bank_ready and not _main._audio._silent and not _main._audio._ladder_strain.playing:
		return _fail("native-driven strain voice is not playing")
	if _main._audio.ladder_motion_frames == 0 or _main._audio.climb_regrips == 0:
		return _fail("state-driven swing feedback absent")
	_detail = "staging=supported_110m swing_jump=1 supported_height_m=121 support=11 deaths=0 native_feedback=1"
	return true


func _touch_cargo_net() -> bool:
	var device := InputRouter.Device.TOUCH
	await _seconds(0.5)
	# Start at the ordinary grade spawn. No fixture, relocation, restart,
	# launch or scripted ascent: all movement enters through viewport touch.
	if not await _walk_to(device, Vector2(20.0, -114.0), 0.12, 35.0):
		return _fail("cargo net grade approach %s" % _position())
	await _face(Vector2(0, -1))
	_main._pitch = 0.35
	await _pose("cargo_net_entry")
	_main._pitch = 0.0
	# Stop at the actual grounded CLIMB offer. Driving to a fixed rest-mesh
	# coordinate can pass the offer and legitimately auto-catch while airborne
	# on the finite tread/net; that does not exercise the requested Action.
	_move(device, 0.25)
	var offered := await _wait_until(func() -> bool: return bool(_native().is_player_grounded()) \
		and bool(_native().is_grip_available()), 6.0)
	_move(device, 0.0)
	if not offered or bool(_ctx()["climbing"]):
		return _fail("cargo net grounded grip approach %s" % _position())
	await _face(Vector2(0, -1))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("cargo net CLIMB not offered %s" % _position())
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_ctx()["climbing"]), 0.8):
		return _fail("cargo net touch Action did not take hold")
	var still_y := _position().y
	await _seconds(0.6)
	if _position().y - still_y > 0.06:
		return _fail("cargo net advanced without held input")
	_move(device, 1.0)
	if not await _wait_until(func() -> bool: return _position().y > 5.5, 8.0):
		_move(device, 0.0)
		return _fail("cargo net lower climb blocked %s" % _position())
	_move(device, 0.0)
	await _seconds(0.3)
	await _pose("cargo_net_climbing")
	_move(device, 1.0)
	# Allow compliant grip/receiver settling after the lift. This remains a
	# bounded test deadline; only actual supported contact proves arrival.
	var arrived := await _wait_until(func() -> bool: return _standing_above(11.7), 12.0)
	_move(device, 0.0)
	if not arrived:
		return _fail("cargo net top-out blocked %s" % _position())
	await _seconds(0.4)
	await _pose("cargo_net_receiver")
	for point in [Vector2(18.5, -120.5), Vector2(18.5, -126.0)]:
		if not await _walk_to(device, point, 0.12, 5.0):
			return _fail("cargo net receiver to first ring %s" % _position())
	await _seconds(0.4)
	if not _standing_above(11.7) or absf(_position().y - 11.9) > 0.15 \
			or int(_native().get_support_entity_id()) != 11 or int(_native().get_death_count()) != 0:
		return _fail("cargo net first tower ring unsupported %s" % _position())
	if float(_native().get_slingshot_state()["work_j"]) != 0.0:
		return _fail("cargo net route used launcher work")
	await _pose("cargo_net_first_ring")
	_detail = "staging=ordinary_grade_spawn supported_height_m=11.00 deaths=0 slingshot_used=0"
	return true


func _ground_foundation() -> bool:
	if _main._regression_scene:
		return _fail("production proof selected a regression scene")
	for entity in range(3, 60):
		if entity not in [11, 51] and int(_native().get_entity_body_count(entity)) != 0:
			return _fail("retired native body %d remains" % entity)
	# Normal-world inventory: original/service lift, six supplied machines,
	# reclaim wheel/material, slab-haul cart and two static taper bodies.
	# Fixture counts are separate.
	var moving_count := int(_native().get_moving_body_count())
	var kit_count := int(_native().get_kit_body_count())
	var cable_count := int(_native().get_kit_cable_count())
	if moving_count != 27 + 9 + 17 + 103 + 8 or kit_count != 38 + 12 + 39 + 109 + 14 + 2 or cable_count != 3 + 3 + 1:
		return _fail("default inventory differs: moving=%d kit=%d cables=%d" % [moving_count, kit_count, cable_count])
	# Check actual scene nodes, independently of native enumeration. This also
	# catches visual-only remnants that would not appear in the physics world.
	var retired := ["IntakeBay", "WaterScrew", "LegalForty", "Hook5",
		"StackGear", "GearMotif", "Crane", "GroundWater", "WellA", "WellB"]
	var rejected_fallback_parts := {"StairFlight": true, "StairTread": true, "StairStringer": true}
	for node in _main.get_node("TowerPresentation").find_children("*", "", true, false):
		for prefix in retired:
			if String(node.name).begins_with(prefix):
				return _fail("retired scene node remains: " + String(node.name))
		if node.has_meta(&"part") and rejected_fallback_parts.has(String(node.get_meta(&"part"))):
			return _fail("rejected fallback scene part remains: " + String(node.get_meta(&"part")))
	if not await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0):
		return _fail("default player did not settle at grade")
	await _pose("cleared_grade")
	if not await _keyboard_core():
		return false
	# Look through the old intake/screw area with normal walking and turning.
	await _face(Vector2(-1.0, -1.0))
	await _pose("cleared_tower")
	_detail = "retired_bodies=0 moving_bodies=%d kit_bodies=%d cables=%d retired_meshes=0 rejected_fallback_meshes=0 default_controls=1" % [moving_count, kit_count, cable_count]
	return true


func _touch_jump() -> bool:
	var settled: bool = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not settled:
		return _fail("never grounded at spawn")
	_tap(0, _center(&"jump"))
	var peak := [0.0]
	var airborne_and_rising := func() -> bool:
		peak[0] = maxf(peak[0], _velocity().y)
		return not bool(_native().is_player_grounded()) and peak[0] > 2.0
	var lifted: bool = await _wait_until(airborne_and_rising, 0.4)
	if not lifted:
		return _fail("touch jump did not leave the ground (peak vy %.2f)" % peak[0])
	await _pose("jump")
	if _main._router.device != InputRouter.Device.TOUCH:
		return _fail("touch press did not switch the interface to touch")
	if _main._audio.jumps < 1:
		return _fail("the takeoff played no jump cue")
	_detail = "vy_peak=%.2f" % peak[0]
	return true


func _touch_move_look() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var start := _position()
	var yaw_start: float = _main._yaw
	var forward := Vector2(-sin(yaw_start), -cos(yaw_start))
	_stick_push(Vector2(0.0, -1.0))
	await _seconds(0.6)
	await _pose("running")
	# Second thumb looks while the first keeps walking: 200 canvas units at
	# the default 0.003 rad/unit must turn exactly 0.6 rad.
	var look_at := Vector2(_viewport_size().x * 0.78, _viewport_size().y * 0.45)
	_touch(1, look_at, true)
	for i in 4:
		_drag(1, look_at + Vector2(50.0 * (i + 1), 0.0), Vector2(50.0, 0.0))
		await _frames(1)
	_touch(1, look_at + Vector2(200.0, 0.0), false)
	await _seconds(0.4)
	var moved := _position() - start
	var along := Vector2(moved.x, moved.z).dot(forward)
	_touch(0, _main._touch.stick_home(), false)
	await _seconds(0.6)
	var turned: float = yaw_start - _main._yaw
	if along < 2.5:
		return _fail("stick moved the player only %.2f m forward" % along)
	if absf(turned - 0.6) > 0.02:
		return _fail("look drag turned %.3f rad, expected 0.600" % turned)
	var speed := Vector2(_velocity().x, _velocity().z).length()
	if speed > 0.6:
		return _fail("player still moving %.2f m/s after the stick was released" % speed)
	# 4.9 m of walking at a 0.69 m stride is several footfalls, each a cue.
	if _main._audio.steps < 4:
		return _fail("walking %.2f m played %d footsteps" % [along, _main._audio.steps])
	# Push beyond the touch ring to sprint; releasing it must clear the latch.
	var home: Vector2 = _main._touch.stick_home()
	var overshoot := TouchControls.STICK_THROW * float(_main._touch._u) * 1.35
	_touch(0, home, true)
	_drag(0, home + Vector2(0, -overshoot), Vector2(0, -overshoot))
	await _seconds(0.8)
	if not bool(_native().is_player_sprinting()) or Vector2(_velocity().x, _velocity().z).length() < 7.9:
		return _fail("touch overshoot did not produce native sprint")
	_touch(0, home, false)
	await _seconds(0.6)
	if bool(_native().is_player_sprinting()) or Vector2(_velocity().x, _velocity().z).length() > 0.6:
		return _fail("released sprint left movement latched")
	_detail = "forward_m=%.2f turned_rad=%.3f settle_mps=%.2f steps=%d" % [along, turned, speed,
		_main._audio.steps]
	return true


# Gyro aim is inert until the player turns it on; then the device's own
# rotation, injected where the platform writes it, turns the view by rate x
# time, and the native walk that follows heads where the view now faces.
func _touch_gyro_aim() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not bool(ProjectSettings.get_setting("input_devices/sensors/enable_gyroscope", false)):
		return _fail("project.godot does not enable the Android gyroscope sensor")
	var yaw_off: float = _main._yaw
	Input.set_gyroscope(Vector3(0.0, -1.0, 0.0))
	await _frames(30)
	Input.set_gyroscope(Vector3.ZERO)
	if absf(_main._yaw - yaw_off) > 1.0e-6:
		return _fail("the device turned the view with GYRO AIM off")
	_main._settings.gyro_aim = true
	_main._apply_settings()
	# 30 frames of a fixed 60 fps clock at 1 rad/s right and 0.4 rad/s up.
	var yaw_start: float = _main._yaw
	var pitch_start: float = _main._pitch
	Input.set_gyroscope(Vector3(0.4, -1.0, 0.0))
	await _frames(30)
	Input.set_gyroscope(Vector3.ZERO)
	await _frames(2)
	var turned: float = yaw_start - _main._yaw
	var raised: float = _main._pitch - pitch_start
	if absf(turned - 0.5) > 0.02:
		return _fail("turning the device right turned the view %.3f rad, expected 0.500" % turned)
	if absf(raised - 0.2) > 0.02:
		return _fail("tipping the device up raised the view %.3f rad, expected 0.200" % raised)
	await _pose("gyro_turned")
	var yaw_before_bad: float = _main._yaw
	Input.set_gyroscope(Vector3(NAN, INF, 0.0))
	await _frames(3)
	Input.set_gyroscope(Vector3.ZERO)
	if not is_equal_approx(_main._yaw, yaw_before_bad) or not is_finite(_main._pitch):
		return _fail("a non-finite gyro sample moved or corrupted the view")
	var start := _position()
	var forward := Vector2(-sin(_main._yaw), -cos(_main._yaw))
	_stick_push(Vector2(0.0, -1.0))
	await _seconds(0.6)
	_touch(0, _main._touch.stick_home(), false)
	var moved := _position() - start
	var heading := Vector2(moved.x, moved.z)
	if heading.length() < 1.0:
		return _fail("stick moved the player only %.2f m" % heading.length())
	var alignment := heading.normalized().dot(forward)
	if alignment < 0.98:
		return _fail("the walk heads %.3f off the gyro-turned view" % acos(clampf(alignment, -1.0, 1.0)))
	_detail = "turned_rad=%.3f raised_rad=%.3f walk_alignment=%.4f" % [turned, raised, alignment]
	return true


func _touch_climb(expected: StringName, expected_state: int) -> bool:
	var reached: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == expected, 2.0)
	if not reached:
		return _fail("ACTION never offered %s (got %s)" % [expected, _ctx()["action"]["id"]])
	if _main._touch.button_label(&"action") != "CLIMB":
		return _fail("ACTION button label was '%s'" % _main._touch.button_label(&"action"))
	await _pose("climb_prompt")
	var start_y := _position().y
	_tap(0, _center(&"action"))
	var reached_2: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == expected_state, 0.2)
	if not reached_2:
		return _fail("ACTION did not commit traversal state %d" % expected_state)
	# The native step-in closes on the wall before the body rises, so both
	# palms land on the lip, not on air an arm's length short of it.
	var both_planted: bool = await _wait_until(func() -> bool: return _main._arms.planted_count() == 2, 0.6)
	if not both_planted:
		return _fail("hands never planted on the lip during the standing mantle")
	await _pose("ground_mantle")
	var completed_grounded := func() -> bool:
		return int(_native().get_traversal_state()) == 0 and bool(_native().is_player_grounded())
	var completed: bool = await _wait_until(completed_grounded, 2.5)
	if not completed:
		return _fail("traversal never completed grounded")
	var rise := _position().y - start_y
	_detail = "state=%d rise_m=%.2f" % [expected_state, rise]
	return rise > 0.3


func _touch_vault() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_stick_push(Vector2(0.0, -1.0))
	# The rail is offered once the approach has real momentum; tap the moment
	# ACTION says CLIMB, exactly like a thumb would.
	var reached: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"climb", 2.5)
	if not reached:
		return _fail("ACTION never offered the vault rail")
	_tap(1, _center(&"action"))
	var reached_2: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 3, 0.2)
	if not reached_2:
		return _fail("ACTION did not commit a vault (state %d)" % int(_native().get_traversal_state()))
	await _pose("vault")
	_touch(0, _main._touch.stick_home(), false)
	var reached_3: bool = await _wait_until(func() -> bool: return int(_native().get_accepted_traversal_count()) >= 1, 1.5)
	if not reached_3:
		return _fail("vault never completed")
	_detail = "x_after=%.2f" % _position().x
	return _position().x > 5.6


# The same rail, vaulted with JUMP twice and ACTION never touched.
func _touch_double_tap_vault() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_stick_push(Vector2(0.0, -1.0))
	var reached: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"climb", 2.5)
	if not reached:
		return _fail("the approach never reached the vault rail")
	_tap(1, _center(&"jump"))
	var lifted: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_player_grounded()), 0.3)
	if not lifted:
		return _fail("the first JUMP did not leave the ground")
	await _seconds(0.08)
	_tap(1, _center(&"jump"))
	# Committed in the air, from the tap itself -- not by a buffered press
	# that waited for the feet to find the rail top.
	var landed := [false]
	var vaulting: bool = await _wait_until(func() -> bool:
		if int(_native().get_traversal_state()) == 3:
			return true
		landed[0] = landed[0] or bool(_native().is_player_grounded())
		return false, 0.25)
	if not vaulting or landed[0]:
		return _fail("the second JUMP did not commit a vault in the air (state %d, landed %s)" % [
			int(_native().get_traversal_state()), str(landed[0])])
	_touch(0, _main._touch.stick_home(), false)
	var done: bool = await _wait_until(func() -> bool: return int(_native().get_accepted_traversal_count()) >= 1, 1.5)
	if not done:
		return _fail("double-tap vault never completed")
	_detail = "x_after=%.2f" % _position().x
	return _position().x > 5.6


# The crawl beam beside the dock has its underside 1.45 m up. Standing, the
# walk stops at its face; CROUCH crouches the native body and the same walk
# passes under; STAND raises it again in the open. The eye follows the body.
func _touch_crouch() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not _main._touch.is_button_shown(&"crouch") or _main._touch.button_label(&"crouch") != "CROUCH":
		return _fail("CROUCH was not offered standing on the deck")
	_stick_push(Vector2(0.0, -1.0))
	await _seconds(1.6)
	var blocked_z := _position().z
	if blocked_z < -12.7:
		return _fail("a standing body walked in under the 1.45 m beam (z %.2f)" % blocked_z)
	if bool(_native().is_player_crouched()):
		return _fail("the body crouched before CROUCH was pressed")
	await _pose("beam_blocks")
	_tap(1, _center(&"crouch"))
	var crouched: bool = await _wait_until(
		func() -> bool: return bool(_native().is_player_crouched()), 0.2)
	if not crouched:
		return _fail("CROUCH did not crouch the native body")
	var passed: bool = await _wait_until(func() -> bool: return _position().z < -13.9, 3.0)
	if not passed:
		return _fail("the crouched walk did not pass under the beam (z %.2f)" % _position().z)
	if not bool(_native().is_player_crouched()):
		return _fail("the body stood up while CROUCH was on")
	_touch(0, _main._touch.stick_home(), false)
	await _seconds(0.4)
	var passed_z := _position().z
	var eye_crouched := _eye_over_soles()
	if _main._touch.button_label(&"crouch") != "STAND":
		return _fail("the crouch button did not read STAND while crouched")
	await _pose("crouched")
	_tap(1, _center(&"crouch"))
	var stood: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_player_crouched()), 0.2)
	if not stood:
		return _fail("STAND did not stand the native body in the open")
	await _seconds(0.4)
	var eye_standing := _eye_over_soles()
	if absf(eye_crouched - 0.95) > 0.03 or absf(eye_standing - 1.52) > 0.03:
		return _fail("eye over the soles %.2f crouched / %.2f standing, expected 0.95 / 1.52" % [
			eye_crouched, eye_standing])
	_detail = "blocked_z=%.2f passed_z=%.2f eye_crouched=%.2f eye_standing=%.2f" % [
		blocked_z, passed_z, eye_crouched, eye_standing]
	return true


# AS-003 from the cage floor, all through the touch pipeline: facing the bar
# across the inside of the door, Action reads PICK UP and takes it; backed off
# with it, SET DOWN leaves the door to swing open on its own drive; facing the
# rack, PICK UP takes the hook block with both hands shown on it; and it is
# carried out through the doorway and set down on the apron.
func _touch_carry() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_main._yaw = 0.0  # south, at the bar
	_stick_push(Vector2(0.0, -1.0))
	var at_bar: bool = await _wait_until(func() -> bool: return _position().z <= -86.75, 2.0)
	_touch(0, _main._touch.stick_home(), false)
	if not at_bar:
		return _fail("never reached the bar (z %.2f)" % _position().z)
	await _seconds(0.4)
	if _main._touch.button_label(&"action") != "PICK UP":
		return _fail("Action read '%s' at the bar, not PICK UP" % _main._touch.button_label(&"action"))
	_tap(1, _center(&"action"))
	var holding_bar: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 55, 0.3)
	if not holding_bar:
		return _fail("PICK UP did not put the bar on the native carry point")
	_stick_push(Vector2(0.0, 1.0))  # back away north, the bar still ahead
	await _wait_until(func() -> bool: return _position().z >= -85.7, 2.0)
	_touch(0, _main._touch.stick_home(), false)
	await _seconds(0.3)
	if _main._touch.button_label(&"action") != "SET DOWN":
		return _fail("Action read '%s' holding the bar, not SET DOWN" % _main._touch.button_label(&"action"))
	_tap(1, _center(&"action"))
	var bar_down: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.3)
	if not bar_down:
		return _fail("SET DOWN did not take the bar off the carry point")
	var opened: bool = await _wait_until(
		func() -> bool: return float(_native().get_hook5_door_angle_radians()) >= 1.2, 5.0)
	if not opened:
		return _fail("the door never travelled once the bar was set down (%.2f rad)" %
			float(_native().get_hook5_door_angle_radians()))
	await _pose("door_open")
	_main._yaw = PI  # north, toward the rack's corner
	_stick_push(Vector2(0.0, -1.0))
	await _wait_until(func() -> bool: return _position().z >= -84.9, 2.0)
	_touch(0, _main._touch.stick_home(), false)
	_main._yaw = -PI * 0.5  # east, at the block
	await _seconds(0.4)
	if _main._touch.button_label(&"action") != "PICK UP":
		return _fail("Action read '%s' at the rack, not PICK UP" % _main._touch.button_label(&"action"))
	_tap(1, _center(&"action"))
	var holding_block: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 56, 0.3)
	if not holding_block:
		return _fail("PICK UP did not put the hook block on the native carry point")
	await _seconds(0.5)
	if _main._arms.hand_poses() != [8, 8]:
		return _fail("hands not on the block (poses %s)" % str(_main._arms.hand_poses()))
	await _pose("holding_block")
	# Out through the doorway, east of the travelled leaf.
	for leg in [Vector2(10.7, -86.8), Vector2(10.7, -89.6)]:
		var at: Vector3 = _position()
		_main._yaw = atan2(-(leg.x - at.x), -(leg.y - at.z))
		_stick_push(Vector2(0.0, -1.0))
		var arrived: bool = await _wait_until(func() -> bool: return _position().z <= leg.y + 0.1, 3.0)
		_touch(0, _main._touch.stick_home(), false)
		if not arrived:
			return _fail("the carry out stalled at (%.2f, %.2f)" % [_position().x, _position().z])
	await _seconds(0.4)
	if int(_native().get_carrying_entity_id()) != 56 or bool(_native().is_hook_in_rack()):
		return _fail("the block was not held off its rack outside the cage")
	await _pose("carried_out")
	_tap(1, _center(&"action"))
	var set_down: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.3)
	if not set_down:
		return _fail("SET DOWN did not release the block outside")
	await _seconds(1.5)
	var block: Vector3 = _native().get_hook5_block_position()
	# Back off south-east and look at the cage the way it is approached.
	_main._yaw = atan2(-(10.3 - 13.5), -(-86.0 - (-94.0)))
	_stick_push(Vector2(0.0, 1.0))
	await _seconds(1.1)
	_touch(0, _main._touch.stick_home(), false)
	_main._pitch = 0.12
	await _seconds(0.6)
	await _pose("cage_exterior")
	_detail = "door_rad=%.2f block=(%.2f,%.2f,%.2f)" % [
		float(_native().get_hook5_door_angle_radians()), block.x, block.y, block.z]
	return block.y < 0.4 and block.z < -88.0


func _eye_over_soles() -> float:
	var half := 0.6 if bool(_native().is_player_crouched()) else 0.9
	return _main._camera.position.y - (_position().y - half)


func _touch_hang(climb: bool) -> bool:
	var reached: bool = await _wait_until(func() -> bool: return bool(_ctx()["hanging"]), 3.0)
	if not reached:
		return _fail("pushing into the wall never produced a hang")
	_touch(0, _main._touch.stick_home(), false)
	await _seconds(0.3)
	if not _main._touch.is_button_shown(&"drop"):
		return _fail("DROP was not offered while hanging")
	if _main._touch.button_label(&"jump") != "CLIMB UP":
		return _fail("JUMP did not relabel to CLIMB UP while hanging")
	# Both hands on the native ledge, not near it: the rendered wrists sit on
	# the grip points the native ledge point implies.
	var grip_error: float = _main._arms.anchored_error()
	if _main._arms.hand_poses() != [4, 4] or grip_error < 0.0 or grip_error > 0.02:
		return _fail("hands not gripping the native ledge (poses %s, error %.3f m)" % [
			str(_main._arms.hand_poses()), grip_error])
	# Native per-hand grips, not a reconstructed central lip, own the pose.
	var normal: Vector3 = _native().get_traversal_normal()
	for hand in _main._arms._hands:
		var hold: Vector3 = _native().get_traversal_left_hand_render_position() if hand.side < 0.0 \
			else _native().get_traversal_right_hand_render_position()
		var expected: Vector3 = hold - normal * (_main._arms.TOP_PROBE_INSET + 0.034) + Vector3.DOWN * 0.074
		if hand.anchor.distance_to(expected) > 0.002:
			return _fail("hanging wrist target does not follow its native hand")
	# Looking around moves the head, not the loaded torso or planted wrists.
	var old_yaw: float = _main._yaw
	var old_pitch: float = _main._pitch
	_main._yaw += PI * 0.5
	_main._pitch = 0.2
	await _seconds(0.1)
	var head_grip_error: float = _main._arms.anchored_error()
	var hand_span: Vector3 = _native().get_traversal_right_hand_render_position() - _native().get_traversal_left_hand_render_position()
	var shoulder_shift := 0.0
	var shoulders: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO]
	for hand in _main._arms._hands:
		var shoulder: Vector3 = 2.0 * hand.upper.global_position - hand.elbow.global_position
		var base: Vector3 = _native().get_player_render_position() + Vector3.UP * _main._arms.HANG_SHOULDER_HEIGHT \
			+ hand_span.normalized() * (_main._arms.HANG_SHOULDER_HALF_SPAN * hand.side) \
			- normal * _main._arms.HANG_SHOULDER_BACK
		shoulder_shift = maxf(shoulder_shift, shoulder.distance_to(base))
		shoulders[0 if hand.side < 0.0 else 1] = shoulder
		var forearm_length: float = hand.elbow.global_position.distance_to(hand.root.global_position)
		if absf(forearm_length - _main._arms.FOREARM) > 0.002:
			return _fail("loaded hang stretched a forearm: %.4f m" % forearm_length)
	if int(_native().get_traversal_state()) != 1 or _main._arms.hand_poses() != [4, 4] \
			or head_grip_error < 0.0 or head_grip_error > 0.02:
		return _fail("loaded hanging wrist left its native grip after head turn: %.4f m" % head_grip_error)
	# A planted wrist alone can hide crossed shoulders. Each shoulder must
	# remain on its own side of the actual native hand pair.
	if (shoulders[1] - shoulders[0]).dot(hand_span) <= 0.0:
		return _fail("hanging shoulders cross their native hand assignment")
	print("SCRAPERX_HANG_RIG shoulder_shift_m=%.4f shoulder_span_m=%.4f wrist_error_m=%.4f" % [
		shoulder_shift, shoulders[0].distance_to(shoulders[1]), head_grip_error])
	_main._yaw = old_yaw
	_main._pitch = old_pitch
	await _frames(2)
	await _pose("hanging")
	if climb:
		_tap(1, _center(&"jump"))
		var reached_2: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 2, 0.2)
		if not reached_2:
			return _fail("CLIMB UP did not commit the mantle")
		var both_planted: bool = await _wait_until(func() -> bool: return _main._arms.planted_count() == 2, 0.3)
		if not both_planted:
			return _fail("hands never planted on the ledge during the mantle")
		await _pose("mantle_mid")
		var reached_3: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.5)
		if not reached_3:
			return _fail("mantle from hang never landed")
		_detail = "top_y=%.2f grip_error_m=%.4f head_grip_error_m=%.4f" % [_position().y, grip_error, head_grip_error]
		return _position().y > 4.4
	_tap(1, _center(&"drop"))
	var reached_4: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 0, 0.2)
	if not reached_4:
		return _fail("DROP did not release the hang")
	await _frames(2)
	_detail = "vy_after_release=%.2f grip_error_m=%.4f head_grip_error_m=%.4f" % [_velocity().y, grip_error, head_grip_error]
	return _velocity().y <= 0.0


func _touch_chute() -> bool:
	var reached: bool = await _wait_until(func() -> bool: return _main._touch.is_button_shown(&"chute"), 3.0)
	if not reached:
		return _fail("CHUTE never offered in a real free fall")
	var offered_at := -_velocity().y
	await _frames(3)
	_tap(0, _center(&"chute"))
	var reached_2: bool = await _wait_until(func() -> bool: return bool(_native().is_parachute_deployed()), 0.2)
	if not reached_2:
		return _fail("CHUTE did not deploy the canopy")
	await _seconds(0.6)
	if not _main._arms.risers_visible() or _main._arms.hand_poses() != [6, 6]:
		return _fail("hands are not on the canopy toggles (poses %s)" % str(_main._arms.hand_poses()))
	await _pose("canopy")
	var reached_3: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 15.0)
	if not reached_3:
		return _fail("canopy descent never landed")
	await _frames(2)
	if int(_native().get_death_count()) != 0:
		return _fail("landed dead at %.1f m/s" % float(_native().get_last_impact_speed_mps()))
	_detail = "offered_at_mps=%.2f impact_mps=%.2f" % [offered_at,
		float(_native().get_last_impact_speed_mps())]
	return true


func _touch_lethal_feedback() -> bool:
	var reached: bool = await _wait_until(func() -> bool: return float(_ctx()["danger"]) >= 0.8, 5.0)
	if not reached:
		return _fail("fall danger never reached 0.8 of the native lethal speed")
	await _pose("fall_danger")
	var reached_2: bool = await _wait_until(func() -> bool: return int(_native().get_death_count()) >= 1, 6.0)
	if not reached_2:
		return _fail("unmitigated fall was not lethal")
	await _frames(3)
	var toasts: Array = _main._hud._toasts
	if toasts.is_empty() or String(toasts[-1]["title"]) != "LETHAL IMPACT":
		return _fail("restore was not announced")
	await _seconds(0.3)
	await _pose("lethal_toast")
	var sub := String(toasts[-1]["sub"])
	var fell_at := sub.get_slice(" ", 2).to_float()
	var lethal := float(_native().get_lethal_impact_speed_mps())
	_detail = "toast='%s' lethal_mps=%.1f" % [sub, lethal]
	# The announced speed must be the lethal fall it reports, not a settle.
	return fell_at > lethal


func _touch_sump() -> bool:
	var reached: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"valve", 2.0)
	if not reached:
		return _fail("ACTION never offered the sump valve")
	var before := bool(_native().is_sump_isolated())
	_tap(0, _center(&"action"))
	var reached_2: bool = await _wait_until(func() -> bool: return bool(_native().is_sump_isolated()) != before, 0.2)
	if not reached_2:
		return _fail("ACTION did not toggle the native sump valve")
	_detail = "isolated %s->%s" % [before, not before]
	return true



func _touch_water_screw() -> bool:
	var reached: bool = await _wait_until(
		func() -> bool: return _ctx()["action"]["id"] == &"screw_toggle", 2.0)
	if not reached:
		return _fail("ACTION never offered the ground screw control")
	if bool(_native().is_water_screw_motor_enabled()):
		return _fail("ground screw must start stopped")
	_tap(0, _center(&"action"))
	var started: bool = await _wait_until(
		func() -> bool: return bool(_native().is_water_screw_motor_enabled()), 0.3)
	if not started:
		return _fail("ACTION did not start the authoritative screw motor")
	var turning: bool = await _wait_until(
		func() -> bool: return absf(float(_native().get_water_screw_rpm())) > 2.0, 1.5)
	if not turning:
		return _fail("started screw did not produce native shaft rotation")
	await _pose("water_screw_running")
	_detail = "rpm=%.1f torque_nm=%.0f tank_m3=%.3f" % [
		float(_native().get_water_screw_rpm()),
		float(_native().get_water_screw_motor_torque_nm()),
		float(_native().get_water_screw_tank_volume_m3())]
	return true


func _touch_water_lift() -> bool:
	var reached: bool = await _wait_until(
		func() -> bool: return _ctx()["action"]["id"] == &"water_lift_valve", 2.0)
	if not reached:
		return _fail("ACTION never offered the water-lift fill valve")
	if bool(_native().is_water_lift_valve_open()):
		return _fail("water-lift fill valve must start shut")
	_tap(0, _center(&"action"))
	var opened: bool = await _wait_until(
		func() -> bool: return bool(_native().is_water_lift_valve_open()), 0.3)
	if not opened:
		return _fail("ACTION did not open the authoritative water-lift valve")
	await _pose("water_lift_fill_control")
	_detail = "valve_open=1 bucket_m3=%.3f cage_m=%.3f" % [
		float(_native().get_water_lift_bucket_water_m3()),
		float(_native().get_water_lift_cage_travel_m())]
	return true


func _touch_pendant() -> bool:
	var reached: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"operate", 2.0)
	if not reached:
		return _fail("ACTION never offered OPERATE at the yard jib pendant")
	_tap(0, _center(&"action"))
	await _frames(3)
	if _main._operating != &"intake" or not _main._touch.is_button_shown(&"pendant_left"):
		return _fail("OPERATE did not open the pendant controls")
	if not _main._arms.remote_visible():
		return _fail("the crane remote is not in hand while operating")
	await _seconds(0.3)
	await _pose("pendant")
	var boom_before := float(_native().get_intake_boom_angle_radians())
	_touch(1, _center(&"pendant_left"), true)
	await _seconds(1.0)
	_touch(1, _center(&"pendant_left"), false)
	var boom_after := float(_native().get_intake_boom_angle_radians())
	var hook_before := float(_native().get_intake_hook_position().y)
	_touch(1, _center(&"pendant_up"), true)
	await _seconds(1.0)
	_touch(1, _center(&"pendant_up"), false)
	var hook_after := float(_native().get_intake_hook_position().y)
	_tap(0, _center(&"action"))
	await _frames(3)
	if absf(boom_after - boom_before) < 0.02:
		return _fail("holding SLEW did not move the native boom (%.4f rad)" % (boom_after - boom_before))
	if absf(hook_after - hook_before) < 0.05:
		return _fail("holding HOIST did not move the native hook (%.4f m)" % (hook_after - hook_before))
	if _main._operating != &"":
		return _fail("DONE did not close the pendant controls")
	_detail = "boom_delta_rad=%.3f hook_delta_m=%.3f" % [boom_after - boom_before, hook_after - hook_before]
	return true


func _touch_pause() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_tap(0, _center(&"pause"))
	await _frames(2)
	if not get_tree().paused:
		return _fail("PAUSE did not pause the tree")
	var tick := int(_native().get_tick_index())
	await _frames(12)
	if int(_native().get_tick_index()) != tick:
		return _fail("the native clock advanced while paused")
	await _pose("pause_menu")
	# A real touch reaches the menu as its emulated mouse twin.
	_click(_main._pause_menu.side_button_center(&"controls"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"controls":
		return _fail("tapping CONTROLS did not open the controls page")
	await _pose("pause_controls")
	_click(_main._pause_menu.side_button_center(&"settings"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"settings":
		return _fail("tapping SETTINGS did not open the settings page")
	var overflow: float = _main._pause_menu.settings_overflow()
	if overflow > 0.0:
		return _fail("the settings rows overflow their plate by %.0f px" % overflow)
	await _pose("pause_settings")
	# GRAPHICS: one tap on QUALITY steps HIGH -> ULTRA, and the preset must
	# reach the renderer, not just the label.
	_click(_main._pause_menu.side_button_center(&"graphics"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"graphics":
		return _fail("tapping GRAPHICS did not open the graphics page")
	_click(_main._pause_menu.page_first_center(&"graphics"))
	await _frames(2)
	var sun: DirectionalLight3D = _main.get_node("Overcast")
	if _main._settings.quality != 3 or get_viewport().msaa_3d != Viewport.MSAA_4X \
			or not is_equal_approx(sun.directional_shadow_max_distance, 300.0):
		return _fail("QUALITY ULTRA did not reach the renderer (quality %d, msaa %d, shadow %.0f m)" % [
			_main._settings.quality, get_viewport().msaa_3d, sun.directional_shadow_max_distance])
	await _pose("pause_graphics")
	_click(_main._pause_menu.side_button_center(&"display"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"display":
		return _fail("tapping DISPLAY did not open the display page")
	await _pose("pause_display")
	_click(_main._pause_menu.side_button_center(&"audio"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"audio":
		return _fail("tapping AUDIO did not open the audio page")
	await _pose("pause_audio")
	_click(_main._pause_menu.side_button_center(&"resume"))
	await _frames(3)
	if get_tree().paused:
		return _fail("tapping RESUME did not resume")
	var resumed := int(_native().get_tick_index())
	await _frames(6)
	_detail = "frozen_tick=%d advanced_to=%d" % [tick, int(_native().get_tick_index())]
	return int(_native().get_tick_index()) > resumed


func _pad_core() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var start := _position()
	var yaw_start: float = _main._yaw
	var forward := Vector2(-sin(yaw_start), -cos(yaw_start))
	_axis(JOY_AXIS_LEFT_Y, -1.0)
	await _seconds(1.0)
	_axis(JOY_AXIS_LEFT_Y, 0.0)
	var moved := _position() - start
	var along := Vector2(moved.x, moved.z).dot(forward)
	if _main._router.device != InputRouter.Device.GAMEPAD:
		return _fail("stick input did not switch the interface to gamepad")
	await _seconds(0.5)
	_button(JOY_BUTTON_A)
	var lifted: bool = await _wait_until(func() -> bool: return _velocity().y > 2.0, 0.4)
	if not lifted:
		return _fail("A did not jump")
	await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0)
	var yaw_before: float = _main._yaw
	_axis(JOY_AXIS_RIGHT_X, 1.0)
	await _seconds(0.5)
	_axis(JOY_AXIS_RIGHT_X, 0.0)
	var turned: float = yaw_before - _main._yaw
	_button(JOY_BUTTON_RIGHT_STICK)
	var r3_crouched: bool = await _wait_until(
		func() -> bool: return bool(_native().is_player_crouched()), 0.3)
	_button(JOY_BUTTON_RIGHT_STICK)
	var r3_stood: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_player_crouched()), 0.3)
	if not (r3_crouched and r3_stood):
		return _fail("the right-stick click did not toggle crouch on and off")
	_button(JOY_BUTTON_START)
	await _frames(2)
	if not get_tree().paused:
		return _fail("START did not pause")
	var tick := int(_native().get_tick_index())
	await _frames(10)
	var frozen := int(_native().get_tick_index()) == tick
	_button(JOY_BUTTON_START)
	await _frames(3)
	if get_tree().paused:
		return _fail("START did not resume")
	if along < 2.5:
		return _fail("left stick moved only %.2f m" % along)
	if turned < 0.8:
		return _fail("right stick turned only %.3f rad in 0.5 s" % turned)
	if not frozen:
		return _fail("native clock advanced while paused")
	_detail = "forward_m=%.2f turned_rad=%.3f" % [along, turned]
	return true


func _pad_pendant() -> bool:
	var reached: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"operate", 2.0)
	if not reached:
		return _fail("ACTION never offered OPERATE at the KX-JIB pendant")
	_button(JOY_BUTTON_X)
	await _frames(3)
	if _main._operating != &"jib":
		return _fail("X did not open the KX-JIB pendant")
	await _seconds(0.3)
	await _pose("pad_pendant")
	var boom_before := float(_native().get_jib_boom_angle_radians())
	_button(JOY_BUTTON_DPAD_LEFT, true)
	await _seconds(1.0)
	_button(JOY_BUTTON_DPAD_LEFT, false)
	var boom_after := float(_native().get_jib_boom_angle_radians())
	var hook_before := float(_native().get_jib_hook_position().y)
	_axis(JOY_AXIS_TRIGGER_RIGHT, 1.0)
	await _seconds(1.0)
	_axis(JOY_AXIS_TRIGGER_RIGHT, 0.0)
	var hook_after := float(_native().get_jib_hook_position().y)
	_button(JOY_BUTTON_B)
	await _frames(3)
	if absf(boom_after - boom_before) < 0.02:
		return _fail("D-pad did not drive the native jib (%.4f rad)" % (boom_after - boom_before))
	if absf(hook_after - hook_before) < 0.05:
		return _fail("RT did not hoist the native hook (%.4f m)" % (hook_after - hook_before))
	if _main._operating != &"":
		return _fail("B did not leave the pendant controls")
	_detail = "boom_delta_rad=%.3f hook_delta_m=%.3f" % [boom_after - boom_before, hook_after - hook_before]
	return true


func _keyboard_core() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var start := _position()
	var yaw_start: float = _main._yaw
	var forward := Vector2(-sin(yaw_start), -cos(yaw_start))
	_key(KEY_W, true)
	await _seconds(1.0)
	_key(KEY_W, false)
	var moved := _position() - start
	var along := Vector2(moved.x, moved.z).dot(forward)
	await _seconds(0.5)
	_key(KEY_SPACE, true)
	_key(KEY_SPACE, false)
	var lifted: bool = await _wait_until(func() -> bool: return _velocity().y > 2.0, 0.4)
	await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0)
	await _seconds(0.3)
	var rejected_before := int(_native().get_rejected_traversal_count())
	_key(KEY_E, true)
	_key(KEY_E, false)
	await _frames(3)
	var rejected_after := int(_native().get_rejected_traversal_count())
	_key(KEY_CTRL, true)
	var ctrl_crouched: bool = await _wait_until(
		func() -> bool: return bool(_native().is_player_crouched()), 0.3)
	_key(KEY_CTRL, false)
	var ctrl_stood: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_player_crouched()), 0.3)
	_key(KEY_C, true)
	_key(KEY_C, false)
	var c_crouched: bool = await _wait_until(
		func() -> bool: return bool(_native().is_player_crouched()), 0.3)
	_key(KEY_C, true)
	_key(KEY_C, false)
	var c_stood: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_player_crouched()), 0.3)
	_key(KEY_ESCAPE, true)
	_key(KEY_ESCAPE, false)
	await _frames(2)
	var paused := get_tree().paused
	await _pose("keyboard_pause")
	_key(KEY_ESCAPE, true)
	_key(KEY_ESCAPE, false)
	await _frames(3)
	if along < 2.5:
		return _fail("W moved only %.2f m" % along)
	if not lifted:
		return _fail("SPACE did not jump")
	if rejected_after != rejected_before + 1:
		return _fail("E with nothing in reach did not reach the native (rejected %d->%d)" % [
			rejected_before, rejected_after])
	if not (ctrl_crouched and ctrl_stood):
		return _fail("holding Ctrl did not crouch, or letting go did not stand")
	if not (c_crouched and c_stood):
		return _fail("C did not toggle crouch on and off")
	if not paused:
		return _fail("ESC did not pause")
	if get_tree().paused:
		return _fail("ESC did not resume from the menu")
	_detail = "forward_m=%.2f rejected=%d->%d" % [along, rejected_before, rejected_after]
	return true


# AS-006 Stage A through the real input pipeline on one device: walk into
# the skip lift's cage, UNHOOK the rope's end from the bollard, HOOK it onto
# the cage's eye, GRAB the trip handle, step back until the catch lets go,
# LET GO, and ride 22 m. Each verb is the one the HUD offers at that moment,
# pressed on the device under test, and each is proven by the native state
# it changed. The hands must move between poses, never jump.
func _rig(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	for leg in [Vector2(-10.0, -129.2), Vector2(-10.2, -130.6), Vector2(-11.35, -131.95)]:
		if not await _walk_to(device, leg, 0.2):
			return _fail("the walk into the cage stalled at (%.2f, %.2f)" % [
				_position().x, _position().z])
	await _face(Vector2(-0.7, -0.7))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s' facing the bollard, not UNHOOK" % _action_label())
	var watch := _watch_wrists()
	_act(device)
	var holding_shackle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2002, 0.5)
	if not holding_shackle:
		return _fail("UNHOOK did not put the rope's shackle in the hands")
	if not await _walk_to(device, Vector2(-11.35, -131.4), 0.2):
		return _fail("the step to the cage's eye stalled")
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at the cage's eye, not HOOK ONTO CAGE EYE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("rig_shackle")
	_act(device)
	var hooked: bool = await _wait_until(
		func() -> bool: return int(_native().get_well_a_rope_end_entity_id()) == 2000, 0.5)
	if not hooked:
		return _fail("HOOK did not put the rope's end on the cage's eye (end %d, at %s, carrying %d)" % [
			int(_native().get_well_a_rope_end_entity_id()), str(_position()),
			int(_native().get_carrying_entity_id())])
	if not await _walk_to(device, Vector2(-11.05, -130.35), 0.2):
		return _fail("the step to the trip handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing the trip handle, not GRAB" % _action_label())
	_act(device)
	var holding_handle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2004, 0.5)
	if not holding_handle:
		return _fail("GRAB did not put the trip handle in the hands")
	await _seconds(0.5)
	if _main._arms.hand_poses() != [8, 8]:
		return _fail("hands not on the trip handle (poses %s)" % str(_main._arms.hand_poses()))
	if _action_label() != "LET GO":
		return _fail("Action read '%s' holding the handle, not LET GO" % _action_label())
	await _pose("rig_handle")
	_move(device, -0.6)
	var tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_well_a_catch_latched()), 3.0)
	_move(device, 0.0)
	if not tripped:
		return _fail("stepping back with the handle never opened the catch")
	_act(device)
	var let_go: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)
	if not let_go:
		return _fail("LET GO did not take the handle out of the hands")
	var worst_jump := _stop_watch(watch)
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_a_cage_travel()) >= 21.95, 15.0)
	if not arrived:
		return _fail("the cage never reached the top (travel %.2f m)" %
			float(_native().get_well_a_cage_travel()))
	await _seconds(0.5)
	await _pose("rig_top")
	if int(_native().get_support_entity_id()) != 2000 or _position().y < 177.0:
		return _fail("the rider is not standing in the cage at the top (y %.2f)" % _position().y)
	# 0.10 m in a 60 Hz frame is 6 m/s across the view: faster than any reach.
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true


# Turns the view to face `direction` the way a thumb or a stick does, at up
# to 4 rad/s, never in one frame.
func _face(direction: Vector2) -> void:
	var goal := atan2(-direction.x, -direction.y)
	while true:
		var left := wrapf(goal - float(_main._yaw), -PI, PI)
		var step := 4.0 * get_process_delta_time()
		if absf(left) <= step:
			_main._yaw = goal
			return
		_main._yaw = float(_main._yaw) + signf(left) * step
		await get_tree().process_frame


func _action_label() -> String:
	return String(_ctx()["action"]["label"])


# Waits for the HUD to offer `id` with `label` (and `detail`, when given) on
# the Action verb: what a player reads before pressing.
func _offered(id: StringName, label: String, detail: String = "") -> bool:
	return await _wait_until(func() -> bool: return _ctx()["action"]["id"] == id and \
		_action_label() == label and \
		(detail.is_empty() or String(_ctx()["action"]["detail"]) == detail), 1.5)


# One press of the Action verb on the device under test.
func _act(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			_tap(1, _center(&"action"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_X)
		_:
			_key(KEY_E, true)
			_key(KEY_E, false)


# Holds forward (+) or back (-) at `amount` of full throw on the device under
# test; 0 lets go.
func _move(device: int, amount: float) -> void:
	_move_dir(device, Vector2(0.0, amount))


# Holds the move input at `v` = (right, forward) of full throw, relative to
# the view, on the device under test. A key has no throw: a component past
# a third holds its key down.
func _move_dir(device: int, v: Vector2) -> void:
	match device:
		InputRouter.Device.TOUCH:
			if v.length() < 0.01:
				_touch(0, _main._touch.stick_home(), false)
			else:
				_stick_hold(Vector2(v.x, -v.y), minf(v.length(), 1.0))
		InputRouter.Device.GAMEPAD:
			_axis(JOY_AXIS_LEFT_X, v.x)
			_axis(JOY_AXIS_LEFT_Y, -v.y)
		_:
			_key(KEY_W, v.y > 0.33)
			_key(KEY_S, v.y < -0.33)
			_key(KEY_D, v.x > 0.33)
			_key(KEY_A, v.x < -0.33)


# Walks to a horizontal point without turning the view, the way a player
# steps into place: the stick (or keys) pushed toward it relative to the
# view, easing off near it -- a key pulses -- and stopping inside `tolerance`.
func _walk_to(device: int, target: Vector2, tolerance: float, timeout: float = 6.0, require_supported_arrival: bool = false) -> bool:
	var waited := 0.0
	var frame := 0
	var physical := require_supported_arrival
	var accumulated := Vector2.ZERO
	while waited < timeout:
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		var owner := int(_native().get_support_entity_id())
		physical = physical or owner in [1600, 2921, 2952, 2954, 2980, 2981, 2983, 2984, 2988, 2992] or (owner >= 2560 and owner <= 2566) or (owner >= 1970 and owner <= 1972) or (owner >= 2970 and owner <= 2978)
		var slip: Vector3 = _velocity() - _native().get_support_point_linear_velocity()
		var settled := bool(_ctx()["grounded"]) and Vector2(slip.x, slip.z).length() < 0.15
		if to.length() <= tolerance and (not physical or settled):
			_move_dir(device, Vector2.ZERO)
			await _seconds(0.2)
			return true
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		var direction := Vector2(to.dot(right), to.dot(forward)).normalized()
		var amount := clampf(to.length() / 0.8, 0.25, 1.0)
		if physical:
			# Early braking through real device input; retain native inertia and
			# wait for actual supported, low-slip arrival. Invert the device
			# deadzone so small deliberate adjustments reach the input owner.
			# A loaded lip needs sustained input: proportional easing alone can
			# stall on its rounded contact. Bound accumulated effort near the
			# target, and brake actual support-relative velocity. All motion
			# still comes through the device; arrival checks remain unchanged.
			# Discard accumulated effort after crossing a target axis. Retaining
			# it steered past the service toe even while countersteering was
			# required. This is device-input control, never a physics override.
			if accumulated.x * to.x < 0.0:
				accumulated.x = 0.0
			if accumulated.y * to.y < 0.0:
				accumulated.y = 0.0
			if to.length() > 0.75:
				accumulated = Vector2.ZERO
			elif to.length() > tolerance:
				accumulated = (accumulated + to * get_process_delta_time() * 0.4).limit_length(0.35)
			var correction := to / 2.75 + accumulated - Vector2(slip.x, slip.z) * 0.12
			direction = Vector2(correction.dot(right), correction.dot(forward)).normalized()
			# A required supported arrival keeps countersteering while airborne
			# or slipping, even after crossing the horizontal target tolerance.
			amount = 0.0 if to.length() <= tolerance and not require_supported_arrival else minf(1.0, correction.length())
			if device != InputRouter.Device.KEYBOARD_MOUSE and amount > 0.0:
				var deadzone := TouchControls.STICK_DEADZONE if device == InputRouter.Device.TOUCH else InputRouter.STICK_DEADZONE
				amount = deadzone + (1.0 - deadzone) * amount
		var v := direction * amount
		if device == InputRouter.Device.KEYBOARD_MOUSE and to.length() < (2.75 if physical else 0.8):
			frame += 1
			var pressed := frame % 6 < roundi(6.0 * amount) if physical else frame % 6 < 2
			v = direction if pressed else Vector2.ZERO
		_move_dir(device, v)
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	return false


# Holds the left stick at `amount` of full throw toward canvas `direction`.
func _stick_hold(direction: Vector2, amount: float) -> void:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u) * clampf(amount, 0.0, 1.0)
	_touch(0, home, true)
	_drag(0, home + direction.normalized() * throw, direction.normalized() * throw)


# Samples both rendered wrists every frame until stopped, in the camera's
# frame -- where a player sees them -- and reports the largest single-frame
# move of either.
func _watch_wrists() -> Dictionary:
	var watch := {"running": true, "worst": 0.0}
	_sample_wrists(watch)
	return watch


func _sample_wrists(watch: Dictionary) -> void:
	var last: Array = []
	while bool(watch["running"]):
		var view: Transform3D = (_main._camera as Camera3D).global_transform.affine_inverse()
		var now: Array = _main._arms.wrist_positions().map(
			func(wrist: Vector3) -> Vector3: return view * wrist)
		if last.size() == now.size():
			for index in now.size():
				var step: float = ((now[index] as Vector3) - (last[index] as Vector3)).length()
				if step > float(watch["worst"]):
					watch["worst"] = step
					watch["at"] = "%s %s" % [str(_main._arms.hand_poses()), _action_label()]
		last = now
		await get_tree().process_frame


func _stop_watch(watch: Dictionary) -> float:
	watch["running"] = false
	return float(watch["worst"])


# The sound a player hears, measured where it leaves Master (after the
# limiter) and in the band a phone speaker reproduces, above 300 Hz. A
# headless run's Dummy driver never mixes, so this runs under Movie Maker
# (--write-movie), which mixes that same driver frame by frame. Standing
# still, the yard's ambience must be there; walking, the footsteps must
# stand out of it; nothing may clip.
func _audio_mix() -> bool:
	if AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty():
		return _fail("no mixing audio driver; run under --write-movie")
	var capture := AudioEffectCapture.new()
	capture.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Master"), capture)
	var ready: bool = await _wait_until(func() -> bool: return bool(_main._audio._bank_ready), 5.0)
	if not ready:
		return _fail("the sound bank never finished building")
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	# The ambience fades in from silence over its first seconds.
	await _capture(capture, 2.5)
	var standing := await _capture(capture, 1.0)
	var steps_before: int = _main._audio.steps
	_key(KEY_W, true)
	var walking := await _capture(capture, 1.4)
	_key(KEY_W, false)
	var steps: int = _main._audio.steps - steps_before
	var bed := _band_levels(standing)
	var walk := _band_levels(walking)
	var peak := 0.0
	for x in standing + walking:
		peak = maxf(peak, absf(x))
	var peak_db := linear_to_db(maxf(peak, 1.0e-9))
	_detail = "bed_db=%.1f steps_db=%.1f peak_db=%.1f steps=%d" % [bed.x, walk.y, peak_db, steps]
	_main._audio.quiesce()
	await _frames(3)
	if steps < 3:
		return _fail("walking 1.4 s played %d footsteps" % steps)
	if bed.x < -40.0:
		return _fail("standing still, the mix above 300 Hz sits at %.1f dBFS" % bed.x)
	if walk.y < bed.x + 4.0:
		return _fail("footsteps (%.1f dBFS) do not stand out of the ambience (%.1f)" % [walk.y, bed.x])
	if peak_db > -0.5:
		return _fail("the mix clips: sample peak %.2f dBFS" % peak_db)
	return true


# Mono samples leaving Master for `seconds` of game time.
func _capture(capture: AudioEffectCapture, seconds: float) -> PackedFloat32Array:
	var mono := PackedFloat32Array()
	var waited := 0.0
	while waited < seconds:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var available := capture.get_frames_available()
		if available > 0:
			for frame in capture.get_buffer(available):
				mono.append((frame.x + frame.y) * 0.5)
	return mono


# 50 ms RMS levels above 300 Hz (two cascaded one-pole high-passes), as
# (median, loudest) in dBFS.
func _band_levels(mono: PackedFloat32Array) -> Vector2:
	var rate := AudioServer.get_mix_rate()
	var a := exp(-TAU * 300.0 / rate)
	var x1 := 0.0
	var y1 := 0.0
	var x2 := 0.0
	var y2 := 0.0
	var window := int(rate * 0.05)
	var levels: Array[float] = []
	var energy := 0.0
	var count := 0
	for x in mono:
		var h1 := a * (y1 + x - x1)
		x1 = x
		y1 = h1
		var h2 := a * (y2 + h1 - x2)
		x2 = h1
		y2 = h2
		energy += h2 * h2
		count += 1
		if count == window:
			levels.append(linear_to_db(maxf(sqrt(energy / float(window)), 1.0e-9)))
			energy = 0.0
			count = 0
	if levels.is_empty():
		return Vector2(-200.0, -200.0)
	levels.sort()
	return Vector2(levels[levels.size() / 2], levels[levels.size() - 1])


# --- helpers -------------------------------------------------------------------


func _native() -> Object:
	return _main._native


func _ctx() -> Dictionary:
	return _main._ctx


func _position() -> Vector3:
	return _main._native.get_player_position()


func _velocity() -> Vector3:
	return _main._native.get_player_linear_velocity()


func _viewport_size() -> Vector2:
	return get_viewport().get_visible_rect().size


func _center(id: StringName) -> Vector2:
	return _main._touch.button_center(id)


func _frames(count: int) -> void:
	for i in count:
		await get_tree().process_frame


func _seconds(duration: float) -> void:
	var waited := 0.0
	while waited < duration:
		await get_tree().process_frame
		waited += get_process_delta_time()


func _wait_until(condition: Callable, timeout: float) -> bool:
	var waited := 0.0
	while not bool(condition.call()):
		if waited >= timeout:
			return false
		await get_tree().process_frame
		waited += get_process_delta_time()
	return true


func _push(event: InputEvent) -> void:
	get_viewport().push_input(event, true)


func _touch(index: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	_push(event)


func _drag(index: int, at: Vector2, relative: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = at
	event.relative = relative
	_push(event)


func _tap(index: int, at: Vector2) -> void:
	_touch(index, at, true)
	_touch(index, at, false)


# Holds the left stick deflected in canvas direction `direction` at full throw.
func _stick_push(direction: Vector2) -> void:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u)
	_touch(0, home, true)
	for i in 3:
		var at := home + direction.normalized() * throw * float(i + 1) / 3.0
		_drag(0, at, direction.normalized() * throw / 3.0)


# A touch's GUI twin: emulated mouse events carry DEVICE_ID_EMULATION.
func _click(at: Vector2) -> void:
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.device = InputEvent.DEVICE_ID_EMULATION
		event.button_index = MOUSE_BUTTON_LEFT
		event.position = at
		event.global_position = at
		event.pressed = pressed
		_push(event)


func _key(code: Key, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = pressed
	_push(event)


func _button(button: JoyButton, pressed: Variant = null) -> void:
	var states: Array = [true, false] if pressed == null else [pressed]
	for state in states:
		var event := InputEventJoypadButton.new()
		event.device = 0
		event.button_index = button
		event.pressed = state
		_push(event)


func _axis(axis: JoyAxis, value: float) -> void:
	var event := InputEventJoypadMotion.new()
	event.device = 0
	event.axis = axis
	event.axis_value = value
	_push(event)


# Screenshots need a real renderer; headless proof runs skip them silently.
func _pose(pose: String) -> void:
	print("SCRAPERX_UITEST_POSE %s tick=%d position=%s elapsed_ms=%d" % [
		pose, _native().get_tick_index(), _position(), Time.get_ticks_msec()])
	if _capture_prefix.is_empty() or DisplayServer.get_name() == "headless":
		return
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	var path := "%s_%s.png" % [_capture_prefix, pose]
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var image := get_viewport().get_texture().get_image()
	if image.save_png(path) == OK:
		print("SCRAPERX_UITEST_SCREENSHOT %s" % path)
	else:
		push_error("SCRAPERX_UITEST_SCREENSHOT_FAILED %s" % path)


func _reel(device: int, held: bool) -> void:
	if device == InputRouter.Device.TOUCH:
		_touch(0, _center(&"slingshot_reel"), held)
	else:
		_move(device, -1.0 if held else 0.0)
