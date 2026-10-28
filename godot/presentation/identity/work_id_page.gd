extends VBoxContainer

signal photo_changed

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const Calibration := preload("res://presentation/identity/portrait_calibration.gd")
const Intake := preload("res://presentation/identity/photo_id_intake.gd")
var store: RefCounted
var _intake: Control
var _portrait: Control
var _status: Label
var _default_button: Button
var choose_button: Button
var _align_button: Button
var _u := 1.0

func configure(owner: RefCounted, scale: float) -> void:
	store = owner
	_u = scale

func _ready() -> void:
	add_theme_constant_override("separation", maxi(8, roundi(20 * _u)))
	add_child(_label("RO VALE  /  ACCESS & RIGGING", UiStyle.PAPER))
	add_child(_label("Use a picture for your work ID and in-game character appearances. Optional; processing and storage stay on this device.", UiStyle.PAPER_DIM))
	_portrait = Calibration.new()
	_portrait.name = "WorkPhoto"
	_portrait.custom_minimum_size = Vector2(240, 280) * _u
	add_child(_portrait)
	_status = _label("", UiStyle.PAPER_DIM)
	_status.name = "PhotoStatus"
	add_child(_status)
	choose_button = _button("CHOOSE PICTURE")
	choose_button.name = "ChoosePicture"
	choose_button.pressed.connect(func() -> void: _intake.request_photo())
	_align_button = _button("ADJUST FACE ALIGNMENT")
	_align_button.name = "AlignFace"
	_align_button.pressed.connect(_start_alignment)
	_portrait.instruction_changed.connect(func(copy: String) -> void: _status.text = copy)
	_portrait.aligned.connect(_save_alignment)
	_default_button = _button("USE DEFAULT RO")
	_default_button.name = "UseDefault"
	_default_button.pressed.connect(_use_default)
	add_child(_label("A clear, front-facing portrait works best. You can replace or remove it without restarting your climb.", UiStyle.PAPER_DIM))
	_intake = Intake.new()
	_intake.store = store
	add_child(_intake)
	_intake.photo_changed.connect(_on_photo_changed)
	_intake.failed.connect(func(_error: Error, message: String) -> void:
		_status.text = message
		_status.add_theme_color_override("font_color", UiStyle.HAZARD))
	refresh()

func _label(copy: String, color: Color) -> Label:
	var label := Label.new()
	label.text = copy
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.add_theme_font_override("font", UiStyle.font_label())
	label.add_theme_font_size_override("font_size", maxi(12, roundi(27 * _u)))
	label.add_theme_color_override("font_color", color)
	return label

func _button(copy: String) -> Button:
	var button := Button.new()
	button.text = copy
	button.custom_minimum_size.y = maxf(44, 72 * _u)
	add_child(button)
	return button

func refresh() -> void:
	if _portrait == null:
		return
	_portrait.texture = store.photo_texture()
	_default_button.disabled = _portrait.texture == null
	_align_button.disabled = _portrait.texture == null
	_status.text = "PHOTO SAVED ON THIS DEVICE" if _portrait.texture != null else "DEFAULT RO  /  NO PERSONAL PHOTO"
	_status.add_theme_color_override("font_color", UiStyle.SAFE if _portrait.texture != null else UiStyle.PAPER_DIM)

func cancel_photo() -> void:
	if _portrait != null:
		_portrait.stop_alignment()
		_portrait.custom_minimum_size.y = 280 * _u
	if _intake != null:
		_intake.cancel()

func _on_photo_changed() -> void:
	refresh()
	photo_changed.emit()

func _use_default() -> void:
	cancel_photo()
	var error: Error = store.clear_photo()
	if error != OK:
		_status.text = "Could not remove the saved photo. Your climb is unchanged."
		_status.add_theme_color_override("font_color", UiStyle.HAZARD)
		return
	_on_photo_changed()

func _start_alignment() -> void:
	_portrait.custom_minimum_size.y = maxf(300, 680 * _u)
	_portrait.start_alignment()

func _save_alignment(points: PackedVector2Array) -> void:
	_portrait.custom_minimum_size.y = 280 * _u
	var error: Error = store.save_face_landmarks(points)
	if error != OK:
		_status.text = "Alignment not saved. Choose the left eye, right eye, then mouth."
		_status.add_theme_color_override("font_color", UiStyle.HAZARD)
		return
	_status.text = "FACE ALIGNMENT SAVED ON THIS DEVICE"
	_status.add_theme_color_override("font_color", UiStyle.SAFE)
	photo_changed.emit()
