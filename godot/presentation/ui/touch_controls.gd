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
const SettingsStore := preload("res://presentation/ui/settings_store.gd")
const ThumbGestures := preload("res://presentation/ui/thumb_parkour_gestures.gd")

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
const B_REEL := &"slingshot_reel"
const B_AUTO_REEL := &"auto_reel"
const B_GRAPPLE := &"grapple"
const B_FORWARD := &"forklift_forward"
const B_NEUTRAL := &"forklift_neutral"
const B_REVERSE := &"forklift_reverse"
const B_PARK := &"forklift_park"
const B_INCH := &"forklift_inch"
const B_HORN := &"forklift_horn"
const B_TOOL := &"forklift_tool"
const B_RIG := &"yard_crane_attach"
const B_UNRIG := &"yard_crane_detach"

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
var reel_effort := 0.0
var touch_scale := 1.0
var forklift_mode := false
var yard_crane_mode := false
var forklift_inch := 0.0
var forklift_horn := false
var control_profile: int = SettingsStore.TouchProfile.BUTTONS

var _u := 1.0
var _buttons := {}
var _order: Array[StringName] = [B_PAUSE, B_JUMP, B_ACTION, B_CROUCH, B_DROP, B_CHUTE, B_UP,
	B_DOWN, B_LEFT, B_RIGHT, B_SLING, B_REEL, B_AUTO_REEL, B_GRAPPLE,
	B_FORWARD, B_NEUTRAL, B_REVERSE, B_PARK, B_INCH, B_HORN, B_TOOL, B_RIG, B_UNRIG]
var _stick_index := -1
var _stick_origin := Vector2.ZERO
var _stick_knob := Vector2.ZERO
var _stick_home := Vector2.ZERO
var _stick_zone := Rect2()
var _safe := Rect2()
var _look_index := -1
var _look_accum := Vector2.ZERO
var _operating := false
var _portable_tool := false
var _split_drop_chute := false
var _movement_blocked := false
var _retrieving := false
var _axis_names := ["", ""]
var _pendant_center := Vector2.ZERO
var _hint_clock := 0.0
var _moved := false
var _looked := false
var _clock := 0.0
var _thumb := ThumbGestures.new()
var _thumb_active := false
var _thumb_ctx := {}
var _thumb_reach_owned := false
var _thumb_drop_identity: Array = []
var _thumb_crouch_ok := false
var _thumb_notice := ""
var _thumb_notice_left := 0.0
var _stick_tap_age := 0.0
var _stick_tap_candidate := false


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for id in _order:
		var button := TouchButton.new()
		button.id = id
		button.hold = id in [B_UP, B_DOWN, B_LEFT, B_RIGHT, B_REEL, B_INCH, B_HORN]
		_buttons[id] = button
	_buttons[B_PAUSE].icon = &"pause"
	_buttons[B_DROP].icon = &"drop"
	_buttons[B_DROP].label = "DROP"
	_buttons[B_UP].icon = &"up"
	_buttons[B_DOWN].icon = &"down"
	_buttons[B_LEFT].icon = &"left"
	_buttons[B_RIGHT].icon = &"right"
	_buttons[B_SLING].icon = &"sling"
	_buttons[B_REEL].icon = &"operate"
	_buttons[B_REEL].label = "HOLD TO REEL"
	_buttons[B_REEL].tone = TONE_PRIMARY
	_buttons[B_AUTO_REEL].icon = &"operate"
	_buttons[B_AUTO_REEL].tone = TONE_PRIMARY
	_buttons[B_GRAPPLE].icon = &"hook"
	_buttons[B_GRAPPLE].tone = TONE_PRIMARY
	for id in [B_FORWARD, B_NEUTRAL, B_REVERSE, B_PARK, B_INCH, B_HORN]:
		_buttons[id].label = {B_FORWARD: "F", B_NEUTRAL: "N", B_REVERSE: "R", B_PARK: "PARK", B_INCH: "INCH", B_HORN: "HORN"}[id]
	resized.connect(_layout)
	_layout()


func set_touch_scale(value: float) -> void:
	if not is_equal_approx(value, touch_scale):
		reset_touches()
	touch_scale = value
	_layout()


func set_control_profile(value: int) -> void:
	var profile := value if value == SettingsStore.TouchProfile.THUMB_FLOW else SettingsStore.TouchProfile.BUTTONS
	if control_profile == profile:
		return
	control_profile = profile
	reset_touches()
	_thumb_active = profile == SettingsStore.TouchProfile.THUMB_FLOW and not _operating \
		and not _retrieving and not bool(_thumb_ctx.get("slingshot_seated", false))
	_hint_clock = 0.0
	_moved = false
	_looked = false
	if not _thumb_ctx.is_empty():
		update_context(_thumb_ctx, 0.0)
	queue_redraw()


