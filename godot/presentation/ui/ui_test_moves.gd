extends "res://presentation/ui/ui_test_base.gd"

# UI scenarios: core movement, the yard's life, the hook, the chute, the
# pause menu, and the pad and keyboard core paths. (Part of the
# ui_test_driver.gd chain: extends the helpers, extended by the next family.)

const VoiceLinesTable := preload("res://presentation/audio/voice_lines.gd")

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

# PARKOUR_MOVE_SET.md M3 on the same rail: a short walk up to it, the eyes
# down on its top (the HUD lays its ring there), JUMP from a stand: the legs
# take off at the one speed that puts the feet on the rail, and the landing
# counts as prepared.
func _touch_precision() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_stick_push(Vector2(0.0, -1.0))
	var walked: bool = await _wait_until(func() -> bool: return _position().x > 1.9, 2.0)
	_touch(0, _main._touch.stick_home(), false)
	if not walked:
		return _fail("never walked up to the rail (x %.2f)" % _position().x)
	await _wait_until(func() -> bool: return _walk_speed().length() < 0.1, 1.5)
	# The eyes come down until the rail's near half is the sighted landing.
	var sighted := false
	var pitch := -0.15
	while pitch > -0.65 and not sighted:
		_main._pitch = pitch
		await _frames(3)
		if bool(_ctx()["precision"]):
			var point: Vector3 = _ctx()["precision_point"]
			sighted = point.x > 4.6 and point.x < 5.1 and point.y > 0.9
		pitch -= 0.03
	if not sighted:
		return _fail("the rail's top was never the sighted landing")
	await _pose("precision_sight")
	_tap(1, _center(&"jump"))
	var took_off: bool = await _wait_until(func() -> bool: return int(_native().get_precision_jump_count()) >= 1, 0.3)
	if not took_off:
		return _fail("JUMP did not take the sighted landing")
	var down: bool = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not down:
		return _fail("never landed")
	await _frames(6)
	_detail = "landed_x=%.2f y=%.2f prepared=%d" % [_position().x, _position().y, int(_native().get_precision_landing_count())]
	return int(_native().get_precision_landing_count()) >= 1 and _position().x > 4.5 and _position().x < 5.3 \
		and _position().y > 1.6

# The same rail at a sprint (the stick past its ring): ACTION is offered well
# short of the rail, and the vault goes over at the run's speed and leaves at it.
func _touch_speed_vault() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var offered: bool = await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"climb", 2.5)
	if not offered:
		return _fail("ACTION never offered the rail at a sprint")
	var entry := _velocity().x
	var offered_x := _position().x
	_tap(1, _center(&"action"))
	var vaulting: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 3, 0.2)
	if not vaulting:
		return _fail("ACTION did not commit a vault")
	await _pose("speed_vault")
	var over: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 0, 1.0)
	if not over:
		return _fail("the speed vault never finished")
	var exit_speed := _velocity().x
	_detail = "entry_mps=%.2f offered_at_x=%.2f exit_mps=%.2f" % [entry, offered_x, exit_speed]
	return entry > 7.0 and exit_speed > 7.0

# Parkour step 4 on the proving run wall (its face at z = -26.75, x = -12..0):
# a sprint along it, JUMP beside it, the stick held forward: the feet take
# steps on the wall as the body comes down.
func _touch_wall_run() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var at_wall: bool = await _wait_until(func() -> bool: return _position().x > -12.5, 3.0)
	if not at_wall:
		return _fail("the sprint never reached the wall (x %.2f)" % _position().x)
	_tap(1, _center(&"jump"))
	var stepped: bool = await _wait_until(func() -> bool: return int(_native().get_wall_step_count()) >= 2, 1.5)
	if not stepped:
		return _fail("no steps on the wall (%d)" % int(_native().get_wall_step_count()))
	await _pose("wall_run")
	var down: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0)
	if not down:
		return _fail("the wall run never came down")
	_detail = "steps=%d landed_x=%.2f" % [int(_native().get_wall_step_count()), _position().x]
	return true

# Mirror's Edge's wall climb: from the static deck to the run-up square to
# the proving ground's 4 m run wall, a sprint at it (the stick pushed past
# its ring) and JUMP 1.5 m short: the planted foot runs the body up it, the
# hands take its top, CLIMB UP stands on it.
func _touch_wall_climb() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _go(InputRouter.Device.TOUCH, Vector2(-6.0, -19.0), 0.15, 20.0):
		return _fail("the walk to the run-up")
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var near: bool = await _wait_until(func() -> bool: return _position().z < -25.2, 3.0)
	if not near:
		return _fail("the sprint never reached the wall (z %.2f)" % _position().z)
	_tap(2, _center(&"jump"))
	var climbed: bool = await _wait_until(func() -> bool: return int(_native().get_wall_climb_count()) >= 1, 1.0)
	if not climbed:
		return _fail("the jump at the wall did not run up it")
	await _pose("wall_climb")
	var hung: bool = await _wait_until(func() -> bool: return bool(_ctx()["hanging"]), 2.0)
	_move(InputRouter.Device.TOUCH, 0.0)
	if not hung:
		return _fail("the hands did not take the wall's top (y %.2f)" % _position().y)
	if not await _climb_up(InputRouter.Device.TOUCH):
		return _fail("CLIMB UP was not offered on the wall's top")
	var up: bool = await _wait_until(func() -> bool:
		return bool(_native().is_player_grounded()) and _position().y > 4.5, 3.0)
	if not up:
		return _fail("the climb never stood on the wall's top (y %.2f)" % _position().y)
	await _pose("wall_climb_top")
	_detail = "climbs=%d top_y=%.2f" % [int(_native().get_wall_climb_count()), _position().y]
	return true

