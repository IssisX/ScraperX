extends Node

# Every device speaks one verb vocabulary; main.gd decides what a verb means
# against native state (a Jump while hanging is the native mantle, an Action at
# a pendant opens its controls). Held state is rebuilt only from events, never
# polled from Input, so an injected test event and a real device take exactly
# one path -- and focus loss can clear it wholesale, because a key released
# while the window was unfocused never arrives and a stuck key must not keep
# walking the player off a ledge.
#
# Verbs: jump, action, drop, chute, back (pad B: drop / leave controls),
# alt (pad Y: chute / sling), sling_toggle, valve, sling_release, sling_attach,
# crouch (a toggle: C, right-stick click, the touch button), pause, telemetry.
# Held Ctrl is the one held crouch; frame() reports it as crouch_held.

enum Device { KEYBOARD_MOUSE, GAMEPAD, TOUCH }

signal device_changed(device: int)
signal pad_disconnected

const UiStyle := preload("res://presentation/ui/ui_style.gd")

# Unchanged from the pre-router scheme at sensitivity 1.0, so nobody's hands
# have to relearn mouse or touch look.
const LOOK_RADIANS_PER_UNIT := 0.003
const STICK_DEADZONE := 0.14
const STICK_LOOK_YAW_RATE := 3.3
const STICK_LOOK_PITCH_RATE := 2.2
const STICK_LOOK_EXPONENT := 1.8
# Console look acceleration: a held full yaw deflection ramps toward a faster
# turn so a 180 does not take a second, while fine aim below full stays exact.
const LOOK_BOOST_DELAY := 0.3
const LOOK_BOOST_RAMP := 0.35
const LOOK_BOOST_MAX := 1.6
const DEVICE_SWITCH_AXIS := 0.45
# Gyro aim, the one polled input: a rate the platform refreshes continuously,
# not held state that could stick. Godot's Android layer (4.7-stable,
# GodotInputHandler.onSensorChanged) rotates the sensor into screen axes --
# x right, y up, z out of the glass -- so turning the device right is a
# negative rate about y, and tipping its top edge toward you, to look up, a
# positive rate about x. Below this rate the turn is tightened toward zero,
# so a steady hand's sensor noise does not drift the view.
const GYRO_TIGHTEN_RADIANS_PER_SECOND := 0.035
const TRIGGER_DEADZONE := 0.08
const AXIS_COUNT := 6

var device := Device.KEYBOARD_MOUSE
var pad_family := UiStyle.Family.XBOX
var active_pad := -1
var enabled := true
var gameplay_active := true
# Desktop touch-emulation mode (--touch): the mouse is the finger, so mouse
# events are left to the touch they were emulated into.
var accept_emulated_touch := false
var capture_mouse := true
var look_sensitivity := 1.0
var stick_sensitivity := 1.0
var invert_y := false
var gyro_aim := false
var gyro_sensitivity := 1.0
var touch: Node = null

var _keys := {}
var _buttons := {}
var _axes := PackedFloat32Array()
var _mouse_look := Vector2.ZERO
var _verbs: Array[StringName] = []
var _boost_timer := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_axes.resize(AXIS_COUNT)
	Input.joy_connection_changed.connect(_on_joy_connection_changed)


func set_device(value: int) -> void:
	if value == device:
		return
	device = value
	if value == Device.TOUCH and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	device_changed.emit(value)


func glyph_family() -> int:
	match device:
		Device.GAMEPAD:
			return pad_family
		Device.TOUCH:
			return UiStyle.Family.TOUCH
	return UiStyle.Family.KEYBOARD


func push_verb(verb: StringName) -> void:
	if gameplay_active:
		_verbs.append(verb)


func clear_held() -> void:
	_keys.clear()
	_buttons.clear()
	_axes.fill(0.0)
	_mouse_look = Vector2.ZERO
	_boost_timer = 0.0
	if touch != null:
		touch.reset_touches()


