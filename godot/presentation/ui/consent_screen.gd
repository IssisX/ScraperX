extends Control

# The microphone, asked once and honestly, outside the fiction (HER_CHART.md:
# "Consent is honest and up front"): what is heard, what is kept and for how
# long, that nothing is uploaded, and how to turn it off. Shown the first
# time the game is played, over everything, the simulation frozen behind it
# (main.gd). ALLOW goes on to the phone's own permission request; NOT NOW
# leaves the microphone off. Either way the answer is the MICROPHONE setting
# on the AUDIO page, and the game is whole without it.

signal answered(allow: bool)

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const PauseMenu := preload("res://presentation/ui/pause_menu.gd")
const VoiceKeeper := preload("res://presentation/uncanny/voice_keeper.gd")

const TITLE_TEXT := "THE MICROPHONE"
const ALLOW_TEXT := "ALLOW MICROPHONE"
const DECLINE_TEXT := "NOT NOW"

var _u := 1.0
var _buttons := {}
var body_text := ""


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP


# What the screen says, word for word: kept here so the scripted run can hold
# it to its promises.
static func explanation() -> String:
	return "\n\n".join(PackedStringArray([
		"This game can listen through this phone's microphone while you play, and only while you play.",
		"WHAT IS HEARD: whatever the microphone picks up, usually your own voice. When a sound is loud enough, the game keeps a few seconds of it.",
		"WHAT IS KEPT: those short clips, on this phone only, in the game's own storage. At most %d of them, and each is deleted within 7 days." % VoiceKeeper.MAX_CLIPS,
		"WHAT HAPPENS TO THEM: now and then, much later, the game may play one back to you, quietly, somewhere in the tower.",
		"WHAT ELSE: how loud your room is, moment to moment, is part of the game. A loud sound where you are can be heard in the tower. Only how loud is used for this, never what was said, and none of it is kept.",
		"NOTHING IS UPLOADED. Nothing is sent anywhere, and nothing else on this phone is read. If that ever changes, you will be asked again first.",
		"TO TURN IT OFF: PAUSE > AUDIO > MICROPHONE. Turning it off deletes every clip kept.",
		"The game is complete without it. If you allow it, Android will ask you to confirm.",
	]))


func open() -> void:
	_build(get_viewport_rect().size)
	visible = true
	(_buttons[&"decline"] as Button).grab_focus()


func close() -> void:
	visible = false
	var focused := get_viewport().gui_get_focus_owner()
	if focused != null and is_ancestor_of(focused):
		focused.release_focus()


func button_center(id: StringName) -> Vector2:
	return (_buttons[id] as Button).get_global_rect().get_center()


func _build(viewport_size: Vector2) -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	_buttons.clear()
	_u = UiStyle.unit(self)
	theme = PauseMenu.menu_theme(_u)
	var safe := UiStyle.safe_rect(self)
	var dim := ColorRect.new()
	dim.color = UiStyle.with_alpha(UiStyle.INK, 0.96)
	dim.mouse_filter = Control.MOUSE_FILTER_STOP
	add_child(dim)
	dim.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", int(safe.position.x + 120.0 * _u))
	margin.add_theme_constant_override("margin_right", int(viewport_size.x - safe.end.x + 120.0 * _u))
	margin.add_theme_constant_override("margin_top", int(safe.position.y + 96.0 * _u))
	margin.add_theme_constant_override("margin_bottom", int(viewport_size.y - safe.end.y + 72.0 * _u))
	add_child(margin)
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", int(22.0 * _u))
	margin.add_child(column)
	column.add_child(_label("BEFORE YOU PLAY", 24.0, UiStyle.AMBER, UiStyle.font_label()))
	column.add_child(_label(TITLE_TEXT, 72.0, UiStyle.PAPER, UiStyle.font_heavy()))
	body_text = explanation()
	var body := _label(body_text, 27.0, UiStyle.PAPER_DIM, UiStyle.font_label())
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_child(body)
	var gap := Control.new()
	gap.custom_minimum_size = Vector2(0.0, 24.0 * _u)
	column.add_child(gap)
	var row := HBoxContainer.new()
	row.add_theme_constant_override("separation", int(36.0 * _u))
	column.add_child(row)
	for entry in [[&"allow", ALLOW_TEXT, true], [&"decline", DECLINE_TEXT, false]]:
		var button := Button.new()
		button.text = entry[1]
		button.focus_mode = Control.FOCUS_ALL
		button.custom_minimum_size = Vector2(520.0 * _u, 0.0)
		button.add_theme_font_override("font", UiStyle.font_heavy())
		button.add_theme_font_size_override("font_size", int(roundf(40.0 * _u)))
		var allow: bool = entry[2]
		button.pressed.connect(func() -> void: answered.emit(allow))
		row.add_child(button)
		_buttons[entry[0]] = button


func _label(text: String, size: float, color: Color, font: Font) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_override("font", font)
	label.add_theme_font_size_override("font_size", int(roundf(size * _u)))
	label.add_theme_color_override("font_color", color)
	return label
