extends RefCounted

# Native Material::Refractory; the existing Rubble slot remains 6.
const NATIVE_MATERIAL_INDEX := 8
const SHADER := preload("res://assets/materials/refractory/refractory_brick.gdshader")
const COLOUR_ROUGHNESS := preload("res://assets/materials/refractory/brick_colour_roughness.png")
const NORMAL_GL := preload("res://assets/materials/refractory/brick_normal_gl.png")
const ATLAS_SIZE := Vector4(768.0, 512.0, 768.0, 512.0)

# Native-resolution interiors from the actual 4K source, packed with extruded
# edge gutters. Rectangles span texel centres; no neighbouring mortar is used.
# provenance.json records the source rectangles and deterministic derivation.
const BRICK_INTERIORS: Array[Vector4] = [
	Vector4(60.5, 48.5, 135.0, 159.0),
	Vector4(290.5, 80.5, 187.0, 95.0),
	Vector4(550.5, 66.5, 179.0, 123.0),
	Vector4(34.5, 330.5, 187.0, 107.0),
	Vector4(288.5, 312.5, 191.0, 143.0),
	Vector4(576.5, 336.5, 127.0, 95.0),
]


static func build() -> ShaderMaterial:
	var material := ShaderMaterial.new()
	material.shader = SHADER
	material.set_shader_parameter(&"brick_colour_roughness", COLOUR_ROUGHNESS)
	material.set_shader_parameter(&"brick_normal_gl", NORMAL_GL)
	return material


static func apply_to(instance: MeshInstance3D, entity_id: int) -> void:
	# The supplied material identity is stable across poses and camera motion.
	# Complementary halves must share their original brick's identity/frame.
	# Share one material; only these scalar/vector instance values vary.
	var seed := (entity_id * 1664525 + 1013904223) & 0x7fffffff
	instance.set_instance_shader_parameter(&"refractory_seed", float((seed >> 3) & 0xffff) / 65535.0)
	instance.set_instance_shader_parameter(&"refractory_uv_rect", BRICK_INTERIORS[seed % BRICK_INTERIORS.size()] / ATLAS_SIZE)
