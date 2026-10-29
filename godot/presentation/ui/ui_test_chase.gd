extends "res://presentation/ui/ui_test_stack.gd"
# The Matron (src/sim/matron.hpp), played and looked at on the 154 m deck.
#
# touch_matron: a sprint past her locker wakes her; the gauge climbs and the
# bolts pop while the player walks quietly on to the catwalk's east end; she
# bursts out and hunts; the player sprints over the rotten catwalk and she
# follows onto a plate that gives way under her: the one thing that kills her.
# touch_matron_ride: Stage A rigged quietly (UNHOOK, HOOK); out of the cage a
# sprint toward her locker wakes her; back in, GRAB the trip handle and wait
# for her. With her 3.8 m from the cage's mouth, step back: she steps in as it
# leaves and rides it up holding on, taking nobody. At the top a hop over the
# 1.09 m to the 176 ring and away along it: frail, she stays on the cages;
# back within her reach, the Turn takes her over the gap.
# touch_matron_line: woken, she is led round the deck to the 154 m line's
# ramp; as she charges, up onto the stand, a JUMP takes the bar and the ride
# down (braked) to 143 m; in the Turn she goes after the player down the bare
# cable, sparks off her forearms, and lets go over 143's south band.
# touch_matron_climb: woken, she bursts out as the player takes hold of the
# ladder from her deck to the 176 ring and climbs: frail she stands at its
# foot looking up; in the Turn she takes hold and comes up it after the
# player, the rungs ringing; at the top the player goes off along the ring
# and she comes over the ladder's head onto it.
# look_matron_*: renders only (not in CI): the locker straining, her bursting
# out, her hunt, from the player's eyes. look_bay: renders only, the bay on
# her deck from either end.
# touch_bay (src/sim/bay.hpp): woken, she is led round the deck and the
# player east over the bay's leaf; she does not follow over it. East of it, a
# dash along the band is heard across it: she drops into the bay and holds
# the leaf up on her palms, its motor and fan stopped. UNHOOK the chain off
# its cleat, a step onto the leaf by its hinge, HOOK it onto the leaf's eye:
# she lets go, the district starts again by itself, and the leaf carries the
# player out over its far half and back.
#
# Loose stock (src/sim/loose_stock.hpp), in the yard south of the start.
# touch_stock: round the beam stack to the drums on their pallet, PICK UP one,
# carry it 3 m and LET GO: it lies on the ground where it was set.
# look_stock_yard: renders only (not in CI): the containers, the steel, the
# pipe racks.
# touch_forklift: to the forklift parked north of the drums, up onto its
# floor, DRIVE; the stick forward creeps it in until its forks are under the
# first pallet, RAISE lifts the pallet with its drums, the stick back carries
# it off, LOWER sets it down, the forks back out, STEP OFF.
# touch_forklift_strain: aboard and driving, round onto the lane of the 5 t
# stillage of billets and in under it; RAISE held: the pump strains, the
# truck's back end comes up off the ground (heard, and felt in the hands);
# let go, the ram holds; LOWER held: the truck comes back down onto its rear
# wheels with a slam.
#
# The yard crane (src/sim/yard_crane.hpp), west of the stack.
# touch_yard_crane: from the start round to the crane's foot, TAKE REMOTE
# from its cabinet; CRANE takes the crane (Jump and Crouch read RAISE and
# LOWER, both hands hold the remote); the stick held right, the view north,
# sends the hook east; Jump held hoists it, Crouch held lets it down; WALK
# lets go and every drive stops.

const YARD_CRANE_MAST_X := -66.0
const YARD_CRANE_MAST_Z := -137.0
const MATRON_STATE_DORMANT := 0
const MATRON_STATE_STRAINING := 1
const MATRON_STATE_HUNTING := 3
const MATRON_STATE_GUARDING := 7
# The bay's west lip (src/sim/bay.hpp): she guards the way out west of it.
const BAY_WEST_X := -4.6
const MATRON_STATE_DEAD := 6
const MATRON_BAND_X := 21.5
const MATRON_NORTH_Z := -128.5
const MATRON_MID_Z := -150.0
const MATRON_DECK_Y := 154.0
const BAY_TRAP_HOLDING := 4
const BAY_SHACKLE := 2653


func _matron_state() -> Dictionary:
	return _native().get_matron_state()


func _crouch_on(device: int, on: bool) -> bool:
	if bool(_native().is_player_crouched()) == on:
		return true
	_press(&"crouch", 1)
	return await _wait_until(func() -> bool: return bool(_native().is_player_crouched()) == on, 1.5)


# A sprint toward the locker for a second or two (the touch stick pushed past
# its ring), then let go.
func _wake_matron(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	await _face(Vector2(10.0, -128.5) - Vector2(_position().x, _position().z))
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var woke: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 6.0)
	_move(device, 0.0)
	if not woke:
		return _fail("a sprint beside her locker did not wake her (at %s)" % str(_position()))
	return true


# Out of her locker she knows only what she last heard (src/sim/matron.cpp
# hear): the sprint that woke her, by the locker. A climber who crept off is
# not where she looks. Up off the crouch, a run up the band and back is
# heard, and she comes.
# Called again while she has not heard it (she may have given up on the
# locker and gone back to her post across the shaft, out of earshot of one
# run), up to eight times.
func _call_her_to_band(device: int) -> bool:
	return await _call_her_to(device, Vector2(MATRON_BAND_X, MATRON_MID_Z), Vector2(MATRON_BAND_X, MATRON_MID_Z + 4.0))


# The same call anywhere on her deck: off the crouch, a run from where the
# climber waits to `run_to` and back, again until her lead is there (up to
# eight times).
func _call_her_to(device: int, at: Vector2, run_to: Vector2) -> bool:
	await _crouch_on(device, false)
	for call in 8:
		if not (await _sprint_to(device, run_to, 0.5, 4.0) and await _go(device, at, 0.5, 8.0)):
			return false
		var m := _matron_state()
		var lead := Vector2(float(m["lead_x"]), float(m["lead_z"]))
		print("SCRAPERX_CHASE call=%d state=%d her=%s lead=%s at %s age=%.1f" % [call, int(m["state"]),
			str(m["position"]), str(m["lead_valid"]), str(lead), float(m["lead_age_s"])])
		if bool(m["lead_valid"]) and lead.distance_to(at) < 8.0:
			return true
	return true


func _matron_touch(device: int) -> bool:
	if not await _wake_matron(device):
		return false
	await _seconds(0.6)
	await _tilt(0.0)
	_main._yaw = atan2(-(11.0 - _position().x), -(-126.2 - _position().z))
	await _frames(3)
	await _pose("matron_straining")
	# Quietly on to the catwalk's east end: crouched, the locker rattling behind.
	if not await _crouch_on(device, true):
		return _fail("CROUCH did not crouch the climber")
	if not (await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0) and \
			await _go(device, Vector2(MATRON_BAND_X, MATRON_MID_Z), 0.5, 40.0)):
		return _fail("the crouched walk to the catwalk's east end stalled at %s" % str(_position()))
	var burst: bool = await _wait_until(func() -> bool: return bool(_matron_state()["released"]), 25.0)
	if not burst:
		return _fail("she never burst out of her locker (strain %.2f)" % float(_matron_state()["strain"]))
	if not (_main._matron_view.door_tore_off > 0 and _main._matron_view.bolts_shown_popped >= 3):
		return _fail("the locker's bolts and door did not show the burst (popped %d, door off %d)" % [
			_main._matron_view.bolts_shown_popped, _main._matron_view.door_tore_off])
	if not await _call_her_to_band(device):
		return _fail("the run up the band and back stalled at %s" % str(_position()))
	var coming: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		return int(m["state"]) == MATRON_STATE_HUNTING and Vector2(at.x, at.z).distance_to(Vector2(_position().x, _position().z)) < 16.0, 40.0)
	if not coming:
		return _fail("she did not come for the player at the catwalk's east end")
	await _face(Vector2(-1.0, 0.0))
	await _pose("matron_hunting")
	# The sprint over the catwalk, west; she follows on to it.
	await _crouch_on(device, false)
	await _face(Vector2(-1.0, 0.0))
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	# Across and off the catwalk onto the west band, then still: she is on the
	# catwalk behind, or she is not.
	var across: bool = await _wait_until(func() -> bool: return _position().x < -19.5, 8.0)
	_move(device, 0.0)
	if not across:
		return _fail("the sprint over the catwalk never reached the west band (at %s)" % str(_position()))
	var dead: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_DEAD, 14.0)
	var m := _matron_state()
	if not dead:
		return _fail("she did not follow over the catwalk and fall (state %d, plates down %d, she at %s, at %s)" % [
			int(m["state"]), int(m["planks_fallen"]), str(m["position"]), str(_position())])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died in the chase")
	if int(m["planks_fallen"]) < 1:
		return _fail("she died without a plate giving way (she was last at %s, at %s)" % [str(m["position"]), str(_position())])
	if not _matron_contacts_owned():
		return false
	_detail = "plates_down=%d deaths=%d steps_heard=%d" % [int(_matron_state()["planks_fallen"]),
		int(_native().get_death_count()), int(_main._audio.matron_cues.get(&"matron_step", 0))]
	return true


# S6: her figure drawn only on what the simulation has her standing on and
# holding (matron_view.gd _check_contacts). Fails the scenario when any frame
# drew a limb planted, or her body ridden, lifted or hung, on a contact the
# simulation does not have.
func _matron_contacts_owned() -> bool:
	var unowned := int(_main._matron_view.unowned_frames)
	if unowned > 0:
		return _fail("her figure was drawn on a contact the simulation does not have, %d frames" % unowned)
	return true


