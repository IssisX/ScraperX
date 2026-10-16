extends SceneTree
const Batch := preload("res://presentation/static_world_batch.gd")
const Geometry := preload("res://presentation/world_geometry.gd")
const Export := preload("res://presentation/solid_export.gd")
# Two ArrayMesh uploads encode unit normals into 16-bit octahedral pairs.
# Allow their bounded encoding error, while rejecting a wrong normal matrix.
const NORMAL_ENCODING_TOLERANCE := 16.0 / 65535.0
var failures := 0
func check(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("STATIC_SHADOW FAIL " + label)
func _initialize() -> void:
	_run.call_deferred()
func points(node: MeshInstance3D) -> Array:
	var result: Array = []
	var arrays := node.mesh.surface_get_arrays(0)
	var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	if indices.is_empty():
		for point in vertices: result.append(node.global_transform * point)
	else:
		for index in indices: result.append(node.global_transform * vertices[index])
	return result
func normals(node: MeshInstance3D) -> Array:
	var result: Array = []
	var arrays := node.mesh.surface_get_arrays(0)
	var source: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
	var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
	var basis := node.global_basis.inverse().transposed()
	if indices.is_empty():
		for normal in source: result.append((basis * normal).normalized())
	else:
		for index in indices: result.append((basis * source[index]).normalized())
	return result
func _run() -> void:
	var host := Node3D.new()
	root.add_child(host)
	host.position = Vector3(20, 5, -30)
	host.rotation.y = 0.3
	var geometry := Geometry.new(host)
	var red := geometry.material(Color("a53a27"), .4, .3)
	var blue := geometry.material(Color("254b6b"), .2, .85)
	var first := geometry.add_box("YardStructure", Vector3(2, 3, 4), Vector3(2, 4, 6), red)
	var second := geometry.add_box("YardStructure", Vector3(4, 2, 1), Vector3(8, 4, 3), blue)
	var curved := SphereMesh.new()
	curved.radius = 1.2
	curved.height = 2.4
	curved.radial_segments = 12
	curved.rings = 6
	curved.material = blue
	second.mesh = curved
	second.rotation.y = 0.4
	second.scale = Vector3(1, 1.5, .8)
	var distant := geometry.add_box("YardStructure", Vector3.ONE, Vector3(2, 92, 6), red)
	var short_range := geometry.add_box("YardStructure", Vector3.ONE, Vector3(12, 4, 6), blue)
	short_range.visibility_range_end = 12
	var moving := geometry.add_box("LiftPlatform", Vector3.ONE, Vector3(2, 4, 6), red)
	var glass_finish := blue.duplicate() as StandardMaterial3D
	glass_finish.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	var glass := geometry.add_box("YardLens", Vector3.ONE, Vector3(2, 4, 6), glass_finish)
	var double_sided := geometry.add_box("YardStructure", Vector3.ONE, Vector3(16, 4, 6), red)
	double_sided.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_DOUBLE_SIDED
	var before := Export.collect(host)
	var expected := points(first) + points(second)
	var expected_normals := normals(first) + normals(second)
	var wrong_matrix_gap := 0.0
	var source_normals: PackedVector3Array = second.mesh.surface_get_arrays(0)[Mesh.ARRAY_NORMAL]
	for normal in source_normals:
		var wrong := (second.global_basis * normal).normalized()
		var correct := (second.global_basis.inverse().transposed() * normal).normalized()
		wrong_matrix_gap = maxf(wrong_matrix_gap, wrong.distance_to(correct))
	check(wrong_matrix_gap > .1, "curved fixture rejects basis-transform bug far beyond normal-encoding tolerance")
	var batch := Batch.new()
	var stats := batch.build_tower(host)
	check(stats.shadow_sources == 2 and stats.shadow_batches == 1, "different opaque finishes share one shadow-only batch")
	check(first.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF and second.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF, "visible sources do not cast duplicate shadows")
	check(first.layers != 0 and second.layers != 0 and first.get_active_material(0) == red and second.get_active_material(0) == blue, "full visible colors/materials remain independently shaded")
	check(distant.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON and short_range.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON, "height cells and visibility ranges stay independent")
	check(moving.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON and glass.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON, "moving and translucent owners excluded")
	check(double_sided.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_DOUBLE_SIDED, "special double-sided cast mode retains its original owner")
	check(Export.collect(host) == before, "native exported collision unchanged")
	var shadow := host.get_node("StaticShadowBatch0") as MeshInstance3D
	check(shadow != null and shadow.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_SHADOWS_ONLY, "combined mesh supplies shadows only")
	if shadow != null:
		var actual := points(shadow)
		var actual_normals := normals(shadow)
		check(actual.size() == expected.size(), "all original shadow triangles retained")
		if actual.size() == expected.size():
			for i in actual.size():
				check(actual[i].distance_to(expected[i]) < .0001, "transformed shadow vertex matches original")
				check(actual_normals[i].distance_to(expected_normals[i]) < NORMAL_ENCODING_TOLERANCE, "curved nonuniform surface retains inverse-transpose shadow normal")
		check(shadow.gi_mode == GeometryInstance3D.GI_MODE_DISABLED, "shadow proxy adds no duplicate GI geometry")
	var count := host.get_child_count()
	batch.build_tower(host)
	check(host.get_child_count() == count, "repeated batching is idempotent")
	host.free()
	print("STATIC_SHADOW failures=", failures)
	quit(1 if failures else 0)
