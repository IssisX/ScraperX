extends RefCounted

# Dressing nothing holds up. A lamp, a sign or a coat of paint is put on the
# structure it belongs to; when that structure is moved, rebuilt or taken
# away, the dressing stays where it was, in the air. This finds it. A tool,
# like solid_export.gd: the game builds its whole world, kit and views with
# this watching every node that enters the tree (so each part knows the
# builder line that placed it), runs AUDIT_FRAMES frames so every view has
# posed its parts, and then
#
#   godot --headless --path godot -- --audit-floating=<report path>
#
# asks of every part what holds it. The native kit's bodies and anything on
# the ground hold themselves. Any other part is held when a held part lies
# within MOUNT_GAP_M of it the way it mounts: behind, in front of, above or
# below a board, a label or a coat of paint (never only at its edge); under a
# machine's beacon; on any side of a lamp, a solid part or other dressing.
# A light is held when a held part, its fitting, is within LIGHT_REACH_M.
# Every light, lamp, sign and non-solid dressing part nothing holds is listed
# with where it is, what it is, the nearest thing that could hold it and the
# builder line that put it there; so is every solid part. Prints
# SCRAPERX_FLOATING and exits 0 when nothing floats, EXIT_FLOATING when
# something does.

const SolidExport := preload("res://presentation/solid_export.gd")

const AUDIT_FRAMES := 3
const MOUNT_GAP_M := 0.15
const CONTACT_M := 0.02
const LIGHT_REACH_M := 1.2
# A label's board: a part at most BOARD_THICK_M half thick, within BOARD_GAP_M
# of the label; the label may pass its face by BOARD_SLACK_M.
const BOARD_THICK_M := 0.15
const BOARD_GAP_M := 0.25
const BOARD_SLACK_M := 0.02
const NEAREST_SEARCH_M := 40.0
const CELL_M := 4.0
const BIG_M := 48.0
# A cone (a mountain, a tree's canopy) is tested as this many stacked boxes.
const CONE_SLICES := 6
const EXIT_FLOATING := 43
const KIT_PART_FLOATS := 12
const KIT_SHAPE_CYLINDER := 1
const KIT_SHAPE_CAPSULE := 2

# Dressing among the solid parts: lamps and signs, collidable, but hung on
# the structure rather than being it.
const LAMPS := ["LampFitting", "LampHead", "CageLamp", "FloodFitting"]
const SIGNS := ["SignPlate", "SignPlateMark", "BayPlate", "Banner", "BannerRod", "BannerMark",
	"BannerBacking", "WordmarkBacking", "ChevronSpine", "ChevronUpper", "ChevronLower",
	"LiftSignBacking"]
# Laid on a surface: held along their thin axis.
const PAINT := ["DeckPlank", "DeckStreak", "StairTread", "Tread", "SkinRungPlate", "LaneStripe",
	"WalkwayPaint", "EdgeStripe", "ColumnLichen", "StandingWater", "StandingWaterTwo",
	"FloorLight", "FloorLightHigh", "UpperLightBand"]
# Builder helpers: the line that matters is the one that called them.
const HELPERS := ["_add_box", "_add_box_to", "_add_cylinder", "_add_cone", "_add_sphere",
	"_add_strut", "_add_sign_text", "_add_peak", "_box", "_cyl", "_ball", "_pivot", "_mesh"]

var exit_code := 0
var report_path := ""
var _frames := 0
# Every part as an oriented box: {name, kind, where, c, u, e, rule, held_by}
# (its centre, unit axes and half extents). Kinds: "kit" and "world" are
# structure; "lamp", "sign", "paint", "beacon" and "dressing" are dressing.
var _parts: Array = []
var _lights: Array = []
var _grid := {}               # Vector3i -> Array of part indices
var _big := PackedInt32Array()


# Before the world is built: every node entering the tree is stamped with
# the builder line that added it.
func watch(tree: SceneTree) -> void:
	tree.node_added.connect(_placed)


func _placed(node: Node) -> void:
	node.set_meta(&"placed_by", _builder_line())


