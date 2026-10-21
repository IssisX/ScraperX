extends RefCounted

# Observes native facts only; Guide owns speech and the actor owns its body.
const ACTOR_ID := 63200
const RECEIVER_ID := 1993
const FOCUS_TICKS := 54 # 0.6 seconds of deliberate attention at 90 Hz.
const MAX_OBSERVATION_GAP := 9
const KEYS := [
	&"double_ro_who", &"double_ari_name", &"double_ro_face", &"double_ari_job",
	&"double_ro_empty", &"double_ari_told", &"double_ro_when", &"double_ari_before",
	&"double_ro_name", &"double_ari_will", &"double_ro_and_face", &"double_ari_cannot", &"double_iona_here"
]
const BRANCH_KEYS := {
	&"ari_first": [&"double_ro_original_scope", &"double_ari_original_scope"],
	&"sent_to_dev": [&"double_ro_dev_has_scope", &"double_ari_dev_has_scope"]
}
const CUES := {
	&"double_ro_who": "Who the fuck are you?",
	&"double_ari_name": "Ari Ward.",
	&"double_ro_face": "That's my face.",
	&"double_ari_job": "I was hired to attend your handovers.",
	&"double_ro_empty": "You told them the rooms were empty.",
	&"double_ari_told": "I was told they were.",
	&"double_ro_original_scope": "I have the original survey. It says occupancy was never assessed. Whose inspection did you read into that camera?",
	&"double_ari_original_scope": "It was a statement they gave me. I read it as you. There was no inspection.",
	&"double_ro_dev_has_scope": "Dev has the original survey. Your handover calls this place empty. Mine never did.",
	&"double_ari_dev_has_scope": "Then he has the proof. They wrote the statement. I read it as you. There was no inspection.",
	&"double_ro_when": "When did you find out?",
	&"double_ari_before": "Before the last recording.",
	&"double_ro_name": "Stop using my name.",
	&"double_ari_will": "I will.",
	&"double_ro_and_face": "And the face?",
	&"double_ari_cannot": "That's not something I can hand back.",
	&"double_iona_here": "Ro. I'm here."
}
const SPEAKERS := {
	&"double_ro_who": "RO", &"double_ari_name": "ARI", &"double_ro_face": "RO",
	&"double_ari_job": "ARI", &"double_ro_empty": "RO", &"double_ari_told": "ARI",
	&"double_ro_when": "RO", &"double_ari_before": "ARI", &"double_ro_name": "RO",
	&"double_ari_will": "ARI", &"double_ro_and_face": "RO", &"double_ari_cannot": "ARI",
	&"double_ro_original_scope": "RO", &"double_ari_original_scope": "ARI",
	&"double_ro_dev_has_scope": "RO", &"double_ari_dev_has_scope": "ARI",
	&"double_iona_here": "IONA"
}
var _last_tick := -1
var _episode := -1
var _focus_ticks := 0
var _latched := false
var _phase := 0
var _requested := &""
var _requested_disposition := &""
var _branch_disposition := &""

func cancel_attention() -> void:
	# Loss of actor/footing, death or episode change cancels attention, never
	# an accepted line. Returning requires another deliberate look.
	_focus_ticks = 0
	_latched = false
	_requested = &""
	_requested_disposition = &""

func reset() -> void:
	# Native time reset starts a new session, including its conversation.
	cancel_attention()
	_last_tick = -1
	_episode = -1
	_phase = 0
	_branch_disposition = &""

func observe(context: Dictionary, tick: int) -> StringName:
	_requested = &""
	_requested_disposition = &""
	if tick < 0:
		cancel_attention()
		return &""
	if _last_tick >= 0 and tick < _last_tick:
		reset()
	if tick == _last_tick:
		return &""
	var elapsed := tick - _last_tick if _last_tick >= 0 else 0
	_last_tick = tick
	var actor: Variant = context.get("story_actor")
	if not actor is Dictionary:
		cancel_attention()
		return &""
	var episode := int(actor.get("episode", -1))
	if episode != _episode:
		cancel_attention()
		_episode = episode
		elapsed = 0
	var at: Variant = context.get("position")
	var other: Variant = actor.get("position")
	var velocity: Variant = actor.get("velocity")
	var camera: Variant = context.get("story_camera_transform")
	var eligible: bool = episode >= 0 and bool(actor.get("present", false)) \
		and bool(actor.get("supported", false)) and int(actor.get("entity_id", 0)) == ACTOR_ID \
		and int(actor.get("support_entity_id", 0)) == RECEIVER_ID \
		and int(context.get("support_entity", 0)) == RECEIVER_ID \
		and bool(context.get("grounded", false)) and bool(context.get("checkpoint_footing_valid", false)) \
		and StringName(context.get("operating", &"")).is_empty() \
		and not bool(context.get("hanging", false)) and not bool(context.get("climbing", false)) \
		and at is Vector3 and at.is_finite() and other is Vector3 and other.is_finite() \
		and velocity is Vector3 and velocity.is_finite() and velocity.length_squared() < 1.0 \
		and camera is Transform3D and camera.is_finite() \
		and bool(context.get("story_actor_line_of_sight", false))
	if eligible:
		eligible = at.distance_squared_to(other) <= 16.0 and absf(at.y - other.y) < .6 \
			and camera.origin.distance_squared_to(at) < 4.0
	if not eligible or elapsed > MAX_OBSERVATION_GAP:
		cancel_attention()
		return &""
	if not _latched:
		var offset: Vector3 = other + Vector3(0, .65, 0) - camera.origin
		if offset.length_squared() < .01 or (-camera.basis.z).normalized().dot(offset.normalized()) < .985:
			_focus_ticks = 0
			return &""
		_focus_ticks += elapsed
		if _focus_ticks < FOCUS_TICKS:
			return &""
		_latched = true
	if _phase < KEYS.size():
		_requested = KEYS[_phase]
		if _phase == 4 or _phase == 5:
			var disposition := _branch_disposition
			if disposition.is_empty():
				disposition = StringName(context.get("handover_disposition", &"undecided"))
				if not BRANCH_KEYS.has(disposition):
					disposition = &"undecided"
			_requested_disposition = disposition
			if BRANCH_KEYS.has(disposition):
				_requested = BRANCH_KEYS[disposition][_phase - 4]
	return _requested

func accept(key: StringName) -> void:
	# A request is not speech: only Guide's accepted queue entry advances it.
	if not _requested.is_empty() and key == _requested:
		if _phase == 4:
			# Knowledge may change while Guide is busy; freeze only heard speech.
			_branch_disposition = _requested_disposition
		_phase += 1
		_requested = &""
		_requested_disposition = &""
