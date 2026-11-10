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
## edge the way the flat cheek quad did (#306). The lid is a second layer, a
## soft band over the upper eye that droops with dose. Both are drawn ON her
## skin, not on a quad (Cory 11-08: from the side the flat quad stood off the
## eye like a monocle, and the #309 grazing fade only hid it there): the mesh
## is a copy of her own skin triangles round the socket, rigid on the head
## bone (every skin vertex there is 100% head-weighted, measured 2026-11-08),
## and matron_injury_ring.gdshader projects the ring and lid textures onto it
## along the head's forward axis, so the bruise follows the face's curve from
## the front, three-quarters and the side. gl_compatibility has no decals.
##
## The eye socket centres are the eye bones' rest offsets in head-local space,
## measured 2026-11-07: the bone called eye_l sits at head-local -X and the
## bone called eye_r at +X — the rig's eye bone NAMES are mirrored against
## the face-anchor convention (head-local +X is her left, verified by the
## #306 stills). The anchors below follow anatomy, not the bone names; the
## eye_sides check in injury_shots_test.gd guards the signs.
##
## The cheek bruises (face_l/face_r) are drawn on her skin the same way
## (the flat cheek quad stood up to 44 mm off her face, overhanging the jaw
## line, measured 2026-11-08). Unlike the eye, the cheek skin is not all
## head: 242 of the 265 skin vertices under a swollen cheek bruise also
## follow the jaw (a few the neck), so a head-rigid copy stood 3 mm off the
## cheek even in the test pose and would part further when the jaw opens.
## The cheek copy keeps her skin weights instead: the shader blends each
## vertex between the head and those bones (`blend`), so it follows her
## cheek as the skin does. Its frame is turned about the head's up axis to
## face out of the cheek (the old quad's tilt), and the bruise is projected
## along that.

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
# The faint yellow-brown edge a cheek bruise takes at the bruise and swelling
# stages (the blood breaking down at the rim of a contusion).
const CHEEK_RIM := Color(0.50, 0.40, 0.17)
const RING_DARK := Color(0.05, 0.01, 0.05)
# J-059: the eye ring and lid are projected onto a copy of her skin round
# the socket (matron_injury_ring.gdshader), so they lie on the face.
const RING_SHADER := preload("res://presentation/life/matron/matron_injury_ring.gdshader")
# The skin copied round each socket: triangles with a vertex within this
# distance of the socket centre. Wider than the largest ring (0.030 m half
# size) and the lid band, so the copy's edge never shows.
const EYE_SKIN_M := 0.045
# ...and no deeper than this behind the eyeball centre (head-local z): the
# inside of the head (mouth, throat) falls within range and takes no bruise.
const EYE_SKIN_BACK_M := -0.03
# Lift off the skin (shader `push`): the lid sits a hair over the ring.
const RING_WIDEN := 1.15  # ring half width / height on her skin (see _update_eye)
const RING_PUSH_M := 0.0004
const LID_PUSH_M := 0.0007
# The cheek skin copy: wider than the largest cheek bruise (0.0425 m half
# size, swollen), the same depth limit as the eye, and a hair under the eye
# ring where the two meet.
const CHEEK_SKIN_M := 0.05
const CHEEK_PUSH_M := 0.0003
const BLEND_SLOTS := ["bone_a", "bone_b", "bone_c", "bone_d"]

var _seen := {}  # region -> {"j_region": float, "j_event": float, "t_hit": float}, maxima only
var _patches := {}  # region -> MeshInstance3D
var _lids := {}  # eye region -> MeshInstance3D (swollen upper lid)
var _texture: Texture2D = null
var _cheek_textures := {}  # "mark" / "bruise" / "swell" -> Texture2D, made on first use
var _ring_textures := {}  # false/true (mirrored) -> Texture2D
var _lid_texture: Texture2D = null
var _mesh: QuadMesh = null
var _skin_meshes := {}  # eye/cheek region -> ArrayMesh, her skin around it
var _skin_bones := {}  # cheek region -> skin bind indices on bone_a..bone_d
var _skin: Skin = null  # her body's skin, for the cheek blend
var _head_bind := -1
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


