extends Node3D

# A thin, tethered dispatch clipboard on the existing1440 gallery deck.
# It is presentation, not a new support/collider or a saved mission flag.
const CARD_POSITION := Vector3(-102.0, 5.922, -95.5)
const DISCOVERY := &"vale_emergency_contact"
const FOCUS_TICKS := 81 # A readable glance:0.9seconds at the native90Hz.
const MAX_OBSERVATION_GAP := 9
var _last_tick := -1
var _focus_ticks := 0
var _read := false
var _print: Label3D

func _ready() -> void:
	position = CARD_POSITION
	var board := MeshInstance3D.new()
	var box := BoxMesh.new()
	box.size = Vector3(.78, .018, .59)
	var material := StandardMaterial3D.new()
	material.albedo_color = Color("343d3d")
	material.roughness = .86
	box.material = material
	board.mesh = box
	board.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(board)
	_print = Label3D.new()
	_print.name = "RetainedContactCard"
	_print.text = "THE EXCHANGE / RECEIVING\nDISPATCH FILE: IONA VALE\n\nEMERGENCY CONTACT\nROWAN \"RO\" VALE\nNEXT OF KIN / RETAINED"
	_print.font = preload("res://presentation/ui/ui_style.gd").font_label()
	_print.font_size = 30
	_print.pixel_size = .0011
	_print.modulate = Color("e9e3ce")
	_print.outline_size = 0
	_print.position.y = .012
	_print.rotation.x = -PI * .5
	_print.no_depth_test = false
	_print.shaded = true
	add_child(_print)
	# Small spring clip, fastened against the deck. All decoration stays below
	# the player's contact-scale geometry and cannot grant footing.
	var clip := MeshInstance3D.new()
	var clip_mesh := BoxMesh.new()
	clip_mesh.size = Vector3(.18, .022, .045)
	var steel := StandardMaterial3D.new()
	steel.albedo_color = Color("798787")
	steel.metallic = .8
	steel.roughness = .5
	clip_mesh.material = steel
	clip.mesh = clip_mesh
	clip.position = Vector3(0, .012, -.273)
	clip.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(clip)
	visible = false

func observe(camera: Transform3D, context: Dictionary, tick: int) -> StringName:
	var at: Variant = context.get("position")
	if tick < 0 or not at is Vector3 or not at.is_finite() or not camera.is_finite():
		_focus_ticks = 0
		visible = false
		return &""
	if _last_tick >= 0 and tick < _last_tick:
		_read = false
		_focus_ticks = 0
	var elapsed := tick - _last_tick if _last_tick >= 0 else 0
	_last_tick = tick
	visible = at.distance_squared_to(CARD_POSITION) < 38.0 * 38.0
	var offset := CARD_POSITION - camera.origin
	# The accepted gallery's open deck is the reading area. Grounded alone,
	# an airborne pass, the ground below it or its back face cannot read it.
	var eligible: bool = bool(context.get("grounded", false)) \
		and bool(context.get("checkpoint_footing_valid", false)) \
		and int(context.get("support_entity", 0)) == 1440 \
		and at.x > -103.2 and at.x < -96.8 and at.z > -96.3 and at.z < -91.7 \
		and at.y > 6.6 and at.y < 7.2 and camera.origin.y > CARD_POSITION.y + .1 \
		and offset.length_squared() > .01 and offset.length_squared() < 2.75 * 2.75 \
		and (-camera.basis.z).dot(offset.normalized()) >= .9925
	if not eligible or elapsed < 0 or elapsed > MAX_OBSERVATION_GAP:
		_focus_ticks = 0
		return &""
	if _read:
		return &""
	_focus_ticks += elapsed
	if _focus_ticks < FOCUS_TICKS:
		return &""
	_read = true
	return DISCOVERY
