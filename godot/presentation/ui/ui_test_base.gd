extends Node

# The UI test driver's ground floor: its state, the thumb (touch, pad and
# keyboard events pushed through the real input path), the verbs built on
# it, and the watchers. The scenarios are layered on top of this, one family
# per file, ending in ui_test_driver.gd, which runs one of them.

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const SettingsStore := preload("res://presentation/ui/settings_store.gd")

# The native's TraversalState and the tower's entity id, as main.gd reads them.
const TRAVERSAL_NONE := 0
# The walker's braking with nothing pressed, as the native has it: 22 m/s^2
# on the ground, 14 in the air. Keys walking to a point let go by the weaker;
# with a load in the hands, which caps it (5.3 for D's 50 kg spool), by less.
const KEY_BRAKE_MPS2 := 14.0
const KEY_CARRY_BRAKE_MPS2 := 5.0

var _main: Node
var _capture_prefix := ""
var _detail := ""
# The touch scheme under test (settings_store TOUCH_SCHEME_*): the gestures,
# the game's default, unless the scenario's name ends in _buttons
# (ui_test_driver). A pad or a keyboard has no scheme.
var _gestures := true
# The right thumb's swipes take turns on two touch indices, so one can follow
# another at once.
var _swipe_index := 5

func _fail(reason: String) -> bool:
	_detail = "reason=" + reason
	return false

# The slab incline's trolley carries two plates (entities after its deck):
# the yellow trip plate and the brake treadle.
const INCLINE_DECK_ENTITY := 2952

func _standing_above(y: float) -> bool:
	return int(_native().get_traversal_state()) == TRAVERSAL_NONE and bool(_native().is_player_grounded()) and \
		_position().y > y

# Turns to face `target` and walks to it, the way a player crosses open ground;
# on the keyboard the mouse keeps the view on it along the way.
# Pulses of `kind` the game has sent the hands (counted in CI, never sent).
func _felt(kind: StringName) -> int:
	return int(_main.haptic_counts.get(kind, 0))

func _go(device: int, target: Vector2, tolerance: float, budget: float) -> bool:
	var at := _position()
	var to := target - Vector2(at.x, at.z)
	if to.length() > tolerance:
		await _face(to)
	return await _walk_to(device, target, tolerance, budget, device == InputRouter.Device.KEYBOARD_MOUSE)

# Presses what reads CLIMB UP on the device under test while hanging: on
# touch the jump button relabels to it (swiped, up reads it, or CLIMB OVER
# at a climb's top); on a pad or keyboard it is Action.
func _climb_up(device: int) -> bool:
	if _gesturing(device):
		var offered: bool = await _wait_until(
			func() -> bool: return _main._touch.gesture_hint(&"up") in ["CLIMB UP", "CLIMB OVER"], 1.5)
		if not offered:
			return false
		_swipe(-1.0)
		return true
	if device == InputRouter.Device.TOUCH:
		var relabelled: bool = await _wait_until(
			func() -> bool: return _main._touch.button_label(&"jump") == "CLIMB UP", 1.5)
		if not relabelled:
			return false
		_tap(2, _center(&"jump"))
		return true
	if not await _offered(&"climb_up", "CLIMB UP"):
		return false
	_act(device)
	return true

# One press of Jump on the device under test.
# Jumps with forward held at `amount` of full throw until `caught` holds or
# `budget` seconds pass. A key has no throw: held down it drives the leap at
# full speed, so on the keyboard W is feathered instead, down for `amount` of
# every tenth of a second, the way a player taps it to keep a leap short.
func _leap(device: int, amount: float, caught: Callable, budget: float) -> bool:
	_jump(device)
	if device != InputRouter.Device.KEYBOARD_MOUSE:
		_move(device, amount)
		var held: bool = await _wait_until(caught, budget)
		_move(device, 0.0)
		return held
	var waited := 0.0
	while waited < budget:
		if caught.call():
			_move(device, 0.0)
			return true
		_move(device, 1.0 if fmod(waited, 0.1) < amount * 0.1 else 0.0)
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move(device, 0.0)
	return bool(caught.call())

