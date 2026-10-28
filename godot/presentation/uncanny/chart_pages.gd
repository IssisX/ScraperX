extends Node3D
## Her chart's pages in the tower (HER_CHART.md). The first is on a clipboard
## hung inside her locker, on whatever deck her home is: a printed observation chart,
## blank until she writes on it, seen once the locker stands open. Later
## pages are left on the decks the route crosses, above where the player is,
## each weighted by a nut against the wind. Paper and board only: drawn,
## never native bodies, outside the exported dressing (solid_export.gd walks
## $TowerPresentation only). The hand is Cedarville Cursive (SIL Open Font
## License 1.1, third_party/README.md), read at runtime like the voice lines.
##
## A page is never put where the player can see it appear: a page on a deck
## goes down only far from the player and out of the view, and writing on the
## locker's clipboard waits until the locker is shut or the clipboard is out of
## sight.

const MainWorld := preload("res://presentation/main_world.gd")
const FONT_PATH := "res://presentation/uncanny/Cedarville-Cursive.ttf"
const LOCKER_SPOT := "locker"
# Her locker (matron_view.gd): the clipboard on its back wall, facing out
# through the door.
const LOCKER_SIZE := Vector3(1.2, 2.2, 0.9)
# Deck 1 until the native says she is still sealed somewhere else.
var locker_centre := Vector3(11.0, 12.1, -126.2)
var clipboard_at := Vector3(11.0, 12.35, -125.84)
# Where the route puts a rider down (the native's spawns, simulation.hpp's
# InitialSpawn, physics_world_support.hpp's spawn_position), each a place the
# climb passes. A page is left on the floor the native reports near one,
# clear of every drop (_clear_floor).
const ROUTE_SPOTS := [
	["deck22", Vector3(10.0, 23.2, -125.5)], ["deck44", Vector3(11.0, 45.2, -125.5)],
	["deck66", Vector3(10.0, 67.2, -125.5)], ["deck88", Vector3(10.0, 89.2, -170.0)],
	["deck132", Vector3(-8.0, 133.2, -170.0)], ["ring176", Vector3(9.5, 177.2, -127.9)],
	["ring198", Vector3(11.0, 199.2, -127.5)], ["ring220", Vector3(4.0, 221.2, -129.2)],
	["ring242", Vector3(-15.0, 243.15, -129.3)], ["ring264", Vector3(-19.5, 265.15, -164.5)],
	["plate340", Vector3(12.0, 341.2, -136.8)], ["ring374", Vector3(-14.5, 375.2, -145.6)],
	["ring484", Vector3(-3.0, 485.2, -139.6)], ["plate640", Vector3(-3.0, 641.2, -156.5)],
	["deck662", Vector3(6.2, 663.05, -146.0)], ["deck684", Vector3(-3.2, 685.05, -155.0)],
	["deck706", Vector3(-3.5, 707.05, -147.5)], ["deck728", Vector3(9.0, 729.05, -144.0)],
	["deck750", Vector3(8.0, 751.0, -145.0)],
]
# A page lies only where no drop is within CLEAR_M of it; it is looked for
# a step or two off the spot first, then on the spot.
const CLEAR_M := 1.0
const DROP_M := 3.0
const NEAR_OFFSETS := [Vector2(1.2, 0.8), Vector2(-1.2, 0.8), Vector2(1.2, -0.8), Vector2(-1.2, -0.8),
	Vector2(2.0, 0.0), Vector2(-2.0, 0.0), Vector2(0.0, 1.6), Vector2(0.0, -1.6), Vector2.ZERO]
# A page goes ahead of the player: on a level at least this far above, and
# no farther than this.
const AHEAD_MIN_M := 6.0
const AHEAD_MAX_M := 160.0
# Never put down within this of the player, nor inside this angle of the view.
const PLACE_MIN_M := 20.0
const UNSEEN_RAD := 1.1
# Writing on the clipboard in sight: only from this far.
const LOCKER_WRITE_MIN_M := 12.0
const DRAW_RANGE_M := 45.0
const PAPER := Vector2(0.216, 0.279)
const BOARD := Vector3(0.235, 0.33, 0.008)
# The hand: ballpoint, blue-black, a page's worth of notes on the sheet (an
# em of 13 mm); read up close through page_reader.gd.
const INK := Color("1f2a4d")
const HAND_FONT_SIZE := 64
const HAND_PIXEL := 0.00021
const MARGIN := 0.018

