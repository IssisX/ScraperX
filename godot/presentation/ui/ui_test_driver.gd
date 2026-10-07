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
	# AS-006 Stage B, the derrick boom rigged and ridden on each device from the 176 m ring.
	"touch_boom": 12,
	"pad_boom": 12,
	"keyboard_boom": 12,
	# AS-006 Stage C, the chute cleared and the platform freed on each device.
	"touch_debris": 13,
	"pad_debris": 13,
	"keyboard_debris": 13,
	# AS-007, Wet Isolation, on its three machines from the 220 ring to TP-340.
	"touch_wet": 16,
	"pad_wet": 16,
	"keyboard_wet": 16,
	# AS-008, the Plate Shop, on its three machines from TP-340 to the 484 ring.
	"touch_shop": 19,
	"pad_shop": 19,
	"keyboard_shop": 19,
	# AS-009, the Facade Crane Stack, on its three machines from the 484 ring to TP-640.
	"touch_crane": 22,
	"pad_crane": 22,
	"keyboard_crane": 22,
	# AS-010, Midstack Service, its service lift from TP-640 to the 662 deck.
	"touch_service": 30,
	"pad_service": 30,
	"keyboard_service": 30,
	# AS-010's C4, the service gantry, climbed from the 662 deck to the 684 deck
	# on touch, the device the game is played on.
	"touch_c4": 31,
	# AS-010's stage N, the granular discharge hoist, from the 684 deck to the
	# 706 deck on touch.
	"touch_n": 32,
	# AS-010's C5, the cooling plant, climbed from the 706 deck to the 728 deck
	# on touch.
	"touch_c5": 33,
	# AS-010's stage O, the gravel wheel, from the 728 deck to the 750 deck on
	# touch.
	"touch_o": 34,
	# The ground slingshot from the game's start: into the pouch, the stick
	# pulled back to draw, RELEASE, the chute over the tower and the stick
	# steering down onto the 220 ring.
	"touch_slingshot": 8,
	# AS-012, the swing, from the 220 ring where the slingshot and the route
	# from grade arrive: out the gangway into the seat, STRAP IN, KICK THE
	# TRIP, the ram's blow and the swing up to the 242 ring, UNBUCKLE and off.
	"touch_swing": 16,
	# C6, the west band, from the 242 ring where the swing sets its rider down
	# to the 264 ring: no ladder, no standpipe.
	"touch_c6": 35,
	# Checkpoint continuation and lethal rollback proof from Deck 4 (+44 m).
	"touch_checkpoint": 26,
	# Upper Stack continuation from Deck 4 checkpoint through S2, C2, S3, C3 to Deck 14 (+154 m).
	"touch_stack_upper": 26,
	"pad_stack_upper": 26,
	"keyboard_stack_upper": 26,
	# The continuous ascent from the game's start at grade through the upper machines to +221 m receiver.
	"touch_stack": 8,
	"pad_stack": 8,
	"keyboard_stack": 8,
	# The fear voice on a ~12 m drop lived through: a yelp, then pain.
	"fall_voice": 10,
	# Need a mixing audio driver: run under --write-movie (see _audio_mix).
	"audio_mix": 8,
	"audio_fall": 9,
}

# The native's TraversalState and the tower's entity id, as main.gd reads them.
const TRAVERSAL_NONE := 0
const TRAVERSAL_HANGING := 1
const TRAVERSAL_CLIMBING := 4
const TOWER_ENTITY := 11
# AS-010's frame: the 662 deck and the 684 deck.
const SERVICE_FRAME_ENTITY := 1012
# The world's solids, the rings among them: what C6's climber stands on at the
# 264 ring.
const RING_264_ENTITY := 51
# The walker's braking with nothing pressed, as the native has it: 22 m/s^2
# on the ground, 14 in the air. Keys walking to a point let go by the weaker;
# with a load in the hands, which caps it (5.3 for D's 50 kg spool), by less.
const KEY_BRAKE_MPS2 := 14.0
const KEY_CARRY_BRAKE_MPS2 := 5.0

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
	# The same lines in the same order on every run of a scenario.
	_main._audio.voice_rng.seed = 0x5C4A9E
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
		"touch_boom":
			ok = await _boom(InputRouter.Device.TOUCH)
		"pad_boom":
			ok = await _boom(InputRouter.Device.GAMEPAD)
		"keyboard_boom":
			ok = await _boom(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_debris":
			ok = await _debris(InputRouter.Device.TOUCH)
		"pad_debris":
			ok = await _debris(InputRouter.Device.GAMEPAD)
		"keyboard_debris":
			ok = await _debris(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_wet":
			ok = await _wet(InputRouter.Device.TOUCH)
		"pad_wet":
			ok = await _wet(InputRouter.Device.GAMEPAD)
		"keyboard_wet":
			ok = await _wet(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_shop":
			ok = await _shop(InputRouter.Device.TOUCH)
		"pad_shop":
			ok = await _shop(InputRouter.Device.GAMEPAD)
		"keyboard_shop":
			ok = await _shop(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_crane":
			ok = await _crane(InputRouter.Device.TOUCH)
		"pad_crane":
			ok = await _crane(InputRouter.Device.GAMEPAD)
		"keyboard_crane":
			ok = await _crane(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_service":
			ok = await _service(InputRouter.Device.TOUCH)
		"pad_service":
			ok = await _service(InputRouter.Device.GAMEPAD)
		"keyboard_service":
			ok = await _service(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_c4":
			ok = await _c4(InputRouter.Device.TOUCH)
		"touch_n":
			ok = await _n(InputRouter.Device.TOUCH)
		"touch_c5":
			ok = await _c5(InputRouter.Device.TOUCH)
		"touch_o":
			ok = await _o(InputRouter.Device.TOUCH)
		"touch_slingshot":
			ok = await _slingshot(InputRouter.Device.TOUCH)
		"touch_swing":
			ok = await _swing(InputRouter.Device.TOUCH)
		"touch_c6":
			ok = await _c6(InputRouter.Device.TOUCH)
		"touch_checkpoint":
			ok = await _checkpoint_continuation(InputRouter.Device.TOUCH)
		"touch_stack_upper":
			ok = await _stack_upper(InputRouter.Device.TOUCH)
		"pad_stack_upper":
			ok = await _stack_upper(InputRouter.Device.GAMEPAD)
		"keyboard_stack_upper":
			ok = await _stack_upper(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_stack":
			ok = await _stack(InputRouter.Device.TOUCH)
		"pad_stack":
			ok = await _stack(InputRouter.Device.GAMEPAD)
		"keyboard_stack":
			ok = await _stack(InputRouter.Device.KEYBOARD_MOUSE)
		"fall_voice":
			ok = await _fall_voice()
		"audio_mix":
			ok = await _audio_mix()
		"audio_fall":
			ok = await _audio_fall()
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
	# The fear voice rode the whole fall -- a yelp, then panic or terror as
	# it went on -- and the impact cut it dead.
	var said: Dictionary = _main._audio.voice_lines
	if int(said.get(&"yelp", 0)) < 1 or int(said.get(&"panic", 0)) + int(said.get(&"terror", 0)) < 1:
		return _fail("the fear voice did not ride the fall: %s" % str(said))
	if StringName(_main._audio._voice_tier) != &"":
		return _fail("the fear voice runs on after the lethal impact (%s)" % _main._audio._voice_tier)
	_detail = "toast='%s' lethal_mps=%.1f voice=%s cut=%d" % [sub, lethal, str(said),
		int(_main._audio.voice_cut)]
	# The announced speed must be the lethal fall it reports, not a settle.
	return fell_at > lethal


# SurvivableDrop: ~12 m with nothing to hold -- fast enough to yelp at, and a
# hard landing lived through, which hurts.
func _fall_voice() -> bool:
	var landed: bool = await _wait_until(func() -> bool: return (bool(_native().is_player_grounded())
		and _velocity().length() < 0.5), 6.0)
	if not landed:
		return _fail("the drop never landed")
	var said: Dictionary = _main._audio.voice_lines
	var impact := float(_native().get_last_impact_speed_mps())
	_detail = "impact_mps=%.2f voice=%s" % [impact, str(said)]
	if int(_native().get_death_count()) != 0:
		return _fail("the survivable drop killed (%.1f m/s)" % impact)
	if int(said.get(&"yelp", 0)) < 1:
		return _fail("no yelp on a %.1f m/s drop" % impact)
	if int(said.get(&"pain", 0)) < 1:
		return _fail("a %.1f m/s landing lived through said nothing" % impact)
	return true


func _touch_pause() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	# Every START option the settings offer must reach the native: a fresh
	# native, its clock at 0, accepts each spawn the menu can ask for.
	for spawn in _main._settings.START_SPAWNS:
		if int(spawn) >= 0 and not bool(ClassDB.instantiate("ScraperXSimulation").configure_initial_spawn(int(spawn))):
			return _fail("the native refused START option spawn %d" % int(spawn))
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


func _well_stage_a(device: int) -> bool:
	for leg in [Vector2(-10.0, -129.2), Vector2(-10.2, -130.6), Vector2(-11.35, -131.95)]:
		if not await _walk_to(device, leg, 0.2):
			return _fail("the walk into the cage stalled at (%.2f, %.2f)" % [
				_position().x, _position().z])
	await _face(Vector2(-0.7, -0.7))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s' facing the bollard, not UNHOOK" % _action_label())
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
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_a_cage_travel()) >= 21.95, 15.0)
	if not arrived:
		return _fail("the cage never reached the top (travel %.2f m)" %
			float(_native().get_well_a_cage_travel()))
	await _seconds(0.5)
	await _pose("rig_top")
	if int(_native().get_support_entity_id()) != 2000 or _position().y < 177.0:
		return _fail("the rider is not standing in the cage at the top (y %.2f)" % _position().y)
	return true


func _rig(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var watch := _watch_wrists()
	if not await _well_stage_a(device):
		return false
	var worst_jump := _stop_watch(watch)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true


func _well_stage_b(device: int) -> bool:
	if _position().x < -8.5:
		if not (await _walk_to(device, Vector2(-9.4, -131.2), 0.2, 4.0) and \
				await _walk_to(device, Vector2(-7.6, -131.2), 0.2, 4.0)):
			return _fail("step across from A's parked cage into B's stalled")
	if not await _walk_to(device, Vector2(-6.3, -130.35), 0.2, 4.0):
		return _fail("the step to Stage B cleat stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s' facing Stage B cleat, not UNHOOK" % _action_label())
	_act(device)
	var holding_shackle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2012, 0.5)
	if not holding_shackle:
		return _fail("UNHOOK did not put Stage B line's shackle in hands")
	if not await _walk_to(device, Vector2(-6.35, -131.40), 0.2, 4.0):
		return _fail("the step to Stage B cage eye stalled")
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at Stage B cage eye, not HOOK ONTO CAGE EYE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("boom_shackle")
	_act(device)
	var hooked: bool = await _wait_until(
		func() -> bool: return int(_native().get_well_b_rope_end_entity_id()) == 2010, 0.5)
	if not hooked:
		return _fail("HOOK did not put Stage B line on cage eye")
	if not await _walk_to(device, Vector2(-8.0, -130.25), 0.2, 4.0):
		return _fail("the step to Stage B trip handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing Stage B trip handle, not GRAB" % _action_label())
	_act(device)
	var holding_handle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2014, 0.5)
	if not holding_handle:
		return _fail("GRAB did not put Stage B trip handle in hands")
	await _pose("boom_handle")
	_move(device, -0.6)
	var tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_well_b_catch_latched()), 3.0)
	_move(device, 0.0)
	if not tripped:
		return _fail("stepping back with Stage B handle never opened catch")
	_act(device)
	var let_go: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)
	if not let_go:
		return _fail("LET GO did not drop Stage B handle")
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_b_cage_travel()) >= 21.9, 16.0)
	if not arrived:
		return _fail("Stage B cage never reached top (travel %.2f m)" %
			float(_native().get_well_b_cage_travel()))
	await _seconds(0.5)
	await _pose("boom_top")
	if int(_native().get_support_entity_id()) != 2010 or _position().y < 198.5:
		return _fail("rider not standing in Stage B cage at top (y %.2f)" % _position().y)
	return true