func _cheek_texture(stage: String) -> Texture2D:
	# The cheek's own bruise (the shared _bruise_texture stays for the skull,
	# jaw, neck and chest quads): its per-cell hash wobble stepped the edge
	# into a painted rectangle on the cheek (Claude's eye check 2026-11-08).
	# Smooth sine wobble on the outline, as #309's ring; soft falloff to clear
	# skin over the outer 30% of the radius (0.56-0.86 of the half size, zero
	# before the texture's edge even where the wobble pulls the outline in).
	# Color is baked in from the stage's color: a darker purple-red core
	# mottled at low frequency, fading out to the stage color and, at the
	# bruise and swelling stages, a faint yellow-brown rim. The material's
	# albedo alpha keeps the old severity scale (texture alpha up to 0.92,
	# as the shared texture). The red mark keeps its red, no rim.
	if _cheek_textures.has(stage):
		return _cheek_textures[stage]
	var core: Color = MARK_COLOR
	var rim := 0.0
	if stage == "bruise":
		core = BRUISE_COLOR.lerp(SWELL_COLOR, 0.35)
		rim = 0.35
	elif stage == "swell":
		core = SWELL_COLOR
		rim = 0.6
	var size := 128
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	for y in size:
		for x in size:
			var u := (float(x) / float(size - 1)) * 2.0 - 1.0
			var v := (float(y) / float(size - 1)) * 2.0 - 1.0
			var wobble := 0.05 * sin(u * 2.3 + 0.9) * sin(v * 2.9 + 1.4) + 0.03 * sin(u * 4.1 + 2.1) * sin(v * 3.3 + 0.3) + 0.03 * sin(atan2(v, u) * 3.0 + 0.7)
			var r := Vector2(u, v).length() + wobble
			var shape := 1.0 - clampf(smoothstep(0.56, 0.86, r), 0.0, 1.0)
			var mottle := 0.5 * sin(u * 3.7 + 0.4) * sin(v * 3.1 + 1.9) + 0.25 * sin(u * 6.3 + 2.6) * sin(v * 5.2 + 0.8)
			var k := 0.7 + 0.12 * mottle
			var col := Color(core.r * k, core.g * k, core.b * k).lerp(core, clampf(smoothstep(0.1, 0.75, r), 0.0, 1.0))
			col = col.lerp(CHEEK_RIM, rim * clampf(smoothstep(0.45, 0.8, r), 0.0, 1.0))
			img.set_pixel(x, y, Color(col.r, col.g, col.b, shape * minf(0.86 + 0.08 * mottle, 0.92)))
	var tex := ImageTexture.create_from_image(img)
	_cheek_textures[stage] = tex
	return tex


