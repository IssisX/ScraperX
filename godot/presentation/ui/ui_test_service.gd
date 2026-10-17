extends "res://presentation/ui/ui_test_rides.gd"

# UI scenarios: Midstack Service (M, N, O, C5, C4) and the Stack's upper
# stages. (Part of the ui_test_driver.gd chain.)

const TOWER_ENTITY := 11
# AS-010's frame: the 662 deck and the 684 deck.
const SERVICE_FRAME_ENTITY := 1012

# AS-010's stage N alone, from the 684 deck start, on the device under test.
func _n(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _service_n(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck706_y=%.2f held_m=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(_service_state()["n_cage_travel"]),
		float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step, float(body["lift"])]
	return true

# AS-010's stage N, the granular discharge hoist, the way its native test
# takes it: from the 684 deck into the cage beside the flap's chain, GRAB it:
# it comes down to the hands and draws the flap's arm past its dead point,
# and the flap falls open. LET GO; the bin pours into the hopper, which sinks
# and hauls the cage to the 706 deck, where its dogs hold it; off east onto
# the deck.
func _service_n(device: int) -> bool:
	if not (await _go(device, Vector2(-5.6, -155.35), 0.15, 20.0) and \
			await _go(device, Vector2(-6.6, -155.35), 0.06, 6.0)):
		return _fail("N: the walk into the cage stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	# Look up at the chain hanging over the head, as a player does.
	await _tilt(0.35)
	if not await _offered(&"pick_up", "GRAB", "FLAP CHAIN"):
		return _fail("Action read '%s %s' beside N's chain, not GRAB FLAP CHAIN" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("service_n_chain")
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2138, 0.5):
		return _fail("GRAB did not put N's flap chain in the hands")
	await _tilt(0.0)
	if not await _wait_until(func() -> bool: return float(_service_state()["n_gate_angle"]) >= 0.3, 4.0):
		return _fail("holding the chain did not throw N's flap open (flap %.2f rad)" %
			float(_service_state()["n_gate_angle"]))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take N's chain out of the hands")
	await _pose("service_n_pour")
	# The ride, until the cage stands still on its dogs for a second.
	var last := float(_service_state()["n_cage_travel"])
	var still := 0.0
	var waited := 0.0
	while waited < 40.0 and still < 1.0:
		await get_tree().process_frame
		var dt := get_process_delta_time()
		waited += dt
		var travel := float(_service_state()["n_cage_travel"])
		still = still + dt if travel > 1.0 and absf(travel - last) < 0.0005 else 0.0
		last = travel
	var held := float(_service_state()["n_cage_travel"])
	if still < 1.0 or held < 21.45:
		return _fail("N's cage never came to rest on its dogs at the 706 deck (travel %.2f m, hopper %.0f kg)" % [
			held, float(_service_state()["n_hopper_kg"])])
	await _pose("service_n_top")
	if int(_native().get_support_entity_id()) != 2135:
		return _fail("the rider is not in N's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	# Off east onto the 706 deck.
	if not await _go(device, Vector2(-3.8, -155.0), 0.15, 6.0):
		return _fail("the step off N's cage onto the 706 deck stalled at %s (on %d, cage %.3f m)" % [
			str(_position()), int(_native().get_support_entity_id()), float(_service_state()["n_cage_travel"])])
	await _seconds(0.5)
	await _pose("service_deck706")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(706.6):
		return _fail("not standing on the 706 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# AS-010's stage O alone, from the 728 deck start, on the device under test.
func _o(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _service_o(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the rider died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck750_y=%.2f held_m=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(_service_state()["o_cab_travel"]),
		float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step, float(body["lift"])]
	return true

# AS-010's stage O, the gravel wheel, the way its native test takes it: from
# the 728 deck into the cab beside the chock's lanyard, GRAB it and step back
# with it until the chock lets the ram go, and LET GO. The ram swings onto the
# latch, the gate falls open and the gravel pours into the bucket, which sinks
# down its well and hauls the cab, by the rope over the wheel, to the 750
# deck, where its dogs hold it; off east onto the deck.
func _service_o(device: int) -> bool:
	if not (await _go(device, Vector2(8.0, -150.3), 0.15, 20.0) and \
			await _go(device, Vector2(5.2, -150.4), 0.15, 6.0)):
		return _fail("O: the walk to the cab stalled at %s" % str(_position()))
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _pull_handle(device, Vector2(3.3, -150.6), Vector2(0.0, -1.0), 2146, "CHOCK LANYARD",
			func() -> bool: return not bool(_service_state()["o_ram_latched"])):
		return false
	await _pose("service_o_pour")
	# The ride, until the cab stands still on its dogs for a second.
	var last := float(_service_state()["o_cab_travel"])
	var still := 0.0
	var waited := 0.0
	while waited < 60.0 and still < 1.0:
		await get_tree().process_frame
		var dt := get_process_delta_time()
		waited += dt
		var travel := float(_service_state()["o_cab_travel"])
		still = still + dt if travel > 1.0 and absf(travel - last) < 0.0005 else 0.0
		last = travel
	var held := float(_service_state()["o_cab_travel"])
	if still < 1.0 or held < 21.45:
		return _fail("O's cab never came to rest on its dogs at the 750 deck (travel %.2f m, bucket %.0f kg)" % [
			held, float(_service_state()["o_bucket_kg"])])
	await _pose("service_o_top")
	if int(_native().get_support_entity_id()) != 2139:
		return _fail("the rider is not in O's cab at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	# Off east onto the 750 deck.
	if not await _go(device, Vector2(5.2, -150.0), 0.15, 6.0):
		return _fail("the step off O's cab onto the 750 deck stalled at %s (on %d, cab %.3f m)" % [
			str(_position()), int(_native().get_support_entity_id()), float(_service_state()["o_cab_travel"])])
	await _seconds(0.5)
	await _pose("service_deck750")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(750.6):
		return _fail("not standing on the 750 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# AS-010's C5 alone, from the 706 deck start, on the device under test.
func _c5(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not await _service_c5(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck728_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true

# AS-010's C5, the cooling plant, the way its native climb takes it: from the
# 706 deck over the manifold in a vault, under the duct bank crouched, onto
# the tank, up its standpipe onto the platform over it, west along that at a
# sprint and over the 6.5 m gap, a hang up onto the platform north of the
# landing and one east of that, up the second standpipe, and over the 728
# deck's north girder.
func _service_c5(device: int) -> bool:
	if not (await _go(device, Vector2(-3.3, -146.0), 0.1, 12.0) and \
			await _go(device, Vector2(-0.85, -145.5), 0.06, 6.0)):
		return _fail("C5: the walk to the manifold stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("C5: Action read '%s %s' facing the manifold, not CLIMB" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == 3, 0.3):
		return _fail("C5: Action did not vault the manifold (state %d)" % int(_native().get_traversal_state()))
	if not await _wait_until(func() -> bool: return _standing_above(706.6) and _position().x > 0.9, 2.0):
		return _fail("C5: the vault over the manifold did not land (at %s)" % str(_position()))
	# Under the duct bank, crouched, and up again past it.
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return bool(_native().is_player_crouched()), 0.3):
		return _fail("C5: CROUCH did not crouch the body before the duct bank")
	if not await _go(device, Vector2(5.6, -145.5), 0.08, 8.0):
		return _fail("C5: the crawl under the duct bank stalled at %s" % str(_position()))
	_tap(1, _center(&"crouch"))
	if not await _wait_until(func() -> bool: return not bool(_native().is_player_crouched()), 0.5):
		return _fail("C5: STAND did not stand the body up past the duct bank")
	# Onto the tank and up its standpipe onto the platform over it.
	if not await _go(device, Vector2(8.5, -146.5), 0.06, 4.0):
		return _fail("C5: the walk to the tank stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(1.0, 0.0), 708.2, "the tank"):
		return false
	if not await _c5_pipe(device, Vector2(10.25, -144.8), Vector2(0.0, 1.0), 714.5, "the tank's standpipe"):
		return false
	# West along the platform at a sprint, and over the gap.
	if not await _go(device, Vector2(10.85, -142.6), 0.08, 4.0):
		return _fail("C5: the walk to the run-up stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	await _seconds(0.2)
	if not _stick_sprint():
		_move(device, 0.0)
		return _fail("C5: the stick pushed past its ring did not latch sprint")
	if not await _wait_until(func() -> bool: return _position().x <= 8.45, 2.0):
		_move(device, 0.0)
		return _fail("C5: the run-up stalled at %s" % str(_position()))
	if not bool(_ctx()["sprinting"]):
		_move(device, 0.0)
		return _fail("C5: not sprinting at the gap's edge (%.2f m/s)" % _walk_speed().length())
	_jump(device)
	var across: bool = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]) and \
		_position().y > 714.5 and _position().x < 1.5, 3.0)
	_move(device, 0.0)
	if not across:
		return _fail("C5: the jump over the gap did not land on the far platform (at %s)" % str(_position()))
	# A hang up onto the platform north of the landing, and one east of that.
	if not await _go(device, Vector2(-0.5, -141.6), 0.05, 6.0):
		return _fail("C5: the walk to the first platform's face stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, 1.0), 717.7, "the first platform"):
		return false
	if not await _go(device, Vector2(2.55, -139.6), 0.05, 6.0):
		return _fail("C5: the walk to the second platform's face stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(1.0, 0.0), 720.9, "the second platform"):
		return false
	# Up the second standpipe, and over the 728 deck's north girder.
	if not await _c5_pipe(device, Vector2(6.35, -139.63), Vector2(1.0, 0.0), 727.0, "the second standpipe"):
		return false
	if not await _go(device, Vector2(9.0, -141.75), 0.05, 6.0):
		return _fail("C5: the walk to the 728 deck's girder stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), 728.6, "the 728 deck"):
		return false
	await _seconds(0.5)
	await _pose("service_deck728")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(728.8):
		return _fail("C5: not standing on the 728 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# Steps to `at` beside a standpipe, turns to `facing` it, takes it on CLIMB
# HOLD and climbs until standing on what it tops out onto, above `top`.
func _c5_pipe(device: int, at: Vector2, facing: Vector2, top: float, what: String) -> bool:
	if not await _go(device, at, 0.06, 6.0):
		return _fail("C5: the step to %s stalled at %s" % [what, str(_position())])
	await _face(facing)
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("C5: Action read '%s %s' at %s, not CLIMB HOLD" % [_action_label(),
			String(_ctx()["action"]["detail"]), what])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("C5: CLIMB did not take %s" % what)
	_move(device, 1.0)
	var topped: bool = await _wait_until(func() -> bool: return _standing_above(top), 20.0)
	_move(device, 0.0)
	if not topped:
		return _fail("C5: %s never topped out (at %s)" % [what, str(_position())])
	return true

func _c4(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var body := _watch_body()
	if not await _service_c4(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	if not _heard_region(&"machine"):
		return false
	_detail = "deck684_y=%.2f seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true

# AS-010's C4, the service gantry, the way its native climb takes it: from
# the 662 deck onto the switchgear cabinet, a hang up onto the duct, east
# along it and over the beam to the pump deck, up the standpipe onto the
# hoist runway, over its edge into a hang and along its lip past the winch
# house, across the 3 m gap, a hang up onto the gallery, onto the riser, a
# hang up onto the hoist platform, and over the 684 deck's yellow girder.
func _service_c4(device: int) -> bool:
	if not await _go(device, Vector2(6.2, -149.1), 0.08, 20.0):
		return _fail("C4: the walk to the cabinet stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(0.0, -1.0), 664.3, "the cabinet"):
		return false
	if not await _walk_to(device, Vector2(6.2, -150.9), 0.05, 2.0):
		return _fail("C4: the step to the cabinet's back edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), 667.5, "the duct"):
		return false
	# East along the duct and over the beam, held on its line.
	if not await _go(device, Vector2(10.0, -152.3), 0.1, 6.0):
		return _fail("C4: the walk along the duct stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _walk_to(device, Vector2(10.0, -153.9), 0.1, 3.0) or not bool(_ctx()["balancing"]):
		return _fail("C4: not balancing on the beam at %s" % str(_position()))
	if not await _walk_to(device, Vector2(10.0, -158.0), 0.1, 6.0) or not _standing_above(667.5):
		return _fail("C4: the beam walk stalled at %s" % str(_position()))
	# Up the standpipe onto the runway.
	if not await _go(device, Vector2(9.5, -158.7), 0.08, 4.0):
		return _fail("C4: the step to the standpipe stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("C4: Action read '%s %s' at the standpipe, not CLIMB HOLD" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("C4: CLIMB did not take the standpipe")
	_move(device, 1.0)
	var on_runway: bool = await _wait_until(func() -> bool: return _standing_above(675.3), 20.0)
	_move(device, 0.0)
	if not on_runway:
		return _fail("C4: the standpipe never topped out onto the runway (at %s)" % str(_position()))
	# Over the runway's edge into a hang, and along its lip past the winch house.
	if not await _go(device, Vector2(7.8, -159.9), 0.08, 4.0):
		return _fail("C4: the walk to the runway's edge stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _wait_until(func() -> bool: return bool(_ctx()["drop_ok"]) and \
			(device != InputRouter.Device.TOUCH or _main._touch.is_button_shown(&"drop")), 1.5):
		return _fail("C4: DROP was not offered with the runway's edge behind (at %s)" % str(_position()))
	_drop(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 1.5):
		return _fail("C4: DROP did not lower the body into a hang on the runway's lip")
	_move_world(device, Vector2(-1.0, 0.0))
	var past: bool = await _wait_until(func() -> bool: return _position().x <= 4.4, 9.0)
	_move(device, 0.0)
	if not past:
		return _fail("C4: the hang stopped along the runway's lip at %s" % str(_position()))
	if not await _climb_up(device):
		return _fail("C4: CLIMB UP not offered hanging west of the winch house")
	if not await _wait_until(func() -> bool: return _standing_above(675.3), 2.5):
		return _fail("C4: the climb up west of the winch house did not land (at %s)" % str(_position()))
	# Across the gap: a run along +z off the runway's west end.
	if not await _go(device, Vector2(2.5, -161.0), 0.1, 6.0):
		return _fail("C4: the walk to the run-up stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	_move(device, 1.0)
	if not await _wait_until(func() -> bool: return _position().z >= -159.9, 2.0):
		_move(device, 0.0)
		return _fail("C4: the run-up stalled at %s" % str(_position()))
	_jump(device)
	var landed: bool = await _wait_until(func() -> bool: return bool(_ctx()["grounded"]) and \
		_position().y > 675.0 and _position().z > -156.4, 2.0)
	_move(device, 0.0)
	if not landed:
		return _fail("C4: the jump across the gap did not land on the landing (at %s)" % str(_position()))
	if not await _walk_to(device, Vector2(2.5, -153.25), 0.05, 4.0):
		return _fail("C4: the step to the landing's edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, 1.0), 678.5, "the gallery"):
		return false
	if not await _go(device, Vector2(0.9, -151.7), 0.08, 6.0):
		return _fail("C4: the walk along the gallery stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(-1.0, 0.0), 680.0, "the riser"):
		return false
	if not await _go(device, Vector2(-0.8, -152.2), 0.05, 3.0):
		return _fail("C4: the step to the riser's edge stalled at %s" % str(_position()))
	if not await _c4_hang_up(device, Vector2(0.0, -1.0), 683.2, "the hoist platform"):
		return false
	if not await _go(device, Vector2(-1.6, -153.7), 0.08, 4.0):
		return _fail("C4: the walk to the 684 deck's girder stalled at %s" % str(_position()))
	if not await _c4_mantle(device, Vector2(-1.0, 0.0), 684.8, "the 684 deck"):
		return false
	if not await _go(device, Vector2(-4.0, -153.7), 0.1, 4.0):
		return _fail("C4: the walk onto the 684 deck stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("service_deck684")
	if int(_native().get_support_entity_id()) != SERVICE_FRAME_ENTITY or not _standing_above(684.8):
		return _fail("C4: not standing on the 684 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

func _stack_s2(device: int) -> bool:
	if not await _go(device, Vector2(10.0, -130.5), 0.15, 15.0) or \
			not await _walk_to(device, Vector2(10.0, -137.6), 0.10, 15.0):
		return _fail("walk across crossover gangway into S2 cage stalled at %s" % str(_position()))
	# Round to the shoot bolt's push plate and walk into it: the body shoves
	# the bolt out along its slide, and the cage is free.
	if not await _walk_to(device, Vector2(9.1, -138.3), 0.08, 6.0):
		return _fail("the walk to S2's bolt plate stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not bool(_native().is_stack_s2_bolt_shot()):
		return _fail("S2's bolt was out before it was shoved")
	_move(device, 0.6)
	var s2_free: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_stack_s2_bolt_shot()), 6.0)
	_move(device, 0.0)
	if not s2_free:
		return _fail("walking into S2's bolt did not shove it out (at %s)" % str(_position()))
	var s2_arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s2_cage_travel()) >= 21.8, 15.0)
	if not s2_arrived:
		return _fail("S2 cage never reached Deck 6 (travel %.2f m)" % float(_native().get_stack_s2_cage_travel()))
	await _face(Vector2(0.0, 1.0))
	if not (await _walk_to(device, Vector2(10.0, -133.0), 0.15, 6.0) and \
			await _walk_to(device, Vector2(10.0, -126.0), 0.15, 6.0)):
		return _fail("walk off S2 onto Deck 6 stalled at %s" % str(_position()))
	await _seconds(0.3)
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != TOWER_ENTITY or \
			_position().y < 66.5:
		return _fail("not standing on Deck 6 (y %.2f)" % _position().y)
	await _pose("stack_deck6")
	return true

func _stack_c2(device: int) -> bool:
	if not await _go(device, Vector2(22.0, -125.5), 0.15, 20.0) or \
			not await _go(device, Vector2(22.0, -135.7), 0.08, 15.0):
		return _fail("walk to C2 switchgear cabinet stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing C2 cabinet, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(68.5), 2.0):
		return _fail("CLIMB did not mantle onto C2 cabinet (y %.2f)" % _position().y)
	if not await _walk_to(device, Vector2(22.0, -138.6), 0.05, 3.0):
		return _fail("step to cabinet north edge stalled")
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	var c2_hung: bool = await _leap(device, 0.4,
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 2.0)
	if not c2_hung:
		return _fail("jump from cabinet did not hang from C2 duct (at %s)" % str(_position()))
	if not await _climb_up(device):
		return _fail("CLIMB UP not offered hanging from C2 duct")
	if not await _wait_until(func() -> bool: return _standing_above(72.0), 2.5):
		return _fail("CLIMB UP did not bring climber onto C2 duct")
	if not (await _go(device, Vector2(22.0, -142.0), 0.1, 6.0) and \
			await _go(device, Vector2(22.0, -148.05), 0.06, 6.0)):
		return _fail("walk along C2 duct stalled")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing C2 wall ladder, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of C2 wall ladder")
	_move(device, 1.0)
	var on_deck7: bool = await _wait_until(func() -> bool: return _standing_above(77.5), 20.0)
	_move(device, 0.0)
	if not on_deck7:
		return _fail("climbing C2 wall ladder did not reach Deck 7 (y %.2f)" % _position().y)
	if not (await _go(device, Vector2(22.0, -155.0), 0.15, 10.0) and \
			await _go(device, Vector2(20.0, -160.20), 0.08, 10.0)):
		return _fail("walk across Deck 7 to pipe rack stalled")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing pipe rack, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(79.5), 2.0):
		return _fail("mantle onto pipe rack failed (y %.2f)" % _position().y)
	if not (await _go(device, Vector2(18.0, -162.0), 0.1, 6.0) and \
			await _go(device, Vector2(10.0, -162.0), 0.08, 15.0)):
		return _fail("walk along monorail beam stalled")
	await _seconds(0.3)
	if _position().y < 82.5:
		return _fail("not standing at monorail beam end (y %.2f)" % _position().y)
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	_jump(device)
	_move(device, 0.4)
	var c2_ladder_caught: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 2.0)
	if not c2_ladder_caught:
		_move(device, 0.0)
		return _fail("leap from monorail did not catch davit ladder")
	_move(device, 1.0)
	var on_c2_arm: bool = await _wait_until(func() -> bool: return _standing_above(88.5), 20.0)
	_move(device, 0.0)
	if not on_c2_arm:
		return _fail("up davit ladder did not top out onto arm (y %.2f)" % _position().y)
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(10.0, -165.0), 0.1, 6.0) and \
			await _walk_to(device, Vector2(10.0, -171.2), 0.1, 6.0)):
		return _fail("walk off davit arm onto Deck 8 stalled")
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or (int(_native().get_support_entity_id()) != TOWER_ENTITY and int(_native().get_support_entity_id()) != 1022) or \
			_position().y < 88.5:
		return _fail("not standing on Deck 8 (y %.2f)" % _position().y)
	await _pose("stack_deck8")
	return true

func _stack_s3(device: int) -> bool:
	if not (await _go(device, Vector2(-8.0, -170.0), 0.15, 20.0) and \
			await _go(device, Vector2(-8.0, -164.0), 0.1, 10.0) and \
			await _go(device, Vector2(-8.0, -157.6), 0.08, 10.0)):
		return _fail("the walk to S3 cage stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing S3 handle, not GRAB (at %s)" % [_action_label(), str(_position())])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2223, 0.5):
		return _fail("GRAB did not take S3 handle")
	_move(device, 0.4)
	var s3_tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_stack_s3_brake_latched()), 2.0)
	_move(device, 0.0)
	if not s3_tripped:
		return _fail("pulling S3 handle did not trip brake")
	var s3_arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s3_cage_travel()) >= 43.8, 25.0)
	if not s3_arrived:
		return _fail("S3 cage never reached Deck 12 (travel %.2f m)" % float(_native().get_stack_s3_cage_travel()))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not release S3 handle")
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(-8.0, -164.0), 0.1, 6.0) and \
			await _walk_to(device, Vector2(-8.0, -171.2), 0.1, 6.0)):
		return _fail("the walk off S3 onto Deck 12 stalled")
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or (int(_native().get_support_entity_id()) != TOWER_ENTITY and int(_native().get_support_entity_id()) != 1018) or \
			_position().y < 132.5:
		return _fail("not standing on Deck 12 (y %.2f)" % _position().y)
	await _pose("stack_deck12")
	return true

func _stack_c3(device: int) -> bool:
	if not (await _go(device, Vector2(-10.5, -171.0), 0.1, 10.0) and \
			await _go(device, Vector2(-10.5, -145.0), 0.1, 20.0)):
		return _fail("walk along atrium girder stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing atrium duct, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(138.0), 2.0):
		return _fail("mantle onto atrium duct failed (y %.2f)" % _position().y)
	if not await _go(device, Vector2(-10.5, -135.85), 0.08, 6.0):
		return _fail("walk to C3 wall ladder stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing C3 wall ladder, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("take hold of C3 wall ladder failed")
	_move(device, 1.0)
	var on_deck13: bool = await _wait_until(func() -> bool: return _standing_above(143.5), 20.0)
	_move(device, 0.0)
	if not on_deck13:
		return _fail("ladder to Deck 13 failed (y %.2f)" % _position().y)
	if not await _go(device, Vector2(-6.0, -133.85), 0.08, 6.0):
		return _fail("walk along Deck 13 plate stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing high riser ladder, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("take hold of high riser ladder failed")
	_move(device, 1.0)
	var on_crossover: bool = await _wait_until(func() -> bool: return _standing_above(154.5), 30.0)
	_move(device, 0.0)
	if not on_crossover:
		return _fail("high riser ladder to Deck 14 failed (y %.2f)" % _position().y)
	if not (await _walk_to(device, Vector2(-6.0, -130.0), 0.1, 4.0) and \
			await _walk_to(device, Vector2(-6.0, -128.2), 0.1, 4.0) and \
			await _walk_to(device, Vector2(-10.5, -128.2), 0.1, 6.0)):
		return _fail("walk down crossover steps onto Deck 14 runway stalled")
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != TOWER_ENTITY or \
			_position().y < 154.5:
		return _fail("not standing on Deck 14 (y %.2f)" % _position().y)
	await _pose("stack_deck14")
	return true

func _stack_upper(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var body := _watch_body()
	var started := int(_native().get_tick_index())
	var at_deck4 := _position().y
	if at_deck4 < 44.0:
		return _fail("upper stack scenario did not start at Deck 4 (y %.2f)" % at_deck4)
	if not await _stack_s2(device):
		return false
	var at_deck6 := _position().y
	if not await _stack_c2(device):
		return false
	var at_deck8 := _position().y
	if not await _stack_s3(device):
		return false
	var at_deck12 := _position().y
	if not await _stack_c3(device):
		return false
	var at_deck14 := _position().y
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck4_y=%.2f deck6_y=%.2f deck8_y=%.2f deck12_y=%.2f deck14_y=%.2f seconds=%.1f worst_body_step_m=%.3f" % [
		at_deck4, at_deck6, at_deck8, at_deck12, at_deck14, float(int(_native().get_tick_index()) - started) / 90.0, worst_step]
	return true
