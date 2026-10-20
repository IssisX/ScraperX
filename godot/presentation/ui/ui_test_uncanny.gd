extends "res://presentation/ui/ui_test_act.gd"

# UI scenarios: her chart (03_EXECUTION/UNCANNY/HER_CHART.md, build steps 1
# and 3). (Part of the ui_test_driver.gd chain.)
#
# touch_her_chart: on the 154 m deck the eyes rest on the Matron's oil drum
# long enough for her chart; time is fast-forwarded; no page is in her locker
# a second before the first page is due, and on the clipboard there after it,
# with the drum in it, the date and the phone's time. The tower's clocks show
# the phone's time. The game's own (long) timing is in force.
# touch_her_chart_fast: the START list's HER CHART (FAST): the consent screen
# still first, then the short clock -- the first page within two minutes of
# play, an echo's clip old enough at one -- which no other START entry has.
# touch_mic_consent: the consent screen at first launch, saying what it must;
# NOT NOW leaves the microphone off; AUDIO > MICROPHONE turns it on (the
# phone asked), off again deletes every clip; refused by the phone, the
# setting goes back off; ALLOW turns it on. Headless there is no input
# device: the microphone is never started, and nothing fails.
# touch_voice_echo: a synthetic voice fed through the loudness gate is kept
# as one clip in user://; standing quietly, no echo a second before the clip
# is old enough, and then it plays once: faint, through the pipe's filter,
# from structure near the player out of their view, never captioned, the
# captions on. The clip caps hold.
# touch_room_hearing: her hearing takes the player's room. The microphone on,
# the room's hush is not heard and she sleeps on in her locker; a bark fed
# through the microphone's path wakes her, heard at the player.

const HerChart := preload("res://presentation/uncanny/her_chart.gd")
const UncannyProfile := preload("res://presentation/uncanny/uncanny_profile.gd")
const VoiceKeeper := preload("res://presentation/uncanny/voice_keeper.gd")

# The Matron's throwables (loose_stock.cpp lay_matron_throwables): the oil
# drum by her deck's north rail.
const HER_DRUM_AT := Vector3(-15.0, 154.0, -125.2)
const KIND_DRUM := 3


# Waits for `condition` up to `seconds` of the wall clock: a clip is worked
# out on a worker thread, which a frame counted at a fixed rate can outrun.
func _wait_real(condition: Callable, seconds: float) -> bool:
	var until := Time.get_ticks_msec() + roundi(seconds * 1000.0)
	while not bool(condition.call()):
		if Time.get_ticks_msec() > until:
			return false
		await get_tree().process_frame
	return true


# The loose piece of `kind` nearest `near` (the bridge's own table).
func _stock_entity_near(kind: int, near: Vector3) -> int:
	var stock: PackedInt32Array = _native().get_loose_stock()
	var best := -1
	var best_d := INF
	for i in range(0, stock.size() - 2, 3):
		if stock[i + 1] != kind:
			continue
		var origin: Vector3 = (_native().get_kit_body_transform(stock[i]) as Transform3D).origin
		var d := origin.distance_to(near)
		if d < best_d:
			best_d = d
			best = int(_native().get_kit_body_entity_id(stock[i]))
	return best


# Turns and tilts the view onto a kit body's middle, the way a thumb does.
func _look_at_entity(entity: int) -> void:
	await _aim_at((_native().get_kit_body_transform(_native().get_kit_body_index(entity)) as Transform3D).origin)
	# The eyes settle: a look counts once the view has stopped turning (two of
	# the tracker's samples).
	await _seconds(0.4)


# Turns and tilts the view onto a point from where the eye really is (the
# camera sits off the body's middle, more so looking down): twice, as the
# eye moves with the tilt.
func _aim_at(at: Vector3) -> void:
	for i in 2:
		var eye: Vector3 = (_main._camera as Camera3D).global_position
		await _face(Vector2(at.x - eye.x, at.z - eye.z))
		eye = (_main._camera as Camera3D).global_position
		# Within the view's own limits (main.gd clamps the look's pitch).
		await _tilt(clampf(atan2(at.y - eye.y, Vector2(at.x - eye.x, at.z - eye.z).length()), -1.24, 1.34))
		await _frames(2)


