extends Node3D

# A display fastened to the existing1960 post, not a new contact or mission gate.
# Coordinates are in the static suspended-ladder frame's material/world space.
const Human := preload("res://presentation/story_human_view.gd")
const Style := preload("res://presentation/ui/ui_style.gd")
const SCREEN_POSITION := Vector3(12.2, 111.6, -183.17)
const SCREEN_SIZE := Vector2(.68, 1.02)
const PIXELS := Vector2i(640, 960)
const RECEIVER_ID := 1960
const FOCUS_TICKS := 54
const MAX_GAP := 9
var identity_store: RefCounted
var _last_tick := -1
var _focus := 0
var _target := &""
var _armed := true
var _first_play := true
var _identity_dirty := true
var _near := false
var _built := false
var _source_selected := false
var _display: SubViewport
var _portrait: SubViewport
var _human: Node3D
var _revision: Label
var _copy: Label
var _hint: Label
var _handover_tab: ColorRect
var _source_tab: ColorRect
var _progress: ColorRect
var _display_key := ""
var _portrait_key := ""
var _last_render_tick := -1
var _choice_available := false
var _choice_receipt := ""
var _send_choice: ColorRect
var _ari_choice: ColorRect

func _ready() -> void:
	position = SCREEN_POSITION
	var case_mesh := BoxMesh.new()
	case_mesh.size = Vector3(.74, 1.1, .10)
	var case_material := StandardMaterial3D.new()
	case_material.albedo_color = Color("35454a")
	case_material.metallic = .65
	case_material.roughness = .48
	case_mesh.material = case_material
	var cabinet := MeshInstance3D.new()
	cabinet.mesh = case_mesh
	cabinet.position.z = -.055
	cabinet.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(cabinet)
	# Cabinet and straps are visual: the existing post bears this static prop.
	for y in [-.40, .40]:
		var strap := MeshInstance3D.new()
		var mesh := BoxMesh.new()
		mesh.size = Vector3(.40, .035, .16)
		mesh.material = case_material
		strap.mesh = mesh
		strap.position = Vector3(0, y, -.13)
		strap.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(strap)
	visible = false

func invalidate_identity() -> void:
	_identity_dirty = true
	_portrait_key = ""
	_display_key = ""

func _label(parent: Node, text: String, at: Vector2, extent: Vector2, font_size: int, color: Color) -> Label:
	var label := Label.new()
	label.text = text
	label.position = at
	label.size = extent
	label.add_theme_font_override("font", Style.font_label())
	label.add_theme_font_size_override("font_size", font_size)
	label.add_theme_color_override("font_color", color)
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	parent.add_child(label)
	return label

