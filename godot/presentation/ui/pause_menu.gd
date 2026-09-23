extends Control

# The pause surface. Built from real Godot controls with a generated theme so
# focus navigation is the engine's own: D-pad / stick / arrows move, A /
# Enter / tap selects, B / Esc backs out, Menu / Start resumes. Opening it
# pauses the SceneTree, which stops main.gd's _process and with it every
# native advance_frame call -- the simulation is frozen, not slowed.

signal resume_requested
signal quit_requested
signal settings_changed

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const SettingsStore := preload("res://presentation/ui/settings_store.gd")

const PAGE_NONE := &""
const PAGE_CONTROLS := &"controls"
const PAGE_SETTINGS := &"settings"

var settings: SettingsStore
var family := UiStyle.Family.KEYBOARD
var summary := ""

var _u := 1.0
var _built_for := Vector2.ZERO
var _side_buttons := {}
var _page := PAGE_NONE
var _summary_label: Label
var _content: Control
var _plate: Control
var _controls_page: Control
var _controls_view: Control
var _controls_tabs := {}
var _controls_family := UiStyle.Family.KEYBOARD
var _settings_page: Control
var _first_setting: Control
var _value_labels := {}
var _footer: Control


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	visible = false


func open(current_family: int, current_summary: String) -> void:
	family = current_family
	summary = current_summary
	var viewport_size := get_viewport_rect().size
	if viewport_size != _built_for:
		_build(viewport_size)
	_summary_label.text = summary
	_controls_family = _canonical(family)
	_show_page(PAGE_NONE)
	visible = true
	if family != UiStyle.Family.TOUCH:
		(_side_buttons[&"resume"] as Button).grab_focus()
	_footer.queue_redraw()


func close() -> void:
	visible = false
	var focused := get_viewport().gui_get_focus_owner()
	if focused != null and is_ancestor_of(focused):
		focused.release_focus()


func press_resume() -> void:
	resume_requested.emit()


func resume_button_center() -> Vector2:
	var button: Button = _side_buttons[&"resume"]
	return button.get_global_rect().get_center()


func side_button_center(id: StringName) -> Vector2:
	var button: Button = _side_buttons[id]
	return button.get_global_rect().get_center()


func current_page() -> StringName:
	return _page


func _input(event: InputEvent) -> void:
	if not visible:
		return
	var start := event is InputEventJoypadButton and (event as InputEventJoypadButton).pressed \
		and (event as InputEventJoypadButton).button_index == JOY_BUTTON_START
	if start:
		get_viewport().set_input_as_handled()
		resume_requested.emit()
		return
	if event.is_action_pressed("ui_cancel"):
		get_viewport().set_input_as_handled()
		back()


# Android's system back, Esc and pad B all land here.
func back() -> void:
	if _page != PAGE_NONE:
		var return_to: StringName = _page
		_show_page(PAGE_NONE)
		if family != UiStyle.Family.TOUCH:
			(_side_buttons[return_to] as Button).grab_focus()
		return
	resume_requested.emit()


func _show_page(page: StringName) -> void:
	_page = page
	_plate.visible = page != PAGE_NONE
	_controls_page.visible = page == PAGE_CONTROLS
	_settings_page.visible = page == PAGE_SETTINGS
	for id in _side_buttons:
		var button: Button = _side_buttons[id]
		button.add_theme_color_override("font_color",
			UiStyle.AMBER if id == page else UiStyle.PAPER)
	if page == PAGE_CONTROLS:
		_select_controls_family(_controls_family)
	if family == UiStyle.Family.TOUCH:
		return
	if page == PAGE_CONTROLS:
		(_controls_tabs[_controls_family] as Button).grab_focus()
	elif page == PAGE_SETTINGS and _first_setting != null:
		_first_setting.grab_focus()


# --- construction ------------------------------------------------------------