func _touch_her_chart() -> bool:
	var uncanny: Node3D = _main._uncanny
	if uncanny == null:
		return _fail("no uncanny layer")
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var profile: RefCounted = uncanny.profile
	if profile.name != UncannyProfile.NORMAL or float(profile.first_page_s) < 600.0 or \
			float(profile.echo_min_age_s) < 900.0:
		return _fail("the game's own timing is not in force (%s, first page %.0f s, echo %.0f s)" % [
			profile.name, float(profile.first_page_s), float(profile.echo_min_age_s)])
	# The tower's clocks: the phone's local time.
	if uncanny.clocks.clocks.size() < 2:
		return _fail("only %d tower clocks hung" % uncanny.clocks.clocks.size())
	var phone := Time.get_time_dict_from_system()
	var shown: Dictionary = uncanny.clocks.shown_time()
	var phone_minutes := (int(phone["hour"]) % 12) * 60 + int(phone["minute"])
	var shown_minutes := int(shown["hour"]) * 60 + int(shown["minute"])
	if absi(phone_minutes - shown_minutes) > 1 and absi(phone_minutes - shown_minutes) < 719:
		return _fail("the clocks show %d:%02d, the phone %d:%02d" % [int(shown["hour"]), int(shown["minute"]),
			int(phone["hour"]), int(phone["minute"])])
	var drum := _stock_entity_near(KIND_DRUM, HER_DRUM_AT)
	if drum < 0:
		return _fail("no oil drum by her deck's north rail")
	await _look_at_entity(drum)
	var key := "e:%d" % drum
	if uncanny.attention.current_key != key:
		return _fail("the eyes on the drum rest on '%s', not %s" % [uncanny.attention.current_key, key])
	# A long look: past her look_min_s, in real frames.
	await _seconds(float(profile.look_min_s) + 4.0)
	var entry: Dictionary = uncanny.attention.entries.get(key, {})
	if entry.is_empty() or float(entry["total"]) < float(profile.look_min_s):
		return _fail("the look at the drum counted %.1f s" % float(entry.get("total", 0.0)))
	var looked := float(entry["total"])
	if uncanny.pages_placed != 0 or uncanny.pages.page_text("locker") != "":
		return _fail("a page appeared while the eyes were still on the drum")
	# And away, up at the sky: she writes about a look only once it is over
	# and late_s old.
	await _tilt(1.1)
	# Time passes. A second before the first page is due: nothing.
	uncanny.advance(float(profile.first_page_s) - 1.0 - float(uncanny.clock))
	await _frames(4)
	if uncanny.pages_placed != 0 or uncanny.pages.page_text("locker") != "":
		return _fail("a page was placed before its delay (clock %.0f s of %.0f)" % [float(uncanny.clock),
			float(profile.first_page_s)])
	var early_clock := float(uncanny.clock)
	uncanny.advance(2.0)
	if not await _wait_until(func() -> bool: return int(uncanny.pages_placed) == 1, 2.0):
		return _fail("no page in her locker after its delay (clock %.0f s)" % float(uncanny.clock))
	var text: String = uncanny.pages.page_text("locker")
	var lines := text.split("\n")
	var local: Dictionary = uncanny.local_now()
	var expected := "Looked at the oil drum for a long time."
	if not expected in lines:
		return _fail("her page does not say '%s': %s" % [expected, text.replace("\n", " / ")])
	if not ("look:" + key) in uncanny.chart.last_facts:
		return _fail("her page's look is not the drum's (%s)" % str(uncanny.chart.last_facts))
	if lines[0] != HerChart.date_line(local) or not lines[1].begins_with(HerChart.hhmm(local).substr(0, 3)):
		return _fail("her page's date and time are not the phone's: %s" % text.replace("\n", " / "))
	for line in lines:
		if line.begins_with("Fell") or line.begins_with("Died") or line.contains("edge") or line.contains("voice"):
			return _fail("her page says what did not happen: '%s'" % line)
	if (uncanny.pages.clipboard as Node3D).global_position.distance_to(Vector3(11.0, 155.35, -125.84)) > 0.1:
		return _fail("the clipboard is not in her locker")
	# Read: her shut locker's page is not; a page at the feet is, once the
	# eyes rest on it, and goes when they leave.
	if bool(uncanny.reader.reading()):
		return _fail("her page was read through her locker's shut door")
	var feet := _position() - Vector3(0.0, 0.9, 0.0)
	var yaw := float(_main._yaw)
	var ground := Vector3(feet.x - sin(yaw) * 1.0, feet.y, feet.z - cos(yaw) * 1.0)
	uncanny.pages.placed["at_the_feet"] = uncanny.pages.lay_page("TestPage", ground, yaw, text)
	await _aim_at(uncanny.pages.page_sheet("at_the_feet").global_position)
	if not await _wait_until(func() -> bool: return bool(uncanny.reader.reading()), 1.5):
		return _fail("her page at the feet was not read with the eyes on it (reading '%s')" % uncanny.reading_spot)
	if String(uncanny.reader.shown_text) != text:
		return _fail("the page read is not the page written")
	await _tilt(0.3)
	if not await _wait_until(func() -> bool: return not bool(uncanny.reader.visible), 1.5):
		return _fail("her page stayed up after the eyes left it")
	_detail = "drum=%d looked_s=%.1f none_at=%.0f page_at=%.0f lines=%d page='%s'" % [drum, looked, early_clock,
		float(uncanny.chart.last_page_at), lines.size(), text.replace("\n", " / ")]
	return true


