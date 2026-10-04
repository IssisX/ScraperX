extends SceneTree

# Retired AS-006 fixture proof: the bollard shackle leaves the native world
# when hooked, becomes a rendered carried body on UNHOOK, then disappears on
# HOOK. Verbs travel through the shipping touch input and native rig command.
class RenderDriver:
	extends "res://presentation/ui/ui_test_driver.gd"
	var _poses: Array[String] = []
	var _render_checks := 0
	var _saw_unhooked_visible := false
	var _saw_rehooked_hidden := false

	func _render_snapshot_pose(pose: String) -> bool:
		get_viewport().disable_3d = false
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		var kit := _main.get_node("KitPresentation") as Node3D
		var native: Object = _native()
		for index in int(native.get_kit_body_count()):
			var entity := int(native.get_kit_body_entity_id(index))
			var body := kit.get_node_or_null("KitBody%d" % entity) as Node3D
			if body == null:
				return _fail("native kit body has no presentation entity=%d pose=%s" % [entity, pose])
			if not bool(native.is_kit_body_dynamic(index)):
				continue
			var enabled := bool(native.is_kit_body_enabled(index))
			if body.visible != enabled or body.is_visible_in_tree() != enabled:
				return _fail("body native/tree visibility mismatch entity=%d enabled=%s visible=%s pose=%s" % [
					entity, enabled, body.visible, pose])
			var meshes := body.find_children("*", "MeshInstance3D", true, false)
			var has_drawable := false
			for item in meshes:
				var mesh := item as MeshInstance3D
				if mesh.mesh != null and mesh.is_visible_in_tree():
					has_drawable = true
			if enabled and not has_drawable:
				return _fail("enabled native body has no visible mesh entity=%d pose=%s" % [entity, pose])
			if enabled and not body.transform.is_equal_approx(native.get_kit_body_render_transform(index)):
				return _fail("moving body transform differs from native entity=%d pose=%s" % [entity, pose])
			_render_checks += 1

		for cable_index in int(native.get_kit_cable_count()):
			var points: PackedVector3Array = native.get_kit_cable_render_points(cable_index)
			for segment in 4:
				var cable := kit.get_node_or_null("KitCable%d_%d" % [cable_index, segment]) as MeshInstance3D
				if cable == null:
					return _fail("native cable segment missing cable=%d segment=%d" % [cable_index, segment])
				var expected_visible := segment + 1 < points.size()
				if expected_visible:
					expected_visible = points[segment].distance_to(points[segment + 1]) >= 0.001
				if cable.visible != expected_visible or cable.is_visible_in_tree() != expected_visible:
					return _fail("native/tree cable visibility mismatch cable=%d segment=%d pose=%s" % [
						cable_index, segment, pose])
				if expected_visible:
					if cable.mesh == null:
						return _fail("visible native cable has no mesh cable=%d segment=%d" % [cable_index, segment])
					if (cable.transform * Vector3(0, 0, -0.5)).distance_to(points[segment]) > 0.0001 \
						or (cable.transform * Vector3(0, 0, 0.5)).distance_to(points[segment + 1]) > 0.0001:
						return _fail("native cable endpoints mismatch cable=%d segment=%d pose=%s" % [
							cable_index, segment, pose])
				_render_checks += 1

		var shackle_index := int(native.get_kit_body_index(2002))
		if shackle_index < 0:
			return _fail("native AS-006 shackle entity 2002 missing")
		var shackle := kit.get_node("KitBody2002") as Node3D
		var enabled := bool(native.is_kit_body_enabled(shackle_index))
		if shackle.visible != enabled or shackle.is_visible_in_tree() != enabled:
			return _fail("shackle visibility differs from native hook state pose=%s" % pose)
		match pose:
			"shackle_hooked_initial":
				if enabled or int(native.get_carrying_entity_id()) != 0:
					return _fail("initial shackle was not hooked and out of hands")
			"shackle_unhooked":
				if not enabled or not shackle.is_visible_in_tree() or int(native.get_carrying_entity_id()) != 2002:
					return _fail("UNHOOK did not render the carried native shackle")
				_saw_unhooked_visible = true
			"shackle_rehooked":
				if enabled or shackle.is_visible_in_tree() or int(native.get_carrying_entity_id()) != 0:
					return _fail("HOOK did not remove the native shackle from the hands/world")
				_saw_rehooked_hidden = true
		_render_checks += 1
		_poses.append(pose)
		print("SCRAPERX_KIT_MECHANISM_DRAW pose=%s enabled=%s renderer=%s" % [
			pose, str(enabled), DisplayServer.get_name()])
		get_viewport().disable_3d = true
		return true

	func _draw_pose(pose: String) -> bool:
		return await _render_snapshot_pose(pose)

	func _touch_shackle() -> bool:
		var native: Object = _native()
		if not await _wait_until(func() -> bool: return bool(native.is_player_grounded()), 2.0):
			return _fail("AS-006 retired fixture spawn never grounded")
		for leg in [Vector2(-10.0, -129.2), Vector2(-10.2, -130.6), Vector2(-11.35, -131.95)]:
			if not await _walk_to(InputRouter.Device.TOUCH, leg, 0.2):
				return _fail("touch movement stalled approaching AS-006 shackle: %s" % _position())
		await _face(Vector2(-0.7, -0.7))
		if not await _offered(&"unhook", "UNHOOK"):
			return _fail("AS-006 shackle UNHOOK action not offered: %s" % _action_label())
		if not await _draw_pose("shackle_hooked_initial"):
			return _fail("initial shackle render failed: " + _detail)
		_act(InputRouter.Device.TOUCH)
		if not await _wait_until(func() -> bool:
			return int(native.get_carrying_entity_id()) == 2002 \
				and bool(native.is_kit_body_enabled(int(native.get_kit_body_index(2002)))), 0.5):
			return _fail("public UNHOOK did not release shackle entity 2002 into hands")
		if not await _draw_pose("shackle_unhooked"):
			return _fail("unhooked shackle render failed: " + _detail)
		if not await _offered(&"hook", "HOOK"):
			return _fail("native rehook action was not offered after unhook")
		_act(InputRouter.Device.TOUCH)
		if not await _wait_until(func() -> bool:
			return int(native.get_carrying_entity_id()) == 0 \
				and not bool(native.is_kit_body_enabled(int(native.get_kit_body_index(2002)))), 0.5):
			return _fail("public HOOK did not remove the shackle from hands and world")
		if not await _draw_pose("shackle_rehooked"):
			return _fail("rehooked shackle render failed: " + _detail)
		var expected_poses: Array[String] = ["shackle_hooked_initial", "shackle_unhooked", "shackle_rehooked"]
		if _poses != expected_poses or not _saw_unhooked_visible or not _saw_rehooked_hidden or _render_checks <= 0:
			return _fail("missing native shackle render stages poses=%s visible=%s hidden=%s checks=%d" % [
				str(_poses), str(_saw_unhooked_visible), str(_saw_rehooked_hidden), _render_checks])
		_detail = "as006_shackle_unhooked_visible_then_hooked_hidden poses=%d render_checks=%d" % [
			_poses.size(), _render_checks]
		return true

	func _run() -> void:
		await _frames(2)
		_tap(7, Vector2(_viewport_size().x * 0.7, _viewport_size().y * 0.35))
		await _frames(2)
		var ok := await _touch_shackle()
		print("SCRAPERX_KIT_MECHANISM %s mode=shackle %s" % ["PASS" if ok else "FAIL", _detail])
		get_tree().paused = false
		get_tree().quit(0 if ok else 31)


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
	if not _driver.begin(main, "touch_rig", ""):
		push_error("SCRAPERX_KIT_MECHANISM FAIL could not configure AS-006 retired fixture")
		quit(1)
		return
	# begin() selects the retired regression spawn after main._ready built the
	# default presentation. Rebuild this test view from the selected native
	# snapshot before the deferred touch driver starts.
	main._kit_view.free()
	main._setup_kit_view()
