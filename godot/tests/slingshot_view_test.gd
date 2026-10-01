extends SceneTree

const View := preload("res://presentation/slingshot_view.gd")

class FakeSettings extends RefCounted:
	var head_bob := true
	var launch_cinematics := true

class FakeNative extends RefCounted:
	func get_kit_body_index(_entity: int) -> int:
		return 0
	func get_kit_body_parts(_body: int) -> PackedFloat32Array:
		return PackedFloat32Array([
			0.8, 0.8, 5.0, -3, 0, -5, 0, 0, 0, 1, 2, 0, 0,
			0.8, 0.8, 5.0, 3, 0, -5, 0, 0, 0, 1, 2, 0, 0])

class Fixture extends Node3D:
	var _settings := FakeSettings.new()
	var _router: Object = null
	var _native := FakeNative.new()
	var _yaw := 0.0
	var _pitch := 0.0


func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> bool:
	if condition:
		return true
	push_error("SCRAPERX_SLINGSHOT_VIEW FAIL " + message)
	quit(41)
	return false


func _run() -> void:
	var main := Fixture.new()
	root.add_child(main)
	var kit := Node3D.new()
	kit.name = "KitPresentation"
	main.add_child(kit)
	var frame := Node3D.new()
	frame.name = "KitBody1950"
	kit.add_child(frame)
	var box := BoxMesh.new()
	var material := StandardMaterial3D.new()
	material.metallic = 0.02
	material.albedo_color = Color("4a3420")
	box.material = material
	var timber := MeshInstance3D.new()
	timber.mesh = box
	frame.add_child(timber)
	var arms := Node3D.new()
	arms.name = "FirstPersonArms"
	main.add_child(arms)
	var camera := Camera3D.new()
	main.add_child(camera)
	var view := View.new()
	view.setup(main)
	var neutral := Vector3(6.0, 0.35, -55.0)
	var pouch := neutral + Vector3.BACK * 4.0
	var elevation := deg_to_rad(82.0)
	var anchor_midpoint := neutral + Vector3(0.0, sin(elevation), -cos(elevation)) * 10.0
	var state := {"station_available": true, "seated": true, "drawing": true,
		"released": false, "draw_m": 4.0, "max_draw_m": 12.0, "energy_j": 62000.0,
		"work_j": 75000.0, "source_power_w": 200000.0, "yaw_rad": 0.0,
		"band_rest_m": sqrt(109.0), "neutral_position": neutral,
		"retrieval_control_position": neutral + Vector3(1.5, 0.35, -1.8),
		"elevation_rad": elevation, "pouch_position": pouch,
		"anchor_left": anchor_midpoint + Vector3.LEFT * 3.0,
		"anchor_right": anchor_midpoint + Vector3.RIGHT * 3.0,
		"trajectory_points": PackedVector3Array([pouch, pouch + Vector3(0, 1, -2),
			pouch + Vector3(0, 2, -4), pouch + Vector3(0, 3, -6)]), "launch_count": 0}
	var player := pouch + Vector3.UP * 0.72
	var normal := Transform3D(Basis.IDENTITY, player + Vector3.UP * 0.62)
	camera.global_transform = normal
	view.update_view(1.0 / 60.0, state, player, Vector3.ZERO, camera)
	if not _check(view._pouch.position == pouch, "leather follows native pouch"):
		return
	if not _check(view._anchor_details[0].position == state.anchor_left,
			"rubber attachments follow native anchors"):
		return
	if not _check(view._preview_mesh.visible and view._preview_points == state.trajectory_points,
			"preview uses native predictor points"):
		return
	if not _check(not view.is_cinematic_active() and arms.visible,
			"draw keeps true first person"):
		return
	var bounds := timber.mesh.get_aabb()
	if not _check(absf(bounds.position.z + 10.0) < 0.00001 and absf(bounds.end.z) < 0.00001 \
			and absf(bounds.size.x - 7.6) < 0.00001 and absf(bounds.size.y - 1.6) < 0.00001,
			"beveled wood keeps exact heavy native timber bounds"):
		return
	state.launch_count = 1
	state.released = true
	# The real harness stays seated while its released shoe traverses the
	# physical launch rail. Forecast graphics must end at physical release.
	state.seated = true
	var native_before := state.duplicate(true)
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view.is_cinematic_active() and view._avatar.visible and not arms.visible,
			"release reveals human rider and hides first-person arms"):
		return
	if not _check(not view._preview_mesh.visible and not view._target_marker.visible,
			"release hides forecast while the harness remains on its guide"):
		return
	if not _check(view.get_simulation_scale() >= 0.16 and view.get_simulation_scale() < 0.18,
			"release asks for elapsed-time slow motion"):
		return
	if not _check(view._avatar.position == player and state == native_before,
			"cinematic rider follows native pose without mutating state"):
		return
	for i in 26:
		camera.global_transform = normal
		view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view._hud.has_thought_bubble(), "brief rising release gets its bounded thought bubble"):
		return
	for i in 130:
		camera.global_transform = normal
		camera.fov = 82.0
		view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(not view.is_cinematic_active() and not view._avatar.visible and arms.visible \
			and view.get_simulation_scale() == 1.0 and camera.global_transform == normal \
			and not view._hud.has_thought_bubble(),
			"brief orbit restores current POV, arms, and real time"):
		return
	state.launch_count = 2
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view.is_cinematic_active(), "second physical release begins another shot"):
		return
	view.cancel_cinematic()
	if not _check(not view.is_cinematic_active() and view.get_simulation_scale() == 1.0 \
			and not view._avatar.visible and not view._speed_rect.visible and arms.visible \
			and view._hud.state.is_empty() and not view._hud.has_thought_bubble(),
			"restart cancellation clears shot and stale launch interface"):
		return
	camera.global_transform = normal
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(not view.is_cinematic_active(), "restart baseline does not replay old release"):
		return
	state.launch_count = 3
	state.reduced_motion = true
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(not view.is_cinematic_active() and view.get_simulation_scale() == 1.0 \
			and not view._speed_rect.visible and camera.global_transform == normal,
			"reduced motion disables orbit, warp, and slow motion"):
		return
	state.reduced_motion = false
	main._settings.head_bob = false
	state.launch_count = 4
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(not view.is_cinematic_active(), "head-motion OFF also quiets launcher"):
		return
	main._settings.head_bob = true
	state.launch_count = 5
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view.is_cinematic_active(), "comfort-enabled next release starts cinema"):
		return
	main._settings.launch_cinematics = false
	if not _check(view.get_simulation_scale() == 1.0 and not view.is_cinematic_active() \
			and not view._avatar.visible and not view._speed_rect.visible and arms.visible,
			"disabling launch cinema immediately cancels its time request and body overlay"):
		return
	main._settings.launch_cinematics = true
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	state.launch_count = 6
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	main._settings.head_bob = false
	if not _check(view.get_simulation_scale() == 1.0 and not view.is_cinematic_active(),
			"head-motion OFF immediately cancels an existing launch sequence"):
		return
	main._settings.head_bob = true
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	state.launch_count = 7
	view.update_view(0.5, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view._hud.has_thought_bubble(), "fresh release thought appears from actual rising velocity"):
		return
	view.update_view(0.0, {}, player, Vector3.ZERO, camera)
	if not _check(not view._hud.has_thought_bubble() and view.get_simulation_scale() == 1.0 \
			and not view._avatar.visible, "empty native state clears lingering launch reaction"):
		return
	await process_frame
	print("SCRAPERX_SLINGSHOT_VIEW PASS")
	main.queue_free()
	await process_frame
	quit(0)