func _touch_her_chart_fast() -> bool:
	var uncanny: Node3D = _main._uncanny
	# The consent screen still shows first.
	if not await _wait_until(func() -> bool: return bool(_main._consent_open), 1.0):
		return _fail("the consent screen did not show first")
	if not get_tree().paused or not (_main._consent as Control).visible:
		return _fail("the consent screen is not over a frozen game")
	_click(_main._consent.button_center(&"decline"))
	if not await _wait_until(func() -> bool: return not bool(_main._consent_open), 1.0):
		return _fail("NOT NOW did not close the consent screen")
	if get_tree().paused or bool(_main._settings.microphone):
		return _fail("after NOT NOW: paused %s, microphone %s" % [str(get_tree().paused), str(_main._settings.microphone)])
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	# The START entry: by her locker, on the short clock.
	var index: int = SettingsStore.START_NAMES.find(SettingsStore.START_HER_CHART_FAST)
	if index < 0 or int(SettingsStore.START_SPAWNS[index]) != 11 or _main._settings.start_at != index:
		return _fail("HER CHART (FAST) is not a START entry at the 154 m deck (index %d)" % index)
	if _position().distance_to(Vector3(-10.5, 155.0, -128.2)) > 1.0:
		return _fail("the fast start is not by her locker (at %s)" % str(_position()))
	var profile: RefCounted = uncanny.profile
	if profile.name != UncannyProfile.FAST or float(profile.first_page_s) > 120.0 or \
			float(profile.echo_min_age_s) != 60.0 or float(profile.look_min_s) > 5.0:
		return _fail("the fast profile is not in force (%s, first page %.0f s, echo %.0f s, look %.0f s)" % [
			profile.name, float(profile.first_page_s), float(profile.echo_min_age_s), float(profile.look_min_s)])
	# Only there: every other START entry keeps the game's own timing.
	for i in SettingsStore.START_NAMES.size():
		var expected := UncannyProfile.FAST if i == index else UncannyProfile.NORMAL
		if _main.uncanny_profile_for_start(i) != expected:
			return _fail("START %s maps to the %s profile" % [SettingsStore.START_NAMES[i], _main.uncanny_profile_for_start(i)])
	var normal: RefCounted = UncannyProfile.new()
	normal.configure(UncannyProfile.NORMAL)
	if float(normal.echo_min_age_s) != 900.0 or float(normal.first_page_s) <= float(profile.first_page_s):
		return _fail("the game's own timing was shortened too")
	var drum := _stock_entity_near(KIND_DRUM, HER_DRUM_AT)
	await _look_at_entity(drum)
	await _seconds(float(profile.look_min_s) + 2.0)
	# And away, up at the sky: she writes about a look only once it is over
	# and late_s old.
	await _tilt(1.1)
	# Played on, a second at a time, until her first page: within two minutes.
	var none_before := true
	while float(uncanny.clock) < 150.0 and int(uncanny.pages_placed) == 0:
		uncanny.advance(1.0)
		await _frames(2)
		if int(uncanny.pages_placed) == 0:
			continue
		none_before = float(uncanny.chart.last_page_at) >= float(profile.first_page_s)
	if int(uncanny.pages_placed) != 1:
		var entry: Dictionary = uncanny.attention.entries.get("e:%d" % drum, {})
		var page: Dictionary = uncanny.chart.compose(float(uncanny.clock), uncanny.local_now(), 0)
		return _fail("no page in her locker within %.0f s of play (look %.1f s, last at %.1f, current '%s', page %s, shut %s)" % [
			float(uncanny.clock), float(entry.get("total", 0.0)), float(entry.get("last_at", -1.0)),
			uncanny.attention.current_key, str(page["lines"]), str(uncanny.locker_shut())])
	var at := float(uncanny.chart.last_page_at)
	if not none_before or at > 120.0:
		return _fail("the fast page came at %.0f s (due from %.0f, by 120)" % [at, float(profile.first_page_s)])
	var text: String = uncanny.pages.page_text("locker")
	if not "Looked at the oil drum for a long time." in text.split("\n"):
		return _fail("the fast page does not name the drum: %s" % text.replace("\n", " / "))
	_detail = "start=%d spawn=11 profile=%s page_at=%.0f echo_age=%.0f page='%s'" % [index, profile.name, at,
		float(profile.echo_min_age_s), text.replace("\n", " / ")]
	return true


