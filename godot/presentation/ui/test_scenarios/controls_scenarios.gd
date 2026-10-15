extends RefCounted

# Ground foundation, touch/gamepad/keyboard controls, regression fixtures and rigging.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")

var h: Harness


func _init(harness: Harness) -> void:
	h = harness


func _ground_foundation() -> bool:
	if h._main._regression_scene:
		return h._fail("production proof selected a regression scene")
	for entity in range(3, 60):
		if entity not in [11, 51] and int(h._native().get_entity_body_count(entity)) != 0:
			return h._fail("retired native body %d remains" % entity)
	# Normal-world inventory: original/service lift, six supplied machines,
	# reclaim wheel/material, slab-haul cart, two static taper bodies, west
	# brace, inspection junction/plank, north transfer and pipe service.
	# Fixture counts are separate.
	var moving_count := int(h._native().get_moving_body_count())
	var kit_count := int(h._native().get_kit_body_count())
	var cable_count := int(h._native().get_kit_cable_count())
	if moving_count != 27 + 9 + 17 + 103 + 8 + 1 + 12 + 2 or kit_count != 38 + 12 + 39 + 109 + 14 + 2 + 1 + 4 + 12 + 1 + 1 + 2 or cable_count != 3 + 3 + 1:
		return h._fail("default inventory differs: moving=%d kit=%d cables=%d" % [moving_count, kit_count, cable_count])
	# Reject compensating omissions/duplicates in the appended assemblies.
	# Native route headers own these identifiers; 1962 is a logical plank
	# member, while 1937 is its actual static seat.
	for entity in [1934, 1935, 1936, 1937, 2935, 1938, 1939, 1963, 1964, 2963] + range(2880, 2892):
		if int(h._native().get_entity_body_count(entity)) != 1:
			return h._fail("appended native assembly body %d differs" % entity)
	# Check actual scene nodes, independently of native enumeration. This also
	# catches visual-only remnants that would not appear in the physics world.
	var retired := ["IntakeBay", "WaterScrew", "LegalForty", "Hook5",
		"StackGear", "GearMotif", "Crane", "GroundWater", "WellA", "WellB"]
	var rejected_fallback_parts := {"StairFlight": true, "StairTread": true, "StairStringer": true}
	for node in h._main.get_node("TowerPresentation").find_children("*", "", true, false):
		for prefix in retired:
			if String(node.name).begins_with(prefix):
				return h._fail("retired scene node remains: " + String(node.name))
		if node.has_meta(&"part") and rejected_fallback_parts.has(String(node.get_meta(&"part"))):
			return h._fail("rejected fallback scene part remains: " + String(node.get_meta(&"part")))
	if not await h._wait_until(func() -> bool: return bool(h._native().is_player_grounded()), 2.0):
		return h._fail("default player did not settle at grade")
	await h._pose("cleared_grade")
	if not await _keyboard_core():
		return false
	# Look through the old intake/screw area with normal walking and turning.
	await h._face(Vector2(-1.0, -1.0))
	await h._pose("cleared_tower")
	h._detail = "retired_bodies=0 moving_bodies=%d kit_bodies=%d cables=%d retired_meshes=0 rejected_fallback_meshes=0 default_controls=1" % [moving_count, kit_count, cable_count]
	return true


func _touch_jump() -> bool:
	var settled: bool = await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	if not settled:
		return h._fail("never grounded at spawn")
	h._tap(0, h._center(&"jump"))
	var peak := [0.0]
	var airborne_and_rising := func() -> bool:
		peak[0] = maxf(peak[0], h._velocity().y)
		return not bool(h._native().is_player_grounded()) and peak[0] > 2.0
	var lifted: bool = await h._wait_until(airborne_and_rising, 0.4)
	if not lifted:
		return h._fail("touch jump did not leave the ground (peak vy %.2f)" % peak[0])
	await h._pose("jump")
	if h._main._router.device != InputRouter.Device.TOUCH:
		return h._fail("touch press did not switch the interface to touch")
	if h._main._audio.jumps < 1:
		return h._fail("the takeoff played no jump cue")
	h._detail = "vy_peak=%.2f" % peak[0]
	return true


