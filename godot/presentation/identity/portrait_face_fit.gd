extends RefCounted
## Photo coordinates are normalized, top-left origin, ordered image-left eye,
## image-right eye, mouth center. This calibration is only for ro-human-prototype.
## UV2 and vertex alpha on its body skin are reserved for the portrait mapping.
## Call on the imported rest mesh, before applying skeleton/morph animation.

const SKIN_MATERIAL := "RoPrototype.Body.body"
const SKIN_SHADER := preload("res://presentation/identity/portrait_skin.gdshader")
const LEFT_EYE := Vector3(0.029386, 1.620422, 0.126349)
const RIGHT_EYE := Vector3(-0.029386, 1.620422, 0.126349)
# Midpoint of actual GLB inner lip seam vertices 520/523. Atlas UVs
# (.905022,.516834)/(.913715,.516852) land on the texture mouth opening.
# oris05 sits above the lips; oris01 is below them near the chin.
const MOUTH := Vector3(0.0, 1.547968, 0.153039)
var last_error := ""

func fit_mesh(source: ArrayMesh, photo: Texture2D, landmarks: PackedVector2Array) -> ArrayMesh:
	last_error = ""
	if source == null or photo == null or photo.get_width() < 1 or photo.get_height() < 1:
		return _reject("Face fit requires a rest ArrayMesh and a nonempty photo")
	if not _valid_landmarks(landmarks):
		return _reject("Face fit requires three finite normalized landmarks: image-left eye, image-right eye, mouth; eyes must be separated and mouth below them")
	var accepted := false
	for surface in source.get_surface_count():
		var material := source.surface_get_material(surface)
		if material != null and material.resource_name == SKIN_MATERIAL:
			if not material is StandardMaterial3D:
				return _reject("Template skin must use StandardMaterial3D")
			if material.transparency not in [BaseMaterial3D.TRANSPARENCY_ALPHA, BaseMaterial3D.TRANSPARENCY_ALPHA_DEPTH_PRE_PASS] or material.cull_mode != BaseMaterial3D.CULL_DISABLED:
				return _reject("Template skin requires its imported alpha-blended, double-sided material")
			if material.shading_mode != BaseMaterial3D.SHADING_MODE_PER_PIXEL or material.uv1_triplanar:
				return _reject("Template skin requires per-pixel UV material")
			var arrays := source.surface_get_arrays(surface)
			if arrays[Mesh.ARRAY_NORMAL] == null or arrays[Mesh.ARRAY_TEX_UV] == null:
				return _reject("Template skin requires rest normals and UV0")
			accepted = true
	if not accepted:
		return _reject("No accepted template body skin material: " + SKIN_MATERIAL)
	var result := ArrayMesh.new()
	result.resource_name = source.resource_name
	result.blend_shape_mode = source.blend_shape_mode
	result.custom_aabb = source.custom_aabb
	result.shadow_mesh = source.shadow_mesh
	for shape in source.get_blend_shape_count():
		result.add_blend_shape(source.get_blend_shape_name(shape))
	for surface in source.get_surface_count():
		var arrays := source.surface_get_arrays(surface).duplicate(true)
		var material := source.surface_get_material(surface)
		if material != null and material.resource_name == SKIN_MATERIAL:
			var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
			var uv2 := PackedVector2Array()
			var colors := PackedColorArray()
			var original_colors: PackedColorArray = arrays[Mesh.ARRAY_COLOR] if arrays[Mesh.ARRAY_COLOR] != null else PackedColorArray()
			for index in vertices.size():
				uv2.append(photo_uv(vertices[index], landmarks))
				var color := original_colors[index] if not original_colors.is_empty() else Color.WHITE
				color.a = face_mask(vertices[index], normals[index])
				colors.append(color)
			arrays[Mesh.ARRAY_TEX_UV2] = uv2
			arrays[Mesh.ARRAY_COLOR] = colors
			material = skin_material(material, photo, landmarks)
		# Retain custom attribute formats and 8-bone/compression flags. Array
		# presence is determined from arrays by add_surface_from_arrays itself.
		var flags := source.surface_get_format(surface) & ~((1 << Mesh.ARRAY_MAX) - 1)
		result.add_surface_from_arrays(source.surface_get_primitive_type(surface), arrays,
			source.surface_get_blend_shape_arrays(surface), _surface_lods(source, surface), flags)
		result.surface_set_material(surface, material)
		result.surface_set_name(surface, source.surface_get_name(surface))
	return result