func button_center(id: StringName) -> Vector2:
	return _buttons[id].center


func stick_home() -> Vector2:
	return _stick_home


func is_button_shown(id: StringName) -> bool:
	return _buttons[id].shown


func button_label(id: StringName) -> String:
	return _buttons[id].label


func _layout() -> void:
	_layout_in_safe_rect(UiStyle.safe_rect(self))


func _layout_in_safe_rect(safe: Rect2) -> void:
	if _safe != safe:
		reset_touches()
	_safe = safe
	# Fit the safe short edge as well as the design canvas. Small windows and
	# inset displays must not inherit the global quarter-unit drawing floor.
	_u = minf(UiStyle.unit(self), minf(_safe.size.x, _safe.size.y) / 1856.0) * touch_scale
	if forklift_mode or yard_crane_mode:
		# Keep both thumb decks below the sightline, including enlarged touch.
		_u = minf(_u, minf(_safe.size.x / 1250.0, _safe.size.y * 0.36 / 620.0))
	var m := 56.0 * _u
	var corner := _safe.end
	var jump := Vector2(corner.x - m - 170.0 * _u, corner.y - m - 190.0 * _u)
	_place(B_JUMP, jump, 122.0)
	_place(B_ACTION, jump + Vector2(-236.0, -150.0) * _u, 98.0)
	# Over Jump, toward the edge: the same thumb slides up to it.
	_place(B_CROUCH, jump + Vector2(40.0, -252.0) * _u, 78.0)
	# Drop and Chute normally share a slot. An equipped grapple can offer
	# both during a fall, so give Chute a distinct hit area in that context.
	_place(B_DROP, jump + Vector2(-306.0, 96.0) * _u, 86.0)
	_place(B_CHUTE, jump + Vector2(-306.0, -450.0 if _split_drop_chute else 96.0) * _u, 86.0)
	_place(B_PAUSE, _safe.position + Vector2(m, m) + Vector2(52.0, 52.0) * _u, 52.0)
	# Leave the HUD altimeter its unscaled 394-unit top-right strip. The
	# equipment circle keeps a 66-unit gap even when touch size changes.
	var grapple_x := corner.x - 394.0 * UiStyle.unit(self) - 126.0 * _u
	_place(B_GRAPPLE, Vector2(grapple_x, _safe.position.y + 108.0 * _u), 60.0)
	_stick_home = Vector2(_safe.position.x + m + 230.0 * _u, corner.y - m - 230.0 * _u)
	_stick_zone = Rect2(_safe.position.x, _safe.position.y + _safe.size.y * 0.22,
		_safe.size.x * 0.46, _safe.size.y * 0.78)
	# Stationary machines use the left thumb for their pendant. The portable
	# grapple and forklift keep the stick and place tool controls to its right.
	_pendant_center = Vector2(_safe.position.x + _safe.size.x * 0.36, corner.y - m - 620.0 * _u) \
		if _portable_tool else _stick_home + Vector2(20.0, -40.0) * _u
	var arm := (190.0 if _portable_tool else 158.0) * _u
	_place(B_REEL, _pendant_center, 110.0)
	# Without a portable pendant the center belongs to the movement stick
	# (or recovery reel), so automatic reeling needs its own target above it.
	_place(B_AUTO_REEL, _pendant_center + Vector2(0.0, 0.0 if _portable_tool else -400.0) * _u, 60.0)
	_place(B_UP, _pendant_center + Vector2(0.0, -arm), 80.0)
	_place(B_DOWN, _pendant_center + Vector2(0.0, arm), 80.0)
	_place(B_LEFT, _pendant_center + Vector2(-arm, 0.0), 80.0)
	_place(B_RIGHT, _pendant_center + Vector2(arm, 0.0), 80.0)
	_place(B_SLING, _pendant_center + Vector2(arm + 250.0 * _u, -arm * 0.6), 78.0)
	if forklift_mode:
		_layout_forklift()
	if yard_crane_mode:
		_layout_yard_crane()
	queue_redraw()


func set_forklift_mode(value: bool) -> void:
	if forklift_mode == value:
		return
	forklift_mode = value
	reset_touches()
	_layout()


func set_yard_crane_mode(value: bool) -> void:
	if yard_crane_mode == value:
		return
	yard_crane_mode = value
	reset_touches()
	_layout()


