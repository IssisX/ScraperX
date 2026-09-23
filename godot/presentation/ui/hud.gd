extends Control

# The ordinary HUD is sparse and contextual (GDD section 22): the only
# permanent mark is the reticle dot. Everything else appears because native
# state says something is actionable or dangerous right now, and recedes when
# it is not. Every number drawn is a native read -- no presentation estimate
# is ever shown as if it were simulation (Governing Law 26).

const UiStyle := preload("res://presentation/ui/ui_style.gd")

const ALT_SHOW_SECONDS := 4.0
const ALT_TRIGGER_METERS := 1.0
const TOAST_IN := 0.18
const TOAST_OUT := 0.45
const GAUGE_MIN_FALL_MPS := 4.0
const TONE_NORMAL := 0
const TONE_DANGER := 1

var family := UiStyle.Family.KEYBOARD
var touch_active := false

var _ctx := {}
var _u := 1.0
var _safe := Rect2()
var _clock := 0.0
var _ledge_cue := 0.0
var _hang_cue := 0.0
var _gauge := 0.0
var _prompt_alpha := 0.0
var _prompts: Array[Dictionary] = []
var _alt_alpha := 0.0
var _alt_timer := 0.0
var _alt_anchor := NAN
var _panel_alpha := 0.0
var _panel := {}
var _toasts: Array[Dictionary] = []
var _flash := 0.0
var _flash_color := UiStyle.HAZARD


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	resized.connect(_layout)
	_layout()


func _layout() -> void:
	_u = UiStyle.unit(self)
	_safe = UiStyle.safe_rect(self)
	queue_redraw()


func toast(title: String, sub: String, color: Color, life: float = 2.4) -> void:
	_toasts.append({"title": title, "sub": sub, "color": color, "t": 0.0, "life": life})
	while _toasts.size() > 2:
		_toasts.pop_front()


func flash(color: Color) -> void:
	_flash = 1.0
	_flash_color = color


func show_altimeter(seconds: float) -> void:
	_alt_timer = maxf(_alt_timer, seconds)


func update_hud(ctx: Dictionary, delta: float) -> void:
	_ctx = ctx
	_clock += delta
	var grounded: bool = ctx["grounded"]
	var velocity: Vector3 = ctx["velocity"]
	var hanging: bool = ctx["hanging"]
	_ledge_cue = move_toward(_ledge_cue, 1.0 if (ctx["climb_ok"] or ctx["grab_hint"]) else 0.0, delta * 7.0)
	_hang_cue = move_toward(_hang_cue, 1.0 if hanging else 0.0, delta * 7.0)
	var falling := not grounded and int(ctx["traversal"]) == 0 \
		and (-velocity.y > GAUGE_MIN_FALL_MPS or bool(ctx["chute"]))
	_gauge = move_toward(_gauge, 1.0 if falling else 0.0, delta * 5.0)

	# Altitude surfaces when the committed checkpoint climbs (grounded
	# progress, not every hop) or while falling, then recedes.
	var checkpoint_y: float = (ctx["checkpoint"] as Vector3).y
	if is_nan(_alt_anchor):
		_alt_anchor = checkpoint_y
	if absf(checkpoint_y - _alt_anchor) > ALT_TRIGGER_METERS:
		_alt_anchor = checkpoint_y
		_alt_timer = ALT_SHOW_SECONDS
	if falling:
		_alt_timer = maxf(_alt_timer, 1.2)
	_alt_timer = maxf(0.0, _alt_timer - delta)
	_alt_alpha = move_toward(_alt_alpha, 1.0 if _alt_timer > 0.0 else 0.0,
		delta * (4.0 if _alt_timer > 0.0 else 1.1))

	_prompts = _build_prompts(ctx)
	_prompt_alpha = move_toward(_prompt_alpha, 1.0 if not _prompts.is_empty() else 0.0, delta * 8.0)
	var panel: Dictionary = ctx["panel"]
	if not panel.is_empty():
		_panel = panel
	_panel_alpha = move_toward(_panel_alpha, 1.0 if not panel.is_empty() else 0.0, delta * 6.0)

	for toast_entry in _toasts:
		toast_entry["t"] = float(toast_entry["t"]) + delta
	while not _toasts.is_empty() and float(_toasts[0]["t"]) >= float(_toasts[0]["life"]):
		_toasts.pop_front()
	_flash = maxf(0.0, _flash - delta * 1.5)
	queue_redraw()


func _prompt(verb: StringName, text: String, detail: String = "", tone: int = TONE_NORMAL) -> Dictionary:
	return {"verb": verb, "text": text, "detail": detail, "tone": tone}