# The first two frames on the stack outside this file and the primitive
# helpers: "main_world.gd:434 _build_stack_dressing <- main_world.gd:197 _build_stack".
static func _builder_line() -> String:
	var found := PackedStringArray()
	for frame: Dictionary in get_stack():
		var source := str(frame["source"]).get_file()
		var function := str(frame["function"])
		if source == "floating_audit.gd" or function in HELPERS:
			continue
		found.append("%s:%d %s" % [source, int(frame["line"]), function])
		if found.size() == 2:
			break
	return " <- ".join(found) if not found.is_empty() else "?"


# Called every frame by main.gd; true once the audit has run and the report
# is written.
func frame(main: Node) -> bool:
	_frames += 1
	if _frames < AUDIT_FRAMES:
		return false
	run(main)
	return true


func run(main: Node) -> void:
	var tower: Node3D = main.get_node("TowerPresentation")
	_gather_world(tower)
	_gather_kit(main._native)
	_gather_views(main)
	_gather_lights_and_labels(main, tower)
	for index in _parts.size():
		_insert(index)
	_settle()
	var floating: Array = []
	var loose: Array = []
	for part in _parts:
		if str(part["held_by"]).is_empty():
			(loose if _is_structure(part) else floating).append(part)
	for light in _lights:
		if str(light["held_by"]).is_empty():
			floating.append(light)
	# Dark lamps (the owner, 2026-10-21: "light(s) that weren't giving off
	# light"): a lamp with no light of its own within LIGHT_REACH_M, or a
	# light whose range reaches nothing held.
	var dark: Array = []
	for part in _parts:
		if str(part["kind"]) == "lamp" and not _lit(part):
			dark.append(part)
	for light in _lights:
		if not _reaches(light):
			dark.append(light)
	var off_board := _text_off_boards()
	var lines := _report(floating, loose)
	lines.append("")
	lines.append("SIGN TEXT OFF ITS BOARD: %d" % off_board.size())
	for item: Dictionary in off_board:
		var c: Vector3 = item["c"]
		lines.append("  %-40s at (%7.2f, %7.2f, %8.2f)  past %s by %.2f m  placed by %s" % [item["name"], c.x, c.y, c.z,
			item["board"], float(item["past_m"]), item["where"]])
	lines.append("")
	lines.append("DARK LAMPS (no light of their own, or a light that reaches nothing): %d" % dark.size())
	for item: Dictionary in dark:
		var at: Vector3 = item["c"]
		lines.append("  %-6s %-34s at (%7.2f, %7.2f, %8.2f)  placed by %s" % [item["kind"], item["name"], at.x,
			at.y, at.z, item["where"]])
	if not report_path.is_empty():
		var file := FileAccess.open(report_path, FileAccess.WRITE)
		if file != null:
			file.store_string("\n".join(lines) + "\n")
	print("\n".join(lines))
	print("SCRAPERX_FLOATING floating=%d loose_solids=%d dark=%d off_board=%d parts=%d lights=%d" % [
		floating.size(), loose.size(), dark.size(), off_board.size(), _parts.size(), _lights.size()])
	exit_code = EXIT_FLOATING if floating.size() + loose.size() + dark.size() + off_board.size() > 0 else 0


# Sign text off its board (the owner, 2026-10-21, of a banner's slogan: "It's
# not even on the sign all the way"): a label laid on a board, a thin part
# lying flat just behind or in front of it, must lie within that board's face.
func _text_off_boards() -> Array:
	var out: Array = []
	for label in _parts:
		if not str(label["name"]).begins_with("Label3D"):
			continue
		var board := _board_of(label)
		if board.is_empty():
			continue
		var past := 0.0
		for axis in 2:
			var along: Vector3 = label["u"][axis]
			var reach := 0.0
			for i in 3:
				reach += absf((board["u"][i] as Vector3).dot(along)) * float((board["e"] as Vector3)[i])
			var off := absf(((label["c"] as Vector3) - (board["c"] as Vector3)).dot(along))
			past = maxf(past, off + float((label["e"] as Vector3)[axis]) - reach)
		if past > BOARD_SLACK_M:
			label["board"] = str(board["name"])
			label["past_m"] = past
			out.append(label)
	return out


