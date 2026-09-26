extends Control

# Fold-first touch surface, per GDD section 22 and Governing Law 27: the left
# thumb moves, the right thumb looks, one contextual Action engages whatever
# valid interaction the native affordances report, and machine controls exist
# only while a machine is actually being operated. Jump and Crouch are the
# permanent buttons because they are locomotion, not interactions; Crouch is
# a toggle that reads STAND while the native reports the body crouched.
#
# Every touch index is owned by exactly one thing -- a button, the stick, or
# look -- from press to release, so two thumbs never fight over a control.
# Action buttons fire on press (the whole point of a jump button is the
# frame it is pressed on); a thumb that presses Jump and keeps dragging turns
# into look without lifting, which is how a mid-air correction feels natural.

signal pressed_feedback

const UiStyle := preload("res://presentation/ui/ui_style.gd")

const B_JUMP := &"jump"
const B_ACTION := &"action"
const B_CROUCH := &"crouch"
const B_DROP := &"drop"
const B_CHUTE := &"chute"
const B_PAUSE := &"pause"
const B_UP := &"pendant_up"
const B_DOWN := &"pendant_down"
const B_LEFT := &"pendant_left"
const B_RIGHT := &"pendant_right"
const B_SLING := &"sling"

const TONE_NORMAL := 0
const TONE_PRIMARY := 1
const TONE_DANGER := 2
const TONE_SAFE := 3
const TONE_GHOST := 4
const PRIMARY_FILL := Color("2b1a0b")

const STICK_THROW := 150.0
const STICK_KNOB := 60.0
const STICK_DEADZONE := 0.1
# Sprint: the thumb pushed on past the stick's ring, forward, latches it; it
# holds while the stick stays pushed forward and lets go below half throw.
const SPRINT_OVERSHOOT := 1.25
const SPRINT_HOLD := 0.5
const LOOK_SLOP := 26.0
const HIT_SLOP := 1.22
const APPEAR_RATE := 8.0
const HINT_SECONDS := 16.0


class TouchButton:
	var id: StringName
	var center := Vector2.ZERO
	var radius := 0.0
	var shown := false
	var enabled := true
	var dimmed := false
	var hold := false
	var label := ""
	var icon: StringName = &""
	var tone := 0
	var index := -1
	var press_position := Vector2.ZERO
	var appear := 0.0
	var flash := 0.0


var router: Node
var move_vector := Vector2.ZERO
var sprint_latched := false
var pendant_axes := Vector2.ZERO
var touch_scale := 1.0

var _u := 1.0
var _buttons := {}
var _order: Array[StringName] = [B_PAUSE, B_JUMP, B_ACTION, B_CROUCH, B_DROP, B_CHUTE, B_UP,
	B_DOWN, B_LEFT, B_RIGHT, B_SLING]
var _stick_index := -1
var _stick_origin := Vector2.ZERO
var _stick_knob := Vector2.ZERO
var _stick_home := Vector2.ZERO
var _stick_zone := Rect2()
var _safe := Rect2()
var _look_index := -1
var _look_accum := Vector2.ZERO
var _operating := false
var _axis_names := ["", ""]
var _pendant_center := Vector2.ZERO
var _hint_clock := 0.0
var _moved := false
var _looked := false
var _clock := 0.0


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for id in _order:
		var button := TouchButton.new()
		button.id = id
		button.hold = id in [B_UP, B_DOWN, B_LEFT, B_RIGHT]
		_buttons[id] = button
	_buttons[B_PAUSE].icon = &"pause"
	_buttons[B_DROP].icon = &"drop"
	_buttons[B_DROP].label = "DROP"
	_buttons[B_UP].icon = &"up"
	_buttons[B_DOWN].icon = &"down"
	_buttons[B_LEFT].icon = &"left"
	_buttons[B_RIGHT].icon = &"right"
	_buttons[B_SLING].icon = &"sling"
	resized.connect(_layout)
	_layout()


func set_touch_scale(value: float) -> void:
	touch_scale = value
	_layout()


func button_center(id: StringName) -> Vector2:
	return _buttons[id].center


func stick_home() -> Vector2:
	return _stick_home


func is_button_shown(id: StringName) -> bool:
	return _buttons[id].shown


func button_label(id: StringName) -> String:
	return _buttons[id].label