func _matron_ride_touch(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _rig_stage_a(device):
		return false
	if int(_matron_state()["state"]) != MATRON_STATE_DORMANT:
		return _fail("rigging the cage woke her (state %d)" % int(_matron_state()["state"]))
	# Out of the cage's mouth; a sprint toward her locker wakes her.
	for leg in [Vector2(-10.2, -130.6), Vector2(-10.0, -129.2)]:
		if not await _walk_to(device, leg, 0.2):
			return _fail("the walk out of the cage stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var woke: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 6.0)
	_move(device, 0.0)
	if not woke:
		return _fail("the sprint toward her locker did not wake her (at %s)" % str(_position()))
	# Back round the gantry's east post, in, and the handle in hand.
	for leg in [Vector2(-10.0, -128.4), Vector2(-10.0, -129.2), Vector2(-10.2, -130.6), Vector2(-11.05, -130.35)]:
		if not await _go(device, leg, 0.2, 8.0):
			return _fail("the walk back in to the trip handle stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing the trip handle, not GRAB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2004, 0.5):
		return _fail("GRAB did not put the trip handle in the hands")
	# Her coming for the cage; at 3.8 m from its mouth, step back. (4.5 m
	# before the bay's leaf put her line's last bend at its west end,
	# src/sim/bay.hpp: she now reaches the mouth a moment later.)
	var near: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		return int(m["state"]) == MATRON_STATE_HUNTING and Vector2(at.x, at.z).distance_to(Vector2(-10.5, -129.9)) < 3.8, 45.0)
	if not near:
		return _fail("she did not come for the cage (state %d, at %s)" % [int(_matron_state()["state"]), str(_matron_state()["position"])])
	# A gentle step back (the stick a third of the way): the catch opens as she
	# comes to the mouth.
	_move(device, -0.35)
	var tripped: bool = await _wait_until(func() -> bool: return not bool(_native().is_well_a_catch_latched()), 3.0)
	_move(device, 0.0)
	if not tripped:
		return _fail("stepping back with the handle never opened the catch")
	_act(device)
	await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)
	var deaths := int(_native().get_death_count())
	# The ride: she steps in as it leaves and holds on to the top, looked at
	# halfway up.
	var seen := {"clung": false, "shot": false}
	var arrived: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		var travel := float(_native().get_well_a_cage_travel())
		seen["clung"] = bool(seen["clung"]) or (bool(m["clinging"]) and travel > 2.0)
		return travel >= 21.95, 20.0)
	if not arrived:
		return _fail("the cage never reached the top (travel %.2f m)" % float(_native().get_well_a_cage_travel()))
	if not bool(seen["clung"]) or int(_native().get_death_count()) != deaths:
		return _fail("she did not ride up holding on (clung %s, deaths %d, she at %s)" % [
			str(seen["clung"]), int(_native().get_death_count()) - deaths, str(_matron_state()["position"])])
	var her: Vector3 = _matron_state()["position"]
	await _face(Vector2(her.x - _position().x, her.z - _position().z))
	await _pose("matron_ride_clinging")
	# At the top: to the cage's mouth, the side of it she is not clinging at
	# (it is 3 m wide), and over the gap to the ring.
	var hop_x := -9.9 if her.x < -10.5 else -11.0
	if not await _walk_to(device, Vector2(hop_x, -130.3), 0.15, 2.0):
		return _fail("the step to the cage's mouth at the top stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	var across: bool = await _leap(device, 0.6, func() -> bool:
		return bool(_ctx()["grounded"]) and _position().z > -128.8 and _position().y > 176.5, 2.0)
	if not across:
		return _fail("the hop over the gap to the ring missed (at %s)" % str(_position()))
	if not await _go(device, Vector2(6.0, -126.9), 0.5, 8.0):
		return _fail("the walk away along the ring stalled at %s" % str(_position()))
	# Frail, she stays on the cages: off them only by the Turn's leap, now
	# (her Turn ready as the player went) or once the player comes back within
	# her reach.
	var held := {"off": false}
	await _wait_until(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		held["off"] = bool(held["off"]) or not bool(m["alive"]) or (int(m["leaps"]) == 0 and (at.z > -129.9 or at.y < 176.5))
		return int(m["leaps"]) > 0, 6.0)
	if bool(held["off"]):
		return _fail("frail, she left the cages without a leap (at %s, alive %s)" % [
			str(_matron_state()["position"]), str(_matron_state()["alive"])])
	var stranded := int(_matron_state()["leaps"]) == 0
	if stranded:
		her = _matron_state()["position"]
		await _face(Vector2(her.x - _position().x, her.z - _position().z))
		await _pose("matron_ride_stranded")
		if not await _go(device, Vector2(-2.0, -126.9), 0.5, 8.0):
			return _fail("the walk back along the ring stalled at %s" % str(_position()))
	var leapt: bool = await _wait_until(func() -> bool: return int(_matron_state()["leaps"]) > 0, 25.0)
	if not leapt:
		return _fail("in reach on the ring, she never leapt the gap (state %d, turn %d, at %s)" % [
			int(_matron_state()["state"]), int(_matron_state()["turn"]), str(_matron_state()["position"])])
	var landed: bool = await _wait_until(func() -> bool:
		var at: Vector3 = _matron_state()["position"]
		return at.z > -128.9 and absf(at.y - 177.3) < 0.06, 3.0)
	if not landed:
		return _fail("she leapt but did not come down on the ring (at %s)" % str(_matron_state()["position"]))
	if not _matron_contacts_owned():
		return false
	_detail = "deaths_on_ride=%d stranded_6s=%s leaps=%d she_at=%s" % [int(_native().get_death_count()) - deaths,
		str(stranded), int(_matron_state()["leaps"]), str(_matron_state()["position"])]
	return true


# A sprint to `target`: the stick pushed past its ring toward it and held,
# the view kept on it, let go within `tolerance`.
func _sprint_to(device: int, target: Vector2, tolerance: float, budget: float) -> bool:
	var at := _position()
	await _face(target - Vector2(at.x, at.z))
	if not _stick_sprint():
		return false
	var waited := 0.0
	while waited < budget:
		at = _position()
		var to := target - Vector2(at.x, at.z)
		if to.length() <= tolerance:
			_move(device, 0.0)
			return true
		var turn := 4.0 * get_process_delta_time()
		_main._yaw = float(_main._yaw) + clampf(wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI), -turn, turn)
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move(device, 0.0)
	return false


# Until `condition` holds, jumping where the climber stands to be heard (a
# landing is loud) every 1.5 s, up to `budget` seconds.
func _wait_heard(condition: Callable, budget: float) -> bool:
	var waited := 0.0
	var next_jump := 0.0
	while waited < budget:
		if bool(condition.call()):
			return true
		# (Standing: a jump with a sprint still in the legs carries off an edge.)
		if waited >= next_jump and bool(_ctx()["grounded"]) and Vector2(_velocity().x, _velocity().z).length() < 0.5:
			_press(&"jump", 1)
			next_jump = waited + 1.5
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


func _matron_line_touch(device: int) -> bool:
	if not await _wake_matron(device):
		return false
	# Quietly west over the bay's leaf and down the west band to wait for her
	# there: the run round then comes to the line's ramp from the east, with
	# her behind on this side of the bay. (Run east over it, the player goes
	# unheard under its fan, src/sim/bay.hpp; or, heard or seen, she does not
	# cross after them: her trap.)
	if not await _crouch_on(device, true):
		return _fail("CROUCH did not crouch the climber")
	if not (await _go(device, Vector2(-MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 60.0) and \
			await _go(device, Vector2(-MATRON_BAND_X, -150.0), 0.5, 40.0)):
		return _fail("the crouched walk down the west band stalled at %s" % str(_position()))
	# Out of her locker she looks where the sprint woke her (she knows only
	# what she hears or sees, src/sim/matron.cpp hear). Called from the west
	# band by jumps (the landings heard), looking down it; away round the deck
	# once she comes, while still farther off than her Turn's reach
	# (kTurnTriggerM, 13 m), whose cooldown would outlast the run to the ramp.
	if not await _wait_until(func() -> bool: return bool(_matron_state()["released"]), 25.0):
		return _fail("she never burst out of her locker (strain %.2f)" % float(_matron_state()["strain"]))
	await _crouch_on(device, false)
	await _face(Vector2(0.0, -1.0))
	var coming: bool = await _wait_heard(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		return int(m["state"]) == MATRON_STATE_HUNTING and Vector2(at.x, at.z).distance_to(Vector2(_position().x, _position().z)) < 25.0, 60.0)
	if not coming:
		return _fail("she did not come for the player on the west band")
	await _crouch_on(device, false)
	# The long way round ahead of her, by the south and east bands back along
	# the north band to the line's ramp.
	for leg in [Vector2(-MATRON_BAND_X, -171.5), Vector2(MATRON_BAND_X, -171.5), Vector2(MATRON_BAND_X, MATRON_NORTH_Z),
			Vector2(1.6, MATRON_NORTH_Z)]:
		if not await _sprint_to(device, leg, 1.0, 12.0):
			return _fail("the sprint round the deck stalled at %s" % str(_position()))
		# She follows by ear: at each corner the climber waits for her to be
		# coming, but not at the last (she would be at the ramp before her charge).
		if leg.x < MATRON_BAND_X - 1.0 and leg.y > MATRON_NORTH_Z - 1.0:
			continue
		if not await _wait_heard(func() -> bool:
			var m := _matron_state()
			var at: Vector3 = m["position"]
			return int(m["state"]) == MATRON_STATE_HUNTING and \
				Vector2(at.x, at.z).distance_to(Vector2(_position().x, _position().z)) < 25.0, 20.0):
			var m := _matron_state()
			return _fail("she lost the climber at %s (state %d at %s, deaths %d %s)" % [str(_position()), int(m["state"]),
				str(m["position"]), int(_native().get_death_count()), str(_native().get_death_state())])
	if not await _walk_to(device, Vector2(2.3, -132.36), 0.3, 4.0, true):
		return _fail("the walk to the ramp's foot stalled at %s" % str(_position()))
	_move_dir(device, Vector2.ZERO)
	# A step short of the ramp's foot, wait for her charge (her Turn),
	# jumping to be heard, looking west along the rail (looking at it, the
	# swipe up takes hold of it); then up onto the stand and the jump for the
	# bar, with her coming up the ramp behind.
	await _face(Vector2(-1.0, 0.0))
	var charging: bool = await _wait_heard(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		return int(m["state"]) == MATRON_STATE_HUNTING and int(m["turn"]) != 0 and \
			Vector2(at.x, at.z).distance_to(Vector2(_position().x, _position().z)) < 14.0, 30.0)
	if not charging or int(_native().get_death_count()) != 0:
		return _fail("she did not come for the player at the line's ramp (deaths %d)" % int(_native().get_death_count()))
	if not await _walk_to(device, Vector2(6.0, -132.36), 0.15, 4.0, true) or _position().y < 155.0:
		return _fail("up the ramp onto the line's stand stalled at %s (she state %d at %s, turn %d)" % [str(_position()),
			int(_matron_state()["state"]), str(_matron_state()["position"]), int(_matron_state()["turn"])])
	_move_dir(device, Vector2.ZERO)
	await _face(Vector2(0.0, -1.0))
	_press(&"jump", 1)
	if not await _wait_until(func() -> bool: return bool(_native().get_zipline_state()["riding"]), 1.5):
		return _fail("the jump from the stand never took the trolley's bar (at %s)" % str(_position()))
	# Down the line, braked over the last stretch; she comes after.
	var line := int(_native().get_zipline_state()["line"])
	var length := float(_native().get_zipline_lines()[line]["length"])
	var seen := {"cable": false, "shot": false}
	var holding_back := false
	var stopped := false
	var waited := 0.0
	while waited < 16.0 and not stopped:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var zip: Dictionary = _native().get_zipline_state()
		if not bool(zip["riding"]):
			return _fail("the hands came off the bar mid-ride (at %s)" % str(_position()))
		var speed := absf(float(zip["speed_mps"]))
		# Braked to 6 m/s: slower, the buffer stops the trolley short of the
		# stop, over the shaft; at 6 it rides on to the stop over the band.
		var brake := float(zip["along_m"]) > length - 12.0 and speed > 6.0
		if brake and not holding_back:
			_stick_push(Vector2(0.0, 1.0))
		elif not brake and holding_back:
			_move_dir(device, Vector2.ZERO)
		holding_back = brake
		seen["cable"] = bool(seen["cable"]) or bool(_matron_state()["on_line"])
		# Into the buffer at the end, over the far band, before letting go.
		stopped = float(zip["along_m"]) > length - 1.5 and speed < 0.3
	_move_dir(device, Vector2.ZERO)
	if not stopped:
		return _fail("the ride never stopped at the bottom")
	await _frames(2)
	_press(&"drop", 1)
	if not await _wait_until(func() -> bool: return bool(_native().is_player_grounded()) and _position().y < 145.0, 2.5):
		return _fail("DROP at the bottom did not put the body down on 143 m (at %s)" % str(_position()))
	# Away along 143's south band, then a look back up the line at her
	# coming down it.
	if not await _sprint_to(device, Vector2(-12.0, -171.5), 1.0, 8.0):
		return _fail("away along 143's south band stalled at %s" % str(_position()))
	var on_cable: bool = await _wait_until(func() -> bool: return bool(_matron_state()["on_line"]), 4.0) or bool(seen["cable"])
	if not on_cable:
		return _fail("in the Turn she never went onto the cable (state %d, turn %d, at %s)" % [
			int(_matron_state()["state"]), int(_matron_state()["turn"]), str(_matron_state()["position"])])
	if bool(_matron_state()["on_line"]):
		var her: Vector3 = _matron_state()["position"]
		await _face(Vector2(her.x - _position().x, her.z - _position().z))
		await _tilt(0.25)
		await _pose("matron_line_cable")
		await _tilt(0.0)
	var down: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		return not bool(m["on_line"]) and bool(m["alive"]) and absf(float(m["floor_y"]) - 143.0) < 0.1, 30.0)
	if not down:
		return _fail("she never let go over 143's band (on line %s, at %s, level %.2f)" % [
			str(_matron_state()["on_line"]), str(_matron_state()["position"]), float(_matron_state()["floor_y"])])
	if not _matron_contacts_owned():
		return false
	_detail = "slides=%d she_at=%s deaths=%d" % [int(_matron_state()["cable_slides"]), str(_matron_state()["position"]),
		int(_native().get_death_count())]
	return true


