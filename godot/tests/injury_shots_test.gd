extends SceneTree
## J-044i first slice: the injury render on her body, driven by the per-region
## injury dictionary J-044g will fill (J-044a §D/E: region -> {j_region, j_event}).
## Bruise, swelling and red mark only; cuts and blood need J-044d's surface
## feature and come in the next slice. Thresholds are the J-044a calibration:
## red mark J_event >= 25 N·s, bruise J_region >= 100, swelling J_region >= 200.
## Injuries never heal within a run (Cory's no-heal ruling): a smaller later
## value never lowers a region. With --out=<dir> saves each moment as a PNG.
##   xvfb-run -a godot --path godot --fixed-fps 30 --resolution 960x720 \
##       --script res://tests/injury_shots_test.gd -- --out=/workspace/injury_shots

const MatronView := preload("res://presentation/life/matron_view.gd")
const DT := 1.0 / 30.0

var _out := ""
var _cam: Camera3D
var _her: Node3D
var _fails := 0
var _pose := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 1, "sees_player": true,
	"alive": true, "released": true, "position": Vector3(0, MatronView.STAND_CENTRE, 0), "yaw": 0.0,
	"render_yaw": 0.0, "exposure": 0.6, "floor_y": 0.0, "injury": {}}


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--out="):
			_out = arg.substr(6)
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("INJURY %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_fails += 1


func _studio() -> void:
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.05, 0.055, 0.06)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.6, 0.62, 0.66)
	env.environment.ambient_light_energy = 0.25
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	root.add_child(env)
	var key := DirectionalLight3D.new()
	key.light_energy = 1.1
	key.light_color = Color(1.0, 0.93, 0.85)
	key.shadow_enabled = true
	root.add_child(key)
	key.look_at_from_position(Vector3(-2.0, 4.0, 3.0), Vector3(0, 0.3, 0))
	var fill := OmniLight3D.new()
	fill.light_energy = 1.4
	fill.omni_range = 6.0
	fill.position = Vector3(1.5, 1.6, 1.8)
	root.add_child(fill)
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true


func _step(frames: int) -> void:
	for i in frames:
		if _her != null:
			_her._clock += DT
			_her._state_age += DT
			_her._update_figure(_pose, DT, _cam.global_position, _cam.global_position)
		await process_frame


func _shot(name: String, at: Vector3, look: Vector3, fov: float = 50.0) -> void:
	_cam.fov = fov
	_cam.look_at_from_position(at, look)
	if _out == "":
		await process_frame
		return
	await process_frame
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(_out.path_join(name + ".png"))
	print("INJURY SHOT ", _out.path_join(name + ".png"))


func _layer() -> Variant:
	return _her.get("_injury") if _her != null else null


func _set_injury(injury: Dictionary) -> void:
	_pose["injury"] = injury


func _run() -> void:
	_studio()
	_her = MatronView.new()
	root.add_child(_her)
	_her._build_figure()
	_her._build_sparks()
	await _step(2)
	# Miss: nothing recorded, nothing shows.
	_set_injury({})
	await _step(2)
	var layer: Variant = _layer()
	_check(layer != null, "layer", "injury layer exists on the view")
	if layer == null:
		print("INJURY FAIL (1 failed)")
		quit(1)
		return
	_check(layer.visible_regions().is_empty(), "miss", "regions %s" % [layer.visible_regions()])
	# Glance: 20 N·s event, below the 25 N·s red-mark line: nothing.
	_set_injury({"face_l": {"j_region": 30.0, "j_event": 20.0}})
	await _step(2)
	_check(layer.visible_regions().is_empty(), "glance", "regions %s" % [layer.visible_regions()])
	# Light tap: 30 N·s event marks, 30 N·s in the region bruises nothing.
	_set_injury({"face_l": {"j_region": 30.0, "j_event": 30.0}})
	await _step(2)
	_check(layer.mark_visible("face_l") and not layer.bruise_visible("face_l") and not layer.swelling_visible("face_l"),
		"tap", "mark %s bruise %s swell %s" % [layer.mark_visible("face_l"), layer.bruise_visible("face_l"), layer.swelling_visible("face_l")])
	# Solid right punch on the left cheek: 150 N·s in the region bruises it, and only it.
	_set_injury({"face_l": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_l") and not layer.bruise_visible("face_r") and layer.patch_visible("face_l"), "left_cheek",
		"l %s r %s patch %s" % [layer.bruise_visible("face_l"), layer.bruise_visible("face_r"), layer.patch_visible("face_l")])
	# Heavy: 250 N·s swells the same cheek.
	_set_injury({"face_l": {"j_region": 250.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_l") and layer.swelling_visible("face_l"), "swell",
		"bruise %s swell %s" % [layer.bruise_visible("face_l"), layer.swelling_visible("face_l")])
	# Right cheek, same dose: bruises on the right, the left keeps its swelling.
	_set_injury({"face_l": {"j_region": 250.0, "j_event": 60.0}, "face_r": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_r") and layer.swelling_visible("face_l"), "right_cheek",
		"r %s l_swell %s" % [layer.bruise_visible("face_r"), layer.swelling_visible("face_l")])
	# No healing: a smaller later value never lowers a region.
	_set_injury({"face_l": {"j_region": 30.0, "j_event": 30.0}, "face_r": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_l") and layer.swelling_visible("face_l"), "no_heal",
		"sev %.0f" % layer.severity("face_l"))
	# Skull: 150 N·s bruises the back of the head, and the patch must face
	# OUT of the skull (the old quad faced into it and culling hid it).
	_set_injury({"skull": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	var head_bone: Transform3D = _her._rig.bone_world("head")
	var skull_out: float = layer.patch_outward("skull", head_bone.origin)
	var face_out: float = layer.patch_outward("face_l", head_bone.origin)
	_check(layer.bruise_visible("skull") and layer.patch_visible("skull") and skull_out > 0.0 and face_out > 0.0,
		"skull", "bruise %s patch %s outward %.3f face %.3f" % [layer.bruise_visible("skull"), layer.patch_visible("skull"), skull_out, face_out])
	# Stills: the bruised left cheek, then the face straight on.
	var head: Vector3 = _her.head_world_position()
	await _shot("injury_left_cheek", head + Vector3(-0.45, 0.08, 0.55), head, 40.0)
	await _shot("injury_face_front", head + Vector3(0.0, 0.05, 0.7), head, 45.0)
	await _shot("injury_skull_back", head + Vector3(0.0, 0.15, -0.7), head, 45.0)
	print("INJURY %s (%d failed)" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(1 if _fails > 0 else 0)
