extends RefCounted
## The look of the basin's mountains. Each one is still a solid cone for the
## world (`solid_export.gd` reads the cones, which stay in the scene, hidden);
## what is drawn is a carved, ridged peak inside that cone, its foot on the
## cone's foot. Presentation only: nothing here collides.
##
## A unit peak (radius 1 at its foot, height 1) is carved from noise: ridges
## and gullies running down its flanks, a ragged summit, a slightly concave
## profile. Rock shade and snow ride in the vertex colour (r: rock shade,
## g: snow). VARIANTS peaks are built once; each mountain takes one, turned
## and scaled to its cone, and all mountains of a variant are one MultiMesh
## draw, their rock tint and snow in the instance's custom data. (MultiMesh
## also keeps them out of main.gd's static batcher, which recolours vertices.)

const VARIANTS := 8
const RADIAL := 64
const RINGS := 30
# The drawn flank never stands proud of the cone by more than this fraction of
# its radius, so the solid it hides is never short of the rock drawn.
const MAX_PROUD := 0.03

static var _peaks: Array[ArrayMesh] = []
static var _material: ShaderMaterial


static func peak(variant: int) -> ArrayMesh:
	if _peaks.is_empty():
		for v in VARIANTS:
			_peaks.append(_carve(7919 * (v + 1)))
	return _peaks[posmod(variant, VARIANTS)]


# Draws `peaks` ([{"foot": Vector3, "radius": float, "height": float,
# "variant": int, "rock": Color, "snowy": bool}...]) under `parent`, one
# MultiMesh per variant.
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
			var basis := Basis(Vector3.UP, float(spec["variant"]) * 2.39996).scaled(
				Vector3(float(spec["radius"]), float(spec["height"]), float(spec["radius"])))
			mm.set_instance_transform(i, Transform3D(basis, spec["foot"]))
			# Custom data is not colour-converted: the tint goes in linear.
			var rock: Color = (spec["rock"] as Color).srgb_to_linear()
			mm.set_instance_custom_data(i, Color(rock.r, rock.g, rock.b, 1.0 if bool(spec["snowy"]) else 0.0))
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
	shader.code = """
shader_type spatial;
render_mode cull_back;
uniform vec3 snow : source_color = vec3(0.83, 0.855, 0.886);
// Air between the eye and a far slope: it lifts the rock toward the haze of
// the horizon with distance. In the albedo, so it is lit (and dark at night)
// like the slope it covers.
uniform vec3 haze : source_color = vec3(0.6, 0.66, 0.74);
uniform float haze_per_m = 0.0009;
varying vec4 tint;
void vertex() { tint = INSTANCE_CUSTOM; }
void fragment() {
	float shade = COLOR.r;
	float white = COLOR.g * tint.a;
	vec3 rock = tint.rgb * shade;
	vec3 surface = mix(mix(rock, rock * 1.6, COLOR.g), snow, white);
	float air = 1.0 - exp(-length(VERTEX) * haze_per_m);
	ALBEDO = mix(surface, haze, air * 0.7);
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
	ridges.frequency = 0.9
	var lumps := FastNoiseLite.new()
	lumps.seed = seed + 1
	lumps.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	lumps.fractal_octaves = 3
	lumps.frequency = 1.6
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	# Where the summit leans, and how low the snow comes on this one.
	var lean := Vector2(rng.randf_range(-0.12, 0.12), rng.randf_range(-0.12, 0.12))
	var snowline := rng.randf_range(0.55, 0.7)
	var concave := rng.randf_range(1.08, 1.22)

	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var rows: Array[PackedVector3Array] = []
	var tones: Array[PackedColorArray] = []
	for i in RINGS + 1:
		var t := float(i) / float(RINGS)
		var row := PackedVector3Array()
		var tone := PackedColorArray()
		for j in RADIAL:
			var a := TAU * float(j) / float(RADIAL)
			var dir := Vector2(cos(a), sin(a))
			# Ridges run down the flank: noise sampled around the peak and
			# stretched along its height.
			var ridge := ridges.get_noise_3d(dir.x * 1.6, t * 0.8, dir.y * 1.6)   # -1..1, ridged
			var lump := lumps.get_noise_3d(dir.x * 2.2, t * 2.5, dir.y * 2.2)
			var cone := 1.0 - t
			var r := pow(cone, concave) * (1.0 + 0.16 * ridge + 0.07 * lump)
			r = minf(r, cone * (1.0 + MAX_PROUD))
			r = maxf(r, 0.0)
			var y := t + 0.025 * lump * (1.0 - t)
			var centre := lean * t * t
			row.append(Vector3(centre.x + dir.x * r, y, centre.y + dir.y * r))
			# Gullies (low ridge value) are darker and hold snow lower down.
			var gully := clampf(0.5 - 0.5 * ridge, 0.0, 1.0)
			var shade := 0.72 + 0.22 * clampf(0.5 + 0.5 * lump, 0.0, 1.0) - 0.18 * gully
			var snow_at := snowline - 0.16 * gully + 0.06 * lump
			var white := smoothstep(snow_at - 0.03, snow_at + 0.04, t)
			tone.append(Color(shade, white, 0.0))
		rows.append(row)
		tones.append(tone)
	for i in RINGS:
		for j in RADIAL:
			var k := (j + 1) % RADIAL
			var a0 := rows[i][j]
			var a1 := rows[i][k]
			var b0 := rows[i + 1][j]
			var b1 := rows[i + 1][k]
			# Clockwise seen from outside: Godot's front face.
			_tri(st, a0, tones[i][j], a1, tones[i][k], b0, tones[i + 1][j])
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
