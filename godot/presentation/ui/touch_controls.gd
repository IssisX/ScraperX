extends Control

# Fold-first touch surface, per GDD section 22 and Governing Law 27: the left
# thumb moves, the right thumb looks, one contextual Action engages whatever
# valid interaction the native affordances report, and machine controls exist
# only while a machine is actually being operated. Jump and Crouch are the
# permanent buttons because they are locomotion, not interactions; Crouch is
# a toggle that reads STAND while the native reports the body crouched.
#
# GESTURES (the default, settings_store TOUCH_SCHEME_GESTURES) takes the run's
# moves off the glass: a right thumb that sets down and flicks up or down is a
# swipe (the verbs up and down, which the native resolves into the move ahead
# of the body), the stick pulled back hard turns about, and Jump, Crouch, Drop
# and Turn are not drawn. A thumb that drags is look, as ever: a drag that
# goes across is look from its first movement; one that goes up or down is
# held back for at most SWIPE_SECONDS, until it is known not to be a swipe.
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
# The grappling hook, once found: tap fires it, held reels the line in.
const B_HOOK := &"hook"
# The quickturn (Mirror's Edge's): half a turn of the view in one tap.
const B_TURN := &"turn"
# The yard crane's remote, once taken: its switch puts the stick and the two
# held buttons on the crane, and takes them back.
const B_CRANE := &"crane"

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
# A swipe: the thumb travels this far (scaled units) up or down within
# SWIPE_SECONDS of touching down, SWIPE_STEEPNESS times more up or down than
# across: 560 units a second at the least, quicker than a look's drag.
const SWIPE_UNITS := 90.0
const SWIPE_SECONDS := 0.16
const SWIPE_STEEPNESS := 1.6
# Under this travel a look touch has not yet gone anywhere.
const GESTURE_SLOP := 16.0
# The quickturn by the stick: a run reversed, the stick pulled back past
# FLICK_BACK of its throw within FLICK_SECONDS of pushing forward past
# FLICK_FORWARD (a stick pulled back from rest walks backward, so the body can
# back away from something with its eyes on it). The run goes on the other
# way: the stick held back reads as forward until it leaves the back half.
const FLICK_BACK := 0.85
const FLICK_FORWARD := 0.5
const FLICK_SECONDS := 0.14
const FLICK_HALF := -0.3
const LOOK_UNDECIDED := 0
const LOOK_DRAG := 1
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
var touch_scale := 1.0
var gestures := true
# What a swipe up and down would do now (main.gd, from the native's choice),
# and whether it is more than a jump or a crouch: then it is drawn where the
# thumb swipes.
var up_hint := ""
var down_hint := ""
var up_hint_special := false
var down_hint_special := false
# The thumb that swiped down is still on the glass.
var down_held := false
var gesture_counts := {&"up": 0, &"down": 0, &"turn": 0}

var _u := 1.0
var _buttons := {}
var _order: Array[StringName] = [B_PAUSE, B_JUMP, B_ACTION, B_CROUCH, B_DROP, B_CHUTE, B_HOOK, B_TURN, B_CRANE]
var _stick_index := -1
var _stick_origin := Vector2.ZERO
var _stick_knob := Vector2.ZERO
var _stick_home := Vector2.ZERO
var _stick_zone := Rect2()
var _safe := Rect2()
var _look_index := -1
var _look_accum := Vector2.ZERO
var _look_kind := LOOK_DRAG
var _look_start := Vector2.ZERO
var _look_start_clock := 0.0
var _look_held := Vector2.ZERO
# A thumb that swiped, until it lifts: it neither looks nor swipes again.
var _swipe_index := -1
var _down_index := -1
# On foot or on holds, in the gesture scheme: swipes and the stick's flick
# are read. In a seat or at a machine's controls the buttons are as they are
# in BUTTONS.
var _gestures_live := true
var _turn_allowed := true
var _stick_flipped := false
var _stick_forward_clock := -INF
# The stick's own forward (not turned about), as it last read.
var _stick_raw_forward := 0.0
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
		_buttons[id] = button
	_buttons[B_PAUSE].icon = &"pause"
	_buttons[B_DROP].icon = &"drop"
	_buttons[B_DROP].label = "DROP"
	_buttons[B_TURN].icon = &"turn"
	_buttons[B_TURN].label = "TURN"
	_buttons[B_CRANE].icon = &"hook"
	resized.connect(_layout)
	_layout()


func set_touch_scale(value: float) -> void:
	touch_scale = value
	_layout()


func button_center(id: StringName) -> Vector2:
	return _buttons[id].center


func stick_home() -> Vector2:
	return _stick_home


