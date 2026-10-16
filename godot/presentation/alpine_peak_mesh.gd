extends RefCounted
# Adapted from Claude505ca38 mountain_mesh.gd. Original CylinderMesh nodes
# remain the sole native export source; only their render layers are replaced.
# Four shared indexed ridge meshes; exact source radius/height per instance.
# The lowest18% follows the12-sided native hull, without yaw rotation. Above
# that foot, gullies/benches carve inward from the envelope at a distance.
# Near the native surface the shader restores the exact cone before contact.
# One opaque shader uses vertex rock/gully values and instance tint/snowline.
# Explicit inverse-transpose normals keep the .62/.70 aspect ratios correct.

const VARIANTS := 4
const RADIAL := 48
const RINGS := 24
const SolidExport := preload("res://presentation/solid_export.gd")
# The part of the height, from the foot, drawn as the cone itself.
const FOOT_TRUE := 0.18
const RESTORE_REACH := 12.0
const RESTORE_CELL_BOUND := 0.17
const RESTORE_FADE := 80.0

static var _peaks: Array[ArrayMesh] = []
static var _material: ShaderMaterial


static func peak(variant: int) -> ArrayMesh:
	if _peaks.is_empty():
		for v in VARIANTS:
			_peaks.append(_carve(7919 * (v + 1)))
	return _peaks[posmod(variant, VARIANTS)]


# specs: foot, radius, height, variant, rock and normalized snowline.
# No yaw: solid_export's polygon footprint fixes the lower face orientation.
static func draw(parent: Node3D, peaks: Array) -> void:
	var by_variant := {}
	for peak_spec in peaks:
		var v := posmod(int(peak_spec["variant"]), VARIANTS)
		if not by_variant.has(v):
			by_variant[v] = []
		by_variant[v].append(peak_spec)
	for v in by_variant:
		var list: Array = by_variant[v]
		var mm := MultiMesh.new()
		mm.transform_format = MultiMesh.TRANSFORM_3D
		mm.use_custom_data = true
		mm.mesh = peak(v)
		mm.instance_count = list.size()
		var bounds := AABB()
		for i in list.size():
			var spec: Dictionary = list[i]
			var basis := Basis.from_scale(Vector3(float(spec["radius"]), float(spec["height"]), float(spec["radius"])))
			mm.set_instance_transform(i, Transform3D(basis, spec["foot"]))
			var envelope := AABB(spec.foot - Vector3(spec.radius, 0, spec.radius), Vector3(spec.radius * 2, spec.height, spec.radius * 2))
			bounds = envelope if i == 0 else bounds.merge(envelope)
			# Match the active renderer color space, as StaticWorldBatch does.
			var rock: Color = spec["rock"]
			if RenderingServer.get_current_rendering_method() != "gl_compatibility":
				rock = rock.srgb_to_linear()
			mm.set_instance_custom_data(i, Color(rock.r, rock.g, rock.b, float(spec["snowline"])))
		var drawn := MultiMeshInstance3D.new()
		drawn.name = "AlpineCarvedPeaks%d" % v
		drawn.set_meta(&"alpine_peak_instances", list.size())
		drawn.multimesh = mm
		# Vertex LOD can rise beyond the distant carved mesh bounds.
		drawn.custom_aabb = bounds
		drawn.material_override = _shared_material()
		# Distant relief has no extra shadow submissions; authored sources
		# have layers0, so static shadow batching also leaves them alone.
		drawn.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		parent.add_child(drawn)


