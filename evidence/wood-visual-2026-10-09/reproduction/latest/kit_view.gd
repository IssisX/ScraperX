extends Node3D

# Read-only native mechanism presentation. Main owns startup/frame ordering
# and supplies shared appearance/sign resources. This node is KitPresentation;
# its direct children and native interpolation retain their existing paths.
const KIT_PART_FLOATS := 13
const KIT_CABLE_SEGMENTS := 4
const RefractoryMaterial := preload("res://presentation/materials/refractory_material.gd")
const PlankWoodShader := preload("res://presentation/plank_wood.gdshader")

var _native: Object
var _create_sign: Callable
var _cargo_net_mesh: Node3D
var _cargo_net_indices := PackedInt32Array()
var _cargo_net_material: StandardMaterial3D
var _cargo_net_knots: MultiMesh
var _cargo_net_strands: MultiMesh
var _cargo_net_links: Array[Vector2i] = []
var _cargo_net_cross_edges: Array[Vector4i] = []
# Display depth, aligned with existing particle standoff; native query faces
# remain the open ribbons. This is a rounded read-only render proxy.
const CARGO_NET_HALF_DEPTH := 0.035
var _kit_bodies: Array[Node3D] = []
var _kit_dynamic: Array[bool] = []
var _kit_cables: Array = []
var _cart_paint_material: StandardMaterial3D
var _cart_materials: Dictionary = {}
var _inspection_materials: Dictionary = {}
var _junction_materials: Dictionary = {}
var _plank_wood_material: ShaderMaterial
var _plank_broken_joint_mask := -1
var _water_lift_bucket_water: MeshInstance3D


func setup(native: Object, palette: Array[Material], cable_material: Material,
		cargo_net_material: StandardMaterial3D, create_sign: Callable) -> void:
	_native = native
	_cargo_net_material = cargo_net_material
	_create_sign = create_sign
	_build_kit(palette, cable_material)


func render_view() -> void:
	_render_kit()


func _kit_tube(outer: float, inner: float, half_length: float, material: Material) -> ArrayMesh:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	surface.set_material(material)
	for segment in 32:
		var a := TAU * float(segment) / 32.0
		var b := TAU * float(segment + 1) / 32.0
		var na := Vector3(cos(a), 0, sin(a))
		var nb := Vector3(cos(b), 0, sin(b))
		for radius in [outer, inner]:
			var sign_normal := 1.0 if radius == outer else -1.0
			var corners: Array[Vector3] = [na * radius + Vector3.UP * half_length,
				nb * radius + Vector3.UP * half_length, nb * radius - Vector3.UP * half_length,
				na * radius - Vector3.UP * half_length]
			var order := [0, 2, 1, 0, 3, 2] if radius == outer else [0, 1, 2, 0, 2, 3]
			for i in order:
				surface.set_normal((na if i in [0, 3] else nb) * sign_normal)
				surface.add_vertex(corners[i])
		for side in [-1.0, 1.0]:
			var end: Vector3 = Vector3.UP * half_length * side
			var corners: Array[Vector3] = [end + na * outer, end + nb * outer,
				end + nb * inner, end + na * inner]
			var order := [0, 1, 2, 0, 2, 3] if side > 0 else [0, 2, 1, 0, 3, 2]
			for i in order:
				surface.set_normal(Vector3.UP * side)
				surface.add_vertex(corners[i])
	return surface.commit()


func _kit_hull(triangles: PackedFloat32Array, material: Material, body: int, part: int) -> ArrayMesh:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	surface.set_material(material)
	if not _append_kit_hull(surface, triangles, Transform3D.IDENTITY, body, part):
		return null
	return surface.commit()


func _append_kit_hull(surface: SurfaceTool, triangles: PackedFloat32Array,
		local: Transform3D, body: int, part: int) -> bool:
	# The bridge exports cooked Jolt faces in part-local authoring space,
	# without the part offset/rotation. Validate the whole hull before adding
	# anything to a shared surface; invalid hulls never become partial solids.
	if triangles.is_empty() or triangles.size() % 9 != 0:
		push_error("KitView invalid cooked hull body=%d part=%d floats=%d" % [body, part, triangles.size()])
		return false
	for value in triangles:
		if not is_finite(value):
			push_error("KitView nonfinite cooked hull body=%d part=%d" % [body, part])
			return false
	var vertices := PackedVector3Array()
	var normals := PackedVector3Array()
	for p in range(0, triangles.size(), 9):
		var a := Vector3(triangles[p], triangles[p + 1], triangles[p + 2])
		var b := Vector3(triangles[p + 3], triangles[p + 4], triangles[p + 5])
		var c := Vector3(triangles[p + 6], triangles[p + 7], triangles[p + 8])
		var cross := (b - a).cross(c - a)
		if not cross.is_finite() or not is_finite(cross.length_squared()) \
				or cross.length_squared() <= 0.000000000000000001:
			push_error("KitView degenerate cooked hull body=%d part=%d triangle=%d" % [body, part, p / 9])
			return false
		# Jolt's outward CCW face order becomes Godot's clockwise front face.
		# Transform the native outward normal once, before the sole mesh commit
		# packs it. An intermediate committed mesh would quantize it twice.
		var normal := (local.basis * cross.normalized()).normalized()
		if not normal.is_finite() or normal.length_squared() <= 0.000000000000000001:
			push_error("KitView invalid cooked hull normal transform body=%d part=%d triangle=%d" % [body, part, p / 9])
			return false
		normals.append(normal)
		for vertex in [a, c, b]:
			var transformed: Vector3 = local * vertex
			if not transformed.is_finite():
				push_error("KitView nonfinite cooked hull transform body=%d part=%d triangle=%d" % [body, part, p / 9])
				return false
			vertices.append(transformed)
	for face in normals.size():
		surface.set_normal(normals[face])
		for corner in 3:
			surface.add_vertex(vertices[face * 3 + corner])
	return true


