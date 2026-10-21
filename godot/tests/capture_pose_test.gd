extends SceneTree
const CapturePose := preload("res://presentation/capture_pose.gd")

func _initialize() -> void:
	var camera := Camera3D.new()
	root.add_child(camera)
	var last := Quaternion.IDENTITY
	var centre := Vector3(3, 10, -2)
	var eye := Vector3(0, 0.62, 0)
	for degree in range(361):
		var physical := Quaternion(Vector3.RIGHT, deg_to_rad(float(degree)))
		camera.quaternion = Quaternion.IDENTITY
		CapturePose.apply(camera, centre, eye, physical, true)
		if camera.position.distance_to(centre + physical * eye) > 0.00001 or \
			absf(camera.quaternion.dot(physical)) < 0.99999 or \
			absf(camera.quaternion.dot(last)) < 0.999:
			push_error("CAPTURE_POSE FAIL: physical inversion discontinuity")
			quit(1)
			return
		last = camera.quaternion
	# The real caller supplies its posture-correct eye, including crouch.
	camera.quaternion = Quaternion.IDENTITY
	CapturePose.apply(camera, centre, Vector3(0, 0.40, 0), Quaternion(Vector3.RIGHT, PI), true)
	if camera.position.distance_to(centre + Vector3(0, -0.40, 0)) > 0.00001:
		push_error("CAPTURE_POSE FAIL: crouched eye discontinuity")
		quit(1)
		return
	camera.position = Vector3(1, 2, 3)
	camera.quaternion = Quaternion(Vector3.UP, 0.5)
	CapturePose.apply(camera, centre, eye, Quaternion.IDENTITY, false)
	if camera.position != Vector3(1, 2, 3) or absf(camera.quaternion.dot(Quaternion(Vector3.UP, 0.5))) < 0.99999:
		push_error("CAPTURE_POSE FAIL: normal view changed")
		quit(1)
		return
	print("CAPTURE_POSE PASS: inversion, continuity, normal camera")
	quit(0)
