extends Control

signal aligned(points: PackedVector2Array)
signal instruction_changed(copy: String)

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const PROMPTS := ["Tap the eye on the LEFT of the picture.", "Tap the eye on the RIGHT of the picture.", "Tap the center of the closed mouth."]
var texture: Texture2D:
	set(value):
		texture = value
		stop_alignment()
		queue_redraw()
var points := PackedVector2Array()
var _placing := false
var _next := 0
var _touch_id := -1
var _pressed_at := Vector2.ZERO
var _dragged := false

func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_PASS
	resized.connect(queue_redraw)

func image_rect() -> Rect2:
	if texture == null or texture.get_width() <= 0 or texture.get_height() <= 0:
		return Rect2()
	var source := Vector2(texture.get_width(), texture.get_height())
	var ratio := minf(size.x / source.x, size.y / source.y)
	var fitted := source * ratio
	return Rect2((size - fitted) * .5, fitted)

func start_alignment() -> void:
	if texture == null:
		return
	points.clear()
	_next = 0
	_placing = true
	_touch_id = -1
	instruction_changed.emit(PROMPTS[0])
	queue_redraw()

func stop_alignment() -> void:
	_placing = false
	_touch_id = -1
	points.clear()
	queue_redraw()

func _gui_input(event: InputEvent) -> void:
	if not _placing:
		return
	if event is InputEventScreenTouch:
		if event.pressed and _touch_id == -1 and image_rect().has_point(event.position):
			_touch_id = event.index
			_pressed_at = event.position
			_dragged = false
			accept_event()
		elif not event.pressed and event.index == _touch_id:
			_touch_id = -1
			if not event.canceled and not _dragged and event.position.distance_to(_pressed_at) <= _tap_slop():
				select_point(event.position)
			accept_event()
	elif event is InputEventScreenDrag and event.index == _touch_id:
		_dragged = _dragged or event.position.distance_to(_pressed_at) > _tap_slop()
		accept_event()

func _tap_slop() -> float:
	return maxf(16, 24 * minf(get_viewport_rect().size.x, get_viewport_rect().size.y) / 1856.0)

func select_point(at: Vector2) -> void:
	var rect := image_rect()
	if not _placing or rect.size.x <= 0 or rect.size.y <= 0 or not rect.has_point(at):
		return
	points.append((at - rect.position) / rect.size)
	_next += 1
	if _next == 3:
		_placing = false
		aligned.emit(points.duplicate())
	else:
		instruction_changed.emit(PROMPTS[_next])
	queue_redraw()

func _draw() -> void:
	if texture == null:
		return
	var rect := image_rect()
	draw_texture_rect(texture, rect, false)
	if _placing:
		for point in points:
			var at := rect.position + point * rect.size
			draw_circle(at, 7.0, UiStyle.INK)
			draw_arc(at, 7.0, 0, TAU, 24, UiStyle.SAFE, 2.0, true)
			draw_circle(at, 2.0, UiStyle.PAPER)