# The board a label is painted on: of the thin parts flat to the label's face
# that its middle lies over, the one whose face is nearest it, within
# BOARD_GAP_M (a stencil on a plate on a door is held to the plate).
func _board_of(label: Dictionary) -> Dictionary:
	var normal: Vector3 = label["u"][2]
	var best := {}
	var best_gap := BOARD_GAP_M
	for index in _near(_grown(label, BOARD_GAP_M)):
		var part: Dictionary = _parts[index]
		if str(part["name"]).begins_with("Label3D"):
			continue
		var e: Vector3 = part["e"]
		var thin := 0 if e.x <= e.y and e.x <= e.z else (1 if e.y <= e.z else 2)
		if e[thin] > BOARD_THICK_M or absf((part["u"][thin] as Vector3).dot(normal)) < 0.95:
			continue
		var offset: Vector3 = (label["c"] as Vector3) - (part["c"] as Vector3)
		var gap := maxf(absf(offset.dot(normal)) - e[thin], 0.0)
		if gap > best_gap:
			continue
		var over := true
		for i in 3:
			if i != thin:
				over = over and absf(offset.dot(part["u"][i] as Vector3)) <= e[i]
		if over:
			best = part
			best_gap = gap
	return best


# A lamp is lit when one of the lights is within LIGHT_REACH_M of its box.
func _lit(lamp: Dictionary) -> bool:
	var half: Vector3 = lamp["e"]
	for light in _lights:
		if (light["c"] as Vector3).distance_to(lamp["c"]) <= LIGHT_REACH_M + half.length():
			return true
	return false


# A light reaches something when a held part's box comes within its range.
func _reaches(light: Dictionary) -> bool:
	var at: Vector3 = light["c"]
	var reach := float(light.get("range", 0.0))
	for part in _parts:
		if str(part["held_by"]).is_empty() and not _is_structure(part):
			continue
		if (part["c"] as Vector3).distance_to(at) - (part["e"] as Vector3).length() <= reach:
			return true
	return false


static func _is_structure(part: Dictionary) -> bool:
	return str(part["kind"]) == "kit" or str(part["kind"]) == "world"


# --- gathering ---------------------------------------------------------------

func _gather_world(node: Node) -> void:
	for child in node.get_children():
		if child is MeshInstance3D and child.has_meta(&"part") and (child as MeshInstance3D).mesh != null:
			var part := SolidExport._base_name(str(child.get_meta(&"part")))
			var shown := (child as Node3D).is_visible_in_tree()
			if part in LAMPS:
				if shown:
					_add(child, part, "lamp", "any")
			elif part in SIGNS:
				if shown:
					_add(child, part, "sign", "thin")
			elif part in SolidExport.SOLID or part in SolidExport.MIRROR or part in SolidExport.MOVING:
				# Hidden or not: a solid part is collision either way.
				_add(child, part, "world", "any")
			elif shown:
				_add(child, part, "paint" if part in PAINT else "dressing", "thin" if part in PAINT else "any")
		elif child is Label3D and (child as Node3D).is_visible_in_tree():
			_add(child, "Label3D \"%s\"" % (child as Label3D).text.replace("\n", " "), "sign", "thin")
		_gather_world(child)


func _gather_kit(native: Object) -> void:
	for body in int(native.get_kit_body_count()):
		if not bool(native.is_kit_body_enabled(body)):
			continue
		var at: Transform3D = native.get_kit_body_transform(body)
		var parts: PackedFloat32Array = native.get_kit_body_parts(body)
		var entity := int(native.get_kit_body_entity_id(body))
		for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
			var half := Vector3(parts[p], parts[p + 1], parts[p + 2])
			match int(parts[p + 11]):
				KIT_SHAPE_CYLINDER:
					half = Vector3(parts[p], parts[p + 1], parts[p])
				KIT_SHAPE_CAPSULE:
					half = Vector3(parts[p], parts[p + 1] + parts[p], parts[p])
			var part := at * Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8],
				parts[p + 9])), Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			var box := _box(Transform3D(part.basis.orthonormalized(), part.origin), half)
			box["name"] = "kit body %d's part at (%.1f, %.1f, %.1f)" % [entity, part.origin.x,
				part.origin.y, part.origin.z]
			box["kind"] = "kit"
			box["where"] = "native"
			box["rule"] = "any"
			box["held_by"] = ""
			_parts.append(box)


