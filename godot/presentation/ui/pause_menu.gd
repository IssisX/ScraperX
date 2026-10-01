extends Control

# The pause surface. Built from real Godot controls with a generated theme so
# focus navigation is the engine's own: D-pad / stick / arrows move, A /
# Enter / tap selects, B / Esc backs out, Menu / Start resumes. Opening it
# pauses the SceneTree, which stops main.gd's _process and with it every
# native advance_frame call -- the simulation is frozen, not slowed.

signal resume_requested
signal quit_requested
signal settings_changed
signal checkpoint_restart_requested
signal restart_requested(position: Vector3)

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const SettingsStore := preload("res://presentation/ui/settings_store.gd")

const PAGE_NONE := &""
const PAGE_CONTROLS := &"controls"
const PAGE_SETTINGS := &"settings"
const PAGE_GRAPHICS := &"graphics"
const PAGE_DISPLAY := &"display"
const PAGE_AUDIO := &"audio"
const PAGE_RESTART := &"restart"
const SETTING_PAGES := [PAGE_SETTINGS, PAGE_GRAPHICS, PAGE_DISPLAY, PAGE_AUDIO, PAGE_RESTART]

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
var _controls_family := UiStyle.Family.TOUCH
var _settings_page: Control
var _first_setting: Control
# Every settings-style page, its first focusable row, the page rows are
# being added to while building, and how to redraw a row from `settings`.
var _pages := {}
var _page_first := {}
var _building: VBoxContainer
var _refreshers := {}
var _value_labels := {}
var _footer: Control
var _stack_rows := false
var _save_timer: Timer
var _restart_status: Label
var _restart_current_position := Vector3.ZERO


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	visible = false
	_save_timer = Timer.new()
	_save_timer.one_shot = true
	_save_timer.wait_time = 0.4
	_save_timer.process_mode = Node.PROCESS_MODE_ALWAYS
	_save_timer.timeout.connect(func() -> void: settings.save_to_disk())
	add_child(_save_timer)
	get_viewport().size_changed.connect(_resize_open_menu)


func _resize_open_menu() -> void:
	if not visible or get_viewport_rect().size == _built_for:
		return
	var selected := _page
	_build(get_viewport_rect().size)
	_summary_label.text = summary
	_show_page(selected)


func open(_current_family: int, current_summary: String) -> void:
	family = UiStyle.Family.TOUCH
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
	if is_instance_valid(_save_timer):
		_save_timer.stop()
	settings.save_to_disk()
	visible = false
	var focused := get_viewport().gui_get_focus_owner()
	if focused != null and is_ancestor_of(focused):
		focused.release_focus()


func set_restart_context(position: Vector3) -> void:
	_restart_current_position = position


func set_restart_result(success: bool, message: String = "") -> void:
	if _restart_status == null:
		return
	_restart_status.text = message if not message.is_empty() else (
		"Restarted." if success else "Position blocked. Choose a clear location and try again.")
	_restart_status.add_theme_color_override("font_color", UiStyle.SAFE if success else UiStyle.HAZARD)


func _changed() -> void:
	settings_changed.emit()
	if is_instance_valid(_save_timer):
		_save_timer.start()


func press_resume() -> void:
	resume_requested.emit()


func resume_button_center() -> Vector2:
	var button: Button = _side_buttons[&"resume"]
	return button.get_global_rect().get_center()


func side_button_center(id: StringName) -> Vector2:
	var button: Button = _side_buttons[id]
	return button.get_global_rect().get_center()


func page_first_center(page: StringName) -> Vector2:
	var control: Control = _page_first[page]
	return control.global_position + control.size * 0.5


func current_page() -> StringName:
	return _page


# Reports inaccessible overflow. Taller pages are accessible through their
# scroll container, including automatic scrolling when pad focus moves.
func settings_overflow() -> float:
	var worst := 0.0
	for page in _pages:
		var scroll := _pages[page] as ScrollContainer
		if scroll == null:
			worst = maxf(worst, (_pages[page] as Control).get_combined_minimum_size().y - _content.size.y)
		elif scroll.horizontal_scroll_mode == ScrollContainer.SCROLL_MODE_DISABLED:
			worst = maxf(worst, scroll.get_child(0).get_combined_minimum_size().x - scroll.size.x)
	return worst


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
	for id in _pages:
		(_pages[id] as Control).visible = page == id
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
	elif _page_first.has(page):
		(_page_first[page] as Control).grab_focus()