func _build_cart_materials(palette: Array[Material]) -> void:
	# Shared cart finishes inherit the existing steel/wood/concrete texture
	# resources. The surrounding machines keep their supplied palette.
	var finishes := {
		"frame": [0, Color("465158"), 1.0, 0.76],
		"running": [5, Color("a6adae"), 1.0, 0.36],
		"receiver": [5, Color("788385"), 1.0, 0.68],
		"timber": [2, Color("998368"), 0.0, 0.90],
		"slab": [3, Color("a69f91"), 0.0, 0.95],
	}
	for key in finishes:
		var finish: Array = finishes[key]
		var material := palette[int(finish[0])].duplicate() as StandardMaterial3D
		material.albedo_color = finish[1]
		material.metallic = finish[2]
		material.roughness = finish[3]
		_cart_materials[key] = material
	var timber: StandardMaterial3D = _cart_materials["timber"]
	var shared_grain := timber.normal_texture as NoiseTexture2D
	if shared_grain != null:
		# One mipmapped 256px colour texture reuses the existing deterministic
		# wood noise. Longitudinal grain follows the actual X-length planks;
		# their native joints and rope well supply the larger surface detail.
		var grain := NoiseTexture2D.new()
		grain.width = 256
		grain.height = 256
		grain.seamless = true
		grain.generate_mipmaps = true
		grain.noise = shared_grain.noise
		var tones := Gradient.new()
		tones.offsets = PackedFloat32Array([0.0, 0.45, 1.0])
		tones.colors = PackedColorArray([Color("665440"), Color("998368"), Color("b6a48a")])
		grain.color_ramp = tones
		timber.albedo_texture = grain
		timber.albedo_color = Color.WHITE
		timber.normal_scale = 0.55
		timber.uv1_scale = Vector3(0.12, 0.65, 2.4)
		timber.uv1_triplanar_sharpness = 4.0
	var slab: StandardMaterial3D = _cart_materials["slab"]
	slab.normal_scale = 0.65
	slab.uv1_scale = Vector3(1.2, 1.2, 1.2)


func _cart_part_material(entity: int, part: int, index: int, original: Material) -> Material:
	if not (entity >= 1954 and entity <= 1959 or entity >= 2320 and entity <= 2327):
		return original
	if entity == 2320 and index == 2:
		return _cart_materials["timber"]
	if entity == 2325 and index == 3:
		return _cart_materials["slab"]
	# The factory's first two track parts are the actual running rails.
	# Rollers have bare steel contact circumferences, rather than yellow tyres.
	if entity == 1954 and part < 2 or entity >= 2321 and entity <= 2324 \
			or entity == 1959 and part < 2 or entity == 2325 and index == 0:
		return _cart_materials["running"]
	if index == 5:
		return _cart_materials["receiver"]
	if index == 0:
		return _cart_materials["frame"]
	# Pendant heads, witness paint and the rusty carried block retain their
	# native material identities and remain distinct from working steel.
	return original


func _inspection_tread_height(uv: Vector2) -> float:
	# A one-metre tile of alternating pressed diagonal ribs, with .25m pitch.
	# Rounded ends and shoulders give a shallow normal, not geometric relief.
	var cell := uv * 4.0
	var u := fposmod(cell.x, 1.0) - 0.5
	var v := fposmod(cell.y, 1.0) - 0.5
	var diagonal := 1.0 if (floori(cell.x) + floori(cell.y)) % 2 == 0 else -1.0
	var along := (u + diagonal * v) * 0.70710678
	var across := (u - diagonal * v) * 0.70710678
	return 0.0014 * (1.0 - smoothstep(0.035, 0.065, absf(across))) \
		* (1.0 - smoothstep(0.20, 0.30, absf(along)))


func _inspection_tread_normal() -> ImageTexture:
	# One deterministic 128px RGBA8 normal texture shared by both fixed bodies.
	# Mipmaps suppress distant tread shimmer; no relief mesh or height shader.
	const SIZE := 128
	var image := Image.create(SIZE, SIZE, false, Image.FORMAT_RGBA8)
	var texel := 1.0 / float(SIZE)
	for y in SIZE:
		for x in SIZE:
			var uv := Vector2(float(x) + 0.5, float(y) + 0.5) * texel
			var dx := (_inspection_tread_height(uv + Vector2(texel, 0)) \
				- _inspection_tread_height(uv - Vector2(texel, 0))) / (2.0 * texel)
			var dy := (_inspection_tread_height(uv + Vector2(0, texel)) \
				- _inspection_tread_height(uv - Vector2(0, texel))) / (2.0 * texel)
			var normal := Vector3(-dx, -dy, 1.0).normalized()
			image.set_pixel(x, y, Color(normal.x * 0.5 + 0.5,
				normal.y * 0.5 + 0.5, normal.z * 0.5 + 0.5, 1.0))
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)


func _build_inspection_materials(palette: Array[Material]) -> void:
	# Keep the shared steel micro-surface resource, with restrained wear rather
	# than another noise texture. Cool girders, warm bearings and light footing
	# separate actual structural functions in this exposed maintenance place.
	var finishes := {
		# Weathered coating/oxide supplies diffuse response in Tower shadows;
		# bare galvanised footing retains its metallic finish below.
		0: [Color("58656a"), 0.35, 0.80, 0.24],
		1: [Color("805c43"), 0.12, 0.92, 0.34],
		5: [Color("9aaba9"), 1.0, 0.64, 0.55],
	}
	for index in finishes:
		var finish: Array = finishes[index]
		var material := palette[int(index)].duplicate() as StandardMaterial3D
		material.albedo_color = finish[0]
		material.metallic = finish[1]
		material.roughness = finish[2]
		material.normal_scale = finish[3]
		material.uv1_scale = Vector3.ONE
		material.uv1_triplanar_sharpness = 4.0
		_inspection_materials[index] = material
	var footing: StandardMaterial3D = _inspection_materials[5]
	footing.normal_enabled = true
	footing.normal_texture = _inspection_tread_normal()
	footing.uv1_triplanar = true
	footing.uv1_world_triplanar = true
	footing.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS


func _inspection_part_material(entity: int, index: int, original: Material) -> Material:
	if entity != 1932 and entity != 1933 and entity != 1935 and entity != 1936:
		return original
	# Native yellow edge-splice warnings retain their existing material. Each
	# native metal class maps to one finish, preserving the existing batches.
	return _inspection_materials.get(index, original)


func _build_junction_materials(palette: Array[Material]) -> void:
	# This bay has its own finish hierarchy. Keep the earlier 363/373m
	# inspection bodies' materials intact, and reuse their mipmapped tread.
	var finishes := {
		"frame": [0, Color("3f555c"), 0.12, 0.86, 0.30],
		"oxide": [1, Color("89664b"), 0.0, 0.93, 0.32],
		"footing": [5, Color("b2beb9"), 1.0, 0.68, 0.48],
		"recovery": [5, Color("7e948f"), 1.0, 0.80, 0.36],
		"return": [0, Color("9caeaa"), 0.12, 0.81, 0.26],
		# Surface oxide/wear supplies diffuse response in the actual Tower
		# lighting, where pure metal without a reflected scene reads black.
		"grip": [0, Color("b1bcb6"), 0.0, 0.62, 0.18],
		"bearing": [5, Color("829390"), 1.0, 0.48, 0.22],
		"warning": [7, Color("cba347"), 0.0, 0.76, 0.0],
		"transfer_frame": [0, Color("526572"), 0.08, 0.87, 0.30],
		"transfer_footing": [5, Color("c3c0ad"), 0.12, 0.72, 0.42],
		"service_frame": [0, Color("69746c"), 0.08, 0.83, 0.28],
		"service_pipe": [1, Color("a17b59"), 0.0, 0.88, 0.32],
		"service_footing": [5, Color("afb6af"), 0.12, 0.72, 0.42],
	}
	for key in finishes:
		var finish: Array = finishes[key]
		var material := palette[int(finish[0])].duplicate() as StandardMaterial3D
		material.albedo_color = finish[1]
		material.metallic = finish[2]
		material.roughness = finish[3]
		material.normal_scale = finish[4]
		material.uv1_scale = Vector3.ONE
		material.uv1_triplanar_sharpness = 4.0
		_junction_materials[key] = material
	for key in ["footing", "recovery", "transfer_footing", "service_footing"]:
		var material: StandardMaterial3D = _junction_materials[key]
		material.normal_enabled = true
		material.normal_texture = (_inspection_materials[5] as StandardMaterial3D).normal_texture
		material.uv1_triplanar = true
		material.uv1_world_triplanar = true
		material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS


func _junction_part_material(entity: int, index: int, half: Vector3,
		local: Transform3D, original: Material) -> Material:
	if entity == 2935:
		# The worn grip belongs to the real moving bar, the warm painted
		# arm to its passive hanger. Neither finish implies a powered motor.
		if index == 0:
			return _junction_materials["grip"]
		if index == 5:
			return _junction_materials["bearing"]
		if index == 7:
			return _junction_materials["warning"]
	elif entity == 1937:
		return _junction_materials["grip" if index == 0 else "bearing"]
	elif entity == 1939:
		# Service steel and the actual pipe keep the shared worn-PBR path.
		# Contrast separates footing from contact obstacles without glow.
		if index == 5:
			return _junction_materials["service_footing"]
		if index == 1:
			return _junction_materials["service_pipe"]
		if index == 7:
			return _junction_materials["warning"]
		if index == 0:
			return _junction_materials["service_frame"]
	elif entity == 1938:
		# The header transfer reuses the surface system, with its own worn
		# blue-grey frame and warm walking steel. All faces remain native parts.
		if index == 5:
			if half.x < 0.08 and half.y < 0.08:
				return _junction_materials["grip"]
			return _junction_materials["transfer_footing"]
		if index == 1:
			return _junction_materials["oxide"]
		if index == 7:
			return _junction_materials["warning"]
		if index == 0:
			if half.x < 0.08 and half.y < 0.08:
				return _junction_materials["grip"]
			if half.x > 1.5 and half.y < 0.17 and half.z > 0.6:
				return _junction_materials["warning"]
			return _junction_materials["transfer_frame"]
	elif entity == 1935 or entity == 1936:
		if index == 5:
			return _junction_materials["recovery" if entity == 1936 else "footing"]
		if index == 1:
			return _junction_materials["oxide"]
		if index == 7:
			return _junction_materials["warning"]
		if index == 0:
			# Actual return-brace top and arrival bolt heads stay legible.
			# Classify native dimensions/orientation instead of part ordering.
			if entity == 1936 and half.x > 3.0 and local.basis.x.y > 0.3:
				return _junction_materials["return"]
			if entity == 1935 and half.y < 0.025 and half.x < 0.07:
				return _junction_materials["grip"]
			return _junction_materials["frame"]
	return original


func _junction_stencil(node: Node3D, text: String, at: Vector3,
		pixel_size: float, rotation: Vector3, colour: Color) -> void:
	# Flat maintenance paint on an existing native face: no sign backing,
	# extra support, interaction, direction arrow or release timing cue.
	var stencil := Label3D.new()
	stencil.name = "InspectionStencil"
	stencil.text = text
	stencil.position = at
	stencil.rotation = rotation
	stencil.font_size = 40
	stencil.pixel_size = pixel_size
	stencil.modulate = colour
	stencil.outline_size = 0
	stencil.shaded = true
	stencil.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	stencil.visibility_range_end = 28.0
	node.add_child(stencil)