func _layout_yard_crane() -> void:
	_stick_home = Vector2(_safe.position.x, _safe.end.y) + Vector2(200.0, -230.0) * _u
	_stick_zone = Rect2(Vector2(_safe.position.x, _safe.end.y - 620.0 * _u), Vector2(490.0, 620.0) * _u)
	_place(B_UP, _safe.end + Vector2(-150.0, -420.0) * _u, 64.0)
	_place(B_DOWN, _safe.end + Vector2(-150.0, -170.0) * _u, 64.0)
	_place(B_ACTION, Vector2(_safe.end.x - 108.0 * _u, _safe.position.y + 108.0 * _u), 52.0)


func _layout_forklift() -> void:
	# Drive and selector belong to the left thumb; hydraulics to the right.
	# No operator button crosses the forward view through the mast.
	var left := Vector2(_safe.position.x, _safe.end.y)
	var right := _safe.end
	_stick_home = left + Vector2(200.0, -230.0) * _u
	_stick_zone = Rect2(Vector2(_safe.position.x, _safe.end.y - 620.0 * _u), Vector2(490.0, 620.0) * _u)
	for row in [[B_FORWARD, 80.0], [B_NEUTRAL, 240.0], [B_REVERSE, 400.0]]:
		_place(row[0], left + Vector2(row[1], -550.0) * _u, 52.0)
	_place(B_PARK, left + Vector2(420.0, -150.0) * _u, 54.0)
	_place(B_INCH, left + Vector2(420.0, -360.0) * _u, 54.0)
	_place(B_UP, right + Vector2(-340.0, -420.0) * _u, 64.0)
	_place(B_DOWN, right + Vector2(-340.0, -170.0) * _u, 64.0)
	_place(B_LEFT, right + Vector2(-130.0, -420.0) * _u, 64.0)
	_place(B_RIGHT, right + Vector2(-130.0, -170.0) * _u, 64.0)
	_place(B_TOOL, Vector2(_safe.get_center().x - 85.0 * _u, right.y - 90.0 * _u), 60.0)
	_place(B_HORN, Vector2(_safe.get_center().x + 85.0 * _u, right.y - 90.0 * _u), 52.0)
	_place(B_ACTION, Vector2(right.x - 108.0 * _u, _safe.position.y + 108.0 * _u), 52.0)


func _place(id: StringName, center: Vector2, radius: float) -> void:
	var button: TouchButton = _buttons[id]
	button.center = center
	button.radius = radius * _u


func _verb_for(id: StringName) -> StringName:
	if id in [B_FORWARD, B_NEUTRAL, B_REVERSE, B_PARK, B_TOOL, B_RIG, B_UNRIG]:
		return id
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
		B_AUTO_REEL:
			return &"auto_reel"
		B_GRAPPLE:
			return &"toggle_grapple"
	return &""


func reset_touches() -> void:
	_thumb.reset()
	_thumb_reach_owned = false
	_thumb_drop_identity.clear()
	_stick_tap_candidate = false
	_stick_tap_age = 0.0
	_stick_index = -1
	_look_index = -1
	_look_accum = Vector2.ZERO
	move_vector = Vector2.ZERO
	sprint_latched = false
	for button in _buttons.values():
		button.index = -1
	_refresh_held_controls()
	queue_redraw()


func take_look_delta() -> Vector2:
	var value := _look_accum + _thumb.take_look()
	_look_accum = Vector2.ZERO
	return value


# Called once by the input owner before it consumes this frame's verbs.
# Menu, rendering and native tick rate do not provide a second gesture clock.
func advance_input(delta: float) -> void:
	if not _thumb_active:
		return
	if _stick_tap_candidate:
		_stick_tap_age += maxf(0.0, delta)
	_thumb.advance(delta)
	if _thumb.armed_feedback or _thumb.ready_feedback:
		pressed_feedback.emit()
		_thumb.armed_feedback = false
		_thumb.ready_feedback = false
	_flush_thumb_commands()


func handle_touch(event: InputEvent) -> bool:
	if event is InputEventScreenTouch:
		var touch := event as InputEventScreenTouch
		if touch.pressed and not touch.canceled:
			return _touch_down(touch.index, touch.position)
		return _touch_up(touch.index, touch.canceled)
	if event is InputEventScreenDrag:
		var drag := event as InputEventScreenDrag
		return _touch_drag(drag.index, drag.position, drag.relative)
	return false


