extends Control
## One of her pages, read (HER_CHART.md): while the eyes rest on a page from
## close by (uncanny.gd), the sheet comes up large in her hand, as near as a
## page held to the face; when the eyes leave it, it goes. No button, no
## pause, no caption: the climb goes on behind it and a touch passes through
## it. A sheet on a deck is too small to read on a phone's glass otherwise.

const UiStyle := preload("res://presentation/ui/ui_style.gd")

const FADE_IN_S := 0.35
const FADE_OUT_S := 0.25
const PAPER_ASPECT := 216.0 / 279.0
const INK := Color("1f2a4d")

# The page being read, as written ("" when none): for the tests.
var shown_text := ""
var _target := 0.0
var _paper: TextureRect
var _printed: Label
var _hand: Label


func setup(font: Font, paper: Texture2D) -> void:
	name = "PageReader"
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	modulate.a = 0.0
	visible = false
	_paper = TextureRect.new()
	_paper.texture = paper
	_paper.stretch_mode = TextureRect.STRETCH_SCALE
	_paper.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_paper.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(_paper)
	_printed = Label.new()
	_printed.text = "OBSERVATION CHART"
	_printed.add_theme_font_override("font", UiStyle.font_label())
	_printed.add_theme_color_override("font_color", Color("3b3a3a"))
	_printed.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	_printed.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_paper.add_child(_printed)
	_hand = Label.new()
	if font != null:
		_hand.add_theme_font_override("font", font)
	_hand.add_theme_color_override("font_color", INK)
	_hand.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_hand.vertical_alignment = VERTICAL_ALIGNMENT_TOP
	_hand.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_paper.add_child(_hand)
	resized.connect(_layout)
	_layout()


func show_page(text: String) -> void:
	if text != shown_text:
		_hand.text = text
	shown_text = text
	_target = 1.0
	visible = true


func hide_page() -> void:
	_target = 0.0
	if modulate.a <= 0.01:
		shown_text = ""


func reading() -> bool:
	return _target > 0.5


func _process(delta: float) -> void:
	if not visible:
		return
	var rate := delta / (FADE_IN_S if _target > modulate.a else FADE_OUT_S)
	modulate.a = move_toward(modulate.a, _target, rate)
	if _target == 0.0 and modulate.a <= 0.01:
		visible = false
		shown_text = ""


# The sheet four fifths of the screen tall, in the middle; the hand inside
# the printed margin, under the printed rule.
func _layout() -> void:
	if _paper == null:
		return
	var screen := get_viewport_rect().size
	var tall := screen.y * 0.82
	var wide := minf(tall * PAPER_ASPECT, screen.x * 0.9)
	tall = wide / PAPER_ASPECT
	_paper.size = Vector2(wide, tall)
	_paper.position = (screen - _paper.size) * 0.5
	_printed.add_theme_font_size_override("font_size", roundi(tall * 0.03))
	_printed.position = Vector2(0.0, tall * 0.022)
	_printed.size = Vector2(wide, tall * 0.05)
	_hand.add_theme_font_size_override("font_size", roundi(tall * 0.046))
	_hand.add_theme_constant_override("line_spacing", -roundi(tall * 0.008))
	_hand.position = Vector2(wide * 0.15, tall * 0.11)
	_hand.size = Vector2(wide * 0.78, tall * 0.85)