func _ring_texture(mirror: bool) -> Texture2D:
	# Soft annulus with a light centre (30%, on the lids), baked dark
	# red-purple, darkest on the lower lid and the inner corner. As the
	# shader projects it onto her skin (head-local x right, y up, as the
	# old quad was placed), image-top is her upper lid and image-right is
	# her outer corner for eye_l: the dark lower-lid weight sits at image-bottom
	# (v = +1) and the dark inner weight at image-left (u = -1); eye_r
	# mirrors u, so it stays symmetric. Colour is baked in; the material's
	# albedo alpha ramps the darkness over the 15 s pool.
	# 256 px, and a smooth low-frequency wobble: the old per-cell hash
	# wobble stepped the annulus edge (a stair-stepped lower edge in the
	# close-up, #309). A sine wobble keeps the organic edge but is
	# continuous, so no steps.
	var size := 256
	var img := Image.create(size, size, false, Image.FORMAT_RGBA8)
	for y in size:
		for x in size:
			var u := (float(x) / float(size - 1)) * 2.0 - 1.0
			var v := (float(y) / float(size - 1)) * 2.0 - 1.0
			if mirror:
				u = -u
			var wobble := 0.035 * sin(u * 5.0 + 1.7) * sin(v * 4.0 + 0.6) + 0.02 * sin(u * 9.0 + 0.4) * sin(v * 7.0 + 2.3)
			var r := Vector2(u, v).length() * (1.0 + wobble)
			# On her skin the ring's edges must not read as a painted rim:
			# the outer edge fades over the cheek and brow (0.80-0.94, zero
			# before the texture's edge even where the wobble pulls it in),
			# and the centre keeps 30% so the lids take some bruise (the
			# eyeball is not skin, so it never darkens).
			var shape := (0.3 + 0.7 * clampf(smoothstep(0.40, 0.58, r), 0.0, 1.0)) * (1.0 - clampf(smoothstep(0.80, 0.94, r), 0.0, 1.0))
			var bottom := clampf((v - 0.2) / 0.6, 0.0, 1.0)
			var inner := clampf((-u - 0.2) / 0.6, 0.0, 1.0)
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


func is_cheek_region(region: String) -> bool:
	return region == "face_l" or region == "face_r"


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


func _skin_material(texture: Texture2D, push: float) -> ShaderMaterial:
	# The eye ring/lid and cheek material: the texture projected onto the
	# skin copy around the region (matron_injury_ring.gdshader).
	var smat := ShaderMaterial.new()
	smat.shader = RING_SHADER
	smat.set_shader_parameter("albedo_texture", texture)
	smat.set_shader_parameter("push", push)
	return smat


func _skin_frame(region: String) -> Transform3D:
	# The skin copy's frame in head-local space: at the region's anchor, the
	# head's axes for the eye; for a cheek, turned about the head's up axis
	# by atan2(|x|, z) of its anchor (~33 deg) toward her left for face_l
	# and her right for face_r, so its +Z faces out of the cheek and the
	# bruise is projected along the cheek's own outward direction.
	var aoff: Vector3 = ANCHORS[region][1]
	var basis := Basis.IDENTITY
	if is_cheek_region(region):
		var tilt := atan2(absf(aoff.x), aoff.z)
		basis = Basis(Vector3.UP, tilt if aoff.x >= 0.0 else -tilt)
	return Transform3D(basis, aoff)


func _bind_bone(skel: Skeleton3D, k: int) -> int:
	var bi := _skin.get_bind_bone(k)
	return bi if bi >= 0 else skel.find_bone(_skin.get_bind_name(k))