func _touch_down(index: int, at: Vector2) -> bool:
	# A fresh press owns exactly one control, including finger-ID reuse.
	# Clear every old owner so release cannot stop at an unrelated capture.
	_touch_up(index, true)
	for id in _order:
		var button: TouchButton = _buttons[id]
		# Fade is presentation only. A newly enabled Cancel must receive the
		# press immediately during the native's short reach-intent window.
		if not button.shown or not button.enabled or button.index != -1:
			continue
		if at.distance_to(button.center) > button.radius * HIT_SLOP:
			continue
		button.index = index
		button.press_position = at
		_refresh_held_controls()
		button.flash = 1.0
		if not button.hold and router != null:
			router.push_verb(_verb_for(id))
		pressed_feedback.emit()
		queue_redraw()
		return true
	if _stick_index == -1 and not _movement_blocked and _stick_zone.has_point(at):
		_stick_index = index
		_stick_origin = at
		_stick_knob = Vector2.ZERO
		_stick_tap_age = 0.0
		_stick_tap_candidate = _thumb_active
		_update_stick_vector()
		return true
	if _look_index == -1:
		_look_index = index
		if _thumb_active and _safe.has_point(at) and at.x >= _safe.get_center().x:
			_thumb.begin(index, at, _u)
		return true
	return false


func _touch_drag(index: int, at: Vector2, relative: Vector2) -> bool:
	if index == _stick_index:
		var throw := STICK_THROW * _u
		var offset := at - _stick_origin
		if offset.length() > maxf(6.0, LOOK_SLOP * _u):
			_stick_tap_candidate = false
		if not forklift_mode and not yard_crane_mode and offset.length() > throw * SPRINT_OVERSHOOT and -offset.y > absf(offset.x):
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
		if index == _thumb.index:
			var old_sector: int = _thumb.sector
			_thumb.drag(at, relative)
			if _thumb.sector == ThumbGestures.Sector.DROP and old_sector != _thumb.sector:
				_thumb_drop_identity = _thumb_contact_identity()
			_flush_thumb_commands()
			_looked = _looked or _thumb.phase == ThumbGestures.Phase.LOOK
			queue_redraw()
			return true
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


func _touch_up(index: int, cancelled: bool = false) -> bool:
	var released := false
	if index == _stick_index:
		if _stick_tap_candidate and not cancelled and _stick_tap_age < ThumbGestures.ARM_SECONDS \
				and _thumb_crouch_ok and router != null:
			router.push_verb(&"crouch")
			pressed_feedback.emit()
		_stick_tap_candidate = false
		_stick_index = -1
		_stick_knob = Vector2.ZERO
		move_vector = Vector2.ZERO
		sprint_latched = false
		released = true
	if index == _look_index:
		if index == _thumb.index:
			_thumb.release(cancelled)
			_flush_thumb_commands()
			_look_accum += _thumb.take_look()
			# Lifting can cancel this finger's unfulfilled reach. It never
			# releases a grip the native solver has actually acquired.
			if _thumb_reach_owned and not bool(_thumb_ctx.get("grounded", false)) \
					and not bool(_thumb_ctx.get("hanging", false)) \
					and not bool(_thumb_ctx.get("climbing", false)) and router != null:
				router.push_verb(&"drop")
			_thumb_reach_owned = false
		_look_index = -1
		released = true
	for button in _buttons.values():
		if button.index == index:
			button.index = -1
			released = true
	if released:
		_refresh_held_controls()
		queue_redraw()
	return released


func _thumb_contact_identity() -> Array:
	return [_thumb_ctx.get("traversal", 0), _thumb_ctx.get("left_hand_generation", 0),
		_thumb_ctx.get("right_hand_generation", 0), _thumb_ctx.get("carrying", 0),
		_thumb_ctx.get("grounded", false), _thumb_ctx.get("lowering", false),
		_thumb_ctx.get("support_entity", 0)]