func _layout() -> void:
	_safe = UiStyle.safe_rect(self)
	_u = UiStyle.unit(self) * touch_scale
	var m := 56.0 * _u
	var corner := _safe.end
	var jump := Vector2(corner.x - m - 170.0 * _u, corner.y - m - 190.0 * _u)
	_place(B_JUMP, jump, 122.0)
	_place(B_ACTION, jump + Vector2(-236.0, -150.0) * _u, 98.0)
	# Over Jump, toward the edge: the same thumb slides up to it.
	_place(B_CROUCH, jump + Vector2(40.0, -252.0) * _u, 78.0)
	# Drop and Chute share one slot: hanging and free-falling never overlap,
	# and "drop, then open the canopy" becomes the same thumb in one place.
	_place(B_DROP, jump + Vector2(-306.0, 96.0) * _u, 86.0)
	_place(B_CHUTE, jump + Vector2(-306.0, 96.0) * _u, 86.0)
	_place(B_PAUSE, _safe.position + Vector2(m, m) + Vector2(52.0, 52.0) * _u, 52.0)
	_stick_home = Vector2(_safe.position.x + m + 230.0 * _u, corner.y - m - 230.0 * _u)
	_stick_zone = Rect2(_safe.position.x, _safe.position.y + _safe.size.y * 0.22,
		_safe.size.x * 0.46, _safe.size.y * 0.78)
	# The pendant takes the stick's place: while operating, the left thumb
	# is on the machine and the right thumb stays free to watch the load.
	_pendant_center = _stick_home + Vector2(20.0, -40.0) * _u
	var arm := 158.0 * _u
	_place(B_UP, _pendant_center + Vector2(0.0, -arm), 80.0)
	_place(B_DOWN, _pendant_center + Vector2(0.0, arm), 80.0)
	_place(B_LEFT, _pendant_center + Vector2(-arm, 0.0), 80.0)
	_place(B_RIGHT, _pendant_center + Vector2(arm, 0.0), 80.0)
	_place(B_SLING, _pendant_center + Vector2(arm + 250.0 * _u, -arm * 0.6), 78.0)
	queue_redraw()


func _place(id: StringName, center: Vector2, radius: float) -> void:
	var button: TouchButton = _buttons[id]
	button.center = center
	button.radius = radius * _u


func _verb_for(id: StringName) -> StringName:
	match id:
		B_JUMP:
			return &"jump"
		B_ACTION:
			return &"action"
		B_CROUCH:
			return &"crouch"
		B_DROP:
			return &"drop"
		B_CHUTE:
			return &"chute"
		B_PAUSE:
			return &"pause"
		B_SLING:
			return &"sling_toggle"
	return &""


func reset_touches() -> void:
	_stick_index = -1
	_look_index = -1
	_look_accum = Vector2.ZERO
	move_vector = Vector2.ZERO
	sprint_latched = false
	pendant_axes = Vector2.ZERO
	for button in _buttons.values():
		button.index = -1
	queue_redraw()


func take_look_delta() -> Vector2:
	var value := _look_accum
	_look_accum = Vector2.ZERO
	return value


func handle_touch(event: InputEvent) -> bool:
	if event is InputEventScreenTouch:
		var touch := event as InputEventScreenTouch
		if touch.pressed and not touch.canceled:
			return _touch_down(touch.index, touch.position)
		return _touch_up(touch.index)
	if event is InputEventScreenDrag:
		var drag := event as InputEventScreenDrag
		return _touch_drag(drag.index, drag.position, drag.relative)
	return false


func _touch_down(index: int, at: Vector2) -> bool:
	for id in _order:
		var button: TouchButton = _buttons[id]
		if not button.shown or not button.enabled or button.index != -1 or button.appear < 0.2:
			continue
		if at.distance_to(button.center) > button.radius * HIT_SLOP:
			continue
		button.index = index
		button.press_position = at
		button.flash = 1.0
		if not button.hold and router != null:
			router.push_verb(_verb_for(id))
		pressed_feedback.emit()
		queue_redraw()
		return true
	if _stick_index == -1 and not _operating and _stick_zone.has_point(at):
		_stick_index = index
		_stick_origin = at
		_stick_knob = Vector2.ZERO
		_update_stick_vector()
		return true
	if _look_index == -1:
		_look_index = index
		return true
	return false


