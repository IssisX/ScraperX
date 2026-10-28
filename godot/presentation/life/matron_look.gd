extends RefCounted
## What her body is made of: the textures tools/body/build_matron_body.py
## wrote next to matron.glb, read at runtime (kept as they are by their
## .import files, so a run that never imports has them too), and a material
## for each of the body's parts. The exposure's work on her, one way: the skin
## drains to a bloodless grey (never red), the uniform goes from dingy to
## filthy, her eyes go bloodshot, and her thin hair thins further.

const DIR := "res://presentation/life/matron/"
# How strongly the skin's wrinkle height becomes its normal map.
const WRINKLE_DEPTH := 3.0

var _skin: StandardMaterial3D
var _cloth: StandardMaterial3D
var _hair: StandardMaterial3D
var _eye: StandardMaterial3D
var _parts := {}   # part name -> its material
var _applied_exposure := -1.0
# Gore (gore/): blood soaking up her uniform from the hem and on her hands,
# overlays on the dressed meshes; off unless apply_gore() is called.
const GoreKit := preload("res://presentation/life/gore/gore_kit.gd")
const HANDS_BLOOD_SHADER := """
shader_type spatial;
render_mode blend_mix, depth_draw_never;
uniform float amount = 0.0;
uniform vec3 hand_l = vec3(0.0, -1000.0, 0.0);
uniform vec3 hand_r = vec3(0.0, -1000.0, 0.0);
uniform float reach = 0.22;
varying vec3 wp;
varying vec3 lp;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void vertex() {
	wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	lp = VERTEX;
	VERTEX += (inverse(MODEL_MATRIX) * vec4(normalize(CAMERA_POSITION_WORLD - wp), 0.0)).xyz * 0.003;
}
void fragment() {
	float d = min(distance(wp, hand_l), distance(wp, hand_r));
	float k = 1.0 - smoothstep(reach * amount * 0.6, reach * amount * (0.9 + 0.4 * n(lp.xy * 30.0)), d);
	k *= step(0.001, amount);
	if (k < 0.03) { discard; }
	ALBEDO = mix(vec3(0.45, 0.02, 0.02), vec3(0.2, 0.0, 0.0), n(lp.xz * 50.0) * 0.6);
	ALPHA = k * 0.95;
	ROUGHNESS = 0.1;
	SPECULAR = 0.85;
}
"""
var _apron_soak: ShaderMaterial
var _hands_blood: ShaderMaterial
var _gore_meshes: Array[MeshInstance3D] = []
var _skin_meshes: Array[MeshInstance3D] = []


static func _image(file: String) -> Image:
	var bytes := FileAccess.get_file_as_bytes(DIR + file)
	if bytes.is_empty():
		push_error("matron look: cannot read %s%s" % [DIR, file])
		return null
	var img := Image.new()
	var err := img.load_jpg_from_buffer(bytes) if file.ends_with(".jpg") else img.load_png_from_buffer(bytes)
	if err != OK:
		push_error("matron look: %s%s is not an image (%s)" % [DIR, file, error_string(err)])
		return null
	return img


static func _texture(file: String) -> ImageTexture:
	var img := _image(file)
	if img == null:
		return null
	img.generate_mipmaps()
	return ImageTexture.create_from_image(img)


static func _normal_from_height(file: String, depth: float) -> ImageTexture:
	var img := _image(file)
	if img == null:
		return null
	img.convert(Image.FORMAT_RGB8)
	img.bump_map_to_normal_map(depth)
	img.generate_mipmaps()
	return ImageTexture.create_from_image(img)


func build() -> void:
	_skin = StandardMaterial3D.new()
	_skin.albedo_texture = _texture("skin_a.jpg")
	_skin.normal_enabled = true
	_skin.normal_texture = _normal_from_height("skin_h.png", WRINKLE_DEPTH)
	_skin.normal_scale = 0.6
	_skin.roughness = 0.6
	_skin.rim_enabled = true
	_skin.rim = 0.22
	_skin.rim_tint = 0.45
	var cloth_n := _texture("cloth_n.png")
	_cloth = StandardMaterial3D.new()
	_cloth.vertex_color_use_as_albedo = true
	_cloth.normal_enabled = true
	_cloth.normal_texture = cloth_n
	_cloth.normal_scale = 0.5
	_cloth.roughness = 0.94
	_cloth.cull_mode = BaseMaterial3D.CULL_DISABLED
	var stocking := StandardMaterial3D.new()
	stocking.vertex_color_use_as_albedo = true
	stocking.normal_enabled = true
	stocking.normal_texture = _texture("knit_n.png")
	stocking.normal_scale = 0.35
	stocking.uv1_scale = Vector3(60.0, 60.0, 1.0)
	stocking.roughness = 0.75
	stocking.rim_enabled = true
	stocking.rim = 0.2
	stocking.rim_tint = 0.3
	var shoe := StandardMaterial3D.new()
	shoe.vertex_color_use_as_albedo = true
	shoe.roughness = 0.5
	shoe.cull_mode = BaseMaterial3D.CULL_DISABLED
	var steel := StandardMaterial3D.new()
	steel.albedo_color = Color("8a8a8c")
	steel.metallic = 0.6
	steel.roughness = 0.45
	var button := StandardMaterial3D.new()
	button.albedo_color = Color("b9b2a2")
	button.roughness = 0.35
	_eye = StandardMaterial3D.new()
	_eye.albedo_texture = _texture("eye_a.jpg")
	_eye.roughness = 0.08
	_eye.clearcoat_enabled = true
	_eye.clearcoat = 1.0
	_eye.clearcoat_roughness = 0.03
	# The clear cornea over each eye: only its wet highlight shows.
	var cornea := StandardMaterial3D.new()
	cornea.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	cornea.albedo_color = Color(1.0, 1.0, 1.0, 0.04)
	cornea.roughness = 0.02
	cornea.metallic_specular = 1.0
	var mouth := StandardMaterial3D.new()
	mouth.albedo_color = Color("140606")
	mouth.roughness = 1.0
	var teeth := StandardMaterial3D.new()
	teeth.albedo_texture = _texture("teeth_a.jpg")
	teeth.roughness = 0.45
	# Each lock's color is its own (vertex colors); the texture is its edges.
	_hair = StandardMaterial3D.new()
	_hair.vertex_color_use_as_albedo = true
	_hair.albedo_texture = _texture("hair_a.png")
	_hair.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	_hair.alpha_scissor_threshold = 0.45
	_hair.cull_mode = BaseMaterial3D.CULL_DISABLED
	_hair.roughness = 0.6
	_hair.rim_enabled = true
	_hair.rim = 0.12
	_hair.rim_tint = 0.6
	var brow := StandardMaterial3D.new()
	brow.albedo_texture = _texture("brow_a.png")
	brow.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	brow.alpha_scissor_threshold = 0.35
	brow.cull_mode = BaseMaterial3D.CULL_DISABLED
	var lash := brow.duplicate() as StandardMaterial3D
	lash.albedo_texture = _texture("lash_a.png")
	_parts = {"skin": _skin, "stockings": stocking, "tunic": _cloth, "collar": _cloth, "pocket": _cloth,
		"placket": _cloth, "skirt_cloth": _cloth, "belt": _cloth, "cap": _cloth, "buttons": button, "buckle": steel,
		"penlight": steel, "pins": steel, "shoes": shoe, "eyes": _eye, "cornea": cornea, "mouth": mouth, "teeth": teeth,
		"brows": brow, "lashes": lash, "hair": _hair}


