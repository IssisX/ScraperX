extends RefCounted

# Suspended ladder and cargo-net traversal through real input.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const CrownSuite := preload("res://presentation/ui/test_scenarios/crown_scenarios.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

var h: Harness
var _crown_suite: CrownSuite


func _init(harness: Harness, crown: CrownSuite) -> void:
	h = harness
	_crown_suite = crown


func _swing_catch_sample(body: int, previous: Dictionary) -> void:
	var tick := int(h._native().get_tick_index())
	if tick == int(previous["tick"]):
		return
	var pose: Transform3D = h._native().get_kit_body_transform(body)
	var angle := pose.basis.get_euler().z
	# Track the same real rung midpoint, not a changing nearest point.
	var midpoint := pose * Vector3(0, -7.82, 0)
	var sample_seconds := float(tick - int(previous["tick"])) * float(h._native().get_fixed_step_seconds())
	var point_velocity := Vector3.ZERO
	var angular_z := 0.0
	if int(previous["tick"]) >= 0 and sample_seconds > 0.0:
		point_velocity = (midpoint - (previous["midpoint"] as Vector3)) / sample_seconds
		angular_z = angle_difference(float(previous["angle"]), angle) / sample_seconds
	var facing := Vector3(-sin(float(h._main._yaw)), 0, -cos(float(h._main._yaw)))
	var hand_aim := h._position() + Vector3(0, 0.45, 0) + facing * 0.4
	var aim_local := pose.affine_inverse() * hand_aim
	var nearest := Vector3.ZERO
	var distance := INF
	for rung in 24:
		var point := pose * Vector3(clampf(aim_local.x, -0.85, 0.85), -8.9 + 0.36 * rung, 0)
		if point.distance_to(hand_aim) < distance:
			nearest = point
			distance = point.distance_to(hand_aim)
	print("SCRAPERX_SWING_CATCH tick=%d player=%s player_velocity=%s grounded=%s crouched=%s traversal=%d view_yaw_rad=%.6f grip=%s grip_point=%s angle_z=%.6f origin=%s body_velocity=%s observed_rung_midpoint_velocity=%s observed_angular_z=%.6f nearest_rung=%s hand_distance=%.6f faced_depth=%.6f" % [
		tick, h._position(), h._velocity(), h._native().is_player_grounded(), h._native().is_player_crouched(),
		h._native().get_traversal_state(), h._main._yaw, h._native().is_grip_available(), h._native().get_grip_point(),
		angle, pose.origin, h._native().get_kit_body_linear_velocity(body), point_velocity, angular_z, nearest,
		distance, (nearest - h._position()).dot(facing)])
	previous["tick"] = tick
	previous["midpoint"] = midpoint
	previous["angle"] = angle