# --- construction ------------------------------------------------------------


func _build(viewport_size: Vector2) -> void:
	for child in get_children():
		if child == _save_timer:
			continue
		remove_child(child)
		child.queue_free()
	_side_buttons.clear()
	_controls_tabs.clear()
	_value_labels.clear()
	_pages.clear()
	_page_first.clear()
	_refreshers.clear()
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
	var side_scroll := ScrollContainer.new()
	side_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	side_scroll.follow_focus = true
	margin.add_child(side_scroll)
	var column := VBoxContainer.new()
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_theme_constant_override("separation", int(14.0 * _u))
	side_scroll.add_child(column)

	var brand := _label("SCRAPERX  /  KELLERWORKS TOWER", 22.0, UiStyle.PAPER_DIM, UiStyle.font_label())
	brand.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(brand)
	column.add_child(_label("PAUSED", 88.0, UiStyle.PAPER, UiStyle.font_heavy()))
	_summary_label = _label(summary, 22.0, UiStyle.SAFE, UiStyle.font_label())
	_summary_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(_summary_label)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0.0, 36.0 * _u)
	column.add_child(gap)

	var entries := [[&"resume", "RESUME"], [PAGE_CONTROLS, "CONTROLS"], [PAGE_SETTINGS, "SETTINGS"],
		[PAGE_GRAPHICS, "GRAPHICS"], [PAGE_DISPLAY, "DISPLAY"], [PAGE_AUDIO, "AUDIO"],
		[PAGE_RESTART, "RESTART / SPAWN"]]
	if not OS.has_feature("mobile"):
		entries.append([&"quit", "QUIT TO DESKTOP"])
	for entry in entries:
		var button := _side_button(entry[1])
		column.add_child(button)
		_side_buttons[entry[0]] = button
	(_side_buttons[&"resume"] as Button).pressed.connect(func() -> void: resume_requested.emit())
	(_side_buttons[PAGE_CONTROLS] as Button).pressed.connect(_show_page.bind(PAGE_CONTROLS))
	for page in SETTING_PAGES:
		(_side_buttons[page] as Button).pressed.connect(_show_page.bind(page))
	if _side_buttons.has(&"quit"):
		(_side_buttons[&"quit"] as Button).pressed.connect(func() -> void:
			settings.save_to_disk()
			quit_requested.emit())

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
	_stack_rows = _content.size.x < 1020.0 * _u
	_build_controls_page()
	_build_settings_page()
	_build_graphics_page()
	_build_display_page()
	_build_audio_page()
	_build_restart_page()


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
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.follow_focus = true
	_controls_page = scroll
	_content.add_child(_controls_page)
	_controls_page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var column := VBoxContainer.new()
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_theme_constant_override("separation", int(24.0 * _u))
	scroll.add_child(column)
	var tabs := HBoxContainer.new()
	tabs.add_theme_constant_override("separation", int(16.0 * _u))
	column.add_child(tabs)
	for entry in [[UiStyle.Family.TOUCH, "TOUCH"]]:
		var tab := Button.new()
		tab.text = entry[1]
		tab.focus_mode = Control.FOCUS_ALL
		tab.add_theme_font_size_override("font_size", int(roundf(28.0 * _u)))
		tab.pressed.connect(_select_controls_family.bind(entry[0]))
		tab.focus_entered.connect(_select_controls_family.bind(entry[0]))
		tabs.add_child(tab)
		_controls_tabs[entry[0]] = tab
	_controls_view = Control.new()
	_controls_view.custom_minimum_size.y = 1300.0 * _u
	_controls_view.draw.connect(_draw_controls)
	_controls_view.resized.connect(_refresh_controls_layout)
	column.add_child(_controls_view)
	_controls_page.visible = false


# Gameplay help follows the touch-only owner direction. Existing saved
# input bindings remain available in settings and in regression fixtures.
func _canonical(_value: int) -> int:
	return UiStyle.Family.TOUCH


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
	_refresh_controls_layout()
	_controls_view.queue_redraw()


