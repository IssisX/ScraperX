extends "res://presentation/main_context.gd"

# The main scene, layer 5 of 6: what the player feels and sees. Haptics,
# voice and feedback, the first-person arms, the HUD layout, the camera's
# feel, the snapshot rendered, and the telemetry overlay.

const EYE_OFFSET := Vector3(0.0, 0.62, 0.0)

# Interface: every device (touch, pad, keyboard/mouse) is read by one router
# into one verb vocabulary; the HUD, touch surface and pause menu draw from
# one per-frame read of native state (_read_context). None of it decides
# anything -- a verb becomes a native request_*, and the native says yes or no.
const UiStyle := preload("res://presentation/ui/ui_style.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
const CorpseView := preload("res://presentation/corpse_view.gd")
const Gait := preload("res://presentation/gait.gd")
const SettingsStore := preload("res://presentation/ui/settings_store.gd")
# Traversal head motion, added on top of the player's own pitch and never
# written into it: a hanging climber looks up at the grip (the lip sits ~46
# degrees above a level gaze, outside the frame), a mantle nods down onto the
# hands taking the push. The offset eases back to exactly zero, so a player
# who is not traversing sees the unaltered pitch -- the CI hold included.
const VIEW_PITCH_HANGING := 0.32
# Swinging from a bar: radians the grip sits above the frame's middle.
const SWING_GRIP_ABOVE_CENTRE := 0.2
const VIEW_PITCH_MANTLING := -0.3
const VIEW_PITCH_VAULTING := -0.12
# On a ladder or pipe the eye leans up toward the next holds, less than a hang
# looks up at its lip.
const VIEW_PITCH_CLIMBING := 0.18
const VIEW_PITCH_RATE := 9.0
const CHECKPOINT_TOAST_RISE_METERS := 3.0
const FOV_SPRINT_MAX_DEGREES := 4.0
# Flow (sim kFlowSpeedMps): the view widens on as the run gathers from the
# sprint's 8 m/s to flow's 9.
const FOV_FLOW_DEGREES := 4.0
const FLOW_FROM_MPS := 8.0
const FLOW_FULL_MPS := 9.0
# On a wall run the head leans off the wall, eased in and out over ~0.15 s.
const WALL_LEAN_RAD := 0.09
const WALL_LEAN_RESPONSE_PER_SECOND := 14.0
# At flow's speed the view trembles a little: two incommensurate tones on
# pitch and roll, never a random jitter (the same run shakes the same way).
const SPEED_SHAKE_RAD := 0.0035
const SPEED_SHAKE_HZ := Vector2(11.3, 7.9)
const FOV_FALL_MAX_DEGREES := 3.0
const FOV_FALL_FULL_MPS := 14.0
const LANDING_DIP_DURATION_SECONDS := 0.22
const LANDING_DIP_MAX_METERS := 0.16
const LANDING_DIP_MIN_IMPACT_MPS := 2.0
const LANDING_DIP_FULL_IMPACT_MPS := 9.0
const HEAD_BOB_VERTICAL_METERS := 0.028
const HEAD_BOB_LATERAL_METERS := 0.016
# Crouched, the eye sits 0.95 m over the soles instead of 1.52: the native
# capsule is 1.2 m, not 1.8 (kPlayerCrouchHalfHeight). The native swaps the
# two in one tick with the soles fixed, so the eye is measured from the soles
# and glides between the heights; the snap never reaches the view.
const STAND_HALF_HEIGHT := 0.9
const CROUCH_HALF_HEIGHT := 0.6
const CROUCH_EYE_OVER_SOLES := 0.95
const CROUCH_EYE_SECONDS := 0.16
# A step up is one native tick's lift of up to 0.36 m (kStepMaximumHeight and
# its clearance), taken whole by the render pose. The eye takes it as a lag
# below the body, closed at 14/s and never slower than 1 m/s, so it glides
# onto the step and settles exactly on it: the snap never reaches the view.
const STEP_EYE_RESPONSE_PER_SECOND := 14.0
const STEP_EYE_MIN_MPS := 1.0
const STEP_EYE_MAX_LAG := 0.36
const HEAD_BOB_SPEED_FLOOR_MPS := 0.3
const HEAD_BOB_SPEED_FULL_MPS := 3.0
const HEAD_BOB_RESPONSE_PER_SECOND := 12.0
const FOV_RESPONSE_PER_SECOND := 9.0
const CAMERA_BANK_MAX_RADIANS := 1.5 * PI / 180.0
const CAMERA_BANK_RESPONSE_PER_SECOND := 10.0
const KIT_DYNAMIC_ENTITY_MIN := 2000
const FALL_AIRBORNE := 1
const FALL_PARACHUTING := 2
var _summit_announced := false
var _ci_mode := false
# Tests only: the simulation held still while frames are still drawn and the
# camera still turns (a capture run's screenshot, so that its frames do not
# move the run it pictures away from the headless one).
var freeze_sim := false
var _yaw := 0.0
var _pitch := -0.02
var _cam_was_grounded := true
var _cam_last_velocity_y := 0.0
var _cam_landing_timer := 0.0
var _cam_landing_strength := 0.0
var _cam_bob_phase := 0.0
var _cam_bob_weight := 0.0
var _cam_fov_offset := 0.0
var _cam_bank := 0.0
var _fb_sliding := false
var _fb_wall_steps := 0
var _fb_springboards := 0
var _fb_throws := 0
var _crouch_eye := 0.0
var _step_eye_lag := 0.0
var _step_meters_seen := 0.0
# DISPLAY settings; the defaults are the tuned values above.
var _fov_base := FOV_BASE
var _head_bob_on := true
var _speed_fov_on := true
var _reduced_motion := false
# A hang torn off: a short drop of the view, as the hands go (seconds left).
const SLIP_JOLT_SECONDS := 0.3
var _slip_jolt := 0.0
var _slip_jolt_depth := 1.0   # how deep this jolt goes, of the full one
var _haul_phase := 0.0
var _viewport_size := Vector2.ZERO

var _router: Node
var _hud: Control
var _settings: SettingsStore
var _telemetry_on := false
var _fb_traversal := 0
var _fb_grounded := true
var _fb_fall_speed := 0.0
var _fb_deaths := 0
var _fb_chute := false
var _fb_warned := false
var _fb_best_checkpoint_y := 0.0
var _view_pitch_offset := 0.0
var _teeter_weight := 0.0
var _teeter_phase := 0.0
# The native's TraversalState for a hang (the arms' grip pose).
const TRAVERSAL_HANGING_STATE := 1
var _beam_bank := 0.0
var _wall_lean := 0.0
var _impact_sequence := 0   # the native's newest hit when last read
var _speed_shake := 0.0   # its weight, 0..1, for the scripted runs
var _shake_clock := 0.0
# The ground slingshot: its view, the predicted shot (refreshed a few times a
# second while a rider sits drawn), and the aim the look is steering.
var _slingshot_view: Node3D
# The slingshot's launch shot: slow time and a look back, after the push.
var _launch_shot := LaunchShot.new()
var _swing_view: Node3D
var _zipline_view: Node3D
var _corpse_view: Node3D
# Live fabrication's objects (fabricated_view.gd), when the world has any.
var _fabricated_view: Node3D
var _summit_view: Node3D
var _matron_view: Node3D
# Each forklift's view (the yard's first, then the yard crane's), and the one
# being driven or stood at, else the yard's.
var _forklift_views: Array[Node3D] = []
var _forklift_view: Node3D
var _crane_view: Node3D
var _sling_prediction := PackedVector3Array()
var _sling_preview_clock := 0.0
# Pigeons on the structure near the player; gulls at height.
var _birds: Node3D
# The yard crew, drawn from the native's workers.
var _crew: Node3D
var _radio: Node   # the foreman's radio (life/foreman_radio.gd)
@onready var _status: Label = $HUD/TopLeft/Status
@onready var _position_value: Label = $HUD/TopLeft/Position
@onready var _velocity_value: Label = $HUD/TopLeft/Velocity
@onready var _support_value: Label = $HUD/TopLeft/Support
@onready var _traversal_value: Label = $HUD/TopLeft/Traversal
@onready var _tick_value: Label = $HUD/TopRight/Tick
@onready var _fall_value: Label = $HUD/TopLeft/Fall

# The arrival at the top of the route, once per launch, from the native's own
# latch: the height, the climb's time and its falls. What stands up there is
# the owner's to decide; this only marks that the climb got there.
func _announce_summit() -> void:
	if _summit_announced:
		return
	var landing: Dictionary = _ctx.get("landing", {})
	if not bool(landing.get("summit_reached", false)):
		return
	_summit_announced = true
	var seconds := float(landing["summit_seconds"])
	var deaths := int(landing["summit_deaths"])
	_hud.toast("THE TOP", "%.1f M  /  %d:%02d  /  %d %s" % [float(landing["route_top"]), int(seconds) / 60,
		int(seconds) % 60, deaths, "FALL" if deaths == 1 else "FALLS"], UiStyle.AMBER, 8.0)
	_hud.flash(UiStyle.AMBER)
	_haptic(&"press", 1.0)
	print("SCRAPERX_SUMMIT seconds=%.1f deaths=%d" % [seconds, deaths])

const FALL_SWEARS := [
	"HOLY SHIIIIIIII—!",
	"WHAT THE F— NO NO NO NO!",
	"SWEET MOTHER OF GOD HELP ME!",
	"FUCK FUCK FUCK FUUUUUCK!",
	"OH SHIT OH SHIT OH SHIIIIIT!",
	"JESUS CHRIST MY LEGS!",
	"NOT LIKE THIS! NOT LIKE THIIIIIS!",
	"I REGRET EVERYTHING AAAAAAGH!",
	"MY SPLEEEEEEN!",
	"SON OF A BIIIIIIITCH!"
]

const CHUTE_RELIEF := [
	"THANK FUCK FOR NYLON!",
	"Holy sweet Jesus that was close.",
	"I think I just shat my coveralls.",
	"God bless whoever packed this canopy.",
	"Never doing that again. (Until next deck)."
]

const LETHAL_RESTORE_QUIPS := [
	"Floor taste: 2/10. Needs salt.",
	"Physics: 1. Hubris: 0.",
	"That's definitely going on the OSHA report.",
	"Walk it off. Just a 40-meter spinal compression.",
	"My ancestors felt that impact."
]

# The broken ankle's toast: what the climber thinks of it.
const BREAK_QUIPS := [
	"That crunch was load-bearing.",
	"Ankle's more of a suggestion now.",
	"Shit. Feet don't bend that way.",
	"Hobble on, legend. Hobble on.",
	"Bone says no. The tower says keep climbing."
]
# The broken arm's toast.
const ARM_QUIPS := [
	"Arm's purely decorative now.",
	"One-handed climbing. Brave. Stupid. Both.",
	"Elbows don't bend that way.",
	"Hold on with the good one.",
	"The bone said no. The tower doesn't care."
]
# The limp (the native's broken ankle): the bad leg's steps drop the head
# LIMP_DIP_M and tip it LIMP_ROLL_RAD to that side.
const LIMP_DIP_M := 0.07
const LIMP_ROLL_RAD := 0.05
var _fb_breaks := -1
var _limping := false
var _fb_arm_breaks := -1
var _arm_broken_side := -1   # the broken arm (0 left, 1 right), -1 none
var _fb_hits := -1

const HAPTICS := {
	&"press": [12, 0.3],
	&"tick": [16, 0.35],
	&"grab": [30, 0.6],
	&"vault": [24, 0.65],
	&"mantle": [42, 0.75],
	&"balance": [18, 0.30],
	&"teeter": [26, 0.45],
	&"stumble": [40, 0.70],
	&"roll": [55, 0.55],
	&"slip": [70, 0.85],
	&"reach": [20, 0.45],
	&"release": [14, 0.30],
	&"strain": [28, 0.40],
	&"land": [34, 0.85],
	&"chute": [45, 0.7],
	&"warn": [80, 0.9],
	&"death": [150, 1.0],
	&"break": [55, 1.0],
	&"her": [18, 0.5],   # her hands on the steel you are on or beside
	&"heave": [60, 0.8],   # the chiller drop's car heaving under the feet
	&"rumble": [20, 0.25],   # its air roaring under the floor while it runs
	&"splash": [90, 0.9],   # the body going into a pool (water_feel.gd)
}

# Kinds that always get through the throttle: they mark a moment that
# matters more than whatever buzzed a few milliseconds before.
const HAPTIC_PRIORITY: Array[StringName] = [&"land", &"death", &"break", &"warn", &"slip"]
const HAPTIC_GAP_MS := 50
var haptic_counts := {}   # kind -> pulses sent (counted in CI too, never sent there)
var _haptic_next_ms := 0

func _haptic(kind: StringName, strength: float = 1.0) -> void:
	if not _settings.vibration or not HAPTICS.has(kind):
		return
	# Coalesced: pulses closer than HAPTIC_GAP_MS run together into a buzz
	# the hand cannot read, so the later one is dropped (monotonic clock).
	var now := Time.get_ticks_msec()
	if now < _haptic_next_ms and not kind in HAPTIC_PRIORITY:
		return
	_haptic_next_ms = now + HAPTIC_GAP_MS
	haptic_counts[kind] = int(haptic_counts.get(kind, 0)) + 1
	if _ci_mode:
		return
	var spec: Array = HAPTICS[kind]
	var duration_ms := int(float(spec[0]) * lerpf(0.6, 1.0, strength))
	var amplitude := clampf(float(spec[1]) * strength, 0.05, 1.0)
	match _router.device:
		InputRouter.Device.TOUCH:
			if OS.has_feature("mobile"):
				Input.vibrate_handheld(duration_ms, amplitude)
		InputRouter.Device.GAMEPAD:
			if _router.active_pad >= 0:
				var heavy := kind in [&"land", &"death", &"warn"]
				Input.start_joy_vibration(_router.active_pad, amplitude * 0.8,
					amplitude if heavy else amplitude * 0.25, float(duration_ms) / 1000.0)

# The body's own physics in the hands (Phase 5): each pulse is a native
# event or load, scaled by its size.
#   - a stumble: by the balance the landing took (the native's loss);
#   - a roll: one long, soft pulse as the shoulder goes down;
#   - an edge teeter: a pulse as the XCoM goes past the footing, and again
#     while it stays there;
#   - a beam: corrections pulse faster and harder as the XCoM nears the
#     edge of what the hips can catch (|beam_xcom| 0.5 to 1);
#   - the hands: a hang torn off by the load, a reach caught from bar to bar,
#     a letting go into free flight;
#   - a haul on the pitman's line: the strain, by the pull in the hands.
var _fb_stumbles := 0
var _fb_rolls := 0
var _fb_teetering := false
var _fb_teeter_s := 0.0
var _fb_balance_s := 0.0
var _fb_hang_slips := 0
var _fb_swing_reaches := 0
var _fb_trapeze_slips := 0
var _fb_strain_s := 0.0
var _fb_haul_n := 0.0   # the pull in the hands on the pitman's line, N

func _update_body_feedback(delta: float) -> void:
	var landing: Dictionary = _native.get_landing_state()
	# A landing that broke an ankle: the crack at the feet, the cry, the view
	# buckling down with the leg, felt twice in the hand, and named.
	var breaks := int(landing["ankle_breaks"])
	if _fb_breaks >= 0 and breaks > _fb_breaks:
		var quip: String = BREAK_QUIPS[randi() % BREAK_QUIPS.size()]
		_hud.toast("BROKEN ANKLE", "LANDED AT %.1f M/S  /  %s  /  LIMPING %d S" % [
			float(landing["normal_speed"]), quip, ceili(float(landing["limp_left"]))], UiStyle.HAZARD, 3.5)
		_hud.flash(UiStyle.HAZARD)
		_haptic(&"break")
		_break_pulses.append(BREAK_PULSE_GAP_S * 1.5)
		_audio.ankle_break(_native.get_player_position())
		_slip_jolt = SLIP_JOLT_SECONDS
		_slip_jolt_depth = 1.0
	_fb_breaks = breaks
	_limping = bool(landing["limping"])
	# An arm broken by a blow or a catch torn out of the hands: the crack at
	# the shoulder, the cry, the view jolted, felt twice, and named; the arm
	# held to the body until it heals.
	var arm_breaks := int(landing["arm_breaks"])
	if _fb_arm_breaks >= 0 and arm_breaks > _fb_arm_breaks:
		var arm_quip: String = ARM_QUIPS[randi() % ARM_QUIPS.size()]
		_hud.toast("BROKEN ARM", "%s ARM  /  %s  /  ONE ARM %d S" % [
			"RIGHT" if int(landing["arm_side"]) == 1 else "LEFT", arm_quip, ceili(float(landing["arm_left"]))],
			UiStyle.HAZARD, 3.5)
		_hud.flash(UiStyle.HAZARD)
		_haptic(&"break")
		_break_pulses.append(BREAK_PULSE_GAP_S * 1.5)
		_audio.arm_break(_native.get_player_position())
		_slip_jolt = SLIP_JOLT_SECONDS
		_slip_jolt_depth = 1.0
	_fb_arm_breaks = arm_breaks
	_arm_broken_side = int(landing["arm_side"]) if bool(landing["arm_broken"]) else -1
	# A blow the body felt and lived (sim kHitFeltSpeed): the jolt in the
	# view and the hand, the thud, and from a hard one the cry and the flash.
	var struck: Dictionary = _native.get_death_state()
	var hits := int(struck["hits"])
	if _fb_hits >= 0 and hits > _fb_hits:
		var speed := float(struck["hit_speed_mps"])
		var hard := clampf((speed - 6.0) / 4.0, 0.0, 1.0)
		_haptic(&"land", lerpf(0.5, 1.0, hard))
		# Always the whole jolt from its start, as deep as the blow was hard.
		_slip_jolt = SLIP_JOLT_SECONDS
		_slip_jolt_depth = lerpf(0.5, 1.0, hard)
		_audio.body_hit(speed)
		if hard >= 0.5:
			_hud.flash(UiStyle.HAZARD)
		if hard >= 0.3:
			_hud.blood(0.35 + 0.45 * hard)
	_fb_hits = hits
	var stumbles := int(landing["stumbles"])
	if stumbles > _fb_stumbles:
		_haptic(&"stumble", clampf(float(landing["loss"]) * 2.0, 0.4, 1.0))
		_audio.body_stumble(float(landing["loss"]))
	_fb_stumbles = stumbles
	var rolls := int(landing["rolls"])
	if rolls > _fb_rolls:
		_haptic(&"roll")
	_fb_rolls = rolls
	var teetering := bool(landing["teetering"])
	_fb_teeter_s -= delta
	if teetering and (not _fb_teetering or _fb_teeter_s <= 0.0):
		_haptic(&"teeter")
		_fb_teeter_s = 0.3
	_fb_teetering = teetering
	var xcom := absf(float(landing["beam_xcom"]))
	_fb_balance_s -= delta
	if bool(_ctx["balancing"]) and xcom > 0.5 and _fb_balance_s <= 0.0:
		var near := clampf((xcom - 0.5) / 0.5, 0.0, 1.0)
		_haptic(&"balance", lerpf(0.5, 1.0, near))
		_fb_balance_s = lerpf(0.4, 0.15, near)
	var hands: Dictionary = _native.get_hand_state()
	var hang_slips := int(hands["hang_slips"])
	var trapeze_slips := int(_native.get_trapeze_state()["slips"])
	if hang_slips > _fb_hang_slips or trapeze_slips > _fb_trapeze_slips:
		_haptic(&"slip")
		_audio.body_slip()
		_slip_jolt = SLIP_JOLT_SECONDS
		_slip_jolt_depth = 1.0
	_fb_hang_slips = hang_slips
	_fb_trapeze_slips = trapeze_slips
	var reaches := int(hands["swing_reaches"])
	if reaches > _fb_swing_reaches:
		_haptic(&"reach")
	_fb_swing_reaches = reaches
	var haul := float(_native.get_lift_state()["pitman_haul_n"])
	_fb_haul_n = haul
	_audio.wind_mps = float(_native.get_wind_mps())
	_fb_strain_s -= delta
	if haul > 1.0 and _fb_strain_s <= 0.0:
		_haptic(&"strain", clampf(haul / 700.0, 0.3, 1.0))
		_fb_strain_s = 0.6

# State transitions become feedback: a grab, a landing, a canopy, a lethal
# fall warning, a restore, a higher checkpoint. Each reads native state only.
func _update_feedback(delta: float) -> void:
	_announce_summit()
	var traversal: int = _ctx["traversal"]
	if traversal != _fb_traversal:
		if traversal == TRAVERSAL_HANGING or traversal == TRAVERSAL_CLIMBING or traversal == TRAVERSAL_SWINGING:
			_haptic(&"grab")
		elif traversal == TRAVERSAL_VAULTING:
			_haptic(&"vault")
		elif traversal == TRAVERSAL_MANTLING:
			_haptic(&"mantle")
		elif traversal != TRAVERSAL_NONE:
			_haptic(&"tick")
		elif _fb_traversal == TRAVERSAL_SWINGING and not bool(_ctx["grounded"]):
			_haptic(&"release")   # let go of the bar, into free flight
		_fb_traversal = traversal
	var sliding: bool = _ctx.get("sliding", false)
	if sliding and not _fb_sliding:
		_haptic(&"vault")
	_fb_sliding = sliding
	var wall_steps: int = _ctx.get("wall_steps", 0)
	if wall_steps > _fb_wall_steps:
		_haptic(&"tick")   # a foot on the wall
	_fb_wall_steps = wall_steps
	var springboards: int = _ctx.get("springboards", 0)
	if springboards > _fb_springboards:
		_haptic(&"vault")   # the foot's push off the top
	_fb_springboards = springboards
	var throws: int = _ctx.get("throws", 0)
	if throws > _fb_throws:
		_haptic(&"vault")   # the arms' heave as the load leaves the hands
	_fb_throws = throws
	_update_body_feedback(delta)
	var grounded: bool = _ctx["grounded"]
	var velocity: Vector3 = _ctx["velocity"]
	var lethal: float = _ctx["lethal"]
	# The native velocity read on the last airborne frame. Not the native's
	# last_impact_speed: after a restore the player re-settles within a tick
	# and that value is overwritten with the settle before any HUD sees it.
	var fall_speed_before := _fb_fall_speed
	if grounded and not _fb_grounded and fall_speed_before > 5.0:
		_haptic(&"land", clampf(fall_speed_before / maxf(lethal, 0.001), 0.25, 1.0))
	_fb_grounded = grounded
	_fb_fall_speed = 0.0 if grounded else maxf(0.0, -velocity.y)

	var deaths: int = _ctx["deaths"]
	if deaths > _fb_deaths:
		_fb_deaths = deaths
		var restored: Vector3 = _ctx["checkpoint"]
		var quip: String = LETHAL_RESTORE_QUIPS[randi() % LETHAL_RESTORE_QUIPS.size()]
		# A fall onto the floor, or a blow side-on (run into something).
		var death: Dictionary = _native.get_death_state()
		var how := "HIT AT %.1f M/S" % float(death["speed_mps"]) if bool(death["side_on"]) \
			else "FELL AT %.1f M/S" % fall_speed_before
		_hud.toast("LETHAL IMPACT", "%s  /  %s  /  RESTORED TO CHECKPOINT %+.1f M" % [
			how, quip, restored.y], UiStyle.HAZARD, 3.8)
		_hud.flash(UiStyle.HAZARD)
		_haptic(&"death")

	var chute: bool = _ctx["chute"]
	if chute and not _fb_chute:
		_haptic(&"chute")
		if _fb_warned:
			var relief: String = CHUTE_RELIEF[randi() % CHUTE_RELIEF.size()]
			_hud.toast("CANOPY DEPLOYED", relief, UiStyle.SAFE, 2.5)
	_fb_chute = chute
	if grounded:
		_fb_warned = false
	elif not chute and (float(_ctx["danger"]) >= 0.70 or fall_speed_before > 11.0) and not _fb_warned:
		_fb_warned = true
		_haptic(&"warn")
		var swear: String = FALL_SWEARS[randi() % FALL_SWEARS.size()]
		_hud.toast("PANIC", swear, UiStyle.HAZARD, 2.2)

	var checkpoint_y: float = (_ctx["checkpoint"] as Vector3).y
	if grounded and checkpoint_y > _fb_best_checkpoint_y + CHECKPOINT_TOAST_RISE_METERS:
		_fb_best_checkpoint_y = checkpoint_y
		_hud.toast("CHECKPOINT", "%+.1f M  SECURED" % checkpoint_y, UiStyle.SAFE)
		_hud.show_altimeter(4.0)

# --- first-person arms ----------------------------------------------------------

func _update_view_pitch_offset(delta: float) -> void:
	var target := 0.0
	match int(_ctx.get("traversal", TRAVERSAL_NONE)):
		TRAVERSAL_HANGING, TRAVERSAL_LOWERING:
			target = VIEW_PITCH_HANGING
		TRAVERSAL_SWINGING:
			# Hung from a bar overhead, the head swings under the hands: the
			# view looks up toward them, the grip held SWING_GRIP_ABOVE_CENTRE
			# above the middle of the frame through the whole swing, so the
			# hands and the next bar ahead stay in view.
			var grip: Vector3 = (_native.get_traversal_left_hand() + _native.get_traversal_right_hand()) * 0.5
			var eye: Vector3 = _camera.global_position
			var ahead := (grip - eye).dot(Vector3(-sin(_yaw), 0.0, -cos(_yaw)))
			target = clampf(atan2(grip.y - eye.y, ahead) - SWING_GRIP_ABOVE_CENTRE - _pitch, -0.2, 1.1)
		TRAVERSAL_CLIMBING:
			target = VIEW_PITCH_CLIMBING
		TRAVERSAL_MANTLING:
			target = VIEW_PITCH_MANTLING
		TRAVERSAL_VAULTING:
			target = VIEW_PITCH_VAULTING
	_view_pitch_offset = lerpf(_view_pitch_offset, target, 1.0 - exp(-VIEW_PITCH_RATE * delta))
	if absf(_view_pitch_offset - target) < 0.0005:
		_view_pitch_offset = target

func _arm_material(color: Color, roughness: float, bump: NoiseTexture2D, grain: float,
		emission_energy: float = 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	if emission_energy > 0.0:
		material.emission_enabled = true
		material.emission = color
		material.emission_energy_multiplier = emission_energy
	if bump != null:
		# The world's own procedural normal noise, at weave/grain scale.
		material.normal_enabled = true
		material.normal_texture = bump
		material.uv1_triplanar = true
		material.uv1_scale = Vector3(grain, grain, grain)
	return material

func _build_arms() -> void:
	# The body below the arms (first_person_body.gd), dressed as they are: the
	# jacket and its band, trousers, leather boots on rubber soles. Cloth
	# barely shines: with the default sheen the sun on a crouched back reads
	# pale.
	var jacket := _arm_material(Color("1f231c"), 0.93, _bump_concrete, 9.0)
	var trousers := _arm_material(Color("23262b"), 0.9, _bump_concrete, 9.0)
	var boots := _arm_material(Color("1c130b"), 0.82, _bump_concrete, 16.0)
	jacket.metallic_specular = 0.08
	trousers.metallic_specular = 0.08
	boots.metallic_specular = 0.15
	_body = FirstPersonBody.new()
	add_child(_body)
	_body.build(jacket, _arm_material(Color("b8611d"), 0.55, null, 1.0, 0.1), trousers, boots,
		_arm_material(Color("0d0d0c"), 0.95, null, 1.0))
	_arms = FirstPersonArms.new()
	_arms.name = "FirstPersonArms"
	add_child(_arms)
	_arms.build(
		# Albedos sit dark on purpose: the yard's exposure lifts anything
		# this close to the lens toward pastel.
		_arm_material(Color("1f231c"), 0.93, _bump_concrete, 9.0),
		_arm_material(Color("b8611d"), 0.55, null, 1.0, 0.1),
		_arm_material(Color("3b2616"), 0.72, _bump_rust, 14.0),
		_arm_material(Color("22170e"), 0.82, _bump_rust, 14.0),
		_arm_material(Color("7d5806"), 0.5, null, 1.0),
		_arm_material(Color("0c0c0d"), 0.62, null, 1.0),
		_arm_material(Color("303033"), 0.5, null, 1.0),
		_arm_material(Color("b8281b"), 0.45, null, 1.0),
		_arm_material(Color("59e08a"), 0.4, null, 1.0, 2.2),
		_arm_material(Color("d7d2c4"), 0.72, null, 1.0))

# Everything the arms may know, all of it native or already-presented state.
func _arms_state(intent: Dictionary) -> Dictionary:
	if bool(_ctx.get("trapeze_hanging", false)):
		# On the trapeze the hands hang on its bar, either side of the body.
		var bar: Vector3 = _native.get_trapeze_state()["bar"]
		var right := Vector3(cos(_yaw), 0.0, -sin(_yaw))
		var across := 0.25 if right.z >= 0.0 else -0.25
		return {
			"traversal": TRAVERSAL_HANGING_STATE,
			"progress": 1.0,
			"ledge_point": bar,
			"hand_left": bar + Vector3(0.0, 0.0, -across),
			"hand_right": bar + Vector3(0.0, 0.0, across),
			"structure_normal": Vector3(1.0, 0.0, 0.0),
			"sprinting": false,
			"balancing": false,
			"affordance": false,
			"affordance_point": Vector3.ZERO,
			"position": _ctx["position"],
			"velocity": _ctx["velocity"],
			"grounded": false,
			"chute": false,
			"carrying": 0,
			"carry_center": Vector3.ZERO,
			"carry_half": 0.18,
			"move": intent["move"],
			"bob_phase": _cam_bob_phase,
			"broken_arm": _arm_broken_side,
		}
	return {
		"strike": _ctx.get("strike", {}),
		"traversal": int(_ctx["traversal"]),
		"progress": float(_native.get_traversal_progress()),
		"ledge_point": _native.get_traversal_ledge_point(),
		"hand_left": _native.get_traversal_left_hand(),
		"hand_right": _native.get_traversal_right_hand(),
		"structure_normal": _native.get_traversal_normal(),
		"sprinting": bool(_ctx["sprinting"]),
		"balancing": bool(_ctx["balancing"]),
		"affordance": bool(_native.is_ledge_available()),
		"affordance_point": _native.get_ledge_point(),
		"position": _ctx["position"],
		"velocity": _ctx["velocity"],
		"grounded": bool(_ctx["grounded"]),
		"chute": bool(_ctx["chute"]),
		"carrying": int(_ctx["carrying"]),
		"carry_center": _carry_center(int(_ctx["carrying"])),
		"carry_half": _carry_half(int(_ctx["carrying"])),
		# The hook out: the right fist closes on its line.
		"hook_line": _hand_line_end(),
		"move": intent["move"],
		"bob_phase": _cam_bob_phase,
		"broken_arm": _arm_broken_side,
		# The yard crane's remote held out in both hands while it is worked,
		# its joysticks pressed the way the stick and the hoist go.
		"operating": &"crane" if bool(_ctx.get("crane_operating", false)) else &"",
		"pendant": Vector2(Vector2(intent["move"]).x,
			(1.0 if bool(intent.get("raise_held", false)) else 0.0) - (1.0 if bool(intent.get("lower_held", false)) else 0.0)),
	}

# --- telemetry overlay, sized to the bounds the device actually gives us ------

func _layout_hud() -> void:
	_viewport_size = get_viewport().get_visible_rect().size
	var short_edge := minf(_viewport_size.x, _viewport_size.y)
	var scale := clampf(short_edge / 1100.0, 0.62, 1.7)
	var gutter := roundf(30.0 * scale)

	# Every readout, not just the first eight: the machine lines were left out
	# of the scaling pass, so at Fold scale they kept their authored size while
	# the VBox stayed at its authored height, and the box squeezed thirteen
	# children into room for eight. They overlapped into an unreadable stack.
	var readouts := [_status, _position_value, _velocity_value, _support_value,
			_traversal_value, _tick_value, _fall_value]
	for label in readouts:
		if label == null:
			continue
		var base := 24.0 if label == _status else 15.0
		label.add_theme_font_size_override("font_size", int(roundf(base * scale)))

	# Below the touch pause button, which owns the top-left corner.
	var top_left: Control = $HUD/TopLeft
	top_left.offset_left = gutter
	top_left.offset_top = gutter + roundf(150.0 * short_edge / 1856.0)
	top_left.offset_right = gutter + _viewport_size.x * 0.52
	# Size the box to what it actually has to hold, so the container never
	# compresses a line out of legibility.
	var line_height := roundf(15.0 * scale) + 9.0
	top_left.offset_bottom = top_left.offset_top + roundf(24.0 * scale) + 9.0 \
		+ line_height * float(readouts.size())

	# Top-centre: the top-right corner belongs to the altimeter.
	var top_right: Control = $HUD/TopRight
	top_right.anchor_left = 0.5
	top_right.anchor_right = 0.5
	top_right.offset_left = -_viewport_size.x * 0.2
	top_right.offset_right = _viewport_size.x * 0.2
	top_right.offset_top = gutter
	for child in top_right.get_children():
		if child is Label:
			(child as Label).horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER

# --- presentation mirror ----------------------------------------------------

# Every term is a pure function of THIS frame's native state (or a bounded
# transient timer that provably reaches exactly zero), so a stationary,
# grounded player always reads back exactly EYE_OFFSET / FOV_BASE -- see this
# file's own header comment on the constants block above for why that must
# hold for the CI runtime proof.
func _apply_camera_feel(position: Vector3, velocity: Vector3, grounded: bool, crouched: bool,
		delta: float) -> void:
	if grounded and not _cam_was_grounded:
		var impact := absf(_cam_last_velocity_y)
		_cam_landing_timer = LANDING_DIP_DURATION_SECONDS
		_cam_landing_strength = smoothstep(LANDING_DIP_MIN_IMPACT_MPS,
			LANDING_DIP_FULL_IMPACT_MPS, impact)
	_cam_was_grounded = grounded
	_cam_last_velocity_y = velocity.y

	var dip := 0.0
	if _cam_landing_timer > 0.0:
		_cam_landing_timer = maxf(0.0, _cam_landing_timer - delta)
		var t := 1.0 - _cam_landing_timer / LANDING_DIP_DURATION_SECONDS
		var landing_wave := sin(PI * t)
		dip = -_cam_landing_strength * LANDING_DIP_MAX_METERS * landing_wave * landing_wave

	var horizontal_speed := Vector2(velocity.x, velocity.z).length()
	# 5.5 mirrors kPlayerMaximumRelativeSpeed (src/sim/simulation.cpp) -- a
	# curve-shape input, not a gameplay bound, so the native constant is not
	# exposed through the bridge just for this.
	var bob_target := 0.0
	if grounded:
		bob_target = smoothstep(HEAD_BOB_SPEED_FLOOR_MPS, HEAD_BOB_SPEED_FULL_MPS, horizontal_speed)
		# The gait's phase (presentation/gait.gd): half a turn a footfall, the
		# left foot down at even multiples of PI, the right at odd.
		# Only while the feet are moving: standing still, the gait holds.
		if horizontal_speed > HEAD_BOB_SPEED_FLOOR_MPS:
			_cam_bob_phase += PI * Gait.cadence(horizontal_speed) * delta
	if not _head_bob_on:
		_cam_bob_weight = 0.0
	else:
		_cam_bob_weight = lerpf(_cam_bob_weight, bob_target,
			1.0 - exp(-HEAD_BOB_RESPONSE_PER_SECOND * delta))
		if absf(_cam_bob_weight - bob_target) < 0.001:
			_cam_bob_weight = bob_target
	# The head is lowest as each foot takes the weight, and sways over the
	# foot that is down: one dip a footfall, one sway a pair.
	var vertical_bob := -HEAD_BOB_VERTICAL_METERS * cos(2.0 * _cam_bob_phase) * _cam_bob_weight
	var lateral_bob := HEAD_BOB_LATERAL_METERS * sin(_cam_bob_phase) * _cam_bob_weight
	var right_vector := Vector3(cos(_yaw), 0.0, -sin(_yaw))
	if not _head_bob_on:
		_cam_bank = 0.0
	else:
		var lateral_speed := velocity.dot(right_vector)
		var bank_target := -CAMERA_BANK_MAX_RADIANS * clampf(lateral_speed / 5.5, -1.0, 1.0)
		_cam_bank = lerpf(_cam_bank, bank_target,
			1.0 - exp(-CAMERA_BANK_RESPONSE_PER_SECOND * delta))
		if absf(_cam_bank - bank_target) < 0.0001:
			_cam_bank = bank_target
	# The roll this frame: the eased bank from the sideways speed, and on it
	# what the body is doing now. Built fresh each frame: added into the eased
	# value, a lean compounded through its easing (a wall run's 5 degrees
	# settled at 34 at 60 fps, more the higher the frame rate).
	var bank := _cam_bank

	# A broken ankle: on the bad leg's steps (every other one) the head drops
	# and tips to that side, the good leg's as before.
	if _limping and grounded:
		var hobbling := smoothstep(0.3, 1.5, horizontal_speed)
		var bad_step := maxf(0.0, -cos(_cam_bob_phase))
		dip += -LIMP_DIP_M * bad_step * bad_step * hobbling
		bank += LIMP_ROLL_RAD * bad_step * hobbling

	# Parkour movement dynamics and tactile camera feel:
	var traversal: int = int(_ctx.get("traversal", TRAVERSAL_NONE))
	var traversal_prog: float = float(_native.get_traversal_progress()) if _native else 0.0
	var parkour_fov := 0.0
	if traversal == TRAVERSAL_VAULTING:
		# Athletic speed vault: tuck dip and subtle roll bank along apex
		var vault_wave := sin(PI * clampf(traversal_prog, 0.0, 1.0))
		dip += -0.07 * vault_wave
		bank += 0.035 * vault_wave
		parkour_fov = 4.5 * vault_wave
	elif traversal == TRAVERSAL_MANTLING:
		# Muscular mantle heave: hand plant compression then powerful crest push-up
		if traversal_prog < 0.45:
			dip += -0.05 * (1.0 - traversal_prog / 0.45)
		else:
			var mantle_heave := sin(PI * (traversal_prog - 0.45) / 0.55)
			dip += 0.04 * mantle_heave
		parkour_fov = 3.5 * sin(PI * clampf(traversal_prog, 0.0, 1.0))
	elif bool(_ctx.get("wall_climbing", false)):
		# Running up a wall: no lean; the view widens with the push up it.
		parkour_fov = 3.5
	elif bool(_ctx.get("wall_running", false)):
		# Running on the wall: the body leans off it, the view with it.
		parkour_fov = 3.0
	elif bool(_ctx.get("sliding", false)):
		# On the hip: the view leans back and a little to the trailing side,
		# and widens with the speed the slide carries.
		bank += 0.05
		parkour_fov = 4.0 * clampf(Vector2(velocity.x, velocity.z).length() / 8.0, 0.0, 1.0)
	elif bool(_ctx.get("balancing", false)) and grounded:
		# Narrow beam balance micro-sway: subtle tightrope dynamic centering
		var sway := sin(_cam_bob_phase * 0.75)
		lateral_bob += 0.025 * sway
		bank += 0.015 * sway

	# The wall's outward normal on the right means the wall is on the left:
	# the head leans right, off it (a negative roll), and the other way round.
	var wall_lean_target := 0.0
	if bool(_ctx.get("wall_running", false)) and not bool(_ctx.get("wall_climbing", false)):
		var wall_normal: Vector3 = _ctx.get("wall_normal", Vector3.ZERO)
		wall_lean_target = -WALL_LEAN_RAD * clampf(wall_normal.dot(right_vector) * 2.0, -1.0, 1.0)
	_wall_lean = lerpf(_wall_lean, wall_lean_target, 1.0 - exp(-WALL_LEAN_RESPONSE_PER_SECOND * delta))
	if absf(_wall_lean - wall_lean_target) < 0.0005:
		_wall_lean = wall_lean_target
	bank += _wall_lean
	# Flow's speed felt: the slight shake, on the ground or in the air.
	var shake_target := smoothstep(FLOW_FROM_MPS, FLOW_FULL_MPS, horizontal_speed) \
		if _head_bob_on and not _reduced_motion else 0.0
	_speed_shake = move_toward(_speed_shake, shake_target, delta / 0.4)
	var shake_pitch := 0.0
	if _speed_shake > 0.0:
		_shake_clock += delta
		shake_pitch = SPEED_SHAKE_RAD * _speed_shake * sin(TAU * SPEED_SHAKE_HZ.x * _shake_clock)
		bank += SPEED_SHAKE_RAD * _speed_shake * sin(TAU * SPEED_SHAKE_HZ.y * _shake_clock + 1.1)
	else:
		_shake_clock = 0.0

	# A hang torn off: the view drops and tips down as the hands go.
	var slip_pitch := 0.0
	if _slip_jolt > 0.0:
		_slip_jolt = maxf(0.0, _slip_jolt - delta)
		var slip_wave := _slip_jolt_depth * sin(PI * (1.0 - _slip_jolt / SLIP_JOLT_SECONDS))
		dip += -0.09 * slip_wave
		slip_pitch = -0.07 * slip_wave
	# Hauling the pitman's line: the body heaves with each pull, by the force
	# in the hands.
	var haul := _fb_haul_n
	if haul > 1.0:
		_haul_phase += delta * 2.6
		dip += -0.018 * clampf(haul / 700.0, 0.0, 1.0) * (0.5 + 0.5 * sin(_haul_phase))
	# Reduced motion: what moves the view by itself is turned down.
	var motion := 0.25 if _reduced_motion else 1.0
	dip *= motion
	var eye := position + EYE_OFFSET
	_crouch_eye = move_toward(_crouch_eye, 1.0 if crouched else 0.0, delta / CROUCH_EYE_SECONDS)
	if crouched or _crouch_eye > 0.0:
		var soles_y := position.y - (CROUCH_HALF_HEIGHT if crouched else STAND_HALF_HEIGHT)
		eye.y = soles_y + lerpf(STAND_HALF_HEIGHT + EYE_OFFSET.y, CROUCH_EYE_OVER_SOLES,
			smoothstep(0.0, 1.0, _crouch_eye))
	# A new native (a restart) counts its steps from zero: only a rise lags.
	var stepped_total := float(_native.get_step_up_meters())
	if stepped_total > _step_meters_seen:
		_step_eye_lag = maxf(_step_eye_lag - (stepped_total - _step_meters_seen), -STEP_EYE_MAX_LAG)
	_step_meters_seen = stepped_total
	_step_eye_lag = move_toward(_step_eye_lag, 0.0, maxf(STEP_EYE_MIN_MPS * delta,
		-_step_eye_lag * (1.0 - exp(-STEP_EYE_RESPONSE_PER_SECOND * delta))))
	eye.y += _step_eye_lag
	_camera.position = eye + Vector3(0.0, dip + vertical_bob, 0.0) + right_vector * lateral_bob
	# Phase 4, read from the native's recovery: a roll turns the view once
	# over, forward, through the native roll's own progress; a stumble's
	# lost balance leans it. Both are zero when recovered.
	var landing: Dictionary = _ctx.get("landing", {})
	var roll_turn := 0.0
	var roll_progress := float(landing.get("roll_progress", 0.0))
	if roll_progress > 0.0:
		roll_turn = -TAU * smoothstep(0.0, 1.0, roll_progress)
	var stagger := 0.0
	if int(landing.get("response", 0)) == 4:
		stagger = 0.12 * (1.0 - float(landing.get("balance", 1.0)))
	# Teetering at an edge: the native's XCoM is past it; the view sways and
	# leans out over the drop until the feet stop the body or it goes over.
	_teeter_weight = move_toward(_teeter_weight, 1.0 if bool(landing.get("teetering", false)) else 0.0,
		delta / 0.15)
	if _teeter_weight > 0.0:
		_teeter_phase += delta * 11.0
		stagger += _teeter_weight * 0.06 * sin(_teeter_phase)
		roll_turn -= _teeter_weight * 0.12
	else:
		_teeter_phase = 0.0
	# On a beam: the view banks toward the side the native's XCoM is on, in
	# proportion to how much of the centre of pressure's reach it has used
	# (1 at the edge of recovery), so the lean reads before it is lost.
	var lean: Vector3 = landing.get("beam_lean", Vector3.ZERO)
	_beam_bank = lerpf(_beam_bank, clampf(lean.dot(right_vector), -1.5, 1.5), 1.0 - exp(-12.0 * delta))
	stagger -= 0.07 * _beam_bank
	if _reduced_motion:
		roll_turn = 0.0
	_camera.rotation = Vector3(_pitch + _view_pitch_offset + roll_turn + slip_pitch * motion + shake_pitch, _yaw,
		bank + stagger * motion)
	# Driving the forklift, the head goes with the seat: the view tilts with
	# the truck's floor, so its nose going down under a load is seen.
	if _forklift_view != null and bool(_ctx.get("forklift_driving", false)) and not _reduced_motion:
		var floor_up: Vector3 = _forklift_view.body_transform().basis.y.normalized()
		var axis := Vector3.UP.cross(floor_up)
		if axis.length() > 1.0e-5:
			_camera.basis = Basis(axis.normalized(), Vector3.UP.angle_to(floor_up)) * _camera.basis
	if _launch_shot.active() and not _reduced_motion:
		_camera.basis = _launch_shot.orient(_camera.basis, _camera.global_position)
	_feel_hold(delta)
	_feel_strike(delta)
	if _death_cam_left > 0.0:
		_ride_death()

	var fov_ground := FOV_SPRINT_MAX_DEGREES * smoothstep(0.0, 5.5, horizontal_speed) + \
		FOV_FLOW_DEGREES * smoothstep(FLOW_FROM_MPS, FLOW_FULL_MPS, horizontal_speed) + parkour_fov
	var fov_fall := 0.0
	if not grounded and velocity.y < 0.0:
		fov_fall = FOV_FALL_MAX_DEGREES * smoothstep(0.0, FOV_FALL_FULL_MPS, -velocity.y)
	if not _speed_fov_on:
		_cam_fov_offset = 0.0
	else:
		var fov_target := fov_ground + fov_fall
		_cam_fov_offset = lerpf(_cam_fov_offset, fov_target,
			1.0 - exp(-FOV_RESPONSE_PER_SECOND * delta))
		if absf(_cam_fov_offset - fov_target) < 0.005:
			_cam_fov_offset = fov_target
	_camera.fov = _fov_base + _cam_fov_offset + _hold_cam.fov_offset() + \
		_strike_feel.fov_offset(_ctx.get("strike", {}))

# The punch, felt (life/strike_feel.gd): its edges to the arms' hit-stop,
# her flinch (matron_view.gd), the gore at the knuckles (gore_view.gd) and
# the audio's hooks; the kick over the camera. Viewer only.
var _strike_feel: RefCounted = preload("res://presentation/life/strike_feel.gd").new()

func _feel_strike(delta: float) -> void:
	var strike: Dictionary = _ctx.get("strike", {})
	if strike.is_empty():
		return
	var edges: Array = _strike_feel.update(strike, _camera.basis, delta, _reduced_motion)
	for edge in edges:
		var at: Vector3 = strike.get("hit_point", _camera.global_position)
		var impulse := float(strike.get("impulse", 0.0))
		_audio.player_strike(edge, at if edge == &"landed" else _camera.global_position, impulse)
		if edge == &"landed":
			var normal: Vector3 = strike.get("hit_normal", Vector3.ZERO)
			if _matron_view != null:
				_matron_view.flinch(normal, impulse)
			if _gore != null:
				_gore._on_strike(at, normal, impulse)
	if _matron_view != null:
		_matron_view.hold_frame = _strike_feel.frozen()
	if not _hold_cam.active():
		_camera.basis = _strike_feel.kick() * _camera.basis

## The drawn arms' step this frame: none while the punch's hit-stop holds.
func _arms_delta(delta: float) -> float:
	return 0.0 if _strike_feel.frozen() else delta

# Her hands on the body (life/matron_hold_camera.gd): while held, dangled or
# claimed the head is the body's quaternion with the free look clamped about
# it; the fatality's beats frame over that. A plain overlay carries the spin
# vignette and the closing black.
var _hold_cam: RefCounted = preload("res://presentation/life/matron_hold_camera.gd").new()
var _hold_veil: ColorRect = null
# The runs she has ended (the native's count). Each one builds the world
# again, her look over it with it: main.gd has that made behind its card.
var _runs_she_ended := -1
var _her_world_new := false

func _feel_hold(delta: float) -> void:
	if _matron_view == null or not _native.has_method("get_matron_hold_state"):
		return
	var hold: Dictionary = _native.get_matron_hold_state()
	var ended := int(hold.get("runs_ended_by_matron", 0))
	if _runs_she_ended >= 0 and ended > _runs_she_ended:
		_her_world_new = true
	_runs_she_ended = ended
	var held_basis: Basis = _hold_cam.orient(hold, _camera.basis, _yaw, _pitch, delta, _reduced_motion)
	# Only while she has the body: written back every frame, an unchanged
	# basis still re-reads the camera's angles inside +-90 deg of pitch, and a
	# roll's view (turned over past that) reads as never having turned.
	if _hold_cam.active():
		_camera.basis = held_basis
	var veil: float = maxf(_hold_cam.vignette() * 0.6, _hold_cam.black())
	if veil > 0.002 and _hold_veil == null:
		_hold_veil = ColorRect.new()
		_hold_veil.color = Color(0, 0, 0, 0)
		_hold_veil.mouse_filter = Control.MOUSE_FILTER_IGNORE
		_hold_veil.set_anchors_preset(Control.PRESET_FULL_RECT)
		var layer := CanvasLayer.new()
		layer.layer = 50
		add_child(layer)
		layer.add_child(_hold_veil)
	if _hold_veil != null:
		_hold_veil.color.a = veil

# Gore (life/gore/gore_view.gd): wounds, blood and the lens, from the hold
# state. Always full: there is no setting that softens it. The same read of
# the hold is what her hands on the body are heard from (matron_view.gd
# hear_hold).
var _gore: Node3D = null

func _feel_gore(her_state: Dictionary, delta: float) -> void:
	if not _native.has_method("get_matron_hold_state"):
		return
	if _gore == null:
		_gore = preload("res://presentation/life/gore/gore_view.gd").new()
		add_child(_gore)
		_gore.attach_arms(_arms)
		_gore.matron_view = _matron_view
	var hold: Dictionary = _native.get_matron_hold_state()
	_gore.update_view(hold, her_state, delta, _camera, float(her_state.get("floor_y", 0.0)))
	_matron_view.hear_hold(hold)

func _render_snapshot(delta: float = 0.0) -> void:
	var position: Vector3 = _native.get_player_position()
	var render_position: Vector3 = _native.get_player_render_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var crouched := bool(_native.is_player_crouched())

	if _corpse_view != null:
		_feel_death(delta)
	_apply_camera_feel(render_position, velocity, grounded, crouched, delta)
	if delta > 0.0:
		_audio.update(delta, position, velocity, grounded, int(_native.get_support_entity_id()),
			int(_ctx["traversal"]), bool(_native.is_parachute_deployed()),
			int(_native.get_death_count()), crouched, _native.get_support_point_linear_velocity())
		var hit_sequence := int(_native.get_last_impact_sequence())
		if hit_sequence != _impact_sequence:
			_impact_sequence = hit_sequence
			_audio.update_impacts(_native.get_impacts(), _camera.global_position)
	# The developer telemetry overlay costs a dozen string formats a frame;
	# it is only paid for while the overlay is actually on screen.
	if _telemetry_on:
		_write_telemetry(position, velocity, grounded)
	_render_kit()
	if _fabricated_view != null:
		_fabricated_view.follow()
	_render_hook()
	if delta > 0.0 and _birds != null:
		_birds.update(delta, position, velocity, grounded)
	if _crew != null:
		_crew.update(delta, _camera.global_position, position)
	_render_slingshot(delta)
	if _swing_view != null:
		_swing_view.update_view(_native.get_swing_state())
	if _zipline_view != null:
		_feel_zipline(delta)
	if _summit_view != null:
		_summit_view.update_view(_native.get_summit_state(), delta, float(_native.get_simulation_time_seconds()))
	if _matron_view != null:
		var her_state: Dictionary = _native.get_matron_state()
		_matron_view.update_view(her_state, delta, _camera.global_position, position)
		_feel_her(her_state, position)
		_feel_gore(her_state, delta)
	if _forklift_view != null:
		_feel_forklift(delta)
	if _crane_view != null:
		_feel_crane(delta)
	_sample_machines(delta)
	_sample_wet_e(delta)
	_sample_bay()
	_feel_bay()
	if delta > 0.0:
		_feel_wet_e(delta)
		_feel_water(delta)


# Water (sim physics_world_water.cpp): the splash, the tint and fog under the
# surface, the muffle on the effects, the haptic at the entry. All of it is
# read from the native's water state; nothing here decides wet or dry.
var _water_feel: Node = null

func _feel_water(delta: float) -> void:
	if not _native.has_method(&"get_water_state"):
		return
	if _water_feel == null:
		_water_feel = preload("res://presentation/water_feel.gd").new()
		add_child(_water_feel)
	var entered: float = _water_feel.update_view(_native.get_water_state(), _camera, _audio, delta)
	if entered > 0.0:
		_haptic(&"splash", clampf(0.4 + entered / 12.0, 0.4, 1.0))


# Deaths (sim/corpse.hpp): the body a death leaves is drawn where it lies.
# The view rides the newest one's head through the hit (DEATH_CAM_HEAD_S,
# shaken), then pulls up and back over the body, looking down at it and the
# blood, until it lies still (DEATH_CAM_MIN_S at least, DEATH_CAM_MAX_S at
# most), the restored body waiting. The blood goes up off where it hit and
# lies there; the bones are heard and felt breaking, as many as it hit hard.
const DEATH_CAM_MIN_S := 1.8
const DEATH_CAM_MAX_S := 2.8
const DEATH_CAM_HEAD_S := 0.35
const DEATH_CAM_PULL_S := 0.6
const DEATH_CAM_OVER_M := 2.6
const DEATH_CAM_BACK_M := 1.6
const DEATH_SHAKE_RAD := 0.06
const BREAK_PULSE_GAP_S := 0.09
var _death_cam_left := 0.0
var _death_cam_shown := 0.0
var _death_seen := -1
var _death_look := Vector3.FORWARD   # the facing at the death, in the head's frame
var _death_shake := 0.0
var _death_point := Vector3.ZERO
var _death_back := Vector3.BACK      # behind the facing at the death
var _corpses: Array = []
var _break_pulses: Array = []   # seconds until each break is felt
var deaths_ridden := 0          # counted for the touch proof

func _feel_death(delta: float) -> void:
	_corpses = _native.get_corpses()
	var death: Dictionary = _native.get_death_state()
	var spawned := int(death["corpses_spawned"])
	if _death_seen < 0:
		_death_seen = spawned
	if spawned > _death_seen and not _corpses.is_empty():
		_death_seen = spawned
		deaths_ridden += 1
		var speed := float(death["speed_mps"])
		var lethal := float(_native.get_lethal_impact_speed_mps())
		var hard := clampf(speed / maxf(lethal, 1.0), 1.0, 3.0)
		_death_point = death["point"]
		_death_back = Vector3(sin(_yaw), 0.0, cos(_yaw))
		_corpse_view.splash(death["point"], speed, lethal)
		_hud.blood(1.0)
		_audio.bone_cracks(death["point"], speed, lethal)
		_break_pulses.clear()
		for i in clampi(roundi(1.0 + 2.0 * (hard - 1.0)), 1, 5):
			_break_pulses.append(BREAK_PULSE_GAP_S * float(i + 1))
		var head: Transform3D = CorpseView.head_of(_corpses)
		_death_look = head.basis.inverse() * Vector3(-sin(_yaw), 0.0, -cos(_yaw))
		_death_cam_left = DEATH_CAM_MAX_S
		_death_cam_shown = 0.0
		_death_shake = clampf(0.4 + 0.3 * (hard - 1.0), 0.4, 1.0)
	_corpse_view.update_view(_corpses, _death_cam_left > 0.0 and _death_cam_shown < DEATH_CAM_HEAD_S + 0.1)
	if delta <= 0.0:
		return
	var due: Array = []
	for left in _break_pulses:
		if float(left) - delta <= 0.0:
			_haptic(&"break")
		else:
			due.append(float(left) - delta)
	_break_pulses = due
	if _death_cam_left > 0.0:
		_death_cam_left -= delta
		_death_cam_shown += delta
		_death_shake = maxf(0.0, _death_shake - 2.0 * delta)
		if _death_cam_shown >= DEATH_CAM_MIN_S and not _corpses.is_empty() and bool(_corpses[-1]["resting"]):
			_death_cam_left = 0.0

# The view in the newest corpse's head (its eyes where the face is, looking
# the way the head now faces, shaken by the hit), then pulled up and back
# over the body, looking down at it and where it hit.
func _ride_death() -> void:
	if _corpses.is_empty():
		return
	var head: Transform3D = CorpseView.head_of(_corpses)
	var up := head.basis.y.normalized()
	var look := (head.basis * _death_look).normalized()
	if absf(look.dot(up)) > 0.98:
		up = head.basis.z.normalized()
	var basis := Basis.looking_at(look, up)
	if _death_shake > 0.0 and not _reduced_motion:
		var t := _death_cam_shown * 37.0
		basis = basis.rotated(basis.x.normalized(), sin(t) * DEATH_SHAKE_RAD * _death_shake)
		basis = basis.rotated(basis.y.normalized(), cos(t * 1.3) * DEATH_SHAKE_RAD * _death_shake)
	var eyes := head.origin + up * 0.03 + look * 0.1
	var out := smoothstep(DEATH_CAM_HEAD_S, DEATH_CAM_HEAD_S + DEATH_CAM_PULL_S, _death_cam_shown)
	if out > 0.0:
		var pelvis: Transform3D = (_corpses[-1]["parts"] as Array)[0]
		var target := pelvis.origin.lerp(_death_point, 0.35)
		var over := pelvis.origin + Vector3.UP * DEATH_CAM_OVER_M + _death_back * DEATH_CAM_BACK_M
		var over_basis := Basis.looking_at(target - over, Vector3.UP)
		basis = Basis(basis.get_rotation_quaternion().slerp(over_basis.get_rotation_quaternion(), out))
		eyes = eyes.lerp(over, out)
	_camera.global_transform = Transform3D(basis, eyes)


# The ziplines (sim/zipline.hpp): every trolley and brake block drawn where
# the native has them, the ride heard (the sheaves on the cable, the brake
# block's drag) and felt in the hands: the bar taken, the block run into,
# and the sheaves' buzz, a pulse every few metres of cable.
const ZIP_BUZZ_M := 3.0
var _zip_was_riding := false
var _zip_was_braking := false
var _zip_buzz_m := 0.0
var _zip_torn_seen := 0

func _feel_zipline(delta: float) -> void:
	var state: Dictionary = _native.get_zipline_state()
	_zipline_view.update_view(_native.get_zipline_trolleys(), state)
	if delta <= 0.0:
		return
	var riding := bool(state["riding"])
	var speed := absf(float(state["speed_mps"]))
	var braking := riding and bool(state["braking"])
	_audio.update_zipline(riding, speed, braking, state["bar"])
	if riding and not _zip_was_riding:
		_haptic(&"reach")
	if braking and not _zip_was_braking:
		_haptic(&"strain")
	# The stop's jolt tore the hands off the bar.
	var torn := int(state.get("torn", 0))
	if torn > _zip_torn_seen:
		_haptic(&"slip")
	_zip_torn_seen = torn
	if riding and speed > 2.0:
		_zip_buzz_m -= delta * speed
		if _zip_buzz_m <= 0.0:
			_zip_buzz_m = ZIP_BUZZ_M
			_haptic(&"strain" if braking else &"release", clampf(speed / 16.0, 0.3, 1.0))
	else:
		_zip_buzz_m = 0.0
	_zip_was_riding = riding
	_zip_was_braking = braking


# The forklift (sim/forklift.hpp): its moving parts drawn, its engine, pump
# and valves heard, and, to its driver, its strain in the hands. The strain
# is the pump's push as a share of what its relief valve allows, while the
# lever lifts. The rear wheels count as off the ground after 0.1 s clear of
# it, so a bump is not a lift-off.
const FORK_REAR_OFF_SECONDS := 0.1
var _fork_lever := 0.0
var _fork_last_n := 0.0
var _fork_rear_off := 0.0
var _fork_strain_clock := 0.0
var _fork_groan_clock := 0.0

# The yard crane: its rope and lamps drawn, its drives heard; a drive coming
# to rest brakes with a clank, the block striking its upper limit with a
# harder one, and a load set down with the rope going slack lands; the
# remote's thumb feels each.
var _crane_slewing := false
var _crane_trolleying := false
var _crane_at_limit := false
var _crane_slack := false
var _crane_had_remote := false
var _crane_holding := 0
var _crane_device := 0

func _feel_crane(delta: float) -> void:
	var state: Dictionary = _native.get_yard_crane_state()
	_crane_view.update_view(state)
	if delta <= 0.0:
		return
	var has_remote := bool(state["has_remote"])
	if has_remote and not _crane_had_remote:
		_hud.toast("THE CRANE'S REMOTE", "CRANE: THE STICK SENDS THE HOOK, JUMP AND CROUCH HOIST", UiStyle.AMBER, 6.0)
		_haptic(&"press", 0.6)
	_crane_had_remote = has_remote
	var jib: Transform3D = _crane_view.jib_transform()
	var magnet: Vector3 = _native.get_kit_body_transform(_native.get_kit_body_index(YARD_CRANE_MAGNET)).origin
	_audio.update_crane(delta, state, jib, magnet)
	var operating := bool(state["operating"])
	# The magnet taking hold of steel and letting it go (switched off, or the
	# load torn off it); the spreader's twistlocks locking and unlocking. Only
	# while the same device stays on the sling: unrigged, it reads nothing held.
	var holding := int(state["holding_entity"])
	var device := int(state["device"])
	if device == _crane_device and (holding != 0) != (_crane_holding != 0):
		var taken := holding != 0
		if device == CRANE_DEVICE_MAGNET:
			_audio.crane_cue(&"magnet_take" if taken else &"magnet_drop", magnet, 1.0 if taken else 0.8)
		elif device == CRANE_DEVICE_SPREADER:
			var spreader: Vector3 = _native.get_kit_body_transform(
				_native.get_kit_body_index(YARD_CRANE_SPREADER)).origin
			_audio.crane_cue(&"twistlocks", spreader, 1.0)
		if operating:
			_haptic(&"land" if taken else &"press", 0.5 if taken else 0.6)
	_crane_holding = holding
	_crane_device = device
	var slewing := absf(float(state["slew_rate"])) > 0.002
	var trolleying := absf(float(state["trolley_rate"])) > 0.02
	if _crane_slewing and not slewing:
		_audio.crane_cue(&"brake", jib * Vector3(0.0, 0.4, 0.0), 0.8)
		if operating:
			_haptic(&"press", 0.4)
	if _crane_trolleying and not trolleying:
		_audio.crane_cue(&"brake", state["rope_top"], 0.6)
	var at_limit := bool(state["upper_limit"])
	if at_limit and not _crane_at_limit:
		_audio.crane_cue(&"limit", state["rope_bottom"], 1.0)
		if operating:
			_haptic(&"press", 0.8)
	var slack := bool(state["slack"])
	if slack and not _crane_slack and int(state["load_entity"]) != 0:
		_audio.crane_cue(&"landed", state["hook"], 1.0)
		if operating:
			_haptic(&"land", 0.6)
	_crane_slewing = slewing
	_crane_trolleying = trolleying
	_crane_at_limit = at_limit
	_crane_slack = slack

func _feel_forklift(delta: float) -> void:
	var state: Dictionary = _native.get_forklift_state()
	var active := int(state["index"])
	_forklift_view = _forklift_views[active]
	for index in _forklift_views.size():
		_forklift_views[index].update_view(state if index == active else _native.get_forklift_state_at(index))
	if delta <= 0.0:
		return
	var body: Transform3D = _forklift_view.body_transform()
	var driving := bool(state["driving"])
	var lever := float(state["lift"])
	var lift_n := float(state["lift_n"])
	var relief_n := float(state["lift_max_n"])
	var strain := clampf(lift_n / relief_n, 0.0, 1.0) if lever > 0.05 else 0.0
	_audio.update_forklift(delta, state, body, strain)
	var carriage := body * Vector3(0.0, 0.56 + float(state["lift_m"]), 1.1)
	var rear := body * Vector3(0.0, 0.3, -1.0)
	# The check valve shutting on a load: the whole truck takes it.
	if absf(_fork_lever) > 0.05 and absf(lever) <= 0.05 and maxf(_fork_last_n, lift_n) > 0.15 * relief_n:
		var weight := clampf(maxf(_fork_last_n, lift_n) / relief_n, 0.3, 1.0)
		_audio.forklift_cue(&"clunk", carriage, weight)
		if driving:
			_haptic(&"press", weight)
	_fork_lever = lever
	_fork_last_n = lift_n
	# The rear wheels: off the ground, and back down onto it.
	var contacts: Array = state["wheel_contact"]
	var loads: PackedFloat32Array = state["wheel_load_n"]
	if bool(contacts[2]) or bool(contacts[3]):
		if _fork_rear_off >= FORK_REAR_OFF_SECONDS:
			var slam := clampf((loads[2] + loads[3]) / 60000.0, 0.3, 1.0)
			_audio.forklift_cue(&"slam", rear, slam)
			if driving:
				_haptic(&"land", slam)
		_fork_rear_off = 0.0
	else:
		var was := _fork_rear_off
		_fork_rear_off += delta
		if was < FORK_REAR_OFF_SECONDS and _fork_rear_off >= FORK_REAR_OFF_SECONDS:
			_audio.forklift_cue(&"unload", rear, 1.0)
	# Straining: the mast groans, and the seat shakes harder and faster.
	if strain > 0.6:
		_fork_groan_clock -= delta
		if _fork_groan_clock <= 0.0:
			_audio.forklift_cue(&"groan", body * Vector3(0.0, 2.4, 0.97), strain)
			_fork_groan_clock = lerpf(1.4, 0.5, strain)
	else:
		_fork_groan_clock = 0.0
	if driving and strain > 0.25:
		_fork_strain_clock -= delta
		if _fork_strain_clock <= 0.0:
			_haptic(&"strain", strain)
			_fork_strain_clock = lerpf(0.32, 0.09, strain)
	else:
		_fork_strain_clock = 0.0

# The slingshot as the native has it now; the predicted shot is re-asked a
# few times a second while a rider sits drawn, not every frame.
func _render_slingshot(delta: float) -> void:
	if _slingshot_view == null:
		return
	var sling: Dictionary = _native.get_slingshot_state()
	_sling_preview_clock -= delta
	if bool(sling["seated"]) and not bool(sling["released"]):
		if _sling_preview_clock <= 0.0:
			_sling_preview_clock = 0.2
			_sling_prediction = _native.get_slingshot_prediction()
	elif not _sling_prediction.is_empty():
		_sling_prediction = PackedVector3Array()
	_slingshot_view.update_view(sling, _sling_prediction)

# Her hands on the corner pipe come through the steel to the hand on the
# phone: a pulse for each hold within HER_FELT_M of the climber, fading with
# the distance; a heavy one when she slams onto a flange.
const HER_FELT_M := 14.0
var _her_holds_seen := -1
var _her_slams_seen := -1

func _feel_her(her: Dictionary, position: Vector3) -> void:
	var holds := int(her.get("pipe_holds", 0))
	var slams := int(her.get("pipe_slams", 0))
	var near := (her["position"] as Vector3).distance_to(position)
	if _her_holds_seen >= 0 and holds > _her_holds_seen and near < HER_FELT_M:
		_haptic(&"her", lerpf(1.0, 0.25, near / HER_FELT_M))
	if _her_slams_seen >= 0 and slams > _her_slams_seen and near < 2.0 * HER_FELT_M:
		_haptic(&"land", 1.0)
	_her_holds_seen = holds
	_her_slams_seen = slams

# On the bay's leaf while only her palms hold it up (src/sim/bay.hpp): her
# strain comes up through the steel under the feet, a pulse every
# BAY_FELT_S, stronger the nearer the load is to all she can give.
const BAY_FELT_S := 0.6
var _bay_felt_s := 0.0

func _feel_bay() -> void:
	_bay_felt_s = maxf(0.0, _bay_felt_s - get_process_delta_time())
	if _bay.is_empty() or not bool(_bay["hands"]) or int(_native.get_support_entity_id()) != BAY_LEAF_ENTITY:
		return
	if _bay_felt_s <= 0.0:
		_bay_felt_s = BAY_FELT_S
		_haptic(&"her", clampf(float(_bay["support_n"]) / BAY_HOLD_FULL_N, 0.3, 1.0))

func _write_telemetry(position: Vector3, velocity: Vector3, grounded: bool) -> void:
	var support := int(_native.get_support_entity_id())
	var support_velocity: Vector3 = _native.get_support_point_linear_velocity()
	var traversal := int(_native.get_traversal_state())

	_position_value.text = "POSITION  %8.2f %7.2f %8.2f m" % [position.x, position.y, position.z]
	_velocity_value.text = "VELOCITY  %8.2f %7.2f %8.2f m/s" % [velocity.x, velocity.y, velocity.z]
	_support_value.text = "SUPPORT   %s / E%04d / POINT V %5.2f %5.2f %5.2f" % [
		"GROUNDED" if grounded else "AIRBORNE", support,
		support_velocity.x, support_velocity.y, support_velocity.z]
	_support_value.modulate = Color("d9c08a") if grounded else Color("b4742c")
	_traversal_value.text = "TRAVERSAL %s / E%04d / %3d%%   LEDGE %s" % [
		_traversal_name(traversal),
		int(_native.get_traversal_support_entity_id()),
		int(round(float(_native.get_traversal_progress()) * 100.0)),
		_ledge_affordance_text()]

	_tick_value.text = "90 HZ NATIVE  /  TICK %08d  /  TOWER %.0f m" % [
		int(_native.get_tick_index()), float(_native.get_tower_height_meters())]

	var fall_state := int(_native.get_fall_state())
	_fall_value.text = "FALL      %s  PEAK %5.1f m/s  CHUTE %s  CHECKPOINT %6.2f %5.2f %6.2f  COMMITS %d  DEATHS %d" % [
		_fall_state_name(fall_state),
		float(_native.get_fall_peak_speed_mps()),
		"DEPLOYED" if bool(_native.is_parachute_deployed()) else "stowed",
		_native.get_checkpoint_position().x, _native.get_checkpoint_position().y,
		_native.get_checkpoint_position().z,
		int(_native.get_checkpoint_commit_count()), int(_native.get_death_count())]
	_fall_value.modulate = Color("8fd9b8") if fall_state == FALL_PARACHUTING else Color("d8e0dc")

	if traversal == TRAVERSAL_HANGING:
		_status.text = "HANGING ON NATIVE LEDGE"
	elif traversal == TRAVERSAL_MANTLING:
		_status.text = "MANTLING REAL GEOMETRY"
	elif traversal == TRAVERSAL_VAULTING:
		_status.text = "VAULTING REAL GEOMETRY"
	elif traversal == TRAVERSAL_CLIMBING:
		_status.text = "CLIMBING"
	elif traversal == TRAVERSAL_SWINGING:
		_status.text = "SWINGING"
	elif bool(_native.is_player_sliding()):
		_status.text = "SLIDING"
	elif bool(_native.is_player_wall_climbing()):
		_status.text = "WALL CLIMB"
	elif bool(_native.is_player_wall_running()):
		_status.text = "WALL RUN"
	elif grounded and _is_kit(support) and support >= KIT_DYNAMIC_ENTITY_MIN and support < STOCK_FIRST_ENTITY:
		_status.text = "RIDING A MACHINE"
	elif grounded:
		_status.text = "AT GRADE"
	elif fall_state == FALL_PARACHUTING:
		_status.text = "PARACHUTE DEPLOYED"
	else:
		_status.text = "AIRBORNE / MOMENTUM PRESERVED"

func _fall_state_name(fall_state: int) -> String:
	match fall_state:
		FALL_PARACHUTING:
			return "PARACHUTING"
		FALL_AIRBORNE:
			return "AIRBORNE   "
		_:
			return "GROUNDED   "

func _traversal_name(traversal: int) -> String:
	match traversal:
		TRAVERSAL_HANGING:
			return "HANG   "
		TRAVERSAL_MANTLING:
			return "MANTLE "
		TRAVERSAL_VAULTING:
			return "VAULT  "
		TRAVERSAL_SWINGING:
			return "SWING  "
		_:
			return "NONE   "

func _ledge_affordance_text() -> String:
	if not bool(_native.is_ledge_available()):
		return "  --"
	return "E%04d +%4.2fm" % [int(_native.get_ledge_entity_id()), float(_native.get_ledge_rise_meters())]


# The chiller drop's car in the hands (AS-007 E): while the feet are on its
# deck or its cab, a heave each time it surges up (the launch, each bob at
# the top: its acceleration past WET_E_HEAVE_MPS2) and a rumble every
# WET_E_RUMBLE_S while it runs faster than WET_E_RUMBLE_MPS. From the
# native's own car speed; gated by the vibration setting like every pulse.
const WET_E_HEAVE_MPS2 := 5.0
const WET_E_RUMBLE_MPS := 1.0
const WET_E_RUMBLE_S := 0.25
var _fb_wet_e_speed := 0.0
var _fb_wet_e_heaving := false
var _fb_wet_e_rumble_s := 0.0

func _feel_wet_e(delta: float) -> void:
	if _wet_e.is_empty():
		_fb_wet_e_heaving = false
		return
	var speed := float(_wet_e.get("e_cab_speed", 0.0))
	var accel := (speed - _fb_wet_e_speed) / delta
	_fb_wet_e_speed = speed
	var support := int(_native.get_support_entity_id())
	if support != WET_E_DECK_ENTITY and support != WET_E_CAB_ENTITY:
		_fb_wet_e_heaving = false
		_fb_wet_e_rumble_s = 0.0
		return
	var heaving := accel > WET_E_HEAVE_MPS2
	if heaving and not _fb_wet_e_heaving:
		_haptic(&"heave", clampf(accel / 12.0, 0.5, 1.0))
	_fb_wet_e_heaving = heaving
	if absf(speed) > WET_E_RUMBLE_MPS:
		_fb_wet_e_rumble_s -= delta
		if _fb_wet_e_rumble_s <= 0.0:
			_fb_wet_e_rumble_s = WET_E_RUMBLE_S
			_haptic(&"rumble", clampf(absf(speed) / 3.5, 0.4, 1.0))
	else:
		_fb_wet_e_rumble_s = 0.0
