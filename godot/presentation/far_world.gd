extends Node3D
## The far world (environment job 4, lane C): the valley floor as ground, and
## the tower's long shadow on it, past the reach of the shadow cascades.
## Presentation only: nothing here collides or feeds the native. The valley
## floor's solids stay the hidden ValleyFloor boxes (solid_export.gd reads
## them); this draws their tops, drawn outside the part system so the batcher
## (which would draw it flat gray from BATCH_NEAR_RANGE out) leaves it alone.
##
## What lies on the floor has its reason in the works: the haul road and the
## rail spur leaving the basin to the south-east, settling ponds and spoil
## below the works, worn tracks, and meadow, scrub, scree and wet ground
## between. All of it is paint on the floor (flush, NON_SOLID in spirit).
##
## The tower's shadow is drawn in the floor's shader: a ray from each point
## toward the sun, tested against the tower's boxes (the batched tower's own
## bounds in TOWER_SLICES height slices, read once after it is built). The
## cascades shade near ground already, so this fades in past SHADOW_NEAR_M.
## The horizon past the far plane is the sky's (sky_horizon.gdshaderinc); this
## node feeds it the eye's height.

const TOWER_SLICES := 8          # measured slices round the Stack's foot
const MEASURED_TOP_M := 480.0    # what TowerPresentation holds there reaches about 420 m
# The column: its decks and rings span these (the spots of the plan's
# appendix A, 154 m at z -128 to the summit deck, 803.65 m at z -187.5).
const COLUMN_LOW := Vector3(-30.0, 0.0, -200.0)
const COLUMN_HIGH := Vector3(30.0, 806.0, -120.0)
const TWIN_HALF := Vector3(46.0, 800.0, 40.0)   # main_world.gd's TowerMass, 92 x 1600 x 80
const TOWER_REACH_M := 90.0      # a tower's boxes lie within half this of its axis, none wider
const SHADOW_NEAR_M := 200.0     # the cascades reach 220 m (main.tscn)
const GRID := 48                 # quads per side of each floor piece (vertex fog)

var _ground: ShaderMaterial
var _sky: ShaderMaterial
var _sun: DirectionalLight3D
var _main: Node
var _tower_read := false


# Draws the tops of the valley floor's boxes ([[size, centre]...]) at `top`.
func build(main: Node, pieces: Array, top: float) -> void:
	_main = main
	name = "FarWorld"
	_ground = _ground_material()
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for piece in pieces:
		var size: Vector3 = piece[0]
		var centre: Vector3 = piece[1]
		for i in GRID:
			for j in GRID:
				var a := Vector3(centre.x + size.x * (float(i) / GRID - 0.5), top, centre.z + size.z * (float(j) / GRID - 0.5))
				var dx := Vector3(size.x / GRID, 0.0, 0.0)
				var dz := Vector3(0.0, 0.0, size.z / GRID)
				st.set_normal(Vector3.UP)
				for p in [a, a + dx, a + dz, a + dx, a + dx + dz, a + dz]:
					st.add_vertex(p)
	var floor_mesh := MeshInstance3D.new()
	floor_mesh.name = "FarGround"
	floor_mesh.mesh = st.commit()
	floor_mesh.material_override = _ground
	floor_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(floor_mesh)