func _skin_mesh_for(region: String, rig: Variant) -> ArrayMesh:
	# Her skin triangles around the region, in the copy's frame
	# (_skin_frame), taken once from the skinned body mesh at its bind
	# pose: frame_inv * bind(head) * vertex. The eye takes only skin that
	# follows the head bone alone, so it is rigid on the head: the drawn
	# skin is bone_world(head) * that wherever the weight is all head,
	# which it is around the eyes. A cheek takes its skin whatever the
	# weights and keeps up to four other bones' weights per vertex in
	# CUSTOM0 (bones in _skin_bones), for the shader's blend. null when her
	# skin is missing.
	if _skin_meshes.has(region):
		return _skin_meshes[region]
	var skel: Skeleton3D = null
	if rig != null:
		skel = rig.get("skel") as Skeleton3D
	var skin_mi: MeshInstance3D = null
	var head_bone := -1
	if skel != null:
		head_bone = skel.find_bone("head")
		for node in skel.find_children("skin", "MeshInstance3D", false, false):
			skin_mi = node as MeshInstance3D
			break
	var bind := Transform3D.IDENTITY
	var head_bind := -1
	if skin_mi != null and skin_mi.mesh != null and skin_mi.skin != null and head_bone >= 0:
		for k in skin_mi.skin.get_bind_count():
			if skin_mi.skin.get_bind_bone(k) == head_bone or skin_mi.skin.get_bind_name(k) == &"head":
				bind = skin_mi.skin.get_bind_pose(k)
				head_bind = k
				break
	if head_bind < 0:
		push_warning("matron injury: no head-bound skin mesh to draw %s on" % region)
		_skin_meshes[region] = null
		return null
	_skin = skin_mi.skin
	_head_bind = head_bind
	var cheek := is_cheek_region(region)
	var radius := CHEEK_SKIN_M if cheek else EYE_SKIN_M
	var to_frame := _skin_frame(region).affine_inverse()
	var arrays: Array = skin_mi.mesh.surface_get_arrays(0)
	var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var norms: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var index: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	var bones: PackedInt32Array = arrays[Mesh.ARRAY_BONES]
	var weights: PackedFloat32Array = arrays[Mesh.ARRAY_WEIGHTS]
	var bpv := int(bones.size() / maxi(verts.size(), 1))
	var nb := to_frame.basis * bind.basis.inverse().transposed()
	# near: 1 within the radius of the anchor and in front of
	# EYE_SKIN_BACK_M (frame z): the inside of the head (mouth, throat)
	# falls within range and takes no bruise. rigid: 1 when the vertex
	# follows the head bone alone (or for a cheek, always). Around the eye
	# every vertex the ring can land on does (measured); a vertex that also
	# follows the jaw (low on the cheek, past the ring) is left out of the
	# eye, or the rigid copy would part from her skin when the jaw moves.
	var near := PackedByteArray()
	var rigid := PackedByteArray()
	near.resize(verts.size())
	rigid.resize(verts.size())
	for i in verts.size():
		var rel := to_frame * (bind * verts[i])
		near[i] = 1 if rel.length() < radius and rel.z > EYE_SKIN_BACK_M else 0
		var hw := 0.0
		for j in bpv:
			if bones[i * bpv + j] == head_bind:
				hw += weights[i * bpv + j]
		rigid[i] = 1 if cheek or hw >= 0.999 else 0
	var remap := {}
	var out_v := PackedVector3Array()
	var out_n := PackedVector3Array()
	var out_i := PackedInt32Array()
	var src := PackedInt32Array()
	for t in range(0, index.size(), 3):
		var tri := [index[t], index[t + 1], index[t + 2]]
		if near[tri[0]] == 0 and near[tri[1]] == 0 and near[tri[2]] == 0:
			continue
		if rigid[tri[0]] == 0 or rigid[tri[1]] == 0 or rigid[tri[2]] == 0:
			continue
		for vi: int in tri:
			if not remap.has(vi):
				remap[vi] = out_v.size()
				out_v.append(to_frame * (bind * verts[vi]))
				out_n.append((nb * norms[vi]).normalized())
				src.append(vi)
			out_i.append(int(remap[vi]))
	var mesh := ArrayMesh.new()
	if out_i.is_empty():
		push_warning("matron injury: no skin round %s" % region)
		_skin_meshes[region] = mesh
		return mesh
	var surf := []
	surf.resize(Mesh.ARRAY_MAX)
	surf[Mesh.ARRAY_VERTEX] = out_v
	surf[Mesh.ARRAY_NORMAL] = out_n
	surf[Mesh.ARRAY_INDEX] = out_i
	var flags := 0
	if cheek:
		# The other bones this skin follows, in bind order, onto the four
		# blend slots; the head keeps the rest. (Measured: jaw, neck_hi and
		# neck_lo under the cheek.) A fifth would fold onto the head.
		var slots := PackedInt32Array()
		for vi: int in src:
			for j in bpv:
				var k := bones[vi * bpv + j]
				if k != head_bind and weights[vi * bpv + j] > 0.0 and not slots.has(k):
					slots.append(k)
		slots.sort()
		if slots.size() > BLEND_SLOTS.size():
			push_warning("matron injury: %s skin follows %d bones, blending 4" % [region, slots.size()])
			slots.resize(BLEND_SLOTS.size())
		var cw := PackedFloat32Array()
		cw.resize(src.size() * 4)
		for n in src.size():
			for j in bpv:
				var slot := slots.find(bones[src[n] * bpv + j])
				if slot >= 0:
					cw[n * 4 + slot] += weights[src[n] * bpv + j]
		surf[Mesh.ARRAY_CUSTOM0] = cw
		flags = Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
		_skin_bones[region] = slots
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surf, [], {}, flags)
	_skin_meshes[region] = mesh
	return mesh