func _bay_touch(device: int) -> bool:
	if not await _wake_matron(device):
		return false
	# Quietly to the catwalk's east end, out of her hearing. Out of her locker
	# she looks where the sprint that woke her was, finds nobody, and goes to
	# stand in the way out's mouth west of the bay (she knows only what she
	# hears or sees: src/sim/matron.cpp hear). Then quietly north up the east
	# band and west along the north band to stand east of the bay, across it
	# from her. She does not follow over it (heard or seen, her trap; or the
	# player unheard under its fan).
	if not await _crouch_on(device, true):
		return _fail("CROUCH did not crouch the climber")
	if not (await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0) and \
			await _go(device, Vector2(MATRON_BAND_X, MATRON_MID_Z), 0.5, 40.0)):
		return _fail("the crouched walk to the catwalk's east end stalled at %s" % str(_position()))
	var guarding: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		return bool(m["released"]) and int(m["state"]) == MATRON_STATE_GUARDING and at.x < BAY_WEST_X, 90.0)
	if not guarding:
		var m := _matron_state()
		return _fail("out, she never went to guard the way out west of the bay (state %d at %s, lead %s at (%.1f, %.1f))" % [
			int(m["state"]), str(m["position"]), str(m["lead_valid"]), float(m["lead_x"]), float(m["lead_z"])])
	if not (await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0) and \
			await _go(device, Vector2(3.0, MATRON_NORTH_Z), 0.3, 40.0)):
		var m := _matron_state()
		return _fail("the crouched walk to east of the bay stalled at %s (she state %d at %s)" % [str(_position()),
			int(m["state"]), str(m["position"])])
	await _crouch_on(device, false)
	if not await _walk_to(device, Vector2(3.0, MATRON_NORTH_Z), 0.3, 4.0, true):
		return _fail("the walk to east of the bay stalled at %s" % str(_position()))
	# A dash along the band and back, heard across the bay, until she is down
	# in it holding the leaf (three dashes at most).
	var holding := false
	for dash in 3:
		if not (await _sprint_to(device, Vector2(8.0, MATRON_NORTH_Z), 0.4, 6.0) and \
				await _walk_to(device, Vector2(3.0, MATRON_NORTH_Z), 0.3, 6.0, true)):
			var m := _matron_state()
			return _fail("the dash along the band stalled at %s (she state %d stage %d at %s, trap %d)" % [
				str(_position()), int(m["state"]), int(m["stage"]), str(m["position"]), int(m.get("trap", 0))])
		holding = await _wait_until(func() -> bool:
			var m := _matron_state()
			return int(m["trap"]) == BAY_TRAP_HOLDING and bool(m["bracing"]), 25.0)
		if holding:
			break
	if not holding:
		return _fail("she never went down into the bay to hold the leaf (state %d, trap %d, at %s)" % [
			int(_matron_state()["state"]), int(_matron_state()["trap"]), str(_matron_state()["position"])])
	var held: Dictionary = _native().get_bay_state()
	if bool(held["leaf_running"]) or not bool(held["hands"]) or absf(float(held["leaf_angle"])) > 0.02:
		return _fail("her hands in it did not stop its motor with the leaf level on her palms (%s)" % str(held))
	var her: Vector3 = _matron_state()["position"]
	await _face(Vector2(her.x - _position().x, her.z - _position().z))
	await _tilt(-0.35)
	await _pose("bay_holding")
	await _tilt(0.0)
	# The chain off its cleat on the rail's end post.
	if not await _walk_to(device, Vector2(0.4, -129.5), 0.08, 6.0, true):
		return _fail("the walk to the cleat stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"unhook", "UNHOOK", "CHAIN"):
		return _fail("Action read '%s' at the cleat, not UNHOOK CHAIN" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == BAY_SHACKLE, 0.8):
		return _fail("UNHOOK did not put the chain's shackle in the hands")
	# A step onto the leaf by its hinge (her hands hold that), the eye in reach.
	if not await _walk_to(device, Vector2(-0.6, -128.1), 0.08, 6.0, true):
		return _fail("the step onto the leaf by its hinge stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO THE WALKWAY'S EYE"):
		return _fail("Action read '%s' at the leaf's eye, not HOOK" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_bay_state()["hooked"]), 0.8):
		return _fail("HOOK did not make the chain fast on the leaf's eye")
	# The chain has it: she lets go; the district starts again by itself.
	var restarted: bool = await _wait_until(func() -> bool:
		var b: Dictionary = _native().get_bay_state()
		return not bool(_matron_state()["bracing"]) and int(b["restarts"]) > 0 and bool(b["leaf_open"]), 30.0)
	if not restarted:
		return _fail("hooked, she did not let go or the district did not start again (%s)" % str(_native().get_bay_state()))
	await _pose("bay_chained")
	# Out over the leaf's far half, where her hands could not have held it
	# with the player on it, and back.
	for leg in [Vector2(-3.0, MATRON_NORTH_Z), Vector2(0.6, MATRON_NORTH_Z)]:
		if not await _walk_to(device, leg, 0.2, 8.0, true):
			return _fail("over the leaf stalled at %s" % str(_position()))
		if _position().y < MATRON_DECK_Y:
			return _fail("the hooked leaf went down under the player (at %s)" % str(_position()))
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died at the bay")
	var cues: Dictionary = _main._audio.bay_cues
	if int(cues.get(&"trip", 0)) < 1 or int(cues.get(&"hook", 0)) < 1 or int(cues.get(&"restart", 0)) < 1:
		return _fail("the bay's trip, hook and restart were not all heard (%s)" % str(cues))
	if not _matron_contacts_owned():
		return false
	_detail = "support_n=%.0f trips=%d hooks=%d restarts=%d deaths=%d" % [float(held["support_n"]),
		int(cues.get(&"trip", 0)), int(cues.get(&"hook", 0)), int(cues.get(&"restart", 0)),
		int(_native().get_death_count())]
	return true


func _matron_climb_touch(device: int) -> bool:
	if not await _wake_matron(device):
		return false
	# To the ladder's foot, round its west rail, facing it.
	if not (await _walk_to(device, Vector2(3.5, -128.2), 0.15, 8.0, true) and \
			await _walk_to(device, Vector2(3.5, -129.8), 0.15, 4.0, true) and \
			await _walk_to(device, Vector2(5.0, -129.6), 0.08, 4.0, true)):
		return _fail("the walk to the ladder's foot stalled at %s" % str(_position()))
	_move_dir(device, Vector2.ZERO)
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s' facing the ladder, not CLIMB HOLD" % _action_label())
	# As she bursts out of her locker, 7 m off, up the ladder.
	if not await _wait_until(func() -> bool: return bool(_matron_state()["released"]), 30.0):
		return _fail("she never burst out of her locker (strain %.2f)" % float(_matron_state()["strain"]))
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the ladder")
	_move(device, 1.0)
	# Frail, she comes to its foot and stands looking up; in the Turn she
	# takes hold.
	var at_foot := 0.0
	var waited := 0.0
	var holding := false
	while waited < 25.0 and not holding:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var m := _matron_state()
		var her: Vector3 = m["position"]
		holding = bool(m["climbing"])
		if not holding and Vector2(her.x, her.z).distance_to(Vector2(5.0, -129.55)) < 0.35:
			at_foot += get_process_delta_time()
	if not holding:
		return _fail("in the Turn she never took hold of the ladder (state %d, turn %d, at %s, %.1f s at its foot)" % [
			int(_matron_state()["state"]), int(_matron_state()["turn"]), str(_matron_state()["position"]), at_foot])
	if at_foot < 1.0:
		return _fail("she took hold without standing frail at the ladder's foot first (%.1f s)" % at_foot)
	# A look down the ladder at her coming up it, the climb going on.
	var rungs_before := int(_main._audio.matron_cues.get(&"clang", 0))
	await _seconds(2.5)
	await _tilt(-1.25)
	await _pose("matron_climb_below")
	await _tilt(0.0)
	var on_ring: bool = await _wait_until(func() -> bool: return _standing_above(176.5), 40.0)
	_move(device, 0.0)
	if not on_ring or int(_native().get_death_count()) != 0:
		return _fail("the climb did not top out onto the 176 ring (y %.2f, deaths %d)" % [_position().y,
			int(_native().get_death_count())])
	var rungs_heard := int(_main._audio.matron_cues.get(&"clang", 0)) - rungs_before
	if rungs_heard < 3:
		return _fail("her climb was not heard on the rungs (%d)" % rungs_heard)
	# Off east along the ring, then a look back at the ladder's head.
	if not await _sprint_to(device, Vector2(16.0, -126.91), 1.0, 8.0):
		return _fail("away east along the ring stalled at %s" % str(_position()))
	var over: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		var her: Vector3 = m["position"]
		return not bool(m["climbing"]) and bool(m["alive"]) and absf(float(m["floor_y"]) - 176.25) < 0.1 and \
			absf(her.y - 177.3) < 0.06, 60.0)
	if not over:
		return _fail("she never came over the ladder's head onto the ring (climbing %s, at %s, level %.2f)" % [
			str(_matron_state()["climbing"]), str(_matron_state()["position"]), float(_matron_state()["floor_y"])])
	var her: Vector3 = _matron_state()["position"]
	await _face(Vector2(her.x - _position().x, her.z - _position().z))
	await _pose("matron_climb_over")
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died")
	if not _matron_contacts_owned():
		return false
	_detail = "climbs=%d at_foot_s=%.1f rungs_heard=%d she_at=%s" % [int(_matron_state()["climbs"]), at_foot, rungs_heard,
		str(her)]
	return true