# Mirror's Edge's tuck: from the static deck down the crawl beam's lane at a
# sprint, JUMP 4.4 m short of its 1.85 m beam and CROUCH as the body rises:
# the legs draw up, the body clears the beam, and lets them down on landing.
func _touch_tuck() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _go(InputRouter.Device.TOUCH, Vector2(-2.0, -5.0), 0.15, 20.0):
		return _fail("the walk to the crawl beam's lane")
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var near: bool = await _wait_until(func() -> bool: return _position().z < -8.6, 3.0)
	if not near:
		return _fail("the sprint never reached the take-off (z %.2f)" % _position().z)
	_tap(2, _center(&"jump"))
	var up: bool = await _wait_until(func() -> bool: return not bool(_native().is_player_grounded()), 0.4)
	if not up:
		return _fail("JUMP did not leave the ground")
	_tap(3, _center(&"crouch"))
	var tucked: bool = await _wait_until(func() -> bool: return bool(_native().is_player_tucked()), 0.4)
	if not tucked:
		return _fail("CROUCH in the jump did not draw the legs up")
	await _pose("tuck")
	var over: bool = await _wait_until(func() -> bool: return _position().z < -14.0, 2.0)
	if not over:
		return _fail("the tucked jump did not clear the beam (z %.2f)" % _position().z)
	var standing: bool = await _wait_until(func() -> bool:
		return bool(_native().is_player_grounded()) and not bool(_ctx()["crouched"]), 2.0)
	_move(InputRouter.Device.TOUCH, 0.0)
	if not standing:
		return _fail("landed, the legs did not let down (still crouched)")
	_detail = "landed_z=%.2f" % _position().z
	return true

# Flow (sim kFlowSpeedMps): on the open ground east of the start, the stick
# pushed past its ring and held: 8 m/s at once, gathering to 9 over a couple
# of seconds of clean running, the view widening with it; JUMP carries all
# of it through the air.
func _touch_flow() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	await _face(Vector2(1.0, 0.0))
	await _seconds(0.3)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	await _seconds(0.75)
	var early := _run_speed()
	var early_fov: float = _main._camera.fov
	await _seconds(1.9)
	var gathered := _run_speed()
	var gathered_fov: float = _main._camera.fov
	await _pose("flow")
	_tap(2, _center(&"jump"))
	var up: bool = await _wait_until(func() -> bool: return not bool(_native().is_player_grounded()), 0.4)
	if not up:
		_move(InputRouter.Device.TOUCH, 0.0)
		return _fail("JUMP did not leave the ground")
	var least := 99.0
	var flown := 0.0
	while not bool(_native().is_player_grounded()) and flown < 2.0:
		least = minf(least, _run_speed())
		await get_tree().process_frame
		flown += get_process_delta_time()
	_move(InputRouter.Device.TOUCH, 0.0)
	_detail = "early_mps=%.2f gathered_mps=%.2f in_air_mps=%.2f fov=%.1f->%.1f" % [
		early, gathered, least, early_fov, gathered_fov]
	if early > 8.4:
		return _fail("the sprint was at flow's speed at once (%.2f m/s)" % early)
	if gathered < 8.9:
		return _fail("a sprint kept up did not gather to flow's speed (%.2f m/s)" % gathered)
	if least < 8.85:
		return _fail("the jump did not keep the run's speed (%.2f m/s)" % least)
	if bool(_main._speed_fov_on) and gathered_fov < early_fov + 1.5:
		return _fail("the view did not widen with the run's flow (%.1f -> %.1f)" % [early_fov, gathered_fov])
	return true

# The quickturn: standing, TURN turns the view half round in about a fifth of
# a second, smoothly, and the native faces the new way; running east with
# the stick held forward, TURN turns the run about: the feet brake it and
# run it back west.
func _touch_quickturn() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	await _face(Vector2(1.0, 0.0))
	await _seconds(0.3)
	var yaw0 := float(_main._yaw)
	_tap(2, _center(&"turn"))
	await _seconds(0.1)
	var midway := wrapf(yaw0 - float(_main._yaw), -PI, PI)
	await _seconds(0.3)
	var turned := wrapf(yaw0 - float(_main._yaw), -PI, PI)
	if absf(absf(turned) - PI) > 0.02:
		return _fail("TURN did not turn the view half round (%.3f rad)" % turned)
	if midway < 0.3 or midway > PI - 0.3:
		return _fail("TURN cut the view round in one frame, not a turn (%.3f rad at 0.1 s)" % midway)
	await _face(Vector2(1.0, 0.0))
	_move(InputRouter.Device.TOUCH, 1.0)
	await _seconds(1.0)
	var east := float(_native().get_player_linear_velocity().x)
	_tap(2, _center(&"turn"))
	await _seconds(0.9)
	var back := float(_native().get_player_linear_velocity().x)
	_move(InputRouter.Device.TOUCH, 0.0)
	_detail = "midway_rad=%.2f east_mps=%.2f back_mps=%.2f" % [midway, east, back]
	if east < 5.0:
		return _fail("the run east never got going (%.2f m/s)" % east)
	if back > -4.0:
		return _fail("TURN in a run did not turn it about (%.2f m/s east)" % back)
	return true

