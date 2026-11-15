extends "res://presentation/ui/ui_test_look.gd"

# Leaving the app mid-anything (J-118 #440): a call, the home button, the
# fold closing, Back. The OS backgrounds the app through the tree exactly as
# a phone delivers it (root.propagate_notification), and each scenario checks
# the goal's list: the sim tick does not move while away, the first frame back
# advances one tick, the pause menu is up except on the title screen, the
# sound is back after resume, the save on disk is the committed checkpoint,
# and Back on the pause menu goes back as _pause_menu.back() does.
#
# One scenario per process (the native spawn is fixed at tick 0). The pause
# is only let through for these names: main.gd refuses a system pause during
# any other scripted run, so a stray focus-out cannot freeze an unrelated
# scenario mid-run.

const _APP_LEAVE_SAVE := "user://uitest_app_leave.save"

# The bytes of the last commit the probe has seen on disk. The pause writes
# only when a commit or a death has changed since the last write
# (main.gd _save_climb), so the file on disk is the last COMMIT, not the live
# state: the sim runs on between the arm write and the pause, and the resumed
# sim runs on after it, so the live bytes differ from the committed ones by
# design. The check is the disk against the last commit, never the live sim.
var _app_leave_committed: PackedByteArray = PackedByteArray()


# The OS backgrounding or foregrounding the app, through the tree as a phone
# delivers it: every node with a _notification handler hears it, main.gd and
# the microphone's keeper alike.
func _app_leave_send(what: int) -> void:
	get_tree().root.propagate_notification(what)


# The committed checkpoint on disk, for the leave to be measured against:
# the save the pause itself will write is compared to this file. The arm is
# the last commit before the pause: it writes the state at arm time, and the
# snapshot is those bytes.
func _app_leave_arm_save() -> bool:
	for leftover in [_APP_LEAVE_SAVE, _APP_LEAVE_SAVE + ".tmp", _APP_LEAVE_SAVE + ".bad"]:
		if FileAccess.file_exists(leftover):
			DirAccess.remove_absolute(leftover)
	_main._save_path = _APP_LEAVE_SAVE
	_main._save_enabled = true
	_main._save_climb(true)
	if not FileAccess.file_exists(_APP_LEAVE_SAVE):
		return _fail("no save written before leaving")
	_app_leave_committed = FileAccess.get_file_as_bytes(_APP_LEAVE_SAVE)
	return true


# The save on disk is the committed checkpoint: the bytes of the last commit
# the probe has seen, or the live bytes when the pause (or the final save)
# itself wrote -- the sim is frozen then, so the live bytes are the commit.
# A passing check refreshes the snapshot to the disk bytes: the disk IS the
# committed checkpoint from here on. The one thing the check cannot cover is
# a checkpoint crossed in the few resumed frames before the final save; if
# that ever trips, the sizes and the stability flag in the reason say so.
func _app_leave_save_is_committed() -> bool:
	var bytes := FileAccess.get_file_as_bytes(_APP_LEAVE_SAVE)
	if bytes.is_empty():
		return _fail("the save on disk is empty")
	var now: PackedByteArray = _native().get_save_game()
	if bytes == _app_leave_committed or bytes == now:
		_app_leave_committed = bytes
		return true
	var now2: PackedByteArray = _native().get_save_game()
	return _fail("the save on disk is not the committed checkpoint (%d bytes on disk, %d now, save stable %s)" % [
		bytes.size(), now.size(), str(now == now2)])


func _app_leave_disarm_save() -> void:
	_main._save_enabled = false
	if FileAccess.file_exists(_APP_LEAVE_SAVE):
		DirAccess.remove_absolute(_APP_LEAVE_SAVE)


# The sound that must come back: the wind bed plays from launch on a real
# device, and the mix runs at the moment's own rate (slow in her hold, full
# elsewhere). On the Dummy driver the director never starts the players
# (_silent, audio_director.gd), so there the mix rate is the checkable sound.
func _app_leave_sound_is_back(moment: String) -> bool:
	if AudioServer.get_driver_name() != "Dummy" and not bool(_main._audio._wind.playing):
		return _fail("%s: the wind bed was not playing after resume" % moment)
	var want := minf(_main._launch_shot.time_scale(), float(_main._hold_cam.time_scale()))
	if not is_equal_approx(AudioServer.playback_speed_scale, want):
		return _fail("%s: the mix runs at %.2f after resume, the moment's rate is %.2f" % [
			moment, AudioServer.playback_speed_scale, want])
	return true


