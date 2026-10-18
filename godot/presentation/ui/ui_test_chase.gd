extends "res://presentation/ui/ui_test_stack.gd"
# The Matron (src/sim/matron.hpp), played and looked at on the 154 m deck.
#
# touch_matron: a sprint past her locker wakes her; the gauge climbs and the
# bolts pop while the player walks quietly on to the catwalk's east end; she
# bursts out and hunts; the player sprints over the rotten catwalk and she
# follows onto a plate that gives way under her: the one thing that kills her.
# look_matron_*: renders only (not in CI): the locker straining, her bursting
# out, her hunt, from the player's eyes.
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

const MATRON_STATE_STRAINING := 1
const MATRON_STATE_HUNTING := 3
const MATRON_STATE_DEAD := 6
const MATRON_BAND_X := 21.5
const MATRON_NORTH_Z := -128.5
const MATRON_MID_Z := -150.0
const MATRON_DECK_Y := 154.0


func _matron_state() -> Dictionary:
	return _native().get_matron_state()


func _crouch_on(device: int, on: bool) -> bool:
	if bool(_native().is_player_crouched()) == on:
		return true
	_tap(1, _center(&"crouch"))
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
	_detail = "plates_down=%d deaths=%d steps_heard=%d" % [int(_matron_state()["planks_fallen"]),
		int(_native().get_death_count()), int(_main._audio.matron_cues.get(&"matron_step", 0))]
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
		# Rising (0.15 m a frame): the vault.
		["turn_vault", {"state": 3, "speed": 4.0, "stride": 0.3, "turn": 2}, 0.15],
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
		if entry[0] in ["shuffle", "hunt_a", "turn_windup", "turn_lunge_a"]:
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
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the drum down")
	await _seconds(3.0)
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
	if not await _offered(&"drive", "STEP OFF"):
		return _fail("STEP OFF was not offered while driving")
	_act(device)
	if not await _wait_until(func() -> bool: return not bool(_native().get_forklift_state()["driving"]), 1.0):
		return _fail("STEP OFF did not let go of the controls")
	await _look_at_point(set_down)
	await _pose("forklift_pallet_set_down")
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
		if stock[i + 1] == 12:   # LooseStock::Kind::BilletStillage
			stillage = stock[i]
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