# The springboard (sim kSpringboardMinimumSpeed): round to the east side of
# the yard's plate stack (its slab on top, 1.1 m up), the stick pushed past
# its ring west at it, and one JUMP a stride short: a foot planted on the
# top, the jump from there, the soles a top's height above a ground jump's.
func _touch_springboard() -> bool:
	for target in [Vector2(30.0, 6.0), Vector2(58.0, 6.0)]:
		if not await _go(InputRouter.Device.TOUCH, target, 0.3, 30.0):
			return _fail("the walk round the yard stopped short of %s" % str(target))
	if not await _go(InputRouter.Device.TOUCH, Vector2(58.0, 12.0), 0.12, 20.0):
		return _fail("the walk to the plate stack's east side")
	await _face(Vector2(-1.0, 0.0))
	await _seconds(0.3)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var near: bool = await _wait_until(func() -> bool: return _position().x < 49.6, 3.0)
	if not near:
		_move(InputRouter.Device.TOUCH, 0.0)
		return _fail("the sprint never reached the plate stack (x %.2f)" % _position().x)
	var entry := _run_speed()
	_tap(2, _center(&"jump"))
	var planted: bool = await _wait_until(func() -> bool: return int(_native().get_springboard_count()) >= 1, 0.6)
	if not planted:
		_move(InputRouter.Device.TOUCH, 0.0)
		return _fail("JUMP a stride short of the plate stack did not springboard off it")
	var launch := _run_speed()
	var peak := 0.0
	var flown := 0.0
	var posed := false
	while flown < 2.5:
		peak = maxf(peak, _position().y - 0.9)
		if not posed and float(_native().get_player_linear_velocity().y) < 0.5:
			posed = true
			await _pose("springboard")
		if bool(_native().is_player_grounded()) and flown > 0.3:
			break
		await get_tree().process_frame
		flown += get_process_delta_time()
	_move(InputRouter.Device.TOUCH, 0.0)
	_detail = "entry_mps=%.2f launch_mps=%.2f soles_peak_m=%.2f" % [entry, launch, peak]
	if peak < 2.5:
		return _fail("the springboard did not carry the soles a top's height up (%.2f m)" % peak)
	if launch < 0.9 * entry:
		return _fail("the springboard did not carry the run on (%.2f of %.2f m/s)" % [launch, entry])
	return true

func _run_speed() -> float:
	var v: Vector3 = _native().get_player_linear_velocity()
	return Vector2(v.x, v.z).length()

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
# Parkour step 3: down the crawl beam's lane at a sprint (the stick pushed
# past its ring), CROUCH 3 m short of the 1.45 m beam: the body slides under
# it on its own momentum.
func _touch_slide() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var near: bool = await _wait_until(func() -> bool: return _position().z < -10.0, 3.0)
	if not near:
		return _fail("the sprint never reached the beam's approach (z %.2f)" % _position().z)
	var entry := Vector2(_velocity().x, _velocity().z).length()
	_tap(1, _center(&"crouch"))
	var slid: bool = await _wait_until(func() -> bool: return bool(_native().is_player_sliding()), 0.2)
	if not slid:
		return _fail("CROUCH at %.1f m/s did not slide" % entry)
	await _pose("slide")
	var under: bool = await _wait_until(func() -> bool: return _position().z < -13.4, 1.5)
	if not under:
		return _fail("the slide stopped short of the beam (z %.2f)" % _position().z)
	_detail = "entry_mps=%.2f under_mps=%.2f" % [entry, Vector2(_velocity().x, _velocity().z).length()]
	return Vector2(_velocity().x, _velocity().z).length() > 2.5

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
	var view_fault: String = _main._arms.view_fault(_main._camera)
	if view_fault != "":
		return _fail("the arms do not read on screen: " + view_fault)
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

# Parkour step 2 on the proving ground's monkey bars (six bars a metre apart,
# 3.1 m up, x = 0..5; a landing block past the last): run and JUMP catches the
# first, the stick held forward swings hand over hand to the last, DROP at the
# bottom of a forward swing flies on to the block.
# Life: a flock of pigeons stands on a real surface near the start. Faced and
# run at with the stick, it flushes before the player reaches it, rises well
# clear of where it stood, and comes down again somewhere real.
# Life L2: the yard crew. From the start, six workers are at their work on
# the grade. The player walks straight at the foreman as he sets off: he
# steps aside and watches them by, and nobody is shoved.
# Life L3: the structure at work. Beside the traction tram, which shuttles
# on its own: its beacon turns while it moves and goes dark when it stops,
# and the stack's vents breathe steam in sight of the player.
func _touch_works() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var works: Node3D = _main._works
	if works.beacon_count() == 0:
		return _fail("no machine carries a beacon")
	var lit: bool = await _wait_until(func() -> bool: return works.lit_count() >= 1, 20.0)
	if not lit:
		return _fail("no beacon lit in 20 s beside a shuttling tram")
	await _pose("works")
	var steaming: bool = await _wait_until(func() -> bool: return works.steaming_count() >= 1, 8.0)
	if not steaming:
		return _fail("no vent breathed steam in sight")
	var vent: Vector3 = works.steaming_at()[0]
	var to: Vector3 = vent - _main._camera.global_position
	_main._yaw = atan2(-to.x, -to.z)
	_main._pitch = atan2(to.y, Vector2(to.x, to.z).length())
	await _seconds(1.5)
	await _pose("steam")
	_detail = "beacons=%d lit=%d steaming=%d" % [works.beacon_count(), works.lit_count(),
		works.steaming_count()]
	return true

