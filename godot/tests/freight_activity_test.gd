extends SceneTree

const Activity := preload("res://presentation/freight_activity.gd")
class NativeSamples extends RefCounted:
	var tick := 0
	var deaths := 0
	var poses: Dictionary = {}
	var rendered: Dictionary = {}
	func get_kit_body_index(entity: int) -> int:
		return entity
	func get_tick_index() -> int:
		return tick
	func get_death_count() -> int:
		return deaths
	func get_kit_body_transform(body: int) -> Transform3D:
		return poses.get(body, Transform3D.IDENTITY)
	func get_kit_body_render_transform(body: int) -> Transform3D:
		return rendered.get(body, get_kit_body_transform(body))
	func is_kit_body_enabled(_body: int) -> bool:
		return true
var failures := 0
func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("FREIGHT_ACTIVITY FAIL " + message)
func _initialize() -> void:
	_run.call_deferred()
func _run() -> void:
	var native := NativeSamples.new()
	var view := Activity.new()
	root.add_child(view)
	view.setup(native)
	view.update_from_native(0.0, Vector3.ZERO, true)
	check(view.active_count() == 0, "initial native pose seeds an unlit interval")
	native.rendered[2440] = Transform3D(Basis.IDENTITY, Vector3(9, 0, 0))
	view.update_from_native(0.1, Vector3.ZERO, true)
	check(view.active_count() == 0, "interpolated presentation motion cannot light a beacon")
	native.tick += 9
	native.poses[2442] = Transform3D(Basis.IDENTITY, Vector3(0, 0.02, 0))
	view.update_from_native(0.1, Vector3.ZERO, true)
	check(view.active_count() == 1, "actual carriage motion lights the truck fitting")
	check(view._lenses.get_instance_custom_data(0).r == 1.0, "reduced motion is a steady warning")
	native.tick += 9
	native.poses[2451] = Transform3D(Basis(Vector3.UP, 0.05), Vector3.ZERO)
	view.update_from_native(0.1, Vector3.ZERO, false)
	check(view.active_count() == 2, "actual rotation is detected separately from translation")
	var mount: Transform3D = view._bases.get_instance_transform(0)
	check(mount.origin.is_equal_approx(Vector3(9.48, 1.97, -1.55)), "fitting follows its native-driven rendered body")
	for i in 20:
		native.tick += 9
		view.update_from_native(0.1, Vector3.ZERO, false)
	check(view.active_count() == 0, "stationary machines return to dark after the bounded hold")
	native.deaths += 1
	native.poses[2440] = Transform3D(Basis.IDENTITY, Vector3(100, 0, 0))
	view.update_from_native(0.1, Vector3.ZERO, false)
	check(view.active_count() == 0, "restore discontinuity cannot fabricate swept motion")
	native.tick = 0
	view.update_from_native(0.1, Vector3.ZERO, false)
	check(view.active_count() == 0, "reset native timeline seeds a new motion interval")
	native.tick = 9
	native.poses[2440] = Transform3D(Basis.IDENTITY, Vector3(100.1, 0, 0))
	view.update_from_native(0.1, Vector3.ZERO, false)
	check(view.active_count() == 1, "ordinary movement resumes after reset")
	view.reset()
	check(view.active_count() == 0, "pause and restart immediately quiesce the layer")
	view.update_from_native(0.1, Vector3(1000, 1000, 1000), false)
	check(view._bases.get_instance_transform(0).basis.determinant() == 0.0, "remote fittings are culled")
	var trolley_mount: Transform3D = view._bases.get_instance_transform(1)
	check(is_equal_approx(trolley_mount.origin.y - 0.025, 0.06) and is_equal_approx(trolley_mount.origin.z, -0.25), "crane fitting rests on the actual trolley crossmember")
	check(view.get_child_count() == 2, "fixed two-draw presentation budget")
	view.free()
	print("FREIGHT_ACTIVITY failures=", failures)
	quit(1 if failures else 0)
