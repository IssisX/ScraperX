extends Node3D
# The night's lamps as the eye sees them in damp mountain air: a halo round
# every lamp of the light rig, a soft cone of lit air under each yard flood,
# and the red obstruction lights a 1.6 km structure must carry (the
# neighbouring shaft's corners, and the main frame's continuation), flashing
# together. Presentation only.
#
# Drawn outside the part system: two MultiMeshes of camera-facing cards, so
# nothing here is a part, a solid or a light (the solids export and the
# floating audit never see them), and the whole is two draw calls. The
# obstruction lights are glow only, never lamps: Compatibility lights at most
# eight lamps per mesh and the batcher limits each lamp's reach.
#
# There is no volumetric fog and no depth read on the phone (a depth copy is a
# full extra pass on a tiled GPU), so a card is pulled toward the eye by its
# own radius: it sits in front of the fitting it surrounds and is still hidden
# by anything nearer the eye.
#
# sky_cycle.gd finds this node among the rig's children (set_night) and fades
# it with the day: nothing shows by day.

# Halo radius per metre of the lamp's reach, and its floor and ceiling.
const HALO_PER_REACH := 0.085
const HALO_MIN_M := 0.9
const HALO_MAX_M := 3.2
# A lamp that hangs over open ground (the yard's masts) carries this meta and
# gets a cone of lit air under it as well as its halo. Sibling lamps of one
# name are renamed by the tree, so the name cannot say which they are.
const CONE_META := &"glow_cone"
# The cone of lit air under a yard flood, its radius at the ground.
const CONE_FOOT_M := 4.6
const CONE_TOP_M := 0.5
# Obstruction lights: aviation red, about 30 flashes a minute, all at once.
const BEACON_COLOR := Color(1.0, 0.12, 0.06)
const BEACON_RADIUS_M := 3.0
# The flash period is the halo shader's PERIOD (2 s).

var _halo_material: ShaderMaterial
var _cone_material: ShaderMaterial


# lamps: the rig's OmniLight3Ds; beacons: where the obstruction lights hang,
# in the rig's space (main_world.gd _build_lighting). Call once the rig is
# built.
func build(lamps: Array, beacons: PackedVector3Array) -> void:
	var halos: Array = []   # [position, radius, colour, flashes]
	var cones: Array = []   # [head position, length, colour]
	for node in lamps:
		var lamp := node as OmniLight3D
		var radius := clampf(lamp.omni_range * HALO_PER_REACH, HALO_MIN_M, HALO_MAX_M)
		halos.append([lamp.position, radius, lamp.light_color, false])
		if lamp.has_meta(CONE_META):
			cones.append([lamp.position, lamp.position.y, lamp.light_color])
	for at in beacons:
		halos.append([at, BEACON_RADIUS_M, BEACON_COLOR, true])
	_halo_material = ShaderMaterial.new()
	_halo_material.shader = _shader(HALO_SHADER)
	_cone_material = ShaderMaterial.new()
	_cone_material.shader = _shader(CONE_SHADER)
	_add_cards("LampHalos", halos.size(), _halo_material, func(mm: MultiMesh) -> void:
		for i in halos.size():
			var h: Array = halos[i]
			mm.set_instance_transform(i, Transform3D(Basis.IDENTITY, h[0]))
			mm.set_instance_color(i, h[2])
			mm.set_instance_custom_data(i, Color(h[1], 1.0 if h[3] else 0.0, 0.0, 0.0)))
	_add_cards("LampCones", cones.size(), _cone_material, func(mm: MultiMesh) -> void:
		for i in cones.size():
			var c: Array = cones[i]
			mm.set_instance_transform(i, Transform3D(Basis.IDENTITY, c[0]))
			mm.set_instance_color(i, c[2])
			mm.set_instance_custom_data(i, Color(c[1], CONE_TOP_M, CONE_FOOT_M, 0.0)))
	set_night(0.0)


# 0 by day .. 1 at night.
func set_night(night: float) -> void:
	for material in [_halo_material, _cone_material]:
		if material != null:
			(material as ShaderMaterial).set_shader_parameter(&"night", night)
	visible = night > 0.002


