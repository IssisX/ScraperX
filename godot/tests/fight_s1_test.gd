extends SceneTree

## Fighting system slice S1: every landed blow reads (presentation only).
## A landed punch (50 N-s, no stagger) must produce a visible rig flinch,
## a grunt sound edge scaled by the judged blow, and a landed-blow haptic
## pair; a whiff must produce none of the three. Run:
##   tools/godot/godot --headless --path godot \
##       --script res://tests/fight_s1_test.gd
## The flinch evidence is the torso pivot quaternion the renderer reads
## (peak tip in radians, then decay), not a PNG: frame_post_draw stills
## stall in --script mode, so no stills path lives in this test.

const MatronView := preload("res://presentation/life/matron_view.gd")
const MatronSound := preload("res://presentation/audio/matron_sound.gd")
const Feel := preload("res://presentation/main_feel.gd")
const Settings := preload("res://presentation/ui/settings_store.gd")

const DT := 1.0 / 30.0
const PUNCH_BLOW_MPS := 0.227  # 50 N-s over her 220 kg, no stagger (bar 0.245)

var _failures := 0
var _view: Node3D
var _cam: Camera3D


class StubAudio extends Node:
	var cues := []  # [name, at, volume_db, pitch]
	var matron_encounter := 0

	func matron_cue(name: StringName, at: Vector3, volume_db: float, pitch: float) -> bool:
		cues.append([name, at, volume_db, pitch])
		return true

	func matron_dread(_head: Vector3, _d: float, _stage: int) -> void:
		pass

	func matron_layers(_delta: float, _data: Dictionary, _on: bool, _head: Vector3, _d: float, _y: float) -> void:
		pass

	func matron_cue_one_of(_names: Array, _at: Vector3, _db: float) -> void:
		pass

	func matron_strain(_hand: Vector3, _db: float, _pitch: float) -> void:
		pass

	func arm_break(_at: Vector3) -> void:
		pass

	func player_strike(_edge: StringName, _at: Vector3, _impulse: float) -> void:
		pass


func _initialize() -> void:
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("FIGHT_S1 %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _data() -> Dictionary:
	return {
		"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 0,
		"sees_player": false, "alive": true, "released": true,
		"position": Vector3(0.0, MatronView.STAND_CENTRE, 0.0), "yaw": 0.0,
		"exposure": 0.0, "floor_y": 0.0, "vaults": 0, "pipe_slams": 0,
		"knockdowns": 0, "staggers": 0, "shoves": 0, "throws": 0,
		"leaps": 0, "climbs": 0, "blow_mps": 0.0,
	}


func _settle(data: Dictionary, frames: int) -> void:
	for _i in frames:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, _cam.global_position)
		await process_frame


func _torso_angle(q0: Quaternion) -> float:
	var q: Quaternion = (_view._p["torso"] as Node3D).quaternion
	return (q0.inverse() * q).get_angle()


