extends RefCounted

# Every body the player can see is a body the player can hit. The world's
# dressing -- braces, footings, halls, rails, trees, gearing -- is authored
# here in GDScript alongside the native mirrors, so this exporter turns it
# into src/sim/world_solids.inc, which the native simulation compiles in as
# static collision. Native tests therefore run against exactly the solid
# world the player walks, and CI regenerates the file and fails on drift.
#
#   godot --headless --path godot -- --export-solids=<path>
#
# Every part name must be classified below. An unclassified part fails the
# export: new geometry never ships passable by default. Static mirrors of
# native bodies may be listed as SOLID safely -- the native loader skips any
# box whose world bounds coincide with a body it already owns.

# Driven at runtime by native state or ambient motion. A static collider at
# the build pose would be a phantom wall once the part moves. Rotating gears
# are covered instead by the rotation-invariant disc on their pivot.
const MOVING := [
	"HoistScoop", "Ballast", "TipperBeam", "TipperBallast", "ValveArm", "ValveWeight",
	"LiftPlatform", "Counterweight", "TreadlePlate", "TreadleWeight", "JibBoomBeam",
	"JibHook", "KernelCrate", "CapacityStandLoad", "NeedleBeam", "SumpGrate", "IntakeBelt",
	"BeltSlat", "DogPlate", "DogRib", "BoomSpine", "BoomLattice", "BoomTail", "BoomBallast",
	"IntakeHook", "IntakePack", "PackBand", "PackBandUpper", "OverweightPack",
	"SwingFlightSlab", "SwingTread", "SwingRail", "CwCradleCar",
	"NativeMovingLedge", "NativeRotatingSupport", "NativeTranslatingSupport",
	# Yard crane boom: ambient sway, 25 m up a bare mast nothing can climb.
	"CraneBoomArm", "CraneCounterArm", "CraneCounterweight", "CraneCrate",
	# Gear parts: their pivot carries a solid_disc instead.
	"GearHub", "GearSpoke", "GearRim", "GearTooth", "GearMotifHub",
]

# Not rigid bodies a person collides with: paint, stripes and flush plates on
# a native surface (a solid lip there would snag every step), light glow,
# cloth, rope and cable, low scrub, water, sky.
const NON_SOLID := [
	"DeckPlank", "DeckStreak", "StairTread", "Tread", "SkinRungPlate", "LaneStripe",
	"EdgeStripe", "FloorLight", "FloorLightHigh", "ColumnLichen", "UpperLightBand",
	"StandingWater", "StandingWaterTwo", "BannerMark", "SignPlateMark", "Banner",
	"Rope", "LiftRope", "JibRope", "JibSling", "JibHoistCable", "NeedleHoistCable",
	"TreadleCableA", "TreadleCableB", "Span", "CraneCable", "RopeFlightSide",
	"RopeCradleSide", "ScrubLobe", "Waterfall",
]

# Native bodies drawn one-for-one. Listed so the exporter never relies on
# the coincidence check for the surfaces the player walks on.
const MIRROR := [
	"Grade", "StackDeck", "StackColumn", "StairFlight", "StairLanding", "IntakeHandoff",
	"SkinRung", "BayBack", "BaySide", "BayJamb", "BayLintel", "MidLanding",
	"UpperFlightSlab", "HallDeckNorth", "HallDeckSouth", "HallDeckEast", "HallDeckWest",
	"SkinWalkway", "KernelDeck", "NativeVaultRail", "NativeMantleLedge", "NativeHangLedge",
	"NativeBlockedLedge", "NativeBlockedCanopy", "NativeCrawlBeam", "NativeCrawlPost", "Catwalk",
	"Vessel", "SumpApproachDeck", "SumpFarDeck",
]

const CYLINDER_SEGMENTS := 12
const DISC_SEGMENTS := 20
const MIN_HALF_EXTENT := 0.005


# Returns {"boxes": [{position, rotation, half, part}...], "hulls": [{part, points}...],
# "unknown": [part names], "counts": {...}}.
static func collect(world: Node3D) -> Dictionary:
	var boxes: Array = []
	var hulls: Array = []
	var unknown := {}
	var counts := {"solid": 0, "moving": 0, "non_solid": 0, "mirror": 0, "disc": 0}
	_walk(world, boxes, hulls, unknown, counts)
	return {"boxes": boxes, "hulls": hulls, "unknown": unknown.keys(), "counts": counts}


static func _walk(node: Node, boxes: Array, hulls: Array, unknown: Dictionary,
		counts: Dictionary) -> void:
	for child in node.get_children():
		if child is Node3D and child.has_meta(&"solid_disc"):
			var disc: Vector2 = child.get_meta(&"solid_disc")
			hulls.append({"part": "%s(disc)" % str(child.get_meta(&"part", child.name)),
				"points": _disc_points((child as Node3D).global_transform, disc.x, disc.y)})
			counts["disc"] += 1
		if child is MeshInstance3D and child.has_meta(&"part"):
			_classify(child, boxes, hulls, unknown, counts)
		_walk(child, boxes, hulls, unknown, counts)


