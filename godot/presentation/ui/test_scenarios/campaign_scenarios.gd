extends RefCounted

# Connected campaign ascent and service, balance, tram, helix, mast and pitman machines.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const CrownSuite := preload("res://presentation/ui/test_scenarios/crown_scenarios.gd")
const FramesSuite := preload("res://presentation/ui/test_scenarios/frames_scenarios.gd")
const LadderSuite := preload("res://presentation/ui/test_scenarios/ladder_scenarios.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")

var h: Harness
var _crown_suite: CrownSuite
var _frames_suite: FramesSuite
var _ladder_suite: LadderSuite


func _init(harness: Harness, crown: CrownSuite, frames: FramesSuite, ladder: LadderSuite) -> void:
	h = harness
	_crown_suite = crown
	_frames_suite = frames
	_ladder_suite = ladder


func _touch_campaign_to_121() -> bool:
	if not h._campaign_world_intact():
		return false
	# Existing upper-route helpers use the cargo-net opener only for this
	# scenario. No fixture selection, restart or player relocation occurs.
	if not await _frames_suite._touch_north_service_frame():
		return false
	if not h._campaign_world_intact():
		return false
	print("SCRAPERX_CAMPAIGN north_frame_exit at=%s support=%d" % [h._position(), h._native().get_support_entity_id()])
	if not await _ladder_suite._touch_suspended_ladder(false):
		return false
	if not h._campaign_world_intact():
		return false
	if not h._standing_above(121.7) \
			or absf(h._position().y - 121.9) > 0.15 or int(h._native().get_support_entity_id()) != 11:
		return h._fail("campaign +121m arrival unsupported at=%s" % h._position())
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 supported_height_m=121 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_campaign_to_143() -> bool:
	if not await _touch_campaign_to_121():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=121 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_service_lift(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 supported_height_m=143 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_service_lift(stage_at_121: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_121:
		if not h._native().debug_restart_at(Vector3(-25, 121.9, -162)):
			return h._fail("service lift lower-ring staging rejected")
		await h._seconds(0.5)
	else:
		for point in [Vector2(-22, -174.5), Vector2(-22, -162), Vector2(-25, -162)]:
			if not await h._walk_to(device, point, 0.12, 10.0):
				return h._fail("121m ring approach to service lift failed at=%s" % h._position())
	if not await h._walk_to(device, Vector2(-27.4, -162), 0.12, 6.0):
		return h._fail("lower landing approach failed")
	if int(h._native().get_service_lift_state()["station"]) != 2:
		return h._fail("lower call station is not reachable")
	if not await h._walk_to(device, Vector2(-30.8, -162), 0.12, 6.0):
		return h._fail("physical lift boarding failed")
	await h._face(Vector2(1.1, -0.75))
	if int(h._native().get_support_entity_id()) != 2973 or not await h._offered(&"operate", "OPERATE", "SERVICE LIFT"):
		return h._fail("deck station does not offer OPERATE at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"service_lift" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("service lift pendant did not open")
	var before: Dictionary = h._native().get_service_lift_state()
	h._touch(1, h._center(&"pendant_up"), true)
	await h._seconds(3.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var paused: Dictionary = h._native().get_service_lift_state()
	if float(paused["surface_y"]) < float(before["surface_y"]) + 0.5 or not bool(paused["braking"]):
		return h._fail("touch hold/release did not drive and brake native lift")
	var stopped_height := float(paused["surface_y"])
	await h._seconds(0.5)
	if absf(float(h._native().get_service_lift_state()["surface_y"]) - stopped_height) > 0.02:
		return h._fail("released lift does not hold its height")
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_service_lift_state()["surface_y"]) >= 142.98, 65.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	if not reached or not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2973:
		return h._fail("touch ride failed before143m receiver at=%s station=%s" % [h._position(), h._native().get_service_lift_state()])
	var arrival: Dictionary = h._native().get_service_lift_state()
	if not bool(arrival["braking"]) or float(arrival["energy_j"]) >= float(before["energy_j"]) or float(arrival["energy_j"]) <= 0:
		return h._fail("lift arrival is not a finite-energy braked state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("DONE did not release lift operation")
	if not await h._walk_to(device, Vector2(-24.7, -162), 0.12, 8.0):
		return h._fail("physical143m exit failed at=%s" % h._position())
	if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 11 or absf(h._position().y - 143.9) > 0.15:
		return h._fail("143m arrival is not supported by Tower11")
	h._detail = "staging=supported_121m shipping_touch=1 hold_release=1 finite_energy=1 supported_height_m=143 support=11 deaths=%d" % int(h._native().get_death_count())
	return true


func _touch_campaign_to_165() -> bool:
	if not await _touch_campaign_to_143():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=143 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_balance_lift(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 hold_release=1 supported_height_m=165 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_balance_lift(stage_at_143: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	# Only the focused slice uses explicit supported staging. From here onward
	# both scenarios walk, jump and operate through ordinary viewport touch.
	if stage_at_143:
		if not h._native().debug_restart_at(Vector3(-24.7, 143.9, -162)):
			return h._fail("gravity balance +143m staging rejected")
		await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(143.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("gravity balance approach requires supported +143m Tower11")
	for point in [Vector2(-24.7, -142), Vector2(-27.5, -142.65)]:
		if not await h._walk_to(device, point, 0.12, 10.0):
			return h._fail("gravity balance lower landing approach at=%s" % h._position())
	if int(h._native().get_supplied_machine_state()["station"]) != 2:
		return h._fail("gravity balance lower call station is not reachable")
	# The landing and moving deck have a real 0.3m gap. Jump from the
	# supported landing; the native body/contact owner decides the transfer.
	if not await h._walk_to(device, Vector2(-28.45, -142.6), 0.08, 5.0):
		return h._fail("gravity balance boarding takeoff at=%s" % h._position())
	await h._face(Vector2(-1, 0))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(-32.8, -142.5), 0.10, 8.0):
		return h._fail("gravity balance physical boarding at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 2980, 2.0):
		return h._fail("gravity balance boarding is not supported by deck2980")
	await h._face(Vector2(0, -1))
	if int(h._native().get_supplied_machine_state()["station"]) != 1 \
			or not await h._offered(&"operate", "OPERATE", "GRAVITY BALANCE"):
		return h._fail("gravity balance deck control unavailable at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("gravity balance touch pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), true)
	var moved := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= float(before["surface_y"]) + 0.8, 12.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var stopped: Dictionary = h._native().get_supplied_machine_state()
	if not moved or not bool(stopped["braking"]) or int(h._native().get_support_entity_id()) != 2980:
		return h._fail("gravity balance UP/release did not move and brake the ridden deck")
	var stopped_height := float(stopped["surface_y"])
	await h._seconds(0.5)
	if absf(float(h._native().get_supplied_machine_state()["surface_y"]) - stopped_height) > 0.02:
		return h._fail("gravity balance released brake does not hold height")
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 164.98, 65.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not reached or not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2980:
		return h._fail("gravity balance touch ride failed at=%s station=%s" % [h._position(), arrival])
	if not bool(arrival["braking"]) or float(arrival["energy_j"]) < 0.0 \
			or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) > float(before["energy_j"]):
		return h._fail("gravity balance arrival violates finite energy/brake state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity balance DONE did not release operation")
	# Jump the same native gap onto the upper receiver, then walk into Tower11.
	if not await h._walk_to(device, Vector2(-29.8, -142.65), 0.08, 7.0):
		return h._fail("gravity balance upper exit takeoff at=%s" % h._position())
	await h._face(Vector2(1, 0))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(-27.5, -142.65), 0.10, 7.0):
		return h._fail("gravity balance upper receiver transfer at=%s" % h._position())
	if stage_at_143:
		if not await _touch_balance_upper_recall():
			return false
	if not await h._walk_to(device, Vector2(-24.7, -142), 0.12, 7.0):
		return h._fail("gravity balance +165m Tower exit at=%s" % h._position())
	if not h._standing_above(165.7) or absf(h._position().y - 165.9) > 0.15 \
			or int(h._native().get_support_entity_id()) != 11 or not h._campaign_world_intact():
		return h._fail("gravity balance +165m arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=%s shipping_touch=1 gravity_balance=1 hold_release=1 finite_energy=1 upper_recall=%d supported_height_m=165 support=11 deaths=0" % [
		"supported_143m" if stage_at_143 else "ordinary_grade_spawn", int(stage_at_143)]
	return true


func _touch_balance_upper_recall() -> bool:
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_supplied_machine_state()["station"]) == 3, 2.0) \
			or not await h._offered(&"operate", "OPERATE", "GRAVITY BALANCE"):
		return h._fail("gravity balance upper call station unavailable at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_down"):
		return h._fail("gravity balance upper call pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_down"), true)
	var lowered := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) <= 163.8, 12.0)
	h._touch(1, h._center(&"pendant_down"), false)
	await h._seconds(0.5)
	if not lowered or not bool(h._native().get_supplied_machine_state()["braking"]) \
			or int(h._native().get_support_entity_id()) not in [11, 1982] \
			or not bool(h._ctx()["grounded"]):
		return h._fail("gravity balance upper station return did not brake from fixed footing")
	h._touch(1, h._center(&"pendant_up"), true)
	var recalled := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 164.98, 12.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var after: Dictionary = h._native().get_supplied_machine_state()
	if not recalled or not bool(after["braking"]) or float(after["energy_j"]) < 0.0 \
			or float(after["energy_j"]) >= float(before["energy_j"]):
		return h._fail("gravity balance upper recall did not spend finite reset energy")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity balance upper call DONE did not release operation")
	return true


