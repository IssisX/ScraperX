extends RefCounted

# Shared mesh/material construction; authored children retain their original host.
var presentation_root: Node3D
# Real bump mapping, not flat-shaded boxes. gl_compatibility (what CI renders
# with) has no SSAO/SSIL/SDFGI -- those are Forward+-only -- so per-pixel
# normal perturbation is the actual lever available here for surface detail,
# and it is a basic, renderer-agnostic feature confirmed working by direct
# probe (NoiseTexture2D.as_normal_map). Three shared textures (steel, timber,
# concrete), not one per material instance, since the grain frequency is what
# distinguishes them and dozens of unique noise textures would cost more than
# they are worth.
var bump_steel: NoiseTexture2D
var bump_timber: NoiseTexture2D
var bump_concrete: NoiseTexture2D


func _init(presentation_host: Node3D) -> void:
	presentation_root = presentation_host


func add_sphere(node_name: String, radius: float, at: Vector3,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 10
	sphere.rings = 6
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = sphere
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else presentation_root
	host.add_child(instance)
	return instance


func add_cone(node_name: String, radius: float, height: float, at: Vector3, material: Material,
		parent: Node3D = null) -> MeshInstance3D:
	var cone := CylinderMesh.new()
	cone.top_radius = 0.0
	cone.bottom_radius = radius
	cone.height = height
	cone.radial_segments = 9
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = cone
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else presentation_root
	host.add_child(instance)
	return instance


# A member spanning two points. Almost every diagonal in a steel frame is one
# of these -- braces, buttress legs, jib booms, tie rods -- and computing the
# pose from the endpoints is far less error-prone than hand-solving rotations.
func add_strut(node_name: String, from: Vector3, to: Vector3, thickness: float,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var delta := to - from
	var length := delta.length()
	if length < 0.01:
		return null
	var host: Node3D = parent if parent != null else presentation_root
	var instance := add_box_to(node_name, Vector3(thickness, thickness, length),
		from + delta * 0.5, material, host)
	var up := Vector3.UP if absf(delta.normalized().y) < 0.99 else Vector3.RIGHT
	instance.look_at(host.to_global(to), (host.global_transform.basis * up).normalized())
	return instance


func add_cylinder(node_name: String, radius: float, height: float, at: Vector3,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var cylinder := CylinderMesh.new()
	cylinder.top_radius = radius
	cylinder.bottom_radius = radius
	cylinder.height = height
	cylinder.radial_segments = 12
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = cylinder
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else presentation_root
	host.add_child(instance)
	return instance


# An exposed gearwheel: hub, spokes and a toothed rim, built in the local XY
# plane so the returned node can be rotated to face any direction and spun on
# its own Z axis. The references lean on these hard -- they are the single
# clearest signal that the building is a machine.
func add_gear(node_name: String, at: Vector3, radius: float, teeth: int,
		hub_material: Material, rim_material: Material) -> Node3D:
	var gear := Node3D.new()
	gear.name = node_name
	gear.position = at
	presentation_root.add_child(gear)

	var depth := maxf(0.5, radius * 0.16)
	# It turns, so its collider is the volume it sweeps: rim and teeth out to
	# 1.13 r, hub depth through.
	gear.set_meta(&"solid_disc", Vector2(radius * 1.13, depth * 1.5))
	gear.set_meta(&"part", node_name)
	add_box_to("GearHub", Vector3(radius * 0.38, radius * 0.38, depth * 1.5),
		Vector3.ZERO, hub_material, gear)
	for spoke in range(6):
		var spoke_mesh := add_box_to("GearSpoke",
			Vector3(radius * 1.7, radius * 0.11, depth * 0.8), Vector3.ZERO, hub_material, gear)
		spoke_mesh.rotation = Vector3(0.0, 0.0, PI * float(spoke) / 6.0)
	# Rim built from short chords, with a tooth standing proud of each joint.
	var rim_step := TAU / float(teeth)
	for index in range(teeth):
		var angle := rim_step * float(index)
		var chord := 2.0 * radius * tan(rim_step * 0.5) * 1.06
		var rim := add_box_to("GearRim", Vector3(chord, radius * 0.13, depth),
			Vector3(cos(angle) * radius, sin(angle) * radius, 0.0), rim_material, gear)
		rim.rotation = Vector3(0.0, 0.0, angle + PI * 0.5)
		var tooth_radius := radius * 1.075
		var tooth := add_box_to("GearTooth",
			Vector3(chord * 0.5, radius * 0.11, depth * 0.92),
			Vector3(cos(angle) * tooth_radius, sin(angle) * tooth_radius, 0.0), rim_material, gear)
		tooth.rotation = Vector3(0.0, 0.0, angle + PI * 0.5)
	return gear


# Painted text on the structure. Label3D keeps this readable at distance
# without needing an atlas, which is what sells the company's presence in the
# references -- the building talks at you.
func add_sign_text(text: String, at: Vector3, yaw: float, height_meters: float,
		color: Color, parent: Node3D = null) -> Label3D:
	var label := Label3D.new()
	label.text = text
	label.font_size = 64
	label.pixel_size = height_meters / 64.0
	label.modulate = color
	label.double_sided = true
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.position = at
	label.rotation = Vector3(0.0, yaw, 0.0)
	# Human-scale instructions are unreadable from distant rings. Large
	# landmark lettering keeps its authored long-range visibility.
	if height_meters <= 0.3:
		label.visibility_range_end = 80.0
	var host: Node3D = parent if parent != null else presentation_root
	host.add_child(label)
	return label


func build_bump_textures() -> void:
	bump_steel = make_bump_texture(1, 0.5, 1.8, false)
	bump_timber = make_bump_texture(2, 0.7, 2.6, true)
	bump_concrete = make_bump_texture(3, 0.7, 1.3, false)


func make_bump_texture(seed_value: int, frequency: float, strength: float,
		directional: bool) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.frequency = frequency
	noise.fractal_octaves = 4
	noise.fractal_lacunarity = 2.1
	if directional:
		# Wood grain: stretched noise reads as fibrous rather than pitted.
		noise.frequency = frequency * 0.2
		noise.fractal_type = FastNoiseLite.FRACTAL_RIDGED
	var tex := NoiseTexture2D.new()
	tex.width = 256
	tex.height = 256
	tex.seamless = true
	tex.generate_mipmaps = true
	tex.as_normal_map = true
	tex.bump_strength = strength
	tex.noise = noise
	return tex


func material(color: Color, metallic: float, roughness: float,
		emission: Color = Color.BLACK, emission_energy: float = 1.0,
		bump: NoiseTexture2D = null) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
	if bump != null:
		material.normal_enabled = true
		material.normal_texture = bump
		material.uv1_triplanar = true
		material.uv1_triplanar_sharpness = 1.0
		material.uv1_scale = Vector3(0.22, 0.22, 0.22)
	return material


func add_box(node_name: String, size: Vector3, at: Vector3, material: Material,
		roll: float = 0.0) -> MeshInstance3D:
	return add_box_to(node_name, size, at, material, presentation_root, roll)


func add_box_to(node_name: String, size: Vector3, at: Vector3, material: Material,
		parent: Node3D, roll: float = 0.0) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = mesh
	instance.position = at
	if not is_zero_approx(roll):
		instance.rotation = Vector3(0.0, 0.0, roll)
	parent.add_child(instance)
	return instance
