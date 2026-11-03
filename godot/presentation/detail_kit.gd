extends Node3D

# Steel detail where the hands are (environment plan, job 7). Every member was
# a plain box with razor edges. This lays, at build time and from the parts'
# own boxes, what built steel has where he holds it:
# - at a member's end that meets another part, cleat plates with a bolt group
#   on its two web faces and a weld bead round the end;
# - at an upright's foot on a floor, a base plate with four anchor nuts;
# - along a member's long edges, and round a plate's top, worn edges: bare
#   steel where hands and boots have rubbed the coat off, in broken runs.
# Drawn only, as MultiMeshes (one per kind per section): nothing here is a
# part (solid_export.gd never sees it), nothing feeds the native, and every
# piece is flush and small (plates 12-22 mm proud, bolt heads 10-30 mm), so it
# changes no collision and looks like no hold. Steel only: timber, concrete,
# glass and lit or see-through parts get none. Moving bodies get none (their
# parts are not batched; their detail would have to ride with them).
#
# The world's own parts are read before they are batched (main.gd calls
# attach before _batch_static_dressing); the native kit's static bodies are
# read once the kit exists, at the end of the frame main._ready runs in.

# Mirrors main_machines.gd: a kit part's floats and its shapes.
const KIT_PART_FLOATS := 12
const KIT_SHAPE_CYLINDER := 1
const KIT_SHAPE_CAPSULE := 2
# Mirrors main_machines.gd _build_kit's palette, by the native's Material
# enum: [sRGB albedo, metallic, roughness, steel]. Timber (2), concrete (3),
# rubble (6) and glass (8) are not steel.
const KIT_FINISHES := [
	[Color("36383b"), 0.3, 0.5, true],
	[Color("57382a"), 0.0, 0.9, true],
	[Color("4a3420"), 0.0, 0.9, false],
	[Color("4e4841"), 0.0, 0.94, false],
	[Color("9a4814"), 0.0, 0.66, true],
	[Color("6e7274"), 0.75, 0.6, true],
	[Color("5b544b"), 0.0, 0.98, false],
	[Color("8c690f"), 0.0, 0.58, true],
	[Color.WHITE, 0.0, 0.1, false],
]

# A member: a steel box at least MEMBER_MIN_LENGTH long, no thicker than
# MEMBER_MAX_SECTION, and at least MEMBER_SLENDER times longer than thick.
const MEMBER_MIN_LENGTH := 0.4
const MEMBER_MAX_SECTION := 0.7
const MEMBER_SLENDER := 3.0
# A plate or deck: no thicker than PLATE_MAX_THICK, at least PLATE_MIN_SPAN
# across both other ways.
const PLATE_MAX_THICK := 0.6
const PLATE_MIN_SPAN := 1.0
# A member's end meets a part when the end's middle lies within JOINT_GAP of
# that part's box.
const JOINT_GAP := 0.06
# An upright: a member within UPRIGHT_MAX_TILT of vertical (cosine).
const UPRIGHT_MIN_COS := 0.9
# Cleats and bolts go on members at least BOLTED_MIN_DEPTH deep on their web
# and BOLTED_MIN_WIDTH across it; thinner members (rails, rods) are welded.
const BOLTED_MIN_DEPTH := 0.09
const BOLTED_MIN_WIDTH := 0.08
const CLEAT_THICK := 0.012
const BOLT_PITCH := 0.09
const BOLT_RADIUS := 0.015
const BOLT_HEIGHT := 0.011
const BEAD_SIZE := 0.016
const BASE_PLATE_THICK := 0.022
const BASE_PLATE_MARGIN := 0.09
const ANCHOR_RADIUS := 0.019
const ANCHOR_HEIGHT := 0.03
const BASE_MIN_SECTION := 0.05
# Worn edges: a run WEAR_RUN_MIN..+WEAR_RUN_SPREAD long, then a gap
# WEAR_GAP_MIN..+WEAR_GAP_SPREAD, along each long edge of a member at least
# WEAR_MIN_SECTION thick and round the top of a plate. The rubbed metal is
# WEAR_MIX of the way from the coat to bare steel (or bright zinc).
const WEAR_MIN_SECTION := 0.04
const WEAR_RUN_MIN := 0.5
const WEAR_RUN_SPREAD := 2.2
const WEAR_GAP_MIN := 0.3
const WEAR_GAP_SPREAD := 1.6
const WEAR_RADIUS_MAX := 0.011
const WEAR_RADIUS_MIN := 0.005
const WEAR_MIX := 0.55
const BARE_STEEL := Color(0.58, 0.58, 0.57)
const BARE_ZINC := Color(0.75, 0.76, 0.76)
const WEAR_METALLIC := 0.85
const WEAR_ROUGHNESS := 0.36