func _flush_thumb_commands() -> void:
	for command in _thumb.take_commands():
		var verb: StringName = command
		var allowed := true
		var action: Dictionary = _thumb_ctx.get("action", {})
		if command == &"primary":
			# A tap cannot accidentally let go of a solo hand or swing.
			# Those native Jump paths mean release, so use deliberate Drop.
			verb = &"action" if bool(_thumb_ctx.get("single_hand_hold", false)) \
				or bool(_thumb_ctx.get("swinging", false)) else &"jump"
		if verb == &"action":
			allowed = action.get("id", &"") != &""
			if action.get("id", &"") == &"reach_hand":
				allowed = bool(_thumb_ctx.get("missing_hand_grip_available", false))
		elif verb == &"crouch":
			allowed = _thumb_crouch_ok
		elif verb == &"drop":
			allowed = (bool(_thumb_ctx.get("drop_ok", false)) or int(_thumb_ctx.get("carrying", 0)) != 0) \
				and _thumb_drop_identity == _thumb_contact_identity()
		if allowed and router != null:
			router.push_verb(verb)
			if verb == &"action" and action.get("id", &"") == &"catch":
				_thumb_reach_owned = true
			_thumb_notice = "REACH · LIFT TO CANCEL" if _thumb_reach_owned else String(
				action.get("label", "REACH") if verb == &"action" else verb).to_upper()
			pressed_feedback.emit()
		else:
			_thumb_notice = "NO CLEAR GRIP" if verb == &"action" else "NOT AVAILABLE"
		_thumb_notice_left = 0.8
	queue_redraw()


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
	_thumb_notice_left = maxf(0.0, _thumb_notice_left - delta)
	if visible:
		_hint_clock += delta
	var hanging: bool = ctx["hanging"]
	var climbing: bool = ctx.get("climbing", false)
	var swinging: bool = ctx.get("swinging", false)
	var passive_release := swinging or bool(ctx.get("single_hand_hold", false))
	var jump: TouchButton = _buttons[B_JUMP]
	jump.shown = true
	# Pressable while airborne on purpose: main.gd buffers a press made just
	# before touchdown and fires it on the grounded tick. Dimmed, not dead.
	jump.enabled = true
	jump.dimmed = not (ctx["jump_ok"] or hanging or climbing)
	# A passive swing or solo hold releases its actual hands without a jump impulse.
	jump.icon = &"drop" if passive_release else (&"climb" if hanging else &"jump")
	jump.label = "LET GO" if passive_release else ("CLIMB UP" if hanging else ("JUMP OFF" if climbing else "JUMP"))
	jump.tone = TONE_PRIMARY

	var action: TouchButton = _buttons[B_ACTION]
	var act: Dictionary = ctx["action"]
	var reach_hand: bool = act["id"] == &"reach_hand"
	# A solo hold offers an explicit second hand independently of its release
	# buttons. Keep the blocked offer visible while native reports no grip.
	action.shown = reach_hand or not passive_release
	# Ordinary two-hand hanging already has CLIMB UP on Jump.
	action.enabled = (bool(ctx.get("missing_hand_grip_available", false)) if reach_hand
		else act["id"] != &"" and not hanging and not passive_release)
	action.icon = act["icon"]
	action.label = act["label"]
	action.tone = TONE_PRIMARY if action.enabled or reach_hand else TONE_GHOST

	# Drop lets go of a ledge or a climb's holds, and at an edge behind the
	# body lowers it over into a hang.
	var drop: TouchButton = _buttons[B_DROP]
	drop.shown = ctx.get("drop_ok", hanging)
	drop.label = ("RELEASE" if ctx.get("grapple_line_attached", false) else "STOW") \
		if ctx.get("grapple_equipped", false) else ("LET GO" if passive_release else ("DROP" if hanging or climbing or ctx.get("lowering", false) else "DROP DOWN"))
	if bool(ctx.get("air_catch_intent_pending", false)):
		drop.label = "CANCEL"
	var chute: TouchButton = _buttons[B_CHUTE]
	chute.shown = ctx["chute_ok"] and not hanging
	var split_drop_chute: bool = drop.shown and chute.shown
	if _split_drop_chute != split_drop_chute:
		_split_drop_chute = split_drop_chute
		_layout()
	chute.icon = &"chute"
	chute.label = "STOW" if ctx["chute"] else "CHUTE"
	if ctx["chute"]:
		chute.tone = TONE_SAFE
	elif ctx["danger"] >= 0.6:
		chute.tone = TONE_DANGER
	else:
		chute.tone = TONE_NORMAL
	_buttons[B_PAUSE].shown = true

	# Native acknowledgement owns equipped state; pressing never changes it here.
	var grapple: TouchButton = _buttons[B_GRAPPLE]
	var equipped := bool(ctx.get("grapple_equipped", false))
	grapple.shown = equipped or bool(ctx.get("grapple_draw_available", false))
	grapple.enabled = grapple.shown
	grapple.label = "STOW" if equipped else "DRAW"
	grapple.tone = TONE_SAFE if equipped else TONE_PRIMARY

	var station: StringName = ctx["operating"]
	set_forklift_mode(station == &"forklift")
	set_yard_crane_mode(station == &"yard_crane")
	_operating = station != &""
	_retrieving = bool(ctx.get("slingshot_recovering", false))
	var portable_tool := station in [&"grappling_gun", &"forklift", &"yard_crane"]
	if _portable_tool != portable_tool:
		_portable_tool = portable_tool
		_layout()
	_movement_blocked = (_operating and not _portable_tool) or _retrieving
	_buttons[B_REEL].shown = _retrieving
	var auto_reel: TouchButton = _buttons[B_AUTO_REEL]
	auto_reel.shown = bool(ctx.get("grapple_equipped", false)) and bool(ctx.get("grapple_line_attached", false))
	auto_reel.enabled = auto_reel.shown
	auto_reel.label = "STOP REEL" if bool(ctx.get("grapple_auto_reel", false)) else "AUTO REEL"
	auto_reel.tone = TONE_SAFE if bool(ctx.get("grapple_auto_reel", false)) else TONE_PRIMARY
	var crouch: TouchButton = _buttons[B_CROUCH]
	var tuck_available: bool = ctx.get("swing_tuck_available", false)
	var tuck_requested: bool = ctx.get("swing_tuck_requested", false)
	crouch.shown = not _movement_blocked and (tuck_available or (not hanging and not climbing))
	var folded: bool = tuck_requested if tuck_available else bool(ctx["crouched"])
	crouch.icon = &"stand" if folded else &"crouch"
	crouch.label = ("EXTEND" if folded else "TUCK") if tuck_available else ("STAND" if folded else "CROUCH")
	crouch.tone = TONE_SAFE if folded else TONE_NORMAL
	var has_x := station in [&"jib", &"intake", &"forklift"]
	_buttons[B_UP].shown = _operating
	_buttons[B_DOWN].shown = _operating
	_buttons[B_LEFT].shown = _operating and has_x
	_buttons[B_RIGHT].shown = _operating and has_x
	var sling: TouchButton = _buttons[B_SLING]
	sling.shown = station == &"intake"
	sling.label = "RELEASE PACK" if ctx["slung"] else "ATTACH PACK"
	sling.tone = TONE_PRIMARY
	match station:
		&"forklift":
			_axis_names = ["RAISE / LOWER", "TILT BACK / FORWARD"]
		&"grappling_gun":
			_axis_names = ["TIGHTEN / PAY OUT", ""]
		&"service_lift":
			_axis_names = ["SERVICE LIFT", ""]
		&"jib":
			_axis_names = ["HOIST", "DRIVE"]
		&"intake":
			_axis_names = ["HOIST", "SLEW"]
		&"needle":
			_axis_names = ["RAISE / LOWER", ""]
		_:
			_axis_names = ["", ""]
	for id in [B_FORWARD, B_NEUTRAL, B_REVERSE, B_PARK, B_INCH, B_HORN]:
		_buttons[id].shown = forklift_mode
	_buttons[B_TOOL].shown = forklift_mode
	if forklift_mode:
		_buttons[B_TOOL].label = "UNCOUPLE" if bool(ctx.get("forklift_tool_coupled", true)) else "COUPLE"
		_buttons[B_TOOL].tone = TONE_SAFE if bool(ctx.get("forklift_tool_ready", false)) else TONE_NORMAL
		for id in [B_JUMP, B_CROUCH, B_DROP, B_CHUTE, B_GRAPPLE, B_AUTO_REEL, B_REEL]:
			_buttons[id].shown = false
		_buttons[B_UP].label = "LIFT +"
		_buttons[B_DOWN].label = "LIFT -"
		_buttons[B_LEFT].label = "TILT BACK"
		_buttons[B_RIGHT].label = "TILT FWD"
		var operator: Dictionary = ctx.get("forklift_operator", {})
		for entry in [[B_FORWARD, 1], [B_NEUTRAL, 0], [B_REVERSE, -1]]:
			_buttons[entry[0]].tone = TONE_SAFE if int(operator.get("gear", 0)) == entry[1] else TONE_NORMAL
		_buttons[B_PARK].tone = TONE_SAFE if bool(operator.get("parking", true)) else TONE_NORMAL
	else:
		for id in [B_UP, B_DOWN, B_LEFT, B_RIGHT]:
			_buttons[id].label = ""
	for id in [B_RIG, B_UNRIG]:
		# Rigging is the ordinary on-foot contextual action at each real eye.
		# The booth cannot reach those eyes, so it offers no remote rig button.
		_buttons[id].shown = false
	if yard_crane_mode:
		for id in [B_JUMP, B_CROUCH, B_DROP, B_CHUTE, B_GRAPPLE, B_AUTO_REEL, B_REEL, B_LEFT, B_RIGHT]:
			_buttons[id].shown = false
		_buttons[B_UP].label = "HOIST +"
		_buttons[B_DOWN].label = "HOIST -"
		_buttons[B_RIG].label = "RIG"
		_buttons[B_UNRIG].label = "UNRIG"
		_axis_names = ["HOIST", ""]
	if _movement_blocked and _stick_index != -1:
		_touch_up(_stick_index)
	_update_thumb_context(ctx)

	for button in _buttons.values():
		if (not button.shown or not button.enabled) and button.index != -1:
			button.index = -1
		button.appear = move_toward(button.appear, 1.0 if button.shown else 0.0, APPEAR_RATE * delta)
		button.flash = maxf(0.0, button.flash - delta * 3.5)
	_refresh_held_controls()
	if visible:
		queue_redraw()