func _touch_workers() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var figures: Array = _main._crew.figures()
	if figures.size() != 14:
		return _fail("expected fourteen of the crew (six in the yard, three on the decks, three on the rings, two welders), saw %d" %
			figures.size())
	# Every worker stands inside the crew's draw bounds, high rings included
	# (they once covered only the yard, and the rest went undrawn).
	var bounds: AABB = _main._crew.draw_bounds()
	for figure in figures:
		if not bounds.has_point(figure[0]):
			return _fail("a worker at %s is outside the crew's draw bounds %s" % [str(figure[0]), str(bounds)])
	var foreman: Vector3 = figures[0][0]
	var to := foreman - _position()
	_main._yaw = atan2(-to.x, -to.z)
	_main._pitch = -0.08
	await _seconds(0.3)
	await _pose("workers")
	await _seconds(2.0)
	# Walk at him, steering the view onto him as he moves.
	_stick_push(Vector2(0.0, -0.8))
	var yielded: bool = await _wait_until(func() -> bool:
		var at: Vector3 = _main._crew.figures()[0][0]
		var d := at - _position()
		_main._yaw = atan2(-d.x, -d.z)
		return int(_native().get_worker_counts()["yields"]) >= 1, 10.0)
	_touch(0, _main._touch.stick_home(), false)
	if not yielded:
		return _fail("walking at the foreman never made him step aside")
	await _seconds(0.6)
	await _pose("workers_aside")
	if not _capture_prefix.is_empty():
		for look in [["sky_up", 0.0, 1.05], ["sky_up_east", 1.2, 0.9], ["sky_up_west", -1.4, 1.2]]:
			_main._yaw = float(look[1])
			_main._pitch = float(look[2])
			await _seconds(0.4)
			await _pose(look[0])
	await _seconds(1.5)
	var counts: Dictionary = _native().get_worker_counts()
	if int(counts["stumbles"]) != 0:
		return _fail("the foreman was shoved instead of stepping aside")
	# The crew is heard: footsteps from the walkers, strikes from the workers.
	var audio: Object = _main._audio
	if int(audio.worker_steps) == 0 or int(audio.worker_strikes) == 0:
		return _fail("the crew was silent (steps %d, strikes %d)" % [int(audio.worker_steps),
			int(audio.worker_strikes)])
	_detail = "yields=%d startles=%d steps=%d strikes=%d" % [int(counts["yields"]), int(counts["startles"]),
		int(audio.worker_steps), int(audio.worker_strikes)]
	return true

func _touch_birds() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var birds: Node3D = _main._birds
	var found: bool = await _wait_until(func() -> bool: return birds.perched_count() >= 4, 2.0)
	if not found:
		return _fail("no flock stood near the start (%d perched, probe %s, flushes %d)" % [birds.perched_count(),
			str(_native().get_surface_below(_position() + Vector3(3.0, 5.0, 0.0), 20.0)), birds.flushes])
	var flock: Vector3 = birds.nearest_perched_flock(_position())
	var to := flock - _position()
	_main._yaw = atan2(-to.x, -to.z)
	_main._pitch = -0.25
	await _pose("birds")
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var flushed: bool = await _wait_until(func() -> bool: return birds.flushes >= 1, 6.0)
	var gap := Vector2(flock.x - _position().x, flock.z - _position().z).length()
	if not flushed:
		return _fail("running at the flock never flushed it (%.1f m off)" % gap)
	if gap < 2.0:
		return _fail("the flock waited until the player was on it (%.1f m)" % gap)
	await _seconds(0.9)
	await _pose("birds_flush")
	_touch(0, _main._touch.stick_home(), false)
	var risen: float = birds.highest_above(flock)
	if risen < 2.0:
		return _fail("the flushed flock rose only %.2f m" % risen)
	var landed: bool = await _wait_until(func() -> bool: return birds.landings >= 2, 12.0)
	if not landed:
		return _fail("the flock never came down again (%d flying)" % birds.flying_count())
	_detail = "flushed_at_m=%.1f risen_m=%.1f landings=%d" % [gap, risen, birds.landings]
	return true

# Parkour step 5: the hook taken off its proving rack with ACTION, aimed with
# the view at the third monkey bar, fired with HOOK, reeled in by holding HOOK
# until the feet leave the deck, and let go with DROP.
func _touch_hook() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if _main._touch.is_button_shown(&"hook"):
		return _fail("HOOK was offered before the hook was taken")
	if _ctx()["action"]["id"] != &"hook_take":
		return _fail("ACTION offered '%s' beside the rack, expected TAKE HOOK" % str(_ctx()["action"]["id"]))
	_tap(0, _center(&"action"))
	var taken: bool = await _wait_until(func() -> bool: return bool(_native().get_hook_state()["has_hook"]), 0.5)
	if not taken:
		return _fail("TAKE HOOK did not take the hook")
	await _frames(2)
	if not _main._touch.is_button_shown(&"hook"):
		return _fail("HOOK was not offered once the hook was taken")
	if not _main._hook_gun.visible:
		return _fail("the hook's gun is not in the right hand once taken")
	await _pose("hook_gun")
	var target := Vector3(3.0, 3.1, -20.0)
	var to: Vector3 = target - _main._camera.global_position
	_main._yaw = atan2(-to.x, -to.z)
	_main._pitch = atan2(to.y, Vector2(to.x, to.z).length())
	await _frames(2)
	to = target - _main._camera.global_position
	var off := rad_to_deg((-_main._camera.global_transform.basis.z).angle_to(to))
	if off > 3.0:
		return _fail("the view did not settle on the bar (%.1f deg off)" % off)
	_tap(1, _center(&"hook"))
	var bit: bool = await _wait_until(func() -> bool: return int(_native().get_hook_state()["state"]) == 2, 1.0)
	if not bit:
		return _fail("HOOK fired at the bar never bit (state %d)" % int(_native().get_hook_state()["state"]))
	var bite: Vector3 = _native().get_hook_state()["position"]
	if bite.distance_to(target) > 0.6:
		return _fail("the hook bit at %s, not on the bar" % str(bite))
	await _frames(2)
	if int(_main._arms.hand_poses()[0]) != _main.FirstPersonArms.POSE_LINE:
		return _fail("the right hand did not close on the line (pose %d)" % int(_main._arms.hand_poses()[0]))
	if _main._touch.button_label(&"hook") != "REEL":
		return _fail("HOOK read '%s' once bitten, expected REEL" % _main._touch.button_label(&"hook"))
	var y0 := _position().y
	_touch(1, _center(&"hook"), true)
	var lifted: bool = await _wait_until(
		func() -> bool: return _position().y > y0 + 0.8 and not bool(_native().is_player_grounded()), 3.0)
	await _pose("hook_reel")
	_touch(1, _center(&"hook"), false)
	if not lifted:
		return _fail("holding HOOK never reeled the body off the deck (rise %.2f)" % (_position().y - y0))
	var risen := _position().y - y0
	if not _main._touch.is_button_shown(&"drop"):
		return _fail("DROP was not offered on the line")
	_tap(2, _center(&"drop"))
	var free: bool = await _wait_until(func() -> bool: return int(_native().get_hook_state()["state"]) == 0, 0.5)
	if not free:
		return _fail("DROP did not let go of the line")
	var down: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0)
	if not down:
		return _fail("let go, the body never came down")
	_detail = "bite=%s risen_m=%.2f" % [str(bite), risen]
	return true