func _jump(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			if _gestures:
				_swipe(-1.0)
			else:
				_tap(2, _center(&"jump"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_A)
		_:
			_key(KEY_SPACE, true)
			_key(KEY_SPACE, false)

# One press of Drop on the device under test: the touch button (swiped,
# down), pad B, Q.
func _drop(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			if _gestures:
				_swipe(1.0)
			else:
				_tap(1, _center(&"drop"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_B)
		_:
			_key(KEY_Q, true)
			_key(KEY_Q, false)

# Presses LET GO when the hands still hold something, and waits for them to
# be empty.
func _let_go_if_held(device: int) -> bool:
	if int(_native().get_carrying_entity_id()) == 0:
		return true
	if not await _offered(&"set_down", "LET GO"):
		return false
	_act(device)
	return await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)

# Turns the view to face `direction` the way a thumb or a stick does, at up
# to 4 rad/s, never in one frame.
func _face(direction: Vector2) -> void:
	var goal := atan2(-direction.x, -direction.y)
	while true:
		var left := wrapf(goal - float(_main._yaw), -PI, PI)
		var step := 4.0 * get_process_delta_time()
		if absf(left) <= step:
			_main._yaw = goal
			return
		_main._yaw = float(_main._yaw) + signf(left) * step
		await get_tree().process_frame

# Tilts the view to `pitch` the way a thumb or a stick does, at up to 2 rad/s.
func _tilt(pitch: float) -> void:
	while absf(pitch - float(_main._pitch)) > 1.0e-3:
		var step := 2.0 * get_process_delta_time()
		_main._pitch = move_toward(float(_main._pitch), pitch, step)
		await get_tree().process_frame

func _action_label() -> String:
	return String(_ctx()["action"]["label"])

# Waits for the HUD to offer `id` with `label` (and `detail`, when given) on
# the Action verb: what a player reads before pressing.
func _offered(id: StringName, label: String, detail: String = "") -> bool:
	return await _wait_until(func() -> bool: return _ctx()["action"]["id"] == id and \
		_action_label() == label and \
		(detail.is_empty() or String(_ctx()["action"]["detail"]) == detail), 1.5)

# One press of the Action verb on the device under test. Swiped, what Action
# offers that is a move is the swipe's: a climb, a climb up, the trapeze's bar
# taken or let go, up; letting go of a bar, down.
func _act(device: int) -> void:
	match device:
		InputRouter.Device.TOUCH:
			if _gestures and _act_by_swipe():
				return
			_tap(1, _center(&"action"))
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_X)
		_:
			_key(KEY_E, true)
			_key(KEY_E, false)

func _act_by_swipe() -> bool:
	match StringName(_ctx()["action"]["id"]):
		&"climb", &"climb_up", &"trapeze":
			_swipe(-1.0)
			return true
		&"let_go":
			_swipe(1.0)
			return true
	return false

# One press of a run's verb on touch, at touch `index` for the buttons: Jump,
# Crouch (STAND when crouched), Drop, Turn or Action. Swiped: up, down (up to
# stand), down, the stick flicked back, and as _act.
func _press(verb: StringName, index: int) -> void:
	if not _gestures:
		_tap(index, _center(verb))
		return
	match verb:
		&"jump":
			_swipe(-1.0)
		&"crouch":
			_swipe(-1.0 if bool(_native().is_player_crouched()) else 1.0)
		&"drop":
			_swipe(1.0)
		&"turn":
			_stick_flick()
		&"action":
			if not _act_by_swipe():
				_tap(index, _center(&"action"))

# What a run's verb reads on the glass: its button's label; swiped, the
# swipe's hint (Jump and Action that climbs: up; Drop: down; Crouch: down,
# or up -- STAND -- when crouched).
func _reads(verb: StringName) -> String:
	if not _gestures:
		return _main._touch.button_label(verb)
	match verb:
		&"jump", &"action":
			return _main._touch.gesture_hint(&"up")
		&"drop":
			return _main._touch.gesture_hint(&"down")
		&"crouch":
			return _main._touch.gesture_hint(&"up" if bool(_native().is_player_crouched()) else &"down")
	return _main._touch.button_label(verb)

# Drop is offered: its button drawn; swiped, down lets go or drops down, and
# the button is not drawn.
func _offers_drop() -> bool:
	if not _gestures:
		return _main._touch.is_button_shown(&"drop")
	return not _main._touch.is_button_shown(&"drop") and \
		_main._touch.gesture_hint(&"down") in ["LET GO", "DROP DOWN", "PUT DOWN"]

func _gesturing(device: int) -> bool:
	return device == InputRouter.Device.TOUCH and _gestures

# Where the right thumb swipes: high on the look side, clear of every button.
func _swipe_point() -> Vector2:
	return Vector2(_viewport_size().x * 0.85, _viewport_size().y * 0.3)

# A swipe of the right thumb, up (-1) or down (+1): set down on the look side
# and flicked 120 units within the frame -- past the recognizer's 90 in its
# 0.16 s -- then lifted the next frame; or, `hold`, kept on the glass until
# _lift. Returns the touch index.
func _swipe(direction: float, hold := false) -> int:
	var index := _swipe_index
	_swipe_index = 11 - _swipe_index
	var at := _swipe_point()
	var step := Vector2(0.0, direction * 40.0 * float(_main._touch._u))
	_touch(index, at, true)
	for i in 3:
		at += step
		_drag(index, at, step)
	if not hold:
		_lift_later(index, at)
	return index

func _lift_later(index: int, at: Vector2) -> void:
	await get_tree().process_frame
	_touch(index, at, false)

func _lift(index: int) -> void:
	_touch(index, _swipe_point(), false)

# The quickturn by the stick: a run reversed, the stick pulled straight back
# to its rim in one frame. A thumb already on the stick (a run) stays there,
# so the run goes on the other way until _move lets it go; standing, the
# thumb sets down, flicks forward and back, and lifts the next frame.
func _stick_flick() -> void:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u)
	var held := int(_main._touch._stick_index) == 0
	if not held:
		_touch(0, home, true)
		_drag(0, home + Vector2(0.0, -throw), Vector2(0.0, -throw))
	_drag(0, home + Vector2(0.0, throw), Vector2(0.0, 2.0 * throw))
	if not held:
		_lift_later(0, home)

# Holds ACTION down (or lets it go) on the device under test: a held verb,
# like the pitman lift's HAUL.
func _hold_act(device: int, pressed: bool) -> void:
	match device:
		InputRouter.Device.TOUCH:
			_touch(1, _center(&"action"), pressed)
		InputRouter.Device.GAMEPAD:
			_button(JOY_BUTTON_X, pressed)
		_:
			_key(KEY_E, pressed)

# Holds forward (+) or back (-) at `amount` of full throw on the device under
# test; 0 lets go.
func _move(device: int, amount: float) -> void:
	_move_dir(device, Vector2(0.0, amount))

# Holds the move input toward world (x, z) `w`, at its length of full throw,
# without turning the view; a key, having no throw, is pressed full.
func _move_world(device: int, w: Vector2) -> void:
	var yaw := float(_main._yaw)
	var v := Vector2(w.dot(Vector2(cos(yaw), -sin(yaw))), w.dot(Vector2(-sin(yaw), -cos(yaw))))
	_move_dir(device, v.normalized() if device == InputRouter.Device.KEYBOARD_MOUSE else v)

# Holds the move input at `v` = (right, forward) of full throw, relative to
# the view, on the device under test. A key has no throw: a component past
# a third holds its key down.
func _move_dir(device: int, v: Vector2) -> void:
	match device:
		InputRouter.Device.TOUCH:
			if v.length() < 0.01:
				_touch(0, _main._touch.stick_home(), false)
			else:
				_stick_hold(Vector2(v.x, -v.y), minf(v.length(), 1.0))
		InputRouter.Device.GAMEPAD:
			_axis(JOY_AXIS_LEFT_X, v.x)
			_axis(JOY_AXIS_LEFT_Y, -v.y)
		_:
			_key(KEY_W, v.y > 0.33)
			_key(KEY_S, v.y < -0.33)
			_key(KEY_D, v.x > 0.33)
			_key(KEY_A, v.x < -0.33)

# Walks to a horizontal point without turning the view, the way a player
# steps into place: the stick pushed toward it relative to the view, easing
# off near it, and stopping inside `tolerance`. A key has no throw: it is
# held until the walker could no longer stop short of the point (a frame
# late, at KEY_BRAKE_MPS2), then let go, and pressed again if it stops
# short; on the keys the walk ends only once the walker has stopped there.
# `steer` turns the view toward a point more than a metre off, as a mouse
# holds a keyboard walk to its line.
func _walk_to(device: int, target: Vector2, tolerance: float, budget: float = 6.0, steer := false) -> bool:
	var keys := device == InputRouter.Device.KEYBOARD_MOUSE
	var waited := 0.0
	while waited < budget:
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		var speed := _walk_speed()
		if to.length() <= tolerance and (not keys or speed.length() < 0.3):
			_move_dir(device, Vector2.ZERO)
			await _seconds(0.2)
			return true
		if steer and to.length() > 1.0:
			var turn := 4.0 * get_process_delta_time()
			_main._yaw = float(_main._yaw) + clampf(wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI), -turn, turn)
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		var v := Vector2(to.dot(right), to.dot(forward)).normalized() * clampf(to.length() / 0.8, 0.25, 1.0)
		if keys:
			var closing := maxf(speed.dot(to.normalized()), 0.0)
			var brake := KEY_CARRY_BRAKE_MPS2 if int(_native().get_carrying_entity_id()) != 0 else KEY_BRAKE_MPS2
			var stopping := closing * (get_process_delta_time() + closing / (2.0 * brake))
			v = Vector2.ZERO if to.length() <= tolerance or stopping >= to.length() - 0.5 * tolerance else v.normalized()
			# Teetering (the native's XCoM past an edge): pull back, as a
			# player does; let go, the feet brake less.
			if v == Vector2.ZERO and closing > 0.0 and bool(_native().get_landing_state()["teetering"]):
				var back := -speed.normalized()
				v = Vector2(back.dot(right), back.dot(forward))
		_move_dir(device, v)
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	return false