func _touch_campaign_to_198() -> bool:
	if not await _touch_campaign_to_165():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=165 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _crown_suite._touch_crown_gondola(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 supported_height_m=198 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_campaign_to_231() -> bool:
	if not await _touch_campaign_to_198():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=198 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_traction_tram(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 supported_height_m=231 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_traction_tram(stage_at_198: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	# The focused slice stages only its initial Tower footing. The campaign
	# continues from Crown; both use ordinary touch for the entire tram route.
	if stage_at_198:
		if not h._native().debug_restart_at(Vector3(-24.7, 198.9, -159.8)):
			return h._fail("traction tram +198m staging rejected")
		await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(198.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("traction tram approach requires supported +198m Tower11")
	# Step around the landing panel post before approaching its deck side.
	for point in [Vector2(-22.5, -159.8), Vector2(-22.5, -125), Vector2(-23.15, -125),
			Vector2(-23.15, -122.75), Vector2(-22.5, -122.75)]:
		if not await h._walk_to(device, point, 0.10, 15.0):
			return h._fail("traction tram lower landing approach at=%s" % h._position())
	var lower: Dictionary = h._native().get_supplied_machine_state()
	if String(lower["name"]) != "TRACTION TRAM" or int(lower["station"]) != 2:
		return h._fail("traction tram lower call station is not reachable at=%s state=%s support=%s" % [h._position(), lower, h._native().get_support_entity_id()])
	for point in [Vector2(-23.15, -119.1), Vector2(-22.5, -119.1)]:
		if not await h._walk_to(device, point, 0.10, 8.0):
			return h._fail("traction tram physical boarding at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 2984, 2.0):
		return h._fail("traction tram boarding is not supported by deck2984")
	await h._face(Vector2(0, -1))
	var deck := int(h._native().get_kit_body_index(2984))
	if deck < 0 or String(h._native().get_supplied_machine_state()["name"]) != "TRACTION TRAM" \
			or int(h._native().get_supplied_machine_state()["station"]) != 1 \
			or not await h._offered(&"operate", "OPERATE", "TRACTION TRAM"):
		return h._fail("traction tram deck control unavailable at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("traction tram touch pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 230.98, 90.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not reached or not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2984:
		return h._fail("traction tram touch ride failed at=%s station=%s" % [h._position(), arrival])
	if String(arrival["name"]) != "TRACTION TRAM" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return h._fail("traction tram arrival violates finite energy/brake state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("traction tram DONE did not release operation")
	var pose: Transform3D = h._native().get_kit_body_transform(deck)
	for point in [Vector2(pose.origin.x - 0.65, -119.1), Vector2(pose.origin.x - 0.65, -122.75)]:
		if not await h._walk_to(device, point, 0.10, 8.0):
			return h._fail("traction tram upper receiver transfer at=%s" % h._position())
	if not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 1988:
		return h._fail("traction tram upper transfer missed the actual receiver1988")
	for point in [Vector2(pose.origin.x - 0.65, -127), Vector2(pose.origin.x, -127)]:
		if not await h._walk_to(device, point, 0.10, 8.0):
			return h._fail("traction tram +231m Tower exit at=%s" % h._position())
	if not h._standing_above(231.7) or absf(h._position().y - 231.9) > 0.15 \
			or int(h._native().get_support_entity_id()) != 11 or not h._campaign_world_intact():
		return h._fail("traction tram +231m arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=%s shipping_touch=1 traction_tram=1 finite_energy=1 supported_height_m=231 support=11 deaths=0" % [
		"supported_198m" if stage_at_198 else "ordinary_grade_spawn"]
	return true


func _touch_campaign_to_253() -> bool:
	if not await _touch_campaign_to_231():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=231 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_barrel_helix(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 barrel_helix=1 supported_height_m=253 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_barrel_helix(stage_at_231: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_231:
		if not h._native().debug_restart_at(Vector3(24.63, 231.9, -127)):
			return h._fail("barrel helix +231m staging rejected")
		await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(231.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("barrel helix approach requires supported +231m Tower11")
	for point in [Vector2(23, -127), Vector2(-22.5, -127), Vector2(-22.5, -142),
			Vector2(-24.7, -142.65), Vector2(-27.5, -142.65)]:
		if not await h._walk_to(device, point, 0.10, 15.0):
			return h._fail("barrel helix lower landing approach at=%s" % h._position())
	var lower: Dictionary = h._native().get_supplied_machine_state()
	if String(lower["name"]) != "BARREL HELIX" or int(lower["station"]) != 2:
		return h._fail("barrel helix lower call station is not reachable")
	if not await h._walk_to(device, Vector2(-31.3, -142.5), 0.10, 8.0):
		return h._fail("barrel helix physical boarding at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 2988, 2.0):
		return h._fail("barrel helix boarding is not supported by deck2988")
	await h._face(Vector2(0, -1))
	if String(h._native().get_supplied_machine_state()["name"]) != "BARREL HELIX" \
			or int(h._native().get_supplied_machine_state()["station"]) != 1 \
			or not await h._offered(&"operate", "OPERATE", "BARREL HELIX"):
		return h._fail("barrel helix deck control unavailable at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("barrel helix touch pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 252.98, 65.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not reached or not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2988:
		return h._fail("barrel helix touch ride failed at=%s station=%s" % [h._position(), arrival])
	if String(arrival["name"]) != "BARREL HELIX" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return h._fail("barrel helix arrival violates finite energy/brake state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("barrel helix DONE did not release operation")
	for point in [Vector2(-27.5, -142.65), Vector2(-24.7, -142.65)]:
		if not await h._walk_to(device, point, 0.10, 8.0):
			return h._fail("barrel helix +253m receiver/Tower exit at=%s" % h._position())
	if not h._standing_above(253.7) or absf(h._position().y - 253.9) > 0.15 \
			or int(h._native().get_support_entity_id()) != 11 or not h._campaign_world_intact():
		return h._fail("barrel helix +253m arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=%s shipping_touch=1 barrel_helix=1 finite_energy=1 supported_height_m=253 support=11 deaths=0" % [
		"supported_231m" if stage_at_231 else "ordinary_grade_spawn"]
	return true


func _touch_campaign_to_286() -> bool:
	if not await _touch_campaign_to_253():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=253 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_cascade_mast(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 barrel_helix=1 cascade_mast=1 supported_height_m=286 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_cascade_mast(stage_at_253: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_253:
		if not h._native().debug_restart_at(Vector3(-24.7, 253.9, -142.65)):
			return h._fail("cascade mast +253m staging rejected")
		await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(253.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("cascade mast approach requires supported +253m Tower11")
	for point in [Vector2(-24.7, -161.10), Vector2(-27.5, -161.10)]:
		if not await h._walk_to(device, point, 0.10, 10.0):
			return h._fail("cascade mast lower landing approach at=%s" % h._position())
	var lower: Dictionary = h._native().get_supplied_machine_state()
	if String(lower["name"]) != "CASCADE MAST" or int(lower["station"]) != 2:
		return h._fail("cascade mast lower call station is not reachable")
	if not await h._walk_to(device, Vector2(-32.8, -160.95), 0.10, 8.0):
		return h._fail("cascade mast physical boarding at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 2992, 2.0):
		return h._fail("cascade mast boarding is not supported by deck2992")
	await h._face(Vector2(0, -1))
	if String(h._native().get_supplied_machine_state()["name"]) != "CASCADE MAST" \
			or int(h._native().get_supplied_machine_state()["station"]) != 1 \
			or not await h._offered(&"operate", "OPERATE", "CASCADE MAST"):
		return h._fail("cascade mast deck control unavailable at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("cascade mast touch pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 285.98, 65.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not reached or not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2992:
		return h._fail("cascade mast touch ride failed at=%s station=%s" % [h._position(), arrival])
	if String(arrival["name"]) != "CASCADE MAST" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return h._fail("cascade mast arrival violates finite energy/brake state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("cascade mast DONE did not release operation")
	for point in [Vector2(-27.5, -161.10), Vector2(-24.7, -161.10)]:
		if not await h._walk_to(device, point, 0.10, 8.0):
			return h._fail("cascade mast +286m receiver/Tower exit at=%s" % h._position())
	if not h._standing_above(286.7) or absf(h._position().y - 286.9) > 0.15 \
			or int(h._native().get_support_entity_id()) != 11 or not h._campaign_world_intact():
		return h._fail("cascade mast +286m arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=%s shipping_touch=1 cascade_mast=1 finite_energy=1 supported_height_m=286 support=11 deaths=0" % [
		"supported_253m" if stage_at_253 else "ordinary_grade_spawn"]
	return true


func _touch_campaign_to_308() -> bool:
	if not await _touch_campaign_to_286():
		return false
	print("SCRAPERX_CAMPAIGN supported_height_m=286 support=11 deaths=0 staging=ordinary_grade_spawn")
	if not await _touch_pitman_lift(false) or not h._campaign_world_intact():
		return false
	h._detail = "staging=ordinary_grade_spawn cargo_net=1 upper_route=1 loaded_swing=1 service_lift=1 gravity_balance=1 crown_gondola=1 traction_tram=1 barrel_helix=1 cascade_mast=1 pitman_lift=1 supported_height_m=308 support=11 deaths=0 slingshot_work_j=0 world=slingshot"
	return true


func _touch_pitman_lift(stage_at_286: bool) -> bool:
	var device := InputRouter.Device.TOUCH
	if stage_at_286:
		if not h._native().debug_restart_at(Vector3(-24.7, 286.9, -161.10)):
			return h._fail("pitman lift +286m staging rejected")
		await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(286.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("pitman lift approach requires supported +286m Tower11")
	for point in [Vector2(-24.7, -142.65), Vector2(-32.25, -142.65)]:
		if not await h._walk_to(device, point, 0.10, 10.0):
			return h._fail("pitman lift lower landing approach at=%s" % h._position())
	var lower: Dictionary = h._native().get_supplied_machine_state()
	if String(lower["name"]) != "PITMAN LIFT" or int(lower["station"]) != 2:
		return h._fail("pitman lift lower call station is not reachable")
	if not await h._walk_to(device, Vector2(-42.5, -142.5), 0.10, 10.0):
		return h._fail("pitman lift physical boarding at=%s" % h._position())
	if not await h._wait_until(func() -> bool:
		return bool(h._ctx()["grounded"]) and int(h._native().get_support_entity_id()) == 2921, 2.0):
		return h._fail("pitman lift boarding is not supported by deck2921")
	await h._face(Vector2(0, -1))
	if String(h._native().get_supplied_machine_state()["name"]) != "PITMAN LIFT" \
			or int(h._native().get_supplied_machine_state()["station"]) != 1 \
			or not await h._offered(&"operate", "OPERATE", "PITMAN LIFT"):
		return h._fail("pitman lift deck control unavailable at=%s" % h._position())
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("pitman lift touch pendant did not open")
	var before: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 307.98, 65.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(0.5)
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not reached or not bool(h._ctx()["grounded"]) or int(h._native().get_support_entity_id()) != 2921:
		return h._fail("pitman lift touch ride failed at=%s station=%s" % [h._position(), arrival])
	if String(arrival["name"]) != "PITMAN LIFT" or not bool(arrival["braking"]) \
			or float(arrival["energy_j"]) < 0.0 or float(arrival["energy_j"]) > float(arrival["capacity_j"]) \
			or float(arrival["energy_j"]) >= float(before["energy_j"]):
		return h._fail("pitman lift arrival violates finite energy/brake state")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("pitman lift DONE did not release operation")
	for point in [Vector2(-32.25, -142.65), Vector2(-24.7, -142.65)]:
		if not await h._walk_to(device, point, 0.10, 10.0):
			return h._fail("pitman lift +308m receiver/Tower exit at=%s" % h._position())
	if not h._standing_above(308.7) or absf(h._position().y - 308.9) > 0.15 \
			or int(h._native().get_support_entity_id()) != 11 or not h._campaign_world_intact():
		return h._fail("pitman lift +308m arrival is not supported by Tower11 at=%s" % h._position())
	h._detail = "staging=%s shipping_touch=1 pitman_lift=1 finite_energy=1 supported_height_m=308 support=11 deaths=0" % [
		"supported_286m" if stage_at_286 else "ordinary_grade_spawn"]
	return true
