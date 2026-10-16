extends SceneTree

# Exercise real presentation/mesh uploads with controlled read-only snapshots.
# Catches offscreen rebuilds, stale re-entry, point-only frustum tests and
# camera caching. No simulation replacement enters shipping code.
const NetView := preload("res://presentation/native_catch_net_view.gd")
class Snapshot:
	extends RefCounted
	var points := PackedVector3Array([Vector3(-1, -1, -5), Vector3(1, -1, -5), Vector3(0, 1, -5)])
	func get_catch_net_vertices() -> PackedVector3Array:
		return points
	func get_catch_net_indices() -> PackedInt32Array:
		return PackedInt32Array([0, 1, 2])

var _failures := 0
func _initialize() -> void:
	call_deferred("_run")

func _check(ok: bool, label: String) -> void:
	if not ok:
		_failures += 1
		push_error(label)

func _run() -> void:
	var source := Snapshot.new()
	var view := NetView.new()
	view.setup(source, StandardMaterial3D.new())
	_check(view._canvas.visible and view._last_vertices == source.points,
		"setup before entering scene tree retains normal mesh construction")
	root.add_child(view)
	view.render_view()
	_check(view._canvas.visible and view._last_vertices == source.points,
		"no current camera preserves headless mesh rebuilds")
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.make_current()
	view.render_view()
	var baseline: Array = view._canvas_mesh.surface_get_arrays(0)
	camera.position.x = 100.0
	var changed := source.points.duplicate()
	changed[2] = Vector3(0, 2, -5)
	source.points = changed
	view.render_view()
	_check(not view._canvas.visible, "fully offscreen canvas hidden")
	_check(view._canvas_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX] == baseline[Mesh.ARRAY_VERTEX],
		"offscreen snapshot does not rebuild mesh")
	camera.position.x = 0.0
	view.render_view()
	_check(view._canvas.visible and view._canvas_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX] == source.points,
		"first visible update uploads freshest native points")
	# Compare full arrays to the uncullable no-camera path, including normals,
	# UVs and tangents. Culling must not change visible surface construction.
	var visible_arrays: Array = view._canvas_mesh.surface_get_arrays(0)
	camera.clear_current()
	var reference := NetView.new()
	root.add_child(reference)
	# Preserve the same original material-coordinate topology before deformation.
	var reference_source := Snapshot.new()
	reference.setup(reference_source, StandardMaterial3D.new())
	reference_source.points = source.points
	reference.render_view()
	_check(reference._canvas_mesh.surface_get_arrays(0) == visible_arrays,
		"visible mesh retains uncullable geometry normals tangents and UVs")
	camera.make_current()
	# Every triangle corner lies outside the view, but its middle is visible.
	source.points = PackedVector3Array([Vector3(-100, -100, -5), Vector3(100, -100, -5), Vector3(0, 100, -5)])
	view.render_view()
	_check(view._canvas.visible and view._last_vertices == source.points,
		"frustum-crossing surface with outside corners remains visible")
	var replacement := Camera3D.new()
	replacement.position.x = 1000.0
	root.add_child(replacement)
	replacement.make_current()
	view.render_view()
	_check(not view._canvas.visible, "camera switch uses newly current camera")
	# Current native points, rather than the last uploaded bounds, re-enter view.
	source.points = PackedVector3Array([Vector3(999, -1, -5), Vector3(1001, -1, -5), Vector3(1000, 1, -5)])
	view.render_view()
	_check(view._canvas.visible and view._last_vertices == source.points,
		"deformation into frustum uses fresh bounds")
	# A translated presentation owner must use its world-space canvas bounds.
	view.position.x = -1000.0
	view.render_view()
	_check(not view._canvas.visible, "frustum test includes presentation transform")
	replacement.clear_current()
	view.render_view()
	_check(view._canvas.visible, "removing current camera restores uncullable path")
	print("SCRAPERX_CATCH_NET_VISIBILITY_TEST failures=%d" % _failures)
	quit(0 if _failures == 0 else 1)
