extends Control

# Presentation only: the story owner supplies every fact and every lifetime.
# No process/input callback, timer, persistence, or mission state lives here.
const UiStyle := preload("res://presentation/ui/ui_style.gd")
const CachedStoryText := preload("res://presentation/ui/cached_story_text.gd")
const FADE_SECONDS := 0.35
const RADIO_ACCENT_SECONDS := 0.6

var _intro: Control
var _intro_title: Label
var _intro_subtitle: Label
var _objective: Control
var _chapter: Label
var _objective_text: Label
var _detail: Label
var _radio: Control
var _speaker: Label
var _transmission: Label
var _built := false
var _intro_alpha := 0.0
var _radio_accent := 0.0
var _has_transmission_id := false
var _transmission_id := 0
var _objective_id := &""
var _last_safe := Rect2()


func _ready() -> void:
	_build()


func _build() -> void:
	if _built:
		return
	_built = true
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	focus_mode = Control.FOCUS_NONE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_intro = _cached_group("Opening")
	_intro_title = _label(_intro.content, "Title", UiStyle.font_heavy(), UiStyle.PAPER)
	_intro_subtitle = _label(_intro.content, "Purpose", UiStyle.font_label(), UiStyle.PAPER_DIM)
	_intro_title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_intro_subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_objective = _cached_group("Objective")
	_chapter = _label(_objective.content, "Chapter", UiStyle.font_label(), UiStyle.AMBER)
	_objective_text = _label(_objective.content, "Direction", UiStyle.font_heavy(), UiStyle.PAPER)
	_detail = _label(_objective.content, "Detail", UiStyle.font_label(), UiStyle.PAPER_DIM)
	_radio = _group("Radio")
	_speaker = _label(_radio, "Speaker", UiStyle.font_label(), UiStyle.SAFE)
	_transmission = _label(_radio, "Transmission", UiStyle.font_label(), UiStyle.PAPER)
	_intro.hide()
	_objective.hide()
	_radio.hide()


func _cached_group(node_name: String) -> Control:
	var group := CachedStoryText.new()
	group.name = node_name
	add_child(group)
	return group


func _group(node_name: String) -> Control:
	var group := Control.new()
	group.name = node_name
	group.mouse_filter = Control.MOUSE_FILTER_IGNORE
	group.focus_mode = Control.FOCUS_NONE
	add_child(group)
	return group


func _label(parent: Control, node_name: String, font: Font, color: Color) -> Label:
	var label := Label.new()
	label.name = node_name
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	label.focus_mode = Control.FOCUS_NONE
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.add_theme_font_override("font", font)
	label.add_theme_color_override("font_color", color)
	label.add_theme_color_override("font_shadow_color", UiStyle.SHADOW)
	label.add_theme_constant_override("shadow_offset_x", 1)
	label.add_theme_constant_override("shadow_offset_y", 2)
	parent.add_child(label)
	return label


func _copy(state: Dictionary, key: String) -> String:
	var value: Variant = state.get(key, "")
	return String(value).strip_edges() if value is String or value is StringName else ""


func present(state: Dictionary, delta: float) -> void:
	_build()
	var intro_changed := _intro_title.text != _copy(state, "intro_title") or _intro_subtitle.text != _copy(state, "intro_subtitle")
	var objective_changed := _chapter.text != _copy(state, "chapter") or _objective_text.text != _copy(state, "objective") or _detail.text != _copy(state, "detail")
	var step := maxf(delta, 0.0) if is_finite(delta) else 0.0
	_intro_title.text = _copy(state, "intro_title")
	_intro_subtitle.text = _copy(state, "intro_subtitle")
	var intro_requested: bool = state.get("intro_visible", false) == true and not _intro_title.text.is_empty()
	# Empty state is cancellation: never leave old words fading over a new run.
	if _intro_title.text.is_empty():
		_intro_alpha = 0.0
	else:
		_intro_alpha = move_toward(_intro_alpha, 1.0 if intro_requested else 0.0, step / FADE_SECONDS)
	_intro.visible = _intro_alpha > 0.0
	_intro.modulate.a = _intro_alpha

	_objective_id = StringName(_copy(state, "objective_id"))
	_chapter.text = _copy(state, "chapter")
	_objective_text.text = _copy(state, "objective")
	_detail.text = _copy(state, "detail")
	_objective.visible = not _objective_text.text.is_empty()

	var line := _copy(state, "transmission")
	var id_value: Variant = state.get("transmission_id", 0)
	var valid_id := id_value is int
	if line.is_empty() or not valid_id:
		_radio.hide()
		_speaker.text = ""
		_transmission.text = ""
		_radio_accent = 0.0
	else:
		var next_id: int = id_value
		if not _has_transmission_id or next_id != _transmission_id:
			_radio_accent = 1.0
			_transmission_id = next_id
			_has_transmission_id = true
		else:
			_radio_accent = move_toward(_radio_accent, 0.0, step / RADIO_ACCENT_SECONDS)
		_speaker.text = _copy(state, "speaker")
		_transmission.text = line
		_radio.show()
	_speaker.modulate.a = 0.72 + 0.28 * _radio_accent
	var safe := UiStyle.safe_rect(self)
	if safe != _last_safe:
		_last_safe = safe
		_layout(safe)
		intro_changed = true
		objective_changed = true
	_intro.refresh(intro_changed)
	_objective.refresh(objective_changed)
	queue_redraw()