static func _classify(mesh_node: MeshInstance3D, boxes: Array, hulls: Array,
		unknown: Dictionary, counts: Dictionary) -> void:
	var part := _base_name(str(mesh_node.get_meta(&"part")))
	if part in MOVING:
		counts["moving"] += 1
		return
	if part in NON_SOLID:
		counts["non_solid"] += 1
		return
	if part in MIRROR:
		counts["mirror"] += 1
		return
	if not (part in SOLID):
		unknown[part] = true
		return
	var xform := mesh_node.global_transform
	var mesh := mesh_node.mesh
	if mesh is BoxMesh:
		var entry := _box_entry(xform, (mesh as BoxMesh).size)
		if entry.is_empty():
			hulls.append({"part": part, "points": _box_points(xform, (mesh as BoxMesh).size)})
		else:
			entry["part"] = part
			boxes.append(entry)
	elif mesh is CylinderMesh:
		var cylinder := mesh as CylinderMesh
		hulls.append({"part": part, "points": _cylinder_points(xform, cylinder.top_radius,
			cylinder.bottom_radius, cylinder.height)})
	elif mesh is SphereMesh:
		var sphere := mesh as SphereMesh
		hulls.append({"part": part, "points": _sphere_points(xform, sphere.radius,
			sphere.height * 0.5)})
	else:
		unknown["%s(%s)" % [part, mesh.get_class()]] = true
		return
	counts["solid"] += 1


# "SkinRung12" and "StairRail-1" are one part each.
static func _base_name(part: String) -> String:
	var regex := RegEx.create_from_string("-?\\d+$")
	return regex.sub(part, "")


# An oriented box when the node's basis is orthogonal (rotation times axis
# scale); empty when it is sheared, in which case the caller falls back to
# the hull of its eight corners.
static func _box_entry(xform: Transform3D, size: Vector3) -> Dictionary:
	var basis := xform.basis
	var x := basis.x
	var y := basis.y
	var z := basis.z
	var tolerance := 1.0e-3 * maxf(x.length(), maxf(y.length(), z.length()))
	if absf(x.dot(y)) > tolerance or absf(y.dot(z)) > tolerance or absf(x.dot(z)) > tolerance:
		return {}
	var half := Vector3(size.x * x.length(), size.y * y.length(), size.z * z.length()) * 0.5
	half = Vector3(maxf(half.x, MIN_HALF_EXTENT), maxf(half.y, MIN_HALF_EXTENT),
		maxf(half.z, MIN_HALF_EXTENT))
	var rotation := basis.orthonormalized()
	if rotation.determinant() < 0.0:
		rotation.x = -rotation.x
	return {"position": xform.origin, "rotation": rotation.get_rotation_quaternion(), "half": half}


static func _box_points(xform: Transform3D, size: Vector3) -> Array:
	var points := []
	for sx in [-0.5, 0.5]:
		for sy in [-0.5, 0.5]:
			for sz in [-0.5, 0.5]:
				points.append(xform * Vector3(size.x * sx, size.y * sy, size.z * sz))
	return points


# CylinderMesh is centred on its origin along local Y; a cone is a cylinder
# with a zero top radius.
static func _cylinder_points(xform: Transform3D, top: float, bottom: float, height: float) -> Array:
	var points := []
	for index in CYLINDER_SEGMENTS:
		var angle := TAU * float(index) / float(CYLINDER_SEGMENTS)
		var ring := Vector3(cos(angle), 0.0, sin(angle))
		if bottom > 0.0:
			points.append(xform * (ring * bottom + Vector3(0.0, -height * 0.5, 0.0)))
		if top > 0.0:
			points.append(xform * (ring * top + Vector3(0.0, height * 0.5, 0.0)))
	if bottom <= 0.0:
		points.append(xform * Vector3(0.0, -height * 0.5, 0.0))
	if top <= 0.0:
		points.append(xform * Vector3(0.0, height * 0.5, 0.0))
	return points


static func _sphere_points(xform: Transform3D, radius: float, half_height: float) -> Array:
	var points := [xform * Vector3(0.0, half_height, 0.0), xform * Vector3(0.0, -half_height, 0.0)]
	for ring in range(1, 6):
		var polar := PI * float(ring) / 6.0
		for index in 10:
			var azimuth := TAU * float(index) / 10.0
			points.append(xform * Vector3(sin(polar) * cos(azimuth) * radius,
				cos(polar) * half_height, sin(polar) * sin(azimuth) * radius))
	return points


# A gear's swept volume about its own Z axis: identical at every angle it
# turns through, so a static collider stays true while the mesh rotates.
static func _disc_points(xform: Transform3D, radius: float, depth: float) -> Array:
	var points := []
	for index in DISC_SEGMENTS:
		var angle := TAU * float(index) / float(DISC_SEGMENTS)
		var rim := Vector3(cos(angle) * radius, sin(angle) * radius, 0.0)
		points.append(xform * (rim + Vector3(0.0, 0.0, depth * 0.5)))
		points.append(xform * (rim - Vector3(0.0, 0.0, depth * 0.5)))
	return points


