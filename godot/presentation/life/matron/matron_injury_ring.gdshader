shader_type spatial;
// J-059 black eye ring (and swollen lid), drawn ON her face (Cory 11-08:
// the flat quad stood off the eye like a monocle from the side, and the
// #309 grazing fade hid it there instead of curving it round the face).
// The mesh is a copy of her own skin triangles round the eye socket
// (matron_injury.gd _skin_mesh_for), rigid on the head bone, with vertices
// in head-local metres relative to the socket centre. The ring texture is
// projected onto that skin along the head's forward axis, so the bruise
// lies on the lids, the cheek top and the side of the face and follows
// their curve from every angle; nothing stands off the skin.
// gl_compatibility has no Decal nodes, hence the skin copy.
// The cheek bruises use it too (blend on): the cheek skin also follows the
// jaw and neck, so that copy keeps her weights and blends like her skin.
// Cost: a few hundred triangles, drawn only while the eye or cheek is injured.
render_mode blend_mix, depth_draw_never, cull_back;

uniform sampler2D albedo_texture : source_color, filter_linear, repeat_disable;
uniform vec4 albedo_color : source_color = vec4(1.0);
// Where the texture lands, in head-local metres off the socket centre:
// its centre and half size (x right = her left, y up).
uniform vec2 uv_center = vec2(0.0, 0.0);
uniform vec2 uv_half = vec2(0.024, 0.024);
// Lift off the skin along its normal, against z-fighting with the skin it
// copies: under a millimetre, too small to see from any side.
uniform float push = 0.0004;
// The cheek copy's skin blend: CUSTOM0 holds each vertex's weights on up
// to four bones besides the head, bone_a..bone_d those bones' current
// poses against the head's in the copy's frame (matron_injury.gd
// _update_cheek); the head keeps the rest of the weight. The texture stays
// on the bind-pose skin, so it moves with the cheek. Off for the eye,
// whose skin is all head.
uniform bool blend = false;
uniform mat4 bone_a = mat4(1.0);
uniform mat4 bone_b = mat4(1.0);
uniform mat4 bone_c = mat4(1.0);
uniform mat4 bone_d = mat4(1.0);

varying vec3 local;
varying float facing_out;

void vertex() {
	local = VERTEX;
	// The skin copy holds the inside of the lid folds too; a surface that
	// faces back into the head takes no bruise.
	facing_out = smoothstep(-0.35, 0.05, NORMAL.z);
	if (blend) {
		vec4 w = CUSTOM0;
		float wh = 1.0 - w.x - w.y - w.z - w.w;
		vec4 p = vec4(VERTEX, 1.0);
		VERTEX = wh * VERTEX + (w.x * (bone_a * p) + w.y * (bone_b * p) + w.z * (bone_c * p) + w.w * (bone_d * p)).xyz;
		NORMAL = normalize(wh * NORMAL + w.x * (mat3(bone_a) * NORMAL) + w.y * (mat3(bone_b) * NORMAL)
				+ w.z * (mat3(bone_c) * NORMAL) + w.w * (mat3(bone_d) * NORMAL));
	}
	VERTEX += NORMAL * push;
}

void fragment() {
	vec2 uv = vec2((local.x - uv_center.x) / uv_half.x, (uv_center.y - local.y) / uv_half.y) * 0.5 + 0.5;
	float inside = step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);
	vec4 tex = texture(albedo_texture, clamp(uv, vec2(0.0), vec2(1.0)));
	ALBEDO = tex.rgb * albedo_color.rgb;
	ALPHA = tex.a * albedo_color.a * inside * facing_out;
	ROUGHNESS = 0.6;
}
