extends "res://presentation/main_world.gd"

# The main scene, layer 3 of 6: the machines. The mechanism kit's bodies
# drawn and moved each frame, the hook's gun and line, rubble, water and
# stones.

const FirstPersonArms := preload("res://presentation/first_person_arms.gd")
const KIT_PART_FLOATS := 12
const KIT_SHAPE_CYLINDER := 1
const KIT_SHAPE_CAPSULE := 2
const KIT_CABLE_SEGMENTS := 4
# Rubble, the native's declared granular model: a mass in each bin, a stream
# while one pours, piles where spills land. Drawn at a loose bulk density; a
# pile is drawn as a low skirt, no taller than a step, since it has no body.
const KIT_BIN_FLOATS := 11
const KIT_POOL_FLOATS := 7
const KIT_SPOUT_FLOATS := 7
const KIT_PILE_FLOATS := 4
const KIT_MAX_PILES := 16
const RUBBLE_DENSITY := 1600.0
const RUBBLE_PILE_HEIGHT := 0.05
# Stones: a course on a bin's fill every RUBBLE_STONE_PITCH metres, a pile's
# spread of RUBBLE_PILE_STONES_PER_M2 out to its radius, and RUBBLE_FALL_STONES
# tumbling down a stream at RUBBLE_FALL_SPEED.
const RUBBLE_STONE_PITCH := 0.24
const RUBBLE_TOP_STONES := 160
const RUBBLE_PILE_STONES := 400
const RUBBLE_PILE_STONES_PER_M2 := 11.0
const RUBBLE_FALL_STONES := 56
const RUBBLE_FALL_SPEED := 6.0

var _native: Object
var _ctx := {}
var _arms: Node3D

var _kit_root: Node3D
var _kit_bodies: Array[Node3D] = []
var _kit_dynamic: Array[bool] = []
# The kit's machines, for the audio: every moving body of MACHINE_MIN_KG or
# more, with the point of it farthest from its origin, whose speed is how fast
# the machine moves.
const MACHINE_MIN_KG := 40.0
var _machines: Array[Dictionary] = []
var _kit_cables: Array = []
# Per bin: the dust thrown up where its stream of stone lands (null for water).
var _kit_bin_dust: Array = []
# The grappling hook's line, from the right hand to the hook, and its head.
var _hook_line: MeshInstance3D
# The line beyond its first bend round the structure (task #102): one
# segment per bend, as many as the native allows (kHookBendsMax).
const HOOK_BENDS_MAX := 8
var _hook_bend_lines: Array[MeshInstance3D] = []
var _hook_head: MeshInstance3D
# The hook's gun, in the right fist once the hook is taken: a compact
# launcher, drawn at HOOK_GUN_SCALE of its modelled size (a pistol-sized
# tool so near the lens reads as a large one).
var _hook_gun: Node3D
const HOOK_GUN_SCALE := 0.62
# The hands on the pitman lift's hand line this frame.
var _hauling := false
# The structure at work: beacons on moving machines, steam from the vents.
var _works: Node3D
# Per bin: the rubble layer on its floor (null for a static bin, whose
# contents cannot be seen), that floor's box, and its stream.
var _kit_bin_layers: Array = []
var _kit_bin_floors: Array = []
var _kit_bin_streams: Array[MeshInstance3D] = []
var _kit_piles: Array[MeshInstance3D] = []
# Rubble drawn as stones (goal phase 7): the top course of each bin's fill,
# the stones tumbling down each stream, and the stones spread where each pile
# landed. Presentation only: the native's declared granular model is unchanged.
var _kit_bin_tops: Array = []            # MultiMeshInstance3D or null, per bin
var _kit_bin_falls: Array[MultiMeshInstance3D] = []
var _kit_pile_stones: Array[MultiMeshInstance3D] = []
# AS-007 water: a body of water per pool, a stream per spout.
var _kit_pools: Array[MeshInstance3D] = []
var _kit_spouts: Array[MeshInstance3D] = []

@onready var _camera: Camera3D = $Camera
var _audio: Node
const KIT_CABLE_RANGE := 200.0

# AS-006: the mechanism kit. Every kit body is declared by the native -- its
# boxes, each box's material class, its pose -- and drawn here from that
# declaration, so what the player sees is exactly what collides; nothing is
# authored twice. Kit bodies live outside $TowerPresentation: they are native
# collision already, never part of the exported solid dressing.
const SUMMIT_FRAME_ENTITY := 1900