func _touch_drag(index: int, at: Vector2, relative: Vector2) -> bool:
	if index == _stick_index:
		var throw := STICK_THROW * _u
		var offset := at - _stick_origin
		if offset.length() > throw * SPRINT_OVERSHOOT and -offset.y > absf(offset.x):
			sprint_latched = true
		if offset.length() > throw:
			# The base follows a thumb that overshoots, so the stick never
			# saturates into a dead direction the thumb is no longer pushing.
			_stick_origin = at - offset.normalized() * throw
			offset = at - _stick_origin
		_stick_knob = offset
		_update_stick_vector()
		return true
	if index == _look_index:
		_look_accum += relative
		_looked = true
		return true
	for button in _buttons.values():
		if button.index != index:
			continue
		if not button.hold and _look_index == -1 \
				and at.distance_to(button.press_position) > LOOK_SLOP * _u:
			button.index = -1
			_look_index = index
			queue_redraw()
		return true
	return false


func _touch_up(index: int) -> bool:
	if index == _stick_index:
		_stick_index = -1
		_stick_knob = Vector2.ZERO
		move_vector = Vector2.ZERO
		sprint_latched = false
		queue_redraw()
		return true
	if index == _look_index:
		_look_index = -1
		return true
	for button in _buttons.values():
		if button.index == index:
			button.index = -1
			queue_redraw()
			return true
	return false


func _update_stick_vector() -> void:
	var throw := STICK_THROW * _u
	var value := _stick_knob / throw
	var magnitude := value.length()
	if magnitude < STICK_DEADZONE:
		move_vector = Vector2.ZERO
		sprint_latched = false
		return
	var scaled := minf(1.0, (magnitude - STICK_DEADZONE) / (1.0 - STICK_DEADZONE))
	move_vector = Vector2(value.x, -value.y) / magnitude * scaled
	if move_vector.y < SPRINT_HOLD:
		sprint_latched = false
	if scaled > 0.3:
		_moved = true


# ctx is main.gd's per-frame read of native state (see _read_context there).
func update_context(ctx: Dictionary, delta: float) -> void:
	_clock += delta
	if visible:
		_hint_clock += delta
	var hanging: bool = ctx["hanging"]
	var climbing: bool = ctx.get("climbing", false)
	var jump: TouchButton = _buttons[B_JUMP]
	jump.shown = true
	# Pressable while airborne on purpose: main.gd buffers a press made just
	# before touchdown and fires it on the grounded tick. Dimmed, not dead.
	jump.enabled = true
	jump.dimmed = not (ctx["jump_ok"] or hanging or climbing)
	jump.icon = &"climb" if hanging else &"jump"
	jump.label = "CLIMB UP" if hanging else ("JUMP OFF" if climbing else "JUMP")
	jump.tone = TONE_PRIMARY

	var action: TouchButton = _buttons[B_ACTION]
	var act: Dictionary = ctx["action"]
	action.shown = true
	# While hanging, Jump already reads CLIMB UP (the same native mantle);
	# a second identical button beside it is noise, so Action rests.
	action.enabled = act["id"] != &"" and not hanging
	action.icon = act["icon"]
	action.label = act["label"]
	action.tone = TONE_PRIMARY if action.enabled else TONE_GHOST

	# Drop lets go of a ledge or a climb's holds, and at an edge behind the
	# body lowers it over into a hang.
	var drop: TouchButton = _buttons[B_DROP]
	drop.shown = ctx.get("drop_ok", hanging)
	drop.label = "DROP DOWN" if not (hanging or climbing) else "DROP"
	var chute: TouchButton = _buttons[B_CHUTE]
	chute.shown = ctx["chute_ok"] and not hanging
	chute.icon = &"chute"
	chute.label = "STOW" if ctx["chute"] else "CHUTE"
	if ctx["chute"]:
		chute.tone = TONE_SAFE
	elif ctx["danger"] >= 0.6:
		chute.tone = TONE_DANGER
	else:
		chute.tone = TONE_NORMAL
	_buttons[B_PAUSE].shown = true

	var station: StringName = ctx["operating"]
	_operating = station != &""
	var crouch: TouchButton = _buttons[B_CROUCH]
	crouch.shown = not _operating and not hanging and not climbing
	crouch.icon = &"stand" if ctx["crouched"] else &"crouch"
	crouch.label = "STAND" if ctx["crouched"] else "CROUCH"
	crouch.tone = TONE_SAFE if ctx["crouched"] else TONE_NORMAL
	var has_x := station == &"jib" or station == &"intake"
	_buttons[B_UP].shown = _operating
	_buttons[B_DOWN].shown = _operating
	_buttons[B_LEFT].shown = _operating and has_x
	_buttons[B_RIGHT].shown = _operating and has_x
	var sling: TouchButton = _buttons[B_SLING]
	sling.shown = station == &"intake"
	sling.label = "RELEASE PACK" if ctx["slung"] else "ATTACH PACK"
	sling.tone = TONE_PRIMARY
	match station:
		&"jib":
			_axis_names = ["HOIST", "DRIVE"]
		&"intake":
			_axis_names = ["HOIST", "SLEW"]
		&"needle":
			_axis_names = ["RAISE / LOWER", ""]
		_:
			_axis_names = ["", ""]
	if _operating and _stick_index != -1:
		_touch_up(_stick_index)

	for button in _buttons.values():
		if not button.shown and button.index != -1:
			button.index = -1
		button.appear = move_toward(button.appear, 1.0 if button.shown else 0.0, APPEAR_RATE * delta)
		button.flash = maxf(0.0, button.flash - delta * 3.5)
	pendant_axes = Vector2(
		_held(B_RIGHT) - _held(B_LEFT), _held(B_UP) - _held(B_DOWN))
	if visible:
		queue_redraw()