# Holds the left stick at `amount` of full throw toward canvas `direction`.
func _stick_hold(direction: Vector2, amount: float) -> void:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u) * clampf(amount, 0.0, 1.0)
	_stick_down(home)
	_drag(0, home + direction.normalized() * throw, direction.normalized() * throw)

# Sets the thumb down on the stick, unless it is on it already: a second
# press of the same index would be taken for look.
func _stick_down(home: Vector2) -> void:
	if int(_main._touch._stick_index) != 0:
		_touch(0, home, true)

# Samples both rendered wrists every frame until stopped, in the camera's
# frame -- where a player sees them -- and reports the largest single-frame
# move of either.
func _watch_wrists() -> Dictionary:
	var watch := {"running": true, "worst": 0.0}
	_sample_wrists(watch)
	return watch

func _sample_wrists(watch: Dictionary) -> void:
	var last: Array = []
	while bool(watch["running"]):
		var view: Transform3D = (_main._camera as Camera3D).global_transform.affine_inverse()
		var now: Array = _main._arms.wrist_positions().map(
			func(wrist: Vector3) -> Vector3: return view * wrist)
		if last.size() == now.size():
			for index in now.size():
				var step: float = ((now[index] as Vector3) - (last[index] as Vector3)).length()
				if step > float(watch["worst"]):
					watch["worst"] = step
					watch["at"] = "%s %s traversal=%d at=%s" % [str(_main._arms.hand_poses()), _action_label(),
						int(_native().get_traversal_state()), str(_position())]
		last = now
		await get_tree().process_frame