func _junction_markings(node: Node3D, entity: int) -> void:
	if entity == 1935:
		_junction_stencil(node, "INSPECTION\n407", Vector3(-22.0, 407.0015, -127.7),
			0.007, Vector3(-PI / 2.0, 0, 0), Color("253c40"))
		_junction_stencil(node, "NORTH FRAME\n418", Vector3(0.2, 407.0015, -127.0),
			0.0045, Vector3(-PI / 2.0, 0, 0), Color("253c40"))
	elif entity == 1936:
		_junction_stencil(node, "INSPECTION RETURN\n403.5", Vector3(-16.5, 403.5015, -128.25),
			0.006, Vector3(-PI / 2.0, 0, 0), Color("263e3e"))
	elif entity == 2935:
		_junction_stencil(node, "J-407", Vector3(0, -0.7, 0.2015),
			0.002, Vector3.ZERO, Color("3d3526"))
	elif entity == 1939:
		_junction_stencil(node, "RETURN HEADER\nN-451", Vector3(-20.8, 451.0015, -130.0),
			0.0055, Vector3(-PI / 2.0, 0, 0), Color("293b35"))
		_junction_stencil(node, "FRAME SERVICE\n462", Vector3(-0.1, 451.0015, -129.3),
			0.0045, Vector3(-PI / 2.0, 0, 0), Color("293b35"))
	elif entity == 1938:
		_junction_stencil(node, "SERVICE HEADER\nN-429", Vector3(-21.1, 429.0015, -128.0),
			0.0055, Vector3(-PI / 2.0, 0, 0), Color("303d43"))
		_junction_stencil(node, "NORTH FRAME\n440", Vector3(-0.1, 429.0015, -128.0),
			0.0045, Vector3(-PI / 2.0, 0, 0), Color("303d43"))


func _is_plank_segment(entity: int) -> bool:
	return entity >= 2880 and entity <= 2891


func _plank_part_material(entity: int, index: int, original: Material) -> Material:
	if not _is_plank_segment(entity) or index != 2:
		return original
	if _plank_wood_material == null:
		_plank_wood_material = ShaderMaterial.new()
		_plank_wood_material.shader = PlankWoodShader
		# Weathered, unvarnished service timber. Coordinates remain continuous
		# across the twelve actual native cells, including separated fragments.
		_plank_wood_material.set_shader_parameter("timber_light", Color("b9a077"))
		_plank_wood_material.set_shader_parameter("timber_dark", Color("65503a"))
	return _plank_wood_material


func _update_plank_material() -> void:
	if _plank_wood_material == null or not _native.has_method("get_landing_state"):
		return
	var landing: Dictionary = _native.get_landing_state()
	if not landing.has("plank_broken_joint_mask"):
		return
	# Bit j belongs to the actual interface between cells j and j+1. Native
	# restart/restoration also owns clearing the mask; no visual damage timer.
	var mask := int(landing["plank_broken_joint_mask"]) & 0x7ff
	if mask != _plank_broken_joint_mask:
		_plank_broken_joint_mask = mask
		_plank_wood_material.set_shader_parameter("broken_joint_mask", mask)


func _plank_grain_mesh(source: Mesh, entity: int) -> ArrayMesh:
	# Each native segment keeps its own exact vertices, normals, indices and
	# moving pose. Only material coordinates span the original 2.4m board.
	# The existing wood shader's longitudinal axis is Z, so encode native X
	# length there and retain width/thickness in the other two coordinates.
	var cell := entity - 2880
	var station := (float(cell) + 0.5) * 0.2
	var mesh := ArrayMesh.new()
	for surface_index in source.get_surface_count():
		var arrays := source.surface_get_arrays(surface_index)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		var uv := PackedVector2Array()
		var uv2 := PackedVector2Array()
		uv.resize(vertices.size())
		uv2.resize(vertices.size())
		for vertex_index in vertices.size():
			var vertex := vertices[vertex_index]
			uv[vertex_index] = Vector2(vertex.z, vertex.x + station)
			# UV2.y is face metadata, not geometry: -1 original sawn end,
			# 1..11 native interface index+1, 0 longitudinal/top/bottom faces.
			var face := 0.0
			if normals[vertex_index].x < -0.5:
				face = -1.0 if cell == 0 else float(cell)
			elif normals[vertex_index].x > 0.5:
				face = -1.0 if cell == 11 else float(cell + 1)
			uv2[vertex_index] = Vector2(vertex.y, face)
		arrays[Mesh.ARRAY_TEX_UV] = uv
		arrays[Mesh.ARRAY_TEX_UV2] = uv2
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		mesh.surface_set_material(surface_index, source.surface_get_material(surface_index))
	return mesh


func _inspection_practicals(node: Node3D) -> void:
	# Two flush lamp faces sit on existing native post colliders. Their cage
	# pattern is painted into one shared 32px texture, with no housing/ledge.
	var image := Image.create(32, 32, false, Image.FORMAT_RGBA8)
	for y in 32:
		for x in 32:
			var cage := x < 3 or x > 28 or y < 3 or y > 28 \
				or x in [10, 11, 20, 21] or y in [15, 16]
			image.set_pixel(x, y, Color.BLACK if cage else Color("ffdda0"))
	image.generate_mipmaps()
	var lens := ImageTexture.create_from_image(image)
	var glow := StandardMaterial3D.new()
	glow.albedo_texture = lens
	glow.roughness = 0.68
	glow.emission_enabled = true
	glow.emission = Color.WHITE
	glow.emission_texture = lens
	glow.emission_energy_multiplier = 1.2
	glow.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	# South face of the real bearing stub: X[-20.50,-20.20],
	# Y[363.10,364.45], Z[-158.95,-158.65]. The upper lamp occupies the
	# west face of the .36m junction post (-19.45,-138.9), top373.5.
	# Each face stays within those bounds, offset just .0005m outward.
	for mount in [[Vector3(-20.35, 363.8, -158.6495), 0.0],
			[Vector3(-19.6305, 373.1, -138.9), -PI / 2.0]]:
		var at: Vector3 = mount[0]
		var face := MeshInstance3D.new()
		face.name = "InspectionLampFace"
		var quad := QuadMesh.new()
		quad.size = Vector2(0.24, 0.32)
		quad.material = glow
		face.mesh = quad
		face.position = at
		face.rotation.y = float(mount[1])
		face.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		face.visibility_range_end = 40.0
		node.add_child(face)
		var light := OmniLight3D.new()
		light.name = "InspectionPractical"
		light.position = at
		light.light_color = Color("ffcf8e")
		light.light_energy = 1.0
		light.omni_range = 7.0
		light.omni_attenuation = 2.0
		light.shadow_enabled = false
		light.distance_fade_enabled = true
		light.distance_fade_begin = 25.0
		light.distance_fade_length = 10.0
		node.add_child(light)