func _patch_for(region: String, texture: Texture2D) -> MeshInstance3D:
	if _patches.has(region):
		return _patches[region]
	var p := MeshInstance3D.new()
	p.mesh = _mesh
	var mat := StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_texture = texture
	mat.roughness = 0.6
	p.material_override = mat
	p.visible = false
	add_child(p)
	_patches[region] = p
	return p


func _skin_patch_for(region: String, mesh: ArrayMesh, texture: Texture2D, push: float) -> MeshInstance3D:
	# The eye ring or cheek bruise: her skin copy around the region, the
	# texture projected on.
	if _patches.has(region):
		return _patches[region]
	var p := MeshInstance3D.new()
	p.mesh = mesh
	p.material_override = _skin_material(texture, push)
	p.visible = false
	add_child(p)
	_patches[region] = p
	return p


func _lid_for(region: String, mesh: ArrayMesh) -> MeshInstance3D:
	# The swollen lid: the same skin copy, lid band projected on, drawn
	# after the ring (render_priority) a hair further off the skin.
	if _lids.has(region):
		return _lids[region]
	var p := MeshInstance3D.new()
	p.mesh = mesh
	var lmat := _skin_material(_lid_texture, LID_PUSH_M)
	lmat.render_priority = 1
	p.material_override = lmat
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
			_update_eye(region, anchor, bone, rig)
			continue
		if is_cheek_region(region):
			_update_cheek(region, bone, rig)
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
		# culling hides it. (The cheeks are on her skin: _update_cheek.)
		# Basis() takes column vectors; scaled() scales rows and shears it.
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


func _update_eye(region: String, anchor: Array, bone: Transform3D, rig: Variant) -> void:
	# The ring lies on her skin round the socket: the skin copy is rigid on
	# the head bone with its origin at the socket centre, and the shader
	# projects the ring texture onto it (transparent centre, so the eyeball
	# shows). The lid band goes on the same skin over the upper eye and
	# droops with dose until the eye is a slit. The texture placement is
	# the old quads' (ring half size erad, lid band 0.018 x lh/2 at ly), so
	# straight on it reads as before; it now wraps round the face.
	if not black_eye_visible(region):
		if _patches.has(region):
			(_patches[region] as MeshInstance3D).visible = false
		if _lids.has(region):
			(_lids[region] as MeshInstance3D).visible = false
		return
	var mesh := _skin_mesh_for(region, rig)
	if mesh == null:
		return
	var p := _skin_patch_for(region, mesh, _ring_textures[region == "eye_r"], RING_PUSH_M)
	var lid := _lid_for(region, mesh)
	var dark := black_eye_darkness(region)
	var eye_sev := clampf((severity(region) - BLACK_EYE_REGION) / (EYE_SLIT_REGION - BLACK_EYE_REGION), 0.0, 1.0)
	var erad := 0.024 + 0.006 * eye_sev
	var mat := p.material_override as ShaderMaterial
	mat.set_shader_parameter("albedo_color", Color(1, 1, 1, 0.25 + 0.70 * dark))
	# Stretched toward the outer corner only: on her skin the bruise reaches
	# round the outer corner toward the temple, which is what the side view
	# sees; the inner edge stays where the round quad had it, short of the
	# nose bridge. Her outer corner is head-local +X for eye_l, -X for eye_r.
	var outer := 1.0 if (anchor[1] as Vector3).x > 0.0 else -1.0
	mat.set_shader_parameter("uv_center", Vector2(outer * erad * (RING_WIDEN - 1.0), 0.0))
	mat.set_shader_parameter("uv_half", Vector2(erad * RING_WIDEN, erad))
	var xf := Transform3D(bone.basis, bone.origin + bone.basis * (anchor[1] as Vector3))
	p.global_transform = xf
	p.visible = _shown
	if lid_visible(region):
		var droop := lid_droop(region)
		var ly := 0.014 - 0.010 * droop
		var lh := 0.016 + 0.006 * droop
		var lmat := lid.material_override as ShaderMaterial
		lmat.set_shader_parameter("albedo_color", Color(1, 1, 1, 0.65 + 0.25 * droop))
		lmat.set_shader_parameter("uv_center", Vector2(0.0, ly))
		lmat.set_shader_parameter("uv_half", Vector2(0.018, lh * 0.5))
		lid.global_transform = xf
		lid.visible = _shown
	else:
		lid.visible = false


