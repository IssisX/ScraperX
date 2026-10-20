extends SceneTree
const Store := preload("res://presentation/identity/player_identity_store.gd")
const Calibration := preload("res://presentation/identity/portrait_calibration.gd")
var failures := 0
var checks := 0
func _initialize() -> void:
	_run.call_deferred()
func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("PORTRAIT_CALIBRATION " + why)
func _run() -> void:
	if DirAccess.dir_exists_absolute(Store.DIRECTORY):
		push_error("Refusing to touch an existing player_identity directory")
		quit(2)
		return
	var owner := Store.new()
	var image := Image.create(100, 200, false, Image.FORMAT_RGBA8)
	image.fill(Color(0.3, 0.5, 0.6))
	var points := PackedVector2Array([Vector2(.32,.37),Vector2(.64,.39),Vector2(.49,.63)])
	check(owner.save_face_landmarks(points) == ERR_DOES_NOT_EXIST, "no image means no persisted alignment")
	check(owner.set_photo_from_image(image) == OK, "save bounded source portrait")
	check(owner.save_face_landmarks(points) == OK and owner.face_landmarks() == points, "alignment round trips against exact portrait")
	var invalid := points.duplicate()
	invalid[1] = invalid[0]
	check(owner.save_face_landmarks(invalid) == ERR_INVALID_DATA and owner.face_landmarks() == points, "invalid alignment preserves accepted one")
	image.fill(Color.RED)
	check(owner.set_photo_from_image(image) == OK and owner.face_landmarks() == PackedVector2Array(Store.DEFAULT_LANDMARKS), "replacement portrait rejects stale calibration")
	var canvas := Calibration.new()
	root.add_child(canvas)
	canvas.size = Vector2(600, 400)
	canvas.texture = ImageTexture.create_from_image(image)
	var results: Array = []
	canvas.aligned.connect(func(value: PackedVector2Array) -> void: results.append(value))
	canvas.start_alignment()
	canvas.select_point(Vector2(10,10))
	check(canvas.points.is_empty(), "letterbox padding cannot become a face point")
	var rect: Rect2 = canvas.image_rect()
	for point in points:
		canvas.select_point(rect.position + point * rect.size)
	check(results.size() == 1 and results[0][0].is_equal_approx(points[0]) and results[0][1].is_equal_approx(points[1]) and results[0][2].is_equal_approx(points[2]), "three image taps map through aspect-fit letterboxing")
	canvas.select_point(rect.get_center())
	check(results.size() == 1, "completed alignment emits once")
	canvas.start_alignment()
	var press := InputEventScreenTouch.new()
	press.index = 3
	press.position = rect.get_center()
	press.pressed = true
	canvas._gui_input(press)
	var drag := InputEventScreenDrag.new()
	drag.index = 3
	drag.position = rect.get_center() + Vector2(60,0)
	canvas._gui_input(drag)
	press.pressed = false
	press.position = drag.position
	canvas._gui_input(press)
	check(canvas.points.is_empty(), "dragging does not accidentally place a feature")
	canvas.stop_alignment()
	canvas.select_point(rect.get_center())
	check(canvas.points.is_empty(), "leaving the page cancels calibration")
	canvas.free()
	check(owner.clear_photo() == OK and not FileAccess.file_exists(Store.FIT_PATH), "removal clears owned photo alignment")
	DirAccess.remove_absolute(Store.DIRECTORY)
	print("PORTRAIT_CALIBRATION %s checks=%d failures=%d" % ["PASS" if failures == 0 else "FAIL", checks, failures])
	quit(0 if failures == 0 else 1)
