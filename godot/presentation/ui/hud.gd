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
var _precision_cue := 0.0
var _gauge := 0.0
var _prompt_alpha := 0.0
var _prompts: Array[Dictionary] = []
var _alt_alpha := 0.0
var _alt_timer := 0.0
var _alt_anchor := NAN
var _toasts: Array[Dictionary] = []
var _flash := 0.0
# Blood on the lens (blood()): how much, which splash, and how long it has
# been running.
var lens_blood := 0.0
var _blood_seed := 0
var _blood_age := 0.0
var _flash_color := UiStyle.HAZARD
# The climber's line, printed while it is said (empty: none, or captions off).
var caption := ""
# The foreman's line over the radio and who it is from (life/foreman_radio.gd),
# shown whatever the captions setting: no voice is recorded for him yet.
var radio_text := ""
var radio_who := ""


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


# Blood thrown onto the lens by a death or a hard blow (0..1): dark drops
# round the frame's edges, where a splash off the face lands, a few of them
# running down, drying off over a few seconds. A harder one is a new splash.
func blood(amount: float) -> void:
	if amount > lens_blood:
		lens_blood = clampf(amount, 0.0, 1.0)
		_blood_seed += 1
		_blood_age = 0.0


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
	_precision_cue = move_toward(_precision_cue, 1.0 if bool(ctx.get("precision", false)) else 0.0, delta * 9.0)
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

	for toast_entry in _toasts:
		toast_entry["t"] = float(toast_entry["t"]) + delta
	while not _toasts.is_empty() and float(_toasts[0]["t"]) >= float(_toasts[0]["life"]):
		_toasts.pop_front()
	_flash = maxf(0.0, _flash - delta * 1.5)
	lens_blood = maxf(0.0, lens_blood - delta * 0.22)
	_blood_age += delta
	queue_redraw()


func _prompt(verb: StringName, text: String, detail: String = "", tone: int = TONE_NORMAL) -> Dictionary:
	return {"verb": verb, "text": text, "detail": detail, "tone": tone}


# What the player can do right now, in the order a hand would reach for it.
# On touch the buttons themselves carry the verbs, so only what no button
# says -- the grab gesture, a lethal-fall warning, the yard crane's readout --
# is repeated here.
func _build_prompts(ctx: Dictionary) -> Array[Dictionary]:
	var out: Array[Dictionary] = []
	var danger := float(ctx["danger"]) >= 0.6 and not bool(ctx["chute"])
	if touch_active:
		if ctx["grab_hint"]:
			out.append(_prompt(&"grab", "PUSH FORWARD TO GRAB"))
		if ctx["chute_ok"] and danger:
			out.append(_prompt(&"chute", "OPEN THE CHUTE", "", TONE_DANGER))
		# Working the crane: what its hook or the device on it reads (how hard
		# it holds, why LOCK is not offered), beside the button it is about.
		var act: Dictionary = ctx["action"]
		if bool(ctx.get("crane_operating", false)) and String(act["detail"]) != "":
			out.append(_prompt(&"crane" if act["id"] == &"crane" else &"action", act["label"], act["detail"]))
		return out
	if ctx["hanging"]:
		out.append(_prompt(&"jump", "CLIMB UP"))
		out.append(_prompt(&"drop", "DROP"))
		return out
	if ctx.get("swinging", false):
		out.append(_prompt(&"move", "SWING", "FORWARD: NEXT BAR"))
		out.append(_prompt(&"drop", "LET GO"))
		return out
	if ctx.get("climbing", false):
		out.append(_prompt(&"action", "CLIMB UP", "AT THE TOP"))
		out.append(_prompt(&"jump", "JUMP OFF"))
		out.append(_prompt(&"drop", "LET GO"))
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
		&"punch", &"climb", &"pick_up", &"set_down", &"hook", &"unhook", &"slingshot", &"swing", &"lift", \
				&"crane_take", &"crane_device", &"summit_lower", &"summit_unrig":
			out.append(_prompt(&"action", action["label"], action["detail"]))
	# The yard crane's remote in hand: CRANE works it, and lets it go.
	if bool(ctx.get("crane_operating", false)):
		out.append(_prompt(&"crane", "WALK", action["detail"] if action["id"] == &"crane" else ""))
	elif bool(ctx.get("crane_able", false)):
		out.append(_prompt(&"crane", "CRANE", "WORK THE YARD CRANE"))
	if ctx.get("sling_seated", false):
		out.append(_prompt(&"drop", "LEAVE", "OUT OF THE POUCH"))
	elif ctx.get("swing_seated", false) and ctx.get("drop_ok", false):
		out.append(_prompt(&"drop", "LEAVE", "OUT OF THE SEAT"))
	elif ctx.get("drop_ok", false):
		out.append(_prompt(&"drop", "DROP DOWN", "OVER THE EDGE"))
	return out


