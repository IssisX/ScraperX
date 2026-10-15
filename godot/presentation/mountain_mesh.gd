extends RefCounted
## The look of the basin's mountains. Each one is still a solid cone for the
## world (`solid_export.gd` reads the cones, which stay in the scene, hidden);
## what is drawn is a carved peak inside that cone, its foot on the cone's
## foot. Presentation only: nothing here collides.
##
## A unit peak (height 1, ASPECT wide at its foot, as every cone is) is a
## height field on a polar grid, carved down from the cone and never above
## it: ridges and the gullies between them, a broad shape that takes the
## summit off the axis and leaves shoulders and lesser tops, and rock bands
## stepped into the faces. The lowest FOOT_TRUE of it, which a climber can
## reach, is the cone itself, so the rock seen there is the rock that stops
## them. Rock shade and how far into a gully a point lies ride in the vertex
## colour (r, g). VARIANTS peaks are built once; each mountain takes one,
## turned and scaled to its cone, and all mountains of a variant are one
## MultiMesh draw, their rock tint and snowline in the instance's custom data.
## The scale is uniform, so the normals stay true without an inverse
## transpose. (MultiMesh also keeps them out of main.gd's static batcher,
## which recolours vertices.)

const VARIANTS := 8
const RADIAL := 64
const RINGS := 32
# Foot radius over height, the cones' own (main.gd builds every cone so).
const ASPECT := 0.62
# The part of the height, from the foot, drawn as the cone itself.
const FOOT_TRUE := 0.12

static var _peaks: Array[ArrayMesh] = []
static var _material: ShaderMaterial


static func peak(variant: int) -> ArrayMesh:
	if _peaks.is_empty():
		for v in VARIANTS:
			_peaks.append(_carve(7919 * (v + 1)))
	return _peaks[posmod(variant, VARIANTS)]


# Draws `peaks` ([{"foot": Vector3, "radius": float, "height": float,
# "variant": int, "rock": Color, "snowline": float}...]) under `parent`, one
# MultiMesh per variant. A snowline is the fraction of the height above which
# snow lies where it can, 0 for none. A cone wider or narrower than ASPECT
# gets the largest peak that stands inside it.
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
		for i in list.size():
			var spec: Dictionary = list[i]
			var size := minf(float(spec["height"]), float(spec["radius"]) / ASPECT)
			var basis := Basis(Vector3.UP, float(spec["variant"]) * 2.39996).scaled(Vector3.ONE * size)
			mm.set_instance_transform(i, Transform3D(basis, spec["foot"]))
			# Custom data is not colour-converted: the tint goes in linear.
			var rock: Color = (spec["rock"] as Color).srgb_to_linear()
			mm.set_instance_custom_data(i, Color(rock.r, rock.g, rock.b, float(spec["snowline"])))
		var drawn := MultiMeshInstance3D.new()
		drawn.name = "Peaks%d" % v
		drawn.multimesh = mm
		drawn.material_override = _shared_material()
		# Far past every shadow cascade.
		drawn.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		parent.add_child(drawn)


static func _shared_material() -> ShaderMaterial:
	if _material != null:
		return _material
	var shader := Shader.new()
	# The air between the eye and a far slope is the environment's fog, as for
	# everything else in the world; the peaks add none of their own.
	shader.code = """
shader_type spatial;
render_mode cull_back;
uniform vec3 snow : source_color = vec3(0.84, 0.865, 0.9);
uniform vec3 scrub : source_color = vec3(0.17, 0.2, 0.13);
uniform vec3 scree : source_color = vec3(0.48, 0.47, 0.45);
varying vec4 tint;
varying float rise;
void vertex() {
	tint = INSTANCE_CUSTOM;
	rise = VERTEX.y;
}
void fragment() {
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
	_material = ShaderMaterial.new()
	_material.shader = shader
	return _material


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
			var h := maxf(cone - free * cone * (0.24 * gully * gully + 0.5 * across + 0.3 * bump * cone), 0.0)
			# Rock bands: a bench, then a step up (only ever lower).
			var step := fposmod(h * bands, 1.0)
			h = lerpf(h, (floor(h * bands) + step * step * step) / bands, benching * free)
			row.append(Vector3(p.x * ASPECT, h, p.y * ASPECT))
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
	return st.commit()


static func _tri(st: SurfaceTool, a: Vector3, ca: Color, b: Vector3, cb: Color, c: Vector3, cc: Color) -> void:
	st.set_color(ca)
	st.add_vertex(a)
	st.set_color(cb)
	st.add_vertex(b)
	st.set_color(cc)
	st.add_vertex(c)
