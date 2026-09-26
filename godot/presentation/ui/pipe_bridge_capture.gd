extends SceneTree
## Rendering inspection from actual player positions after public native input.
## Viewport event routing is independently exercised by ui_test_driver.gd.
var game: Node
var sim: Object
var failed := false

func _initialize() -> void:
	_run.call_deferred()

func step(seconds: float) -> void:
	for tick in int(round(seconds * 90.0)):
		sim.advance_frame(1.0 / 90.0)

func walk(x: float, z: float) -> void:
	for tick in 1800:
		var at: Vector3 = sim.get_player_position()
		var to := Vector2(x - at.x, z - at.z)
		if to.length() < 0.07:
			sim.set_move_input(0, 0)
			step(0.2)
			return
		var move := to.normalized() * minf(1, to.length() / 0.6)
		sim.set_move_input(move.x, move.y)
		sim.set_facing(to.normalized().x, to.normalized().y)
		step(1.0 / 90.0)
	failed = true
	push_error("BRIDGE_CAPTURE approach failed at %s" % sim.get_player_position())
	sim.set_move_input(0, 0)

func pose(label: String, facing: Vector2, pitch: float = 0.12) -> void:
	game._yaw = atan2(-facing.x, -facing.y)
	game._pitch = pitch
	game._render_snapshot()
	game._ctx = game._read_context()
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	var directory := "user://bridge-captures"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="):
			directory = arg.trim_prefix("--capture-dir=")
	DirAccess.make_dir_recursive_absolute(directory)
	var path := directory.path_join(label + ".png")
	var error := root.get_texture().get_image().save_png(path)
	if error != OK:
		failed = true
	print("BRIDGE_CAPTURE %s position=%s tip=%.4f pipes=%d" % [path,
		sim.get_player_position(), sim.get_pipe_bridge_tip_height(), sim.get_pipe_bridge_retained_pipes()])

func _run() -> void:
	game = load("res://main.tscn").instantiate()
	root.add_child(game)
	game.set_process(false)
	sim = game._native
	step(0.5)
	walk(13, -75)
	walk(12.7, -78.95)
	await pose("rack_ready", Vector2(-1, -0.2), 0.2)
	sim.set_facing(-1, 0)
	step(0.1)
	sim.request_pick_up()
	step(0.15)
	sim.set_move_input(-0.35, 0)
	step(0.6)
	sim.set_move_input(0, 0)
	sim.request_set_down()
	step(10)
	for point in [Vector2(12.7, -75), Vector2(-1.4, -75), Vector2(-1.4, -85.6), Vector2(-0.5, -85.6)]:
		walk(point.x, point.y)
	await pose("loaded_pan", Vector2(1, 0.2), 0.2)
	sim.set_facing(0, 1)
	step(0.1)
	sim.request_pick_up()
	step(0.15)
	sim.set_move_input(-0.35, 0)
	step(0.6)
	sim.set_move_input(0, 0)
	sim.request_set_down()
	step(15)
	await pose("raised_bridge", Vector2(1, -1), 0.25)
	var tip_z := -90.0 - 20.0 * cos(asin(7.4 / 20.0))
	for point in [Vector2(-0.6, -86), Vector2(1.94, -86), Vector2(1.94, -91.1), Vector2(6, -91.1),
			Vector2(6, tip_z + 1.5), Vector2(9.06, tip_z + 1.5), Vector2(9.06, tip_z - 2), Vector2(9.06, -125.2)]:
		walk(point.x, point.y)
	await pose("tower_arrival", Vector2(-0.1, 1), -0.22)
	if sim.get_player_position().y < 11.6 or sim.get_death_count() != 0:
		failed = true
	game._audio.quiesce()
	for frame in 4:
		await process_frame
	print("BRIDGE_CAPTURE %s" % ("FAIL" if failed else "PASS"))
	quit(1 if failed else 0)
