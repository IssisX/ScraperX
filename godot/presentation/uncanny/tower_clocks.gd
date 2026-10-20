extends Node3D
## The tower's clocks keep the player's real time (HER_CHART.md, "Your
## time"): the phone's own local clock, read once a second, on a few plain
## wall clocks the climb passes. If it is the middle of the player's night,
## so is the tower's.
##
## Each clock hangs where a look ray meets the structure (the bridge's
## get_look_target, from where the clock is seen toward what it hangs on), so
## it is always on something that stands; a mount that meets nothing, or a
## moving body, is left without a clock. Drawn only: never native bodies,
## outside the exported dressing.

# [from, toward, reach]: where the clock is seen from, and the way to its wall.
const MOUNTS := [
	# The yard's first lamp mast, east face, seen from the game's start.
	[Vector3(6.0, 3.1, -24.0), Vector3(-1.0, 0.0, 0.0), 30.0],
	# Her locker on the 154 m deck: its east side, the strain gauge on the
	# other.
	[Vector3(16.0, 155.85, -126.2), Vector3(-1.0, 0.0, 0.0), 8.0],
	# The 220 ring's north wall, by where a rider steps off C's platform.
	[Vector3(4.0, 222.55, -129.2), Vector3(0.38, 0.0, 0.92), 6.0],
	# TP-340, by F's hole.
	[Vector3(12.0, 342.55, -136.8), Vector3(-0.38, 0.0, -0.92), 10.0],
	# TP-640, by L's hole.
	[Vector3(-3.0, 642.55, -156.5), Vector3(0.0, 0.0, -1.0), 8.0],
	# The 750 deck, by stage O's cab.
	[Vector3(8.0, 752.4, -145.0), Vector3(-0.92, 0.0, -0.38), 8.0],
]
const RADIUS := 0.17
const DRAW_RANGE_M := 70.0
const SECOND_TICK_MS := 1000

# Each clock: {node, hour, minute, second} (the hands' pivots).
var clocks: Array = []
var _shown_second := -1
var _dial: ImageTexture


func setup(native: Object) -> void:
	name = "TowerClocks"
	_dial = _dial_texture(160)
	for mount in MOUNTS:
		var hit: Dictionary = native.get_look_target(mount[0], mount[1], mount[2])
		if not bool(hit.get("hit", false)) or not _stands(int(hit["entity"])):
			continue
		var normal: Vector3 = hit["normal"]
		if absf(normal.y) > 0.2:
			continue
		_build_clock(hit["point"], normal)
	update_clocks(true)


# What a clock may hang on: the world's static structure, the kit's static
# band structure, her locker. Never a body that moves.
static func _stands(entity: int) -> bool:
	return entity in [1, 11, 51, 3201] or (entity >= 1000 and entity < 2000)


func _build_clock(at: Vector3, normal: Vector3) -> void:
	var clock := Node3D.new()
	clock.name = "TowerClock%d" % clocks.size()
	add_child(clock)
	# Local +z out of the wall, toward whoever reads it.
	clock.basis = Basis.looking_at(-normal, Vector3.UP)
	clock.position = at + normal * 0.03
	var body_material := StandardMaterial3D.new()
	body_material.albedo_color = Color("2b2b2a")
	body_material.metallic = 0.6
	body_material.roughness = 0.45
	var rim := MeshInstance3D.new()
	var case_mesh := CylinderMesh.new()
	case_mesh.top_radius = RADIUS + 0.012
	case_mesh.bottom_radius = RADIUS + 0.012
	case_mesh.height = 0.05
	case_mesh.radial_segments = 24
	case_mesh.rings = 1
	rim.mesh = case_mesh
	rim.material_override = body_material
	rim.rotation.x = PI * 0.5
	rim.visibility_range_end = DRAW_RANGE_M
	clock.add_child(rim)
	var face := MeshInstance3D.new()
	var quad := QuadMesh.new()
	quad.size = Vector2(RADIUS * 2.0, RADIUS * 2.0)
	face.mesh = quad
	var face_material := StandardMaterial3D.new()
	face_material.albedo_texture = _dial
	face_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	face_material.alpha_scissor_threshold = 0.5
	face_material.roughness = 0.6
	face.material_override = face_material
	face.position.z = 0.026
	face.visibility_range_end = DRAW_RANGE_M
	clock.add_child(face)
	var black := StandardMaterial3D.new()
	black.albedo_color = Color("141414")
	black.roughness = 0.5
	var red := StandardMaterial3D.new()
	red.albedo_color = Color("a81c14")
	red.roughness = 0.5
	clocks.append({
		"node": clock,
		"hour": _hand(clock, Vector3(0.016, RADIUS * 0.55, 0.004), black, 0.030),
		"minute": _hand(clock, Vector3(0.011, RADIUS * 0.82, 0.004), black, 0.034),
		"second": _hand(clock, Vector3(0.004, RADIUS * 0.86, 0.003), red, 0.038),
	})