# Each of the body's meshes gets its part's material (a mesh the look does
# not know keeps the file's own, and says so). The music box she carried is
# gone for good: a body built while she still had it loses it here.
func dress(root: Node) -> void:
	for node in root.find_children("*", "MeshInstance3D", true, false):
		var mesh := node as MeshInstance3D
		var part := String(mesh.name)
		if part == "music_box":
			mesh.free()
			continue
		if _parts.has(part):
			mesh.material_override = _parts[part]
		else:
			push_warning("matron look: no material for the part %s" % part)
		# Posed far from her bind pose (on all fours, the Turn's long limbs), she
		# reaches past the mesh's own bounds.
		mesh.extra_cull_margin = 1.5
		if part in ["tunic", "skirt_cloth", "pocket", "placket"]:
			_gore_meshes.append(mesh)
		elif part == "skin":
			_skin_meshes.append(mesh)


## Blood on her: `soak` 0..1 up the uniform from the hem (the apron), its
## `spatter` flecks, the Wipe's `smear` across it, and `hands` 0..1 on her
## hands round the sim's hand points. Off (all 0) removes the overlays.
func apply_gore(soak: float, spatter: float, smear: float, hands: float, hand_l: Vector3, hand_r: Vector3,
		floor_y: float = 0.0) -> void:
	var on := soak > 0.001 or spatter > 0.001 or smear > 0.001 or hands > 0.001
	if on and _apron_soak == null:
		_apron_soak = GoreKit.soak_material(0.3, 1.35, 3.0)
		var sh := Shader.new()
		sh.code = HANDS_BLOOD_SHADER
		_hands_blood = ShaderMaterial.new()
		_hands_blood.shader = sh
	if _apron_soak == null:
		return
	for m in _gore_meshes:
		m.material_overlay = _apron_soak if on else null
	for m in _skin_meshes:
		m.material_overlay = _hands_blood if on else null
	_apron_soak.set_shader_parameter("soak", soak)
	_apron_soak.set_shader_parameter("spatter", spatter)
	_apron_soak.set_shader_parameter("smear", smear)
	_apron_soak.set_shader_parameter("base_y", floor_y)
	_hands_blood.set_shader_parameter("amount", hands)
	_hands_blood.set_shader_parameter("hand_l", hand_l)
	_hands_blood.set_shader_parameter("hand_r", hand_r)


## Her tints for an exposure (0..1), as albedo multipliers: the skin goes
## pale and waxy (it stays light, a little cooler) while the uniform darkens,
## so the two separate more the further she is gone. Static so the figure
## test can check the separation without a scene.
static func exposure_tints(exposure: float) -> Dictionary:
	var e := clampf(exposure, 0.0, 1.0)
	var first := clampf(e * 2.0, 0.0, 1.0)
	var second := clampf((e - 0.5) * 2.0, 0.0, 1.0)
	return {
		"skin": Color.WHITE.lerp(Color(0.95, 0.95, 0.98), first).lerp(Color(0.9, 0.92, 0.98), second),
		"cloth": Color.WHITE.lerp(Color(0.78, 0.72, 0.65), first).lerp(Color(0.36, 0.31, 0.28), second),
	}


func apply_exposure(exposure: float) -> void:
	if _skin == null or _cloth == null or _eye == null or _hair == null:
		return  # before build(): nothing to tint yet
	var e := clampf(exposure, 0.0, 1.0)
	if absf(e - _applied_exposure) < 0.01:
		return
	_applied_exposure = e
	var tints := exposure_tints(e)
	_skin.albedo_color = tints["skin"]
	_skin.roughness = lerpf(0.6, 0.42, e)   # clammy, a waxy sheen
	_cloth.albedo_color = tints["cloth"]
	_eye.albedo_color = Color.WHITE.lerp(Color(1.0, 0.84, 0.8), e)
	_hair.alpha_scissor_threshold = lerpf(0.45, 0.78, e)