# Task #102, a real rope, where the hook is found: on the 220 ring, the hook
# taken off its rack, bitten on a member over the well, and the rider walks
# south off the ring's edge on the stick. The line must catch over the
# deck's lip (a bend the native reports at the lip, drawn through it) and
# hold the body hanging below it, alive; then the reel winds the body up
# to the lip.
func _touch_hook_rope() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var device := InputRouter.Device.TOUCH
	if not await _walk_to(device, Vector2(-21.6, -127.6), 0.3, 14.0, true):
		return _fail("never reached the hook's rack (at %s)" % str(_position()))
	if _ctx()["action"]["id"] != &"hook_take":
		return _fail("ACTION offered '%s' at the rack, expected TAKE HOOK" % str(_ctx()["action"]["id"]))
	_tap(0, _center(&"action"))
	var taken: bool = await _wait_until(func() -> bool: return bool(_native().get_hook_state()["has_hook"]), 0.5)
	if not taken:
		return _fail("TAKE HOOK did not take the hook")
	var target := Vector3(-6.0, 226.0, -133.0)
	var to: Vector3 = target - _main._camera.global_position
	_main._yaw = atan2(-to.x, -to.z)
	_main._pitch = atan2(to.y, Vector2(to.x, to.z).length())
	await _frames(2)
	_tap(1, _center(&"hook"))
	var bit: bool = await _wait_until(func() -> bool: return int(_native().get_hook_state()["state"]) == 2, 1.0)
	if not bit:
		return _fail("HOOK fired over the well never bit")
	var rope: float = _native().get_hook_state()["line_length"]
	var deaths := int(_native().get_death_count())
	# Off the ring's south edge.
	_main._yaw = atan2(21.5 - 19.0, 127.6 - 125.0)
	await _walk_to(device, Vector2(-19.0, -125.0), 0.3, 4.0)
	var hanging: bool = await _wait_until(
		func() -> bool: return _position().y < 219.5 and int(_native().get_hook_state()["bends"]) >= 1, 3.0)
	if not hanging:
		return _fail("off the edge, the line never caught over the lip (y %.2f, bends %d)" % [
			_position().y, int(_native().get_hook_state()["bends"])])
	await _seconds(1.0)
	var hook: Dictionary = _native().get_hook_state()
	var points: PackedVector3Array = hook["points"]
	# The bend over the ring's top lip, among the bends: the body may have
	# swung back in under the band, the line round its underside nearer it.
	var lip := Vector3.INF
	for i in range(1, mini(int(hook["bends"]) + 1, points.size())):
		if absf(points[i].y - 220.25) <= 0.3:
			lip = points[i]
			break
	if lip == Vector3.INF:
		return _fail("the line bent at %s, never over the ring's lip" % str(points.slice(1, int(hook["bends"]) + 1)))
	if int(_native().get_death_count()) != deaths or int(hook["state"]) != 2:
		return _fail("the rope did not hold the fall")
	if not _main._hook_bend_lines[0].visible:
		return _fail("the line past the lip is not drawn")
	# Look up the line at the lip.
	var up: Vector3 = lip - _main._camera.global_position
	_main._yaw = atan2(-up.x, -up.z)
	_main._pitch = atan2(up.y, Vector2(up.x, up.z).length())
	await _frames(3)
	await _pose("hook_rope")
	var hung_y := _position().y
	_touch(1, _center(&"hook"), true)
	var wound: bool = await _wait_until(func() -> bool: return _position().y > hung_y + 1.0, 4.0)
	_touch(1, _center(&"hook"), false)
	if not wound:
		return _fail("the reel did not wind the body up toward the lip")
	_detail = "lip=%s hung_y=%.2f rope_m=%.2f" % [str(lip), hung_y, rope]
	return true

# The swing's launch (sim kSwingLaunchMps): across the monkey bars hand over
# hand, and on the last bar, rising up the front of its swing, JUMP OFF
# throws the body on as the hands open (touch_monkey_bars lets the same
# swing go with DROP and lands about x 6.6).
func _touch_swing_launch() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_stick_push(Vector2(0.0, -1.0))
	await _frames(2)
	_tap(1, _center(&"jump"))
	var caught: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 6, 1.5)
	if not caught:
		return _fail("a running JUMP under the bars never caught the first bar")
	var crossed: bool = await _wait_until(
		func() -> bool: return _native().get_traversal_left_hand().x > 4.95, 4.0)
	if not crossed:
		return _fail("held forward, the swing never carried the hands to the last bar")
	var rising: bool = await _wait_until(
		func() -> bool: return _velocity().x > 1.0 and _velocity().y > 0.8 and _position().x > 5.0, 4.0)
	if not rising:
		return _fail("the last bar's swing never came up its front")
	var release_y := _position().y
	_tap(2, _center(&"jump"))
	var thrown: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 0, 0.2)
	if not thrown or int(_native().get_swing_launch_count()) != 1:
		return _fail("JUMP OFF at the front of the swing did not throw the body on")
	var peak := release_y
	var landed := false
	var flown := 0.0
	while flown < 2.5 and not landed:
		await get_tree().process_frame
		flown += get_process_delta_time()
		peak = maxf(peak, _position().y)
		landed = bool(_native().is_player_grounded())
	_detail = "rise_m=%.2f landed_x=%.2f landed_y=%.2f" % [peak - release_y, _position().x, _position().y]
	if not landed:
		return _fail("the throw off the last bar never came down")
	if _position().x < 7.0:
		return _fail("the throw did not carry past where a let-go lands (x %.2f)" % _position().x)
	return true

