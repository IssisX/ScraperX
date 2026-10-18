extends RefCounted

# A right-thumb contact is either free look or a deliberately armed command.
# It never changes from look into a command because the thumb paused. Distances
# are viewport pixels; time comes from InputRouter.frame, not a wall clock.
enum Phase { IDLE, PENDING, LOOK, COMMAND, CANCELLED }
enum Sector { NONE, REACH, CROUCH, DROP, CANCEL }

const ARM_SECONDS := 0.16
const DROP_HOLD_SECONDS := 0.35
const DIRECTION_RATIO := 1.35

var phase := Phase.IDLE
var sector := Sector.NONE
var index := -1
var origin := Vector2.ZERO
var position := Vector2.ZERO
var age := 0.0
var drop_hold := 0.0
var slop := 10.0
var stroke := 48.0
var armed_feedback := false
var ready_feedback := false

var _commands: Array[StringName] = []
var _look := Vector2.ZERO
var _pending_look := Vector2.ZERO


func reset() -> void:
	phase = Phase.IDLE
	sector = Sector.NONE
	index = -1
	age = 0.0
	drop_hold = 0.0
	armed_feedback = false
	ready_feedback = false
	_commands.clear()
	_look = Vector2.ZERO
	_pending_look = Vector2.ZERO


func begin(finger: int, at: Vector2, scale: float) -> void:
	reset()
	index = finger
	origin = at
	position = at
	slop = maxf(6.0, 26.0 * scale)
	stroke = maxf(24.0, 144.0 * scale)
	phase = Phase.PENDING


func drag(at: Vector2, relative: Vector2) -> void:
	position = at
	match phase:
		Phase.PENDING:
			_pending_look += relative
			if at.distance_to(origin) > slop:
				phase = Phase.LOOK
				_look += _pending_look
				_pending_look = Vector2.ZERO
		Phase.LOOK:
			_look += relative
		Phase.COMMAND:
			_select_sector()


func advance(delta: float) -> void:
	if phase == Phase.IDLE:
		return
	age += maxf(0.0, delta)
	if phase == Phase.PENDING and age >= ARM_SECONDS:
		phase = Phase.COMMAND
		_pending_look = Vector2.ZERO
		armed_feedback = true
	elif phase == Phase.COMMAND and sector == Sector.DROP:
		var was_ready := drop_hold >= DROP_HOLD_SECONDS
		drop_hold = minf(DROP_HOLD_SECONDS, drop_hold + maxf(0.0, delta))
		if not was_ready and drop_hold >= DROP_HOLD_SECONDS:
			ready_feedback = true


func release(cancelled: bool) -> void:
	if not cancelled:
		if phase == Phase.PENDING:
			_commands.append(&"primary")
		elif phase == Phase.COMMAND and sector == Sector.DROP \
				and drop_hold >= DROP_HOLD_SECONDS:
			_commands.append(&"drop")
	phase = Phase.IDLE
	sector = Sector.NONE
	index = -1
	drop_hold = 0.0
	_pending_look = Vector2.ZERO


func take_commands() -> Array[StringName]:
	var commands := _commands.duplicate()
	_commands.clear()
	return commands


func take_look() -> Vector2:
	var look := _look
	_look = Vector2.ZERO
	return look


func _select_sector() -> void:
	var offset := position - origin
	var next := Sector.NONE
	if offset.length() >= stroke:
		if absf(offset.y) > absf(offset.x) * DIRECTION_RATIO:
			next = Sector.REACH if offset.y < 0.0 else Sector.CROUCH
		elif absf(offset.x) > absf(offset.y) * DIRECTION_RATIO:
			next = Sector.DROP if offset.x < 0.0 else Sector.CANCEL
	if next != sector:
		drop_hold = 0.0
		ready_feedback = false
	sector = next
	match sector:
		Sector.REACH, Sector.CROUCH:
			_commands.append(&"action" if sector == Sector.REACH else &"crouch")
			# The command stroke cannot turn the camera. Subsequent movement
			# resumes look with the same thumb, with no second action on lift.
			phase = Phase.LOOK
		Sector.CANCEL:
			phase = Phase.CANCELLED
