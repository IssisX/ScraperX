extends RefCounted
## The Matron heard (her bank: sound_bank.gd build_her; her players and
## layers: audio_director.gd matron_layers). What she does is matched to her
## sounds here, every cue on a change the native reports (a count rising, a
## state left or entered) or on a timer while she is in a state: nothing here
## decides anything about her. The trackers follow her every frame; the cues
## play only while she is out and near (matron_view.gd DRAW_RANGE_M). Her body
## (steps, joints, the steel she strikes) is heard where it happens; her
## voice at her head.
##
## The scripted runs count the cue names: every rung she climbs and every
## hold she takes on the corner pipe is still a `clang` (touch_matron_climb,
## touch_matron_act), and every cue is counted by the director before it
## looks at a bank or a device.

# Her states (sim/matron.hpp).
const STATE_HUNTING := 3
const STATE_SEARCHING := 4
const STATE_SHUFFLING := 5
const STATE_GUARDING := 7
const STATE_PEERING := 8
const STATE_UNRIGGING := 9
const STATE_DOWN := 11
# Her strides (matron_view.gd STEP_M): one footfall each, a scuttle's
# shorter; never two within STEP_GAP_S. A rung each RUNG_M of a climb.
const STEP_M := 0.7
const STEP_GAP_S := 0.09
const RUNG_M := 0.3
# Seeing the player: her dread (or fully changed, her shriek) within
# DREAD_M, at most once in DREAD_REST_S.
const DREAD_M := 40.0
const DREAD_REST_S := 7.0
const SHRIEK_DB := -3.0
# The Turn (sim/matron.hpp kTurnWindupSeconds, kTurnCycleSeconds,
# kTurnBurstSeconds): her joints in the wind-up, a slam at each burst.
const TURN_WINDUP_S := 0.9
const TURN_CYCLE_S := 0.5
const TURN_BURST_S := 0.2
const TURN_JOINTS_S := 0.4
# Listening: after LISTEN_CROAK_S, one time in three, a croak.
const LISTEN_CROAK_S := 2.5
# Idle: searching within SEARCH_M, a sniff or a croak every SEARCH_GAP_S;
# shuffling, or at her post and still, within IDLE_M, a cluck or a swallow
# every IDLE_GAP_S.
const SEARCH_M := 30.0
const SEARCH_GAP_S := Vector2(4.0, 8.0)
const IDLE_M := 8.0
const IDLE_GAP_S := Vector2(5.0, 9.0)
# Her hands on his arm: an effort every EFFORT_GAP_S while she plays with it.
const EFFORT_GAP_S := 1.4
# The thud of her end heard within this of the player (matron_view.gd
# DRAW_RANGE_M).
const REACH_M := 140.0
# Cues due later: at most this many waiting.
const DUE_SLOTS := 6

var _audio: Object
# The rest after her dread (the locker's burst sets it too, matron_view.gd).
var dread_in := 0.0
var _on := false
var _head := Vector3.ZERO
var _dt := 0.0
var _was_seen := false
var _stage_seen := -1
var _turn_seen := 0
var _burst := false
var _state_seen := -1
var _alive_seen := -1
var _vaults_seen := -1
var _knockdowns_seen := -1
var _staggers_seen := -1
var _shoves_seen := -1
var _throws_seen := -1
var _leaps_seen := -1
var _climbs_seen := -1
var _catches_seen := -1
var _takes_seen := -1
var _fooled_seen := -1
var _unrigs_seen := -1
var _pipe_holds_seen := -1
var _pipe_flanges_seen := -1
var _pipe_slams_seen := -1
var _last_rung := 0
var _last_step := 0
var _step_gap := 0.0
var _rattle_in := 0.0
var _pipe_scrape_in := 0.0
var _screech_in := 0.0
var _cry_fall := false
var _listen_s := 0.0
var _listen_rolled := false
var _idle_kind := 0
var _idle_in := 0.0
# Her hold (get_matron_hold_state).
var _grabs_seen := -1
var _beat_seen := -1
var _tears_seen := -1
var _releases_seen := -1
var _spares_seen := -1
var _claims_seen := -1
var _effort_in := 0.0
# Cues due later, by slot (a slot whose time is under 0 is free).
var _due_s := PackedFloat32Array()
var _due_name: Array[StringName] = []
var _due_at := PackedVector3Array()
var _due_db := PackedFloat32Array()
var _due_pitch := PackedFloat32Array()


