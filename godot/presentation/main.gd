extends Node3D

const YardWorld := preload("res://presentation/yard_world.gd")
const WALK_SPEED := 5.0
const RUN_SPEED := 7.0
const JUMP_SPEED := 6.2
const GRAVITY := 22.0
const MOUSE_LOOK := 0.0028
const TOUCH_LOOK := 0.0032
const STICK_LOOK := 2.5

var _world: Node3D
var _player: CharacterBody3D
var _camera: Camera3D
var _floor_probe: RayCast3D
var _stage: Object
var _hud: CanvasLayer
var _status: Label
var _hint: Label
var _tank: Label
var _bucket: Label
var _cage: Label
var _meter: ProgressBar
var _footer: PanelContainer
var _action_button: Button
var _valve_button: Button
var _reverse_button: Button
var _jump_button: Button
var _joystick_base: PanelContainer
var _joystick_knob: ColorRect
var _touch_move_id := -1
var _touch_look_id := -1
var _touch_origin := Vector2.ZERO
var _touch_axis := Vector2.ZERO
var _jump_queued := false
var _last_pad_jump := false
var _touch_mode := false
var _ascent_finished := false

func _ready() -> void:
	_world = YardWorld.new()
	_world.name = "YardWorld"
	add_child(_world)
	_build_player()
	_build_hud()
	if ClassDB.class_exists("ScraperXScrew"):
		_stage = ClassDB.instantiate("ScraperXScrew")
	else:
		_status.text = "NATIVE SCREW MODULE MISSING"
		_hint.text = "The game build is incomplete."
		push_error("ScraperXScrew GDExtension was not loaded")
	_touch_mode = OS.has_feature("mobile")
	_update_hud()
	if not _touch_mode:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _build_player() -> void:
	_player = CharacterBody3D.new()
	_player.name = "Player"
	_player.position = Vector3(5.5, 0.91, 3.0)
	_player.floor_snap_length = 0.26
	_player.safe_margin = 0.025
	add_child(_player)
	var collision := CollisionShape3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.36
	capsule.height = 1.80
	collision.shape = capsule
	_player.add_child(collision)
	_floor_probe = RayCast3D.new()
	_floor_probe.name = "FloorContactProbe"
	_floor_probe.position = Vector3(0.0, -0.70, 0.0)
	_floor_probe.target_position = Vector3(0.0, -0.45, 0.0)
	_floor_probe.exclude_parent = true
	_floor_probe.enabled = true
	_player.add_child(_floor_probe)
	_camera = Camera3D.new()
	_camera.name = "FirstPersonCamera"
	_camera.position.y = 0.62
	_camera.fov = 80.0
	_camera.current = true
	_player.add_child(_camera)

func _panel_style(color: Color, border: Color) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = color
	style.border_color = border
	style.set_border_width_all(2)
	style.set_corner_radius_all(8)
	style.content_margin_left = 18
	style.content_margin_right = 18
	style.content_margin_top = 14
	style.content_margin_bottom = 14
	return style

func _label(copy: String, size: int, color: Color) -> Label:
	var label := Label.new()
	label.text = copy
	label.add_theme_font_size_override("font_size", size)
	label.add_theme_color_override("font_color", color)
	return label

func _button(copy: String, where: Rect2, color: Color) -> Button:
	var button := Button.new()
	button.text = copy
	button.position = where.position
	button.size = where.size
	button.add_theme_font_size_override("font_size", 25)
	button.add_theme_color_override("font_color", Color(0.08, 0.11, 0.12))
	button.add_theme_stylebox_override("normal", _panel_style(color, color.darkened(0.22)))
	button.add_theme_stylebox_override("hover", _panel_style(color.lightened(0.15), color))
	button.add_theme_stylebox_override("pressed", _panel_style(color.darkened(0.15), color))
	return button

