extends "res://presentation/ui/ui_test_uncanny.gd"
# The Matron look suite (tools/look/look.sh): stills of her in her key
# moments, each from a camera placed about her own drawn body and framing all
# of her (and the climber, in her hands), never the player's eye. Renders
# only, not in CI, and no check: look_matron_moves, look_matron_hold and
# look_matron_fatal.
#
# Each set puts her where a moment starts with the native tests' own setup
# hooks (the bridge's TEST-ONLY debug_*: placed on her deck, her stage, her
# ladder or pipe taken, a noise heard, the climber set by her rail, his arm
# already gone) and then lets the simulation take her into the moment for
# real: her walk, her run on all fours, her climb, her hands on him. Nothing
# poses her figure. Drawing is off between stills (the simulation runs on),
# and at a still the simulation is held for its few frames.
#
# Each still is <capture prefix>_<shot>.png, with a line in
# <capture prefix>_index.tsv: shot, sim time, her state, what it shows, the
# wall seconds it took, and what was measured of her there.

const LookHoldCam := preload("res://presentation/life/matron_hold_camera.gd")
const LOOK_FOV := 50.0
const LOOK_HOLD_FRAMES := 2
const LOOK_MARGIN := 1.5
const LOOK_MIN_M := 1.6     # pulled in for room, never nearer than this
# The corner pipe's riser (world_solids.inc Riser, src/sim/matron.hpp): 0.7 m
# square about (15.8, -134.2), from grade to 154.
const LOOK_RISER_MIN := Vector2(15.45, -134.55)
const LOOK_RISER_MAX := Vector2(16.15, -133.85)
const LOOK_DECK := 154.0
const LOOK_STAND_CENTRE := 1.05   # her capsule's centre over her soles (Matron::kStandCentre)
const LOOK_STATE_GRAB := 12
const LOOK_STATE_DANGLE := 14
const LOOK_STATE_ARMPLAY := 15

var _look_cam: Camera3D = null
var _look_wall_ms := 0
var _look_hidden: Array = []


func _look_set(set_name: String) -> bool:
	if not _native().has_method("debug_place_matron"):
		return _fail("this native library has no look hooks (debug_place_matron): build scraperx_native from a tree that has them")
	_look_wall_ms = 0
	# By day, so she is lit.
	_main._sky_cycle.set_hour(15.0)
	_look_render(false)
	_native().prepare_matron()
	await _look_sim(1.0)
	var ok := false
	match set_name:
		"moves":
			ok = await _look_moves()
		"hold":
			ok = await _look_hold(false)
		"fatal":
			ok = await _look_hold(true)
	_look_render(true)
	return ok


# --- her moves: walking upright, on all fours, her ladder, her pipe ----------