# A hand on its pivot at the dial's center, pointing to twelve at angle 0.
func _hand(clock: Node3D, size: Vector3, material: Material, depth: float) -> Node3D:
	var pivot := Node3D.new()
	pivot.position.z = depth
	clock.add_child(pivot)
	var hand := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = size
	hand.mesh = box
	hand.material_override = material
	# From a short tail behind the center to its tip.
	hand.position.y = size.y * 0.5 - size.y * 0.12
	hand.visibility_range_end = DRAW_RANGE_M
	pivot.add_child(hand)
	return pivot


func _process(_delta: float) -> void:
	update_clocks()


# The phone's local time on every clock, once a second (a quartz clock's
# tick).
func update_clocks(force := false) -> void:
	var second := Time.get_ticks_msec() / SECOND_TICK_MS
	if second == _shown_second and not force:
		return
	_shown_second = second
	var now := Time.get_time_dict_from_system()
	var h := float(int(now["hour"]) % 12)
	var m := float(now["minute"])
	var s := float(now["second"])
	for clock in clocks:
		# Clockwise as read: a negative turn about the face's +z.
		(clock["hour"] as Node3D).rotation.z = -TAU * (h + m / 60.0) / 12.0
		(clock["minute"] as Node3D).rotation.z = -TAU * (m + s / 60.0) / 60.0
		(clock["second"] as Node3D).rotation.z = -TAU * s / 60.0


# The time the first clock shows, read back off its hands (for the tests):
# {hour (0..11), minute}.
func shown_time() -> Dictionary:
	if clocks.is_empty():
		return {}
	var hour_turn := fposmod(-(clocks[0]["hour"] as Node3D).rotation.z, TAU) / TAU
	var minute_turn := fposmod(-(clocks[0]["minute"] as Node3D).rotation.z, TAU) / TAU
	return {"hour": int(floorf(hour_turn * 12.0 + 1.0e-4)) % 12, "minute": int(floorf(minute_turn * 60.0 + 1.0e-4)) % 60}


# A plain dial: white enamel, a bar at each hour and a tick at each minute,
# round (alpha outside the rim).
static func _dial_texture(size: int) -> ImageTexture:
	var image := Image.create_empty(size, size, false, Image.FORMAT_RGBA8)
	var r := float(size) * 0.5
	var enamel := Color("ece9e1")
	var ink := Color("161616")
	for y in size:
		for x in size:
			var d := Vector2(float(x) + 0.5 - r, float(y) + 0.5 - r)
			var rho := d.length() / r
			if rho > 1.0:
				image.set_pixel(x, y, Color(0.0, 0.0, 0.0, 0.0))
				continue
			var color := enamel
			if rho > 0.97:
				color = ink
			else:
				# The angle from twelve, clockwise, in minutes.
				var minutes := fposmod(atan2(d.x, -d.y) / TAU * 60.0, 60.0)
				var off := absf(minutes - roundf(minutes))
				var hour_mark := int(roundf(minutes)) % 5 == 0
				if hour_mark and rho > 0.72 and rho < 0.92 and off < 0.55:
					color = ink
				elif rho > 0.84 and rho < 0.92 and off < 0.16:
					color = ink
			image.set_pixel(x, y, color)
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)