func _boom(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var watch := _watch_wrists()
	if not await _well_stage_b(device):
		return false
	var worst_jump := _stop_watch(watch)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true


func _well_stage_c(device: int) -> bool:
	if _position().x < -5.0:
		if not (await _walk_to(device, Vector2(-6.2, -131.9), 0.2, 4.0) and \
				await _walk_to(device, Vector2(-4.4, -131.9), 0.2, 4.0)):
			return _fail("step across onto Stage C platform stalled")
	if not await _walk_to(device, Vector2(-4.4, -131.6), 0.2, 4.0):
		return _fail("the step to the rebar's handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB", "REBAR LINE"):
		return _fail("Action read '%s %s' facing the rebar's handle, not GRAB REBAR LINE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
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
		return _fail("stepping back never threw the rebar over")
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take the rebar's handle out of the hands")
	await _face(Vector2(-1.0, 0.2))
	await _wait_until(func() -> bool: return float(_native().get_well_c_dumpster_kg()) > 300.0, 4.0)
	await _pose("debris_pour")
	var filled: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_dumpster_kg()) >= 899.0, 8.0)
	if not filled:
		return _fail("the dumpster never filled (%.0f kg)" % float(_native().get_well_c_dumpster_kg()))
	if float(_native().get_well_c_platform_travel()) > 0.05:
		return _fail("latched, the platform moved %.2f m" % float(_native().get_well_c_platform_travel()))
	if not await _walk_to(device, Vector2(-3.0, -131.6), 0.2, 4.0):
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
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_platform_travel()) >= 21.9, 15.0)
	if not arrived:
		return _fail("the platform never reached the top (travel %.2f m)" %
			float(_native().get_well_c_platform_travel()))
	await _seconds(0.5)
	if not await _walk_to(device, Vector2(-3.0, -129.0), 0.2, 4.0):
		return _fail("the step off Stage C onto Ring 220 stalled")
	await _seconds(0.5)
	await _pose("debris_top")
	if not bool(_native().is_player_grounded()) or _position().y < 221.0 or \
			int(_native().get_support_entity_id()) == 2020:
		return _fail("not standing on Ring 220 receiver slab (y %.2f, support %d)" % [
			_position().y, int(_native().get_support_entity_id())])
	return true


func _debris(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var watch := _watch_wrists()
	if not await _well_stage_c(device):
		return false
	var worst_jump := _stop_watch(watch)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true


# AS-007, Wet Isolation, the way its native band test runs it: from the 220
# ring, D's spool carried into its gap and its fill valve thrown, a ride on
# the float's platform to 256 m; E's door drawn shut and its chiller tripped,
# a ride in the cab on the chiller's air to 298 m; F's hose hooked onto the
# ram's inlet and its stop valve thrown, a ride on the accumulator's platform,
# and off onto TP-340.
func _wet(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not (await _wet_d(device) and await _wet_e(device) and await _wet_f(device)):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "plate_y=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step]
	return true


func _wet_state() -> Dictionary:
	return _native().get_wet_state()


# Faces the handle ahead at (x, z) along `facing`, GRABs `name` (entity
# `handle`) and steps back with it until `done` holds, then lets go.
func _pull_handle(device: int, at: Vector2, facing: Vector2, handle: int, name: String,
		done: Callable) -> bool:
	if not await _walk_to(device, at, 0.08, 6.0):
		return _fail("the step to %s stalled at %s" % [name, str(_position())])
	await _face(facing)
	if not await _offered(&"pick_up", "GRAB", name):
		return _fail("Action read '%s %s' facing %s, not GRAB" % [_action_label(),
			String(_ctx()["action"]["detail"]), name])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == handle, 0.5):
		return _fail("GRAB did not take %s" % name)
	_move(device, -0.35)
	var thrown: bool = await _wait_until(done, 4.0)
	_move(device, 0.0)
	if not thrown:
		return _fail("stepping back with %s never threw it" % name)
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take %s out of the hands" % name)
	return true


func _wet_d(device: int) -> bool:
	if not await _go(device, Vector2(9.5, -130.6), 0.08, 12.0):
		return _fail("the walk to D's spool stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP", "PIPE SPOOL"):
		return _fail("Action read '%s %s' facing D's spool, not PICK UP PIPE SPOOL" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2031, 0.5):
		return _fail("PICK UP did not lift D's spool")
	# Carried north into the gap as the native test carries it, at 0.35 of
	# full throw to z -128.25 and a coast; a key, with no throw, walks it
	# there and stops.
	var carried := false
	if device == InputRouter.Device.KEYBOARD_MOUSE:
		carried = await _walk_to(device, Vector2(9.5, -128.17), 0.05, 6.0)
	else:
		_move(device, 0.35)
		carried = await _wait_until(func() -> bool: return _position().z >= -128.25, 6.0)
		_move(device, 0.0)
	if not carried:
		return _fail("carrying D's spool to its gap stalled at %s" % str(_position()))
	# Steadied before it is set down, as a player does with a swinging load.
	await _seconds(0.6)
	var spool := int(_native().get_kit_body_index(2031))
	var spool_last := (_native().get_kit_body_transform(spool) as Transform3D).origin
	for frame in 180:
		await get_tree().process_frame
		var spool_now := (_native().get_kit_body_transform(spool) as Transform3D).origin
		var swing := (spool_now - spool_last).length() / get_process_delta_time()
		spool_last = spool_now
		if swing < 0.05:
			break
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set D's spool down")
	await _seconds(1.5)
	if not bool(_wet_state()["d_pipe_whole"]):
		return _fail("set down, D's spool did not close the fill line's gap")
	if not (await _go(device, Vector2(13.4, -129.6), 0.15, 8.0) and \
			await _go(device, Vector2(13.4, -132.7), 0.15, 6.0)):
		return _fail("the walk onto D's platform stalled at %s" % str(_position()))
	if not await _pull_handle(device, Vector2(12.3, -132.7), Vector2(0.0, 1.0), 2033, "FILL VALVE",
			func() -> bool: return float(_wet_state()["d_valve_angle"]) > 1.8):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_wet_state()["d_platform_travel"]) >= 35.95, 120.0)
	if not arrived:
		return _fail("D's platform never reached 256 m (travel %.2f m)" % float(_wet_state()["d_platform_travel"]))
	await _seconds(1.0)
	await _pose("wet_d_top")
	if int(_native().get_support_entity_id()) != 2030:
		return _fail("the rider is not on D's platform at the top (y %.2f)" % _position().y)
	return true