func _touch_move_look() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	var start := h._position()
	var yaw_start: float = h._main._yaw
	var forward := Vector2(-sin(yaw_start), -cos(yaw_start))
	h._stick_push(Vector2(0.0, -1.0))
	await h._seconds(0.6)
	await h._pose("running")
	# Second thumb looks while the first keeps walking: 200 canvas units at
	# the default 0.003 rad/unit must turn exactly 0.6 rad.
	var look_at := Vector2(h._viewport_size().x * 0.78, h._viewport_size().y * 0.45)
	h._touch(1, look_at, true)
	for i in 4:
		h._drag(1, look_at + Vector2(50.0 * (i + 1), 0.0), Vector2(50.0, 0.0))
		await h._frames(1)
	h._touch(1, look_at + Vector2(200.0, 0.0), false)
	await h._seconds(0.4)
	var moved := h._position() - start
	var along := Vector2(moved.x, moved.z).dot(forward)
	h._touch(0, h._main._touch.stick_home(), false)
	await h._seconds(0.6)
	var turned: float = yaw_start - h._main._yaw
	if along < 2.5:
		return h._fail("stick moved the player only %.2f m forward" % along)
	if absf(turned - 0.6) > 0.02:
		return h._fail("look drag turned %.3f rad, expected 0.600" % turned)
	var speed := Vector2(h._velocity().x, h._velocity().z).length()
	if speed > 0.6:
		return h._fail("player still moving %.2f m/s after the stick was released" % speed)
	# 4.9 m of walking at a 0.69 m stride is several footfalls, each a cue.
	if h._main._audio.steps < 4:
		return h._fail("walking %.2f m played %d footsteps" % [along, h._main._audio.steps])
	# Push beyond the touch ring to sprint; releasing it must clear the latch.
	var home: Vector2 = h._main._touch.stick_home()
	var overshoot := TouchControls.STICK_THROW * float(h._main._touch._u) * 1.35
	var sprint_state := func() -> String:
		return "tick=%d sprint=%s speed_mps=%.3f grounded=%s support=%d move=%s latch=%s position=%s" % [
			int(h._native().get_tick_index()), bool(h._native().is_player_sprinting()),
			Vector2(h._velocity().x, h._velocity().z).length(), bool(h._native().is_player_grounded()),
			int(h._native().get_support_entity_id()), h._main._touch.move_vector,
			h._main._touch.sprint_latched, h._position()]
	var sprint_start_tick := int(h._native().get_tick_index())
	h._touch(0, home, true)
	h._drag(0, home + Vector2(0, -overshoot), Vector2(0, -overshoot))
	if not await h._wait_until(func() -> bool: return bool(h._native().is_player_sprinting()), 0.1):
		return h._fail("touch overshoot sprint input was not recognized promptly %s" % sprint_state.call())
	var sprint_recognized_tick := int(h._native().get_tick_index())
	print("SCRAPERX_UITEST_SPRINT recognized %s" % sprint_state.call())
	# On flat ground, finite traction permits at most 0.85*g acceleration;
	# the 3 kW work cap further slows the ramp above about 4.2 m/s. Reaching
	# 7.9 m/s takes about 1.2 s from rest, so bound the state wait at 1.5 s.
	if not await h._wait_until(func() -> bool:
		return bool(h._native().is_player_sprinting()) and Vector2(h._velocity().x, h._velocity().z).length() >= 7.9, 1.5):
		return h._fail("touch overshoot did not produce native sprint %s" % sprint_state.call())
	var sprint_full_tick := int(h._native().get_tick_index())
	print("SCRAPERX_UITEST_SPRINT full %s" % sprint_state.call())
	h._touch(0, home, false)
	if not await h._wait_until(func() -> bool: return not bool(h._native().is_player_sprinting()), 0.1):
		return h._fail("released sprint input was not cleared promptly %s" % sprint_state.call())
	var sprint_released_tick := int(h._native().get_tick_index())
	print("SCRAPERX_UITEST_SPRINT released %s" % sprint_state.call())
	# Braking from 8 m/s at 0.85*g needs about 0.96 s; release must clear
	# the input promptly while finite native traction settles within 1.2 s.
	if not await h._wait_until(func() -> bool:
		return not bool(h._native().is_player_sprinting()) and Vector2(h._velocity().x, h._velocity().z).length() <= 0.6, 1.2):
		return h._fail("released sprint left movement latched %s" % sprint_state.call())
	print("SCRAPERX_UITEST_SPRINT settled %s" % sprint_state.call())
	h._detail = "forward_m=%.2f turned_rad=%.3f settle_mps=%.2f steps=%d sprint_input_ticks=%d sprint_accel_ticks=%d sprint_release_ticks=%d sprint_stop_ticks=%d" % [
		along, turned, speed, h._main._audio.steps, sprint_recognized_tick - sprint_start_tick,
		sprint_full_tick - sprint_start_tick, sprint_released_tick - sprint_full_tick,
		int(h._native().get_tick_index()) - sprint_full_tick]
	return true


# Gyro aim is inert until the player turns it on; then the device's own
# rotation, injected where the platform writes it, turns the view by rate x
# time, and the native walk that follows heads where the view now faces.
func _touch_gyro_aim() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	if not bool(ProjectSettings.get_setting("input_devices/sensors/enable_gyroscope", false)):
		return h._fail("project.godot does not enable the Android gyroscope sensor")
	var yaw_off: float = h._main._yaw
	Input.set_gyroscope(Vector3(0.0, -1.0, 0.0))
	await h._frames(30)
	Input.set_gyroscope(Vector3.ZERO)
	if absf(h._main._yaw - yaw_off) > 1.0e-6:
		return h._fail("the device turned the view with GYRO AIM off")
	h._main._settings.gyro_aim = true
	h._main._apply_settings()
	# 30 frames of a fixed 60 fps clock at 1 rad/s right and 0.4 rad/s up.
	var yaw_start: float = h._main._yaw
	var pitch_start: float = h._main._pitch
	Input.set_gyroscope(Vector3(0.4, -1.0, 0.0))
	await h._frames(30)
	Input.set_gyroscope(Vector3.ZERO)
	await h._frames(2)
	var turned: float = yaw_start - h._main._yaw
	var raised: float = h._main._pitch - pitch_start
	if absf(turned - 0.5) > 0.02:
		return h._fail("turning the device right turned the view %.3f rad, expected 0.500" % turned)
	if absf(raised - 0.2) > 0.02:
		return h._fail("tipping the device up raised the view %.3f rad, expected 0.200" % raised)
	await h._pose("gyro_turned")
	var yaw_before_bad: float = h._main._yaw
	Input.set_gyroscope(Vector3(NAN, INF, 0.0))
	await h._frames(3)
	Input.set_gyroscope(Vector3.ZERO)
	if not is_equal_approx(h._main._yaw, yaw_before_bad) or not is_finite(h._main._pitch):
		return h._fail("a non-finite gyro sample moved or corrupted the view")
	var start := h._position()
	var forward := Vector2(-sin(h._main._yaw), -cos(h._main._yaw))
	h._stick_push(Vector2(0.0, -1.0))
	await h._seconds(0.6)
	h._touch(0, h._main._touch.stick_home(), false)
	var moved := h._position() - start
	var heading := Vector2(moved.x, moved.z)
	if heading.length() < 1.0:
		return h._fail("stick moved the player only %.2f m" % heading.length())
	var alignment := heading.normalized().dot(forward)
	if alignment < 0.98:
		return h._fail("the walk heads %.3f off the gyro-turned view" % acos(clampf(alignment, -1.0, 1.0)))
	h._detail = "turned_rad=%.3f raised_rad=%.3f walk_alignment=%.4f" % [turned, raised, alignment]
	return true


func _touch_climb(expected: StringName, expected_state: int) -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return h._ctx()["action"]["id"] == expected, 2.0)
	if not reached:
		return h._fail("ACTION never offered %s (got %s)" % [expected, h._ctx()["action"]["id"]])
	if h._main._touch.button_label(&"action") != "CLIMB":
		return h._fail("ACTION button label was '%s'" % h._main._touch.button_label(&"action"))
	await h._pose("climb_prompt")
	var start_y := h._position().y
	h._tap(0, h._center(&"action"))
	var reached_2: bool = await h._wait_until(func() -> bool: return int(h._native().get_traversal_state()) == expected_state, 0.2)
	if not reached_2:
		return h._fail("ACTION did not commit traversal state %d" % expected_state)
	# The native step-in closes on the wall before the body rises, so both
	# palms land on the lip, not on air an arm's length short of it.
	var both_planted: bool = await h._wait_until(func() -> bool: return h._main._arms.planted_count() == 2, 0.6)
	if not both_planted:
		return h._fail("hands never planted on the lip during the standing mantle")
	await h._pose("ground_mantle")
	var completed_grounded := func() -> bool:
		return int(h._native().get_traversal_state()) == 0 and bool(h._native().is_player_grounded())
	var completed: bool = await h._wait_until(completed_grounded, 2.5)
	if not completed:
		return h._fail("traversal never completed grounded")
	var rise := h._position().y - start_y
	h._detail = "state=%d rise_m=%.2f" % [expected_state, rise]
	return rise > 0.3