# The views' own fixtures: the yard crane's lamps and falls, the Matron's
# locker and its gauge, and every machine's beacon.
func _gather_views(main: Node) -> void:
	var crane: Node = main.get_node_or_null("YardCraneView")
	if crane != null:
		for child in crane.get_children():
			if child is MeshInstance3D and (child as Node3D).is_visible_in_tree():
				var lamp := str(child.name).begins_with("CraneLamp")
				_add(child, str(child.name), "lamp" if lamp else "dressing", "any")
	var matron: Node = main.get("_matron_view")
	if matron != null:
		# Drawn only near her locker or her (matron_view.gd DRAW_RANGE_M), and
		# the audit's camera is at the start: seen as it stands when near, so
		# what hangs on her locker is checked against it.
		(matron as Node3D).visible = true
		_gather_meshes(matron, "MatronView")
	# The uncanny view's own fixtures (uncanny/uncanny.gd): the clipboard in
	# her locker and the tower's clocks, each on what it hangs on.
	var uncanny: Node = main.get("_uncanny")
	if uncanny != null:
		_gather_meshes(uncanny, "Uncanny")
	var works: Node = main.get("_works")
	if works != null:
		var nodes: Array = works.get("_beacon_nodes")
		var local: PackedVector3Array = works.get("_beacon_local")
		for i in nodes.size():
			var node: Node3D = nodes[i]
			if not node.is_visible_in_tree():
				continue
			var at: Vector3 = node.global_transform * local[i]
			_parts.append({"name": "MachineBeacon on %s" % node.name, "kind": "beacon",
				"where": "works.gd add_beacon <- main.gd _ready", "c": at + Vector3(0.0, 0.09, 0.0),
				"u": [Vector3.RIGHT, Vector3.UP, Vector3.BACK], "e": Vector3(0.11, 0.09, 0.11),
				"rule": "down", "held_by": ""})


func _gather_meshes(node: Node, label: String) -> void:
	for child in node.get_children():
		if child is MeshInstance3D and (child as Node3D).is_visible_in_tree() and (child as MeshInstance3D).mesh != null:
			_add(child, "%s part" % label, "dressing", "any")
		_gather_meshes(child, label)


func _gather_lights_and_labels(node: Node, tower: Node) -> void:
	for child in node.get_children():
		if child == tower:
			continue
		if child is Light3D and not (child is DirectionalLight3D) and (child as Node3D).is_visible_in_tree():
			# A light renamed for a sibling of the same name is known by its class.
			var named := "" if str(child.name).begins_with("@") else " %s" % child.name
			var reach := (child as OmniLight3D).omni_range if child is OmniLight3D else \
				((child as SpotLight3D).spot_range if child is SpotLight3D else 0.0)
			_lights.append({"name": child.get_class() + named, "kind": "light",
				"where": str(child.get_meta(&"placed_by", "?")), "c": (child as Node3D).global_position,
				"u": [Vector3.RIGHT, Vector3.UP, Vector3.BACK], "e": Vector3.ONE * LIGHT_REACH_M,
				"held_by": "", "range": reach})
		elif child is Label3D and (child as Node3D).is_visible_in_tree():
			_add(child, "Label3D \"%s\"" % (child as Label3D).text.replace("\n", " "), "sign", "thin")
		_gather_lights_and_labels(child, tower)


func _add(node: VisualInstance3D, part: String, kind: String, rule: String) -> void:
	var where := str(node.get_meta(&"placed_by", "?"))
	var mesh: Mesh = (node as MeshInstance3D).mesh if node is MeshInstance3D else null
	var boxes: Array = []
	if mesh is CylinderMesh and (mesh as CylinderMesh).top_radius < 0.01:
		# A cone, as stacked slices narrowing to its tip.
		var cone := mesh as CylinderMesh
		var at := node.global_transform
		for slice in CONE_SLICES:
			var radius := cone.bottom_radius * (1.0 - float(slice) / CONE_SLICES)
			var height := cone.height / CONE_SLICES
			var centre := Vector3(0.0, -cone.height * 0.5 + height * (float(slice) + 0.5), 0.0)
			boxes.append(_box(at * Transform3D(Basis(), centre), Vector3(radius, height * 0.5, radius)))
	else:
		var bounds := node.get_aabb()
		boxes.append(_box(node.global_transform * Transform3D(Basis(), bounds.get_center()), bounds.size * 0.5))
	for box: Dictionary in boxes:
		box["name"] = part
		box["kind"] = kind
		box["where"] = where
		box["rule"] = rule
		box["held_by"] = ""
		_parts.append(box)


