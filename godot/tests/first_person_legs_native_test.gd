extends SceneTree

# Actual Simulation -> bridge -> shipping Main adapter -> drawn sole.
const STEP := 1.0 / 90.0
var failed := false
var checks := 0
var main: Node
var native: RefCounted
var capture_dir := ""

func _initialize() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	_run.call_deferred()

func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok:
		failed = true
		push_error("SCRAPERX_LEGS_NATIVE FAIL " + label)

func present() -> Dictionary:
	main._ctx = main._read_context()
	var body: Dictionary = main._arms_state({"pendant": false, "move": Vector2.ZERO})
	main._camera.position = native.get_player_render_position() + main.EYE_OFFSET
	main._update_legs(body, STEP)
	return body["landing"]

func step() -> Dictionary:
	check(native.advance_frame(STEP) == 1, "one ordinary native fixed step")
	check(native.get_death_count() == 0, "no hidden respawn")
	return present()

func actual_plant(state: Dictionary, label: String) -> void:
	var point: Vector3 = state["foot_push_world_anchor"]
	var normal: Vector3 = state["foot_push_world_normal"]
	check(bool(state["foot_push_active"]) and int(state["foot_push_source_contact_tick"]) > 0,
		label + " is backed by a solved native contact")
	check(normal.is_finite() and absf(normal.length() - 1.0) < 0.0001,
		label + " has an actual unit surface normal")
	check(main._legs.planted_foot_index() >= 0,
		label + " is reachable by the visible leg")
	check(main._legs.planted_foot_position().distance_to(point) < 0.0001,
		label + " drawn sole follows the bridge material point")
	check(main._legs.planted_foot_normal().dot(normal) > 0.9999,
		label + " drawn sole follows the bridge surface normal")

func capture(label: String) -> void:
	if capture_dir.is_empty() or main._legs.planted_foot_index() < 0:
		return
	main._camera.look_at(main._legs.planted_foot_position() + Vector3.UP * 0.1)
	await process_frame
	await RenderingServer.frame_post_draw
	var image := root.get_texture().get_image()
	check(image != null and image.save_png(capture_dir.path_join(label + ".png")) == OK,
		"actual Main viewport capture " + label)

func _run() -> void:
	main = load("res://main.tscn").instantiate()
	root.add_child(main)
	main.set_process(false)
	native = main._native
	check(native != null and native.get_class() == "ScraperXSimulation", "shipping native owner")
	main._yaw = -PI * 0.5
	check(native.debug_restart_at(Vector3(-46, 0.9, -38.45)) and native.set_move_input(0, 0)
		and native.set_facing(1, 0), "one actual yard approach stage")
	for i in 30:
		step()
	check(native.is_player_grounded(), "real approach footing")
	check(native.set_move_input(1, 0) and native.set_sprint_input(true), "ordinary sprint")
	for i in 180:
		step()
		if native.is_player_grounded() and native.get_player_linear_velocity().x > 6.7:
			break
	check(native.get_player_linear_velocity().x > 6.7, "earned approach momentum")
	check(native.set_move_input(0.9682458, 0.25) and native.request_jump(), "ordinary wall approach Jump")
	var first := false
	var second := false
	var unloaded := false
	var first_start := 0
	for i in 180:
		var state := step()
		if bool(state["foot_push_active"]) and float(state["foot_push_last_load_n"]) > 0.0:
			var start := int(state["foot_push_start_count"])
			if not first:
				first = true
				first_start = start
				actual_plant(state, "first wall step")
				check((state["foot_push_world_normal"] as Vector3).z < -0.9,
					"wall sole faces the actual wall")
				await capture("01-wall-step")
			elif start > first_start:
				actual_plant(state, "second wall step")
				check(unloaded, "second plant follows a real unloaded interval")
				second = true
				break
		elif first:
			unloaded = true
			check(main._legs.planted_foot_index() == -1, "unloaded handoff releases the visual plant")
	check(first and second, "two bounded actual wall contacts reach shipping legs")
	check(native.request_release(), "deliberate wall release")
	step()
	check(main._legs.planted_foot_index() == -1, "released native row cannot leave a planted boot")

	check(native.debug_restart_at(Vector3(26.68, 87.92, -132)) and native.set_move_input(0, 0)
		and native.set_sprint_input(false) and native.set_facing(-1, 0), "single Tower88 approach stage")
	main._yaw = PI * 0.5
	for i in 45:
		step()
	check(native.request_traversal(), "ordinary top-out Action")
	var topout_foot := false
	for i in 153:
		var state := step()
		if bool(state["foot_push_active"]):
			actual_plant(state, "top-out receiving foot")
			check(int(state["foot_push_support_entity_id"]) == 11,
				"top-out boot uses the actual receiving tower")
			await capture("02-topout-step")
			topout_foot = true
			break
	check(topout_foot, "bounded real top-out presents a receiving foot")
	for i in 90:
		step()
	check(native.is_player_grounded() and main._legs.planted_foot_index() == -1,
		"stable actual exit returns to ordinary unplanted legs")
	print("SCRAPERX_LEGS_NATIVE %s checks=%d" % ["FAIL" if failed else "PASS", checks])
	main.queue_free()
	await process_frame
	quit(1 if failed else 0)