# Samples the body's position every frame until stopped, and reports the
# largest single-frame sideways move: a body the native walks, climbs and
# mantles never jumps; a snap across a blocked path does. It also keeps, as
# "lift", the largest single-frame rise or fall of the eye beyond the body's
# own continuous motion: a native step lifts the body in one tick, and the
# view must glide over it, never show it in one frame.
func _watch_body() -> Dictionary:
	var watch := {"running": true, "worst": 0.0, "lift": 0.0}
	_sample_body(watch)
	return watch

func _sample_body(watch: Dictionary) -> void:
	var last := _position()
	var last_eye := (_main._camera as Camera3D).global_transform.origin.y
	var last_body := _render_soles_y()
	var last_steps := float(_native().get_step_up_meters())
	while bool(watch["running"]):
		await get_tree().process_frame
		var now := _position()
		var step := Vector2(now.x - last.x, now.z - last.z).length()
		if step > float(watch["worst"]):
			watch["worst"] = step
			watch["at"] = "traversal=%d from %s to %s" % [int(_native().get_traversal_state()), str(last),
				str(now)]
		last = now
		var eye := (_main._camera as Camera3D).global_transform.origin.y
		var body := _render_soles_y()
		var steps := float(_native().get_step_up_meters())
		var lift := absf((eye - last_eye) - (body - last_body - (steps - last_steps)))
		if lift > float(watch["lift"]):
			watch["lift"] = lift
			watch["lift_at"] = "stepped %.3f m at %s" % [steps - last_steps, str(now)]
		last_eye = eye
		last_body = body
		last_steps = steps

