class_name ForemanRadio
extends Node
# The foreman, Walt Brenner, on the radio from the yard: the story's one
# human voice (the owner, 2026-10-19). Each of his lines is set off by a place
# or an event in the native's state, never by a timer, and comes once a
# launch; one line shows at a time, on the HUD's radio plate, opened and
# closed by the handheld's squelch (a machine's sound). The lines are
# captions until a person records them. Nothing here changes the simulation,
# and a saved climb continued hears again whatever its places set off.

const WHO := "BRENNER, ON THE RADIO"
const SHOW_SECONDS := 5.0
const PER_CHARACTER_SECONDS := 0.045
const GAP_SECONDS := 1.2
# S1's cage (Simulation::kStackS1CageEntityId) and the stack's decks.
const S1_CAGE_ENTITY := 2200
const TOWER_ENTITY := 11

# Act 1, in the order the climb meets them. Jack Miller the climber, Walt
# Brenner the foreman (the owner, 2026-10-21: plain American names).
# Missions 1 "Clock in" and 7 "Bring the plant up" first (J-130, B5 item 2):
# a job line when the job is given (a place cue, the same as every other
# beat), a done line when J-129's flag flips. The flag read is a stub until
# J-129 lands: ctx carries `objectives_done` (objective index -> bool, the
# same numbering as Objectives::Id: 0 is mission 1, 1 is mission 7), and
# main.gd passes an empty stub for now. A done line shows only for a flag
# that flips from false to true during this launch: a flag already true at
# launch was earned (and said) in an earlier launch, so it is never shown
# twice for one done flag. No line here is wider than the longest line
# already shipping (last_beam): the plate has no wrap, and the Fold folded
# is the narrowest it is ever drawn.
const MISSION_1 := 0  # Objectives::Id::Mission1ClockIn
const MISSION_7 := 1  # Objectives::Id::Mission7BringPlantUp
const LINES := {
	&"last_beam": "I signed her accident off as her fault. The beam's home, Jack. Your turn.",
	&"s1_cage": "Miller? It's Saturday. What are you doing on my tower?",
	&"stack": "Get down before somebody sees you.",
	&"locker": "What was that? Miller, get away from that locker.",
	&"ring_220": "Don't come down. She's below you. The only way off is the top.",
	&"mission_1_given": "Clock in, Jack. Get S1 running and ride her up.",
	&"mission_1_done": "S1's running and you're up. One line off the list.",
	&"mission_7_given": "Bring the plant up, Jack. Ram, chiller, accumulator, in that order.",
	&"mission_7_done": "Plant's up. The tower's breathing again.",
}

var text := ""         # the line showing; "" between lines
var who := ""
var lines_said := 0    # for the tests
var _said := {}        # beat -> true, once it has been queued
var _done_at_launch := {}  # objective index -> true, if done when this launch began
var _seen_objectives := false  # the launch snapshot above has been taken
var _queue: Array[StringName] = []
var _left := 0.0       # seconds the line showing has to run
var _gap := 0.0        # seconds of air between lines
var _audio = null


func setup(audio) -> void:
	_audio = audio


# `ctx`: y (the climber's height, m), support (the entity underfoot),
# matron_released (she is out of her locker), objectives_done (J-129's done
# flags, objective index -> bool; an empty stub until J-129 lands).
func update(delta: float, ctx: Dictionary) -> void:
	# Snapshot the flags this launch began with before any cue reads them:
	# the flags must be present from the first frame, or a late arrival
	# reads as a fresh completion.
	if not _seen_objectives:
		_seen_objectives = true
		_done_at_launch = _objectives_done(ctx).duplicate()
	_watch(ctx)
	if _left > 0.0:
		_left -= delta
		if _left <= 0.0:
			text = ""
			who = ""
			_gap = GAP_SECONDS
			if _audio != null:
				_audio.radio(false)
		return
	if _gap > 0.0:
		_gap -= delta
		return
	if _queue.is_empty():
		return
	var beat: StringName = _queue.pop_front()
	text = String(LINES[beat])
	who = WHO
	_left = SHOW_SECONDS + PER_CHARACTER_SECONDS * float(text.length())
	lines_said += 1
	if _audio != null:
		_audio.radio(true)


func _watch(ctx: Dictionary) -> void:
	# The beam's physical seating is the story payoff. Never queue old
	# climb instructions over his admission, and never synthesize his voice.
	if bool(ctx.get("beam_seated", false)):
		if not _said.has(&"last_beam"):
			_queue.clear()
			_left = 0.0
			_gap = 0.0
			_cue(&"last_beam")
		return
	var y := float(ctx.get("y", 0.0))
	var support := int(ctx.get("support", 0))
	var released := bool(ctx.get("matron_released", false))
	# He spots the badge on the gate log as the climber steps into S1's cage.
	if support == S1_CAGE_ENTITY:
		_cue(&"s1_cage")
		_cue(&"mission_1_given")
	# Grumbling, as the climber keeps going up the Stack.
	if _said.has(&"s1_cage") and support == TOWER_ENTITY and y > 44.5:
		_cue(&"stack")
	# Her locker bursts open at 154 m: he hears it over the deck intercom.
	if released:
		_cue(&"locker")
	# The 220 ring with her out below: the lifeline.
	if released and y > 220.5:
		_cue(&"ring_220")
		_cue(&"mission_7_given")
	# A job done: J-129's flag, once, only if it flips during this launch.
	# The beam check above already returned, so no done line queues over
	# his admission.
	if _newly_done(ctx, MISSION_1):
		_cue(&"mission_1_done")
	if _newly_done(ctx, MISSION_7):
		_cue(&"mission_7_done")


func _objectives_done(ctx: Dictionary) -> Dictionary:
	var done = ctx.get("objectives_done", {})
	return done if done is Dictionary else {}


func _newly_done(ctx: Dictionary, mission: int) -> bool:
	if not bool(_objectives_done(ctx).get(mission, false)):
		return false
	return not bool(_done_at_launch.get(mission, false))


func _cue(beat: StringName) -> void:
	if _said.has(beat):
		return
	_said[beat] = true
	_queue.append(beat)
