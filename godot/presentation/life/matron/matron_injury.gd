extends Node3D
## J-059 adds the black eye: eye_l/eye_r regions, a ring hugging the eye
## socket (darkest lower lid + inner corner, darkening over ~15 s, then
## holding); the upper lid swells at 150 N·s, the eye is a slit at 250 N·s.
##
## Input is the per-region injury dictionary (J-044a §D/E), region -> dose:
##   {"face_l": {"j_region": 150.0, "j_event": 60.0}, ...}
## The live feed is J-044g's: the view passes data["injury"] each frame, and
## until g lands that key is absent and nothing shows. Tests drive it directly.
## Thresholds are the J-044a calibration, J_REF = 50 N·s (one solid punch):
## red mark at J_event >= 25, bruise at J_region >= 100, swelling at >= 200.
## Eye thresholds are J-059's: the first visible eye injury is the black-eye
## ring at J_region >= 75 (1.5 J_ref); the lid swells at >= 150, the eye is
## a slit at >= 250. The generic 25 N·s red mark does not render on the eye
## (issue #307 tests: below 75, nothing on the eye).
## Injuries never heal within a run: a smaller later value never lowers a
## region (Cory's no-heal ruling). Cuts and blood need J-044d's surface
## feature and come in the next slice; blood stays with the gore system.
## Presentation only: nothing here decides anything, it shows what the sim did.
##
## The mark is a flat quad with a soft-edged radial texture, 4 mm off the skin
## at the region's anchor, facing out along the anchor's outward normal (the
## skull's is the back of the head). (Decals were tried first
## and render nothing here: two rotations, plain wall, verified texture, zero pixels.)
## The black eye is a ring-textured quad, not a solid one: the annulus has a
## transparent centre so the eyeball shows through, and it is centred on the
## measured eye socket, so it hugs the socket instead of overhanging the face
## edge the way the flat cheek quad did (#306). The lid is a second quad, a
## soft band over the upper eye that droops with dose.
##
## The eye socket centres are the eye bones' rest offsets in head-local space,
## measured 2026-11-07: the bone called eye_l sits at head-local -X and the
## bone called eye_r at +X — the rig's eye bone NAMES are mirrored against
## the face-anchor convention (head-local +X is her left, verified by the
## #306 stills). The anchors below follow anatomy, not the bone names; the
## eye_sides check in injury_shots_test.gd guards the signs.

const J_REF := 50.0
const MARK_EVENT := 25.0
const BRUISE_REGION := 100.0
const SWELL_REGION := 200.0
# J-059 black eye: ring at 75, lid swells at 150, slit at 250; the ring
# darkens over 15 s after the hit. A fist contact within 0.035 m of a
# socket centre goes to that eye instead of face_l/face_r.
const BLACK_EYE_REGION := 75.0
const LID_SWELL_REGION := 150.0
const EYE_SLIT_REGION := 250.0
const BLACK_EYE_DARKEN_S := 15.0
const EYE_CLASSIFY_M := 0.035
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
# The eye anchors are the measured socket centres (probe 2026-11-07): eye_l
# +0.0315 (her left), eye_r -0.0315 (her right).
const ANCHORS := {
	"face_l": ["head", Vector3(0.055, -0.015, 0.085), false], "face_r": ["head", Vector3(-0.055, -0.015, 0.085), false],
	"jaw": ["jaw", Vector3(0.0, -0.02, 0.08), false], "skull": ["head", Vector3(0.0, 0.06, -0.106), true],
	"neck": ["neck_hi", Vector3(0.0, 0.0, 0.09), false], "chest": ["chest", Vector3(0.0, 0.05, 0.16), false],
	"eye_l": ["head", Vector3(0.0315, 0.0505, 0.0847), false], "eye_r": ["head", Vector3(-0.0315, 0.0505, 0.0847), false],
}
const MARK_COLOR := Color(0.55, 0.12, 0.10)
const BRUISE_COLOR := Color(0.23, 0.08, 0.20)
const SWELL_COLOR := Color(0.35, 0.10, 0.14)
const RING_BASE := Color(0.18, 0.05, 0.13)
const RING_DARK := Color(0.05, 0.01, 0.05)

var _seen := {}  # region -> {"j_region": float, "j_event": float, "t_hit": float}, maxima only
var _patches := {}  # region -> MeshInstance3D
var _lids := {}  # eye region -> MeshInstance3D (swollen upper lid)
var _texture: Texture2D = null
var _ring_textures := {}  # false/true (mirrored) -> Texture2D
var _lid_texture: Texture2D = null
var _mesh: QuadMesh = null
var _shown := true
var _clock := 0.0  # view seconds; the black eye darkens over 15 s after the hit


func _ready() -> void:
	_texture = _bruise_texture()
	_ring_textures[false] = _ring_texture(false)
	_ring_textures[true] = _ring_texture(true)
	_lid_texture = _lid_band_texture()
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