# The rendered body's soles, which the eye hangs from (main.gd): a crouch
# swaps the native's capsule round them in one tick, its centre moving 0.3 m
# while the soles stay put and the eye glides.
func _render_soles_y() -> float:
	return (_native().get_player_render_position() as Vector3).y - \
		(0.6 if bool(_native().is_player_crouched()) else 0.9)

func _stop_watch(watch: Dictionary) -> float:
	watch["running"] = false
	return float(watch["worst"])

# The sound a player hears, measured where it leaves Master (after the
# limiter) and in the band a phone speaker reproduces, above 300 Hz. A
# headless run's Dummy driver never mixes, so this runs under Movie Maker
# (--write-movie), which mixes that same driver frame by frame. Standing
# still, the yard's ambience must be there; walking, the footsteps must
# stand out of it; nothing may clip.
func _audio_mix() -> bool:
	if AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty():
		return _fail("no mixing audio driver; run under --write-movie")
	var capture := AudioEffectCapture.new()
	capture.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Master"), capture)
	var ready: bool = await _wait_until(func() -> bool: return bool(_main._audio._bank_ready), 5.0)
	if not ready:
		return _fail("the sound bank never finished building")
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	# The ambience fades in from silence over its first seconds.
	await _capture(capture, 2.5)
	var standing := await _capture(capture, 1.0)
	var steps_before: int = _main._audio.steps
	_key(KEY_W, true)
	var walking := await _capture(capture, 1.4)
	_key(KEY_W, false)
	var steps: int = _main._audio.steps - steps_before
	var bed := _band_levels(standing)
	var walk := _band_levels(walking)
	var peak := 0.0
	for x in standing + walking:
		peak = maxf(peak, absf(x))
	var peak_db := linear_to_db(maxf(peak, 1.0e-9))
	_detail = "bed_db=%.1f steps_db=%.1f peak_db=%.1f steps=%d" % [bed.x, walk.y, peak_db, steps]
	_main._audio.quiesce()
	await _frames(3)
	if steps < 3:
		return _fail("walking 1.4 s played %d footsteps" % steps)
	if bed.x < -40.0:
		return _fail("standing still, the mix above 300 Hz sits at %.1f dBFS" % bed.x)
	if walk.y < bed.x + 4.0:
		return _fail("footsteps (%.1f dBFS) do not stand out of the ambience (%.1f)" % [walk.y, bed.x])
	if peak_db > -0.5:
		return _fail("the mix clips: sample peak %.2f dBFS" % peak_db)
	return true