func _update_thumb_context(ctx: Dictionary) -> void:
	var active := control_profile == SettingsStore.TouchProfile.THUMB_FLOW and not _operating \
		and not _retrieving and not bool(ctx.get("slingshot_seated", false))
	if active != _thumb_active:
		reset_touches()
		_thumb_active = active
	_thumb_ctx = ctx
	if bool(ctx.get("hanging", false)) or bool(ctx.get("climbing", false)) or bool(ctx.get("grounded", false)):
		_thumb_reach_owned = false
	_thumb_crouch_ok = _buttons[B_CROUCH].shown
	if _thumb_active:
		for id in [B_JUMP, B_ACTION, B_CROUCH, B_DROP]:
			_buttons[id].shown = false


func _refresh_held_controls() -> void:
	pendant_axes = Vector2(
		_held(B_RIGHT) - _held(B_LEFT), _held(B_UP) - _held(B_DOWN))
	reel_effort = _held(B_REEL)
	forklift_inch = _held(B_INCH)
	forklift_horn = _held(B_HORN) > 0.0


func _held(id: StringName) -> float:
	var button: TouchButton = _buttons[id]
	return 1.0 if button.shown and button.index != -1 else 0.0


func _draw() -> void:
	if not _movement_blocked:
		_draw_stick()
	for id in _order:
		_draw_button(_buttons[id])
	if _operating:
		_draw_pendant_labels()
	if _thumb_active:
		_draw_thumb_commands()
	else:
		_draw_hints()