func _controls_row_height(description: String) -> float:
	var description_x := (190.0 if _stack_rows else 520.0) * _u
	var width := maxf(1.0, _controls_view.size.x - description_x)
	var text_size := UiStyle.font_label().get_multiline_string_size(description,
		HORIZONTAL_ALIGNMENT_LEFT, width, int(roundf(22.0 * _u)))
	return maxf((114.0 if _stack_rows else 76.0) * _u,
		(78.0 if _stack_rows else 48.0) * _u + text_size.y)


func _refresh_controls_layout() -> void:
	if not is_instance_valid(_controls_view):
		return
	var height := 80.0 * _u
	for row in _controls_rows(_view_family()):
		height += _controls_row_height(row[3])
	_controls_view.custom_minimum_size.y = height
	_controls_view.queue_redraw()


# [kind, value, name, description]; kind "verb" draws the shared binding,
# "key" a keycap, "pad" a pad glyph, "icon" a touch icon.
func _controls_rows(_view_family: int) -> Array:
	return [
		["icon", &"move", "LEFT THUMB", "Move; the stick appears where you touch"],
		["icon", &"look", "RIGHT THUMB", "Drag to look; Gyro Aim adds tilt"],
		["icon", &"sling", "BOARD", "Stand in the leather pouch, then tap BOARD"],
		["icon", &"look", "AIM", "Drag on the right while the bands are slack; aim locks when charged"],
		["icon", &"move", "DRAW", "Pull the left stick backward; watch the power bar, then let go to hold"],
		["icon", &"sling", "RELEASE", "Tap RELEASE to fire the chosen stretch"],
		["icon", &"chute", "CHUTE / BRAKE", "After the pouch exit, open the canopy to brake ascent or descent; tap again to stow"],
		["icon", &"operate", "RETRIEVE", "At the grade control, tap Action and pull backward to return the pouch"],
		["icon", &"drop", "DROP", "Leave the pouch or a ledge; stop retrieval"],
		["icon", &"jump", "JUMP", "Jump; climb up while hanging"],
		["icon", &"crouch", "CROUCH", "Duck under low gaps; tap again to stand"],
		["icon", &"climb", "ACTION", "Catch a ledge or operate a machine"],
		["icon", &"move", "LANDING", "Impact and sideways speed affect balance; find your footing before moving on"],
		["icon", &"operate", "MACHINE", "Hold its arrows or move its load; Action leaves pendant controls"],
		["icon", &"pause", "PAUSE", "Options, checkpoint and supported-ring restarts"],
	]


func _draw_controls() -> void:
	var view := _controls_view
	var view_family := _view_family()
	var font := UiStyle.font_label()
	var h := 50.0 * _u
	var y := 40.0 * _u
	var name_x := 190.0 * _u
	var desc_x := (190.0 if _stack_rows else 520.0) * _u
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
		view.draw_multiline_string(font, Vector2(desc_x, y + baseline_offset + (30.0 * _u if _stack_rows else 0.0)),
			row[3], HORIZONTAL_ALIGNMENT_LEFT, maxf(1.0, view.size.x - desc_x), int(roundf(22.0 * _u)),
			-1, UiStyle.PAPER_DIM)
		y += _controls_row_height(row[3])


func _begin_page(page: StringName) -> VBoxContainer:
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.follow_focus = true
	_content.add_child(scroll)
	scroll.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	scroll.visible = false
	var box := VBoxContainer.new()
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	box.add_theme_constant_override("separation", int(22.0 * _u))
	scroll.add_child(box)
	_pages[page] = scroll
	_building = box
	box.add_child(_label(String(page).to_upper(), 40.0, UiStyle.AMBER, UiStyle.font_heavy()))
	return box


func _end_page(page: StringName, first: Control, note: String) -> void:
	_page_first[page] = first
	if not note.is_empty():
		var label := _label(note, 21.0, UiStyle.PAPER_DIM, UiStyle.font_label())
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		_building.add_child(label)