# The fear voice as a player hears it: from the high drop to the lethal
# impact, captured on the Voice bus and on the Ambience bus (the altitude
# wind, the yard, the falling-air rush). While a line runs the voice must
# stand 6 dB clear of the bed it ducks, and the mix leaving Master must not
# clip.
func _audio_fall() -> bool:
	if AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty():
		return _fail("no mixing audio driver; run under --write-movie")
	var captures: Array[AudioEffectCapture] = []
	for bus in [&"Voice", &"Ambience", &"Master"]:
		var capture := AudioEffectCapture.new()
		capture.buffer_length = 1.0
		AudioServer.add_bus_effect(AudioServer.get_bus_index(bus), capture)
		captures.append(capture)
	var ready: bool = await _wait_until(func() -> bool: return bool(_main._audio._bank_ready), 5.0)
	if not ready:
		return _fail("the sound bank never finished building")
	var voice := PackedFloat32Array()
	var air := PackedFloat32Array()
	var master := PackedFloat32Array()
	var waited := 0.0
	while int(_native().get_death_count()) < 1 and waited < 12.0:
		await get_tree().process_frame
		waited += get_process_delta_time()
		for pair in [[captures[0], voice], [captures[1], air], [captures[2], master]]:
			var capture: AudioEffectCapture = pair[0]
			var available := capture.get_frames_available()
			if available > 0:
				for frame in capture.get_buffer(available):
					pair[1].append((frame.x + frame.y) * 0.5)
	_main._audio.quiesce()
	await _frames(3)
	if int(_native().get_death_count()) < 1:
		return _fail("the high drop never ended")
	# 50 ms windows where the voice is speaking (within 20 dB of its loudest).
	var window := int(AudioServer.get_mix_rate() * 0.05)
	var count := mini(voice.size(), air.size()) / window
	var voice_db: Array[float] = []
	var air_db: Array[float] = []
	var loudest := -200.0
	for k in count:
		loudest = maxf(loudest, _window_db(voice, k * window, window))
	for k in count:
		var v := _window_db(voice, k * window, window)
		if v > loudest - 20.0 and v > -60.0:
			voice_db.append(v)
			air_db.append(_window_db(air, k * window, window))
	var peak := 0.0
	for x in master:
		peak = maxf(peak, absf(x))
	var peak_db := linear_to_db(maxf(peak, 1.0e-9))
	if voice_db.is_empty():
		return _fail("nothing was heard on the Voice bus in a %.1f s fall" % waited)
	voice_db.sort()
	air_db.sort()
	var voice_mid := voice_db[voice_db.size() / 2]
	var air_mid := air_db[air_db.size() / 2]
	_detail = "voice_db=%.1f ambience_db=%.1f speaking_s=%.2f peak_db=%.1f voice=%s" % [voice_mid,
		air_mid, voice_db.size() * 0.05, peak_db, str(_main._audio.voice_lines)]
	if voice_db.size() * 0.05 < 1.0:
		return _fail("the voice spoke for only %.2f s of the fall" % (voice_db.size() * 0.05))
	if voice_mid < air_mid + 6.0:
		return _fail("the voice (%.1f dBFS) is lost in the ambience (%.1f)" % [voice_mid, air_mid])
	if peak_db > -0.5:
		return _fail("the mix clips: sample peak %.2f dBFS" % peak_db)
	return true

func _window_db(mono: PackedFloat32Array, first: int, count: int) -> float:
	var energy := 0.0
	for i in range(first, mini(first + count, mono.size())):
		energy += mono[i] * mono[i]
	return linear_to_db(maxf(sqrt(energy / float(maxi(count, 1))), 1.0e-9))

# Mono samples leaving Master for `seconds` of game time.
func _capture(capture: AudioEffectCapture, seconds: float) -> PackedFloat32Array:
	var mono := PackedFloat32Array()
	var waited := 0.0
	while waited < seconds:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var available := capture.get_frames_available()
		if available > 0:
			for frame in capture.get_buffer(available):
				mono.append((frame.x + frame.y) * 0.5)
	return mono

# 50 ms RMS levels above 300 Hz (two cascaded one-pole high-passes), as
# (median, loudest) in dBFS.
func _band_levels(mono: PackedFloat32Array) -> Vector2:
	var rate := AudioServer.get_mix_rate()
	var a := exp(-TAU * 300.0 / rate)
	var x1 := 0.0
	var y1 := 0.0
	var x2 := 0.0
	var y2 := 0.0
	var window := int(rate * 0.05)
	var levels: Array[float] = []
	var energy := 0.0
	var count := 0
	for x in mono:
		var h1 := a * (y1 + x - x1)
		x1 = x
		y1 = h1
		var h2 := a * (y2 + h1 - x2)
		x2 = h1
		y2 = h2
		energy += h2 * h2
		count += 1
		if count == window:
			levels.append(linear_to_db(maxf(sqrt(energy / float(window)), 1.0e-9)))
			energy = 0.0
			count = 0
	if levels.is_empty():
		return Vector2(-200.0, -200.0)
	levels.sort()
	return Vector2(levels[levels.size() / 2], levels[levels.size() - 1])

# --- helpers -------------------------------------------------------------------