# Where the parts are looked up: HASH_CELL metres; a part longer than
# HASH_BIG_SPAN any way in cells of HASH_BIG_CELL.
const HASH_CELL := 4.0
const HASH_BIG_SPAN := 24.0
const HASH_BIG_CELL := 32.0

# The kinds, each drawn per section of its own size to its own distance:
# plates (cleats, base plates, weld beads) as far as the world's fine batches
# (main_geometry.gd BATCH_FINE_RANGE); bolt and nut heads, and worn edges,
# only near, where they are more than a pixel or two.
enum Kind { PLATE, BOLT, EDGE }
const KIND_CELL := [96.0, 48.0, 32.0]
const KIND_RANGE := [160.0, 60.0, 45.0]
const KIND_FADE := [20.0, 10.0, 10.0]
# LOW (the GRAPHICS preset) keeps the plates only, nearer.
const LOW_PLATE_RANGE := 80.0
const QUALITY_LOW := 0
const QUALITY_POLL_S := 0.5
# Floats per instance in a MultiMesh buffer: a 3x4 transform, a colour and
# custom data (metallic, roughness).
const FLOATS := 20

const SHADER_CODE := """
shader_type spatial;
// Steel detail: the colour (sRGB) is the instance's, read as
// StandardMaterial3D reads an sRGB vertex colour (vertex_color_is_srgb), so
// it matches the batched parts under either renderer; INSTANCE_CUSTOM
// carries metallic and roughness.
varying vec2 finish;
void vertex() {
	finish = INSTANCE_CUSTOM.rg;
	if (!OUTPUT_IS_SRGB) {
		COLOR.rgb = mix(pow((COLOR.rgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)),
			COLOR.rgb * (1.0 / 12.92), lessThan(COLOR.rgb, vec3(0.04045)));
	}
}
void fragment() {
	ALBEDO = COLOR.rgb;
	METALLIC = finish.x;
	ROUGHNESS = finish.y;
}
"""

var _main: Node3D
# The parts, one entry per index: middle, unit axes, half extents along them,
# sRGB colour, metallic, roughness, steel, and a member's or plate's class.
var _centres := PackedVector3Array()
var _axes: Array[Basis] = []
var _halves := PackedVector3Array()
var _colours := PackedColorArray()
var _metallic := PackedFloat32Array()
var _roughness := PackedFloat32Array()
var _steel := PackedByteArray()
var _box := PackedByteArray()
# Cell -> Array of part indices. (Arrays, not packed arrays: a packed array
# read out of a Dictionary is a copy, so appending to it is lost.)
var _hash := {}
var _hash_big := {}
# One kind's instances in one section, FLOATS apiece, placed from the
# section's middle (so its draw distance is measured to the section). Held in
# an object so the buffer is appended in place.
class Section:
	var origin: Vector3
	var data := PackedFloat32Array()

# Kind -> section key -> Section; and each kind's last section, since the
# pieces of one joint or one edge land together.
var _sections := [{}, {}, {}]
var _last: Array[Section] = [null, null, null]
var _instances: Array = [[], [], []]
var _joints := 0
var _bases := 0
var _quality := -1
var _poll := 0.0