func _held(id: StringName) -> float:
	var button: TouchButton = _buttons[id]
	return 1.0 if button.shown and button.index != -1 else 0.0


func _draw() -> void:
	if not _operating:
		_draw_stick()
	for id in _order:
		_draw_button(_buttons[id])
	if _operating:
		_draw_pendant_labels()
	_draw_hints()


func _draw_button(button: TouchButton) -> void:
	if button.appear <= 0.01:
		return
	var alpha := clampf(button.appear * 1.4, 0.0, 1.0)
	var radius := button.radius * lerpf(0.72, 1.0, UiStyle.ease_out_back(button.appear))
	var pressed := button.index != -1
	var dim := 0.36 if (not button.enabled or button.dimmed) else 1.0
	var accent := UiStyle.PAPER
	match button.tone:
		TONE_PRIMARY:
			accent = UiStyle.AMBER
		TONE_DANGER:
			accent = UiStyle.HAZARD
		TONE_SAFE:
			accent = UiStyle.SAFE
	if button.tone == TONE_GHOST:
		UiStyle.ring(self, button.center, radius, UiStyle.with_alpha(UiStyle.PAPER, 0.2 * alpha), 3.0 * _u)
		return
	# Dense enough to read over sunlit concrete as well as night sky.
	var fill := UiStyle.with_alpha(UiStyle.INK, 0.62 * alpha)
	if pressed:
		radius *= 0.94
		fill = UiStyle.with_alpha(accent, 0.86 * alpha)
	elif button.tone == TONE_DANGER:
		fill = UiStyle.with_alpha(UiStyle.HAZARD, (0.34 + 0.2 * sin(_clock * 9.0)) * alpha)
	elif button.tone == TONE_PRIMARY and button.enabled and not button.dimmed:
		fill = UiStyle.with_alpha(PRIMARY_FILL, 0.78 * alpha)
	UiStyle.disc(self, button.center, radius, fill)
	var ring_width := (4.5 if button.id == B_JUMP else 3.5) * _u
	UiStyle.ring(self, button.center, radius, UiStyle.with_alpha(accent, 0.92 * dim * alpha), ring_width)
	if button.flash > 0.0:
		UiStyle.ring(self, button.center, radius + (1.0 - button.flash) * 22.0 * _u,
			UiStyle.with_alpha(accent, button.flash * 0.55 * alpha), 3.0 * _u)
	var icon_color := UiStyle.INK if pressed else UiStyle.with_alpha(UiStyle.PAPER, dim * alpha)
	UiStyle.draw_icon(self, button.icon, button.center, radius * 0.78, icon_color, 4.0 * _u)
	if not button.label.is_empty():
		UiStyle.text_centered(self, UiStyle.font_label(), button.label,
			button.center + Vector2(0.0, radius + 30.0 * _u), int(roundf(25.0 * _u)),
			UiStyle.with_alpha(UiStyle.PAPER, dim * alpha), int(roundf(6.0 * _u)))