# The leave itself, shared by the four in-play moments: backgrounded at the
# staged moment, the sim frozen, the menu up, the save the committed
# checkpoint; back, the first frame one tick, the menu's Back going back,
# the sound back. `moment` names the staged moment for the detail line.
func _app_leave_probe(moment: String) -> bool:
	# On a real device the wind bed plays from launch; on Dummy it never
	# starts, by the director's design, so there is nothing to pin before.
	if AudioServer.get_driver_name() != "Dummy" and not bool(_main._audio._wind.playing):
		return _fail("%s: the wind bed was not playing before the leave" % moment)
	if not await _app_leave_arm_save():
		return false
	var tick := int(_native().get_tick_index())
	# A call: the OS backgrounds the app.
	_app_leave_send(NOTIFICATION_APPLICATION_PAUSED)
	await _frames(2)
	if not bool(_main._paused) or not get_tree().paused or not _main._pause_menu.visible:
		return _fail("%s: the backgrounding did not open the pause menu (paused %s, tree %s, menu %s)" % [
			moment, str(bool(_main._paused)), str(get_tree().paused), str(_main._pause_menu.visible)])
	await _frames(12)
	if int(_native().get_tick_index()) != tick:
		return _fail("%s: the sim tick moved while away (%d -> %d)" % [
			moment, tick, int(_native().get_tick_index())])
	# The microphone was let go while away: nothing heard. (The mic is off in
	# scripted runs, so this pins that the pause leaves it off, not the
	# device let-go itself.)
	if _main._uncanny != null and bool(_main._uncanny.voice.listening()):
		return _fail("%s: the microphone was still listening while away" % moment)
	if not _app_leave_save_is_committed():
		return false
	# Back: the OS foregrounds the app. The first frame back is one tick, not
	# up to nine: the load gap's one-tick cap, re-armed on resume, probed here
	# with a second's delta, the kind a real backgrounding leaves behind.
	_app_leave_send(NOTIFICATION_APPLICATION_RESUMED)
	_main._process(1.0)
	if int(_native().get_tick_index()) - tick > 1:
		return _fail("%s: the first frame back advanced %d ticks" % [
			moment, int(_native().get_tick_index()) - tick])
	# Back on the pause menu: a sub-page first, so Back has somewhere to go.
	_click(_main._pause_menu.side_button_center(&"controls"))
	await _frames(2)
	if _main._pause_menu.current_page() != &"controls":
		return _fail("%s: tapping CONTROLS did not open the controls page" % moment)
	_app_leave_send(NOTIFICATION_WM_GO_BACK_REQUEST)
	await _frames(2)
	if _main._pause_menu.current_page() != &"":
		return _fail("%s: Back did not leave the controls page (on %s)" % [
			moment, String(_main._pause_menu.current_page())])
	if not bool(_main._paused) or not get_tree().paused:
		return _fail("%s: Back left the controls page and resumed, expected the menu" % moment)
	# Back again on the menu's main page: that is resume.
	_app_leave_send(NOTIFICATION_WM_GO_BACK_REQUEST)
	await _frames(2)
	if bool(_main._paused) or get_tree().paused:
		return _fail("%s: Back on the pause menu did not resume" % moment)
	# The first real frames back: a normal frame's advance, and the sound back.
	var tick_after := int(_native().get_tick_index())
	await _frames(2)
	if int(_native().get_tick_index()) - tick_after > 2:
		return _fail("%s: the first frames back advanced %d ticks" % [
			moment, int(_native().get_tick_index()) - tick_after])
	if not _app_leave_sound_is_back(moment):
		return false
	_main._save_climb(true)
	if not _app_leave_save_is_committed():
		return false
	_app_leave_disarm_save()
	_detail = "%s: away_ticks=%d back_ticks=%d" % [moment, tick, int(_native().get_tick_index())]
	return true


# A zipline ride: rolling, hands on the bar, when the call comes.
func _touch_app_leave_ride() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var device := InputRouter.Device.TOUCH
	if not await _board_yard_zipline(device):
		return false
	var rolling: bool = await _wait_until(func() -> bool:
		var zip: Dictionary = _native().get_zipline_state()
		return bool(zip["riding"]) and absf(float(zip["speed_mps"])) > 4.0, 8.0)
	if not rolling:
		return _fail("ride: never rolling on the line (at %s)" % str(_position()))
	await _pose("app_leave_ride")
	return await _app_leave_probe("ride")