# Ziplines (sim/zipline.hpp): along deck 2's south band, up the ramp onto the
# stand under the yard line's parked trolley; a standing JUMP takes its bar;
# the ride down to the yard, fast, the stick held
# back over the last stretch (the hand brake), the buffer stopping it; DROP
# lets go onto the yard. The hands on the bar, the trolley drawn over them,
# the HUD naming the line and its brake.
# Round to the foot of the yard line's stand ramp on deck 2's band, up onto
# the stand under the parked trolley, and a standing jump into its bar.
func _board_yard_zipline(device: int) -> bool:
	# Round to the foot of the stand's ramp, east of it along deck 2's band,
	# up the ramp onto the stand under the parked trolley, stopped there.
	if not (await _walk_to(device, Vector2(2.5, -127.0), 0.3, 20.0, true) and \
			await _walk_to(device, Vector2(1.6, -124.77), 0.3, 10.0, true)):
		return _fail("never reached the ramp's foot (at %s)" % str(_position()))
	await _pose("zipline_stand")
	if not await _walk_to(device, Vector2(-2.0, -124.77), 0.15, 8.0, true) or _position().y < 23.9:
		return _fail("never got up the ramp onto the stand (at %s)" % str(_position()))
	_move_dir(device, Vector2.ZERO)
	await _seconds(0.4)
	# Face down the line and jump straight up into the bar.
	await _face(Vector2(0.0, 1.0))
	_tap(1, _center(&"jump"))
	var caught: bool = await _wait_until(
		func() -> bool: return bool(_native().get_zipline_state()["riding"]), 1.5)
	if not caught:
		return _fail("the jump from the stand never took the trolley's bar (at %s)" % str(_position()))
	return true

# Ridden flat out, never braked: the stop's jolt tears the hands off the bar
# and the body goes on into the bottom post, a blow that kills (sim
# kHitLethalSpeed): the body left by the post, the hit named.
func _touch_zipline_slam() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _board_yard_zipline(InputRouter.Device.TOUCH):
		return false
	var deaths := int(_native().get_death_count())
	var died: bool = await _wait_until(func() -> bool: return int(_native().get_death_count()) > deaths, 15.0)
	if not died:
		return _fail("flat out into the end, the rider lived (at %s)" % str(_position()))
	var death: Dictionary = _native().get_death_state()
	if not bool(death["side_on"]) or float(death["speed_mps"]) < 10.0:
		return _fail("the death at the end was not the post's blow (side-on %s, %.1f m/s)" % [
			str(death["side_on"]), float(death["speed_mps"])])
	await _frames(3)
	var toasts: Array = _main._hud._toasts
	if toasts.is_empty() or not String(toasts[-1]["sub"]).begins_with("HIT AT"):
		return _fail("the slam was not named as a hit")
	var corpses: Array = _native().get_corpses()
	if corpses.is_empty():
		return _fail("the slam left no body")
	var pelvis: Transform3D = (corpses[-1]["parts"] as Array)[0]
	var bottom: Vector3 = _native().get_zipline_lines()[0]["bottom"]
	var off_post := Vector2(pelvis.origin.x - bottom.x, pelvis.origin.z - bottom.z).length()
	await _seconds(1.0)
	await _pose("zipline_slam")
	if off_post > 4.0:
		return _fail("the body lies %.1f m from the bottom post" % off_post)
	_detail = "hit_mps=%.1f body_off_post_m=%.1f" % [float(death["speed_mps"]), off_post]
	return true