# Reads the world's parts under `world` now, and the kit's once it exists;
# adds the detail under `main`.
static func attach(main: Node3D, world: Node3D) -> void:
	var kit: Node3D = (load("res://presentation/detail_kit.gd") as GDScript).new()
	kit.name = "DetailKit"
	kit._main = main
	kit._read_world(world)
	main.add_child(kit)
	kit._finish.call_deferred()

func _read_world(world: Node3D) -> void:
	var bump_concrete: Texture2D = _main.get("_bump_concrete")
	var bump_timber: Texture2D = _main.get("_bump_timber")
	var to_root := world.global_transform.affine_inverse()
	var stack: Array[Node] = world.get_children()
	while not stack.is_empty():
		var node: Node = stack.pop_back()
		stack.append_array(node.get_children())
		var instance := node as MeshInstance3D
		# As the batcher reads them (_batch_static_dressing): a part with a
		# script of its own may move, so it gets no detail.
		if instance == null or instance.mesh == null or not instance.visible or instance.get_script() != null:
			continue
		var at := to_root * instance.global_transform
		var is_box := instance.mesh is BoxMesh
		if not is_box and not (instance.mesh is CylinderMesh):
			continue
		var material := (instance.material_override if instance.material_override != null
			else instance.get_active_material(0)) as StandardMaterial3D
		var steel := material != null and not material.emission_enabled \
			and material.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED \
			and material.normal_enabled and material.normal_texture != null \
			and material.normal_texture != bump_concrete and material.normal_texture != bump_timber
		var colour := material.albedo_color if material != null else Color(0.3, 0.3, 0.3)
		_add_part(at, instance.mesh.get_aabb(), colour,
			material.metallic if material != null else 0.0,
			material.roughness if material != null else 0.9, steel, is_box)

func _read_kit() -> void:
	var native: Object = _main.get("_native")
	if native == null:
		return
	for body in int(native.get_kit_body_count()):
		if bool(native.is_kit_body_dynamic(body)):
			continue
		var body_at: Transform3D = native.get_kit_body_transform(body)
		var parts: PackedFloat32Array = native.get_kit_body_parts(body)
		for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
			var shape := int(parts[p + 11])
			var half := Vector3(parts[p], parts[p + 1], parts[p + 2])
			if shape == KIT_SHAPE_CYLINDER:
				half = Vector3(parts[p], parts[p + 1], parts[p])
			elif shape == KIT_SHAPE_CAPSULE:
				half = Vector3(parts[p], parts[p + 1] + parts[p], parts[p])
			var finish: Array = KIT_FINISHES[clampi(int(parts[p + 10]), 0, KIT_FINISHES.size() - 1)]
			var at := body_at * Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8],
				parts[p + 9])), Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			_add_part(at, AABB(-half, half * 2.0), finish[0], finish[1], finish[2], finish[3],
				shape != KIT_SHAPE_CYLINDER and shape != KIT_SHAPE_CAPSULE)