func _touch_mic_consent() -> bool:
	var voice: Node = _main._uncanny.voice
	if not await _wait_until(func() -> bool: return bool(_main._consent_open), 1.0):
		return _fail("the consent screen did not show at first launch")
	var tick := int(_native().get_tick_index())
	await _seconds(0.4)
	if not get_tree().paused or int(_native().get_tick_index()) != tick:
		return _fail("the game ran behind the consent screen")
	# Honest: what is heard, where it is kept and for how long, nothing
	# uploaded, how to turn it off.
	var said: String = _main._consent.body_text
	for promise in ["usually your own voice", "on this phone only", "deleted within 7 days",
			"NOTHING IS UPLOADED", "nothing else on this phone is read", "PAUSE > AUDIO > MICROPHONE",
			"deletes every clip"]:
		if not said.contains(promise):
			return _fail("the consent screen does not say '%s'" % promise)
	await _pose("consent")
	if bool(_main._settings.microphone) or voice.state != VoiceKeeper.STATE_OFF or voice.listening():
		return _fail("the microphone was on before it was allowed")
	# NOT NOW: off, and not asked again.
	_click(_main._consent.button_center(&"decline"))
	await _frames(3)
	if bool(_main._consent_open) or bool(_main._settings.microphone) or not bool(_main._settings.microphone_asked) \
			or get_tree().paused or voice.state != VoiceKeeper.STATE_OFF:
		return _fail("NOT NOW: open %s, microphone %s, asked %s, paused %s, state %s" % [str(_main._consent_open),
			str(_main._settings.microphone), str(_main._settings.microphone_asked), str(get_tree().paused), voice.state])
	# AUDIO > MICROPHONE on: the phone asked (here, granted); no input device
	# headless, so nothing is started and nothing fails.
	_tap(0, _center(&"pause"))
	await _frames(2)
	_click(_main._pause_menu.side_button_center(&"audio"))
	await _frames(2)
	var toggle: Button = _main._pause_menu._setting_buttons[&"microphone"]
	if toggle.text != "OFF" or not toggle.is_visible_in_tree():
		return _fail("the MICROPHONE setting is not shown off on the AUDIO page")
	_click(_main._pause_menu.setting_center(&"microphone"))
	await _frames(3)
	var headless := AudioServer.get_driver_name() == "Dummy"
	var on_state: StringName = voice.state
	if not bool(_main._settings.microphone) or toggle.text != "ON" or \
			not (on_state in [VoiceKeeper.STATE_LISTENING, VoiceKeeper.STATE_UNAVAILABLE]) or \
			(headless and (on_state != VoiceKeeper.STATE_UNAVAILABLE or voice.find_child("Microphone", false, false) != null)):
		return _fail("MICROPHONE on: setting %s, shown %s, state %s" % [str(_main._settings.microphone), toggle.text,
			on_state])
	# A clip kept while it is on; off deletes it.
	voice.feed_samples(_synthetic_voice(32000.0), 32000.0)
	if not await _wait_real(func() -> bool: return voice.index.size() == 1, 20.0):
		return _fail("no clip kept from the synthetic voice while on (cut %d)" % int(voice.clips_cut))
	var kept_path: String = String(voice.dir).path_join(String(voice.index[0]["file"]))
	if not FileAccess.file_exists(kept_path):
		return _fail("the clip is not in user:// (%s)" % kept_path)
	_click(_main._pause_menu.setting_center(&"microphone"))
	await _frames(3)
	if bool(_main._settings.microphone) or toggle.text != "OFF" or voice.state != VoiceKeeper.STATE_OFF or \
			not voice.index.is_empty() or FileAccess.file_exists(kept_path):
		return _fail("MICROPHONE off: setting %s, state %s, clips %d, file left %s" % [str(_main._settings.microphone),
			voice.state, voice.index.size(), str(FileAccess.file_exists(kept_path))])
	# Refused by the phone: back off, and said so.
	voice.permission_override = "deny"
	_click(_main._pause_menu.setting_center(&"microphone"))
	await _frames(3)
	voice.permission_override = ""
	if bool(_main._settings.microphone) or toggle.text != "OFF" or voice.state != VoiceKeeper.STATE_DENIED:
		return _fail("refused: setting %s, shown %s, state %s" % [str(_main._settings.microphone), toggle.text,
			voice.state])
	_click(_main._pause_menu.side_button_center(&"resume"))
	await _frames(3)
	if get_tree().paused:
		return _fail("RESUME did not resume")
	# ALLOW, as at a first launch: on.
	_main._settings.microphone_asked = false
	_main._show_consent()
	await _frames(2)
	_click(_main._consent.button_center(&"allow"))
	await _frames(3)
	if bool(_main._consent_open) or not bool(_main._settings.microphone) or get_tree().paused or \
			not (voice.state in [VoiceKeeper.STATE_LISTENING, VoiceKeeper.STATE_UNAVAILABLE]):
		return _fail("ALLOW: open %s, microphone %s, paused %s, state %s" % [str(_main._consent_open),
			str(_main._settings.microphone), str(get_tree().paused), voice.state])
	var resumed := int(_native().get_tick_index())
	await _frames(6)
	if int(_native().get_tick_index()) <= resumed:
		return _fail("the climb did not go on after ALLOW")
	_main._settings.microphone = false
	_main._apply_settings()
	_detail = "on_state=%s headless=%s denied_state=denied" % [on_state, str(headless)]
	return true