func _touch_suspended_ladder(stage_at_110: bool = true) -> bool:
	var device := InputRouter.Device.TOUCH
	# Explicit normal-world supported staging; not full campaign ascent proof.
	if stage_at_110:
		if not h._native().debug_restart_at(Vector3(10, 110.9, -174)):
			return h._fail("+110m supported staging rejected")
	else:
		# Walk the real +110m ring from the north-frame exit to the AS-026
		# approach; retain the airborne catch and loaded moving climb below.
		if not await h._walk_to(device, Vector2(10, -174.5), 0.12, 5.0):
			return h._fail("north frame to suspended ladder ring transfer %s" % h._position())
	await h._seconds(0.5)
	# Brake through ordinary touch input before taking the fixed rungs. A
	# distance-only arrival coasted to z=-179.86; that acquired stance stalled
	# below the shelf at y=112.722 even with continued upward input.
	if not await h._walk_to(device, Vector2(10, -179.60), 0.12, 5.0, true):
		return h._fail("fixed ladder approach %s" % h._position())
	await h._face(Vector2(0, -1))
	await h._pose("swing_approach")
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("fixed ladder grip unavailable")
	h._act(device)
	await h._seconds(0.2)
	h._move(device, 1.0)
	var reached := await h._wait_until(func() -> bool:
		return h._standing_above(114.7) and int(h._native().get_support_entity_id()) == 1960, 8.0)
	h._move(device, 0.0)
	if not reached:
		return h._fail("launch shelf not reached %s" % h._position())
	# Stand near the shelf's front edge before the north-facing side jump.
	# The old loose stop left z=-180.31: the descending capsule crossed the
	# rung plane before its x coordinate entered the unchanged hand query.
	if not await h._walk_to(device, Vector2(9.15, -180.10), 0.04, 4.0):
		return h._fail("launch edge not reached")
	if not h._standing_above(114.7) or int(h._native().get_support_entity_id()) != 1960:
		return h._fail("launch edge lost supported footing %s" % h._position())
	await h._face(Vector2(-1, -0.25))
	h._main._pitch = -0.55
	await h._pose("swing_launch_view")
	# Catch from the front. Native acquisition retains this normal for hand
	# loading and jump-off; turning the camera later cannot change that stance.
	await h._face(Vector2(0, -1))
	h._main._pitch = 0.0
	var body := int(h._native().get_kit_body_index(2960))
	var catch_sample := {"tick": -1, "midpoint": Vector3.ZERO, "angle": 0.0}
	_swing_catch_sample(body, catch_sample)
	h._move_dir(device, Vector2(-1, 0.35))
	h._tap(1, h._center(&"jump"))
	var caught := await h._wait_until(func() -> bool:
		_swing_catch_sample(body, catch_sample)
		return int(h._native().get_traversal_support_entity_id()) == 2960, 1.5)
	h._move(device, 0.0)
	if not caught:
		return h._fail("touch jump missed moving grip %s" % h._position())
	await h._seconds(0.5)
	await h._pose("swing_caught")
	var first_angle := h._teeter_angle(body)
	var swing_span := [0.0]
	h._move(device, 1.0)
	reached = await h._wait_until(func() -> bool:
		swing_span[0] = maxf(swing_span[0], absf(h._teeter_angle(body) - first_angle))
		return h._position().y > 119.8, 8.0)
	h._move(device, 0.0)
	if not reached or swing_span[0] < 0.002:
		return h._fail("loaded swing/climb not demonstrated reached=%s span=%.4f at=%s" % [reached, swing_span[0], h._position()])
	var phase := {"min_vx": INF, "max_vx": -INF, "min_angle": INF, "max_angle": -INF, "tick": -1}
	print("SCRAPERX_SWING_RELEASE_START player=%s normal=%s loaded_span=%.6f" % [h._position(), h._native().get_traversal_normal(), swing_span[0]])
	var rightward := await h._wait_until(func() -> bool:
		var velocity: Vector3 = h._native().get_kit_body_linear_velocity(body)
		var angle := h._teeter_angle(body)
		phase["min_vx"] = minf(phase["min_vx"], velocity.x)
		phase["max_vx"] = maxf(phase["max_vx"], velocity.x)
		phase["min_angle"] = minf(phase["min_angle"], angle)
		phase["max_angle"] = maxf(phase["max_angle"], angle)
		if int(h._native().get_tick_index()) - int(phase["tick"]) >= 45:
			phase["tick"] = int(h._native().get_tick_index())
			print("SCRAPERX_SWING_RELEASE_PHASE tick=%d player=%s velocity=%s angle=%.6f normal=%s support=%d" % [phase["tick"], h._position(), velocity, angle, h._native().get_traversal_normal(), h._native().get_traversal_support_entity_id()])
		return velocity.x > 0.12, 6.7)
	if not rightward:
		return h._fail("rightward release phase unavailable phase=%s at=%s normal=%s" % [phase, h._position(), h._native().get_traversal_normal()])
	await h._pose("swing_release_height")
	h._move_dir(device, Vector2(0.7, -0.35))
	h._tap(1, h._center(&"jump"))
	reached = await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 1.7)
	h._move(device, 0.0)
	await h._seconds(0.3)
	if not reached or not h._standing_above(119.7) or int(h._native().get_support_entity_id()) != 1960:
		return h._fail("swing release missed offset receiver reached=%s at=%s" % [reached, h._position()])
	await h._face(Vector2(0, 1))
	await h._pose("swing_landing")
	if not await h._walk_to(device, Vector2(9, -178.45), 0.12, 4.0):
		return h._fail("receiver exit climb approach failed")
	await h._face(Vector2(0, 1))
	if not await h._offered(&"climb", "CLIMB", "HOLD"):
		return h._fail("upper climb unavailable %s" % h._position())
	h._act(device)
	await h._seconds(0.2)
	h._move(device, 1.0)
	reached = await h._wait_until(func() -> bool: return h._standing_above(121.7), 5.0)
	h._move(device, 0.0)
	if not reached or not await h._walk_to(device, Vector2(9, -174.5), 0.12, 4.0):
		return h._fail("+121m tower exit failed %s" % h._position())
	await h._seconds(0.3)
	await h._pose("swing_tower_exit")
	if int(h._native().get_support_entity_id()) != 11 or int(h._native().get_death_count()) != 0:
		return h._fail("+121m exit is unsupported or deaths occurred")
	if h._main._audio._bank_ready and not h._main._audio._silent and not h._main._audio._ladder_strain.playing:
		return h._fail("native-driven strain voice is not playing")
	if h._main._audio.ladder_motion_frames == 0 or h._main._audio.climb_regrips == 0:
		return h._fail("state-driven swing feedback absent")
	h._detail = "staging=supported_110m swing_jump=1 supported_height_m=121 support=11 deaths=0 native_feedback=1"
	return true


