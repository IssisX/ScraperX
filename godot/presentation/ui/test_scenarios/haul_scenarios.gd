extends RefCounted

# Slab cart, taper inspection and gravity reclaim operations and recovery.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")

var h: Harness


func _init(harness: Harness) -> void:
	h = harness


# Focused normal-world route: only the initial supported Tower330 staging
# relocates the player. Every transfer uses the shipping viewport controls.
# An actual failed jump/recovery branch remains pending a feasible observed
# failure; the inspection stop below proves voluntary escape and reboarding.
func _touch_slab_haul_cart() -> bool:
	var device := InputRouter.Device.TOUCH
	var miss_state := func() -> String:
		return "tick=%d at=%s velocity=%s support=%d grounded=%s traversal=%d target=%d deaths=%d" % [
			int(h._native().get_tick_index()), h._position(), h._velocity(), int(h._native().get_support_entity_id()),
			bool(h._native().is_player_grounded()), int(h._native().get_traversal_state()),
			int(h._native().get_carry_target_entity_id()), int(h._native().get_death_count())]
	if not h._native().debug_restart_at(Vector3(-24.7, 330.9, -158.25)):
		return h._fail("slab haul +330m staging rejected")
	await h._seconds(0.5)
	if not h._campaign_world_intact() or not _reclaim_tower_arrived():
		return h._fail("slab haul approach requires supported +330m Tower11")
	# Preserve the raised wheel-exit tray: the longer inland approach stays
	# grounded on the existing Tower ring before reaching the cart control.
	var tray_watch := {"running": true, "grounded_only": true, "invalid": false,
		"airborne": false, "peak": h._position().y}
	_reclaim_sample_tray(tray_watch)
	for point in [Vector2(-22.4, -158.25), Vector2(-22.4, -156.9), Vector2(-24.7, -156.9)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true) or bool(tray_watch["invalid"]):
			tray_watch["running"] = false
			return h._fail("slab haul grounded tray detour target=%s at=%s" % [point, h._position()])
	tray_watch["running"] = false
	for point in [Vector2(-24.7, -140.15), Vector2(-25.05, -140.15)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("slab haul lower control approach at=%s" % h._position())
	if not _cart_station(2) or not await h._offered(&"operate", "OPERATE", "SLAB HAUL CART"):
		return h._fail("slab haul actual lower station2 unavailable")
	var deck := int(h._native().get_kit_body_index(2320))
	var slab := int(h._native().get_kit_body_index(2325))
	if deck < 0 or slab < 0:
		return h._fail("slab haul native deck2320/slab2325 missing")
	for point in [Vector2(-26.5, -140.25), Vector2(-41.8, -140.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("slab haul loading floor approach at=%s" % h._position())
	if int(h._native().get_support_entity_id()) != 1955:
		return h._fail("slab haul loading Jump lacks actual receiver1955")
	await h._face(Vector2(0, -1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(-41.8, -141.6), 0.10, 8.0, true) \
			or not await _wait_cart_footing(deck, 330.0, 8.0):
		return h._fail("slab haul south Jump missed stable cart2320 at=%s" % h._position())
	if not await _cart_operate(1):
		return false
	var loaded: Dictionary = h._native().get_supplied_machine_state()
	var slab_loaded: Transform3D = h._native().get_kit_body_transform(slab)
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 340.985, 90.0)
	h._touch(1, h._center(&"pendant_up"), false)
	if not reached or not await _wait_cart_footing(deck, 341.0, 12.0):
		return h._fail("slab haul neutral341 lacks stable cart2320 footing")
	var inspection: Dictionary = h._native().get_supplied_machine_state()
	var slab_inspection: Transform3D = h._native().get_kit_body_transform(slab)
	if not _cart_station(1) or not bool(inspection["braking"]) or not _cart_bank(inspection) \
			or slab_inspection.origin.y >= slab_loaded.origin.y - 5.0 \
			or float(inspection["energy_j"]) > float(loaded["energy_j"]) + 0.01:
		return h._fail("slab haul inspection lacks falling slab/neutral brake/finite energy")
	if not await _cart_done():
		return false
	var pose: Transform3D = h._native().get_kit_body_transform(deck)
	var floor_point := pose * Vector3(-0.30, 2.75, 0)
	if not await h._walk_to(device, Vector2(floor_point.x, -141.6), 0.10, 8.0, true):
		return h._fail("slab haul inspection takeoff unavailable")
	await h._face(Vector2(0, 1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(floor_point.x, -140.25), 0.10, 8.0, true) \
			or int(h._native().get_support_entity_id()) != 1956:
		return h._fail("slab haul341 side Jump missed actual inspection tongue1956")
	# Keep the fixed-control stance wholly on1956, away from the Tower seam.
	if not await h._walk_to(device, Vector2(-26.5, -140.15), 0.10, 12.0, true):
		return h._fail("slab haul inspection fixed post approach at=%s" % h._position())
	if not _cart_station(4) or not await h._offered(&"operate", "OPERATE", "SLAB HAUL CART") \
			or not h._standing_above(341.7) or int(h._native().get_support_entity_id()) != 1956:
		return h._fail("slab haul actual inspection station4 unavailable")
	for point in [Vector2(-26.5, -140.25), Vector2(floor_point.x, -140.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("slab haul inspection reboarding approach at=%s" % h._position())
	await h._face(Vector2(0, -1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(floor_point.x, -141.6), 0.10, 8.0, true) \
			or not await _wait_cart_footing(deck, 341.0, 8.0):
		return h._fail("slab haul341 reboard Jump missed actual cart2320")
	if not await _cart_operate(1):
		return false
	h._touch(1, h._center(&"pendant_up"), true)
	reached = await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 351.985, 90.0)
	h._touch(1, h._center(&"pendant_up"), false)
	if not reached or not await _wait_cart_footing(deck, 352.0, 12.0):
		return h._fail("slab haul upper seat lacks stable352 cart2320")
	if not await _cart_done():
		return false
	# A missed commitment has a real inspection apron eleven metres below.
	# Walk off without Jump, then steer in flight; no relocation or catch grant.
	print("SCRAPERX_CART_MISS before at=%s support=%d grounded=%s deck=%s" % [
		h._position(), h._native().get_support_entity_id(), h._native().is_player_grounded(),
		h._native().get_supplied_machine_state()["surface_y"]])
	await h._face(Vector2(1, 0))
	h._move(device, 1.0)
	var left_deck := await h._wait_until(func() -> bool:
		return not bool(h._native().is_player_grounded()), 5.0)
	h._move(device, 0.0)
	print("SCRAPERX_CART_MISS departure=%s at=%s support=%d grounded=%s" % [
		left_deck, h._position(), h._native().get_support_entity_id(), h._native().is_player_grounded()])
	if not left_deck:
		return h._fail("slab haul miss did not leave cart at=%s support=%d" % [h._position(), h._native().get_support_entity_id()])
	# The old x=-26.9 aim landed on the real upper track1954 at y=350.04.
	# Match the native walking-miss route's x=-28 corridor to clear that
	# track and reach the actual341 apron; upper-frame footing cannot pass.
	var recovered := await h._walk_to(device, Vector2(-28.0, -141.6), 0.10, 10.0, true)
	if not recovered or int(h._native().get_support_entity_id()) != 1956:
		return h._fail("slab haul missed exit lacks341 apron arrived=%s %s" % [recovered, miss_state.call()])
	print("SCRAPERX_CART_MISS apron %s landing=%s" % [miss_state.call(), h._native().get_landing_state()])
	# The real heavy landing temporarily compacts the capsule. Require its
	# native recovery and clearance to restore standing before the next action.
	var recovered_stance := await h._wait_until(func() -> bool:
		return h._standing_above(341.7) and int(h._native().get_support_entity_id()) == 1956, 3.0)
	if not recovered_stance:
		return h._fail("slab haul341 landing did not recover standing %s landing=%s" % [
			miss_state.call(), h._native().get_landing_state()])
	print("SCRAPERX_CART_MISS recovered %s" % miss_state.call())
	for point in [Vector2(-28.0, -140.15), Vector2(-26.5, -140.15)]:
		if not await h._walk_to(device, point, 0.10, 8.0, true):
			return h._fail("slab haul recovery post approach failed at=%s" % h._position())
	if not await _cart_operate(4):
		return false
	var before_recall: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_down"), true)
	var recalled := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) <= 341.015, 90.0)
	h._touch(1, h._center(&"pendant_down"), false)
	await h._seconds(0.5)
	var after_recall: Dictionary = h._native().get_supplied_machine_state()
	if not recalled or absf(float(after_recall["surface_y"]) - 341.0) > 0.10 \
			or not _cart_station(4) or not _cart_bank(after_recall) \
			or float(after_recall["energy_j"]) >= float(before_recall["energy_j"]) \
			or int(h._native().get_support_entity_id()) != 1956:
		return h._fail("slab haul paid341 recall lacks real returned deck/source work/ashore footing")
	print("SCRAPERX_CART_MISS paid_recall %s energy_before_j=%s energy_after_j=%s work_before_j=%s work_after_j=%s" % [
		miss_state.call(), before_recall["energy_j"], after_recall["energy_j"],
		before_recall["positive_work_j"], after_recall["positive_work_j"]])
	if not await _cart_done():
		return false
	if not await h._walk_to(device, Vector2(-35.45, -140.25), 0.10, 12.0, true):
		return h._fail("slab haul recovery reboarding approach failed")
	await h._face(Vector2(0, -1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(-35.45, -141.6), 0.10, 8.0, true) \
			or not await _wait_cart_footing(deck, 341.0, 8.0) or not await _cart_operate(1):
		return h._fail("slab haul recovery reboard missed actual cart")
	h._touch(1, h._center(&"pendant_up"), true)
	var retried := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 351.985, 90.0)
	h._touch(1, h._center(&"pendant_up"), false)
	if not retried or not await _wait_cart_footing(deck, 352.0, 12.0) or not await _cart_done():
		return h._fail("slab haul recovery retry lacks actual352 footing")
	if not await h._walk_to(device, Vector2(-28.10, -141.6), 0.10, 8.0, true) \
			or int(h._native().get_support_entity_id()) != 2320:
		return h._fail("slab haul352 north lane takeoff lacks actual cart footing")
	await h._face(Vector2(1, 0))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(-25.35, -141.6), 0.10, 8.0, true) \
			or not _cart_tower_arrived():
		return h._fail("slab haul352 real north lane Jump missed Tower11 at=%s" % h._position())
	await h._seconds(0.5)
	if not _cart_tower_arrived():
		return h._fail("slab haul352 Tower11 footing did not remain stable")
	if not await h._walk_to(device, Vector2(-25.05, -140.15), 0.10, 8.0, true) \
			or not await _cart_operate(3):
		return h._fail("slab haul upper DOWN control approach failed")
	var before_return: Dictionary = h._native().get_supplied_machine_state()
	var slab_before_return: Transform3D = h._native().get_kit_body_transform(slab)
	var ashore := h._position()
	var ashore_support := int(h._native().get_support_entity_id())
	if ashore_support not in [11, 1957] or not h._standing_above(352.7):
		return h._fail("slab haul upper return has no actual ashore footing")
	h._touch(1, h._center(&"pendant_down"), true)
	var returned := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) <= 330.05, 120.0)
	h._touch(1, h._center(&"pendant_down"), false)
	await h._seconds(0.5)
	var after: Dictionary = h._native().get_supplied_machine_state()
	var deck_after: Transform3D = h._native().get_kit_body_transform(deck)
	var slab_after: Transform3D = h._native().get_kit_body_transform(slab)
	var floor_after := deck_after * Vector3(-0.30, 2.75, 0)
	if not returned or not _cart_station(3) or not bool(after["braking"]) or not _cart_bank(after) \
			or absf(floor_after.y - 330.0) > 0.10 or absf(floor_after.x + 41.8) > 0.15 \
			or slab_after.origin.y <= slab_before_return.origin.y + 20.0 \
			or float(after["positive_work_j"]) <= float(before_return["positive_work_j"]) \
			or float(after["energy_j"]) >= float(before_return["energy_j"]) \
			or not h._standing_above(352.7) or int(h._native().get_support_entity_id()) != ashore_support \
			or h._position().distance_to(ashore) > 0.15:
		return h._fail("slab haul paid return lacks real raised slab/cart330/unchanged ashore footing state=%s" % after)
	if not await _cart_done():
		return false
	if not await h._walk_to(device, Vector2(-25.35, -141.6), 0.10, 8.0, true) \
			or not _cart_tower_arrived() or not h._campaign_world_intact():
		return h._fail("slab haul final352 proof lacks stable death-free Tower11")
	print("SCRAPERX_CART_MISS complete %s energy_before_return_j=%s energy_j=%s work_before_return_j=%s work_j=%s capacity_j=%s overdraft_j=%s cutoff=%s" % [
		miss_state.call(), before_return["energy_j"], after["energy_j"],
		before_return["positive_work_j"], after["positive_work_j"], after["capacity_j"],
		after["energy_overdraft_j"], after["energy_cutoff"]])
	h._detail = "staging=supported_330m shipping_touch=1 slab_haul_cart=1 inspection341=1 reboard=1 north_lane_jump=1 paid_return=1 finite_energy=1 supported_height_m=352 support=11 deaths=0 failure_recovery=upper_miss341_paid_recall_reboard_retry"
	return true


func _cart_station(station: int) -> bool:
	var state: Dictionary = h._native().get_supplied_machine_state()
	return int(state["index"]) == 7 and int(state["station"]) == station \
		and String(state["name"]) == "SLAB HAUL CART"


func _cart_bank(state: Dictionary) -> bool:
	return is_finite(float(state["energy_j"])) and is_finite(float(state["positive_work_j"])) \
		and float(state["energy_j"]) >= 0.0 and float(state["energy_j"]) <= float(state["capacity_j"]) \
		and float(state["positive_work_j"]) >= 0.0 and float(state["energy_overdraft_j"]) == 0.0 \
		and not bool(state["energy_cutoff"])


func _cart_tower_arrived() -> bool:
	return h._standing_above(352.7) and absf(h._position().y - 352.9) <= 0.15 \
		and int(h._native().get_support_entity_id()) == 11


func _cart_operate(station: int) -> bool:
	if not _cart_station(station) or not await h._offered(&"operate", "OPERATE", "SLAB HAUL CART"):
		return h._fail("slab haul actual station%d OPERATE unavailable at=%s" % [station, h._position()])
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up") \
			or not h._main._touch.is_button_shown(&"pendant_down"):
		return h._fail("slab haul Action did not open ordinary pendant")
	return true


func _cart_done() -> bool:
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	return h._main._operating == &"" or h._fail("slab haul DONE did not release operation")


func _wait_cart_footing(deck: int, surface: float, timeout: float) -> bool:
	var stable_ticks := 0
	var previous_tick := int(h._native().get_tick_index())
	var waited := 0.0
	while waited < timeout:
		var pose: Transform3D = h._native().get_kit_body_transform(deck)
		var floor_point := pose * Vector3(-0.30, 2.75, 0)
		var tick := int(h._native().get_tick_index())
		var settled: bool = h._standing_above(surface + 0.7) and int(h._native().get_support_entity_id()) == 2320 \
			and absf(floor_point.y - surface) <= 0.10 \
			and absf(h._position().y - surface - 0.9) <= 0.15 \
			and h._native().get_kit_body_linear_velocity(deck).length() < 0.10 \
			and (h._velocity() - h._native().get_support_point_linear_velocity()).length() < 0.15
		stable_ticks = stable_ticks + maxi(0, tick - previous_tick) if settled else 0
		previous_tick = tick
		if stable_ticks >= 18:
			return true
		await h.get_tree().process_frame
		waited += h.get_process_delta_time()
	return false


func _touch_taper_inspection_route() -> bool:
	var device := InputRouter.Device.TOUCH
	# The sole staging operation is the measured, supported cart Tower exit.
	# Everything after it uses viewport touch movement, Jump and crouch.
	if not h._campaign_world_intact() or not h._native().debug_restart_at(Vector3(-25.4203, 352.9, -141.599)):
		return h._fail("taper supported cart-exit staging rejected")
	h._move(device, 0.0)
	await h._seconds(2.0)
	if not await _taper_stable(11, 352.9) or int(h._native().get_entity_body_count(1932)) != 1 \
			or int(h._native().get_entity_body_count(1933)) != 1:
		return h._fail("taper requires normal-world cart footing and actual inspection/recovery bodies")
	var lower_work := float(h._native().get_landing_state()["landing_recovery_work_j"])
	for point in [Vector2(-24.12, -138.6), Vector2(-24.0, -139.5)]:
		if not await h._walk_to(device, point, 0.08, 12.0, true) or not _taper_ordinary():
			return h._fail("taper lower toe approach target=%s at=%s" % [point, h._position()])
	if not bool(h._native().is_player_grounded()) or h._position().y >= 353.3:
		return h._fail("taper lower girder toe has no actual352 footing at=%s" % h._position())
	if not await h._walk_to(device, Vector2(-24.0, -164.0), 0.08, 45.0, true) \
			or not await _taper_stable(1932, 363.9) \
			or float(h._native().get_landing_state()["landing_recovery_work_j"]) <= lower_work:
		return h._fail("taper lower girder did not earn supported landingA363 at=%s" % h._position())
	print("SCRAPERX_TAPER landing_a at=%s support=%d" % [h._position(), h._native().get_support_entity_id()])
	if not await h._walk_to(device, Vector2(-25.9, -164.0), 0.08, 10.0, true):
		return h._fail("taper A run-up unavailable")
	await h._face(Vector2(1, 0))
	h._move(device, 1.0)
	var run_up := await h._wait_until(func() -> bool: return h._position().x >= -23.95, 2.0)
	var launch := h._position()
	var launch_work := float(h._native().get_landing_state()["landing_jump_work_j"])
	if not run_up or not bool(h._native().is_player_grounded()) or int(h._native().get_support_entity_id()) != 1932 \
			or launch.x < -24.02 or launch.x >= -23.7 or h._velocity().x <= 2.0 \
			or not h._main._touch.is_button_shown(&"jump"):
		h._move(device, 0.0)
		return h._fail("taper gap Jump lacks supported running stance at=%s velocity=%s" % [launch, h._velocity()])
	var gap := {"running": true, "rise": false, "middle": false, "fall": false, "invalid": false, "peak": launch.y}
	_taper_sample_flight(gap)
	h._tap(1, h._center(&"jump"))
	var crossed := await h._walk_to(device, Vector2(-20.15, -164.0), 0.08, 8.0, true)
	gap["running"] = false
	var jump_work := float(h._native().get_landing_state()["landing_jump_work_j"]) - launch_work
	if not crossed or not bool(gap["rise"]) or not bool(gap["middle"]) or bool(gap["invalid"]) \
			or float(gap["peak"]) <= launch.y + 0.8 or jump_work <= 0.0 or jump_work > 1285.725 \
			or not await _taper_stable(1932, 363.9):
		return h._fail("taper real2m Jump did not reach stable landingB at=%s flight=%s work_j=%.3f" % [h._position(), gap, jump_work])
	print("SCRAPERX_TAPER gap_b at=%s peak=%.3f jump_work_j=%.3f" % [h._position(), gap["peak"], jump_work])
	# Walk back into the open gap without Jump. This connected middle miss
	# must be caught by the real apron, then use the longer crouched bypass.
	var before_miss := h._position()
	var before_landing: Dictionary = h._native().get_landing_state()
	var miss := {"running": true, "rise": false, "middle": false, "fall": false, "invalid": false, "peak": before_miss.y}
	_taper_sample_flight(miss)
	var caught := await h._walk_to(device, Vector2(-22.4, -164.0), 0.08, 12.0, true)
	miss["running"] = false
	var landing: Dictionary = h._native().get_landing_state()
	if not caught or not bool(miss["fall"]) or bool(miss["invalid"]) \
			or int(landing["landing_count"]) <= int(before_landing["landing_count"]) \
			or int(landing["landing_support_entity_id"]) != 1933 \
			or float(landing["landing_jump_work_j"]) != float(before_landing["landing_jump_work_j"]) \
			or h._position().y >= before_miss.y - 2.0 or not await _taper_stable(1933, 361.4):
		return h._fail("taper walking miss lacks actual360.5 apron recovery at=%s flight=%s landing=%s" % [h._position(), miss, landing])
	print("SCRAPERX_TAPER middle_apron at=%s support=%d" % [h._position(), h._native().get_support_entity_id()])
	for point in [Vector2(-22.4, -157.7), Vector2(-19.55, -157.7), Vector2(-19.55, -157.55)]:
		if not await h._walk_to(device, point, 0.08, 15.0, true) or not _taper_ordinary():
			return h._fail("taper service toe approach target=%s at=%s" % [point, h._position()])
	var stood_through := await h._walk_to(device, Vector2(-19.55, -160.2), 0.08, 3.0, true)
	if stood_through or not _taper_ordinary() or not bool(h._native().is_player_grounded()) \
			or bool(h._native().is_player_crouched()) or h._position().z <= -159.5 or h._position().z >= -157.6:
		return h._fail("taper real bearing header did not block standing at=%s" % h._position())
	h._tap(1, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool: return bool(h._native().is_player_crouched()), 0.4):
		return h._fail("taper touch crouch input did not reach native posture")
	# Hold deliberate forward effort through the loaded incline. Proportional
	# easing far from the target supplies too little uphill crouched effort;
	# the player can hold the ordinary stick, then brake beyond the header.
	await h._face(Vector2(0, -1))
	h._move(device, 1.0)
	var cleared_header := await h._wait_until(func() -> bool: return h._position().z < -160.3, 6.0)
	h._move(device, 0.0)
	if not cleared_header or not await h._walk_to(device, Vector2(-19.55, -160.2), 0.08, 12.0, true) \
			or not bool(h._native().is_player_crouched()) or not bool(h._native().is_player_grounded()) \
			or int(h._native().get_support_entity_id()) != 1932 or not _taper_ordinary():
		return h._fail("taper touch crouch failed actual inclined header passage at=%s" % h._position())
	h._tap(1, h._center(&"crouch"))
	if not await h._wait_until(func() -> bool: return not bool(h._native().is_player_crouched()), 0.4) \
			or h._position().z >= -159.8:
		return h._fail("taper touch Stand lacks clearance beyond real header at=%s" % h._position())
	if not await h._walk_to(device, Vector2(-19.55, -164.0), 0.08, 15.0, true) \
			or not await _taper_stable(1932, 363.9):
		return h._fail("taper service bypass did not rejoin supported landingB at=%s" % h._position())
	print("SCRAPERX_TAPER service_rejoin at=%s support=%d" % [h._position(), h._native().get_support_entity_id()])
	if not await h._walk_to(device, Vector2(-20.35, -164.0), 0.08, 10.0, true) \
			or not await h._walk_to(device, Vector2(-20.35, -139.05), 0.08, 45.0, true) \
			or not await _taper_stable(1932, 374.9):
		return h._fail("taper upper girder did not reach actual turning platform374 at=%s" % h._position())
	for point in [Vector2(-20.35, -139.5), Vector2(-22.5, -139.5)]:
		if not await h._walk_to(device, point, 0.08, 15.0, true):
			return h._fail("taper final return beam approach target=%s at=%s" % [point, h._position()])
	if not await _taper_stable(51, 375.15):
		return h._fail("taper return did not step onto existing WorldSolid51 floor374.25 at=%s" % h._position())
	var exit := h._position()
	if not await h._walk_to(device, Vector2(-22.5, -143.0), 0.08, 15.0, true) \
			or not await _taper_stable(51, 375.15) or h._position().distance_to(exit) <= 3.0 \
			or h._main._router.device != InputRouter.Device.TOUCH or not h._campaign_world_intact():
		return h._fail("taper existing upper floor lacks stable ordinary onward walking at=%s" % h._position())
	await h._pose("taper_existing_floor374_25")
	h._detail = "staging=supported_cart_exit352 shipping_touch=1 lower_girder=1 ordinary2m_gap_jump=1 middle_miss_apron360_5=1 standing_header_blocked=1 crouched_service_bypass=1 upper_girder=1 existing_floor374_25=1 onward_m=%.3f support=51 deaths=0 gravity=1 upper_miss=native_only jump_work_j=%.3f" % [h._position().distance_to(exit), jump_work]
	return true


func _taper_ordinary() -> bool:
	var state: Dictionary = h._native().get_landing_state()
	return int(h._native().get_death_count()) == 0 and int(h._native().get_traversal_state()) == 0 \
		and int(h._native().get_accepted_traversal_count()) == 0 and not bool(h._native().is_parachute_deployed()) \
		and not bool(h._native().is_player_sprinting()) and h._position().is_finite() and h._velocity().is_finite() \
		and float(state.get("player_gravity_factor", -1.0)) == 1.0 \
		and int(state.get("traversal_hand_constraint_count", -1)) == 0 \
		and is_finite(float(state["landing_recovery_work_j"])) and is_finite(float(state["landing_jump_work_j"]))


func _taper_stable(support: int, standing_y: float) -> bool:
	h._move(InputRouter.Device.TOUCH, 0.0)
	var footing := func() -> bool:
		return _taper_ordinary() and bool(h._native().is_player_grounded()) \
			and int(h._native().get_support_entity_id()) == support and not bool(h._native().is_player_crouched()) \
			and absf(h._position().y - standing_y) < 0.12 and absf(h._velocity().y) < 0.12 \
			and Vector2(h._velocity().x, h._velocity().z).length() < 0.18
	if not await h._wait_until(footing, 3.0):
		return false
	var start := int(h._native().get_tick_index())
	while int(h._native().get_tick_index()) - start < 90:
		await h.get_tree().process_frame
		if not footing.call():
			return false
	return true


# Runs beside the actual touch walk helper, reading native flight each frame.
func _taper_sample_flight(watch: Dictionary) -> void:
	var previous_airborne := false
	var previous_vy := h._velocity().y
	while bool(watch["running"]):
		var at := h._position()
		var velocity := h._velocity()
		var airborne := not bool(h._native().is_player_grounded()) and int(h._native().get_support_entity_id()) == 0
		watch["invalid"] = bool(watch["invalid"]) or not _taper_ordinary()
		watch["peak"] = maxf(float(watch["peak"]), at.y)
		watch["rise"] = bool(watch["rise"]) or (airborne and velocity.y > 0.5)
		watch["fall"] = bool(watch["fall"]) or (airborne and velocity.y < -3.0)
		if airborne and previous_airborne and at.x > -23.0 and at.x < -21.8 and at.y > 364.1:
			watch["middle"] = true
			watch["invalid"] = bool(watch["invalid"]) or velocity.y > previous_vy + 0.01
		previous_airborne = airborne
		previous_vy = velocity.y
		await h.get_tree().process_frame


func _touch_gravity_reclaim() -> bool:
	var device := InputRouter.Device.TOUCH
	if not h._native().debug_restart_at(Vector3(-24.7, 308.9, -142.65)):
		return h._fail("gravity reclaim +308m staging rejected")
	await h._seconds(0.5)
	if not h._campaign_world_intact() or not h._standing_above(308.7) \
			or int(h._native().get_support_entity_id()) != 11:
		return h._fail("gravity reclaim approach requires supported +308m Tower11")
	# Approach alongside the physical landing post, within ordinary reach.
	for point in [Vector2(-24.7, -158.75), Vector2(-33.4, -158.75), Vector2(-33.4, -158.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("gravity reclaim feed approach at=%s" % h._position())
	var before: Dictionary = h._native().get_supplied_machine_state()
	if not _reclaim_station(2) or not await h._offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return h._fail("gravity reclaim lower feed control unavailable")
	if float(before["hopper_mass_kg"]) <= 0.0:
		return h._fail("gravity reclaim has no finite starting hopper material")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("gravity reclaim feed pendant did not open")
	h._touch(1, h._center(&"pendant_up"), true)
	await h._seconds(18.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(4.0)
	var loaded: Dictionary = h._native().get_supplied_machine_state()
	if not _reclaim_station(2) or not bool(loaded["braking"]) or not _reclaim_bank(loaded) \
			or float(loaded["hopper_mass_kg"]) < 0.0 \
			or float(loaded["hopper_mass_kg"]) >= float(before["hopper_mass_kg"]) \
			or float(loaded["energy_j"]) >= float(before["energy_j"]):
		return h._fail("gravity reclaim touch feed did not draw from finite material/power")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity reclaim feed DONE did not release operation")
	var cabin := int(h._native().get_kit_body_index(2201))
	if cabin < 0:
		return h._fail("gravity reclaim cabin2201 body unavailable")
	var boarding_pose: Transform3D = h._native().get_kit_body_transform(cabin)
	var boarding_x := boarding_pose.origin.x - 0.65
	if not await h._walk_to(device, Vector2(boarding_x, -158.5), 0.10, 8.0, true):
		return h._fail("gravity reclaim boarding tongue approach at=%s" % h._position())
	await h._face(Vector2(0, -1))
	if not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 1941:
		return h._fail("gravity reclaim boarding Jump lacks lower receiver footing")
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(boarding_x, -160.10), 0.10, 8.0, true):
		return h._fail("gravity reclaim physical cabin boarding at=%s" % h._position())
	# Off-centre rider weight tilts the passive hanger. Walk behind the post
	# to its actual transformed floor centre before asking the brake to release.
	boarding_pose = h._native().get_kit_body_transform(cabin)
	var floor_centre := boarding_pose * Vector3(0, -2.5, -0.3)
	if not await h._walk_to(device, Vector2(floor_centre.x, floor_centre.z), 0.10, 8.0, true):
		return h._fail("gravity reclaim actual cabin floor centre approach at=%s" % h._position())
	if not await _wait_reclaim_cabin(cabin, 307.97, 8.0):
		return h._fail("gravity reclaim boarding lacks stable actual cabin2201 floor")
	if not _reclaim_station(1) or not await h._offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return h._fail("gravity reclaim cabin brake control unavailable")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("gravity reclaim cabin pendant did not open")
	h._touch(1, h._center(&"pendant_up"), true)
	var moved := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 314.0, 60.0)
	var rising: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), false)
	if not _reclaim_station(1) or bool(rising["braking"]) or bool(rising["wheel_motor_enabled"]):
		return h._fail("gravity reclaim cabin UP did not release a passive, unpowered wheel")
	if not moved or not await _wait_reclaim_cabin(cabin, 313.9, 12.0):
		return h._fail("gravity reclaim gravity ride did not stop on supported cabin")
	var paused: Dictionary = h._native().get_supplied_machine_state()
	var paused_height := float(paused["surface_y"])
	await h._seconds(0.5)
	if absf(float(h._native().get_supplied_machine_state()["surface_y"]) - paused_height) > 0.10 \
			or not _reclaim_bank(paused):
		return h._fail("gravity reclaim released brake did not retain its physical stop")
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 329.97, 90.0)
	var resumed: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), false)
	if bool(resumed["braking"]) or bool(resumed["wheel_motor_enabled"]):
		return h._fail("gravity reclaim resumed ascent violated passive brake-release semantics")
	if not reached or not await _wait_reclaim_cabin(cabin, 329.9, 15.0):
		return h._fail("gravity reclaim loaded ascent did not settle at +330m")
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not _reclaim_station(1) or not _reclaim_bank(arrival) \
			or float(arrival["energy_j"]) > float(loaded["energy_j"]) + 0.01 \
			or float(arrival["hopper_mass_kg"]) > float(loaded["hopper_mass_kg"]) + 0.01 \
			or absf(float(arrival["wheel_angle_rad"]) - float(loaded["wheel_angle_rad"])) < 0.1:
		return h._fail("gravity reclaim ascent violates finite material/bank or actual wheel motion")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity reclaim cabin DONE did not release operation")
	var pose: Transform3D = h._native().get_kit_body_transform(cabin)
	var exit_floor := pose * Vector3(-0.65, -2.5, 0)
	var exit_x := exit_floor.x
	for point in [Vector2(exit_x, exit_floor.z), Vector2(exit_x, -159.65)]:
		if not await h._walk_to(device, point, 0.10, 7.0, true):
			return h._fail("gravity reclaim upper cabin takeoff at=%s" % h._position())
	await h._face(Vector2(0, 1))
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(exit_x, -158.5), 0.10, 8.0, true) \
			or int(h._native().get_support_entity_id()) != 1942:
		return h._fail("gravity reclaim upper transfer missed actual receiver1942")
	for point in [Vector2(exit_x, -158.75), Vector2(-24.7, -158.75), Vector2(-24.7, -158.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("gravity reclaim +330m Tower exit at=%s" % h._position())
	if not _reclaim_tower_arrived():
		return h._fail("gravity reclaim +330m arrival lacks actual Tower11 footing")
	if not await _touch_reclaim_tray_choice():
		return false
	# The far-side panel opens the real scoop doors and releases the wheel
	# brake. Empty return remains gravity-driven while the player stays ashore.
	for point in [Vector2(-24.7, -158.75), Vector2(-33.4, -158.75), Vector2(-33.4, -158.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("gravity reclaim upper discharge approach at=%s" % h._position())
	if not _reclaim_station(3) or not await h._offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return h._fail("gravity reclaim upper discharge control unavailable")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_down"):
		return h._fail("gravity reclaim discharge pendant did not open")
	h._touch(1, h._center(&"pendant_down"), true)
	var returned := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) <= 308.05, 120.0)
	h._touch(1, h._center(&"pendant_down"), false)
	await h._seconds(0.5)
	var after: Dictionary = h._native().get_supplied_machine_state()
	if not returned or not _reclaim_station(3) or not bool(after["braking"]) \
			or not _reclaim_bank(after) or float(after["energy_j"]) >= float(arrival["energy_j"]) \
			or float(after["hopper_mass_kg"]) > float(loaded["hopper_mass_kg"]) + 0.01 \
			or not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 1942:
		return h._fail("gravity reclaim discharge/empty return did not preserve upper footing and finite supply")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity reclaim discharge DONE did not release operation")
	for point in [Vector2(-33.4, -158.75), Vector2(-24.7, -158.75), Vector2(-24.7, -158.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true):
			return h._fail("gravity reclaim final Tower footing at=%s" % h._position())
	if not _reclaim_tower_arrived() or not h._campaign_world_intact():
		return h._fail("gravity reclaim final +330m proof lacks Tower11/death-free world")
	if not await _touch_reclaim_retry(before):
		return false
	h._detail = "staging=supported_308m shipping_touch=1 finite_hopper=1 gravity_reclaim=1 stop_resume=1 grounded_tray_detour=1 ordinary_tray_jump=1 tray_gravity=1 tray_free_hands=1 upper_discharge=1 empty_gravity_return=1 finite_energy=1 first_supported_height_m=330 first_support=11 retry_from_feeding=1 second_supported_height_m=%.2f second_support=2201 deaths=0" % float(h._native().get_supplied_machine_state()["surface_y"])
	return true


func _touch_reclaim_tray_choice() -> bool:
	var device := InputRouter.Device.TOUCH
	var watch := {"running": true, "grounded_only": true, "invalid": false,
		"airborne": false, "peak": h._position().y}
	_reclaim_sample_tray(watch)
	# Walk around the actual receiver tray on the existing inland Tower ring,
	# then return to supported takeoff footing for the shorter ordinary Jump.
	for point in [Vector2(-22.4, -158.25), Vector2(-22.4, -156.9), Vector2(-24.7, -156.9),
			Vector2(-22.4, -156.9), Vector2(-22.4, -159.25), Vector2(-24.7, -159.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true) or bool(watch["invalid"]):
			watch["running"] = false
			return h._fail("gravity reclaim grounded tray detour target=%s at=%s" % [point, h._position()])
	await h._face(Vector2(0, 1))
	# Invert the touch deadzone to deliver native 0.6 forward effort. Keep
	# that same input through the flight; no contextual hand transfer is used.
	h._move(device, TouchControls.STICK_DEADZONE + (1.0 - TouchControls.STICK_DEADZONE) * 0.6)
	var start_tick := int(h._native().get_tick_index())
	var ran_up := await h._wait_until(func() -> bool:
		return int(h._native().get_tick_index()) - start_tick >= 10, 0.5)
	var launch := h._position()
	var launch_work := float(h._native().get_landing_state()["landing_jump_work_j"])
	if not ran_up or bool(watch["invalid"]) or not _reclaim_tower_arrived() \
			or h._velocity().z <= 0.0 or not h._main._touch.is_button_shown(&"jump") \
			or h._main._touch.button_label(&"jump") != "JUMP":
		h._move(device, 0.0)
		watch["running"] = false
		return h._fail("gravity reclaim tray Jump lacks ordinary running Tower footing at=%s" % launch)
	watch["grounded_only"] = false
	h._touch(1, h._center(&"jump"), true)
	var took_off := await h._wait_until(func() -> bool: return bool(watch["airborne"]), 0.5)
	h._touch(1, h._center(&"jump"), false)
	var crossed := await h._wait_until(func() -> bool:
		return bool(watch["invalid"]) or (bool(watch["airborne"]) \
			and h._native().is_player_grounded() and h._position().z > -157.05), 3.0)
	h._move(device, 0.0)
	var jump_work := float(h._native().get_landing_state()["landing_jump_work_j"]) - launch_work
	if not took_off or not crossed or bool(watch["invalid"]) or float(watch["peak"]) <= 331.9 \
			or jump_work <= 0.0 \
			or not await h._walk_to(device, Vector2(-24.7, -156.9), 0.10, 8.0, true) \
			or not _reclaim_tower_arrived() or bool(watch["invalid"]):
		watch["running"] = false
		return h._fail("gravity reclaim ordinary tray Jump failed at=%s flight=%s work_j=%.3f" % [h._position(), watch, jump_work])
	watch["grounded_only"] = true
	for point in [Vector2(-22.4, -156.9), Vector2(-22.4, -158.5), Vector2(-24.7, -158.25)]:
		if not await h._walk_to(device, point, 0.10, 12.0, true) or bool(watch["invalid"]):
			watch["running"] = false
			return h._fail("gravity reclaim tray return detour target=%s at=%s" % [point, h._position()])
	watch["running"] = false
	if not _reclaim_tower_arrived() or h._main._router.device != InputRouter.Device.TOUCH:
		return h._fail("gravity reclaim tray choice lacks restored ordinary touch Tower footing")
	print("SCRAPERX_RECLAIM tray grounded_detour=1 ordinary_jump=1 gravity=1 free_hands=1 support=11 peak_y=%.3f jump_work_j=%.3f" % [watch["peak"], jump_work])
	return true


func _reclaim_sample_tray(watch: Dictionary) -> void:
	while bool(watch["running"]):
		var state: Dictionary = h._native().get_landing_state()
		var grounded := bool(h._native().is_player_grounded())
		var support := int(h._native().get_support_entity_id())
		watch["invalid"] = bool(watch["invalid"]) or float(state.get("player_gravity_factor", -1.0)) != 1.0 \
			or int(state.get("traversal_hand_constraint_count", -1)) != 0 \
			or int(h._native().get_traversal_state()) != 0 or int(h._native().get_death_count()) != 0 \
			or bool(h._native().is_parachute_deployed()) or bool(h._native().is_player_sprinting()) \
			or not h._position().is_finite() or not h._velocity().is_finite() \
			or (bool(watch["grounded_only"]) and (not grounded or support != 11))
		watch["airborne"] = bool(watch["airborne"]) or (not grounded and support == 0 and h._velocity().y > 0.5)
		watch["peak"] = maxf(float(watch["peak"]), h._position().y)
		await h.get_tree().process_frame


func _touch_reclaim_retry(initial: Dictionary) -> bool:
	var device := InputRouter.Device.TOUCH
	var deaths := int(h._native().get_death_count())
	h._tap(0, h._center(&"pause"))
	await h._frames(2)
	if not h.get_tree().paused:
		return h._fail("gravity reclaim retry PAUSE did not open")
	h._click(h._main._pause_menu.side_button_center(&"restart"))
	await h._frames(2)
	var retry: Button = h._main._pause_menu._reclaim_retry_button
	if h._main._pause_menu.current_page() != &"restart" or not is_instance_valid(retry) \
			or not retry.is_visible_in_tree() or retry.disabled or retry.text != "RETRY FROM FEEDING":
		return h._fail("gravity reclaim retry button unavailable on actual RESTART page")
	# The same emulated GUI twin used by the ordinary touch pause-page tests.
	h._click(retry.get_global_rect().get_center())
	await h._frames(3)
	if h.get_tree().paused:
		return h._fail("gravity reclaim retry did not restore and resume the world")
	await h._seconds(0.5)
	var restored: Dictionary = h._native().get_supplied_machine_state()
	if not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 1941 \
			or absf(h._position().y - 308.9) > 0.15 or int(h._native().get_death_count()) != deaths \
			or not _reclaim_station(2) or not _reclaim_bank(restored) \
			or float(restored["hopper_mass_kg"]) <= 0.0 \
			or float(restored["hopper_mass_kg"]) <= float(initial["hopper_mass_kg"]) - 100.0 \
			or float(restored["hopper_mass_kg"]) > float(initial["hopper_mass_kg"]) + 0.01 \
			or absf(float(restored["surface_y"]) - 308.0) >= 0.05:
		return h._fail("gravity reclaim retry lacks actual safe1941 footing/finite restored stock at=%s state=%s" % [h._position(), restored])
	if not await h._walk_to(device, Vector2(-33.4, -158.25), 0.10, 8.0, true) \
			or not _reclaim_station(2) or not await h._offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return h._fail("gravity reclaim retry ordinary feeding approach failed")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("gravity reclaim retry feed pendant did not open")
	h._touch(1, h._center(&"pendant_up"), true)
	await h._seconds(18.0)
	h._touch(1, h._center(&"pendant_up"), false)
	await h._seconds(4.0)
	var loaded: Dictionary = h._native().get_supplied_machine_state()
	if not _reclaim_station(2) or not bool(loaded["braking"]) or not _reclaim_bank(loaded) \
			or float(loaded["hopper_mass_kg"]) < 0.0 \
			or float(loaded["hopper_mass_kg"]) >= float(restored["hopper_mass_kg"]) \
			or float(loaded["energy_j"]) >= float(restored["energy_j"]):
		return h._fail("gravity reclaim retry did not feed actual restored finite material/power")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity reclaim retry feed DONE did not release operation")
	var cabin := int(h._native().get_kit_body_index(2201))
	if cabin < 0:
		return h._fail("gravity reclaim retry cabin2201 unavailable")
	var pose: Transform3D = h._native().get_kit_body_transform(cabin)
	var boarding_x := pose.origin.x - 0.65
	if not await h._walk_to(device, Vector2(boarding_x, -158.5), 0.10, 8.0, true):
		return h._fail("gravity reclaim retry boarding tongue approach failed")
	await h._face(Vector2(0, -1))
	if not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 1941:
		return h._fail("gravity reclaim retry Jump lacks lower receiver footing")
	h._tap(1, h._center(&"jump"))
	if not await h._walk_to(device, Vector2(boarding_x, -160.10), 0.10, 8.0, true):
		return h._fail("gravity reclaim retry physical cabin reboarding at=%s" % h._position())
	pose = h._native().get_kit_body_transform(cabin)
	var floor_centre := pose * Vector3(0, -2.5, -0.3)
	if not await h._walk_to(device, Vector2(floor_centre.x, floor_centre.z), 0.10, 8.0, true) \
			or not await _wait_reclaim_cabin(cabin, 307.97, 8.0):
		return h._fail("gravity reclaim retry ordinary reboarding lacks stable cabin2201 floor")
	if not _reclaim_station(1) or not await h._offered(&"operate", "OPERATE", "REFRACTORY RECLAIM"):
		return h._fail("gravity reclaim retry cabin brake control unavailable")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"supplied_machine" or not h._main._touch.is_button_shown(&"pendant_up"):
		return h._fail("gravity reclaim retry cabin pendant did not open")
	h._touch(1, h._center(&"pendant_up"), true)
	var reached := await h._wait_until(func() -> bool:
		return float(h._native().get_supplied_machine_state()["surface_y"]) >= 311.0 \
			or not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 2201, 60.0)
	var rising: Dictionary = h._native().get_supplied_machine_state()
	h._touch(1, h._center(&"pendant_up"), false)
	if not reached or float(rising["surface_y"]) < 311.0 or not _reclaim_station(1) \
			or not h._native().is_player_grounded() or int(h._native().get_support_entity_id()) != 2201 \
			or bool(rising["braking"]) or bool(rising["wheel_motor_enabled"]) \
			or not await _wait_reclaim_cabin(cabin, 311.0, 12.0):
		return h._fail("gravity reclaim retry finite load did not earn supported passive +311m ascent")
	var arrival: Dictionary = h._native().get_supplied_machine_state()
	if not _reclaim_bank(arrival) or float(arrival["energy_j"]) > float(loaded["energy_j"]) + 0.01 \
			or float(arrival["hopper_mass_kg"]) > float(loaded["hopper_mass_kg"]) + 0.01 \
			or int(h._native().get_death_count()) != deaths or not h._campaign_world_intact():
		return h._fail("gravity reclaim retry ascent violated finite supply/death-free world")
	h._tap(0, h._center(&"action"))
	await h._frames(3)
	if h._main._operating != &"":
		return h._fail("gravity reclaim retry cabin DONE did not release operation")
	return true


func _reclaim_station(station: int) -> bool:
	var state: Dictionary = h._native().get_supplied_machine_state()
	return int(state["index"]) == 6 and int(state["station"]) == station \
		and String(state["name"]) == "REFRACTORY RECLAIM"


func _reclaim_bank(state: Dictionary) -> bool:
	return is_finite(float(state["energy_j"])) and float(state["energy_j"]) >= 0.0 \
		and float(state["energy_j"]) <= float(state["capacity_j"]) and not bool(state["energy_cutoff"])


func _reclaim_tower_arrived() -> bool:
	return h._standing_above(330.7) and absf(h._position().y - 330.9) <= 0.15 \
		and int(h._native().get_support_entity_id()) == 11


func _wait_reclaim_cabin(cabin: int, minimum_surface: float, timeout: float) -> bool:
	var waited := 0.0
	var stable_ticks := 0
	var previous_tick := int(h._native().get_tick_index())
	var previous_angle := float(h._native().get_supplied_machine_state()["wheel_angle_rad"])
	var previous_floor := float(h._native().get_supplied_machine_state()["surface_y"])
	var angular_speed := INF
	var floor_speed := INF
	var step := float(h._native().get_fixed_step_seconds())
	while waited < timeout:
		var pose: Transform3D = h._native().get_kit_body_transform(cabin)
		var state: Dictionary = h._native().get_supplied_machine_state()
		var tick := int(h._native().get_tick_index())
		var elapsed_ticks := tick - previous_tick
		if elapsed_ticks > 0:
			angular_speed = absf(angle_difference(previous_angle, float(state["wheel_angle_rad"]))) \
				/ (float(elapsed_ticks) * step)
			floor_speed = absf(float(state["surface_y"]) - previous_floor) / (float(elapsed_ticks) * step)
			previous_tick = tick
			previous_angle = float(state["wheel_angle_rad"])
			previous_floor = float(state["surface_y"])
		var floor_at := pose * Vector3(0, -2.5, 0)
		var slip: Vector3 = h._velocity() - h._native().get_support_point_linear_velocity()
		var ready: bool = h._native().is_player_grounded() and int(h._native().get_support_entity_id()) == 2201 \
			and bool(state["braking"]) and float(state["surface_y"]) >= minimum_surface \
			and absf(h._position().y - (floor_at.y + 0.9)) <= 0.15 \
			and Vector2(slip.x, slip.z).length() < 0.15 and pose.basis.y.dot(Vector3.UP) > 0.995 \
			and (h._native().get_kit_body_linear_velocity(cabin) as Vector3).length() < 0.15 \
			and angular_speed < 0.0045 and floor_speed < 0.018
		# A finite brake first decelerates the wheel. Require 30 actual native
		# ticks of low wheel/floor speed and floor contact; suspension can keep
		# moving after the wheel stops.
		stable_ticks = stable_ticks + elapsed_ticks if ready else 0
		if stable_ticks >= 30:
			return true
		await h.get_tree().process_frame
		waited += h.get_process_delta_time()
	return false