func _wet_e(device: int) -> bool:
	if not (await _go(device, Vector2(13.2, -134.5), 0.15, 6.0) and \
			await _walk_to(device, Vector2(13.2, -135.8), 0.15, 6.0) and \
			await _walk_to(device, Vector2(13.05, -136.4), 0.08, 6.0)):
		return _fail("the walk through E's door into its cab stalled at %s" % str(_position()))
	var leaf := int(_native().get_kit_body_index(2042))
	var last := (_native().get_kit_body_transform(leaf) as Transform3D).origin
	for frame in 240:
		await get_tree().process_frame
		var now := (_native().get_kit_body_transform(leaf) as Transform3D).origin
		var moved := Vector2(now.x - last.x, now.z - last.z).length() / get_process_delta_time()
		last = now
		if moved < 0.2:
			break
	var at := _position()
	await _face(Vector2(last.x - at.x, last.z - at.z))
	if not await _offered(&"pick_up", "GRAB", "DOOR"):
		return _fail("Action read '%s %s' facing E's door, not GRAB DOOR" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2042, 0.5):
		return _fail("GRAB did not take E's door")
	_move_world(device, Vector2(0.4, 0.0))
	var latched: bool = await _wait_until(func() -> bool: return bool(_wet_state()["e_door_latched"]), 6.0)
	_move_dir(device, Vector2.ZERO)
	if not latched:
		return _fail("drawing E's door east never latched it")
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take E's door out of the hands")
	if not await _pull_handle(device, Vector2(13.4, -136.65), Vector2(1.0, 0.0), 2045, "CHILLER TRIP",
			func() -> bool: return not bool(_wet_state()["e_catch_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_wet_state()["e_cab_travel"]) >= 41.95, 90.0)
	if not arrived:
		return _fail("E's cab never reached 298 m (travel %.2f m)" % float(_wet_state()["e_cab_travel"]))
	await _seconds(1.0)
	await _pose("wet_e_top")
	if int(_native().get_support_entity_id()) != 2040:
		return _fail("the rider is not in E's cab at the top (y %.2f)" % _position().y)
	return true


func _wet_f(device: int) -> bool:
	if not (await _go(device, Vector2(13.4, -137.2), 0.15, 4.0) and \
			await _walk_to(device, Vector2(13.4, -139.9), 0.15, 6.0) and \
			await _walk_to(device, Vector2(12.6, -139.95), 0.08, 6.0)):
		return _fail("the walk from E's cab onto F's platform stalled at %s" % str(_position()))
	await _face(Vector2(-0.3, -0.95))
	if not await _offered(&"pick_up", "TAKE", "HOSE"):
		return _fail("Action read '%s %s' facing F's hose, not TAKE HOSE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2052, 0.5):
		return _fail("TAKE did not put F's hose in the hands")
	if not await _walk_to(device, Vector2(12.4, -139.55), 0.08, 4.0):
		return _fail("the step to the ram's inlet stalled at %s" % str(_position()))
	await _face(Vector2(-0.6, 0.8))
	if not await _offered(&"hook", "HOOK", "ONTO RAM INLET"):
		return _fail("Action read '%s %s' at the ram's inlet, not HOOK ONTO RAM INLET" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_wet_state()["f_hose_coupled"]), 0.5):
		return _fail("HOOK did not couple F's hose to the ram")
	if not await _pull_handle(device, Vector2(12.25, -140.7), Vector2(0.0, -1.0), 2054, "STOP VALVE",
			func() -> bool: return not bool(_wet_state()["f_catch_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_wet_state()["f_platform_travel"]) >= 41.95, 90.0)
	if not arrived:
		var f := _wet_state()
		return _fail("F's platform never reached TP-340 (travel %.2f m, hose coupled %s, catch latched %s, accumulator %.2f m)" % [
			float(f["f_platform_travel"]), str(f["f_hose_coupled"]), str(f["f_catch_latched"]),
			float(f["f_accumulator_travel"])])
	await _seconds(1.0)
	if not await _go(device, Vector2(13.0, -137.2), 0.15, 6.0):
		return _fail("the step off F's platform onto TP-340 stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("wet_plate")
	if not bool(_native().is_player_grounded()) or _position().y < 340.25 or \
			int(_native().get_support_entity_id()) != 1005:
		return _fail("not standing on TP-340 (y %.2f, support %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# AS-008, the Plate Shop, the way its native band test runs it: from TP-340,
# G's rope off its cleat and hooked on its platform's eye, its prop pin
# drawn, a ride to the 374 ring; the girder's tail pin carried out, over the
# gangway onto H, its chock yanked, a ride to the 418 ring; across onto I's
# cage, its rope hooked on, the domino's pin drawn, the cascade's ride to
# 462 m; up the ladder onto the 484 ring.
func _shop(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _shop_g(device):
		return false
	var at_g := _position().y
	if not await _shop_h(device):
		return false
	var at_h := _position().y
	if not await _shop_i(device):
		return false
	# The hands are watched on the machines' ropes, pins and lanyards, as
	# _wet and _stack watch them; a ladder's top-out is traversal, whose
	# planted hands the climb scenarios prove.
	var worst_wrist := _stop_watch(wrists)
	if not await _shop_ladder(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "shop_g_y=%.2f shop_h_y=%.2f ring484_y=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		at_g, at_h, _position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step,
		float(body["lift"])]
	return true


func _shop_state() -> Dictionary:
	return _native().get_shop_state()


func _shop_g(device: int) -> bool:
	# From TP-340 by F's hole, west past the header, to the south of G's cleat.
	if not (await _go(device, Vector2(11.3, -138.0), 0.15, 6.0) and \
			await _go(device, Vector2(1.0, -141.0), 0.15, 16.0) and \
			await _go(device, Vector2(-5.0, -153.5), 0.15, 12.0) and \
			await _go(device, Vector2(-9.7, -152.9), 0.08, 8.0)):
		return _fail("the walk to G's cleat stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s %s' facing G's cleat, not UNHOOK" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2074, 0.5):
		return _fail("UNHOOK did not put G's rope shackle in the hands")
	# Round the shaft's posts onto the platform, the rope in hand.
	if not (await _go(device, Vector2(-8.5, -152.9), 0.15, 4.0) and \
			await _go(device, Vector2(-8.5, -150.1), 0.15, 4.0) and \
			await _go(device, Vector2(-11.2, -150.0), 0.08, 6.0)):
		return _fail("carrying G's rope onto its platform stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO PLATFORM EYE"):
		return _fail("Action read '%s %s' at G's platform eye, not HOOK ONTO PLATFORM EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_shop_state()["g_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put G's rope on the platform's eye")
	if not await _pull_handle(device, Vector2(-11.3, -149.25), Vector2(0.0, 1.0), 2073, "LANYARD",
			func() -> bool: return not bool(_shop_state()["g_tower_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_shop_state()["g_platform_travel"]) >= 33.7, 60.0)
	if not arrived:
		return _fail("G's platform never reached the 374 ring (travel %.2f m)" %
			float(_shop_state()["g_platform_travel"]))
	await _seconds(1.0)
	await _pose("shop_g_top")
	if int(_native().get_support_entity_id()) != 2070:
		return _fail("the rider is not on G's platform at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


func _shop_h(device: int) -> bool:
	# Off G onto the 374 ring, round its west band to the north band and the
	# girder's tail pin.
	if not (await _go(device, Vector2(-14.5, -149.3), 0.15, 8.0) and \
			await _go(device, Vector2(-14.5, -134.3), 0.15, 16.0) and \
			await _go(device, Vector2(-2.8, -134.0), 0.15, 16.0) and \
			await _walk_to(device, Vector2(-3.6, -134.0), 0.08, 4.0)):
		return _fail("the walk round the 374 ring to the girder's tail pin stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"pick_up", "PICK UP", "TAIL PIN"):
		return _fail("Action read '%s %s' facing the girder's tail pin, not PICK UP TAIL PIN" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2084, 0.5):
		return _fail("PICK UP did not lift the girder's tail pin")
	# Back a metre with it, out of its socket, and down.
	if not await _walk_to(device, Vector2(-2.6, -134.0), 0.15, 4.0):
		return _fail("stepping back with the tail pin stalled at %s" % str(_position()))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the tail pin down")
	await _seconds(1.0)
	if bool(_shop_state()["h_girder_latched"]):
		return _fail("the tail pin out, the girder is still latched")
	# Round the ring to the gangway and over it onto H's platform.
	if not (await _go(device, Vector2(-14.5, -134.3), 0.15, 16.0) and \
			await _go(device, Vector2(-14.5, -145.6), 0.15, 16.0) and \
			await _go(device, Vector2(-9.6, -145.6), 0.15, 10.0)):
		return _fail("the walk over the gangway onto H stalled at %s" % str(_position()))
	if not await _pull_handle(device, Vector2(-9.4, -144.9), Vector2(0.0, 1.0), 2086, "LANYARD",
			func() -> bool: return not bool(_shop_state()["h_trolley_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_shop_state()["h_platform_travel"]) >= 43.9, 90.0)
	if not arrived:
		return _fail("H's platform never reached the 418 ring (travel %.2f m)" %
			float(_shop_state()["h_platform_travel"]))
	await _seconds(1.0)
	await _pose("shop_h_top")
	if int(_native().get_support_entity_id()) != 2080:
		return _fail("the rider is not on H's platform at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


func _shop_i(device: int) -> bool:
	# Across onto I's cage; the rope's shackle hangs a metre west of the eye.
	if not await _walk_to(device, Vector2(-7.3, -145.6), 0.08, 6.0):
		return _fail("the step across onto I's cage stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "TAKE", "ROPE SHACKLE"):
		return _fail("Action read '%s %s' facing I's rope shackle, not TAKE ROPE SHACKLE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2096, 0.5):
		return _fail("TAKE did not put I's rope shackle in the hands")
	if not await _walk_to(device, Vector2(-6.3, -145.6), 0.08, 4.0):
		return _fail("the step to I's cage eye stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at I's cage eye, not HOOK ONTO CAGE EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_shop_state()["i_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put I's rope on the cage's eye")
	if not await _pull_handle(device, Vector2(-6.5, -144.9), Vector2(0.0, 1.0), 2095, "LANYARD",
			func() -> bool: return not bool(_shop_state()["i_domino_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_shop_state()["i_cage_travel"]) >= 43.9, 90.0)
	if not arrived:
		var i := _shop_state()
		return _fail("the cascade never hauled I's cage to 462 m (travel %.2f m, domino %.2f rad, trip %.2f rad, monolith %.2f rad)" % [
			float(i["i_cage_travel"]), float(i["i_domino_angle"]), float(i["i_trip_angle"]),
			float(i["i_monolith_angle"])])
	await _seconds(1.0)
	await _pose("shop_i_top")
	if int(_native().get_support_entity_id()) != 2090:
		return _fail("the rider is not in I's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# Up the ladder from I's cage onto the 484 ring.
func _shop_ladder(device: int) -> bool:
	if not await _walk_to(device, Vector2(-7.6, -145.6), 0.08, 6.0):
		return _fail("the step to the ladder in I's cage stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s %s' facing the ladder from I's cage, not CLIMB HOLD" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the ladder from I's cage")
	_move(device, 1.0)
	var on_ring: bool = await _wait_until(func() -> bool: return _standing_above(484.5), 60.0)
	_move(device, 0.0)
	if not on_ring:
		return _fail("climbing the ladder did not top out onto the 484 ring (y %.2f)" % _position().y)
	await _seconds(0.5)
	await _pose("shop_ring484")
	return true


# AS-009, the Facade Crane Stack, the way its native band test runs it: from
# the 484 ring, J's rail joint carried onto its traveler and laid in the
# cradle, the wagon's chock pulled, a ride to the 528 ring; round the ring to
# K's cage, its rope hooked on, the jib's pendant pin drawn, a ride to the 572
# ring; round to L's cab, the winch's clutch thrown in and the drop weight's
# pin drawn, a ride into TP-640; off onto the plate.
func _crane(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _crane_j(device):
		return false
	var at_j := _position().y
	if not await _crane_k(device):
		return false
	var at_k := _position().y
	if not await _crane_l(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "crane_j_y=%.2f crane_k_y=%.2f tp640_y=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		at_j, at_k, _position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step,
		float(body["lift"])]
	return true


func _crane_state() -> Dictionary:
	return _native().get_crane_state()


func _crane_j(device: int) -> bool:
	# Off the ladder from I's cage, round to the 484 ring's north band.
	var here := _position()
	if here.x < -8.0 and not await _go(device, Vector2(here.x, -140.3), 0.15, 12.0):
		return _fail("the walk round to the 484 ring's north band stalled at %s" % str(_position()))
	if not (await _go(device, Vector2(-3.0, -140.3), 0.15, 12.0) and \
			await _go(device, Vector2(2.8, -139.0), 0.08, 12.0)):
		return _fail("the walk to J's rail joint stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "PICK UP", "RAIL JOINT"):
		return _fail("Action read '%s %s' facing J's rail joint, not PICK UP RAIL JOINT" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2102, 0.5):
		return _fail("PICK UP did not lift J's rail joint")
	# Onto J's traveler and to the cradle, the joint in hand.
	if not (await _go(device, Vector2(-0.3, -138.3), 0.15, 8.0) and \
			await _go(device, Vector2(-0.3, -136.2), 0.08, 6.0) and \
			await _go(device, Vector2(0.9, -136.2), 0.08, 6.0)):
		return _fail("carrying J's rail joint to its cradle stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	# Steadied before it is set down, as a player does with a swinging load.
	await _seconds(0.6)
	var joint := int(_native().get_kit_body_index(2102))
	var joint_last := (_native().get_kit_body_transform(joint) as Transform3D).origin
	for frame in 180:
		await get_tree().process_frame
		var joint_now := (_native().get_kit_body_transform(joint) as Transform3D).origin
		var swing := (joint_now - joint_last).length() / get_process_delta_time()
		joint_last = joint_now
		if swing < 0.05:
			break
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set J's rail joint down")
	await _seconds(1.5)
	if not bool(_crane_state()["j_rail_whole"]):
		var rest := (_native().get_kit_body_transform(joint) as Transform3D).origin
		return _fail("set down, J's rail joint did not close the rail's gap (it lies at %s, its seat (1.6, 485.0, -136.2); the rider at %s)" % [
			str(rest), str(_position())])
	if not await _pull_handle(device, Vector2(0.0, -135.5), Vector2(0.0, 1.0), 2104, "LANYARD",
			func() -> bool: return not bool(_crane_state()["j_wagon_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_crane_state()["j_traveler_travel"]) >= 43.9, 60.0)
	if not arrived:
		return _fail("J's traveler never reached the 528 ring (travel %.2f m)" %
			float(_crane_state()["j_traveler_travel"]))
	await _seconds(1.0)
	await _pose("crane_j_top")
	if int(_native().get_support_entity_id()) != 2100:
		return _fail("the rider is not on J's traveler at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


func _crane_k(device: int) -> bool:
	# Off J round the 528 ring and over the board to K's cage.
	if not (await _go(device, Vector2(0.0, -138.6), 0.15, 8.0) and \
			await _go(device, Vector2(0.0, -141.5), 0.15, 8.0) and \
			await _go(device, Vector2(-8.5, -141.5), 0.15, 12.0) and \
			await _go(device, Vector2(-8.5, -155.0), 0.15, 16.0) and \
			await _go(device, Vector2(-11.6, -155.0), 0.15, 8.0)):
		return _fail("the walk round the 528 ring to K's cage stalled at %s" % str(_position()))
	# To the cage's north side, along it clear of the shackle on its long
	# rope, and round behind it.
	if not (await _go(device, Vector2(_position().x, -154.2), 0.15, 6.0) and \
			await _go(device, Vector2(-13.3, -154.2), 0.15, 10.0) and \
			await _go(device, Vector2(-13.3, -155.0), 0.08, 4.0)):
		return _fail("the walk round K's shackle stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "TAKE", "ROPE SHACKLE"):
		return _fail("Action read '%s %s' facing K's rope shackle, not TAKE ROPE SHACKLE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2114, 0.5):
		return _fail("TAKE did not put K's rope shackle in the hands")
	if not await _walk_to(device, Vector2(-12.3, -155.0), 0.08, 4.0):
		return _fail("the step to K's cage eye stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at K's cage eye, not HOOK ONTO CAGE EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_crane_state()["k_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put K's rope on the cage's eye")
	if not await _pull_handle(device, Vector2(-12.6, -154.3), Vector2(0.0, 1.0), 2113, "LANYARD",
			func() -> bool: return not bool(_crane_state()["k_jib_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_crane_state()["k_cage_travel"]) >= 43.9, 90.0)
	if not arrived:
		return _fail("K's cage never reached the 572 ring (travel %.2f m)" %
			float(_crane_state()["k_cage_travel"]))
	await _seconds(1.0)
	await _pose("crane_k_top")
	if int(_native().get_support_entity_id()) != 2110:
		return _fail("the rider is not in K's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


func _crane_l(device: int) -> bool:
	# Over the board and round the 572 ring to L's cab.
	if not (await _go(device, Vector2(-11.9, -155.0), 0.15, 6.0) and \
			await _go(device, Vector2(-6.5, -155.0), 0.15, 10.0) and \
			await _go(device, Vector2(-6.5, -156.7), 0.15, 6.0) and \
			await _go(device, Vector2(-3.0, -156.7), 0.15, 8.0) and \
			await _go(device, Vector2(-3.0, -159.6), 0.15, 6.0)):
		return _fail("the walk round the 572 ring into L's cab stalled at %s" % str(_position()))
	if not await _pull_handle(device, Vector2(-3.7, -159.4), Vector2(0.0, 1.0), 2127, "CLUTCH HANDLE",
			func() -> bool: return bool(_crane_state()["l_clutch_in"])):
		return false
	if not await _pull_handle(device, Vector2(-3.0, -160.8), Vector2(0.0, -1.0), 2125, "LANYARD",
			func() -> bool: return not bool(_crane_state()["l_weight_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_crane_state()["l_cab_travel"]) >= 67.9, 90.0)
	if not arrived:
		var l := _crane_state()
		return _fail("L's cab never reached TP-640 (travel %.2f m, clutch in %s, weight latched %s, cart latched %s)" % [
			float(l["l_cab_travel"]), str(l["l_clutch_in"]), str(l["l_weight_latched"]), str(l["l_cart_latched"])])
	await _seconds(1.0)
	if not await _go(device, Vector2(-3.0, -156.5), 0.15, 8.0):
		return _fail("the step off L's cab onto TP-640 stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("crane_plate")
	if not _standing_above(640.2):
		return _fail("not standing on TP-640 (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# AS-010, Midstack Service, its first stage the way its native band test
# runs it: from TP-640 into the service cage, the rope's shackle hooked on
# the cage's eye and the reel's chock pulled by its lanyard; the reel falls
# paying out its cable and hauls the cage to the 662 deck, where its dogs
# hold it; off onto the deck.
func _service(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _service_m(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck662_y=%.2f held_m=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(_service_state()["m_cage_travel"]),
		float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step, float(body["lift"])]
	return true


func _service_state() -> Dictionary:
	return _native().get_service_state()


func _sling() -> Dictionary:
	return _native().get_slingshot_state()


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
	if not await _wait_until(func() -> bool: return bool(_sling()["flight"]), 3.0):
		return _fail("RELEASE did not throw the rider off the bands")
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
	while waited < 20.0 and not bool(_native().is_player_grounded()):
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
	await _seconds(0.5)
	await _pose("sling_landed")
	var at_end := _position()
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died on the way (apex %.1f m)" % apex)
	if not bool(_native().is_player_grounded()) or at_end.y < 221.0 or at_end.y > 221.3 or \
			at_end.z < -130.73 or at_end.z > -126.73:
		return _fail("the rider came down at %s, not on the 220 ring (apex %.1f m)" % [str(at_end), apex])
	_detail = "ring220_y=%.2f draw_m=%.2f energy_kJ=%.0f apex=%.1f seconds=%.1f" % [at_end.y, draw_m, energy_kj,
		apex, float(int(_native().get_tick_index()) - started) / 90.0]
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


func _service_m(device: int) -> bool:
	# From L's hole north across TP-640 and west into the service cage, round
	# the shackle hanging over its middle.
	if not (await _go(device, Vector2(-3.0, -144.3), 0.15, 12.0) and \
			await _go(device, Vector2(-8.7, -144.3), 0.15, 8.0) and \
			await _go(device, Vector2(-8.7, -143.5), 0.08, 4.0)):
		return _fail("the walk into the service cage stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "TAKE", "ROPE SHACKLE"):
		return _fail("Action read '%s %s' facing M's rope shackle, not TAKE ROPE SHACKLE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2134, 0.5):
		return _fail("TAKE did not put M's rope shackle in the hands")
	if not await _walk_to(device, Vector2(-7.7, -143.5), 0.08, 4.0):
		return _fail("the step to M's cage eye stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at M's cage eye, not HOOK ONTO CAGE EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_service_state()["m_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put M's rope on the cage's eye")
	if not await _pull_handle(device, Vector2(-8.0, -142.8), Vector2(0.0, 1.0), 2133, "LANYARD",
			func() -> bool: return not bool(_service_state()["m_reel_latched"])):
		return false
	# The ride, until the cage stands still on its dogs for a second.
	var last := float(_service_state()["m_cage_travel"])
	var still := 0.0
	var waited := 0.0
	while waited < 40.0 and still < 1.0:
		await get_tree().process_frame
		var dt := get_process_delta_time()
		waited += dt
		var travel := float(_service_state()["m_cage_travel"])
		still = still + dt if travel > 1.0 and absf(travel - last) < 0.0005 else 0.0
		last = travel
	var held := float(_service_state()["m_cage_travel"])
	if still < 1.0 or held < 21.05:
		return _fail("M's cage never came to rest on its dogs at the 662 deck (travel %.2f m, reel %.2f m)" % [
			held, float(_service_state()["m_reel_travel"])])
	await _pose("service_m_top")
	if int(_native().get_support_entity_id()) != 2130:
		return _fail("the rider is not in M's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	# Off east onto the 662 deck.
	if not await _go(device, Vector2(-5.8, -143.5), 0.15, 6.0):
		return _fail("the step off M's cage onto the 662 deck stalled at %s (on %d, crouched %s, grounded %s, v %s, cage %.3f m)" % [
			str(_position()), int(_native().get_support_entity_id()), str(_native().is_player_crouched()),
			str(_ctx()["grounded"]), str(_velocity()), float(_service_state()["m_cage_travel"])])
	await _seconds(0.5)
	await _pose("service_deck662")
	if not _standing_above(662.5):
		return _fail("not standing on the 662 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# AS-010's stage N alone, from the 684 deck start, on the device under test.
func _n(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _service_n(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck706_y=%.2f held_m=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(_service_state()["n_cage_travel"]),
		float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step, float(body["lift"])]
	return true


# AS-010's stage N, the granular discharge hoist, the way its native test
# takes it: from the 684 deck into the cage beside the flap's chain, GRAB it:
# it comes down to the hands and draws the flap's arm past its dead point,
# and the flap falls open. LET GO; the bin pours into the hopper, which sinks
# and hauls the cage to the 706 deck, where its dogs hold it; off east onto
# the deck.
func _service_n(device: int) -> bool:
	if not (await _go(device, Vector2(-5.6, -155.35), 0.15, 20.0) and \
			await _go(device, Vector2(-6.6, -155.35), 0.06, 6.0)):
		return _fail("N: the walk into the cage stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	# Look up at the chain hanging over the head, as a player does.
	await _tilt(0.35)
	if not await _offered(&"pick_up", "GRAB", "FLAP CHAIN"):
		return _fail("Action read '%s %s' beside N's chain, not GRAB FLAP CHAIN" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("service_n_chain")
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2138, 0.5):
		return _fail("GRAB did not put N's flap chain in the hands")
	await _tilt(0.0)
	if not await _wait_until(func() -> bool: return float(_service_state()["n_gate_angle"]) >= 0.3, 4.0):
		return _fail("holding the chain did not throw N's flap open (flap %.2f rad)" %
			float(_service_state()["n_gate_angle"]))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take N's chain out of the hands")
	await _pose("service_n_pour")
	# The ride, until the cage stands still on its dogs for a second.
	var last := float(_service_state()["n_cage_travel"])
	var still := 0.0
	var waited := 0.0
	while waited < 40.0 and still < 1.0:
		await get_tree().process_frame
		var dt := get_process_delta_time()
		waited += dt
		var travel := float(_service_state()["n_cage_travel"])
		still = still + dt if travel > 1.0 and absf(travel - last) < 0.0005 else 0.0
		last = travel
	var held := float(_service_state()["n_cage_travel"])
	if still < 1.0 or held < 21.45:
		return _fail("N's cage never came to rest on its dogs at the 706 deck (travel %.2f m, hopper %.0f kg)" % [
			held, float(_service_state()["n_hopper_kg"])])
	await _pose("service_n_top")
	if int(_native().get_support_entity_id()) != 2135:
		return _fail("the rider is not in N's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	# Off east onto the 706 deck.
	if not await _go(device, Vector2(-3.8, -155.0), 0.15, 6.0):
		return _fail("the step off N's cage onto the 706 deck stalled at %s (on %d, cage %.3f m)" % [
			str(_position()), int(_native().get_support_entity_id()), float(_service_state()["n_cage_travel"])])
	await _seconds(0.5)
	await _pose("service_deck706")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(706.6):
		return _fail("not standing on the 706 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# AS-010's stage O alone, from the 728 deck start, on the device under test.
func _o(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _service_o(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck750_y=%.2f held_m=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(_service_state()["o_cab_travel"]),
		float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step, float(body["lift"])]
	return true


# AS-010's stage O, the gravel wheel, the way its native test takes it: from
# the 728 deck into the cab beside the chock's lanyard, GRAB it and step back
# with it until the chock lets the ram go, and LET GO. The ram swings onto the
# latch, the gate falls open and the gravel pours into the bucket, which sinks
# down its well and hauls the cab, by the rope over the wheel, to the 750
# deck, where its dogs hold it; off east onto the deck.
func _service_o(device: int) -> bool:
	if not (await _go(device, Vector2(8.0, -150.3), 0.15, 20.0) and \
			await _go(device, Vector2(5.2, -150.4), 0.15, 6.0)):
		return _fail("O: the walk to the cab stalled at %s" % str(_position()))
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _pull_handle(device, Vector2(3.3, -150.6), Vector2(0.0, -1.0), 2146, "CHOCK LANYARD",
			func() -> bool: return not bool(_service_state()["o_ram_latched"])):
		return false
	await _pose("service_o_pour")
	# The ride, until the cab stands still on its dogs for a second.
	var last := float(_service_state()["o_cab_travel"])
	var still := 0.0
	var waited := 0.0
	while waited < 60.0 and still < 1.0:
		await get_tree().process_frame
		var dt := get_process_delta_time()
		waited += dt
		var travel := float(_service_state()["o_cab_travel"])
		still = still + dt if travel > 1.0 and absf(travel - last) < 0.0005 else 0.0
		last = travel
	var held := float(_service_state()["o_cab_travel"])
	if still < 1.0 or held < 21.45:
		return _fail("O's cab never came to rest on its dogs at the 750 deck (travel %.2f m, bucket %.0f kg)" % [
			held, float(_service_state()["o_bucket_kg"])])
	await _pose("service_o_top")
	if int(_native().get_support_entity_id()) != 2139:
		return _fail("the rider is not in O's cab at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	# Off east onto the 750 deck.
	if not await _go(device, Vector2(5.2, -150.0), 0.15, 6.0):
		return _fail("the step off O's cab onto the 750 deck stalled at %s (on %d, cab %.3f m)" % [
			str(_position()), int(_native().get_support_entity_id()), float(_service_state()["o_cab_travel"])])
	await _seconds(0.5)
	await _pose("service_deck750")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(750.6):
		return _fail("not standing on the 750 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# AS-010's C5 alone, from the 706 deck start, on the device under test.
func _c5(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not await _service_c5(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck728_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true


# AS-010's C5, the cooling plant, the way its native climb takes it: from the
# 706 deck over the manifold in a vault, under the duct bank crouched, onto
# the tank, up its standpipe onto the platform over it, west along that at a
# sprint and over the 6.5 m gap, a hang up onto the platform north of the
# landing and one east of that, up the second standpipe, and over the 728
# deck's north girder.
func _service_c5(device: int) -> bool:
	if not (await _go(device, Vector2(-3.3, -146.0), 0.1, 12.0) and \
			await _go(device, Vector2(-0.85, -145.5), 0.06, 6.0)):
		return _fail("C5: the walk to the manifold stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("C5: Action read '%s %s' facing the manifold, not CLIMB" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 3, 0.3):
		return _fail("C5: Action did not vault the manifold (state %d)" % int(_native().get_traversal_state()))
	if not await _wait_until(func() -> bool: return _standing_above(706.6) and _position().x > 0.9, 2.0):
		return _fail("C5: the vault over the manifold did not land (at %s)" % str(_position()))
	# Under the duct bank, crouched, and up again past it.
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return bool(_native().is_player_crouched()), 0.3):
		return _fail("C5: CROUCH did not crouch the body before the duct bank")
	if not await _go(device, Vector2(5.6, -145.5), 0.08, 8.0):
		return _fail("C5: the crawl under the duct bank stalled at %s" % str(_position()))
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return not bool(_native().is_player_crouched()), 0.5):
		return _fail("C5: STAND did not stand the body up past the duct bank")
	# Onto the tank and up its standpipe onto the platform over it.
	if not await _go(device, Vector2(8.5, -146.5), 0.06, 4.0):
		return _fail("C5: the walk to the tank stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(1.0, 0.0), 708.2, "the tank"):
		return false
	if not await _c5_pipe(device, Vector2(10.25, -144.8), Vector2(0.0, 1.0), 714.5, "the tank's standpipe"):
		return false
	# West along the platform at a sprint, and over the gap.
	if not await _go(device, Vector2(10.85, -142.6), 0.08, 4.0):
		return _fail("C5: the walk to the run-up stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	await _seconds(0.2)
	if not _stick_sprint():
		_move(device, 0.0)
		return _fail("C5: the stick pushed past its ring did not latch sprint")
	if not await _wait_until(func() -> bool: return _position().x <= 8.45, 2.0):
		_move(device, 0.0)
		return _fail("C5: the run-up stalled at %s" % str(_position()))
	if not bool(_ctx()["sprinting"]):
		_move(device, 0.0)
		return _fail("C5: not sprinting at the gap's edge (%.2f m/s)" % _walk_speed().length())
	_jump(device)
	var across: bool = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]) and \
		_position().y > 714.5 and _position().x < 1.5, 3.0)
	_move(device, 0.0)
	if not across:
		return _fail("C5: the jump over the gap did not land on the far platform (at %s)" % str(_position()))
	# A hang up onto the platform north of the landing, and one east of that.
	if not await _go(device, Vector2(-0.5, -141.6), 0.05, 6.0):
		return _fail("C5: the walk to the first platform's face stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, 1.0), 717.7, "the first platform"):
		return false
	if not await _go(device, Vector2(2.55, -139.6), 0.05, 6.0):
		return _fail("C5: the walk to the second platform's face stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(1.0, 0.0), 720.9, "the second platform"):
		return false
	# Up the second standpipe, and over the 728 deck's north girder.
	if not await _c5_pipe(device, Vector2(6.35, -139.63), Vector2(1.0, 0.0), 727.0, "the second standpipe"):
		return false
	if not await _go(device, Vector2(9.0, -141.75), 0.05, 6.0):
		return _fail("C5: the walk to the 728 deck's girder stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), 728.6, "the 728 deck"):
		return false
	await _seconds(0.5)
	await _pose("service_deck728")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(728.8):
		return _fail("C5: not standing on the 728 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


# Steps to `at` beside a standpipe, turns to `facing` it, takes it on CLIMB
# HOLD and climbs until standing on what it tops out onto, above `top`.
func _c5_pipe(device: int, at: Vector2, facing: Vector2, top: float, what: String) -> bool:
	if not await _go(device, at, 0.06, 6.0):
		return _fail("C5: the step to %s stalled at %s" % [what, str(_position())])
	await _face(facing)
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("C5: Action read '%s %s' at %s, not CLIMB HOLD" % [_action_label(),
			String(_ctx()["action"]["detail"]), what])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("C5: CLIMB did not take %s" % what)
	_move(device, 1.0)
	var topped: bool = await _wait_until(func() -> bool: return _standing_above(top), 20.0)
	_move(device, 0.0)
	if not topped:
		return _fail("C5: %s never topped out (at %s)" % [what, str(_position())])
	return true


# AS-010's C4 alone, from the 662 deck start, on the device under test.
func _c4(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not await _service_c4(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck684_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true


# AS-010's C4, the service gantry, the way its native climb takes it: from
# the 662 deck onto the switchgear cabinet, a hang up onto the duct, east
# along it and over the beam to the pump deck, up the standpipe onto the
# hoist runway, over its edge into a hang and along its lip past the winch
# house, across the 3 m gap, a hang up onto the gallery, onto the riser, a
# hang up onto the hoist platform, and over the 684 deck's yellow girder.
func _service_c4(device: int) -> bool:
	if not await _go(device, Vector2(6.2, -149.1), 0.08, 20.0):
		return _fail("C4: the walk to the cabinet stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), 664.3, "the cabinet"):
		return false
	if not await _walk_to(device, Vector2(6.2, -150.9), 0.05, 2.0):
		return _fail("C4: the step to the cabinet's back edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), 667.5, "the duct"):
		return false
	# East along the duct and over the beam, held on its line.
	if not await _go(device, Vector2(10.0, -152.3), 0.1, 6.0):
		return _fail("C4: the walk along the duct stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _walk_to(device, Vector2(10.0, -153.9), 0.1, 3.0) or not bool(_ctx()["balancing"]):
		return _fail("C4: not balancing on the beam at %s" % str(_position()))
	if not await _walk_to(device, Vector2(10.0, -158.0), 0.1, 6.0) or not _standing_above(667.5):
		return _fail("C4: the beam walk stalled at %s" % str(_position()))
	# Up the standpipe onto the runway.
	if not await _go(device, Vector2(9.5, -158.7), 0.08, 4.0):
		return _fail("C4: the step to the standpipe stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("C4: Action read '%s %s' at the standpipe, not CLIMB HOLD" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("C4: CLIMB did not take the standpipe")
	_move(device, 1.0)
	var on_runway: bool = await _wait_until(func() -> bool: return _standing_above(675.3), 20.0)
	_move(device, 0.0)
	if not on_runway:
		return _fail("C4: the standpipe never topped out onto the runway (at %s)" % str(_position()))
	# Over the runway's edge into a hang, and along its lip past the winch house.
	if not await _go(device, Vector2(7.8, -159.9), 0.08, 4.0):
		return _fail("C4: the walk to the runway's edge stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _wait_until(func() -> bool: return bool(_ctx()["drop_ok"]) and \
			(device != InputRouter.Device.TOUCH or _main._touch.is_button_shown(&"drop")), 1.5):
		return _fail("C4: DROP was not offered with the runway's edge behind (at %s)" % str(_position()))
	_drop(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 1.5):
		return _fail("C4: DROP did not lower the body into a hang on the runway's lip")
	_move_world(device, Vector2(-1.0, 0.0))
	var past: bool = await _wait_until(func() -> bool: return _position().x <= 4.4, 9.0)
	_move(device, 0.0)
	if not past:
		return _fail("C4: the hang stopped along the runway's lip at %s" % str(_position()))
	if not await _climb_up(device):
		return _fail("C4: CLIMB UP not offered hanging west of the winch house")
	if not await _wait_until(func() -> bool: return _standing_above(675.3), 2.5):
		return _fail("C4: the climb up west of the winch house did not land (at %s)" % str(_position()))
	# Across the gap: a run along +z off the runway's west end.
	if not await _go(device, Vector2(2.5, -161.0), 0.1, 6.0):
		return _fail("C4: the walk to the run-up stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	_move(device, 1.0)
	if not await _wait_until(func() -> bool: return _position().z >= -159.9, 2.0):
		_move(device, 0.0)
		return _fail("C4: the run-up stalled at %s" % str(_position()))
	_jump(device)
	var landed: bool = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]) and \
		_position().y > 675.0 and _position().z > -156.4, 2.0)
	_move(device, 0.0)
	if not landed:
		return _fail("C4: the jump across the gap did not land on the landing (at %s)" % str(_position()))
	if not await _walk_to(device, Vector2(2.5, -153.25), 0.05, 4.0):
		return _fail("C4: the step to the landing's edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, 1.0), 678.5, "the gallery"):
		return false
	if not await _go(device, Vector2(0.9, -151.7), 0.08, 6.0):
		return _fail("C4: the walk along the gallery stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(-1.0, 0.0), 680.0, "the riser"):
		return false
	if not await _go(device, Vector2(-0.8, -152.2), 0.05, 3.0):
		return _fail("C4: the step to the riser's edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), 683.2, "the hoist platform"):
		return false
	if not await _go(device, Vector2(-1.6, -153.7), 0.08, 4.0):
		return _fail("C4: the walk to the 684 deck's girder stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(-1.0, 0.0), 684.8, "the 684 deck"):
		return false
	if not await _go(device, Vector2(-4.0, -153.7), 0.1, 4.0):
		return _fail("C4: the walk onto the 684 deck stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("service_deck684")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(684.8):
		return _fail("C4: not standing on the 684 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true


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


func _stack_s2(device: int) -> bool:
	if not await _go(device, Vector2(10.0, -130.5), 0.15, 15.0) or \
			not await _walk_to(device, Vector2(10.0, -137.6), 0.10, 15.0):
		return _fail("walk across crossover gangway into S2 cage stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing S2 handle, not GRAB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2214, 0.5):
		return _fail("GRAB did not take S2 handle")
	_move(device, 0.4)
	var s2_tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_stack_s2_chock_latched()), 2.0)
	_move(device, 0.0)
	if not s2_tripped:
		return _fail("pulling S2 handle did not trip chock")
	var s2_arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s2_cage_travel()) >= 21.8, 15.0)
	if not s2_arrived:
		return _fail("S2 cage never reached Deck 6 (travel %.2f m)" % float(_native().get_stack_s2_cage_travel()))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not release S2 handle")
	await _face(Vector2(0.0, 1.0))
	if not (await _walk_to(device, Vector2(10.0, -133.0), 0.15, 6.0) and \
			await _walk_to(device, Vector2(10.0, -126.0), 0.15, 6.0)):
		return _fail("walk off S2 onto Deck 6 stalled at %s" % str(_position()))
	await _seconds(0.3)
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != TOWER_ENTITY or \
			_position().y < 66.5:
		return _fail("not standing on Deck 6 (y %.2f)" % _position().y)
	await _pose("stack_deck6")
	return true


func _stack_c2(device: int) -> bool:
	if not await _go(device, Vector2(22.0, -125.5), 0.15, 20.0) or \
			not await _go(device, Vector2(22.0, -135.7), 0.08, 15.0):
		return _fail("walk to C2 switchgear cabinet stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing C2 cabinet, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(68.5), 2.0):
		return _fail("CLIMB did not mantle onto C2 cabinet (y %.2f)" % _position().y)
	if not await _walk_to(device, Vector2(22.0, -138.6), 0.05, 3.0):
		return _fail("step to cabinet north edge stalled")
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	var c2_hung: bool = await _leap(device, 0.4,
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 2.0)
	if not c2_hung:
		return _fail("jump from cabinet did not hang from C2 duct (at %s)" % str(_position()))
	if not await _climb_up(device):
		return _fail("CLIMB UP not offered hanging from C2 duct")
	if not await _wait_until(func() -> bool: return _standing_above(72.0), 2.5):
		return _fail("CLIMB UP did not bring climber onto C2 duct")
	if not (await _go(device, Vector2(22.0, -142.0), 0.1, 6.0) and \
			await _go(device, Vector2(22.0, -148.05), 0.06, 6.0)):
		return _fail("walk along C2 duct stalled")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing C2 wall ladder, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of C2 wall ladder")
	_move(device, 1.0)
	var on_deck7: bool = await _wait_until(func() -> bool: return _standing_above(77.5), 20.0)
	_move(device, 0.0)
	if not on_deck7:
		return _fail("climbing C2 wall ladder did not reach Deck 7 (y %.2f)" % _position().y)
	if not (await _go(device, Vector2(22.0, -155.0), 0.15, 10.0) and \
			await _go(device, Vector2(20.0, -160.20), 0.08, 10.0)):
		return _fail("walk across Deck 7 to pipe rack stalled")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing pipe rack, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(79.5), 2.0):
		return _fail("mantle onto pipe rack failed (y %.2f)" % _position().y)
	if not (await _go(device, Vector2(18.0, -162.0), 0.1, 6.0) and \
			await _go(device, Vector2(10.0, -162.0), 0.08, 15.0)):
		return _fail("walk along monorail beam stalled")
	await _seconds(0.3)
	if _position().y < 82.5:
		return _fail("not standing at monorail beam end (y %.2f)" % _position().y)
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	_jump(device)
	_move(device, 0.4)
	var c2_ladder_caught: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 2.0)
	if not c2_ladder_caught:
		_move(device, 0.0)
		return _fail("leap from monorail did not catch davit ladder")
	_move(device, 1.0)
	var on_c2_arm: bool = await _wait_until(func() -> bool: return _standing_above(88.5), 20.0)
	_move(device, 0.0)
	if not on_c2_arm:
		return _fail("up davit ladder did not top out onto arm (y %.2f)" % _position().y)
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(10.0, -165.0), 0.1, 6.0) and \
			await _walk_to(device, Vector2(10.0, -171.2), 0.1, 6.0)):
		return _fail("walk off davit arm onto Deck 8 stalled")
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or (int(_native().get_support_entity_id()) != TOWER_ENTITY and int(_native().get_support_entity_id()) != 1022) or \
			_position().y < 88.5:
		return _fail("not standing on Deck 8 (y %.2f)" % _position().y)
	await _pose("stack_deck8")
	return true


func _stack_s3(device: int) -> bool:
	if not (await _go(device, Vector2(-8.0, -170.0), 0.15, 20.0) and \
			await _go(device, Vector2(-8.0, -164.0), 0.1, 10.0) and \
			await _go(device, Vector2(-8.0, -157.6), 0.08, 10.0)):
		return _fail("the walk to S3 cage stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing S3 handle, not GRAB (at %s)" % [_action_label(), str(_position())])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2223, 0.5):
		return _fail("GRAB did not take S3 handle")
	_move(device, 0.4)
	var s3_tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_stack_s3_brake_latched()), 2.0)
	_move(device, 0.0)
	if not s3_tripped:
		return _fail("pulling S3 handle did not trip brake")
	var s3_arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s3_cage_travel()) >= 43.8, 25.0)
	if not s3_arrived:
		return _fail("S3 cage never reached Deck 12 (travel %.2f m)" % float(_native().get_stack_s3_cage_travel()))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not release S3 handle")
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(-8.0, -164.0), 0.1, 6.0) and \
			await _walk_to(device, Vector2(-8.0, -171.2), 0.1, 6.0)):
		return _fail("the walk off S3 onto Deck 12 stalled")
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or (int(_native().get_support_entity_id()) != TOWER_ENTITY and int(_native().get_support_entity_id()) != 1018) or \
			_position().y < 132.5:
		return _fail("not standing on Deck 12 (y %.2f)" % _position().y)
	await _pose("stack_deck12")
	return true


func _stack_c3(device: int) -> bool:
	if not (await _go(device, Vector2(-10.5, -171.0), 0.1, 10.0) and \
			await _go(device, Vector2(-10.5, -145.0), 0.1, 20.0)):
		return _fail("walk along atrium girder stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing atrium duct, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(138.0), 2.0):
		return _fail("mantle onto atrium duct failed (y %.2f)" % _position().y)
	if not await _go(device, Vector2(-10.5, -135.85), 0.08, 6.0):
		return _fail("walk to C3 wall ladder stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing C3 wall ladder, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("take hold of C3 wall ladder failed")
	_move(device, 1.0)
	var on_deck13: bool = await _wait_until(func() -> bool: return _standing_above(143.5), 20.0)
	_move(device, 0.0)
	if not on_deck13:
		return _fail("ladder to Deck 13 failed (y %.2f)" % _position().y)
	if not await _go(device, Vector2(-6.0, -133.85), 0.08, 6.0):
		return _fail("walk along Deck 13 plate stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing high riser ladder, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("take hold of high riser ladder failed")
	_move(device, 1.0)
	var on_crossover: bool = await _wait_until(func() -> bool: return _standing_above(154.5), 30.0)
	_move(device, 0.0)
	if not on_crossover:
		return _fail("high riser ladder to Deck 14 failed (y %.2f)" % _position().y)
	if not (await _walk_to(device, Vector2(-6.0, -130.0), 0.1, 4.0) and \
			await _walk_to(device, Vector2(-6.0, -128.2), 0.1, 4.0) and \
			await _walk_to(device, Vector2(-10.5, -128.2), 0.1, 6.0)):
		return _fail("walk down crossover steps onto Deck 14 runway stalled")
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != TOWER_ENTITY or \
			_position().y < 154.5:
		return _fail("not standing on Deck 14 (y %.2f)" % _position().y)
	await _pose("stack_deck14")
	return true


func _stack_upper(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var body := _watch_body()
	var started := int(_native().get_tick_index())
	var at_deck4 := _position().y
	if at_deck4 < 44.0:
		return _fail("upper stack scenario did not start at Deck 4 (y %.2f)" % at_deck4)
	if not await _stack_s2(device):
		return false
	var at_deck6 := _position().y
	if not await _stack_c2(device):
		return false
	var at_deck8 := _position().y
	if not await _stack_s3(device):
		return false
	var at_deck12 := _position().y
	if not await _stack_c3(device):
		return false
	var at_deck14 := _position().y
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck4_y=%.2f deck6_y=%.2f deck8_y=%.2f deck12_y=%.2f deck14_y=%.2f seconds=%.1f worst_body_step_m=%.3f" % [
		at_deck4, at_deck6, at_deck8, at_deck12, at_deck14, float(int(_native().get_tick_index()) - started) / 90.0, worst_step]
	return true


func _checkpoint_continuation(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var initial_deaths := int(_native().get_death_count())
	var at0 := _position()
	if at0.y < 44.0:
		return _fail("checkpoint scenario did not start at Deck 4 (y %.2f)" % at0.y)
	await _walk_to(device, Vector2(10.0, -125.5), 0.1, 4.0)
	await _seconds(0.5)
	var cp_pos: Vector3 = _native().get_checkpoint_position()
	if cp_pos.y < 44.0:
		return _fail("committed checkpoint altitude too low (cp_y %.2f)" % cp_pos.y)
	await _face(Vector2(0.0, 1.0))
	_move(device, 1.0)
	var died: bool = await _wait_until(
		func() -> bool: return int(_native().get_death_count()) > initial_deaths, 7.0)
	_move(device, 0.0)
	if not died:
		return _fail("fall off deck was not fatal")
	await _seconds(0.5)
	var after_restore: Vector3 = _position()
	if after_restore.y < 44.0 or not bool(_native().is_player_grounded()):
		return _fail("checkpoint restore failed to return to deck (y %.2f, grounded %s)" % [
			after_restore.y, str(_native().is_player_grounded())])
	if absf(after_restore.y - cp_pos.y) > 0.5:
		return _fail("restored y %.2f != checkpoint y %.2f" % [after_restore.y, cp_pos.y])
	if not await _stack_s2(device):
		return false
	_detail = "restored_y=%.2f continued_deck6_y=%.2f deaths=%d" % [
		cp_pos.y, _position().y, int(_native().get_death_count())]
	return true


# The Stack from the game's own start at grade to the 220 m ring, on the
# device under test. Across the yard into S1's cage and onto the scale plate
# in its floor, nothing pressed: the rider's weight works the valve, the
# water runs into the bucket until it outweighs the cage and the rider, and
# the cage carries them 21.8 m to deck 2. Off over the gangway, and up C1 to
# deck 4. Then S2's walking-beam cage to deck 6, the C2 climb to deck 8, S3's
# counterweight carriage to deck 12, the C3 climb to deck 14 (154 m), and the
# well's stages A, B and C to the 220 m ring. Every verb is the one the HUD
# offers at that moment, pressed on the device; every leg is proven by the
# native state it changed.
func _stack(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var body := _watch_body()
	var started := int(_native().get_tick_index())
	# S1, the water-balance hoist.
	if not await _go(device, Vector2(10.0, -117.3), 0.3, 40.0):
		return _fail("the walk across the yard to S1 stalled at %s" % str(_position()))
	if not await _go(device, Vector2(10.0, -120.8), 0.1, 6.0):
		return _fail("the step into S1's cage and onto its plate stalled at %s" % str(_position()))
	var opened: bool = await _wait_until(func() -> bool:
		return bool(_native().is_stack_s1_valve_pawled()) and \
			not bool(_native().is_stack_s1_catch_latched()), 1.5)
	if not opened:
		return _fail("standing on S1's plate did not pawl its valve open (lever %.2f rad, on %d)" % [
			float(_native().get_stack_s1_valve_angle()), int(_native().get_support_entity_id())])
	var counting: bool = await _wait_until(func() -> bool:
		return _action_label() == "FILLING" and \
			String(_ctx()["action"]["detail"]).begins_with("BUCKET") and \
			float(_native().get_stack_s1_bucket_water_kg()) > 120.0, 2.0)
	if not counting:
		return _fail("on S1's plate, the HUD read '%s %s', not the bucket filling" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("stack_filling")
	var lifted: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s1_cage_travel()) > 3.0, 8.0)
	if not lifted:
		return _fail("on its plate, S1's cage never left the yard (bucket %.0f kg)" %
			float(_native().get_stack_s1_bucket_water_kg()))
	await _pose("stack_ride")
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s1_cage_travel()) >= 21.79, 20.0)
	if not arrived:
		return _fail("S1's cage never reached deck 2 (travel %.2f m)" %
			float(_native().get_stack_s1_cage_travel()))
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(10.0, -123.2), 0.2) and \
			await _walk_to(device, Vector2(10.0, -126.5), 0.2)):
		return _fail("the walk off S1 over the gangway stalled at %s" % str(_position()))
	await _seconds(0.3)
	if int(_native().get_support_entity_id()) != TOWER_ENTITY or _position().y < 22.5:
		return _fail("off S1, not standing on deck 2 (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	var at_deck2 := _position().y
	await _pose("stack_deck2")
	# C1, the facade: out onto the landing, round the cabinet, up onto it.
	if not await _go(device, Vector2(21.2, -124.4), 0.15, 20.0) or \
			not await _go(device, Vector2(21.2, -121.3), 0.15, 6.0) or \
			not await _go(device, Vector2(20.0, -121.3), 0.08, 6.0):
		return _fail("the walk to C1's cabinet stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing the cabinet, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(24.5), 2.0):
		return _fail("CLIMB did not mantle onto the cabinet (y %.2f)" % _position().y)
	# A jump from the cabinet to hang from the duct's lip, and up onto it.
	if not await _walk_to(device, Vector2(20.0, -122.10), 0.05):
		return _fail("the step to the cabinet's north edge stalled at %s" % str(_position()))
	await _seconds(0.3)
	_jump(device)
	_move(device, 0.4)
	var hung: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 2.0)
	_move(device, 0.0)
	if not hung:
		return _fail("the jump from the cabinet did not hang from the duct (at %s)" % str(_position()))
	await _pose("stack_hang")
	if not await _climb_up(device):
		return _fail("CLIMB UP was not offered hanging from the duct (Action '%s', touch jump '%s')" % [
			_action_label(), _main._touch.button_label(&"jump")])
	if not await _wait_until(func() -> bool: return _standing_above(28.0), 2.5):
		return _fail("CLIMB UP did not bring the climber up onto the duct (y %.2f)" % _position().y)
	# Along the duct to the vent stack, and up it over deck 3's edge.
	if not await _go(device, Vector2(22.5, -123.05), 0.1, 6.0) or \
			not await _go(device, Vector2(24.0, -123.0), 0.06, 6.0):
		return _fail("the walk along the duct stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s %s' facing the vent stack, not CLIMB HOLD" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the vent stack")
	_move(device, 1.0)
	var on_deck3: bool = await _wait_until(func() -> bool: return _standing_above(33.5), 20.0)
	_move(device, 0.0)
	if not on_deck3:
		return _fail("climbing the vent stack did not top out onto deck 3 (y %.2f)" % _position().y)
	# Along deck 3 and out along the monorail under the davit's ladder.
	if not await _go(device, Vector2(22.0, -125.2), 0.15, 6.0) or \
			not await _go(device, Vector2(12.5, -125.2), 0.08, 12.0) or \
			not await _go(device, Vector2(12.5, -119.65), 0.06, 12.0):
		return _fail("the walk out along the monorail stalled at %s" % str(_position()))
	await _seconds(0.3)
	if not bool(_native().is_player_grounded()) or _position().y < 34.0:
		return _fail("not standing at the monorail's end (y %.2f)" % _position().y)
	# Turn to the ladder and leap for it; up it onto the davit's arm.
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	await _pose("stack_monorail")
	_jump(device)
	_move(device, 0.5)
	var caught: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 2.0)
	if not caught:
		_move(device, 0.0)
		return _fail("the leap from the monorail did not catch the davit's ladder (at %s)" % str(_position()))
	_move(device, 1.0)
	var on_arm: bool = await _wait_until(func() -> bool: return _standing_above(44.5), 20.0)
	_move(device, 0.0)
	if not on_arm:
		return _fail("up the ladder did not top out onto the davit's arm (y %.2f)" % _position().y)
	await _face(Vector2(0.0, 1.0))
	if not await _walk_to(device, Vector2(12.5, -125.0), 0.1, 10.0) or \
			not await _go(device, Vector2(11.2, -125.3), 0.1, 4.0):
		return _fail("the walk back along the arm onto deck 4 stalled at %s" % str(_position()))
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != TOWER_ENTITY or \
			_position().y < 44.5:
		return _fail("not standing on deck 4 (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	await _pose("stack_deck4")
	var at_deck4 := _position().y

	# Continue upward through the full Stack sequence to Deck 14, then the AS-006 well to Ring 220.
	if not await _stack_s2(device):
		return false
	var at_deck6 := _position().y

	if not await _stack_c2(device):
		return false
	var at_deck8 := _position().y

	if not await _stack_s3(device):
		return false
	var at_deck12 := _position().y

	if not await _stack_c3(device):
		return false
	var at_deck14 := _position().y

	if not await _well_stage_a(device):
		return false
	var at_ring176 := _position().y

	if not await _well_stage_b(device):
		return false
	var at_ring198 := _position().y

	if not await _well_stage_c(device):
		return false
	var at_ring220 := _position().y

	# Along the 220 ring to AS-007, and up its three machines to TP-340.
	if not (await _go(device, Vector2(2.5, -129.6), 0.15, 8.0) and \
			await _go(device, Vector2(4.0, -129.2), 0.15, 6.0)):
		return _fail("the walk along the 220 ring to AS-007 stalled at %s" % str(_position()))
	if not await _wet_d(device):
		return false
	var at_wet_d := _position().y
	if not await _wet_e(device):
		return false
	var at_wet_e := _position().y
	if not await _wet_f(device):
		return false
	var at_tp340 := _position().y

	# West across TP-340 to AS-008, up its three machines and the ladder to
	# the 484 ring.
	if not await _shop_g(device):
		return false
	var at_shop_g := _position().y
	if not await _shop_h(device):
		return false
	var at_shop_h := _position().y
	if not await _shop_i(device):
		return false
	if not await _shop_ladder(device):
		return false
	var at_ring484 := _position().y

	# Round the 484 ring to AS-009, up its three machines into TP-640.
	if not await _crane_j(device):
		return false
	var at_crane_j := _position().y
	if not await _crane_k(device):
		return false
	var at_crane_k := _position().y
	if not await _crane_l(device):
		return false
	var at_tp640 := _position().y

	# AS-010's service lift from TP-640 to the 662 deck.
	if not await _service_m(device):
		return false
	var at_deck662 := _position().y

	# AS-010's C4, the service gantry, to the 684 deck, stage N, the granular
	# discharge hoist, to the 706 deck, C5, the cooling plant, to the 728 deck,
	# and stage O, the gravel wheel, to the 750 deck: on touch, the device the game is played on (the owner, 2026-09-24:
	# no keyboard or gamepad needed); the pad and keyboard runs end on the 662
	# deck.
	var reached := ""
	if device == InputRouter.Device.TOUCH:
		if not await _service_c4(device):
			return false
		reached = " deck684_y=%.2f" % _position().y
		# AS-010's stage N, the granular discharge hoist, to the 706 deck.
		if not await _service_n(device):
			return false
		reached += " deck706_y=%.2f" % _position().y
		# AS-010's C5, the cooling plant, to the 728 deck.
		if not await _service_c5(device):
			return false
		reached += " deck728_y=%.2f" % _position().y
		# AS-010's stage O, the gravel wheel, to the 750 deck.
		if not await _service_o(device):
			return false
		reached += " deck750_y=%.2f" % _position().y

	# A frame here is one or two native ticks: 0.25 m is over 11 m/s sideways,
	# faster than a sprint; only a snap moves the view that far.
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step,
			str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck2_y=%.2f deck4_y=%.2f deck6_y=%.2f deck8_y=%.2f deck12_y=%.2f deck14_y=%.2f ring176_y=%.2f ring198_y=%.2f ring220_y=%.2f wet_d_y=%.2f wet_e_y=%.2f tp340_y=%.2f shop_g_y=%.2f shop_h_y=%.2f ring484_y=%.2f crane_j_y=%.2f crane_k_y=%.2f tp640_y=%.2f deck662_y=%.2f%s seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		at_deck2, at_deck4, at_deck6, at_deck8, at_deck12, at_deck14, at_ring176, at_ring198, at_ring220,
		at_wet_d, at_wet_e, at_tp340, at_shop_g, at_shop_h, at_ring484, at_crane_j, at_crane_k, at_tp640, at_deck662,
		reached, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true


func _standing_above(y: float) -> bool:
	return int(_native().get_traversal_state()) == TRAVERSAL_NONE and bool(_native().is_player_grounded()) and \
		_position().y > y


# Turns to face `target` and walks to it, the way a player crosses open ground;
# on the keyboard the mouse keeps the view on it along the way.
func _go(device: int, target: Vector2, tolerance: float, budget: float) -> bool:
	var at := _position()
	var to := target - Vector2(at.x, at.z)
	if to.length() > tolerance:
		await _face(to)
	return await _walk_to(device, target, tolerance, budget, device == InputRouter.Device.KEYBOARD_MOUSE)


# Presses what reads CLIMB UP on the device under test while hanging: on
# touch the jump button relabels to it; on a pad or keyboard it is Action.
func _climb_up(device: int) -> bool:
	if device == InputRouter.Device.TOUCH:
		var relabelled: bool = await _wait_until(
			func() -> bool: return _main._touch.button_label(&"jump") == "CLIMB UP", 1.5)
		if not relabelled:
			return false
		_tap(2, _center(&"jump"))
		return true
	if not await _offered(&"climb_up", "CLIMB UP"):
		return false
	_act(device)
	return true


# One press of Jump on the device under test.
# Jumps with forward held at `amount` of full throw until `caught` holds or
# `budget` seconds pass. A key has no throw: held down it drives the leap at
# full speed, so on the keyboard W is feathered instead, down for `amount` of
# every tenth of a second, the way a player taps it to keep a leap short.
func _leap(device: int, amount: float, caught: Callable, budget: float) -> bool:
	_jump(device)
	if device != InputRouter.Device.KEYBOARD_MOUSE:
		_move(device, amount)
		var held: bool = await _wait_until(caught, budget)
		_move(device, 0.0)
		return held
	var waited := 0.0
	while waited < budget:
		if caught.call():
			_move(device, 0.0)
			return true
		_move(device, 1.0 if fmod(waited, 0.1) < amount * 0.1 else 0.0)
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move(device, 0.0)
	return bool(caught.call())


func _jump(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			_tap(2, _center(&"jump"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_A)
		_:
			_key(KEY_SPACE, true)
			_key(KEY_SPACE, false)


# One press of Drop on the device under test: the touch button, pad B, Q.
func _drop(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			_tap(1, _center(&"drop"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_B)
		_:
			_key(KEY_Q, true)
			_key(KEY_Q, false)


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


# Tilts the view to `pitch` the way a thumb or a stick does, at up to 2 rad/s.
func _tilt(pitch: float) -> void:
	while absf(pitch - float(_main._pitch)) > 1.0e-3:
		var step := 2.0 * get_process_delta_time()
		_main._pitch = move_toward(float(_main._pitch), pitch, step)
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


# Holds the move input toward world (x, z) `w`, at its length of full throw,
# without turning the view; a key, having no throw, is pressed full.
func _move_world(device: int, w: Vector2) -> void:
	var yaw := float(_main._yaw)
	var v := Vector2(w.dot(Vector2(cos(yaw), -sin(yaw))), w.dot(Vector2(-sin(yaw), -cos(yaw))))
	_move_dir(device, v.normalized() if device == InputRouter.Device.KEYBOARD_MOUSE else v)


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
# steps into place: the stick pushed toward it relative to the view, easing
# off near it, and stopping inside `tolerance`. A key has no throw: it is
# held until the walker could no longer stop short of the point (a frame
# late, at KEY_BRAKE_MPS2), then let go, and pressed again if it stops
# short; on the keys the walk ends only once the walker has stopped there.
# `steer` turns the view toward a point more than a metre off, as a mouse
# holds a keyboard walk to its line.
func _walk_to(device: int, target: Vector2, tolerance: float, budget: float = 6.0, steer := false) -> bool:
	var keys := device == InputRouter.Device.KEYBOARD_MOUSE
	var waited := 0.0
	while waited < budget:
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		var speed := _walk_speed()
		if to.length() <= tolerance and (not keys or speed.length() < 0.3):
			_move_dir(device, Vector2.ZERO)
			await _seconds(0.2)
			return true
		if steer and to.length() > 1.0:
			var turn := 4.0 * get_process_delta_time()
			_main._yaw = float(_main._yaw) + clampf(wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI), -turn, turn)
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		var v := Vector2(to.dot(right), to.dot(forward)).normalized() * clampf(to.length() / 0.8, 0.25, 1.0)
		if keys:
			var closing := maxf(speed.dot(to.normalized()), 0.0)
			var brake := KEY_CARRY_BRAKE_MPS2 if int(_native().get_carrying_entity_id()) != 0 else KEY_BRAKE_MPS2
			var stopping := closing * (get_process_delta_time() + closing / (2.0 * brake))
			v = Vector2.ZERO if to.length() <= tolerance or stopping >= to.length() - 0.5 * tolerance else v.normalized()
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
					watch["at"] = "%s %s traversal=%d at=%s" % [str(_main._arms.hand_poses()), _action_label(),
						int(_native().get_traversal_state()), str(_position())]
		last = now
		await get_tree().process_frame


# Samples the body's position every frame until stopped, and reports the
# largest single-frame sideways move: a body the native walks, climbs and
# mantles never jumps; a snap across a blocked path does. It also keeps, as
# "lift", the largest single-frame rise or fall of the eye beyond the body's
# own continuous motion: a native step lifts the body in one tick, and the
# view must glide over it, never show it in one frame.
func _watch_body() -> Dictionary:
	var watch := {"running": true, "worst": 0.0, "lift": 0.0}
	_sample_body(watch)
	return watch


func _sample_body(watch: Dictionary) -> void:
	var last := _position()
	var last_eye := (_main._camera as Camera3D).global_transform.origin.y
	var last_body := _render_soles_y()
	var last_steps := float(_native().get_step_up_meters())
	while bool(watch["running"]):
		await get_tree().process_frame
		var now := _position()
		var step := Vector2(now.x - last.x, now.z - last.z).length()
		if step > float(watch["worst"]):
			watch["worst"] = step
			watch["at"] = "traversal=%d from %s to %s" % [int(_native().get_traversal_state()), str(last),
				str(now)]
		last = now
		var eye := (_main._camera as Camera3D).global_transform.origin.y
		var body := _render_soles_y()
		var steps := float(_native().get_step_up_meters())
		var lift := absf((eye - last_eye) - (body - last_body - (steps - last_steps)))
		if lift > float(watch["lift"]):
			watch["lift"] = lift
			watch["lift_at"] = "stepped %.3f m at %s" % [steps - last_steps, str(now)]
		last_eye = eye
		last_body = body
		last_steps = steps


# The rendered body's soles, which the eye hangs from (main.gd): a crouch
# swaps the native's capsule round them in one tick, its centre moving 0.3 m
# while the soles stay put and the eye glides.
func _render_soles_y() -> float:
	return (_native().get_player_render_position() as Vector3).y - \
		(0.6 if bool(_native().is_player_crouched()) else 0.9)


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


# The fear voice as a player hears it: from the high drop to the lethal
# impact, captured on the Voice bus and on the Ambience bus (the altitude
# wind, the yard, the falling-air rush). While a line runs the voice must
# stand 6 dB clear of the bed it ducks, and the mix leaving Master must not
# clip.
func _audio_fall() -> bool:
	if AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty():
		return _fail("no mixing audio driver; run under --write-movie")
	var captures: Array[AudioEffectCapture] = []
	for bus in [&"Voice", &"Ambience", &"Master"]:
		var capture := AudioEffectCapture.new()
		capture.buffer_length = 1.0
		AudioServer.add_bus_effect(AudioServer.get_bus_index(bus), capture)
		captures.append(capture)
	var ready: bool = await _wait_until(func() -> bool: return bool(_main._audio._bank_ready), 5.0)
	if not ready:
		return _fail("the sound bank never finished building")
	var voice := PackedFloat32Array()
	var air := PackedFloat32Array()
	var master := PackedFloat32Array()
	var waited := 0.0
	while int(_native().get_death_count()) < 1 and waited < 12.0:
		await get_tree().process_frame
		waited += get_process_delta_time()
		for pair in [[captures[0], voice], [captures[1], air], [captures[2], master]]:
			var capture: AudioEffectCapture = pair[0]
			var available := capture.get_frames_available()
			if available > 0:
				for frame in capture.get_buffer(available):
					pair[1].append((frame.x + frame.y) * 0.5)
	_main._audio.quiesce()
	await _frames(3)
	if int(_native().get_death_count()) < 1:
		return _fail("the high drop never ended")
	# 50 ms windows where the voice is speaking (within 20 dB of its loudest).
	var window := int(AudioServer.get_mix_rate() * 0.05)
	var count := mini(voice.size(), air.size()) / window
	var voice_db: Array[float] = []
	var air_db: Array[float] = []
	var loudest := -200.0
	for k in count:
		loudest = maxf(loudest, _window_db(voice, k * window, window))
	for k in count:
		var v := _window_db(voice, k * window, window)
		if v > loudest - 20.0 and v > -60.0:
			voice_db.append(v)
			air_db.append(_window_db(air, k * window, window))
	var peak := 0.0
	for x in master:
		peak = maxf(peak, absf(x))
	var peak_db := linear_to_db(maxf(peak, 1.0e-9))
	if voice_db.is_empty():
		return _fail("nothing was heard on the Voice bus in a %.1f s fall" % waited)
	voice_db.sort()
	air_db.sort()
	var voice_mid := voice_db[voice_db.size() / 2]
	var air_mid := air_db[air_db.size() / 2]
	_detail = "voice_db=%.1f ambience_db=%.1f speaking_s=%.2f peak_db=%.1f voice=%s" % [voice_mid,
		air_mid, voice_db.size() * 0.05, peak_db, str(_main._audio.voice_lines)]
	if voice_db.size() * 0.05 < 1.0:
		return _fail("the voice spoke for only %.2f s of the fall" % (voice_db.size() * 0.05))
	if voice_mid < air_mid + 6.0:
		return _fail("the voice (%.1f dBFS) is lost in the ambience (%.1f)" % [voice_mid, air_mid])
	if peak_db > -0.5:
		return _fail("the mix clips: sample peak %.2f dBFS" % peak_db)
	return true


func _window_db(mono: PackedFloat32Array, first: int, count: int) -> float:
	var energy := 0.0
	for i in range(first, mini(first + count, mono.size())):
		energy += mono[i] * mono[i]
	return linear_to_db(maxf(sqrt(energy / float(maxi(count, 1))), 1.0e-9))


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


# The walker's horizontal velocity over what its feet stand on.
func _walk_speed() -> Vector2:
	var v := _velocity()
	if bool(_native().is_player_grounded()):
		v -= _native().get_support_point_linear_velocity()
	return Vector2(v.x, v.z)


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


# Pushes the left stick straight forward on past its ring, the way a thumb
# latches sprint, and holds it there at full throw; _move(device, 0.0) lets
# go. True when the touch layer reads sprint latched.
func _stick_sprint() -> bool:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u)
	_touch(0, home, true)
	# Four thirds of the throw: past SPRINT_OVERSHOOT's 1.25.
	for i in 4:
		_drag(0, home + Vector2(0.0, -throw) * float(i + 1) / 3.0, Vector2(0.0, -throw / 3.0))
	return bool(_main._touch.sprint_latched)


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