# --- boxes -------------------------------------------------------------------

static func _box(at: Transform3D, half: Vector3) -> Dictionary:
	var x := at.basis.x
	var y := at.basis.y
	var z := at.basis.z
	return {"c": at.origin, "u": _axes(at.basis),
		"e": Vector3(half.x * x.length(), half.y * y.length(), half.z * z.length())}


static func _axes(basis: Basis) -> Array:
	var x := basis.x.normalized() if basis.x.length() > 1.0e-6 else Vector3.RIGHT
	var y := basis.y.normalized() if basis.y.length() > 1.0e-6 else Vector3.UP
	var z := basis.z.normalized() if basis.z.length() > 1.0e-6 else Vector3.BACK
	return [x, y, z]


static func _grown(box: Dictionary, by: float) -> Dictionary:
	var grown := box.duplicate()
	grown["e"] = ((box["e"] as Vector3) + Vector3.ONE * by).max(Vector3.ZERO)
	return grown


# The box pushed out by `gap` on the face of it that looks most nearly along
# `direction`, and by CONTACT_M all round.
static func _reaching(box: Dictionary, direction: Vector3, gap: float) -> Dictionary:
	var axes: Array = box["u"]
	var best := 0
	for i in 3:
		if absf((axes[i] as Vector3).dot(direction)) > absf((axes[best] as Vector3).dot(direction)):
			best = i
	var side := signf((axes[best] as Vector3).dot(direction))
	var reach := _grown(box, CONTACT_M)
	var e: Vector3 = reach["e"]
	e[best] += gap * 0.5
	reach["e"] = e
	reach["c"] = (box["c"] as Vector3) + (axes[best] as Vector3) * side * gap * 0.5
	return reach


# Where something holding the part may be: a board, a label or paint behind,
# in front of, above or below it, never only at its edge; a beacon under it;
# anything else on any side.
static func _probes(part: Dictionary) -> Array:
	var rule := str(part["rule"])
	if rule == "down":
		return [_reaching(part, Vector3.DOWN, MOUNT_GAP_M)]
	if rule == "thin":
		var e: Vector3 = part["e"]
		var thin := 0
		for i in 3:
			if e[i] < e[thin]:
				thin = i
		if e[thin] < 0.5 * minf(e[(thin + 1) % 3], e[(thin + 2) % 3]):
			var normal: Vector3 = (part["u"] as Array)[thin]
			return [_reaching(part, normal, MOUNT_GAP_M), _reaching(part, -normal, MOUNT_GAP_M),
				_reaching(part, Vector3.UP, MOUNT_GAP_M), _reaching(part, Vector3.DOWN, MOUNT_GAP_M)]
	return [_grown(part, MOUNT_GAP_M)]


static func _bounds(box: Dictionary) -> AABB:
	var axes: Array = box["u"]
	var e: Vector3 = box["e"]
	var reach := Vector3.ZERO
	for i in 3:
		var axis: Vector3 = axes[i]
		reach += Vector3(absf(axis.x), absf(axis.y), absf(axis.z)) * e[i]
	return AABB((box["c"] as Vector3) - reach, reach * 2.0)