func _process(_delta: float) -> void:
	if _main == null:
		return
	if _sun == null:
		_sun = _main.get_node_or_null("Overcast") as DirectionalLight3D
		var env := (_main.get_node_or_null("Environment") as WorldEnvironment)
		if env != null and env.environment != null and env.environment.sky != null:
			_sky = env.environment.sky.sky_material as ShaderMaterial
	if not _tower_read:
		_tower_read = _read_tower()
	var cam := get_viewport().get_camera_3d()
	var eye := cam.global_position if cam != null else Vector3.ZERO
	var quality := int(_main._settings.quality) if "_settings" in _main and _main._settings != null else 2
	var detail := 0.0 if quality <= 0 else 1.0
	_ground.set_shader_parameter("detail", detail)
	var day := 1.0
	var moon := false
	if _sky != null:
		_sky.set_shader_parameter("eye_height", eye.y)
		_sky.set_shader_parameter("horizon_detail", detail)
		day = float(_sky.get_shader_parameter("day_factor"))
		moon = bool(_sky.get_shader_parameter("light_is_moon"))
		# What the water mirrors: the dome's own two colours at this hour.
		var zen := _sky_colour("zenith_night").lerp(_sky_colour("zenith_day"), day)
		var hor := _sky_colour("horizon_night").lerp(_sky_colour("horizon_day"), day)
		for m in [_ground, water_material()]:
			m.set_shader_parameter("sky_zenith", zen)
			m.set_shader_parameter("sky_horizon", hor)
	if _sun != null:
		var to_sun := _sun.global_transform.basis.z.normalized()
		_ground.set_shader_parameter("to_sun", to_sun)
		var strength := 0.0 if moon or not _sun.visible else day * smoothstep(0.0, 0.08, to_sun.y)
		_ground.set_shader_parameter("shadow_strength", strength * _sun.light_energy / maxf(_sun.light_energy, 1.0))


# A colour of the sky's dome: as sky_cycle.gd set it, else the shader's own.
func _sky_colour(uniform: StringName) -> Color:
	var value: Variant = _sky.get_shader_parameter(uniform)
	if value == null and _sky.shader != null:
		value = RenderingServer.shader_get_parameter_default(_sky.shader.get_rid(), uniform)
	if value is Color:
		return value
	if value is Vector3:
		return Color(value.x, value.y, value.z)
	return Color(0.5, 0.6, 0.7)


# The towers' boxes. Known: the tower's column over the Stack up to its
# summit deck (COLUMN_*: its decks and rings, 154 m to the summit at z -187)
# and the twin's TowerMass. Measured: what
# was built under TowerPresentation about the Stack (the plant, buttresses and
# skin round its foot; after batching, the batches' bounds), in TOWER_SLICES
# height slices. The kit's own bodies are drawn by their views and are inside
# the column. False until built.
func _read_tower() -> bool:
	var root := _main.get_node_or_null("TowerPresentation") as Node3D
	if root == null or root.get_child_count() == 0:
		return false
	var stack_at: Vector3 = _main.STACK_CENTER
	var twin: Vector3 = _main.TWIN_TOWER_CENTER
	var lows: Array[Vector3] = [COLUMN_LOW, twin - TWIN_HALF]
	var highs: Array[Vector3] = [COLUMN_HIGH, twin + TWIN_HALF]
	var first := lows.size()
	for k in TOWER_SLICES:
		lows.append(Vector3.INF)
		highs.append(-Vector3.INF)
	var slice_h := MEASURED_TOP_M / TOWER_SLICES
	var axis := Vector2(stack_at.x, stack_at.z)
	var nodes: Array[Node] = root.get_children()
	while not nodes.is_empty():
		var node: Node = nodes.pop_back()
		nodes.append_array(node.get_children())
		var mi := node as MeshInstance3D
		if mi == null or mi.mesh == null or not mi.visible:
			continue
		var box := mi.global_transform * mi.mesh.get_aabb()
		var c := box.get_center()
		if box.end.y < 4.0 or box.size.x > TOWER_REACH_M or box.size.z > TOWER_REACH_M \
				or Vector2(c.x, c.z).distance_to(axis) > TOWER_REACH_M * 0.5:
			continue
		for k in TOWER_SLICES:
			var lo := slice_h * k
			var hi := lo + slice_h
			if box.end.y < lo or box.position.y > hi:
				continue
			var n := first + k
			lows[n] = Vector3(minf(lows[n].x, box.position.x), lo, minf(lows[n].z, box.position.z))
			highs[n] = Vector3(maxf(highs[n].x, box.end.x), minf(hi, maxf(highs[n].y, box.end.y)), maxf(highs[n].z, box.end.z))
	while lows.size() < 16:
		lows.append(Vector3.INF)
		highs.append(-Vector3.INF)
	for n in lows.size():
		if lows[n].x == INF:
			lows[n] = Vector3(0.0, -10.0, 0.0)
			highs[n] = Vector3(0.0, -10.0, 0.0)
	_ground.set_shader_parameter("tower_low", PackedVector3Array(lows))
	_ground.set_shader_parameter("tower_high", PackedVector3Array(highs))
	return true


