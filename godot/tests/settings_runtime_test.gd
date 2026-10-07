extends SceneTree

# Exercises the shipping main scene, pause controls, renderer resources and
# native restart authority. Run with --fixed-fps 90. Optional genuine captures:
# -- --capture-dir=/tmp/scraperx-current-evidence

const SettingsStore := preload("res://presentation/ui/settings_store.gd")

var _checks := 0
var _failures: Array[String] = []
var _capture_dir := ""


func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			_capture_dir = argument.trim_prefix("--capture-dir=")
	_run.call_deferred()


func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error("SCRAPERX_SETTINGS_RUNTIME FAIL " + message)


func _frames(count: int) -> void:
	for _frame in range(count):
		await process_frame


func _button(node: Node, caption: String) -> Button:
	if node is Button and node.text == caption:
		return node as Button
	for child in node.get_children():
		var found := _button(child, caption)
		if found != null:
			return found
	return null


func _capture(name: String) -> void:
	if _capture_dir.is_empty():
		return
	if DisplayServer.get_name() == "headless":
		_check(false, "capture requires an actual renderer/display")
		return
	DirAccess.make_dir_recursive_absolute(_capture_dir)
	await RenderingServer.frame_post_draw
	var shot := root.get_texture().get_image()
	_check(shot != null and not shot.is_empty(), "capture image exists for " + name)
	if shot != null and not shot.is_empty():
		_check(shot.save_png(_capture_dir.path_join(name + ".png")) == OK, "save capture " + name)