func _add_part(at: Transform3D, bounds: AABB, colour: Color, metallic: float, roughness: float,
		steel: bool, box: bool) -> void:
	var axes := Basis()
	var half := Vector3.ZERO
	for i in 3:
		var column := at.basis[i]
		var length := column.length()
		if length < 1e-6:
			return
		axes[i] = column / length
		half[i] = 0.5 * bounds.size[i] * length
	var index := _centres.size()
	_centres.append(at * bounds.get_center())
	_axes.append(axes)
	_halves.append(half)
	_colours.append(colour)
	_metallic.append(metallic)
	_roughness.append(roughness)
	_steel.append(1 if steel else 0)
	_box.append(1 if box else 0)
	var extent := Vector3(
		absf(axes.x.x) * half.x + absf(axes.y.x) * half.y + absf(axes.z.x) * half.z,
		absf(axes.x.y) * half.x + absf(axes.y.y) * half.y + absf(axes.z.y) * half.z,
		absf(axes.x.z) * half.x + absf(axes.y.z) * half.y + absf(axes.z.z) * half.z)
	var big := 2.0 * maxf(extent.x, maxf(extent.y, extent.z)) > HASH_BIG_SPAN
	var hash: Dictionary = _hash_big if big else _hash
	var cell := HASH_BIG_CELL if big else HASH_CELL
	var low := ((_centres[index] - extent - Vector3.ONE * JOINT_GAP) / cell).floor()
	var high := ((_centres[index] + extent + Vector3.ONE * JOINT_GAP) / cell).floor()
	for x in range(int(low.x), int(high.x) + 1):
		for y in range(int(low.y), int(high.y) + 1):
			for z in range(int(low.z), int(high.z) + 1):
				var key := Vector3i(x, y, z)
				if not hash.has(key):
					hash[key] = []
				(hash[key] as Array).append(index)

func _finish() -> void:
	var started := Time.get_ticks_usec()
	_read_kit()
	for index in _centres.size():
		if _steel[index] == 1 and _box[index] == 1:
			_detail(index)
	var made := 0
	for sections: Dictionary in _sections:
		for key in sections:
			made += (sections[key] as Section).data.size() / FLOATS
	_emit()
	print("SCRAPERX_DETAIL parts=%d joints=%d bases=%d instances=%d meshes=%d ms=%d" % [_centres.size(),
		_joints, _bases, made, _instances[0].size() + _instances[1].size() + _instances[2].size(),
		(Time.get_ticks_usec() - started) / 1000])
	# Only the drawn instances are kept.
	_sections = [{}, {}, {}]
	_hash.clear()
	_hash_big.clear()
	_centres.clear()
	_axes.clear()
	_halves.clear()
	_colours.clear()
	_metallic.clear()
	_roughness.clear()
	_steel.clear()
	_box.clear()

# The detail one steel box gets, by its shape.
func _detail(index: int) -> void:
	var half := _halves[index]
	var axes := _axes[index]
	# Its axes by length: long, then the web's depth, then its width.
	var order := [0, 1, 2]
	order.sort_custom(func(a: int, b: int) -> bool: return half[a] > half[b])
	var a: float = half[order[0]]
	var b: float = half[order[1]]
	var c: float = half[order[2]]
	var along: Vector3 = axes[order[0]]
	var deep: Vector3 = axes[order[1]]
	var wide: Vector3 = axes[order[2]]
	if 2.0 * a >= MEMBER_MIN_LENGTH and 2.0 * b <= MEMBER_MAX_SECTION and a >= MEMBER_SLENDER * b:
		for end: float in [-1.0, 1.0]:
			var tip := _centres[index] + along * (end * a)
			var met := _meets(tip, index)
			if met < 0:
				continue
			_joints += 1
			var out := along * end
			if absf(along.y) >= UPRIGHT_MIN_COS and out.y < 0.0 and _is_floor(met, tip) \
					and c >= 0.5 * BASE_MIN_SECTION:
				_base_plate(index, tip, deep, wide, b, c)
			else:
				_end_joint(index, tip, out, deep, wide, b, c)
		if c >= 0.5 * WEAR_MIN_SECTION:
			for sb: float in [-1.0, 1.0]:
				for sc: float in [-1.0, 1.0]:
					_wear(index, _centres[index] + deep * (sb * b) + wide * (sc * c), along, a,
						clampf(minf(b, c) * 0.12, WEAR_RADIUS_MIN, WEAR_RADIUS_MAX), int(sb + 1.0) * 2 + int(sc + 1.0))
	elif 2.0 * c <= PLATE_MAX_THICK and 2.0 * b >= PLATE_MIN_SPAN:
		# A plate's top edges, where he mantles and steps: the face of the
		# thin axis that looks most up.
		if absf(wide.y) < 0.7:
			return
		var up := wide if wide.y > 0.0 else -wide
		var top := _centres[index] + up * c
		var radius := clampf(c * 0.12, WEAR_RADIUS_MIN, WEAR_RADIUS_MAX)
		for side: float in [-1.0, 1.0]:
			_wear(index, top + deep * (side * b), along, a, radius, int(side + 1.0))
			_wear(index, top + along * (side * a), deep, b, radius, 4 + int(side + 1.0))