func _build_display() -> void:
	if _built:
		return
	_built = true
	_display = SubViewport.new()
	_display.name = "HandoverDisplay"
	_display.size = PIXELS
	_display.disable_3d = true
	_display.render_target_update_mode = SubViewport.UPDATE_DISABLED
	add_child(_display)
	var background := ColorRect.new()
	background.size = Vector2(PIXELS)
	background.color = Color("101b20")
	background.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_display.add_child(background)
	_label(_display, "THE EXCHANGE  /  HANDOVER RECORD", Vector2(28, 17), Vector2(584, 37), 25, Color("a8bfbe"))
	_label(_display, "ROWAN ‘RO’ VALE", Vector2(28, 58), Vector2(584, 40), 31, Color("f0eadc"))
	_portrait = SubViewport.new()
	_portrait.name = "RecordedPortrait"
	_portrait.size = Vector2i(584, 340)
	_portrait.own_world_3d = true
	_portrait.render_target_update_mode = SubViewport.UPDATE_DISABLED
	# Child viewports render before their parent: the screen must receive this
	# frame's fitted/default portrait rather than retain the previous image.
	_display.add_child(_portrait)
	var world := WorldEnvironment.new()
	world.environment = Environment.new()
	world.environment.background_mode = Environment.BG_COLOR
	world.environment.background_color = Color("273438")
	world.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	world.environment.ambient_light_color = Color("e0e7e5")
	world.environment.ambient_light_energy = .65
	_portrait.add_child(world)
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-35, -25, 0)
	light.light_color = Color("f3e8d9")
	light.light_energy = 1.2
	light.shadow_enabled = false
	_portrait.add_child(light)
	_human = Human.new()
	_human.name = "ArchivedRo"
	_portrait.add_child(_human)
	var camera := Camera3D.new()
	_portrait.add_child(camera)
	camera.position = Vector3(0, 1.60, 1.25)
	camera.look_at(Vector3(0, 1.55, .05))
	camera.fov = 26
	camera.current = true
	var picture := TextureRect.new()
	picture.position = Vector2(28, 108)
	picture.size = Vector2(584, 340)
	picture.texture = _portrait.get_texture()
	picture.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_display.add_child(picture)
	_revision = _label(_display, "REV 04  /  OCCUPANCY ASSURANCE", Vector2(28, 465), Vector2(584, 42), 28, Color("e5b879"))
	_copy = _label(_display, "No occupied areas remain within the handover boundary.", Vector2(28, 516), Vector2(584, 84), 28, Color("f0eadc"))
	_send_choice = ColorRect.new()
	_send_choice.position = Vector2(28, 610)
	_send_choice.size = Vector2(280, 58)
	_send_choice.color = Color("253d44")
	_display.add_child(_send_choice)
	_label(_send_choice, "SEND TO DEV", Vector2(12, 12), Vector2(256, 40), 26, Color("f0eadc"))
	_ari_choice = ColorRect.new()
	_ari_choice.position = Vector2(332, 610)
	_ari_choice.size = Vector2(280, 58)
	_ari_choice.color = Color("253d44")
	_display.add_child(_ari_choice)
	_label(_ari_choice, "ARI FIRST", Vector2(12, 12), Vector2(256, 40), 26, Color("f0eadc"))
	_send_choice.hide()
	_ari_choice.hide()
	_handover_tab = ColorRect.new()
	_handover_tab.position = Vector2(28, 685)
	_handover_tab.size = Vector2(584, 92)
	_handover_tab.color = Color("253d44")
	_display.add_child(_handover_tab)
	_label(_handover_tab, "PLAY HANDOVER  /  REV 04", Vector2(18, 24), Vector2(548, 50), 28, Color("f0eadc"))
	_source_tab = ColorRect.new()
	_source_tab.position = Vector2(28, 794)
	_source_tab.size = Vector2(584, 92)
	_source_tab.color = Color("253d44")
	_display.add_child(_source_tab)
	_label(_source_tab, "ORIGINAL SURVEY  /  REV 03", Vector2(18, 24), Vector2(548, 50), 28, Color("f0eadc"))
	_hint = _label(_display, "LOOK AT A FILE TO PLAY", Vector2(28, 902), Vector2(584, 35), 23, Color("a8bfbe"))
	_progress = ColorRect.new()
	_progress.position = Vector2(28, 945)
	_progress.size = Vector2(0, 5)
	_progress.color = Color("83ceca")
	_display.add_child(_progress)
	var screen := MeshInstance3D.new()
	screen.name = "HandoverScreen"
	var quad := QuadMesh.new()
	quad.size = SCREEN_SIZE
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_texture = _display.get_texture()
	material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR
	quad.material = material
	screen.mesh = quad
	screen.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(screen)

func _disable_display() -> void:
	if _built:
		_display.render_target_update_mode = SubViewport.UPDATE_DISABLED
		_portrait.render_target_update_mode = SubViewport.UPDATE_DISABLED

func hide_record() -> void:
	visible = false
	_near = false
	_focus = 0
	_target = &""
	_armed = true
	_disable_display()

func _screen_pixel(camera: Transform3D, context: Dictionary) -> Vector2:
	var at: Variant = context.get("position")
	if not camera.is_finite() or not at is Vector3 or not at.is_finite():
		return Vector2(-1, -1)
	# This same clear passage validates playback and a live Action press.
	# A cached highlight can never submit from a fall or another support.
	var eligible: bool = bool(context.get("grounded", false)) \
		and bool(context.get("checkpoint_footing_valid", false)) \
		and int(context.get("support_entity", 0)) == RECEIVER_ID \
		and StringName(context.get("operating", &"")).is_empty() \
		and int(context.get("carrying", 0)) == 0 \
		and not bool(context.get("hanging", false)) and not bool(context.get("climbing", false)) \
		and at.x >= 11.85 and at.x <= 12.22 and at.z > -182.95 and at.z < -181.35 \
		and at.y > 110.65 and at.y < 111.15 \
		and camera.origin.distance_squared_to(at) < 4.0 \
		and camera.origin.x > 11.65 and camera.origin.x < 12.5 \
		and camera.origin.z > SCREEN_POSITION.z + .12
	if not eligible:
		return Vector2(-1, -1)
	var direction := -camera.basis.z.normalized()
	if direction.z >= -.1:
		return Vector2(-1, -1)
	var distance := (SCREEN_POSITION.z - camera.origin.z) / direction.z
	var hit := camera.origin + direction * distance - SCREEN_POSITION
	if distance <= 0 or distance >= 2.5 or absf(hit.x) >= SCREEN_SIZE.x * .5 or absf(hit.y) >= SCREEN_SIZE.y * .5:
		return Vector2(-1, -1)
	return Vector2((hit.x / SCREEN_SIZE.x + .5) * PIXELS.x, (.5 - hit.y / SCREEN_SIZE.y) * PIXELS.y)

