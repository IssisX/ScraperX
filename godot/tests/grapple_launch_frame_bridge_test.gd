extends SceneTree

# Shipping Main/native diagnostic rack stage. launch_origin is prospective
# current validated aim at interpolated player COM, never a last-shot receipt.
const STEP := 1.0 / 90.0
const MOUTH := Vector3(0.0, 0.012, -0.297)
var main: Node
var native: Object
var checks := 0
var failures := 0
var capture_dir := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("GRAPPLE_LAUNCH_BRIDGE FAIL " + why)

func step(count: int = 1, delta: float = STEP) -> void:
	for i in count:
		main._process(delta)

func aligned(why: String) -> void:
	var state: Dictionary = native.get_grapple_gun_render_state()
	var direction: Vector3 = state.launch_direction
	var origin: Vector3 = state.launch_origin
	var expected_direction := Vector3(-sin(main._yaw) * cos(main._pitch), sin(main._pitch), -cos(main._yaw) * cos(main._pitch))
	var expected_origin: Vector3 = native.get_player_render_position() + Vector3.UP * 0.35 + direction * 0.85
	check(direction.dot(expected_direction) > 0.99999 and absf(direction.length() - 1.0) < 0.00001,
		why + " validated pending aim reaches bridge before a new shot")
	check(origin.distance_to(expected_origin) < 0.0002, why + " prospective origin uses interpolated native player COM")
	var view: Node = main._grapple_view
	check(view._tool.visible and (view._tool.global_transform * MOUTH).distance_to(origin) < 0.00005,
		why + " actual model mouth equals native prospective launch origin")
	check((-view._tool.global_basis.z).normalized().dot(direction) > 0.99999 and view._tool.global_transform.is_finite(),
		why + " actual barrel axis equals native direction")

func nearest_depth(meshes: Array) -> float:
	var nearest := INF
	var camera_inverse: Transform3D = main._camera.global_transform.affine_inverse()
	for node in meshes:
		if not node is MeshInstance3D or node.mesh == null or not node.is_visible_in_tree():
			continue
		var to_camera: Transform3D = camera_inverse * node.global_transform
		for vertex: Vector3 in node.mesh.get_faces():
			nearest = minf(nearest, -(to_camera * vertex).z)
	return nearest

func near_plane_clear(why: String) -> void:
	var near: float = main._camera.near
	check(is_equal_approx(near, 0.25), why + " uses shipping250mm camera near plane")
	var tool_depth := nearest_depth([main._grapple_view._tool])
	check(is_finite(tool_depth) and tool_depth > near + 0.03, why + " all actual held-tool vertices clear near plane by30mm")
	var gloves: Array = []
	for hand in main._arms._hands:
		gloves.append_array(hand.root.find_children("*", "MeshInstance3D", true, false))
	var glove_depth := nearest_depth(gloves)
	check(not gloves.is_empty() and is_finite(glove_depth) and glove_depth > near + 0.005,
		why + " actual palm/finger meshes stay beyond shipping near plane")
	print("GRAPPLE_NEAR_DEPTH ", why, " tool=", tool_depth, " gloves=", glove_depth, " near=", near)

func capture(why: String) -> void:
	if capture_dir.is_empty():
		return
	await process_frame
	await RenderingServer.frame_post_draw
	check(root.get_texture().get_image().save_png(capture_dir.path_join(why + ".png")) == OK,
		"actual Godot capture " + why)