func _reject(message: String) -> ArrayMesh:
	last_error = message
	return null

func _valid_landmarks(points: PackedVector2Array) -> bool:
	if points.size() != 3:
		return false
	for point in points:
		if not point.is_finite() or point.x < 0.0 or point.x > 1.0 or point.y < 0.0 or point.y > 1.0:
			return false
	var eyes := points[1] - points[0]
	return eyes.x >= 0.025 and eyes.length() >= 0.04 and eyes.cross(points[2] - points[0]) >= 0.001

func photo_uv(position: Vector3, points: PackedVector2Array) -> Vector2:
	# Barycentric affine fit in the template frontal XY plane. Bake once so
	# the portrait follows vertices through skinning and facial blend shapes.
	var across := (LEFT_EYE.x - position.x) / (LEFT_EYE.x - RIGHT_EYE.x)
	var down := (LEFT_EYE.y - position.y) / (LEFT_EYE.y - MOUTH.y)
	return points[0] + (points[1] - points[0]) * across + (points[2] - (points[0] + points[1]) * 0.5) * down

func face_mask(position: Vector3, normal: Vector3) -> float:
	# Template meters: side/ear and back exclusion precedes soft face edges.
	if absf(position.x) >= 0.077 or position.y <= 1.515 or position.y >= 1.695 or position.z <= 0.09:
		return 0.0
	var radial := Vector2(position.x / 0.077, (position.y - 1.605) / 0.09).length()
	return (1.0 - smoothstep(0.72, 1.0, radial)) * smoothstep(0.15, 0.65, normal.z) * smoothstep(0.09, 0.12, position.z)

## Copies the accepted imported StandardMaterial3D's skin channels. Callers may
## adjust the returned ShaderMaterial uniforms for rendering calibration.
func skin_material(base: StandardMaterial3D, photo: Texture2D, landmarks: PackedVector2Array = PackedVector2Array()) -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = SKIN_SHADER
	if base.transparency == BaseMaterial3D.TRANSPARENCY_ALPHA:
		# Keep ordinary alpha blending free of a depth prepass. The imported
		# template uses ALPHA_DEPTH_PRE_PASS and retains the shared shader.
		var alpha_shader := Shader.new()
		alpha_shader.code = SKIN_SHADER.code.replace(", depth_prepass_alpha", "")
		material.shader = alpha_shader
	material.resource_name = "PortraitSkin"
	material.set_shader_parameter("portrait_texture", photo)
	material.set_shader_parameter("skin_color", base.albedo_color)
	material.set_shader_parameter("skin_tint", _skin_tint(base, photo, landmarks))
	material.set_shader_parameter("skin_albedo", base.albedo_texture)
	material.set_shader_parameter("has_skin_albedo", base.albedo_texture != null)
	material.set_shader_parameter("skin_normal", base.normal_texture)
	material.set_shader_parameter("has_skin_normal", base.normal_enabled and base.normal_texture != null)
	material.set_shader_parameter("normal_scale", base.normal_scale)
	material.set_shader_parameter("skin_roughness", base.roughness_texture)
	material.set_shader_parameter("has_skin_roughness", base.roughness_texture != null)
	material.set_shader_parameter("roughness_value", base.roughness)
	material.set_shader_parameter("roughness_channel", _roughness_channel(base.roughness_texture_channel))
	material.set_shader_parameter("metallic_value", base.metallic)
	material.set_shader_parameter("specular_value", base.metallic_specular)
	material.set_shader_parameter("skin_uv_scale", Vector2(base.uv1_scale.x, base.uv1_scale.y))
	material.set_shader_parameter("skin_uv_offset", Vector2(base.uv1_offset.x, base.uv1_offset.y))
	return material

