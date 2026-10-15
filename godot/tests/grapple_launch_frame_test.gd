extends SceneTree

const View := preload("res://presentation/grappling_gun_view.gd")
const MOUTH := Vector3(0, 0.012, -0.297)
var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("GUN_LAUNCH_FRAME FAIL " + why)

func sightline_clear(tool: MeshInstance3D, camera: Camera3D) -> bool:
	# Inspect actual rendered triangles, not a proxy bounding box. A sight
	# ray through the camera reticle must not hit the held tool's solid mesh.
	var inverse := tool.global_transform.affine_inverse()
	var origin := inverse * camera.global_position
	var direction := (inverse.basis * -camera.global_basis.z).normalized()
	var faces := tool.mesh.get_faces()
	for i in range(0, faces.size(), 3):
		var hit: Variant = Geometry3D.ray_intersects_triangle(origin, direction, faces[i], faces[i + 1], faces[i + 2])
		if hit is Vector3 and (hit - origin).dot(direction) > camera.near:
			return false
	return true

func _run() -> void:
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.near = 0.25
	var view := View.new()
	root.add_child(view)
	# A convex mesh fixture lets this isolated tool-pose check build the view.
	# Native hook geometry and physics have separate actual bridge/fire tests.
	view.configure_hook_geometry([{"polygon": PackedVector3Array([
		Vector3(-0.01, -0.01, 0), Vector3(0.01, -0.01, 0),
		Vector3(0.01, 0.01, 0), Vector3(-0.01, 0.01, 0)]), "z_low": -0.01, "z_high": 0.01}])
	view.configure(camera)
	for angles in [Vector3.ZERO, Vector3(0.65, 1.1, 0), Vector3(-0.7, -2.2, 0.12),
		Vector3(PI / 2.0 - 0.001, 0.6, 0), Vector3(-PI / 2.0 + 0.001, -1.1, 0),
		Vector3(PI / 2.0, 0, 0), Vector3(-PI / 2.0, 1.5, 0)]:
		camera.global_transform = Transform3D(Basis.from_euler(angles), Vector3(12, 40.62, -15))
		var direction := -camera.global_basis.z
		var origin := Vector3(12, 40.35, -15) + direction * 0.85
		var state := {"equipped": true, "launch_origin": origin, "launch_direction": direction}
		var frozen := state.duplicate(true)
		var camera_before := camera.global_transform
		view.reset_feedback()
		view.update_state(state)
		check(view._tool.visible and (view._tool.global_transform * MOUTH).distance_to(origin) < 0.00002,
			"visible barrel mouth equals supplied native origin at yaw/pitch " + str(angles))
		check((-view._tool.global_basis.z).normalized().dot(direction) > 0.99999,
			"barrel axis equals supplied native aim including vertical")
		check(view._tool.global_transform.is_finite() and state == frozen and camera.global_transform == camera_before,
			"finite presentation leaves camera and native receipt unchanged")
		view.present_feedback({"equipped": true, "fresh": true, "launch": true}, 0.0, false)
		view.update_state(state)
		check((view._tool.global_transform * MOUTH).distance_to(origin) > 0.001 and state == frozen,
			"existing decorative recoil never relocates supplied native launch")
		view.present_feedback({"equipped": true, "fresh": false}, 0.0, true)
		view.update_state(state)
		check((view._tool.global_transform * MOUTH).distance_to(origin) < 0.00002,
			"reduced motion returns exactly to native launch frame")
	var clear := true
	var near_clear := true
	for i in 65:
		var pitch := lerpf(-PI / 2.0, PI / 2.0, float(i) / 64.0)
		camera.global_transform = Transform3D(Basis.from_euler(Vector3(pitch, 0.6, 0.12)), Vector3(12, 40.62, -15))
		var direction := -camera.global_basis.z
		view.update_state({"equipped": true, "launch_origin": Vector3(12, 40.35, -15) + direction * 0.85,
			"launch_direction": direction})
		clear = clear and sightline_clear(view._tool, camera)
		var camera_inverse := camera.global_transform.affine_inverse()
		for point in view._tool.mesh.get_faces():
			near_clear = near_clear and -(camera_inverse * (view._tool.global_transform * point)).z > camera.near + 0.03
	check(clear, "actual launcher triangles keep reticle sightline clear through all65 pitch samples")
	check(near_clear, "entire actual launcher mesh clears shipping250mm near plane with30mm margin")
	view.update_state({"equipped": true, "launch_origin": Vector3.ZERO, "launch_direction": Vector3.ZERO})
	check(not view._tool.visible, "invalid direction cannot display a fabricated launch pose")
	view.update_state({"equipped": true})
	check(not view._tool.visible, "missing native launch receipt keeps tool quiet")
	print("GUN_LAUNCH_FRAME checks=", checks, " failures=", failures, " tool_fixture_only=1")
	view.free()
	camera.free()
	quit(0 if failures == 0 else 1)
