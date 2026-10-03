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
		"seat_surface_position": pouch + Vector3.DOWN * 0.17,
		"simulation_time_seconds": 0.0, "rider_specific_acceleration": Vector3(0, 9.81, 0),
		"guided_launch": true,
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
	var opening_blur: Variant = view._speed_material.get_shader_parameter("soft_blur")
	if not _check(opening_blur != null and float(opening_blur) > 0.8,
			"bullet-time opens softly blurred before the accelerating clear orbit"):
		return
	if not _check(view.get_simulation_scale() == 1.0,
			"release first shows actual spring recoil before asking for slow motion"):
		return
	if not _check(view._avatar.position == player and state == native_before,
			"cinematic rider follows native pose without mutating state"):
		return
	# A fast rider must carry the opening lens with them. Blending from a
	# release-time world position used to leave the lens behind the body.
	player += Vector3.UP * 12.0
	normal.origin += Vector3.UP * 12.0
	state.simulation_time_seconds = 0.15
	state.rider_specific_acceleration = Vector3(0, 600, -100)
	camera.global_transform = normal
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 86.0, -10.0), camera)
	if not _check(camera.global_position.distance_to(player) < 4.5,
			"bullet-time opening follows rider displacement without a stale world anchor"):
		return
	for i in 26:
		camera.global_transform = normal
		view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view._hud.has_thought_bubble(), "brief rising release gets its bounded thought bubble"):
		return
	var first_rate := 0.0
	var last_rate := 0.0
	var previous_angle := 0.0
	var sampled := false
	var orbit_samples := 0
	for i in 160:
		camera.global_transform = normal
		camera.fov = 82.0
		view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
		var shot: Dictionary = view.get_cinematic_state()
		if view._cinematic_clock > 0.2 and view._cinematic_clock < view.ORBIT_SECONDS:
			var offset := camera.global_position - player
			var frame_basis: Basis = shot.get("orbit_frame", Basis.IDENTITY)
			var actual_angle := atan2(offset.dot(frame_basis.x), offset.dot(frame_basis.z))
			if not _check(absf(wrapf(actual_angle + float(shot.angle_rad), -PI, PI)) < 0.0001,
					"actual camera completes the measured orbital phase in the physical flight frame"):
				return
			if sampled:
				var rate := absf(wrapf(actual_angle - previous_angle, -PI, PI)) * 60.0
				if not _check(rate >= last_rate - 0.001, "orbital lens accelerates until 360 degrees"):
					return
				last_rate = rate
				if first_rate == 0.0:
					first_rate = rate
			previous_angle = actual_angle
			sampled = true
			orbit_samples += 1
		elif view._cinematic_clock >= view.ORBIT_SECONDS:
			if not _check(float(shot.orbit_phase) == 1.0 and float(shot.soft_blur) == 0.0,
					"exact full revolution is clear before POV handoff"):
				return
	if not _check(orbit_samples > 100 and last_rate > first_rate * 4.0, "slow opening rushes into a much faster complete sweep"):
		return
	if not _check(not view.is_cinematic_active() and not view._avatar.visible and arms.visible \
			and view.get_simulation_scale() == 1.0 and camera.global_transform == normal \
			and not view._hud.has_thought_bubble(),
			"brief orbit restores current POV, arms, and real time"):
		return
	state.launch_count = 2
	view.update_view(1.0 / 60.0, state, player, Vector3(0.0, 18.0, -10.0), camera)
	if not _check(view.is_cinematic_active(), "second physical release begins another shot"):
		return
	camera.global_transform = normal
	view.update_view(0.4, state, player, Vector3.ZERO, camera)
	var slow_frame := camera.global_position - player
	camera.global_transform = normal
	view.update_view(0.0, state, player, Vector3(0, 95, -35), camera)
	# Zero elapsed presentation time cannot jitter the lens.
	if not _check((camera.global_position - player).distance_to(slow_frame) < 0.001,
			"camera framing is stable when no presentation time passes"):
		return
	var previous_axis: Vector3 = view.get_cinematic_state()["orbit_frame"].y
	var changed_velocity := Vector3(70.0, 24.0, 40.0)
	camera.global_transform = normal
	view.update_view(0.5, state, player, changed_velocity, camera)
	var changed_axis: Vector3 = view.get_cinematic_state()["orbit_frame"].y
	if not _check(changed_axis.dot(changed_velocity.normalized()) > 0.97 \
			and changed_axis.distance_to(previous_axis) > 0.3,
			"positive elapsed time reorients the orbit toward a different actual flight direction"):
		return
	view.cancel_cinematic()
	if not _check(not view.is_cinematic_active() and view.get_simulation_scale() == 1.0 \
			and not view._avatar.visible and not view._speed_rect.visible and arms.visible \
			and view._hud.state.is_empty() and view._hud.landing_hint.is_empty() and not view._hud.has_thought_bubble(),
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
	if not _check(view._hud.landing_hint.is_empty(), "empty state cancels stale landing advice"):
		return
	state.launch_count = 7
	view.update_view(0.0, state, player, Vector3.ZERO, camera) # Baseline after cancellation.
	state.launch_count = 8
	state.seated = false
	state.player_grounded = false
	view.update_view(0.1, state, player, Vector3(0, 25, -10), camera)
	if not _check(not view._hud.landing_hint.is_empty(), "real new release starts contextual landing advice"):
		return
	state.player_grounded = true
	view.update_view(0.1, state, player, Vector3.ZERO, camera)
	state.player_grounded = false
	view.update_view(0.1, state, player, Vector3(0, 4, 0), camera)
	if not _check(view._hud.landing_hint.is_empty(), "ordinary jump after landing does not replay launch advice"):
		return
	await process_frame
	print("SCRAPERX_SLINGSHOT_VIEW PASS")
	main.queue_free()
	await process_frame
	quit(0)