static func write_cpp(collected: Dictionary, path: String) -> Error:
	var lines := PackedStringArray()
	lines.append("// GENERATED by godot/presentation/solid_export.gd (--export-solids). Do not edit:")
	lines.append("// change the builders in main.gd and regenerate. CI fails when this drifts.")
	lines.append("// %d boxes, %d hulls." % [collected["boxes"].size(), collected["hulls"].size()])
	lines.append("")
	lines.append("constexpr WorldSolidBox kWorldSolidBoxes[] = {")
	for box in collected["boxes"]:
		var p: Vector3 = box["position"]
		var q: Quaternion = box["rotation"]
		var h: Vector3 = box["half"]
		lines.append("    {%s, %s, %s, %s, %s, %s, %s, %s, %s, %s},  // %s" % [
			_f(p.x), _f(p.y), _f(p.z), _f(q.x), _f(q.y), _f(q.z), _f(q.w),
			_f(h.x), _f(h.y), _f(h.z), box["part"]])
	lines.append("};")
	lines.append("")
	lines.append("constexpr float kWorldSolidHullPoints[] = {")
	var hull_ranges := []
	var cursor := 0
	for hull in collected["hulls"]:
		var points: Array = hull["points"]
		lines.append("    // %s" % hull["part"])
		var row := PackedStringArray()
		for point in points:
			var v: Vector3 = point
			row.append("%s, %s, %s," % [_f(v.x), _f(v.y), _f(v.z)])
		lines.append("    " + " ".join(row))
		hull_ranges.append([cursor, points.size(), hull["part"]])
		cursor += points.size()
	lines.append("};")
	lines.append("")
	lines.append("constexpr WorldSolidHull kWorldSolidHulls[] = {")
	for hull_range in hull_ranges:
		lines.append("    {%d, %d},  // %s" % [hull_range[0], hull_range[1], hull_range[2]])
	lines.append("};")
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		return FileAccess.get_open_error()
	file.store_string("\n".join(lines) + "\n")
	return OK


# Four decimals is 0.1 mm, well inside Jolt's collision tolerance, and keeps
# the file byte-stable across runs.
static func _f(value: float) -> String:
	var text := "%.4f" % value
	if text == "-0.0000":
		text = "0.0000"
	return text + "F"


const SOLID := [
	# Stack frame dressing and its halls, gearing, lifts, jibs, bridges.
	"ShaftEdgeBeam", "OuterEdgeBeam", "ShaftRail", "ShaftPost", "StackBrace", "StairStringer",
	"Buttress", "ButtressFoot", "ButtressTie", "MachineHall", "HallCapping", "HallSill",
	"HallWindow", "HallWindowFront", "HallRib", "GearShaft", "WinchDrum", "DrumCable",
	"DrumHousing", "CageRoof", "CagePost", "CageLamp", "CageFloor", "CageBack", "LiftGuide",
	"LiftHead", "JibMast", "JibChord", "JibWeb", "JibStay", "JibBackStay", "JibBlock", "JibLoad",
	"JibLoadBand", "SignPlate", "BannerRod", "BannerBacking", "LiftSignBacking", "BridgeDeck",
	"BridgeChord", "BridgeWeb", "BridgeRail", "BridgePost", "BridgePylon", "Riser",
	"RiserFlange", "VentStack", "LampFitting", "VerdigrisPipe", "VerdigrisFlange", "ValveHousing",
	"ValveWheel", "UpperDeck", "UpperColumn", "UpperBrace", "TimberCladding", "TowerMass",
	"FacePier", "FaceBand", "FaceDuct", "FaceDrum", "WordmarkBacking", "ChevronUpper",
	"ChevronSpine", "ChevronLower",
	# Yard, plant and kernels.
	"LampMast", "LampHead", "KerbRun", "YardCrate", "CraneMast", "TipperPylon", "ValvePylon",
	"HoistMast", "LiftMast", "AccessStepOne", "AccessStepTwo", "AccessLanding", "ReturnBasin",
	"BasinGuard", "CatwalkRail", "SheaveHead", "TreadlePylon", "TreadleSheaveMast",
	"ValveSheaveMast", "JibMast", "CapacityStandMast", "NeedlePierApproachMain",
	"NeedlePierApproachNotch", "NeedlePierFarMain", "NeedlePierFarNotch", "NeedleHoistMast",
	"NeedleHoistHead", "SumpApproachLeg", "SumpFarLeg",
	# AS-001 / AS-002 dressing around the native route.
	"StairRail", "SkinStay", "MastBrace", "PendantCatwalk", "PendantRamp", "PendantStand",
	"PendantBox", "YardJibMast", "OverweightMast", "BayPlate", "CradleGuideMast", "SheaveA",
	"SheaveB",
	# Landscape.
	"TreeTrunk", "TreeCanopy", "MountainPeak", "MountainSnowCap", "MountainRidgeFar",
	"MountainRidgeFarSnow", "ValleyFloor", "RimRidge", "RimRidgeSnow",
]