# The part (not `self_index`) whose box holds `point` within JOINT_GAP, or -1.
func _meets(point: Vector3, self_index: int) -> int:
	var key := Vector3i((point / HASH_CELL).floor())
	if _hash.has(key):
		for other in _hash[key]:
			if other != self_index and _holds(other, point):
				return other
	key = Vector3i((point / HASH_BIG_CELL).floor())
	if _hash_big.has(key):
		for other in _hash_big[key]:
			if other != self_index and _holds(other, point):
				return other
	return -1

func _holds(index: int, point: Vector3) -> bool:
	var local := point - _centres[index]
	var axes := _axes[index]
	var half := _halves[index]
	return absf(local.dot(axes.x)) <= half.x + JOINT_GAP and absf(local.dot(axes.y)) <= half.y + JOINT_GAP \
		and absf(local.dot(axes.z)) <= half.z + JOINT_GAP

# Whether `point` stands on the top of part `index`: its upper face is level
# and `point` is at that face.
func _is_floor(index: int, point: Vector3) -> bool:
	var axes := _axes[index]
	var half := _halves[index]
	for i in 3:
		if absf(axes[i].y) >= UPRIGHT_MIN_COS:
			var up: Vector3 = axes[i] if axes[i].y > 0.0 else -axes[i]
			var top := _centres[index] + up * half[i]
			return absf((point - top).dot(up)) <= JOINT_GAP
	return false

func _end_joint(index: int, tip: Vector3, out: Vector3, deep: Vector3, wide: Vector3, b: float,
		c: float) -> void:
	var colour := _colours[index]
	_bead_ring(index, tip, out, deep, wide, b, c)
	if 2.0 * b < BOLTED_MIN_DEPTH or 2.0 * c < BOLTED_MIN_WIDTH:
		return
	# Double cleats on the web faces, bolted through: two columns along the
	# member, as many rows across the web as fit at BOLT_PITCH.
	var length := clampf(0.16 + 0.5 * b, 0.16, 0.36)
	var height := 2.0 * b * 0.78
	var rows := clampi(int(height / BOLT_PITCH), 1, 4)
	var bolt_colour := Color(colour.r * 0.7, colour.g * 0.7, colour.b * 0.7)
	for side: float in [-1.0, 1.0]:
		var face := wide * side
		var middle := tip - out * (length * 0.5) + face * (c + CLEAT_THICK * 0.5)
		_add(Kind.PLATE, middle, Basis(out * length, deep * height, face * CLEAT_THICK), colour,
			_metallic[index], _roughness[index])
		for column: float in [0.3, 0.72]:
			for row in rows:
				var across := (float(row) - 0.5 * float(rows - 1)) * BOLT_PITCH
				var head := tip - out * (length * column) + deep * across \
					+ face * (c + CLEAT_THICK + BOLT_HEIGHT * 0.5)
				_add(Kind.BOLT, head, Basis(out * BOLT_RADIUS, face * BOLT_HEIGHT, deep * BOLT_RADIUS),
					bolt_colour, 0.45, 0.55)