func _touch_vault() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	h._stick_push(Vector2(0.0, -1.0))
	# The rail is offered once the approach has real momentum; tap the moment
	# ACTION says CLIMB, exactly like a thumb would.
	var reached: bool = await h._wait_until(func() -> bool: return h._ctx()["action"]["id"] == &"climb", 2.5)
	if not reached:
		return h._fail("ACTION never offered the vault rail")
	h._tap(1, h._center(&"action"))
	var reached_2: bool = await h._wait_until(func() -> bool: return int(h._native().get_traversal_state()) == 3, 0.2)
	if not reached_2:
		return h._fail("ACTION did not commit a vault (state %d)" % int(h._native().get_traversal_state()))
	await h._pose("vault")
	h._touch(0, h._main._touch.stick_home(), false)
	var reached_3: bool = await h._wait_until(func() -> bool: return int(h._native().get_accepted_traversal_count()) >= 1, 1.5)
	if not reached_3:
		return h._fail("vault never completed")
	h._detail = "x_after=%.2f" % h._position().x
	return h._position().x > 5.6


# The same rail, vaulted with JUMP twice and ACTION never touched.
func _touch_double_tap_vault() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	h._stick_push(Vector2(0.0, -1.0))
	var reached: bool = await h._wait_until(func() -> bool: return h._ctx()["action"]["id"] == &"climb", 2.5)
	if not reached:
		return h._fail("the approach never reached the vault rail")
	h._tap(1, h._center(&"jump"))
	var lifted: bool = await h._wait_until(
		func() -> bool: return not bool(h._native().is_player_grounded()), 0.3)
	if not lifted:
		return h._fail("the first JUMP did not leave the ground")
	await h._seconds(0.08)
	h._tap(1, h._center(&"jump"))
	# Committed in the air, from the tap itself -- not by a buffered press
	# that waited for the feet to find the rail top.
	var landed := [false]
	var vaulting: bool = await h._wait_until(func() -> bool:
		if int(h._native().get_traversal_state()) == 3:
			return true
		landed[0] = landed[0] or bool(h._native().is_player_grounded())
		return false, 0.25)
	if not vaulting or landed[0]:
		return h._fail("the second JUMP did not commit a vault in the air (state %d, landed %s)" % [
			int(h._native().get_traversal_state()), str(landed[0])])
	h._touch(0, h._main._touch.stick_home(), false)
	var done: bool = await h._wait_until(func() -> bool: return int(h._native().get_accepted_traversal_count()) >= 1, 1.5)
	if not done:
		return h._fail("double-tap vault never completed")
	h._detail = "x_after=%.2f" % h._position().x
	return h._position().x > 5.6


# The crawl beam beside the dock has its underside 1.45 m up. Standing, the
# walk stops at its face; CROUCH crouches the native body and the same walk
# passes under; STAND raises it again in the open. The eye follows the body.
func _touch_crouch() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	if not h._main._touch.is_button_shown(&"crouch") or h._main._touch.button_label(&"crouch") != "CROUCH":
		return h._fail("CROUCH was not offered standing on the deck")
	h._stick_push(Vector2(0.0, -1.0))
	await h._seconds(1.6)
	var blocked_z := h._position().z
	if blocked_z < -12.7:
		return h._fail("a standing body walked in under the 1.45 m beam (z %.2f)" % blocked_z)
	if bool(h._native().is_player_crouched()):
		return h._fail("the body crouched before CROUCH was pressed")
	await h._pose("beam_blocks")
	h._tap(1, h._center(&"crouch"))
	var crouched: bool = await h._wait_until(
		func() -> bool: return bool(h._native().is_player_crouched()), 0.2)
	if not crouched:
		return h._fail("CROUCH did not crouch the native body")
	var passed: bool = await h._wait_until(func() -> bool: return h._position().z < -13.9, 3.0)
	if not passed:
		return h._fail("the crouched walk did not pass under the beam (z %.2f)" % h._position().z)
	if not bool(h._native().is_player_crouched()):
		return h._fail("the body stood up while CROUCH was on")
	h._touch(0, h._main._touch.stick_home(), false)
	await h._seconds(0.4)
	var passed_z := h._position().z
	var eye_crouched := _eye_over_soles()
	if h._main._touch.button_label(&"crouch") != "STAND":
		return h._fail("the crouch button did not read STAND while crouched")
	await h._pose("crouched")
	h._tap(1, h._center(&"crouch"))
	var stood: bool = await h._wait_until(
		func() -> bool: return not bool(h._native().is_player_crouched()), 0.2)
	if not stood:
		return h._fail("STAND did not stand the native body in the open")
	await h._seconds(0.4)
	var eye_standing := _eye_over_soles()
	if absf(eye_crouched - 0.95) > 0.03 or absf(eye_standing - 1.52) > 0.03:
		return h._fail("eye over the soles %.2f crouched / %.2f standing, expected 0.95 / 1.52" % [
			eye_crouched, eye_standing])
	h._detail = "blocked_z=%.2f passed_z=%.2f eye_crouched=%.2f eye_standing=%.2f" % [
		blocked_z, passed_z, eye_crouched, eye_standing]
	return true


