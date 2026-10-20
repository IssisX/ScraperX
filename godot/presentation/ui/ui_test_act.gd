extends "res://presentation/ui/ui_test_chase.gd"
# The Matron's first act (src/sim/matron.hpp), played on touch from the 154 m
# deck to the 220 m ring: touch_matron_act.
#
# Asleep, the climber rigs Stage A's cage and carries a toolbox from the north
# band's rail to her locker door; its clank set down there wakes her. While
# she strains, the line down to 143 m, and crouched round under her post. Out,
# a pest, unseen: she takes the toolbox, carries it off, and at her post
# undoes the cage's rope. Sprints along 143's shaft edge under her make her
# dangerous, and she comes down her corner pipe after the climber, knocking
# on it (touch_matron_pipe times it, and drops a drum on her). Through the
# gate in 143's rail and up the riser to her deck meanwhile; off the pipe she
# listens, and comes back up it after them. Round on her east band by the
# rail, her shove throws the climber at the shaft. More noise and she is
# fully changed: her hands kill. Back on 143 (the checkpoint: where the
# climber stood as she turned dangerous over them), up the riser again
# while she looks for the climber where she killed them, quietly to her
# ladder and up it to the 176 ring, her Turn
# bringing her up after; on by the climbing route (boards and standpipe,
# catwalk and scaffold), where she cannot follow, to the 220 ring.
#
# With --capture, the screen at each stage: the cage she unrigged, her coming
# dangerous and her shove at the rail, her coming fully changed, her coming up
# the ladder after, and the 220 ring. Between those the drawing is
# switched off (the simulation runs on), so the capture is quick.

const MATRON_STATE_GUARDING := 7
const STAGE_PEST := 0
const STAGE_DANGEROUS := 1
const STAGE_CHANGED := 2
const ACT_TOOLBOX_PICK := Vector2(-6.5, -125.75)
const ACT_TOOLBOX_DOOR := Vector2(6.0, -126.4)
const ACT_143_EDGE_Z := -132.6
const ACT_GATE := Vector2(-6.8, -132.2)
const ACT_PLATE := Vector2(-6.8, -134.1)
const ACT_RISER_FOOT := Vector2(-6.0, -133.85)
# By the east band's rail, south of the gate in it where the catwalk crosses:
# come up her pipe in the north-east corner, she runs down the band at the
# climber and, lunging in her Turn, carries them on south along the rail, into
# the corner and out onto the south band, not out through the gate.
const ACT_RIM := Vector2(18.0, -158.0)
# The chase up and down the east band, 3.6 m from the shaft's rim: past the
# 3 m within which, dangerous, she aims her shove at it.
const ACT_LAP_NORTH := Vector2(20.6, -134.5)
const ACT_LAP_SOUTH := Vector2(20.6, -143.5)
const WELL_A_CAGE_ENTITY := 2000
# Frames a screenshot holds the simulation still: enough to draw a few after
# drawing comes back on.
const ACT_POSE_FRAMES := 5

var _act_box := 0
var _act_stages_seen := {}
var _act_drawing := true
var _act_view_back := Vector2(NAN, NAN)   # the view's yaw and pitch before a screenshot aimed it
var _act_running := false   # screenshots are the act's own while it plays
# Deaths the act means at this point (her fully changed hands: one).
var _act_deaths_allowed := 0


func _act_capturing() -> bool:
	return not _capture_prefix.is_empty() and DisplayServer.get_name() != "headless"


func _act_draw(on: bool) -> void:
	if _act_capturing() and _act_drawing != on:
		_act_drawing = on
		RenderingServer.render_loop_enabled = on


# Every screenshot the act takes (its own, and those of the routes it reuses)
# holds the simulation still for the same few frames, headless or drawn, so
# the run played is the same frame for frame with or without pictures: the
# frames a screenshot spends still run the touch router and the view's
# smoothing. Only a capture run draws them and writes the last one. Outside
# the act, screenshots are the base's.
func _pose(pose: String) -> void:
	if not _act_running:
		await super._pose(pose)
		return
	var was := _act_drawing
	_main.freeze_sim = true
	_act_draw(true)
	await _frames(ACT_POSE_FRAMES)
	if _act_capturing():
		var path := "%s_%s.png" % [_capture_prefix, pose]
		DirAccess.make_dir_recursive_absolute(path.get_base_dir())
		if get_viewport().get_texture().get_image().save_png(path) == OK:
			print("SCRAPERX_UITEST_SCREENSHOT %s" % path)
		else:
			push_error("SCRAPERX_UITEST_SCREENSHOT_FAILED %s" % path)
	_act_draw(was)
	_main.freeze_sim = false


func _act_pose(pose: String) -> void:
	await _pose(pose)
	if not is_nan(_act_view_back.x):
		_main._yaw = _act_view_back.x
		_main._pitch = _act_view_back.y
		_act_view_back = Vector2(NAN, NAN)


# The eyes on her head for a screenshot, with the simulation held still till
# it is taken (headless too, for the same frames); the view's yaw and pitch go
# back after.
func _act_frame_her() -> void:
	_main.freeze_sim = true
	var view := Vector2(float(_main._yaw), float(_main._pitch))
	await _look_at_point(_main._matron_view.head_world_position() - Vector3(0.0, 0.25, 0.0))
	_act_view_back = view


# The eyes on the toolbox in the air.
func _act_frame_box() -> void:
	if _act_box != 0:
		_main.freeze_sim = true
		var view := Vector2(float(_main._yaw), float(_main._pitch))
		await _look_at_point(_native().get_kit_body_transform(_native().get_kit_body_index(_act_box)).origin)
		_act_view_back = view