func _build_kit() -> void:
	_kit_root = Node3D.new()
	_kit_root.name = "KitPresentation"
	add_child(_kit_root)
	# Indexed by the native's Material enum: steel, rust, timber, concrete,
	# hazard, galvanised, rubble, yellow.
	var palette: Array[Material] = [
		_material(Color("2a2723"), 0.7, 0.55, Color.BLACK, 1.0, _bump_steel),
		_material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _bump_steel),
		_material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _bump_timber),
		_material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _bump_concrete),
		_material(Color("a04d16"), 0.18, 0.76),
		_material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _bump_steel),
		_material(Color("5b544b"), 0.0, 0.98, Color.BLACK, 1.0, _bump_concrete),
		_material(Color("c19a2a"), 0.2, 0.62),
	]
	# Drawn in batches (see _batch_static_dressing): a moving body's parts as
	# one mesh per material, moving with it; the static bodies' parts with
	# the dressing's batcher.
	var statics_batcher := Batcher.new()
	for body in int(_native.get_kit_body_count()):
		var node := Node3D.new()
		node.name = "KitBody%d" % int(_native.get_kit_body_entity_id(body))
		node.transform = _native.get_kit_body_transform(body)
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body)
		var dynamic := bool(_native.is_kit_body_dynamic(body))
		# The summit's frame (pylon, deck, arm) is the thing to see from the
		# ground: drawn like a moving body's parts, one mesh per material, and at
		# any distance, not batched into the statics' near-only batches.
		var far_visible := int(_native.get_kit_body_entity_id(body)) == SUMMIT_FRAME_ENTITY
		var body_groups := {}
		for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
			var mesh: PrimitiveMesh
			match int(parts[p + 11]):
				KIT_SHAPE_CYLINDER:
					var cylinder := CylinderMesh.new()
					cylinder.top_radius = parts[p]
					cylinder.bottom_radius = parts[p]
					cylinder.height = parts[p + 1] * 2.0
					cylinder.radial_segments = 16
					mesh = cylinder
				KIT_SHAPE_CAPSULE:
					# A capsule's half.y runs between its sphere centres.
					var capsule := CapsuleMesh.new()
					capsule.radius = parts[p]
					capsule.height = parts[p + 1] * 2.0 + parts[p] * 2.0
					capsule.radial_segments = 16
					capsule.rings = 4
					mesh = capsule
				_:
					var box := BoxMesh.new()
					box.size = Vector3(parts[p], parts[p + 1], parts[p + 2]) * 2.0
					mesh = box
			var material: Material = palette[clampi(int(parts[p + 10]), 0, palette.size() - 1)]
			var part := Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
				Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			if dynamic or far_visible:
				var key := str(material.get_instance_id())
				Batcher.append(body_groups, key, mesh, 0, part, "", false)
				body_groups[key][1] = material
			else:
				var at: Transform3D = node.transform * part
				var bounds := at * mesh.get_aabb()
				statics_batcher.add(mesh, 0, at, material,
					bounds.size[bounds.get_longest_axis_index()] <= BATCH_FINE_SIZE)
		for key in body_groups:
			var piece := Batcher.instance(body_groups[key][0], body_groups[key][1])
			# A moving part is drawn as far as its size carries: a 1 m part to
			# about 120 m, the 26 m wheel across the whole tower.
			var extent := piece.get_aabb().size
			piece.visibility_range_end = 0.0 if far_visible else \
				clampf(40.0 * maxf(extent.x, maxf(extent.y, extent.z)), 120.0, 1200.0)
			piece.visibility_range_end_margin = BATCH_FADE
			piece.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
			node.add_child(piece)
		_kit_root.add_child(node)
		_kit_bodies.append(node)
		_kit_dynamic.append(bool(_native.is_kit_body_dynamic(body)))
		if bool(_native.is_kit_body_dynamic(body)) and \
				float(_native.get_kit_body_mass(body)) >= MACHINE_MIN_KG:
			_machines.append({"body": body, "tip": _farthest_corner(parts), "last_tip": Vector3.INF,
				"last_at": Vector3.INF, "speed": 0.0, "turning": false, "at": Vector3.ZERO,
				"tip_at": Vector3.ZERO, "mass": float(_native.get_kit_body_mass(body)),
				"top": _beacon_seat(parts)})
	var statics := Node3D.new()
	statics.name = "KitStatics"
	_kit_root.add_child(statics)
	_batch_report += "kit statics %d parts -> %d meshes, %d wet; " % [statics_batcher.parts,
		statics_batcher.emit(statics), statics_batcher.wet_parts]
	var cable := BoxMesh.new()
	cable.size = Vector3(0.035, 0.035, 1.0)
	cable.material = _material(Color("1b1916"), 0.6, 0.5)
	for index in int(_native.get_kit_cable_count()):
		var segments: Array[MeshInstance3D] = []
		for segment in KIT_CABLE_SEGMENTS:
			var instance := MeshInstance3D.new()
			instance.name = "KitCable%d_%d" % [index, segment]
			instance.mesh = cable
			instance.visible = false
			# A rope a few centimetres thick: not drawn from afar.
			instance.visibility_range_end = KIT_CABLE_RANGE
			_kit_root.add_child(instance)
			segments.append(instance)
		_kit_cables.append(segments)
	var stream_mesh := BoxMesh.new()
	stream_mesh.size = Vector3(0.12, 0.12, 1.0)
	stream_mesh.material = palette[6]
	var stone := StandardMaterial3D.new()
	stone.vertex_color_use_as_albedo = true
	stone.roughness = 0.93
	stone.metallic = 0.0
	var rocks: Array[ArrayMesh] = [_rock_mesh(1), _rock_mesh(2), _rock_mesh(3)]
	for rock in rocks:
		rock.surface_set_material(0, stone)
	var water := StandardMaterial3D.new()
	water.albedo_color = Color(0.16, 0.30, 0.33, 0.74)
	water.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	water.metallic = 0.1
	water.roughness = 0.08
	water.cull_mode = BaseMaterial3D.CULL_DISABLED
	var water_stream := BoxMesh.new()
	water_stream.size = Vector3(0.22, 0.22, 1.0)
	water_stream.material = water
	var bins: PackedFloat32Array = _native.get_kit_bins()
	for b in range(0, bins.size() - KIT_BIN_FLOATS + 1, KIT_BIN_FLOATS):
		var index := _kit_bin_streams.size()
		var body := int(bins[b])
		var is_water := bins[b + 10] > 0.5
		var layer: MeshInstance3D = null
		var floor_box := PackedFloat32Array()
		var moving := body >= 0 and body < _kit_dynamic.size() and _kit_dynamic[body]
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body) if moving \
			else PackedFloat32Array()
		if parts.size() >= KIT_PART_FLOATS:
			# The rubble lies on the bin's floor, its first part.
			floor_box = parts.slice(0, KIT_PART_FLOATS)
			var layer_mesh := BoxMesh.new()
			layer_mesh.material = water if is_water else palette[6]
			layer = MeshInstance3D.new()
			layer.name = "KitRubble%d" % index
			layer.mesh = layer_mesh
			layer.visible = false
			_kit_bodies[body].add_child(layer)
		_kit_bin_layers.append(layer)
		_kit_bin_floors.append(floor_box)
		var top: MultiMeshInstance3D = null
		if layer != null and not is_water:
			top = _stone_course(rocks[index % rocks.size()], floor_box, index)
			top.name = "KitRubbleTop%d" % index
			top.visible = false
			_kit_bodies[body].add_child(top)
		_kit_bin_tops.append(top)
		var fall := _stone_multimesh(rocks[(index + 1) % rocks.size()], RUBBLE_FALL_STONES)
		fall.name = "KitStreamStones%d" % index
		fall.visible = false
		_kit_root.add_child(fall)
		_kit_bin_falls.append(fall)
		var stream := MeshInstance3D.new()
		stream.name = "KitStream%d" % index
		stream.mesh = water_stream if is_water else stream_mesh
		stream.visible = false
		_kit_root.add_child(stream)
		_kit_bin_streams.append(stream)
		var dust: CPUParticles3D = null
		if not is_water:
			dust = _stone_dust()
			dust.name = "KitStreamDust%d" % index
			_kit_root.add_child(dust)
		_kit_bin_dust.append(dust)
	var pile_mesh := CylinderMesh.new()
	pile_mesh.top_radius = 0.6
	pile_mesh.bottom_radius = 1.0
	pile_mesh.height = 1.0
	pile_mesh.radial_segments = 20
	pile_mesh.rings = 1
	pile_mesh.material = palette[6]
	for index in KIT_MAX_PILES:
		var pile := MeshInstance3D.new()
		pile.name = "KitPile%d" % index
		pile.mesh = pile_mesh
		pile.visible = false
		_kit_root.add_child(pile)
		_kit_piles.append(pile)
		var spread := _stone_spread(rocks[index % rocks.size()], index)
		spread.name = "KitPileStones%d" % index
		spread.visible = false
		_kit_root.add_child(spread)
		_kit_pile_stones.append(spread)
	var pools: PackedFloat32Array = _native.get_kit_pools()
	for p in range(0, pools.size() - KIT_POOL_FLOATS + 1, KIT_POOL_FLOATS):
		var body_mesh := BoxMesh.new()
		body_mesh.size = Vector3.ONE
		body_mesh.material = water
		var pool := MeshInstance3D.new()
		pool.name = "KitPool%d" % _kit_pools.size()
		pool.mesh = body_mesh
		pool.visible = false
		_kit_root.add_child(pool)
		_kit_pools.append(pool)
	var spouts: PackedFloat32Array = _native.get_kit_spouts()
	for p in range(0, spouts.size() - KIT_SPOUT_FLOATS + 1, KIT_SPOUT_FLOATS):
		var spout := MeshInstance3D.new()
		spout.name = "KitSpout%d" % _kit_spouts.size()
		spout.mesh = water_stream
		spout.visible = false
		_kit_root.add_child(spout)
		_kit_spouts.append(spout)
	_render_kit()

