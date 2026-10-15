extends Node

# Shared UI-test state and the real viewport/native test operations. Suites
# borrow this Node; it remains the sole owner of timing, input and reporting.
const UiStyle := preload("res://presentation/ui/ui_style.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")

var _main: Node
var _scenario := ""
var _capture_prefix := ""
var _detail := ""


func _fail(reason: String) -> bool:
	_detail = "reason=" + reason
	return false



func _teeter_angle(body: int) -> float:
	var pose: Transform3D = _native().get_kit_body_transform(body)
	return pose.basis.get_euler().z


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
