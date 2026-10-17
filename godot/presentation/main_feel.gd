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
const FOV_FALL_MAX_DEGREES := 3.0
const FOV_FALL_FULL_MPS := 14.0
const LANDING_DIP_DURATION_SECONDS := 0.22
const LANDING_DIP_MAX_METERS := 0.16
const LANDING_DIP_MIN_IMPACT_MPS := 2.0
const LANDING_DIP_FULL_IMPACT_MPS := 9.0
const HEAD_BOB_CYCLES_PER_METER := 0.72
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
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3
const TRAVERSAL_LOWERING := 5
const FALL_AIRBORNE := 1
const FALL_PARACHUTING := 2
var _summit_announced := false
var _ci_mode := false
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
# The ground slingshot: its view, the predicted shot (refreshed a few times a
# second while a rider sits drawn), and the aim the look is steering.
var _slingshot_view: Node3D
# The slingshot's launch shot: slow time and a look back, after the push.
var _launch_shot := LaunchShot.new()
var _swing_view: Node3D
var _summit_view: Node3D
var _matron_view: Node3D
var _forklift_view: Node3D
var _sling_prediction := PackedVector3Array()
var _sling_preview_clock := 0.0
# Pigeons on the structure near the player; gulls at height.
var _birds: Node3D
# The yard crew, drawn from the native's workers.
var _crew: Node3D
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
}

# Kinds that always get through the throttle: they mark a moment that
# matters more than whatever buzzed a few milliseconds before.
const HAPTIC_PRIORITY: Array[StringName] = [&"land", &"death", &"warn", &"slip"]
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
		_hud.toast("LETHAL IMPACT", "FELL AT %.1f M/S  /  %s  /  RESTORED TO CHECKPOINT %+.1f M" % [
			fall_speed_before, quip, restored.y], UiStyle.HAZARD, 3.8)
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
	_arms = FirstPersonArms.new()
	_arms.name = "FirstPersonArms"
	add_child(_arms)
	_arms.build(
		# Albedos sit dark on purpose: the yard's exposure lifts anything
		# this close to the lens toward pastel.
		_arm_material(Color("1f231c"), 0.93, _bump_concrete, 9.0),
		_arm_material(Color("b8611d"), 0.55, null, 1.0, 0.1),
		_arm_material(Color("3b2616"), 0.72, _bump_steel, 14.0),
		_arm_material(Color("22170e"), 0.82, _bump_steel, 14.0),
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
		}
	return {
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
		_cam_bob_phase += horizontal_speed * HEAD_BOB_CYCLES_PER_METER * TAU * delta
	if not _head_bob_on:
		_cam_bob_weight = 0.0
	else:
		_cam_bob_weight = lerpf(_cam_bob_weight, bob_target,
			1.0 - exp(-HEAD_BOB_RESPONSE_PER_SECOND * delta))
		if absf(_cam_bob_weight - bob_target) < 0.001:
			_cam_bob_weight = bob_target
	var vertical_bob := HEAD_BOB_VERTICAL_METERS * sin(_cam_bob_phase) * _cam_bob_weight
	var lateral_bob := HEAD_BOB_LATERAL_METERS * sin(_cam_bob_phase * 0.5) * _cam_bob_weight
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

	# Parkour movement dynamics and tactile camera feel:
	var traversal: int = int(_ctx.get("traversal", TRAVERSAL_NONE))
	var traversal_prog: float = float(_native.get_traversal_progress()) if _native else 0.0
	var parkour_fov := 0.0
	if traversal == TRAVERSAL_VAULTING:
		# Athletic speed vault: tuck dip and subtle roll bank along apex
		var vault_wave := sin(PI * clampf(traversal_prog, 0.0, 1.0))
		dip += -0.07 * vault_wave
		_cam_bank += 0.035 * vault_wave
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
		_cam_bank += 0.09
		parkour_fov = 3.0
	elif bool(_ctx.get("sliding", false)):
		# On the hip: the view leans back and a little to the trailing side,
		# and widens with the speed the slide carries.
		_cam_bank += 0.05
		parkour_fov = 4.0 * clampf(Vector2(velocity.x, velocity.z).length() / 8.0, 0.0, 1.0)
	elif bool(_ctx.get("balancing", false)) and grounded:
		# Narrow beam balance micro-sway: subtle tightrope dynamic centering
		var sway := sin(_cam_bob_phase * 0.75)
		lateral_bob += 0.025 * sway
		_cam_bank += 0.015 * sway

	# A hang torn off: the view drops and tips down as the hands go.
	var slip_pitch := 0.0
	if _slip_jolt > 0.0:
		_slip_jolt = maxf(0.0, _slip_jolt - delta)
		var slip_wave := sin(PI * (1.0 - _slip_jolt / SLIP_JOLT_SECONDS))
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
	_camera.rotation = Vector3(_pitch + _view_pitch_offset + roll_turn + slip_pitch * motion, _yaw,
		_cam_bank + stagger * motion)
	if _launch_shot.active() and not _reduced_motion:
		_camera.basis = _launch_shot.orient(_camera.basis, _camera.global_position)

	var fov_ground := FOV_SPRINT_MAX_DEGREES * smoothstep(0.0, 5.5, horizontal_speed) + parkour_fov
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
	_camera.fov = _fov_base + _cam_fov_offset

func _render_snapshot(delta: float = 0.0) -> void:
	var position: Vector3 = _native.get_player_position()
	var render_position: Vector3 = _native.get_player_render_position()
	var velocity: Vector3 = _native.get_player_linear_velocity()
	var grounded := bool(_native.is_player_grounded())
	var crouched := bool(_native.is_player_crouched())

	_apply_camera_feel(render_position, velocity, grounded, crouched, delta)
	if delta > 0.0:
		_audio.update(delta, position, velocity, grounded, int(_native.get_support_entity_id()),
			int(_native.get_traversal_state()), bool(_native.is_parachute_deployed()),
			int(_native.get_death_count()), crouched, _native.get_support_point_linear_velocity())
	# The developer telemetry overlay costs a dozen string formats a frame;
	# it is only paid for while the overlay is actually on screen.
	if _telemetry_on:
		_write_telemetry(position, velocity, grounded)
	_render_kit()
	_render_hook()
	if delta > 0.0 and _birds != null:
		_birds.update(delta, position, velocity, grounded)
	if _crew != null:
		_crew.update(delta, _camera.global_position, position)
	_render_slingshot(delta)
	if _swing_view != null:
		_swing_view.update_view(_native.get_swing_state())
	if _summit_view != null:
		_summit_view.update_view(_native.get_summit_state(), delta, float(_native.get_simulation_time_seconds()))
	if _matron_view != null:
		_matron_view.update_view(_native.get_matron_state(), delta, _camera.global_position, position)
	if _forklift_view != null:
		_forklift_view.update_view()
	_sample_machines(delta)

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
