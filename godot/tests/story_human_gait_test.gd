extends SceneTree
const Gait := preload("res://presentation/story_human_gait.gd")
var checks := 0
var failures := 0

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("STORY_HUMAN_GAIT " + why)

func poses(skeleton: Skeleton3D) -> Array[Transform3D]:
	var result: Array[Transform3D] = []
	for i in skeleton.get_bone_count():
		result.append(skeleton.get_bone_pose(i))
	return result

func same(a: Array[Transform3D], b: Array[Transform3D]) -> bool:
	if a.size() != b.size():
		return false
	for i in a.size():
		if not a[i].is_equal_approx(b[i]):
			return false
	return true

func _run() -> void:
	# Read the actual shipped GLB, including its unusual bone axes and split
	# segments. A fabricated upright test rig would miss imported-rig defects.
	var document := GLTFDocument.new()
	var state := GLTFState.new()
	check(document.append_from_file("res://assets/characters/ro_human.glb", state) == OK, "actual human GLB loads")
	var model := document.generate_scene(state) as Node3D
	if model == null:
		check(false, "actual human scene generated")
		quit(1)
		return
	root.add_child(model)
	var skeletons := model.find_children("*", "Skeleton3D", true, false)
	check(skeletons.size() == 1, "actual human has one skeleton")
	if skeletons.size() != 1:
		model.free()
		quit(1)
		return
	var skeleton: Skeleton3D = skeletons[0]
	var gait := Gait.new()
	check(gait.configure(skeleton), "actual MPFB leg chain accepted: " + gait.last_error)
	var rest := poses(skeleton)
	var original_transform := model.transform
	gait.apply(Vector3.ZERO, Basis.IDENTITY, true, 100, 1)
	check(same(rest, poses(skeleton)), "initial native sample has no invented step")
	for tick in range(101, 121):
		gait.apply(Vector3.ZERO, Basis.IDENTITY, true, tick, 1)
	check(same(rest, poses(skeleton)), "time alone cannot produce a walk")
	var spine := skeleton.find_bone("spine02")
	var sentinel := skeleton.get_bone_pose(spine)
	sentinel.basis *= Basis(Vector3.RIGHT, .03)
	skeleton.set_bone_pose(spine, sentinel)
	var left := skeleton.find_bone("foot.L")
	var right := skeleton.find_bone("foot.R")
	var owned: Array[int] = []
	for side in ["L", "R"]:
		for stem in ["upperleg01.", "upperleg02.", "lowerleg01.", "lowerleg02.", "foot."]:
			owned.append(skeleton.find_bone(stem + side))
	var max_lift := 0.0
	var movement_poses: Array[Transform3D] = []
	var planted_before := Vector3.ZERO
	var planted_after := Vector3.ZERO
	for tick in range(121, 361):
		var travel := float(tick - 120) * .001
		gait.apply(Vector3(0, 0, travel), Basis.IDENTITY, true, tick, 1)
		var left_pose := skeleton.get_bone_global_pose(left)
		var right_pose := skeleton.get_bone_global_pose(right)
		max_lift = maxf(max_lift, maxf(left_pose.origin.y - skeleton.get_bone_global_rest(left).origin.y, right_pose.origin.y - skeleton.get_bone_global_rest(right).origin.y))
		if tick == 160:
			planted_before = left_pose.origin + Vector3(0, 0, travel)
		if tick == 170:
			planted_after = left_pose.origin + Vector3(0, 0, travel)
		if tick == 200:
			movement_poses = poses(skeleton)
		for bone in owned:
			check(skeleton.get_bone_pose(bone).is_finite(), "finite leg pose at tick %d bone %d" % [tick, bone])
	check(not same(rest, movement_poses), "measured slow movement bends actual legs")
	check(planted_before.distance_to(planted_after) < .0005, "stance foot stays in world within half a millimeter: %f" % planted_before.distance_to(planted_after))
	check(max_lift > .025 and max_lift < .045, "swing foot clears floor by bounded centimeters: %f" % max_lift)
	check(skeleton.get_bone_pose(spine).is_equal_approx(sentinel), "gait preserves the separate breathing/head owner")
	check(model.transform == original_transform, "no actor/model root motion")
	var before_repeat := poses(skeleton)
	gait.apply(Vector3(0, 0, .24), Basis.IDENTITY, true, 360, 1)
	check(same(before_repeat, poses(skeleton)), "duplicate native tick does not advance or settle")
	for tick in range(361, 380):
		gait.apply(Vector3(0, 0, .24), Basis.IDENTITY, true, tick, 1)
	for bone in owned:
		check(skeleton.get_bone_pose(bone).is_equal_approx(rest[bone]), "stopped leg returns to original rest")
	gait.apply(Vector3(0, 0, .241), Basis.IDENTITY, true, 380, 1)
	gait.apply(Vector3(0, 0, .242), Basis.IDENTITY, false, 381, 1)
	for bone in owned:
		check(skeleton.get_bone_pose(bone).is_equal_approx(rest[bone]), "unsupported actor has no gait")
	gait.apply(Vector3(0, 0, 20), Basis.IDENTITY, true, 382, 2)
	for bone in owned:
		check(skeleton.get_bone_pose(bone).is_equal_approx(rest[bone]), "new episode resets instead of walking teleport")
	# Replay identical input and compare the full actual skeleton pose.
	gait.apply(Vector3.ZERO, Basis.IDENTITY, true, 120, 3)
	for tick in range(121, 201):
		gait.apply(Vector3(0, 0, float(tick - 120) * .001), Basis.IDENTITY, true, tick, 3)
	check(same(movement_poses, poses(skeleton)), "same native input gives the same presentation pose")
	var turned := Basis(Vector3.UP, PI * .5)
	gait.apply(Vector3.ZERO, turned, true, 120, 4)
	for tick in range(121, 201):
		gait.apply(turned * Vector3(0, 0, float(tick - 120) * .001), turned, true, tick, 4)
	check(same(movement_poses, poses(skeleton)), "actor yaw converts actual world travel into the same local forward step")
	gait.apply(Vector3(100, 0, 100), Basis.IDENTITY, true, 201, 4)
	for bone in owned:
		check(skeleton.get_bone_pose(bone).is_equal_approx(rest[bone]), "same-episode teleport resets leg pose")
	var valid_pose := poses(skeleton)
	gait.apply(Vector3(NAN, 0, 0), Basis.IDENTITY, true, 202, 4)
	check(same(valid_pose, poses(skeleton)) and not gait.last_error.is_empty(), "nonfinite boundary input rejected without corrupting pose")
	var invalid := Gait.new()
	check(not invalid.configure(null) and not invalid.last_error.is_empty(), "missing skeleton rejected clearly")
	model.free()
	print("STORY_HUMAN_GAIT %s checks=%d failures=%d stance_drift_m=%.8f max_lift_m=%.5f" % ["PASS" if failures == 0 else "FAIL", checks, failures, planted_before.distance_to(planted_after), max_lift])
	quit(0 if failures == 0 else 1)