# Match exposed template skin to the photo's two cheeks. A multiplicative tint
# retains the imported skin texture's detail and leaves its alpha untouched.
func _skin_tint(base: StandardMaterial3D, photo: Texture2D, points: PackedVector2Array) -> Vector3:
	if points.size() != 3:
		return Vector3.ONE
	var photo_image := photo.get_image()
	if photo_image == null or photo_image.is_empty():
		return Vector3.ONE
	if photo_image.is_compressed() and photo_image.decompress() != OK:
		return Vector3.ONE
	var down := points[2] - (points[0] + points[1]) * 0.5
	var radius := points[0].distance_to(points[1]) * 0.06
	var target := (_sample_patch(photo_image, points[0] + down * 0.52, radius)
		+ _sample_patch(photo_image, points[1] + down * 0.52, radius)) * 0.5
	var reference := base.albedo_color.srgb_to_linear()
	if base.albedo_texture != null:
		var skin_image := base.albedo_texture.get_image()
		if skin_image == null or skin_image.is_empty():
			return Vector3.ONE
		if skin_image.is_compressed() and skin_image.decompress() != OK:
			return Vector3.ONE
		# Actual template cheek vertices 7968/3812 and their UV0 positions.
		var scale := Vector2(base.uv1_scale.x, base.uv1_scale.y)
		var offset := Vector2(base.uv1_offset.x, base.uv1_offset.y)
		var cheeks := (_sample_patch(skin_image, Vector2(0.869593, 0.475513) * scale + offset, 0.002)
			+ _sample_patch(skin_image, Vector2(0.869593, 0.558152) * scale + offset, 0.002)) * 0.5
		reference *= cheeks
	return Vector3(clampf(target.r / maxf(reference.r, 0.01), 0.15, 3.0),
		clampf(target.g / maxf(reference.g, 0.01), 0.15, 3.0),
		clampf(target.b / maxf(reference.b, 0.01), 0.15, 3.0))

func _sample_patch(image: Image, center: Vector2, radius: float) -> Color:
	var total := Color(0, 0, 0, 0)
	for y in range(-2, 3):
		for x in range(-2, 3):
			var uv := center + Vector2(x, y) * radius * 0.5
			var pixel := Vector2i(clampi(roundi(uv.x * (image.get_width() - 1)), 0, image.get_width() - 1),
				clampi(roundi(uv.y * (image.get_height() - 1)), 0, image.get_height() - 1))
			total += image.get_pixelv(pixel).srgb_to_linear()
	return total / 25.0

func _roughness_channel(channel: int) -> Vector4:
	match channel:
		BaseMaterial3D.TEXTURE_CHANNEL_RED: return Vector4(1, 0, 0, 0)
		BaseMaterial3D.TEXTURE_CHANNEL_GREEN: return Vector4(0, 1, 0, 0)
		BaseMaterial3D.TEXTURE_CHANNEL_BLUE: return Vector4(0, 0, 1, 0)
		BaseMaterial3D.TEXTURE_CHANNEL_ALPHA: return Vector4(0, 0, 0, 1)
	return Vector4(0.333333, 0.333333, 0.333333, 0)

func _surface_lods(mesh: ArrayMesh, surface: int) -> Dictionary:
	var data := RenderingServer.mesh_get_surface(mesh.get_rid(), surface)
	var lods := {}
	var stride := RenderingServer.mesh_surface_get_format_index_stride(mesh.surface_get_format(surface), mesh.surface_get_array_len(surface))
	for lod in data.get("lods", []):
		var bytes: PackedByteArray = lod.index_data
		var indices := PackedInt32Array()
		for offset in range(0, bytes.size(), stride):
			indices.append(bytes.decode_u16(offset) if stride == 2 else bytes.decode_u32(offset))
		lods[lod.edge_length] = indices
	return lods
