extends RefCounted

# Master-mix capture and measured ambience/footstep levels.
const Harness := preload("res://presentation/ui/test_scenarios/ui_test_harness.gd")

var h: Harness


func _init(harness: Harness) -> void:
	h = harness


# The sound a player hears, measured where it leaves Master (after the
# limiter) and in the band a phone speaker reproduces, above 300 Hz. A
# headless run's Dummy driver never mixes, so this runs under Movie Maker
# (--write-movie), which mixes that same driver frame by frame. Standing
# still, the yard's ambience must be there; walking, the footsteps must
# stand out of it; nothing may clip.
func _audio_mix() -> bool:
	if AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty():
		return h._fail("no mixing audio driver; run under --write-movie")
	var capture := AudioEffectCapture.new()
	capture.buffer_length = 1.0
	AudioServer.add_bus_effect(AudioServer.get_bus_index(&"Master"), capture)
	var ready: bool = await h._wait_until(func() -> bool: return bool(h._main._audio._bank_ready), 5.0)
	if not ready:
		return h._fail("the sound bank never finished building")
	await h._wait_until(func() -> bool: return bool(h._ctx()["grounded"]), 2.0)
	# The ambience fades in from silence over its first seconds.
	await _capture(capture, 2.5)
	var standing := await _capture(capture, 1.0)
	var steps_before: int = h._main._audio.steps
	h._key(KEY_W, true)
	var walking := await _capture(capture, 1.4)
	h._key(KEY_W, false)
	var steps: int = h._main._audio.steps - steps_before
	var bed := _band_levels(standing)
	var walk := _band_levels(walking)
	var peak := 0.0
	for x in standing + walking:
		peak = maxf(peak, absf(x))
	var peak_db := linear_to_db(maxf(peak, 1.0e-9))
	h._detail = "bed_db=%.1f steps_db=%.1f peak_db=%.1f steps=%d" % [bed.x, walk.y, peak_db, steps]
	h._main._audio.quiesce()
	await h._frames(3)
	if steps < 3:
		return h._fail("walking 1.4 s played %d footsteps" % steps)
	if bed.x < -40.0:
		return h._fail("standing still, the mix above 300 Hz sits at %.1f dBFS" % bed.x)
	if walk.y < bed.x + 4.0:
		return h._fail("footsteps (%.1f dBFS) do not stand out of the ambience (%.1f)" % [walk.y, bed.x])
	if peak_db > -0.5:
		return h._fail("the mix clips: sample peak %.2f dBFS" % peak_db)
	return true


# Mono samples leaving Master for `seconds` of game time.
func _capture(capture: AudioEffectCapture, seconds: float) -> PackedFloat32Array:
	var mono := PackedFloat32Array()
	var waited := 0.0
	while waited < seconds:
		await h.get_tree().process_frame
		waited += h.get_process_delta_time()
		var available := capture.get_frames_available()
		if available > 0:
			for frame in capture.get_buffer(available):
				mono.append((frame.x + frame.y) * 0.5)
	return mono


# 50 ms RMS levels above 300 Hz (two cascaded one-pole high-passes), as
# (median, loudest) in dBFS.
func _band_levels(mono: PackedFloat32Array) -> Vector2:
	var rate := AudioServer.get_mix_rate()
	var a := exp(-TAU * 300.0 / rate)
	var x1 := 0.0
	var y1 := 0.0
	var x2 := 0.0
	var y2 := 0.0
	var window := int(rate * 0.05)
	var levels: Array[float] = []
	var energy := 0.0
	var count := 0
	for x in mono:
		var h1 := a * (y1 + x - x1)
		x1 = x
		y1 = h1
		var h2 := a * (y2 + h1 - x2)
		x2 = h1
		y2 = h2
		energy += h2 * h2
		count += 1
		if count == window:
			levels.append(linear_to_db(maxf(sqrt(energy / float(window)), 1.0e-9)))
			energy = 0.0
			count = 0
	if levels.is_empty():
		return Vector2(-200.0, -200.0)
	levels.sort()
	return Vector2(levels[levels.size() / 2], levels[levels.size() - 1])