# Renders only: the locker straining at three points of the climb, then her
# bursting out and the hunt, from where the player stands.
func _look_matron(stage: String) -> bool:
	var device := InputRouter.Device.TOUCH
	_main._sky_cycle.set_hour(20.2)
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if stage == "stock":
		_main._sky_cycle.set_hour(17.5)
		await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0)
		await _go(device, Vector2(22.6, -142.0), 0.5, 20.0)
		await _look_at_point(Vector3(19.4, MATRON_DECK_Y + 0.3, -147.4))
		await _pose("matron_stock_dolly")
		await _go(device, Vector2(MATRON_BAND_X, -165.0), 0.5, 20.0)
		await _go(device, Vector2(12.0, -170.5), 0.5, 20.0)
		await _look_at_point(Vector3(15.0, MATRON_DECK_Y + 0.8, -174.6))
		await _pose("matron_stock_stack")
		return true
	if stage == "bay":
		# The bay on her deck (src/sim/bay.hpp) by day: from its west end
		# over the leaf, and from its east end back over it and down into it.
		_main._sky_cycle.set_hour(15.0)
		await _go(device, Vector2(-6.2, MATRON_NORTH_Z), 0.3, 20.0)
		await _look_at_point(Vector3(-1.5, MATRON_DECK_Y - 0.6, -128.0))
		await _pose("bay_west")
		for leg in [Vector2(-4.6, MATRON_NORTH_Z), Vector2(0.4, MATRON_NORTH_Z), Vector2(0.9, -130.6)]:
			await _go(device, leg, 0.3, 20.0)
		await _look_at_point(Vector3(-3.0, MATRON_DECK_Y - 1.2, -129.6))
		await _pose("bay_east")
		return true
	if stage == "catwalk":
		await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0)
		await _go(device, Vector2(19.0, MATRON_MID_Z), 0.5, 20.0)
		await _look_at_point(Vector3(0.0, MATRON_DECK_Y, MATRON_MID_Z))
		await _pose("matron_catwalk")
		return true
	# Wake her from a few metres off, then look at the locker.
	await _go(device, Vector2(8.0, -131.0), 0.4, 20.0)
	await _face(Vector2(10.5, -126.2) - Vector2(_position().x, _position().z))
	if not _stick_sprint():
		return _fail("no sprint latched")
	await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 6.0)
	_move(device, 0.0)
	await _seconds(0.3)
	await _go(device, Vector2(7.6, -130.5), 0.3, 10.0)
	for mark in [[0.25, "a"], [0.6, "b"], [0.92, "c"]]:
		await _wait_until(func() -> bool: return float(_matron_state()["strain"]) >= float(mark[0]), 20.0)
		await _look_at_point(Vector3(10.6, MATRON_DECK_Y + 1.3, -126.4))
		await _pose("matron_locker_%s" % mark[1])
	if stage == "locker":
		return true
	await _wait_until(func() -> bool: return bool(_matron_state()["released"]), 8.0)
	await _seconds(0.25)
	await _look_at_point(Vector3(11.0, MATRON_DECK_Y + 1.2, -126.8))
	await _pose("matron_burst")
	await _seconds(0.9)
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_emerging")
	await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_HUNTING, 6.0)
	# Back away so she is seen coming over the deck.
	await _go(device, Vector2(2.0, -146.0), 0.5, 10.0)
	await _seconds(1.2)
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_hunt_a")
	await _seconds(0.5)
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_hunt_b")
	return true


# Renders only: her figure in each of its poses, a few metres in front of the
# player, from made-up data (the native's frames are ignored meanwhile).
func _look_matron_gallery() -> bool:
	_main._sky_cycle.set_hour(20.2)
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var view: Node3D = _main._matron_view
	view.previewing = true
	var here := _position()
	var forward := Vector3(-sin(float(_main._yaw)), 0.0, -cos(float(_main._yaw)))
	var spot := here + forward * 3.6
	var base: Dictionary = _native().get_matron_state()
	var yaw := atan2(-forward.x, -forward.z)
	var poses := [
		["shuffle", {"state": 5, "speed": 1.0, "stride": 0.6, "turn": 0}],
		["search", {"state": 4, "speed": 0.0, "stride": 0.0, "turn": 0}],
		["hunt_a", {"state": 3, "speed": 6.8, "stride": 0.5, "turn": 0, "sees_player": true}],
		["hunt_b", {"state": 3, "speed": 6.8, "stride": 1.2, "turn": 0, "sees_player": false}],
		["turn_windup", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 1}],
		["turn_lunge_a", {"state": 3, "speed": 9.0, "stride": 0.3, "turn": 2}],
		["turn_lunge_b", {"state": 3, "speed": 9.0, "stride": 0.9, "turn": 2}],
		["emerge", {"state": 2, "speed": 0.0, "stride": 0.0, "turn": 0}],
		["guard", {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0}],
		["peer", {"state": 8, "speed": 0.0, "stride": 0.0, "turn": 0}],
		["cling", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 0, "clinging": true}],
		["unrig", {"state": 9, "speed": 0.0, "stride": 0.0, "turn": 0}],
		["on_line", {"state": 3, "speed": 4.0, "stride": 0.0, "turn": 0, "clinging": true, "on_line": true}],
		["climb", {"state": 3, "speed": 0.0, "stride": 0.45, "turn": 0, "climbing": true}],
		# Rising (0.15 m a frame): the vault.
		["turn_vault", {"state": 3, "speed": 4.0, "stride": 0.3, "turn": 2}, 0.15],
		# The exposure's work on her look, one way: the attendant in her white
		# tunic; blistered and stained; grey and filthy.
		["tunic", {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "exposure": 0.0}],
		["blistered", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 0, "sees_player": true, "exposure": 0.5}],
		["grey", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 0, "sees_player": true, "exposure": 1.0}],
	]
	var t := 0.0
	for entry in poses:
		var data: Dictionary = base.duplicate()
		data["alive"] = true
		data["released"] = true
		data["position"] = Vector3(spot.x, here.y - 0.9 + 1.05, spot.z)
		data["yaw"] = yaw
		for key in entry[1]:
			data[key] = entry[1][key]
		# Settle the pose a few frames (the state's own clock runs).
		var rise := float(entry[2]) if entry.size() > 2 else 0.0
		for k in 6:
			t += 0.04 + 0.013 * float(k)
			data["position"] = data["position"] + Vector3(0.0, rise, 0.0)
			view.preview(data, 0.04 + 0.013 * float(k), _main._camera.global_position, here)
			await _frames(1)
		await _look_at_point(view.head_world_position() - Vector3(0.0, 0.5, 0.0))
		await _pose("matron_pose_%s" % entry[0])
		if entry[0] in ["shuffle", "hunt_a", "turn_windup", "turn_lunge_a", "tunic", "blistered", "grey"]:
			# Close on the face: she stands 1.3 m from the player's eyes.
			var near := here + forward * 1.3
			data["position"] = Vector3(near.x, here.y - 0.9 + 1.05, near.z)
			for k in 4:
				view.preview(data, 0.04, _main._camera.global_position, here)
				await _frames(1)
			await _look_at_point(view.head_world_position())
			await _pose("matron_face_%s" % entry[0])
	view.previewing = false
	return true


# The first oil drum on the drums' pallet: [entity, kit body].
func _first_drum() -> Array:
	var stock: PackedInt32Array = _native().get_loose_stock()
	var piece := 0
	for i in range(0, stock.size() - 2, 3):
		if stock[i + 1] == 3:
			return [5000 + piece, stock[i]]
		piece += 1
	return [0, -1]


func _stock_touch(device: int) -> bool:
	var drum: Array = _first_drum()
	if int(drum[1]) < 0:
		return _fail("no drum in the loose stock")
	var at: Vector3 = _native().get_kit_body_transform(int(drum[1])).origin
	# Round the east end of the beam stack, between it and the plate.
	for target in [Vector2(30.0, -10.0), Vector2(43.5, 6.0), Vector2(43.5, 21.0),
			Vector2(at.x, at.z - 1.6)]:
		if not await _go(device, target, 0.3, 30.0):
			return _fail("the walk to the drums stopped short of %s" % str(target))
	if not await _go(device, Vector2(at.x, at.z - 0.75), 0.1, 6.0):
		return _fail("the step up to the drum")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP", "OIL DRUM"):
		return _fail("PICK UP OIL DRUM was not offered at the drum")
	_act(device)
	if not await _wait_until(func() -> bool:
			return int(_native().get_carrying_entity_id()) == int(drum[0]), 1.0):
		return _fail("the drum is not in the hands")
	if not await _go(device, Vector2(at.x, at.z - 3.75), 0.2, 8.0):
		return _fail("the walk back with the drum")
	await _seconds(0.5)
	await _pose("stock_drum_carried")
	# The hits heard: none from the drum in the hands; set down, it lands
	# hollow (ImpactSound.Hollow, 1).
	var hollow_before := int(_main._audio.impacts_heard.get(1, 0))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the drum down")
	await _seconds(3.0)
	var hollow := int(_main._audio.impacts_heard.get(1, 0)) - hollow_before
	print("SCRAPERX_UITEST stock: %d hollow hits heard as the drum landed" % hollow)
	if hollow < 1:
		return _fail("the drum set down was not heard landing")
	var now: Vector3 = _native().get_kit_body_transform(int(drum[1])).origin
	var carried := Vector2(now.x - at.x, now.z - at.z).length()
	print("SCRAPERX_UITEST stock: the drum carried %.2f m, at y %.2f" % [carried, now.y])
	if carried < 2.0 or now.y >= at.y:
		return _fail("the drum was not set down off its pallet, on the ground")
	await _look_at_point(now)
	await _pose("stock_drum_set_down")
	return true


func _look_stock_yard() -> bool:
	_main._sky_cycle.set_hour(14.0)
	var device := InputRouter.Device.TOUCH
	await _go(device, Vector2(-24.0, -6.0), 0.4, 20.0)
	await _look_at_point(Vector3(-44.0, 1.6, 12.0))
	await _seconds(1.0)
	await _pose("stock_containers")
	await _go(device, Vector2(30.0, -2.0), 0.4, 40.0)
	await _look_at_point(Vector3(40.0, 0.5, 14.0))
	await _seconds(1.0)
	await _pose("stock_steel")
	await _go(device, Vector2(43.5, 18.0), 0.4, 30.0)
	await _look_at_point(Vector3(50.0, 0.4, 26.5))
	await _seconds(1.0)
	await _pose("stock_pipes_drums")
	return true


# look_finishes: renders only (not in CI): close up, the parts that are smooth
# (the forklift's propane tank and its steel, a drum, gas bottles, a toolbox,
# a galvanized deck and its rails, the copper pipe up the stack's face) and
# rough ones beside them (a rusted beam, the stack's rusted frame). The view
# is held at each shot, not walked there: the whole world is built from the
# start. The HUD is hidden.
const FINISH_STOCK_DRUM := 3
const FINISH_STOCK_BEAM := 6
const FINISH_STOCK_BOTTLE := 14
const FINISH_STOCK_TOOLBOX := 16

