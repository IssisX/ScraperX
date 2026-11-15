extends "res://presentation/ui/ui_test_app_leave.gd"

# J-119 (#441): every on-screen control fits every screen. A touch scenario
# running at four window sizes -- the Fold 6 folded and open, a narrow phone,
# a tablet, each in portrait and landscape -- reading every control's rect and
# failing on any overlap, any rect outside the safe area, or any button smaller
# than the minimum touch size; then a resize from cover to inner and back in
# the middle of a climb, and once while a button is held, with every control
# laid out again on the same frame and the held button letting go cleanly.
#
# What the audit reads, and why it is the layout under test, not the game
# state: _layout() places every button from the window size alone, so a
# button's disc and hit circle are where it will draw whenever it shows,
# shown or not. The two named shared slots are the exception -- Drop and
# Chute share one slot, STOW takes Jump's -- and only those two pairs are
# skipped, because the context makes them exclusive (chute shows only when
# not hanging, Drop's slot-mate only with the canopy out). A label is audited
# from the button's current label even while hidden, because update_context
# sets every label from live context each frame: when Drop shows at an edge
# it shows "DROP DOWN" with exactly the rect audited here. Ghosted buttons
# draw no label, so theirs is skipped.
#
# The minimum touch size is 44 canvas units across: the smallest button
# today is Pause, 54 units across on the cover. The number is the bar a
# future shrink has to clear, stated here so the check is not a magic
# constant in the code below.

# The four screens from the issue, each in portrait and landscape, as WINDOW
# sizes: headless has no window, so DisplayServer.window_set_size is a no-op
# there and the scenario sizes the root Window instead. The visible canvas
# is what the stretch mode (canvas_items, expand) computes from the window --
# 2160 wide in portrait, 1856 tall in landscape -- and _layout reads that,
# so the audit below validates the layout the player actually sees. The
# order matters: consecutive entries always differ, so a resize that leaves
# Jump's center where it was proves _layout did not run.
const SCREENS: Array = [
	["cover portrait", Vector2i(968, 2376)],
	["cover landscape", Vector2i(2376, 968)],
	["inner portrait", Vector2i(1856, 2160)],
	["inner landscape", Vector2i(2160, 1856)],
	["phone portrait", Vector2i(1080, 2400)],
	["phone landscape", Vector2i(2400, 1080)],
	["tablet portrait", Vector2i(1600, 2560)],
	["tablet landscape", Vector2i(2560, 1600)],
]
const MIN_TOUCH_DIAMETER := 44.0
const SHARED_SLOTS: Array = [[&"drop", &"chute"], [&"jump", &"chute"]]