# Where a machine's beacon sits, in its own frame: on the highest corner of
# its highest part, set in a little from the edge. On a roof's corner, not
# its middle, where a rider stands and would have it lit at their feet.
func _beacon_seat(parts: PackedFloat32Array) -> Vector3:
	var best := Vector3.ZERO
	var best_centre := Vector3.ZERO
	var found := false
	for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
		var basis := Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9]))
		var centre := Vector3(parts[p + 3], parts[p + 4], parts[p + 5])
		for corner in 8:
			var flip := Vector3(1.0 if corner & 1 else -1.0, 1.0 if corner & 2 else -1.0,
				1.0 if corner & 4 else -1.0)
			var at := centre + basis * (flip * Vector3(parts[p], parts[p + 1], parts[p + 2]))
			# The first corner within a centimetre of the top wins: the same
			# seat every run.
			if not found or at.y > best.y + 0.01:
				best = at
				best_centre = centre
				found = true
	if not found:
		return Vector3.ZERO
	var inward := Vector2(best_centre.x - best.x, best_centre.z - best.z)
	if inward.length() > 0.001:
		inward = inward.normalized() * minf(0.15, inward.length() * 0.5)
	return Vector3(best.x + inward.x, best.y, best.z + inward.y)

# The corner of a body's parts farthest from its origin, in its own frame.
func _farthest_corner(parts: PackedFloat32Array) -> Vector3:
	var best := Vector3.ZERO
	for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
		var basis := Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9]))
		var centre := Vector3(parts[p + 3], parts[p + 4], parts[p + 5])
		for corner in 8:
			var flip := Vector3(1.0 if corner & 1 else -1.0, 1.0 if corner & 2 else -1.0,
				1.0 if corner & 4 else -1.0)
			var at := centre + basis * (flip * Vector3(parts[p], parts[p + 1], parts[p + 2]))
			if at.length() > best.length():
				best = at
	return best

