shader_type spatial;
// Capsule ambient occlusion and a soft capsule shadow on a receiver (probe),
// for gl_compatibility: up to 12 capsules from her limbs as uniforms.
// Occlusion of a sphere (Quilez) applied at the closest point of each capsule.
uniform vec3 albedo : source_color = vec3(0.32, 0.31, 0.30);
uniform int count = 0;
uniform vec4 cap_a[12];   // xyz end A, w radius
uniform vec3 cap_b[12];   // end B
uniform vec3 key_dir = vec3(0.3, 1.0, 0.2);  // toward the key light (world)
uniform float strength = 1.0;
varying vec3 w_pos;
varying vec3 w_nrm;
void vertex() {
	w_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	w_nrm = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}
float occ_sphere(vec3 p, vec3 n, vec3 c, float r) {
	vec3 d = c - p;
	float l = length(d);
	return clamp(dot(n, d / l) * (r * r) / (l * l), 0.0, 1.0);
}
float shadow_sphere(vec3 p, vec3 L, vec3 c, float r) {
	// cone test: soft shadow toward the key light (angular light size ~0.25)
	vec3 d = c - p;
	float t = dot(d, L);
	if (t < 0.0) return 1.0;
	float dist = length(d - L * t);
	float pen = 0.25 * t + 0.01;
	return clamp((dist - r + pen) / (2.0 * pen), 0.0, 1.0);
}
void fragment() {
	float ao = 1.0;
	float sh = 1.0;
	vec3 L = normalize(key_dir);
	for (int i = 0; i < 12; i++) {
		if (i >= count) break;
		vec3 a = cap_a[i].xyz;
		vec3 b = cap_b[i];
		float r = cap_a[i].w;
		vec3 ab = b - a;
		float h = clamp(dot(w_pos - a, ab) / dot(ab, ab), 0.0, 1.0);
		vec3 c = a + ab * h;
		ao *= 1.0 - occ_sphere(w_pos, w_nrm, c, r) * 0.9;
		// shadow: closest point of the segment to the light ray
		vec3 m = w_pos + L * dot(c - w_pos, L);
		float h2 = clamp(dot(m - a, ab) / dot(ab, ab), 0.0, 1.0);
		sh = min(sh, shadow_sphere(w_pos, L, a + ab * h2, r));
	}
	float k = mix(1.0, ao * mix(0.35, 1.0, sh), strength);
	ALBEDO = albedo * k;
	ROUGHNESS = 0.8;
	METALLIC = 0.3;
}