func _touch_screens() -> bool:
	var live: bool = await _wait_until(func() -> bool: return _main._touch._safe.size.x > 0.0 and \
		_main._touch.button_center(&"jump") != Vector2.ZERO, 2.0)
	if not live:
		return _fail("touch interface never laid out")
	# The audit at every size, in both orientations.
	for entry in SCREENS:
		var bad: String = await _screens_resize(String(entry[0]), entry[1])
		if not bad.is_empty():
			return _fail(bad)
	# A button held through a resize: Jump held cover -> inner, then let go.
	var bad: String = await _screens_resize("held-jump cover", Vector2i(968, 2376))
	if not bad.is_empty():
		return _fail(bad)
	var center: Vector2 = _main._touch.button_center(&"jump")
	_touch(2, center, true)
	await _frames(2)
	if not _main._touch.is_held(&"jump"):
		return _fail("Jump did not take the hold")
	bad = await _screens_resize("held-jump inner", Vector2i(1856, 2160))
	if not bad.is_empty():
		return _fail(bad)
	if not _main._touch.is_held(&"jump"):
		return _fail("Jump let go on the resize while held")
	_touch(2, _main._touch.button_center(&"jump"), false)
	await _frames(2)
	if _main._touch.is_held(&"jump"):
		return _fail("Jump stayed held after release post-resize")
	# It still works: a fresh press takes it again, and lets go again.
	_touch(2, _main._touch.button_center(&"jump"), true)
	await _frames(1)
	var retakes: bool = _main._touch.is_held(&"jump")
	_touch(2, _main._touch.button_center(&"jump"), false)
	await _frames(1)
	if not retakes or _main._touch.is_held(&"jump"):
		return _fail("Jump did not retake cleanly after the held resize")
	# The stick held through a resize: forward, inner -> cover, then let go,
	# and the walk stops -- no stuck thumb, no stuck move.
	var grounded: bool = await _wait_until(func() -> bool: return bool(_native().is_player_grounded()), 2.0)
	if not grounded:
		return _fail("never grounded after the held-Jump test")
	_stick_push(Vector2(0.0, -1.0))
	await _frames(2)
	if int(_main._touch._stick_index) == -1:
		return _fail("stick did not take the hold")
	bad = await _screens_resize("held-stick cover", Vector2i(968, 2376))
	if not bad.is_empty():
		return _fail(bad)
	if int(_main._touch._stick_index) == -1:
		return _fail("stick let go on the resize while held")
	_touch(0, _main._touch.stick_home(), false)
	await _frames(2)
	if int(_main._touch._stick_index) != -1 or _main._touch.move_vector != Vector2.ZERO:
		return _fail("stick stayed held after release post-resize")
	var stopped: bool = await _wait_until(func() -> bool: return _walk_speed().length() < 0.1, 1.5)
	if not stopped:
		return _fail("player kept walking after the stick let go post-resize (speed %.2f)" % _walk_speed().length())
	# The resize mid-climb: cover -> inner and back while the mantle runs.
	bad = await _screens_resize("climb cover", Vector2i(968, 2376))
	if not bad.is_empty():
		return _fail(bad)
	var offered: bool = await _wait_until(func() -> bool: return String(_ctx()["action"]["id"]) == "climb", 2.0)
	if not offered:
		return _fail("ACTION never offered the climb (got %s)" % str(_ctx()["action"]["id"]))
	_press(&"action", 0)
	var climbing: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) != 0, 0.5)
	if not climbing:
		return _fail("ACTION did not start the climb")
	bad = await _screens_resize("mid-climb inner", Vector2i(1856, 2160))
	if not bad.is_empty():
		return _fail(bad)
	if int(_native().get_traversal_state()) == 0:
		return _fail("climb finished before the mid-climb resize to inner")
	bad = await _screens_resize("mid-climb cover", Vector2i(968, 2376))
	if not bad.is_empty():
		return _fail(bad)
	if int(_native().get_traversal_state()) == 0:
		return _fail("climb finished before the mid-climb resize to cover")
	var done: bool = await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 0 and \
		bool(_native().is_player_grounded()), 2.5)
	if not done:
		return _fail("climb never completed grounded after the resizes")
	_detail = "sizes=8 held=jump+stick climb=ok"
	return true


# Resize the window to `size`, prove it moved and _layout ran on the same
# frame, and audit the layout there. "" is a pass. The root Window is sized
# directly: headless has no DisplayServer window (window_get_size is (0, 0)
# and window_set_size does nothing there), and the visible canvas follows
# the root size through the stretch mode.
func _screens_resize(tag: String, size: Vector2i) -> String:
	var before: Vector2 = _main._touch.button_center(&"jump")
	var changed: bool = get_tree().root.size != size
	get_tree().root.size = size
	await _frames(3)
	if get_tree().root.size != size:
		return "%s resize to %s did not move the window (still %s)" % [tag, str(size), str(get_tree().root.size)]
	# A resize to the size already there has nothing to move; the audit below
	# is the check there. A real resize must move the layout on the frame.
	if changed and _main._touch.button_center(&"jump") == before:
		return "%s layout did not move after the resize to %s" % [tag, str(size)]
	var bad: String = _audit_layout(tag)
	# A still at each size, for the eye check: silent headless, saved under a
	# real renderer with --capture=<prefix>.
	if bad.is_empty():
		await _pose("screens_" + tag.replace(" ", "_"))
	return bad