func _build_settings_page() -> void:
	_settings_page = _begin_page(PAGE_SETTINGS)
	_first_setting = _slider_row("LOOK SENSITIVITY", &"look_sensitivity",
		SettingsStore.LOOK_SENSITIVITY_RANGE, 0.05, "%.2fx")
	_slider_row("STICK SENSITIVITY", &"stick_sensitivity", SettingsStore.STICK_SENSITIVITY_RANGE,
		0.05, "%.2fx")
	_toggle_row("INVERT LOOK", &"invert_y")
	_toggle_row("GYRO AIM", &"gyro_aim")
	_slider_row("GYRO SENSITIVITY", &"gyro_sensitivity", SettingsStore.GYRO_SENSITIVITY_RANGE, 0.05,
		"%.2fx")
	_slider_row("TOUCH CONTROL SIZE", &"touch_scale", SettingsStore.TOUCH_SCALE_RANGE, 0.05, "%.0f%%",
		100.0)
	_toggle_row("VIBRATION", &"vibration")
	_toggle_row("TELEMETRY OVERLAY", &"telemetry")
	_end_page(PAGE_SETTINGS, _first_setting, "Changes apply immediately and are saved on this device. Scroll for more options.")


# The preset writes its detail/render rows; touching any makes it
# CUSTOM. Neither direction re-emits, so one change is one apply.
func _build_graphics_page() -> void:
	_begin_page(PAGE_GRAPHICS)
	var first := _choice_row("QUALITY", &"quality", SettingsStore.QUALITY_NAMES,
		func() -> void:
			settings.apply_quality(settings.quality)
			for key in [&"render_scale", &"shadow_quality", &"msaa", &"bloom", &"detail_distance", &"view_distance"]:
				_refreshers[key].call())
	var to_custom := func() -> void:
		if settings.quality != SettingsStore.QUALITY_CUSTOM:
			settings.quality = SettingsStore.QUALITY_CUSTOM
			_refreshers[&"quality"].call()
	_slider_row("RENDER SCALE", &"render_scale", SettingsStore.RENDER_SCALE_RANGE, 0.05, "%.0f%%",
		100.0, to_custom)
	_choice_row("SHADOWS", &"shadow_quality", SettingsStore.SHADOW_NAMES, to_custom)
	_choice_row("ANTI-ALIASING", &"msaa", SettingsStore.MSAA_NAMES, to_custom)
	_toggle_row("BLOOM", &"bloom", to_custom)
	_choice_row("DETAIL / LOD", &"detail_distance", SettingsStore.DETAIL_DISTANCE_NAMES, to_custom)
	_slider_row("DRAW DISTANCE", &"view_distance", SettingsStore.VIEW_DISTANCE_RANGE, 100.0,
		"%.0f M", 1.0, to_custom)
	_choice_row("VSYNC", &"vsync", SettingsStore.VSYNC_NAMES)
	_choice_row("FRAME RATE CAP", &"fps_cap", SettingsStore.FPS_CAP_NAMES)
	_toggle_row("SHOW FPS", &"show_fps")
	_end_page(PAGE_GRAPHICS, first,
		"Lower render scale, shadows and detail first if the device runs hot. Detail / LOD reduces distant decoration; physics stays active. Draw distance controls the rendered horizon.")


func _build_display_page() -> void:
	_begin_page(PAGE_DISPLAY)
	var first := _slider_row("FIELD OF VIEW", &"fov", SettingsStore.FOV_RANGE, 1.0, "%.0f")
	_slider_row("BRIGHTNESS", &"brightness", SettingsStore.BRIGHTNESS_RANGE, 0.05, "%.2fx")
	_toggle_row("HEAD BOB", &"head_bob")
	_toggle_row("LAUNCH CINEMATICS", &"launch_cinematics")
	_toggle_row("SPEED FOV KICK", &"speed_fov")
	_choice_row("TIME OF DAY", &"time_of_day", SettingsStore.TIME_OF_DAY_NAMES)
	_slider_row("DAY LENGTH", &"day_minutes", SettingsStore.DAY_MINUTES_RANGE, 1.0, "%.0f MIN")
	if not OS.has_feature("mobile"):
		_choice_row("WINDOW MODE", &"window_mode", SettingsStore.WINDOW_MODE_NAMES)
	_end_page(PAGE_DISPLAY, first, "")


