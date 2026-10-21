extends "res://presentation/main_world.gd"

# The main scene, layer 3 of 6: the machines. The mechanism kit's bodies
# drawn and moved each frame, the hook's gun and line, rubble, water and
# stones.

const FirstPersonArms := preload("res://presentation/first_person_arms.gd")
const FirstPersonBody := preload("res://presentation/first_person_body.gd")
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
var _body: Node3D

var _kit_root: Node3D
var _kit_bodies: Array[Node3D] = []
var _kit_dynamic: Array[bool] = []
# The kit's machines, for the audio: every moving body of MACHINE_MIN_KG or
# more, with the point of it farthest from its origin, whose speed is how fast
# the machine moves.
const MACHINE_MIN_KG := 40.0
# The kit bodies sounded by their own parts: each forklift's frame and fork
# carriage (sim/forklift.hpp: the yard's, the crane's, the 11 m and 44 m
# decks') and the yard crane's jib, trolley, hook block, shackle, lifting
# magnet and container spreader (sim/yard_crane.hpp).
const OWN_SOUND_ENTITIES := [2982, 2983, 2989, 2990, 2991, 2992, 2993, 2994, 2984, 2985, 2986, 2987, 2995, 2996]
# Loose stock (kit entities from here, src/sim/loose_stock.hpp) is not a
# machine: crates and drums lying about neither hum nor carry beacons.
const STOCK_FIRST_ENTITY := 5000
const STOCK_ENTITY_LIMIT := 6000
# Fabricated objects (src/sim/fabricated.hpp) are loose stock too, from a
# range of their own; fabricated_view.gd draws them.
const FABRICATED_FIRST_ENTITY := 0x20000
const FABRICATED_ENTITY_LIMIT := 0x20000 + 32
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
# The yard crane's static bodies (sim/yard_crane.hpp): its mast, its remote's
# cabinet and the three docks on the stack's west face.
const YARD_CRANE_STATIC_FIRST := 1700
const YARD_CRANE_STATIC_LAST := 1704

func _build_kit() -> void:
	_kit_root = Node3D.new()
	_kit_root.name = "KitPresentation"
	add_child(_kit_root)
	# Indexed by the native's Material enum: steel, rust, timber, concrete,
	# hazard, galvanised, rubble, yellow, glass. The steel and the galvanized
	# are smooth (a machine's frame, a tank, a rail); only the rust is pitted.
	var palette: Array[Material] = [
		_material(Color("2a2723"), 0.7, 0.55, Color.BLACK, 1.0, _bump_smooth),
		_material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _bump_rust),
		_material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _bump_timber),
		_material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _bump_concrete),
		_material(Color("a04d16"), 0.18, 0.76),
		_material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _bump_smooth),
		_material(Color("5b544b"), 0.0, 0.98, Color.BLACK, 1.0, _bump_concrete),
		_material(Color("c19a2a"), 0.2, 0.62),
		_glass_material(),
	]
	const KIT_GLASS := 8
	# The static glass (AS-007 E's shaft) under its own key: one transparent
	# draw for all of it, never in the opaque batches.
	var glass_groups := {}
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
		var body_entity := int(_native.get_kit_body_entity_id(body))
		# So is the yard crane's mast, and the docks it lands loads on.
		var far_visible := body_entity == SUMMIT_FRAME_ENTITY or \
			(body_entity >= YARD_CRANE_STATIC_FIRST and body_entity <= YARD_CRANE_STATIC_LAST)
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
			if not dynamic and int(parts[p + 10]) == KIT_GLASS:
				Batcher.append(glass_groups, "glass", mesh, 0, node.transform * part, "", false)
			elif dynamic or far_visible:
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
		var entity := int(_native.get_kit_body_entity_id(body))
		var stock := (entity >= STOCK_FIRST_ENTITY and entity < STOCK_ENTITY_LIMIT) or \
			(entity >= FABRICATED_FIRST_ENTITY and entity < FABRICATED_ENTITY_LIMIT)
		if bool(_native.is_kit_body_dynamic(body)) and not stock and \
				float(_native.get_kit_body_mass(body)) >= MACHINE_MIN_KG:
			# The forklifts and the yard crane are heard from their own parts
			# (audio_director update_forklift, update_crane), not as a groan.
			_machines.append({"heard": not (entity in OWN_SOUND_ENTITIES), "body": body,
				"tip": _farthest_corner(parts), "last_tip": Vector3.INF,
				"last_at": Vector3.INF, "speed": 0.0, "turning": false, "at": Vector3.ZERO,
				"tip_at": Vector3.ZERO, "mass": float(_native.get_kit_body_mass(body)),
				"top": _beacon_seat(parts)})
	var statics := Node3D.new()
	statics.name = "KitStatics"
	_kit_root.add_child(statics)
	if glass_groups.has("glass"):
		var glass := Batcher.instance(glass_groups["glass"][0], palette[KIT_GLASS])
		glass.name = "KitGlass"
		glass.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		glass.visibility_range_end = BATCH_NEAR_RANGE
		statics.add_child(glass)
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
	_build_wet_e()
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