func _build(viewport_size: Vector2) -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_side_buttons.clear()
	_controls_tabs.clear()
	_value_labels.clear()
	_built_for = viewport_size
	_u = UiStyle.unit(self)
	theme = _make_theme()
	var safe := UiStyle.safe_rect(self)

	var dim := ColorRect.new()
	dim.color = UiStyle.with_alpha(UiStyle.INK, 0.74)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(dim)
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)

	var side_width := maxf(viewport_size.x * 0.34, 660.0 * _u)
	var side := PanelContainer.new()
	var side_style := StyleBoxFlat.new()
	side_style.bg_color = UiStyle.with_alpha(UiStyle.INK, 0.95)
	side_style.border_color = UiStyle.with_alpha(UiStyle.AMBER, 0.7)
	side_style.border_width_right = int(roundf(4.0 * _u))
	side.add_theme_stylebox_override("panel", side_style)
	add_child(side)
	side.position = Vector2.ZERO
	side.size = Vector2(safe.position.x + side_width, viewport_size.y)

	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", int(safe.position.x + 72.0 * _u))
	margin.add_theme_constant_override("margin_right", int(48.0 * _u))
	margin.add_theme_constant_override("margin_top", int(safe.position.y + 84.0 * _u))
	margin.add_theme_constant_override("margin_bottom", int(64.0 * _u))
	side.add_child(margin)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", int(14.0 * _u))
	margin.add_child(column)

	column.add_child(_label("SCRAPERX  /  KELLERWORKS TOWER", 22.0, UiStyle.PAPER_DIM, UiStyle.font_label()))
	column.add_child(_label("PAUSED", 88.0, UiStyle.PAPER, UiStyle.font_heavy()))
	_summary_label = _label(summary, 22.0, UiStyle.SAFE, UiStyle.font_label())
	_summary_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(_summary_label)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0.0, 36.0 * _u)
	column.add_child(gap)

	var entries := [[&"resume", "RESUME"], [PAGE_CONTROLS, "CONTROLS"], [PAGE_SETTINGS, "SETTINGS"]]
	if not OS.has_feature("mobile"):
		entries.append([&"quit", "QUIT TO DESKTOP"])
	for entry in entries:
		var button := _side_button(entry[1])
		column.add_child(button)
		_side_buttons[entry[0]] = button
	(_side_buttons[&"resume"] as Button).pressed.connect(func() -> void: resume_requested.emit())
	(_side_buttons[PAGE_CONTROLS] as Button).pressed.connect(_show_page.bind(PAGE_CONTROLS))
	(_side_buttons[PAGE_SETTINGS] as Button).pressed.connect(_show_page.bind(PAGE_SETTINGS))
	if _side_buttons.has(&"quit"):
		(_side_buttons[&"quit"] as Button).pressed.connect(func() -> void: quit_requested.emit())

	var filler := Control.new()
	filler.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(filler)
	_footer = Control.new()
	_footer.custom_minimum_size = Vector2(0.0, 56.0 * _u)
	_footer.draw.connect(_draw_footer)
	column.add_child(_footer)

	# The pages sit on their own plate: the dimmed world behind them can be
	# anything from night sky to sunlit concrete.
	var plate := Panel.new()
	var plate_style := StyleBoxFlat.new()
	plate_style.bg_color = UiStyle.with_alpha(UiStyle.INK, 0.86)
	plate.add_theme_stylebox_override("panel", plate_style)
	plate.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(plate)
	_plate = plate
	plate.position = Vector2(safe.position.x + side_width + 36.0 * _u, safe.position.y + 48.0 * _u)
	plate.size = Vector2(safe.end.x - plate.position.x - 36.0 * _u, safe.end.y - plate.position.y - 36.0 * _u)
	_content = Control.new()
	add_child(_content)
	_content.position = Vector2(safe.position.x + side_width + 72.0 * _u, safe.position.y + 84.0 * _u)
	_content.size = Vector2(safe.end.x - _content.position.x - 72.0 * _u,
		safe.end.y - _content.position.y - 64.0 * _u)
	_build_controls_page()
	_build_settings_page()