func _touch_voice_echo() -> bool:
	var uncanny: Node3D = _main._uncanny
	var voice: Node = uncanny.voice
	var profile: RefCounted = uncanny.profile
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	# Consent given, the phone's permission granted (as ALLOW would), and the
	# captions on: the echo must be uncaptioned even so.
	voice.permission_override = "grant"
	_main._settings.microphone = true
	_main._settings.microphone_asked = true
	_main._settings.captions = true
	_main._apply_settings()
	if not (voice.state in [VoiceKeeper.STATE_LISTENING, VoiceKeeper.STATE_UNAVAILABLE]):
		return _fail("the microphone did not come on (state %s)" % voice.state)
	# A synthetic voice through the loudness gate: one clip.
	voice.feed_samples(_synthetic_voice(32000.0), 32000.0)
	if not await _wait_real(func() -> bool: return voice.index.size() == 1, 20.0):
		return _fail("the synthetic voice was not kept as one clip (cut %d, kept %d)" % [int(voice.clips_cut),
			voice.index.size()])
	var clip: Dictionary = voice.index[0]
	if float(clip["seconds"]) < 1.0 or float(clip["seconds"]) > 2.6 or float(clip["voiced"]) < VoiceKeeper.VOICED_MIN:
		return _fail("the clip is %.2f s, voiced %.2f" % [float(clip["seconds"]), float(clip["voiced"])])
	if not FileAccess.file_exists(String(voice.dir).path_join(String(clip["file"]))):
		return _fail("the clip is not in user://")
	# Standing quietly on the deck, past her quiet stretch.
	await _seconds(float(profile.quiet_s) + 1.0)
	# A second short of the clip being old enough: nothing.
	var age := float(voice.now_unix()) - float(clip["at"])
	uncanny.advance(float(profile.echo_min_age_s) - age - 1.0)
	await _frames(5)
	if int(voice.echoes_played) != 0:
		return _fail("an echo played before its delay (the clip %.0f s old)" % (float(voice.now_unix()) - float(clip["at"])))
	var early_age := float(voice.now_unix()) - float(clip["at"])
	uncanny.advance(2.0)
	if not await _wait_until(func() -> bool: return int(voice.echoes_played) == 1, 2.0):
		return _fail("no echo once the clip was old enough (quiet %.1f s, clock %.0f)" % [float(voice._quiet_for),
			float(uncanny.clock)])
	var echo: Dictionary = voice.last_echo
	var player: AudioStreamPlayer3D = voice._echo_player
	# Uncaptioned, the captions on, for the length of it.
	var waited := 0.0
	while waited < float(clip["seconds"]):
		if String(_main._audio.caption) != "" or String(_main._hud.caption) != "":
			return _fail("the echo was captioned: '%s'" % String(_main._hud.caption))
		await get_tree().process_frame
		waited += get_process_delta_time()
	# Faint: its loudest moment at ECHO_PEAK_DB a unit off, farther off still.
	var at_listener := float(echo["peak_db"]) - 20.0 * log(maxf(float(echo["distance"]), player.unit_size) / player.unit_size) / log(10.0)
	if float(echo["peak_db"]) > -12.0 or at_listener > -14.0 or player.unit_size > 2.0:
		return _fail("the echo is not faint (peak %.1f dB, at the listener %.1f dB, unit %.1f m)" % [
			float(echo["peak_db"]), at_listener, player.unit_size])
	# Filtered: through the pipe's bus.
	var bus := AudioServer.get_bus_index(player.bus)
	var kinds := []
	for i in AudioServer.get_bus_effect_count(bus):
		kinds.append(AudioServer.get_bus_effect(bus, i).get_class())
	if player.bus != VoiceKeeper.BUS_ECHO or not ("AudioEffectHighPassFilter" in kinds and "AudioEffectLowPassFilter" in kinds \
			and "AudioEffectDelay" in kinds):
		return _fail("the echo is not filtered (bus %s: %s)" % [player.bus, str(kinds)])
	# From the world, near the player, out of their view.
	var forward := -(_main._camera as Camera3D).global_transform.basis.z
	var to_source: Vector3 = (echo["point"] as Vector3) - _position()
	var flat := Vector2(forward.x, forward.z).normalized()
	if to_source.length() > 9.0 or Vector2(to_source.x, to_source.z).normalized().dot(flat) > VoiceKeeper.ECHO_AHEAD_DOT:
		return _fail("the echo's source is %.1f m off at %s, in view" % [to_source.length(), str(echo["point"])])
	if player.stream == null or not bool(voice.index[0]["echoed"]):
		return _fail("the echo had no clip, or its clip was not marked echoed")
	# Her chart heard it too.
	if uncanny.chart.echoes.size() != 1:
		return _fail("her chart did not note the echo")
	# The caps: count, size and age.
	var small := PackedByteArray()
	small.resize(3200)
	for i in VoiceKeeper.MAX_CLIPS + 4:
		voice._keep_clip(small, 16000.0, float(voice.now_unix()) - 100.0 + float(i), -20.0, 0.5, 0.1)
	if voice.index.size() != VoiceKeeper.MAX_CLIPS or int(voice.bytes_kept()) > VoiceKeeper.MAX_BYTES:
		return _fail("the caps did not hold: %d clips, %d bytes" % [voice.index.size(), int(voice.bytes_kept())])
	voice.time_offset_s += float(profile.clip_max_age_s) + 200.0
	voice._enforce_caps()
	if not voice.index.is_empty():
		return _fail("%d clips outlived their age" % voice.index.size())
	_main._settings.microphone = false
	_main._apply_settings()
	voice.permission_override = ""
	_detail = "clip_s=%.2f voiced=%.2f none_at_age=%.0f echo_age=%.0f peak_db=%.1f at_listener_db=%.1f distance=%.1f bus=%s" % [
		float(clip["seconds"]), float(clip["voiced"]), early_age, float(echo["age_s"]), float(echo["peak_db"]),
		at_listener, float(echo["distance"]), player.bus]
	return true


