extends "res://tests/grapple_toolbelt_input_case.gd"

# Normal arrival and real touch/router/native path; no player/contact staging.
func step(count: int = 1) -> void:
	for tick in count:
		main._process(STEP)

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	step(30)
	check(main._native.is_player_grounded(), "normal arrival has solved footing")
	check(main._legs.visible and main._legs._body.ready_to_pose(), "shipping view shows a rigged body")
	check(main._legs._body._meshes.size() == 8, "shipping body retains its bounded surfaces")
	var start: Vector3 = main._native.get_player_position()
	stick(Vector2(0.4,0.0))
	step(36)
	stick(Vector2.ZERO)
	step(8)
	check(main._native.get_player_position().distance_to(start) > 0.15, "touch stick moves the actual player")
	check(main._legs.visible, "body follows ordinary touch walking")
	var standing_hip: Vector3 = main._legs._body.pelvis_world()
	tap(&"crouch")
	step(30)
	check(bool(main._ctx.crouched), "touch crouch reaches native posture")
	check(main._legs._body.pelvis_world().y < standing_hip.y - 0.15, "workwear lowers with native crouch and camera blend")
	tap(&"crouch")
	step(30)
	check(not bool(main._ctx.crouched), "touch stand restores native posture")
	tap(&"jump")
	var airborne := false
	# Wait for actual landing within two native seconds, rather than ending
	# the check before the existing jump has finished its ballistic arc.
	for tick in 180:
		step()
		if not main._native.is_player_grounded():
			airborne = true
			check(main._legs.visible, "airborne body remains visible")
			if not bool(main._native.get_landing_state().get("foot_push_active",false)):
				check(main._legs.planted_foot_index() < 0, "unloaded airborne body carries no invented foot contact")
		elif airborne:
			break
	check(airborne and main._native.is_player_grounded(), "touch jump returns to actual ground")
	select_tool(&"grapple")
	check(main._ctx.operating == &"grappling_gun" and main._legs.visible,
		"touch-equipped portable grapple keeps the body visible")
	var hip: Vector3 = main._legs._body.joint_world(0,"hp")
	var knee: Vector3 = main._legs._body.joint_world(0,"kn")
	check(hip.is_finite() and knee.is_finite() and hip.y > knee.y, "real posed skeleton remains finite after movement and equipment")
	check(main._native.get_death_count() == 0, "ordinary touch body sequence uses no respawn")
	return {"checks": checks, "failures": failures, "actual_native": true, "touch": true, "done": true}