func _ring_texture(mirror: bool) -> Texture2D:
	# Annulus with a transparent centre (the eyeball shows through), baked
	# dark red-purple, darkest on the lower lid and the inner corner. The
	# quad samples the texture with u mirrored and v as-is (verified by
	# stills): the dark lower-lid weight sits at image-top, the dark inner
	# weight at image-right for eye_l; eye_r mirrors u. Colour is baked in;
	# the material's albedo alpha ramps the darkness over the 15 s pool.
	var size := 128
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	for y in size:
		for x in size:
			var u := (float(x) / float(size - 1)) * 2.0 - 1.0
			var v := (float(y) / float(size - 1)) * 2.0 - 1.0
			if mirror:
				u = -u
			var wobble := 0.10 * (_hash(int(u * 7.0) + 11, int(v * 7.0) - 7) - 0.5)
			var r := Vector2(u, v).length() * (1.0 + wobble)
			var shape := clampf(smoothstep(0.50, 0.62, r), 0.0, 1.0) * (1.0 - clampf(smoothstep(0.88, 0.96, r), 0.0, 1.0))
			var bottom := clampf((-v - 0.2) / 0.6, 0.0, 1.0)
			var inner := clampf((u - 0.2) / 0.6, 0.0, 1.0)
			var w := clampf(0.55 * bottom + 0.45 * inner, 0.0, 1.0)
			var col := RING_BASE.lerp(RING_DARK, w)
			img.set_pixel(x, y, Color(col.r, col.g, col.b, shape * (0.75 + 0.25 * w)))
	return ImageTexture.create_from_image(img)


func _lid_band_texture() -> Texture2D:
	# Soft horizontal band: the swollen upper lid. Colour is baked in;
	# the material's albedo alpha follows the droop.
	var size := 128
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	for y in size:
		for x in size:
			var u := (float(x) / float(size - 1)) * 2.0 - 1.0
			var v := (float(y) / float(size - 1)) * 2.0 - 1.0
			var a := (1.0 - clampf(smoothstep(0.45, 0.85, absf(v)), 0.0, 1.0)) * (1.0 - clampf(smoothstep(0.80, 0.98, absf(u)), 0.0, 1.0))
			img.set_pixel(x, y, Color(SWELL_COLOR.r, SWELL_COLOR.g, SWELL_COLOR.b, a * 0.9))
	return ImageTexture.create_from_image(img)


func set_shown(shown: bool) -> void:
	_shown = shown
	if not shown:
		for p in _patches.values():
			(p as MeshInstance3D).visible = false
		for p in _lids.values():
			(p as MeshInstance3D).visible = false


func is_eye_region(region: String) -> bool:
	return region == "eye_l" or region == "eye_r"


func classify_head_hit(local: Vector3) -> String:
	# Head-local contact on the front of the head -> region. Within
	# EYE_CLASSIFY_M of a socket centre it goes to that eye instead of the
	# cheek; otherwise the cheek by the head-local sagittal plane. The
	# caller routes only front-of-head contacts here (J-044g's feed).
	var sl: Vector3 = ANCHORS["eye_l"][1]
	var sr: Vector3 = ANCHORS["eye_r"][1]
	var dl := local.distance_to(sl)
	var dr := local.distance_to(sr)
	if dl <= EYE_CLASSIFY_M and dl <= dr:
		return "eye_l"
	if dr <= EYE_CLASSIFY_M:
		return "eye_r"
	return "face_l" if local.x >= 0.0 else "face_r"


func severity(region: String) -> float:
	return float((_seen.get(region, {}) as Dictionary).get("j_region", 0.0))


func mark_visible(region: String) -> bool:
	# The eye has no 25 N·s red-mark stage: its first visible injury is
	# the ring at 75 (issue #307 tests: below 75, nothing on the eye).
	if is_eye_region(region):
		return black_eye_visible(region)
	var d: Dictionary = _seen.get(region, {})
	return float(d.get("j_event", 0.0)) >= MARK_EVENT


func bruise_visible(region: String) -> bool:
	return severity(region) >= BRUISE_REGION


func swelling_visible(region: String) -> bool:
	return severity(region) >= SWELL_REGION


func black_eye_visible(region: String) -> bool:
	return is_eye_region(region) and severity(region) >= BLACK_EYE_REGION


func black_eye_darkness(region: String) -> float:
	# 0 just after the hit, 1 once the blood has pooled (15 s), then holds.
	if not black_eye_visible(region):
		return 0.0
	var t_hit: float = float((_seen.get(region, {}) as Dictionary).get("t_hit", -1.0))
	if t_hit < 0.0:
		return 0.0
	return clampf((_clock - t_hit) / BLACK_EYE_DARKEN_S, 0.0, 1.0)


func lid_visible(region: String) -> bool:
	return is_eye_region(region) and severity(region) >= LID_SWELL_REGION


func lid_droop(region: String) -> float:
	# 0 when the lid first swells (150), 1 when the eye is a slit (250).
	if not is_eye_region(region):
		return 0.0
	return clampf((severity(region) - LID_SWELL_REGION) / (EYE_SLIT_REGION - LID_SWELL_REGION), 0.0, 1.0)


func eye_slit_visible(region: String) -> bool:
	return is_eye_region(region) and severity(region) >= EYE_SLIT_REGION