func _run() -> void:
	root.size = Vector2i(432, 371)
	root.content_scale_size = Vector2i(432, 371)
	if DisplayServer.get_name() != "headless":
		DisplayServer.window_set_size(Vector2i(432, 371))
	var main = load("res://main.tscn").instantiate()
	root.add_child(main)
	_check(main._native != null, "Godot 4.7 native extension loads in the full scene")
	if main._native == null:
		quit(42)
		return
	main._settings.persistent = false
	main._router.enabled = false
	main._router.capture_mouse = false
	Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	await _frames(8)
	_check(not main._regression_scene, "normal campaign scene remains active")
	var native: Object = main._native
	var menu: Control = main._pause_menu
	main._open_pause()
	_check(paused and menu.visible, "opening menu pauses the full game")
	var tick := int(native.get_tick_index())
	await _frames(8)
	_check(native.get_tick_index() == tick, "native clock stays frozen while paused")
	menu._show_page(&"controls")
	await _frames(2)
	await _capture("pause_controls_current")
	menu._show_page(&"graphics")
	await _frames(2)
	var quality: Button = menu._page_first[&"graphics"]
	# Real control -> preset -> main settings handler -> actual viewport/light.
	main._settings.quality = 2
	quality.pressed.emit()
	_check(main._settings.quality == 3 and root.msaa_3d == Viewport.MSAA_4X,
		"quality control reaches viewport MSAA")
	_check(main._camera.far == 6000.0 and main.get_node("Overcast").directional_shadow_max_distance == 300.0,
		"quality reaches camera horizon and sun shadow distance")
	_check(menu.settings_overflow() <= 0.0, "graphics controls remain accessible")
	await _capture("pause_graphics_current")
	var saved_hour: float = main._sky_cycle.hour
	main._sky_cycle.set_hour(0.0)
	_check(main.get_node("Overcast").light_energy >= 0.85 and main.get_node("SkyFill").light_energy >= 0.35,
		"night uses readable directional moonlight and unshadowed fill")
	_check(main._sky_cycle._sky_material.get_shader_parameter(&"light_is_moon"), "night selects the moon disc")
	main._sky_cycle.set_hour(12.0)
	_check(is_equal_approx(main.get_node("Overcast").light_energy, 2.4)
		and is_equal_approx(main.get_node("SkyFill").light_energy, 0.55), "day lighting energies stay unchanged")
	main._sky_cycle.set_hour(saved_hour)
	menu._show_page(&"audio")
	var mix_toggle := _button(menu, "SHOW AUDIO MIX")
	var credits_toggle := _button(menu, "SHOW AUDIO CREDITS")
	_check(mix_toggle != null and credits_toggle != null, "audio mix and credits remain accessible")
	var mix_details: Control = mix_toggle.get_parent().get_child(mix_toggle.get_index() + 1)
	var credits_details: Control = credits_toggle.get_parent().get_child(credits_toggle.get_index() + 1)
	_check(not mix_details.visible and not credits_details.visible, "audio starts with primary volume and mute controls")
	mix_toggle.button_pressed = true
	credits_toggle.button_pressed = true
	_check(mix_details.is_visible_in_tree() and credits_details.is_visible_in_tree(), "audio details open through real controls")
	var notice := FileAccess.get_file_as_string("res://assets/audio/footsteps/NOTICE.txt")
	_check(not notice.is_empty() and credits_details.get_child(0).text == notice + "\n" + FileAccess.get_file_as_string("res://assets/audio/fall/NOTICE.md"), "audio credits preserve the complete source notice")
	main._settings.audio_muted = true
	main._apply_settings()
	_check(AudioServer.is_bus_mute(AudioServer.get_bus_index("Master")), "mute reaches real audio bus")
	main._settings.audio_muted = false
	main._settings.apply_quality(0)
	main._settings.fps_cap = 5
	main._apply_settings()
	_check(root.scaling_3d_scale == 0.75 and main._camera.far == 1000.0 and Engine.max_fps == 45,
		"LOW quality and 45 FPS reach actual renderer resources")
	var decoration_count := 0
	var structural_count := 0
	for mesh in main.get_node("TowerPresentation").find_children("*", "MeshInstance3D", true, false):
		var part: String = mesh.get_meta(&"part", "")
		if part in ["FaceBand", "FaceDuct", "FloorLight", "FloorLightHigh", "TimberCladding",
				"LaneStripe", "TreeTrunk", "TreeCanopy", "ScrubLobe"]:
			decoration_count += 1
			_check(mesh.visibility_range_end == 150.0, "decorative LOD reaches " + part)
		else:
			structural_count += 1
			_check(mesh.visibility_range_end == 0.0, "structural route remains visible for " + part)
	_check(decoration_count > 50 and structural_count > 50, "real scene geometry exercised")
	main._settings.apply_quality(2)
	main._settings.fps_cap = 0
	main._apply_settings()
	menu._show_page(&"restart")
	var advanced_toggle := _button(menu, "SHOW ADVANCED RESTART")
	_check(advanced_toggle != null, "advanced restart disclosure exists")
	var advanced_details: Control = advanced_toggle.get_parent().get_child(advanced_toggle.get_index() + 1)
	_check(not advanced_details.is_visible_in_tree(), "restart defaults to supported destinations")
	_check(_button(menu, "RESTART AT GRADE").is_visible_in_tree(), "grade restart is directly accessible")
	main._settings.restart_ring = 10
	menu._refreshers[&"restart_ring"].call()
	await _frames(2)
	_check(menu.settings_overflow() <= 0.0, "restart controls remain accessible")
	await _capture("pause_restart_current")
	var ring_button := _button(menu, "RESTART ON RING")
	_check(ring_button != null, "supported ring restart control exists")
	if ring_button == null:
		quit(42)
		return
	var ring_choices := menu.find_children("*", "OptionButton", true, false)
	_check(ring_choices.size() == 1, "supported rings are directly selectable")
	if ring_choices.size() != 1:
		quit(42)
		return
	var ring_select := ring_choices[0] as OptionButton
	_check(ring_select.item_count == SettingsStore.RESTART_RING_HEIGHTS.size(), "every supported ring appears in the selector")
	var deaths := int(native.get_death_count())
	_button(menu, "RESTART AT GRADE").pressed.emit()
	_check(not paused and not menu.visible and main._settings.restart_ring == 0,
		"grade control resumes through the native supported preset")
	_check(native.get_player_position().distance_to(main._settings.ring_position()) < 0.001,
		"grade control reaches the authoritative grade destination")
	for ring in range(SettingsStore.RESTART_RING_HEIGHTS.size()):
		if not main._paused:
			main._open_pause()
		ring_select.select(ring)
		ring_select.item_selected.emit(ring)
		var expected: Vector3 = main._settings.ring_position()
		ring_button.pressed.emit()
		_check(not paused and not menu.visible, "accepted ring restart resumes game at ring %d" % ring)
		_check(native.get_player_position().distance_to(expected) < 0.001,
			"menu signal reaches native destination at ring %d" % ring)
		await _frames(12)
		var actual: Vector3 = native.get_player_position()
		_check(native.is_player_grounded() and absf(actual.y - expected.y) < 0.06,
			"ring %d provides native-supported footing y=%.3f" % [ring, actual.y])
		_check(native.get_death_count() == deaths, "ring restart is not a death")
	# The exact current standing pose includes normal Jolt contact slop. It
	# must be accepted without requiring an artificial lift above the deck.
	main._open_pause()
	menu._show_page(&"restart")
	var standing: Vector3 = native.get_player_position()
	advanced_toggle.button_pressed = true
	_check(advanced_details.is_visible_in_tree(), "height and XYZ expand through the real control")
	_button(menu, "USE CURRENT XYZ").pressed.emit()
	_button(menu, "RESTART AT XYZ").pressed.emit()
	_check(not paused and not menu.visible, "current standing XYZ restart is accepted")
	_check(native.get_player_position().distance_to(standing) < 0.05,
		"current standing XYZ restart preserves the chosen point")
	# A rejected occupied destination leaves both simulation and pause intact.
	main._open_pause()
	menu._show_page(&"restart")
	var before: Vector3 = native.get_player_position()
	var before_checkpoint: Vector3 = native.get_checkpoint_position()
	main._settings.restart_x = -30.0
	main._settings.restart_y = 800.0
	main._settings.restart_z = -330.0
	_button(menu, "RESTART AT XYZ").pressed.emit()
	_check(paused and menu.visible and main._paused, "occupied restart remains paused with menu")
	_check(native.get_player_position().is_equal_approx(before), "occupied restart preserves player pose")
	_check(native.get_checkpoint_position().is_equal_approx(before_checkpoint), "occupied restart preserves checkpoint")
	_check(not menu._restart_status.text.is_empty(), "occupied restart supplies feedback")
	# Footing commits every grounded tick. Jump from a real deck so the
	# checkpoint remains on the deck while the player moves above it.
	main._resume()
	main._router.enabled = false
	native.set_move_input(-1.0, 0.0)
	for _step in range(16):
		native.advance_frame(native.get_fixed_step_seconds())
	native.set_move_input(0.0, 0.0)
	_check(native.request_jump(), "checkpoint proof can jump from supported ring")
	for _step in range(12):
		native.advance_frame(native.get_fixed_step_seconds())
	main._ctx = main._read_context()
	var checkpoint: Vector3 = native.get_checkpoint_position()
	_check(native.get_player_position().distance_to(checkpoint) > 0.05,
		"checkpoint restoration begins from a distinct player pose")
	main._open_pause()
	_button(menu, "RESTART AT CHECKPOINT").pressed.emit()
	_check(not paused and not menu.visible, "checkpoint control resumes game")
	_check(native.get_player_position().distance_to(checkpoint) < 0.001,
		"checkpoint menu signal invokes native restoration")
	_check(native.get_death_count() == deaths, "checkpoint menu restart does not add a death")
	_check_lowering_drop(main)
	_check_canopy(main)
	print("SCRAPERX_SETTINGS_RUNTIME checks=%d failures=%d rings=%d decorative=%d structural=%d native=1" %
		[_checks, _failures.size(), SettingsStore.RESTART_RING_HEIGHTS.size(), decoration_count, structural_count])
	var launcher: Dictionary = native.get_slingshot_state()
	print("SCRAPERX_SETTINGS_RUNTIME_SOURCE draw_limit=%.2f power_limit=%.0f neutral=%s" % [
		float(launcher.get("max_draw_m", 0.0)), float(launcher.get("max_source_power_w", 0.0)),
		str(launcher.get("neutral_position", Vector3.ZERO))])
	paused = false
	main.queue_free()
	await process_frame
	quit(0 if _failures.is_empty() else 42)


