extends SceneTree

# CI runs the real scene and CharacterBody3D on the moving cage. Water charging
# is advanced deterministically to avoid a wall-clock wait in the proof job.
func _initialize() -> void:
	call_deferred("_run")

func _fail(message: String) -> void:
	push_error("SCRAPERX_ASCENT_FAIL " + message)
	quit(1)

func _run() -> void:
	var scene: PackedScene = load("res://main.tscn")
	if scene == null:
		_fail("main scene missing")
		return
	var game := scene.instantiate()
	root.add_child(game)
	for i in range(8):
		await process_frame
	var stage: Object = game.get("_stage")
	if stage == null:
		_fail("native screw bridge missing")
		return
	var player: CharacterBody3D = game.get_node("Player")
	var world: Node3D = game.get("_world")
	var cage: AnimatableBody3D = world.get("cage")
	if not bool(stage.call("request_motor_toggle", 3.5, 0.0, 0.5)):
		_fail("pump switch refused")
		return
	for i in range(90 * 45):
		stage.call("advance_frame", 1.0 / 90.0)
	if float(stage.call("get_tank_water")) < 1.999:
		_fail("screw did not charge tank")
		return
	stage.call("request_motor_toggle", 3.5, 0.0, 0.5)
	for i in range(90 * 8):
		stage.call("advance_frame", 1.0 / 90.0)
	if not bool(stage.call("request_valve_toggle", 3.5, 0.0, 0.5)):
		_fail("outlet valve refused")
		return
	for i in range(90 * 15):
		stage.call("advance_frame", 1.0 / 90.0)
	if float(stage.call("get_bucket_water")) < 1.999:
		_fail("tank water did not reach bucket")
		return
	player.position = Vector3(4.0, 0.91, -9.0)
	player.velocity = Vector3.ZERO
	for i in range(20):
		await process_frame
	if not player.is_on_floor() or game.call("_floor_collider") != cage:
		_fail("player is not supported by the cage floor")
		return
	if not bool(stage.call("request_release", 4.0, 0.91, -9.0)):
		_fail("cage release refused with rider aboard")
		return
	var caught := false
	for i in range(90 * 15):
		await process_frame
		if bool(stage.call("is_upper_caught")):
			caught = true
			break
	if not caught or player.position.y < 8.65:
		_fail("moving cage did not carry the player to +8 m")
		return
	if not player.is_on_floor() or game.call("_floor_collider") != cage:
		_fail("player lost moving support before handoff")
		return
	game.set("_touch_axis", Vector2(1.0, 0.0))
	for i in range(90):
		await process_frame
	game.set("_touch_axis", Vector2.ZERO)
	if player.position.x < 5.9 or player.position.y < 8.65 or not player.is_on_floor() or \
		game.call("_floor_collider") != world.get_node("UpperLandingCollision"):
		_fail("player could not step onto the fixed upper deck")
		return
	if not bool(game.get("_ascent_finished")):
		_fail("completion requires an actual upper-deck stance")
		return
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture="):
			await RenderingServer.frame_post_draw
			var path := argument.trim_prefix("--capture=")
			var image := root.get_viewport().get_texture().get_image()
			if image.save_png(path) != OK:
				_fail("screenshot write failed")
				return
	print("SCRAPERX_ASCENT_PASS player_y=%.3f deck_x=%.3f tank=%.3f bucket=%.3f" % [
		player.position.y, player.position.x, float(stage.call("get_tank_water")),
		float(stage.call("get_bucket_water"))])
	quit(0)
