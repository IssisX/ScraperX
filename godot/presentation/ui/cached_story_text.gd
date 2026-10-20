extends Control

# Stable story copy is rasterized at the canvas pixel density only when its
# source labels or layout change. Parent opacity remains live for intro fades.
var content: Control
var _viewport: SubViewport
var _image: TextureRect
var _pixel_scale := Vector2.ZERO
var _dirty := true
var _pixel_origin := Vector2.ZERO


func _init() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	focus_mode = Control.FOCUS_NONE
	_viewport = SubViewport.new()
	_viewport.transparent_bg = true
	_viewport.disable_3d = true
	_viewport.gui_disable_input = true
	_viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	add_child(_viewport)
	content = Control.new()
	content.mouse_filter = Control.MOUSE_FILTER_IGNORE
	content.focus_mode = Control.FOCUS_NONE
	_viewport.add_child(content)
	_image = TextureRect.new()
	_image.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_image.focus_mode = Control.FOCUS_NONE
	_image.texture = _viewport.get_texture()
	_image.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	add_child(_image)


func refresh(changed: bool = false) -> void:
	_dirty = _dirty or changed
	# Include window stretching: rendering the text at logical resolution would
	# blur it on a larger display. Rotation/skew is not used by this overlay.
	var transform := get_viewport().get_final_transform() * get_global_transform_with_canvas()
	var scale := Vector2(transform.x.length(), transform.y.length())
	scale = scale.max(Vector2(0.001, 0.001))
	var origin := transform.origin - transform.origin.floor()
	if scale != _pixel_scale or origin != _pixel_origin:
		_pixel_scale = scale
		_pixel_origin = origin
		_dirty = true
	if not _dirty or not visible:
		return
	var bounds := Rect2(Vector2.ZERO, size)
	for label in content.get_children():
		bounds = bounds.merge(label.get_rect())
	# Retain the exact label clipping and allow the existing text shadow.
	bounds = bounds.grow(3.0)
	var pixel_start := (transform.origin + bounds.position * scale).floor()
	var pixel_end := (transform.origin + bounds.end * scale).ceil()
	var pixels := Vector2i(pixel_end - pixel_start).max(Vector2i.ONE)
	var local_start := (pixel_start - transform.origin) / scale
	_viewport.size = pixels
	content.position = -local_start * scale
	content.scale = scale
	_image.position = local_start
	_image.size = Vector2(pixels) / scale
	_viewport.render_target_update_mode = SubViewport.UPDATE_ONCE
	_dirty = false