func _input(event: InputEvent) -> void:
	if not enabled:
		return
	if event is InputEventScreenTouch or event is InputEventScreenDrag:
		if event.device == InputEvent.DEVICE_ID_EMULATION and not accept_emulated_touch:
			return
		if not gameplay_active or touch == null:
			return
		if event is InputEventScreenTouch and (event as InputEventScreenTouch).pressed:
			set_device(Device.TOUCH)
		if touch.handle_touch(event):
			get_viewport().set_input_as_handled()
		return
	if event is InputEventMouseButton:
		# A touch's emulated mouse twin: the touch itself was already routed.
		if event.device == InputEvent.DEVICE_ID_EMULATION or accept_emulated_touch:
			return
		var button := event as InputEventMouseButton
		if button.pressed:
			set_device(Device.KEYBOARD_MOUSE)
			if gameplay_active and capture_mouse and button.button_index == MOUSE_BUTTON_LEFT:
				Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
		return
	if event is InputEventMouseMotion:
		if event.device == InputEvent.DEVICE_ID_EMULATION or accept_emulated_touch:
			return
		var motion := event as InputEventMouseMotion
		if gameplay_active and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
			_mouse_look += motion.relative
			if motion.relative.length_squared() > 4.0:
				set_device(Device.KEYBOARD_MOUSE)
		return
	if event is InputEventKey:
		_handle_key(event as InputEventKey)
	elif event is InputEventJoypadButton:
		_handle_pad_button(event as InputEventJoypadButton)
	elif event is InputEventJoypadMotion:
		_handle_pad_motion(event as InputEventJoypadMotion)


# Physical keycodes: WASD stays where the fingers are on AZERTY or Dvorak.
func _key_code(key: InputEventKey) -> Key:
	return key.physical_keycode if key.physical_keycode != KEY_NONE else key.keycode


func _handle_key(key: InputEventKey) -> void:
	if key.echo:
		return
	var code := _key_code(key)
	if not key.pressed:
		_keys.erase(code)
		return
	_keys[code] = true
	set_device(Device.KEYBOARD_MOUSE)
	if not gameplay_active:
		return
	match code:
		KEY_SPACE:
			_verbs.append(&"jump")
		KEY_E:
			_verbs.append(&"action")
		KEY_Q:
			_verbs.append(&"drop")
		KEY_F:
			_verbs.append(&"chute")
		KEY_C:
			_verbs.append(&"crouch")
		KEY_V:
			_verbs.append(&"valve")
		KEY_R:
			_verbs.append(&"sling_release")
		KEY_G:
			_verbs.append(&"sling_attach")
		KEY_ESCAPE:
			_verbs.append(&"pause")
		KEY_F3:
			_verbs.append(&"telemetry")


func _handle_pad_button(button: InputEventJoypadButton) -> void:
	if not button.pressed:
		if button.device == active_pad:
			_buttons.erase(button.button_index)
		return
	_adopt_pad(button.device)
	set_device(Device.GAMEPAD)
	_buttons[button.button_index] = true
	if not gameplay_active:
		return
	match button.button_index:
		JOY_BUTTON_A:
			_verbs.append(&"jump")
		JOY_BUTTON_X:
			_verbs.append(&"action")
		JOY_BUTTON_B:
			_verbs.append(&"back")
		JOY_BUTTON_Y:
			_verbs.append(&"alt")
		JOY_BUTTON_RIGHT_STICK:
			_verbs.append(&"crouch")
		JOY_BUTTON_START:
			_verbs.append(&"pause")
		JOY_BUTTON_BACK:
			_verbs.append(&"telemetry")


func _handle_pad_motion(motion: InputEventJoypadMotion) -> void:
	var axis := int(motion.axis)
	if axis < 0 or axis >= AXIS_COUNT:
		return
	var value := clampf(motion.axis_value, -1.0, 1.0)
	if motion.device != active_pad:
		# A resting second pad's drift must not steal the player.
		if absf(value) < DEVICE_SWITCH_AXIS:
			return
		_adopt_pad(motion.device)
	_axes[axis] = value
	if absf(value) >= DEVICE_SWITCH_AXIS:
		set_device(Device.GAMEPAD)


func _adopt_pad(pad: int) -> void:
	if pad == active_pad:
		return
	active_pad = pad
	_axes.fill(0.0)
	_buttons.clear()
	var name := Input.get_joy_name(pad).to_lower()
	if (name.contains("dualsense") or name.contains("dualshock") or name.contains("sony")
			or name.contains("playstation") or name.begins_with("ps")
			or name.contains("ps3") or name.contains("ps4") or name.contains("ps5")):
		pad_family = UiStyle.Family.PLAYSTATION
	elif (name.contains("nintendo") or name.contains("switch") or name.contains("joy-con")
			or name.contains("pro controller")):
		pad_family = UiStyle.Family.NINTENDO
	else:
		pad_family = UiStyle.Family.XBOX


func _on_joy_connection_changed(pad: int, connected: bool) -> void:
	if connected or pad != active_pad:
		return
	active_pad = -1
	_axes.fill(0.0)
	_buttons.clear()
	if device == Device.GAMEPAD:
		pad_disconnected.emit()


func _held(code: Key) -> float:
	return 1.0 if _keys.has(code) else 0.0


func _pad_button(button: JoyButton) -> float:
	return 1.0 if _buttons.has(button) else 0.0


