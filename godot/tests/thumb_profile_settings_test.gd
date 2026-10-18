extends SceneTree

# Standalone settings/menu check: no Main scene, native extension or gameplay.
const SettingsStore := preload("res://presentation/ui/settings_store.gd")
const PauseMenu := preload("res://presentation/ui/pause_menu.gd")

var _checks := 0
var _failures: Array[String] = []


func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures.append(message)
		push_error("SCRAPERX_THUMB_PROFILE_SETTINGS FAIL " + message)


func _help(menu: Control) -> String:
	var descriptions := ""
	for row in menu._controls_rows(4):
		descriptions += row[2] + ": " + row[3] + "\n"
	return descriptions


func _persistence() -> void:
	# Preserve an existing user file even when invoked in the shipping project.
	var existed := FileAccess.file_exists(SettingsStore.PATH)
	var original := FileAccess.get_file_as_bytes(SettingsStore.PATH) if existed else PackedByteArray()
	var settings := SettingsStore.new()
	_check(settings.touch_profile == SettingsStore.TouchProfile.BUTTONS, "new settings default to Buttons")
	for profile in [SettingsStore.TouchProfile.BUTTONS, SettingsStore.TouchProfile.THUMB_FLOW]:
		settings.touch_profile = profile
		settings.save_to_disk()
		var loaded := SettingsStore.new()
		loaded.load_from_disk()
		_check(loaded.touch_profile == profile, "saved profile roundtrip %d" % profile)
	for invalid in [-1, 2, "1", 1.0, true, null, {}, []]:
		var file := ConfigFile.new()
		file.set_value(SettingsStore.SECTION, "touch_profile", invalid)
		_check(file.save(SettingsStore.PATH) == OK, "write invalid-profile fixture")
		settings.touch_profile = SettingsStore.TouchProfile.THUMB_FLOW
		settings.load_from_disk()
		_check(settings.touch_profile == SettingsStore.TouchProfile.BUTTONS,
			"invalid profile falls back to Buttons: " + str(invalid))
	var legacy := ConfigFile.new()
	legacy.set_value(SettingsStore.SECTION, "look_sensitivity", 1.25)
	_check(legacy.save(SettingsStore.PATH) == OK, "write legacy fixture")
	settings.touch_profile = SettingsStore.TouchProfile.THUMB_FLOW
	settings.load_from_disk()
	_check(settings.touch_profile == SettingsStore.TouchProfile.BUTTONS, "missing profile falls back to Buttons")
	_check(settings.look_sensitivity == 1.25, "legacy settings still load")
	if existed:
		var restore := FileAccess.open(SettingsStore.PATH, FileAccess.WRITE)
		_check(restore != null, "open original settings for restoration")
		if restore != null:
			restore.store_buffer(original)
			restore.close()
			_check(FileAccess.get_file_as_bytes(SettingsStore.PATH) == original, "restore original settings bytes")
	else:
		_check(DirAccess.remove_absolute(SettingsStore.PATH) == OK, "remove isolated settings fixture")


func _run() -> void:
	_persistence()
	root.size = Vector2i(432, 371)
	root.content_scale_size = Vector2i(432, 371)
	var settings := SettingsStore.new()
	settings.persistent = false
	var menu := PauseMenu.new()
	menu.settings = settings
	root.add_child(menu)
	menu.open(4, "Thumb profile settings check")
	menu._show_page(&"settings")
	await process_frame
	await process_frame
	var choice: Button = menu._page_first[&"settings"]
	_check(choice.text == "BUTTONS", "profile is the first accessible setting")
	_check(_help(menu).contains("JUMP: Jump;"), "Buttons help retains Jump button")
	var button_help_height: float = menu._controls_view.custom_minimum_size.y
	var changes := [0]
	menu.settings_changed.connect(func() -> void: changes[0] += 1)
	choice.pressed.emit()
	_check(settings.touch_profile == SettingsStore.TouchProfile.THUMB_FLOW and choice.text == "THUMB FLOW",
		"selection applies Thumb Flow")
	_check(changes[0] == 1, "one selection emits one settings change")
	_check(menu._controls_view.custom_minimum_size.y > button_help_height, "profile change refreshes help layout immediately")
	var help := _help(menu)
	_check(help.contains("QUICK TAP:") and help.contains("NEXT HAND:"), "Thumb Flow teaches context-aware tap")
	_check(help.contains("LEFT TAP:") and help.contains("Lifting after a drag never changes stance"), "Thumb Flow teaches stationary left tap and drag safety")
	_check(help.contains("quick tap LEFT before a hard landing") and help.contains("quick tap RIGHT exits"), "landing help distinguishes stance and Jump thumbs")
	_check(help.contains("Fan DOWN is another way"), "fan DOWN remains a stance alternative")
	_check(help.contains("INWARD toward screen center") and help.contains("hold until armed"), "Thumb Flow teaches deliberate release")
	_check(help.contains("OUTWARD") and help.contains("a caught grip stays held"), "Thumb Flow teaches cancellation without grip release")
	_check(not help.contains("JUMP: Jump;"), "Thumb Flow removes generic Jump button help")
	for equipment in ["BOARD:", "AIM:", "DRAW:", "RELEASE:", "CHUTE / BRAKE:", "RETRIEVE:", "DROP:", "GRAPPLE:", "AUTO REEL / STOP:", "MACHINE:", "PAUSE:"]:
		_check(help.contains(equipment), "preserve equipment/help row " + equipment)
	for viewport in [Vector2i(432, 371), Vector2i(360, 640)]:
		root.size = viewport
		root.content_scale_size = viewport
		await process_frame
		await process_frame
		menu._resize_open_menu()
		menu._show_page(&"settings")
		await process_frame
		await process_frame
		_check(menu.settings_overflow() <= 0.0, "settings accessible at " + str(viewport))
		menu._show_page(&"controls")
		await process_frame
		await process_frame
		_check(menu._controls_view.custom_minimum_size.y > 0.0, "gesture help lays out in scrolling controls")
	choice = menu._page_first[&"settings"]
	choice.pressed.emit()
	_check(settings.touch_profile == SettingsStore.TouchProfile.BUTTONS and changes[0] == 2,
		"selection wraps to Buttons with one further change")
	_check(_help(menu).contains("JUMP: Jump;"), "help returns to Buttons")
	menu.close()
	menu.queue_free()
	await process_frame
	if _failures.is_empty():
		print("SCRAPERX_THUMB_PROFILE_SETTINGS PASS checks=%d standalone_ui=1" % _checks)
		quit(0)
	else:
		print("SCRAPERX_THUMB_PROFILE_SETTINGS FAIL checks=%d failures=%d" % [_checks, _failures.size()])
		quit(42)