func _build_kit(palette: Array[Material], cable_material: Material) -> void:
	for body in int(_native.get_kit_body_count()):
		var entity := int(_native.get_kit_body_entity_id(body))
		if entity == 1954 and _cart_materials.is_empty():
			_build_cart_materials(palette)
		if (entity == 1932 or entity == 1933 or entity == 1935 or entity == 1936) and _inspection_materials.is_empty():
			_build_inspection_materials(palette)
		if entity in [1935, 1936, 1937, 1938, 1939, 2935] and _junction_materials.is_empty():
			if _inspection_materials.is_empty():
				_build_inspection_materials(palette)
			_build_junction_materials(palette)
		var node := Node3D.new()
		node.name = "KitBody%d" % int(_native.get_kit_body_entity_id(body))
		node.transform = _native.get_kit_body_transform(body)
		if int(_native.get_kit_body_entity_id(body)) == 1960:
			_create_sign.call("SERVICE +121\nSWING TRANSFER", Vector3(11.6, 112.1, -180.42), 0.0, 0.22, Color("e7c67c"), node)
			_create_sign.call("+121\nUP", Vector3(9, 121.8, -177.74), PI, 0.26, Color("e7c67c"), node)
		if int(_native.get_kit_body_entity_id(body)) == 2560:
			_create_sign.call("UP · JUMP TO DUCT\nCLIMB CABINET FIRST", Vector3(20.0, 23.5, -121.76), 0.0, 0.10, Color("e7c67c"), node)
		if int(_native.get_kit_body_entity_id(body)) == 2562:
			_create_sign.call("UP · VENT TO NEXT DECK\nCLIMB · HOLD FORWARD", Vector3(24.0, 28.2, -123.34), 0.0, 0.10, Color("e7c67c"), node)
		var parts: PackedFloat32Array = _native.get_kit_body_parts(body)
		# Parts on one rigid body share a pose. Batch their triangles by
		# material once, instead of submitting every tread/rung separately.
		# This retains the native shapes, local poses, UVs and tube bores.
		var surfaces: Dictionary = {}
		for p in range(0, parts.size() - KIT_PART_FLOATS + 1, KIT_PART_FLOATS):
			var mesh: Mesh
			var material_index := clampi(int(parts[p + 10]), 0, palette.size() - 1)
			var material := _cart_part_material(entity, p / KIT_PART_FLOATS,
				material_index, palette[material_index])
			material = _inspection_part_material(entity, material_index, material)
			material = _plank_part_material(entity, material_index, material)
			var local := Transform3D(
				Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
				Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			material = _junction_part_material(entity, material_index,
				Vector3(parts[p], parts[p + 1], parts[p + 2]), local, material)
			# Rails and structural steel can share a native material class while
			# needing different finishes. Keep each finish in its own rigid batch.
			var finish_key := "" if material == palette[material_index] else "/%d" % material.get_instance_id()
			if int(parts[p + 11]) == 1:
				if parts[p + 12] > 0.0:
					mesh = _kit_tube(parts[p], parts[p + 12], parts[p + 1], material)
				else:
					var cylinder := CylinderMesh.new()
					cylinder.top_radius = parts[p]
					cylinder.bottom_radius = parts[p]
					cylinder.height = parts[p + 1] * 2.0
					cylinder.material = material
					mesh = cylinder
			elif int(parts[p + 11]) == 2:
				var capsule := CapsuleMesh.new()
				capsule.radius = parts[p]
				capsule.height = (parts[p + 1] + parts[p]) * 2.0
				capsule.radial_segments = 20
				capsule.rings = 8
				capsule.material = material
				mesh = capsule
			elif int(parts[p + 11]) == 3:
				if not _native.has_method("get_kit_body_part_mesh"):
					push_error("KitView cooked hull bridge unavailable body=%d part=%d" % [body, p / KIT_PART_FLOATS])
					continue
				var triangles: PackedFloat32Array = _native.get_kit_body_part_mesh(body, p / KIT_PART_FLOATS)
				# Hulls carry unindexed vertices and flat normals. Append directly
				# to their final material batch, keeping indexed boxes separate.
				var channels := (1 << Mesh.ARRAY_VERTEX) | (1 << Mesh.ARRAY_NORMAL)
				var key := "%d/%d" % [material_index, channels] + finish_key
				var surface: SurfaceTool = surfaces.get(key)
				if surface == null:
					surface = SurfaceTool.new()
					surface.begin(Mesh.PRIMITIVE_TRIANGLES)
					surface.set_material(material)
				if _append_kit_hull(surface, triangles, local, body, p / KIT_PART_FLOATS):
					surfaces[key] = surface
				continue
			else:
				var box := BoxMesh.new()
				box.size = Vector3(parts[p], parts[p + 1], parts[p + 2]) * 2.0
				box.material = material
				mesh = box
				if _is_plank_segment(entity) and material_index == 2:
					mesh = _plank_grain_mesh(mesh, entity)
			for surface_index in mesh.get_surface_count():
				# Indexed boxes and the unindexed tube bore need separate
				# surfaces; mixing them would omit the unindexed triangles.
				var channels := 0
				var arrays := mesh.surface_get_arrays(surface_index)
				for channel in arrays.size():
					if arrays[channel] != null:
						channels |= 1 << channel
				var key := "%d/%d" % [material_index, channels] + finish_key
				if not surfaces.has(key):
					var surface := SurfaceTool.new()
					surface.begin(Mesh.PRIMITIVE_TRIANGLES)
					surface.set_material(material)
					surfaces[key] = surface
				surfaces[key].append_from(mesh, surface_index, local)
		for key in surfaces:
			var surface: SurfaceTool = surfaces[key]
			var instance := MeshInstance3D.new()
			instance.mesh = surface.commit()
			if int(String(key).get_slice("/", 0)) == RefractoryMaterial.NATIVE_MATERIAL_INDEX:
				RefractoryMaterial.apply_to(instance, int(_native.get_kit_body_material_key(body)))
			node.add_child(instance)
		if entity == 1932:
			_inspection_practicals(node)
		if entity in [1935, 1936, 1938, 1939, 2935]:
			_junction_markings(node, entity)
		# Signs stay on the native landing/deck bodies, including the moving
		# pendant. They label the real stations without owning machine state.
		if entity == 1981:
			_balance_sign(node, "GRAVITY BALANCE · +143\nOPERATE · HOLD UP TO RISE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1982:
			_balance_sign(node, "EXIT · +165\nHOLD DOWN TO RESET", Vector3(0.0, 1.8, 0.0))
		elif entity == 2980:
			_balance_sign(node, "GRAVITY BALANCE\nUP: RISE · DOWN: RESET\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1984:
			_balance_sign(node, "CROWN GONDOLA · +165\nOPERATE · HOLD UP TO RISE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1985:
			_balance_sign(node, "EXIT · +198\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
			_balance_sign(node, "CROWN RECALL\nHOLD DOWN TO RETURN", Vector3(-14.337928, 1.8, -0.55))
		elif entity == 2983:
			_balance_sign(node, "CROWN GONDOLA\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1987:
			_balance_sign(node, "TRACTION TRAM · +198\nOPERATE · HOLD UP TO CLIMB", Vector3(0.0, 1.8, 0.0))
		elif entity == 1988:
			_balance_sign(node, "EXIT · +231\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2984:
			_balance_sign(node, "TRACTION TRAM\nUP: CLIMB · DOWN: RETURN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1992:
			_balance_sign(node, "BARREL HELIX · +231\nOPERATE · ROLLER RIDES CAM", Vector3(0.0, 1.8, 0.0))
		elif entity == 1993:
			_balance_sign(node, "EXIT · +253\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2988:
			_balance_sign(node, "BARREL HELIX\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1995:
			_balance_sign(node, "CASCADE MAST · +253\nOPERATE · 1:2:3 CABLE CASCADE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1996:
			_balance_sign(node, "EXIT · +286\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2992:
			_balance_sign(node, "CASCADE MAST\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		elif entity == 1921:
			_balance_sign(node, "PITMAN LIFT · +286\nOPERATE · CRANK DRIVES ROD", Vector3(0.0, 1.8, 0.0))
		elif entity == 1922:
			_balance_sign(node, "EXIT · +308\nHOLD DOWN TO RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 2921:
			_balance_sign(node, "PITMAN LIFT\nHOLD UP / DOWN\nRELEASE TO BRAKE", Vector3(0.0, 1.8, -1.2))
		if entity == 1941:
			_balance_sign(node, "REFRACTORY RECLAIM · +308\nFEED WHILE HELD · BOARD TO RELEASE", Vector3(0.0, 1.8, 0.0))
		elif entity == 1942:
			_balance_sign(node, "RECLAIM EXIT · +330\nEMPTY THE SCOOPS BEFORE RETURN", Vector3(0.0, 1.8, 0.0))
		elif entity == 1945:
			_balance_sign(node, "MAINTENANCE CROSSING · +319\nDUMP TO RETURN · LOWER FEED BELOW", Vector3(3.5, -0.5, 5.75))
		elif entity == 2201:
			_balance_sign(node, "RECLAIM CABIN\nUP: RELEASE BRAKE · DOWN: DUMP\nRELEASE TO HOLD", Vector3(0.0, -0.7, 0.0))
		# Factory static parts share (-41.5,327.25,-142); the cart control
		# moves with its deck. Each sign is local to its actual native owner.
		if entity == 1955:
			_balance_sign(node, "SLAB HAUL · +330\nFALLING SLAB LIFTS CART\nDOWN: PAID RETURN", Vector3(15.6, 3.85, 2.5))
		elif entity == 1956:
			_balance_sign(node, "INSPECTION · +341\nJUMP ASHORE · DOWN TO RECOVER\nREBOARD TO CONTINUE", Vector3(15.6, 14.85, 2.5))
		elif entity == 1957:
			_balance_sign(node, "EXIT · +352\nJUMP NORTH LANE TO TOWER\nDOWN: RAISE SLAB / RETURN CART", Vector3(16.4, 25.85, 2.5))
		elif entity == 2320:
			_balance_sign(node, "SLAB HAUL CART\nUP: RELEASE · NEUTRAL: BRAKE\n341 INSPECTION · 352 JUMP", Vector3(-1.15, 3.85, -0.45))
		# Paint identifies a maintenance transfer, not a scripted cargo objective.
		# Every mark lies on an existing native surface and follows that owner.
		if entity == 1957:
			_cart_storage_marks(node, Vector3(15.7, 24.751, 2.35))
			var stores := Label3D.new()
			stores.text = "MAINTENANCE STORES"
			stores.position = Vector3(15.7, 24.752, 2.65)
			stores.rotation.x = -PI / 2.0
			stores.font_size = 32
			stores.pixel_size = 0.0025
			stores.modulate = Color("d7b54f")
			node.add_child(stores)
		elif entity == 2327:
			_cart_paint(node, Vector3(0.25, 0.001, 0.045), Vector3(0, 0.1306, 0))
		elif entity == 2326:
			_cart_paint(node, Vector3(0.08, 0.30, 0.001),
				Vector3(0.1797, -0.10375, 0.1806), Vector3(0, 0, PI / 3.0))
		elif entity == 1958:
			# Slab-centre witness bands for cart330/341/352; no implied powered ascent.
			for slab_y in [351.721281, 339.019575, 326.317869]:
				_cart_paint(node, Vector3(0.001, 0.045, 0.17),
					Vector3(-2.0985, slab_y - 327.25, 0.7))
		if entity == 2952:
			var sign := Label3D.new()
			sign.name = "CargoNetSign"
			sign.text = "EASY WAY UP · FIRST DECK +11 M\nTAP CLIMB · HOLD FORWARD"
			sign.position = Vector3(20.0, 2.8, -117.65)
			sign.font_size = 44
			sign.pixel_size = 0.006
			sign.modulate = Color("fff0c4")
			sign.outline_size = 12
			node.add_child(sign)
		if entity == 2011:
			var water_mesh := BoxMesh.new()
			water_mesh.size = Vector3(1.54, 1.0, 1.54)
			var water_mat := StandardMaterial3D.new()
			water_mat.albedo_color = Color(0.08, 0.30, 0.38, 0.72)
			water_mat.roughness = 0.16
			water_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			water_mesh.material = water_mat
			_water_lift_bucket_water = MeshInstance3D.new()
			_water_lift_bucket_water.name = "BucketWater"
			_water_lift_bucket_water.mesh = water_mesh
			_water_lift_bucket_water.visible = false
			node.add_child(_water_lift_bucket_water)
		add_child(node)
		_kit_bodies.append(node)
		_kit_dynamic.append(bool(_native.is_kit_body_dynamic(body)))
	_cargo_net_indices = _native.get_cargo_net_indices()
	if not _cargo_net_indices.is_empty():
		_build_cargo_net()
	var cable := BoxMesh.new()
	cable.size = Vector3(0.035, 0.035, 1.0)
	cable.material = cable_material
	for index in int(_native.get_kit_cable_count()):
		var segments: Array[MeshInstance3D] = []
		for segment in KIT_CABLE_SEGMENTS:
			var instance := MeshInstance3D.new()
			instance.name = "KitCable%d_%d" % [index, segment]
			instance.mesh = cable
			instance.visible = false
			add_child(instance)
			segments.append(instance)
		_kit_cables.append(segments)
	_render_kit()


func _cart_paint(parent: Node3D, size: Vector3, at: Vector3,
		rotation: Vector3 = Vector3.ZERO) -> void:
	if _cart_paint_material == null:
		_cart_paint_material = StandardMaterial3D.new()
		_cart_paint_material.albedo_color = Color("bca04c")
		_cart_paint_material.roughness = 0.94
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = _cart_paint_material
	var marking := MeshInstance3D.new()
	marking.name = "MaintenancePaint"
	marking.mesh = mesh
	marking.position = at
	marking.rotation = rotation
	marking.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	parent.add_child(marking)


func _cart_storage_marks(parent: Node3D, centre: Vector3) -> void:
	# A .36m painted footprint suits the real .26m block without claiming
	# a new structural surface, delivery reward or hidden cargo constraint.
	for side in [-1.0, 1.0]:
		_cart_paint(parent, Vector3(0.36, 0.001, 0.012), centre + Vector3(0, 0, side * 0.18))
		_cart_paint(parent, Vector3(0.012, 0.001, 0.36), centre + Vector3(side * 0.18, 0, 0))


func _balance_sign(parent: Node3D, text: String, at: Vector3) -> void:
	var plate := MeshInstance3D.new()
	plate.name = "GravityBalanceControlSign"
	var mesh := BoxMesh.new()
	mesh.size = Vector3(3.8, 0.84, 0.035)
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("252a2d")
	material.roughness = 0.85
	mesh.material = material
	plate.mesh = mesh
	plate.position = at
	parent.add_child(plate)
	for face in [-1.0, 1.0]:
		var label: Label3D = _create_sign.call(text, at + Vector3(0.0, 0.0, face * 0.025),
			0.0 if face > 0.0 else PI, 0.16, Color("fff0c4"), parent)
		label.name = "GravityBalanceInstructions"
		label.outline_size = 8


# Native CargoNet exposes four corners per knot and two triangles per woven
# quad. Derive links from that topology, without authoring another grid.
func _build_cargo_net() -> void:
	_cargo_net_mesh = Node3D.new()
	_cargo_net_mesh.name = "NativeSoftCargoNet"
	add_child(_cargo_net_mesh)
	var vertices: PackedVector3Array = _native.get_cargo_net_vertices()
	for i in range(0, _cargo_net_indices.size(), 6):
		var a := _cargo_net_indices[i]
		var b := _cargo_net_indices[i + 1]
		var c := _cargo_net_indices[i + 2]
		var d := _cargo_net_indices[i + 5]
		var node_a := a / 4
		var node_b := b / 4
		var node_c := c / 4
		if node_a == node_b and node_a == node_c:
			continue # Knot face; its rounded display follows all four corners.
		if node_a != node_b:
			_cargo_net_links.append(Vector2i(node_a, node_b))
			_cargo_net_cross_edges.append(Vector4i(a, d, b, c))
		else:
			_cargo_net_links.append(Vector2i(node_a, node_c))
			_cargo_net_cross_edges.append(Vector4i(a, b, d, c))
	var rope := CylinderMesh.new()
	rope.top_radius = 1.0
	rope.bottom_radius = 1.0
	rope.height = 1.0
	rope.radial_segments = 16
	rope.rings = 1
	rope.material = _cargo_net_material
	_cargo_net_strands = _net_instances("Strands", rope, _cargo_net_links.size())
	var knot := SphereMesh.new()
	knot.radius = 1.0
	knot.height = 2.0
	knot.radial_segments = 16
	knot.rings = 7
	knot.material = _cargo_net_material
	_cargo_net_knots = _net_instances("Knots", knot, vertices.size() / 4)


func _net_instances(node_name: String, mesh: Mesh, count: int) -> MultiMesh:
	var instances := MultiMesh.new()
	instances.transform_format = MultiMesh.TRANSFORM_3D
	instances.mesh = mesh
	instances.instance_count = count
	var node := MultiMeshInstance3D.new()
	node.name = node_name
	node.multimesh = instances
	_cargo_net_mesh.add_child(node)
	return instances


func _update_cargo_net() -> void:
	if _cargo_net_mesh == null or DisplayServer.get_name() == "headless":
		return
	var vertices: PackedVector3Array = _native.get_cargo_net_vertices()
	# Regression fixture selection can remove the native soft body after
	# startup. Its old display must disappear with that body.
	_cargo_net_mesh.visible = not vertices.is_empty()
	if vertices.is_empty():
		return
	var centres := PackedVector3Array()
	centres.resize(_cargo_net_knots.instance_count)
	var bounds := AABB(vertices[0], Vector3.ZERO)
	for i in centres.size():
		var p := i * 4
		var centre := (vertices[p] + vertices[p + 1] + vertices[p + 2] + vertices[p + 3]) * 0.25
		var right := (vertices[p + 1] + vertices[p + 3] - vertices[p] - vertices[p + 2]) * 0.25
		var up := (vertices[p + 2] + vertices[p + 3] - vertices[p] - vertices[p + 1]) * 0.25
		var normal := right.cross(up)
		centres[i] = centre
		# Affine rounded proxy follows average stretch/shear/orientation.
		# sqrt(2) spans planar parallelogram corners, not arbitrary warp.
		var basis := Basis(Vector3.ZERO, Vector3.ZERO, Vector3.ZERO)
		if normal.length_squared() > 0.000000000001:
			basis = Basis(right * sqrt(2.0), up * sqrt(2.0), normal.normalized() * CARGO_NET_HALF_DEPTH)
		var pose := Transform3D(basis, centre)
		_cargo_net_knots.set_instance_transform(i, pose)
		bounds = bounds.merge(pose * _cargo_net_knots.mesh.get_aabb())
	for i in _cargo_net_links.size():
		var link := _cargo_net_links[i]
		var from := centres[link.x]
		var to := centres[link.y]
		var delta := to - from
		var length := delta.length()
		var edge := _cargo_net_cross_edges[i]
		# The in-plane width follows the native ribbon, retaining its 80 mm
		# unloaded width. Display depth supplies a rounded cross-section.
		var cross_edge := (vertices[edge.y] - vertices[edge.x] + vertices[edge.w] - vertices[edge.z]) * 0.25
		var basis := Basis(Vector3.ZERO, Vector3.ZERO, Vector3.ZERO)
		if length >= 0.001:
			var along := delta / length
			var right := cross_edge - along * cross_edge.dot(along)
			if right.length_squared() > 0.000000000001:
				var normal := right.normalized().cross(along)
				basis = Basis(right, delta, normal * CARGO_NET_HALF_DEPTH)
		var pose := Transform3D(basis, (from + to) * 0.5)
		_cargo_net_strands.set_instance_transform(i, pose)
		bounds = bounds.merge(pose * _cargo_net_strands.mesh.get_aabb())
	# One shared geometry batch per primitive; bounds include live deformation.
	# AABB stores size and reconstructs the endpoint; roundoff can otherwise
	# place a transformed knot one float ULP beyond the exact merged bounds.
	bounds = bounds.grow(0.0001)
	_cargo_net_knots.custom_aabb = bounds
	_cargo_net_strands.custom_aabb = bounds


# Poses moving kit bodies and cables from this frame's native snapshot.
# A disabled native body is hidden without replacing its physical state.
func _render_kit() -> void:
	_update_cargo_net()
	if _native == null:
		return
	for body in _kit_bodies.size():
		if not _kit_dynamic[body]:
			continue
		var node: Node3D = _kit_bodies[body]
		node.visible = bool(_native.is_kit_body_enabled(body))
		if node.visible:
			node.transform = _native.get_kit_body_render_transform(body)
	_update_plank_material()
	if _water_lift_bucket_water != null:
		var volume := clampf(float(_native.get_water_lift_bucket_water_m3()), 0.0, 2.0)
		var depth := clampf(volume / (1.54 * 1.54), 0.0, 0.94)
		_water_lift_bucket_water.visible = depth > 0.002
		if _water_lift_bucket_water.visible:
			var water_mesh := _water_lift_bucket_water.mesh as BoxMesh
			water_mesh.size = Vector3(1.54, depth, 1.54)
			_water_lift_bucket_water.position = Vector3(0.0, -0.29 + depth * 0.5, 0.0)
	for index in _kit_cables.size():
		var points: PackedVector3Array = _native.get_kit_cable_render_points(index)
		var segments: Array = _kit_cables[index]
		for segment in segments.size():
			var instance: MeshInstance3D = segments[segment]
			if segment + 1 < points.size():
				_lay_segment(instance, points[segment], points[segment + 1])
			else:
				instance.visible = false


func _lay_segment(instance: MeshInstance3D, from: Vector3, to: Vector3) -> void:
	var delta := to - from
	var length := delta.length()
	if length < 0.001:
		instance.visible = false
		return
	var along := delta / length
	var up := Vector3.UP if absf(along.y) < 0.99 else Vector3.RIGHT
	var side := up.cross(along).normalized()
	instance.transform = Transform3D(Basis(side, along.cross(side), along * length),
		from + delta * 0.5)
	instance.visible = true
