extends RefCounted
## Sole owner of the optional local portrait. No source path or image metadata is saved.
## Main-thread synchronous API: callers share this owner; there are no background writers.

const DIRECTORY := "user://player_identity"
const PHOTO_PATH := DIRECTORY + "/portrait.png"
const TEMP_PATH := DIRECTORY + "/portrait.tmp"
const FIT_PATH := DIRECTORY + "/face_fit.json"
const FIT_TEMP_PATH := DIRECTORY + "/face_fit.tmp"
const DEFAULT_LANDMARKS := [Vector2(.37, .40), Vector2(.63, .40), Vector2(.50, .66)]
const MAX_FILE_BYTES := 24 * 1024 * 1024
const MAX_SOURCE_EDGE := 4096
const MAX_SOURCE_PIXELS := 16 * 1024 * 1024
const PORTRAIT_EDGE := 768
var last_error: Error = OK

func load_identity() -> Dictionary:
	var image := _read_photo()
	return {"version": 1, "has_photo": image != null,
		"photo_path": PHOTO_PATH if image != null else ""}

func has_photo() -> bool:
	return _read_photo() != null

func photo_texture() -> Texture2D:
	var image := _read_photo()
	return ImageTexture.create_from_image(image) if image != null else null

func set_photo_from_image(image: Image) -> Error:
	if image == null or image.is_empty() or image.is_compressed():
		return _error(ERR_INVALID_DATA)
	if not _valid_size(image.get_width(), image.get_height()) or image.get_data_size() > MAX_SOURCE_PIXELS * 4:
		return _error(ERR_PARAMETER_RANGE_ERROR)
	var normalized := image.duplicate() as Image
	normalized.clear_mipmaps()
	normalized.convert(Image.FORMAT_RGBA8)
	var longest := maxi(normalized.get_width(), normalized.get_height())
	if longest > PORTRAIT_EDGE:
		var scale := float(PORTRAIT_EDGE) / float(longest)
		normalized.resize(maxi(1, roundi(normalized.get_width() * scale)), maxi(1, roundi(normalized.get_height() * scale)), Image.INTERPOLATE_LANCZOS)
	var bytes := normalized.save_png_to_buffer()
	if bytes.is_empty():
		return _error(ERR_FILE_CORRUPT)
	var err := _check_owned_paths()
	if err != OK:
		return _error(err)
	err = DirAccess.make_dir_recursive_absolute(DIRECTORY)
	if err != OK:
		return _error(err)
	var file := FileAccess.open(TEMP_PATH, FileAccess.WRITE)
	if file == null:
		return _error(FileAccess.get_open_error())
	file.store_buffer(bytes)
	file.flush()
	err = file.get_error()
	file.close()
	if err != OK:
		return _error(err)
	# Same-directory rename replaces only our portrait, keeping the old photo on failure.
	err = DirAccess.rename_absolute(TEMP_PATH, PHOTO_PATH)
	return _error(err)

## Alignment belongs to this exact normalized portrait. Replacing the image
## invalidates old points even when a native picker reuses its source filename.
func face_landmarks() -> PackedVector2Array:
	var fallback := PackedVector2Array(DEFAULT_LANDMARKS)
	if _check_owned_paths() != OK or not FileAccess.file_exists(PHOTO_PATH) or not FileAccess.file_exists(FIT_PATH):
		return fallback
	var file := FileAccess.open(FIT_PATH, FileAccess.READ)
	if file == null:
		return fallback
	if file.get_length() > 1024:
		file.close()
		return fallback
	var value: Variant = JSON.parse_string(file.get_as_text())
	file.close()
	if not value is Dictionary or value.get("version") != 1 or value.get("portrait_sha256") != FileAccess.get_sha256(PHOTO_PATH):
		return fallback
	var raw: Variant = value.get("points")
	if not raw is Array or raw.size() != 3:
		return fallback
	var points := PackedVector2Array()
	for pair in raw:
		if not pair is Array or pair.size() != 2 or not (pair[0] is float or pair[0] is int) or not (pair[1] is float or pair[1] is int):
			return fallback
		points.append(Vector2(pair[0], pair[1]))
	return points if valid_face_landmarks(points) else fallback