var font: FontFile
var clipboard: Node3D
var clipboard_text: Label3D
# spot -> the page node there (the locker's is the clipboard).
var placed := {}
var _native: Object
var _paper_material: StandardMaterial3D
var _board_material: StandardMaterial3D
var _steel_material: StandardMaterial3D


func setup(native: Object) -> void:
	name = "ChartPages"
	_native = native
	font = _load_font()
	_paper_material = StandardMaterial3D.new()
	_paper_material.albedo_texture = _paper_texture()
	_paper_material.roughness = 0.92
	_board_material = StandardMaterial3D.new()
	_board_material.albedo_color = Color("6a4b31")
	_board_material.roughness = 0.8
	_steel_material = StandardMaterial3D.new()
	_steel_material.albedo_color = Color("8a8c8e")
	_steel_material.metallic = 0.85
	_steel_material.roughness = 0.35
	_build_clipboard()


static func _load_font() -> FontFile:
	var bytes := FileAccess.get_file_as_bytes(FONT_PATH)
	if bytes.is_empty():
		push_warning("her chart: cannot read %s; the pages fall back to the default font" % FONT_PATH)
		return null
	var loaded := FontFile.new()
	loaded.data = bytes
	return loaded


# The clipboard on the locker's back wall: board, clip, the printed chart and
# the hand (empty until the first page).
func _build_clipboard() -> void:
	clipboard = Node3D.new()
	clipboard.name = "LockerClipboard"
	add_child(clipboard)
	_sync_locker_home()
	clipboard.position = clipboard_at
	# Its face toward the door (-z), so the page is read from in front of
	# the locker.
	clipboard.rotation.y = PI
	var board := _mesh(BoxMesh.new(), _board_material, clipboard)
	(board.mesh as BoxMesh).size = BOARD
	var clip := _mesh(BoxMesh.new(), _steel_material, clipboard)
	(clip.mesh as BoxMesh).size = Vector3(0.11, 0.035, 0.022)
	clip.position = Vector3(0.0, BOARD.y * 0.5 - 0.03, 0.012)
	var sheet := _sheet(clipboard)
	sheet.position = Vector3(0.0, -0.02, BOARD.z * 0.5 + 0.0015)
	clipboard_text = _hand(sheet)
	placed[LOCKER_SPOT] = clipboard


# A sheet of the printed chart, its hand above it: a quad facing +z.
func _sheet(parent: Node3D) -> Node3D:
	var sheet := Node3D.new()
	sheet.name = "ChartSheet"
	parent.add_child(sheet)
	var quad := QuadMesh.new()
	quad.size = PAPER
	var paper := _mesh(quad, _paper_material, sheet)
	paper.name = "Paper"
	var printed := Label3D.new()
	printed.name = "Printed"
	printed.text = "OBSERVATION CHART"
	printed.font_size = 40
	printed.pixel_size = 0.00028
	printed.modulate = Color("3b3a3a")
	printed.outline_size = 0
	printed.shaded = true
	printed.double_sided = false
	printed.position = Vector3(0.0, PAPER.y * 0.5 - 0.016, 0.0006)
	printed.visibility_range_end = DRAW_RANGE_M
	sheet.add_child(printed)
	return sheet


func _hand(sheet: Node3D) -> Label3D:
	var hand := Label3D.new()
	hand.name = "Hand"
	if font != null:
		hand.font = font
	hand.font_size = HAND_FONT_SIZE
	hand.pixel_size = HAND_PIXEL
	hand.modulate = INK
	hand.outline_size = 0
	hand.shaded = true
	hand.double_sided = false
	hand.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	# Left-aligned text starts at the label's origin: just inside the red
	# margin, under the printed rule.
	hand.width = (PAPER.x - 0.03 - MARGIN) / HAND_PIXEL
	hand.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	hand.vertical_alignment = VERTICAL_ALIGNMENT_TOP
	hand.line_spacing = -4.0
	hand.position = Vector3(-PAPER.x * 0.5 + 0.03, PAPER.y * 0.5 - 0.034, 0.0008)
	hand.visibility_range_end = DRAW_RANGE_M
	sheet.add_child(hand)
	return hand