func _touch_cargo_net() -> bool:
	var device := InputRouter.Device.TOUCH
	await h._seconds(0.5)
	# Start at the ordinary grade spawn. No fixture, relocation, restart,
	# launch or scripted ascent: all movement enters through viewport touch.
	if not await h._walk_to(device, Vector2(20.0, -114.0), 0.12, 35.0):
		return h._fail("cargo net grade approach %s" % h._position())
	await h._face(Vector2(0, -1))
	h._main._pitch = 0.35
	await h._pose("cargo_net_entry")
	h._main._pitch = 0.0
	# Stop at the actual grounded CLIMB offer. Driving to a fixed rest-mesh
	# coordinate can pass the offer and legitimately auto-catch while airborne
	# on the finite tread/net; that does not exercise the requested Action.
	h._move(device, 0.25)
	var offered := await h._wait_until(func() -> bool: return bool(h._native().is_player_grounded()) \
		and bool(h._native().is_grip_available()), 6.0)
	h._move(device, 0.0)
	if not offered or bool(h._ctx()["climbing"]):
		return h._fail("cargo net grounded grip approach %s" % h._position())
	await h._face(Vector2(0, -1))
	if not await h._offered(&"climb", "CLIMB"):
		return h._fail("cargo net CLIMB not offered %s" % h._position())
	h._act(device)
	if not await h._wait_until(func() -> bool: return bool(h._ctx()["climbing"]), 0.8):
		return h._fail("cargo net touch Action did not take hold")
	var still_y := h._position().y
	await h._seconds(0.6)
	if h._position().y - still_y > 0.06:
		return h._fail("cargo net advanced without held input")
	var grip_sample: Dictionary = h._native().get_landing_state()
	var feedback_watch := {"left": int(grip_sample.get("left_hand_generation", 0)),
		"right": int(grip_sample.get("right_hand_generation", 0)),
		"cues": int(h._main._audio.get_grip_feedback_stats()["climb_regrips"]),
		"changes": 0, "actual_cues": 0, "right_cues": 0, "invalid": false}
	h._move(device, 1.0)
	if not await h._wait_until(func() -> bool:
		var state: Dictionary = h._native().get_landing_state()
		var stats: Dictionary = h._main._audio.get_grip_feedback_stats()
		var left_generation := int(state.get("left_hand_generation", 0))
		var right_generation := int(state.get("right_hand_generation", 0))
		var left_changed := bool(state.get("left_hand_attached", false)) and left_generation > 0 \
			and left_generation != int(feedback_watch["left"])
		var right_changed := bool(state.get("right_hand_attached", false)) and right_generation > 0 \
			and right_generation != int(feedback_watch["right"])
		if left_changed or right_changed:
			feedback_watch["changes"] = int(feedback_watch["changes"]) + 1
		var new_cues := int(stats["climb_regrips"]) - int(feedback_watch["cues"])
		if new_cues > 0:
			var at: Vector3 = stats["last_climb_regrip_position"]
			var actual: Vector3 = h._native().get_traversal_left_hand() if left_changed else h._native().get_traversal_right_hand()
			feedback_watch["invalid"] = bool(feedback_watch["invalid"]) or new_cues != 1 \
				or not (left_changed or right_changed) or at.distance_to(actual) > 0.05
			feedback_watch["actual_cues"] = int(feedback_watch["actual_cues"]) + new_cues
			if right_changed and not left_changed:
				feedback_watch["right_cues"] = int(feedback_watch["right_cues"]) + new_cues
		feedback_watch["left"] = left_generation
		feedback_watch["right"] = right_generation
		feedback_watch["cues"] = int(stats["climb_regrips"])
		return h._position().y > 5.5, 8.0):
		h._move(device, 0.0)
		return h._fail("cargo net lower climb blocked %s" % h._position())
	h._move(device, 0.0)
	if bool(feedback_watch["invalid"]) or int(feedback_watch["changes"]) == 0 \
			or int(feedback_watch["actual_cues"]) == 0:
		return h._fail("cargo net feedback lacks actual generation-owned contact cues: %s" % feedback_watch)
	print("SCRAPERX_GRIP_FEEDBACK material_net active_generation_changes=%d actual_cues=%d right_only_change_cues=%d" % [
		feedback_watch["changes"], feedback_watch["actual_cues"], feedback_watch["right_cues"]])
	await h._seconds(0.3)
	await h._pose("cargo_net_climbing")
	var posture_before: Dictionary = h._native().get_landing_state()
	if int(h._native().get_traversal_support_entity_id()) != 1953 \
			or int(posture_before["traversal_hand_constraint_count"]) != 2 \
			or bool(h._native().is_player_grounded()):
		return h._fail("cargo net shared posture requires actual airborne two-hand net hold")
	if not await _crown_suite._touch_hanging_posture():
		return false
	var posture_after: Dictionary = h._native().get_landing_state()
	if int(h._native().get_traversal_support_entity_id()) != 1953 \
			or int(posture_after["traversal_hand_constraint_count"]) != 2 \
			or bool(h._native().is_player_grounded()):
		return h._fail("cargo net shared posture lost actual airborne two-hand net hold")
	print("SCRAPERX_HANGING_POSTURE material_net/sharedtouch_tuck_extend=1 support=1953 hands=2 airborne=1")
	h._move(device, 1.0)
	# Allow compliant grip/receiver settling after the lift. This remains a
	# bounded test deadline; only actual supported contact proves arrival.
	var arrived := await h._wait_until(func() -> bool: return h._standing_above(11.7), 12.0)
	h._move(device, 0.0)
	if not arrived:
		return h._fail("cargo net top-out blocked %s" % h._position())
	await h._seconds(0.4)
	await h._pose("cargo_net_receiver")
	for point in [Vector2(18.5, -120.5), Vector2(18.5, -126.0)]:
		if not await h._walk_to(device, point, 0.12, 5.0):
			return h._fail("cargo net receiver to first ring %s" % h._position())
	await h._seconds(0.4)
	if not h._standing_above(11.7) or absf(h._position().y - 11.9) > 0.15 \
			or int(h._native().get_support_entity_id()) != 11 or int(h._native().get_death_count()) != 0:
		return h._fail("cargo net first tower ring unsupported %s" % h._position())
	if float(h._native().get_slingshot_state()["work_j"]) != 0.0:
		return h._fail("cargo net route used launcher work")
	await h._pose("cargo_net_first_ring")
	h._detail = "staging=ordinary_grade_spawn supported_height_m=11.00 deaths=0 slingshot_used=0 shared_material_posture=1"
	return true