# How fast each machine moves this frame, from the poses _render_kit just set:
# its farthest point's speed, and whether it turns about its origin (a hinge)
# rather than moving with it. The audio hears it from that.
func _sample_machines(delta: float) -> void:
	if delta <= 0.0 or _machines.is_empty():
		return
	for machine in _machines:
		var node: Node3D = _kit_bodies[int(machine["body"])]
		var tip: Vector3 = node.transform * (machine["tip"] as Vector3)
		var at: Vector3 = node.transform.origin
		var last_tip: Vector3 = machine["last_tip"]
		var last_at: Vector3 = machine["last_at"]
		var speed := 0.0
		var origin_speed := 0.0
		if last_tip != Vector3.INF and node.visible:
			speed = tip.distance_to(last_tip) / delta
			origin_speed = at.distance_to(last_at) / delta
		machine["speed"] = speed
		machine["turning"] = speed > 2.0 * origin_speed + 0.05
		machine["at"] = at
		machine["tip_at"] = tip
		machine["last_tip"] = tip
		machine["last_at"] = at
	_audio.update_machines(delta, _machines)
	if _works != null:
		_works.update(delta, _camera.global_position, _machines)

func _build_hook_gun() -> Node3D:
	var gun := Node3D.new()
	gun.name = "HookGun"
	gun.visible = false
	var steel := _material(Color("2c2f33"), 0.7, 0.45)
	var grip := _material(Color("1a1612"), 0.1, 0.85)
	var yellow := _material(Color("d9a514"), 0.3, 0.5)
	# Along the hand's fingers (+y local) the barrel runs forward; the body
	# sits over the fist, the reel drum under it, the hook's claw at the muzzle.
	var parts := [
		[_box_mesh(Vector3(0.07, 0.26, 0.09)), steel, Vector3(0.0, 0.07, 0.07)],
		[_box_mesh(Vector3(0.05, 0.1, 0.06)), grip, Vector3(0.0, 0.0, 0.0)],
		[_cylinder_mesh(0.024, 0.2), steel, Vector3(0.0, 0.27, 0.085)],
		[_cylinder_mesh(0.045, 0.05), yellow, Vector3(0.0, 0.06, -0.01)],
		[_box_mesh(Vector3(0.06, 0.035, 0.025)), yellow, Vector3(0.0, 0.385, 0.085)],
	]
	for part in parts:
		var instance := MeshInstance3D.new()
		instance.mesh = part[0]
		instance.material_override = part[1]
		instance.position = part[2]
		if part[0] is CylinderMesh and part[2].y > 0.1:
			pass   # the barrel's axis is +y already
		elif part[0] is CylinderMesh:
			instance.rotation = Vector3(0.0, 0.0, PI * 0.5)   # the drum's axis across the hand
		gun.add_child(instance)
	return gun