# Separating axes (Ericson, Real-Time Collision Detection 4.4.1).
static func _overlap(a: Dictionary, b: Dictionary) -> bool:
	var au: Array = a["u"]
	var bu: Array = b["u"]
	var ae: Vector3 = a["e"]
	var be: Vector3 = b["e"]
	var r := [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
	var ar := [[0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]]
	for i in 3:
		for j in 3:
			r[i][j] = (au[i] as Vector3).dot(bu[j])
			ar[i][j] = absf(r[i][j]) + 1.0e-6
	var d: Vector3 = (b["c"] as Vector3) - (a["c"] as Vector3)
	var t := Vector3(d.dot(au[0]), d.dot(au[1]), d.dot(au[2]))
	for i in 3:
		if absf(t[i]) > ae[i] + be[0] * ar[i][0] + be[1] * ar[i][1] + be[2] * ar[i][2]:
			return false
	for j in 3:
		if absf(t[0] * r[0][j] + t[1] * r[1][j] + t[2] * r[2][j]) > \
				ae[0] * ar[0][j] + ae[1] * ar[1][j] + ae[2] * ar[2][j] + be[j]:
			return false
	if absf(t[2] * r[1][0] - t[1] * r[2][0]) > ae[1] * ar[2][0] + ae[2] * ar[1][0] + be[1] * ar[0][2] + be[2] * ar[0][1]:
		return false
	if absf(t[2] * r[1][1] - t[1] * r[2][1]) > ae[1] * ar[2][1] + ae[2] * ar[1][1] + be[0] * ar[0][2] + be[2] * ar[0][0]:
		return false
	if absf(t[2] * r[1][2] - t[1] * r[2][2]) > ae[1] * ar[2][2] + ae[2] * ar[1][2] + be[0] * ar[0][1] + be[1] * ar[0][0]:
		return false
	if absf(t[0] * r[2][0] - t[2] * r[0][0]) > ae[0] * ar[2][0] + ae[2] * ar[0][0] + be[1] * ar[1][2] + be[2] * ar[1][1]:
		return false
	if absf(t[0] * r[2][1] - t[2] * r[0][1]) > ae[0] * ar[2][1] + ae[2] * ar[0][1] + be[0] * ar[1][2] + be[2] * ar[1][0]:
		return false
	if absf(t[0] * r[2][2] - t[2] * r[0][2]) > ae[0] * ar[2][2] + ae[2] * ar[0][2] + be[0] * ar[1][1] + be[1] * ar[1][0]:
		return false
	if absf(t[1] * r[0][0] - t[0] * r[1][0]) > ae[0] * ar[1][0] + ae[1] * ar[0][0] + be[1] * ar[2][2] + be[2] * ar[2][1]:
		return false
	if absf(t[1] * r[0][1] - t[0] * r[1][1]) > ae[0] * ar[1][1] + ae[1] * ar[0][1] + be[0] * ar[2][2] + be[2] * ar[2][0]:
		return false
	if absf(t[1] * r[0][2] - t[0] * r[1][2]) > ae[0] * ar[1][2] + ae[1] * ar[0][2] + be[0] * ar[2][1] + be[1] * ar[2][0]:
		return false
	return true


# --- the parts' grid -----------------------------------------------------------

func _insert(index: int) -> void:
	var bounds := _bounds(_parts[index])
	if bounds.size[bounds.get_longest_axis_index()] > BIG_M:
		_big.append(index)
		return
	for cell in _cells(bounds):
		if not _grid.has(cell):
			_grid[cell] = []
		(_grid[cell] as Array).append(index)


static func _cells(bounds: AABB) -> Array[Vector3i]:
	var low := Vector3i((bounds.position / CELL_M).floor())
	var high := Vector3i((bounds.end / CELL_M).floor())
	var cells: Array[Vector3i] = []
	for x in range(low.x, high.x + 1):
		for y in range(low.y, high.y + 1):
			for z in range(low.z, high.z + 1):
				cells.append(Vector3i(x, y, z))
	return cells


func _near(box: Dictionary) -> PackedInt32Array:
	var seen := {}
	for cell in _cells(_bounds(box)):
		if _grid.has(cell):
			for index in _grid[cell]:
				seen[index] = true
	var found := PackedInt32Array(seen.keys())
	found.append_array(_big)
	return found


# --- what holds what ----------------------------------------------------------

# From what holds itself -- a native body, a part on the ground -- outward:
# each part newly held holds every part it lies in the mounting reach of.
func _settle() -> void:
	var probes: Array = []
	var frontier: Array[int] = []
	for index in _parts.size():
		var part: Dictionary = _parts[index]
		probes.append(_probes(part))
		if str(part["kind"]) == "kit":
			part["held_by"] = "itself (native)"
			frontier.append(index)
		elif str(part["kind"]) == "world" and _bounds(part).position.y <= CONTACT_M:
			part["held_by"] = "the ground"
			frontier.append(index)
	while not frontier.is_empty():
		var holder: int = frontier.pop_back()
		var reach := _grown(_parts[holder], MOUNT_GAP_M + CONTACT_M)
		for index in _near(reach):
			if index == holder or not str(_parts[index]["held_by"]).is_empty():
				continue
			if _any_overlap(probes[index], _parts[holder]):
				_parts[index]["held_by"] = str(_parts[holder]["name"])
				frontier.append(index)
	for light in _lights:
		for index in _near(light):
			if not str(_parts[index]["held_by"]).is_empty() and _overlap(light, _parts[index]):
				light["held_by"] = str(_parts[index]["name"])
				break


static func _any_overlap(probes: Array, box: Dictionary) -> bool:
	for probe in probes:
		if _overlap(probe, box):
			return true
	return false


# How far the nearest held part is: the least growth of the item that
# reaches one, to a centimeter.
func _nearest(item: Dictionary) -> Array:
	var best := NEAREST_SEARCH_M
	var best_name := "nothing within %.0f m" % NEAREST_SEARCH_M
	for index in _near(_grown(item, NEAREST_SEARCH_M)):
		var candidate: Dictionary = _parts[index]
		if is_same(candidate, item) or str(candidate["held_by"]).is_empty() \
				or not _overlap(_grown(item, best), candidate):
			continue
		var low := 0.0
		var high := best
		while high - low > 0.01:
			var mid := 0.5 * (low + high)
			if _overlap(_grown(item, mid), candidate):
				high = mid
			else:
				low = mid
		best = high
		best_name = str(candidate["name"])
	return [best, best_name]


# --- the report ----------------------------------------------------------------

func _report(floating: Array, loose: Array) -> PackedStringArray:
	var lines := PackedStringArray()
	var counts := {}
	for item in _parts + _lights:
		counts[item["kind"]] = int(counts.get(item["kind"], 0)) + 1
	lines.append("FLOATING DRESSING AUDIT: %d parts and lights checked %s" % [
		_parts.size() + _lights.size(), str(counts)])
	lines.append("Held: a native body or the ground; else a held part within %.2f m the way it mounts;" % MOUNT_GAP_M)
	lines.append("a light: a held part within %.1f m of it (its fitting)." % LIGHT_REACH_M)
	var by_where := func(a: Dictionary, b: Dictionary) -> bool:
		return str(a["where"]) + str(a["name"]) < str(b["where"]) + str(b["name"])
	floating.sort_custom(by_where)
	loose.sort_custom(by_where)
	lines.append("")
	lines.append("FLOATING LIGHTS, LAMPS, SIGNS AND DRESSING: %d" % floating.size())
	for item: Dictionary in floating:
		lines.append(_line(item, _nearest(_grown(item, 0.05 - LIGHT_REACH_M) if str(item["kind"]) == "light" else item)))
	lines.append("")
	lines.append("SOLID PARTS NOTHING HOLDS UP: %d" % loose.size())
	for item: Dictionary in loose:
		lines.append(_line(item, _nearest(item)))
	lines.append("")
	lines.append("HELD LIGHTS, LAMPS AND SIGNS, AND WHAT HOLDS THEM:")
	for item in _parts + _lights:
		var kind := str(item["kind"])
		if (kind == "light" or kind == "lamp" or kind == "sign") and not str(item["held_by"]).is_empty():
			var at: Vector3 = item["c"]
			lines.append("  %-6s %-34s at (%7.2f, %7.2f, %8.2f)  held by %s" % [kind, item["name"], at.x, at.y,
				at.z, item["held_by"]])
	return lines


static func _line(item: Dictionary, nearest: Array) -> String:
	var at: Vector3 = item["c"]
	return "  %-6s %-34s at (%7.2f, %7.2f, %8.2f)  nearest hold %5.2f m: %s  placed by %s" % [
		item["kind"], item["name"], at.x, at.y, at.z, float(nearest[0]), nearest[1], item["where"]]