func _draw_thumb_commands() -> void:
	var font := UiStyle.font_label()
	var text_size := maxi(10, int(roundf(26.0 * _u)))
	var home: Vector2 = _buttons[B_JUMP].center
	var action: Dictionary = _thumb_ctx.get("action", {})
	var primary := "REACH" if bool(_thumb_ctx.get("single_hand_hold", false)) \
		or bool(_thumb_ctx.get("swinging", false)) else ("CLIMB UP" if bool(_thumb_ctx.get("hanging", false)) else "JUMP")
	if _thumb.phase != ThumbGestures.Phase.COMMAND:
		if _thumb_crouch_ok:
			UiStyle.text_centered(self, font, "TAP: " + _buttons[B_CROUCH].label,
				_stick_home + Vector2(0.0, 200.0) * _u, text_size, UiStyle.PAPER_DIM, int(5.0 * _u))
		UiStyle.ring(self, home, 90.0 * _u, UiStyle.with_alpha(UiStyle.PAPER, 0.13), 3.0 * _u)
		UiStyle.text_centered(self, font, "TAP: " + primary, home, text_size, UiStyle.PAPER, int(5.0 * _u))
		UiStyle.text_centered(self, font, "HOLD: ACTIONS", home + Vector2(0.0, 40.0) * _u,
			text_size, UiStyle.PAPER_DIM, int(5.0 * _u))
		if _thumb_notice_left > 0.0:
			UiStyle.text_centered(self, font, _thumb_notice, home + Vector2(-80.0, -160.0) * _u,
				text_size, UiStyle.AMBER, int(5.0 * _u))
		if _hint_clock < HINT_SECONDS:
			var hint := _safe.position + Vector2(_safe.size.x * 0.5, _safe.size.y * 0.82)
			UiStyle.text_centered(self, font, "LEFT: DRAG TO MOVE / TAP TO CROUCH · RIGHT: LOOK / TAP TO JUMP", hint,
				text_size, UiStyle.PAPER, int(5.0 * _u))
			UiStyle.text_centered(self, font, "HOLD STILL, THEN ↑ REACH · ↓ CROUCH · ← HOLD + LIFT TO DROP",
				hint + Vector2(0.0, 44.0) * _u, text_size, UiStyle.PAPER_DIM, int(5.0 * _u))
		return
	var margin := Vector2(320.0, 280.0) * _u
	var center := _thumb.origin.clamp(_safe.position + margin, _safe.end - margin)
	UiStyle.disc(self, center, 230.0 * _u, UiStyle.with_alpha(UiStyle.INK, 0.65))
	UiStyle.ring(self, center, _thumb.stroke, UiStyle.with_alpha(UiStyle.AMBER, 0.6), 3.0 * _u)
	var drop_ok := bool(_thumb_ctx.get("drop_ok", false)) or int(_thumb_ctx.get("carrying", 0)) != 0
	var labels := [[Vector2(0.0, -180.0), String(action.get("label", "REACH")), action.get("id", &"") != &""],
		[Vector2(0.0, 180.0), _buttons[B_CROUCH].label, _thumb_crouch_ok],
		[Vector2(-190.0, 0.0), "DROP", drop_ok], [Vector2(190.0, 0.0), "CANCEL", true]]
	for entry in labels:
		UiStyle.text_centered(self, font, entry[1], center + entry[0] * _u, text_size,
			UiStyle.PAPER if entry[2] else UiStyle.PAPER_DIM, int(5.0 * _u))
	if _thumb.sector == ThumbGestures.Sector.DROP:
		var ready: bool = _thumb.drop_hold >= ThumbGestures.DROP_HOLD_SECONDS
		draw_arc(center, _thumb.stroke + 12.0 * _u, -PI * 0.5,
			-PI * 0.5 + TAU * _thumb.drop_hold / ThumbGestures.DROP_HOLD_SECONDS,
			32, UiStyle.HAZARD if ready else UiStyle.AMBER, 8.0 * _u, true)
		UiStyle.text_centered(self, font, "LIFT TO DROP" if ready and drop_ok else "HOLD TO ARM",
			center, text_size, UiStyle.HAZARD if ready and drop_ok else UiStyle.AMBER, int(5.0 * _u))


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
		if forklift_mode and button.id in [B_FORWARD, B_NEUTRAL, B_REVERSE, B_PARK, B_INCH, B_HORN, B_TOOL]:
			# Text-only controls carry their name inside, not beneath an empty disc.
			var font_size := 34.0 if button.id in [B_FORWARD, B_NEUTRAL, B_REVERSE] else 18.0
			var pixels := int(roundf(font_size * _u))
			while pixels > 1 and UiStyle.text_width(UiStyle.font_label(), button.label, pixels) > radius * 1.7:
				pixels -= 1
			UiStyle.text_centered(self, UiStyle.font_label(), button.label,
				button.center, pixels, icon_color)
			return
		UiStyle.text_centered(self, UiStyle.font_label(), button.label,
			button.center + Vector2(0.0, radius + 30.0 * _u), int(roundf(25.0 * _u)),
			UiStyle.with_alpha(UiStyle.PAPER, dim * alpha), int(roundf(6.0 * _u)))