func _act_log(tag: String) -> void:
	var m := _matron_state()
	var at := _position()
	var her: Vector3 = m["position"]
	var box := Vector3.ZERO
	if _act_box != 0:
		box = _native().get_kit_body_transform(_native().get_kit_body_index(_act_box)).origin
	print("SCRAPERX_ACT %s t=%.1f me=(%.1f,%.1f,%.1f) her=(%.1f,%.1f,%.1f) state=%d stage=%d rage=%.3f turn=%d carrying=%d takes=%d throws=%d shoves=%d unrigs=%d deaths=%d sees=%s speed=%.2f box=(%.2f,%.2f,%.2f)" % [
		tag, float(Engine.get_process_frames()) / 60.0, at.x, at.y, at.z, her.x, her.y, her.z, int(m["state"]),
		int(m["stage"]), float(m["rage"]), int(m["turn"]), int(m["carrying"]), int(m["takes"]), int(m["throws"]),
		int(m["shoves"]), int(m["unrigs"]), int(_native().get_death_count()), str(m["sees_player"]), float(m["speed"]),
		box.x, box.y, box.z])


# _wait_until, noting every stage she passes through and logging now and then.
func _act_wait(condition: Callable, timeout: float, tag: String) -> bool:
	var waited := 0.0
	var next_log := 0.0
	while true:
		_act_stages_seen[int(_matron_state()["stage"])] = true
		if bool(condition.call()):
			return true
		if waited >= timeout or int(_native().get_death_count()) > _act_deaths_allowed:
			_act_log(tag + ("_timeout" if waited >= timeout else "_died"))
			return false
		if waited >= next_log:
			_act_log(tag)
			next_log += 3.0
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


# Walks to `target` with the stick held short of `amount` of its throw: under
# the pace whose footfalls carry (3.5 m/s), so a sleeper is not woken.
func _act_creep_to(device: int, target: Vector2, tolerance: float, budget: float, amount: float) -> bool:
	var waited := 0.0
	while waited < budget:
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		if to.length() <= tolerance:
			_move_dir(device, Vector2.ZERO)
			await _seconds(0.2)
			return true
		var turn := 4.0 * get_process_delta_time()
		if to.length() > 1.0:
			_main._yaw = float(_main._yaw) + clampf(wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI), -turn, turn)
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		_move_dir(device, Vector2(to.dot(right), to.dot(forward)).normalized() * clampf(to.length() / 0.8, 0.2, amount))
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	return false


func _act_her_xz() -> Vector2:
	var her: Vector3 = _matron_state()["position"]
	return Vector2(her.x, her.z)


func _act_me_xz() -> Vector2:
	return Vector2(_position().x, _position().z)


# Sprints back and forth between `a` and `b` until `done` holds or `budget`
# seconds pass, checking every frame; the stick let go at the end.
func _act_laps(device: int, a: Vector2, b: Vector2, done: Callable, budget: float, tag: String) -> bool:
	var start := Engine.get_process_frames()
	var leg := 0
	while float(Engine.get_process_frames() - start) / 60.0 < budget:
		var target := a if leg % 2 == 0 else b
		await _face(target - _act_me_xz())
		if not _stick_sprint():
			_move(device, 0.0)
			return _fail("the stick pushed past its ring did not latch sprint")
		var on_leg := 0.0
		while on_leg < 6.0:
			_act_stages_seen[int(_matron_state()["stage"])] = true
			if bool(done.call()):
				_move(device, 0.0)
				return true
			if int(_native().get_death_count()) > _act_deaths_allowed:
				_move(device, 0.0)
				_act_log(tag + "_died")
				print("SCRAPERX_ACT death %s" % str(_native().get_death_state()))
				return false
			var to := target - _act_me_xz()
			if to.length() <= 1.2:
				break
			var turn := 4.0 * get_process_delta_time()
			_main._yaw = float(_main._yaw) + clampf(wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI), -turn, turn)
			await get_tree().process_frame
			on_leg += get_process_delta_time()
		_move(device, 0.0)
		leg += 1
		_act_log(tag)
	_move(device, 0.0)
	_act_log(tag + "_timeout")
	return bool(done.call())


func _matron_act_touch(device: int) -> bool:
	_act_running = true
	_act_draw(false)
	var played: bool = await _act_play(device)
	_act_draw(true)
	_act_running = false
	return played


func _act_play(device: int) -> bool:
	if not await _act_asleep(device):
		return false
	if not await _act_wake_and_go_below(device):
		return false
	if not await _act_pest_unseen(device):
		return false
	if not await _act_down_the_pipe(device):
		return false
	if not await _act_dangerous_and_changed(device):
		return false
	if not await _act_way_up(device):
		return false
	_detail = "stages_seen=%s deaths=%d she_at=%s" % [str(_act_stages_seen.keys()), int(_native().get_death_count()),
		str(_matron_state()["position"])]
	return true