# Original snow-cap colliders protrude beyond the main cone. They must also
# be visible before contact, even though distant snow is a material mask.
# One shared twelve-sided native proxy batch; far instances collapse to a
# point in the vertex stage, without alpha layers or per-frame CPU work.
static func draw_near_snow(parent: Node3D, caps: Array) -> void:
	if caps.is_empty():
		return
	var mesh := CylinderMesh.new()
	mesh.top_radius = 0.0
	mesh.bottom_radius = 1.0
	mesh.height = 1.0
	mesh.radial_segments = SolidExport.CYLINDER_SEGMENTS
	mesh.rings = 1
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	mm.instance_count = caps.size()
	for i in caps.size():
		var spec: Dictionary = caps[i]
		mm.set_instance_transform(i, Transform3D(Basis.from_scale(Vector3(spec.radius, spec.height, spec.radius)), spec.centre))
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_back, skip_vertex_transform;
uniform vec3 snow : source_color = vec3(0.7255, 0.7608, 0.8);
void vertex() {
	float radius = length(MODEL_MATRIX[0].xyz);
	float height = length(MODEL_MATRIX[1].xyz);
	float limit = length(vec2(radius, height * 0.5)) + 160.0;
	// A whole primitive appears at least160m before its contact envelope.
	// Every vertex makes the same decision, so distant faces have zero area.
	if (distance(MODEL_MATRIX[3].xyz, CAMERA_POSITION_WORLD) > limit) {
		VERTEX = vec3(0.0, 0.0, -1.0);
	} else {
		VERTEX = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
	}
	NORMAL = normalize(transpose(inverse(mat3(MODELVIEW_MATRIX))) * NORMAL);
}
void fragment() {
	ALBEDO = snow;
	ROUGHNESS = 0.78;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	var instance := MultiMeshInstance3D.new()
	instance.name = "NativeNearSnowCaps"
	instance.set_meta(&"native_snow_cap_instances", caps.size())
	instance.multimesh = mm
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(instance)


static func _shared_material() -> ShaderMaterial:
	if _material != null:
		return _material
	var shader := Shader.new()
	# The air between the eye and a far slope is the environment's fog, as for
	# everything else in the world; the peaks add none of their own.
	shader.code = """
shader_type spatial;
render_mode cull_back, skip_vertex_transform;
uniform vec3 snow : source_color = vec3(0.80, 0.84, 0.88);
uniform vec3 scrub : source_color = vec3(0.17, 0.2, 0.13);
uniform vec3 scree : source_color = vec3(0.48, 0.47, 0.45);
varying vec4 tint;
varying float rise;
varying vec2 native_xz;
varying float restored;
varying mat3 view_normal_basis;
float native_height(vec2 point) {
	float support = 0.0;
	for (int face = 0; face < 12; face++) {
		float angle = 6.28318530718 * (float(face) + 0.5) / 12.0;
		support = max(support, dot(point, vec2(cos(angle), sin(angle))));
	}
	return 1.0 - support / 0.96592582629;
}
void vertex() {
	tint = INSTANCE_CUSTOM;
	native_xz = VERTEX.xz;
	vec3 original = vec3(VERTEX.x, native_height(VERTEX.xz), VERTEX.z);
	vec3 original_world = (MODEL_MATRIX * vec4(original, 1.0)).xyz;
	float dimension = max(length(MODEL_MATRIX[0].xyz), length(MODEL_MATRIX[1].xyz));
	float close_range = $REACH + $CELL * dimension;
	// Distance to the native surface, not the recessed distant surface.
	float gap = distance(original_world, CAMERA_POSITION_WORLD);
	restored = 1.0 - smoothstep(close_range, close_range + $FADE, gap);
	VERTEX = mix(VERTEX, original, restored);
	rise = VERTEX.y;
	view_normal_basis = transpose(inverse(mat3(MODELVIEW_MATRIX)));
	NORMAL = normalize(view_normal_basis * NORMAL);
	VERTEX = (MODELVIEW_MATRIX * vec4(VERTEX, 1.0)).xyz;
}
void fragment() {
	// Fragment selection preserves each native facet's hard normal, including
	// the twelve corners whose vertices are shared by adjacent faces.
	float angle = atan(native_xz.y, native_xz.x);
	float face_angle = (floor(angle / (6.28318530718 / 12.0)) + 0.5) * (6.28318530718 / 12.0);
	vec3 native_normal = normalize(view_normal_basis * vec3(cos(face_angle), 0.96592582629, sin(face_angle)));
	NORMAL = normalize(mix(NORMAL, native_normal, restored));
	float level = clamp((INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).y, 0.0, 1.0);
	float shade = COLOR.r;
	float gully = COLOR.g;
	// Rock, in bands across the face.
	vec3 surface = tint.rgb * shade * (0.88 + 0.12 * sin(rise * 41.0 + shade * 7.0));
	// Scree lies on the benches and gentler ground.
	surface = mix(surface, scree * shade, smoothstep(0.78, 0.93, level) * 0.65);
	// Scrub and stunted pines low down, where the ground is not too steep.
	float green = (1.0 - smoothstep(0.16, 0.3, rise + 0.1 * shade - 0.05 * gully)) * smoothstep(0.5, 0.7, level);
	surface = mix(surface, scrub * (0.75 + 0.4 * shade), green);
	// Snow above the snowline, lower in the gullies, only where it can lie.
	float line = tint.a > 0.0 ? tint.a - 0.14 * gully : 2.0;
	float white = smoothstep(line - 0.03, line + 0.05, rise) * smoothstep(0.32, 0.55, level);
	surface = mix(surface, snow, white);
	ALBEDO = surface;
	ROUGHNESS = mix(0.95, 0.72, white);
	SPECULAR = mix(0.2, 0.45, white);
}
"""
	shader.code = shader.code.replace("$REACH", "%.6f" % RESTORE_REACH) \
		.replace("$CELL", "%.6f" % RESTORE_CELL_BOUND).replace("$FADE", "%.6f" % RESTORE_FADE)
	_material = ShaderMaterial.new()
	_material.shader = shader
	return _material


static func footprint_radius(angle: float) -> float:
	var sector := TAU / float(SolidExport.CYLINDER_SEGMENTS)
	return cos(sector * 0.5) / cos(fposmod(angle, sector) - sector * 0.5)


static func _carve(seed: int) -> ArrayMesh:
	var ridges := FastNoiseLite.new()
	ridges.seed = seed
	ridges.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	ridges.fractal_type = FastNoiseLite.FRACTAL_RIDGED
	ridges.fractal_octaves = 4
	ridges.frequency = 1.0
	var shape := FastNoiseLite.new()
	shape.seed = seed + 1
	shape.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	shape.fractal_octaves = 2
	shape.frequency = 0.9
	var lumps := FastNoiseLite.new()
	lumps.seed = seed + 2
	lumps.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	lumps.fractal_octaves = 3
	lumps.frequency = 3.0
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	# Where in the noise this one sits; the line its crest runs along; how
	# many rock bands, how hard stepped.
	var at := Vector2(rng.randf_range(-40.0, 40.0), rng.randf_range(-40.0, 40.0))
	var crest := Vector2.from_angle(rng.randf_range(0.0, PI))
	var bands := float(rng.randi_range(5, 8))
	var benching := rng.randf_range(0.3, 0.5)

	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	st.set_smooth_group(0)
	var rows: Array[PackedVector3Array] = []
	var tones: Array[PackedColorArray] = []
	for i in RINGS + 1:
		# From the foot up; rings closer near the top, where the outline is cut.
		var rho := pow(1.0 - float(i) / float(RINGS), 1.2)
		var row := PackedVector3Array()
		var tone := PackedColorArray()
		for j in RADIAL:
			var a := TAU * float(j) / float(RADIAL)
			var p := Vector2(cos(a), sin(a)) * rho
			var cone := 1.0 - rho
			# How deep the carve may go: not at all on the foot.
			var free := smoothstep(FOOT_TRUE, FOOT_TRUE + 0.25, cone)
			var ridge := clampf(0.5 + 0.5 * ridges.get_noise_2d(at.x + p.x * 2.2, at.y + p.y * 2.2), 0.0, 1.0)
			var gully := 1.0 - ridge
			# The top is a crest: the flanks across it fall away faster than
			# the ends along it, and it rises and dips along its length to a
			# summit and lesser tops, not always at the middle.
			var across := absf(p.cross(crest))
			var bump := clampf(0.5 + 0.5 * shape.get_noise_1d(at.x + p.dot(crest) * 2.6), 0.0, 1.0)
			var h := maxf(cone - free * cone * (0.36 * gully * gully + 0.6 * across + 0.3 * bump * cone), 0.0)
			# Rock bands: a bench, then a step up (only ever lower).
			var step := fposmod(h * bands, 1.0)
			h = lerpf(h, (floor(h * bands) + step * step * step) / bands, benching * free)
			if free > 0.0:
				h = maxf(h, FOOT_TRUE)
			var radial := footprint_radius(a)
			row.append(Vector3(p.x * radial, h, p.y * radial))
			var lump := clampf(0.5 + 0.5 * lumps.get_noise_2d(at.x + p.x * 5.0, at.y + p.y * 5.0), 0.0, 1.0)
			tone.append(Color(0.68 + 0.26 * lump - 0.16 * gully, gully * free, 0.0))
		rows.append(row)
		tones.append(tone)
	for i in RINGS:
		for j in RADIAL:
			var k := (j + 1) % RADIAL
			var a0 := rows[i][j]
			var a1 := rows[i][k]
			var b0 := rows[i + 1][j]
			var b1 := rows[i + 1][k]
			# Clockwise seen from outside: Godot's front face. The top ring is
			# the summit's one point, met by every spoke: no quad there.
			_tri(st, a0, tones[i][j], a1, tones[i][k], b0, tones[i + 1][j])
			if i + 1 < RINGS:
				_tri(st, a1, tones[i][k], b1, tones[i + 1][k], b0, tones[i + 1][j])
	st.generate_normals()
	st.index()
	return st.commit()


static func _tri(st: SurfaceTool, a: Vector3, ca: Color, b: Vector3, cb: Color, c: Vector3, cc: Color) -> void:
	st.set_color(ca)
	st.add_vertex(a)
	st.set_color(cb)
	st.add_vertex(b)
	st.set_color(cc)
	st.add_vertex(c)