# The slingshot draw: seated, the pouch pulled back, RELEASE offered, when
# the call comes.
func _touch_app_leave_slingshot() -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var device := InputRouter.Device.TOUCH
	if not (await _go(device, Vector2(6.0, -40.0), 0.3, 25.0) and \
			await _go(device, Vector2(6.0, -57.6), 0.2, 25.0) and \
			await _go(device, Vector2(-3.0, -57.6), 0.15, 12.0) and \
			await _go(device, Vector2(-3.0, -55.0), 0.1, 6.0)):
		return _fail("slingshot: the walk into the pouch stalled at %s" % str(_position()))
	if not await _offered(&"slingshot", "ENTER POUCH"):
		return _fail("slingshot: in the pouch, Action read '%s %s', not ENTER POUCH" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_sling()["seated"]), 1.0):
		return _fail("slingshot: ENTER POUCH did not harness the rider")
	if not await _offered(&"slingshot", "PULL BACK"):
		return _fail("slingshot: seated, Action read '%s', not PULL BACK" % _action_label())
	_move_dir(device, Vector2(0.0, -1.0))
	var drawn: bool = await _wait_until(func() -> bool: return float(_sling()["draw_m"]) >= 10.2, 8.0)
	if not drawn:
		_move_dir(device, Vector2.ZERO)
		return _fail("slingshot: the stick held back drew the pouch only %.2f m" % float(_sling()["draw_m"]))
	await _pose("app_leave_slingshot")
	return await _app_leave_probe("slingshot")


# Her hold: the act's play to her first grab, stopped at her hands on the
# climber instead of played through the rip (ui_test_act.gd).
func _touch_app_leave_hold() -> bool:
	var device := InputRouter.Device.TOUCH
	if not await _act_asleep(device):
		return false
	if not await _act_wake_and_go_below(device):
		return false
	if not await _act_pest_unseen(device):
		return false
	if not await _act_down_the_pipe(device):
		return false
	# Dangerous, by the rail: the preamble of _act_dangerous_and_changed,
	# stopped at her first hold.
	if not (await _walk_to(device, ACT_BAY_WEST, 0.3, 4.0, true) and \
			await _sprint_to(device, Vector2(16.0, -128.6), 1.0, 8.0) and \
			await _sprint_to(device, ACT_LAP_NORTH, 1.0, 6.0)):
		return _fail("hold: the run round onto the east band stalled at %s" % str(_position()))
	if int(_matron_state()["stage"]) < STAGE_DANGEROUS:
		var raged: bool = await _act_laps(device, ACT_LAP_SOUTH, ACT_LAP_NORTH,
				func() -> bool: return int(_matron_state()["stage"]) >= STAGE_DANGEROUS and \
					_act_her_xz().distance_to(_act_me_xz()) > 6.0, 150.0, "chase_to_dangerous")
		if not raged:
			return _fail("hold: the chase never made her dangerous (rage %.3f)" % float(_matron_state()["rage"]))
	if not (await _sprint_to(device, ACT_RIM, 1.0, 8.0) and await _walk_to(device, ACT_RIM, 0.3, 3.0)):
		return _fail("hold: the run to the shaft's rail stalled at %s" % str(_position()))
	if not await _act_call_her(40.0):
		var m := _matron_state()
		return _fail("hold: at the rail she never heard the climber (state %d at %s, lead %s at (%.1f, %.1f))" % [
			int(m["state"]), str(m["position"]), str(m["lead_valid"]), float(m["lead_x"]), float(m["lead_z"])])
	var catches := int(_matron_state()["catches"])
	await _act_laps(device, Vector2(ACT_RAIL_LAP_X, ACT_RIM.y + 3.0), Vector2(ACT_RAIL_LAP_X, ACT_RIM.y - 3.0),
			func() -> bool: return float(_matron_state()["rage"]) >= ACT_RAIL_RAGE or \
				_act_her_xz().distance_to(_act_me_xz()) < ACT_RAIL_NEAR_M or int(_matron_state()["catches"]) > catches,
			30.0, "rage_at_rail")
	if int(_native().get_death_count()) > 0:
		return _fail("hold: the sprints by the rail cost a death (deaths %d)" % int(_native().get_death_count()))
	if int(_matron_state()["catches"]) == catches:
		await _walk_to(device, ACT_RIM, 0.3, 3.0)
	# The grab north of the pin line: at the rim she pins in a Lunge 4 m
	# short (twice, the same spot, z=-154.0 at x=18.7 and at x=21.1),
	# drive applied but no displacement, while the act's own run carries
	# through there -- a knife-edge of Lunge phase, not a broken rail, and
	# no static solid or mesh wall stands at that line. North of it her
	# route to the climber stays on the east band's north segment and
	# never needs the edge south of it; sprint him there before she
	# starts south, so her first route south ends north of the line.
	if not await _sprint_to(device, Vector2(21.0, -150.0), 1.0, 6.0):
		return _fail("hold: the sprint north to the grab point stalled at %s" % str(_position()))
	if not await _walk_to(device, Vector2(21.0, -150.0), 0.3, 3.0):
		return _fail("hold: the walk north to the grab point stalled at %s" % str(_position()))
	var held := false
	for attempt in ACT_GRAB_TRIES:
		var caught := _act_in_her_hands()
		if attempt > 0 and not caught:
			# A pin does not clear by standing still: walk south through
			# the pin line and back north, so her chase and her Lunge
			# start fresh. A leg that stalls against her body is a hold,
			# not a failure: check her hands before failing it.
			var stalled := false
			if not await _sprint_to(device, Vector2(21.0, -158.0), 1.0, 6.0):
				stalled = true
			elif not await _walk_to(device, Vector2(21.0, -150.0), 0.3, 5.0):
				stalled = true
			if stalled:
				if _act_in_her_hands():
					caught = true
				else:
					return _fail("hold: the re-approach stalled at %s" % str(_position()))
			catches = int(_matron_state()["catches"])
		# She may have taken him during the re-approach: then no watch.
		if not caught:
			caught = await _act_watch_her(func() -> bool:
				return int(_matron_state()["catches"]) > catches, 35.0,
				"dangerous_at_rail", "act_dangerous_coming", 4.0)
		if not caught:
			if attempt == ACT_GRAB_TRIES - 1:
				return _fail("hold: dangerous, her hands never took the climber at the grab point (stage %d)" % int(_matron_state()["stage"]))
			continue
		if not await _act_wait(func() -> bool: return _act_in_her_hands(), 5.0, "rail_grab"):
			return _fail("hold: at the grab point her hands never came on the climber (state %d)" % int(_matron_state()["state"]))
		await _act_wait(func() -> bool:
			return int(_native().get_matron_hold_state()["grabs"]) >= 1 or not _act_in_her_hands(), 5.0, "rail_grab")
		if int(_native().get_matron_hold_state()["grabs"]) >= 1:
			held = true
			break
	if not held:
		return _fail("hold: dangerous, her hand never closed on the climber's ankle at the grab point in %d reaches" % ACT_GRAB_TRIES)
	var hold: Dictionary = _native().get_matron_hold_state()
	if not bool(hold.get("held", false)):
		return _fail("hold: her hands took the climber but the hold is not held (state %d, beat %d)" % [
			int(_matron_state()["state"]), int(hold.get("fatality_beat", -1))])
	await _pose("app_leave_hold")
	return await _app_leave_probe("hold")


