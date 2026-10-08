shader_type spatial;
render_mode cull_back, diffuse_burley, specular_schlick_ggx;

// RGB is source colour; alpha is linear source roughness. Opaque rendering
// never writes ALPHA, so the packed channel does not enable transparency.
uniform sampler2D brick_colour_roughness : source_color, filter_linear_mipmap, repeat_disable;
uniform sampler2D brick_normal_gl : hint_normal, filter_linear_mipmap, repeat_disable;
// A 128px patch spans this length. Preserve the source aspect and texel scale
// across all six crops instead of stretching each rectangle into a square.
uniform float detail_tile_m : hint_range(0.04, 0.3) = 0.11;
uniform float normal_strength : hint_range(0.0, 1.0) = 0.20;
const vec2 ATLAS_SIZE = vec2(768.0, 512.0);

instance uniform float refractory_seed : instance_index(0) = 0.0;
instance uniform vec4 refractory_uv_rect : instance_index(1) = vec4(0.0787760417, 0.0947265625, 0.17578125, 0.310546875);

varying vec3 body_position;
varying vec3 body_normal;
varying mat3 body_to_view_normal;

void vertex() {
	// KitView has already applied each part pose. These are rigid-body local
	// coordinates, so a rotating or moving shard carries its texture with it.
	body_position = VERTEX;
	body_normal = NORMAL;
	body_to_view_normal = MODELVIEW_NORMAL_MATRIX;
}

vec4 interior_uv(vec2 coordinates) {
	// Mirrored repetition is continuous at patch edges. zw retain the UV
	// derivative signs so the OpenGL detail normals mirror with the colour.
	vec2 patch_pixels = refractory_uv_rect.zw * ATLAS_SIZE;
	vec2 phase = fract(coordinates * (128.0 / patch_pixels) * 0.5) * 2.0;
	vec2 mirror_sign = step(vec2(1.0), phase) * 2.0 - 1.0;
	vec2 uv = refractory_uv_rect.xy + abs(phase - 1.0) * refractory_uv_rect.zw;
	return vec4(uv, mirror_sign);
}

float interior_lod(vec2 uv) {
	float footprint = max(length(dFdx(uv) * ATLAS_SIZE), length(dFdy(uv) * ATLAS_SIZE));
	// Each 256px cell contains only its own crop and >=32px edge gutters.
	// Stop at 16px mip footprints before any neighbouring cell can enter.
	return clamp(log2(max(footprint, 1.0)), 0.0, 4.0);
}

void fragment() {
	vec3 n = normalize(body_normal);
	vec3 weight = pow(abs(n), vec3(4.0));
	weight /= max(dot(weight, vec3(1.0)), 0.0001);
	vec3 axis_sign = step(vec3(0.0), n) * 2.0 - 1.0;
	vec3 p = body_position / detail_tile_m + vec3(refractory_seed * 11.0);
	vec4 ux = interior_uv(vec2(-axis_sign.x * p.z, p.y));
	vec4 uy = interior_uv(vec2(p.x, -axis_sign.y * p.z));
	vec4 uz = interior_uv(vec2(axis_sign.z * p.x, p.y));
	float lx = interior_lod(ux.xy);
	float ly = interior_lod(uy.xy);
	float lz = interior_lod(uz.xy);

	vec4 surface = textureLod(brick_colour_roughness, ux.xy, lx) * weight.x
		+ textureLod(brick_colour_roughness, uy.xy, ly) * weight.y
		+ textureLod(brick_colour_roughness, uz.xy, lz) * weight.z;
	// Low-contrast broad variation breaks mirrored colour repetition. It stays
	// in the same authored body frame and never changes geometry or normals.
	float broad_wear = sin(dot(body_position, vec3(13.1, 9.7, 7.3)) + refractory_seed * 19.0)
		* sin(dot(body_position, vec3(-6.3, 14.2, 11.5)) + refractory_seed * 7.0);
	vec3 clay_tint = mix(vec3(1.12, 1.0, 0.92), vec3(1.02, 1.03, 1.04), refractory_seed);
	ALBEDO = clamp(surface.rgb * clay_tint * 1.15 * (1.0 + broad_wear * 0.045), vec3(0.0), vec3(1.0));
	METALLIC = 0.0;
	SPECULAR = 0.28;
	ROUGHNESS = clamp(surface.a + broad_wear * 0.012, 0.78, 0.99);

	// Read RG only: normal-map compression may reconstruct/discard blue.
	vec2 nx = (textureLod(brick_normal_gl, ux.xy, lx).rg * 2.0 - 1.0) * ux.zw;
	vec2 ny = (textureLod(brick_normal_gl, uy.xy, ly).rg * 2.0 - 1.0) * uy.zw;
	vec2 nz = (textureLod(brick_normal_gl, uz.xy, lz).rg * 2.0 - 1.0) * uz.zw;
	vec3 detail = vec3(0.0, nx.y, -axis_sign.x * nx.x) * weight.x
		+ vec3(ny.x, 0.0, -axis_sign.y * ny.y) * weight.y
		+ vec3(axis_sign.z * nz.x, nz.y, 0.0) * weight.z;
	detail -= n * dot(detail, n);
	// A flat detail map retains the exact native facet normal. Fragment NORMAL
	// is view-space in Godot; transform the local result with the vertex matrix.
	NORMAL = normalize(body_to_view_normal * normalize(n + detail * normal_strength));
}
