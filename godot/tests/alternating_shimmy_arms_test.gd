extends SceneTree

const Arms := preload("res://presentation/first_person_arms.gd")
var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SHIMMY_ARMS FAIL " + why)

func shoulder(hand) -> Vector3:
	# The actual rendered upper-arm segment starts at the solved shoulder.
	return hand.upper.global_position - hand.upper.global_basis.y * Arms.UPPER_ARM * 0.5

func _run() -> void:
	var arms := Arms.new()
	root.add_child(arms)
	var material := StandardMaterial3D.new()
	arms.build(material, material, material, material, material, material, material, material, material, material)
	var state := {"traversal": 1, "progress": 0.0, "position": Vector3.ZERO,
		"render_position": Vector3.ZERO, "velocity": Vector3.ZERO, "grounded": false, "affordance": false,
		"ledge_point": Vector3(0, 0.8, -0.25), "structure_normal": Vector3.FORWARD,
		"operating": &"", "chute": false, "hand_left": Vector3(0.22, 0.8, -0.25),
		"hand_right": Vector3(-0.22, 0.8, -0.25), "landing": {"traversal_hand_constraint_count": 2}}
	var camera := Transform3D(Basis.IDENTITY, Vector3(0, 0.65, 0.3))
	for i in 60:
		arms.update_arms(state, camera, 1.0 / 90.0)
	var before := []
	for hand in arms._hands:
		before.append(shoulder(hand))
	# One real hand placement can cross the other grip's coordinate. It
	# changes wrist targets; it must not exchange the body's shoulders.
	state.hand_left = Vector3(-0.35, 0.8, -0.25)
	var unchanged: Vector3 = state.hand_right
	var frozen := state.duplicate(true)
	arms.update_arms(state, camera, 1.0 / 90.0)
	for i in arms._hands.size():
		check(shoulder(arms._hands[i]).distance_to(before[i]) < 0.001, "crossing grip preserves rendered shoulder side")
	check(state == frozen and state.hand_right == unchanged, "presentation preserves supplied anchor state")
	for i in 60:
		arms.update_arms(state, camera, 1.0 / 90.0)
	check(arms.anchored_error() < 0.012, "wrists settle onto supplied independent anchors")
	# Camera yaw does not turn a hanging torso or exchange its arms.
	camera.basis = Basis(Vector3.UP, 1.1)
	arms.update_arms(state, camera, 1.0 / 90.0)
	for i in arms._hands.size():
		check(shoulder(arms._hands[i]).distance_to(before[i]) < 0.001, "head turn preserves hanging shoulder side")
	# A new hang may face a different ledge; its first real pair establishes
	# a fresh orientation instead of inheriting the previous hang's axis.
	state.traversal = 0
	arms.update_arms(state, camera, 1.0 / 90.0)
	state.traversal = 1
	state.hand_left = Vector3(-0.22, 0.8, -0.25)
	state.hand_right = Vector3(0.22, 0.8, -0.25)
	arms.update_arms(state, camera, 1.0 / 90.0)
	check(shoulder(arms._hands[0]).x > shoulder(arms._hands[1]).x, "new hang establishes its actual pair orientation")
	print("SHIMMY_ARMS checks=", checks, " failures=", failures, " fixture_presentation_only=1")
	arms.free()
	quit(0 if failures == 0 else 1)