func _build_hud() -> void:
	_hud = CanvasLayer.new()
	_hud.name = "HUD"
	add_child(_hud)
	var root := Control.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hud.add_child(root)

	var heading := PanelContainer.new()
	heading.position = Vector2(34, 30)
	heading.custom_minimum_size = Vector2(570, 0)
	heading.add_theme_stylebox_override("panel", _panel_style(
		Color(0.055, 0.08, 0.09, 0.90), Color(0.34, 0.46, 0.47)))
	root.add_child(heading)
	var stack := VBoxContainer.new()
	stack.add_theme_constant_override("separation", 6)
	heading.add_child(stack)
	stack.add_child(_label("SCRAPERX  /  GROUND ASCENT 01", 19, Color(0.95, 0.61, 0.27)))
	_status = _label("CHARGE THE UPPER TANK", 31, Color(0.94, 0.94, 0.89))
	stack.add_child(_status)
	_hint = _label("Operate the screw, fill the bucket, ride the cage.",
		18, Color(0.73, 0.81, 0.80))
	_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	stack.add_child(_hint)
	_meter = ProgressBar.new()
	_meter.custom_minimum_size = Vector2(500, 20)
	_meter.min_value = 0
	_meter.max_value = 2
	_meter.show_percentage = false
	stack.add_child(_meter)
	_tank = _label("UPPER TANK  0.00 / 2.00 m³", 20, Color(0.42, 0.78, 0.86))
	_bucket = _label("LIFT BUCKET  0.00 / 2.00 m³", 20, Color(0.77, 0.83, 0.80))
	_cage = _label("CAGE  +0.00 / +8.00 m", 20, Color(0.95, 0.61, 0.27))
	stack.add_child(_tank)
	stack.add_child(_bucket)
	stack.add_child(_cage)

	var reticle := _label("+", 38, Color(0.96, 0.91, 0.74, 0.8))
	reticle.set_anchors_preset(Control.PRESET_CENTER)
	reticle.position = Vector2(-12, -20)
	root.add_child(reticle)

	_footer = PanelContainer.new()
	_footer.anchor_top = 1
	_footer.anchor_bottom = 1
	_footer.position = Vector2(34, -92)
	_footer.custom_minimum_size = Vector2(730, 0)
	_footer.add_theme_stylebox_override("panel", _panel_style(
		Color(0.055, 0.08, 0.09, 0.82), Color(0.26, 0.36, 0.37)))
	root.add_child(_footer)
	_footer.add_child(_label("WASD MOVE   •   MOUSE LOOK   •   E OPERATE   •   V VALVE   •   SPACE JUMP",
		18, Color(0.86, 0.89, 0.87)))

	_action_button = _button("OPERATE", Rect2(Vector2.ZERO, Vector2(228, 94)),
		Color(0.94, 0.60, 0.22))
	_action_button.anchor_left = 1
	_action_button.anchor_right = 1
	_action_button.anchor_top = 1
	_action_button.anchor_bottom = 1
	_action_button.position = Vector2(-252, -126)
	_action_button.pressed.connect(_primary_action)
	root.add_child(_action_button)
	_valve_button = _button("VALVE", Rect2(Vector2.ZERO, Vector2(200, 84)),
		Color(0.44, 0.78, 0.84))
	_valve_button.anchor_left = 1
	_valve_button.anchor_right = 1
	_valve_button.anchor_top = 1
	_valve_button.anchor_bottom = 1
	_valve_button.position = Vector2(-468, -120)
	_valve_button.pressed.connect(_valve_action)
	root.add_child(_valve_button)
	_reverse_button = _button("REVERSE", Rect2(Vector2.ZERO, Vector2(195, 78)),
		Color(0.70, 0.74, 0.72))
	_reverse_button.anchor_left = 1
	_reverse_button.anchor_right = 1
	_reverse_button.anchor_top = 1
	_reverse_button.anchor_bottom = 1
	_reverse_button.position = Vector2(-465, -216)
	_reverse_button.pressed.connect(_reverse_action)
	root.add_child(_reverse_button)
	_jump_button = _button("JUMP", Rect2(Vector2.ZERO, Vector2(165, 78)),
		Color(0.68, 0.72, 0.70))
	_jump_button.anchor_left = 1
	_jump_button.anchor_right = 1
	_jump_button.anchor_top = 1
	_jump_button.anchor_bottom = 1
	_jump_button.position = Vector2(-194, -222)
	_jump_button.pressed.connect(_queue_jump)
	root.add_child(_jump_button)
	_joystick_base = PanelContainer.new()
	_joystick_base.anchor_top = 1
	_joystick_base.anchor_bottom = 1
	_joystick_base.position = Vector2(54, -232)
	_joystick_base.custom_minimum_size = Vector2(170, 170)
	_joystick_base.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_joystick_base.add_theme_stylebox_override("panel", _panel_style(
		Color(0.08, 0.13, 0.15, 0.55), Color(0.42, 0.59, 0.61, 0.7)))
	root.add_child(_joystick_base)
	_joystick_knob = ColorRect.new()
	_joystick_knob.color = Color(0.94, 0.60, 0.22, 0.85)
	_joystick_knob.size = Vector2(54, 54)
	_joystick_knob.position = Vector2(58, 58)
	_joystick_knob.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_joystick_base.add_child(_joystick_knob)

