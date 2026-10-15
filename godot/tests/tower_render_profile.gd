extends SceneTree

# Render-only production construction. No native stepping, player placement,
# traversal or device performance is established by this diagnostic.
# Run with -- --tower-batch=off|on [--inventory-only].
class RenderOnly:
	extends "res://presentation/main.gd"

	func _ready() -> void:
		set_process(false)
		_build_world()
		if ClassDB.class_exists("ScraperXSimulation"):
			_native = ClassDB.instantiate("ScraperXSimulation")
			_setup_kit_view()

	func _process(_delta: float) -> void:
		pass


func _initialize() -> void:
	call_deferred("_profile")


func _inventory(node: Node) -> Dictionary:
	var counts := {"meshes": 0, "multimeshes": 0, "instances": 0, "materials": {}}
	_collect(node, counts)
	counts["materials"] = counts["materials"].size()
	return counts


func _collect(node: Node, counts: Dictionary) -> void:
	if node is MeshInstance3D and node.mesh != null:
		counts["meshes"] += 1
		for surface in node.mesh.get_surface_count():
			var material: Material = node.get_active_material(surface)
			if material != null:
				counts["materials"][material.get_instance_id()] = true
	elif node is MultiMeshInstance3D and node.multimesh != null:
		counts["multimeshes"] += 1
		counts["instances"] += node.multimesh.instance_count
		var mesh: Mesh = node.multimesh.mesh
		if mesh != null:
			for surface in mesh.get_surface_count():
				var material: Material = node.material_override
				if material == null:
					material = mesh.surface_get_material(surface)
				if material != null:
					counts["materials"][material.get_instance_id()] = true
	for child in node.get_children():
		_collect(child, counts)


func _profile() -> void:
	root.size = Vector2i(160, 120)
	root.content_scale_size = Vector2i.ZERO
	var packed: PackedScene = load("res://main.tscn")
	var world: Node3D = packed.instantiate()
	world.set_script(RenderOnly)
	root.add_child(world)
	world.get_node("HUD").hide()
	var batch_on := "--tower-batch=on" in OS.get_cmdline_user_args()
	var profile_far := 2600.0
	var detail_distance := 0.0
	if "--shipping-defaults" in OS.get_cmdline_user_args():
		# Fresh defaults, never the owner's saved settings. The original
		# 2600m/unlimited-detail stress profile remains separately available.
		var settings: RefCounted = load("res://presentation/ui/settings_store.gd").new()
		profile_far = settings.view_distance
		detail_distance = settings.DETAIL_DISTANCES[settings.detail_distance]
		world._apply_visual_detail(world.get_node("TowerPresentation"), detail_distance)
	print("TOWER_PROFILE_CONFIG far_m=%s decorative_detail_m=%s shipping_defaults=%s" % [
		profile_far, detail_distance, "--shipping-defaults" in OS.get_cmdline_user_args()])
	print("TOWER_PROFILE_SOURCE main_sha256=%s batch=%s engine=%s renderer=%s size=%s shadow_atlas=project_default native_kit=%s" % [
		FileAccess.get_sha256("res://presentation/main.gd"), batch_on,
		Engine.get_version_info()["string"], RenderingServer.get_video_adapter_name(), root.size,
		world.get_node_or_null("KitPresentation") != null])
	print("TOWER_PROFILE_BEFORE tower=%s kit=%s" % [
		JSON.stringify(_inventory(world.get_node("TowerPresentation"))),
		JSON.stringify(_inventory(world.get_node("KitPresentation"))) if world.has_node("KitPresentation") else "unavailable"])
	if batch_on:
		var batch_script: GDScript = load("res://presentation/static_world_batch.gd")
		if batch_script == null:
			quit(2)
			return
		print("TOWER_PROFILE_BATCH_SOURCE sha256=%s" % FileAccess.get_sha256("res://presentation/static_world_batch.gd"))
		var batch: RefCounted = batch_script.new()
		print("TOWER_PROFILE_BATCH tower=%s" % JSON.stringify(batch.build_tower(world.get_node("TowerPresentation"))))
		if world.has_node("KitPresentation"):
			var kit: Node3D = world.get_node("KitPresentation")
			var static_roots: Array[Node3D] = []
			if kit.has_method("static_render_roots"):
				static_roots = kit.static_render_roots()
			else:
				for index in kit._kit_bodies.size():
					if not kit._kit_dynamic[index]:
						static_roots.append(kit._kit_bodies[index])
			print("TOWER_PROFILE_BATCH kit=%s" % JSON.stringify(batch.build_kit(kit, static_roots)))
			if kit.has_method("batch_cables"):
				kit.batch_cables()
	if "--inventory-only" in OS.get_cmdline_user_args():
		quit()
		return
	if DisplayServer.get_name() == "headless":
		push_error("Actual viewport draw counts require a rendering display")
		quit(3)
		return
	var camera: Camera3D = world.get_node("Camera")
	camera.fov = 82.0
	camera.far = profile_far
	var views := [
		["ground_up", Vector3(8, 2, -60), Vector3(-30, 800, -330)],
		["top_down", Vector3(-30, 1598, -280), Vector3(-30, 0, -250)],
	]
	for view in views:
		camera.position = view[1]
		camera.look_at(view[2])
		for frame in 3:
			await process_frame
			await RenderingServer.frame_post_draw
		print("TOWER_PROFILE_VIEW %s" % JSON.stringify({
			"view": view[0], "eye": str(view[1]), "target": str(view[2]),
			"draw_calls": RenderingServer.viewport_get_render_info(root.get_viewport_rid(), RenderingServer.VIEWPORT_RENDER_INFO_TYPE_VISIBLE, RenderingServer.VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME),
			"objects": RenderingServer.viewport_get_render_info(root.get_viewport_rid(), RenderingServer.VIEWPORT_RENDER_INFO_TYPE_VISIBLE, RenderingServer.VIEWPORT_RENDER_INFO_OBJECTS_IN_FRAME),
			"primitives": RenderingServer.viewport_get_render_info(root.get_viewport_rid(), RenderingServer.VIEWPORT_RENDER_INFO_TYPE_VISIBLE, RenderingServer.VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME),
			"total_draw_calls": Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
		}))
	world.queue_free()
	await process_frame
	quit()