# Holds the camera at a pose and a field of view after the view's own
# update every frame (a later process priority than the scene's).
class HeldView extends Node:
	var camera: Camera3D
	var pose := Transform3D()
	var fov := 40.0

	func _init(view: Camera3D) -> void:
		camera = view
		process_priority = 1000

	func _process(_delta: float) -> void:
		camera.global_transform = pose
		camera.fov = fov


func _look_finishes() -> bool:
	_main._sky_cycle.set_hour(14.0)
	var truck: Transform3D = _native().get_kit_body_transform(int(_native().get_kit_body_index(2982)))
	var shots := [
		["finish_propane_tank", truck * Vector3(-0.85, 1.55, -2.55), truck * Vector3(0.0, 1.13, -1.4), 30.0],
		["finish_forklift_back", truck * Vector3(2.6, 2.1, -3.8), truck * Vector3(0.0, 0.9, -0.9), 50.0],
		# The stack's south face from the yard: its decks, rails and rusted
		# edge beams.
		["finish_stack", Vector3(14.0, 4.0, -98.0), Vector3(0.0, 14.0, -124.0), 50.0],
		# The verdigris copper pipe up the face, west of the middle.
		["finish_copper_pipe", Vector3(-19.9, 3.0, -119.9), Vector3(-21.5, 5.0, -123.1), 35.0],
	]
	var stock: PackedInt32Array = _native().get_loose_stock()
	var found := {}
	for i in range(0, stock.size() - 2, 3):
		var at: Vector3 = (_native().get_kit_body_transform(stock[i]) as Transform3D).origin
		var kind := stock[i + 1]
		if kind == FINISH_STOCK_DRUM and at.y < 2.0 and not found.has("drum"):
			found["drum"] = at
			shots.append(["finish_drum", at + Vector3(-1.4, 1.3, -1.6), at + Vector3(0.0, 0.2, 0.0), 32.0])
		elif kind == FINISH_STOCK_BEAM and stock[i + 2] % 2 == 1 and at.y < 3.0 and not found.has("beam"):
			found["beam"] = at
			shots.append(["finish_rust_beam", at + Vector3(1.3, 1.9, 1.3), at, 32.0])
		elif kind == FINISH_STOCK_BOTTLE and not found.has("bottle"):
			# Stood against a ring's edge: seen from the band's middle.
			found["bottle"] = at
			var from := at
			if absf(at.x) > absf(at.z + 150.0):
				from.x = signf(at.x) * 21.5
			else:
				from.z = -150.0 + signf(at.z + 150.0) * 21.5
			shots.append(["finish_gas_bottles", from + Vector3(0.0, 1.1, 0.0), at + Vector3(0.0, 0.1, 0.0), 26.0])
			shots.append(["finish_galvanized_deck", from + Vector3(0.0, 1.7, 0.0), at + Vector3(0.0, -0.4, 0.0), 60.0])
		elif kind == FINISH_STOCK_TOOLBOX and at.y > 150.0 and at.y < 160.0 and not found.has("toolbox"):
			found["toolbox"] = at
			shots.append(["finish_toolbox", at + Vector3(0.7, 1.0, -1.5), at, 30.0])
	var hud := _main.get_node("HUD") as CanvasLayer
	hud.visible = false
	var held := HeldView.new(_main._camera)
	add_child(held)
	for shot in shots:
		held.pose = Transform3D(Basis(), shot[1]).looking_at(shot[2], Vector3.UP)
		held.fov = shot[3]
		await _frames(6)
		await _pose(shot[0])
	held.queue_free()
	hud.visible = true
	_detail = "shots=%d %s" % [shots.size(), str(found)]
	return true


# look_night_lamps: renders only (not in CI): the lamps at night, each kind
# near (the stack's lamps low, middle and high; the yard's sodium floods; the
# tower foot's floods) and the stack's south face whole from the yard, to see
# which lamps light the steel around them and whether any hangs on nothing.
func _look_night_lamps() -> bool:
	_main._sky_cycle.set_hour(23.5)
	var by_kind := {}
	for light in (_main._light_rig as Node3D).get_children():
		if not (light is OmniLight3D):
			continue
		for kind in ["StackLamp", "SodiumFlood", "TowerFootFlood"]:
			if String(light.name).contains(kind):
				if not by_kind.has(kind):
					by_kind[kind] = []
				by_kind[kind].append((light as Node3D).global_position)
	var shots := []
	for kind in by_kind:
		var at: Array = by_kind[kind]
		at.sort_custom(func(a: Vector3, b: Vector3) -> bool: return a.y < b.y)
		for i in [0, at.size() / 2, at.size() - 1]:
			var lamp: Vector3 = at[i]
			# Out from the tower's middle (or the mast), a little below, looking
			# at the lamp and the floor under it.
			var out := Vector3(lamp.x, 0.0, lamp.z + 150.0)
			out = out.normalized() if out.length() > 1.0 else Vector3.BACK
			var far := 12.0 if kind == "SodiumFlood" else 6.0
			shots.append(["night_%s_%d" % [kind.to_snake_case(), i], lamp + out * far + Vector3(0.0, -0.6, 0.0),
				lamp + Vector3(0.0, -1.6, 0.0), 60.0])
	shots.append(["night_stack_face", Vector3(14.0, 4.0, -98.0), Vector3(0.0, 14.0, -124.0), 50.0])
	shots.append(["night_stack_high", Vector3(30.0, 60.0, -95.0), Vector3(0.0, 90.0, -150.0), 55.0])
	var hud := _main.get_node("HUD") as CanvasLayer
	hud.visible = false
	var held := HeldView.new(_main._camera)
	add_child(held)
	for shot in shots:
		held.pose = Transform3D(Basis(), shot[1]).looking_at(shot[2], Vector3.UP)
		held.fov = shot[3]
		await _frames(8)
		await _pose(shot[0])
	held.queue_free()
	hud.visible = true
	_detail = "shots=%d lamps=%s" % [shots.size(), str(by_kind.keys().map(func(k): return "%s:%d" % [k, by_kind[k].size()]))]
	return true


# look_tower_stock: renders only (not in CI): the lived-in spots of stock up
# the tower (LooseStock::dress_tower), from deck 2's south band and from the
# 176 ring: to the nearest spot on the same deck, and looked at.
func _look_tower_stock(pose: String) -> bool:
	_main._sky_cycle.set_hour(15.5)
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 3.0)
	var at := _position()
	var nearest := Vector3.INF
	var stock: PackedInt32Array = _native().get_loose_stock()
	for i in range(0, stock.size() - 2, 3):
		var p: Vector3 = (_native().get_kit_body_transform(stock[i]) as Transform3D).origin
		if absf(p.y - at.y) < 2.0 and Vector2(p.x - at.x, p.z - at.z).length() < Vector2(nearest.x - at.x, nearest.z - at.z).length():
			nearest = p
	if nearest == Vector3.INF:
		return _fail("no stock on this deck")
	var toward := Vector2(nearest.x - at.x, nearest.z - at.z)
	var stand := Vector2(nearest.x, nearest.z) - toward.normalized() * minf(5.0, toward.length())
	await _go(InputRouter.Device.TOUCH, stand, 0.5, 20.0)
	await _look_at_point(nearest + Vector3(0.0, 0.3, 0.0))
	await _seconds(1.0)
	await _pose(pose)
	_detail = "spot=%s" % str(nearest)
	return true


# look_yard_crane: renders only (not in CI): the yard crane west of the
# stack, from the game's start; from its laydown, the jib over it; and its
# three docks on the stack's west face.
func _look_yard_crane() -> bool:
	_main._sky_cycle.set_hour(15.0)
	var device := InputRouter.Device.TOUCH
	await _seconds(0.5)
	await _look_at_point(Vector3(YARD_CRANE_MAST_X, 40.0, YARD_CRANE_MAST_Z))
	await _seconds(1.0)
	await _pose("yard_crane_from_start")
	for target in [Vector2(-25.0, -40.0), Vector2(-50.0, -100.0), Vector2(-62.0, -104.0)]:
		await _go(device, target, 0.5, 50.0)
	await _look_at_point(Vector3(YARD_CRANE_MAST_X, 52.0, YARD_CRANE_MAST_Z))
	await _seconds(1.0)
	await _pose("yard_crane_laydown")
	await _look_at_point(Vector3(-30.0, 26.0, -137.0))
	await _seconds(1.0)
	await _pose("yard_crane_docks")
	return true


# The drums' first pallet: [entity, kit body].
func _first_pallet() -> Array:
	var stock: PackedInt32Array = _native().get_loose_stock()
	var piece := 0
	for i in range(0, stock.size() - 2, 3):
		if stock[i + 1] == 4:
			return [5000 + piece, stock[i]]
		piece += 1
	return [0, -1]


# From the start: round the beam stack, up onto the forklift's floor at its
# wheel, facing its nose, DRIVE.
func _board_forklift(device: int) -> bool:
	for target in [Vector2(30.0, 10.0), Vector2(36.4, 16.35)]:
		if not await _go(device, target, 0.3, 30.0):
			return _fail("the walk to the forklift stopped short of %s" % str(target))
	if not await _go(device, Vector2(38.0, 16.35), 0.12, 8.0):
		return _fail("up onto the forklift's floor")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"drive", "DRIVE", "THE FORKLIFT"):
		return _fail("DRIVE was not offered at the forklift's wheel")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_forklift_state()["driving"]), 1.0):
		return _fail("DRIVE did not take the controls")
	if _main._touch.button_label(&"jump") != "RAISE" or _main._touch.button_label(&"crouch") != "LOWER":
		return _fail("driving, Jump and Crouch do not read RAISE and LOWER")
	return true


