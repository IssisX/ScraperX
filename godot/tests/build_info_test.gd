extends SceneTree

const BuildInfo := preload("res://presentation/ui/build_info.gd")
const Pause := preload("res://presentation/ui/pause_menu.gd")
const Settings := preload("res://presentation/ui/settings_store.gd")
const SHA := "a7fbce49878830a4991d1c14498f66b45db4d045"
var failed := false

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error("BUILD_INFO FAIL " + message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var path := "user://build_info_test.json"
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(JSON.stringify({"source_sha": SHA, "version_code": 214999999}))
	file.close()
	check(BuildInfo.label(path) == "BUILD 214999999  /  SOURCE a7fbce498788", "reads final APK identity")
	file = FileAccess.open(path, FileAccess.WRITE)
	file.store_string(JSON.stringify({"source_sha": SHA, "version_code": null}))
	file.close()
	check(BuildInfo.label(path) == "DEVELOPMENT  /  SOURCE a7fbce498788", "preexport does not invent a final code")
	for identity in [{"source_sha": "unknown", "version_code": 214999999},
		{"source_sha": SHA, "version_code": -1},
		{"source_sha": SHA, "version_code": 42.5},
		{"source_sha": SHA, "version_code": "42"}]:
		file = FileAccess.open(path, FileAccess.WRITE)
		file.store_string(JSON.stringify(identity))
		file.close()
		check(BuildInfo.label(path) == "LOCAL DEVELOPMENT", "invalid identity cannot claim a build")
	file = FileAccess.open(path, FileAccess.WRITE)
	file.store_string("not json")
	file.close()
	check(BuildInfo.label(path) == "LOCAL DEVELOPMENT", "malformed metadata is honest")
	DirAccess.remove_absolute(path)
	check(BuildInfo.label(path) == "LOCAL DEVELOPMENT", "absent metadata is honest")
	var menu := Pause.new()
	menu.settings = Settings.new()
	root.add_child(menu)
	menu.open(0, "TEST")
	await process_frame
	var label := menu.find_child("BuildInfo", true, false) as Label
	check(label != null, "pause contains build label")
	if label != null:
		check(label.text == BuildInfo.label(), "pause uses the actual packaged identity")
		check(label.is_visible_in_tree(), "identity is visible on opening pause")
		check(label.autowrap_mode == TextServer.AUTOWRAP_WORD_SMART, "identity wraps on phone")
	menu.free()
	print("BUILD_INFO failures=" + str(int(failed)))
	quit(1 if failed else 0)
