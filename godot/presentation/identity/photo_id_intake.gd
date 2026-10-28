extends Control
## Optional local selection. The parent UI displays PURPOSE before calling request_photo.
## Native Android picker is documented for ACCESS_FILESYSTEM; device execution is separate.

signal photo_changed
signal cancelled
signal failed(error: Error, message: String)

const Store := preload("res://presentation/identity/player_identity_store.gd")
const PURPOSE := "Personalize your work ID and in-game character appearances"
var store := Store.new()
var _dialog: FileDialog
var _pending := false
var _request_id := 0

func request_photo() -> void:
	if _pending:
		return
	if not is_inside_tree():
		failed.emit(ERR_UNCONFIGURED, "Add photo intake to the scene before choosing a photo.")
		return
	_request_id += 1
	if _dialog != null:
		_dialog.queue_free()
	_dialog = null
	_dialog = FileDialog.new()
	_dialog.access = FileDialog.ACCESS_FILESYSTEM
	_dialog.file_mode = FileDialog.FILE_MODE_OPEN_FILE
	_dialog.use_native_dialog = true
	_dialog.title = "Choose an optional work ID photo"
	_dialog.add_filter("*.png,*.jpg,*.jpeg", "Photos", "image/png,image/jpeg")
	_dialog.file_selected.connect(_on_file_selected.bind(_request_id))
	_dialog.canceled.connect(_on_dialog_cancelled.bind(_request_id))
	add_child(_dialog)
	_pending = true
	_dialog.popup_centered_clamped(Vector2i(720, 800), 0.9)

func cancel() -> void:
	if not _pending:
		return
	_pending = false
	if _dialog != null:
		_dialog.hide()
	cancelled.emit()

func _on_dialog_cancelled(request_id: int) -> void:
	if request_id == _request_id:
		cancel()

func _on_file_selected(path: String, request_id: int) -> void:
	# A late native picker callback after cancel cannot write a photo.
	if not _pending or request_id != _request_id:
		return
	_pending = false
	var error: Error = store.import_photo(path)
	if error != OK:
		failed.emit(error, "Could not use this photo. Choose a PNG or JPEG up to 4096 pixels per side and 24 MB. Nothing was changed.")
		return
	photo_changed.emit()

func _exit_tree() -> void:
	cancel()