# The audit at the current size: every button's disc and hit circle inside
# the safe area and at least the minimum touch size; no two discs and no two
# hit circles overlapping, except the named shared slots; every label and
# note inside the safe area and clear of every other control; the stick's
# ring and zone clear of the buttons; the first-run hints and the gesture
# plates, when drawn, inside the safe area and clear of the buttons; the HUD
# boxes inside the safe area and clear of each other and the buttons.
# "" is a pass; otherwise the first failure, with the size that failed.
func _audit_layout(tag: String) -> String:
	var touch: Node = _main._touch
	var safe: Rect2 = touch._safe
	var u: float = float(touch._u)
	var buttons: Dictionary = touch._buttons
	var ids: Array = buttons.keys()
	var label_rects := {}
	var note_rects := {}
	for id in ids:
		var b = buttons[id]
		var center: Vector2 = b.center
		var radius := float(b.radius)
		var disc := Rect2(center - Vector2(radius, radius), Vector2(2.0 * radius, 2.0 * radius))
		if not safe.encloses(disc):
			return "%s %s disc %s outside safe %s" % [tag, String(id), str(disc), str(safe)]
		var hit := radius * TouchControls.HIT_SLOP
		var hit_rect := Rect2(center - Vector2(hit, hit), Vector2(2.0 * hit, 2.0 * hit))
		if not safe.encloses(hit_rect):
			return "%s %s hit area %s outside safe %s" % [tag, String(id), str(hit_rect), str(safe)]
		if 2.0 * radius < MIN_TOUCH_DIAMETER:
			return "%s %s diameter %.1f below minimum %.1f" % [tag, String(id), 2.0 * radius, MIN_TOUCH_DIAMETER]
		# A ghosted button draws a ring only: its label is set but never drawn.
		if String(b.label) != "" and int(b.tone) != TouchControls.TONE_GHOST:
			var lines: PackedStringArray = _screens_label_lines(String(b.label), int(roundf(25.0 * u)), radius)
			var rects: Array = []
			for i in lines.size():
				rects.append(_screens_text_rect(UiStyle.font_label(), lines[i], int(roundf(25.0 * u)),
					center + Vector2(0.0, radius + 30.0 * u + float(i) * 28.0 * u)))
			for rect in rects:
				if not safe.encloses(rect):
					return "%s %s label '%s' %s outside safe %s" % [tag, String(id), String(b.label), str(rect), str(safe)]
			label_rects[id] = rects
		if String(b.note) != "":
			var note_rect := _screens_text_rect(UiStyle.font_label(), String(b.note), int(roundf(30.0 * u)),
				center - Vector2(0.0, radius + 14.0 * u))
			if not safe.encloses(note_rect):
				return "%s %s note '%s' %s outside safe %s" % [tag, String(id), String(b.note), str(note_rect), str(safe)]
			note_rects[id] = note_rect
	for a in range(ids.size()):
		for c in range(a + 1, ids.size()):
			var ida: StringName = ids[a]
			var idb: StringName = ids[c]
			if _screens_shared(ida, idb):
				continue
			var ba = buttons[ida]
			var bb = buttons[idb]
			var dist: float = ba.center.distance_to(bb.center)
			if dist < float(ba.radius) + float(bb.radius):
				return "%s discs overlap: %s/%s dist %.1f radii %.1f+%.1f" % [tag, String(ida), String(idb),
					dist, float(ba.radius), float(bb.radius)]
			if dist < (float(ba.radius) + float(bb.radius)) * TouchControls.HIT_SLOP:
				return "%s hit areas overlap: %s/%s dist %.1f hit radii %.1f+%.1f" % [tag, String(ida), String(idb),
					dist, float(ba.radius) * TouchControls.HIT_SLOP, float(bb.radius) * TouchControls.HIT_SLOP]
			for rect in label_rects.get(ida, []):
				if _screens_circle_hits_rect(bb.center, float(bb.radius), rect):
					return "%s label of %s covers %s disc" % [tag, String(ida), String(idb)]
			for rect in label_rects.get(idb, []):
				if _screens_circle_hits_rect(ba.center, float(ba.radius), rect):
					return "%s label of %s covers %s disc" % [tag, String(idb), String(ida)]
			for rect in label_rects.get(ida, []):
				for other in label_rects.get(idb, []):
					if rect.intersects(other):
						return "%s labels overlap: %s/%s" % [tag, String(ida), String(idb)]
			if note_rects.has(ida) and _screens_circle_hits_rect(bb.center, float(bb.radius), note_rects[ida]):
				return "%s note of %s covers %s disc" % [tag, String(ida), String(idb)]
			if note_rects.has(idb) and _screens_circle_hits_rect(ba.center, float(ba.radius), note_rects[idb]):
				return "%s note of %s covers %s disc" % [tag, String(idb), String(ida)]
	# The stick: its ring, and the zone a thumb lands in to move, clear of
	# every button's disc.
	var home: Vector2 = touch.stick_home()
	var throw := TouchControls.STICK_THROW * u
	if not safe.encloses(Rect2(home - Vector2(throw, throw), Vector2(2.0 * throw, 2.0 * throw))):
		return "%s stick ring outside safe %s" % [tag, str(safe)]
	for id in ids:
		var b = buttons[id]
		if home.distance_to(b.center) < throw + float(b.radius):
			return "%s stick ring overlaps %s" % [tag, String(id)]
		if _screens_circle_hits_rect(b.center, float(b.radius), touch._stick_zone):
			return "%s stick zone overlaps %s" % [tag, String(id)]
	# The first run's hints, while they draw: MOVE over the stick, and the
	# look hint on the open glass.
	if float(touch._hint_clock) <= TouchControls.HINT_SECONDS:
		var hint_size := int(roundf(26.0 * u))
		var font := UiStyle.font_label()
		if not bool(touch._moved):
			var move_rect := _screens_text_rect(font, "MOVE", hint_size,
				home + Vector2(0.0, -TouchControls.STICK_THROW * u - 34.0 * u))
			var bad := _screens_hint_rect(tag, "MOVE", move_rect, buttons, ids, safe)
			if not bad.is_empty():
				return bad
		if not bool(touch._looked):
			var at := Vector2(safe.position.x + safe.size.x * 0.72, safe.position.y + safe.size.y * 0.6)
			var look_rect := _screens_text_rect(font, "DRAG TO LOOK", hint_size, at)
			var bad2 := _screens_hint_rect(tag, "DRAG TO LOOK", look_rect, buttons, ids, safe)
			if not bad2.is_empty():
				return bad2
			for id in ids:
				var b2 = buttons[id]
				if (at + Vector2(0.0, -48.0 * u)).distance_to(b2.center) < 40.0 * u + float(b2.radius):
					return "%s look icon overlaps %s" % [tag, String(id)]
	# The swipe hints' plates, when drawn: the same column, the same dodge
	# above any button under it, as _draw_gesture_hints draws them.
	if bool(touch._gestures_live):
		var at2 := Vector2(safe.position.x + safe.size.x * 0.8, safe.position.y + safe.size.y * 0.5)
		var down_y := at2.y + 64.0 * u
		for id in ids:
			var b3 = buttons[id]
			if float(b3.appear) > 0.01 and absf(b3.center.x - at2.x) < float(b3.radius) + 190.0 * u:
				down_y = minf(down_y, b3.center.y - float(b3.radius) - 44.0 * u)
		down_y = maxf(down_y, safe.position.y + 200.0 * u)
		if bool(touch.up_hint_special) and String(touch.up_hint) != "" and \
				not _screens_read_on_a_button(String(touch.up_hint), buttons, ids):
			var bad3 := _screens_plate(tag, String(touch.up_hint), Vector2(at2.x, down_y - 128.0 * u), buttons, ids, safe, u)
			if not bad3.is_empty():
				return bad3
		if bool(touch.down_hint_special) and String(touch.down_hint) != "" and \
				not _screens_read_on_a_button(String(touch.down_hint), buttons, ids):
			var bad4 := _screens_plate(tag, String(touch.down_hint), Vector2(at2.x, down_y), buttons, ids, safe, u)
			if not bad4.is_empty():
				return bad4
	# The HUD boxes: inside the safe area, clear of each other and the buttons.
	var top_left: Control = _main.get_node("HUD/TopLeft")
	var top_right: Control = _main.get_node("HUD/TopRight")
	var left_rect := top_left.get_global_rect()
	var right_rect := top_right.get_global_rect()
	if not safe.encloses(left_rect):
		return "%s HUD TopLeft %s outside safe %s" % [tag, str(left_rect), str(safe)]
	if not safe.encloses(right_rect):
		return "%s HUD TopRight %s outside safe %s" % [tag, str(right_rect), str(safe)]
	if left_rect.intersects(right_rect):
		return "%s HUD TopLeft overlaps TopRight" % tag
	for id in ids:
		var b4 = buttons[id]
		if _screens_circle_hits_rect(b4.center, float(b4.radius), left_rect):
			return "%s HUD TopLeft overlaps %s" % [tag, String(id)]
		if _screens_circle_hits_rect(b4.center, float(b4.radius), right_rect):
			return "%s HUD TopRight overlaps %s" % [tag, String(id)]
	return ""


