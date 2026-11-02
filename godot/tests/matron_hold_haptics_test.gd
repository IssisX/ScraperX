extends SceneTree

const Feel = preload("res://presentation/main_feel.gd")
const Settings = preload("res://presentation/ui/settings_store.gd")
var failures := 0

func check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		print("FAIL ", description)

func _initialize() -> void:
	var feel = Feel.new()
	feel._settings = Settings.new()
	feel._settings.vibration = true
	feel._ci_mode = true
	var state := {"grabs": 4, "arm_tears": 2, "fatal_hang": true}
	feel._feel_hold_events(state)
	check(feel.haptic_counts.is_empty(), "first observation")
	state = {"grabs": 4, "arm_tears": 2, "fatal_hang": false}
	feel._feel_hold_events(state)
	state = {"grabs": 5, "arm_tears": 3, "fatal_hang": true}
	feel._feel_hold_events(state)
	check(feel.haptic_counts == {&"matron_grab": 1, &"matron_tear": 1, &"matron_hang": 1}, "each event bypasses throttle")
	feel._feel_hold_events(state)
	check(feel.haptic_counts.values().reduce(func(a, b): return a + b, 0) == 3, "unchanged state")
	state = {"grabs": 0, "arm_tears": 0, "fatal_hang": false}
	feel._feel_hold_events(state)
	check(feel.haptic_counts.values().reduce(func(a, b): return a + b, 0) == 3, "counter reset")
	state = {"grabs": 1, "arm_tears": 1, "fatal_hang": true}
	feel._feel_hold_events(state)
	check(feel.haptic_counts == {&"matron_grab": 2, &"matron_tear": 2, &"matron_hang": 2}, "new encounter")
	feel._settings.vibration = false
	feel._feel_hold_events({"grabs": 1, "arm_tears": 1, "fatal_hang": false})
	feel._feel_hold_events({"grabs": 2, "arm_tears": 2, "fatal_hang": true})
	check(feel.haptic_counts == {&"matron_grab": 2, &"matron_tear": 2, &"matron_hang": 2}, "vibration disabled")
	feel.free()
	print("MATRON_HOLD_HAPTICS %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(1 if failures else 0)