func _draw() -> void:
	if _ctx.is_empty():
		return
	if int(_ctx.get("ending", 0)) == 4:
		_draw_ending()
		return
	if _flash > 0.0:
		draw_rect(Rect2(Vector2.ZERO, size), UiStyle.with_alpha(_flash_color, 0.34 * _flash * _flash))
	if lens_blood > 0.0:
		_draw_lens_blood()
	var center := size * 0.5
	_draw_reticle(center)
	if _gauge > 0.01:
		_draw_gauge(center)
	if _prompt_alpha > 0.01 and not _prompts.is_empty():
		_draw_prompts(center)
	if _alt_alpha > 0.01:
		_draw_altimeter()
	if _precision_cue > 0.01:
		_draw_precision()
	route_marks = 0
	if not route_points.is_empty():
		_draw_route_hint()
	if not her_marker.is_empty():
		_draw_her_marker()
	if not _toasts.is_empty():
		_draw_toasts()
	if not caption.is_empty():
		_draw_caption()
	if not radio_text.is_empty():
		_draw_radio()


func _draw_lens_blood() -> void:
	# The same splash every frame it lasts, from its own seed: heavy drops
	# near the frame's edges, each a ragged clot with a fan of spatter thrown
	# off it the way it hit, most running down the glass into a bead, wet
	# enough to catch a little light.
	var rng := RandomNumberGenerator.new()
	rng.seed = 9173 + _blood_seed * 7919
	var fade := clampf(lens_blood * 1.4, 0.0, 1.0)
	for i in 9:
		var t := rng.randf_range(0.04, 0.96)
		var inset := rng.randf_range(0.0, 0.14)
		var at := Vector2.ZERO
		match rng.randi() % 4:
			0:
				at = Vector2(t * size.x, inset * size.y)
			1:
				at = Vector2(t * size.x, (1.0 - inset) * size.y)
			2:
				at = Vector2(inset * size.x, t * size.y)
			_:
				at = Vector2((1.0 - inset) * size.x, t * size.y)
		var r := rng.randf_range(12.0, 40.0) * _u
		var colour := Color(rng.randf_range(0.26, 0.38), 0.01, 0.015, 0.9 * fade)
		# The clot: a ragged outline.
		var outline := PackedVector2Array()
		var wobble := rng.randf_range(0.0, TAU)
		for k in 18:
			var a := TAU * float(k) / 18.0
			var reach := r * (0.75 + 0.18 * sin(a * 3.0 + wobble) + rng.randf_range(-0.08, 0.12))
			outline.append(at + Vector2.from_angle(a) * reach)
		draw_colored_polygon(outline, colour)
		# Spatter thrown off the way it hit.
		var away := Vector2.from_angle(rng.randf() * TAU)
		for k in 10:
			var spot := at + away.rotated(rng.randf_range(-0.7, 0.7)) * r * rng.randf_range(1.1, 3.4)
			UiStyle.disc(self, spot, r * rng.randf_range(0.04, 0.16), colour)
		# Down the glass from its low edge: a streak narrowing to a bead.
		if rng.randf() < 0.6:
			var run := minf(_blood_age * 55.0, rng.randf_range(40.0, 160.0)) * _u
			var w := r * rng.randf_range(0.18, 0.32)
			var top := at + Vector2(rng.randf_range(-0.35, 0.35) * r, 0.5 * r)
			var end := top + Vector2(rng.randf_range(-4.0, 4.0) * _u, run)
			draw_colored_polygon(PackedVector2Array([top + Vector2(-w, 0.0), top + Vector2(w, 0.0),
				end + Vector2(w * 0.4, 0.0), end + Vector2(-w * 0.4, 0.0)]), colour)
			UiStyle.disc(self, end, w * 0.7, colour)
		# Wet: a glint on it.
		UiStyle.disc(self, at + Vector2(-0.3, -0.35) * r, r * 0.1, Color(1.0, 0.75, 0.75, 0.22 * fade))


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
	# Danger in words and weight as well as colour: the arc thickens and the
	# label names what the landing would be.
	var word := gauge_word()
	if fraction > 0.002:
		draw_arc(c, radius, start, start + sweep * fraction, 48, UiStyle.with_alpha(color, alpha),
			(12.0 if word == "LETHAL" else 7.0) * u, true)
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
	UiStyle.text(self, UiStyle.font_label(), word, Vector2(x, c.y + 40.0 * u), int(roundf(18.0 * u)),
		UiStyle.with_alpha(UiStyle.PAPER_DIM if word == "M/S FALL" else color, alpha), HORIZONTAL_ALIGNMENT_LEFT,
		int(roundf(5.0 * u)))