func _touch_zipline() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var device := InputRouter.Device.TOUCH
	if not await _board_yard_zipline(device):
		return false
	if not _main._touch.is_button_shown(&"drop"):
		return _fail("DROP was not offered on the zipline")
	if _main._touch.button_label(&"jump") != "JUMP OFF":
		return _fail("JUMP read '%s' on the zipline, expected JUMP OFF" % _main._touch.button_label(&"jump"))
	if not String(_ctx()["action"]["detail"]).begins_with("ZIPLINE"):
		return _fail("the HUD did not name the zipline ('%s')" % String(_ctx()["action"]["detail"]))
	await _seconds(0.4)
	var grip_error: float = _main._arms.anchored_error()
	if _main._arms.hand_poses() != [11, 11] or grip_error < 0.0 or grip_error > 0.02:
		var hands: Array = []
		for hand in _main._arms._hands:
			hands.append([hand.anchor, hand.root.global_position])
		return _fail("hands not gripping the trolley's bar (poses %s, error %.3f m; eye %s pitch %.2f, body %s, native %s / %s, hands %s)" % [
			str(_main._arms.hand_poses()), grip_error, str(_main._camera.global_position), float(_main._pitch),
			str(_position()), str(_native().get_traversal_left_hand()), str(_native().get_traversal_right_hand()),
			str(hands)])
	await _seconds(1.2)
	await _pose("zipline")
	# Ridden as a player rides it: flat out, then the stick held back (the
	# hand brake) over the last 22 m while faster than 4 m/s, into the buffer.
	var length := float(_native().get_zipline_lines()[0]["length"])
	var fastest := 0.0
	var braked := false
	var stopped := false
	var holding_back := false
	var waited := 0.0
	while waited < 14.0 and not stopped:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var zip: Dictionary = _native().get_zipline_state()
		if not bool(zip["riding"]):
			return _fail("the hands came off the bar mid-ride (at %s)" % str(_position()))
		var speed := absf(float(zip["speed_mps"]))
		var brake := float(zip["along_m"]) > length - 22.0 and speed > 4.0
		if brake and not holding_back:
			_stick_push(Vector2(0.0, 1.0))
		elif not brake and holding_back:
			_move_dir(device, Vector2.ZERO)
		holding_back = brake
		fastest = maxf(fastest, speed)
		braked = braked or float(zip["hand_brake"]) > 0.5
		stopped = float(zip["along_m"]) > length - 4.0 and speed < 0.3
	_move_dir(device, Vector2.ZERO)
	if not stopped or not braked:
		return _fail("the ride never stopped braked at the bottom (fastest %.1f m/s, braked %s)" % [fastest, str(braked)])
	if fastest < 12.0:
		return _fail("the ride never got up to speed (%.1f m/s)" % fastest)
	_move_dir(device, Vector2.ZERO)
	await _frames(2)
	_tap(1, _center(&"drop"))
	var landed: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.5)
	_detail = "fastest_mps=%.1f landed=(%.1f, %.2f, %.1f) deaths=%d" % [
		fastest, _position().x, _position().y, _position().z, int(_native().get_death_count())]
	if not landed or int(_native().get_death_count()) != 0:
		return _fail("DROP at the bottom did not put the body down alive on the yard")
	if _position().y > 1.5:
		return _fail("let go, the body is not on the yard (y %.2f)" % _position().y)
	return true

func _touch_monkey_bars() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_stick_push(Vector2(0.0, -1.0))
	await _frames(2)
	_tap(1, _center(&"jump"))
	var caught: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 6, 1.5)
	if not caught:
		return _fail("a running JUMP under the bars never caught the first bar")
	if not _main._touch.is_button_shown(&"drop"):
		return _fail("DROP was not offered while swinging")
	if _main._touch.button_label(&"jump") != "JUMP OFF":
		return _fail("JUMP read '%s' while swinging, expected JUMP OFF" % _main._touch.button_label(&"jump"))
	await _seconds(0.2)
	var grip_error: float = _main._arms.anchored_error()
	if _main._arms.hand_poses() != [11, 11] or grip_error < 0.0 or grip_error > 0.02:
		return _fail("hands not gripping the native bar (poses %s, error %.3f m)" % [
			str(_main._arms.hand_poses()), grip_error])
	await _seconds(0.3)
	var view_fault_settled: String = _main._arms.view_fault(_main._camera)
	if view_fault_settled != "":
		return _fail("the arms do not read on screen: " + view_fault_settled)
	await _pose("swinging")
	var crossed: bool = await _wait_until(
		func() -> bool: return _native().get_traversal_left_hand().x > 4.95, 4.0)
	if not crossed:
		return _fail("held forward, the swing never carried the hands to the last bar (hands at x %.2f)" % [
			_native().get_traversal_left_hand().x])
	var forward: bool = await _wait_until(
		func() -> bool: return _velocity().x > 2.0 and _position().x > 4.95, 4.0)
	if not forward:
		return _fail("the last bar's swing never came forward fast")
	var fling_vx := _velocity().x
	_tap(2, _center(&"drop"))
	var let_go: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 0, 0.2)
	if not let_go:
		return _fail("DROP did not let go of the bar")
	var landed: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0)
	if not landed:
		return _fail("the fling off the last bar never came down")
	_detail = "fling_vx=%.2f landed_x=%.2f landed_y=%.2f grip_error_m=%.4f" % [
		fling_vx, _position().x, _position().y, grip_error]
	return _position().x > 5.9 and _position().y > 1.3