func _build_hook_view() -> void:
	var line := BoxMesh.new()
	line.size = Vector3(0.008, 0.008, 1.0)
	line.material = _material(Color("1b1916"), 0.6, 0.5)
	_hook_line = MeshInstance3D.new()
	_hook_line.name = "HookLine"
	_hook_line.mesh = line
	_hook_line.visible = false
	add_child(_hook_line)
	for i in HOOK_BENDS_MAX:
		var bent := MeshInstance3D.new()
		bent.name = "HookLineBent%d" % i
		bent.mesh = line
		bent.visible = false
		add_child(bent)
		_hook_bend_lines.append(bent)
	var head := BoxMesh.new()
	head.size = Vector3(0.12, 0.12, 0.12)
	head.material = _material(Color("303033"), 0.5, 0.8)
	_hook_head = MeshInstance3D.new()
	_hook_head.name = "HookHead"
	_hook_head.mesh = head
	_hook_head.visible = false
	add_child(_hook_head)
	_hook_gun = _build_hook_gun()
	add_child(_hook_gun)

# Where a line the right hand holds runs to: the grappling hook when it is
# out, or, hauling the pitman's hand line, straight up the line to the head
# sheave; null when the hand holds no line.
func _hand_line_end() -> Variant:
	if int(_ctx.get("hook_state", 0)) != 0:
		# The hand holds the line toward where it goes: its first bend round
		# the structure, or the hook.
		var hook: Dictionary = _native.get_hook_state()
		var points: PackedVector3Array = hook.get("points", PackedVector3Array())
		return points[1] if points.size() >= 3 else hook["position"]
	if _hauling:
		return _camera.global_position + Vector3(0.0, 9.0, 0.0) - _camera.global_transform.basis.z * 0.5
	return null

# The hook in flight or bitten: its line runs from the right hand to it.
# The arms are posed earlier in the frame, so the wrist is this frame's.
func _render_hook() -> void:
	if _hook_line == null:
		return
	var hook: Dictionary = _native.get_hook_state()
	var out := int(hook["state"]) != 0
	_hook_head.visible = out
	# The gun rides in the right fist once taken; hauling the pitman's line
	# the fist holds the line instead.
	_hook_gun.visible = bool(hook["has_hook"]) and not _hauling
	if _hook_gun.visible:
		_hook_gun.global_transform = _arms.right_hand_transform().scaled_local(Vector3.ONE * HOOK_GUN_SCALE)
	var end: Variant = _hand_line_end()
	for bent in _hook_bend_lines:
		bent.visible = false
	if end == null:
		_hook_line.visible = false
		return
	var at: Vector3 = end
	# Out of the gun's muzzle when the gun is in the fist; else out of the
	# right fist, closed on the line (FirstPersonArms POSE_LINE): its wrist, a
	# palm's length along the line.
	var wrist: Vector3 = _arms.wrist_positions()[0]
	var hand := wrist + (at - wrist).normalized() * FirstPersonArms.LINE_GRIP_REACH
	if _hook_gun.visible:
		hand = _hook_gun.global_transform * Vector3(0.0, 0.4, 0.085)
	_lay_segment(_hook_line, hand, at)
	# On from the first bend, bend to bend, to the hook (the native's points:
	# chest, bends nearest first, hook).
	var points: PackedVector3Array = hook.get("points", PackedVector3Array())
	for i in range(1, points.size() - 1):
		if i - 1 < _hook_bend_lines.size():
			_lay_segment(_hook_bend_lines[i - 1], points[i], points[i + 1])
	_hook_head.global_position = hook["position"] if out else at

# Poses every moving kit body and lays every cable through its points, from
# this frame's native state. A body the native has taken out of the world (a
# shackle hooked onto an anchor) is hidden, not moved.
func _render_kit() -> void:
	if _kit_root == null:
		return
	for body in _kit_bodies.size():
		if not _kit_dynamic[body]:
			continue
		var node: Node3D = _kit_bodies[body]
		node.visible = bool(_native.is_kit_body_enabled(body))
		if node.visible:
			node.transform = _native.get_kit_body_transform(body)
	for index in _kit_cables.size():
		var points: PackedVector3Array = _native.get_kit_cable_points(index)
		var segments: Array = _kit_cables[index]
		for segment in segments.size():
			var instance: MeshInstance3D = segments[segment]
			if segment + 1 < points.size():
				_lay_segment(instance, points[segment], points[segment + 1])
			else:
				instance.visible = false
	_render_rubble()