func _mesh(mesh: Mesh, material: Material, parent: Node3D) -> MeshInstance3D:
	var node := MeshInstance3D.new()
	node.mesh = mesh
	node.material_override = material
	node.visibility_range_end = DRAW_RANGE_M
	node.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(node)
	return node


# The printed form: off-white stock, a header band, faint blue rules and a
# red margin, drawn once.
static func _paper_texture() -> ImageTexture:
	var w := 216
	var h := 279
	var image := Image.create_empty(w, h, false, Image.FORMAT_RGB8)
	image.fill(Color("e8e1cd"))
	image.fill_rect(Rect2i(0, 0, w, 3), Color("c9c0aa"))
	image.fill_rect(Rect2i(8, 22, w - 16, 1), Color("4a4743"))
	var y := 36
	while y < h - 8:
		image.fill_rect(Rect2i(8, y, w - 16, 1), Color("b8c3d4"))
		y += 13
	image.fill_rect(Rect2i(26, 23, 1, h - 31), Color("d49a92"))
	return ImageTexture.create_from_image(image)


# --- placing -------------------------------------------------------------------


# Same rule as matron_view.gd: sealed, position is the step-out centre
# (home floor + 1.05). Clipboard sits where it did relative to the cabinet.
func _sync_locker_home() -> void:
	if _native == null or not _native.has_method("get_matron_state"):
		return
	var data: Dictionary = _native.get_matron_state()
	var state := int(data.get("state", -1))
	if state != 0 and state != 1:
		return
	var p: Vector3 = data.get("position", Vector3.ZERO)
	if p.y < 1.0:
		return
	locker_centre = Vector3(11.0, p.y - 1.05 + LOCKER_SIZE.y * 0.5, -126.2)
	clipboard_at = locker_centre + Vector3(0.0, 0.25, 0.36)
	if clipboard != null:
		clipboard.position = clipboard_at


# The spot for the next page: the locker's for the first she ever writes;
# after that, the lowest route spot ahead of the player, not used yet.
func spot_for(page_index: int, feet_y: float, used: Array) -> String:
	if page_index == 0:
		return LOCKER_SPOT
	for spot in ROUTE_SPOTS:
		var y: float = (spot[1] as Vector3).y
		if String(spot[0]) in used or placed.has(String(spot[0])):
			continue
		if y >= feet_y + AHEAD_MIN_M and y <= feet_y + AHEAD_MAX_M:
			return String(spot[0])
	return ""


# Whether a page may appear at `spot` now: never in sight, never close.
func can_place(spot: String, eye: Vector3, forward: Vector3, locker_shut: bool) -> bool:
	_sync_locker_home()
	if spot == LOCKER_SPOT:
		if locker_shut:
			return true
		var to_board := clipboard_at - eye
		return to_board.length() >= LOCKER_WRITE_MIN_M or forward.angle_to(to_board) > UNSEEN_RAD
	var at := spot_point(spot)
	if at == Vector3.INF:
		return false
	var to := at - eye
	return to.length() >= PLACE_MIN_M and forward.angle_to(to) > UNSEEN_RAD


func spot_point(spot: String) -> Vector3:
	_sync_locker_home()
	if spot == LOCKER_SPOT:
		return clipboard_at
	for entry in ROUTE_SPOTS:
		if String(entry[0]) == spot:
			return entry[1]
	return Vector3.INF


# Puts the page at `spot` with her `lines`. True when it is there.
func place(spot: String, lines: PackedStringArray) -> bool:
	var text := "\n".join(lines)
	if spot == LOCKER_SPOT:
		clipboard_text.text = text
		return true
	var at := spot_point(spot)
	if at == Vector3.INF or _native == null:
		return false
	var ground := _clear_floor(at)
	if ground == Vector3.INF:
		return false
	placed[spot] = lay_page("ChartPage_" + spot, ground, 0.35 + 0.6 * float(placed.size() % 3), text)
	return true