func visible_regions() -> Array:
	var out: Array = []
	for region in _seen:
		if mark_visible(region) or bruise_visible(region):
			out.append(region)
	return out


func patch_visible(region: String) -> bool:
	var p := _patches.get(region) as MeshInstance3D
	return p != null and p.visible


func lid_patch_visible(region: String) -> bool:
	var p := _lids.get(region) as MeshInstance3D
	return p != null and p.visible


func patch_outward(region: String, center: Vector3) -> float:
	# How far the patch faces away from center: positive means the quad's
	# +Z points out of the body (Claude's #280 probe). -2.0 when no patch.
	var p := _patches.get(region) as MeshInstance3D
	if p == null:
		return -2.0
	return (p.global_position - center).dot(p.global_transform.basis.z.normalized())


func _patch_for(region: String, texture: Texture2D) -> MeshInstance3D:
	if _patches.has(region):
		return _patches[region]
	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_texture = texture
	mat.roughness = 0.6
	var p := MeshInstance3D.new()
	p.mesh = _mesh
	p.material_override = mat
	p.visible = false
	add_child(p)
	_patches[region] = p
	return p


func _lid_for(region: String) -> MeshInstance3D:
	if _lids.has(region):
		return _lids[region]
	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_texture = _lid_texture
	mat.roughness = 0.6
	var p := MeshInstance3D.new()
	p.mesh = _mesh
	p.material_override = mat
	p.visible = false
	add_child(p)
	_lids[region] = p
	return p


func update(injury: Dictionary, rig: Variant, dt: float = 0.0) -> void:
	_shown = true
	_clock += dt
	for region in injury:
		if not ANCHORS.has(region):
			continue
		var dose: Dictionary = injury[region]
		var o: Dictionary = _seen.get(region, {})
		var j_region := maxf(float(dose.get("j_region", 0.0)), float(o.get("j_region", 0.0)))
		var j_event := maxf(float(dose.get("j_event", 0.0)), float(o.get("j_event", 0.0)))
		var t_hit := float(o.get("t_hit", -1.0))
		if is_eye_region(region) and t_hit < 0.0 and j_region >= BLACK_EYE_REGION:
			t_hit = _clock
		_seen[region] = {"j_region": j_region, "j_event": j_event, "t_hit": t_hit}
	if rig == null:
		return
	for region in _seen:
		var anchor: Array = ANCHORS[region]
		var bone: Transform3D = rig.bone_world(String(anchor[0]))
		if is_eye_region(region):
			_update_eye(region, anchor, bone)
			continue
		var p := _patch_for(region, _texture)
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
		# culling hides it. Basis() takes column vectors; scaled() scales
		# rows and shears it.
		var faces_back := bool(anchor[2])
		var bx := bone.basis.x * rad
		var bz := bone.basis.z
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


func _update_eye(region: String, anchor: Array, bone: Transform3D) -> void:
	# The ring hugs the socket: baked-colour annulus, transparent centre
	# so the eyeball shows through. The lid is a second quad, a soft band
	# over the upper eye that droops with dose until the eye is a slit; it
	# sits 2 mm in front of the ring so it never z-fights it.
	var tex: Texture2D = _ring_textures[region == "eye_r"]
	var p := _patch_for(region, tex)
	var lid := _lid_for(region)
	if not black_eye_visible(region):
		p.visible = false
		lid.visible = false
		return
	var dark := black_eye_darkness(region)
	var eye_sev := clampf((severity(region) - BLACK_EYE_REGION) / (EYE_SLIT_REGION - BLACK_EYE_REGION), 0.0, 1.0)
	var erad := 0.024 + 0.006 * eye_sev
	var mat := p.material_override as StandardMaterial3D
	mat.albedo_color = Color(1, 1, 1, 0.25 + 0.70 * dark)
	# The socket anchor is the eyeball CENTRE, not the skin: the lids sit
	# ~10 mm in front of it, so the ring needs a 12 mm nudge to sit 4 mm
	# off the lid skin the way the cheek quads sit 4 mm off the cheek.
	# A 4 mm nudge buried the ring just under the lids (the dark arc never
	# moved when the texture was flipped — it was a hair shadow, the ring
	# itself showed only as a faint halo).
	var at: Vector3 = (anchor[1] as Vector3) + Vector3(0.0, 0.0, 0.012)
	p.global_transform = Transform3D(
		Basis(bone.basis.x * erad, bone.basis.y * erad, bone.basis.z),
		bone.origin + bone.basis * at)
	p.visible = _shown
	if lid_visible(region):
		var droop := lid_droop(region)
		var ly := 0.014 - 0.010 * droop
		var lh := 0.016 + 0.006 * droop
		var lmat := lid.material_override as StandardMaterial3D
		lmat.albedo_color = Color(1, 1, 1, 0.65 + 0.25 * droop)
		var lat: Vector3 = (anchor[1] as Vector3) + Vector3(0.0, ly, 0.014)
		lid.global_transform = Transform3D(
			Basis(bone.basis.x * 0.018, bone.basis.y * (lh * 0.5), bone.basis.z),
			bone.origin + bone.basis * lat)
		lid.visible = _shown
	else:
		lid.visible = false