func _forklift_touch(device: int) -> bool:
	var pallet: Array = _first_pallet()
	if int(pallet[1]) < 0:
		return _fail("no pallet in the loose stock")
	var truck := int(_native().get_kit_body_index(2982))
	var before: Vector3 = _native().get_kit_body_transform(int(pallet[1])).origin
	if not await _board_forklift(device):
		return false
	# In until the forks are under the pallet's deck, the carriage short of it.
	_move_dir(device, Vector2(0.0, 0.25))
	var under := await _wait_until(func() -> bool:
		return (_native().get_kit_body_transform(truck) as Transform3D).origin.z >= before.z - 1.75, 20.0)
	_move_dir(device, Vector2.ZERO)
	if not under:
		return _fail("the forklift did not drive in to the pallet")
	await _seconds(1.5)
	_touch(2, _center(&"jump"), true)
	await _seconds(1.0)
	_touch(2, _center(&"jump"), false)
	await _seconds(0.5)
	var lifted: Vector3 = _native().get_kit_body_transform(int(pallet[1])).origin
	if lifted.y < 0.3:
		return _fail("RAISE did not lift the pallet (at y %.2f)" % lifted.y)
	await _pose("forklift_pallet_raised")
	# Back with it until it has come 3.5 m off its place.
	_move_dir(device, Vector2(0.0, -0.4))
	var off := await _wait_until(func() -> bool:
		return (_native().get_kit_body_transform(int(pallet[1])) as Transform3D).origin.z <= before.z - 3.5, 10.0)
	_move_dir(device, Vector2.ZERO)
	if not off:
		return _fail("the stick back did not carry the pallet off")
	await _seconds(1.5)
	_touch(3, _center(&"crouch"), true)
	await _seconds(1.5)
	_touch(3, _center(&"crouch"), false)
	_move_dir(device, Vector2(0.0, -0.4))
	await _seconds(1.5)
	_move_dir(device, Vector2.ZERO)
	await _seconds(1.5)
	var set_down: Vector3 = _native().get_kit_body_transform(int(pallet[1])).origin
	print("SCRAPERX_UITEST forklift: the pallet carried %.2f m, set down at y %.2f" % [before.z - set_down.z, set_down.y])
	if before.z - set_down.z < 3.0 or set_down.y > 0.2:
		return _fail("the pallet was not carried off and set down")
	# Heard from its parts as the native ran them: the engine while driven, the
	# tires rolling, the mast's chains as the forks rose and fell, and the
	# reverse alarm backing off with the pallet.
	var heard: Dictionary = _main._audio.forklift_heard
	print("SCRAPERX_UITEST forklift: heard %s" % str(heard))
	for part in [&"engine", &"tires", &"chain", &"beep"]:
		if int(heard.get(part, 0)) < 1:
			return _fail("the forklift's %s was not heard (%s)" % [part, str(heard)])
	if not await _offered(&"drive", "STEP OFF"):
		return _fail("STEP OFF was not offered while driving")
	_act(device)
	if not await _wait_until(func() -> bool: return not bool(_native().get_forklift_state()["driving"]), 1.0):
		return _fail("STEP OFF did not let go of the controls")
	await _look_at_point(set_down)
	await _pose("forklift_pallet_set_down")
	return true


