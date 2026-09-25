extends RefCounted

# Kellerworks interface language: sodium amber on mill-scale black, tracked
# capitals, verdigris for "safe/engaged", hazard red-orange reserved for real
# danger. One embedded typeface -- Godot's own fallback font, shaped through
# FontVariation -- so the Fold, the desktop and the CI capture all render the
# same glyphs from the engine binary: no font files, no per-device SystemFont
# drift. Every icon and controller glyph is drawn from primitives here, so the
# project still ships zero imported UI assets.

enum Family { KEYBOARD, XBOX, PLAYSTATION, NINTENDO, TOUCH }

const INK := Color("0f0d0b")
const PAPER := Color("efe7d6")
const PAPER_DIM := Color("b9ae9a")
const PAPER_FAINT := Color("7d7466")
const AMBER := Color("e8a23a")
const AMBER_DEEP := Color("9c5f1c")
const HAZARD := Color("e0512f")
const SAFE := Color("7fcfb4")
const SHADOW := Color(0.0, 0.0, 0.0, 0.55)

# Semantic controller positions. Prompts name a position, never a letter, so
# one table serves Xbox, PlayStation and Nintendo pads.
const G_SOUTH := &"south"
const G_EAST := &"east"
const G_WEST := &"west"
const G_NORTH := &"north"
const G_START := &"start"
const G_SELECT := &"select"
const G_LT := &"lt"
const G_RT := &"rt"
const G_DPAD_V := &"dpad_v"
const G_DPAD_H := &"dpad_h"
const G_LSTICK_FWD := &"lstick_fwd"
const G_RSTICK_CLICK := &"rstick_click"

# One binding table for every prompt and the pause menu's controls page:
# verb -> [pad position, keyboard key label, touch icon]. It has to agree
# with input_router.gd's event mapping; the scripted --uitest run drives
# every row of it through the real router to keep the two honest.
const BINDINGS := {
	&"jump": [G_SOUTH, "SPACE", &"jump"],
	&"action": [G_WEST, "E", &"climb"],
	&"drop": [G_EAST, "Q", &"drop"],
	&"chute": [G_NORTH, "F", &"chute"],
	&"crouch": [G_RSTICK_CLICK, "C", &"crouch"],
	&"grab": [G_LSTICK_FWD, "W", &"move"],
	&"pause": [G_START, "ESC", &"pause"],
	&"telemetry": [G_SELECT, "F3", &""],
}

static var _fonts := {}


static func _variation(key: String, embolden: float, glyph_spacing: int, tabular: bool) -> Font:
	if _fonts.has(key):
		return _fonts[key]
	var variation := FontVariation.new()
	variation.base_font = ThemeDB.fallback_font
	variation.variation_embolden = embolden
	variation.spacing_glyph = glyph_spacing
	if tabular:
		# The embedded face carries tnum and zero; digits that keep their
		# width stop a changing readout from shimmering sideways.
		var server := TextServerManager.get_primary_interface()
		variation.opentype_features = {
			server.name_to_tag("tnum"): 1,
			server.name_to_tag("zero"): 1,
		}
	_fonts[key] = variation
	return variation


static func font_label() -> Font:
	return _variation("label", 0.35, 3, false)


static func font_heavy() -> Font:
	return _variation("heavy", 0.9, 2, false)


static func font_digits() -> Font:
	return _variation("digits", 0.45, 1, true)


static func with_alpha(color: Color, alpha: float) -> Color:
	return Color(color.r, color.g, color.b, color.a * alpha)


# Layout unit: 1.0 on the Fold 6 inner panel's 1856-unit short edge, so every
# size below is authored once at the target device's own scale.
static func unit(control: Control) -> float:
	var size := control.get_viewport_rect().size
	return maxf(0.25, minf(size.x, size.y) / 1856.0)


# The canvas-space rectangle free of cutouts and rounded corners. Only mobile
# reports a meaningful display safe area; on desktop the window is the stage.
static func safe_rect(control: Control) -> Rect2:
	var full := Rect2(Vector2.ZERO, control.get_viewport_rect().size)
	if not OS.has_feature("mobile"):
		return full
	var safe := DisplayServer.get_display_safe_area()
	var window := Vector2(DisplayServer.window_get_size())
	if safe.size.x <= 0 or safe.size.y <= 0 or window.x <= 0.0 or window.y <= 0.0:
		return full
	var to_canvas := full.size / window
	var mapped := Rect2(Vector2(safe.position) * to_canvas, Vector2(safe.size) * to_canvas)
	return mapped.intersection(full) if mapped.intersects(full) else full