func _check_lowering_drop(main: Node) -> void:
	# Real cargo receiver, real context and touch hit testing. Staging supplies
	# footing only; the two Drop presses must enter and cancel native lowering.
	var native: Object = main._native
	_check(native.debug_restart_at(Vector3(22, 12, -120.3)), "cargo Drop staging accepted")
	native.set_move_input(0.0, 0.0)
	native.set_facing(-1.0, 0.0)
	for _step in range(180):
		native.advance_frame(native.get_fixed_step_seconds())
	main._router.clear_held()
	main._router.gameplay_active = true
	for press in range(2):
		main._ctx = main._read_context()
		main._touch.update_context(main._ctx, 1.0)
		_check(main._touch.is_button_shown(&"drop"), "Drop remains reachable at cargo lowering press %d" % press)
		_check(main._touch.button_label(&"drop") == ("DROP DOWN" if press == 0 else "DROP"),
			"Drop caption distinguishes lowering entry from detachment")
		var touch := InputEventScreenTouch.new()
		touch.index = 17
		touch.position = main._touch.button_center(&"drop")
		touch.pressed = true
		main._touch.handle_touch(touch)
		touch.pressed = false
		main._touch.handle_touch(touch)
		var input: Dictionary = main._router.frame(native.get_fixed_step_seconds())
		_check(input["verbs"].has(&"drop"), "cargo touch press produces the Drop verb")
		main._dispatch(input["verbs"], native.get_fixed_step_seconds())
		native.advance_frame(native.get_fixed_step_seconds())
		_check(native.get_traversal_state() == (5 if press == 0 else 0),
			"cargo touch Drop enters Lowering then releases it")


