extends SceneTree
## Headless check for Fighting S6: stage-driven fight tells presentation
## (pose lean offsets, guard drop, limp weight in MatronView/MatronRig,
## stagger_left_s tell, stage blood soak, and HUD damage pip indicators).

const MatronView = preload("res://presentation/life/matron_view.gd")
const HudOverlay = preload("res://presentation/ui/hud.gd")

const DT := 1.0 / 60.0

var _failures: Array[String] = []
var _passes := 0


func _init() -> void:
	call_deferred("_run")


func _check(cond: bool, label: String, detail: String = "") -> void:
	if cond:
		_passes += 1
		print("FIGHT_S6 PASS: %s" % label)
	else:
		var msg := label if detail.is_empty() else "%s (%s)" % [label, detail]
		_failures.append(msg)
		push_error("FIGHT_S6 FAIL: %s" % msg)


func _base_snap(pos: Vector3, vel: Vector3, state: int = 4) -> Dictionary:
	return {
		"state": state,
		"alive": true,
		"released": true,
		"speed": Vector2(vel.x, vel.z).length(),
		"stride": 0.0,
		"turn": 0,
		"stage": 1,
		"sees_player": true,
		"exposure": 0.0,
		"floor_y": 0.0,
		"position": pos,
		"velocity": vel,
		"vertical_speed": vel.y,
		"yaw": 0.0,
		"pitch": 0.0,
		"crouch": 0.0,
		"grounded": true,
		"climbing": false,
		"on_ladder": false,
		"on_pipe": false,
		"vaulting": false,
		"mantling": false,
		"gap_crossing": false,
		"leap_phase": 0,
		"on_all_fours": false,
		"on_line": false,
		"hold_stage": 0,
		"bracing": false,
		"brace_effort": 0.0,
		"route_mode": 0,
		"time_since_seen": 0.2,
		"last_known_player": pos + Vector3(0.0, 0.0, 4.0),
		"damage_ns": 0.0,
		"player_damage_ns": 0.0,
		"damage_stage": 0,
		"stagger_left_s": 0.0,
	}


func _reset_view(view: Node3D) -> void:
	view._pose_fresh = true
	view._gait_phase = 0.0
	view._w_fours = 0.0
	view._w_fours_v = 0.0
	view.unowned_frames = 0
	if view.get("limp_weight") != null:
		view.set("limp_weight", 0.0)
	if view.get("_limp_v") != null:
		view.set("_limp_v", 0.0)


func _step(view: Node3D, snap: Dictionary, dt: float, player: Vector3) -> void:
	view._clock += dt
	view._state_age += dt
	view._update_figure(snap, dt, player, player)


func _run() -> void:
	var view := MatronView.new()
	root.add_child(view)
	view._build_figure()
	view._build_sparks()

	_test_stage_pose_leans_and_guard_drop(view)
	_test_stagger_left_s_tell(view)
	_test_limp_weight_view_and_rig(view)
	_test_climbing_and_down_invariants(view)
	_test_stage_blood_tell(view)
	_test_hud_damage_indicators()

	view.queue_free()
	print("FIGHT_S6 SUMMARY: %d passed, %d failed" % [_passes, _failures.size()])
	quit(0 if _failures.is_empty() else 1)


