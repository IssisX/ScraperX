shader_type spatial;
render_mode specular_schlick_ggx;
varying float strength;
void vertex() { strength = INSTANCE_CUSTOM.r; }
void fragment() {
    ALBEDO = mix(vec3(0.20, 0.095, 0.012), vec3(0.85, 0.38, 0.025), strength);
    EMISSION = vec3(1.0, 0.34, 0.025) * strength * 2.4;
    ROUGHNESS = 0.3;
    METALLIC = 0.0;
}