# A hint's text rect, and a plate's, must sit inside the safe area and clear
# of every button's disc.
func _screens_hint_rect(tag: String, what: String, rect: Rect2, buttons: Dictionary, ids: Array, safe: Rect2) -> String:
	if not safe.encloses(rect):
		return "%s %s %s outside safe %s" % [tag, what, str(rect), str(safe)]
	for id in ids:
		var b = buttons[id]
		if _screens_circle_hits_rect(b.center, float(b.radius), rect):
			return "%s %s covers %s disc" % [tag, what, String(id)]
	return ""


func _screens_plate(tag: String, label: String, center: Vector2, buttons: Dictionary, ids: Array, safe: Rect2, u: float) -> String:
	var size := int(roundf(26.0 * u))
	var width := 2.0 * 18.0 * u + 14.0 * u + UiStyle.text_width(UiStyle.font_label(), label, size) + 40.0 * u
	var rect := Rect2(center - Vector2(width, 56.0 * u) * 0.5, Vector2(width, 56.0 * u))
	return _screens_hint_rect(tag, "hint plate '%s'" % label, rect, buttons, ids, safe)


func _screens_read_on_a_button(label: String, buttons: Dictionary, ids: Array) -> bool:
	for id in ids:
		var b = buttons[id]
		if bool(b.shown) and String(b.label) == label:
			return true
	return false