func _run() -> void:
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true

	# ---- Part A: the rig flinch must be visible, then decay; whiff: none.
	_view = MatronView.new()
	root.add_child(_view)
	_view._build_figure()
	_view._build_sparks()
	_check(_view._rig != null, "flinch-rig", "figure and rig built")
	var data := _data()
	await _settle(data, 24)
	var q0: Quaternion = (_view._p["torso"] as Node3D).quaternion
	_check(_view.flinches == 0, "flinch-quiet", "no flinch before any blow")
	_view.flinch(Vector3(0.0, 0.0, -1.0), 50.0)
	_check(_view.flinches == 1, "flinch-count", "one flinch recorded")
	var peak := 0.0
	for _i in 6:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, _cam.global_position)
		await process_frame
		peak = maxf(peak, _torso_angle(q0))
	_check(peak > 0.035, "flinch-visible", "peak torso tip %.3f rad (> 0.035)" % peak)
	for _i in 60:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, _cam.global_position)
		await process_frame
	_check(_torso_angle(q0) < 0.02, "flinch-decay", "tip after 2 s %.4f rad (< 0.02)" % _torso_angle(q0))
	var q1: Quaternion = (_view._p["torso"] as Node3D).quaternion
	for _i in 10:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, _cam.global_position)
		await process_frame
	var drift := (q1.inverse() * (_view._p["torso"] as Node3D).quaternion).get_angle()
	_check(drift < 0.005, "flinch-whiff", "no flinch without a blow, drift %.4f rad" % drift)

	# ---- Part B: a grunt edge on a landed blow, scaled, edge-triggered.
	var stub := StubAudio.new()
	var sound := MatronSound.new(stub)
	var feet := Vector3(0.0, 0.0, 0.0)
	var head := Vector3(0.0, 1.6, 0.0)
	var player_pos := Vector3(0.0, 0.0, 3.0)
	var sdata := _data()
	sound.update(sdata, DT, true, feet, head, player_pos)
	_check(stub.cues.is_empty(), "grunt-prime", "first observation fires nothing")
	sdata["blow_mps"] = PUNCH_BLOW_MPS
	sound.update(sdata, DT, true, feet, head, player_pos)
	var grunts := stub.cues.filter(func(c: Array) -> bool: return c[0] == &"matron_hit")
	_check(grunts.size() == 1, "grunt-landed", "%d grunt(s) on a landed punch" % grunts.size())
	var v1 := float(grunts[0][2]) if grunts.size() == 1 else 0.0
	_check(v1 < -8.0 and v1 > -13.0, "grunt-quiet", "punch grunt %.1f dB, quieter than a stagger" % v1)
	sdata["blow_mps"] = 0.0
	sound.update(sdata, DT, true, feet, head, player_pos)
	sdata["blow_mps"] = 0.5
	sound.update(sdata, DT, true, feet, head, player_pos)
	grunts = stub.cues.filter(func(c: Array) -> bool: return c[0] == &"matron_hit")
	_check(grunts.size() == 2, "grunt-second", "%d grunts after a second landing" % grunts.size())
	var v2 := float(grunts[1][2]) if grunts.size() == 2 else 0.0
	_check(v2 > v1, "grunt-scaled", "bigger blow grunts louder (%.1f > %.1f dB)" % [v2, v1])
	sound.update(sdata, DT, true, feet, head, player_pos)
	grunts = stub.cues.filter(func(c: Array) -> bool: return c[0] == &"matron_hit")
	_check(grunts.size() == 2, "grunt-held", "held blow fires no repeat grunt")
	sdata["blow_mps"] = 0.0
	sound.update(sdata, DT, true, feet, head, player_pos)
	sound.update(sdata, DT, true, feet, head, player_pos)
	grunts = stub.cues.filter(func(c: Array) -> bool: return c[0] == &"matron_hit")
	_check(grunts.size() == 2, "grunt-whiff", "no grunt without a blow")

	# ---- Part C: landed-blow haptics via the existing HAPTICS path.
	var feel := Feel.new()
	feel._settings = Settings.new()
	feel._settings.vibration = true
	feel._ci_mode = true
	var fcam := Camera3D.new()
	root.add_child(fcam)
	feel._camera = fcam
	var stub2 := StubAudio.new()
	feel._audio = stub2
	feel._ctx = {"strike": {"strikes": 0, "hits": 0, "phase": 0, "impulse": 0.0}}
	feel._feel_strike(DT)
	feel._ctx = {"strike": {"strikes": 1, "hits": 0, "phase": 1, "impulse": 0.0}}
	feel._feel_strike(DT)
	feel._ctx = {"strike": {"strikes": 1, "hits": 1, "phase": 2, "impulse": 50.0,
		"hit_point": Vector3(0.0, 1.4, 0.5), "hit_normal": Vector3(0.0, 0.0, -1.0)}}
	feel._feel_strike(DT)
	_check(feel.haptic_counts.get(&"strike_punch", 0) == 1, "haptic-contact",
		"strike_punch pulses %d" % feel.haptic_counts.get(&"strike_punch", 0))
	_check(feel.haptic_counts.get(&"strike_land", 0) == 1, "haptic-land",
		"strike_land pulses %d" % feel.haptic_counts.get(&"strike_land", 0))
	var feel2 := Feel.new()
	feel2._settings = Settings.new()
	feel2._settings.vibration = true
	feel2._ci_mode = true
	feel2._camera = fcam
	feel2._audio = stub2
	feel2._ctx = {"strike": {"strikes": 0, "hits": 0, "phase": 0, "impulse": 0.0}}
	feel2._feel_strike(DT)
	feel2._ctx = {"strike": {"strikes": 1, "hits": 0, "phase": 1, "impulse": 0.0}}
	feel2._feel_strike(DT)
	feel2._ctx = {"strike": {"strikes": 2, "hits": 0, "phase": 3, "impulse": 0.0}}
	feel2._feel_strike(DT)
	_check(feel2.haptic_counts.is_empty(), "haptic-whiff", "whiff vibrates nothing")
	var feel3 := Feel.new()
	feel3._settings = Settings.new()
	feel3._settings.vibration = false
	feel3._ci_mode = true
	feel3._camera = fcam
	feel3._audio = stub2
	feel3._ctx = {"strike": {"strikes": 1, "hits": 1, "phase": 2, "impulse": 50.0,
		"hit_point": Vector3(0.0, 1.4, 0.5), "hit_normal": Vector3(0.0, 0.0, -1.0)}}
	feel3._feel_strike(DT)
	_check(feel3.haptic_counts.is_empty(), "haptic-off", "vibration off silences fight haptics")

	print("FIGHT_S1 %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(1 if _failures else 0)