func _draw_stick() -> void:
	var throw := STICK_THROW * _u
	var knob := STICK_KNOB * _u
	if _stick_index == -1:
		UiStyle.ring(self, _stick_home, throw, UiStyle.with_alpha(UiStyle.PAPER, 0.16), 3.0 * _u)
		UiStyle.disc(self, _stick_home, knob, UiStyle.with_alpha(UiStyle.PAPER, 0.12))
		return
	# Drawn inside the safe rect even when the thumb landed at the very edge;
	# the logic keeps the true origin so a corner landing never walks.
	var base := Vector2(
		clampf(_stick_origin.x, _safe.position.x + throw * 0.8, _safe.end.x - throw * 0.8),
		clampf(_stick_origin.y, _safe.position.y + throw * 0.8, _safe.end.y - throw * 0.8))
	UiStyle.disc(self, base, throw + 10.0 * _u, UiStyle.with_alpha(UiStyle.INK, 0.26))
	UiStyle.ring(self, base, throw, UiStyle.with_alpha(UiStyle.PAPER, 0.4), 3.0 * _u)
	if sprint_latched:
		# Sprinting: an amber outer ring, where the thumb pushed through.
		UiStyle.ring(self, base, throw * SPRINT_OVERSHOOT,
			UiStyle.with_alpha(UiStyle.AMBER, 0.85), 4.0 * _u)
	var offset := _stick_knob.limit_length(throw)
	var magnitude := offset.length() / throw
	if magnitude > STICK_DEADZONE:
		var angle := offset.angle()
		var spread := 0.22 + 0.5 * magnitude
		draw_arc(base, throw, angle - spread, angle + spread, 18,
			UiStyle.with_alpha(UiStyle.AMBER, 0.92), 7.0 * _u, true)
	UiStyle.disc(self, base + offset, knob, UiStyle.with_alpha(UiStyle.PAPER, 0.84))
	UiStyle.ring(self, base + offset, knob,
		UiStyle.with_alpha(UiStyle.AMBER if magnitude > 0.97 else UiStyle.INK, 0.8), 3.0 * _u)


func _draw_pendant_labels() -> void:
	var size := int(roundf(22.0 * _u))
	var font := UiStyle.font_label()
	if not _axis_names[0].is_empty():
		UiStyle.text_centered(self, font, _axis_names[0], _pendant_center + Vector2(0.0, -14.0 * _u),
			size, UiStyle.PAPER, int(roundf(5.0 * _u)))
	if not _axis_names[1].is_empty():
		UiStyle.text_centered(self, font, _axis_names[1], _pendant_center + Vector2(0.0, 18.0 * _u),
			size, UiStyle.PAPER_DIM, int(roundf(5.0 * _u)))


func _draw_hints() -> void:
	if _hint_clock > HINT_SECONDS or (_moved and _looked):
		return
	var alpha := clampf(_hint_clock * 2.0, 0.0, 1.0) * clampf(HINT_SECONDS - _hint_clock, 0.0, 1.0)
	var size := int(roundf(26.0 * _u))
	var font := UiStyle.font_label()
	if not _moved and not _operating:
		UiStyle.text_centered(self, font, "MOVE", _stick_home + Vector2(0.0, -STICK_THROW * _u - 34.0 * _u),
			size, UiStyle.with_alpha(UiStyle.PAPER, 0.8 * alpha), int(roundf(6.0 * _u)))
	if not _looked:
		var at := Vector2(_safe.position.x + _safe.size.x * 0.72, _safe.position.y + _safe.size.y * 0.6)
		UiStyle.draw_icon(self, &"look", at + Vector2(0.0, -48.0 * _u), 40.0 * _u,
			UiStyle.with_alpha(UiStyle.PAPER, 0.7 * alpha), 4.0 * _u)
		UiStyle.text_centered(self, font, "DRAG TO LOOK", at, size,
			UiStyle.with_alpha(UiStyle.PAPER, 0.8 * alpha), int(roundf(6.0 * _u)))
