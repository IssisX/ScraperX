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
const LINES := {
	&"last_beam": "I signed her accident off as her fault. The beam's home, Jack. Your turn.",
	&"s1_cage": "Miller? It's Saturday. What are you doing on my tower?",
	&"stack": "Get down before somebody sees you.",
	&"locker": "What was that? Miller, get away from that locker.",
	&"ring_220": "Don't come down. She's below you. The only way off is the top.",
}

var text := ""         # the line showing; "" between lines
var who := ""
var lines_said := 0    # for the tests
var _said := {}        # beat -> true, once it has been queued
var _queue: Array[StringName] = []
var _left := 0.0       # seconds the line showing has to run
var _gap := 0.0        # seconds of air between lines
var _audio = null


func setup(audio) -> void:
	_audio = audio


# `ctx`: y (the climber's height, m), support (the entity underfoot),
# matron_released (she is out of her locker).
func update(delta: float, ctx: Dictionary) -> void:
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
	# Grumbling, as the climber keeps going up the Stack.
	if _said.has(&"s1_cage") and support == TOWER_ENTITY and y > 44.5:
		_cue(&"stack")
	# Her locker bursts open at 154 m: he hears it over the deck intercom.
	if released:
		_cue(&"locker")
	# The 220 ring with her out below: the lifeline.
	if released and y > 220.5:
		_cue(&"ring_220")


func _cue(beat: StringName) -> void:
	if _said.has(beat):
		return
	_said[beat] = true
	_queue.append(beat)