# What the player can do right now, in the order a hand would reach for it.
# On touch the buttons themselves carry the verbs, so only what no button
# says -- the grab gesture and a lethal-fall warning -- is repeated here.
func _build_prompts(ctx: Dictionary) -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	var danger := float(ctx["danger"]) >= 0.6 and not bool(ctx["chute"])
	if touch_active:
		if ctx["grab_hint"]:
			out.append(_prompt(&"grab", "PUSH FORWARD TO GRAB"))
		if ctx["chute_ok"] and danger:
			out.append(_prompt(&"chute", "OPEN THE CHUTE", "", TONE_DANGER))
		return out
	if ctx["operating"] != &"":
		out.append(_prompt(&"done", "DONE"))
		return out
	if ctx["hanging"]:
		out.append(_prompt(&"jump", "CLIMB UP"))
		out.append(_prompt(&"drop", "DROP"))
		return out
	if int(ctx["traversal"]) != 0:
		return out
	if ctx["grab_hint"]:
		out.append(_prompt(&"grab", "HOLD FORWARD TO GRAB"))
	if ctx["chute_ok"]:
		out.append(_prompt(&"chute", "STOW CHUTE" if ctx["chute"] else "DEPLOY CHUTE", "",
			TONE_DANGER if danger else TONE_NORMAL))
	var action: Dictionary = ctx["action"]
	match action["id"]:
		&"climb", &"operate":
			out.append(_prompt(&"action", action["label"], action["detail"]))
		&"valve":
			out.append(_prompt(&"valve", action["label"], action["detail"]))
	return out


func _draw() -> void:
	if _ctx.is_empty():
		return
	if _flash > 0.0:
		draw_rect(Rect2(Vector2.ZERO, size), UiStyle.with_alpha(_flash_color, 0.34 * _flash * _flash))
	var center := size * 0.5
	_draw_reticle(center)
	if _gauge > 0.01:
		_draw_gauge(center)
	if _prompt_alpha > 0.01 and not _prompts.is_empty():
		_draw_prompts(center)
	if _alt_alpha > 0.01:
		_draw_altimeter()
	if _panel_alpha > 0.01 and not _panel.is_empty():
		_draw_station_panel()
	if not _toasts.is_empty():
		_draw_toasts()


func _draw_reticle(c: Vector2) -> void:
	var u := _u
	UiStyle.disc(self, c, 5.5 * u, UiStyle.with_alpha(UiStyle.INK, 0.4))
	UiStyle.disc(self, c, 3.2 * u, UiStyle.with_alpha(UiStyle.PAPER, 0.92))
	var arm := 15.0 * u
	var width := 3.0 * u
	# An edge within reach: two corners framing a lip just above the aim.
	if _ledge_cue > 0.01:
		var spread := lerpf(56.0, 30.0, UiStyle.ease_out_back(_ledge_cue)) * u
		var color := UiStyle.with_alpha(UiStyle.AMBER, _ledge_cue)
		for side in [-1.0, 1.0]:
			var corner := c + Vector2(side * spread, -24.0 * u)
			draw_polyline(PackedVector2Array([
				corner + Vector2(-side * arm, 0.0), corner, corner + Vector2(0.0, arm)]),
				color, width, true)
	# Holding: the corners close onto the aim line in the engaged colour.
	if _hang_cue > 0.01:
		var color := UiStyle.with_alpha(UiStyle.SAFE, _hang_cue)
		for side in [-1.0, 1.0]:
			var corner := c + Vector2(side * 24.0 * u, -6.0 * u)
			draw_polyline(PackedVector2Array([
				corner + Vector2(-side * arm, 0.0), corner, corner + Vector2(0.0, arm * 0.9)]),
				color, width * 1.2, true)


