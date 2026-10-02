shader_type spatial;
render_mode diffuse_burley, cull_disabled;
uniform float native_fold_m = 0.0;
varying vec3 leather_position;
float grain_hash(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 19.19);
	return fract((p.x + p.y) * p.z);
}
void vertex() {
    // A passive fold measured in native physics; never a time-driven wobble.
    float x = clamp(VERTEX.x / 0.84, -1.0, 1.0);
    float z = clamp(VERTEX.z / 0.65, -1.0, 1.0);
    float weight = (1.0-x*x)*(1.0-z*z);
    VERTEX.y += native_fold_m * weight;
    vec3 tangent_x = vec3(1.0, -2.0*x/0.84*(1.0-z*z)*native_fold_m, 0.0);
    vec3 tangent_z = vec3(0.0, -2.0*z/0.65*(1.0-x*x)*native_fold_m, 1.0);
    NORMAL = normalize(NORMAL + cross(tangent_z,tangent_x) - vec3(0.0,1.0,0.0));
    leather_position = VERTEX;
}
void fragment() {
	float grain = grain_hash(floor(leather_position * 320.0));
	float worn = 0.5 + 0.5 * sin(leather_position.x * 7.0 + leather_position.z * 11.0);
	ALBEDO = mix(vec3(0.075, 0.021, 0.009), vec3(0.245, 0.092, 0.032), grain * 0.34 + worn * 0.37);
	ROUGHNESS = 0.68 + grain * 0.22;
	NORMAL_MAP = vec3(0.5 + (grain - 0.5) * 0.12, 0.5 + (worn - 0.5) * 0.035, 1.0);
	NORMAL_MAP_DEPTH = 0.28;
}