# AS-003 from the cage floor, all through the touch pipeline: facing the bar
# across the inside of the door, Action reads PICK UP and takes it; backed off
# with it, SET DOWN leaves the door to swing open on its own drive; facing the
# rack, PICK UP takes the hook block with both hands shown on it; and it is
# carried out through the doorway and set down on the apron.
func _touch_carry() -> bool:
	var carry_state := func() -> String:
		return "tick=%d at=%s velocity=%s grounded=%s support=%d traversal=%d held=%d target=%d kind=%d move=%s deaths=%d" % [
			int(h._native().get_tick_index()), h._position(), h._velocity(), bool(h._native().is_player_grounded()),
			int(h._native().get_support_entity_id()), int(h._native().get_traversal_state()),
			int(h._native().get_carrying_entity_id()), int(h._native().get_carry_target_entity_id()),
			int(h._native().get_carry_target_kind()), h._main._touch.move_vector, int(h._native().get_death_count())]
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	h._main._yaw = 0.0  # south, at the bar
	h._stick_push(Vector2(0.0, -1.0))
	var at_bar: bool = await h._wait_until(func() -> bool: return h._position().z <= -86.75, 2.0)
	h._touch(0, h._main._touch.stick_home(), false)
	if not at_bar:
		return h._fail("never reached the bar (z %.2f)" % h._position().z)
	await h._seconds(0.4)
	if h._main._touch.button_label(&"action") != "PICK UP":
		return h._fail("Action read '%s' at the bar, not PICK UP" % h._main._touch.button_label(&"action"))
	h._tap(1, h._center(&"action"))
	var holding_bar: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 55, 0.3)
	if not holding_bar:
		return h._fail("PICK UP did not put the bar on the native carry point")
	h._stick_push(Vector2(0.0, 1.0))  # back away north, the bar still ahead
	await h._wait_until(func() -> bool: return h._position().z >= -85.7, 2.0)
	h._touch(0, h._main._touch.stick_home(), false)
	await h._seconds(0.3)
	if h._main._touch.button_label(&"action") != "SET DOWN":
		return h._fail("Action read '%s' holding the bar, not SET DOWN" % h._main._touch.button_label(&"action"))
	h._tap(1, h._center(&"action"))
	var bar_down: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 0, 0.3)
	if not bar_down:
		return h._fail("SET DOWN did not take the bar off the carry point")
	var opened: bool = await h._wait_until(
		func() -> bool: return float(h._native().get_hook5_door_angle_radians()) >= 1.2, 5.0)
	if not opened:
		return h._fail("the door never travelled once the bar was set down (%.2f rad)" %
			float(h._native().get_hook5_door_angle_radians()))
	await h._pose("door_open")
	# The north-Z guard was already true at x=10.42: facing east there offered
	# the nearer dropped bar55. Step east until its centre is behind us. The
	# pedestal's west face x=10.95 and capsule radius0.35 bound this stance
	# at x=10.60. Settle clear of the pedestal, past the dropped bar's centre,
	# then face east and require the actual block offer before touching Action.
	if not await h._walk_to(InputRouter.Device.TOUCH, Vector2(10.57, -84.65), 0.01, 4.0, true):
		return h._fail("hook block settled approach failed %s" % carry_state.call())
	h._main._yaw = -PI * 0.5  # east, at the block
	await h._seconds(0.4)
	if not bool(h._native().is_player_grounded()) or int(h._native().get_traversal_state()) != 0 \
			or int(h._native().get_carrying_entity_id()) != 0 or int(h._native().get_carry_target_entity_id()) != 56 \
			or h._main._touch.button_label(&"action") != "PICK UP":
		return h._fail("rack did not offer actual freehands target56 PICK UP label=%s %s" % [
			h._main._touch.button_label(&"action"), carry_state.call()])
	print("SCRAPERX_UITEST_CARRY rack_before_action %s" % carry_state.call())
	h._tap(1, h._center(&"action"))
	var holding_block: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 56, 0.3)
	if not holding_block:
		return h._fail("PICK UP did not put the hook block on the native carry point %s" % carry_state.call())
	print("SCRAPERX_UITEST_CARRY holding_block %s" % carry_state.call())
	await h._seconds(0.5)
	if h._main._arms.hand_poses() != [8, 8]:
		return h._fail("hands not on the block (poses %s)" % str(h._main._arms.hand_poses()))
	await h._pose("holding_block")
	# Out through the doorway, east of the travelled leaf.
	for leg in [Vector2(10.7, -86.8), Vector2(10.7, -89.6)]:
		var at: Vector3 = h._position()
		h._main._yaw = atan2(-(leg.x - at.x), -(leg.y - at.z))
		h._stick_push(Vector2(0.0, -1.0))
		var arrived: bool = await h._wait_until(func() -> bool: return h._position().z <= leg.y + 0.1, 3.0)
		h._touch(0, h._main._touch.stick_home(), false)
		if not arrived:
			return h._fail("the carry out stalled at (%.2f, %.2f)" % [h._position().x, h._position().z])
	await h._seconds(0.4)
	if int(h._native().get_carrying_entity_id()) != 56 or bool(h._native().is_hook_in_rack()):
		return h._fail("the block was not held off its rack outside the cage")
	await h._pose("carried_out")
	h._tap(1, h._center(&"action"))
	var set_down: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 0, 0.3)
	if not set_down:
		return h._fail("SET DOWN did not release the block outside")
	await h._seconds(1.5)
	var block: Vector3 = h._native().get_hook5_block_position()
	# Back off south-east and look at the cage the way it is approached.
	h._main._yaw = atan2(-(10.3 - 13.5), -(-86.0 - (-94.0)))
	h._stick_push(Vector2(0.0, 1.0))
	await h._seconds(1.1)
	h._touch(0, h._main._touch.stick_home(), false)
	h._main._pitch = 0.12
	await h._seconds(0.6)
	await h._pose("cage_exterior")
	h._detail = "door_rad=%.2f block=(%.2f,%.2f,%.2f)" % [
		float(h._native().get_hook5_door_angle_radians()), block.x, block.y, block.z]
	print("SCRAPERX_UITEST_CARRY complete %s hands=%s block=%s" % [
		carry_state.call(), h._main._arms.hand_poses(), block])
	return block.y < 0.4 and block.z < -88.0


func _eye_over_soles() -> float:
	var half := 0.6 if bool(h._native().is_player_crouched()) else 0.9
	return h._main._camera.position.y - (h._position().y - half)