# ---- AS-007 E, the chiller drop (03_EXECUTION/CLIMB_REDESIGN/chiller_drop_spec.md) ----
# Glass in its shaft's walls, two work lamps and the drips riding with its
# car, and the vent collar at the shaft's mouth: its slotted band, and the air
# it lets out seen as it goes, a fast blast of spray, mist and grit when the
# seal breaks, then mist hanging over the mouth. All read from the native
# (get_wet_state, get_kit_vents); nothing here moves anything.
const WET_E_CAB_ENTITY := 2040
const WET_E_DECK_ENTITY := 2045
const WET_E_SHAFT_LOW := Vector3(13.0, 249.0, -136.65)
const WET_E_SHAFT_HIGH := Vector3(13.0, 300.0, -136.65)
const WET_E_NEAR_M := 60.0
const KIT_VENT_FLOATS := 7
# The vent's air, m^3/s at atmospheric, that reads as a full blast.
const VENT_FULL_M3S := 12.0
# How long the burst lasts once the seal breaks, and the flow under which the
# collar is taken as shut again.
const VENT_BURST_S := 1.0
const VENT_SHUT_M3S := 0.3
var _wet_e := {}   # get_wet_state() this frame while near E's shaft, else empty
var _wet_e_lamps: Array[OmniLight3D] = []
var _wet_e_drips: CPUParticles3D
var _vents: Array[Dictionary] = []
# Seals broken and their bursts shown, and how far the eye was from the last
# (the owner, 2026-10-20: "Make sure we see a whoosh of air/wind when vacuum
# seal is broken"; touch_wet holds it to that).
var vent_bursts := 0
var vent_burst_eye_m := INF

func _glass_material() -> StandardMaterial3D:
	# Clear glass with a green-grey cast; a steel mullion every 3 m, drawn by
	# the material's own texture (triplanar, one tile a 3 m square), not by
	# parts. Drawn after the opaque world, both faces.
	const SIZE := 64
	var image := Image.create(SIZE, SIZE, false, Image.FORMAT_RGBA8)
	for y in SIZE:
		for x in SIZE:
			var mullion := x < 3 or y < 2
			image.set_pixel(x, y, Color(0.17, 0.17, 0.16, 1.0) if mullion else Color(0.62, 0.70, 0.66, 0.24))
	image.generate_mipmaps()
	var material := StandardMaterial3D.new()
	material.albedo_texture = ImageTexture.create_from_image(image)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	material.metallic = 0.0
	material.roughness = 0.08
	material.uv1_triplanar = true
	material.uv1_scale = Vector3.ONE / 3.0
	if _bump_smooth != null:
		# The band's wet streaks run down the panes as ripples.
		material.normal_enabled = true
		material.normal_texture = _bump_smooth
		material.normal_scale = 0.4
	return material