func _forklift_stick_captions() -> Array:
	return [["ACCEL", _stick_home + Vector2(0.0, -205.0) * _u],
		["BRAKE", _stick_home + Vector2(0.0, 190.0) * _u], ["STEER", _stick_home]]


func _draw_stick() -> void:
	var throw := STICK_THROW * _u
	var knob := STICK_KNOB * _u
	if yard_crane_mode:
		UiStyle.text_centered(self, UiStyle.font_label(), "SLEW / TROLLEY", _stick_home + Vector2(0, -205.0) * _u,
			int(roundf(30.0 * _u)), UiStyle.PAPER, int(roundf(5.0 * _u)))
	if forklift_mode:
		for caption in _forklift_stick_captions():
			UiStyle.text_centered(self, UiStyle.font_label(), caption[0], caption[1],
				int(roundf(30.0 * _u)), UiStyle.PAPER, int(roundf(5.0 * _u)))
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
	if forklift_mode or yard_crane_mode:
		return
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
	if not forklift_mode and not yard_crane_mode and not _moved and not _movement_blocked:
		UiStyle.text_centered(self, font, "MOVE", _stick_home + Vector2(0.0, -STICK_THROW * _u - 34.0 * _u),
			size, UiStyle.with_alpha(UiStyle.PAPER, 0.8 * alpha), int(roundf(6.0 * _u)))
	if not _looked:
		var at := Vector2(_safe.position.x + _safe.size.x * 0.72, _safe.position.y + _safe.size.y * 0.6)
		UiStyle.draw_icon(self, &"look", at + Vector2(0.0, -48.0 * _u), 40.0 * _u,
			UiStyle.with_alpha(UiStyle.PAPER, 0.7 * alpha), 4.0 * _u)
		UiStyle.text_centered(self, font, "DRAG TO LOOK", at, size,
			UiStyle.with_alpha(UiStyle.PAPER, 0.8 * alpha), int(roundf(6.0 * _u)))