func save_face_landmarks(points: PackedVector2Array) -> Error:
	if not valid_face_landmarks(points):
		return _error(ERR_INVALID_DATA)
	var err := _check_owned_paths()
	if err != OK:
		return _error(err)
	if not FileAccess.file_exists(PHOTO_PATH):
		return _error(ERR_DOES_NOT_EXIST)
	var pairs: Array = []
	for point in points:
		pairs.append([point.x, point.y])
	var data := {"version": 1, "portrait_sha256": FileAccess.get_sha256(PHOTO_PATH), "points": pairs}
	var file := FileAccess.open(FIT_TEMP_PATH, FileAccess.WRITE)
	if file == null:
		return _error(FileAccess.get_open_error())
	file.store_string(JSON.stringify(data))
	file.flush()
	err = file.get_error()
	file.close()
	if err != OK:
		return _error(err)
	return _error(DirAccess.rename_absolute(FIT_TEMP_PATH, FIT_PATH))

static func valid_face_landmarks(points: PackedVector2Array) -> bool:
	if points.size() != 3:
		return false
	for point in points:
		if not point.is_finite() or point.x < 0 or point.x > 1 or point.y < 0 or point.y > 1:
			return false
	var eyes := points[1] - points[0]
	return eyes.x >= .025 and eyes.length() >= .04 and eyes.cross(points[2] - points[0]) >= .001

func clear_photo() -> Error:
	var err := _check_owned_paths()
	if err != OK:
		return _error(err)
	for path in [FIT_TEMP_PATH, FIT_PATH, TEMP_PATH, PHOTO_PATH]:
		if FileAccess.file_exists(path):
			err = DirAccess.remove_absolute(path)
			if err != OK:
				return _error(err)
	return _error(OK)

## Reads only a selected PNG or JPEG. Header checks bound decoder allocations first.
## Uses FileAccess for the selected path without URI rewriting or broad permission.
## Native Android FileDialog is documented; Android content-URI reads require device proof.
func import_photo(path: String) -> Error:
	var image := read_selected_image(path)
	if image == null:
		return last_error
	return set_photo_from_image(image)

func read_selected_image(path: String) -> Image:
	var file := FileAccess.open(path, FileAccess.READ)
	if file == null:
		_error(FileAccess.get_open_error())
		return null
	var size := file.get_length()
	if size < 24 or size > MAX_FILE_BYTES:
		file.close()
		_error(ERR_PARAMETER_RANGE_ERROR)
		return null
	var bytes := file.get_buffer(size)
	var err := file.get_error()
	file.close()
	if bytes.size() != size or (err != OK and err != ERR_FILE_EOF):
		_error(ERR_FILE_CANT_READ if err == OK else err)
		return null
	var dimensions := _encoded_size(bytes)
	if not _valid_size(dimensions.x, dimensions.y):
		_error(ERR_INVALID_DATA)
		return null
	var image := Image.new()
	if bytes[0] == 137:
		err = image.load_png_from_buffer(bytes)
	else:
		err = image.load_jpg_from_buffer(bytes)
	if err != OK:
		_error(err)
		return null
	if image.get_width() != dimensions.x or image.get_height() != dimensions.y:
		_error(ERR_INVALID_DATA)
		return null
	if bytes[0] == 255:
		_apply_orientation(image, _jpeg_orientation(bytes))
	_error(OK)
	return image

func _read_photo() -> Image:
	if _check_owned_paths() != OK:
		_error(ERR_FILE_BAD_PATH)
		return null
	if not FileAccess.file_exists(PHOTO_PATH):
		_error(OK)
		return null
	return read_selected_image(PHOTO_PATH)

func _check_owned_paths() -> Error:
	var user := DirAccess.open("user://")
	if user == null:
		return DirAccess.get_open_error()
	if user.is_link("player_identity"):
		return ERR_FILE_BAD_PATH
	if DirAccess.dir_exists_absolute(DIRECTORY):
		var directory := DirAccess.open(DIRECTORY)
		if directory == null:
			return DirAccess.get_open_error()
		if directory.is_link("portrait.png") or directory.is_link("portrait.tmp") or directory.is_link("face_fit.json") or directory.is_link("face_fit.tmp"):
			return ERR_FILE_BAD_PATH
	return OK

func _error(error: Error) -> Error:
	last_error = error
	return error

static func _valid_size(width: int, height: int) -> bool:
	return width > 0 and height > 0 and width <= MAX_SOURCE_EDGE and height <= MAX_SOURCE_EDGE and width * height <= MAX_SOURCE_PIXELS

static func _be16(bytes: PackedByteArray, offset: int) -> int:
	return (int(bytes[offset]) << 8) | int(bytes[offset + 1])

static func _be32(bytes: PackedByteArray, offset: int) -> int:
	return (_be16(bytes, offset) << 16) | _be16(bytes, offset + 2)