# Asleep: Stage A's cage rigged, and a toolbox from the north band's rail
# carried to her locker door at a pace that does not wake her; set down
# there, it does.
func _act_asleep(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _rig_stage_a(device):
		return false
	for leg in [Vector2(-10.2, -130.6), Vector2(-10.0, -129.2)]:
		if not await _walk_to(device, leg, 0.2):
			return _fail("the walk out of the cage stalled at %s" % str(_position()))
	if not await _act_creep_to(device, ACT_TOOLBOX_PICK, 0.08, 10.0, 0.5):
		return _fail("the walk to the toolbox stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP"):
		return _fail("Action read '%s' at the toolbox, not PICK UP" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) != 0, 1.0):
		return _fail("PICK UP did not put the toolbox in the hands")
	_act_box = int(_native().get_carrying_entity_id())
	if not await _act_creep_to(device, ACT_TOOLBOX_DOOR, 0.2, 14.0, 0.5):
		return _fail("the carry to her locker door stalled at %s" % str(_position()))
	if int(_matron_state()["state"]) != MATRON_STATE_DORMANT:
		return _fail("rigging and the carry woke her (state %d)" % int(_matron_state()["state"]))
	# Set down at her door, its clank wakes her.
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the toolbox down")
	var woke: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 2.0)
	_act_log("woke_by_the_toolbox")
	if not woke:
		return _fail("the toolbox set down at her door did not wake her (state %d)" % int(_matron_state()["state"]))
	return true


# While she strains, the line down to 143, and crouched round its deck to the
# shaft's north edge, under her post.
func _act_wake_and_go_below(device: int) -> bool:
	if not (await _walk_to(device, Vector2(3.0, -132.36), 0.3, 6.0, true) and \
			await _walk_to(device, Vector2(6.0, -132.36), 0.15, 6.0, true)) or _position().y < 155.0:
		return _fail("up the ramp onto the line's stand stalled at %s" % str(_position()))
	_move_dir(device, Vector2.ZERO)
	await _face(Vector2(0.0, -1.0))
	_press(&"jump", 1)
	if not await _wait_until(func() -> bool: return bool(_native().get_zipline_state()["riding"]), 1.5):
		return _fail("the jump from the stand never took the trolley's bar (at %s)" % str(_position()))
	if bool(_matron_state()["released"]):
		return _fail("she was out before the climber was on the line")
	var line := int(_native().get_zipline_state()["line"])
	var length := float(_native().get_zipline_lines()[line]["length"])
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
		var brake := float(zip["along_m"]) > length - 12.0 and speed > 6.0
		if brake and not holding_back:
			_stick_push(Vector2(0.0, 1.0))
		elif not brake and holding_back:
			_move_dir(device, Vector2.ZERO)
		holding_back = brake
		stopped = float(zip["along_m"]) > length - 1.5 and speed < 0.3
	_move_dir(device, Vector2.ZERO)
	if not stopped:
		return _fail("the ride never stopped at the bottom")
	await _frames(2)
	_press(&"drop", 1)
	if not await _wait_until(func() -> bool: return bool(_native().is_player_grounded()) and _position().y < 145.0, 2.5):
		return _fail("DROP at the bottom did not put the body down on 143 m (at %s)" % str(_position()))
	if not await _crouch_on(device, true):
		return _fail("CROUCH did not crouch the climber")
	for leg in [Vector2(-MATRON_BAND_X, -171.5), Vector2(-MATRON_BAND_X, MATRON_NORTH_Z), Vector2(-16.0, ACT_143_EDGE_Z)]:
		if not await _go(device, leg, 0.6, 40.0):
			return _fail("crouched round 143 stalled at %s" % str(_position()))
	_act_log("under_her_post")
	return true


# Out, a pest: unseen, she takes the toolbox from her door and carries it
# off, and at her post undoes Stage A's rope.
func _act_pest_unseen(device: int) -> bool:
	var seen := {"carried": false}
	var done: bool = await _act_wait(func() -> bool:
		var m := _matron_state()
		seen["carried"] = bool(seen["carried"]) or int(m["carrying"]) == _act_box
		return int(m["unrigs"]) >= 1 and int(m["state"]) == MATRON_STATE_GUARDING, 150.0, "pest_unseen")
	if not done:
		return _fail("she never stood guard having undone the cage's rope (state %d, unrigs %d)" % [
			int(_matron_state()["state"]), int(_matron_state()["unrigs"])])
	if not bool(seen["carried"]) or int(_matron_state()["takes"]) < 1:
		return _fail("a pest, she never took the toolbox from her door (takes %d)" % int(_matron_state()["takes"]))
	if int(_native().get_well_a_rope_end_entity_id()) == WELL_A_CAGE_ENTITY:
		return _fail("her unrigging left the rope's end on the cage's eye")
	if int(_matron_state()["stage"]) != STAGE_PEST or int(_native().get_death_count()) != 0:
		return _fail("out and unseen, she was not a pest (stage %d)" % int(_matron_state()["stage"]))
	_act_log("pest_unrigged")
	return true


# Sprints along 143's shaft edge under her post, heard up the shaft, make her
# dangerous, and she takes her corner pipe down after the climber. Through
# the gate and up the riser to her deck meanwhile (from its head, the cage she
# unrigged); off the pipe she listens, and comes back up it after them.
func _act_down_the_pipe(device: int) -> bool:
	await _crouch_on(device, false)
	var links := int(_matron_state()["pipe_links"])
	var took: bool = await _act_laps(device, Vector2(-3.0, ACT_143_EDGE_Z), Vector2(-10.5, ACT_143_EDGE_Z),
		func() -> bool: return int(_matron_state()["pipe_links"]) > links, 200.0, "rage_below")
	if not took:
		return _fail("sprints under her never brought her down her pipe (stage %d, rage %.3f)" % [
			int(_matron_state()["stage"]), float(_matron_state()["rage"])])
	_act_log("down_her_pipe")
	if not await _act_up_riser(device):
		return false
	_act_log("on_her_deck")
	var cage_eye := Vector3(-11.35, MATRON_DECK_Y + 1.0, -131.4)
	await _look_at_point(cage_eye)
	await _act_pose("act_pest_unrigged")
	var back: bool = await _act_wait(func() -> bool: return int(_matron_state()["pipe_links"]) >= links + 2, 60.0,
		"up_her_pipe")
	if not back:
		return _fail("off her pipe on 143, she never came back up it after the climber (links %d, at %s)" % [
			int(_matron_state()["pipe_links"]), str(_matron_state()["position"])])
	return true