func _look_moves() -> bool:
	var n: Object = _native()
	# A pest (stage 0) walks upright: down her east band to a noise 20 m off.
	n.debug_place_matron(Vector3(20.6, LOOK_DECK, -162.0))
	n.debug_set_matron_stage(0)
	await _look_sim(0.3)
	n.debug_matron_hear(Vector3(20.6, LOOK_DECK, -140.0))
	if not await _look_until(func() -> bool: return float(_matron_state()["speed"]) > 0.6, 6.0, 0.6):
		return _fail("walk: she never walked (speed %.2f, state %d)" % [float(_matron_state()["speed"]), int(_matron_state()["state"])])
	await _look_shot("walk", "her walk upright, a pest (stage 0) going to a noise down her east band", [55.0, -55.0], 12.0)
	# Dangerous (stage 1), at a run she goes down on all fours.
	n.debug_place_matron(Vector3(20.6, LOOK_DECK, -166.0))
	n.debug_set_matron_stage(1)
	await _look_sim(0.3)
	n.debug_matron_hear(Vector3(20.6, LOOK_DECK, -136.0))
	var fours := func() -> bool:
		n.debug_set_matron_stage(1)
		return float(_matron_state()["speed"]) > 1.4 and float(_main._matron_view._w_fours) > 0.9
	if not await _look_until(fours, 8.0, 0.6):
		return _fail("crawl: never on all fours (speed %.2f, fours %.2f)" % [float(_matron_state()["speed"]),
			float(_main._matron_view._w_fours)])
	await _look_shot("crawl", "on all fours at a run, dangerous (stage 1), down her east band", [90.0, -90.0], 10.0)
	# Her ladder, 154 to the 176 ring: up it, 3 m off the deck.
	n.debug_place_matron(Vector3(5.0, LOOK_DECK, -129.55))
	n.debug_set_matron_stage(0)
	await _look_sim(0.35)
	if not bool(n.debug_matron_climb_way(false, false)):
		return _fail("ladder: she would not take her ladder")
	var up := func() -> bool:
		var m := _matron_state()
		return bool(m["climbing"]) and Vector3(m["position"]).y - LOOK_STAND_CENTRE > LOOK_DECK + 3.0
	if not await _look_until(up, 20.0, 0.0):
		return _fail("ladder: never 3 m up it (climbing %s, at %s)" % [str(_matron_state()["climbing"]),
			str(_matron_state()["position"])])
	await _look_shot("ladder", "on her ladder from the 154 deck to the 176 ring, 3 m up it", [160.0, -160.0, 120.0, -120.0], 6.0)
	# Her corner pipe, 154 down to 143: over the north rail and down its
	# riser, hands at her grip, feet braced on it (S6), 3.5 m down.
	n.debug_place_matron(Vector3(14.85, LOOK_DECK, -132.55))
	await _look_sim(0.35)
	if not bool(n.debug_matron_climb_way(true, true)):
		return _fail("pipe: she would not take her pipe")
	var down := func() -> bool:
		var m := _matron_state()
		return bool(m["on_pipe"]) and Vector3(m["position"]).y - LOOK_STAND_CENTRE < LOOK_DECK - 3.5
	if not await _look_until(down, 30.0, 0.0):
		return _fail("pipe: never 3.5 m down it (on_pipe %s, at %s)" % [str(_matron_state()["on_pipe"]),
			str(_matron_state()["position"])])
	await _look_shot("pipe", "on her corner pipe going down 154 to 143, from the shaft behind her: hands at her grip, feet braced on the riser",
		[150.0, -150.0, 180.0], 8.0)
	await _look_shot("pipe_side", "the same moment from her open side, the riser's face edge on: her hands and feet against it",
		[90.0, -90.0, 115.0, -115.0], 4.0)
	return true


# --- her hands on him: the grab, upside down, the rip; the fatality ----------

# A still climber by her east band's rail, her 1.6 m along it, dangerous
# (the native tests' at_the_rail). His arm already gone (fatal), her catch is
# the fatality.
func _look_hold(fatal: bool) -> bool:
	var n: Object = _native()
	if fatal:
		n.debug_lose_arm(0)
	n.debug_launch_player(Vector3(18.0, LOOK_DECK + 0.95, -159.5), Vector3.ZERO)
	await _look_sim(1.0)
	n.debug_place_matron(Vector3(18.3, LOOK_DECK, -157.9))
	var hold := func() -> Dictionary:
		n.debug_set_matron_stage(1)
		return n.get_matron_hold_state()
	if not fatal:
		if not await _look_until(func() -> bool: return bool(hold.call()["held"]), 15.0, 0.12):
			return _fail("grab: her hand never closed on him (state %d)" % int(_matron_state()["state"]))
		await _look_shot("grab", "her grab: her hand just closed on his ankle by her east band's rail", [60.0, -60.0], 15.0, true)
		var upside_down := func() -> bool:
			var h: Dictionary = hold.call()
			var up: Vector3 = Quaternion(h["body_rotation"]) * Vector3.UP
			return int(h["state"]) == LOOK_STATE_DANGLE and up.y < -0.8
		if not await _look_until(upside_down, 20.0, 0.6):
			return _fail("hold: never held out upside down (state %d)" % int(hold.call()["state"]))
		await _look_shot("hold", "the hold: him upside down by the ankle, held out over the drop", [70.0, -70.0, 110.0, -110.0], 12.0, true)
		if not await _look_until(func() -> bool: return int(hold.call()["arm_tear_count"]) >= 1, 40.0, 0.08):
			return _fail("rip: no arm torn (state %d, beat %d)" % [int(hold.call()["state"]), int(hold.call()["armplay_beat"])])
		await _look_shot("rip", "the arm rip: his arm just torn from the shoulder in her other hand", [60.0, -60.0, 110.0, -110.0], 12.0, true)
		await _look_sim_with(1.0, hold)
		await _look_shot("rip_after", "a second after the rip: the arm hers, shown", [60.0, -60.0, 110.0, -110.0], 12.0, true)
		return true
	var window := func() -> bool:
		var h: Dictionary = hold.call()
		return bool(h["fatal_hang"]) and float(h["fatal_window_s"]) >= 0.6
	if not await _look_until(window, 25.0, 0.0):
		return _fail("hang: no fatal hang window (path %d, state %d)" % [int(hold.call()["fatal_path"]), int(hold.call()["state"])])
	await _look_shot("hang", "the fatality's hang window: him one-armed, upside down out over the drop, his punch's chance", [70.0, -70.0, 110.0, -110.0], 12.0, true)
	var beat0 := int(hold.call()["fatal_release_beat"])
	if not await _look_until(func() -> bool: return int(hold.call()["fatal_release_beat"]) > beat0, 15.0, 0.08):
		return _fail("let-go: she never let go (state %d)" % int(hold.call()["state"]))
	await _look_shot("let_go", "the let-go: her fingers just opened, him leaving her hand over the drop", [70.0, -70.0, 110.0, -110.0], 12.0, true)
	await _look_sim_with(0.35, hold)
	await _look_shot("let_go_fall", "0.35 s after the let-go: him falling away below her hand", [70.0, -70.0, 110.0, -110.0], 30.0, true)
	return true