func _yard_crane_touch(device: int) -> bool:
	# East of the laydown's plate stack on its sleepers (the native test's route).
	for target in [Vector2(-25.0, -40.0), Vector2(-50.0, -100.0), Vector2(-50.0, -125.0)]:
		if not await _go(device, target, 0.3, 50.0):
			return _fail("the walk to the crane stopped short of %s" % str(target))
	if not await _go(device, Vector2(YARD_CRANE_MAST_X + 4.2, YARD_CRANE_MAST_Z + 5.6), 0.12, 20.0):
		return _fail("the walk to the remote's cabinet stopped short")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"crane_take", "TAKE REMOTE", "THE YARD CRANE"):
		return _fail("TAKE REMOTE was not offered at the cabinet")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["has_remote"]), 1.0):
		return _fail("TAKE REMOTE did not take the remote")
	await _seconds(0.4)
	if not _main._touch.is_button_shown(&"crane") or _main._touch.button_label(&"crane") != "CRANE":
		return _fail("with the remote, its CRANE button is not shown")
	_tap(4, _center(&"crane"))
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("CRANE did not take the crane")
	await _seconds(0.4)
	if _main._touch.button_label(&"jump") != "RAISE" or _main._touch.button_label(&"crouch") != "LOWER" or \
			_main._touch.button_label(&"crane") != "WALK":
		return _fail("working the crane, Jump, Crouch and the remote do not read RAISE, LOWER and WALK")
	var remote_pose: int = _main.FirstPersonArms.POSE_REMOTE
	if _main._arms.hand_poses() != [remote_pose, remote_pose]:
		return _fail("working the crane, the hands do not hold the remote (poses %s)" % str(_main._arms.hand_poses()))
	var start: Dictionary = _native().get_yard_crane_state()
	_move_dir(device, Vector2(1.0, 0.0))
	await _seconds(6.0)
	_move_dir(device, Vector2.ZERO)
	var slewed: Dictionary = _native().get_yard_crane_state()
	var east := (slewed["rope_top"] as Vector3).x - (start["rope_top"] as Vector3).x
	if east < 2.0:
		return _fail("the stick right did not send the hook east (%.2f m)" % east)
	_touch(2, _center(&"jump"), true)
	await _seconds(4.0)
	_touch(2, _center(&"jump"), false)
	var hoisted: Dictionary = _native().get_yard_crane_state()
	var rose := (hoisted["hook"] as Vector3).y - (slewed["hook"] as Vector3).y
	if rose < 3.0:
		return _fail("RAISE held did not hoist the hook (%.2f m)" % rose)
	_touch(3, _center(&"crouch"), true)
	await _seconds(2.0)
	_touch(3, _center(&"crouch"), false)
	var lowered: Dictionary = _native().get_yard_crane_state()
	var fell := (hoisted["hook"] as Vector3).y - (lowered["hook"] as Vector3).y
	if fell < 1.0:
		return _fail("LOWER held did not let the hook down (%.2f m)" % fell)
	print("SCRAPERX_UITEST yard crane: the hook sent %.2f m east, hoisted %.2f m, let down %.2f m" % [east, rose, fell])
	# Heard from its parts as the native measured them: the slewing motors, the
	# trolley's wheels and the hoist's winch.
	var heard: Dictionary = _main._audio.crane_heard
	print("SCRAPERX_UITEST yard crane: heard %s" % str(heard))
	for part in [&"slew", &"trolley", &"winch"]:
		if int(heard.get(part, 0)) < 1:
			return _fail("the crane's %s was not heard (%s)" % [part, str(heard)])
	await _look_at_point(lowered["hook"])
	await _pose("yard_crane_hook")
	_tap(4, _center(&"crane"))
	if not await _wait_until(func() -> bool: return not bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("WALK did not let go of the crane")
	await _seconds(2.0)
	var still: Dictionary = _native().get_yard_crane_state()
	if absf(float(still["slew_rate"])) > 1.0e-4 or absf(float(still["trolley_rate"])) > 1.0e-3:
		return _fail("let go, the crane's drives did not stop")
	return true


# The yard crane's lifting magnet (sim/yard_crane.hpp) and the top of the two
# slabs laid for it (LooseStock::lay_crane_steel), found where they lie.
const YARD_CRANE_MAGNET := 2995
const YARD_CRANE_SLING_SHACKLE := 2987
const MAGNET_SLAB_AT := Vector3(-64.0, 0.458, -124.0)

# The remote's switch on the device under test: the CRANE button, R, LB.
func _crane_switch(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			_tap(4, _center(&"crane"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_LEFT_SHOULDER)
		_:
			_key(KEY_R, true)
			_key(KEY_R, false)

# The hoist's two held buttons on the device under test: RAISE (+1) is Jump,
# LOWER (-1) is Crouch; 0 lets both go.
func _crane_hoist(device: int, way: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			_touch(2, _center(&"jump"), way > 0)
			_touch(3, _center(&"crouch"), way < 0)
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_A, way > 0)
			_button(JOY_BUTTON_RIGHT_STICK, way < 0)
		_:
			_key(KEY_SPACE, way > 0)
			_key(KEY_C, way < 0)

# The operator sends the hook over `target` (world x, z) with the stick and
# waits until the trolley is there and the hook hangs still under it. A key
# has no throw: it is held toward the spot while the drives could still stop
# short of it, and let go inside that.
func _crane_hook_over(device: int, target: Vector2, tolerance: float, timeout: float) -> bool:
	var waited := 0.0
	var settled := 0.0
	while waited < timeout:
		var crane: Dictionary = _native().get_yard_crane_state()
		var top: Vector3 = crane["rope_top"]
		var hook: Vector3 = crane["hook"]
		var off := target - Vector2(top.x, top.z)
		if off.length() < tolerance and (target - Vector2(hook.x, hook.z)).length() < 2.0 * tolerance:
			_move_dir(device, Vector2.ZERO)
			settled += get_process_delta_time()
			if settled > 1.0:
				return true
		else:
			settled = 0.0
			if device == InputRouter.Device.KEYBOARD_MOUSE:
				# Pressed while it would stop short, at rest whenever it is off
				# by more than the tolerance.
				var speed := Vector2(float(crane["trolley_rate"]),
					float(crane["slew_rate"]) * float(crane["trolley_m"])).length()
				var stopping := speed * speed / 1.2
				_move_world(device, off.normalized() if off.length() > 0.75 * tolerance + stopping else Vector2.ZERO)
			else:
				# Never less than a push past the stick's dead zone (0.14).
				_move_world(device, off.normalized() * clampf(off.length() / 6.0, 0.25, 1.0))
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	return false

# Up to the sling's shackle where it lies (on a device's lug, or beside the
# device `beside`), facing it, until TAKE CRANE SLING is offered; then TAKE.
func _take_crane_sling(device: int, beside: int = YARD_CRANE_MAGNET) -> bool:
	for attempt in 6:
		# From the side away from the device (its body in the way), or, on its
		# lug, from where the rigger stands.
		var shackle := _kit_origin(YARD_CRANE_SLING_SHACKLE)
		var magnet := _kit_origin(beside)
		var me := _position()
		var out := Vector2(shackle.x - magnet.x, shackle.z - magnet.z)
		var side := out.normalized() if out.length() > 0.3 else Vector2(me.x - shackle.x, me.z - shackle.z).normalized()
		# Round it a sixth of a turn each try: the sling hanging from the hook
		# can stand between the rigger and the shackle on one side.
		side = side.rotated(float(attempt) * PI / 3.0)
		# 0.6 m off, give or take 0.2: within the hands' 1.2 m of the body's
		# middle even on the ground.
		await _go(device, Vector2(shackle.x, shackle.z) + side * 0.6, 0.2, 6.0)
		var at := _position()
		await _face(Vector2(shackle.x - at.x, shackle.z - at.z))
		if await _offered(&"pick_up", "TAKE", "CRANE SLING"):
			_act(device)
			if await _wait_until(func() -> bool: return int(_ctx()["carrying"]) == YARD_CRANE_SLING_SHACKLE, 1.0):
				return true
	return false

# The hoist held one way until `done`, or `timeout`.
func _crane_hoist_until(device: int, way: int, done: Callable, timeout: float) -> bool:
	_crane_hoist(device, way)
	var ok := await _wait_until(done, timeout)
	_crane_hoist(device, 0)
	return ok

# The magnet's job on the device under test: the remote taken, the hook sent
# just north of the magnet and let down until its shackle lies on the ground
# by it; the shackle taken and HOOKed on; worked, MAGNET ON (the remote's device switch is
# Action), up, over the slabs and down: it takes the top one; up with it;
# MAGNET OFF, and the slab falls. Heard: the coil humming while on, the steel
# taken against the face and let go. CRANE (the button, R, LB) works the
# crane and lets it go.
func _crane_magnet(device: int) -> bool:
	for target in [Vector2(-25.0, -40.0), Vector2(-50.0, -100.0), Vector2(-50.0, -125.0)]:
		if not await _go(device, target, 0.3, 50.0):
			return _fail("magnet: the walk to the crane stopped short of %s" % str(target))
	if not await _go(device, Vector2(YARD_CRANE_MAST_X + 4.2, YARD_CRANE_MAST_Z + 5.6), 0.12, 20.0):
		return _fail("magnet: the walk to the remote's cabinet stopped short")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"crane_take", "TAKE REMOTE", "THE YARD CRANE"):
		return _fail("magnet: TAKE REMOTE was not offered at the cabinet")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["has_remote"]), 1.0):
		return _fail("magnet: TAKE REMOTE did not take the remote")
	await _seconds(0.4)
	var magnet := _kit_origin(YARD_CRANE_MAGNET)
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("magnet: CRANE did not take the crane")
	# The hook just north of the magnet, on the rigger's side, let down until
	# its shackle lies on the ground there with slack in the sling (the throat
	# 3.3 m up; at 3.5 m the sling only just reaches the bail, and a hair's
	# difference in how the hook stops leaves it too short).
	if not await _crane_hook_over(device, Vector2(magnet.x, magnet.z - 0.9), 0.3, 120.0):
		return _fail("magnet: the hook was not sent by the magnet")
	if not await _crane_hoist_until(device, -1,
			func() -> bool: return float((_native().get_yard_crane_state()["hook"] as Vector3).y) < 3.3, 60.0):
		return _fail("magnet: LOWER did not let the hook down by the magnet")
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return not bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("magnet: CRANE did not let go of the crane")
	# By the magnet: TAKE the sling where it lies, then HOOK it on.
	if not await _go(device, Vector2(magnet.x, magnet.z - 1.3), 0.15, 30.0):
		return _fail("magnet: the walk to the magnet stopped short (at %s)" % str(_position()))
	if not await _take_crane_sling(device):
		var shackle := _kit_origin(YARD_CRANE_SLING_SHACKLE)
		return _fail("magnet: TAKE CRANE SLING was not offered by the shackle at %s, the rigger at %s, %.2f m off (read '%s %s')" % [
			str(shackle), str(_position()), (shackle - _position()).length(), _action_label(),
			String(_ctx()["action"]["detail"])])
	if not await _go(device, Vector2(magnet.x, magnet.z - 1.0), 0.08, 6.0):
		return _fail("magnet: the step to the magnet's bail stopped short")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"hook", "HOOK", "ONTO THE MAGNET"):
		return _fail("magnet: HOOK ONTO THE MAGNET was not offered (read '%s %s')" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_yard_crane_state()["device"]) == 1, 1.5):
		return _fail("magnet: HOOK did not put the sling on the magnet")
	# Worked: Action is the magnet's switch.
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("magnet: CRANE did not take the crane again")
	if not await _offered(&"crane_device", "MAGNET ON"):
		return _fail("magnet: worked with the magnet on the sling, Action did not read MAGNET ON (read '%s')" %
			_action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["magnet_on"]), 1.0):
		return _fail("magnet: MAGNET ON did not switch it on")
	# Up 2 m, over the top slab, and down on to it until it takes hold.
	var lifted_from := float((_native().get_yard_crane_state()["hook"] as Vector3).y)
	if not await _crane_hoist_until(device, 1,
			func() -> bool: return float((_native().get_yard_crane_state()["hook"] as Vector3).y) > lifted_from + 2.0, 20.0):
		return _fail("magnet: RAISE did not lift the magnet")
	# Close over the slab's middle, so the most of the magnet's face lands on it
	# (a magnet holds by the steel under its face).
	if not await _crane_hook_over(device, Vector2(MAGNET_SLAB_AT.x, MAGNET_SLAB_AT.z), 0.12, 150.0):
		return _fail("magnet: the magnet was not sent over the slabs")
	# Let down only once it hangs still: within a 5 cm box for 3 s (the hook
	# block swings on 50 m of rope, 15 s a swing, and stands still for a
	# moment at either end of it).
	var marks: Array[Vector2] = []
	var settling := 0.0
	while settling < 60.0:
		await _seconds(0.25)
		settling += 0.25
		var now := _kit_origin(YARD_CRANE_MAGNET)
		marks.append(Vector2(now.x, now.z))
		if marks.size() > 12:
			marks.pop_front()
		if marks.size() == 12:
			var low := marks[0]
			var high := marks[0]
			for mark in marks:
				low = Vector2(minf(low.x, mark.x), minf(low.y, mark.y))
				high = Vector2(maxf(high.x, mark.x), maxf(high.y, mark.y))
			if high.x - low.x < 0.05 and high.y - low.y < 0.05:
				break
	if not await _crane_hoist_until(device, -1,
			func() -> bool: return int(_native().get_yard_crane_state()["holding_entity"]) != 0, 40.0):
		var at := _kit_origin(YARD_CRANE_MAGNET)
		return _fail("magnet: let down on to the slabs, it took hold of nothing (the magnet at %s)" % str(at))
	var held := int(_native().get_yard_crane_state()["holding_entity"])
	var slab_y := _kit_origin(held).y
	if absf(slab_y - MAGNET_SLAB_AT.y) > 0.1:
		return _fail("magnet: it took hold of %d at y %.2f, not the top slab" % [held, slab_y])
	if not await _crane_hoist_until(device, 1, func() -> bool: return _kit_origin(held).y > slab_y + 1.0, 20.0):
		return _fail("magnet: RAISE did not lift the slab on the magnet")
	if not await _offered(&"crane_device", "MAGNET OFF"):
		return _fail("magnet: holding the slab, Action did not read MAGNET OFF (read '%s %s')" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	var detail := String(_ctx()["action"]["detail"])
	var up := _kit_origin(held).y
	_act(device)
	if not await _wait_until(func() -> bool: return _kit_origin(held).y < up - 0.8, 3.0):
		return _fail("magnet: MAGNET OFF did not let the slab fall")
	await _seconds(0.5)
	var heard: Dictionary = _main._audio.crane_heard
	var cues: Dictionary = _main._audio.crane_cues
	if int(heard.get(&"magnet", 0)) < 1 or int(cues.get(&"magnet_take", 0)) < 1 or int(cues.get(&"magnet_drop", 0)) < 1:
		return _fail("magnet: not heard on, taking hold and letting go (heard %s, cues %s)" % [str(heard), str(cues)])
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return not bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("magnet: CRANE did not let go of the crane at the end")
	_detail = "slab=%d lifted_to=%.2f held_read='%s' heard=%s cues=%s" % [held, up, detail, str(heard), str(cues)]
	return true


# The yard crane's container spreader (sim/yard_crane.hpp) and the laydown's
# empty 40 ft box it lifts (LooseStock), found where they lie.
const YARD_CRANE_SPREADER := 2996
const CONTAINER_KIND := 5
const YARD_CONTAINER_NEAR := Vector2(-76.0, -113.0)

# The container lying nearest the laydown's place for it: its entity.
func _yard_container() -> int:
	var stock: PackedInt32Array = _native().get_loose_stock()
	var piece := 0
	var best := 0
	var best_off := 20.0
	for i in range(0, stock.size() - 2, 3):
		if stock[i + 1] == CONTAINER_KIND:
			var at := _kit_origin(5000 + piece)
			var off := (Vector2(at.x, at.z) - YARD_CONTAINER_NEAR).length()
			if off < best_off:
				best = 5000 + piece
				best_off = off
		piece += 1
	return best

# Until the device on the sling hangs still: within a 5 cm box for 8 s. The
# hook block swings on 50 m of rope, 14 s a swing, and stands near still for
# a few seconds at either end of it; over more than half a swing, only a
# swing of under 2.5 cm stays in the box.
func _crane_device_still(entity: int, timeout: float) -> bool:
	var marks: Array[Vector2] = []
	var waited := 0.0
	while waited < timeout:
		await _seconds(0.25)
		waited += 0.25
		var now := _kit_origin(entity)
		marks.append(Vector2(now.x, now.z))
		if marks.size() > 32:
			marks.pop_front()
		if marks.size() == 32:
			var low := marks[0]
			var high := marks[0]
			for mark in marks:
				low = Vector2(minf(low.x, mark.x), minf(low.y, mark.y))
				high = Vector2(maxf(high.x, mark.x), maxf(high.y, mark.y))
			if high.x - low.x < 0.05 and high.y - low.y < 0.05:
				return true
	return false

# The device on the sling sent over `target` and hanging still within
# `tolerance` of it: the operator watches the device, not the trolley, and
# sets the trolley again by how far off the spot it hangs.
func _crane_device_over(device: int, entity: int, target: Vector2, tolerance: float) -> bool:
	if not await _crane_hook_over(device, target, 0.12, 150.0):
		return false
	for pass_n in 4:
		var still := await _crane_device_still(entity, 90.0)
		var at := _kit_origin(entity)
		var off := target - Vector2(at.x, at.z)
		print("SCRAPERX_UITEST crane: pass %d, %d %s at %s, %.3f m off" % [pass_n, entity,
			"still" if still else "not still", str(at), off.length()])
		if not still:
			return false
		if off.length() < tolerance:
			return true
		var top: Vector3 = _native().get_yard_crane_state()["rope_top"]
		if not await _crane_hook_over(device, Vector2(top.x, top.z) + off, 0.1, 60.0):
			print("SCRAPERX_UITEST crane: the trolley not set again over %s" % str(Vector2(top.x, top.z) + off))
			return false
	return false

# The spreader's job by touch: the remote taken, the hook let down by the
# frame's side; round the yard's steel to it, the sling's shackle taken and
# HOOKed on its lug. Worked: up, over the laydown's container and down on to
# its corner castings until the rope goes slack; Action reads LOCK, and the
# box rises with it. In the air there is no UNLOCK (the interlock); set down,
# Action reads UNLOCK and lets go, and the frame raised leaves the box where
# it was set. Heard: the twistlocks turning, locking and unlocking.
func _crane_spreader(device: int) -> bool:
	var box := _yard_container()
	if box == 0:
		return _fail("spreader: the laydown's container was not found")
	for target in [Vector2(-25.0, -40.0), Vector2(-50.0, -100.0), Vector2(-50.0, -125.0)]:
		if not await _go(device, target, 0.3, 50.0):
			return _fail("spreader: the walk to the crane stopped short of %s" % str(target))
	if not await _go(device, Vector2(YARD_CRANE_MAST_X + 4.2, YARD_CRANE_MAST_Z + 5.6), 0.12, 20.0):
		return _fail("spreader: the walk to the remote's cabinet stopped short")
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"crane_take", "TAKE REMOTE", "THE YARD CRANE"):
		return _fail("spreader: TAKE REMOTE was not offered at the cabinet")
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["has_remote"]), 1.0):
		return _fail("spreader: TAKE REMOTE did not take the remote")
	await _seconds(0.4)
	var frame := _kit_origin(YARD_CRANE_SPREADER)
	var box0 := _kit_origin(box)
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("spreader: CRANE did not take the crane")
	# The hook by the frame's east side, let down until its shackle lies on
	# the ground there.
	if not await _crane_hook_over(device, Vector2(frame.x + 1.7, frame.z), 0.3, 120.0):
		return _fail("spreader: the hook was not sent by the spreader")
	if not await _crane_hoist_until(device, -1,
			func() -> bool: return float((_native().get_yard_crane_state()["hook"] as Vector3).y) < 3.4, 60.0):
		return _fail("spreader: LOWER did not let the hook down by the spreader")
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return not bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("spreader: CRANE did not let go of the crane")
	# Round the slabs, the magnet and the beams, south of them, to the frame.
	for target in [Vector2(-60.5, -126.0), Vector2(-60.5, -119.5), Vector2(-80.0, -119.5),
			Vector2(frame.x + 2.4, frame.z - 1.0)]:
		if not await _go(device, target, 0.2, 40.0):
			return _fail("spreader: the walk to the spreader stopped short of %s (at %s)" % [str(target), str(_position())])
	if not await _take_crane_sling(device, YARD_CRANE_SPREADER):
		var shackle := _kit_origin(YARD_CRANE_SLING_SHACKLE)
		return _fail("spreader: TAKE CRANE SLING was not offered by the shackle at %s, the rigger at %s (read '%s %s')" % [
			str(shackle), str(_position()), _action_label(), String(_ctx()["action"]["detail"])])
	if not await _go(device, Vector2(frame.x + 1.65, frame.z), 0.08, 6.0):
		return _fail("spreader: the step to the spreader's lug stopped short")
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO THE SPREADER"):
		return _fail("spreader: HOOK ONTO THE SPREADER was not offered (read '%s %s')" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_yard_crane_state()["device"]) == 2, 1.5):
		return _fail("spreader: HOOK did not put the sling on the spreader")
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("spreader: CRANE did not take the crane again")
	# On the ground off a box: no LOCK to offer.
	await _seconds(0.5)
	if _ctx()["action"]["id"] == &"crane_device":
		return _fail("spreader: lying off a box, Action offered '%s'" % _action_label())
	# Up 4.5 m, over the box, and down on to its castings until the rope goes
	# slack.
	var lifted_from := float((_native().get_yard_crane_state()["hook"] as Vector3).y)
	if not await _crane_hoist_until(device, 1,
			func() -> bool: return float((_native().get_yard_crane_state()["hook"] as Vector3).y) > lifted_from + 4.5, 30.0):
		return _fail("spreader: RAISE did not lift the spreader")
	if not await _crane_device_over(device, YARD_CRANE_SPREADER, Vector2(box0.x, box0.z), 0.08):
		var at := _kit_origin(YARD_CRANE_SPREADER)
		return _fail("spreader: it was not brought still over the container (at %s, the box at %s)" % [str(at), str(box0)])
	if not await _crane_hoist_until(device, -1,
			func() -> bool: return bool(_native().get_yard_crane_state()["spreader_landed"]), 40.0):
		var at := _kit_origin(YARD_CRANE_SPREADER)
		var crane: Dictionary = _native().get_yard_crane_state()
		return _fail("spreader: let down on to the container, it did not land on its castings (at %s, seated %s, the sling %.0f N)" % [
			str(at), str(crane["spreader_seated"]), float(crane["sling_n"])])
	if not await _offered(&"crane_device", "LOCK", "ON THE BOX'S CORNERS"):
		return _fail("spreader: landed on the box, Action did not read LOCK (read '%s %s')" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_yard_crane_state()["holding_entity"]) == box, 1.0):
		return _fail("spreader: LOCK did not lock on to the box")
	if not await _crane_hoist_until(device, 1, func() -> bool: return _kit_origin(box).y > box0.y + 2.0, 40.0):
		return _fail("spreader: RAISE did not lift the box on the spreader")
	await _seconds(1.0)
	var high := _kit_origin(box).y
	# In the air: Action is the crane's WALK again, the hold read beside it,
	# and on touch drawn by the button.
	var held_read := String(_ctx()["action"]["detail"])
	if _ctx()["action"]["id"] == &"crane_device" or not held_read.begins_with("LOCKED"):
		return _fail("spreader: the box in the air, Action read '%s %s'" % [_action_label(), held_read])
	var drawn := false
	for prompt in _main._hud._prompts:
		drawn = drawn or String(prompt["detail"]) == held_read
	if not drawn:
		return _fail("spreader: the box in the air, the hold '%s' is not drawn (prompts %s)" % [held_read,
			str(_main._hud._prompts)])
	await _look_at_point(_kit_origin(box))
	await _pose("crane_spreader_lifted")
	if not await _crane_hoist_until(device, -1,
			func() -> bool: return bool(_native().get_yard_crane_state()["spreader_landed"]), 40.0):
		return _fail("spreader: the box was not set down")
	if not await _offered(&"crane_device", "UNLOCK", "THE BOX IS DOWN"):
		return _fail("spreader: the box set down, Action did not read UNLOCK (read '%s %s')" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_yard_crane_state()["holding_entity"]) == 0, 1.0):
		return _fail("spreader: UNLOCK did not let go of the box")
	var landed := _kit_origin(box).y
	var raised_from := float((_native().get_yard_crane_state()["hook"] as Vector3).y)
	if not await _crane_hoist_until(device, 1,
			func() -> bool: return float((_native().get_yard_crane_state()["hook"] as Vector3).y) > raised_from + 2.0, 20.0):
		return _fail("spreader: RAISE did not lift the frame off the box")
	await _seconds(1.0)
	var left := _kit_origin(box).y
	if absf(left - landed) > 0.02 or absf(landed - box0.y) > 0.05:
		return _fail("spreader: the frame raised did not leave the box where it was set (set at %.3f, left at %.3f, from %.3f)" % [
			landed, left, box0.y])
	var cues: Dictionary = _main._audio.crane_cues
	if int(cues.get(&"twistlocks", 0)) < 2:
		return _fail("spreader: the twistlocks not heard locking and unlocking (cues %s)" % str(cues))
	_crane_switch(device)
	if not await _wait_until(func() -> bool: return not bool(_native().get_yard_crane_state()["operating"]), 1.0):
		return _fail("spreader: CRANE did not let go of the crane at the end")
	_detail = "box=%d lifted_to=%.2f set_down=%.3f held_read='%s' cues=%s" % [box, high, landed, held_read, str(cues)]
	return true