func _build_audio_page() -> void:
	_begin_page(PAGE_AUDIO)
	var first := _toggle_row("MUTE ALL", &"audio_muted")
	_slider_row("MASTER", &"master_volume", SettingsStore.VOLUME_RANGE, 0.05, "%.0f%%", 100.0)
	_slider_row("EFFECTS", &"effects_volume", SettingsStore.VOLUME_RANGE, 0.05, "%.0f%%", 100.0)
	_slider_row("AMBIENCE", &"ambience_volume", SettingsStore.VOLUME_RANGE, 0.05, "%.0f%%", 100.0)
	_slider_row("INTERFACE", &"interface_volume", SettingsStore.VOLUME_RANGE, 0.05, "%.0f%%", 100.0)
	_end_page(PAGE_AUDIO, first, "Footsteps, landings, grabs and machines are Effects; wind and hum are Ambience.")


func _build_restart_page() -> void:
	_begin_page(PAGE_RESTART)
	var checkpoint := _action_row("LAST SAFE CHECKPOINT", "RESTART AT CHECKPOINT",
		func() -> void: checkpoint_restart_requested.emit())
	_ring_choice_row()
	_action_row("RING DESTINATION", "RESTART ON RING", func() -> void:
		restart_requested.emit(settings.ring_position()))
	_action_row("CURRENT LOCATION", "USE CURRENT XYZ", func() -> void:
		settings.restart_x = _restart_current_position.x
		settings.restart_y = _restart_current_position.y
		settings.restart_z = _restart_current_position.z
		for key in [&"restart_x", &"restart_y", &"restart_z"]:
			_refreshers[key].call()
		_changed())
	_number_row("TOWER HEIGHT", &"restart_height", SettingsStore.RESTART_HEIGHT_RANGE, 0.5, " M")
	_choice_row("TOWER SIDE", &"restart_side", SettingsStore.RESTART_SIDE_NAMES)
	_number_row("OUTSIDE FACE", &"restart_clearance", SettingsStore.RESTART_CLEARANCE_RANGE, 0.5, " M")
	_action_row("HEIGHT + SIDE", "RESTART THERE", func() -> void:
		restart_requested.emit(settings.tower_restart_position()))
	_number_row("EXACT X", &"restart_x", SettingsStore.RESTART_COORDINATE_RANGE, 0.1, " M")
	_number_row("EXACT Y (CAPSULE CENTRE)", &"restart_y", SettingsStore.RESTART_COORDINATE_RANGE, 0.1, " M")
	_number_row("EXACT Z", &"restart_z", SettingsStore.RESTART_COORDINATE_RANGE, 0.1, " M")
	_action_row("EXACT COORDINATES", "RESTART AT XYZ", func() -> void:
		restart_requested.emit(settings.exact_restart_position()))
	_restart_status = _label("", 22.0, UiStyle.PAPER_DIM, UiStyle.font_label())
	_restart_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_building.add_child(_restart_status)
	_end_page(PAGE_RESTART, checkpoint,
		"Ring presets put you on existing tower decks. Arbitrary height or XYZ can have no floor: you can fall. Height is above grade; XYZ uses the capsule centre. Occupied positions are rejected. Press a restart button to apply the chosen destination.")


# The expanded receiving frame has many rings. A directly selectable list
# avoids cycling through every lower ring to choose a high restart point.
func _ring_choice_row() -> OptionButton:
	var row := _row("SUPPORTED TOWER RING")
	var choice := OptionButton.new()
	choice.focus_mode = Control.FOCUS_ALL
	choice.custom_minimum_size = Vector2(300.0 * _u, 0.0)
	choice.add_theme_font_override("font", UiStyle.font_heavy())
	for caption in SettingsStore.RESTART_RING_NAMES:
		choice.add_item(caption)
	choice.select(settings.restart_ring)
	choice.get_popup().add_theme_font_override("font", UiStyle.font_label())
	choice.get_popup().add_theme_font_size_override("font_size", int(roundf(28.0 * _u)))
	choice.item_selected.connect(func(index: int) -> void:
		settings.restart_ring = index
		_changed())
	row.add_child(choice)
	_refreshers[&"restart_ring"] = func() -> void: choice.select(settings.restart_ring)
	return choice