static func _encoded_size(bytes: PackedByteArray) -> Vector2i:
	if bytes.size() >= 33 and bytes.slice(0, 8) == PackedByteArray([137, 80, 78, 71, 13, 10, 26, 10]) and bytes.slice(12, 16).get_string_from_ascii() == "IHDR" and _be32(bytes, 8) == 13:
		return Vector2i(_be32(bytes, 16), _be32(bytes, 20))
	if bytes.size() < 4 or bytes[0] != 255 or bytes[1] != 216:
		return Vector2i.ZERO
	var offset := 2
	while offset + 4 <= bytes.size():
		if bytes[offset] != 255:
			return Vector2i.ZERO
		while offset < bytes.size() and bytes[offset] == 255:
			offset += 1
		if offset >= bytes.size():
			return Vector2i.ZERO
		var marker := int(bytes[offset])
		offset += 1
		if marker == 217 or marker == 218:
			return Vector2i.ZERO
		if marker == 1 or (marker >= 208 and marker <= 215):
			continue
		if offset + 2 > bytes.size():
			return Vector2i.ZERO
		var length := _be16(bytes, offset)
		if length < 2 or offset + length > bytes.size():
			return Vector2i.ZERO
		# Godot's JPEG decoder supports baseline/progressive JPEGs.
		if marker == 192 or marker == 194:
			if length < 8:
				return Vector2i.ZERO
			return Vector2i(_be16(bytes, offset + 5), _be16(bytes, offset + 3))
		offset += length
	return Vector2i.ZERO

static func _apply_orientation(image: Image, orientation: int) -> void:
	# Transform pixels before the PNG encoder discards the camera metadata.
	match orientation:
		2:
			image.flip_x()
		3:
			image.rotate_180()
		4:
			image.flip_y()
		5:
			image.rotate_90(CLOCKWISE)
			image.flip_x()
		6:
			image.rotate_90(CLOCKWISE)
		7:
			image.rotate_90(CLOCKWISE)
			image.flip_y()
		8:
			image.rotate_90(COUNTERCLOCKWISE)

static func _jpeg_orientation(bytes: PackedByteArray) -> int:
	var offset := 2
	while offset + 4 <= bytes.size():
		if bytes[offset] != 255:
			return 1
		while offset < bytes.size() and bytes[offset] == 255:
			offset += 1
		if offset >= bytes.size():
			return 1
		var marker := int(bytes[offset])
		offset += 1
		if marker == 217 or marker == 218:
			return 1
		if marker == 1 or (marker >= 208 and marker <= 215):
			continue
		if offset + 2 > bytes.size():
			return 1
		var length := _be16(bytes, offset)
		if length < 2 or offset + length > bytes.size():
			return 1
		if marker == 225 and length >= 16 and bytes.slice(offset + 2, offset + 8) == PackedByteArray([69, 120, 105, 102, 0, 0]):
			return _tiff_orientation(bytes, offset + 8, offset + length)
		offset += length
	return 1

static func _tiff16(bytes: PackedByteArray, offset: int, little: bool) -> int:
	return int(bytes[offset]) | (int(bytes[offset + 1]) << 8) if little else _be16(bytes, offset)

static func _tiff32(bytes: PackedByteArray, offset: int, little: bool) -> int:
	return _tiff16(bytes, offset, true) | (_tiff16(bytes, offset + 2, true) << 16) if little else _be32(bytes, offset)

static func _tiff_orientation(bytes: PackedByteArray, start: int, end: int) -> int:
	# Only IFD0's inline SHORT orientation is needed. Never follow arbitrary offsets
	# into another JPEG segment, recursively parse directories, or allocate tag data.
	if start + 8 > end:
		return 1
	var little := bytes[start] == 73 and bytes[start + 1] == 73
	if not little and not (bytes[start] == 77 and bytes[start + 1] == 77):
		return 1
	if _tiff16(bytes, start + 2, little) != 42:
		return 1
	var relative := _tiff32(bytes, start + 4, little)
	if relative < 8 or relative > end - start - 2:
		return 1
	var directory := start + relative
	var count := _tiff16(bytes, directory, little)
	if directory + 2 + count * 12 + 4 > end:
		return 1
	for index in range(count):
		var entry := directory + 2 + index * 12
		if _tiff16(bytes, entry, little) == 274:
			if _tiff16(bytes, entry + 2, little) != 3 or _tiff32(bytes, entry + 4, little) != 1:
				return 1
			var orientation := _tiff16(bytes, entry + 8, little)
			return orientation if orientation >= 1 and orientation <= 8 else 1
	return 1
