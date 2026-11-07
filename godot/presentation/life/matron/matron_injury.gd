extends Node3D
## J-044i, first slice: per-region injury rendering (red mark, bruise, swelling).
##
## Input is the per-region injury dictionary (J-044a §D/E), region -> dose:
##   {"face_l": {"j_region": 150.0, "j_event": 60.0}, ...}
## The live feed is J-044g's: the view passes data["injury"] each frame, and
## until g lands that key is absent and nothing shows. Tests drive it directly.
## Thresholds are the J-044a calibration, J_REF = 50 N·s (one solid punch):
## red mark at J_event >= 25, bruise at J_region >= 100, swelling at >= 200.
## Injuries never heal within a run: a smaller later value never lowers a
## region (Cory's no-heal ruling). Cuts and blood need J-044d's surface
## feature and come in the next slice; blood stays with the gore system.
## Presentation only: nothing here decides anything, it shows what the sim did.
##
## The mark is a flat quad with a soft-edged radial texture, 4 mm off the skin
## at the region's anchor, facing out along the anchor's outward normal (the
## skull's is the back of the head). (Decals were tried first
## and render nothing here: two rotations, plain wall, verified texture, zero pixels.)

const J_REF := 50.0
const MARK_EVENT := 25.0
const BRUISE_REGION := 100.0
const SWELL_REGION := 200.0
# First slice: chest, neck and head only (J-044 plan). [bone, local offset,
# faces_back]: the skull anchor sits on the back of the head, so its quad
# faces the bone's -Z; the rest face +Z. The skull -0.106 is measured: the
# scalp there is ~0.10 from the bone origin, so -0.08 buried the quad
# inside the head and no skull injury could show. Lateral sign: the head
# bone's local +X is her left (right-handed basis, local +Z out of the
# face), so face_l takes +0.055 and face_r -0.055; the signs were swapped
# before 2026-11-07 and a left-cheek punch bruised her right cheek.
# Verified by rendered stills: a face_l-only dose must mark the cheek on
# the image-right side of a front shot (her left, facing the camera).
const ANCHORS := {
	"face_l": ["head", Vector3(0.055, -0.015, 0.085), false], "face_r": ["head", Vector3(-0.055, -0.015, 0.085), false],
	"jaw": ["jaw", Vector3(0.0, -0.02, 0.08), false], "skull": ["head", Vector3(0.0, 0.06, -0.106), true],
	"neck": ["neck_hi", Vector3(0.0, 0.0, 0.09), false], "chest": ["chest", Vector3(0.0, 0.05, 0.16), false],
}
const MARK_COLOR := Color(0.55, 0.12, 0.10)
const BRUISE_COLOR := Color(0.23, 0.08, 0.20)
const SWELL_COLOR := Color(0.35, 0.10, 0.14)

var _seen := {}  # region -> {"j_region": float, "j_event": float}, maxima only
var _patches := {}  # region -> MeshInstance3D
var _texture: Texture2D = null
var _mesh: QuadMesh = null
var _shown := true


func _ready() -> void:
	_texture = _bruise_texture()
	_mesh = QuadMesh.new()
	_mesh.size = Vector2(2.0, 2.0)


func _hash(x: int, y: int) -> float:
	var h: int = (x * 374761393 + y * 668265263) & 0x7fffffff
	h = ((h ^ (h >> 13)) * 1274126177) & 0x7fffffff
	return float((h ^ (h >> 16)) & 0x7fffffff) / 2147483647.0


func _bruise_texture() -> Texture2D:
	var size := 128
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	for y in size:
		for x in size:
			var u := (float(x) / float(size - 1)) * 2.0 - 1.0
			var v := (float(y) / float(size - 1)) * 2.0 - 1.0
			var r := Vector2(u, v).length()
			var wobble := 0.22 * _hash(int(u * 7.0) + 5, int(v * 7.0) - 3)
			var a := clampf(1.0 - smoothstep(0.45, 0.95, r * (1.0 + wobble)), 0.0, 1.0)
			img.set_pixel(x, y, Color(1, 1, 1, a * 0.92))
	return ImageTexture.create_from_image(img)


func set_shown(shown: bool) -> void:
	_shown = shown
	if not shown:
		for p in _patches.values():
			(p as MeshInstance3D).visible = false


