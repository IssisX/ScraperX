extends "res://presentation/ui/ui_test_uncanny.gd"

# A real GUI input round trip; no unapproved human recordings may play.
func _touch_photo_id_radio() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_key(KEY_ESCAPE, true)
	_key(KEY_ESCAPE, false)
	await _frames(3)
	if not _main._paused or not _main._pause_menu.visible:
		return _fail("Escape did not open the pause menu")
	_click(_main._pause_menu.side_button_center(&"work_id"))
	await _frames(3)
	if _main._pause_menu.current_page() != &"work_id":
		return _fail("WORK ID did not open on touch")
	var page: Control = _main._pause_menu._identity_page
	if not is_instance_valid(page) or not page.is_visible_in_tree() or page.choose_button.disabled:
		return _fail("photo intake is not available on WORK ID")
	if not page.get_child(0) is Label or "JACK MILLER" not in (page.get_child(0) as Label).text:
		return _fail("Work ID did not show Jack Miller")
	if page._default_button.text != "USE DEFAULT JACK":
		return _fail("Work ID default name is not Jack")
	if _main._pause_menu.identity_store != _main._player_identity:
		return _fail("photo page does not share identity owner")
	await _pose("photo_id_page")
	_key(KEY_ESCAPE, true)
	_key(KEY_ESCAPE, false)
	await _frames(3)
	if _main._pause_menu.current_page() != &"":
		return _fail("Back did not leave WORK ID")
	_click(_main._pause_menu.resume_button_center())
	await _frames(3)
	if _main._paused or _main._pause_menu.visible:
		return _fail("resume did not restore gameplay")
	var radio: Node = _main._story_radio
	var cue: Dictionary = radio.cue_metadata(&"receiving_iona_recovery")
	if cue.is_empty():
		return _fail("recovery cue metadata missing")
	radio.present({"transmission_id": 1, "transmission_key": &"receiving_iona_recovery",
		"speaker": cue["speaker"], "transmission": cue["text"]}, 0.0)
	var state: Dictionary = radio.readback()
	if String(state["status"]) != "voice_unapproved" or state["playing"] or int(state["scheduled_plays"]) != 0:
		return _fail("unapproved voice was not blocked")
	if not await _touch_radio(InputRouter.Device.TOUCH):
		return false
	_detail += " work_id=opened story_voice=blocked foreman=played"
	return true
