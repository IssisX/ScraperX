extends SceneTree

const View := preload("res://presentation/grappling_gun_view.gd")
var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("GRAPPLE_AIM_VIEW FAIL " + why)

func _run() -> void:
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.position = Vector3(3, 2, 4)
	var view := View.new()
	root.add_child(view)
	view.position = Vector3(50, 0, 0)
	view.configure_hook_geometry([{"polygon": PackedVector3Array([
		Vector3(-0.01, -0.01, 0), Vector3(0.01, -0.01, 0),
		Vector3(0.01, 0.01, 0), Vector3(-0.01, 0.01, 0)]), "z_low": -0.01, "z_high": 0.01}])
	view.configure(camera)
	var points := PackedVector3Array([Vector3(1, 1, -1), Vector3(1, 2, -3), Vector3(1, 1.5, -7)])
	var preview := {"points": points, "blocked": false, "line_limit": false, "charge_fraction": 0.5}
	var before := preview.duplicate(true)
	var line_transform: Transform3D = view._line.global_transform
	var node_count: int = view.get_child_count()
	view.update_aim_preview(preview, true, true)
	check(view._aim_guide.visible, "equipped charge displays supplied trajectory")
	check(view._aim_mesh.get_surface_count() == 1, "arc and endpoint use one surface")
	check(view._aim_guide.top_level and view._aim_guide.global_transform == Transform3D.IDENTITY,
		"trajectory stays in world coordinates under translated presentation owner")
	var faces: PackedVector3Array = view._aim_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	check(faces.size() == (points.size() - 1 + 12) * 6, "bounded triangle ribbon plus open endpoint ring")
	if faces.size() >= 12:
		check(((faces[0] + faces[1]) * 0.5).distance_to(points[0]) < 0.00001,
			"ribbon starts at supplied native launch point")
		check(((faces[8] + faces[11]) * 0.5).distance_to(points[2]) < 0.00001,
			"ribbon ends at supplied native endpoint")
	check(not view._aim_material.no_depth_test and view._aim_guide.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF,
		"world guide depth tests with no shadow or light")
	var cyan: Color = view._aim_material.albedo_color
	preview["blocked"] = true
	view.update_aim_preview(preview, true, true)
	check(view._aim_material.albedo_color.r > cyan.r, "obstruction uses amber rather than attachment cue")
	var mesh: ImmediateMesh = view._aim_mesh
	for i in 30:
		view.update_aim_preview(preview, true, true)
	check(view._aim_mesh == mesh and view._aim_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX] == faces and view.get_child_count() == node_count,
		"unchanged updates retain one resource and bounded nodes/geometry")
	check(view._line.global_transform == line_transform and not view._line.visible and before["points"] == preview["points"],
		"aim presentation leaves native points and physical rope presentation untouched")
	view.update_aim_preview(preview, true, false)
	check(not view._aim_guide.visible, "release hides guide")
	view.update_aim_preview(preview, false, true)
	check(not view._aim_guide.visible, "stow hides guide")
	view.update_aim_preview(preview, true, true)
	view.update_state({"equipped": false})
	check(not view._aim_guide.visible, "equipment state also hides stale guide")
	for invalid in [PackedVector3Array(), PackedVector3Array([Vector3.ZERO]),
		PackedVector3Array([Vector3.ZERO, Vector3.ZERO]),
		PackedVector3Array([Vector3.ZERO, Vector3(NAN, 0, 0)]),
		PackedVector3Array([Vector3.ZERO, Vector3(INF, 0, 0)])]:
		view.update_aim_preview(preview, true, true)
		view.update_aim_preview({"points": invalid}, true, true)
		check(not view._aim_guide.visible, "invalid or degenerate trajectory clears visibility")
	var too_many := PackedVector3Array()
	for i in 34:
		too_many.append(Vector3(0, 0, -i))
	view.update_aim_preview({"points": too_many}, true, true)
	check(not view._aim_guide.visible, "samples beyond native capacity are rejected")
	view.update_aim_preview({"points": points, "charge_fraction": NAN}, true, true)
	check(not view._aim_guide.visible, "non-finite charge hides guide")
	view.update_aim_preview(before, true, true)
	camera.position += Vector3(1, 0, 0)
	view.update_aim_preview(before, true, true)
	check(view._aim_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX] != faces, "camera movement refreshes billboard facing")
	view.reset_feedback()
	check(not view._aim_guide.visible, "reset hides guide")
	view.update_aim_preview(before, true, true)
	view.configure(camera)
	check(not view._aim_guide.visible and view.get_child_count() == node_count, "reconfigure hides guide without duplicating resources")
	var guide_ref: MeshInstance3D = view._aim_guide
	view.free()
	check(not is_instance_valid(guide_ref), "view teardown releases guide node")
	camera.free()
	print("GRAPPLE_AIM_VIEW checks=", checks, " failures=", failures, " supplied_preview_only=1")
	quit(0 if failures == 0 else 1)