# _act_wait with the eyes kept on her (turned at a thumb's 4 rad/s), and the
# screen taken the first time she is seen carrying the toolbox, and just after
# each throw.
func _act_watch_her(condition: Callable, timeout: float, tag: String, near_pose := "", near_m := 0.0) -> bool:
	var waited := 0.0
	var next_log := 0.0
	var carried := false
	var near_posed := near_pose.is_empty()
	var throws := int(_matron_state()["throws"])
	while true:
		var m := _matron_state()
		_act_stages_seen[int(m["stage"])] = true
		if bool(condition.call()):
			return true
		if waited >= timeout or int(_native().get_death_count()) > _act_deaths_allowed:
			_act_log(tag + ("_timeout" if waited >= timeout else "_died"))
			return false
		if waited >= next_log:
			_act_log(tag)
			next_log += 3.0
		var to := _act_her_xz() - _act_me_xz()
		var off := 0.0
		if to.length() > 0.3:
			var turn := 4.0 * get_process_delta_time()
			off = wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI)
			_main._yaw = float(_main._yaw) + clampf(off, -turn, turn)
		if not near_posed and to.length() < near_m and absf(off) < 0.3:
			near_posed = true
			await _act_frame_her()
			await _act_pose(near_pose)
		if not carried and int(m["carrying"]) == _act_box and absf(off) < 0.25:
			carried = true
			await _act_frame_her()
			await _act_pose("act_pest_carrying")
		if int(m["throws"]) > throws:
			throws = int(m["throws"])
			await _seconds(0.15)
			await _act_frame_box()
			await _act_pose("act_pest_throw")
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