# The pipe gallery in the yard, in the game's world: run along the start
# stand, JUMP from its painted edge, the stick held forward hand over hand
# along the pipes to the platform's lip, CLIMB UP onto it.
func _touch_pipe_gallery() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_stick_push(Vector2(0.0, -1.0))
	var at_edge: bool = await _wait_until(func() -> bool: return _position().x > 25.5, 2.0)
	if not at_edge:
		return _fail("the run never reached the start stand's edge (x %.2f)" % _position().x)
	_tap(1, _center(&"jump"))
	var caught: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 6, 1.5)
	if not caught:
		return _fail("the JUMP from the stand's edge never caught a pipe")
	await _seconds(0.2)
	var grip_error: float = _main._arms.anchored_error()
	if _main._arms.hand_poses() != [11, 11] or grip_error < 0.0 or grip_error > 0.02:
		return _fail("hands not gripping the pipe (poses %s, error %.3f m)" % [
			str(_main._arms.hand_poses()), grip_error])
	await _seconds(0.3)
	var view_fault_settled: String = _main._arms.view_fault(_main._camera)
	if view_fault_settled != "":
		return _fail("the arms do not read on screen: " + view_fault_settled)
	await _pose("gallery_swing")
	var lip: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 1, 8.0)
	if not lip:
		return _fail("held forward, the swing never carried the hands to the platform's lip (hands at x %.2f)" % [
			_native().get_traversal_left_hand().x])
	if _felt(&"reach") < 1:
		return _fail("swung bar to bar, the hands felt no reach (haptics %s)" % str(_main.haptic_counts))
	_touch(0, _main._touch.stick_home(), false)
	await _seconds(0.3)
	if _main._touch.button_label(&"jump") != "CLIMB UP":
		return _fail("JUMP read '%s' on the lip, expected CLIMB UP" % _main._touch.button_label(&"jump"))
	_tap(1, _center(&"jump"))
	var up: bool = await _wait_until(
		func() -> bool: return bool(_native().is_player_grounded()) and _position().y > 5.4, 3.0)
	if not up:
		return _fail("CLIMB UP never stood the body on the platform (y %.2f)" % _position().y)
	await _pose("gallery_top")
	_detail = "top_x=%.2f top_y=%.2f grip_error_m=%.4f" % [_position().x, _position().y, grip_error]
	return true

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
	# Captions on for this fall: each line the climber says is printed as said.
	_main._settings.captions = true
	var captioned := []
	var reached: bool = await _wait_until(func() -> bool:
		var line := String(_main._hud.caption)
		if not line.is_empty() and (captioned.is_empty() or captioned[-1] != line):
			captioned.append(line)
		return float(_ctx()["danger"]) >= 0.8, 5.0)
	if not reached:
		return _fail("fall danger never reached 0.8 of the native lethal speed")
	# Danger is named, not only coloured.
	if _main._hud.gauge_word() != "LETHAL":
		return _fail("at 0.8 of the lethal speed the fall gauge reads '%s', not LETHAL" % _main._hud.gauge_word())
	await _pose("fall_danger")
	var reached_2: bool = await _wait_until(func() -> bool: return int(_native().get_death_count()) >= 1, 6.0)
	if not reached_2:
		return _fail("unmitigated fall was not lethal")
	await _frames(3)
	var toasts: Array = _main._hud._toasts
	if toasts.is_empty() or String(toasts[-1]["title"]) != "LETHAL IMPACT":
		return _fail("restore was not announced")
	# The death is seen from inside it: the view rides the head of the body it
	# left, the blood lies where it hit, and the breaks are felt.
	var corpses: Array = _native().get_corpses()
	if corpses.is_empty():
		return _fail("the lethal impact left no body")
	if int(_main.deaths_ridden) < 1 or float(_main._death_cam_left) <= 0.0:
		return _fail("the view did not ride the body the death left")
	var head: Transform3D = (corpses[-1]["parts"] as Array)[2]
	var from_head: float = _main._camera.global_position.distance_to(head.origin)
	if from_head > 1.0:
		return _fail("riding the death, the view is %.2f m from the body's head" % from_head)
	if int(_main._corpse_view.splashes_shown()) < 1:
		return _fail("no blood where the body hit")
	await _seconds(0.3)
	await _pose("lethal_toast")
	if _felt(&"break") < 1:
		return _fail("the breaks were not felt")
	# Pulled up over the body: it and the blood in view.
	await _seconds(0.8)
	await _pose("death_over")
	var cam_done: bool = await _wait_until(func() -> bool: return float(_main._death_cam_left) <= 0.0, 3.0)
	if not cam_done:
		return _fail("the view never came back from the body")
	await _frames(2)
	var eye_gap: float = _main._camera.global_position.distance_to(_position())
	if eye_gap > 2.0:
		return _fail("back from the death, the view is %.2f m from the restored body" % eye_gap)
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
	# What was printed is what was said: lines of the voice's own table, word
	# for word, gone when the impact cut the voice.
	var spoken: Array = VoiceLinesTable.CAPTIONS.values()
	if captioned.is_empty():
		return _fail("captions on, the fall's lines were never printed")
	for line in captioned:
		if not spoken.has(line):
			return _fail("caption '%s' is not a line the voice says" % line)
	if not String(_main._hud.caption).is_empty():
		return _fail("the caption '%s' stays after the voice was cut" % _main._hud.caption)
	_main._settings.captions = false
	_detail = "breaks_felt=%d toast='%s' lethal_mps=%.1f voice=%s cut=%d captions=%s" % [_felt(&"break"), sub, lethal, str(said),
		int(_main._audio.voice_cut), str(captioned)]
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
	# Twelve metres onto steel, planted: an ankle broken (sim kAnkleBreakSpeed),
	# said once, named, felt, and walked on as a hobble.
	if int(said.get(&"pain", 0)) != 1:
		return _fail("the break cried out %d times, not once" % int(said.get(&"pain", 0)))
	var landing: Dictionary = _native().get_landing_state()
	if not bool(landing["limping"]) or int(landing["ankle_breaks"]) != 1:
		return _fail("a %.1f m/s planted landing broke nothing (breaks %d)" % [impact, int(landing["ankle_breaks"])])
	var toasts: Array = _main._hud._toasts
	if toasts.is_empty() or String(toasts[-1]["title"]) != "BROKEN ANKLE":
		return _fail("the break was not named")
	# Felt twice, the second a beat after the first.
	if not await _wait_until(func() -> bool: return _felt(&"break") >= 2, 0.5):
		return _fail("the break was felt %d times, not twice" % _felt(&"break"))
	await _pose("broken_ankle")
	_stick_push(Vector2(0.0, -1.0))
	await _seconds(1.5)
	var hobble := _walk_speed().length()
	_touch(0, _main._touch.stick_home(), false)
	if hobble < 1.0 or hobble > 2.7:
		return _fail("on a broken ankle the full stick walks %.2f m/s, not a hobble" % hobble)
	_detail = "impact_mps=%.2f hobble_mps=%.2f limp_left=%.1f voice=%s" % [impact, hobble,
		float(_native().get_landing_state()["limp_left"]), str(said)]
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
	_click(_main._pause_menu.side_button_center(&"licences"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"licences":
		return _fail("tapping LICENCES did not open the licences page")
	var notices: String = (_main._pause_menu._pages[&"licences"] as RichTextLabel).get_parsed_text()
	for owed in ["Jorrit Rouwe", "Godot Engine contributors", "Chatterbox", "LICENCE: "]:
		if not notices.contains(owed):
			return _fail("the licences page does not carry '%s'" % owed)
	await _pose("pause_licences")
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