func _make_theme() -> Theme:
	var built := Theme.new()
	built.default_font = UiStyle.font_label()
	built.default_font_size = int(roundf(30.0 * _u))
	var normal := StyleBoxFlat.new()
	normal.bg_color = Color(0.0, 0.0, 0.0, 0.0)
	normal.border_color = UiStyle.with_alpha(UiStyle.PAPER_FAINT, 0.6)
	normal.border_width_left = int(roundf(4.0 * _u))
	normal.content_margin_left = 28.0 * _u
	normal.content_margin_right = 20.0 * _u
	normal.content_margin_top = 16.0 * _u
	normal.content_margin_bottom = 16.0 * _u
	var hot := normal.duplicate() as StyleBoxFlat
	hot.bg_color = UiStyle.with_alpha(UiStyle.AMBER, 0.16)
	hot.border_color = UiStyle.AMBER
	hot.border_width_left = int(roundf(10.0 * _u))
	var down := hot.duplicate() as StyleBoxFlat
	down.bg_color = UiStyle.with_alpha(UiStyle.AMBER, 0.55)
	# Focus draws over the current state: an amber keyline, nothing opaque.
	var focus := StyleBoxFlat.new()
	focus.draw_center = false
	focus.border_color = UiStyle.AMBER
	focus.border_width_left = int(roundf(10.0 * _u))
	focus.border_width_top = int(roundf(2.0 * _u))
	focus.border_width_bottom = int(roundf(2.0 * _u))
	focus.border_width_right = int(roundf(2.0 * _u))
	for kind in ["Button", "CheckButton"]:
		built.set_stylebox("normal", kind, normal)
		built.set_stylebox("hover", kind, hot)
		built.set_stylebox("pressed", kind, down)
		built.set_stylebox("hover_pressed", kind, down)
		built.set_stylebox("focus", kind, focus)
		built.set_stylebox("disabled", kind, normal)
		built.set_color("font_color", kind, UiStyle.PAPER)
		built.set_color("font_hover_color", kind, UiStyle.PAPER)
		built.set_color("font_focus_color", kind, UiStyle.PAPER)
		built.set_color("font_pressed_color", kind, UiStyle.PAPER)
		built.set_color("font_hover_pressed_color", kind, UiStyle.PAPER)
	built.set_color("font_color", "Label", UiStyle.PAPER)
	var track := StyleBoxFlat.new()
	track.bg_color = UiStyle.with_alpha(UiStyle.PAPER, 0.18)
	track.content_margin_top = 5.0 * _u
	track.content_margin_bottom = 5.0 * _u
	var filled := StyleBoxFlat.new()
	filled.bg_color = UiStyle.AMBER
	filled.content_margin_top = 5.0 * _u
	filled.content_margin_bottom = 5.0 * _u
	built.set_stylebox("slider", "HSlider", track)
	built.set_stylebox("grabber_area", "HSlider", filled)
	built.set_stylebox("grabber_area_highlight", "HSlider", filled)
	built.set_stylebox("focus", "HSlider", focus)
	var grabber := _disc_texture(int(roundf(40.0 * _u)), UiStyle.PAPER, UiStyle.AMBER)
	built.set_icon("grabber", "HSlider", grabber)
	built.set_icon("grabber_highlight", "HSlider", _disc_texture(int(roundf(44.0 * _u)),
		UiStyle.AMBER, UiStyle.PAPER))
	return built


func _disc_texture(diameter: int, fill: Color, rim: Color) -> ImageTexture:
	var image := Image.create_empty(diameter, diameter, false, Image.FORMAT_RGBA8)
	var r := float(diameter) * 0.5
	for y in diameter:
		for x in diameter:
			var d := Vector2(float(x) + 0.5 - r, float(y) + 0.5 - r).length()
			var edge := clampf(r - d, 0.0, 1.0)
			var color := rim if d > r - maxf(2.0, r * 0.18) else fill
			image.set_pixel(x, y, Color(color.r, color.g, color.b, color.a * edge))
	return ImageTexture.create_from_image(image)


