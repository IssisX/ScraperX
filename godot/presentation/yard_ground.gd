extends RefCounted
## The yard's asphalt, drawn as a site's ground ages: broad worn patches,
## darker fresh patching, the aggregate's grain, a network of tar-sealed
## cracks and oil stains. Presentation only: the grade's collision is the
## same box.
##
## Everything is sampled from a few noise textures, made once, at the
## fragment's world position (the grade is a plane), so it costs a handful of
## texture reads per pixel and nothing per frame. Its average colour is the
## old flat asphalt's (17150f), so the lighting balance is unchanged.

static var _material: ShaderMaterial


static func material() -> ShaderMaterial:
	if _material != null:
		return _material
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_back;
uniform sampler2D patches : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform sampler2D grain : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform sampler2D cracks : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform sampler2D relief : hint_normal, filter_linear_mipmap, repeat_enable;
uniform vec3 asphalt : source_color = vec3(0.090, 0.082, 0.060);
uniform vec3 worn : source_color = vec3(0.150, 0.144, 0.128);
uniform vec3 patched : source_color = vec3(0.055, 0.052, 0.046);
varying vec3 world;
void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}
void fragment() {
	vec2 p = world.xz;
	float big = texture(patches, p * 0.004).r;
	float mid = texture(patches, p * 0.023 + vec2(0.37, 0.11)).r;
	float fine = texture(grain, p * 0.6).r;
	float edge = texture(cracks, p * 0.03).r;
	// Worn: the binder gone grey where traffic runs; patched: newer, darker
	// asphalt laid over old trenches, in blocks.
	float wear = smoothstep(0.5, 0.78, big * 0.65 + mid * 0.35);
	vec2 block = floor(p / vec2(9.0, 5.0));
	float pick = fract(sin(dot(block, vec2(12.9898, 78.233))) * 43758.5453);
	float repair = step(0.93, pick) * step(0.45, mid);
	vec3 colour = mix(asphalt, worn, wear * 0.75);
	colour = mix(colour, patched, repair);
	colour *= 0.82 + 0.36 * fine;
	// Tar-sealed cracks along the cells' edges where the surface has
	// broken up (the worn ground, not everywhere), and oil soaked into it.
	float crack = (1.0 - smoothstep(0.0, 0.04, edge)) * smoothstep(0.5, 0.7, big * 0.55 + mid * 0.45);
	// Stains a metre or two across, soft-edged, where a vehicle stood: not
	// puddles of black (they were 5-10 m, hard-edged, and read as holes).
	float stain = smoothstep(0.78, 0.9, texture(patches, p * 0.17 + vec2(0.71, 0.29)).r);
	colour = mix(colour, vec3(0.025, 0.024, 0.022), max(crack * 0.85, stain * 0.42));
	ALBEDO = colour;
	ROUGHNESS = clamp(mix(0.62, 0.9, wear) - 0.25 * max(crack, stain), 0.2, 1.0);
	SPECULAR = 0.35;
	NORMAL_MAP = texture(relief, p * 0.6).rgb;
	NORMAL_MAP_DEPTH = 0.5;
}
"""
	_material = ShaderMaterial.new()
	_material.shader = shader
	_material.set_shader_parameter(&"patches", _noise(41, FastNoiseLite.TYPE_SIMPLEX_SMOOTH, 0.008, 4, 512, false))
	_material.set_shader_parameter(&"grain", _noise(42, FastNoiseLite.TYPE_SIMPLEX, 0.09, 2, 256, false))
	_material.set_shader_parameter(&"cracks", _cracks(43))
	_material.set_shader_parameter(&"relief", _noise(44, FastNoiseLite.TYPE_SIMPLEX, 0.06, 3, 256, true))
	return _material


static func _noise(seed_value: int, kind: FastNoiseLite.NoiseType, frequency: float, octaves: int,
		size: int, normal_map: bool) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.noise_type = kind
	noise.frequency = frequency
	noise.fractal_octaves = octaves
	var texture := NoiseTexture2D.new()
	texture.width = size
	texture.height = size
	texture.seamless = true
	texture.generate_mipmaps = true
	texture.noise = noise
	if normal_map:
		texture.as_normal_map = true
		texture.bump_strength = 1.4
	return texture


# The distance to the nearest cell edge (second-nearest minus nearest
# point): low along the edges, where the cracks run.
static func _cracks(seed_value: int) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.noise_type = FastNoiseLite.TYPE_CELLULAR
	noise.frequency = 0.02
	noise.fractal_type = FastNoiseLite.FRACTAL_NONE
	noise.cellular_distance_function = FastNoiseLite.DISTANCE_EUCLIDEAN
	noise.cellular_return_type = FastNoiseLite.RETURN_DISTANCE2_SUB
	noise.cellular_jitter = 1.0
	var texture := NoiseTexture2D.new()
	texture.width = 512
	texture.height = 512
	texture.seamless = true
	texture.generate_mipmaps = true
	texture.noise = noise
	return texture