# A fillet weld round a member's end where it meets what holds it.
func _bead_ring(index: int, tip: Vector3, out: Vector3, deep: Vector3, wide: Vector3, b: float,
		c: float) -> void:
	var size := minf(BEAD_SIZE, minf(b, c) * 0.6)
	var colour := _colours[index] * 0.62
	colour.a = 1.0
	var at := tip - out * (size * 0.5)
	for side: float in [-1.0, 1.0]:
		_add(Kind.PLATE, at + wide * (side * (c + size * 0.5)),
			Basis(deep * (2.0 * b + 2.0 * size), wide * size, out * size), colour, 0.2, 0.8)
		_add(Kind.PLATE, at + deep * (side * (b + size * 0.5)),
			Basis(deep * size, wide * (2.0 * c), out * size), colour, 0.2, 0.8)

# An upright's foot on a floor: a base plate, four anchor nuts, and the weld
# round the upright on the plate.
func _base_plate(index: int, foot: Vector3, deep: Vector3, wide: Vector3, b: float, c: float) -> void:
	_bases += 1
	var colour := _colours[index]
	var span_deep := 2.0 * b + 2.0 * BASE_PLATE_MARGIN
	var span_wide := 2.0 * c + 2.0 * BASE_PLATE_MARGIN
	var up := Vector3.UP
	_add(Kind.PLATE, foot + up * (BASE_PLATE_THICK * 0.5),
		Basis(deep * span_deep, up * BASE_PLATE_THICK, wide * span_wide), colour,
		_metallic[index], _roughness[index])
	var nut_colour := Color(colour.r * 0.55, colour.g * 0.55, colour.b * 0.55)
	var inset := BASE_PLATE_MARGIN * 0.5
	for sd: float in [-1.0, 1.0]:
		for sw: float in [-1.0, 1.0]:
			var nut := foot + deep * (sd * (b + inset)) + wide * (sw * (c + inset)) \
				+ up * (BASE_PLATE_THICK + ANCHOR_HEIGHT * 0.5)
			_add(Kind.BOLT, nut, Basis(deep * ANCHOR_RADIUS, up * ANCHOR_HEIGHT, wide * ANCHOR_RADIUS),
				nut_colour, 0.4, 0.6)
	_bead_ring(index, foot + up * BASE_PLATE_THICK, -up, deep, wide, b, c)

# Worn runs along an edge through `middle`, `half_length` each way along `along`.
func _wear(index: int, middle: Vector3, along: Vector3, half_length: float, radius: float,
		edge: int) -> void:
	var bare := BARE_ZINC if _metallic[index] > 0.6 else BARE_STEEL
	var worn := _colours[index].lerp(bare, WEAR_MIX)
	worn.a = 1.0
	var side := along.cross(Vector3.UP if absf(along.y) < 0.9 else Vector3.RIGHT).normalized()
	var front := side.cross(along)
	var s := -half_length + WEAR_GAP_MIN * _hash01(index, edge * 97)
	var step := 0
	while s < half_length:
		var run := WEAR_RUN_MIN + WEAR_RUN_SPREAD * _hash01(index, edge * 97 + step * 2 + 1)
		var to := minf(s + run, half_length)
		if to - s > 0.1:
			_add(Kind.EDGE, middle + along * (0.5 * (s + to)),
				Basis(side * radius, along * (to - s), front * radius), worn, WEAR_METALLIC, WEAR_ROUGHNESS)
		s = to + WEAR_GAP_MIN + WEAR_GAP_SPREAD * _hash01(index, edge * 97 + step * 2 + 2)
		step += 1

# An integer hash in 0..1, the same on every machine.
static func _hash01(a: int, b: int) -> float:
	var h := (a * 374761393 + b * 668265263) & 0x7fffffff
	h = ((h ^ (h >> 13)) * 1274126177) & 0x7fffffff
	h = h ^ (h >> 16)
	return float(h & 0xffff) / 65535.0