func _label(text: String, size: float, color: Color, font: Font) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_override("font", font)
	label.add_theme_font_size_override("font_size", int(roundf(size * _u)))
	label.add_theme_color_override("font_color", color)
	return label


func _side_button(text: String) -> Button:
	var button := Button.new()
	button.text = text
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.focus_mode = Control.FOCUS_ALL
	button.add_theme_font_override("font", UiStyle.font_heavy())
	button.add_theme_font_size_override("font_size", int(roundf(40.0 * _u)))
	return button


func _build_controls_page() -> void:
	_controls_page = VBoxContainer.new()
	_controls_page.add_theme_constant_override("separation", int(24.0 * _u))
	_content.add_child(_controls_page)
	_controls_page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var tabs := HBoxContainer.new()
	tabs.add_theme_constant_override("separation", int(16.0 * _u))
	_controls_page.add_child(tabs)
	for entry in [[UiStyle.Family.TOUCH, "TOUCH"], [UiStyle.Family.XBOX, "CONTROLLER"],
			[UiStyle.Family.KEYBOARD, "KEYBOARD"]]:
		var tab := Button.new()
		tab.text = entry[1]
		tab.focus_mode = Control.FOCUS_ALL
		tab.add_theme_font_size_override("font_size", int(roundf(28.0 * _u)))
		tab.pressed.connect(_select_controls_family.bind(entry[0]))
		tab.focus_entered.connect(_select_controls_family.bind(entry[0]))
		tabs.add_child(tab)
		_controls_tabs[entry[0]] = tab
	_controls_view = Control.new()
	_controls_view.size_flags_vertical = Control.SIZE_EXPAND_FILL
	_controls_view.draw.connect(_draw_controls)
	_controls_page.add_child(_controls_view)
	_controls_page.visible = false


# Tabs are keyed TOUCH / XBOX / KEYBOARD; a PlayStation or Nintendo pad in
# use is drawn in its own glyphs on the controller tab.
func _canonical(value: int) -> int:
	if value in [UiStyle.Family.PLAYSTATION, UiStyle.Family.NINTENDO]:
		return UiStyle.Family.XBOX
	return value


func _view_family() -> int:
	if _controls_family == UiStyle.Family.XBOX and family in [UiStyle.Family.PLAYSTATION,
			UiStyle.Family.NINTENDO]:
		return family
	return _controls_family


func _select_controls_family(value: int) -> void:
	_controls_family = _canonical(value)
	for key in _controls_tabs:
		(_controls_tabs[key] as Button).add_theme_color_override("font_color",
			UiStyle.AMBER if key == _controls_family else UiStyle.PAPER)
	_controls_view.queue_redraw()