func _run() -> void:
	if not capture_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(capture_dir)
	root.size = Vector2i(864, 742)
	main = load("res://main.tscn").instantiate()
	main._settings_persistent = false
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	native = main._native
	check(native != null and not main._regression_scene and not main._ci_mode, "actual shipping Main/native world")
	check(native.debug_restart_at(Vector3(12, 485.15, -135.5)), "diagnostic rack stage accepted")
	step(45)
	main._router.push_verb(&"toggle_grapple")
	step(2)
	check(bool(native.get_grapple_gun_state().equipped), "router draw awaits actual native equipment acknowledgement")
	# Let real neutral native ticks and the shipping arm blend settle the grip.
	step(60)
	var work: float = native.get_grapple_gun_state().total_launch_work_j
	for angles in [Vector2(0.0, 0.25), Vector2(1.1, -0.7), Vector2(-2.2, 0.65), Vector2(0.6, PI * 0.5 - 0.001), Vector2(-1.1, -PI * 0.5 + 0.001)]:
		main._yaw = angles.x
		main._pitch = angles.y
		var tick: int = native.get_tick_index()
		step(1, 0.0)
		check(native.get_tick_index() == tick and float(native.get_grapple_gun_state().total_launch_work_j) == work,
			"prospective frame updates on pending aim without advancing native tick or firing")
		aligned("yaw/pitch " + str(angles))
	main._yaw = 0.0
	main._pitch = 0.25
	step(60)
	aligned("settled ready")
	near_plane_clear("settled ready")
	await capture("ready")
	var frozen: Dictionary = native.get_grapple_gun_render_state()
	var camera_before: Transform3D = main._camera.global_transform
	main._camera.global_position += Vector3(0.013, -0.027, 0.009)
	main._grapple_view.update_state(frozen)
	check((main._grapple_view._tool.global_transform * MOUTH).distance_to(frozen.launch_origin) < 0.00005
		and (-main._grapple_view._tool.global_basis.z).normalized().dot(frozen.launch_direction) > 0.99999,
		"presentation camera head-bob translation cannot drive native aim or displace muzzle")
	check(native.get_grapple_gun_render_state() == frozen, "camera-only presentation leaves native launch receipt and hooks untouched")
	main._camera.global_transform = camera_before
	main._yaw = 0.0
	main._pitch = 1.56
	step(60)
	aligned("near vertical ready")
	near_plane_clear("near vertical ready")
	await capture("near-vertical")
	main._pitch = 1.49
	step(60)
	var stock: int = native.get_grapple_gun_state().cartridges_remaining
	var sounds: int = main._audio.grapple_launches
	var receipts: int = main._grapple_view.launch_receipts
	main._router.push_verb(&"action")
	step(2)
	var launched: Dictionary = native.get_grapple_gun_state()
	check(float(launched.total_launch_work_j) > work and int(launched.cartridges_remaining) == stock - 1,
		"ordinary Action commits native finite launch work and spends one cartridge")
	check(main._audio.grapple_launches == sounds + 1 and main._grapple_view.launch_receipts == receipts + 1,
		"actual launch retains one audio and decorative recoil receipt")
	var hook_state: Dictionary = native.get_grapple_gun_render_state()
	var active := 0
	var exact := true
	for i in hook_state.hooks.size():
		var pose: Dictionary = hook_state.hooks[i]
		var mesh: MeshInstance3D = main._grapple_view._hooks[i]
		exact = exact and mesh.visible == bool(pose.active)
		if pose.active:
			active += 1
			exact = exact and mesh.global_transform.is_equal_approx(Transform3D(Basis(pose.rotation), pose.position))
	check(active == 1 and exact, "launched hook mesh preserves actual interpolated native body pose")
	await capture("launched")
	var native_position: Vector3 = native.get_player_position()
	var native_tick: int = native.get_tick_index()
	main._grapple_view.present_feedback({"equipped": true, "fresh": true, "launch": true}, 0.0, false)
	main._grapple_view.update_state(hook_state)
	check(native.get_grapple_gun_render_state() == hook_state and native.get_player_position() == native_position
		and native.get_tick_index() == native_tick, "decorative recoil cannot relocate native hook, aim, or body")
	main._grapple_view.present_feedback({"equipped": true, "fresh": false}, 0.0, true)
	main._grapple_view.update_state(hook_state)
	check((main._grapple_view._tool.global_transform * MOUTH).distance_to(hook_state.launch_origin) < 0.00005,
		"reduced motion returns muzzle to current prospective native launch frame")
	main._router.push_verb(&"toggle_grapple")
	step(2)
	check(not bool(native.get_grapple_gun_state().equipped) and not main._grapple_view._tool.visible
		and main._grapple_view.recoil_amount == 0.0 and not main._audio.grapple_motor_active,
		"native stow hides carried gun and clears feedback")
	check(int(native.get_grapple_gun_state().cartridges_remaining) == stock - 1, "stow never refunds spent cartridge")
	check(native.get_death_count() == 0, "diagnostic launch scenario has no recovery")
	print("GRAPPLE_LAUNCH_BRIDGE %s checks=%d native_tick=%d diagnostic_stage=1 phone_play=0" % ["FAIL" if failures else "PASS", checks, native.get_tick_index()])
	quit(1 if failures else 0)