# --- the camera and the still ------------------------------------------------

func _look_render(on: bool) -> void:
	if DisplayServer.get_name() != "headless":
		RenderingServer.render_loop_enabled = on


func _sim_s() -> float:
	return float(_native().get_simulation_time_seconds())


func _look_sim(seconds: float) -> void:
	var until := _sim_s() + seconds
	while _sim_s() < until:
		await get_tree().process_frame


func _look_sim_with(seconds: float, each: Callable) -> void:
	var until := _sim_s() + seconds
	while _sim_s() < until:
		each.call()
		await get_tree().process_frame


# Waits (simulated seconds) till `condition` holds, then `after` more.
func _look_until(condition: Callable, timeout: float, after: float) -> bool:
	var until := _sim_s() + timeout
	while not bool(condition.call()):
		if _sim_s() >= until:
			return false
		await get_tree().process_frame
	var settle := _sim_s() + after
	while _sim_s() < settle:
		condition.call()
		await get_tree().process_frame
	return true


func _skel_points(skel: Skeleton3D, into: PackedVector3Array) -> void:
	for i in skel.get_bone_count():
		into.append(skel.global_transform * skel.get_bone_global_pose(i).origin)


# Her drawn bones (and his, in her hands; his torn arm): what the frame holds.
func _look_points(with_him: bool) -> PackedVector3Array:
	var pts := PackedVector3Array()
	var rig: RefCounted = _main._matron_view._rig
	if rig != null and rig.skel != null:
		_skel_points(rig.skel, pts)
	if with_him and _main._gore != null and _main._gore.victim != null and _main._gore.victim.visible:
		_skel_points(_main._gore.victim.body.skel, pts)
	if pts.is_empty():
		pts.append(Vector3(_matron_state()["render_position"]))
	return pts


# The camera for a still: about the frame's centre, `az` degrees round from
# her facing (0 in front of her, 90 at her left), `el` up, as far as all of
# it fits the view; of the azimuths offered, the first with the room behind
# it (the native's look rays), else the one with the most.
func _look_place(azimuths: Array, el: float, with_him: bool) -> Dictionary:
	var pts := _look_points(with_him)
	var lo := pts[0]
	var hi := pts[0]
	for p in pts:
		lo = lo.min(p)
		hi = hi.max(p)
	var centre := (lo + hi) * 0.5
	var radius := 0.3
	for p in pts:
		radius = maxf(radius, p.distance_to(centre))
	# Her skin, dress and hair stand out past her bones, and her near side is
	# drawn larger: half again the bones' sphere.
	var dist := maxf(radius / sin(deg_to_rad(LOOK_FOV) * 0.5) * LOOK_MARGIN, 2.0)
	var yaw := float(_matron_state()["render_yaw"])
	var facing := Vector3(sin(yaw), 0.0, cos(yaw))
	var best := {}
	for az in azimuths:
		var flat := facing.rotated(Vector3.UP, deg_to_rad(float(az)))
		var dir := (flat * cos(deg_to_rad(el)) + Vector3.UP * sin(deg_to_rad(el))).normalized()
		var start := centre + dir * minf(radius, 1.0)
		var free := minf(radius, 1.0) + float(LookHoldCam.cone_cast(_native(), start, centre + dir * dist, 0.25, Vector3.INF))
		var room := minf(free - 0.3, dist)
		if best.is_empty() or room > float(best["room"]) + 0.05:
			best = {"az": float(az), "dir": dir, "room": room}
		if room >= dist - 0.01:
			break
	var used := clampf(float(best["room"]), minf(LOOK_MIN_M, dist), dist)
	return {"from": centre + Vector3(best["dir"]) * used, "at": centre, "az": float(best["az"]),
		"dist": used, "fit_dist": dist, "radius": radius}