# The fall gauge's label: what landing now would be, at the same thresholds
# as its colours (0.55 and 0.8 of the native lethal speed).
func gauge_word() -> String:
	if _ctx.is_empty():
		return "M/S FALL"
	if bool(_ctx["chute"]):
		return "CANOPY"
	var fraction := maxf(0.0, -(_ctx["velocity"] as Vector3).y) / maxf(float(_ctx["lethal"]), 0.001)
	if fraction >= 0.8:
		return "LETHAL"
	if fraction >= 0.55:
		return "HARD LANDING"
	return "M/S FALL"


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
	var tower: float = _ctx["route_top"]
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
	# The whole climb on one rail: honest scale, from grade to the top of the
	# route as built, so the rail's top is where the climb ends.
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


# Low and centred, above the touch controls, on a plate for any background.
func _draw_caption() -> void:
	var u := _u
	var font_size := int(roundf(32.0 * u))
	var cx := _safe.position.x + _safe.size.x * 0.5
	var width := UiStyle.text_width(UiStyle.font_label(), caption, font_size) + 48.0 * u
	var rect := Rect2(Vector2(cx - width * 0.5, _safe.end.y - 320.0 * u), Vector2(width, 60.0 * u))
	UiStyle.plate(self, rect, UiStyle.with_alpha(UiStyle.INK, 0.78), UiStyle.with_alpha(UiStyle.PAPER, 0.0),
		0.0, 10.0 * u)
	UiStyle.text(self, UiStyle.font_label(), caption, Vector2(cx, rect.position.y + 41.0 * u), font_size,
		UiStyle.PAPER, HORIZONTAL_ALIGNMENT_CENTER)



# The foreman's radio: above the caption's place, a plate with who it is
# from in small heavy type over his line.
func _draw_radio() -> void:
	var u := _u
	var who_size := int(roundf(20.0 * u))
	var line_size := int(roundf(32.0 * u))
	var cx := _safe.position.x + _safe.size.x * 0.5
	var width := maxf(UiStyle.text_width(UiStyle.font_heavy(), radio_who, who_size),
		UiStyle.text_width(UiStyle.font_label(), radio_text, line_size)) + 56.0 * u
	# Keep radio text above the touch action controls, including long final lines.
	var top := _safe.end.y - (690.0 if touch_active else 430.0) * u
	var rect := Rect2(Vector2(cx - width * 0.5, top), Vector2(width, 96.0 * u))
	UiStyle.plate(self, rect, UiStyle.with_alpha(UiStyle.INK, 0.82), UiStyle.with_alpha(UiStyle.HAZARD, 0.9),
		3.0 * u, 10.0 * u)
	UiStyle.text(self, UiStyle.font_heavy(), radio_who, Vector2(cx, rect.position.y + 30.0 * u), who_size,
		UiStyle.HAZARD, HORIZONTAL_ALIGNMENT_CENTER)
	UiStyle.text(self, UiStyle.font_label(), radio_text, Vector2(cx, rect.position.y + 74.0 * u), line_size,
		UiStyle.PAPER, HORIZONTAL_ALIGNMENT_CENTER)


# The faint route hint (a setting): what the hands could take ahead, each a
# short, faint red stroke along the lip or hold, smaller the farther it is.
# Counted as drawn for the tests.
var route_points := PackedVector3Array()
var route_marks := 0

