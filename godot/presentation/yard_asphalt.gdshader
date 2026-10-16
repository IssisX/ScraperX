shader_type spatial;
render_mode specular_schlick_ggx;
varying vec2 ground_xz;
float grain(vec2 p) {
    vec3 h = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    h += dot(h, h.yzx + 33.33);
    return fract((h.x + h.y) * h.z);
}
float patch(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(grain(cell), grain(cell + vec2(1.0, 0.0)), f.x),
        mix(grain(cell + vec2(0.0, 1.0)), grain(cell + vec2(1.0)), f.x), f.y);
}
void vertex() { ground_xz = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xz; }
void fragment() {
    // Millimetre aggregate must disappear below a pixel instead of turning
    // the entire distant apron into crawling salt-and-pepper noise.
    float footprint = max(fwidth(ground_xz.x), fwidth(ground_xz.y));
    float aggregate_weight = 1.0 - smoothstep(0.5, 1.5, footprint * 95.0);
    float aggregate = (grain(floor(ground_xz * 95.0)) - 0.5) * aggregate_weight;
    float weather = patch(ground_xz * 0.12);
    float repairs = smoothstep(0.60, 0.68, patch(ground_xz * 0.055));
    vec3 dry = mix(vec3(0.047, 0.055, 0.058), vec3(0.11, 0.101, 0.082), weather);
    dry += aggregate * 0.035;
    // The receiving apron has a lighter, worn topcoat. Its broad boundary
    // frames the actual working aisle; these are material changes on grade,
    // never raised slabs, new collision surfaces or simulated tire receipts.
    vec2 apron_distance = abs(ground_xz - vec2(-95.0, -59.0)) - vec2(17.0, 36.0);
    float apron = 1.0 - smoothstep(-0.4, 0.5, max(apron_distance.x, apron_distance.y));
    dry = mix(dry, mix(vec3(0.083, 0.088, 0.084), vec3(0.137, 0.128, 0.108), weather)
        + aggregate * 0.025, apron * 0.75);
    // Historic worn tire lanes in the freight apron, not live simulated tracks.
    float line_aa = max(footprint, 0.035);
    float lane = 1.0 - smoothstep(0.15, 0.35 + line_aa, abs(abs(ground_xz.x + 99.5) - 0.68));
    lane *= smoothstep(-102.0, -80.0, ground_xz.y) * (1.0 - smoothstep(-30.0, -22.0, ground_xz.y));
    float turn_radius = length(ground_xz - vec2(-96.0, -54.0));
    float turn = 1.0 - smoothstep(0.12, 0.29 + line_aa, abs(abs(turn_radius - 5.7) - 0.68));
    turn *= (1.0 - smoothstep(-53.0, -49.0, ground_xz.y)) * apron;
    float worn = max(lane, turn) * mix(0.45, 1.0, weather);
    ALBEDO = mix(dry, dry * 0.48, repairs * 0.32 + worn * 0.40);
    ROUGHNESS = mix(0.94, 0.63, repairs);
    METALLIC = 0.0;
}