func _action_row(title: String, caption: String, action: Callable) -> Button:
	var row := _row(title)
	var button := Button.new()
	button.text = caption
	button.focus_mode = Control.FOCUS_ALL
	button.add_theme_font_override("font", UiStyle.font_heavy())
	button.add_theme_font_size_override("font_size", int(roundf(25.0 * _u)))
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.pressed.connect(action)
	row.add_child(button)
	return button


func _number_row(title: String, key: StringName, bounds: Vector2, step: float, suffix: String) -> SpinBox:
	var row := _row(title)
	var number := SpinBox.new()
	number.min_value = bounds.x
	number.max_value = bounds.y
	number.step = step
	number.suffix = suffix
	number.value = float(settings.get(key))
	number.custom_minimum_size = Vector2(300.0 * _u, 64.0 * _u)
	number.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	number.value_changed.connect(func(value: float) -> void:
		settings.set(key, value)
		_changed())
	row.add_child(number)
	_refreshers[key] = func() -> void: number.set_value_no_signal(float(settings.get(key)))
	return number


func _row(title: String) -> BoxContainer:
	var row: BoxContainer = VBoxContainer.new() if _stack_rows else HBoxContainer.new()
	row.add_theme_constant_override("separation", int(16.0 * _u))
	var label := _label(title, 28.0, UiStyle.PAPER, UiStyle.font_label())
	label.custom_minimum_size = Vector2(0.0 if _stack_rows else 410.0 * _u, 0.0)
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	row.add_child(label)
	_building.add_child(row)
	return row


func _slider_row(title: String, key: StringName, bounds: Vector2, step: float, format: String,
		display_scale: float = 1.0, on_change: Callable = Callable()) -> HSlider:
	var row := _row(title)
	var values := HBoxContainer.new()
	values.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	values.add_theme_constant_override("separation", int(16.0 * _u))
	row.add_child(values)
	var slider := HSlider.new()
	slider.min_value = bounds.x
	slider.max_value = bounds.y
	slider.step = step
	slider.value = float(settings.get(key))
	slider.focus_mode = Control.FOCUS_ALL
	slider.custom_minimum_size = Vector2(260.0 * _u, 56.0 * _u)
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	values.add_child(slider)
	var value_label := _label(format % (slider.value * display_scale), 28.0, UiStyle.AMBER, UiStyle.font_digits())
	value_label.custom_minimum_size = Vector2(140.0 * _u, 0.0)
	value_label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	values.add_child(value_label)
	slider.value_changed.connect(func(value: float) -> void:
		settings.set(key, value)
		value_label.text = format % (value * display_scale)
		if on_change.is_valid():
			on_change.call()
		_changed())
	_refreshers[key] = func() -> void:
		slider.set_value_no_signal(float(settings.get(key)))
		value_label.text = format % (slider.value * display_scale)
	return slider


func _toggle_row(title: String, key: StringName, on_change: Callable = Callable()) -> Button:
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
		if on_change.is_valid():
			on_change.call()
		_changed())
	_refreshers[key] = func() -> void:
		toggle.set_pressed_no_signal(bool(settings.get(key)))
		toggle.text = "ON" if toggle.button_pressed else "OFF"
	return toggle


# A named option that steps to the next on each press (wrapping), so it is one
# tap on touch and one A / Enter on a pad or keyboard.
func _choice_row(title: String, key: StringName, names: Array, on_change: Callable = Callable()) -> Button:
	var row := _row(title)
	var choice := Button.new()
	choice.text = names[int(settings.get(key))]
	choice.focus_mode = Control.FOCUS_ALL
	choice.custom_minimum_size = Vector2(300.0 * _u, 0.0)
	choice.add_theme_font_override("font", UiStyle.font_heavy())
	row.add_child(choice)
	choice.pressed.connect(func() -> void:
		settings.set(key, (int(settings.get(key)) + 1) % names.size())
		if on_change.is_valid():
			on_change.call()
		choice.text = names[int(settings.get(key))]
		_changed())
	_refreshers[key] = func() -> void:
		choice.text = names[int(settings.get(key))]
	return choice


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
