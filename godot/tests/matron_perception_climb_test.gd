extends SceneTree
const MatronView := preload("res://presentation/life/matron_view.gd")

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var native = ClassDB.instantiate("ScraperXSimulation")
	if native == null:
		quit(1)
		return
	native.advance_frame(1.0 / 90.0)
	var state: Dictionary = native.get_matron_state()
	for key in ["lead_x", "lead_z", "lead_age_s", "lead_seen", "lead_valid", "acoustic_queries", "climb_hand", "climb_force_n", "climb_grips", "climb_releases", "mantling"]:
		if not state.has(key):
			push_error("MATRON_BRIDGE missing " + key)
			quit(1)
			return
	var view := MatronView.new()
	root.add_child(view)
	view._build_figure()
	var acoustic := {"sees_player": false, "lead_valid": true, "lead_x": 3.0, "lead_z": 5.0, "lead_floor_y": 154.0}
	if view.perceived_target(acoustic, Vector3(100, 100, 100)) != view.perceived_target(acoustic, Vector3(-100, -100, -100)):
		push_error("MATRON_ATTENTION hidden player steers head")
		quit(1)
		return
	var rig = view._rig
	var wrist: Vector3 = rig._gpos("wr_r")
	var target := wrist + Vector3(0.06, 0.08, 0.02)
	var world_target: Vector3 = rig.skel.global_transform * target
	rig.contact({"state": 3, "climbing": true, "climb_hand": world_target}, false)
	if rig._gpos("wr_r").distance_to(target) > 0.03:
		push_error("MATRON_CONTACT hand did not follow native anchor")
		quit(1)
		return
	print("MATRON_PERCEPTION_CLIMB PASS: native bridge fields and rig hand anchor")
	quit(0)