func _test_stage_pose_leans_and_guard_drop(view: Node3D) -> void:
	var prev_lean_x := -0.001
	var prev_lean_z := -0.001
	var prev_guard_drop := -0.001
	var stage0_torso_basis := Basis.IDENTITY
	var prev_torso_tilt := -0.001

	for st in range(5):
		_reset_view(view)
		var pos := Vector3(0.0, MatronView.STAND_CENTRE, 0.0)
		var player := Vector3(0.0, MatronView.STAND_CENTRE, 3.5)
		for _i in range(45):
			var snap := _base_snap(pos, Vector3.ZERO, MatronView.STATE_HUNTING)
			snap["damage_stage"] = st
			_step(view, snap, DT, player)

		var lean_v: Variant = view.get("damage_lean")
		var gd_v: Variant = view.get("guard_drop")
		var lean: Vector3 = lean_v if lean_v is Vector3 else Vector3(-1.0, -1.0, -1.0)
		var gd: float = float(gd_v) if gd_v != null else -1.0

		var torso: Node3D = view._p.get("torso", null)
		var torso_basis := torso.transform.basis if torso != null else Basis.IDENTITY
		if st == 0:
			stage0_torso_basis = torso_basis
			_check(lean.length() < 0.001, "stage_0_zero_damage_lean", "lean=%s" % str(lean))
			_check(absf(gd) < 0.001, "stage_0_zero_guard_drop", "guard_drop=%.4f" % gd)
		else:
			_check(lean.x > prev_lean_x + 0.02, "stage_%d_lean_pitch_monotonic" % st,
				"lean_x=%.4f prev=%.4f" % [lean.x, prev_lean_x])
			_check(lean.z > prev_lean_z + 0.02, "stage_%d_lean_list_monotonic" % st,
				"lean_z=%.4f prev=%.4f" % [lean.z, prev_lean_z])
			_check(gd > prev_guard_drop + 0.04, "stage_%d_guard_drop_monotonic" % st,
				"gd=%.4f prev=%.4f" % [gd, prev_guard_drop])

		var tilt := stage0_torso_basis.y.angle_to(torso_basis.y)
		if st > 0:
			_check(tilt > prev_torso_tilt + 0.015, "stage_%d_settled_torso_tilt_increases" % st,
				"tilt=%.4f prev=%.4f" % [tilt, prev_torso_tilt])
		prev_lean_x = lean.x
		prev_lean_z = lean.z
		prev_guard_drop = gd
		prev_torso_tilt = tilt

	# Verify rate limit during abrupt 0 -> 4 stage jump (no snapped pose)
	_reset_view(view)
	var p0 := Vector3(0.0, MatronView.STAND_CENTRE, 0.0)
	var pl := Vector3(0.0, MatronView.STAND_CENTRE, 3.5)
	for _i in range(20):
		var s0 := _base_snap(p0, Vector3.ZERO, MatronView.STATE_HUNTING)
		s0["damage_stage"] = 0
		_step(view, s0, DT, pl)
	var torso_node: Node3D = view._p["torso"]
	var b_before := torso_node.transform.basis
	var s4 := _base_snap(p0, Vector3.ZERO, MatronView.STATE_HUNTING)
	s4["damage_stage"] = 4
	_step(view, s4, DT, pl)
	var b_after := torso_node.transform.basis
	var step_rate := b_before.get_rotation_quaternion().angle_to(b_after.get_rotation_quaternion()) / DT
	_check(step_rate <= 12.0, "stage_jump_respects_pose_rate_limit", "rate=%.2f rad/s" % step_rate)


func _test_stagger_left_s_tell(view: Node3D) -> void:
	_reset_view(view)
	var pos := Vector3(0.0, MatronView.STAND_CENTRE, 0.0)
	var player := Vector3(0.0, MatronView.STAND_CENTRE, 3.5)

	for _i in range(20):
		var snap := _base_snap(pos, Vector3.ZERO, MatronView.STATE_STAGGERING)
		snap["damage_stage"] = 2
		snap["stagger_left_s"] = 0.65
		_step(view, snap, DT, player)

	var stag_lean_v: Variant = view.get("damage_lean")
	var stag_gd_v: Variant = view.get("guard_drop")
	var stag_lean: Vector3 = stag_lean_v if stag_lean_v is Vector3 else Vector3.ZERO
	var stag_gd: float = float(stag_gd_v) if stag_gd_v != null else 0.0
	var stag_s_v: Variant = view.get("stagger_left_s")
	var stag_s: float = float(stag_s_v) if stag_s_v != null else -1.0

	_check(absf(stag_s - 0.65) < 0.001, "stagger_left_s_stored_on_view", "stagger_left_s=%.3f" % stag_s)

	for _i in range(45):
		var snap := _base_snap(pos, Vector3.ZERO, MatronView.STATE_HUNTING)
		snap["damage_stage"] = 2
		snap["stagger_left_s"] = 0.0
		_step(view, snap, DT, player)

	var base_lean_v: Variant = view.get("damage_lean")
	var base_gd_v: Variant = view.get("guard_drop")
	var base_lean: Vector3 = base_lean_v if base_lean_v is Vector3 else Vector3.ZERO
	var base_gd: float = float(base_gd_v) if base_gd_v != null else 0.0

	_check(stag_lean.length() > base_lean.length() + 0.08,
		"stagger_left_s_amplifies_lean_beyond_stage_baseline",
		"stag=%.4f base=%.4f" % [stag_lean.length(), base_lean.length()])
	_check(stag_gd > base_gd + 0.08,
		"stagger_left_s_amplifies_guard_drop_beyond_stage_baseline",
		"stag_gd=%.4f base_gd=%.4f" % [stag_gd, base_gd])


