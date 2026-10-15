extends Node3D
## The parachute over the rider (presentation only; its drag is the native's,
## kParachuteDragCoefficient). A ram-air canopy of eleven cells, arched down
## to its tips, 4.4 m over the shoulders and turned with the view, on lines
## fanned from the two risers in the fists. Opened, it fills from a bundle in
## OPEN_SECONDS; flown, its trailing edge works in the air.

const SPAN := 7.2              # tip to tip, m
const CHORD := 2.7             # leading to trailing edge, m
const CELLS := 11
const ABOVE := 4.4             # its underside's centre over the eye, m
const OPEN_SECONDS := 0.7
const LINE_POINTS := 5         # attachment points along each side's line set

var _canopy: MeshInstance3D
var _lines: Array[MeshInstance3D] = []
var _open := 0.0               # 0 packed .. 1 flying
var _shown := false


func _ready() -> void:
	_canopy = MeshInstance3D.new()
	_canopy.name = "Canopy"
	_canopy.mesh = _build_canopy()
	_canopy.material_override = _canopy_material()
	_canopy.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	add_child(_canopy)
	var line_material := StandardMaterial3D.new()
	line_material.albedo_color = Color(0.86, 0.84, 0.78)
	line_material.roughness = 0.7
	for i in 2 * LINE_POINTS:
		var line := MeshInstance3D.new()
		var cylinder := CylinderMesh.new()
		cylinder.top_radius = 0.006
		cylinder.bottom_radius = 0.006
		cylinder.height = 1.0
		cylinder.radial_segments = 4
		cylinder.rings = 1
		line.mesh = cylinder
		line.material_override = line_material
		line.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(line)
		_lines.append(line)
	visible = false


func open_fraction() -> float:
	return _open


# Once per frame: whether the canopy is out, the eye, the view's yaw, and the
# two riser tops (left, right) the lines come down to.
func update(show: bool, eye: Vector3, yaw: float, riser_tops: Array, delta: float) -> void:
	if show and not _shown:
		_open = 0.0
	_shown = show
	visible = show
	if not show:
		return
	_open = minf(1.0, _open + delta / OPEN_SECONDS)
	# Out of its bag: it fills fast, then settles (a small overshoot).
	var t := _open
	var fill := 1.0 + 2.2 * pow(t - 1.0, 3.0) + 1.2 * pow(t - 1.0, 2.0)
	var turn := Basis(Vector3.UP, yaw)
	var centre := eye + Vector3.UP * lerpf(1.8, ABOVE, minf(1.0, t * 1.6))
	_canopy.global_transform = Transform3D(turn.scaled(Vector3(fill, lerpf(0.4, 1.0, t), fill)), centre)
	# The lines: each riser to its side's attachment points, along the
	# canopy's underside from leading to trailing edge.
	# Left lines to the left riser, whichever hand holds it.
	var right := turn * Vector3.RIGHT
	var tops: Array = riser_tops.duplicate()
	if (tops[0] as Vector3).dot(right) > (tops[1] as Vector3).dot(right):
		tops.reverse()
	for side in 2:
		var top: Vector3 = tops[side]
		for k in LINE_POINTS:
			var along := lerpf(-0.42, 0.42, float(k) / float(LINE_POINTS - 1)) * CHORD
			var across := (-1.0 if side == 0 else 1.0) * 0.46 * SPAN
			var local := Vector3(across, -_tip_drop(across / (0.5 * SPAN)), along)
			var point := _canopy.global_transform * local
			var span := point - top
			var line := _lines[side * LINE_POINTS + k]
			var y := span.normalized()
			var x := y.cross(Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT).normalized()
			var z := x.cross(y)
			line.global_transform = Transform3D(Basis(x, y * span.length(), z), (top + point) * 0.5)


# How far the canopy's arch has come down at `u` (-1..1, tip to tip), m.
static func _tip_drop(u: float) -> float:
	return 0.95 * u * u


# An arched wing: top skin cambered over a flat bottom skin, closed at the
# trailing edge, open cell mouths at the leading edge. Vertex colour carries
# each cell's colour, the shader the air working the trailing edge.
func _build_canopy() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var across := 6 * CELLS
	var chord := 10
	var colours := [Color(0.86, 0.3, 0.1), Color(0.91, 0.88, 0.8)]
	for skin in 2:   # 0 top, 1 bottom
		for i in across:
			for j in chord:
				var corners := []
				for c in [[i, j], [i + 1, j], [i + 1, j + 1], [i, j + 1]]:
					var u := -1.0 + 2.0 * float(c[0]) / float(across)
					var v := float(c[1]) / float(chord)      # 0 leading .. 1 trailing
					var thick := 0.32 * pow(sin(PI * clampf(v * 0.9 + 0.06, 0.0, 1.0)), 0.6) * (1.0 - 0.4 * absf(u))
					var y := -_tip_drop(u) + (thick if skin == 0 else 0.0)
					corners.append(Vector3(u * 0.5 * SPAN * cos(0.12 * u), y, (v - 0.5) * CHORD))
				var cell := int(floor(float(i) / 6.0))
				var colour: Color = colours[cell % 2]
				# Seams between cells read darker.
				if i % 6 == 0:
					colour = colour.darkened(0.25)
				var a: Vector3 = corners[0]
				var b: Vector3 = corners[1]
				var c2: Vector3 = corners[2]
				var d: Vector3 = corners[3]
				if skin == 1:
					var swap := b
					b = d
					d = swap
				for p in [a, b, c2, a, c2, d]:
					st.set_color(colour)
					st.set_uv(Vector2(0.0, clampf((p.z / CHORD) + 0.5, 0.0, 1.0)))
					st.add_vertex(p)
	st.generate_normals()
	return st.commit()


func _canopy_material() -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_disabled;
void vertex() {
	// The air works the trailing edge, and a little the whole wing.
	float aft = UV.y;
	VERTEX.y += (0.06 * aft * aft) * sin(TIME * 9.0 + VERTEX.x * 1.7) + 0.015 * sin(TIME * 3.1 + VERTEX.x);
}
void fragment() {
	ALBEDO = COLOR.rgb;
	ROUGHNESS = 0.82;
	SPECULAR = 0.25;
	// Fabric lets the sun through: lit from above, glowing from below.
	BACKLIGHT = COLOR.rgb * 0.45;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	return material