func _check_canopy(main: Node) -> void:
	var native: Object = main._native
	_check(native.debug_restart_at(Vector3(6.0, 18.0, -58.0)), "canopy proof starts in real free fall")
	native.set_move_input(0.0, 0.0)
	native.advance_frame(native.get_fixed_step_seconds())
	native.request_parachute()
	native.advance_frame(native.get_fixed_step_seconds())
	main._ctx = main._read_context()
	var intent := {"pendant": {}, "move": Vector2.ZERO}
	var camera: Transform3D = main._camera.global_transform
	main._arms.update_arms(main._arms_state(intent), camera, 0.1)
	_check(native.is_parachute_deployed() and main._arms.canopy_visible() and main._arms.risers_visible(),
		"native deployment displays cloth and risers")
	var cloth: MeshInstance3D = main._arms._canopy
	_check(cloth.mesh.get_aabb().size.x > 5.0 and cloth.global_position.y > camera.origin.y + 2.5,
		"canopy has an overhead cloth surface visible when looking up")
	for i in main._arms._risers.size():
		var line: MeshInstance3D = main._arms._risers[i]
		var endpoint: Vector3 = line.global_transform * Vector3(0.0, 0.5, 0.0)
		var local: Vector3 = cloth.global_transform.affine_inverse() * endpoint
		var expected_y := 0.55 * (1.0 - pow(local.x / 2.6, 2.0))
		_check(absf(local.z - 1.0) < 0.001 and absf(local.y - expected_y) < 0.001,
			"riser %d terminates on the cloth attachment" % i)
	var overhead: Vector3 = cloth.global_position
	var look_up := Transform3D(Basis.looking_at(Vector3(0.0, 0.94, -0.342).normalized(), Vector3.UP), camera.origin)
	main._camera.global_transform = look_up
	main._arms.update_arms(main._arms_state(intent), look_up, 0.1)
	_check(main._camera.is_position_in_frustum(cloth.global_position), "looking up brings native canopy into the camera frustum")
	_check(is_equal_approx(cloth.global_position.y, overhead.y), "looking up keeps cloth overhead instead of pitching it with the eye")
	main._camera.global_transform = camera
	native.request_parachute()
	native.advance_frame(native.get_fixed_step_seconds())
	main._ctx = main._read_context()
	main._arms.update_arms(main._arms_state(intent), camera, 0.1)
	_check(not native.is_parachute_deployed() and not main._arms.canopy_visible() and not main._arms.risers_visible(),
		"native retraction hides cloth and risers")