func _target_at(camera: Transform3D, context: Dictionary) -> StringName:
	var pixels := _screen_pixel(camera, context)
	if pixels.x < 0:
		return &""
	if _choice_available and pixels.y >= 610 and pixels.y <= 668:
		if pixels.x >= 28 and pixels.x <= 308:
			return &"sent_to_dev"
		if pixels.x >= 332 and pixels.x <= 612:
			return &"ari_first"
	if _first_play or (pixels.y >= 685 and pixels.y <= 777):
		return &"handover"
	if pixels.y >= 794 and pixels.y <= 886:
		return &"source"
	return &""

func choice_offer() -> StringName:
	if _choice_available and _armed and _focus >= FOCUS_TICKS \
			and _target in [&"ari_first", &"sent_to_dev"]:
		return _target
	return &""

func confirm_choice(camera: Transform3D, context: Dictionary, tick: int) -> StringName:
	var offered := choice_offer()
	if offered.is_empty() or tick != _last_tick or _target_at(camera, context) != offered:
		return &""
	_armed = false
	return offered

func observe(camera: Transform3D, context: Dictionary, tick: int) -> StringName:
	var at: Variant = context.get("position")
	if tick < 0 or not camera.is_finite() or not at is Vector3 or not at.is_finite():
		hide_record()
		return &""
	if _last_tick >= 0 and tick < _last_tick:
		_first_play = true
		_source_selected = false
		_focus = 0
		_target = &""
		_armed = true
	var elapsed := tick - _last_tick if _last_tick >= 0 else 0
	_last_tick = tick
	visible = at.distance_squared_to(SCREEN_POSITION) < 30.0 * 30.0
	_near = visible and camera.origin.distance_squared_to(SCREEN_POSITION) < 6.0 * 6.0
	if _near:
		_build_display()
	else:
		_disable_display()
	var target := _target_at(camera, context)
	if target.is_empty() or elapsed < 0 or elapsed > MAX_GAP or target != _target:
		_focus = 0
		_armed = true
	_target = target
	if target.is_empty() or not _armed:
		return &""
	_focus += maxi(0, elapsed) if elapsed <= MAX_GAP else 0
	if _focus < FOCUS_TICKS:
		return &""
	if target == &"ari_first" or target == &"sent_to_dev":
		# Looking only offers the choice. Existing contextual Action commits it.
		return &""
	_armed = false
	_first_play = false
	_source_selected = target == &"source"
	return target

func present_record(state: Dictionary, voice: Node, tick: int) -> void:
	_choice_available = bool(state.get("handover_choice_available", false))
	_choice_receipt = String(state.get("handover_receipt", ""))
	if not _near or not _built:
		return
	if _identity_dirty and identity_store != null:
		_identity_dirty = false
		if not _human.apply_portrait(identity_store.photo_texture(), identity_store.face_landmarks()):
			push_warning("Cannot fit archived local work-ID portrait: " + _human.last_error)
	var active: StringName = state.get("transmission_key", &"") if not String(state.get("transmission", "")).is_empty() else &""
	if active == &"handover_record_assurance":
		_source_selected = false
	elif active == &"handover_source_survey" or active == &"handover_iona_revision":
		_source_selected = true
	var level: float = voice.speech_level_for(&"SOURCE" if _source_selected else &"RECORD")
	var seconds: float = voice.playback_seconds_for(&"SOURCE" if _source_selected else &"RECORD")
	var portrait_key := str([_source_selected, seconds, level])
	if portrait_key != _portrait_key:
		_portrait_key = portrait_key
		_human.present(Transform3D.IDENTITY, int(seconds * 90), level, Vector3(0, 1.6, 1.25))
		_portrait.render_target_update_mode = SubViewport.UPDATE_ONCE
	_revision.text = "REV 03  /  ACCESS SURVEY" if _source_selected else "REV 04  /  OCCUPANCY ASSURANCE"
	_copy.text = "Temporary construction access withdrawn.\nOccupancy not assessed." if _source_selected else "No occupied areas remain within the handover boundary."
	_send_choice.visible = _choice_available
	_ari_choice.visible = _choice_available
	_send_choice.color = Color("416269") if _target == &"sent_to_dev" else Color("253d44")
	_ari_choice.color = Color("416269") if _target == &"ari_first" else Color("253d44")
	_hint.text = _choice_receipt if not _choice_receipt.is_empty() else ("LOOK, THEN PRESS ACTION TO CHOOSE" if _choice_available else ("LOOK AT A FILE TO REPLAY / COMPARE" if not _first_play else "LOOK AT THE SCREEN TO PLAY"))
	_handover_tab.color = Color("416269") if _target == &"handover" else Color("253d44")
	_source_tab.color = Color("416269") if _target == &"source" else Color("253d44")
	_progress.size.x = 584 * minf(1, float(_focus) / FOCUS_TICKS) if _armed else 0
	var key := str([_identity_dirty, _source_selected, _target, _focus, active, seconds, _choice_available, _choice_receipt])
	if tick != _last_render_tick and (key != _display_key or _last_render_tick < 0):
		_last_render_tick = tick
		_display_key = key
		_display.render_target_update_mode = SubViewport.UPDATE_ONCE