# Rubble from the native's bins and piles: a layer on each moving bin's floor
# as deep as its contents would lie, a stream while a bin pours, and a low
# skirt of spilled rubble where each pile landed.
func _render_rubble() -> void:
	var bins: PackedFloat32Array = _native.get_kit_bins()
	var pours: Array = []
	for index in _kit_bin_streams.size():
		var b := index * KIT_BIN_FLOATS
		if b + KIT_BIN_FLOATS > bins.size():
			break
		var layer: MeshInstance3D = _kit_bin_layers[index]
		if layer != null:
			var floor_box: PackedFloat32Array = _kit_bin_floors[index]
			var half_x := maxf(floor_box[0] - 0.06, 0.05)
			var half_z := maxf(floor_box[2] - 0.06, 0.05)
			var density := 1000.0 if bins[b + 10] > 0.5 else RUBBLE_DENSITY
			var depth := bins[b + 1] / density / (4.0 * half_x * half_z)
			layer.visible = bins[b + 1] >= 1.0
			if layer.visible:
				var rotation := Basis(Quaternion(floor_box[6], floor_box[7], floor_box[8],
					floor_box[9]))
				var on_floor := Vector3(floor_box[3], floor_box[4], floor_box[5]) \
					+ rotation * Vector3(0.0, floor_box[1] + depth * 0.5, 0.0)
				layer.transform = Transform3D(
					rotation * Basis.from_scale(Vector3(half_x * 2.0, depth, half_z * 2.0)),
					on_floor)
			var top: MultiMeshInstance3D = _kit_bin_tops[index]
			if top != null:
				top.visible = layer.visible
				if top.visible:
					var floor_rotation := Basis(Quaternion(floor_box[6], floor_box[7], floor_box[8],
						floor_box[9]))
					top.transform = Transform3D(floor_rotation, Vector3(floor_box[3], floor_box[4], floor_box[5]) \
						+ floor_rotation * Vector3(0.0, floor_box[1] + depth, 0.0))
		var stream: MeshInstance3D = _kit_bin_streams[index]
		var fall: MultiMeshInstance3D = _kit_bin_falls[index]
		var dust: CPUParticles3D = _kit_bin_dust[index]
		if bins[b + 3] > 0.5:
			var from := Vector3(bins[b + 4], bins[b + 5], bins[b + 6])
			var to := Vector3(bins[b + 7], bins[b + 8], bins[b + 9])
			_lay_segment(stream, from, to)
			fall.visible = bins[b + 10] < 0.5
			if fall.visible:
				_tumble_stones(fall, from, to, index)
				pours.append([(from + to) * 0.5, from.distance_to(to)])
			if dust != null:
				dust.global_position = to
				dust.emitting = fall.visible
		else:
			stream.visible = false
			fall.visible = false
			if dust != null:
				dust.emitting = false
	var piles: PackedFloat32Array = _native.get_kit_piles()
	for index in _kit_piles.size():
		var pile: MeshInstance3D = _kit_piles[index]
		var p := index * KIT_PILE_FLOATS
		pile.visible = p + KIT_PILE_FLOATS <= piles.size() and piles[p + 3] >= 1.0
		if pile.visible:
			# A frustum of this height and base radius r holds about
			# 0.65 * PI * r^2 * height.
			var volume := piles[p + 3] / RUBBLE_DENSITY
			var radius := sqrt(volume / (0.65 * PI * RUBBLE_PILE_HEIGHT))
			pile.transform = Transform3D(
				Basis.from_scale(Vector3(radius, RUBBLE_PILE_HEIGHT, radius)),
				Vector3(piles[p], piles[p + 1] + RUBBLE_PILE_HEIGHT * 0.5, piles[p + 2]))
		var spread: MultiMeshInstance3D = _kit_pile_stones[index]
		spread.visible = pile.visible
		if spread.visible:
			var reach := sqrt(piles[p + 3] / RUBBLE_DENSITY / (0.65 * PI * RUBBLE_PILE_HEIGHT))
			spread.multimesh.visible_instance_count = mini(RUBBLE_PILE_STONES,
				int(RUBBLE_PILE_STONES_PER_M2 * PI * reach * reach))
			spread.transform = Transform3D(Basis.IDENTITY, Vector3(piles[p], piles[p + 1], piles[p + 2]))
	_audio.update_pours(pours, _camera.global_position)
	_render_water()