func _draw_gauge(c: Vector2) -> void:
	var u := _u
	var alpha := _gauge
	var radius := 64.0 * u
	var start := PI * 0.75
	var sweep := PI * 1.5
	var velocity: Vector3 = _ctx["velocity"]
	var lethal: float = _ctx["lethal"]
	var speed := maxf(0.0, -velocity.y)
	var fraction := clampf(speed / maxf(lethal, 0.001), 0.0, 1.0)
	var chute: bool = _ctx["chute"]
	var color := UiStyle.PAPER
	if chute:
		color = UiStyle.SAFE
	elif fraction >= 0.8:
		color = UiStyle.HAZARD.lerp(UiStyle.PAPER, 0.25 * (0.5 + 0.5 * sin(_clock * 14.0)))
	elif fraction >= 0.55:
		color = UiStyle.AMBER
	draw_arc(c, radius, start, start + sweep, 48, UiStyle.with_alpha(UiStyle.PAPER, 0.16 * alpha),
		5.0 * u, true)
	if fraction > 0.002:
		draw_arc(c, radius, start, start + sweep * fraction, 48, UiStyle.with_alpha(color, alpha),
			7.0 * u, true)
	# The native lethal landing speed sits at the end of the arc.
	var end_dir := Vector2.from_angle(start + sweep)
	draw_line(c + end_dir * (radius - 12.0 * u), c + end_dir * (radius + 12.0 * u),
		UiStyle.with_alpha(UiStyle.HAZARD, alpha), 4.0 * u, true)
	var x := c.x + radius + 24.0 * u
	UiStyle.plate(self, Rect2(Vector2(x - 12.0 * u, c.y - 26.0 * u), Vector2(150.0 * u, 76.0 * u)),
		UiStyle.with_alpha(UiStyle.INK, 0.55 * alpha), UiStyle.with_alpha(color, alpha), 3.0 * u, 10.0 * u)
	UiStyle.text(self, UiStyle.font_digits(), "%4.1f" % speed, Vector2(x, c.y + 12.0 * u),
		int(roundf(36.0 * u)), UiStyle.with_alpha(color, alpha), HORIZONTAL_ALIGNMENT_LEFT,
		int(roundf(6.0 * u)))
	UiStyle.text(self, UiStyle.font_label(), "CANOPY" if chute else "M/S FALL",
		Vector2(x, c.y + 40.0 * u), int(roundf(18.0 * u)),
		UiStyle.with_alpha(UiStyle.PAPER_DIM, alpha), HORIZONTAL_ALIGNMENT_LEFT, int(roundf(5.0 * u)))


func _draw_prompts(c: Vector2) -> void:
	var u := _u
	var alpha := _prompt_alpha
	var h := 46.0 * u
	var pad := 18.0 * u
	var gap := 26.0 * u
	var verb_size := int(roundf(29.0 * u))
	var detail_size := int(roundf(24.0 * u))
	var widths: Array[float] = []
	var total := 0.0
	for p in _prompts:
		var w := pad * 2.0 + UiStyle.binding_width(family, p["verb"], h) + 14.0 * u \
			+ UiStyle.text_width(UiStyle.font_label(), p["text"], verb_size)
		if not String(p["detail"]).is_empty():
			w += 12.0 * u + UiStyle.text_width(UiStyle.font_digits(), p["detail"], detail_size)
		widths.append(w)
		total += w
	total += gap * float(_prompts.size() - 1)
	var x := c.x - total * 0.5
	var y := c.y + 176.0 * u
	var font := UiStyle.font_label()
	var baseline := y + (font.get_ascent(verb_size) - font.get_descent(verb_size)) * 0.5
	for i in _prompts.size():
		var p: Dictionary = _prompts[i]
		var accent := UiStyle.HAZARD if p["tone"] == TONE_DANGER else UiStyle.AMBER
		var rect := Rect2(Vector2(x, y - h * 0.5 - 11.0 * u), Vector2(widths[i], h + 22.0 * u))
		UiStyle.plate(self, rect, UiStyle.with_alpha(UiStyle.INK, 0.64 * alpha),
			UiStyle.with_alpha(accent, alpha), 4.0 * u, 12.0 * u)
		var cursor := x + pad
		cursor += UiStyle.draw_binding(self, family, p["verb"], Vector2(cursor, y), h, alpha)
		cursor += 14.0 * u
		UiStyle.text(self, font, p["text"], Vector2(cursor, baseline), verb_size,
			UiStyle.with_alpha(UiStyle.PAPER, alpha))
		if not String(p["detail"]).is_empty():
			cursor += UiStyle.text_width(font, p["text"], verb_size) + 12.0 * u
			UiStyle.text(self, UiStyle.font_digits(), p["detail"], Vector2(cursor, baseline),
				detail_size, UiStyle.with_alpha(UiStyle.PAPER_DIM, alpha))
		x += widths[i] + gap