# The truck's heading as the native's yaw (0 its nose +z, turning right -).
func _truck_yaw() -> float:
	var nose: Vector3 = (_native().get_kit_body_transform(int(_native().get_kit_body_index(2982))) as Transform3D).basis.z
	return atan2(nose.x, nose.z)


# Drives as a driver does, the view turning with the truck, from where it is
# parked (nose +z) to the lane x = lane_x west of it: a full right turn,
# along until a full left turn (which carries it as far on as the right one
# did) will end on the lane, the left turn, then up the lane looking 1 m
# ahead until the truck stands at z = stop_z. Gets there only with its forks
# in under the load: crooked, a blade strikes a runner and it stops short.
func _drive_to_lane(device: int, lane_x: float, stop_z: float, pace: float) -> bool:
	var truck := int(_native().get_kit_body_index(2982))
	var west := -0.5 * PI
	var phase := 0
	var start_z: float = (_native().get_kit_body_transform(truck) as Transform3D).origin.z
	var advance := 0.0
	var budget := 40.0
	while budget > 0.0:
		var at: Vector3 = (_native().get_kit_body_transform(truck) as Transform3D).origin
		var yaw := _truck_yaw()
		var steer := 0.0
		if phase == 0:
			steer = 1.0
			if yaw <= west + 0.2:
				advance = at.z - start_z
				phase = 1
		elif phase == 1:
			steer = clampf(2.0 * (yaw - west), -1.0, 1.0)
			if at.x <= lane_x + advance:
				phase = 2
		elif phase == 2:
			steer = -1.0
			if yaw >= -0.2:
				phase = 3
		else:
			if at.z >= stop_z:
				_move_dir(device, Vector2.ZERO)
				return true
			var dx := lane_x - at.x
			var right := -dx * cos(yaw) + 1.0 * sin(yaw)
			var ahead := dx * sin(yaw) + 1.0 * cos(yaw)
			steer = clampf(4.0 * atan2(right, ahead), -1.0, 1.0)
		_move_dir(device, Vector2(steer, pace))
		await get_tree().process_frame
		budget -= get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	return false


func _forklift_strain_touch(device: int) -> bool:
	var stillage := -1
	var stock: PackedInt32Array = _native().get_loose_stock()
	for i in range(0, stock.size(), 3):
		if stock[i + 1] == 12:   # LooseStock::Kind::BilletStillage: the yard's, first
			stillage = stock[i]
			break
	if stillage < 0:
		return _fail("no stillage of billets in the loose stock")
	var load0: Vector3 = _native().get_kit_body_transform(stillage).origin
	if not await _board_forklift(device):
		return false
	if not await _drive_to_lane(device, load0.x, load0.z - 1.85, 0.35):
		return _fail("the forklift was not driven in under the stillage (at %s)" % str(
			(_native().get_kit_body_transform(int(_native().get_kit_body_index(2982))) as Transform3D).origin))
	await _seconds(1.5)
	var strains_before := int(_main.haptic_counts.get(&"strain", 0))
	var lands_before := int(_main.haptic_counts.get(&"land", 0))
	# RAISE held: the truck, not the load, gives.
	_touch(2, _center(&"jump"), true)
	var tipped := await _wait_until(func() -> bool:
		var f: Dictionary = _native().get_forklift_state()
		var contacts: Array = f["wheel_contact"]
		return not bool(contacts[2]) and not bool(contacts[3]) and float(f["pitch_rad"]) > 0.03, 3.0)
	await _seconds(0.6)
	var rise: float = _native().get_kit_body_transform(stillage).origin.y - load0.y
	var pitch := float(_native().get_forklift_state()["pitch_rad"])
	await _pose("forklift_strain")
	_touch(2, _center(&"jump"), false)
	print("SCRAPERX_UITEST forklift strain: nose down %.1f deg, the stillage up %.2f m, strain pulses %d, cues %s" % [
		rad_to_deg(pitch), rise, int(_main.haptic_counts.get(&"strain", 0)) - strains_before,
		str(_main._audio.forklift_cues)])
	if not tipped:
		return _fail("RAISE under the stillage did not bring the truck's back end up")
	if int(_main.haptic_counts.get(&"strain", 0)) <= strains_before:
		return _fail("the strain was not felt in the hands")
	if int(_main._audio.forklift_cues.get(&"unload", 0)) < 1 or int(_main._audio.forklift_cues.get(&"groan", 0)) < 1:
		return _fail("the rear wheels lifting and the mast straining were not heard")
	await _seconds(1.0)
	# LOWER held: back down onto the rear wheels.
	_touch(3, _center(&"crouch"), true)
	var down := await _wait_until(func() -> bool:
		var contacts: Array = _native().get_forklift_state()["wheel_contact"]
		return bool(contacts[2]) and bool(contacts[3]), 4.0)
	await _seconds(1.0)
	_touch(3, _center(&"crouch"), false)
	if not down:
		return _fail("LOWER did not bring the truck back down onto its rear wheels")
	if int(_main._audio.forklift_cues.get(&"slam", 0)) < 1 or int(_main.haptic_counts.get(&"land", 0)) <= lands_before:
		return _fail("the rear wheels coming down were not heard and felt")
	if not await _offered(&"drive", "STEP OFF"):
		return _fail("STEP OFF was not offered while driving")
	_act(device)
	return await _wait_until(func() -> bool: return not bool(_native().get_forklift_state()["driving"]), 1.0)