func _touch_hang(climb: bool) -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return bool(h._ctx()["hanging"]), 3.0)
	if not reached:
		return h._fail("pushing into the wall never produced a hang")
	h._touch(0, h._main._touch.stick_home(), false)
	await h._seconds(0.3)
	if not h._main._touch.is_button_shown(&"drop"):
		return h._fail("DROP was not offered while hanging")
	if h._main._touch.button_label(&"jump") != "CLIMB UP":
		return h._fail("JUMP did not relabel to CLIMB UP while hanging")
	# Both hands on the native ledge, not near it: the rendered wrists sit on
	# the grip points the native ledge point implies.
	var grip_error: float = h._main._arms.anchored_error()
	if h._main._arms.hand_poses() != [4, 4] or grip_error < 0.0 or grip_error > 0.02:
		return h._fail("hands not gripping the native ledge (poses %s, error %.3f m)" % [
			str(h._main._arms.hand_poses()), grip_error])
	# Native per-hand grips, not a reconstructed central lip, own the pose.
	var normal: Vector3 = h._native().get_traversal_normal()
	for hand in h._main._arms._hands:
		var hold: Vector3 = h._native().get_traversal_left_hand_render_position() if hand.side < 0.0 \
			else h._native().get_traversal_right_hand_render_position()
		var expected: Vector3 = hold - normal * (h._main._arms.TOP_PROBE_INSET + 0.034) + Vector3.DOWN * 0.074
		if hand.anchor.distance_to(expected) > 0.002:
			return h._fail("hanging wrist target does not follow its native hand")
	# Looking around moves the head, not the loaded torso or planted wrists.
	var old_yaw: float = h._main._yaw
	var old_pitch: float = h._main._pitch
	h._main._yaw += PI * 0.5
	h._main._pitch = 0.2
	await h._seconds(0.1)
	var head_grip_error: float = h._main._arms.anchored_error()
	var hand_span: Vector3 = h._native().get_traversal_right_hand_render_position() - h._native().get_traversal_left_hand_render_position()
	var shoulder_shift := 0.0
	var shoulders: Array[Vector3] = [Vector3.ZERO, Vector3.ZERO]
	for hand in h._main._arms._hands:
		var shoulder: Vector3 = 2.0 * hand.upper.global_position - hand.elbow.global_position
		var base: Vector3 = h._native().get_player_render_position() + Vector3.UP * h._main._arms.HANG_SHOULDER_HEIGHT \
			+ hand_span.normalized() * (h._main._arms.HANG_SHOULDER_HALF_SPAN * hand.side) \
			- normal * h._main._arms.HANG_SHOULDER_BACK
		shoulder_shift = maxf(shoulder_shift, shoulder.distance_to(base))
		shoulders[0 if hand.side < 0.0 else 1] = shoulder
		var forearm_length: float = hand.elbow.global_position.distance_to(hand.root.global_position)
		if absf(forearm_length - h._main._arms.FOREARM) > 0.002:
			return h._fail("loaded hang stretched a forearm: %.4f m" % forearm_length)
	if int(h._native().get_traversal_state()) != 1 or h._main._arms.hand_poses() != [4, 4] \
			or head_grip_error < 0.0 or head_grip_error > 0.02:
		return h._fail("loaded hanging wrist left its native grip after head turn: %.4f m" % head_grip_error)
	# A planted wrist alone can hide crossed shoulders. Each shoulder must
	# remain on its own side of the actual native hand pair.
	if (shoulders[1] - shoulders[0]).dot(hand_span) <= 0.0:
		return h._fail("hanging shoulders cross their native hand assignment")
	print("SCRAPERX_HANG_RIG shoulder_shift_m=%.4f shoulder_span_m=%.4f wrist_error_m=%.4f" % [
		shoulder_shift, shoulders[0].distance_to(shoulders[1]), head_grip_error])
	h._main._yaw = old_yaw
	h._main._pitch = old_pitch
	await h._frames(2)
	await h._pose("hanging")
	if climb:
		h._tap(1, h._center(&"jump"))
		var reached_2: bool = await h._wait_until(func() -> bool: return int(h._native().get_traversal_state()) == 2, 0.2)
		if not reached_2:
			return h._fail("CLIMB UP did not commit the mantle")
		var both_planted: bool = await h._wait_until(func() -> bool: return h._main._arms.planted_count() == 2, 0.3)
		if not both_planted:
			return h._fail("hands never planted on the ledge during the mantle")
		await h._pose("mantle_mid")
		var reached_3: bool = await h._wait_until(func() -> bool: return bool(h._native().is_player_grounded()), 2.5)
		if not reached_3:
			return h._fail("mantle from hang never landed")
		h._detail = "top_y=%.2f grip_error_m=%.4f head_grip_error_m=%.4f" % [h._position().y, grip_error, head_grip_error]
		return h._position().y > 4.4
	# Passive hanging carries momentum. Release on the downward phase so
	# the retained immediate falling assertion tests Drop without demanding
	# an unphysical velocity reset from the native release owner.
	if not await h._wait_until(func() -> bool: return bool(h._ctx()["hanging"]) and h._velocity().y < -0.1, 2.0):
		return h._fail("hang did not reach a downward phase for DROP")
	h._tap(1, h._center(&"drop"))
	var reached_4: bool = await h._wait_until(func() -> bool: return int(h._native().get_traversal_state()) == 0, 0.2)
	if not reached_4:
		return h._fail("DROP did not release the hang")
	await h._frames(2)
	h._detail = "vy_after_release=%.2f grip_error_m=%.4f head_grip_error_m=%.4f" % [h._velocity().y, grip_error, head_grip_error]
	return h._velocity().y <= 0.0


func _touch_chute() -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return h._main._touch.is_button_shown(&"chute"), 3.0)
	if not reached:
		return h._fail("CHUTE never offered in a real free fall")
	var offered_at := -h._velocity().y
	await h._frames(3)
	h._tap(0, h._center(&"chute"))
	var reached_2: bool = await h._wait_until(func() -> bool: return bool(h._native().is_parachute_deployed()), 0.2)
	if not reached_2:
		return h._fail("CHUTE did not deploy the canopy")
	await h._seconds(0.6)
	if not h._main._arms.risers_visible() or h._main._arms.hand_poses() != [6, 6]:
		return h._fail("hands are not on the canopy toggles (poses %s)" % str(h._main._arms.hand_poses()))
	await h._pose("canopy")
	var reached_3: bool = await h._wait_until(func() -> bool: return bool(h._native().is_player_grounded()), 15.0)
	if not reached_3:
		return h._fail("canopy descent never landed")
	await h._frames(2)
	if int(h._native().get_death_count()) != 0:
		return h._fail("landed dead at %.1f m/s" % float(h._native().get_last_impact_speed_mps()))
	h._detail = "offered_at_mps=%.2f impact_mps=%.2f" % [offered_at,
		float(h._native().get_last_impact_speed_mps())]
	return true