# A button with a finger on it now (the hook's reel is held, not tapped).
func is_held(id: StringName) -> bool:
	return _buttons[id].index != -1


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
	# Over Action, away from the stick: the hook is aimed with the view.
	_place(B_HOOK, jump + Vector2(-236.0, -370.0) * _u, 86.0)
	# Over Crouch, the same thumb on up: the turn is a move of the run's.
	_place(B_TURN, jump + Vector2(40.0, -430.0) * _u, 64.0)
	# Inboard of Action, clear of the stick: the remote's switch.
	_place(B_CRANE, jump + Vector2(-470.0, -110.0) * _u, 80.0)
	_place(B_PAUSE, _safe.position + Vector2(m, m) + Vector2(52.0, 52.0) * _u, 52.0)
	_stick_home = Vector2(_safe.position.x + m + 230.0 * _u, corner.y - m - 230.0 * _u)
	_stick_zone = Rect2(_safe.position.x, _safe.position.y + _safe.size.y * 0.22,
		_safe.size.x * 0.46, _safe.size.y * 0.78)
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
		B_HOOK:
			return &"hook"
		B_TURN:
			return &"turn"
		B_CRANE:
			return &"crane"
	return &""


func reset_touches() -> void:
	_stick_index = -1
	_look_index = -1
	_look_accum = Vector2.ZERO
	_look_held = Vector2.ZERO
	_swipe_index = -1
	_down_index = -1
	down_held = false
	_stick_flipped = false
	_stick_raw_forward = 0.0
	move_vector = Vector2.ZERO
	sprint_latched = false
	for button in _buttons.values():
		button.index = -1
	queue_redraw()


# What a swipe up (&"up") or down (&"down") would do now, as drawn.
func gesture_hint(verb: StringName) -> String:
	return up_hint if verb == &"up" else down_hint


func gestures_live() -> bool:
	return _gestures_live


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
	if _stick_index == -1 and _stick_zone.has_point(at):
		_stick_index = index
		_stick_origin = at
		_stick_knob = Vector2.ZERO
		_stick_flipped = false
		_stick_forward_clock = -INF
		_stick_raw_forward = 0.0
		_update_stick_vector()
		return true
	if _look_index == -1:
		_look_index = index
		_look_start = at
		_look_start_clock = _clock
		_look_held = Vector2.ZERO
		_look_kind = LOOK_UNDECIDED if _gestures_live else LOOK_DRAG
		return true
	return false


func _touch_drag(index: int, at: Vector2, relative: Vector2) -> bool:
	if index == _stick_index:
		var throw := STICK_THROW * _u
		var offset := at - _stick_origin
		# Turned about by the stick, the thumb pushing on back is the run's forward.
		var ahead := offset.y if _stick_flipped else -offset.y
		if offset.length() > throw * SPRINT_OVERSHOOT and ahead > absf(offset.x):
			sprint_latched = true
		if offset.length() > throw:
			# The base follows a thumb that overshoots, so the stick never
			# saturates into a dead direction the thumb is no longer pushing.
			_stick_origin = at - offset.normalized() * throw
			offset = at - _stick_origin
		_stick_knob = offset
		_update_stick_vector()
		return true
	if index == _swipe_index:
		return true
	if index == _look_index:
		_looked = true
		if _look_kind == LOOK_DRAG:
			_look_accum += relative
			return true
		_look_held += relative
		var travel := at - _look_start
		if travel.length() < GESTURE_SLOP * _u:
			return true
		if absf(travel.x) >= absf(travel.y):
			_settle_look()
			return true
		if absf(travel.y) >= SWIPE_UNITS * _u and absf(travel.y) >= SWIPE_STEEPNESS * absf(travel.x) \
				and _clock - _look_start_clock <= SWIPE_SECONDS:
			_swipe(index, &"up" if travel.y < 0.0 else &"down")
		return true
	for button in _buttons.values():
		if button.index != index:
			continue
		if not button.hold and _look_index == -1 \
				and at.distance_to(button.press_position) > LOOK_SLOP * _u:
			button.index = -1
			_look_index = index
			_look_kind = LOOK_DRAG
			queue_redraw()
		return true
	return false


func _touch_up(index: int) -> bool:
	if index == _stick_index:
		_stick_index = -1
		_stick_knob = Vector2.ZERO
		_stick_raw_forward = 0.0
		move_vector = Vector2.ZERO
		sprint_latched = false
		queue_redraw()
		return true
	if index == _swipe_index:
		_swipe_index = -1
		if index == _down_index:
			_down_index = -1
			down_held = false
		return true
	if index == _look_index:
		if _look_kind == LOOK_UNDECIDED:
			_settle_look()
		_look_index = -1
		return true
	for button in _buttons.values():
		if button.index == index:
			button.index = -1
			queue_redraw()
			return true
	return false


# The look touch is look after all: what was held back goes to the view.
func _settle_look() -> void:
	_look_kind = LOOK_DRAG
	_look_accum += _look_held
	_look_held = Vector2.ZERO