func _test_limp_weight_view_and_rig(view: Node3D) -> void:
	var r_knee_swing_s0 := 0.0
	var r_knee_swing_s4 := 0.0
	var prev_limp := -0.001

	for st in [0, 1, 2, 3, 4]:
		_reset_view(view)
		var pos := Vector3(0.0, MatronView.STAND_CENTRE, 0.0)
		var vel := Vector3(0.0, 0.0, 2.2)
		var min_rx := 999.0
		var max_rx := -999.0
		for i in range(60):
			pos += vel * DT
			var snap := _base_snap(pos, vel, MatronView.STATE_HUNTING)
			snap["stage"] = 0
			snap["stride"] = float(i) * vel.length() * DT
			snap["damage_stage"] = st
			_step(view, snap, DT, pos + Vector3(0.0, 0.0, 4.0))
			if i >= 20:
				var kn_r: Node3D = view._p["kn_r"]
				var rx := kn_r.rotation.x
				min_rx = minf(min_rx, rx)
				max_rx = maxf(max_rx, rx)
		var lw_v: Variant = view.get("limp_weight")
		var lw: float = float(lw_v) if lw_v != null else -1.0
		if st == 0:
			_check(absf(lw) < 0.01, "limp_weight_zero_at_stage_0", "lw=%.4f" % lw)
			r_knee_swing_s0 = max_rx - min_rx
		else:
			_check(lw > prev_limp + 0.18, "limp_weight_monotonic_stage_%d" % st,
				"lw=%.4f prev=%.4f" % [lw, prev_limp])
		if st == 4:
			r_knee_swing_s4 = max_rx - min_rx
		prev_limp = lw

	_check(r_knee_swing_s4 < r_knee_swing_s0 * 0.85,
		"bipedal_limp_reduces_right_knee_swing_at_stage_4",
		"s4=%.4f s0=%.4f" % [r_knee_swing_s4, r_knee_swing_s0])

	# All-fours MatronRig limp_weight and contact truth
	_reset_view(view)
	var pos4 := Vector3(0.0, MatronView.STAND_CENTRE, 0.0)
	var vel4 := Vector3(0.0, 0.0, 2.8)
	var max_lift_l := 0.0
	var max_lift_r := 0.0
	for i in range(75):
		pos4 += vel4 * DT
		var snap := _base_snap(pos4, vel4, MatronView.STATE_HUNTING)
		snap["stage"] = 2
		snap["stride"] = float(i) * vel4.length() * DT
		snap["on_all_fours"] = true
		snap["crouch"] = 1.0
		snap["contact_supported"] = true
		snap["damage_stage"] = 4
		_step(view, snap, DT, pos4 + Vector3(0.0, 0.0, 4.0))
		if i >= 25 and view._rig != null:
			var st_l: Dictionary = view._rig._plants.get("an_l", {})
			var st_r: Dictionary = view._rig._plants.get("an_r", {})
			var foot_l: Vector3 = st_l.get("at", Vector3.ZERO)
			var foot_r: Vector3 = st_r.get("at", Vector3.ZERO)
			max_lift_l = maxf(max_lift_l, foot_l.y)
			max_lift_r = maxf(max_lift_r, foot_r.y)

	var rig_lw_v: Variant = view._rig.get("limp_weight") if view._rig != null else null
	var rig_lw: float = float(rig_lw_v) if rig_lw_v != null else -1.0
	_check(rig_lw > 0.85, "rig_limp_weight_synced_at_stage_4", "rig_lw=%.4f" % rig_lw)
	_check(max_lift_r < max_lift_l * 0.85,
		"rig_all_fours_right_foot_lift_reduced_by_limp",
		"lift_r=%.4f lift_l=%.4f" % [max_lift_r, max_lift_l])
	_check(view.unowned_frames == 0,
		"rig_all_fours_limp_preserves_zero_unowned_frames",
		"unowned_frames=%d" % view.unowned_frames)