func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch:
		_touch_mode = true
		var touch := event as InputEventScreenTouch
		var screen := get_viewport().get_visible_rect().size
		if touch.pressed:
			if touch.position.x < screen.x * 0.42 and touch.position.y > screen.y * 0.35:
				_touch_move_id = touch.index
				_touch_origin = touch.position
				_touch_axis = Vector2.ZERO
			elif touch.position.x < screen.x * 0.72 or touch.position.y < screen.y * 0.68:
				_touch_look_id = touch.index
		else:
			if touch.index == _touch_move_id:
				_touch_move_id = -1
				_touch_axis = Vector2.ZERO
			if touch.index == _touch_look_id:
				_touch_look_id = -1
	elif event is InputEventScreenDrag:
		var drag := event as InputEventScreenDrag
		if drag.index == _touch_move_id:
			_touch_axis = (drag.position - _touch_origin).limit_length(75.0) / 75.0
			_joystick_knob.position = Vector2(58, 58) + _touch_axis * 48.0
		elif drag.index == _touch_look_id:
			_apply_look(drag.relative * TOUCH_LOOK)
	elif event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		_apply_look((event as InputEventMouseMotion).relative * MOUSE_LOOK)
	elif event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
		if not _touch_mode:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	elif event is InputEventKey:
		var key := event as InputEventKey
		if key.pressed and not key.echo:
			match key.physical_keycode:
				KEY_E: _primary_action()
				KEY_V: _valve_action()
				KEY_R: _reverse_action()
				KEY_SPACE: _jump_queued = true
				KEY_ESCAPE: Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _apply_look(motion: Vector2) -> void:
	_player.rotation.y -= motion.x
	_camera.rotation.x = clampf(_camera.rotation.x - motion.y, -1.40, 1.40)

func _queue_jump() -> void:
	_jump_queued = true

func _stage_position() -> Vector3:
	return _player.global_position

func _call_local(method: String) -> bool:
	if _stage == null:
		return false
	var p := _stage_position()
	return bool(_stage.call(method, p.x, p.y, p.z))

func _primary_action() -> void:
	if _stage == null:
		return
	if _call_local("at_upper_control") and bool(_stage.call("is_upper_caught")):
		_call_local("request_reset")
	elif _call_local("at_pump_station"):
		if bool(_stage.call("is_upper_caught")) and float(_stage.call("get_bucket_water")) < 0.001:
			_call_local("request_reset")
		else:
			_call_local("request_motor_toggle")
	elif _call_local("at_cage_control"):
		_call_local("request_release")

func _valve_action() -> void:
	_call_local("request_valve_toggle")

func _reverse_action() -> void:
	_call_local("request_reverse")

func _physics_process(delta: float) -> void:
	if _stage != null:
		_stage.call("advance_frame", delta)
		_world.call("update_stage", _stage)
	_move_player(delta)
	if _stage != null:
		var aboard: bool = _player.is_on_floor() and _floor_collider() == _world.get("cage")
		_stage.call("set_rider_on_cage", aboard)
	_update_hud()

func _floor_collider() -> Object:
	_floor_probe.force_raycast_update()
	if _floor_probe.is_colliding():
		return _floor_probe.get_collider()
	return null

func _move_player(delta: float) -> void:
	var move_axis := Vector2.ZERO
	if Input.is_key_pressed(KEY_A): move_axis.x -= 1.0
	if Input.is_key_pressed(KEY_D): move_axis.x += 1.0
	if Input.is_key_pressed(KEY_W): move_axis.y += 1.0
	if Input.is_key_pressed(KEY_S): move_axis.y -= 1.0
	move_axis += Vector2(_touch_axis.x, -_touch_axis.y)
	var pad_x := Input.get_joy_axis(0, JOY_AXIS_LEFT_X)
	var pad_y := Input.get_joy_axis(0, JOY_AXIS_LEFT_Y)
	if Vector2(pad_x, pad_y).length() > 0.18:
		move_axis += Vector2(pad_x, -pad_y)
	move_axis = move_axis.limit_length(1.0)
	var forward := -_player.global_transform.basis.z
	var right := _player.global_transform.basis.x
	var direction := (right * move_axis.x + forward * move_axis.y).normalized()
	var speed := RUN_SPEED if Input.is_key_pressed(KEY_SHIFT) else WALK_SPEED
	var horizontal := Vector3(_player.velocity.x, 0.0, _player.velocity.z)
	horizontal = horizontal.move_toward(direction * speed, 19.0 * delta)
	_player.velocity.x = horizontal.x
	_player.velocity.z = horizontal.z
	if _player.is_on_floor():
		if _jump_queued:
			_player.velocity.y = JUMP_SPEED
	else:
		_player.velocity.y -= GRAVITY * delta
	_jump_queued = false
	var pad_jump := Input.is_joy_button_pressed(0, JOY_BUTTON_A)
	if pad_jump and not _last_pad_jump and _player.is_on_floor():
		_player.velocity.y = JUMP_SPEED
	_last_pad_jump = pad_jump
	_player.move_and_slide()
	var look_x := Input.get_joy_axis(0, JOY_AXIS_RIGHT_X)
	var look_y := Input.get_joy_axis(0, JOY_AXIS_RIGHT_Y)
	if Vector2(look_x, look_y).length() > 0.14:
		_apply_look(Vector2(look_x, look_y) * STICK_LOOK * delta)
	if _player.position.y < -4.0:
		_player.position = Vector3(5.5, 0.91, 3.0)
		_player.velocity = Vector3.ZERO

