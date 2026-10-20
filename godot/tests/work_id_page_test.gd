extends SceneTree

const Store := preload("res://presentation/identity/player_identity_store.gd")
const Menu := preload("res://presentation/ui/pause_menu.gd")
const Settings := preload("res://presentation/ui/settings_store.gd")
var checks := 0
var failures: Array[String] = []

# Isolate menu behavior from personal files. Real persistence/JPEG handling
# is exercised by player_identity_test; this does not prove Android picking.
class MemoryStore extends Store:
	var texture: Texture2D
	var clears := 0
	func photo_texture() -> Texture2D:
		return texture
	func clear_photo() -> Error:
		texture = null
		clears += 1
		return OK

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("WORK_ID_PAGE " + why)

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	var settings := Settings.new()
	settings.persistent = false
	var store := MemoryStore.new()
	var menu := Menu.new()
	menu.settings = settings
	menu.identity_store = store
	root.add_child(menu)
	menu.open(4, "Work ID integration")
	menu._side_buttons[&"work_id"].pressed.emit()
	check(menu.current_page() == &"work_id", "pause button opens optional work ID")
	var page: Control = menu._identity_page
	check(page._intake.store == store, "UI and intake share the one supplied identity owner")
	check(page._portrait.texture == null and page._default_button.disabled, "default character needs no personal image")
	var changed := [0]
	menu.identity_changed.connect(func() -> void: changed[0] += 1)
	var image := Image.create(64, 80, false, Image.FORMAT_RGB8)
	image.fill(Color.BLUE)
	store.texture = ImageTexture.create_from_image(image)
	page._intake.photo_changed.emit()
	check(page._portrait.texture == store.texture and changed[0] == 1, "selected image refreshes preview and emits one change")
	page._default_button.pressed.emit()
	check(page._portrait.texture == null and store.clears == 1 and changed[0] == 2, "default button removes only the portrait through its owner")
	page._intake.request_photo()
	check(page._intake._pending, "picker request starts from the explicit menu control")
	menu.back()
	check(menu.current_page() == &"" and not page._intake._pending, "back cancels selection before returning to the game menu")
	for size: Vector2i in [Vector2i(2160, 1856), Vector2i(1920, 1080), Vector2i(1080, 2400)]:
		root.size = size
		menu.open(4, "Work ID layout")
		menu._show_page(&"work_id")
		await process_frame
		await process_frame
		check(menu.settings_overflow() <= 1.0, "work ID preserves accessible page width at " + str(size))
	menu.close()
	check(not menu.visible, "resume closes optional identity surface")
	menu.queue_free()
	await process_frame
	print("WORK_ID_PAGE %s checks=%d failures=%d native_picker_device_unverified=1" % ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	quit(0 if failures.is_empty() else 1)