func _look_shot(shot: String, what: String, azimuths: Array, el: float, with_him := false) -> void:
	var wall0 := _look_wall_ms
	_main.freeze_sim = true
	if _look_cam == null:
		_look_cam = Camera3D.new()
		_look_cam.name = "LookCamera"
		_look_cam.fov = LOOK_FOV
		_look_cam.near = 0.05
		_look_cam.far = 2000.0
		_main.add_child(_look_cam)
	# Her pose and his drawn for this held tick, then the camera on them.
	await _frames(1)
	var place := _look_place(azimuths, el, with_him)
	_look_cam.look_at_from_position(place["from"], place["at"], Vector3.UP)
	_look_cam.current = true
	_look_hidden.clear()
	for child in _main.get_children():
		if child is CanvasLayer and (child as CanvasLayer).visible:
			(child as CanvasLayer).visible = false
			_look_hidden.append(child)
	_look_render(true)
	var path := "%s_%s.png" % [_capture_prefix, shot]
	var saved := false
	if not _capture_prefix.is_empty() and DisplayServer.get_name() != "headless":
		for i in LOOK_HOLD_FRAMES:
			await RenderingServer.frame_post_draw
		DirAccess.make_dir_recursive_absolute(path.get_base_dir())
		saved = get_viewport().get_texture().get_image().save_png(path) == OK
	_look_render(false)
	for layer in _look_hidden:
		(layer as CanvasLayer).visible = true
	_main._camera.current = true
	_main.freeze_sim = false
	_look_wall_ms = Time.get_ticks_msec()
	var m := _matron_state()
	var notes := _look_notes(shot, m)
	var wall_s := float(_look_wall_ms - wall0) / 1000.0
	var line := "%s\t%.2f\t%d\t%d\t%s\t%.1f\t%s\taz=%.0f dist=%.2f fit=%.2f%s" % [shot, _sim_s(), int(m["state"]),
		int(m["stage"]), what, wall_s, path.get_file() if saved else "(not drawn)", float(place["az"]),
		float(place["dist"]), float(place["fit_dist"]), notes]
	print("SCRAPERX_LOOK_SHOT %s" % line)
	if not _capture_prefix.is_empty():
		var index_path := "%s_index.tsv" % _capture_prefix
		DirAccess.make_dir_recursive_absolute(index_path.get_base_dir())
		var f := FileAccess.open(index_path, FileAccess.READ_WRITE if FileAccess.file_exists(index_path) else FileAccess.WRITE)
		if f != null:
			f.seek_end()
			f.store_line(line)
			f.close()


# What was measured of her at a still: on her pipe, how far each drawn wrist
# and ankle is from the riser's faces (across the ground), and what the
# native's contacts say.
func _look_notes(shot: String, m: Dictionary) -> String:
	var rig: RefCounted = _main._matron_view._rig
	if rig == null:
		return ""
	var s := ""
	if bool(m.get("on_pipe", false)):
		s += " on_pipe=1 hand_held=%d hand2_held=%d braced=%d" % [int(bool(m["contact_hand_held"])),
			int(bool(m["contact_hand2_held"])), int(bool(m["contact_braced"]))]
		for bone in ["wr_l", "wr_r", "an_l", "an_r"]:
			var at: Vector3 = rig.bone_world(bone).origin
			var nearest := Vector2(clampf(at.x, LOOK_RISER_MIN.x, LOOK_RISER_MAX.x), clampf(at.z, LOOK_RISER_MIN.y, LOOK_RISER_MAX.y))
			s += " %s_to_riser=%.3f" % [bone, Vector2(at.x, at.z).distance_to(nearest)]
	if bool(m.get("climbing", false)):
		s += " climbing=1 move=%s" % str(m["climb_move"])
	s += " speed=%.2f fours=%.2f" % [float(m["speed"]), float(_main._matron_view._w_fours)]
	if shot in ["grab", "hold", "rip", "rip_after", "hang", "let_go", "let_go_fall"]:
		var h: Dictionary = _native().get_matron_hold_state()
		var up: Vector3 = Quaternion(h["body_rotation"]) * Vector3.UP
		s += " held=%d his_up_y=%.2f arm_tears=%d fatal_hang=%d window_s=%.2f" % [int(bool(h["held"])), up.y,
			int(h["arm_tear_count"]), int(bool(h["fatal_hang"])), float(h["fatal_window_s"])]
	return s
