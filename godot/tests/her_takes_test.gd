extends SceneTree
## Her takes (presentation/audio/her_takes.gd) chosen as the spec's
## "Selection" and "Degenerate banks" checks ask
## (03_EXECUTION/MATRON_REDESIGN/HER_SOUND_VARIATION_SPEC.md): for banks of
## 1 to 8 takes over 10 seeds, 1,000 cues each with the family's cooldown
## past, no take twice running, none within the family's history, none of
## her ring's within her ring, and every round of a bag visiting every take; an
## empty bank silent; a singleton silent while it is the last she played; a
## mixed hold (her families interleaved) never repeats; a cue that finds no
## player commits nothing.
##
##   tools/godot/godot --headless --path godot --script res://tests/her_takes_test.gd

const HerTakes := preload("res://presentation/audio/her_takes.gd")

var _fails := 0


func _init() -> void:
	_selection()
	_degenerate()
	_mixed()
	_uncommitted()
	print("HER_TAKES %s fails=%d" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(0 if _fails == 0 else 1)


func _check(ok: bool, what: String) -> void:
	if not ok:
		_fails += 1
		if _fails <= 20:
			print("HER_TAKES FAIL %s" % what)


# One family alone, cue after cue, its cooldown just past each time.
func _selection() -> void:
	for family in [&"matron_breath", &"matron_effort", &"matron_laugh", &"matron_rip", &"matron_step"]:
		var spec: Array = HerTakes.FAMILIES[family]
		for count in range(2, 9):
			for seed_value in 10:
				var takes := HerTakes.new(seed_value + 1)
				var clock := 0.0
				var played: Array[int] = []
				var silent := 0
				for i in 1000:
					clock += float(spec[1]) + 0.01
					var k := takes.choose(family, count, clock)
					if k < 0:
						silent += 1
						continue
					_check(k >= 0 and k < count, "%s n=%d take %d out of range" % [family, count, k])
					var keep := mini(int(spec[0]), count - 1)
					_check(not (k in played.slice(maxi(0, played.size() - keep))),
						"%s n=%d seed %d: take %d within its last %d" % [family, count, seed_value, k, keep])
					takes.commit(family, k, clock, 0)
					played.append(k)
				_check(played.size() > 0, "%s n=%d seed %d played nothing" % [family, count, seed_value])
				# Every round of count plays visits every take when nothing
				# but its own history holds one back (no ring: her contacts;
				# a ring family once it has more takes than the ring).
				if not HerTakes.in_ring(family) or count > HerTakes.RING:
					for r in played.size() / count:
						var round := played.slice(r * count, (r + 1) * count)
						var seen := {}
						for k in round:
							seen[k] = true
						_check(seen.size() == count, "%s n=%d seed %d: round %d visits %d of %d" % [family, count,
							seed_value, r, seen.size(), count])
				_ring_holds(takes, "%s n=%d seed %d" % [family, count, seed_value])


# No take of a ring family within the ring (the last RING ring starts within
# RING_S), and none twice running, over everything `takes` started. A
# required family (a tear, a blow) with no take outside the ring is still
# heard, never twice running: the ring is not asked of it.
func _ring_holds(takes: RefCounted, what: String) -> void:
	var ring: Array = []
	var last := &""
	for e in takes.started:
		_check(StringName(e["take"]) != last, "%s: %s twice running" % [what, e["take"]])
		last = StringName(e["take"])
		if not HerTakes.in_ring(StringName(e["family"])):
			continue
		var required := bool(HerTakes.FAMILIES[StringName(e["family"])][5])
		for r in ring:
			if required:
				break
			_check(not (r["take"] == e["take"] and float(e["t"]) - float(r["t"]) < HerTakes.RING_S),
				"%s: %s within her ring" % [what, e["take"]])
		ring.append(e)
		if ring.size() > HerTakes.RING:
			ring.pop_front()


func _degenerate() -> void:
	var takes := HerTakes.new(7)
	_check(takes.choose(&"matron_laugh", 0, 0.0) == -1, "an empty bank chose a take")
	_check(takes.choose(&"no_such", 3, 0.0) == -1, "a family not hers chose a take")
	# A singleton: once, then silent while it is the last she played (and in
	# her ring), whatever time passes.
	var k := takes.choose(&"matron_laugh", 1, 0.0)
	_check(k == 0, "a singleton's first cue did not play")
	takes.commit(&"matron_laugh", k, 0.0, 1)
	_check(takes.choose(&"matron_laugh", 1, 20.0) == -1, "a singleton played twice running")
	_check(takes.choose(&"matron_laugh", 1, 5.0) == -1, "a singleton played again inside its cooldown")
	takes.commit(&"matron_breath", takes.choose(&"matron_breath", 3, 20.0), 20.0, 1)
	_check(takes.choose(&"matron_laugh", 1, 20.5) == 0, "a singleton never came back once another played")
	takes.commit(&"matron_effort", takes.choose(&"matron_effort", 1, 30.0), 30.0, 1)
	takes.commit(&"matron_breath", takes.choose(&"matron_breath", 3, 31.0), 31.0, 1)
	_check(takes.choose(&"matron_effort", 1, 32.0) == -1, "a singleton played again inside her ring")
	_check(takes.choose(&"matron_effort", 1, 38.5) == 0, "a singleton never came back once out of her ring")
	# A required pair (a rip) under its cooldown: still heard, never the last.
	var rips := HerTakes.new(3)
	var a := rips.choose(&"matron_rip", 2, 0.0)
	rips.commit(&"matron_rip", a, 0.0, 1)
	var b := rips.choose(&"matron_rip", 2, 0.05)
	_check(b >= 0 and b != a, "a second tear at once was silent or the same rip")
	rips.commit(&"matron_rip", b, 0.05, 1)
	var c := rips.choose(&"matron_rip", 2, 0.1)
	_check(c >= 0 and c != b, "a third tear at once was silent or the rip before it")
	# A required singleton is silent rather than twice running.
	var hits := HerTakes.new(5)
	hits.commit(&"matron_hit", hits.choose(&"matron_hit", 1, 0.0), 0.0, 0)
	_check(hits.choose(&"matron_hit", 1, 0.1) == -1, "a required singleton played twice running")


# Her families in a hold, interleaved at random, over 10 seeds: never twice
# running, never within her ring.
func _mixed() -> void:
	var bank := {&"matron_breath": 3, &"matron_effort": 1, &"matron_laugh": 1, &"matron_swallow": 1,
		&"matron_sniff": 1, &"matron_cluck": 1, &"matron_rip": 2, &"matron_hit": 1, &"matron_shriek": 2,
		&"matron_step": 3, &"matron_plant": 3}
	var names := bank.keys()
	for seed_value in 10:
		var takes := HerTakes.new(100 + seed_value)
		var pick := RandomNumberGenerator.new()
		pick.seed = seed_value
		var clock := 0.0
		for i in 2000:
			clock += pick.randf_range(0.05, 1.5)
			var family: StringName = names[pick.randi_range(0, names.size() - 1)]
			var k := takes.choose(family, int(bank[family]), clock)
			if k >= 0:
				takes.commit(family, k, clock, 1)
		_check(takes.started_total > 200, "mixed seed %d played only %d" % [seed_value, takes.started_total])
		_ring_holds(takes, "mixed seed %d" % seed_value)


# Chosen but never started (no free player, muted): nothing used up.
func _uncommitted() -> void:
	var takes := HerTakes.new(11)
	var k := takes.choose(&"matron_laugh", 1, 0.0)
	_check(takes.choose(&"matron_laugh", 1, 0.0) == k and takes.started_total == 0,
		"a take chosen and never started was used up")
