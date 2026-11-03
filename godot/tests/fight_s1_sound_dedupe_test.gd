extends SceneTree

## Fighting system slice S1, sound follow-up (ChatGPT FIX on PR #153):
## one landed blow must produce exactly one matron_hit cue. Before the fix,
## a blow that staggered or downed her cued matron_hit twice in one frame:
## once on the stagger/knockdown edge and again on the blow_mps edge.
## Run:
## tools/godot/godot --headless --path godot \
## --script res://tests/fight_s1_sound_dedupe_test.gd
## Proved by cue counts, not by listening: the VM has no audio device.

const MatronSound := preload("res://presentation/audio/matron_sound.gd")

const DT := 1.0 / 30.0

var _failures := 0


class StubAudio extends Node:
	var cues := [] # [name, at, volume_db, pitch]
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
	print("FIGHT_S1_DEDUPE %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _data() -> Dictionary:
	return {
		"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 0,
		"sees_player": false, "alive": true, "released": true,
		"position": Vector3(0.0, 1.0, 0.0), "yaw": 0.0,
		"exposure": 0.0, "floor_y": 0.0, "vaults": 0, "pipe_slams": 0,
		"knockdowns": 0, "staggers": 0, "shoves": 0, "throws": 0,
		"leaps": 0, "climbs": 0, "blow_mps": 0.0,
	}


func _hits(stub: StubAudio) -> Array:
	return stub.cues.filter(func(c: Array) -> bool: return c[0] == &"matron_hit")


func _heavies(stub: StubAudio) -> Array:
	return stub.cues.filter(func(c: Array) -> bool: return c[0] == &"hit_heavy")


func _run() -> void:
	var feet := Vector3(0.0, 0.0, 0.0)
	var head := Vector3(0.0, 1.6, 0.0)
	var player_pos := Vector3(0.0, 0.0, 3.0)

	# ---- A staggering blow: one matron_hit, the stagger's own, not two.
	var stub := StubAudio.new()
	var sound := MatronSound.new(stub)
	var data := _data()
	sound.update(data, DT, true, feet, head, player_pos)
	_check(stub.cues.is_empty(), "prime", "first observation fires nothing")
	data["staggers"] = 1
	data["blow_mps"] = 0.5
	sound.update(data, DT, true, feet, head, player_pos)
	var hits := _hits(stub)
	_check(hits.size() == 1, "stagger-one-hit", "%d matron_hit(s) on a staggering blow" % hits.size())
	if hits.size() == 1:
		_check(float(hits[0][2]) == -8.0, "stagger-hit-kept",
			"surviving hit is the stagger's (-8.0 dB), got %.1f" % float(hits[0][2]))

	# ---- A downing blow: one matron_hit, the knockdown's own, not two.
	# Its body boom (hit_heavy) is a different sound and must survive.
	stub = StubAudio.new()
	sound = MatronSound.new(stub)
	data = _data()
	sound.update(data, DT, true, feet, head, player_pos)
	data["knockdowns"] = 1
	data["blow_mps"] = 0.5
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 1, "down-one-hit", "%d matron_hit(s) on a downing blow" % hits.size())
	if hits.size() == 1:
		_check(float(hits[0][2]) == -2.0, "down-hit-kept",
			"surviving hit is the knockdown's (-2.0 dB), got %.1f" % float(hits[0][2]))
	_check(_heavies(stub).size() == 1, "down-boom-kept",
		"%d hit_heavy(s) on a downing blow" % _heavies(stub).size())

	# ---- A downing blow whose knockdown edge is silenced by a pipe slam:
	# the blow edge is then the only hit, still exactly one.
	stub = StubAudio.new()
	sound = MatronSound.new(stub)
	data = _data()
	sound.update(data, DT, true, feet, head, player_pos)
	data["pipe_slams"] = 1
	data["knockdowns"] = 1
	data["blow_mps"] = 0.5
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 1, "slammed-down-one-hit",
		"%d matron_hit(s) on a slammed downing blow" % hits.size())

	# ---- A plain landed blow (no stagger, no knockdown): still one grunt.
	# The fix must not silence the S1 grunt itself.
	stub = StubAudio.new()
	sound = MatronSound.new(stub)
	data = _data()
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.227
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 1, "plain-one-hit", "%d matron_hit(s) on a plain blow" % hits.size())
	data["blow_mps"] = 0.0
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.5
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 2, "plain-second-hit",
		"%d matron_hit(s) after a second plain blow" % hits.size())

	# ---- A stagger that lands frames after the blow's first contact (Codex
	# on #173): the native adds a blow up tick by tick, so the first frame can
	# grunt before the stagger edge arrives. Still one matron_hit per window.
	stub = StubAudio.new()
	sound = MatronSound.new(stub)
	data = _data()
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.2
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.6
	data["staggers"] = 1
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 1, "late-stagger-one-hit",
		"%d matron_hit(s) when the stagger lands a frame after first contact" % hits.size())

	# ---- The same with a knockdown: one matron_hit, and its body boom kept.
	stub = StubAudio.new()
	sound = MatronSound.new(stub)
	data = _data()
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.3
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.9
	data["knockdowns"] = 1
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 1, "late-down-one-hit",
		"%d matron_hit(s) when the knockdown lands a frame after first contact" % hits.size())
	_check(_heavies(stub).size() == 1, "late-down-boom-kept",
		"%d hit_heavy(s) on a late knockdown" % _heavies(stub).size())

	# ---- Once the window closes (blow back to zero), the next blow sounds again.
	data["blow_mps"] = 0.0
	sound.update(data, DT, true, feet, head, player_pos)
	data["blow_mps"] = 0.4
	data["staggers"] = 1
	sound.update(data, DT, true, feet, head, player_pos)
	hits = _hits(stub)
	_check(hits.size() == 2, "next-window-sounds",
		"%d matron_hit(s) after a second blow in a new window" % hits.size())

	print("FIGHT_S1_DEDUPE %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(1 if _failures else 0)
