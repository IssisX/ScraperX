extends SceneTree

const Store := preload("res://presentation/identity/player_identity_store.gd")
const Intake := preload("res://presentation/identity/photo_id_intake.gd")
var failed := false
var changes := 0
var cancellations := 0
var errors := 0

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error("PLAYER_IDENTITY FAIL " + message)

func write_bytes(path: String, bytes: PackedByteArray) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	check(file != null, "open test input")
	if file != null:
		file.store_buffer(bytes)
		file.close()

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	# Never erase a real profile even if this script is accidentally run in production.
	if DirAccess.dir_exists_absolute(Store.DIRECTORY):
		push_error("PLAYER_IDENTITY requires a fixture without an existing player_identity directory")
		quit(1)
		return
	var store := Store.new()
	check(store.load_identity() == {"version": 1, "has_photo": false, "photo_path": ""}, "absent identity uses defaults")
	check(store.photo_texture() == null, "absent photo has no texture")
	check(store.clear_photo() == OK, "clear absent photo is safe")
	check(store.set_photo_from_image(null) == ERR_INVALID_DATA, "null photo rejected")
	var image := Image.create(1200, 600, false, Image.FORMAT_RGB8)
	image.fill(Color.RED)
	check(store.set_photo_from_image(image) == OK, "save photo")
	check(image.get_width() == 1200, "normalization does not mutate caller image")
	check(Store.new().has_photo(), "photo survives new store")
	var texture := store.photo_texture()
	check(texture != null and texture.get_width() == 768 and texture.get_height() == 384, "portrait bounded with aspect preserved")
	check(not FileAccess.file_exists(Store.TEMP_PATH), "commit consumes temporary file")
	var sentinel := Store.DIRECTORY + "/keep-me.txt"
	var outside := "user://player_identity_test_outside.txt"
	var source := "user://player_identity_test_source.jpg"
	write_bytes(sentinel, "untouched".to_utf8_buffer())
	write_bytes(outside, "untouched".to_utf8_buffer())
	image.fill(Color.BLUE)
	write_bytes(source, image.save_jpg_to_buffer())
	check(store.import_photo(source) == OK, "JPEG intake")
	var pixel := store.photo_texture().get_image().get_pixel(0, 0)
	check(pixel.b > 0.9 and pixel.r < 0.1, "replacement photo is visible")
	_check_orientations(store, source)
	var before := FileAccess.get_file_as_bytes(Store.PHOTO_PATH)
	write_bytes(source, "this is corrupt image data".to_utf8_buffer())
	check(store.import_photo(source) != OK, "corrupt selected image rejected")
	check(FileAccess.get_file_as_bytes(Store.PHOTO_PATH) == before, "failed import keeps old photo")
	# A valid-looking PNG with enormous dimensions must fail before decoding it.
	var header := image.save_png_to_buffer().slice(0, 33)
	header[16] = 127
	write_bytes(source, header)
	check(store.import_photo(source) == ERR_INVALID_DATA, "oversized encoded image rejected before allocation")
	check(DirAccess.make_dir_absolute(Store.TEMP_PATH) == OK, "create blocked temporary target")
	check(store.set_photo_from_image(image) != OK, "write failure is reported")
	check(FileAccess.get_file_as_bytes(Store.PHOTO_PATH) == before, "write failure preserves committed photo")
	check(DirAccess.remove_absolute(Store.TEMP_PATH) == OK, "remove test obstruction")
	var intake := Intake.new()
	intake.store = store
	root.add_child(intake)
	intake.photo_changed.connect(func(): changes += 1)
	intake.cancelled.connect(func(): cancellations += 1)
	intake.failed.connect(func(_error: Error, _message: String): errors += 1)
	intake.request_photo()
	var cancelled_id: int = intake._request_id
	intake.cancel()
	intake.cancel()
	intake._on_file_selected(source, cancelled_id)
	check(cancellations == 1 and changes == 0 and errors == 0, "cancel emits once and late selection is ignored")
	intake.request_photo()
	intake._on_file_selected(source, cancelled_id)
	check(intake._pending, "old selection cannot complete a newer request")
	write_bytes(source, image.save_png_to_buffer())
	intake._on_file_selected(source, intake._request_id)
	check(changes == 1 and errors == 0, "current request imports and emits change")
	intake.free()
	check(store.clear_photo() == OK and not store.has_photo(), "clear returns to default appearance")
	check(FileAccess.get_file_as_string(sentinel) == "untouched", "clear preserves other identity-directory files")
	check(FileAccess.get_file_as_string(outside) == "untouched", "clear preserves other user paths")
	write_bytes(Store.PHOTO_PATH, "corrupt saved portrait".to_utf8_buffer())
	check(not store.load_identity().has_photo and store.last_error != OK, "corrupt saved image has defaults and explicit error")
	check(store.clear_photo() == OK, "corrupt portrait can be cleared")
	if OS.get_name() == "Linux":
		var directory := DirAccess.open(Store.DIRECTORY)
		check(directory.create_link(ProjectSettings.globalize_path(outside), "portrait.tmp") == OK, "create test-owned redirected temporary file")
		check(store.set_photo_from_image(image) == ERR_FILE_BAD_PATH, "redirected temporary target is rejected")
		check(store.clear_photo() == ERR_FILE_BAD_PATH, "clear refuses redirected owned paths")
		check(FileAccess.get_file_as_string(outside) == "untouched", "redirect cannot overwrite another user file")
		check(DirAccess.remove_absolute(Store.TEMP_PATH) == OK, "remove test-owned link")
	for path in [sentinel, outside, source]:
		check(DirAccess.remove_absolute(path) == OK, "remove test-owned input")
	check(DirAccess.remove_absolute(Store.DIRECTORY) == OK, "remove empty test-owned directory")
	print("PLAYER_IDENTITY failures=" + str(int(failed)))
	quit(1 if failed else 0)