func _layout(safe: Rect2) -> void:
	var u := minf(safe.size.x, safe.size.y) / 1856.0
	var margin := 56.0 * u
	# The left header clears Pause; the right altimeter/tool rail is untouched.
	_objective.position = safe.position + Vector2(margin, 200.0 * u)
	_objective.size = Vector2(minf(730.0 * u, safe.size.x * 0.34), 205.0 * u)
	_place_label(_chapter, Rect2(0.0, 0.0, _objective.size.x, 38.0 * u), 23.0 * u)
	_place_label(_objective_text, Rect2(0.0, 39.0 * u, _objective.size.x, 94.0 * u), 34.0 * u)
	_place_label(_detail, Rect2(0.0, 137.0 * u, _objective.size.x, 68.0 * u), 24.0 * u)

	# Keep the sightline and both lower thumb decks open. Radio sits under the
	# top-center station banner, with bounded copy rather than a modal card.
	_radio.size = Vector2(minf(1020.0 * u, safe.size.x * 0.46), 150.0 * u)
	_radio.position = Vector2(safe.get_center().x - _radio.size.x * 0.5, safe.position.y + safe.size.y * 0.245)
	_place_label(_speaker, Rect2(0.0, 0.0, _radio.size.x, 35.0 * u), 23.0 * u)
	_place_label(_transmission, Rect2(0.0, 39.0 * u, _radio.size.x, 111.0 * u), 29.0 * u)
	_intro.size = Vector2(safe.size.x * 0.64, 180.0 * u)
	_intro.position = Vector2(safe.get_center().x - _intro.size.x * 0.5, safe.position.y + safe.size.y * 0.36)
	_place_label(_intro_title, Rect2(0.0, 0.0, _intro.size.x, 88.0 * u), 54.0 * u)
	_place_label(_intro_subtitle, Rect2(0.0, 94.0 * u, _intro.size.x, 86.0 * u), 28.0 * u)
	_intro.refresh(true)
	_objective.refresh(true)


func _place_label(label: Label, rect: Rect2, font_size: float) -> void:
	label.position = rect.position
	label.size = rect.size
	label.add_theme_font_size_override("font_size", maxi(10, int(roundf(font_size))))
	label.clip_text = true
	label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	label.max_lines_visible = 2 if label == _objective_text or label == _intro_title else 3


func _draw() -> void:
	if not _built:
		return
	var u := minf(_last_safe.size.x, _last_safe.size.y) / 1856.0
	if _objective.visible:
		var at := _objective.position - Vector2(13.0 * u, 0.0)
		draw_line(at, at + Vector2(0.0, 194.0 * u), UiStyle.with_alpha(UiStyle.AMBER, 0.7), maxf(1.0, 3.0 * u))
	if _radio.visible:
		var rect := Rect2(_radio.position - Vector2(16.0, 10.0) * u, _radio.size + Vector2(32.0, 20.0) * u)
		UiStyle.plate(self, rect, UiStyle.with_alpha(UiStyle.INK, 0.72),
			UiStyle.with_alpha(UiStyle.SAFE, 0.3 + 0.45 * _radio_accent), maxf(1.0, 2.0 * u), 9.0 * u)