func _touch_room_hearing() -> bool:
	var voice: Node = _main._uncanny.voice
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	voice.permission_override = "grant"
	_main._settings.microphone = true
	_main._settings.microphone_asked = true
	_main._apply_settings()
	if not (voice.state in [VoiceKeeper.STATE_LISTENING, VoiceKeeper.STATE_UNAVAILABLE]):
		return _fail("the microphone did not come on (state %s)" % voice.state)
	var asleep := int(_native().get_matron_state()["state"])
	# The room's hush, two seconds of it: nothing to hear.
	voice.feed_samples(_room_sound(32000.0, 2.0, 0.0), 32000.0)
	var hush := float(voice.room_loudness())
	await _seconds(2.0)
	var quiet: Dictionary = _native().get_matron_state()
	if hush > 0.0 or float(quiet["noise"]) > 0.0 or int(quiet["state"]) != asleep:
		return _fail("the hush was heard (room %.2f, she heard %.2f, state %d)" % [hush, float(quiet["noise"]),
			int(quiet["state"])])
	# A bark: a third of a second of it, 46 dB over the hush.
	voice.feed_samples(_room_sound(32000.0, 0.5, 0.33), 32000.0)
	var bark := float(voice.room_loudness())
	var heard := 0.0
	var woke := false
	for i in 60:
		await get_tree().physics_frame
		var m: Dictionary = _native().get_matron_state()
		heard = maxf(heard, float(m["noise"]))
		woke = woke or int(m["state"]) != asleep
	voice.feed_samples(_room_sound(32000.0, 0.5, 0.0), 32000.0)
	var after := float(voice.room_loudness())
	_main._settings.microphone = false
	_main._apply_settings()
	voice.permission_override = ""
	if bark < 0.9 or heard <= 0.0 or not woke or after > 0.0:
		return _fail("the bark (room %.2f) was not heard (she heard %.2f, woke %s; after it the room %.2f)" % [bark,
			heard, str(woke), after])
	_detail = "hush_room=%.2f bark_room=%.2f heard=%.2f woke=%s after_room=%.2f" % [hush, bark, heard, str(woke), after]
	return true


