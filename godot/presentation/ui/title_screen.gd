extends Control

# The title, shown over the live tower at launch. The simulation stays
# frozen behind it (main.gd pauses the tree); the view drifts slowly up and
# across the tower while it is up. CONTINUE takes up the climb the save put
# back (main.gd has already loaded it); NEW CLIMB starts over, asking once
# more when there is a climb to lose. The title text is the working name
# until the owner settles the store name.

signal continue_requested
signal new_climb_requested
signal quit_requested

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const PauseMenu := preload("res://presentation/ui/pause_menu.gd")

const TITLE_TEXT := "SCRAPERX"
const KICKER_TEXT := "KELLERWORKS TOWER"
const NEW_CLIMB_TEXT := "NEW CLIMB"
const NEW_CLIMB_CONFIRM_TEXT := "CONFIRM: START OVER"
# The view's drift while the title is up: up the tower and slowly across it.
const DRIFT_PITCH := 0.42
const DRIFT_YAW_SWING := 0.22
const DRIFT_PERIOD_SECONDS := 38.0

# Set before the title is shown: the climb a CONTINUE takes up, if any.
var saved_altitude := NAN
var camera: Camera3D
var base_yaw := 0.0

var _u := 1.0
var _buttons := {}
var _clock := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP


func open() -> void:
	_build(get_viewport_rect().size)
	visible = true
	var first: Button = _buttons.get(&"continue", _buttons[&"new"])
	first.grab_focus()


func close() -> void:
	visible = false
	var focused := get_viewport().gui_get_focus_owner()
	if focused != null and is_ancestor_of(focused):
		focused.release_focus()


func has_continue() -> bool:
	return _buttons.has(&"continue")


func button_center(id: StringName) -> Vector2:
	var button: Button = _buttons[id]
	return button.get_global_rect().get_center()


func button_text(id: StringName) -> String:
	return (_buttons[id] as Button).text


func _process(delta: float) -> void:
	if not visible or camera == null:
		return
	_clock += delta
	var phase := TAU * _clock / DRIFT_PERIOD_SECONDS
	camera.rotation = Vector3(DRIFT_PITCH + 0.05 * sin(phase * 0.5), base_yaw + DRIFT_YAW_SWING * sin(phase), 0.0)


func _build(viewport_size: Vector2) -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_buttons.clear()
	_u = UiStyle.unit(self)
	theme = PauseMenu.menu_theme(_u)
	var safe := UiStyle.safe_rect(self)

	# A wash on the left third only: the tower stays the picture.
	var wash := TextureRect.new()
	var gradient := Gradient.new()
	gradient.set_color(0, UiStyle.with_alpha(UiStyle.INK, 0.9))
	gradient.set_color(1, UiStyle.with_alpha(UiStyle.INK, 0.0))
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.fill_from = Vector2(0.0, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	wash.texture = texture
	wash.stretch_mode = TextureRect.STRETCH_SCALE
	wash.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(wash)
	wash.position = Vector2.ZERO
	wash.size = Vector2(maxf(viewport_size.x * 0.55, 900.0 * _u), viewport_size.y)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", int(safe.position.x + 96.0 * _u))
	margin.add_theme_constant_override("margin_top", int(safe.position.y + 120.0 * _u))
	margin.add_theme_constant_override("margin_bottom", int(96.0 * _u))
	add_child(margin)
	margin.position = Vector2.ZERO
	margin.size = Vector2(maxf(viewport_size.x * 0.42, 720.0 * _u), viewport_size.y)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", int(16.0 * _u))
	margin.add_child(column)

	column.add_child(_label(KICKER_TEXT, 26.0, UiStyle.AMBER, UiStyle.font_label()))
	column.add_child(_label(TITLE_TEXT, 150.0, UiStyle.PAPER, UiStyle.font_heavy()))
	var line := ColorRect.new()
	line.color = UiStyle.AMBER
	line.custom_minimum_size = Vector2(180.0 * _u, 6.0 * _u)
	line.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	column.add_child(line)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0.0, 48.0 * _u)
	column.add_child(gap)

	if not is_nan(saved_altitude):
		var resume := _button("CONTINUE")
		column.add_child(resume)
		_buttons[&"continue"] = resume
		column.add_child(_label("FROM %+.1f M" % saved_altitude, 24.0, UiStyle.SAFE, UiStyle.font_label()))
		resume.pressed.connect(func() -> void: continue_requested.emit())
	var fresh := _button(NEW_CLIMB_TEXT)
	column.add_child(fresh)
	_buttons[&"new"] = fresh
	fresh.pressed.connect(func() -> void:
		if not _buttons.has(&"continue") or fresh.text == NEW_CLIMB_CONFIRM_TEXT:
			new_climb_requested.emit()
		else:
			fresh.text = NEW_CLIMB_CONFIRM_TEXT)
	if not OS.has_feature("mobile"):
		var leave := _button("QUIT TO DESKTOP")
		column.add_child(leave)
		_buttons[&"quit"] = leave
		leave.pressed.connect(func() -> void: quit_requested.emit())


func _label(text: String, size: float, color: Color, font: Font) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_override("font", font)
	label.add_theme_font_size_override("font_size", int(roundf(size * _u)))
	label.add_theme_color_override("font_color", color)
	return label


func _button(text: String) -> Button:
	var button := Button.new()
	button.text = text
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.focus_mode = Control.FOCUS_ALL
	button.custom_minimum_size = Vector2(560.0 * _u, 0.0)
	button.size_flags_horizontal = Control.SIZE_SHRINK_BEGIN
	button.add_theme_font_override("font", UiStyle.font_heavy())
	button.add_theme_font_size_override("font_size", int(roundf(48.0 * _u)))
	return button