func _touch_lethal_feedback() -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return float(h._ctx()["danger"]) >= 0.8, 5.0)
	if not reached:
		return h._fail("fall danger never reached 0.8 of the native lethal speed")
	await h._pose("fall_danger")
	var reached_2: bool = await h._wait_until(func() -> bool: return int(h._native().get_death_count()) >= 1, 6.0)
	if not reached_2:
		return h._fail("unmitigated fall was not lethal")
	await h._frames(3)
	var toasts: Array = h._main._hud._toasts
	if toasts.is_empty() or String(toasts[-1]["title"]) != "LETHAL IMPACT":
		return h._fail("restore was not announced")
	await h._seconds(0.3)
	await h._pose("lethal_toast")
	var sub := String(toasts[-1]["sub"])
	var fell_at := sub.get_slice(" ", 2).to_float()
	var lethal := float(h._native().get_lethal_impact_speed_mps())
	h._detail = "toast='%s' lethal_mps=%.1f" % [sub, lethal]
	# The announced speed must be the lethal fall it reports, not a settle.
	return fell_at > lethal


func _touch_sump() -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return h._ctx()["action"]["id"] == &"valve", 2.0)
	if not reached:
		return h._fail("ACTION never offered the sump valve")
	var before := bool(h._native().is_sump_isolated())
	h._tap(0, h._center(&"action"))
	var reached_2: bool = await h._wait_until(func() -> bool: return bool(h._native().is_sump_isolated()) != before, 0.2)
	if not reached_2:
		return h._fail("ACTION did not toggle the native sump valve")
	h._detail = "isolated %s->%s" % [before, not before]
	return true



func _touch_water_screw() -> bool:
	var reached: bool = await h._wait_until(
		func() -> bool: return h._ctx()["action"]["id"] == &"screw_toggle", 2.0)
	if not reached:
		return h._fail("ACTION never offered the ground screw control")
	if bool(h._native().is_water_screw_motor_enabled()):
		return h._fail("ground screw must start stopped")
	h._tap(0, h._center(&"action"))
	var started: bool = await h._wait_until(
		func() -> bool: return bool(h._native().is_water_screw_motor_enabled()), 0.3)
	if not started:
		return h._fail("ACTION did not start the authoritative screw motor")
	var turning: bool = await h._wait_until(
		func() -> bool: return absf(float(h._native().get_water_screw_rpm())) > 2.0, 1.5)
	if not turning:
		return h._fail("started screw did not produce native shaft rotation")
	await h._pose("water_screw_running")
	h._detail = "rpm=%.1f torque_nm=%.0f tank_m3=%.3f" % [
		float(h._native().get_water_screw_rpm()),
		float(h._native().get_water_screw_motor_torque_nm()),
		float(h._native().get_water_screw_tank_volume_m3())]
	return true


func _touch_water_lift() -> bool:
	var reached: bool = await h._wait_until(
		func() -> bool: return h._ctx()["action"]["id"] == &"water_lift_valve", 2.0)
	if not reached:
		return h._fail("ACTION never offered the water-lift fill valve")
	if bool(h._native().is_water_lift_valve_open()):
		return h._fail("water-lift fill valve must start shut")
	h._tap(0, h._center(&"action"))
	var opened: bool = await h._wait_until(
		func() -> bool: return bool(h._native().is_water_lift_valve_open()), 0.3)
	if not opened:
		return h._fail("ACTION did not open the authoritative water-lift valve")
	await h._pose("water_lift_fill_control")
	h._detail = "valve_open=1 bucket_m3=%.3f cage_m=%.3f" % [
		float(h._native().get_water_lift_bucket_water_m3()),
		float(h._native().get_water_lift_cage_travel_m())]
	return true


func _touch_pendant() -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return h._ctx()["action"]["id"] == &"operate", 2.0)
	if not reached:
		return h._fail("ACTION never offered OPERATE at the yard jib pendant")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"intake" or not h._main._touch.is_button_shown(&"pendant_left"):
		return h._fail("OPERATE did not open the pendant controls")
	if not h._main._arms.remote_visible():
		return h._fail("the crane remote is not in hand while operating")
	await h._seconds(0.3)
	await h._pose("pendant")
	var boom_before := float(h._native().get_intake_boom_angle_radians())
	h._touch(1, h._center(&"pendant_left"), true)
	await h._seconds(1.0)
	h._touch(1, h._center(&"pendant_left"), false)
	var boom_after := float(h._native().get_intake_boom_angle_radians())
	var hook_before := float(h._native().get_intake_hook_position().y)
	h._touch(1, h._center(&"pendant_up"), true)
	await h._seconds(1.0)
	h._touch(1, h._center(&"pendant_up"), false)
	var hook_after := float(h._native().get_intake_hook_position().y)
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if absf(boom_after - boom_before) < 0.02:
		return h._fail("holding SLEW did not move the native boom (%.4f rad)" % (boom_after - boom_before))
	if absf(hook_after - hook_before) < 0.05:
		return h._fail("holding HOIST did not move the native hook (%.4f m)" % (hook_after - hook_before))
	if h._main._operating != &"":
		return h._fail("DONE did not close the pendant controls")
	h._detail = "boom_delta_rad=%.3f hook_delta_m=%.3f" % [boom_after - boom_before, hook_after - hook_before]
	return true


