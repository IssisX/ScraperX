extends SceneTree

const View := preload("res://presentation/forklift_view.gd")
const Arms := preload("res://presentation/first_person_arms.gd")
var failed := false

func check(ok: bool, why: String) -> void:
	if not ok:
		failed = true
		push_error("FORKLIFT_CAB " + why)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var view := View.new()
	root.add_child(view)
	check(view.has_method("get_operator_hand_targets"), "cab supplies actual control hand anchors")
	if failed:
		view.free()
		quit(1)
		return
	var chassis := Transform3D(Basis(Vector3.UP, 0.7), Vector3(8, 2, 3))
	var wheel_pose := Transform3D(Basis(Vector3.RIGHT, 0.2), Vector3(9, 1, 4))
	var state := {"available": true, "steering_mount": Vector3(.1, .78, -1.03),
		"lift_lever_mount": Vector3(-.3, .585, -1.02), "tilt_lever_mount": Vector3(-.41, .585, -1.02),
		"steering_rad": 0.0, "lift_target_mps": 0.0, "tilt_target_rad_s": 0.0, "forward_speed_mps": 0.0,
		"wheels": [{"transform": wheel_pose, "radius_m": .35, "width_m": .2}]}
	view.call("render_state", state, chassis)
	var rest: Dictionary = view.call("get_operator_hand_targets")
	check(view._wheels[0].global_transform.is_equal_approx(wheel_pose), "cab frame never double transforms native wheels")
	check(view._cab.global_transform.is_equal_approx(chassis), "cab follows native chassis frame")
	state.steering_rad = .9
	state.lift_target_mps = .32
	state.tilt_target_rad_s = .1
	view.call("render_state", state, chassis)
	var moved: Dictionary = view.call("get_operator_hand_targets")
	check(rest.left_hand_world.distance_to(moved.left_hand_world) > .15, "native steering turns actual rim hand anchor")
	check(rest.right_hand_world.distance_to(moved.right_hand_world) > .03, "native lift target moves actual lever grip")
	check(view._lift.rotation.x < 0.0 and view._tilt.rotation.x < 0.0, "positive hydraulics pull levers back toward driver")
	state.forward_speed_mps = -1.75
	view.call("render_state", state, chassis)
	check(view._speed_needle.basis.is_equal_approx(Basis(Vector3.RIGHT, -.68)), "actual reverse speed magnitude drives calibrated needle")
	state.lift_target_mps = 0.0
	view.call("render_state", state, chassis)
	var tilt_only: Dictionary = view.call("get_operator_hand_targets")
	check(tilt_only.right_hand_world.distance_to(moved.right_hand_world) > .08, "tilt-only command selects adjacent actual lever")
	state.lift_target_mps = .32
	view.call("render_state", state, chassis)
	var held: Dictionary = view.call("get_operator_hand_targets")
	view.call("render_state", state, chassis)
	check(held == view.call("get_operator_hand_targets"), "identical native state invents no visual motion")
	var arms := Arms.new()
	root.add_child(arms)
	var material := StandardMaterial3D.new()
	arms.build(material, material, material, material, material, material, material, material, material, material)
	var eye := chassis * Vector3(0, 1.28, -1.65)
	var camera := Transform3D(chassis.basis * Basis(Vector3.UP, PI), eye)
	var arm_state := {"traversal": 0, "progress": 0.0, "position": eye, "render_position": eye,
		"velocity": Vector3.ZERO, "grounded": true, "affordance": false, "ledge_point": Vector3.ZERO,
		"operating": &"forklift", "chute": false, "pendant": Vector2.ZERO, "forklift_active": true,
		"forklift_frame": moved.operator_frame, "forklift_left_hand_world": moved.left_hand_world, "forklift_right_hand_world": moved.right_hand_world}
	for i in 60:
		arms.update_arms(arm_state, camera, 1.0 / 60.0)
	check(not arms.remote_visible(), "forklift operation hides crane radio")
	for hand in arms._hands:
		var wanted: Vector3 = moved.left_hand_world if hand.side < 0.0 else moved.right_hand_world
		check(hand.root.global_position.distance_to(wanted) < .025, "rendered finite arm reaches actual control grip")
		check(hand.root.global_transform.is_finite(), "cab IK remains finite")
	# Looking behind changes the view, not the control attachment.
	camera.basis = chassis.basis
	arms.update_arms(arm_state, camera, 1.0 / 60.0)
	check(not arms.remote_visible(), "looking backward never spawns remote")
	for hand in arms._hands:
		var wanted: Vector3 = moved.left_hand_world if hand.side < 0.0 else moved.right_hand_world
		check(hand.root.global_position.distance_to(wanted) < .025, "looking backward preserves actual control attachment")
	arm_state.forklift_active = false
	arm_state.operating = &"crane"
	arms.update_arms(arm_state, camera, 1.0 / 60.0)
	check(arms.remote_visible(), "other machine retains radio")
	view.call("render_state", {"available": false}, chassis)
	check(not view.visible and view.call("get_operator_hand_targets").is_empty(), "unavailable truck supplies no cab grips")
	arms.free()
	view.free()
	print("FORKLIFT_CAB ", "FAIL" if failed else "PASS", " presentation_fixture_only=1")
	quit(1 if failed else 0)