func _native() -> Object:
	return _main._native

func _ctx() -> Dictionary:
	return _main._ctx

func _position() -> Vector3:
	return _main._native.get_player_position()

func _velocity() -> Vector3:
	return _main._native.get_player_linear_velocity()

# The walker's horizontal velocity over what its feet stand on.
func _walk_speed() -> Vector2:
	var v := _velocity()
	if bool(_native().is_player_grounded()):
		v -= _native().get_support_point_linear_velocity()
	return Vector2(v.x, v.z)

func _viewport_size() -> Vector2:
	return get_viewport().get_visible_rect().size

func _center(id: StringName) -> Vector2:
	return _main._touch.button_center(id)

func _frames(count: int) -> void:
	for i in count:
		await get_tree().process_frame

func _seconds(duration: float) -> void:
	var waited := 0.0
	while waited < duration:
		await get_tree().process_frame
		waited += get_process_delta_time()

func _wait_until(condition: Callable, timeout: float) -> bool:
	var waited := 0.0
	while not bool(condition.call()):
		if waited >= timeout:
			return false
		await get_tree().process_frame
		waited += get_process_delta_time()
	return true

func _push(event: InputEvent) -> void:
	get_viewport().push_input(event, true)

func _touch(index: int, at: Vector2, pressed: bool) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	_push(event)

func _drag(index: int, at: Vector2, relative: Vector2) -> void:
	var event := InputEventScreenDrag.new()
	event.index = index
	event.position = at
	event.relative = relative
	_push(event)

func _tap(index: int, at: Vector2) -> void:
	_touch(index, at, true)
	_touch(index, at, false)

# Holds the left stick deflected in canvas direction `direction` at full throw.
func _stick_push(direction: Vector2) -> void:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u)
	_stick_down(home)
	for i in 3:
		var at := home + direction.normalized() * throw * float(i + 1) / 3.0
		_drag(0, at, direction.normalized() * throw / 3.0)

# Pushes the left stick straight forward on past its ring, the way a thumb
# latches sprint, and holds it there at full throw; _move(device, 0.0) lets
# go. True when the touch layer reads sprint latched.
func _stick_sprint() -> bool:
	var home: Vector2 = _main._touch.stick_home()
	var throw: float = TouchControls.STICK_THROW * float(_main._touch._u)
	_stick_down(home)
	# Four thirds of the throw: past SPRINT_OVERSHOOT's 1.25.
	for i in 4:
		_drag(0, home + Vector2(0.0, -throw) * float(i + 1) / 3.0, Vector2(0.0, -throw / 3.0))
	return bool(_main._touch.sprint_latched)

# A touch's GUI twin: emulated mouse events carry DEVICE_ID_EMULATION.
func _click(at: Vector2) -> void:
	for pressed in [true, false]:
		var event := InputEventMouseButton.new()
		event.device = InputEvent.DEVICE_ID_EMULATION
		event.button_index = MOUSE_BUTTON_LEFT
		event.position = at
		event.global_position = at
		event.pressed = pressed
		_push(event)

func _key(code: Key, pressed: bool) -> void:
	var event := InputEventKey.new()
	event.physical_keycode = code
	event.keycode = code
	event.pressed = pressed
	_push(event)

func _button(button: JoyButton, pressed: Variant = null) -> void:
	var states: Array = [true, false] if pressed == null else [pressed]
	for state in states:
		var event := InputEventJoypadButton.new()
		event.device = 0
		event.button_index = button
		event.pressed = state
		_push(event)

func _axis(axis: JoyAxis, value: float) -> void:
	var event := InputEventJoypadMotion.new()
	event.device = 0
	event.axis = axis
	event.axis_value = value
	_push(event)

# Screenshots need a real renderer; headless proof runs skip them silently.
func _pose(pose: String) -> void:
	if _capture_prefix.is_empty() or DisplayServer.get_name() == "headless":
		return
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	var path := "%s_%s.png" % [_capture_prefix, pose]
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var image := get_viewport().get_texture().get_image()
	if image.save_png(path) == OK:
		print("SCRAPERX_UITEST_SCREENSHOT %s" % path)
	else:
		push_error("SCRAPERX_UITEST_SCREENSHOT_FAILED %s" % path)