func _touch_pause() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	h._tap(0, h._center(&"pause"))
	await h._frames(2)
	if not h.get_tree().paused:
		return h._fail("PAUSE did not pause the tree")
	var tick := int(h._native().get_tick_index())
	await h._frames(12)
	if int(h._native().get_tick_index()) != tick:
		return h._fail("the native clock advanced while paused")
	await h._pose("pause_menu")
	# A real touch reaches the menu as its emulated mouse twin.
	h._click(h._main._pause_menu.side_button_center(&"controls"))
	await h._frames(2)
	if h._main._pause_menu.current_page() != &"controls":
		return h._fail("tapping CONTROLS did not open the controls page")
	await h._pose("pause_controls")
	h._click(h._main._pause_menu.side_button_center(&"settings"))
	await h._frames(2)
	if h._main._pause_menu.current_page() != &"settings":
		return h._fail("tapping SETTINGS did not open the settings page")
	var overflow: float = h._main._pause_menu.settings_overflow()
	if overflow > 0.0:
		return h._fail("the settings rows overflow their plate by %.0f px" % overflow)
	await h._pose("pause_settings")
	# GRAPHICS: one tap on QUALITY steps HIGH -> ULTRA, and the preset must
	# reach the renderer, not just the label.
	h._click(h._main._pause_menu.side_button_center(&"graphics"))
	await h._frames(2)
	if h._main._pause_menu.current_page() != &"graphics":
		return h._fail("tapping GRAPHICS did not open the graphics page")
	h._click(h._main._pause_menu.page_first_center(&"graphics"))
	await h._frames(2)
	var sun: DirectionalLight3D = h._main.get_node("Overcast")
	if h._main._settings.quality != 3 or h.get_viewport().msaa_3d != Viewport.MSAA_4X \
			or not is_equal_approx(sun.directional_shadow_max_distance, 300.0):
		return h._fail("QUALITY ULTRA did not reach the renderer (quality %d, msaa %d, shadow %.0f m)" % [
			h._main._settings.quality, h.get_viewport().msaa_3d, sun.directional_shadow_max_distance])
	await h._pose("pause_graphics")
	h._click(h._main._pause_menu.side_button_center(&"display"))
	await h._frames(2)
	if h._main._pause_menu.current_page() != &"display":
		return h._fail("tapping DISPLAY did not open the display page")
	await h._pose("pause_display")
	h._click(h._main._pause_menu.side_button_center(&"audio"))
	await h._frames(2)
	if h._main._pause_menu.current_page() != &"audio":
		return h._fail("tapping AUDIO did not open the audio page")
	await h._pose("pause_audio")
	h._click(h._main._pause_menu.side_button_center(&"resume"))
	await h._frames(3)
	if h.get_tree().paused:
		return h._fail("tapping RESUME did not resume")
	var resumed := int(h._native().get_tick_index())
	await h._frames(6)
	h._detail = "frozen_tick=%d advanced_to=%d" % [tick, int(h._native().get_tick_index())]
	return int(h._native().get_tick_index()) > resumed


func _pad_core() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	var start := h._position()
	var yaw_start: float = h._main._yaw
	var forward := Vector2(-sin(yaw_start), -cos(yaw_start))
	h._axis(JOY_AXIS_LEFT_Y, -1.0)
	await h._seconds(1.0)
	h._axis(JOY_AXIS_LEFT_Y, 0.0)
	var moved := h._position() - start
	var along := Vector2(moved.x, moved.z).dot(forward)
	if h._main._router.device != InputRouter.Device.GAMEPAD:
		return h._fail("stick input did not switch the interface to gamepad")
	await h._seconds(0.5)
	h._button(JOY_BUTTON_A)
	var lifted: bool = await h._wait_until(func() -> bool: return h._velocity().y > 2.0, 0.4)
	if not lifted:
		return h._fail("A did not jump")
	await h._wait_until(func() -> bool: return bool(h._native().is_player_grounded()), 2.0)
	var yaw_before: float = h._main._yaw
	h._axis(JOY_AXIS_RIGHT_X, 1.0)
	await h._seconds(0.5)
	h._axis(JOY_AXIS_RIGHT_X, 0.0)
	var turned: float = yaw_before - h._main._yaw
	h._button(JOY_BUTTON_RIGHT_STICK)
	var r3_crouched: bool = await h._wait_until(
		func() -> bool: return bool(h._native().is_player_crouched()), 0.3)
	h._button(JOY_BUTTON_RIGHT_STICK)
	var r3_stood: bool = await h._wait_until(
		func() -> bool: return not bool(h._native().is_player_crouched()), 0.3)
	if not (r3_crouched and r3_stood):
		return h._fail("the right-stick click did not toggle crouch on and off")
	h._button(JOY_BUTTON_START)
	await h._frames(2)
	if not h.get_tree().paused:
		return h._fail("START did not pause")
	var tick := int(h._native().get_tick_index())
	await h._frames(10)
	var frozen := int(h._native().get_tick_index()) == tick
	h._button(JOY_BUTTON_START)
	await h._frames(3)
	if h.get_tree().paused:
		return h._fail("START did not resume")
	if along < 2.5:
		return h._fail("left stick moved only %.2f m" % along)
	if turned < 0.8:
		return h._fail("right stick turned only %.3f rad in 0.5 s" % turned)
	if not frozen:
		return h._fail("native clock advanced while paused")
	h._detail = "forward_m=%.2f turned_rad=%.3f" % [along, turned]
	return true


func _pad_pendant() -> bool:
	var reached: bool = await h._wait_until(func() -> bool: return h._ctx()["action"]["id"] == &"operate", 2.0)
	if not reached:
		return h._fail("ACTION never offered OPERATE at the KX-JIB pendant")
	h._button(JOY_BUTTON_X)
	await h._frames(3)
	if h._main._operating != &"jib":
		return h._fail("X did not open the KX-JIB pendant")
	await h._seconds(0.3)
	await h._pose("pad_pendant")
	var boom_before := float(h._native().get_jib_boom_angle_radians())
	h._button(JOY_BUTTON_DPAD_LEFT, true)
	await h._seconds(1.0)
	h._button(JOY_BUTTON_DPAD_LEFT, false)
	var boom_after := float(h._native().get_jib_boom_angle_radians())
	var hook_before := float(h._native().get_jib_hook_position().y)
	h._axis(JOY_AXIS_TRIGGER_RIGHT, 1.0)
	await h._seconds(1.0)
	h._axis(JOY_AXIS_TRIGGER_RIGHT, 0.0)
	var hook_after := float(h._native().get_jib_hook_position().y)
	h._button(JOY_BUTTON_B)
	await h._frames(3)
	if absf(boom_after - boom_before) < 0.02:
		return h._fail("D-pad did not drive the native jib (%.4f rad)" % (boom_after - boom_before))
	if absf(hook_after - hook_before) < 0.05:
		return h._fail("RT did not hoist the native hook (%.4f m)" % (hook_after - hook_before))
	if h._main._operating != &"":
		return h._fail("B did not leave the pendant controls")
	h._detail = "boom_delta_rad=%.3f hook_delta_m=%.3f" % [boom_after - boom_before, hook_after - hook_before]
	return true


