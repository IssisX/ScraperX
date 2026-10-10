shader_type spatial;
render_mode diffuse_burley;

// Service plank only. UV/UV2 retain the actual 2.4 m board coordinates across
// all twelve native bodies. Shader detail never displaces native geometry.
uniform vec4 timber_light : source_color = vec4(0.725, 0.627, 0.467, 1.0);
uniform vec4 timber_dark : source_color = vec4(0.396, 0.314, 0.227, 1.0);
uniform vec4 fresh_light : source_color = vec4(0.83, 0.73, 0.56, 1.0);
uniform vec4 fresh_dark : source_color = vec4(0.54, 0.39, 0.24, 1.0);
uniform int broken_joint_mask = 0;

varying vec3 timber_position;
varying float end_face;
varying vec3 width_normal_view;
varying vec3 thickness_normal_view;
varying vec3 length_normal_view;

float hash2(vec2 p) {
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}

float noise2(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash2(i), hash2(i + vec2(1.0, 0.0)), f.x),
		mix(hash2(i + vec2(0.0, 1.0)), hash2(i + vec2(1.0, 1.0)), f.x), f.y);
}

// Average subpixel ring/fiber contrast instead of letting it sparkle as the
// player moves. Derivatives affect appearance only, never the native surface.
float resolved_band(float phase) {
	return 1.0 - smoothstep(0.65, 3.0, fwidth(phase));
}

void vertex() {
	// Width Z, thickness Y, length X in native part-local metres. The source
	// mesh normals/tangents and vertices remain untouched. Carry native axes
	// into view space so bump detail follows rotating fragments correctly.
	timber_position = vec3(UV.x, UV2.x, UV.y);
	end_face = UV2.y;
	width_normal_view = MODELVIEW_NORMAL_MATRIX * vec3(0.0, 0.0, 1.0);
	thickness_normal_view = MODELVIEW_NORMAL_MATRIX * vec3(0.0, 1.0, 0.0);
	length_normal_view = MODELVIEW_NORMAL_MATRIX * vec3(1.0, 0.0, 0.0);
}

void fragment() {
	vec3 p = timber_position;
	bool original_end = end_face < -0.5;
	int interface_index = int(end_face + 0.5) - 1;
	bool fresh_end = false;
	if (interface_index >= 0 && interface_index < 11) {
		fresh_end = (broken_joint_mask & (1 << interface_index)) != 0;
	}
	bool cut_end = original_end || fresh_end;

	// A single coherent growth-ring field supplies the same cross-section
	// to side grain and exposed ends. Heart lies outside this sawn board;
	// broad grain bends gently rather than repeating once per solver cell.
	float weather = noise2(vec2(p.z * 2.3, p.x * 12.0));
	float drift = 0.0018 * sin(p.z * 3.7) + 0.0008 * sin(p.z * 11.0);
	vec2 radial = p.xy - vec2(0.055 + drift, -0.075);
	float radius = max(length(radial), 0.001);
	// Reuse the same broad field for modest growth irregularity. Bands and
	// fibers wander together along the original board instead of reading as
	// perfectly printed parallel stripes; no additional noise evaluations.
	float growth_warp = (weather - 0.5) * 4.0;
	float ring_phase = radius * 1000.0 + growth_warp;
	float ring_resolution = resolved_band(ring_phase);
	float annual_ring = 0.5 + 0.5 * sin(ring_phase);
	// Retain the narrow dark band's mean when minified: thresholding an
	// already averaged sine would erase latewood and bleach distant timber.
	float latewood = mix(0.32, smoothstep(0.56 + weather * 0.04,
		0.92 + weather * 0.04, annual_ring), ring_resolution);
	float fiber_phase = p.x * 3400.0 + sin(p.z * 15.0) * 0.8 + growth_warp * 1.7;
	float fiber_resolution = resolved_band(fiber_phase);
	float fibers = 0.5 + 0.5 * sin(fiber_phase) * fiber_resolution;
	// Two four-corner noise evaluations: fewer hashes than the former shared
	// three-volume-noise shader. End pores replace longitudinal fibers.
	vec2 pore_coords = cut_end ? p.xy * vec2(1400.0, 1400.0)
		: vec2(p.x * 1600.0, p.z * 32.0);
	float pore_resolution = 1.0 - smoothstep(0.45, 1.5, length(fwidth(pore_coords)));
	float pores = mix(0.5, noise2(pore_coords), pore_resolution);

	float tone = clamp(0.76 - latewood * 0.42 + (weather - 0.5) * 0.24
		+ (fibers - 0.5) * (cut_end ? 0.03 : 0.12), 0.0, 1.0);
	vec3 surface_color = mix(timber_dark.rgb, timber_light.rgb, tone);
	// Weathered outer sawn ends retain the old board's tones. Newly exposed
	// native interfaces reveal warm pale interior fibers, never black seams
	// or a painted crack on an intact joint. No splinter geometry is invented.
	vec3 fresh_color = mix(fresh_dark.rgb, fresh_light.rgb, tone);
	ALBEDO = fresh_end ? fresh_color : surface_color;
	if (cut_end) {
		ALBEDO *= 0.94 + pores * 0.09;
	}
	ROUGHNESS = clamp((cut_end ? 0.90 : 0.80) + (pores - 0.5) * 0.08
		+ (weather - 0.5) * 0.06, 0.74, 0.96);
	METALLIC = 0.0;
	SPECULAR = 0.28;

	// Analytic gradients in board metres: subtle ~25 micrometre ring relief
	// and longitudinal fiber ridges. Project off the actual face normal;
	// no reliance on BoxMesh tangents after the continuous UV remapping.
	vec2 ring_gradient = radial / radius * cos(ring_phase) * 0.025 * ring_resolution;
	float fiber_gradient = cos(fiber_phase) * 0.0204 * fiber_resolution;
	vec3 relief = width_normal_view * (ring_gradient.x + (cut_end ? 0.0 : fiber_gradient))
		+ thickness_normal_view * ring_gradient.y;
	if (!cut_end) {
		float drift_gradient = 0.00666 * cos(p.z * 3.7) + 0.0088 * cos(p.z * 11.0);
		relief += length_normal_view * (-ring_gradient.x * drift_gradient);
	}
	vec3 base_normal = normalize(NORMAL);
	NORMAL = normalize(base_normal - relief + base_normal * dot(base_normal, relief));
}
