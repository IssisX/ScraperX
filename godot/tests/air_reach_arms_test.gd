extends SceneTree

# Supplied-state presentation checks. Native acquisition/cancellation is
# exercised separately through shipping Main in air_catch_input_test.gd.
const Arms := preload("res://presentation/first_person_arms.gd")
const STEP := 1.0 / 90.0
var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SCRAPERX_AIR_REACH_ARMS FAIL " + why)

func _run() -> void:
	var arms := Arms.new()
	root.add_child(arms)
	var material := StandardMaterial3D.new()
	arms.build(material, material, material, material, material, material, material, material, material, material)
	var origin := Vector3(40, 50, 60)
	var camera := Transform3D(Basis.IDENTITY, origin + Vector3(0, 0.65, 0.3))
	var state := {"traversal": 0, "progress": 0.0, "position": origin,
		"render_position": origin, "velocity": Vector3(0, -4, 0), "grounded": false,
		"affordance": false, "affordance_point": Vector3.ZERO,
		"ledge_point": Vector3.ZERO, "structure_normal": Vector3.FORWARD,
		"operating": &"", "chute": false, "hand_left": Vector3.ZERO,
		"hand_right": Vector3.ZERO, "landing": {"traversal_hand_constraint_count": 0,
			"air_catch_intent_pending": false}}
	for i in 60:
		arms.update_arms(state, camera, STEP)
	check(arms.hand_poses().all(func(pose: int) -> bool: return pose == Arms.POSE_AIR),
		"ordinary empty flight retains its existing balance pose")
	var rest: Array = arms.wrist_positions()
	state.landing.air_catch_intent_pending = true
	var frozen := state.duplicate(true)
	for frame in 8:
		var before: Array = arms.wrist_positions()
		arms.update_arms(state, camera, STEP)
		for i in arms._hands.size():
			var hand = arms._hands[i]
			check(hand.pose == Arms.POSE_REACH and not hand.anchored and not hand.planted,
				"pending reach has open motion without a planted hand")
			check(hand.world_wrist.is_finite() and hand.world_wrist.distance_to(before[i]) <= Arms.REACH_SPEED * STEP + 0.00001,
				"reach moves within the existing wrist speed limit")
			check((camera.affine_inverse() * hand.world_wrist).length() < 1.0,
				"absent affordance cannot send a wrist toward the unrelated world origin")
	check(state == frozen, "reach presentation never changes supplied input or contact state")
	for i in arms._hands.size():
		var hand = arms._hands[i]
		check(hand.world_wrist.y > (rest[i] as Vector3).y + 0.07 and
			hand.curl < 0.30 and hand.thumb_curl < 0.25,
			"short native window visibly raises the open palm")
	check(arms.anchored_error() == -1, "empty reach creates no visual anchor")
	state.landing.air_catch_intent_pending = false
	arms.update_arms(state, camera, STEP)
	check(arms.hand_poses().all(func(pose: int) -> bool: return pose == Arms.POSE_AIR),
		"cancel or expiry immediately selects the ordinary air pose")
	for i in 30:
		arms.update_arms(state, camera, STEP)
	for i in arms._hands.size():
		# Existing air sway spans24mm peak-to-peak; the remaining allowance
		# covers the bounded blend back, rather than requiring a frozen pose.
		check(arms._hands[i].world_wrist.distance_to(rest[i]) < 0.04,
			"cancelled reach blends back without leaving a raised palm")
	state.traversal = Arms.TRAVERSAL_HANGING
	state.landing.traversal_hand_constraint_count = 2
	state.landing.air_catch_intent_pending = true
	state.hand_left = origin + Vector3(0.22, 0.8, -0.25)
	state.hand_right = origin + Vector3(-0.22, 0.8, -0.25)
	state.ledge_point = origin + Vector3(0, 0.8, -0.25)
	for i in 60:
		arms.update_arms(state, camera, STEP)
	check(arms.hand_poses().all(func(pose: int) -> bool: return pose == Arms.POSE_GRIP),
		"actual hand topology takes priority over a stale intent sample")
	check(arms.anchored_error() >= 0 and arms.anchored_error() < 0.012,
		"existing planted wrists settle onto their supplied real-grip targets")
	print("SCRAPERX_AIR_REACH_ARMS %s checks=%d supplied_state_only=1" % ["PASS" if failures == 0 else "FAIL", checks])
	arms.free()
	quit(0 if failures == 0 else 1)