func _update_hud() -> void:
	if _stage == null:
		_action_button.visible = false
		_valve_button.visible = false
		_reverse_button.visible = false
		_jump_button.visible = _touch_mode
		_joystick_base.visible = _touch_mode
		_footer.visible = not _touch_mode
		return
	var tank_m3 := float(_stage.call("get_tank_water"))
	var bucket_m3 := float(_stage.call("get_bucket_water"))
	var q := float(_stage.call("get_cage_travel"))
	var p := _stage_position()
	var at_pump := _call_local("at_pump_station")
	var at_cage := _call_local("at_cage_control")
	var at_upper := _call_local("at_upper_control")
	var caught := bool(_stage.call("is_upper_caught"))
	var motor := bool(_stage.call("is_motor_enabled"))
	_tank.text = "UPPER TANK  %.2f / 2.00 m³" % tank_m3
	_bucket.text = "LIFT BUCKET  %.2f / 2.00 m³" % bucket_m3
	_cage.text = "CAGE  +%.2f / +8.00 m" % q
	_meter.value = tank_m3
	if not _ascent_finished and p.y > 8.5 and _player.is_on_floor() and \
		_floor_collider() == _world.get_node("UpperLandingCollision"):
		_ascent_finished = true
	if _ascent_finished:
		_status.text = "UPPER DECK REACHED"
		_hint.text = "The screw-fed lift carried you +8 m. The next ascent is still unbuilt."
	elif caught:
		_status.text = "CAGE SECURED AT +8 M"
		_hint.text = "Step east onto the fixed upper landing."
	elif q > 0.05:
		_status.text = "ASCENDING  /  WATER WEIGHT DRIVE"
		_hint.text = "The loaded bucket descends 1 m for every 2 m you rise."
	elif bucket_m3 > 1.60:
		_status.text = "BUCKET LOADED  /  BOARD CAGE"
		_hint.text = "Stand on the cage floor and release its catch."
	elif tank_m3 > 0.20 or bucket_m3 > 0.01:
		_status.text = "TRANSFER WATER TO BUCKET"
		_hint.text = "Open the outlet valve at the amber pump control."
	else:
		_status.text = "CHARGE THE UPPER TANK"
		_hint.text = "Start the Archimedes screw at the amber control."
	if at_pump:
		if caught and bucket_m3 < 0.001:
			_hint.text = "E / OPERATE: reset the empty cage from grade."
		else:
			_hint.text = "E / OPERATE: %s screw  •  V: %s valve  •  R: reverse" % [
				"stop" if motor else "start", "close" if bool(_stage.call("is_valve_open")) else "open"]
	elif at_cage and not caught and q < 0.01:
		_hint.text = "Stand on the cage. E / OPERATE releases the bucket catch."
	elif at_upper and caught:
		_hint.text = "Upper landing reached. E / OPERATE resets after the bucket drains."
	_action_button.visible = _touch_mode and (at_pump or at_cage or (at_upper and caught))
	_action_button.text = "RESET" if caught else ("RELEASE" if at_cage else ("STOP" if motor else "START"))
	_valve_button.visible = _touch_mode and at_pump and not caught
	_valve_button.text = "CLOSE" if bool(_stage.call("is_valve_open")) else "VALVE"
	_reverse_button.visible = _touch_mode and at_pump and not motor and not caught
	_jump_button.visible = _touch_mode
	_joystick_base.visible = _touch_mode
	_footer.visible = not _touch_mode
