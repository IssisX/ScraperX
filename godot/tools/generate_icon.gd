extends SceneTree

# Draws the game's launcher icon: a tapering steel lattice tower, cross-
# braced, in the HUD's amber on its ink, with one lamp at the top. No text,
# so it does not depend on the store name (owner decision D1). Run from the
# repo root:
#   tools/godot/godot --headless --path godot --script res://tools/generate_icon.gd
# It writes presentation/brand/: icon.png (512, the project icon),
# launcher_192.png, adaptive_foreground_432.png (transparent, the mark inside
# Android's 66 % safe zone) and adaptive_background_432.png (solid ink).
# Deterministic: the same script draws the same pixels.

const INK := Color("0e0d0c")
const AMBER := Color("e8a23a")
const LAMP := Color("efe7d6")
const OUT := "res://presentation/brand/"
const SUPER := 4


func _init() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(OUT))
	_save(_draw(512, true, 0.0), "icon.png")
	_save(_draw(192, true, 0.0), "launcher_192.png")
	_save(_draw(432, false, 0.17), "adaptive_foreground_432.png")
	var background := Image.create_empty(432, 432, false, Image.FORMAT_RGBA8)
	background.fill(INK)
	_save(background, "adaptive_background_432.png")
	quit(0)


func _save(image: Image, name: String) -> void:
	var error := image.save_png(OUT + name)
	if error != OK:
		push_error("SCRAPERX_ICON_SAVE_FAILED %s %d" % [name, error])
		quit(1)
	print("SCRAPERX_ICON %s %dx%d" % [name, image.get_width(), image.get_height()])


# The mark in unit coordinates (0..1, y down), inset by `inset` on each side.
func _segments() -> Array:
	var lines := []
	var base_y := 0.90
	var top_y := 0.14
	var base_half := 0.20
	var top_half := 0.055
	var leg := func(t: float, side: float) -> Vector2:
		return Vector2(0.5 + side * lerpf(base_half, top_half, t), lerpf(base_y, top_y, t))
	# The legs.
	lines.append([leg.call(0.0, -1.0), leg.call(1.0, -1.0), 0.034])
	lines.append([leg.call(0.0, 1.0), leg.call(1.0, 1.0), 0.034])
	# Girders, closer together as the tower narrows, and an X in each bay.
	var stations := [0.0, 0.2, 0.38, 0.54, 0.68, 0.8, 0.9, 1.0]
	for i in stations.size():
		var t: float = stations[i]
		lines.append([leg.call(t, -1.0), leg.call(t, 1.0), 0.022])
		if i > 0:
			var below: float = stations[i - 1]
			lines.append([leg.call(below, -1.0), leg.call(t, 1.0), 0.013])
			lines.append([leg.call(below, 1.0), leg.call(t, -1.0), 0.013])
	# The mast above the crown.
	lines.append([Vector2(0.5, top_y), Vector2(0.5, 0.07), 0.016])
	return lines


func _draw(size: int, opaque: bool, inset: float) -> Image:
	var big := size * SUPER
	var image := Image.create_empty(big, big, false, Image.FORMAT_RGBA8)
	image.fill(INK if opaque else Color(0.0, 0.0, 0.0, 0.0))
	var scale := float(big) * (1.0 - 2.0 * inset)
	var offset := float(big) * inset
	for line in _segments():
		var a: Vector2 = (line[0] as Vector2) * scale + Vector2(offset, offset)
		var b: Vector2 = (line[1] as Vector2) * scale + Vector2(offset, offset)
		_stroke(image, a, b, float(line[2]) * scale * 0.5, AMBER)
	var lamp := Vector2(0.5, 0.07) * scale + Vector2(offset, offset)
	_disc(image, lamp, 0.032 * scale, LAMP)
	image.resize(size, size, Image.INTERPOLATE_LANCZOS)
	return image


func _stroke(image: Image, a: Vector2, b: Vector2, half_width: float, color: Color) -> void:
	var low := Vector2(minf(a.x, b.x), minf(a.y, b.y)) - Vector2.ONE * (half_width + 1.0)
	var high := Vector2(maxf(a.x, b.x), maxf(a.y, b.y)) + Vector2.ONE * (half_width + 1.0)
	var size := image.get_width()
	for y in range(maxi(0, int(low.y)), mini(size, int(high.y) + 1)):
		for x in range(maxi(0, int(low.x)), mini(size, int(high.x) + 1)):
			var p := Vector2(float(x) + 0.5, float(y) + 0.5)
			var ab := b - a
			var t := clampf((p - a).dot(ab) / maxf(ab.length_squared(), 1e-6), 0.0, 1.0)
			if p.distance_to(a + ab * t) <= half_width:
				image.set_pixel(x, y, color)


func _disc(image: Image, centre: Vector2, radius: float, color: Color) -> void:
	_stroke(image, centre, centre, radius, color)