# [kind, value, name, description]; kind "verb" draws the shared binding,
# "key" a keycap, "pad" a pad glyph, "icon" a touch icon.
func _controls_rows(view_family: int) -> Array:
	if view_family == UiStyle.Family.TOUCH:
		return [
			["icon", &"move", "LEFT THUMB", "Move - the stick appears where you touch"],
			["icon", &"look", "RIGHT THUMB", "Look - drag anywhere free, even from Jump"],
			["icon", &"jump", "JUMP", "Jump; climbs up while hanging"],
			["icon", &"climb", "ACTION", "Climb a ledge, operate a pendant, work a valve"],
			["icon", &"drop", "DROP", "Let go of a ledge (only while hanging)"],
			["icon", &"chute", "CHUTE", "Open or stow the canopy (only while falling)"],
			["icon", &"operate", "PENDANT", "Hold the arrows to drive the machine; Action = done"],
			["icon", &"pause", "PAUSE", "This menu"],
		]
	var keyboard := view_family == UiStyle.Family.KEYBOARD
	return [
		["key", "WASD", "MOVE", "Walk and run"] if keyboard else ["pad", UiStyle.G_LSTICK_FWD, "MOVE", "Left stick"],
		["key", "MOUSE", "LOOK", "Click to capture the pointer"] if keyboard else ["pad", UiStyle.G_LSTICK_FWD, "LOOK", "Right stick"],
		["verb", &"jump", "JUMP", "Jump; climbs up while hanging"],
		["verb", &"action", "ACTION", "Climb a ledge, operate a pendant, work a valve"],
		["verb", &"drop", "DROP", "Let go of a ledge; leave pendant controls"],
		["verb", &"chute", "CHUTE", "Open or stow the canopy while falling"],
		["verb", &"hoist", "HOIST / RAISE", "Pendant up and down" + ("" if keyboard else "; triggers are analog")],
		["verb", &"slew", "SLEW / DRIVE", "Pendant left and right"],
		["verb", &"sling_release", "SLING", "Release (R) or attach (G) the pack" if keyboard else "Release or attach the pack at the yard jib"],
		["verb", &"pause", "PAUSE", "This menu"],
		["verb", &"telemetry", "TELEMETRY", "Native readouts overlay"],
	]


func _draw_controls() -> void:
	var view := _controls_view
	var view_family := _view_family()
	var font := UiStyle.font_label()
	var h := 50.0 * _u
	var y := 40.0 * _u
	var name_x := 190.0 * _u
	var desc_x := 520.0 * _u
	var name_size := int(roundf(28.0 * _u))
	var baseline_offset := (font.get_ascent(name_size) - font.get_descent(name_size)) * 0.5
	for row in _controls_rows(view_family):
		var left := Vector2(0.0, y)
		match row[0]:
			"icon":
				var c := left + Vector2(h * 0.5 + 40.0 * _u, 0.0)
				UiStyle.disc(view, c, h * 0.55, UiStyle.with_alpha(UiStyle.INK, 0.9))
				UiStyle.ring(view, c, h * 0.55, UiStyle.AMBER, 3.0 * _u)
				UiStyle.draw_icon(view, row[1], c, h * 0.45, UiStyle.PAPER, 3.5 * _u)
			"key":
				UiStyle.draw_glyph(view, UiStyle.Family.KEYBOARD, &"", row[1], left, h)
			"pad":
				UiStyle.draw_glyph(view, view_family, row[1], "", left, h)
			"verb":
				var used := UiStyle.draw_binding(view, view_family, row[1], left, h)
				if row[1] == &"sling_release" and view_family == UiStyle.Family.KEYBOARD:
					UiStyle.draw_binding(view, view_family, &"sling_attach", left + Vector2(used + 12.0 * _u, 0.0), h)
				elif row[1] == &"hoist" and view_family != UiStyle.Family.KEYBOARD:
					UiStyle.draw_binding(view, view_family, &"hoist_analog", left + Vector2(used + 12.0 * _u, 0.0), h)
		UiStyle.text(view, UiStyle.font_heavy(), row[2], Vector2(name_x, y + baseline_offset), name_size,
			UiStyle.PAPER)
		UiStyle.text(view, font, row[3], Vector2(desc_x, y + baseline_offset), int(roundf(22.0 * _u)),
			UiStyle.PAPER_DIM)
		y += 76.0 * _u


