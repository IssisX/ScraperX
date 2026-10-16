shader_type spatial;
render_mode specular_schlick_ggx;
varying vec2 ground_xz;
float grain(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float patch(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(grain(cell), grain(cell + vec2(1.0, 0.0)), f.x),
        mix(grain(cell + vec2(0.0, 1.0)), grain(cell + vec2(1.0)), f.x), f.y);
}
void vertex() { ground_xz = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz; }
void fragment() {
    float aggregate = grain(floor(ground_xz * 95.0));
    float weather = patch(ground_xz * 0.12);
    float repairs = smoothstep(0.60, 0.68, patch(ground_xz * 0.055));
    vec3 dry = mix(vec3(0.071, 0.075, 0.071), vec3(0.13, 0.12, 0.098), weather);
    dry += (aggregate - 0.5) * 0.035;
    // Historic worn tire lanes in the freight apron, not live simulated tracks.
    float lane = 1.0 - smoothstep(1.0, 2.1, abs(abs(ground_xz.x + 96.0) - 1.15));
    lane *= smoothstep(-102.0, -80.0, ground_xz.y) * (1.0 - smoothstep(-30.0, -22.0, ground_xz.y));
    ALBEDO = mix(dry, dry * 0.58, repairs * 0.32 + lane * 0.24);
    ROUGHNESS = mix(0.94, 0.63, repairs);
    METALLIC = 0.0;
}
