extends SceneTree
## The hold's mixer effect must not outlive its director. Restarting through
## the pause menu reloads the scene (main.gd), destroying the director while
## the global AudioServer buses survive. Before the fix, each restart left the
## old AudioEffectAmplify on the Ambience bus and the new director added
## another, so the duck accumulated and ambience stayed quieter outside holds.
##   godot --headless --path godot --script res://tests/hold_mix_leak_probe.gd

const AudioDirector := preload("res://presentation/audio/audio_director.gd")

var _fails := 0


func _initialize() -> void:
	var root := get_root()
	# No director yet: the Ambience bus does not exist until _ready makes it.
	# _ready runs on the next frame, not on add_child during _initialize.
	var first := AudioDirector.new()
	root.add_child(first)
	await process_frame
	var ambience := AudioServer.get_bus_index(&"Ambience")
	if ambience < 0:
		_fail("bus", "Ambience bus missing after first director _ready")
		quit(1)
		return
	if _amps(ambience) != 0:
		_fail("baseline", "%d amps on Ambience before any hold" % _amps(ambience))
	else:
		print("HOLD_MIX_LEAK no amp before the hold: PASS")
	# Her hands have him for a full second: the duck reaches its -8 dB floor.
	for _f in 60:
		first.hold_mix(1.0 / 60.0, true)
	if _amps(ambience) != 1:
		_fail("add", "%d amps on Ambience during the hold (want 1)" % _amps(ambience))
	else:
		print("HOLD_MIX_LEAK one amp during the hold: PASS")
	# Restart: the scene reloads, the director leaves the tree, the bus stays.
	# queue_free deletes at the end of the frame; _exit_tree fires with it.
	first.queue_free()
	await process_frame
	await process_frame
	if _amps(ambience) != 0:
		_fail("orphan", "%d amps on Ambience after the director left (want 0)" % _amps(ambience))
	else:
		print("HOLD_MIX_LEAK no orphan after the director left: PASS")
	# A fresh director holds again: it must not add a second amp.
	var second := AudioDirector.new()
	root.add_child(second)
	await process_frame
	for _f in 60:
		second.hold_mix(1.0 / 60.0, true)
	if _amps(ambience) != 1:
		_fail("restart", "%d amps on Ambience after restart (want 1)" % _amps(ambience))
	else:
		print("HOLD_MIX_LEAK one amp after restart: PASS")
	# The hold ends: the duck must reach silence, not sit under an orphan.
	for _f in 120:
		second.hold_mix(1.0 / 60.0, false)
	var duck := _duck_db(ambience)
	if absf(duck) > 0.01:
		_fail("duck", "Ambience still ducked %.2f dB after release (want 0)" % duck)
	else:
		print("HOLD_MIX_LEAK duck released to 0 dB: PASS")
	second.queue_free()
	await process_frame
	print("HOLD_MIX_LEAK %s" % ("PASS" if _fails == 0 else "FAIL"))
	quit(1 if _fails > 0 else 0)


func _amps(bus: int) -> int:
	var n := 0
	for i in AudioServer.get_bus_effect_count(bus):
		if AudioServer.get_bus_effect(bus, i) is AudioEffectAmplify:
			n += 1
	return n


func _duck_db(bus: int) -> float:
	var total := 0.0
	for i in AudioServer.get_bus_effect_count(bus):
		var effect := AudioServer.get_bus_effect(bus, i)
		if effect is AudioEffectAmplify:
			total += effect.volume_db
	return total


func _fail(check_name: String, detail: String) -> void:
	_fails += 1
	print("HOLD_MIX_LEAK %s FAIL: %s" % [check_name, detail])