# Water (the native's declared model): each pool drawn from its floor to its
# level, and a stream from each spout pouring this frame to where it lands.
func _render_water() -> void:
	var pools: PackedFloat32Array = _native.get_kit_pools()
	for index in _kit_pools.size():
		var p := index * KIT_POOL_FLOATS
		var pool: MeshInstance3D = _kit_pools[index]
		if p + KIT_POOL_FLOATS > pools.size():
			pool.visible = false
			continue
		var low := Vector3(pools[p], pools[p + 1], pools[p + 2])
		var high := Vector3(pools[p + 3], pools[p + 6], pools[p + 5])
		pool.visible = high.y - low.y > 0.02
		if pool.visible:
			pool.transform = Transform3D(Basis.from_scale(high - low), (low + high) * 0.5)
	var spouts: PackedFloat32Array = _native.get_kit_spouts()
	for index in _kit_spouts.size():
		var p := index * KIT_SPOUT_FLOATS
		var spout: MeshInstance3D = _kit_spouts[index]
		if p + KIT_SPOUT_FLOATS <= spouts.size() and spouts[p] > 0.5:
			_lay_segment(spout, Vector3(spouts[p + 1], spouts[p + 2], spouts[p + 3]),
				Vector3(spouts[p + 4], spouts[p + 5], spouts[p + 6]))
		else:
			spout.visible = false

# An irregular stone about 0.2 m across: an icosahedron split once, each
# vertex pushed in or out, flattened a little; faceted.
static func _rock_mesh(variant: int) -> ArrayMesh:
	var t := (1.0 + sqrt(5.0)) * 0.5
	var corners: Array[Vector3] = [Vector3(-1, t, 0), Vector3(1, t, 0), Vector3(-1, -t, 0), Vector3(1, -t, 0),
		Vector3(0, -1, t), Vector3(0, 1, t), Vector3(0, -1, -t), Vector3(0, 1, -t),
		Vector3(t, 0, -1), Vector3(t, 0, 1), Vector3(-t, 0, -1), Vector3(-t, 0, 1)]
	var faces := [[0, 11, 5], [0, 5, 1], [0, 1, 7], [0, 7, 10], [0, 10, 11], [1, 5, 9], [5, 11, 4],
		[11, 10, 2], [10, 7, 6], [7, 1, 8], [3, 9, 4], [3, 4, 2], [3, 2, 6], [3, 6, 8], [3, 8, 9],
		[4, 9, 5], [2, 4, 11], [6, 2, 10], [8, 6, 7], [9, 8, 1]]
	var shape := func(v: Vector3) -> Vector3:
		var n := v.normalized()
		var key := int(round(n.x * 97.0)) * 7919 + int(round(n.y * 97.0)) * 104729 + int(round(n.z * 97.0))
		var r := 0.1 * (0.78 + 0.44 * _hash01(key, variant))
		return Vector3(n.x * r, n.y * r * 0.72, n.z * r * 0.9)
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for f in faces:
		var a: Vector3 = corners[f[0]]
		var b: Vector3 = corners[f[1]]
		var c: Vector3 = corners[f[2]]
		var ab := (a + b) * 0.5
		var bc := (b + c) * 0.5
		var ca := (c + a) * 0.5
		for tri in [[a, ab, ca], [ab, b, bc], [ca, bc, c], [ab, bc, ca]]:
			for v in tri:
				st.add_vertex(shape.call(v))
	st.generate_normals()
	return st.commit()

static func _stone_multimesh(rock: ArrayMesh, count: int) -> MultiMeshInstance3D:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = true
	mm.mesh = rock
	mm.instance_count = count
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = mm
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return instance

# A stone's tint: the rubble's grey-brown, each a little different.
static func _stone_tint(i: int, j: int) -> Color:
	var k := _hash01(i, j + 31)
	return Color(0.42, 0.39, 0.35).lerp(Color(0.58, 0.54, 0.47), k).darkened(0.25 * _hash01(i, j + 57))

static func _stone_basis(i: int, j: int, size: float) -> Basis:
	return Basis.from_euler(Vector3(_hash01(i, j + 1) * TAU, _hash01(i, j + 2) * TAU, _hash01(i, j + 3) * TAU)) \
		.scaled(Vector3.ONE * size * (0.7 + 0.6 * _hash01(i, j + 4)))

# The top course of a bin's fill: stones on a grid over its floor, in the
# floor's frame, sitting on the fill's surface (y 0).
func _stone_course(rock: ArrayMesh, floor_box: PackedFloat32Array, key: int) -> MultiMeshInstance3D:
	var half_x := maxf(floor_box[0] - 0.12, 0.05)
	var half_z := maxf(floor_box[2] - 0.12, 0.05)
	var nx := maxi(1, int(2.0 * half_x / RUBBLE_STONE_PITCH))
	var nz := maxi(1, int(2.0 * half_z / RUBBLE_STONE_PITCH))
	var count := mini(RUBBLE_TOP_STONES, nx * nz)
	var instance := _stone_multimesh(rock, count)
	for k in count:
		var gx := k % nx
		var gz := int(k / nx)
		var x := -half_x + (float(gx) + 0.5 + 0.5 * (_hash01(key * 131 + k, 5) - 0.5)) * 2.0 * half_x / float(nx)
		var z := -half_z + (float(gz) + 0.5 + 0.5 * (_hash01(key * 131 + k, 6) - 0.5)) * 2.0 * half_z / float(nz)
		instance.multimesh.set_instance_transform(k, Transform3D(_stone_basis(key * 131 + k, 0, 1.2),
			Vector3(x, 0.02 * _hash01(key * 131 + k, 7), z)))
		instance.multimesh.set_instance_color(k, _stone_tint(key * 131 + k, 0))
	return instance