# The lamps and the drips, on the car's own nodes, so they ride with it.
func _build_wet_e() -> void:
	var cab := int(_native.get_kit_body_index(WET_E_CAB_ENTITY))
	var deck := int(_native.get_kit_body_index(WET_E_DECK_ENTITY))
	if cab < 0 or cab >= _kit_bodies.size() or deck < 0 or deck >= _kit_bodies.size():
		return
	for side in [-1.0, 1.0]:
		# Warm work lamps on the east and west rails' tops, lighting the wet
		# walls as the car climbs.
		var lamp := OmniLight3D.new()
		lamp.name = "WetELamp"
		lamp.light_color = Color(1.0, 0.78, 0.5)
		lamp.light_energy = 1.6
		lamp.omni_range = 6.0
		lamp.shadow_enabled = false
		lamp.position = Vector3(side * 1.2, 1.1, 0.0)
		lamp.visible = false
		_kit_bodies[cab].add_child(lamp)
		_wet_e_lamps.append(lamp)
		var housing := MeshInstance3D.new()
		housing.mesh = _box_mesh(Vector3(0.14, 0.1, 0.12))
		housing.material_override = _material(Color("e0b040"), 0.2, 0.5, Color(1.0, 0.8, 0.5), 2.0)
		housing.position = Vector3(side * 1.27, 1.06, 0.0)
		_kit_bodies[cab].add_child(housing)
	# Water running off the deck's edges while it moves.
	var drips := CPUParticles3D.new()
	drips.name = "WetEDrips"
	drips.amount = 40
	drips.lifetime = 1.1
	drips.emitting = false
	drips.local_coords = false
	drips.emission_shape = CPUParticles3D.EMISSION_SHAPE_RING
	drips.emission_ring_axis = Vector3.UP
	drips.emission_ring_radius = 1.3
	drips.emission_ring_inner_radius = 1.15
	drips.emission_ring_height = 0.02
	drips.position = Vector3(0.0, -0.08, 0.0)
	drips.direction = Vector3.DOWN
	drips.spread = 6.0
	drips.initial_velocity_min = 0.2
	drips.initial_velocity_max = 0.8
	drips.gravity = Vector3(0.0, -9.8, 0.0)
	var drop := QuadMesh.new()
	drop.size = Vector2(0.015, 0.09)
	var wet := StandardMaterial3D.new()
	wet.albedo_color = Color(0.7, 0.8, 0.85, 0.55)
	wet.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	wet.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	wet.billboard_mode = BaseMaterial3D.BILLBOARD_FIXED_Y
	drop.material = wet
	drips.mesh = drop
	_kit_bodies[deck].add_child(drips)
	_wet_e_drips = drips
	_build_vents()

# Each vent collar: its slotted band, and its air.
func _build_vents() -> void:
	var vents: PackedFloat32Array = _native.get_kit_vents()
	for v in range(0, vents.size() - KIT_VENT_FLOATS + 1, KIT_VENT_FLOATS):
		var low := Vector3(vents[v], vents[v + 4], vents[v + 1])
		var high := Vector3(vents[v + 2], vents[v + 5], vents[v + 3])
		var root := Node3D.new()
		root.name = "KitVent%d" % _vents.size()
		_kit_root.add_child(root)
		var centre := (low + high) * 0.5
		var size := high - low
		# The collar: a slotted band on the inside of each wall, as deep as
		# the slots run.
		var band := _material(Color("3a3632"), 0.6, 0.55)
		band.albedo_texture = _slot_texture()
		band.uv1_triplanar = true
		band.uv1_scale = Vector3.ONE * 0.5
		var faces := [
			[Vector3(centre.x, centre.y, low.z + 0.02), Vector3(size.x, size.y, 0.04), Vector3(0, 0, -1)],
			[Vector3(centre.x, centre.y, high.z - 0.02), Vector3(size.x, size.y, 0.04), Vector3(0, 0, 1)],
			[Vector3(low.x + 0.02, centre.y, centre.z), Vector3(0.04, size.y, size.z), Vector3(-1, 0, 0)],
			[Vector3(high.x - 0.02, centre.y, centre.z), Vector3(0.04, size.y, size.z), Vector3(1, 0, 0)],
		]
		var sheets: Array[CPUParticles3D] = []
		var bursts: Array[CPUParticles3D] = []
		for face in faces:
			var slab := MeshInstance3D.new()
			slab.mesh = _box_mesh(face[1])
			slab.material_override = band
			slab.position = face[0]
			root.add_child(slab)
			# Air out of the slots: a sheet of spray off the wall's top edge,
			# up and out over the rim, as long and thick as the flow.
			var along: Vector3 = face[1]
			var top := Vector3(face[0].x, high.y + 0.05, face[0].z)
			var sheet := _spray(along, face[2], 28, 1.4, false)
			sheet.position = top
			root.add_child(sheet)
			sheets.append(sheet)
			# The burst when the seal breaks: everything at once, fast.
			var burst := _spray(along, face[2], 36, 0.9, true)
			burst.position = top
			root.add_child(burst)
			bursts.append(burst)
		var grit := _grit(Vector3(size.x, 0.1, size.z))
		grit.position = Vector3(centre.x, high.y + 0.1, centre.z)
		root.add_child(grit)
		var mist := _mist(Vector3(size.x, 0.2, size.z))
		mist.position = Vector3(centre.x, high.y + 0.6, centre.z)
		root.add_child(mist)
		_vents.append({"root": root, "sheets": sheets, "bursts": bursts, "grit": grit, "mist": mist,
			"centre": centre, "flowing": false, "burst_left": 0.0})