# The two named shared slots, and only those.
func _screens_shared(a: StringName, b: StringName) -> bool:
	for pair in SHARED_SLOTS:
		if (pair[0] == a and pair[1] == b) or (pair[0] == b and pair[1] == a):
			return true
	return false


# The text's box as text_centered draws it: centred on `center`, height the
# font's ascent plus descent.
func _screens_text_rect(font: Font, value: String, size: int, center: Vector2) -> Rect2:
	var w := UiStyle.text_width(font, value, size)
	var h := font.get_height(size)
	return Rect2(center - Vector2(w, h) * 0.5, Vector2(w, h))


# The label's lines, exactly as touch_controls breaks them: one line while
# it fits the button's own column, else split at the space nearest the middle.
func _screens_label_lines(label: String, size: int, radius: float) -> PackedStringArray:
	var font := UiStyle.font_label()
	var room := 2.4 * radius
	if UiStyle.text_width(font, label, size) <= room or not label.contains(" "):
		return PackedStringArray([label])
	var best := -1
	for i in label.length():
		if label[i] == " " and (best < 0 or absi(i - label.length() / 2) < absi(best - label.length() / 2)):
			best = i
	return PackedStringArray([label.substr(0, best), label.substr(best + 1)])


func _screens_circle_hits_rect(center: Vector2, radius: float, rect: Rect2) -> bool:
	var closest := Vector2(clampf(center.x, rect.position.x, rect.end.x),
		clampf(center.y, rect.position.y, rect.end.y))
	return closest.distance_to(center) < radius