static func ease_out_back(t: float) -> float:
	var c1 := 1.70158
	var x := clampf(t, 0.0, 1.0) - 1.0
	return 1.0 + (c1 + 1.0) * x * x * x + c1 * x * x


static func text_width(font: Font, value: String, size: int) -> float:
	return font.get_string_size(value, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x


# pos is the baseline anchor; align picks which end of the run sits on pos.x.
static func text(ci: CanvasItem, font: Font, value: String, pos: Vector2, size: int, color: Color,
		align: int = HORIZONTAL_ALIGNMENT_LEFT, outline: int = 0,
		outline_color: Color = SHADOW) -> void:
	if value.is_empty() or color.a <= 0.003:
		return
	var width := text_width(font, value, size)
	var x := pos.x
	if align == HORIZONTAL_ALIGNMENT_CENTER:
		x -= width * 0.5
	elif align == HORIZONTAL_ALIGNMENT_RIGHT:
		x -= width
	var at := Vector2(roundf(x), roundf(pos.y))
	if outline > 0:
		ci.draw_string_outline(font, at, value, HORIZONTAL_ALIGNMENT_LEFT, -1, size, outline,
			with_alpha(outline_color, color.a))
	ci.draw_string(font, at, value, HORIZONTAL_ALIGNMENT_LEFT, -1, size, color)


static func text_centered(ci: CanvasItem, font: Font, value: String, center: Vector2, size: int,
		color: Color, outline: int = 0) -> void:
	var baseline := center.y + (font.get_ascent(size) - font.get_descent(size)) * 0.5
	text(ci, font, value, Vector2(center.x, baseline), size, color, HORIZONTAL_ALIGNMENT_CENTER, outline)


static func ring(ci: CanvasItem, center: Vector2, radius: float, color: Color, width: float) -> void:
	ci.draw_arc(center, radius, 0.0, TAU, maxi(24, int(radius * 0.6)), color, width, true)


static func disc(ci: CanvasItem, center: Vector2, radius: float, color: Color) -> void:
	ci.draw_circle(center, radius, color)


# An industrial label plate: dark body, chamfered top-right/bottom-left
# corners, one accent bar on the leading edge.
static func plate(ci: CanvasItem, rect: Rect2, fill: Color, accent: Color, accent_width: float,
		chamfer: float) -> void:
	var c := minf(chamfer, minf(rect.size.x, rect.size.y) * 0.45)
	var p := rect.position
	var s := rect.size
	var body := PackedVector2Array([
		p, Vector2(p.x + s.x - c, p.y), Vector2(p.x + s.x, p.y + c),
		p + s, Vector2(p.x + c, p.y + s.y), Vector2(p.x, p.y + s.y - c),
	])
	ci.draw_colored_polygon(body, fill)
	if accent_width > 0.0 and accent.a > 0.0:
		ci.draw_colored_polygon(PackedVector2Array([
			p, Vector2(p.x + accent_width, p.y), Vector2(p.x + accent_width, p.y + s.y - c + accent_width),
			Vector2(p.x + minf(c, accent_width), p.y + s.y), Vector2(p.x, p.y + s.y - c),
		]), accent)


static func hazard_band(ci: CanvasItem, rect: Rect2, color_a: Color, color_b: Color,
		stripe: float) -> void:
	ci.draw_rect(rect, color_b)
	var clip := PackedVector2Array([
		rect.position, Vector2(rect.end.x, rect.position.y), rect.end,
		Vector2(rect.position.x, rect.end.y),
	])
	var x := rect.position.x - rect.size.y
	while x < rect.end.x:
		var band := PackedVector2Array([
			Vector2(x, rect.end.y), Vector2(x + stripe, rect.end.y),
			Vector2(x + stripe + rect.size.y, rect.position.y), Vector2(x + rect.size.y, rect.position.y),
		])
		for piece in Geometry2D.intersect_polygons(band, clip):
			if piece.size() >= 3:
				ci.draw_colored_polygon(piece, color_a)
		x += stripe * 2.0


static func triangle(ci: CanvasItem, center: Vector2, radius: float, direction: Vector2,
		color: Color) -> void:
	var d := direction.normalized()
	var n := Vector2(-d.y, d.x)
	ci.draw_colored_polygon(PackedVector2Array([
		center + d * radius,
		center - d * radius * 0.62 + n * radius * 0.86,
		center - d * radius * 0.62 - n * radius * 0.86,
	]), color)


static func chevron(ci: CanvasItem, center: Vector2, half: float, direction: Vector2, color: Color,
		width: float) -> void:
	var d := direction.normalized()
	var n := Vector2(-d.y, d.x)
	ci.draw_polyline(PackedVector2Array([
		center - d * half * 0.5 + n * half, center + d * half * 0.5, center - d * half * 0.5 - n * half,
	]), color, width, true)


# --- vector icons (touch buttons and touch-family prompts) -----------------


static func draw_icon(ci: CanvasItem, icon: StringName, c: Vector2, r: float, color: Color,
		width: float) -> void:
	match icon:
		&"jump":
			chevron(ci, c + Vector2(0.0, -r * 0.18), r * 0.46, Vector2.UP, color, width)
			chevron(ci, c + Vector2(0.0, r * 0.16), r * 0.46, Vector2.UP, with_alpha(color, 0.55), width)
			ci.draw_line(c + Vector2(-r * 0.5, r * 0.56), c + Vector2(r * 0.5, r * 0.56), color, width, true)
		&"crouch":
			# Down under a low beam, onto the floor.
			ci.draw_line(c + Vector2(-r * 0.56, -r * 0.5), c + Vector2(r * 0.56, -r * 0.5),
				with_alpha(color, 0.55), width, true)
			chevron(ci, c + Vector2(0.0, r * 0.02), r * 0.42, Vector2.DOWN, color, width)
			ci.draw_line(c + Vector2(-r * 0.5, r * 0.56), c + Vector2(r * 0.5, r * 0.56), color, width, true)
		&"stand":
			chevron(ci, c + Vector2(0.0, -r * 0.08), r * 0.42, Vector2.UP, color, width)
			ci.draw_line(c + Vector2(-r * 0.5, r * 0.56), c + Vector2(r * 0.5, r * 0.56), color, width, true)
		&"climb":
			# A block and the path over it: up the face, then onto the top.
			ci.draw_rect(Rect2(c + Vector2(-r * 0.05, -r * 0.02), Vector2(r * 0.6, r * 0.58)),
				with_alpha(color, 0.5), false, width)
			ci.draw_polyline(PackedVector2Array([
				c + Vector2(-r * 0.52, r * 0.56), c + Vector2(-r * 0.52, -r * 0.3),
				c + Vector2(r * 0.08, -r * 0.3),
			]), color, width, true)
			triangle(ci, c + Vector2(r * 0.22, -r * 0.3), r * 0.2, Vector2.RIGHT, color)
		&"drop":
			ci.draw_line(c + Vector2(-r * 0.52, -r * 0.5), c + Vector2(r * 0.52, -r * 0.5), color, width, true)
			ci.draw_line(c + Vector2(0.0, -r * 0.3), c + Vector2(0.0, r * 0.32), color, width, true)
			triangle(ci, c + Vector2(0.0, r * 0.42), r * 0.24, Vector2.DOWN, color)
		&"chute":
			ci.draw_arc(c + Vector2(0.0, -r * 0.02), r * 0.6, PI, TAU, 20, color, width, true)
			var knot := c + Vector2(0.0, r * 0.6)
			for fx in [-0.6, -0.2, 0.2, 0.6]:
				ci.draw_line(c + Vector2(r * fx, -r * 0.02), knot, with_alpha(color, 0.8), width * 0.6, true)
			disc(ci, knot, width * 0.9, color)
		&"pick_up":
			# A load on the floor with an arrow lifting out of it.
			ci.draw_rect(Rect2(c + Vector2(-r * 0.36, r * 0.1), Vector2(r * 0.72, r * 0.46)),
				color, false, width)
			ci.draw_line(c + Vector2(0.0, r * 0.02), c + Vector2(0.0, -r * 0.42), color, width, true)
			triangle(ci, c + Vector2(0.0, -r * 0.5), r * 0.2, Vector2.UP, color)
		&"hook":
			# A rope coming down to an open hook.
			ci.draw_line(c + Vector2(0.0, -r * 0.62), c + Vector2(0.0, r * 0.1), color, width, true)
			ci.draw_arc(c + Vector2(-r * 0.2, r * 0.1), r * 0.2, 0.0, PI, 12, color, width, true)
			ci.draw_line(c + Vector2(-r * 0.4, r * 0.1), c + Vector2(-r * 0.4, -r * 0.08), color, width, true)
			disc(ci, c + Vector2(0.0, -r * 0.62), width * 0.9, color)
		&"set_down":
			# A load lowered onto a floor line.
			ci.draw_line(c + Vector2(-r * 0.56, r * 0.6), c + Vector2(r * 0.56, r * 0.6), color, width, true)
			ci.draw_rect(Rect2(c + Vector2(-r * 0.32, r * 0.08), Vector2(r * 0.64, r * 0.42)),
				with_alpha(color, 0.75), false, width)
			ci.draw_line(c + Vector2(0.0, -r * 0.62), c + Vector2(0.0, -r * 0.2), color, width, true)
			triangle(ci, c + Vector2(0.0, -r * 0.12), r * 0.18, Vector2.DOWN, color)
		&"done":
			ci.draw_polyline(PackedVector2Array([
				c + Vector2(-r * 0.42, 0.0), c + Vector2(-r * 0.1, r * 0.32), c + Vector2(r * 0.46, -r * 0.3),
			]), color, width * 1.1, true)
		&"pause":
			ci.draw_rect(Rect2(c + Vector2(-r * 0.32, -r * 0.4), Vector2(r * 0.22, r * 0.8)), color)
			ci.draw_rect(Rect2(c + Vector2(r * 0.1, -r * 0.4), Vector2(r * 0.22, r * 0.8)), color)
		&"up":
			triangle(ci, c + Vector2(0.0, -r * 0.08), r * 0.42, Vector2.UP, color)
		&"down":
			triangle(ci, c + Vector2(0.0, r * 0.08), r * 0.42, Vector2.DOWN, color)
		&"left":
			triangle(ci, c + Vector2(-r * 0.08, 0.0), r * 0.42, Vector2.LEFT, color)
		&"right":
			triangle(ci, c + Vector2(r * 0.08, 0.0), r * 0.42, Vector2.RIGHT, color)
		&"move":
			ring(ci, c, r * 0.55, with_alpha(color, 0.6), width * 0.8)
			disc(ci, c + Vector2(0.0, -r * 0.22), r * 0.22, color)
		&"look":
			ci.draw_arc(c, r * 0.5, -PI * 0.85, -PI * 0.15, 16, color, width, true)
			triangle(ci, c + Vector2(r * 0.46, -r * 0.2), r * 0.16, Vector2(0.4, 1.0), color)


# --- controller / keyboard glyphs ------------------------------------------


static func _face_letter(family: int, glyph: StringName) -> String:
	if family == Family.NINTENDO:
		return {G_SOUTH: "B", G_EAST: "A", G_WEST: "Y", G_NORTH: "X"}.get(glyph, "")
	return {G_SOUTH: "A", G_EAST: "B", G_WEST: "X", G_NORTH: "Y"}.get(glyph, "")


static func _xbox_color(glyph: StringName) -> Color:
	match glyph:
		G_SOUTH:
			return Color("5dbb4a")
		G_EAST:
			return Color("e0463c")
		G_WEST:
			return Color("3f8fe0")
		G_NORTH:
			return Color("f0b92e")
	return PAPER


static func _playstation_color(glyph: StringName) -> Color:
	match glyph:
		G_SOUTH:
			return Color("8fb4e8")
		G_EAST:
			return Color("ef6b61")
		G_WEST:
			return Color("e28fc4")
		G_NORTH:
			return Color("5fcfa6")
	return PAPER


# Keyboard key labels "#V" / "#H" draw an arrow-key pair: the embedded face
# has no arrow characters, so they are drawn, not typed.
static func glyph_width(family: int, glyph: StringName, key_label: String, h: float) -> float:
	if family == Family.KEYBOARD:
		if key_label == "#V" or key_label == "#H":
			return h * 1.25
		var size := int(roundf(h * 0.46))
		return maxf(h, text_width(font_label(), key_label, size) + h * 0.62)
	match glyph:
		G_LT, G_RT, G_START, G_SELECT:
			return h * 1.5
		G_LSTICK_FWD, G_RSTICK_CLICK:
			return h * 1.05
	return h


static func binding_width(family: int, verb: StringName, h: float) -> float:
	var binding: Array = BINDINGS.get(verb, [G_SOUTH, "?", &""])
	if family == Family.TOUCH:
		return h
	return glyph_width(family, binding[0], binding[1], h)


# A verb's glyph in the active device's own language: a coloured face button,
# a PlayStation symbol, a keycap -- or, on touch, the icon on the button.
static func draw_binding(ci: CanvasItem, family: int, verb: StringName, left_center: Vector2,
		h: float, alpha: float = 1.0) -> float:
	var binding: Array = BINDINGS.get(verb, [G_SOUTH, "?", &""])
	if family == Family.TOUCH:
		var c := left_center + Vector2(h * 0.5, 0.0)
		disc(ci, c, h * 0.5, with_alpha(INK, 0.85 * alpha))
		ring(ci, c, h * 0.5 - 1.5, with_alpha(AMBER, alpha), maxf(1.5, h * 0.06))
		draw_icon(ci, binding[2], c, h * 0.4, with_alpha(PAPER, alpha), maxf(1.5, h * 0.07))
		return h
	return draw_glyph(ci, family, binding[0], binding[1], left_center, h, alpha)


# Draws one prompt glyph with its left edge at left_center.x, vertically
# centred on left_center.y; returns the width it used.
static func draw_glyph(ci: CanvasItem, family: int, glyph: StringName, key_label: String,
		left_center: Vector2, h: float, alpha: float = 1.0) -> float:
	var width := glyph_width(family, glyph, key_label, h)
	var c := left_center + Vector2(width * 0.5, 0.0)
	var r := h * 0.5
	var line := maxf(1.5, h * 0.07)
	if family == Family.KEYBOARD:
		var rect := Rect2(Vector2(left_center.x, left_center.y - r), Vector2(width, h))
		ci.draw_rect(rect, with_alpha(INK, 0.82 * alpha))
		ci.draw_rect(rect, with_alpha(PAPER, 0.85 * alpha), false, line)
		ci.draw_line(Vector2(rect.position.x + line, rect.end.y - line * 1.6),
			Vector2(rect.end.x - line, rect.end.y - line * 1.6), with_alpha(PAPER, 0.3 * alpha), line)
		if key_label == "#V":
			triangle(ci, c + Vector2(0.0, -h * 0.2), h * 0.16, Vector2.UP, with_alpha(PAPER, alpha))
			triangle(ci, c + Vector2(0.0, h * 0.17), h * 0.16, Vector2.DOWN, with_alpha(PAPER, alpha))
		elif key_label == "#H":
			triangle(ci, c + Vector2(-h * 0.22, 0.0), h * 0.16, Vector2.LEFT, with_alpha(PAPER, alpha))
			triangle(ci, c + Vector2(h * 0.22, 0.0), h * 0.16, Vector2.RIGHT, with_alpha(PAPER, alpha))
		else:
			text_centered(ci, font_label(), key_label, c + Vector2(0.0, -h * 0.03), int(roundf(h * 0.46)),
				with_alpha(PAPER, alpha))
		return width
	match glyph:
		G_SOUTH, G_EAST, G_WEST, G_NORTH:
			if family == Family.XBOX:
				disc(ci, c, r, with_alpha(_xbox_color(glyph), alpha))
				text_centered(ci, font_heavy(), _face_letter(family, glyph), c, int(roundf(h * 0.58)),
					with_alpha(INK, alpha))
			elif family == Family.PLAYSTATION:
				disc(ci, c, r, with_alpha(INK, 0.9 * alpha))
				ring(ci, c, r - line * 0.5, with_alpha(PAPER, 0.35 * alpha), line)
				var sc := with_alpha(_playstation_color(glyph), alpha)
				var k := r * 0.42
				match glyph:
					G_SOUTH:
						ci.draw_line(c + Vector2(-k, -k), c + Vector2(k, k), sc, line * 1.6, true)
						ci.draw_line(c + Vector2(-k, k), c + Vector2(k, -k), sc, line * 1.6, true)
					G_EAST:
						ring(ci, c, k * 1.05, sc, line * 1.5)
					G_WEST:
						ci.draw_rect(Rect2(c - Vector2(k, k) * 0.9, Vector2(k, k) * 1.8), sc, false, line * 1.5)
					G_NORTH:
						ci.draw_polyline(PackedVector2Array([
							c + Vector2(0.0, -k * 1.05), c + Vector2(k * 1.0, k * 0.72),
							c + Vector2(-k * 1.0, k * 0.72), c + Vector2(0.0, -k * 1.05),
						]), sc, line * 1.5, true)
			else:
				disc(ci, c, r, with_alpha(INK, 0.9 * alpha))
				ring(ci, c, r - line * 0.5, with_alpha(PAPER, 0.7 * alpha), line)
				text_centered(ci, font_heavy(), _face_letter(family, glyph), c, int(roundf(h * 0.56)),
					with_alpha(PAPER, alpha))
		G_LT, G_RT, G_START, G_SELECT:
			var rect := Rect2(Vector2(left_center.x, left_center.y - r * 0.8), Vector2(width, h * 0.8))
			ci.draw_rect(rect, with_alpha(INK, 0.88 * alpha))
			ci.draw_rect(rect, with_alpha(PAPER, 0.75 * alpha), false, line)
			var label := ""
			match glyph:
				G_LT:
					label = "L2" if family == Family.PLAYSTATION else ("ZL" if family == Family.NINTENDO else "LT")
				G_RT:
					label = "R2" if family == Family.PLAYSTATION else ("ZR" if family == Family.NINTENDO else "RT")
				G_START:
					label = "OPT" if family == Family.PLAYSTATION else ("+" if family == Family.NINTENDO else "MENU")
				G_SELECT:
					label = "PAD" if family == Family.PLAYSTATION else ("-" if family == Family.NINTENDO else "VIEW")
			text_centered(ci, font_label(), label, c, int(roundf(h * 0.36)), with_alpha(PAPER, alpha))
		G_DPAD_V, G_DPAD_H:
			# A plus-shaped pad whose live arms are filled solid: at small
			# sizes a lit arm reads where a tiny arrow would not.
			var arm := r * 0.4
			var outline_color := with_alpha(PAPER, 0.8 * alpha)
			var body := with_alpha(INK, 0.92 * alpha)
			var vertical := Rect2(c - Vector2(arm, r), Vector2(arm * 2.0, h))
			var horizontal := Rect2(c - Vector2(r, arm), Vector2(h, arm * 2.0))
			ci.draw_rect(vertical, body)
			ci.draw_rect(horizontal, body)
			var lit := with_alpha(AMBER, alpha)
			if glyph == G_DPAD_V:
				ci.draw_rect(Rect2(vertical.position, Vector2(arm * 2.0, r - arm)), lit)
				ci.draw_rect(Rect2(c + Vector2(-arm, arm), Vector2(arm * 2.0, r - arm)), lit)
			else:
				ci.draw_rect(Rect2(horizontal.position, Vector2(r - arm, arm * 2.0)), lit)
				ci.draw_rect(Rect2(c + Vector2(arm, -arm), Vector2(r - arm, arm * 2.0)), lit)
			ci.draw_rect(vertical, outline_color, false, line * 0.8)
			ci.draw_rect(horizontal, outline_color, false, line * 0.8)
		G_LSTICK_FWD:
			ring(ci, c, r * 0.92, with_alpha(PAPER, 0.8 * alpha), line)
			disc(ci, c + Vector2(0.0, -r * 0.3), r * 0.42, with_alpha(PAPER, 0.9 * alpha))
			triangle(ci, c + Vector2(0.0, -r * 0.3), r * 0.2, Vector2.UP, with_alpha(INK, alpha))
		G_RSTICK_CLICK:
			# The right stick pressed in: a centred cap with a ring pushed around it.
			ring(ci, c, r * 0.92, with_alpha(PAPER, 0.8 * alpha), line)
			disc(ci, c, r * 0.5, with_alpha(PAPER, 0.9 * alpha))
			var click := "R3" if family == Family.PLAYSTATION else ("R" if family == Family.NINTENDO else "RS")
			text_centered(ci, font_label(), click, c, int(roundf(h * 0.3)), with_alpha(INK, alpha))
	return width