func _init(audio: Object) -> void:
	_audio = audio
	_due_s.resize(DUE_SLOTS)
	_due_s.fill(-1.0)
	_due_name.resize(DUE_SLOTS)
	_due_at.resize(DUE_SLOTS)
	_due_db.resize(DUE_SLOTS)
	_due_pitch.resize(DUE_SLOTS)


# Once a frame, out or not, with her state (`data`, get_matron_state):
# `out_near` she is out, alive and near enough to be drawn; `feet` her soles;
# `head` her head; `falling` her plunge as the view poses it.
func update(data: Dictionary, delta: float, out_near: bool, feet: Vector3, head: Vector3,
		player_position: Vector3, falling: bool = false) -> void:
	_on = out_near
	_head = head
	_dt = delta
	var on := out_near
	var state := int(data["state"])
	var centre: Vector3 = data["position"]
	var speed := float(data["speed"])
	var turn := int(data.get("turn", 0))
	var stage := int(data.get("stage", 0))
	var d := feet.distance_to(player_position)
	dread_in = maxf(0.0, dread_in - delta)
	_tick_due(delta)
	# She sees the player: her dread, near enough and not too soon again.
	var sees := bool(data["sees_player"])
	if on and sees and not _was_seen and dread_in <= 0.0 and d < DREAD_M:
		_audio.matron_dread(head, d, stage)
		dread_in = DREAD_REST_S
	_was_seen = sees
	# A stage up: a croak (and her layers swap their bed and growl).
	if on and _stage_seen >= 0 and stage > _stage_seen:
		_cue(&"matron_croak", head, -4.0, _pitch())
	_stage_seen = stage
	# The Turn: her loops cut (matron_layers) and her joints going in the
	# wind-up; the shriek as she lunges, and a slam at every burst of it.
	if turn != _turn_seen:
		if on and turn == 1:
			_due(TURN_JOINTS_S, &"matron_joints", centre, 3.0, randf_range(0.9, 1.1))
		elif on and turn == 2:
			_cue(&"matron_shriek", head, SHRIEK_DB, _pitch())
		_turn_seen = turn
	if turn == 2:
		var burst := fmod(float(data.get("turn_s", TURN_WINDUP_S)) - TURN_WINDUP_S, TURN_CYCLE_S) < TURN_BURST_S
		if on and burst and not _burst:
			_cue(&"matron_step", feet, 3.0, 1.25)
		_burst = burst
	else:
		_burst = false
	# A vault: her hands slamming down on what she goes over, her joints.
	var vaults := int(data.get("vaults", 0))
	if on and _vaults_seen >= 0 and vaults > _vaults_seen:
		_cue(&"matron_step", centre, 2.0, 0.7)
		_cue(&"matron_joints", centre, -1.0, randf_range(0.9, 1.1))
	_vaults_seen = vaults
	# A blow off her feet: her body booming onto the steel and the breath
	# knocked out of her (unless a slam on the pipe made it, heard below).
	var pipe_slams := int(data.get("pipe_slams", 0))
	var slammed := _pipe_slams_seen >= 0 and pipe_slams > _pipe_slams_seen
	_pipe_slams_seen = pipe_slams
	var knockdowns := int(data.get("knockdowns", 0))
	if on and _knockdowns_seen >= 0 and knockdowns > _knockdowns_seen and not slammed:
		_cue(&"hit_heavy", centre, 4.0, randf_range(0.55, 0.65))
		_cue(&"matron_hit", head, -2.0, _pitch())
	_knockdowns_seen = knockdowns
	# Up off the steel: her joints.
	if on and _state_seen == STATE_DOWN and state != STATE_DOWN:
		_cue(&"matron_joints", centre, 1.0, randf_range(0.9, 1.1))
	_state_seen = state
	# Off her balance: her feet stamping and slewing, and the blow in her.
	var staggers := int(data.get("staggers", 0))
	if on and _staggers_seen >= 0 and staggers > _staggers_seen:
		_cue(&"matron_step", feet, 3.0, randf_range(0.6, 0.7))
		_cue(&"scrape", feet, -4.0, randf_range(0.9, 1.1))
		_cue(&"matron_hit", head, -8.0, _pitch())
	_staggers_seen = staggers
	# A shove, a throw, a leap over a gap, a ladder taken after the player:
	# her shriek.
	var shoves := int(data.get("shoves", 0))
	var throws := int(data.get("throws", 0))
	var leaps := int(data.get("leaps", 0))
	var climbs := int(data.get("climbs", 0))
	if on and ((_shoves_seen >= 0 and shoves > _shoves_seen) or (_throws_seen >= 0 and throws > _throws_seen)
			or (_leaps_seen >= 0 and leaps > _leaps_seen) or (_climbs_seen >= 0 and climbs > _climbs_seen)):
		_cue(&"matron_shriek", head, SHRIEK_DB, _pitch())
	_shoves_seen = shoves
	_throws_seen = throws
	_leaps_seen = leaps
	_climbs_seen = climbs
	# A catch: she sniffs at it. A take: a cluck, then her laugh. Fooled (a
	# noise she ran to, and nobody there): a croak.
	var catches := int(data.get("catches", 0))
	if on and _catches_seen >= 0 and catches > _catches_seen:
		_cue(&"matron_sniff", head, 0.0, _pitch())
	_catches_seen = catches
	var takes := int(data.get("takes", 0))
	if on and _takes_seen >= 0 and takes > _takes_seen:
		_cue(&"matron_cluck", head, 0.0, _pitch())
		_due(0.3, &"matron_laugh", head, -6.0, _pitch())
	_takes_seen = takes
	var fooled := int(data.get("fooled", 0))
	if on and _fooled_seen >= 0 and fooled > _fooled_seen:
		_cue(&"matron_croak", head, -2.0, _pitch())
	_fooled_seen = fooled
	# At a rigged cage's eye: the shackle rattled, then its clang off.
	if on and state == STATE_UNRIGGING and speed < 0.3:
		_rattle_in -= delta
		if _rattle_in <= 0.0:
			_rattle_in = randf_range(0.35, 0.55)
			_cue(&"scrape", centre, -10.0, randf_range(1.3, 1.6))
	var unrigs := int(data.get("unrigs", 0))
	if on and _unrigs_seen >= 0 and unrigs > _unrigs_seen:
		_cue(&"clang", centre, -2.0, randf_range(1.1, 1.25))
	_unrigs_seen = unrigs
	# Climbing: every rung rings under her hands and feet, louder while the
	# Turn has her.
	if bool(data.get("climbing", false)):
		var rung := int(float(data["stride"]) / RUNG_M)
		if rung != _last_rung:
			_last_rung = rung
			if on:
				_cue(&"clang", centre + Vector3(0.0, 1.2, 0.0), -9.0 if turn == 0 else -3.0, randf_range(1.35, 1.6))
	# On the corner pipe: each hold a hollow knock up and down the riser, each
	# flange passed a deep bong, the slam onto one a boom, her joints and her
	# shriek; torn off and sliding, her skin and cloth shrieking on it.
	var hands := centre + Vector3(0.0, 1.0, 0.0)
	var pipe_holds := int(data.get("pipe_holds", 0))
	if on and _pipe_holds_seen >= 0 and pipe_holds > _pipe_holds_seen:
		_cue(&"clang", hands, -4.0 if turn == 0 else 0.0, randf_range(0.5, 0.62))
	_pipe_holds_seen = pipe_holds
	var pipe_flanges := int(data.get("pipe_flanges", 0))
	if on and _pipe_flanges_seen >= 0 and pipe_flanges > _pipe_flanges_seen:
		_cue(&"hit_heavy", hands, 0.0, randf_range(0.42, 0.5))
		_cue(&"clang", hands, -2.0, randf_range(0.36, 0.42))
	_pipe_flanges_seen = pipe_flanges
	if on and slammed:
		_cue(&"hit_heavy", hands, 5.0, randf_range(0.5, 0.58))
		_cue(&"matron_joints", hands, 3.0, randf_range(0.85, 1.0))
		_cue(&"matron_shriek", head, SHRIEK_DB, _pitch())
	if on and bool(data.get("pipe_sliding", false)):
		_pipe_scrape_in -= delta
		if _pipe_scrape_in <= 0.0:
			_pipe_scrape_in = randf_range(0.08, 0.12)
			_cue(&"scrape", hands, 0.0, randf_range(1.4, 1.8))
	# Down a cable on her forearms: steel shrieking under them.
	if on and bool(data.get("on_line", false)):
		_screech_in -= delta
		if _screech_in <= 0.0:
			_screech_in = randf_range(0.1, 0.16)
			_cue(&"scrape", centre + Vector3(0.0, 1.55, 0.0), -3.0, randf_range(1.7, 2.0))
	# Steps: a dragged slipper, or on the hunt hands and feet slapping.
	_step_gap = maxf(0.0, _step_gap - delta)
	var steps := int(float(data["stride"]) / (STEP_M * (0.55 if state == STATE_HUNTING else 1.0)))
	if steps != _last_step:
		_last_step = steps
		if on and speed > 0.3 and not falling and _step_gap <= 0.0:
			_step_gap = STEP_GAP_S
			_cue(&"matron_step", feet, lerpf(-11.0, 0.0, clampf(speed / 6.8, 0.0, 1.0)),
				randf_range(0.85, 1.1) * (1.25 if state == STATE_HUNTING else 1.0))
	# The long fall: her shriek as she goes.
	if falling and not _cry_fall:
		_cry_fall = true
		if on:
			_cue(&"matron_shriek", head, SHRIEK_DB, _pitch())
	elif not falling:
		_cry_fall = false
	# Her end: her body onto the steel, and then nothing of her.
	var alive := 1 if bool(data["alive"]) else 0
	if _alive_seen == 1 and alive == 0 and d <= REACH_M:
		_cue(&"hit_heavy", centre, 4.0, 0.5)
	_alive_seen = alive
	# Listening: silent, and after a while, now and then, a croak.
	if on and (state == STATE_PEERING or bool(data.get("pipe_listening", false))):
		_listen_s += delta
		if _listen_s >= LISTEN_CROAK_S and not _listen_rolled:
			_listen_rolled = true
			if randf() < 1.0 / 3.0:
				_cue(&"matron_croak", head, 0.0, _pitch())
	else:
		_listen_s = 0.0
		_listen_rolled = false
	# Idle, now and then: searching, a sniff or a croak; shuffling, or at her
	# post and still, a cluck or a swallow.
	var idle := 0
	if on and state == STATE_SEARCHING and d < SEARCH_M:
		idle = 1
	elif on and (state == STATE_SHUFFLING or (state == STATE_GUARDING and speed < 0.3)) and d < IDLE_M:
		idle = 2
	if idle != _idle_kind:
		_idle_kind = idle
		_idle_in = randf_range(SEARCH_GAP_S.x, SEARCH_GAP_S.y) if idle == 1 else randf_range(IDLE_GAP_S.x, IDLE_GAP_S.y)
	if idle > 0:
		_idle_in -= delta
		if _idle_in <= 0.0:
			if idle == 1:
				_idle_in = randf_range(SEARCH_GAP_S.x, SEARCH_GAP_S.y)
				_cue(&"matron_sniff" if randf() < 0.5 else &"matron_croak", head, 0.0, _pitch())
			else:
				_idle_in = randf_range(IDLE_GAP_S.x, IDLE_GAP_S.y)
				_cue(&"matron_cluck" if randf() < 0.5 else &"matron_swallow", head, 0.0, _pitch())
	# Her layers: the bed, her voice, her mouth.
	_audio.matron_layers(delta, data, on, head, d, player_position.y)