# A pile's spread: stones scattered out from its centre, nearest first, so
# showing the first n fills a disc that grows with the pile.
func _stone_spread(rock: ArrayMesh, key: int) -> MultiMeshInstance3D:
	var instance := _stone_multimesh(rock, RUBBLE_PILE_STONES)
	var max_reach := sqrt(float(RUBBLE_PILE_STONES) / (RUBBLE_PILE_STONES_PER_M2 * PI))
	for k in RUBBLE_PILE_STONES:
		var r := max_reach * sqrt((float(k) + _hash01(key * 977 + k, 8)) / float(RUBBLE_PILE_STONES))
		var a := TAU * _hash01(key * 977 + k, 9)
		instance.multimesh.set_instance_transform(k, Transform3D(_stone_basis(key * 977 + k, 0, 1.1),
			Vector3(r * cos(a), 0.05 + 0.04 * _hash01(key * 977 + k, 10), r * sin(a))))
		instance.multimesh.set_instance_color(k, _stone_tint(key * 977 + k, 1))
	instance.multimesh.visible_instance_count = 0
	return instance

# Stones tumbling down a pouring stream, spread along it and turning as they fall.
# Dust where poured stone lands: soft brown-grey puffs kicked up and out,
# settling as they fade.
func _stone_dust() -> CPUParticles3D:
	var dust := CPUParticles3D.new()
	dust.amount = 18
	dust.lifetime = 1.6
	dust.emitting = false
	dust.local_coords = false
	dust.direction = Vector3.UP
	dust.spread = 75.0
	dust.initial_velocity_min = 0.6
	dust.initial_velocity_max = 1.6
	dust.gravity = Vector3(0.0, -0.4, 0.0)
	dust.damping_min = 0.8
	dust.damping_max = 1.4
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.4))
	grow.add_point(Vector2(1.0, 1.6))
	dust.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, Color(0.55, 0.5, 0.44, 0.45))
	fade.set_color(1, Color(0.55, 0.5, 0.44, 0.0))
	dust.color_ramp = fade
	var quad := QuadMesh.new()
	quad.size = Vector2(0.9, 0.9)
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	material.vertex_color_use_as_albedo = true
	material.albedo_texture = _soft_puff()
	quad.material = material
	dust.mesh = quad
	return dust

func _tumble_stones(fall: MultiMeshInstance3D, from: Vector3, to: Vector3, key: int) -> void:
	var length := from.distance_to(to)
	if length < 0.05:
		fall.visible = false
		return
	var now := float(Time.get_ticks_msec()) * 0.001
	var along := (to - from) / length
	var side := (Vector3.RIGHT if absf(along.x) < 0.9 else Vector3.FORWARD).cross(along).normalized()
	var other := along.cross(side)
	fall.transform = Transform3D.IDENTITY
	# Each stone falls the stream's length under gravity, leaving the spout at
	# RUBBLE_FALL_SPEED: it speeds up as it goes, so the stones string out
	# toward the bottom, and the stream spreads as stones jostle apart.
	var leave := RUBBLE_FALL_SPEED * 0.25
	var drop_s := (-leave + sqrt(leave * leave + 2.0 * 9.81 * length)) / 9.81
	for k in RUBBLE_FALL_STONES:
		var t := fposmod(float(k) / float(RUBBLE_FALL_STONES) + now / maxf(drop_s, 0.1), 1.0) * drop_s
		var phase := clampf((leave * t + 0.5 * 9.81 * t * t) / length, 0.0, 1.0)
		var spread := 0.1 + 0.32 * phase
		var offset := side * (spread * (_hash01(key * 53 + k, 11) - 0.5) * 2.0) + \
			other * (spread * (_hash01(key * 53 + k, 12) - 0.5) * 2.0)
		var spin := Basis(Vector3(_hash01(key, k), 1.0, _hash01(k, key)).normalized(), now * (3.0 + 4.0 * _hash01(key + k, 13)))
		fall.multimesh.set_instance_transform(k, Transform3D(spin * _stone_basis(key * 53 + k, 0, 1.0),
			from + along * (length * phase) + offset))
		fall.multimesh.set_instance_color(k, _stone_tint(key * 53 + k, 2))
