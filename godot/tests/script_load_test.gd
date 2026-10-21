extends SceneTree
## Every script under res://presentation loads (parses and compiles), and the
## native bridge answers get_matron_hold_state().
##   godot --headless --path godot --script res://tests/script_load_test.gd
var _fails := 0

func _walk(dir: String) -> void:
	for f in DirAccess.get_files_at(dir):
		if f.ends_with(".gd"):
			var p := dir.path_join(f)
			var s: Script = load(p)
			if s == null or not s.can_instantiate():
				_fails += 1
				print("SCRIPT_LOAD FAIL ", p)
	for d in DirAccess.get_directories_at(dir):
		_walk(dir.path_join(d))

func _initialize() -> void:
	_walk("res://presentation")
	print("SCRIPT_LOAD %s scripts (%d failed)" % ["PASS" if _fails == 0 else "FAIL", _fails])
	if ClassDB.class_exists("ScraperXSimulation"):
		var sim: Object = ClassDB.instantiate("ScraperXSimulation")
		var ok: bool = sim.has_method("get_matron_hold_state")
		var hold: Dictionary = sim.get_matron_hold_state() if ok else {}
		ok = ok and hold.has("fatality_beat_edges") and (hold["fatality_beat_edges"] as PackedInt64Array).size() == 10
		print("SCRIPT_LOAD %s bridge get_matron_hold_state keys %d" % ["PASS" if ok else "FAIL", hold.size()])
		if not ok:
			_fails += 1
	else:
		print("SCRIPT_LOAD FAIL native class missing")
		_fails += 1
	quit(1 if _fails > 0 else 0)
