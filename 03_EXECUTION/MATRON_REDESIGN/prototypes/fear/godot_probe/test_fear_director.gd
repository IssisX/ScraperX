extends SceneTree
# Headless check of the fear director's rules on a scripted chase (no native).
# Run: godot --headless --script test_fear_director.gd
# Each segment holds her state and the player for some seconds at 60 fps.

const Director := preload("res://fear_director.gd")
var _fails := 0


func _her(state: int, pos: Vector3, extra := {}) -> Dictionary:
	var d := {"state": state, "stage": 0, "released": state != 0 and state != 1, "alive": true,
		"sees_player": false, "position": pos, "rage": 0.1, "shoves": 0, "catches": 0, "climbing": false}
	d.merge(extra, true)
	return d


func _cam(at: Vector3, look: Vector3) -> Transform3D:
	return Transform3D(Basis.looking_at(look - at, Vector3.UP), at)


func _run(dir, seconds: float, her: Dictionary, ctx: Dictionary) -> void:
	var frames := int(seconds * 60.0)
	for i in frames:
		dir.update(1.0 / 60.0, her, ctx)


func _expect(dir, kind: String, n: int) -> void:
	var got := int(dir.counts.get(kind, 0))
	if got != n:
		_fails += 1
		print("FEAR_TEST FAIL %s expected %d got %d" % [kind, n, got])
	else:
		print("FEAR_TEST ok %s = %d" % [kind, n])


func _init() -> void:
	var dir = Director.new()
	var player := Vector3(11.0, 155.0, -128.8)
	var still := {"player": player, "velocity": Vector3.ZERO, "camera": _cam(player + Vector3(0, 0.6, 0), Vector3(11, 155, -126)),
		"radio": "", "deaths": 0, "fear_clip": "", "fear_line": "", "climbing": false, "hour": 16.3}
	# Asleep: the player stands quiet at her locker for 50 s: three pleas, 20 s apart.
	_run(dir, 50.0, _her(0, Vector3(11, 155.05, -126.2)), still)
	_expect(dir, "locker_plea", 3)
	# He moves on; the locker bursts, Brenner says his name while she is out and near.
	var ctx := still.duplicate()
	ctx["player"] = Vector3(0.0, 155.0, -128.5)
	ctx["camera"] = _cam(Vector3(0.0, 155.6, -128.5), Vector3(5.0, 155.6, -128.5))
	ctx["radio"] = "What was that? Navarro, get away from that locker."
	_run(dir, 2.0, _her(2, Vector3(11, 155.05, -127.0)), ctx)
	_expect(dir, "name_learned", 1)
	ctx["radio"] = ""
	# Hunting him unseen 9 m off for 12 s: the echo of the radio's tail comes 10 s on.
	_run(dir, 12.0, _her(3, Vector3(9, 155.05, -128.5)), ctx)
	_expect(dir, "echo", 1)
	# She sees him (after more than 3 s unseen): one freeze, none while she keeps seeing.
	_run(dir, 3.0, _her(3, Vector3(5, 155.05, -128.5), {"sees_player": true}), ctx)
	_expect(dir, "freeze", 1)
	# He lands hard near her as a pest: rage jumps 0.03 in a frame -> one hush; a second jump
	# 5 s later is inside the 40 s quiet and says nothing.
	_run(dir, 0.5, _her(3, Vector3(5, 155.05, -128.5), {"rage": 0.10}), ctx)
	_run(dir, 0.5, _her(3, Vector3(5, 155.05, -128.5), {"rage": 0.13}), ctx)
	_run(dir, 5.0, _her(3, Vector3(5, 155.05, -128.5), {"rage": 0.16}), ctx)
	_expect(dir, "shh", 1)
	# Peering for him, unseen, 8 m: "Mr. Navarro?"
	_run(dir, 3.0, _her(8, Vector3(8, 155.05, -128.5), {"rage": 0.16}), ctx)
	_expect(dir, "call_name", 1)
	# A dangerous shove with him 2 m from the shaft's rim (x 19 on the east band): "Mind the edge."
	var rim_ctx := ctx.duplicate()
	rim_ctx["player"] = Vector3(19.0, 155.0, -150.0)
	_run(dir, 0.1, _her(3, Vector3(19.8, 155.05, -150.0), {"stage": 1, "rage": 0.5, "shoves": 0}), rim_ctx)
	_run(dir, 0.1, _her(3, Vector3(19.8, 155.05, -150.0), {"stage": 1, "rage": 0.5, "shoves": 1}), rim_ctx)
	_expect(dir, "shove_line", 1)
	print("FEAR_TEST shove line was: %s" % str(dir.cues[-1]))
	# Close behind him (1.5 m at his back) for 6 s: her breath in his hand, about 6 times.
	var back := ctx.duplicate()
	back["player"] = Vector3(0.0, 155.0, -128.5)
	back["camera"] = _cam(Vector3(0.0, 155.6, -128.5), Vector3(5.0, 155.6, -128.5))
	_run(dir, 6.0, _her(5, Vector3(-1.5, 155.05, -128.5), {"stage": 1, "rage": 0.5}), back)
	print("FEAR_TEST haptic breaths in 6 s behind at 1.5 m: %d" % dir._haptic_breaths)
	# A catch (fully changed) at 16:20: the pupil check says the time.
	_run(dir, 0.1, _her(3, Vector3(0.5, 155.05, -128.5), {"stage": 2, "rage": 0.9, "catches": 0}), back)
	_run(dir, 0.1, _her(3, Vector3(0.5, 155.05, -128.5), {"stage": 2, "rage": 0.9, "catches": 1}), back)
	_expect(dir, "pupil_check", 1)
	print("FEAR_TEST pupil check said: %s" % str(dir.cues[-1]["line"]))
	# A fall death with "Help! Help me! Help!" playing; next life he is 11 m above her, she
	# searches one deck down: his own yell comes up the shaft, once.
	var died := back.duplicate()
	died["deaths"] = 1
	died["fear_clip"] = "terror_10.ogg"
	died["fear_line"] = "Help! Help me! Help!"
	_run(dir, 0.1, _her(4, Vector3(21.5, 144.05, -150.0), {"stage": 1}), died)
	var above := died.duplicate()
	above["player"] = Vector3(21.5, 155.0, -150.0)
	_run(dir, 40.0, _her(4, Vector3(21.5, 144.05, -150.0), {"stage": 1}), above)
	_expect(dir, "mimic_armed", 1)
	_expect(dir, "mimic", 1)
	print("FEAR_TEST mimic cue: %s" % str(dir.cues[-1]))
	# On the ladder: her 1.5 m under him.
	var ladder := above.duplicate()
	ladder["climbing"] = true
	ladder["player"] = Vector3(5.0, 158.0, -129.55)
	_run(dir, 1.0, _her(3, Vector3(5.0, 156.5, -129.55), {"stage": 2, "climbing": true}), ladder)
	_expect(dir, "ladder_hang", 1)
	print(dir.summary())
	print("FEAR_TEST %s fails=%d" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(0 if _fails == 0 else 1)