# Steel with a row of dark slots, for the vent collar's band.
func _slot_texture() -> ImageTexture:
	const SIZE := 32
	var image := Image.create(SIZE, SIZE, false, Image.FORMAT_RGBA8)
	for y in SIZE:
		for x in SIZE:
			var slot := (x % 8) >= 2 and (x % 8) <= 5 and y >= 6 and y <= 26
			image.set_pixel(x, y, Color(0.05, 0.05, 0.05) if slot else Color(1.0, 1.0, 1.0))
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)

# Spray off a slotted edge `along` long, out along `out`: soft white puffs
# blown up and out. A burst is one shot, all at once.
func _spray(along: Vector3, out: Vector3, amount: int, lifetime: float, burst: bool) -> CPUParticles3D:
	var spray := CPUParticles3D.new()
	spray.amount = amount
	spray.lifetime = lifetime
	spray.emitting = false
	spray.one_shot = burst
	spray.explosiveness = 0.85 if burst else 0.0
	spray.local_coords = false
	spray.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	spray.emission_box_extents = Vector3(maxf(along.x * 0.5, 0.02), 0.02, maxf(along.z * 0.5, 0.02))
	spray.direction = (out * 0.8 + Vector3.UP).normalized()
	spray.spread = 28.0 if burst else 18.0
	spray.initial_velocity_min = 7.0 if burst else 2.0
	spray.initial_velocity_max = 13.0 if burst else 5.0
	spray.gravity = Vector3(0.0, -3.0, 0.0)
	spray.damping_min = 2.0
	spray.damping_max = 4.0
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.3))
	grow.add_point(Vector2(1.0, 1.8 if burst else 1.4))
	spray.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, Color(0.92, 0.95, 0.97, 0.75 if burst else 0.5))
	fade.set_color(1, Color(0.92, 0.95, 0.97, 0.0))
	spray.color_ramp = fade
	var quad := QuadMesh.new()
	quad.size = Vector2(0.6, 0.6) if burst else Vector2(0.4, 0.4)
	quad.material = _puff_material()
	spray.mesh = quad
	return spray

# Rust and grit blown out of the slots with the first of the air: small,
# dark, fast, opaque, falling back.
func _grit(extent: Vector3) -> CPUParticles3D:
	var grit := CPUParticles3D.new()
	grit.amount = 40
	grit.lifetime = 1.6
	grit.one_shot = true
	grit.explosiveness = 0.9
	grit.emitting = false
	grit.local_coords = false
	grit.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	grit.emission_box_extents = extent * 0.5
	grit.direction = Vector3.UP
	grit.spread = 55.0
	grit.initial_velocity_min = 5.0
	grit.initial_velocity_max = 11.0
	grit.gravity = Vector3(0.0, -9.8, 0.0)
	grit.angular_velocity_min = -360.0
	grit.angular_velocity_max = 360.0
	var chip := QuadMesh.new()
	chip.size = Vector2(0.035, 0.035)
	var dark := StandardMaterial3D.new()
	dark.albedo_color = Color(0.22, 0.17, 0.13)
	dark.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	dark.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	chip.material = dark
	grit.mesh = chip
	return grit