func _add_cards(card_name: String, count: int, material: ShaderMaterial, fill: Callable) -> void:
	if count == 0:
		return
	var quad := QuadMesh.new()
	quad.size = Vector2(2.0, 2.0)
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = true
	mm.use_custom_data = true
	mm.mesh = quad
	mm.instance_count = count
	fill.call(mm)
	var cards := MultiMeshInstance3D.new()
	cards.name = card_name
	cards.multimesh = mm
	cards.material_override = material
	cards.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# The cards move in the vertex shader; the bounds must hold every pose.
	cards.custom_aabb = AABB(Vector3(-4000.0, -100.0, -4000.0), Vector3(8000.0, 2000.0, 8000.0))
	add_child(cards)


static func _shader(code: String) -> Shader:
	var shader := Shader.new()
	shader.code = code
	return shader


# A halo: a disc square to the eye, pulled toward it by its radius. Far away
# it keeps a few pixels' size (a lamp a kilometre off is still a point of
# light), its energy spread over the larger card. Fog is left off and the
# air's loss done here, so the additive card never paints the haze's colour.
const HALO_SHADER := """
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, skip_vertex_transform;
uniform float night = 0.0;
varying vec3 tint;
varying float energy;
varying vec2 disc;
const float MIN_ANGLE = 0.0075;
const float PERIOD = 2.0;
void vertex() {
	disc = VERTEX.xy;
	vec3 centre = (VIEW_MATRIX * vec4(MODEL_MATRIX[3].xyz, 1.0)).xyz;
	float radius = INSTANCE_CUSTOM.r;
	float dist = max(length(centre), 0.01);
	float size = max(radius, dist * MIN_ANGLE);
	energy = clamp(pow(radius / size, 2.0), 0.35, 1.0) * exp(-dist / 2600.0);
	if (INSTANCE_CUSTOM.g > 0.5) {
		// About 30 a minute: a quick rise, a short hold, a slow fall. A
		// point a kilometre off must still read, so it is not let fade.
		float t = fract(TIME / PERIOD);
		energy = max(energy, 0.55) * smoothstep(0.0, 0.06, t) * (1.0 - smoothstep(0.22, 0.55, t)) * 3.0;
	}
	tint = COLOR.rgb;
	centre *= max(dist - radius, 0.05) / dist;
	VERTEX = centre + vec3(VERTEX.xy * size, 0.0);
}
void fragment() {
	float r2 = dot(disc, disc);
	float glow = exp(-r2 * 9.0) * 0.9 + exp(-r2 * 2.6) * 0.22;
	glow *= 1.0 - smoothstep(0.7, 1.0, r2);
	ALBEDO = tint * glow * energy * night;
}
"""

# A cone of lit air: a card turned about the vertical to face the eye, from
# the lamp head down to the ground, wide at its foot, brightest under the
# head and gone before it meets the ground.
const CONE_SHADER := """
shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_disabled, shadows_disabled, fog_disabled, skip_vertex_transform;
uniform float night = 0.0;
varying vec3 tint;
varying vec2 cone;
void vertex() {
	vec3 head = MODEL_MATRIX[3].xyz;
	float length_m = INSTANCE_CUSTOM.r;
	float down = (1.0 - VERTEX.y) * 0.5;
	cone = vec2(VERTEX.x, down);
	vec3 to_eye = INV_VIEW_MATRIX[3].xyz - head;
	vec3 across = normalize(cross(vec3(0.0, 1.0, 0.0), to_eye + vec3(0.001, 0.0, 0.0)));
	float width = mix(INSTANCE_CUSTOM.g, INSTANCE_CUSTOM.b, down);
	vec3 world = head + vec3(0.0, -length_m * down, 0.0) + across * VERTEX.x * width;
	tint = COLOR.rgb;
	VERTEX = (VIEW_MATRIX * vec4(world, 1.0)).xyz;
}
void fragment() {
	float edge = 1.0 - cone.x * cone.x;
	float air = edge * edge * exp(-cone.y * 1.6) * smoothstep(0.0, 0.06, cone.y)
		* (1.0 - smoothstep(0.75, 0.98, cone.y));
	ALBEDO = tint * air * 0.3 * night;
}
"""
