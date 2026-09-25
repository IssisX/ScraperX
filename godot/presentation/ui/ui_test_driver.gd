extends Node

# Scripted proof that every control path reaches the native authority. Each
# scenario injects events through the real viewport input pipeline -- the
# same _input the devices feed -- and asserts a NATIVE state change (tick,
# traversal, velocity, carry and rig state), never a presentation variable
# alone.
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
	"touch_jump": 8,
	"touch_move_look": 8,
	"touch_gyro_aim": 8,
	"touch_climb": 4,
	"touch_vault": 3,
	"touch_double_tap_vault": 3,
	"touch_crouch": 0,
	"touch_hang_drop": 5,
	"touch_hang_climb": 5,
	"touch_chute": 9,
	"touch_lethal_feedback": 9,
	"touch_pause": 8,
	"pad_core": 8,
	"keyboard_core": 8,
	# AS-006 Stage A, rigged and ridden on each device from the 154 m deck.
	"touch_rig": 11,
	"pad_rig": 11,
	"keyboard_rig": 11,
	# AS-006 Stage C, the chute cleared and the platform freed on each device.
	"touch_debris": 13,
	"pad_debris": 13,
	"keyboard_debris": 13,
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
	if not bool(main._native.configure_initial_spawn(int(SCENARIOS[scenario]))):
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
		"touch_pause":
			ok = await _touch_pause()
		"pad_core":
			ok = await _pad_core()
		"keyboard_core":
			ok = await _keyboard_core()
		"touch_rig":
			ok = await _rig(InputRouter.Device.TOUCH)
		"pad_rig":
			ok = await _rig(InputRouter.Device.GAMEPAD)
		"keyboard_rig":
			ok = await _rig(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_debris":
			ok = await _debris(InputRouter.Device.TOUCH)
		"pad_debris":
			ok = await _debris(InputRouter.Device.GAMEPAD)
		"keyboard_debris":
			ok = await _debris(InputRouter.Device.KEYBOARD_MOUSE)
		"audio_mix":
			ok = await _audio_mix()
	print("SCRAPERX_UITEST %s %s %s" % ["PASS" if ok else "FAIL", _scenario, _detail])
	get_tree().paused = false
	get_tree().quit(0 if ok else 31)


func _fail(reason: String) -> bool:
	_detail = "reason=" + reason
	return false


# --- scenarios -----------------------------------------------------------------


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
		_detail = "top_y=%.2f grip_error_m=%.4f" % [_position().y, grip_error]
		return _position().y > 4.4
	_tap(1, _center(&"drop"))
	var reached_4: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 0, 0.2)
	if not reached_4:
		return _fail("DROP did not release the hang")
	await _frames(2)
	_detail = "vy_after_release=%.2f" % _velocity().y
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


# AS-006 Stage C from its platform at 198 m: GRAB the rebar's line, step
# back until the rebar goes over upright and clear of the chute, LET GO, and
# watch the rubble pour into the dumpster; GRAB the keeper latch's handle,
# step back until the pin leaves the platform, LET GO, and ride 22 m to the
# 220 ring. Each verb is the one the HUD offers, pressed on the device under
# test, and proven by the native state it changed; the hands never jump.
func _debris(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _walk_to(device, Vector2(-4.4, -131.6), 0.2):
		return _fail("the step to the rebar's handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB", "REBAR LINE"):
		return _fail("Action read '%s %s' facing the rebar's handle, not GRAB REBAR LINE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	var watch := _watch_wrists()
	_act(device)
	var holding_line: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2024, 0.5)
	if not holding_line:
		return _fail("GRAB did not put the rebar's handle in the hands")
	_move(device, -0.6)
	var thrown: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_rebar_angle()) > 1.7, 3.0)
	_move(device, 0.0)
	if not thrown:
		return _fail("stepping back never threw the rebar over (angle %.2f rad, at %s, carrying %d)" % [
			float(_native().get_well_c_rebar_angle()), str(_position()),
			int(_native().get_carrying_entity_id())])
	# A hard pull can have the line torn out of the hands as the bar goes
	# over; LET GO is only there to press while it is still held.
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take the rebar's handle out of the hands")
	# Turn to the chute across the well and watch it pour.
	await _face(Vector2(-1.0, 0.2))
	await _wait_until(func() -> bool: return float(_native().get_well_c_dumpster_kg()) > 300.0, 4.0)
	await _pose("debris_pour")
	var filled: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_dumpster_kg()) >= 899.0, 8.0)
	if not filled:
		return _fail("the dumpster never filled (%.0f kg)" % float(_native().get_well_c_dumpster_kg()))
	if float(_native().get_well_c_platform_travel()) > 0.05:
		return _fail("latched, the platform moved %.2f m" % float(_native().get_well_c_platform_travel()))
	if not await _walk_to(device, Vector2(-3.0, -131.6), 0.2):
		return _fail("the step to the latch's handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB", "LATCH HANDLE"):
		return _fail("Action read '%s %s' facing the latch's handle, not GRAB LATCH HANDLE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	_act(device)
	var holding_latch: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2026, 0.5)
	if not holding_latch:
		return _fail("GRAB did not put the latch's handle in the hands")
	_move(device, -0.6)
	var freed: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_well_c_catch_latched()), 3.0)
	_move(device, 0.0)
	if not freed:
		return _fail("stepping back with the latch's handle never freed the platform")
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take the latch's handle out of the hands")
	var worst_jump := _stop_watch(watch)
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_platform_travel()) >= 21.9, 15.0)
	if not arrived:
		return _fail("the platform never reached the top (travel %.2f m)" %
			float(_native().get_well_c_platform_travel()))
	await _seconds(0.5)
	await _pose("debris_top")
	if int(_native().get_support_entity_id()) != 2020 or _position().y < 221.0:
		return _fail("the rider is not standing on the platform at the top (y %.2f)" % _position().y)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true


# Presses LET GO when the hands still hold something, and waits for them to
# be empty.
func _let_go_if_held(device: int) -> bool:
	if int(_native().get_carrying_entity_id()) == 0:
		return true
	if not await _offered(&"set_down", "LET GO"):
		return false
	_act(device)
	return await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)


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
func _walk_to(device: int, target: Vector2, tolerance: float) -> bool:
	var waited := 0.0
	var frame := 0
	while waited < 6.0:
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		if to.length() <= tolerance:
			_move_dir(device, Vector2.ZERO)
			await _seconds(0.2)
			return true
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		var v := Vector2(to.dot(right), to.dot(forward)).normalized() * clampf(to.length() / 0.8, 0.25, 1.0)
		if device == InputRouter.Device.KEYBOARD_MOUSE and to.length() < 0.8:
			frame += 1
			v = v.normalized() if frame % 6 < 2 else Vector2.ZERO
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