# Mist that hangs over the mouth after the air has gone, and fades.
func _mist(extent: Vector3) -> CPUParticles3D:
	var mist := CPUParticles3D.new()
	mist.amount = 16
	mist.lifetime = 4.5
	mist.emitting = false
	mist.local_coords = false
	mist.emission_shape = CPUParticles3D.EMISSION_SHAPE_BOX
	mist.emission_box_extents = extent * 0.5
	mist.direction = Vector3.UP
	mist.spread = 80.0
	mist.initial_velocity_min = 0.2
	mist.initial_velocity_max = 0.7
	mist.gravity = Vector3(0.0, 0.05, 0.0)
	mist.damping_min = 0.3
	mist.damping_max = 0.6
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.6))
	grow.add_point(Vector2(1.0, 2.2))
	mist.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, Color(0.88, 0.91, 0.93, 0.35))
	fade.set_color(1, Color(0.88, 0.91, 0.93, 0.0))
	mist.color_ramp = fade
	var quad := QuadMesh.new()
	quad.size = Vector2(1.4, 1.4)
	quad.material = _puff_material()
	mist.mesh = quad
	return mist

func _puff_material() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	material.vertex_color_use_as_albedo = true
	material.albedo_texture = _soft_puff()
	return material

# Once a frame: E's state while the camera is near its shaft, the lamps and
# drips on the car, the vents' air.
func _sample_wet_e(delta: float) -> void:
	var at := _camera.global_position
	var axis := Geometry3D.get_closest_point_to_segment(at, WET_E_SHAFT_LOW, WET_E_SHAFT_HIGH)
	var near := at.distance_to(axis) <= WET_E_NEAR_M
	_wet_e = _native.get_wet_state() if near else {}
	for lamp in _wet_e_lamps:
		lamp.visible = near
	if _wet_e_drips != null:
		_wet_e_drips.emitting = near and absf(float(_wet_e.get("e_cab_speed", 0.0))) > 0.3
	_render_vents(delta, near)
	if near:
		var lever := Vector3(10.0, 305.3, -134.8)
		var cab := int(_native.get_kit_body_index(WET_E_CAB_ENTITY))
		var floor_at: Vector3 = (_native.get_kit_body_transform(cab) as Transform3D) * Vector3(0.0, -1.0, 0.0)
		var mouth := (WET_E_SHAFT_HIGH + Vector3(0.0, -2.0, 0.0))
		_audio.update_wet_e(delta, _wet_e, floor_at, mouth, lever)

# The vents' air: the burst for VENT_BURST_S once a vent opens, by its flow;
# the spray sheets while it flows; the mist hanging on after.
func _render_vents(delta: float, near: bool) -> void:
	if _vents.is_empty():
		return
	var vents: PackedFloat32Array = _native.get_kit_vents() if near else PackedFloat32Array()
	for index in _vents.size():
		var vent: Dictionary = _vents[index]
		var p := index * KIT_VENT_FLOATS
		var flow := absf(vents[p + 6]) if p + KIT_VENT_FLOATS <= vents.size() else 0.0
		var strength := clampf(flow / VENT_FULL_M3S, 0.0, 1.0)
		var flowing := flow > VENT_SHUT_M3S
		if flowing and not bool(vent["flowing"]):
			# The seal breaks: everything out of the slots at once.
			vent["burst_left"] = VENT_BURST_S
			vent_bursts += 1
			vent_burst_eye_m = _camera.global_position.distance_to(vent["centre"])
			for burst: CPUParticles3D in vent["bursts"]:
				burst.initial_velocity_min = lerpf(4.0, 9.0, strength)
				burst.initial_velocity_max = lerpf(7.0, 15.0, strength)
				burst.restart()
			(vent["grit"] as CPUParticles3D).restart()
		vent["flowing"] = flowing
		vent["burst_left"] = maxf(0.0, float(vent["burst_left"]) - delta)
		var bursting := float(vent["burst_left"]) > 0.0
		for sheet: CPUParticles3D in vent["sheets"]:
			sheet.emitting = flowing
			# Longest and thickest while the burst lasts, then as the flow.
			var push := strength * (1.6 if bursting else 1.0)
			sheet.initial_velocity_min = 1.5 + 4.0 * push
			sheet.initial_velocity_max = 3.0 + 7.0 * push
			sheet.speed_scale = 0.7 + 0.6 * clampf(push, 0.0, 1.0)
		(vent["mist"] as CPUParticles3D).emitting = flowing or bursting