# A loose page flat on the floor at `ground`, turned `yaw`, a nut on its
# corner against the wind.
func lay_page(node_name: String, ground: Vector3, yaw: float, text: String) -> Node3D:
	var page := Node3D.new()
	page.name = node_name
	add_child(page)
	page.position = on_planks(ground) + Vector3(0.0, 0.004, 0.0)
	page.rotation = Vector3(-PI * 0.5, yaw, 0.0)
	var sheet := _sheet(page)
	var hand := _hand(sheet)
	hand.text = text
	var nut := MeshInstance3D.new()
	var hex := CylinderMesh.new()
	hex.top_radius = 0.028
	hex.bottom_radius = 0.028
	hex.height = 0.022
	hex.radial_segments = 6
	hex.rings = 1
	nut.mesh = hex
	nut.material_override = _steel_material
	nut.visibility_range_end = DRAW_RANGE_M
	page.add_child(nut)
	# On the sheet's top corner, standing on the deck.
	nut.position = Vector3(PAPER.x * 0.36, PAPER.y * 0.42, 0.011)
	nut.rotation.x = PI * 0.5
	return page


# On a Stack deck's walking band the floor seen is its timber planks, drawn
# over the native deck (main_world.gd): there a page lies along the nearest
# plank, on its top. Anywhere else what is drawn is the native floor itself.
static func on_planks(ground: Vector3) -> Vector3:
	var level := roundi(ground.y / MainWorld.STACK_LEVEL_HEIGHT)
	var deck_y := float(level) * MainWorld.STACK_LEVEL_HEIGHT
	if level < 1 or level > MainWorld.STACK_LEVEL_COUNT or absf(ground.y - deck_y) > 0.05:
		return ground
	var center := MainWorld.STACK_CENTER
	if absf(ground.x - center.x) > MainWorld.STACK_HALF_EXTENT - 1.4:
		return ground
	var band_middle := MainWorld.STACK_HALF_EXTENT - MainWorld.STACK_DECK_BAND_DEPTH * 0.5
	var across := absf(ground.z - center.z) - band_middle
	var plank := clampi(roundi(across / MainWorld.DECK_PLANK_PITCH), -2, 2)
	if absf(across - float(plank) * MainWorld.DECK_PLANK_PITCH) > MainWorld.DECK_PLANK_PITCH:
		return ground
	var side := signf(ground.z - center.z)
	return Vector3(ground.x, deck_y + MainWorld.DECK_PLANK_TOP,
		center.z + side * (band_middle + float(plank) * MainWorld.DECK_PLANK_PITCH))


# The floor near `at` a page may lie on, as the native has it: what stands
# (the world's structure, a band's static frame; never a machine that moves)
# and level, with no drop within CLEAR_M all round. Vector3.INF for none.
func _clear_floor(at: Vector3) -> Vector3:
	for offset in NEAR_OFFSETS:
		var from := at + Vector3(offset.x, 1.0, offset.y)
		var down: Dictionary = _native.get_look_target(from, Vector3.DOWN, 3.0)
		if not bool(down.get("hit", false)):
			continue
		var entity := int(down["entity"])
		if not (entity in [1, 11, 51] or (entity >= 1000 and entity < 2000)):
			continue
		if (down["normal"] as Vector3).y < 0.95:
			continue
		var floor_y := (down["point"] as Vector3).y
		var clear := true
		for k in 12:
			var a := TAU * float(k) / 12.0
			for r in [CLEAR_M * 0.5, CLEAR_M]:
				var probe := Vector3(from.x + sin(a) * float(r), floor_y + 0.6, from.z + cos(a) * float(r))
				var below := float(_native.get_surface_below(probe, DROP_M + 0.6))
				# A drop, a step, or something standing there: not clear.
				if is_nan(below) or absf(below - floor_y) > 0.3:
					clear = false
					break
			if not clear:
				break
		if clear:
			return Vector3(from.x, floor_y, from.z)
	return Vector3.INF


# The sheet of the page at `spot` (its face is its +z), null when none.
func page_sheet(spot: String) -> Node3D:
	var page: Node3D = placed.get(spot)
	return page.find_child("ChartSheet", true, false) as Node3D if page != null else null


func paper_texture() -> Texture2D:
	return _paper_material.albedo_texture


# The text of the page at `spot`, "" when there is none (for the tests).
func page_text(spot: String) -> String:
	if spot == LOCKER_SPOT:
		return clipboard_text.text
	var page: Node3D = placed.get(spot)
	if page == null:
		return ""
	var hand := page.find_child("Hand", true, false) as Label3D
	return hand.text if hand != null else ""
