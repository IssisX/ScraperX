extends RefCounted

const Surface := preload("res://presentation/materials/freight_surface.gdshader")
# One finish per supplied palette resource, reused across every freight body.
# No per-part resources or frame updates; native material groups stay intact.
var _cache: Dictionary = {}

func for_part(entity: int, index: int, original: Material, dynamic: bool) -> Material:
	# Static freight retains the standard finish and whole-world batching.
	if not dynamic:
		return original
	# Native WarehouseForkliftYard and YardCraneGeometry identities only.
	if not (entity >= 2440 and entity <= 2447 or entity == 1450
			or entity >= 2450 and entity <= 2453):
		return original
	# Keep timber, concrete, hazard markings, tires and other surfaces intact.
	if index not in [0, 1, 5, 7] or not original is StandardMaterial3D:
		return original
	var key: Array = [original.get_instance_id(), index]
	if not _cache.has(key):
		var source := original as StandardMaterial3D
		var finish := ShaderMaterial.new()
		finish.shader = Surface
		finish.set_shader_parameter("base_color", source.albedo_color)
		finish.set_shader_parameter("painted", index == 7)
		finish.set_shader_parameter("base_metallic", 0.0 if index == 7 or index == 1 else 1.0)
		finish.set_shader_parameter("base_roughness", .56 if index == 7 else (.88 if index == 1 else .52))
		_cache[key] = finish
	return _cache[key]