func _draw_altimeter() -> void:
	var u := _u
	var alpha := _alt_alpha
	var right := _safe.end.x - 64.0 * u
	var top := _safe.position.y + 56.0 * u
	var outline := int(roundf(6.0 * u))
	var position: Vector3 = _ctx["position"]
	var checkpoint: Vector3 = _ctx["checkpoint"]
	var tower: float = _ctx["tower_height"]
	# A soft backing so the readout holds over snow, sky and sodium glare.
	UiStyle.plate(self, Rect2(Vector2(right - 330.0 * u, top - 8.0 * u), Vector2(356.0 * u, 166.0 * u)),
		UiStyle.with_alpha(UiStyle.INK, 0.42 * alpha), Color(0.0, 0.0, 0.0, 0.0), 0.0, 18.0 * u)
	UiStyle.text(self, UiStyle.font_label(), "ALTITUDE", Vector2(right, top + 24.0 * u),
		int(roundf(22.0 * u)), UiStyle.with_alpha(UiStyle.PAPER_DIM, alpha), HORIZONTAL_ALIGNMENT_RIGHT,
		outline)
	var unit_size := int(roundf(30.0 * u))
	UiStyle.text(self, UiStyle.font_label(), "M", Vector2(right, top + 104.0 * u), unit_size,
		UiStyle.with_alpha(UiStyle.PAPER, alpha), HORIZONTAL_ALIGNMENT_RIGHT, outline)
	var unit_width := UiStyle.text_width(UiStyle.font_label(), "M", unit_size) + 10.0 * u
	UiStyle.text(self, UiStyle.font_digits(), "%+.1f" % position.y, Vector2(right - unit_width, top + 104.0 * u),
		int(roundf(76.0 * u)), UiStyle.with_alpha(UiStyle.PAPER, alpha), HORIZONTAL_ALIGNMENT_RIGHT,
		outline)
	UiStyle.text(self, UiStyle.font_label(), "CHECKPOINT %+.1f M" % checkpoint.y,
		Vector2(right, top + 142.0 * u), int(roundf(21.0 * u)),
		UiStyle.with_alpha(UiStyle.SAFE, 0.92 * alpha), HORIZONTAL_ALIGNMENT_RIGHT, outline)
	# The whole climb on one rail: honest scale, so 24 m reads as the first
	# step of 1 600, not as progress the tower has not granted.
	if tower <= 0.0:
		return
	var rail_x := right - 6.0 * u
	var rail_top := top + 190.0 * u
	var rail_height := 420.0 * u
	draw_line(Vector2(rail_x, rail_top), Vector2(rail_x, rail_top + rail_height),
		UiStyle.with_alpha(UiStyle.PAPER, 0.3 * alpha), 3.0 * u)
	for i in 9:
		var ty := rail_top + rail_height * (1.0 - float(i) / 8.0)
		var tick := (14.0 if i % 4 == 0 else 8.0) * u
		draw_line(Vector2(rail_x - tick, ty), Vector2(rail_x, ty),
			UiStyle.with_alpha(UiStyle.PAPER, 0.35 * alpha), 2.0 * u)
	UiStyle.text(self, UiStyle.font_digits(), "%.0f" % tower, Vector2(rail_x - 22.0 * u, rail_top + 8.0 * u),
		int(roundf(19.0 * u)), UiStyle.with_alpha(UiStyle.PAPER_FAINT, alpha), HORIZONTAL_ALIGNMENT_RIGHT,
		outline)
	var checkpoint_y := rail_top + rail_height * (1.0 - clampf(checkpoint.y / tower, 0.0, 1.0))
	draw_line(Vector2(rail_x - 16.0 * u, checkpoint_y), Vector2(rail_x + 6.0 * u, checkpoint_y),
		UiStyle.with_alpha(UiStyle.SAFE, alpha), 3.0 * u)
	var player_y := rail_top + rail_height * (1.0 - clampf(position.y / tower, 0.0, 1.0))
	UiStyle.triangle(self, Vector2(rail_x - 20.0 * u, player_y), 10.0 * u, Vector2.RIGHT,
		UiStyle.with_alpha(UiStyle.AMBER, alpha))