func _build_settings_page() -> void:
	_settings_page = VBoxContainer.new()
	_settings_page.add_theme_constant_override("separation", int(22.0 * _u))
	_content.add_child(_settings_page)
	_settings_page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_first_setting = _slider_row("LOOK SENSITIVITY", &"look_sensitivity",
		SettingsStore.LOOK_SENSITIVITY_RANGE, 0.05, "%.2fx")
	_slider_row("STICK SENSITIVITY", &"stick_sensitivity", SettingsStore.STICK_SENSITIVITY_RANGE,
		0.05, "%.2fx")
	_toggle_row("INVERT LOOK", &"invert_y")
	_slider_row("TOUCH CONTROL SIZE", &"touch_scale", SettingsStore.TOUCH_SCALE_RANGE, 0.05, "%.0f%%",
		100.0)
	_toggle_row("VIBRATION", &"vibration")
	_toggle_row("TELEMETRY OVERLAY", &"telemetry")
	var note := _label("Settings are saved on this device when you resume.", 21.0, UiStyle.PAPER_FAINT,
		UiStyle.font_label())
	_settings_page.add_child(note)
	_settings_page.visible = false


func _row(title: String) -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", int(28.0 * _u))
	var label := _label(title, 28.0, UiStyle.PAPER, UiStyle.font_label())
	label.custom_minimum_size = Vector2(520.0 * _u, 0.0)
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(label)
	_settings_page.add_child(row)
	return row


func _slider_row(title: String, key: StringName, bounds: Vector2, step: float, format: String,
		display_scale: float = 1.0) -> HSlider:
	var row := _row(title)
	var slider := HSlider.new()
	slider.min_value = bounds.x
	slider.max_value = bounds.y
	slider.step = step
	slider.value = float(settings.get(key))
	slider.focus_mode = Control.FOCUS_ALL
	slider.custom_minimum_size = Vector2(420.0 * _u, 56.0 * _u)
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(slider)
	var value_label := _label(format % (slider.value * display_scale), 28.0, UiStyle.AMBER, UiStyle.font_digits())
	value_label.custom_minimum_size = Vector2(140.0 * _u, 0.0)
	value_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(value_label)
	slider.value_changed.connect(func(value: float) -> void:
		settings.set(key, value)
		value_label.text = format % (value * display_scale)
		settings_changed.emit())
	return slider


func _toggle_row(title: String, key: StringName) -> Button:
	var row := _row(title)
	var toggle := Button.new()
	toggle.toggle_mode = true
	toggle.button_pressed = bool(settings.get(key))
	toggle.text = "ON" if toggle.button_pressed else "OFF"
	toggle.focus_mode = Control.FOCUS_ALL
	toggle.custom_minimum_size = Vector2(180.0 * _u, 0.0)
	toggle.add_theme_font_override("font", UiStyle.font_heavy())
	row.add_child(toggle)
	toggle.toggled.connect(func(on: bool) -> void:
		settings.set(key, on)
		toggle.text = "ON" if on else "OFF"
		settings_changed.emit())
	return toggle


func _draw_footer() -> void:
	var h := 40.0 * _u
	var y := _footer.size.y * 0.5
	var x := 0.0
	var font := UiStyle.font_label()
	var size := int(roundf(22.0 * _u))
	var baseline := y + (font.get_ascent(size) - font.get_descent(size)) * 0.5
	if family == UiStyle.Family.TOUCH:
		UiStyle.text(_footer, font, "TAP TO SELECT", Vector2(0.0, baseline), size, UiStyle.PAPER_DIM)
		return
	var pairs: Array = []
	if family == UiStyle.Family.KEYBOARD:
		pairs = [["ENTER", "SELECT"], ["ESC", "BACK"]]
	else:
		pairs = [[UiStyle.G_SOUTH, "SELECT"], [UiStyle.G_EAST, "BACK"], [UiStyle.G_START, "RESUME"]]
	for pair in pairs:
		if family == UiStyle.Family.KEYBOARD:
			x += UiStyle.draw_glyph(_footer, family, &"", pair[0], Vector2(x, y), h)
		else:
			x += UiStyle.draw_glyph(_footer, family, pair[0], "", Vector2(x, y), h)
		x += 12.0 * _u
		UiStyle.text(_footer, font, pair[1], Vector2(x, baseline), size, UiStyle.PAPER_DIM)
		x += UiStyle.text_width(font, pair[1], size) + 34.0 * _u