# The room as the microphone hears it at `rate`: `seconds` of hush at -62
# dBFS, ending in `bark_s` of a broadband bark at -16 dBFS.
func _room_sound(rate: float, seconds: float, bark_s: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	var rng := RandomNumberGenerator.new()
	rng.seed = 0xBA4C
	var hush := db_to_linear(-62.0)
	var loud := db_to_linear(-16.0) * sqrt(3.0)
	var bark_from := roundi(rate * (seconds - bark_s))
	for i in roundi(rate * seconds):
		out.append(rng.randf_range(-1.0, 1.0) * (loud if i >= bark_from else hush))
	return out


# Renders only (not in CI): one of her exact pages lying on the 154 m deck
# at the player's feet, read the way a player reads it, looking down.
func _look_her_chart() -> bool:
	var uncanny: Node3D = _main._uncanny
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var local: Dictionary = uncanny.local_now()
	var lines := PackedStringArray([HerChart.date_line(local), "02:14. Still awake.",
		"Looked at the yellow oil drum on the 154 m deck for 41 seconds.",
		"Fell 31 m to the 143 m deck at 01:52. Left the body there.", "Afraid of the edge (stands well back)."])
	var feet := _position() - Vector3(0.0, 0.9, 0.0)
	var yaw := float(_main._yaw)
	var ground := Vector3(feet.x - sin(yaw) * 1.0, feet.y, feet.z - cos(yaw) * 1.0)
	ground.y = float(_native().get_surface_below(ground + Vector3(0.0, 1.0, 0.0), 2.0))
	uncanny.pages.placed["look"] = uncanny.pages.lay_page("LookPage", ground, yaw, "\n".join(lines))
	await _aim_at(uncanny.pages.page_sheet("look").global_position)
	await _seconds(0.3)
	uncanny.reader.hide_page()
	uncanny._reading_s = -60.0
	await _frames(20)
	await _pose("her_chart_page")
	# The eyes on it: read up close.
	uncanny._reading_s = 0.0
	await _seconds(1.2)
	await _pose("her_chart_read")
	_detail = "page_at=%s read=%s player=%s eye=%s pitch=%.2f" % [str(ground), str(uncanny.reader.reading()),
		str(_position()), str((_main._camera as Camera3D).global_position), float(_main._pitch)]
	return true


# A synthetic voice at `rate`: room hush, then 1.4 s of a voiced vowel
# (a 140 Hz pulse through two formants, its pitch drifting, in syllables),
# then hush again.
func _synthetic_voice(rate: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	var rng := RandomNumberGenerator.new()
	rng.seed = 0x5EED
	var hush := db_to_linear(-62.0)
	for i in roundi(rate * 0.6):
		out.append(rng.randf_range(-1.0, 1.0) * hush)
	var phase := 0.0
	for i in roundi(rate * 1.4):
		var t := float(i) / rate
		var f0 := 140.0 + 12.0 * sin(TAU * 1.3 * t)
		phase += f0 / rate
		var s := 0.0
		for h in range(1, 9):
			var fh := f0 * float(h)
			var weight := exp(-pow((fh - 700.0) / 300.0, 2.0)) + 0.6 * exp(-pow((fh - 1150.0) / 380.0, 2.0)) + 0.12
			s += weight * sin(TAU * phase * float(h))
		var envelope := pow(absf(sin(PI * t * 2.2)), 0.5) * 0.8 + 0.2
		out.append(s * envelope * 0.06 + rng.randf_range(-1.0, 1.0) * hush)
	for i in roundi(rate * 0.8):
		out.append(rng.randf_range(-1.0, 1.0) * hush)
	return out