func _draw_station_panel() -> void:
	var u := _u
	var alpha := _panel_alpha
	var rows: Array = _panel["rows"]
	var verbs: Array = [] if touch_active else _panel["verbs"]
	var width := 620.0 * u
	var height := 118.0 * u + 46.0 * u * float(rows.size()) + 24.0 * u
	if not verbs.is_empty():
		height += 30.0 * u + 56.0 * u * float(verbs.size())
	var origin := Vector2(_safe.position.x + 64.0 * u, _safe.position.y + _safe.size.y * 0.29)
	var rect := Rect2(origin, Vector2(width, height))
	UiStyle.plate(self, rect, UiStyle.with_alpha(UiStyle.INK, 0.8 * alpha),
		UiStyle.with_alpha(UiStyle.AMBER, alpha), 6.0 * u, 22.0 * u)
	UiStyle.hazard_band(self, Rect2(origin + Vector2(6.0 * u, 0.0), Vector2(width - 34.0 * u, 12.0 * u)),
		UiStyle.with_alpha(UiStyle.AMBER, 0.9 * alpha), UiStyle.with_alpha(UiStyle.INK, 0.9 * alpha), 14.0 * u)
	var left := origin.x + 32.0 * u
	var right := origin.x + width - 32.0 * u
	UiStyle.text(self, UiStyle.font_heavy(), _panel["title"], Vector2(left, origin.y + 64.0 * u),
		int(roundf(34.0 * u)), UiStyle.with_alpha(UiStyle.PAPER, alpha))
	UiStyle.text(self, UiStyle.font_label(), _panel["subtitle"], Vector2(left, origin.y + 94.0 * u),
		int(roundf(19.0 * u)), UiStyle.with_alpha(UiStyle.PAPER_DIM, alpha))
	var y := origin.y + 118.0 * u
	for row in rows:
		y += 46.0 * u
		var tone_color := UiStyle.PAPER
		match int(row[2]):
			1:
				tone_color = UiStyle.SAFE
			2:
				tone_color = UiStyle.HAZARD
		UiStyle.text(self, UiStyle.font_label(), row[0], Vector2(left, y), int(roundf(21.0 * u)),
			UiStyle.with_alpha(UiStyle.PAPER_DIM, alpha))
		UiStyle.text(self, UiStyle.font_digits(), row[1], Vector2(right, y), int(roundf(27.0 * u)),
			UiStyle.with_alpha(tone_color, alpha), HORIZONTAL_ALIGNMENT_RIGHT)
	if verbs.is_empty():
		return
	y += 30.0 * u
	draw_line(Vector2(left, y), Vector2(right, y), UiStyle.with_alpha(UiStyle.PAPER, 0.18 * alpha), 2.0 * u)
	var h := 48.0 * u
	var font := UiStyle.font_label()
	var size := int(roundf(25.0 * u))
	for entry in verbs:
		y += 56.0 * u
		var glyph_center := Vector2(left, y - 9.0 * u)
		var used := UiStyle.draw_binding(self, family, entry[0], glyph_center, h, alpha)
		# Pads hoist on the triggers too; the panel shows both ways in.
		if entry[0] == &"hoist" and family not in [UiStyle.Family.KEYBOARD, UiStyle.Family.TOUCH]:
			used += 10.0 * u + UiStyle.draw_binding(self, family, &"hoist_analog",
				glyph_center + Vector2(used + 10.0 * u, 0.0), h, alpha)
		UiStyle.text(self, font, entry[1], Vector2(left + used + 16.0 * u, y), size,
			UiStyle.with_alpha(UiStyle.PAPER, alpha))


func _draw_toasts() -> void:
	var u := _u
	var y := _safe.position.y + 132.0 * u
	var cx := _safe.position.x + _safe.size.x * 0.5
	for entry in _toasts:
		var t: float = entry["t"]
		var life: float = entry["life"]
		var alpha := clampf(t / TOAST_IN, 0.0, 1.0) * clampf((life - t) / TOAST_OUT, 0.0, 1.0)
		if alpha <= 0.01:
			continue
		var slide := (1.0 - clampf(t / TOAST_IN, 0.0, 1.0)) * -22.0 * u
		var title_size := int(roundf(36.0 * u))
		var sub_size := int(roundf(23.0 * u))
		var width := maxf(UiStyle.text_width(UiStyle.font_heavy(), entry["title"], title_size),
			UiStyle.text_width(UiStyle.font_label(), entry["sub"], sub_size)) + 96.0 * u
		var rect := Rect2(Vector2(cx - width * 0.5, y + slide), Vector2(width, 108.0 * u))
		var color: Color = entry["color"]
		UiStyle.plate(self, rect, UiStyle.with_alpha(UiStyle.INK, 0.82 * alpha),
			UiStyle.with_alpha(color, alpha), 6.0 * u, 18.0 * u)
		UiStyle.text(self, UiStyle.font_heavy(), entry["title"], Vector2(cx, rect.position.y + 50.0 * u),
			title_size, UiStyle.with_alpha(color, alpha), HORIZONTAL_ALIGNMENT_CENTER)
		UiStyle.text(self, UiStyle.font_label(), entry["sub"], Vector2(cx, rect.position.y + 86.0 * u),
			sub_size, UiStyle.with_alpha(UiStyle.PAPER, alpha), HORIZONTAL_ALIGNMENT_CENTER)
		y += 128.0 * u