# Where she is, while she is out: "bearing" off his view (radians, 0 ahead,
# positive to his right), "dist" across the ground and "rise" above him, in
# meters. Empty while she is in her locker or dead.
var her_marker := {}

func _draw_her_marker() -> void:
	var u := _u
	var outline := int(roundf(5.0 * u))
	var c := Vector2(size.x * 0.5, _safe.position.y + 96.0 * u)
	var bearing := float(her_marker["bearing"])
	var way := Vector2(sin(bearing), -cos(bearing))
	# A soft backing so the marker holds over snow, sky and sodium glare.
	UiStyle.plate(self, Rect2(c - Vector2(230.0 * u, 56.0 * u), Vector2(460.0 * u, 160.0 * u)),
		UiStyle.with_alpha(UiStyle.INK, 0.42), Color(0.0, 0.0, 0.0, 0.0), 0.0, 18.0 * u)
	draw_arc(c, 40.0 * u, 0.0, TAU, 48, UiStyle.with_alpha(UiStyle.PAPER, 0.55), 3.0 * u, true)
	UiStyle.triangle(self, c + way * 40.0 * u, 20.0 * u, way, UiStyle.HAZARD)
	var rise := float(her_marker["rise"])
	var height := ""
	if rise >= 2.0:
		height = "  %d M ABOVE" % int(roundf(rise))
	elif rise <= -2.0:
		height = "  %d M BELOW" % int(roundf(-rise))
	UiStyle.text(self, UiStyle.font_label(), "HER %d M%s" % [int(roundf(float(her_marker["dist"]))), height],
		c + Vector2(0.0, 88.0 * u), int(roundf(32.0 * u)), UiStyle.HAZARD, HORIZONTAL_ALIGNMENT_CENTER, outline)

func _draw_route_hint() -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	var u := _u
	# A soft red glow under a brighter core: faint, but legible over the dark
	# steel of a rung or a lip.
	var glow := UiStyle.with_alpha(UiStyle.HAZARD, 0.18)
	var core := UiStyle.with_alpha(UiStyle.HAZARD.lightened(0.2), 0.62)
	for point in route_points:
		if camera.is_position_behind(point):
			continue
		var at := camera.unproject_position(point)
		var far := maxf(1.0, camera.global_position.distance_to(point))
		var half := clampf(90.0 / far, 14.0, 40.0) * u
		draw_line(at - Vector2(half, 0.0), at + Vector2(half, 0.0), glow, 11.0 * u, true)
		draw_line(at - Vector2(half, 0.0), at + Vector2(half, 0.0), core, 3.0 * u, true)
		route_marks += 1


# The precision jump's sighted landing: a small ring laid on the surface the
# eyes are on, where the feet will come down.
func _draw_precision() -> void:
	var camera := get_viewport().get_camera_3d()
	if camera == null:
		return
	var point: Vector3 = _ctx.get("precision_point", Vector3.ZERO)
	if camera.is_position_behind(point):
		return
	var at := camera.unproject_position(point)
	var u := _u
	var color := UiStyle.with_alpha(UiStyle.AMBER, 0.85 * _precision_cue)
	var radius := lerpf(22.0, 14.0, UiStyle.ease_out_back(_precision_cue)) * u
	UiStyle.ring(self, at, radius, color, 2.5 * u)
	UiStyle.disc(self, at, 2.5 * u, color)


# The card is reached only after the native observes the unrigged beam and
# an uninterrupted canopy flight to ground. It leaves the world running.
func _draw_ending() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.025, 0.035, 0.045, 0.90))
	var c := size * 0.5
	UiStyle.text(self, UiStyle.font_heavy(), "SCRAPERX", c + Vector2(0, -80 * _u),
		roundi(62 * _u), UiStyle.PAPER, HORIZONTAL_ALIGNMENT_CENTER)
	UiStyle.text(self, UiStyle.font_label(), "THE LAST BEAM IS HOME. SO ARE YOU.", c,
		roundi(26 * _u), UiStyle.PAPER, HORIZONTAL_ALIGNMENT_CENTER)
	UiStyle.text(self, UiStyle.font_label(), "A game by Cory", c + Vector2(0, 90 * _u),
		roundi(22 * _u), UiStyle.AMBER, HORIZONTAL_ALIGNMENT_CENTER)