# One instance of `kind`: a unit mesh carried by `basis` (its columns the
# piece's scaled axes) to `at`.
func _add(kind: int, at: Vector3, basis: Basis, colour: Color, metallic: float, roughness: float) -> void:
	# A MultiMesh cannot flip one instance's winding, so a mirrored basis
	# would draw that piece inside out (a bolt's cap culled, a plate lit from
	# its far side). Every mesh here is symmetric across its x, so turning x
	# round keeps the shape and makes the basis right-handed.
	if basis.determinant() < 0.0:
		basis.x = -basis.x
	var size: float = KIND_CELL[kind]
	var section := _last[kind]
	var local := at - section.origin if section != null else Vector3.INF
	var half := 0.5 * size
	if not (absf(local.x) < half and absf(local.y) < half and absf(local.z) < half):
		var cell := (at / size).floor()
		var key := Vector3i(cell)
		var sections: Dictionary = _sections[kind]
		section = sections.get(key)
		if section == null:
			section = Section.new()
			section.origin = (cell + Vector3(0.5, 0.5, 0.5)) * size
			sections[key] = section
		_last[kind] = section
		local = at - section.origin
	section.data.append_array([
		basis.x.x, basis.y.x, basis.z.x, local.x,
		basis.x.y, basis.y.y, basis.z.y, local.y,
		basis.x.z, basis.y.z, basis.z.z, local.z,
		colour.r, colour.g, colour.b, 1.0,
		metallic, roughness, 0.0, 0.0])

func _emit() -> void:
	var shader := Shader.new()
	shader.code = SHADER_CODE
	var material := ShaderMaterial.new()
	material.shader = shader
	var plate := BoxMesh.new()
	var bolt := CylinderMesh.new()
	bolt.top_radius = 1.0
	bolt.bottom_radius = 1.0
	bolt.height = 1.0
	bolt.radial_segments = 6
	bolt.rings = 0
	bolt.cap_bottom = false
	var edge := CylinderMesh.new()
	edge.top_radius = 1.0
	edge.bottom_radius = 1.0
	edge.height = 1.0
	edge.radial_segments = 5
	edge.rings = 0
	edge.cap_top = false
	edge.cap_bottom = false
	var meshes: Array[Mesh] = [plate, bolt, edge]
	for kind in _sections.size():
		for key: Vector3i in _sections[kind]:
			var section: Section = _sections[kind][key]
			var data := section.data
			var origin := section.origin
			var multimesh := MultiMesh.new()
			multimesh.transform_format = MultiMesh.TRANSFORM_3D
			multimesh.use_colors = true
			multimesh.use_custom_data = true
			multimesh.mesh = meshes[kind]
			multimesh.instance_count = data.size() / FLOATS
			multimesh.buffer = data
			var instance := MultiMeshInstance3D.new()
			instance.name = "Detail%s" % Kind.keys()[kind].capitalize()
			instance.multimesh = multimesh
			instance.material_override = material
			instance.position = origin
			instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			instance.visibility_range_end = KIND_RANGE[kind]
			instance.visibility_range_end_margin = KIND_FADE[kind]
			instance.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
			add_child(instance)
			_instances[kind].append(instance)

# The GRAPHICS preset: LOW keeps only the plates, nearer.
func _process(delta: float) -> void:
	_poll -= delta
	if _poll > 0.0:
		return
	_poll = QUALITY_POLL_S
	var settings: Object = _main.get("_settings")
	if settings == null:
		return
	var quality := int(settings.get("quality"))
	if quality == _quality:
		return
	_quality = quality
	var low := quality == QUALITY_LOW
	for instance: MultiMeshInstance3D in _instances[Kind.PLATE]:
		instance.visibility_range_end = LOW_PLATE_RANGE if low else KIND_RANGE[Kind.PLATE]
	for kind in [Kind.BOLT, Kind.EDGE]:
		for instance: MultiMeshInstance3D in _instances[kind]:
			instance.visible = not low
	var face := _main.get_node_or_null("TowerTwoFace") as GeometryInstance3D
	if face != null and face.material_override is ShaderMaterial:
		(face.material_override as ShaderMaterial).set_shader_parameter("fine", 0.0 if low else 1.0)