func _swipe(index: int, verb: StringName) -> void:
	_look_index = -1
	_look_held = Vector2.ZERO
	_swipe_index = index
	if verb == &"down":
		_down_index = index
		down_held = true
	gesture_counts[verb] += 1
	if router != null:
		router.push_verb(verb)
	pressed_feedback.emit()


func _update_stick_vector() -> void:
	# Held forward until this moment: a thumb held still sends no drags.
	if _stick_raw_forward >= FLICK_FORWARD:
		_stick_forward_clock = _clock
	var throw := STICK_THROW * _u
	var value := _stick_knob / throw
	var magnitude := value.length()
	if magnitude < STICK_DEADZONE:
		move_vector = Vector2.ZERO
		sprint_latched = false
		_stick_flipped = false
		_stick_raw_forward = 0.0
		return
	var scaled := minf(1.0, (magnitude - STICK_DEADZONE) / (1.0 - STICK_DEADZONE))
	move_vector = Vector2(value.x, -value.y) / magnitude * scaled
	_stick_raw_forward = move_vector.y
	if move_vector.y > FLICK_HALF:
		_stick_flipped = false
		if move_vector.y >= FLICK_FORWARD:
			_stick_forward_clock = _clock
	elif _gestures_live and _turn_allowed and not _stick_flipped and move_vector.y <= -FLICK_BACK \
			and _clock - _stick_forward_clock <= FLICK_SECONDS:
		_stick_flipped = true
		gesture_counts[&"turn"] += 1
		if router != null:
			router.push_verb(&"turn")
	if _stick_flipped:
		move_vector = -move_vector
	if move_vector.y < SPRINT_HOLD:
		sprint_latched = false
	if scaled > 0.3:
		_moved = true


# ctx is main.gd's per-frame read of native state (see _read_context there).
func update_context(ctx: Dictionary, delta: float) -> void:
	_clock += delta
	if visible:
		_hint_clock += delta
	if _look_index != -1 and _look_kind == LOOK_UNDECIDED and _clock - _look_start_clock > SWIPE_SECONDS:
		_settle_look()
	var hanging: bool = ctx["hanging"]
	var climbing: bool = ctx.get("climbing", false)
	var swinging: bool = ctx.get("swinging", false)
	var driving: bool = ctx.get("forklift_driving", false)
	var craning: bool = ctx.get("crane_operating", false)
	var seated: bool = ctx.get("sling_seated", false) or ctx.get("sling_recovering", false) or \
		ctx.get("swing_seated", false)
	_gestures_live = gestures and not (driving or craning or seated)
	var jump: TouchButton = _buttons[B_JUMP]
	jump.shown = not _gestures_live
	# Pressable while airborne on purpose: main.gd buffers a press made just
	# before touchdown and fires it on the grounded tick. Dimmed, not dead.
	jump.enabled = true
	jump.dimmed = not (ctx["jump_ok"] or hanging or climbing or swinging)
	jump.icon = &"climb" if hanging else &"jump"
	jump.label = "CLIMB UP" if hanging else ("JUMP OFF" if climbing or swinging else "JUMP")
	jump.tone = TONE_PRIMARY
	if driving or craning:
		jump.icon = &"up"
		jump.label = "RAISE"
		jump.dimmed = false

	var action: TouchButton = _buttons[B_ACTION]
	var act: Dictionary = ctx["action"]
	# Swiped, the climbs and the letting go are the swipes': Action is drawn
	# only for what it alone does.
	action.shown = not _gestures_live or (act["id"] != &"" and
		not act["id"] in [&"climb", &"climb_up", &"let_go", &"trapeze"])
	# While hanging, Jump already reads CLIMB UP (the same native mantle);
	# a second identical button beside it is noise, so Action rests.
	action.enabled = act["id"] != &"" and not hanging
	action.icon = act["icon"]
	action.label = act["label"]
	action.tone = TONE_PRIMARY if action.enabled else TONE_GHOST

	# Drop lets go of a ledge or a climb's holds, and at an edge behind the
	# body lowers it over into a hang.
	var drop: TouchButton = _buttons[B_DROP]
	drop.shown = ctx.get("drop_ok", hanging) and not _gestures_live
	if ctx.get("sling_seated", false) or ctx.get("sling_recovering", false) or ctx.get("swing_seated", false):
		drop.label = "LEAVE"
	else:
		drop.label = "DROP DOWN" if not (hanging or climbing or swinging) else "DROP"
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
	var hook: TouchButton = _buttons[B_HOOK]
	var hook_state: int = ctx.get("hook_state", 0)
	hook.shown = ctx.get("has_hook", false)
	hook.icon = &"up"
	hook.label = "HOOK" if hook_state == 0 else ("REEL" if hook_state == 2 else "...")
	hook.tone = TONE_PRIMARY if hook_state == 2 else TONE_NORMAL

	# The quickturn: on foot, in the air and on a wall; not with the hands on
	# something, nor in a seat or at the forklift's wheel.
	var crane_button: TouchButton = _buttons[B_CRANE]
	crane_button.shown = ctx.get("has_remote", false)
	crane_button.enabled = craning or ctx.get("crane_able", false)
	crane_button.label = "WALK" if craning else "CRANE"
	crane_button.tone = TONE_PRIMARY if craning else TONE_NORMAL
	_turn_allowed = not (hanging or climbing or swinging or driving or craning or seated or
		ctx.get("trapeze_hanging", false))
	_buttons[B_TURN].shown = _turn_allowed and not gestures
	var crouch: TouchButton = _buttons[B_CROUCH]
	crouch.shown = not hanging and not _gestures_live
	crouch.icon = &"stand" if ctx["crouched"] else &"crouch"
	crouch.label = "STAND" if ctx["crouched"] else "CROUCH"
	crouch.tone = TONE_SAFE if ctx["crouched"] else TONE_NORMAL
	if climbing:
		# On a climb's holds: slide down them (the fireman's slide), or hold on.
		var sliding_down: bool = ctx.get("crouch_toggled", false)
		crouch.icon = &"up" if sliding_down else &"down"
		crouch.label = "HOLD ON" if sliding_down else "SLIDE DOWN"
		crouch.tone = TONE_SAFE if sliding_down else TONE_NORMAL
	if driving or craning:
		crouch.icon = &"down"
		crouch.label = "LOWER"
		crouch.tone = TONE_NORMAL

	for button in _buttons.values():
		if not button.shown and button.index != -1:
			button.index = -1
		button.appear = move_toward(button.appear, 1.0 if button.shown else 0.0, APPEAR_RATE * delta)
		button.flash = maxf(0.0, button.flash - delta * 3.5)
	if visible:
		queue_redraw()