# From 143's band through the gate in the shaft's rail onto deck 13's walkway
# plate, to the riser's foot, facing its rungs.
# (Put back by a death the climber may be on the plate already.)
func _act_to_riser_foot(device: int) -> bool:
	await _crouch_on(device, false)
	var on_plate := _position().z < -133.2
	if not on_plate and not (await _go(device, ACT_GATE, 0.2, 12.0) and await _walk_to(device, ACT_PLATE, 0.15, 4.0)):
		return _fail("through the rail's gate onto the plate stalled at %s" % str(_position()))
	if _act_me_xz().distance_to(ACT_RISER_FOOT) > 0.25 and not await _walk_to(device, ACT_RISER_FOOT, 0.08, 6.0):
		return _fail("the step to the riser's foot stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	return true


func _act_up_riser(device: int, off_its_head := true) -> bool:
	if not await _act_to_riser_foot(device):
		return false
	# A step into its rungs from the plate already takes hold of it.
	if int(_native().get_traversal_state()) != TRAVERSAL_CLIMBING:
		if not await _offered(&"climb", "CLIMB"):
			return _fail("Action read '%s' facing the riser, not CLIMB" % _action_label())
		_act(device)
		if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
			return _fail("CLIMB did not take hold of the riser")
	_move(device, 1.0)
	var up: bool = await _act_wait(func() -> bool: return _standing_above(154.5), 30.0, "riser")
	_move(device, 0.0)
	if not up:
		return _fail("the riser to her deck failed (y %.2f)" % _position().y)
	if off_its_head and not await _walk_to(device, Vector2(-6.0, -130.0), 0.15, 4.0):
		return _fail("the step off the riser's head stalled at %s" % str(_position()))
	return true


# Dangerous, she comes up her pipe after the climber: by the shaft's rim on
# her east band her shove throws the climber at it. More noise, and fully
# changed her hands kill.
func _act_dangerous_and_changed(device: int) -> bool:
	# Round onto the east band while she climbs, beside the shaft's rail
	# (whatever her hands do there throws the climber at the rail, not off the
	# deck's open outer edge).
	if not (await _sprint_to(device, Vector2(16.0, -128.6), 1.0, 8.0) and \
			await _sprint_to(device, ACT_LAP_NORTH, 1.0, 6.0)):
		return _fail("the run round onto the east band stalled at %s" % str(_position()))
	# Gone back to a pest on the way (her rage cools as she climbs): sprints up
	# and down the band till she is dangerous again, and a few strides behind
	# (at the climber's heels her shove throws them anywhere, not at the rail).
	if int(_matron_state()["stage"]) < STAGE_DANGEROUS:
		var raged: bool = await _act_laps(device, ACT_LAP_SOUTH, ACT_LAP_NORTH,
			func() -> bool: return int(_matron_state()["stage"]) >= STAGE_DANGEROUS and \
				_act_her_xz().distance_to(_act_me_xz()) > 6.0, 150.0, "chase_to_dangerous")
		if not raged:
			return _fail("the chase never made her dangerous (rage %.3f)" % float(_matron_state()["rage"]))
	_act_log("dangerous")
	# By the rail, still: she comes (off her pipe, a listen, then on), and
	# shoves at the shaft.
	# A sprint there, heard all the way from her pipe: she knows where to come.
	if not (await _sprint_to(device, ACT_RIM, 1.0, 8.0) and await _walk_to(device, ACT_RIM, 0.3, 3.0)):
		return _fail("the run to the shaft's rail stalled at %s" % str(_position()))
	var shoves := int(_matron_state()["shoves"])
	var x0 := _position().x
	var watch := {"nearest": x0}
	var shoved: bool = await _act_watch_her(func() -> bool:
		watch["nearest"] = minf(float(watch["nearest"]), _position().x)
		return int(_matron_state()["shoves"]) > shoves and int(_matron_state()["stage"]) == STAGE_DANGEROUS, 60.0,
		"dangerous_at_rail", "act_dangerous_coming", 4.0)
	if not shoved:
		return _fail("dangerous, she never shoved the climber at the rail (stage %d)" % int(_matron_state()["stage"]))
	await _act_wait(func() -> bool:
		watch["nearest"] = minf(float(watch["nearest"]), _position().x)
		return false, 0.6, "rail")
	await _act_frame_her()
	await _act_pose("act_dangerous_shove")
	_act_log("rail_shove dx=%.2f" % (float(watch["nearest"]) - x0))
	if float(watch["nearest"]) > x0 - 0.3:
		return _fail("dangerous, her shove did not throw the climber at the shaft (dx %.2f)" % (
			float(watch["nearest"]) - x0))
	# Away from the rail before her next shove (the next could put the climber
	# over it), and away from her: come up her pipe north of the climber, she
	# presses on along the rail, and straight out from it is into her.
	var away := ((_act_me_xz() - _act_her_xz()).normalized() + Vector2(1.0, 0.0)).normalized()
	if not await _sprint_to(device, _act_me_xz() + away * 5.0, 0.8, 4.0):
		return _fail("the break away from the rail stalled at %s (she at %s)" % [str(_position()),
			str(_matron_state()["position"])])
	# Fully changed, her hands kill: the one death the act means. It can come
	# in the same stride she changes.
	_act_deaths_allowed = 1
	var catches := int(_matron_state()["catches"])
	var raged_more: bool = await _act_laps(device, ACT_LAP_SOUTH, ACT_LAP_NORTH,
		func() -> bool: return int(_matron_state()["stage"]) == STAGE_CHANGED or int(_native().get_death_count()) >= 1,
		150.0, "chase_to_changed")
	if not raged_more:
		return _fail("the chase never changed her fully (rage %.3f)" % float(_matron_state()["rage"]))
	if int(_native().get_death_count()) >= 1:
		if int(_matron_state()["catches"]) <= catches:
			return _fail("the climber died in the chase, not in her hands (%s)" % str(_native().get_death_state()))
		# Only fully changed are her hands a death.
		_act_stages_seen[STAGE_CHANGED] = true
	_act_log("changed")
	var killed: bool = await _act_watch_her(func() -> bool: return int(_native().get_death_count()) >= 1, 30.0,
		"changed_kill", "act_changed_coming", 3.0)
	if not killed:
		return _fail("fully changed, she never caught the climber")
	if int(_matron_state()["catches"]) <= catches:
		return _fail("the climber died, but not in her hands (%s)" % str(_native().get_death_state()))
	await _seconds(0.5)
	_act_log("killed")
	return true


# Back on 143, the checkpoint (frozen once she turned dangerous over the
# climber: a deck her pipe joins to hers is hers too): up the riser again
# while she looks for the climber where she killed them, quietly to her
# ladder and up it to the 176
# ring; on by the climbing route, no lift, to the 220 ring (its boards are
# narrower than she is: she stays on the ring).
func _act_way_up(device: int) -> bool:
	if _position().y > 150.0:
		return _fail("the death did not put the climber back below her deck (at %s)" % str(_position()))
	if not await _act_up_riser(device):
		return false
	_act_log("back_on_her_deck")
	for leg in [Vector2(3.5, -128.2), Vector2(3.5, -129.8), Vector2(5.0, -129.6)]:
		if not await _act_creep_to(device, leg, 0.08 if leg.x > 4.0 else 0.15, 10.0, 0.5):
			return _fail("the creep to the ladder's foot stalled at %s (deaths %d)" % [str(_position()),
				int(_native().get_death_count())])
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s' facing the ladder, not CLIMB HOLD" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the ladder")
	_move(device, 1.0)
	var on_ring: bool = await _act_wait(func() -> bool: return _standing_above(176.5), 40.0, "ladder")
	_move(device, 0.0)
	if not on_ring or int(_native().get_death_count()) != 1:
		return _fail("the climb did not top out onto the 176 ring (y %.2f, deaths %d, she climbing %s)" % [
			_position().y, int(_native().get_death_count()), str(_matron_state()["climbing"])])
	_act_log("on_176")
	# A look back down the ladder: in her Turn she took hold, and comes up it.
	if _act_her_xz().distance_to(Vector2(5.0, -129.55)) < 2.5:
		await _face(Vector2(0.0, -1.0))
		await _tilt(-1.2)
		await _act_pose("act_up_the_ladder_after_you")
		await _tilt(0.0)
	# On by the climbing route, no lift: the boards out from the ring to the
	# standpipe and up it to 198; the catwalk under the scaffold panel, a jump
	# for it, and up it to 220.
	for leg in [Vector2(7.05, -128.4), Vector2(7.05, -130.9), Vector2(7.6, -130.9)]:
		if not await _walk_to(device, leg, 0.08 if leg.x > 7.5 else 0.1, 8.0, true):
			return _fail("along the boards to the standpipe stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing the standpipe, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the standpipe")
	_move(device, 1.0)
	var on_198: bool = await _act_wait(func() -> bool: return _standing_above(198.5), 40.0, "standpipe")
	_move(device, 0.0)
	if not on_198:
		return _fail("the standpipe did not top out onto the 198 ring (y %.2f)" % _position().y)
	if not (await _walk_to(device, Vector2(11.0, -129.3), 0.1, 16.0, true) and \
			await _walk_to(device, Vector2(11.0, -131.4), 0.08, 6.0, true)):
		return _fail("out along the catwalk under the scaffold stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _leap(device, 0.6, func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 3.0):
		return _fail("the jump for the scaffold panel never caught it (at %s)" % str(_position()))
	_move(device, 1.0)
	var on_220: bool = await _act_wait(func() -> bool: return _standing_above(220.5), 40.0, "scaffold")
	_move(device, 0.0)
	if not on_220:
		return _fail("the scaffold panel did not top out onto the 220 ring (y %.2f)" % _position().y)
	_act_log("on_220")
	if int(_native().get_death_count()) != 1:
		return _fail("the climb on from her deck cost a death (deaths %d)" % int(_native().get_death_count()))
	for stage in [STAGE_PEST, STAGE_DANGEROUS, STAGE_CHANGED]:
		if not _act_stages_seen.has(stage):
			return _fail("she never reached stage %d" % stage)
	await _act_pose("act_220")
	return true


# Knock her down, then throw what you carry (the owner's goal, 2026-10-19):
# a sprint by her locker wakes her; the toolbox by the north rail taken up;
# as a pest comes for the climber (upright), the view kept on where she will
# be a moment on, a swipe up held on the glass reads THROW and draws the
# throw's path; lifted, the toolbox flies and she goes down. A few seconds on
# the steel, up again, nobody dead.
const HEAVE_BOX_PICK := Vector2(-4.4, -125.85)
const MATRON_STATE_DOWN := 11

func _matron_heave_touch(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _sprint_to(device, Vector2(8.0, -128.5), 1.0, 6.0):
		return _fail("the sprint toward her locker stalled at %s" % str(_position()))
	if not await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 3.0):
		return _fail("the sprint by her locker did not wake her (state %d)" % int(_matron_state()["state"]))
	if not await _walk_to(device, HEAVE_BOX_PICK, 0.08, 14.0):
		return _fail("the walk to the toolbox stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP"):
		return _fail("Action read '%s' at the toolbox, not PICK UP" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) != 0, 1.0):
		return _fail("PICK UP did not put the toolbox in the hands")
	# Still, until a pest comes for the climber in sight: from 9 m the view
	# kept level on her, leading her.
	var last: Vector3 = _matron_state()["position"]
	var close := false
	var waited := 0.0
	while waited < 90.0 and not close:
		await get_tree().process_frame
		var dt := maxf(get_process_delta_time(), 1.0e-4)
		waited += dt
		var m := _matron_state()
		var her: Vector3 = m["position"]
		var lead := her + (her - last) / dt * 0.22 - _position()
		last = her
		var d := Vector2(her.x - _position().x, her.z - _position().z).length()
		if int(m["state"]) == MATRON_STATE_HUNTING and int(m["stage"]) == 0 and d < 9.0:
			_main._yaw = atan2(-lead.x, -lead.z)
			_main._pitch = 0.0
			close = d < 3.4
	if not close:
		return _fail("no pest came for the climber (state %d, stage %d)" % [int(_matron_state()["state"]),
			int(_matron_state()["stage"])])
	if _main._touch.gesture_hint(&"up") != "THROW":
		return _fail("with the toolbox in the hands, up read '%s', not THROW" % _main._touch.gesture_hint(&"up"))
	var throws := int(_native().get_throw_count())
	var index := _swipe(-1.0, true)
	await _frames(2)
	var drawn: bool = _main._throw_arc.visible and _main._throw_path.size() >= 3
	_lift(index)
	if not drawn:
		return _fail("the throw's path was not drawn while the thumb swiped")
	if not await _wait_until(func() -> bool: return int(_native().get_throw_count()) == throws + 1, 0.5):
		return _fail("the swipe up, lifted, did not throw the toolbox")
	var deaths := int(_native().get_death_count())
	if not await _wait_until(func() -> bool: return int(_matron_state()["knockdowns"]) >= 1, 1.5):
		return _fail("the toolbox did not put her down (blow %.3f m/s, state %d)" % [
			float(_matron_state()["blow_mps"]), int(_matron_state()["state"])])
	await _pose("matron_heave_down")
	if not await _wait_until(func() -> bool: return int(_matron_state()["state"]) != MATRON_STATE_DOWN, 7.0):
		return _fail("she never got up again")
	if int(_native().get_death_count()) != deaths or not bool(_matron_state()["alive"]):
		return _fail("a death in the heave (deaths %d, she alive %s)" % [int(_native().get_death_count()),
			str(_matron_state()["alive"])])
	_detail = "blow_mps=%.3f knockdowns=%d throws=%d" % [float(_matron_state()["blow_mps"]),
		int(_matron_state()["knockdowns"]), int(_native().get_throw_count())]
	return true



# Her corner pipe (the owner's goal, 2026-10-19: she comes down the shaft
# after you; one shaft pipe is hers to climb; you hear her at least 4 s before
# she arrives; a drum dropped on her knocks her off without killing her).
# Asleep, the drum by the north band's rail carried quietly along the band and
# set down by the shaft's edge over her pipe, west of its flanges; its clank
# (or a sprint by her locker) wakes her. The line down to 143 while she strains, and
# crouched round under her post. Out, sprints along 143's shaft edge make her
# dangerous: she takes the corner pipe down after the climber, a knock on the
# steel at every hold. Up the riser to her deck, and she comes back up the pipe
# after; as she climbs, a shove sends the drum off the edge onto her: torn
# off, she slams onto the flange below, alive, and nobody is dead.
const PIPE_DRUM_PICK := Vector2(-15.0, -125.95)
# The drum waits in the north band's corner by the shaft, east of where she
# comes along the band to her pipe (walking the edge she barges through
# whatever lies on it): set down from here, facing south, and taken up again
# from 0.75 m off where it lies.
const PIPE_DRUM_SET_FROM := Vector2(16.6, -128.4)
# At the rail over her line (x 15.15), west of the riser's flanges (to x
# 15.25): the drum held out under the rail's bar, thrown down past them.
const PIPE_THROW_FROM := Vector2(14.75, -132.55)
const PIPE_WARNING_S := 4.0

var _pipe_drum := 0
var _pipe_drum_rest := Vector3.ZERO   # where it was set down
# Her first knock on the pipe and her feet on 143 after it, in process frames
# (-1 till then), kept by _pipe_watching while `on`.
var _pipe_watch := {}


func _matron_pipe_touch(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _pipe_drum_to_corner(device):
		return false
	if int(_matron_state()["state"]) == MATRON_STATE_DORMANT and not await _wake_matron(device):
		return false
	if not await _walk_to(device, Vector2(2.5, -130.4), 0.3, 10.0, true):
		return _fail("the walk back to the line's ramp stalled at %s" % str(_position()))
	if not await _act_wake_and_go_below(device):
		return false
	if not await _act_wait(func() -> bool: return bool(_matron_state()["released"]), 40.0, "pipe_out"):
		return _fail("she never came out of her locker")
	if not await _pipe_down_after_you(device):
		return false
	if not await _pipe_drum_onto_her(device):
		return false
	var m := _matron_state()
	_detail = "warned_s=%.2f holds=%d flanges=%d slams=%d slam_mps=%.2f knockdowns=%d" % [
		float(_pipe_watch["warned_s"]), int(m["pipe_holds"]), int(m["pipe_flanges"]), int(m["pipe_slams"]),
		float(m["pipe_slam_mps"]), int(m["knockdowns"])]
	return true


# The drum from the north band's rail, carried along the band (quietly: she
# sleeps on) and set down by the shaft's edge over her pipe.
func _pipe_drum_to_corner(device: int) -> bool:
	if not await _act_creep_to(device, PIPE_DRUM_PICK, 0.08, 14.0, 0.5):
		return _fail("the walk to the drum stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP"):
		return _fail("Action read '%s' at the drum, not PICK UP" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) != 0, 1.0):
		return _fail("PICK UP did not put the drum in the hands")
	_pipe_drum = int(_native().get_carrying_entity_id())
	# Laden, the full walk is a slow one, its steps far under the pace that
	# carries to her locker.
	for leg in [Vector2(-15.0, -128.0), Vector2(PIPE_DRUM_SET_FROM.x, -128.0), PIPE_DRUM_SET_FROM]:
		if not await _act_creep_to(device, leg, 0.12 if leg == PIPE_DRUM_SET_FROM else 0.3, 60.0, 1.0):
			return _fail("the carry along the band to her pipe stalled at %s (%.2f m/s)" % [str(_position()),
				Vector2(_velocity().x, _velocity().z).length()])
	if int(_matron_state()["state"]) != MATRON_STATE_DORMANT:
		return _fail("the carry woke her (state %d)" % int(_matron_state()["state"]))
	await _face(Vector2(0.0, -1.0))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the drum down")
	await _seconds(1.0)
	var drum := _pipe_drum_at()
	print("SCRAPERX_PIPE drum set down at (%.2f, %.2f, %.2f), her state %d" % [drum.x, drum.y, drum.z,
		int(_matron_state()["state"])])
	# (The turn to set it down swings it: where it lies is near enough, in the
	# corner and off her way along the edge.)
	if drum.y < MATRON_DECK_Y or Vector2(drum.x, drum.z).distance_to(PIPE_DRUM_SET_FROM + Vector2(0.0, -0.6)) > 0.8:
		return _fail("the drum did not come to rest in the corner by the shaft (at %s)" % str(drum))
	_pipe_drum_rest = drum
	return true


func _pipe_drum_at() -> Vector3:
	return _native().get_kit_body_transform(_native().get_kit_body_index(_pipe_drum)).origin


# Kept every frame while _pipe_watch["on"]: her first knock on the pipe, and
# her feet on 143 off it after that.
func _pipe_watching() -> void:
	while bool(_pipe_watch["on"]):
		var m := _matron_state()
		var frame := int(Engine.get_process_frames())
		if int(_pipe_watch["first"]) < 0 and int(m["pipe_holds"]) > int(_pipe_watch["holds0"]):
			_pipe_watch["first"] = frame
			_pipe_watch["clangs0"] = int(_main._audio.matron_cues.get(&"clang", 0))
		if int(_pipe_watch["first"]) >= 0 and int(_pipe_watch["arrived"]) < 0 and not bool(m["on_pipe"]) and \
				absf(float(m["floor_y"]) - 143.0) < 0.1:
			_pipe_watch["arrived"] = frame
			_pipe_watch["clangs"] = int(_main._audio.matron_cues.get(&"clang", 0)) - int(_pipe_watch["clangs0"])
		await get_tree().process_frame


# Sprints along 143's shaft edge by the gate in its rail, heard up the shaft,
# until she takes the corner pipe down after the climber, knocking at every
# hold (4 s and more before her feet are on 143: _pipe_heard_coming). Through
# the gate and up the riser to her deck meanwhile: off the pipe she stands and
# listens, hears the climber on the riser, and goes back up it after them.
func _pipe_down_after_you(device: int) -> bool:
	await _crouch_on(device, false)
	_pipe_watch = {"on": true, "holds0": int(_matron_state()["pipe_holds"]), "first": -1, "arrived": -1,
		"clangs0": 0, "clangs": 0, "warned_s": 0.0}
	_pipe_watching()
	var links := int(_matron_state()["pipe_links"])
	var took: bool = await _act_laps(device, Vector2(-3.0, ACT_143_EDGE_Z), Vector2(-10.5, ACT_143_EDGE_Z),
		func() -> bool: return int(_matron_state()["pipe_links"]) > links, 200.0, "pipe_rage")
	if not took:
		_pipe_watch["on"] = false
		return _fail("sprints under her never brought her down the pipe (stage %d, rage %.3f)" % [
			int(_matron_state()["stage"]), float(_matron_state()["rage"])])
	_act_log("pipe_taken")
	if not await _act_up_riser(device):
		_pipe_watch["on"] = false
		return false
	return true


# Her coming down the pipe, as the climber heard it: from her first knock on
# it to her feet on 143, and the knocks heard in between.
func _pipe_heard_coming() -> bool:
	_pipe_watch["on"] = false
	if int(_pipe_watch["arrived"]) < 0:
		return _fail("she never came off the pipe onto 143 (first knock at frame %d)" % int(_pipe_watch["first"]))
	_pipe_watch["warned_s"] = float(int(_pipe_watch["arrived"]) - int(_pipe_watch["first"])) / 60.0
	print("SCRAPERX_PIPE down: first knock to her feet on 143 %.2f s, %d knocks heard" % [
		float(_pipe_watch["warned_s"]), int(_pipe_watch["clangs"])])
	if float(_pipe_watch["warned_s"]) < PIPE_WARNING_S:
		return _fail("her first knock on the pipe came only %.2f s before she was on 143" % float(_pipe_watch["warned_s"]))
	if int(_pipe_watch["clangs"]) < 8:
		return _fail("only %d knocks were heard from her on the pipe" % int(_pipe_watch["clangs"]))
	return true


# Back along the band to the drum by the shaft's edge while she comes up the
# pipe after the climber: taken up, held out under the rail over her, the eyes
# down the pipe. With her a few metres below the deck and climbing, a swipe up
# throws it down onto her (knock her down, then throw what you carry): torn
# off, she slams onto the flange below her, alive.
func _pipe_drum_onto_her(device: int) -> bool:
	var take_from := Vector2(_pipe_drum_rest.x, _pipe_drum_rest.z + 0.75)
	if not (await _sprint_to(device, Vector2(take_from.x, -127.2), 1.0, 10.0) and \
			await _walk_to(device, take_from, 0.08, 4.0)):
		return _fail("the run back round to the drum stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"pick_up", "PICK UP"):
		return _fail("Action read '%s' at the drum by the edge, not PICK UP (drum at %s, me at %s, carry target %d, pitch %.2f)" % [
			_action_label(), str(_pipe_drum_at()), str(_position()), int(_native().get_carry_target_entity_id()),
			float(_main._pitch)])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == _pipe_drum, 1.0):
		return _fail("PICK UP did not put the drum in the hands")
	await _tilt(-1.2)
	if not await _walk_to(device, PIPE_THROW_FROM, 0.08, 4.0):
		return _fail("the step to the rail with the drum stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	var held := _pipe_drum_at()
	print("SCRAPERX_PIPE held out at (%.2f, %.2f, %.2f), me %s" % [held.x, held.y, held.z, str(_position())])
	var links := int(_matron_state()["pipe_links"])
	var climbing: bool = await _act_wait(func() -> bool:
		var m := _matron_state()
		var her: Vector3 = m["position"]
		return bool(m["on_pipe"]) and float(m["floor_y"]) < 150.0 and her.y > 148.6 and her.y < 151.0, 40.0,
		"pipe_up")
	if not climbing:
		return _fail("she never came up the pipe below the drum (on it %s, at %s, links %d from %d)" % [
			str(_matron_state()["on_pipe"]), str(_matron_state()["position"]), int(_matron_state()["pipe_links"]),
			links])
	if not _pipe_heard_coming():
		return false
	_act_log("pipe_throw")
	if _main._touch.gesture_hint(&"up") != "THROW":
		return _fail("with the drum in the hands, up read '%s', not THROW" % _main._touch.gesture_hint(&"up"))
	var slams := int(_matron_state()["pipe_slams"])
	var deaths := int(_native().get_death_count())
	var throws := int(_native().get_throw_count())
	var index := _swipe(-1.0, true)
	await _frames(2)
	_lift(index)
	if not await _wait_until(func() -> bool: return int(_native().get_throw_count()) == throws + 1, 0.5):
		return _fail("the swipe up, lifted, did not throw the drum")
	# How near the drum came to her, across, as it fell past her height.
	var pass_by := {"nearest": INF}
	var hit: bool = await _wait_until(func() -> bool:
		var her: Vector3 = _matron_state()["position"]
		var d := _pipe_drum_at()
		if absf(d.y - her.y) < 1.2:
			pass_by["nearest"] = minf(float(pass_by["nearest"]), Vector2(d.x - her.x, d.z - her.z).length())
		return int(_matron_state()["pipe_slams"]) > slams, 4.0)
	_act_log("pipe_slam")
	if not hit:
		return _fail("the drum did not knock her off the pipe (blow %.3f m/s, nearest across %.2f m, drum at %s)" % [
			float(_matron_state()["blow_mps"]), float(pass_by["nearest"]), str(_pipe_drum_at())])
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_pipe_slam")
	if not bool(_matron_state()["alive"]) or int(_native().get_death_count()) != deaths:
		return _fail("the drop killed someone (she alive %s, deaths %d)" % [str(_matron_state()["alive"]),
			int(_native().get_death_count())])
	return true