static var _water: ShaderMaterial


# Standing water on the yard (environment job 11): a thin dark sheet that
# mirrors the sky by the Fresnel term (no screen-space reflections on either
# renderer, so it mirrors the dome's colours, fed by FarWorld each frame),
# stirred by a little wind ripple, with a damp rim. Still NON_SOLID paint
# (solid_export.gd: StandingWater).
static func water_material() -> ShaderMaterial:
	if _water != null:
		return _water
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_back;
uniform vec3 sky_zenith : source_color = vec3(0.17, 0.34, 0.66);
uniform vec3 sky_horizon : source_color = vec3(0.66, 0.73, 0.80);
uniform vec3 bed : source_color = vec3(0.06, 0.058, 0.05);
// The slabs it lies on (centre x, z, half x, z), so its edge can wander in from theirs.
uniform vec4 slabs[2];
varying vec3 world;
float h21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}
float vn(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(h21(i), h21(i + vec2(1.0, 0.0)), u.x), mix(h21(i + vec2(0.0, 1.0)), h21(i + vec2(1.0, 1.0)), u.x), u.y);
}
void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}
void fragment() {
	vec2 p = world.xz;
	// Ragged edges: the water fills the low ground of its slab, not a box.
	float inside = 1e5;
	for (int k = 0; k < 2; k++) {
		vec2 q = abs(p - slabs[k].xy) - slabs[k].zw;
		inside = min(inside, length(max(q, 0.0)) + min(max(q.x, q.y), 0.0));
	}
	float edge = vn(p * 0.35) * 0.6 + vn(p * 1.3) * 0.4;
	float depth = -inside - 1.0 - 5.0 * edge;   // metres in from its ragged shore
	if (depth < 0.0) {
		discard;
	}
	float water = smoothstep(0.3, 1.2, depth);
	vec3 eye_w = (INV_VIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	vec3 view = normalize(world - eye_w);
	vec2 ripple = (vec2(vn(p * 2.0 + vec2(TIME * 0.6, 0.0)), vn(p * 2.0 + vec2(3.0, -TIME * 0.45))) - 0.5) * 0.06;
	vec3 n = normalize(vec3(ripple.x, 1.0, ripple.y));
	vec3 r = reflect(view, n);
	vec3 mirrored = mix(sky_horizon, sky_zenith, pow(clamp(r.y, 0.0, 1.0), 0.55));
	float fresnel = 0.02 + 0.98 * pow(1.0 - clamp(-dot(view, n), 0.0, 1.0), 5.0);
	vec3 damp = vec3(0.07, 0.065, 0.05);
	ALBEDO = mix(damp, bed * (1.0 - fresnel), water);
	EMISSION = mirrored * fresnel * water * 0.7;
	ROUGHNESS = mix(0.7, 0.04, water);
	SPECULAR = mix(0.35, 0.6, water);
}
"""
	_water = ShaderMaterial.new()
	_water.shader = shader
	return _water


static func _ground_material() -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_back;
uniform float detail = 1.0;
uniform vec3 to_sun = vec3(0.3, 0.8, 0.2);
uniform float shadow_strength = 0.0;
uniform vec3 tower_low[16];
uniform vec3 tower_high[16];
uniform vec3 meadow : source_color = vec3(0.247, 0.275, 0.204);
uniform vec3 dry : source_color = vec3(0.36, 0.35, 0.25);
uniform vec3 scrub : source_color = vec3(0.17, 0.2, 0.14);
uniform vec3 wet : source_color = vec3(0.16, 0.19, 0.16);
uniform vec3 scree : source_color = vec3(0.40, 0.39, 0.37);
uniform vec3 gravel : source_color = vec3(0.44, 0.42, 0.38);
uniform vec3 spoil : source_color = vec3(0.42, 0.34, 0.27);
uniform vec3 pond : source_color = vec3(0.12, 0.17, 0.17);
uniform vec3 sky_zenith : source_color = vec3(0.17, 0.34, 0.66);
uniform vec3 sky_horizon : source_color = vec3(0.66, 0.73, 0.80);
varying vec3 world;

float h21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}
float vn(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(h21(i), h21(i + vec2(1.0, 0.0)), u.x), mix(h21(i + vec2(0.0, 1.0)), h21(i + vec2(1.0, 1.0)), u.x), u.y);
}
float fbm(vec2 p) {
	float s = 0.0;
	float a = 0.5;
	for (int o = 0; o < 4; o++) {
		s += a * vn(p);
		p = p * 2.03 + vec2(17.1, 9.3);
		a *= 0.5;
	}
	return s;
}
// Distance from p to the segment a-b.
float seg(vec2 p, vec2 a, vec2 b) {
	vec2 ab = b - a;
	float t = clamp(dot(p - a, ab) / dot(ab, ab), 0.0, 1.0);
	return length(p - a - ab * t);
}
// The haul road: from the yard's south-east gate out through the rim's
// saddle, bending with the ground.
float road(vec2 p) {
	float d = seg(p, vec2(240.0, 120.0), vec2(520.0, 330.0));
	d = min(d, seg(p, vec2(520.0, 330.0), vec2(760.0, 460.0)));
	d = min(d, seg(p, vec2(760.0, 460.0), vec2(1150.0, 560.0)));
	d = min(d, seg(p, vec2(-240.0, 40.0), vec2(-520.0, 170.0)));
	d = min(d, seg(p, vec2(-520.0, 170.0), vec2(-900.0, 210.0)));
	return d;
}
// The rail spur: straighter, out of the yard's east side.
float rail(vec2 p) {
	float d = seg(p, vec2(240.0, -60.0), vec2(600.0, 80.0));
	d = min(d, seg(p, vec2(600.0, 80.0), vec2(1180.0, 300.0)));
	return d;
}
float box_d(vec2 p, vec2 c, vec2 h) {
	vec2 q = abs(p - c) - h;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0);
}
// The stream: the fall's water comes up below the north range at the pool
// and runs east and south-east out through the rim.
float stream(vec2 p) {
	float d = seg(p, vec2(330.0, -160.0), vec2(500.0, -165.0));
	d = min(d, seg(p, vec2(500.0, -165.0), vec2(640.0, -60.0)));
	d = min(d, seg(p, vec2(640.0, -60.0), vec2(760.0, 120.0)));
	d = min(d, seg(p, vec2(760.0, 120.0), vec2(900.0, 300.0)));
	d = min(d, seg(p, vec2(900.0, 300.0), vec2(1080.0, 420.0)));
	return d;
}
// The tower between this point and the sun: 1 if hit.
float tower_hit(vec3 o, vec3 d) {
	vec3 inv = 1.0 / d;
	float hit = 0.0;
	for (int k = 0; k < 16; k++) {
		vec3 t0 = (tower_low[k] - o) * inv;
		vec3 t1 = (tower_high[k] - o) * inv;
		vec3 tn = min(t0, t1);
		vec3 tf = max(t0, t1);
		float near = max(max(tn.x, tn.y), tn.z);
		float far = min(min(tf.x, tf.y), tf.z);
		hit = max(hit, step(max(near, 0.0), far) * step(0.0, far) * step(0.0, tower_high[k].y - tower_low[k].y - 1.0));
	}
	return hit;
}

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	vec2 p = world.xz;
	float big = fbm(p * 0.0021);
	float mid = fbm(p * 0.011 + 7.0);
	float fine = vn(p * 0.35);
	vec3 c = meadow;
	if (detail > 0.5) {
		// Meadow, drier ground on the rises, scrub, and wet ground in the hollows.
		c = mix(c, dry, smoothstep(0.5, 0.68, big) * 0.8);
		c = mix(c, scrub, smoothstep(0.55, 0.72, mid) * 0.7);
		c = mix(c, wet, smoothstep(0.42, 0.3, big) * 0.75);
		// Scree run out from the rim's feet.
		float out_r = length(p - vec2(0.0, -100.0));
		c = mix(c, scree, smoothstep(820.0, 1000.0, out_r + 120.0 * (mid - 0.5)) * 0.55);
		// Below the works: spoil heaps and the settling ponds.
		float spoil_d = min(length(p - vec2(330.0, -260.0)) - 70.0 - 30.0 * mid, length(p - vec2(-320.0, -330.0)) - 55.0 - 25.0 * mid);
		c = mix(c, spoil * (0.85 + 0.3 * fine), smoothstep(10.0, 0.0, spoil_d));
		float pond_d = min(box_d(p, vec2(410.0, -90.0), vec2(48.0, 26.0)), box_d(p, vec2(410.0, -30.0), vec2(48.0, 22.0)));
		pond_d = min(pond_d, box_d(p, vec2(-330.0, -200.0), vec2(36.0, 30.0)));
		float in_pond = smoothstep(1.5, -1.0, pond_d);
		c = mix(c, gravel * 0.8, smoothstep(5.0, 1.5, pond_d) * (1.0 - in_pond));   // the bunds
		// Worn tracks across the meadow.
		float track = abs(fbm(p * 0.004 + 31.0) - 0.5);
		c = mix(c, dry * 0.9, smoothstep(0.012, 0.004, track) * 0.6);
		// Road and rail, with their verges.
		float r = road(p);
		c = mix(c, dry * 0.8, smoothstep(10.0, 6.0, r) * 0.6);
		c = mix(c, gravel * (0.9 + 0.2 * fine), smoothstep(5.0, 4.0, r));
		float rl = rail(p);
		c = mix(c, spoil * 0.75, smoothstep(4.0, 3.0, rl));   // ballast
		c = mix(c, vec3(0.13, 0.11, 0.1), smoothstep(0.9, 0.6, abs(rl - 0.75)) * step(rl, 1.2));  // the rails' line
		// The stream (its line wanders a little) and its pool.
		vec2 wander = vec2(fbm(p * 0.01 + 3.0), fbm(p * 0.01 + 9.0)) - 0.5;
		float st = min(stream(p + wander * 30.0) - 3.0 - 2.0 * mid, length(p - vec2(320.0, -165.0)) - 24.0 - 8.0 * mid);
		float in_water = smoothstep(0.8, -0.8, st);
		c = mix(c, scrub * 0.8, smoothstep(9.0, 1.0, st) * (1.0 - in_water));   // the banks grow thicker
		c *= 0.88 + 0.24 * fine;
		c = mix(c, pond, in_pond);
		in_water = max(in_water, in_pond);
		c = mix(c, pond * 0.7, in_water);
		// The sky in the water, by the Fresnel term, rippled as it runs.
		vec3 eye_w = (INV_VIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
		vec3 view = normalize(world - eye_w);
		vec2 ripple = (vec2(vn(p * 0.6 + vec2(TIME * 0.7, 0.0)), vn(p * 0.6 + vec2(5.0, -TIME * 0.5))) - 0.5) * 0.12;
		vec3 n = normalize(vec3(ripple.x, 1.0, ripple.y));
		vec3 bounce = reflect(view, n);
		vec3 mirrored = mix(sky_horizon, sky_zenith, pow(clamp(bounce.y, 0.0, 1.0), 0.55));
		float fresnel = 0.02 + 0.98 * pow(1.0 - clamp(-dot(view, n), 0.0, 1.0), 5.0);
		EMISSION = mirrored * fresnel * in_water * 0.7;
		c *= 1.0 - fresnel * in_water * 0.8;
		ROUGHNESS = mix(0.95, 0.06, in_water);
		SPECULAR = mix(0.25, 0.6, in_water);
	} else {
		ROUGHNESS = 0.95;
	}
	// The tower's long shadow, out past the cascades.
	float eye_d = length(world - (INV_VIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz);
	float fade = smoothstep(SHADOW_NEAR, SHADOW_NEAR + 40.0, eye_d) * shadow_strength;
	if (fade > 0.001) {
		float lit = 1.0 - tower_hit(world + vec3(0.0, 0.3, 0.0), normalize(to_sun));
		c *= mix(1.0, 0.45 + 0.55 * lit, fade);
	}
	ALBEDO = c;
}
""".replace("SHADOW_NEAR", "%.1f" % SHADOW_NEAR_M)
	var material := ShaderMaterial.new()
	material.shader = shader
	return material