func _draw() -> void:
	_draw_stick()
	for id in _order:
		_draw_button(_buttons[id])
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


func _draw_hints() -> void:
	_draw_gesture_hints()
	if _hint_clock > HINT_SECONDS or (_moved and _looked):
		return
	var alpha := clampf(_hint_clock * 2.0, 0.0, 1.0) * clampf(HINT_SECONDS - _hint_clock, 0.0, 1.0)
	var size := int(roundf(26.0 * _u))
	var font := UiStyle.font_label()
	if not _moved:
		UiStyle.text_centered(self, font, "MOVE", _stick_home + Vector2(0.0, -STICK_THROW * _u - 34.0 * _u),
			size, UiStyle.with_alpha(UiStyle.PAPER, 0.8 * alpha), int(roundf(6.0 * _u)))
	if not _looked:
		var at := Vector2(_safe.position.x + _safe.size.x * 0.72, _safe.position.y + _safe.size.y * 0.6)
		UiStyle.draw_icon(self, &"look", at + Vector2(0.0, -48.0 * _u), 40.0 * _u,
			UiStyle.with_alpha(UiStyle.PAPER, 0.7 * alpha), 4.0 * _u)
		UiStyle.text_centered(self, font, "DRAG TO LOOK", at, size,
			UiStyle.with_alpha(UiStyle.PAPER, 0.8 * alpha), int(roundf(6.0 * _u)))


# Where the right thumb swipes: what up and down would do, when it is more
# than a jump or a crouch.
func _draw_gesture_hints() -> void:
	if not _gestures_live:
		return
	var at := Vector2(_safe.position.x + _safe.size.x * 0.8, _safe.position.y + _safe.size.y * 0.5)
	var size := int(roundf(26.0 * _u))
	var font := UiStyle.font_label()
	if up_hint_special and not up_hint.is_empty():
		UiStyle.draw_icon(self, &"up", at + Vector2(0.0, -86.0 * _u), 26.0 * _u,
			UiStyle.with_alpha(UiStyle.AMBER, 0.9), 4.0 * _u)
		UiStyle.text_centered(self, font, up_hint, at + Vector2(0.0, -42.0 * _u), size,
			UiStyle.with_alpha(UiStyle.PAPER, 0.9), int(roundf(6.0 * _u)))
	if down_hint_special and not down_hint.is_empty():
		UiStyle.text_centered(self, font, down_hint, at + Vector2(0.0, 42.0 * _u), size,
			UiStyle.with_alpha(UiStyle.PAPER, 0.9), int(roundf(6.0 * _u)))
		UiStyle.draw_icon(self, &"down", at + Vector2(0.0, 86.0 * _u), 26.0 * _u,
			UiStyle.with_alpha(UiStyle.AMBER, 0.9), 4.0 * _u)