# Radial deadzone rescaled to the full range, so a small, deliberate push is
# a small, deliberate walk rather than a jump from zero to 14 percent.
func _stick(axis_x: int, axis_y: int) -> Vector2:
	if active_pad < 0:
		return Vector2.ZERO
	var value := Vector2(_axes[axis_x], _axes[axis_y])
	var magnitude := value.length()
	if magnitude <= STICK_DEADZONE:
		return Vector2.ZERO
	var scaled := minf(1.0, (magnitude - STICK_DEADZONE) / (1.0 - STICK_DEADZONE))
	return value / magnitude * scaled


func _trigger(axis: int) -> float:
	if active_pad < 0:
		return 0.0
	var value := _axes[axis]
	return 0.0 if value <= TRIGGER_DEADZONE else (value - TRIGGER_DEADZONE) / (1.0 - TRIGGER_DEADZONE)


func _stick_look(stick: Vector2, delta: float) -> Vector2:
	var magnitude := stick.length()
	if magnitude <= 0.0:
		_boost_timer = 0.0
		return Vector2.ZERO
	var direction := stick / magnitude
	var curved := pow(magnitude, STICK_LOOK_EXPONENT)
	if magnitude > 0.94 and absf(direction.x) > 0.7:
		_boost_timer += delta
	else:
		_boost_timer = 0.0
	var boost := 1.0 + (LOOK_BOOST_MAX - 1.0) * clampf(
		(_boost_timer - LOOK_BOOST_DELAY) / LOOK_BOOST_RAMP, 0.0, 1.0)
	return Vector2(direction.x * curved * STICK_LOOK_YAW_RATE * boost,
		direction.y * curved * STICK_LOOK_PITCH_RATE) * stick_sensitivity * delta


# 1:1 at sensitivity 1.0: turn the device 90 degrees and the view turns 90.
func _gyro_look(delta: float) -> Vector2:
	if not gyro_aim:
		return Vector2.ZERO
	var rate := Input.get_gyroscope()
	# A non-finite sample would poison the yaw it accumulates into for good.
	if not rate.is_finite():
		return Vector2.ZERO
	var turn := Vector2(-rate.y, -rate.x)
	var speed := turn.length()
	if speed < GYRO_TIGHTEN_RADIANS_PER_SECOND:
		turn *= speed / GYRO_TIGHTEN_RADIANS_PER_SECOND
	return turn * gyro_sensitivity * delta


# One frame of intent. look is in radians with screen orientation (x right,
# y down), exactly what the old _apply_look_delta consumed; move is x right,
# y forward; pendant is x slew/drive, y raise(+)/lower(-).
func frame(delta: float) -> Dictionary:
	var move := Vector2.ZERO
	var look := Vector2.ZERO
	var pendant := Vector2.ZERO
	var crouch_held := false
	if enabled and gameplay_active:
		crouch_held = _keys.has(KEY_CTRL)
		var keyboard := Vector2(_held(KEY_D) - _held(KEY_A), _held(KEY_W) - _held(KEY_S))
		if keyboard != Vector2.ZERO:
			keyboard = keyboard.normalized()
		var left := _stick(JOY_AXIS_LEFT_X, JOY_AXIS_LEFT_Y)
		move = keyboard + Vector2(left.x, -left.y)
		var look_units := _mouse_look
		if touch != null:
			move += touch.move_vector
			look_units += touch.take_look_delta()
			pendant += touch.pendant_axes
		move = move.limit_length(1.0)
		look = look_units * LOOK_RADIANS_PER_UNIT * look_sensitivity
		look += _stick_look(_stick(JOY_AXIS_RIGHT_X, JOY_AXIS_RIGHT_Y), delta)
		if invert_y:
			look.y = -look.y
		# After the invert: tipping the device up is looking up for everyone.
		look += _gyro_look(delta)
		pendant += Vector2(_held(KEY_RIGHT) - _held(KEY_LEFT), _held(KEY_UP) - _held(KEY_DOWN))
		pendant += Vector2(
			_pad_button(JOY_BUTTON_DPAD_RIGHT) - _pad_button(JOY_BUTTON_DPAD_LEFT),
			_pad_button(JOY_BUTTON_DPAD_UP) - _pad_button(JOY_BUTTON_DPAD_DOWN))
		pendant.y += _trigger(JOY_AXIS_TRIGGER_RIGHT) - _trigger(JOY_AXIS_TRIGGER_LEFT)
		pendant = Vector2(clampf(pendant.x, -1.0, 1.0), clampf(pendant.y, -1.0, 1.0))
	_mouse_look = Vector2.ZERO
	var verbs: Array[StringName] = _verbs.duplicate()
	_verbs.clear()
	return {"move": move, "look": look, "pendant": pendant, "verbs": verbs,
		"crouch_held": crouch_held}
