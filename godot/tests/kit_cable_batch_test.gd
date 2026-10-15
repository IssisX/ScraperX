extends SceneTree

# Actual native cable snapshots, including motion, drive the presentation.
# RenderOnly avoids gameplay input/UI but does not replace the native owner.
const Probe := preload("res://tests/tower_render_profile.gd")
var _failures := 0
var _checks := 0


func _initialize() -> void:
	call_deferred("_run")


func _check(ok: bool, label: String) -> void:
	_checks += 1
	if not ok:
		_failures += 1
		push_error(label)


func _run() -> void:
	root.disable_3d = true
	var packed: PackedScene = load("res://main.tscn")
	var world: Node3D = packed.instantiate()
	world.set_script(Probe.RenderOnly)
	root.add_child(world)
	if world._native == null:
		push_error("actual native bridge required")
		quit(1)
		return
	var kit: Node3D = world._kit_view
	kit.batch_cables()
	for frame in 4:
		world._native.advance_frame(1.0 / 90.0)
		var tick := int(world._native.get_tick_index())
		kit.render_view()
		_check(int(world._native.get_tick_index()) == tick, "cable presentation cannot advance simulation")
		var batch := kit.get_node_or_null("NativeCableBatch") as MultiMeshInstance3D
		_check(batch != null, "native cable batch exists")
		if batch == null:
			break
		var used := 0
		for cable in int(world._native.get_kit_cable_count()):
			var points: PackedVector3Array = world._native.get_kit_cable_render_points(cable)
			for segment in 4:
				var source := kit.get_node("KitCable%d_%d" % [cable, segment]) as MeshInstance3D
				_check(source.layers == 0, "source cable does not submit duplicate geometry")
				if segment + 1 >= points.size() or points[segment].distance_to(points[segment + 1]) < 0.001:
					continue
				var pose := batch.multimesh.get_instance_transform(used)
				_check((pose * Vector3(0, 0, -0.5)).distance_to(points[segment]) < 0.0001,
					"instanced cable starts at native endpoint")
				_check((pose * Vector3(0, 0, 0.5)).distance_to(points[segment + 1]) < 0.0001,
					"instanced cable ends at native endpoint")
				_check(batch.multimesh.custom_aabb.grow(0.0001).encloses(pose * source.mesh.get_aabb()),
					"updated cable bounds enclose real segment")
				used += 1
		_check(batch.multimesh.visible_instance_count == used, "only actual nondegenerate native cable segments draw")
		_check(batch.visible == (used > 0), "empty cable batch is hidden")
		await process_frame
	print("SCRAPERX_CABLE_BATCH_TEST failures=%d checks=%d" % [_failures, _checks])
	quit(0 if _failures == 0 else 1)