func _jpeg_with_orientation(jpeg: PackedByteArray, orientation: int) -> PackedByteArray:
	# Real APP1 Exif header, little-endian TIFF, IFD0 SHORT orientation tag.
	var app1 := PackedByteArray([255, 225, 0, 34, 69, 120, 105, 102, 0, 0,
		73, 73, 42, 0, 8, 0, 0, 0, 1, 0, 18, 1, 3, 0, 1, 0, 0, 0,
		orientation, 0, 0, 0, 0, 0, 0, 0])
	return jpeg.slice(0, 2) + app1 + jpeg.slice(2)

func _check_orientations(store: RefCounted, path: String) -> void:
	var corners := Image.create(80, 40, false, Image.FORMAT_RGB8)
	var colors := [Color.RED, Color.GREEN, Color.BLUE, Color.YELLOW]
	corners.fill_rect(Rect2i(0, 0, 40, 20), colors[0])
	corners.fill_rect(Rect2i(40, 0, 40, 20), colors[1])
	corners.fill_rect(Rect2i(0, 20, 40, 20), colors[2])
	corners.fill_rect(Rect2i(40, 20, 40, 20), colors[3])
	var jpeg := corners.save_jpg_to_buffer(1.0)
	var raw := Image.new()
	check(raw.load_jpg_from_buffer(_jpeg_with_orientation(jpeg, 6)) == OK, "orientation probe JPEG decodes")
	print("PLAYER_IDENTITY raw orientation6 decoded_size=", raw.get_size(), " top_left=", raw.get_pixel(10, 10))
	var expected := [[0, 1, 2, 3], [1, 0, 3, 2], [3, 2, 1, 0], [2, 3, 0, 1],
		[0, 2, 1, 3], [2, 0, 3, 1], [3, 1, 2, 0], [1, 3, 0, 2]]
	for orientation in range(1, 9):
		write_bytes(path, _jpeg_with_orientation(jpeg, orientation))
		check(store.import_photo(path) == OK, "EXIF orientation import " + str(orientation))
		var result: Image = store.photo_texture().get_image()
		var size := Vector2i(40, 80) if orientation >= 5 else Vector2i(80, 40)
		check(result.get_size() == size, "EXIF orientation dimensions " + str(orientation))
		var positions := [Vector2i(10, 10), Vector2i(size.x - 10, 10),
			Vector2i(10, size.y - 10), Vector2i(size.x - 10, size.y - 10)]
		# Only sample valid pixels when the loader failed to rotate; dimensions already failed.
		if result.get_size() != size:
			continue
		for index in range(4):
			var actual := result.get_pixelv(positions[index])
			var wanted: Color = colors[expected[orientation - 1][index]]
			check(Vector3(actual.r, actual.g, actual.b).distance_to(Vector3(wanted.r, wanted.g, wanted.b)) < 0.08,
				"EXIF orientation corners " + str(orientation) + "/" + str(index))
