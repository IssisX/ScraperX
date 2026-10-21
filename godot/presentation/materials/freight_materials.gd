extends RefCounted

const Surface := preload("res://presentation/materials/freight_surface.gdshader")
# One finish per supplied palette resource, reused across every freight body.
# No per-part resources or frame updates; native material groups stay intact.
var _cache: Dictionary = {}
var _service_wear: Texture2D
var _service_bump: NoiseTexture2D

func for_part(entity: int, index: int, original: Material, dynamic: bool) -> Material:
	if entity in [2560, 2561, 2562] and index in [0, 5] and original is StandardMaterial3D:
		return _service_finish(index, original as StandardMaterial3D)
	# Static freight retains the standard finish and whole-world batching.
	if not dynamic:
		return original
	# Native WarehouseForkliftYard and YardCraneGeometry identities only.
	if not (entity >= 2440 and entity <= 2447 or entity == 1450
			or entity >= 2450 and entity <= 2453):
		return original
	# Keep timber, concrete, hazard markings, tires and other surfaces intact.
	if index not in [0, 1, 5, 7] or not original is StandardMaterial3D:
		return original
	var key: Array = [original.get_instance_id(), index]
	if not _cache.has(key):
		var source := original as StandardMaterial3D
		var finish := ShaderMaterial.new()
		finish.shader = Surface
		finish.set_shader_parameter("base_color", source.albedo_color)
		finish.set_shader_parameter("painted", index == 7)
		finish.set_shader_parameter("base_metallic", 0.0 if index == 7 or index == 1 else 1.0)
		finish.set_shader_parameter("base_roughness", .56 if index == 7 else (.88 if index == 1 else .52))
		_cache[key] = finish
	return _cache[key]


func _service_finish(index: int, source: StandardMaterial3D) -> StandardMaterial3D:
	# Existing switchgear and galvanized ventilation belong to one maintained
	# installation. Keep its real normal map, projection, culling and shadows.
	var key: Array = [source.get_instance_id(), "service", index]
	if not _cache.has(key):
		var finish := source.duplicate() as StandardMaterial3D
		finish.albedo_color = source.albedo_color * (Color(0.85, 1.05, 0.98) if index == 0 else Color(0.95, 1.05, 1.0))
		finish.metallic = 0.0 if index == 0 else 1.0
		finish.roughness = 0.78 if index == 0 else 0.62
		if not source.normal_enabled or source.normal_texture == null:
			if _service_bump == null:
				var noise := FastNoiseLite.new()
				noise.seed = 2560
				noise.frequency = 0.5
				noise.fractal_octaves = 4
				_service_bump = NoiseTexture2D.new()
				_service_bump.width = 128
				_service_bump.height = 128
				_service_bump.seamless = true
				_service_bump.generate_mipmaps = true
				_service_bump.as_normal_map = true
				_service_bump.bump_strength = 0.1
				_service_bump.noise = noise
			finish.normal_enabled = true
			finish.normal_texture = _service_bump
			finish.normal_scale = 0.18
			finish.uv1_triplanar = true
			finish.uv1_scale = Vector3(0.22, 0.22, 0.22)
		finish.albedo_texture = _service_wear_texture()
		finish.roughness_texture = _service_wear
		finish.roughness_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_ALPHA
		_cache[key] = finish
	return _cache[key]


func _service_wear_texture() -> Texture2D:
	if _service_wear != null:
		return _service_wear
	# One seamless meter-scale patina map, shared by every service finish.
	# Sparse elongated rubs interrupt dusty paint/zinc; alpha packs roughness
	# so cleaned patches catch light without adding another pass or texture.
	var image := Image.create(128, 128, false, Image.FORMAT_RGBA8)
	var noise := FastNoiseLite.new()
	noise.seed = 2561
	noise.frequency = 0.045
	noise.fractal_octaves = 3
	for y in 128:
		for x in 128:
			# Periodic samples avoid a seam when triplanar coordinates repeat.
			var angle_x := TAU * float(x) / 128.0
			var angle_y := TAU * float(y) / 128.0
			var patina := noise.get_noise_3d(cos(angle_x) * 30.0,
				sin(angle_x) * 30.0, sin(angle_y) * 30.0) * 0.5 + 0.5
			var rub := smoothstep(0.58, 0.76, patina)
			var grain := 0.015 * sin(angle_x * 37.0 + sin(angle_y * 3.0))
			var value := clampf(0.91 + patina * 0.06 + rub * 0.03 + grain, 0.0, 1.0)
			image.set_pixel(x, y, Color(value, value, value, 0.98 - rub * 0.30))
	image.generate_mipmaps()
	_service_wear = ImageTexture.create_from_image(image)
	return _service_wear