func _update_cheek(region: String, bone: Transform3D, rig: Variant) -> void:
	# The cheek bruise on her skin: the cheek's bruise texture projected onto
	# the copy of her cheek skin, sized as the old quad was (half size rad)
	# with the old alpha scale and the mark, bruise and swelling colors. The copy sits in its
	# frame on the head bone and the shader blends each vertex toward the
	# jaw and neck by her own weights: bone_a..bone_d are those bones'
	# current poses against the head's, in the copy's frame
	# (frame_inv * head_pose_inv * bone_pose * bind * bind_head_inv * frame;
	# the head's own is the identity).
	var p := _patches.get(region) as MeshInstance3D
	var sev := clampf((severity(region) - BRUISE_REGION) / BRUISE_REGION, 0.0, 1.0)
	var swelling := swelling_visible(region)
	var rad := (0.04 + 0.025 * sev + (0.02 if swelling else 0.0)) * 0.5
	# Color is in the stage's texture (_cheek_texture); the alpha is the
	# old scale.
	var alpha := 0.0
	var stage := ""
	if bruise_visible(region):
		alpha = 0.5 + 0.25 * sev if not swelling else 0.7
		stage = "swell" if swelling else "bruise"
	elif mark_visible(region):
		alpha = 0.3
		stage = "mark"
		rad *= 0.7
	else:
		if p != null:
			p.visible = false
		return
	var mesh := _skin_mesh_for(region, rig)
	if mesh == null:
		return
	var tex := _cheek_texture(stage)
	p = _skin_patch_for(region, mesh, tex, CHEEK_PUSH_M)
	var mat := p.material_override as ShaderMaterial
	mat.set_shader_parameter("albedo_texture", tex)
	mat.set_shader_parameter("albedo_color", Color(1, 1, 1, alpha))
	mat.set_shader_parameter("uv_center", Vector2.ZERO)
	mat.set_shader_parameter("uv_half", Vector2(rad, rad))
	mat.set_shader_parameter("blend", true)
	var frame := _skin_frame(region)
	var skel := rig.get("skel") as Skeleton3D
	var slots: PackedInt32Array = _skin_bones.get(region, PackedInt32Array())
	var to_frame := (skel.get_bone_global_pose(_bind_bone(skel, _head_bind)) * frame).affine_inverse()
	var from_frame := _skin.get_bind_pose(_head_bind).affine_inverse() * frame
	for slot in BLEND_SLOTS.size():
		var d := Transform3D.IDENTITY
		if slot < slots.size():
			var k := slots[slot]
			d = to_frame * skel.get_bone_global_pose(_bind_bone(skel, k)) * _skin.get_bind_pose(k) * from_frame
		mat.set_shader_parameter(BLEND_SLOTS[slot], d)
	p.global_transform = bone * frame
	p.visible = _shown
