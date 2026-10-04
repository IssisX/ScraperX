extends SceneTree

# Exercise the water counterweight through the shipping viewport input path,
# then compare the rendered mirrors with the native authority at real draws.
class RenderDriver:
	extends "res://presentation/ui/ui_test_driver.gd"
	var _render_checks := 0
	var _poses: Array[String] = []

	func _pose(pose: String) -> void:
		get_viewport().disable_3d = false
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		var kit := _main.get_node("KitPresentation") as Node3D
		var native: Object = _native()
		var native_net: PackedVector3Array = native.get_cargo_net_vertices()
		var net := kit.get_node("NativeSoftCargoNet") as Node3D
		if native_net.is_empty() and net.visible:
			_fail("empty native cargo net remained visible pose=%s" % pose)
			return
		if not native_net.is_empty() and not net.visible:
			_fail("native cargo net with vertices was hidden pose=%s" % pose)
			return
		for index in int(native.get_kit_body_count()):
			var entity := int(native.get_kit_body_entity_id(index))
			var body := kit.get_node_or_null("KitBody%d" % entity) as Node3D
			if body == null:
				_fail("rendered kit body missing entity=%d pose=%s" % [entity, pose])
				return
			if bool(native.is_kit_body_dynamic(index)):
				var enabled := bool(native.is_kit_body_enabled(index))
				if body.visible != enabled:
					_fail("body enabled/visible mismatch entity=%d enabled=%s visible=%s pose=%s" % [
						entity, enabled, body.visible, pose])
					return
				if enabled and not body.transform.is_equal_approx(native.get_kit_body_render_transform(index)):
					_fail("native body pose mismatch entity=%d pose=%s" % [entity, pose])
					return
				_render_checks += 1
		for cable_index in int(native.get_kit_cable_count()):
			var points: PackedVector3Array = native.get_kit_cable_render_points(cable_index)
			for segment in 4:
				var cable := kit.get_node_or_null("KitCable%d_%d" % [cable_index, segment]) as MeshInstance3D
				if cable == null:
					_fail("rendered cable segment missing cable=%d segment=%d" % [cable_index, segment])
					return
				var expected_visible := segment + 1 < points.size()
				if expected_visible:
					expected_visible = points[segment].distance_to(points[segment + 1]) >= 0.001
				if cable.visible != expected_visible:
					_fail("native cable visibility mismatch cable=%d segment=%d pose=%s" % [
						cable_index, segment, pose])
					return
				if expected_visible:
					if (cable.transform * Vector3(0, 0, -0.5)).distance_to(points[segment]) > 0.0001 \
						or (cable.transform * Vector3(0, 0, 0.5)).distance_to(points[segment + 1]) > 0.0001:
						_fail("native cable endpoints mismatch cable=%d segment=%d pose=%s" % [
							cable_index, segment, pose])
						return
				_render_checks += 1
		var bucket_water := kit.get_node("KitBody2011/BucketWater") as MeshInstance3D
		var volume := float(native.get_water_lift_bucket_water_m3())
		var depth := clampf(volume / (1.54 * 1.54), 0.0, 0.94)
		if bucket_water.visible != (depth > 0.002):
			_fail("bucket water visibility mismatch volume=%.5f pose=%s" % [volume, pose])
			return
		if bucket_water.visible:
			var mesh := bucket_water.mesh as BoxMesh
			if absf(mesh.size.y - depth) > 0.0001 \
				or absf(bucket_water.position.y - (-0.29 + depth * 0.5)) > 0.0001:
				_fail("bucket water geometry mismatch volume=%.5f depth=%.5f pose=%s" % [
					volume, depth, pose])
				return
		_render_checks += 1
		_poses.append(pose)
		print("SCRAPERX_KIT_MECHANISM_DRAW pose=%s bucket_m3=%.5f cage_m=%.4f renderer=%s" % [
			pose, volume, float(native.get_water_lift_cage_travel_m()), DisplayServer.get_name()])
		get_viewport().disable_3d = true

	func _touch_water_lift() -> bool:
		# Existing regression spawn is on the valve deck. The pump is nearly
		# eleven metres away, so reach and start it with ordinary touch movement.
		if not await _walk_to(InputRouter.Device.TOUCH, Vector2(-14.0, -96.5), 0.65, 14.0):
			return _fail("touch movement did not reach the ground screw: %s" % _position())
		if not await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"screw_toggle", 2.0):
			return _fail("screw ACTION was not offered at %s" % _position())
		if bool(_native().is_water_screw_motor_enabled()):
			return _fail("ground screw unexpectedly started")
		_tap(0, _center(&"action"))
		if not await _wait_until(func() -> bool: return bool(_native().is_water_screw_motor_enabled()), 0.3):
			return _fail("touch ACTION did not start the native screw")
		await _pose("water_lift_empty_pump_running")
		var pump_samples := [0.5, 1.5, 1.99]
		for target in pump_samples:
			if not await _wait_until(func() -> bool:
				return float(_native().get_water_screw_tank_volume_m3()) >= target, 55.0):
				return _fail("pump failed to supply %.2f m3; tank=%.5f" % [
				target, float(_native().get_water_screw_tank_volume_m3())])
			await _pose("water_screw_tank_%.2f" % target)
		_tap(0, _center(&"action"))
		if not await _wait_until(func() -> bool: return not bool(_native().is_water_screw_motor_enabled()), 0.3):
			return _fail("touch ACTION did not stop the native screw")
		if not await _walk_to(InputRouter.Device.TOUCH, Vector2(-14.45, -107.0), 0.65, 14.0):
			return _fail("touch movement did not return to the lift valve: %s" % _position())
		if not await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"water_lift_valve", 2.0):
			return _fail("water-lift valve ACTION was not offered")
		if bool(_native().is_water_lift_valve_open()):
			return _fail("water-lift valve unexpectedly started open")
		_tap(0, _center(&"action"))
		if not await _wait_until(func() -> bool: return bool(_native().is_water_lift_valve_open()), 0.3):
			return _fail("touch ACTION did not open the native fill valve")
		await _pose("bucket_fill_start")
		for target in [0.2, 1.0, 1.98]:
			if not await _wait_until(func() -> bool:
				return float(_native().get_water_lift_bucket_water_m3()) >= target, 28.0):
				return _fail("bucket fill stopped below %.2f m3; bucket=%.5f tank=%.5f" % [
					target, float(_native().get_water_lift_bucket_water_m3()),
					float(_native().get_water_screw_tank_volume_m3())])
			await _pose("bucket_fill_%.2f" % target)
		_tap(0, _center(&"action"))
		if not await _wait_until(func() -> bool: return not bool(_native().is_water_lift_valve_open()), 0.3):
			return _fail("touch ACTION did not close the fill valve")
		# Use the clear north opening to walk onto the actual lift cage and
		# press its offered release control. No native pose/volume setters.
		if not await _walk_to(InputRouter.Device.TOUCH, Vector2(-12.50, -107.10), 0.14, 6.0):
			return _fail("touch movement missed the cage opening: %s" % _position())
		if not await _walk_to(InputRouter.Device.TOUCH, Vector2(-13.35, -107.65), 0.14, 5.0):
			return _fail("touch movement missed the cage release point: %s" % _position())
		if not await _wait_until(func() -> bool: return _ctx()["action"]["id"] == &"water_lift_release", 2.0):
			return _fail("loaded lift release ACTION was not offered at %s" % _position())
		await _pose("loaded_bucket_before_release")
		_tap(0, _center(&"action"))
		if not await _wait_until(func() -> bool: return float(_native().get_water_lift_cage_travel_m()) > 0.35, 8.0):
			return _fail("loaded lift did not move cage; bucket=%.4f cage=%.4f" % [
				float(_native().get_water_lift_bucket_water_m3()),
				float(_native().get_water_lift_cage_travel_m())])
		await _pose("loaded_cage_ascending")
		if not await _wait_until(func() -> bool: return bool(_native().is_water_lift_upper_catch_latched()), 18.0):
			return _fail("loaded cage missed its upper catch; cage=%.4f bucket=%.4f" % [
				float(_native().get_water_lift_cage_travel_m()),
				float(_native().get_water_lift_bucket_water_m3())])
		await _pose("loaded_cage_upper_catch")
		if not await _wait_until(func() -> bool: return float(_native().get_water_lift_bucket_water_m3()) < 0.01, 22.0):
			return _fail("bucket did not drain at lower stop; cage=%.4f bucket=%.5f" % [
				float(_native().get_water_lift_cage_travel_m()),
				float(_native().get_water_lift_bucket_water_m3())])
		await _pose("bucket_drained")
		_detail = "tank_filled_and_bucket_cycled poses=%d render_checks=%d cage_m=%.3f" % [
			_poses.size(), _render_checks, float(_native().get_water_lift_cage_travel_m())]
		return true


var _driver: RenderDriver


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	if DisplayServer.get_name() == "headless":
		push_error("SCRAPERX_KIT_MECHANISM FAIL actual renderer required")
		quit(1)
		return
	root.size = Vector2i(432, 371)
	root.content_scale_size = root.size
	root.disable_3d = true
	var main := (load("res://main.tscn") as PackedScene).instantiate()
	root.add_child(main)
	main._settings.apply_quality(0)
	main._apply_settings()
	_driver = RenderDriver.new()
	main._uitest = _driver
	main.add_child(_driver)
	if not _driver.begin(main, "touch_water_lift", ""):
		push_error("SCRAPERX_KIT_MECHANISM FAIL could not start existing touch driver")
		quit(1)
