extends SceneTree
## J-021 job 4: the far world (environment job 4, lane C).
## The basin reads as a real valley out to a real horizon — all of it
## presentation only: the cones stay the collision, the valley floor's
## boxes stay solids (hidden, not removed).
## The job-11 water checks (water_material, the stream/pool in the ground
## shader, the waterfall and yard water through shaders) live on the
## job-11 branch, which stacks on this one; the full test is restored there.
##
##   tools/godot/godot --headless --path godot --script res://tests/far_world_test.gd
var _fails := 0


func _check(cond: bool, label: String) -> void:
	if cond:
		return
	_fails += 1
	print("FAR_WORLD FAIL ", label)


func _param_names(shader: Shader) -> Array:
	if shader == null:
		return []
	var names: Array = []
	for entry in RenderingServer.get_shader_parameter_list(shader.get_rid()):
		names.append(String(entry["name"]))
	return names


func _initialize() -> void:
	_run.call_deferred()


func _run() -> void:
	# 1. The far world script exists and builds the valley floor's drawn top.
	var far_script: Script = load("res://presentation/far_world.gd")
	_check(far_script != null, "far_world.gd loads")
	var far = null
	var ground: ShaderMaterial = null
	if far_script != null:
		far = far_script.new()
		var stub := Node3D.new()
		far.build(stub, [[Vector3(5200.0, 1.0, 2400.0), Vector3(0.0, -0.8, -1500.0)]], -0.3)
		var floor_mesh := far.get_node_or_null("FarGround") as MeshInstance3D
		_check(floor_mesh != null, "FarWorld builds a FarGround mesh")
		if floor_mesh != null:
			_check((floor_mesh.mesh as ArrayMesh) != null and (floor_mesh.mesh as ArrayMesh).surface_get_arrays(0).size() > 0, "FarGround has vertices")
			ground = floor_mesh.material_override as ShaderMaterial
			_check(ground != null, "FarGround wears a ShaderMaterial")
		if ground != null:
			var names := _param_names(ground.shader)
			_check("tower_low" in names, "ground shader has tower_low")
			_check("tower_high" in names, "ground shader has tower_high")
		# _process is not called here: the node is not in the tree, so
		# get_viewport() is null. The integration check below calls it on
		# the FarWorld that _build_basin added to the tree.
		if ground != null:
			var names2 := _param_names(ground.shader)
			_check("detail" in names2, "ground shader has detail")

	# 2. The horizon past the far plane lives in the sky shader (job 4):
	# far_world.gd feeds it the eye's height every frame.
	var sky: Shader = load("res://presentation/sky.gdshader")
	_check(sky != null, "sky.gdshader loads")
	if sky != null:
		var names := _param_names(sky)
		if names.is_empty():
			var text := FileAccess.get_file_as_string("res://presentation/sky.gdshader")
			_check(text.contains("sky_horizon.gdshaderinc"), "sky includes the horizon band")
			_check(text.contains("far_horizon("), "sky calls far_horizon")
		else:
			_check("eye_height" in names, "sky has eye_height")
			_check("horizon_detail" in names, "sky has horizon_detail")

	# 3. The rim is not look-alike cones: 12 variants, and two peaks of one
	# variant never show the same face — the turn depends on where it stands.
	var mountain: Script = load("res://presentation/mountain_mesh.gd")
	_check(mountain != null, "mountain_mesh.gd loads")
	if mountain != null:
		_check(int(mountain.VARIANTS) == 12, "mountain VARIANTS is 12")
		var parent := Node3D.new()
		mountain.draw(parent, [
			{"foot": Vector3(1000.0, 0.0, -100.0), "radius": 300.0, "height": 400.0,
				"variant": 3, "rock": Color("5d636a"), "snowline": 0.5},
			{"foot": Vector3(-800.0, 0.0, 300.0), "radius": 300.0, "height": 400.0,
				"variant": 3, "rock": Color("5d636a"), "snowline": 0.5},
		])
		_check(parent.get_child_count() == 1, "one variant draws one MultiMesh")
		var drawn := parent.get_child(0) as MultiMeshInstance3D
		_check(drawn != null and drawn.name == "Peaks3", "same-variant peaks share Peaks3")
		if drawn != null and drawn.multimesh != null:
			_check(drawn.multimesh.instance_count == 2, "both peaks ride one MultiMesh")
		# Headless Godot's dummy renderer does not store MultiMesh instance
		# data (get_buffer() is empty there), so the turn itself cannot be
		# read back in this test — it was verified graphically instead (two
		# peaks of variant 3 at different feet turn pi apart, x-axis distance
		# 800). This pins the wiring: the turn is the variant's turn plus a
		# term from where the peak stands.
		var mountain_text := FileAccess.get_file_as_string("res://presentation/mountain_mesh.gd")
		_check(mountain_text.contains("fposmod(foot.x"), "turn has a term from the foot's x")
		_check(mountain_text.contains("foot.z * 0.0291"), "turn has a term from the foot's z")

	# 4. The integration: the world builds the far world and hides the floor's
	# solids (they stay solids). The yard water and waterfall run through
	# shaders on the job-11 branch; here they are the flat tar/quad they
	# were, and nothing asserts on them.
	var world_script: Script = load("res://presentation/main_world.gd")
	_check(world_script != null, "main_world.gd loads")
	if world_script != null:
		var main = world_script.new()
		var tower := Node3D.new()
		tower.name = "TowerPresentation"
		main.add_child(tower)
		# In the tree, so global_transform works for the solids export and
		# FarWorld's _process runs as it does in the game. Neither
		# main_world.gd nor main_geometry.gd defines _ready, so adding the
		# instance builds nothing on its own.
		root.add_child(main)
		main._build_mountains_and_waterfall()
		main._build_yard(StandardMaterial3D.new(), StandardMaterial3D.new(), StandardMaterial3D.new())
		var far_node = main.get_node_or_null("FarWorld")
		_check(far_node != null, "_build_basin adds a FarWorld node")
		var floors: Array = []
		for child in tower.get_children():
			# Node names are not unique among siblings (Godot renames the
			# later ValleyFloor boxes); the part meta is the identity.
			var part := str(child.get_meta("part", ""))
			if part == "ValleyFloor":
				floors.append(child)
		_check(floors.size() == 4, "four ValleyFloor boxes stand")
		for floor_box in floors:
			_check(not (floor_box as MeshInstance3D).visible, "ValleyFloor boxes are hidden, not removed")
		# Collision is unchanged: the hidden floor boxes still export as
		# solids.
		var solid_script: Script = load("res://presentation/solid_export.gd")
		if solid_script != null:
			var collected: Dictionary = solid_script.collect(main)
			var valley := 0
			for box in (collected["boxes"] as Array):
				if String((box as Dictionary).get("part", "")) == "ValleyFloor":
					valley += 1
			_check(valley == 4, "solids table still holds four ValleyFloor boxes")
		# The tower's long shadow has its boxes: the known column and twin
		# are the first two slices, 16 slices in all.
		if far_node != null and far_script != null:
			var ok: bool = far_node._read_tower()
			_check(ok, "FarWorld reads the tower's boxes")
			if ok:
				var ground_mat := (far_node.get_node("FarGround") as MeshInstance3D).material_override as ShaderMaterial
				var lows = ground_mat.get_shader_parameter("tower_low")
				var highs = ground_mat.get_shader_parameter("tower_high")
				_check(lows != null and (lows as PackedVector3Array).size() == 16, "tower_low holds 16 slices")
				_check(highs != null and (highs as PackedVector3Array).size() == 16, "tower_high holds 16 slices")
				if lows != null and (lows as PackedVector3Array).size() == 16:
					_check((lows as PackedVector3Array)[0].is_equal_approx(far_script.COLUMN_LOW), "first slice is the tower column")
					_check((highs as PackedVector3Array)[0].is_equal_approx(far_script.COLUMN_HIGH), "first slice tops at the summit deck")
			# One frame of its process, in the tree: no crash on the real
			# main, detail on at full quality.
			far_node._process(0.016)
			var detail = (far_node.get_node("FarGround") as MeshInstance3D).material_override.get_shader_parameter("detail")
			_check(detail != null and float(detail) == 1.0, "ground detail is on at full quality")

	if _fails == 0:
		print("FAR_WORLD PASS (0 failed)")
	else:
		print("FAR_WORLD FAIL (%d failed)" % _fails)
	quit(1 if _fails > 0 else 0)
