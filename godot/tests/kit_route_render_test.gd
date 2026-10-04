extends SceneTree

# The existing touch driver owns every gameplay action. Render only its
# telling poses on slow software hosts; inspect actual drawn native mirrors.
class RenderDriver:
	extends "res://presentation/ui/ui_test_driver.gd"
	var _rest := PackedVector3Array()
	var _draws := 0
	var _peak_deformation := 0.0

	func _pose(pose: String) -> void:
		await super._pose(pose)
		get_viewport().disable_3d = false
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		_draws += 1
		var kit := _main.get_node("KitPresentation") as Node3D
		var native: Object = _native()
		for body in int(native.get_kit_body_count()):
			var node := kit.get_node("KitBody%d" % int(native.get_kit_body_entity_id(body))) as Node3D
			if bool(native.is_kit_body_dynamic(body)):
				if node.visible != bool(native.is_kit_body_enabled(body)) or (node.visible \
					and not node.transform.is_equal_approx(native.get_kit_body_render_transform(body))):
					_reject("moving body snapshot")
		for cable in int(native.get_kit_cable_count()):
			var points: PackedVector3Array = native.get_kit_cable_render_points(cable)
			for segment in 4:
				var node := kit.get_node("KitCable%d_%d" % [cable, segment]) as MeshInstance3D
				if node.visible and segment + 1 < points.size():
					if (node.transform * Vector3(0, 0, -0.5)).distance_to(points[segment]) > 0.0001 \
						or (node.transform * Vector3(0, 0, 0.5)).distance_to(points[segment + 1]) > 0.0001:
						_reject("moving cable snapshot")
		var bucket := kit.get_node_or_null("KitBody2011/BucketWater") as MeshInstance3D
		if bucket != null:
			var depth := clampf(float(native.get_water_lift_bucket_water_m3()) / (1.54 * 1.54), 0.0, 0.94)
			if bucket.visible != (depth > 0.002):
				_reject("bucket water visibility")
			if bucket.visible and (absf((bucket.mesh as BoxMesh).size.y - depth) > 0.0001 \
				or absf(bucket.position.y - (-0.29 + depth * 0.5)) > 0.0001):
				_reject("bucket water volume geometry")
		var vertices: PackedVector3Array = native.get_cargo_net_vertices()
		if not vertices.is_empty():
			if _rest.is_empty():
				_rest = vertices
			for i in vertices.size():
				_peak_deformation = maxf(_peak_deformation, vertices[i].distance_to(_rest[i]))
			var net := kit.get_node("NativeSoftCargoNet") as Node3D
			var strands := net.get_node_or_null("Strands") as MultiMeshInstance3D
			var knots := net.get_node_or_null("Knots") as MultiMeshInstance3D
			if strands == null or knots == null:
				_reject("rounded substantial net strands and knots missing")
			else:
				var rope := strands.multimesh.mesh as CylinderMesh
				if rope == null or rope.top_radius < 0.03 or rope.bottom_radius < 0.03 or rope.radial_segments < 12:
					_reject("round rope profile")
				if knots.multimesh.instance_count != 207 or strands.multimesh.instance_count != 382:
					_reject("open native weave topology")
				for i in knots.multimesh.instance_count:
					var centre := Vector3.ZERO
					for corner in 4:
						centre += vertices[i * 4 + corner] * 0.25
					if knots.multimesh.get_instance_transform(i).origin.distance_to(centre) > 0.0001:
						_reject("knot interpolated native centre")
				# Independently authored 9x23 open weave: reject duplicate,
				# diagonal or missing links even if endpoints touch some knot.
				var expected_edges: Dictionary = {}
				for row in 23:
					for column in 9:
						var a := row * 9 + column
						if column < 8:
							expected_edges[Vector2i(a, a + 1)] = true
						if row < 22:
							expected_edges[Vector2i(a, a + 9)] = true
				for i in strands.multimesh.instance_count:
					var transform := strands.multimesh.get_instance_transform(i)
					if not transform.is_finite() or absf(transform.basis.z.length() * 2.0 - 0.07) > 0.0001 \
						or absf(transform.basis.x.dot(transform.basis.y)) > 0.0001:
						_reject("rounded world rope cross-section")
					if not strands.multimesh.custom_aabb.encloses(transform * rope.get_aabb()):
						_reject("loaded rope culling bounds")
					var ends: Array[int] = []
					for end in [-0.5, 0.5]:
						var endpoint := transform * Vector3(0, end, 0)
						var nearest := INF
						var knot := -1
						for j in knots.multimesh.instance_count:
							var distance := endpoint.distance_to(knots.multimesh.get_instance_transform(j).origin)
							if distance < nearest:
								nearest = distance
								knot = j
						if nearest > 0.0001:
							_reject("rope endpoint detached from native weave")
						ends.append(knot)
					var edge := Vector2i(mini(ends[0], ends[1]), maxi(ends[0], ends[1]))
					if not expected_edges.erase(edge):
						_reject("duplicate or non-native rope edge")
				if not expected_edges.is_empty():
					_reject("missing native rope edges")
			if pose == "cargo_net_climbing":
				var wrists: Array = _main._arms.wrist_positions()
				var holds: Array[Vector3] = [native.get_traversal_left_hand_render_position(),
					native.get_traversal_right_hand_render_position()]
				for i in wrists.size():
					if wrists[i].distance_to(holds[i]) > 0.065:
						_reject("planted net wrist leaves native hold")
			if pose == "cargo_net_first_ring" and _peak_deformation < 0.02:
				_reject("loaded net did not visibly deform")
		print("SCRAPERX_KIT_ROUTE_DRAW pose=%s bodies=%d cables=%d deformation_m=%.5f renderer=%s" % [
			pose, int(native.get_kit_body_count()), int(native.get_kit_cable_count()),
			_peak_deformation, DisplayServer.get_name()])
		get_viewport().disable_3d = true

	func _reject(reason: String) -> void:
		push_error("SCRAPERX_KIT_ROUTE_RENDER FAIL " + reason)
		get_tree().quit(1)


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("SCRAPERX_KIT_ROUTE_RENDER FAIL actual renderer required")
		quit(1)
		return
	# At 192x164 the UI's minimum scale puts CLIMB over the stick home;
	# that thumbnail is suitable for passive mesh tests, not touch driving.
	root.size = Vector2i(432, 371)
	root.content_scale_size = root.size
	root.disable_3d = true
	var main := (load("res://main.tscn") as PackedScene).instantiate()
	root.add_child(main)
	main._settings.apply_quality(0)
	main._apply_settings()
	var scenario := "touch_cargo_net"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--kit-route="):
			scenario = arg.trim_prefix("--kit-route=")
	var driver := RenderDriver.new()
	main._uitest = driver
	main.add_child(driver)
	if not driver.begin(main, scenario, ""):
		push_error("SCRAPERX_KIT_ROUTE_RENDER FAIL unknown route")
		quit(1)