func _keyboard_core() -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	var start := h._position()
	var yaw_start: float = h._main._yaw
	var forward := Vector2(-sin(yaw_start), -cos(yaw_start))
	h._key(KEY_W, true)
	await h._seconds(1.0)
	h._key(KEY_W, false)
	var moved := h._position() - start
	var along := Vector2(moved.x, moved.z).dot(forward)
	await h._seconds(0.5)
	h._key(KEY_SPACE, true)
	h._key(KEY_SPACE, false)
	var lifted: bool = await h._wait_until(func() -> bool: return h._velocity().y > 2.0, 0.4)
	await h._wait_until(func() -> bool: return bool(h._native().is_player_grounded()), 2.0)
	await h._seconds(0.3)
	var rejected_before := int(h._native().get_rejected_traversal_count())
	h._key(KEY_E, true)
	h._key(KEY_E, false)
	await h._frames(3)
	var rejected_after := int(h._native().get_rejected_traversal_count())
	h._key(KEY_CTRL, true)
	var ctrl_crouched: bool = await h._wait_until(
		func() -> bool: return bool(h._native().is_player_crouched()), 0.3)
	h._key(KEY_CTRL, false)
	var ctrl_stood: bool = await h._wait_until(
		func() -> bool: return not bool(h._native().is_player_crouched()), 0.3)
	h._key(KEY_C, true)
	h._key(KEY_C, false)
	var c_crouched: bool = await h._wait_until(
		func() -> bool: return bool(h._native().is_player_crouched()), 0.3)
	h._key(KEY_C, true)
	h._key(KEY_C, false)
	var c_stood: bool = await h._wait_until(
		func() -> bool: return not bool(h._native().is_player_crouched()), 0.3)
	h._key(KEY_ESCAPE, true)
	h._key(KEY_ESCAPE, false)
	await h._frames(2)
	var paused := h.get_tree().paused
	await h._pose("keyboard_pause")
	h._key(KEY_ESCAPE, true)
	h._key(KEY_ESCAPE, false)
	await h._frames(3)
	if along < 2.5:
		return h._fail("W moved only %.2f m" % along)
	if not lifted:
		return h._fail("SPACE did not jump")
	if rejected_after != rejected_before + 1:
		return h._fail("E with nothing in reach did not reach the native (rejected %d->%d)" % [
			rejected_before, rejected_after])
	if not (ctrl_crouched and ctrl_stood):
		return h._fail("holding Ctrl did not crouch, or letting go did not stand")
	if not (c_crouched and c_stood):
		return h._fail("C did not toggle crouch on and off")
	if not paused:
		return h._fail("ESC did not pause")
	if h.get_tree().paused:
		return h._fail("ESC did not resume from the menu")
	h._detail = "forward_m=%.2f rejected=%d->%d" % [along, rejected_before, rejected_after]
	return true


# AS-006 Stage A through the real input pipeline on one device: walk into
# the skip lift's cage, UNHOOK the rope's end from the bollard, HOOK it onto
# the cage's eye, GRAB the trip handle, step back until the catch lets go,
# LET GO, and ride 22 m. Each verb is the one the HUD offers at that moment,
# pressed on the device under test, and each is proven by the native state
# it changed. The hands must move between poses, never jump.
func _rig(device: int) -> bool:
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	for leg in [Vector2(-10.0, -129.2), Vector2(-10.2, -130.6), Vector2(-11.35, -131.95)]:
		if not await h._walk_to(device, leg, 0.2):
			return h._fail("the walk into the cage stalled at (%.2f, %.2f)" % [
				h._position().x, h._position().z])
	await h._face(Vector2(-0.7, -0.7))
	if not await h._offered(&"unhook", "UNHOOK"):
		return h._fail("Action read '%s' facing the bollard, not UNHOOK" % h._action_label())
	var watch := h._watch_wrists()
	h._act(device)
	var holding_shackle: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 2002, 0.5)
	if not holding_shackle:
		return h._fail("UNHOOK did not put the rope's shackle in the hands")
	if not await h._walk_to(device, Vector2(-11.35, -131.4), 0.2):
		return h._fail("the step to the cage's eye stalled")
	await h._face(Vector2(-1.0, 0.0))
	if not await h._offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return h._fail("Action read '%s %s' at the cage's eye, not HOOK ONTO CAGE EYE" % [
			h._action_label(), String(h._ctx()["action"]["detail"])])
	await h._pose("rig_shackle")
	h._act(device)
	var hooked: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_well_a_rope_end_entity_id()) == 2000, 0.5)
	if not hooked:
		return h._fail("HOOK did not put the rope's end on the cage's eye (end %d, at %s, carrying %d)" % [
			int(h._native().get_well_a_rope_end_entity_id()), str(h._position()),
			int(h._native().get_carrying_entity_id())])
	if not await h._walk_to(device, Vector2(-11.05, -130.35), 0.2):
		return h._fail("the step to the trip handle stalled")
	await h._face(Vector2(0.0, 1.0))
	if not await h._offered(&"pick_up", "GRAB"):
		return h._fail("Action read '%s' facing the trip handle, not GRAB" % h._action_label())
	h._act(device)
	var holding_handle: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 2004, 0.5)
	if not holding_handle:
		return h._fail("GRAB did not put the trip handle in the hands")
	await h._seconds(0.5)
	if h._main._arms.hand_poses() != [8, 8]:
		return h._fail("hands not on the trip handle (poses %s)" % str(h._main._arms.hand_poses()))
	if h._action_label() != "LET GO":
		return h._fail("Action read '%s' holding the handle, not LET GO" % h._action_label())
	await h._pose("rig_handle")
	h._move(device, -0.6)
	var tripped: bool = await h._wait_until(
		func() -> bool: return not bool(h._native().is_well_a_catch_latched()), 3.0)
	h._move(device, 0.0)
	if not tripped:
		return h._fail("stepping back with the handle never opened the catch")
	h._act(device)
	var let_go: bool = await h._wait_until(
		func() -> bool: return int(h._native().get_carrying_entity_id()) == 0, 0.5)
	if not let_go:
		return h._fail("LET GO did not take the handle out of the hands")
	var worst_jump := h._stop_watch(watch)
	var arrived: bool = await h._wait_until(
		func() -> bool: return float(h._native().get_well_a_cage_travel()) >= 21.95, 15.0)
	if not arrived:
		return h._fail("the cage never reached the top (travel %.2f m)" %
			float(h._native().get_well_a_cage_travel()))
	await h._seconds(0.5)
	await h._pose("rig_top")
	if int(h._native().get_support_entity_id()) != 2000 or h._position().y < 177.0:
		return h._fail("the rider is not standing in the cage at the top (y %.2f)" % h._position().y)
	# 0.10 m in a 60 Hz frame is 6 m/s across the view: faster than any reach.
	if worst_jump > 0.10:
		return h._fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	h._detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [h._position().y, worst_jump]
	return true