func _test_climbing_and_down_invariants(view: Node3D) -> void:
	_reset_view(view)
	var pos := Vector3(0.0, 3.0, 0.0)
	var snap_climb := _base_snap(pos, Vector3(0.0, 1.4, 0.0), MatronView.STATE_HUNTING)
	snap_climb["climbing"] = true
	snap_climb["on_ladder"] = true
	snap_climb["grounded"] = false
	snap_climb["damage_stage"] = 4
	snap_climb["stagger_left_s"] = 0.5
	for _i in range(15):
		_step(view, snap_climb, DT, pos + Vector3(0.0, 2.0, 0.0))

	var climb_lean_v: Variant = view.get("damage_lean")
	var climb_lean: Vector3 = climb_lean_v if climb_lean_v is Vector3 else Vector3.ONE
	_check(climb_lean.length() < 0.001, "climbing_suppresses_standing_damage_lean", "lean=%s" % str(climb_lean))

	var snap_down := _base_snap(Vector3(0.0, MatronView.STAND_CENTRE, 0.0), Vector3.ZERO, MatronView.STATE_DOWN)
	snap_down["damage_stage"] = 4
	_step(view, snap_down, DT, Vector3(0.0, MatronView.STAND_CENTRE, 3.0))
	var down_lean_v: Variant = view.get("damage_lean")
	var down_lean: Vector3 = down_lean_v if down_lean_v is Vector3 else Vector3.ONE
	_check(down_lean.length() < 0.001, "state_down_suppresses_standing_damage_lean", "lean=%s" % str(down_lean))


func _test_stage_blood_tell(view: Node3D) -> void:
	var prev_blood := -0.001
	for st in range(5):
		_reset_view(view)
		var pos := Vector3(0.0, MatronView.STAND_CENTRE, 0.0)
		var snap := _base_snap(pos, Vector3.ZERO, MatronView.STATE_HUNTING)
		snap["damage_stage"] = st
		_step(view, snap, DT, pos + Vector3(0.0, 0.0, 3.0))
		var db_v: Variant = view.get("damage_blood")
		var db: float = float(db_v) if db_v != null else -1.0
		var shader_blood := 0.0
		if view._look != null and view._look._apron_soak != null:
			var sb_v: Variant = view._look._apron_soak.get_shader_parameter("soak")
			if sb_v != null:
				shader_blood = float(sb_v)
		if st == 0:
			_check(absf(db) < 0.001, "stage_0_zero_damage_blood", "db=%.4f" % db)
		else:
			_check(db > prev_blood + 0.1, "stage_%d_damage_blood_monotonic" % st,
				"db=%.4f prev=%.4f" % [db, prev_blood])
			_check(shader_blood > 0.05 and shader_blood >= db - 0.01, "stage_%d_apron_soak_shader_blood_applied" % st,
				"shader_blood=%.4f db=%.4f" % [shader_blood, db])
		prev_blood = db


func _test_hud_damage_indicators() -> void:
	var hud := HudOverlay.new()
	hud.her_marker = {
		"visible": true,
		"pos": Vector2(400.0, 220.0),
		"state": 4,
		"clamp": false,
		"damage_stage": 3,
		"stagger_left_s": 0.45,
	}
	if hud.has_method("sync_her_damage_tells"):
		hud.call("sync_her_damage_tells")
	var pips_v: Variant = hud.get("her_damage_pips")
	var stag_v: Variant = hud.get("her_stagger_cue")
	var pips: int = int(pips_v) if pips_v != null else -1
	var stag: float = float(stag_v) if stag_v != null else -1.0
	_check(pips == 3, "hud_damage_pips_tracks_stage_3", "pips=%d" % pips)
	_check(stag > 0.4, "hud_stagger_cue_tracks_stagger_left_s", "stag=%.3f" % stag)

	hud.her_marker["damage_stage"] = 0
	hud.her_marker["stagger_left_s"] = 0.0
	if hud.has_method("sync_her_damage_tells"):
		hud.call("sync_her_damage_tells")
	var pips0: int = int(hud.get("her_damage_pips")) if hud.get("her_damage_pips") != null else -1
	var stag0: float = float(hud.get("her_stagger_cue")) if hud.get("her_stagger_cue") != null else -1.0
	_check(pips0 == 0 and absf(stag0) < 0.001, "hud_damage_pips_clears_at_stage_0",
		"pips0=%d stag0=%.3f" % [pips0, stag0])
	hud.free()