func severity(region: String) -> float:
	return float((_seen.get(region, {}) as Dictionary).get("j_region", 0.0))


func mark_visible(region: String) -> bool:
	var d: Dictionary = _seen.get(region, {})
	return float(d.get("j_event", 0.0)) >= MARK_EVENT


func bruise_visible(region: String) -> bool:
	return severity(region) >= BRUISE_REGION


func swelling_visible(region: String) -> bool:
	return severity(region) >= SWELL_REGION


func visible_regions() -> Array:
	var out: Array = []
	for region in _seen:
		if mark_visible(region) or bruise_visible(region):
			out.append(region)
	return out


func patch_visible(region: String) -> bool:
	var p := _patches.get(region) as MeshInstance3D
	return p != null and p.visible


func patch_outward(region: String, center: Vector3) -> float:
	# How far the patch faces away from center: positive means the quad's
	# +Z points out of the body (Claude's #280 probe). -2.0 when no patch.
	var p := _patches.get(region) as MeshInstance3D
	if p == null:
		return -2.0
	return (p.global_position - center).dot(p.global_transform.basis.z.normalized())


func _patch_for(region: String) -> MeshInstance3D:
	if _patches.has(region):
		return _patches[region]
	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_texture = _texture
	mat.roughness = 0.6
	var p := MeshInstance3D.new()
	p.mesh = _mesh
	p.material_override = mat
	p.visible = false
	add_child(p)
	_patches[region] = p
	return p


func update(injury: Dictionary, rig: Variant) -> void:
	_shown = true
	for region in injury:
		if not ANCHORS.has(region):
			continue
		var dose: Dictionary = injury[region]
		var o: Dictionary = _seen.get(region, {})
		_seen[region] = {"j_region": maxf(float(dose.get("j_region", 0.0)), float(o.get("j_region", 0.0))),
			"j_event": maxf(float(dose.get("j_event", 0.0)), float(o.get("j_event", 0.0)))}
	if rig == null:
		return
	for region in _seen:
		var p := _patch_for(region)
		var anchor: Array = ANCHORS[region]
		var bone: Transform3D = rig.bone_world(String(anchor[0]))
		var sev := clampf((severity(region) - BRUISE_REGION) / BRUISE_REGION, 0.0, 1.0)
		var swelling := swelling_visible(region)
		var rad := (0.04 + 0.025 * sev + (0.02 if swelling else 0.0)) * 0.5
		var mat := p.material_override as StandardMaterial3D
		if bruise_visible(region):
			mat.albedo_color = Color(BRUISE_COLOR, 0.5 + 0.25 * sev) if not swelling else Color(SWELL_COLOR, 0.7)
		elif mark_visible(region):
			mat.albedo_color = Color(MARK_COLOR, 0.3)
			rad *= 0.7
		else:
			p.visible = false
			continue
		# Flat quad 4 mm off the skin, facing out along the anchor's outward
		# normal: the skull anchor sits on the back of the head, so its quad
		# flips (negating X and Z keeps the basis right-handed) and the
		# nudge goes the other way, or it faces into the skull and back-face
		# culling hides it. Cheek quads tilt about the bone Y axis so the
		# normal follows the anchor's radial direction, atan2(|x|, z) off
		# +Z (~33 deg for the cheek anchors), toward her left for face_l
		# and her right for face_r: flat +Z left the outer edge floating
		# past the face edge straight on (Claude #306 eye-check follow-up).
		# Basis() takes column vectors; scaled() scales rows and shears it.
		var faces_back := bool(anchor[2])
		var tilt := 0.0
		if region == "face_l" or region == "face_r":
			var aoff: Vector3 = anchor[1]
			tilt = atan2(absf(aoff.x), aoff.z)
			if aoff.x < 0.0:
				tilt = -tilt
		var tc := cos(tilt)
		var ts := sin(tilt)
		var bx := (bone.basis.x * tc - bone.basis.z * ts) * rad
		var bz := bone.basis.x * ts + bone.basis.z * tc
		var nudge := 0.004
		if faces_back:
			bx = -bx
			bz = -bz
			nudge = -nudge
		var at: Vector3 = (anchor[1] as Vector3) + Vector3(0.0, 0.0, nudge)
		p.global_transform = Transform3D(
			Basis(bx, bone.basis.y * rad, bz),
			bone.origin + bone.basis * at)
		p.visible = _shown
