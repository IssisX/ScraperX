extends SceneTree
## A planted foot stays planted: walking her through the studio, a foot on
## the deck must not travel with the body. The M6 S0 probe measured walking
## planted feet travelling ~0.037 m/frame median with slides to 54.7 m: the
## rig's contact pass rode the body onto the lowest foot but never held a
## foot's place on the deck while upright (its world-locked plants only ran
## on all fours). This test drives the same walk in the studio and fails
## while a planted foot rides the body. Standing still is the control: feet
## already still there, so a control failure means the harness is broken,
## not the rig.

const MatronView := preload("res://presentation/life/matron_view.gd")
const DT := 1.0 / 60.0
const PLANT_M := 0.03
const MEDIAN_BAR_M := 0.01
const SLIDE_BAR_M := 0.5
const SETTLE_FRAMES := 30
const WALK_FRAMES := 180
const CONTROL_FRAMES := 60

var _cam: Camera3D
var _view: Node3D
var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("MATRON_FOOT_PLANT %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _data(base: Dictionary) -> Dictionary:
	var d := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 1, "sees_player": false,
		"alive": true, "released": true, "position": Vector3(0.0, MatronView.STAND_CENTRE, 0.0), "yaw": 0.0,
		"exposure": 0.0, "floor_y": 0.0}
	for k in base:
		d[k] = base[k]
	return d


func _sole(rig: RefCounted, s: String) -> Dictionary:
	var heel: Vector3 = rig.point("an_" + s, "heel")
	var toe: Vector3 = rig.point("an_" + s, "toe")
	return {"y": minf(heel.y, toe.y), "xz": Vector2((heel.x + toe.x) * 0.5, (heel.z + toe.z) * 0.5)}


func _median(values: Array) -> float:
	if values.is_empty():
		return 0.0
	var sorted := values.duplicate()
	sorted.sort()
	var n := sorted.size()
	if n % 2 == 1:
		return float(sorted[n / 2])
	return (float(sorted[n / 2 - 1]) + float(sorted[n / 2])) * 0.5


# Walk one scenario, measuring planted-foot travel exactly as the S0 probe
# does: consecutive frames where the sole stays within PLANT_M of the deck.
func _walk(name: String, base: Dictionary, frames: int) -> void:
	var rig: RefCounted = _view._rig
	var data := _data(base)
	var speed := float(base["speed"])
	var pos: Vector3 = data["position"]
	var stride := float(base.get("stride", 0.0))
	var player := Vector3(0.0, 1.6, 50.0)
	for i in SETTLE_FRAMES:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, player)
		await process_frame
	var stats := {}
	for s in ["l", "r"]:
		stats[s] = {"ds": [], "slides": [], "skates": 0, "pairs": 0, "cur": 0.0}
	var prev := {}
	for i in frames:
		pos += Vector3(0.0, 0.0, speed * DT)
		stride += speed * DT
		data["position"] = pos
		data["stride"] = stride
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, player)
		await process_frame
		var allow := maxf(2.5 * speed * DT, 0.05)
		for s in ["l", "r"]:
			var sole := _sole(rig, s)
			var st: Dictionary = stats[s]
			var planted := absf(float(sole["y"])) < PLANT_M
			if planted and prev.has(s):
				var d := float((sole["xz"] as Vector2).distance_to(prev[s] as Vector2))
				st["ds"].append(d)
				st["pairs"] += 1
				st["cur"] += d
				if d > allow:
					st["skates"] += 1
			elif not planted and float(st["cur"]) > 0.0:
				st["slides"].append(float(st["cur"]))
				st["cur"] = 0.0
			if planted:
				prev[s] = sole["xz"]
			else:
				prev.erase(s)
	for s in ["l", "r"]:
		var st: Dictionary = stats[s]
		if float(st["cur"]) > 0.0:
			st["slides"].append(float(st["cur"]))
		var big: Array = []
		for slide in st["slides"]:
			if float(slide) > 0.05:
				big.append(float(slide))
		var longest := 0.0
		for slide in big:
			longest = maxf(longest, float(slide))
		var med := _median(st["ds"])
		var ok_pairs := int(st["pairs"]) >= 50
		var ok_med := med <= MEDIAN_BAR_M
		var ok_slide := longest <= SLIDE_BAR_M
		var ok_skate := int(st["skates"]) == 0
		_check(ok_pairs and ok_med and ok_slide and ok_skate, name + "_foot_" + s,
			"pairs %d median %.4f m longest %.3f m skates %d" % [int(st["pairs"]), med, longest, int(st["skates"])])


# A slow walk (gait under the pin's 0.15 m/s bar): after the proof pin
# has seated a foot, the touchdown landing must start from the foot's
# drawn height — never lift the seated foot back up to the 0.15 m
# tracking lift and land it again (Fix 1). The old code ran the five
# frame landing from UPLANT_TRACK_LIFT_M, so the drawn sole rose to
# 0.036-0.040 m during landing frames; the fixed code starts from the
# drawn height, so it stays under 0.020 m. The landing frames are read
# off the rig's own plant state (the same object the test drives) — the
# asserted value is the drawn sole's height, which is the behavior.
# The pin's own first-frame drop (Fix 2) has no studio signal: in this
# harness the pin engages when the drawn sole is already within 0.02 m
# of the deck, so its drop is 0.017 m on the old code and 0.024 m on the
# fixed code — govern, not the pin, sets both numbers. Fix 2 is verified
# by the grab-settle act run, not by this test.
const LANDING_HEIGHT_BAR_M := 0.025