# Once a frame after update(), with her hold (get_matron_hold_state, the one
# read main_feel.gd makes for the gore too).
func hear_hold(hold: Dictionary) -> void:
	if hold.is_empty():
		return
	var on := _on
	var hand: Vector3 = hold.get("matron_hand", _head)
	# Her hand closing on the ankle: the slap of it, and a cluck.
	var grabs := int(hold.get("grabs", 0))
	if on and _grabs_seen >= 0 and grabs > _grabs_seen:
		_cue(&"matron_step", hand, -2.0, 1.4)
		_cue(&"matron_cluck", _head, 0.0, _pitch())
	_grabs_seen = grabs
	# Playing with the arm: the joint creaking in her hand with the strain on
	# the wrist, higher and louder toward the tear; her effort now and then.
	var beat := int(hold.get("armplay_beat", 0))
	if on and beat == 1:
		var s := clampf(float(hold.get("wrist_strain_ns", 0.0)) / maxf(float(hold.get("wrist_tear_ns", 1.0)), 1.0),
			0.0, 1.0)
		_audio.matron_strain(hand, lerpf(-18.0, -2.0, s), lerpf(0.7, 1.6, s))
		_effort_in -= _dt
		if _effort_in <= 0.0:
			_effort_in = EFFORT_GAP_S
			_cue(&"matron_effort", _head, -3.0, _pitch())
	else:
		_audio.matron_strain(hand, -80.0, 1.0)
		_effort_in = 0.5
	# The arm gone to her: a sniff at it, and a swallow.
	if on and beat == 2 and _beat_seen >= 0 and _beat_seen != 2:
		_cue(&"matron_sniff", _head, 0.0, _pitch())
		_due(0.9, &"matron_swallow", _head, 0.0, _pitch())
	_beat_seen = beat
	# The arm torn off: the rip where it parted, his crack and his cry, then
	# her laugh.
	var tears := int(hold.get("arm_tear_count", 0))
	if on and _tears_seen >= 0 and tears > _tears_seen:
		_cue(&"matron_rip", hold.get("severed_arm_position", hand), 4.0, randf_range(0.95, 1.05))
		_audio.arm_break(hold.get("player_position", hand))
		_due(0.6, &"matron_laugh", _head, 0.0, _pitch())
	_tears_seen = tears
	# Let go at a ledge, set back on the deck, or claimed: a laugh, a cluck,
	# a laugh.
	var releases := int(hold.get("releases", 0))
	if on and _releases_seen >= 0 and releases > _releases_seen:
		_cue(&"matron_laugh", _head, 0.0, _pitch())
	_releases_seen = releases
	var spares := int(hold.get("spares", 0))
	if on and _spares_seen >= 0 and spares > _spares_seen:
		_cue(&"matron_cluck", _head, 0.0, _pitch())
	_spares_seen = spares
	var claims := int(hold.get("claims", 0))
	if on and _claims_seen >= 0 and claims > _claims_seen:
		_cue(&"matron_laugh", _head, 0.0, _pitch())
	_claims_seen = claims


func _cue(name: StringName, at: Vector3, volume_db: float, pitch: float) -> void:
	_audio.matron_cue(name, at, volume_db, pitch)


# Her voice's own pitch, a little off every time (the director keeps it
# within its HER_PITCH).
func _pitch() -> float:
	return randf_range(0.92, 1.08)


func _due(after: float, name: StringName, at: Vector3, volume_db: float, pitch: float) -> void:
	for i in DUE_SLOTS:
		if _due_s[i] < 0.0:
			_due_s[i] = after
			_due_name[i] = name
			_due_at[i] = at
			_due_db[i] = volume_db
			_due_pitch[i] = pitch
			return


# Cues come due: each plays only if she is still out and near.
func _tick_due(delta: float) -> void:
	for i in DUE_SLOTS:
		if _due_s[i] < 0.0:
			continue
		_due_s[i] -= delta
		if _due_s[i] <= 0.0:
			_due_s[i] = -1.0
			if _on:
				_cue(_due_name[i], _due_at[i], _due_db[i], _due_pitch[i])