# A fall with the canopy closed: stage 9 starts in a real free fall, as
# touch_chute stages it, and the CHUTE button is left alone.
func _touch_app_leave_fall() -> bool:
	var offered: bool = await _wait_until(func() -> bool: return _main._touch.is_button_shown(&"chute"), 3.0)
	if not offered:
		return _fail("fall: CHUTE never offered in a real free fall")
	if bool(_native().is_parachute_deployed()):
		return _fail("fall: the canopy was already open")
	await _pose("app_leave_fall")
	return await _app_leave_probe("fall")


# The title screen: a call there must not open the pause menu (the title is
# the screen), the sim stays frozen, the save stays committed, and coming
# back leaves the title up and the game starting from it.
func _touch_app_leave_title() -> bool:
	if not await _wait_until(func() -> bool: return bool(_main._title_open), 1.0):
		return _fail("title: not shown at launch")
	var tick := int(_native().get_tick_index())
	if not await _app_leave_arm_save():
		return false
	_app_leave_send(NOTIFICATION_APPLICATION_PAUSED)
	await _frames(2)
	if bool(_main._paused) or _main._pause_menu.visible:
		return _fail("title: the backgrounding opened the pause menu over the title")
	await _frames(12)
	if int(_native().get_tick_index()) != tick:
		return _fail("title: the simulation ran behind the title while away (%d -> %d)" % [
			tick, int(_native().get_tick_index())])
	if not _app_leave_save_is_committed():
		return false
	_app_leave_send(NOTIFICATION_APPLICATION_RESUMED)
	await _frames(2)
	if not bool(_main._title_open) or not _main._title.visible:
		return _fail("title: coming back closed the title")
	if int(_native().get_tick_index()) != tick:
		return _fail("title: the simulation ran behind the title on the way back (%d -> %d)" % [
			tick, int(_native().get_tick_index())])
	_click(_main._title.button_center(&"new"))
	if not await _wait_until(func() -> bool: return not bool(_main._title_open), 0.5):
		return _fail("title: NEW CLIMB did not start the climb after coming back")
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	_app_leave_disarm_save()
	_detail = "title: frozen_ticks=%d" % tick
	return true