func _landing_no_relift_walk() -> void:
	var rig: RefCounted = _view._rig
	var data := _data({"state": 5, "speed": 0.12})
	var pos: Vector3 = data["position"]
	var stride := 0.0
	var player := Vector3(0.0, 1.6, 50.0)
	for i in SETTLE_FRAMES:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, player)
		await process_frame
	var max_y := {"l": 0.0, "r": 0.0}
	for i in 750:
		pos += Vector3(0.0, 0.0, 0.12 * DT)
		stride += 0.12 * DT
		data["position"] = pos
		data["stride"] = stride
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, player)
		await process_frame
		var ups: Dictionary = rig._uplants
		for s in ["l", "r"]:
			var st: Dictionary = ups.get(s, {})
			if int(st.get("landing", 0)) > 0:
				var sole := _sole(rig, s)
				max_y[s] = maxf(float(max_y[s]), float(sole["y"]))
	for s in ["l", "r"]:
		_check(float(max_y[s]) <= LANDING_HEIGHT_BAR_M, "landing_no_relift_foot_" + s,
			"max sole height during landing %.4f m" % float(max_y[s]))


# Taking off with a foot planted: the plant must release over the release
# blend, from where the foot is drawn — never jump to the pose in one
# frame (Fix 3). The body rises gently (1.2 m/s, just over the airborne
# bar) so the rise itself does not swamp the measurement. The check is
# the foot's distance from the pose sole, in skeleton space, five frames
# into the rise: the old code cleared the plants on the takeoff frame,
# so the foot was already at the pose (0.003 m); the fixed code is still
# mid-blend (0.060 m). A single-frame jump bar does NOT pin this — the
# jump is dominated by the airborne pose/ride transition (0.082 m, both
# feet, one frame, no plant state, present in the old code too), so the
# old code measured 0.101 m and the fixed code 0.099 m on the same bar.
# The distance from the pose is the blend itself, and the pose motion
# cancels out of it.
const TAKEOFF_POSE_DIST_BAR_M := 0.03


func _takeoff() -> void:
	var rig: RefCounted = _view._rig
	var data := _data({"state": 7, "speed": 0.0})
	var pos: Vector3 = data["position"]
	var player := Vector3(0.0, 1.6, 50.0)
	# Long settle: the slow walk before this ends mid-stride, and the
	# walk-to-stand pose blend must finish (feet re-planted) before the
	# rise starts, or the blend's own motion inflates the takeoff jump.
	for i in 180:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, player)
		await process_frame
	var dist_at_5 := {}
	for i in 30:
		pos += Vector3(0.0, 0.02, 0.0)
		data["position"] = pos
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, player)
		await process_frame
		if i == 5:
			var inv: Transform3D = rig.skel.global_transform.affine_inverse()
			for s in ["l", "r"]:
				# The true sole midpoint, not the _sole min-y: the
				# distance is mostly vertical, and the min-y would
				# cancel it against the pose sole's own min-y.
				var heel: Vector3 = rig.point("an_" + s, "heel")
				var toe: Vector3 = rig.point("an_" + s, "toe")
				var drawn_mid := (heel + toe) * 0.5
				var skel_sole: Vector3 = inv * drawn_mid
				var pose_sole: Dictionary = rig._sole_skel(s)
				var pose_mid: Vector3 = pose_sole["mid"]
				var pose_skel := Vector3(pose_mid.x, float(pose_sole["y"]), pose_mid.z)
				dist_at_5[s] = (skel_sole - pose_skel).length()
	for s in ["l", "r"]:
		_check(float(dist_at_5[s]) >= TAKEOFF_POSE_DIST_BAR_M, "takeoff_foot_" + s,
			"distance from pose sole at frame 5 %.4f m" % float(dist_at_5[s]))


func _run() -> void:
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true
	_cam.global_position = Vector3(0.0, 1.5, 3.2)
	_view = MatronView.new()
	root.add_child(_view)
	_view._build_figure()
	_view._build_sparks()
	var rig: RefCounted = _view._rig
	_check(rig != null, "loads", "the body and its rig")
	if rig == null:
		quit(1)
		return
	# Control first: standing still, feet must already be still. If this
	# fails, the harness (not the rig) is broken.
	await _walk("control_stand", {"state": 7, "speed": 0.0}, CONTROL_FRAMES)
	# The measured defect, at two speeds of normal walking. The pest hobble
	# is NOT a scenario here on purpose: the pose describes it as "one leg
	# dragging a beat behind the other" (matron_view.gd) — that dragging
	# leg is characterization, not the measured defect, and this fix must
	# not erase it.
	await _walk("shuffle", {"state": 5, "speed": 1.0}, WALK_FRAMES)
	# Faster gait, longer walk: at speed the sole is flat fewer frames,
	# and each step spends frames hovering and landing, so the walk runs
	# longer to keep the planted-pair sample meaningful (the pairs guard
	# stays 50; the walk grows, the bar does not shrink).
	await _walk("shuffle_fast", {"state": 5, "speed": 3.0}, 420)
	# Slow enough for the proof pin: the touchdown landing must start
	# from the foot's drawn height, never lift a seated foot and land
	# it again (Fix 1). Taking off with a foot planted: the plant must
	# release over the blend, never snap to the pose (Fix 3).
	await _landing_no_relift_walk()
	await _takeoff()
	quit(1 if _failures else 0)
