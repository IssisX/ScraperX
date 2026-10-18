extends RefCounted
# Every sound the presentation plays, synthesised once at startup from
# seeded noise, filters and decaying partials -- no audio files ship. The
# seed is fixed, so the bank is identical on every run and every device.
#
# Voiced for a phone speaker first. Those reproduce little under ~300 Hz, so
# every clip carries its identity in the 300 Hz - 6 kHz band -- the grit of a
# boot, the ring of steel, the hiss of air -- and keeps its low thump only
# as weight for headphones. (Measured: the first bank put its energy in
# 50-90 Hz thumps and hum; above 300 Hz a walk peaked at -27.6 dBFS and the
# ambience sat at -48.6 dBFS, which on a phone is next to silence.)
#
# Struck metal is voiced as noise through broad, fast-damped resonances, never
# as sine partials: a pure partial ringing for a quarter second reads as a
# bell ("ting") rather than a boot on plate or a mass landing on steel
# (observed on the first bank's metal steps, grabs, clangs and UI taps).
#
# The human fear voice on large falls (GDD 8.4) is not synthesised here: it is
# generated speech, presentation/audio/voice/, played by the director.

const MIX_RATE := 22050
const STEP_VARIANTS := 4

var clips := {}   # StringName -> Array[AudioStreamWAV] (variants)
var build_msec := 0

var _rng := RandomNumberGenerator.new()


func build() -> void:
	var started := Time.get_ticks_msec()
	_rng.seed = 0x5C4A9E
	clips[&"step_concrete"] = _variants(STEP_VARIANTS, _step_concrete)
	clips[&"step_metal"] = _variants(STEP_VARIANTS, _step_metal)
	clips[&"step_earth"] = _variants(STEP_VARIANTS, _step_earth)
	clips[&"jump"] = _variants(2, _jump)
	clips[&"land_soft"] = _variants(2, _land.bind(0.0))
	clips[&"land_hard"] = _variants(2, _land.bind(1.0))
	clips[&"grab"] = _variants(3, _grab)
	clips[&"scrape"] = _variants(2, _scrape)
	clips[&"rustle"] = _variants(2, _rustle)
	clips[&"chute"] = [_wav(_chute())]
	clips[&"impact_lethal"] = [_wav(_impact_lethal())]
	clips[&"ui_tap"] = [_wav(_tick(1.0))]
	clips[&"ui_back"] = [_wav(_tick(0.7))]
	clips[&"clang"] = _variants(3, _clang)
	clips[&"creak"] = _variants(2, _creak)
	clips[&"bird"] = _variants(3, _bird)
	clips[&"flutter"] = _variants(3, _flutter)
	clips[&"wind_loop"] = [_loop(_wind(4.5), 0.5)]
	clips[&"rush_loop"] = [_loop(_rush(2.4), 0.4)]
	clips[&"hum_loop"] = [_loop(_hum(2.0), 0.3)]
	clips[&"drone_loop"] = [_loop(_drone(3.6), 0.6)]
	clips[&"hiss_loop"] = [_loop(_hiss(1.6), 0.3)]
	clips[&"groan_loop"] = [_loop(_groan(3.2), 0.5)]
	clips[&"motor_loop"] = [_loop(_motor(1.8), 0.3)]
	clips[&"pour_loop"] = [_loop(_pour(1.8), 0.3)]
	# Last, so every clip above draws the same seeded noise it always has.
	clips[&"drip"] = _variants(3, _drip)
	clips[&"arc_loop"] = [_loop(_arc(1.6), 0.3)]
	# The Matron's (sim/matron.hpp), after everything else for the same reason.
	clips[&"locker_knock"] = _variants(3, _locker_knock)
	clips[&"bolt_pop"] = _variants(2, _bolt_pop)
	clips[&"locker_burst"] = [_wav(_locker_burst())]
	clips[&"matron_lullaby_loop"] = [_loop(_lullaby(9.6), 0.4)]
	clips[&"matron_breath"] = _variants(3, _matron_breath)
	clips[&"matron_step"] = _variants(3, _matron_step)
	clips[&"matron_shriek"] = _variants(2, _matron_shriek)
	clips[&"plank_crack"] = _variants(2, _plank_crack)
	clips[&"bone_crack"] = _variants(3, _bone_crack)
	# The forklift's (sim/forklift.hpp), after everything else for the same reason.
	clips[&"engine_loop"] = [_loop(_engine(2.0), 0.3)]
	clips[&"pump_loop"] = [_loop(_pump(1.6), 0.3)]
	clips[&"relief_loop"] = [_loop(_relief(1.2), 0.25)]
	# The ziplines' (sim/zipline.hpp).
	clips[&"zip_loop"] = [_loop(_zip(1.5), 0.3)]
	clips[&"zip_brake_loop"] = [_loop(_zip_brake(1.2), 0.25)]
	# The climber's breath, after everything else for the same reason.
	clips[&"breath_in"] = _variants(3, _breath.bind(true))
	clips[&"breath_out"] = _variants(3, _breath.bind(false))
	# What a struck body sounds like (sim/impacts.hpp ImpactSound, in its
	# order), after everything else for the same reason.
	clips[&"hit_timber"] = _variants(3, _hit_timber)
	clips[&"hit_hollow"] = _variants(3, _hit_hollow)
	clips[&"hit_heavy"] = _variants(3, _hit_heavy)
	clips[&"hit_container"] = _variants(3, _hit_container)
	clips[&"hit_machine"] = _variants(3, _hit_machine)
	# The forklift's and the yard crane's own parts (sim/forklift.hpp,
	# sim/yard_crane.hpp), after everything else for the same reason.
	clips[&"tire_concrete_loop"] = [_loop(_tire(1.6, false), 0.3)]
	clips[&"tire_steel_loop"] = [_loop(_tire(1.6, true), 0.3)]
	clips[&"fork_scrape_loop"] = [_loop(_fork_scrape(1.4), 0.25)]
	clips[&"lift_chain_loop"] = [_loop(_lift_chain(1.25), 0.2)]
	clips[&"beep"] = [_wav(_beep())]
	clips[&"slew_loop"] = [_loop(_slew_motor(2.0), 0.3)]
	clips[&"trolley_loop"] = [_loop(_trolley_run(1.6), 0.3)]
	clips[&"winch_loop"] = [_loop(_winch(2.0), 0.3)]
	clips[&"jingle"] = _variants(3, _jingle)
	build_msec = Time.get_ticks_msec() - started


func pick(name: StringName) -> AudioStreamWAV:
	var list: Array = clips.get(name, [])
	if list.is_empty():
		return null
	return list[_rng.randi_range(0, list.size() - 1)]


# --- clips -------------------------------------------------------------------


# A boot on concrete under a running body: a thud, not a tap. The sole meets
# the slab as a soft, dull slap (no heel click); the weight comes down the
# leg as a thump in the low mids, where a phone speaker still carries it,
# held a little while the knee gives; a little grit under the sole. (The
# first bank's steps were 30-40 ms clicks with 40-45% of their energy above
# 1.5 kHz, a centroid over 2 kHz: tap shoes.)
func _step_concrete() -> PackedFloat32Array:
	var out := _silence(0.24)
	_add_band_noise(out, 0.0, 0.03, 0.5, 0.007, 500.0, 2200.0)
	_add_band_noise(out, 0.0, 0.2, 1.6, 0.045, 200.0, 800.0)
	_add_mode(out, _rng.randf_range(260.0, 340.0), 0.0, 0.2, 0.9, 0.05, 2.5)
	_add_tone(out, _rng.randf_range(70.0, 85.0), 0.0, 0.2, 0.15, 0.05)
	_add_band_noise(out, 0.008, 0.08, 0.22, 0.02, 900.0, 3000.0)
	return out


# A boot on steel plate or grating: the same thud, and the plate answering
# low and dull, its modes choked by the boot on it.
func _step_metal() -> PackedFloat32Array:
	var out := _silence(0.26)
	_add_band_noise(out, 0.0, 0.025, 0.45, 0.006, 600.0, 2500.0)
	_add_band_noise(out, 0.0, 0.2, 1.4, 0.04, 200.0, 750.0)
	var base := _rng.randf_range(230.0, 290.0)
	for mode in [[1.0, 1.0, 0.06], [2.1, 0.55, 0.04], [3.4, 0.25, 0.025]]:
		_add_mode(out, base * mode[0] * _rng.randf_range(0.95, 1.05), 0.0, 0.24, mode[1],
			mode[2], 4.0)
	_add_tone(out, _rng.randf_range(75.0, 90.0), 0.0, 0.2, 0.12, 0.045)
	return out


# Meadow: the thump into soil, the crunch of stalks and grit over it.
func _step_earth() -> PackedFloat32Array:
	var out := _silence(0.22)
	_add_band_noise(out, 0.0, 0.2, 1.3, 0.045, 180.0, 700.0)
	_add_band_noise(out, 0.005, 0.12, 0.55, 0.035, 400.0, 1800.0)
	for i in 5:
		var at := _rng.randf_range(0.0, 0.07)
		_add_band_noise(out, at, 0.006, 0.15, 0.002, 1500.0, 4000.0)
	_add_tone(out, 60.0, 0.0, 0.15, 0.12, 0.04)
	return out


# Push-off: a cloth whoosh that swells and falls, and the shoe leaving.
func _jump() -> PackedFloat32Array:
	var out := _silence(0.24)
	var n := out.size()
	var band := _band_noise_buffer(n, _rng.randf_range(500.0, 700.0),
		_rng.randf_range(2200.0, 3000.0))
	for i in n:
		var t := float(i) / float(n)
		out[i] += band[i] * sin(PI * t) * 1.3
	_add_band_noise(out, 0.0, 0.03, 0.5, 0.008, 800.0, 4000.0)
	_add_tone(out, 70.0, 0.0, 0.05, 0.25, 0.015)
	return out


# Landing: weight through the legs, the soles slapping; `hard` is heavier.
func _land(hard: float) -> PackedFloat32Array:
	var length := lerpf(0.2, 0.34, hard)
	var out := _silence(length)
	_add_band_noise(out, 0.0, 0.03, lerpf(0.8, 1.0, hard), 0.006, 900.0, 6000.0)
	_add_tone(out, lerpf(260.0, 190.0, hard), 0.0, 0.08, lerpf(0.3, 0.4, hard), 0.012)
	_add_band_noise(out, 0.0, length, lerpf(0.6, 0.85, hard), lerpf(0.04, 0.07, hard), 250.0,
		lerpf(2500.0, 1800.0, hard))
	_add_tone(out, lerpf(62.0, 44.0, hard), 0.0, length, 0.5, lerpf(0.035, 0.07, hard))
	return out


# Palms slapping onto a steel lip: skin on metal, a dull knock of the lip,
# the glove's grit as the grip closes.
func _grab() -> PackedFloat32Array:
	var out := _silence(0.2)
	_add_band_noise(out, 0.0, 0.015, 1.0, 0.004, 900.0, 6000.0)
	_add_mode(out, _rng.randf_range(300.0, 380.0), 0.0, 0.12, 0.8, 0.02, 2.5)
	_add_mode(out, _rng.randf_range(700.0, 820.0), 0.0, 0.08, 0.4, 0.012, 2.5)
	_add_band_noise(out, 0.02, 0.16, 0.35, 0.05, 1200.0, 4500.0)
	return out


# Boots and body dragging over an edge.
func _scrape() -> PackedFloat32Array:
	var out := _silence(0.36)
	var n := out.size()
	var band := _band_noise_buffer(n, 700.0, 2600.0)
	var grain := 0.0
	for i in n:
		var t := float(i) / float(n)
		if i % 90 == 0:
			grain = _rng.randf_range(0.4, 1.0)
		out[i] += band[i] * grain * sin(PI * t) * 1.6
	return out


# Clothing and kit shifting as the body folds down or straightens.
func _rustle() -> PackedFloat32Array:
	var out := _silence(0.2)
	var n := out.size()
	var band := _band_noise_buffer(n, 900.0, 4200.0)
	var grain := 0.0
	for i in n:
		var t := float(i) / float(n)
		if i % 60 == 0:
			grain = _rng.randf_range(0.2, 1.0)
		out[i] += band[i] * grain * pow(sin(PI * t), 1.5) * 1.2
	return out


# A flock taking off: a dense clatter of wing claps, each a short soft
# crack, falling away over two thirds of a second.
func _flutter() -> PackedFloat32Array:
	var out := _silence(0.7)
	var claps := _rng.randi_range(14, 22)
	for c in claps:
		var at := _rng.randf_range(0.0, 0.45) * _rng.randf_range(0.4, 1.0)
		_add_band_noise(out, at, 0.035, _rng.randf_range(0.35, 0.8) * (1.0 - at),
			0.012, 500.0, 3200.0)
	return out


# Canopy snapping open: a sharp crack then heavy flapping cloth.
func _chute() -> PackedFloat32Array:
	var out := _silence(0.7)
	_add_band_noise(out, 0.0, 0.02, 1.0, 0.006, 1500.0, 8000.0)
	var n := out.size()
	var band := _band_noise_buffer(n, 250.0, 2200.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		var flap := 0.5 + 0.5 * sin(TAU * 11.0 * t)
		out[i] += band[i] * flap * exp(-t * 3.5) * 1.6
	return out


func _impact_lethal() -> PackedFloat32Array:
	var out := _silence(0.5)
	_add_tone(out, 36.0, 0.0, 0.5, 0.8, 0.12)
	_add_band_noise(out, 0.0, 0.5, 1.0, 0.08, 150.0, 2500.0)
	_add_band_noise(out, 0.01, 0.2, 0.6, 0.03, 1500.0, 6000.0)
	return out


# A switch under the thumb: a dry click with a little body; `pitch` < 1 is
# the softer, lower click of going back.
func _tick(pitch: float) -> PackedFloat32Array:
	var out := _silence(0.05)
	_add_band_noise(out, 0.0, 0.01, 0.8, 0.0015, 1800.0 * pitch, 6000.0 * pitch)
	_add_mode(out, 900.0 * pitch, 0.0, 0.04, 0.5, 0.006, 2.0)
	return out


# Tonnes of steel coming up against a stop: a heavy clunk through the frame.
# The weight is in the low modes and the thump; the upper modes are broad
# and die within a tenth of a second, so it booms rather than rings.
func _clang() -> PackedFloat32Array:
	var out := _silence(0.8)
	_add_band_noise(out, 0.0, 0.03, 1.0, 0.008, 400.0, 5000.0)
	var base := _rng.randf_range(85.0, 110.0)
	for mode in [[1.0, 1.0, 0.18, 6.0], [2.2, 0.8, 0.12, 5.0], [3.9, 0.6, 0.07, 4.0],
			[6.3, 0.4, 0.04, 3.0], [9.8, 0.25, 0.025, 2.5]]:
		_add_mode(out, base * mode[0] * _rng.randf_range(0.96, 1.04), 0.0, 0.8, mode[1], mode[2],
			mode[3])
	_add_tone(out, _rng.randf_range(45.0, 55.0), 0.0, 0.4, 0.6, 0.09)
	_add_band_noise(out, 0.01, 0.4, 0.3, 0.12, 150.0, 900.0)
	return out


# A drop of water landing in a pool: the tick of the strike, then the
# bubble it traps ringing as it shrinks, its pitch rising (1-2 kHz, where a
# phone speaker carries it), and the same again, fainter and duller, off the
# wet walls of a closed space.
func _drip() -> PackedFloat32Array:
	var out := _silence(0.45)
	_add_band_noise(out, 0.0, 0.006, 0.5, 0.0015, 2000.0, 7000.0)
	var f0 := _rng.randf_range(950.0, 1500.0)
	var rise := _rng.randf_range(1.4, 1.8)
	var tau := _rng.randf_range(0.018, 0.03)
	var first := int(0.004 * MIX_RATE)
	var phase := 0.0
	var env := 0.9
	var decay := _decay(tau)
	for i in range(first, out.size()):
		var t := float(i - first) / float(MIX_RATE)
		phase += TAU * f0 * (1.0 + (rise - 1.0) * minf(t / 0.05, 1.0)) / float(MIX_RATE)
		out[i] += sin(phase) * env
		env *= decay
	var echo := int(_rng.randf_range(0.07, 0.11) * MIX_RATE)
	var dull := 0.0
	for i in range(out.size() - 1, echo - 1, -1):
		dull += (out[i - echo] - dull) * 0.35
		out[i] += dull * 0.3
	return out


# An electric arc on steel: a frying crackle -- sparse sharp clicks of
# grit-band noise over a thin hiss -- with the welding set's buzz in its
# upper harmonics (300-500 Hz, where a phone speaker carries it).
func _arc(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var hiss := _band_noise_buffer(out.size(), 2500.0, 7000.0)
	for i in out.size():
		out[i] = hiss[i] * 0.18
	for c in int(length * 220.0):
		var bite := _rng.randf_range(0.3, 0.95)
		_add_band_noise(out, _rng.randf_range(0.0, length - 0.01), 0.004, bite * bite, 0.0012,
			1500.0, 6500.0)
	for harmonic in [3.0, 4.0, 5.0]:
		_add_tone(out, 100.0 * harmonic, 0.0, length, 0.15 / harmonic, 0.0)
	return out


# A loaded hinge turning: stick-slip grains, each a short burst of grit
# through the pin's low, heavily damped body -- a groan, not a whistle.
func _creak() -> PackedFloat32Array:
	var out := _silence(0.6)
	var n := out.size()
	var rate := _rng.randf_range(22.0, 34.0)
	var grit := _band_noise_buffer(n, 250.0, 2200.0)
	var drive := PackedFloat32Array()
	drive.resize(n)
	var phase := 0.0
	var burst := 0.0
	for i in n:
		var t := float(i) / float(n)
		phase += (rate * (0.8 + 0.4 * t)) / float(MIX_RATE)
		if phase >= 1.0:
			phase -= 1.0
			burst = _rng.randf_range(0.6, 1.0)
		drive[i] = grit[i] * burst
		burst *= 0.9985
	var body := _resonate(drive, _rng.randf_range(240.0, 340.0), 3.0)
	var upper := _resonate(drive, _rng.randf_range(620.0, 760.0), 2.5)
	for i in n:
		var t := float(i) / float(n)
		out[i] += (body[i] * 1.4 + upper[i] * 0.6 + drive[i] * 0.25) * sin(PI * t)
	return out


# A small bird in the meadow conifers: two to four quick falling whistles.
func _bird() -> PackedFloat32Array:
	var out := _silence(0.7)
	var at := 0.0
	var notes := _rng.randi_range(2, 4)
	var top := _rng.randf_range(3800.0, 5200.0)
	for k in notes:
		var length := _rng.randf_range(0.05, 0.09)
		var first := int(at * MIX_RATE)
		var last := mini(out.size(), first + int(length * MIX_RATE))
		var phase := 0.0
		for i in range(first, last):
			var u := float(i - first) / float(last - first)
			var freq := lerpf(top, top * 0.62, u) * (1.0 + 0.03 * sin(TAU * 38.0 * u))
			phase += TAU * freq / float(MIX_RATE)
			out[i] += sin(phase) * sin(PI * u) * 0.5
		at += length + _rng.randf_range(0.04, 0.09)
		top *= _rng.randf_range(0.9, 1.08)
	return out


# Mountain wind: broad air with slow gusts, and a thin whistle that wanders
# with them.
func _wind(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var air := _band_noise_buffer(n, 160.0, 1400.0)
	var whistle := _resonant_noise_buffer(n, 780.0, 0.25, 14.0)
	var gust := 0.6
	var gust_target := 0.6
	for i in n:
		if i % 2205 == 0:
			gust_target = _rng.randf_range(0.35, 1.0)
		gust += 0.0004 * (gust_target - gust)
		out[i] = (air[i] * 2.2 + whistle[i] * 0.5 * gust) * gust
	return out


# Air tearing past a falling body: brighter, steadier.
func _rush(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var band := _band_noise_buffer(out.size(), 500.0, 3800.0)
	for i in out.size():
		out[i] = band[i] * 1.4
	return out


# Motor and gearbox at the plant: mains hum for weight, the motor's slot
# whine and bearing noise so it carries on a small speaker.
func _hum(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	for pair in [[50.0, 0.3], [100.0, 0.2], [150.0, 0.1], [300.0, 0.06], [600.0, 0.08],
			[1200.0, 0.05], [1800.0, 0.025]]:
		_add_tone(out, pair[0], 0.0, length, pair[1], 0.0)
	var bearing := _band_noise_buffer(out.size(), 1800.0, 4500.0)
	for i in out.size():
		out[i] += bearing[i] * 0.25
	return out


# The yard itself, heard from anywhere in it: a far-off industrial bed of
# rumble, distant machinery tones beating slowly against each other.
func _drone(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var bed := _band_noise_buffer(n, 90.0, 900.0)
	var swell := 0.7
	var swell_target := 0.7
	for i in n:
		if i % 4410 == 0:
			swell_target = _rng.randf_range(0.55, 1.0)
		swell += 0.00025 * (swell_target - swell)
		out[i] = bed[i] * 1.8 * swell
	# Tones beating slowly: each partial's level swings at its own rate.
	for pair in [[220.0, 0.05, 0.56], [331.0, 0.04, 0.28], [587.0, 0.02, 0.83]]:
		var tone := _silence(length)
		_add_tone(tone, pair[0], 0.0, length, pair[1], 0.0)
		var swing := _silence(length)
		_add_tone(swing, pair[2], 0.0, length, 0.4, 0.0)
		for i in n:
			out[i] += tone[i] * (0.6 + swing[i])
	return out


# Steam venting through the plant's orifice.
func _hiss(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var band := _band_noise_buffer(out.size(), 1800.0, 8500.0)
	for i in out.size():
		out[i] = band[i] * 2.2 * (0.9 + 0.1 * sin(TAU * 7.0 * float(i) / float(MIX_RATE)))
	return out


# Stone pouring from a spout into a bucket: a dense rattle of small stones
# striking stone and steel, each a short bright click, over the low rush of
# the mass going down.
func _pour(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var rush := _band_noise_buffer(out.size(), 180.0, 900.0)
	for i in out.size():
		out[i] = rush[i] * 0.55
	var clicks := int(length * 140.0)
	for c in clicks:
		_add_band_noise(out, _rng.randf_range(0.0, length - 0.02), 0.012,
			_rng.randf_range(0.15, 0.6), 0.003, 1200.0, 5200.0)
	return out


# A heavy body moving on its bearings and guides -- a stair swinging, a
# cage riding: a low rumble that swells and sags, grit in the pins, and now
# and then a knock as the load shifts.
func _groan(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var bed := _band_noise_buffer(n, 70.0, 520.0)
	var grit := _band_noise_buffer(n, 600.0, 2400.0)
	var swell := 0.7
	var swell_target := 0.7
	for i in n:
		if i % 3300 == 0:
			swell_target = _rng.randf_range(0.45, 1.0)
		swell += 0.0006 * (swell_target - swell)
		out[i] = bed[i] * 2.4 * swell + grit[i] * 0.35 * swell * swell
	var at := _rng.randf_range(0.2, 0.6)
	while at < length - 0.3:
		_add_mode(out, _rng.randf_range(140.0, 220.0), at, 0.25, _rng.randf_range(0.3, 0.6), 0.04,
			3.0)
		at += _rng.randf_range(0.5, 1.1)
	return out


# The lift's drive: a geared motor under load.
func _motor(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	for k in range(1, 6):
		_add_tone(out, 110.0 * float(k), 0.0, length, 0.18 / float(k), 0.0)
	var grind := _band_noise_buffer(out.size(), 400.0, 1600.0)
	for i in out.size():
		out[i] += grind[i] * 0.5
	return out


# --- the Matron --------------------------------------------------------------


# Something inside a steel cabinet throwing itself at the door: a dull,
# boxy knock through thin sheet, and the door rattling on its latch.
func _locker_knock() -> PackedFloat32Array:
	var out := _silence(0.55)
	_add_band_noise(out, 0.0, 0.03, 1.0, 0.006, 300.0, 3500.0)
	var base := _rng.randf_range(150.0, 210.0)
	for mode in [[1.0, 0.9, 0.09, 5.0], [2.7, 0.6, 0.06, 4.0], [4.6, 0.4, 0.04, 3.5]]:
		_add_mode(out, base * mode[0], 0.0, 0.5, mode[1], mode[2], mode[3])
	for k in 4:
		_add_band_noise(out, 0.08 + 0.07 * float(k) + _rng.randf_range(0.0, 0.03), 0.04, 0.35 - 0.06 * float(k), 0.01,
			900.0, 3800.0)
	return out


# A rivet letting go: a hard crack and a thin ring, then the shard
# skittering over the deck.
func _bolt_pop() -> PackedFloat32Array:
	var out := _silence(0.5)
	_add_band_noise(out, 0.0, 0.012, 1.0, 0.0025, 1500.0, 8000.0)
	_add_mode(out, _rng.randf_range(1600.0, 2300.0), 0.0, 0.2, 0.7, 0.05, 9.0)
	_add_band_noise(out, 0.04, 0.3, 0.3, 0.08, 800.0, 3000.0)
	for k in 3:
		_add_band_noise(out, 0.12 + 0.07 * float(k), 0.02, 0.3, 0.004, 2000.0, 6500.0)
	return out


# The door torn off its hinges: a huge bang, the tearing of sheet steel and
# the slab skidding away over the deck.
func _locker_burst() -> PackedFloat32Array:
	var out := _silence(1.8)
	_add_band_noise(out, 0.0, 0.05, 1.0, 0.012, 200.0, 6000.0)
	_add_tone(out, 62.0, 0.0, 0.6, 0.7, 0.14)
	for mode in [[1.0, 1.0, 0.25, 5.0], [2.4, 0.8, 0.18, 4.0], [4.1, 0.6, 0.1, 3.0], [7.0, 0.4, 0.06, 2.5]]:
		_add_mode(out, 130.0 * mode[0], 0.0, 1.2, mode[1], mode[2], mode[3])
	_add_band_noise(out, 0.05, 1.0, 0.55, 0.35, 500.0, 3200.0)
	for k in 6:
		_add_mode(out, _rng.randf_range(200.0, 500.0), 0.3 + 0.17 * float(k), 0.3, 0.3, 0.07, 3.0)
	return out


# A music box that has run down: a plain minor phrase in tinkling bells, the
# pitch wobbling as the spring tires, a few notes late and a few flat. This is
# hers; she carries it, and it is how she is heard before she is seen.
func _lullaby(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	# Scale degrees (semitones above the tonic), each in beats.
	var tune := [[0, 1.0], [3, 1.0], [7, 2.0], [5, 1.0], [3, 1.0], [2, 2.0],
		[0, 1.0], [2, 1.0], [3, 1.5], [-2, 0.5], [0, 3.0]]
	var tonic := 523.25
	var beat := 0.52
	var at := 0.2
	for note in tune:
		var detune := 1.0 + _rng.randf_range(-0.012, 0.012)
		var freq: float = tonic * pow(2.0, float(note[0]) / 12.0) * detune
		var late := _rng.randf_range(0.0, 0.05)
		var tau := 0.55 * float(note[1]) ** 0.5
		_add_tone(out, freq, at + late, 1.6, 0.30, tau)
		_add_tone(out, freq * 2.76, at + late, 0.5, 0.08, 0.1)
		_add_tone(out, freq * 5.4, at + late, 0.2, 0.04, 0.04)
		_add_band_noise(out, at + late, 0.012, 0.12, 0.003, 2500.0, 7000.0)
		at += beat * float(note[1])
	# The comb's drag: a faint rasp under it all.
	var rasp := _band_noise_buffer(out.size(), 1200.0, 4200.0)
	for i in out.size():
		var t := float(i) / float(MIX_RATE)
		out[i] += rasp[i] * 0.025 * (0.6 + 0.4 * sin(TAU * 9.0 * t))
	return out


# Her breath: a dry wheeze in, a rattle out, thin and close.
func _matron_breath() -> PackedFloat32Array:
	var out := _silence(1.7)
	var n := out.size()
	var air := _band_noise_buffer(n, 700.0, 3800.0)
	var wheeze := _band_noise_buffer(n, 1700.0, 2600.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		var inhale := sin(PI * clampf(t / 0.7, 0.0, 1.0)) if t < 0.7 else 0.0
		var exhale := sin(PI * clampf((t - 0.85) / 0.8, 0.0, 1.0)) if t > 0.85 else 0.0
		var rattle := 0.5 + 0.5 * sin(TAU * 31.0 * t)
		out[i] = air[i] * (0.9 * inhale + 0.7 * exhale * rattle) + wheeze[i] * 0.8 * inhale
	return out


# The climber's own breath, one half of it: air drawn in through the mouth
# (brighter, rising) or pushed out (darker, falling, with a little voice in
# the throat). The director paces the halves by the effort.
func _breath(inhale: bool) -> PackedFloat32Array:
	var length := 0.34 if inhale else 0.42
	var out := _silence(length)
	var n := out.size()
	var air := _band_noise_buffer(n, 450.0 if inhale else 350.0, 3200.0 if inhale else 2200.0)
	var throat := _band_noise_buffer(n, 900.0, 1500.0)
	var tilt := _rng.randf_range(0.9, 1.1)
	for i in n:
		var u := float(i) / float(n)
		var env := sin(PI * pow(u, 0.6 if inhale else 1.4))
		var lift := (0.6 + 0.4 * u) if inhale else (1.0 - 0.4 * u)
		out[i] = (air[i] * 0.9 * lift + throat[i] * (0.25 if inhale else 0.45)) * env * env * tilt
	return out


# Timber struck (a crate, a pallet, a baulk landing): the dry crack of the
# grain, the boards' own knock in the low mids, quickly choked, and a loose
# board answering a moment after.
func _hit_timber() -> PackedFloat32Array:
	var out := _silence(0.4)
	_add_band_noise(out, 0.0, 0.015, 0.9, 0.003, 1200.0, 5000.0)
	var base := _rng.randf_range(210.0, 290.0)
	for mode in [[1.0, 1.0, 0.045, 5.0], [1.9, 0.7, 0.03, 5.0], [3.1, 0.45, 0.02, 4.0]]:
		_add_mode(out, base * mode[0] * _rng.randf_range(0.95, 1.05), 0.0, 0.3, mode[1], mode[2], mode[3])
	_add_tone(out, _rng.randf_range(70.0, 90.0), 0.0, 0.15, 0.4, 0.03)
	var rattle := _rng.randf_range(0.025, 0.05)
	_add_band_noise(out, rattle, 0.03, 0.35, 0.008, 700.0, 3500.0)
	_add_mode(out, base * 1.4, rattle, 0.12, 0.3, 0.02, 5.0)
	return out


# An empty drum or a gas bottle struck: thin steel round air. The strike's
# click, the shell's broad modes ringing a little, and the air inside
# booming low under them: a bong, not a bell (every mode is noise through a
# resonance, none a pure tone).
func _hit_hollow() -> PackedFloat32Array:
	var out := _silence(0.9)
	_add_band_noise(out, 0.0, 0.012, 0.8, 0.003, 1500.0, 6000.0)
	_add_mode(out, _rng.randf_range(140.0, 185.0), 0.0, 0.9, 1.0, 0.22, 10.0)
	var shell := _rng.randf_range(430.0, 520.0)
	for mode in [[1.0, 0.7, 0.16, 9.0], [1.72, 0.5, 0.12, 9.0], [2.65, 0.35, 0.09, 8.0], [4.1, 0.2, 0.05, 6.0]]:
		_add_mode(out, shell * mode[0] * _rng.randf_range(0.97, 1.03), 0.0, 0.8, mode[1], mode[2], mode[3])
	_add_tone(out, _rng.randf_range(55.0, 70.0), 0.0, 0.2, 0.35, 0.04)
	return out


# Solid steel landing (a beam, plate, a coil, billets): a dull, massive slam.
# The weight is a low thump and low modes that die in a tenth of a second;
# over them the grit of steel on concrete, scraped and crushed.
func _hit_heavy() -> PackedFloat32Array:
	var out := _silence(0.7)
	_add_band_noise(out, 0.0, 0.02, 1.0, 0.005, 800.0, 5000.0)
	_add_tone(out, _rng.randf_range(48.0, 62.0), 0.0, 0.4, 0.9, 0.09)
	var base := _rng.randf_range(115.0, 150.0)
	for mode in [[1.0, 1.0, 0.1, 5.0], [2.3, 0.7, 0.07, 5.0], [4.2, 0.45, 0.04, 4.0]]:
		_add_mode(out, base * mode[0] * _rng.randf_range(0.95, 1.05), 0.0, 0.6, mode[1], mode[2], mode[3])
	_add_band_noise(out, 0.005, 0.25, 0.45, 0.06, 600.0, 4000.0)
	return out


# A shipping container set down or struck: its walls are big corrugated
# panels, so a low boom that hangs, the panels' oil-can thrum over it, and
# the doors and their bars rattling in their keepers after.
func _hit_container() -> PackedFloat32Array:
	var out := _silence(1.4)
	_add_band_noise(out, 0.0, 0.03, 0.8, 0.008, 600.0, 4000.0)
	_add_mode(out, _rng.randf_range(58.0, 72.0), 0.0, 1.4, 1.0, 0.3, 4.0)
	_add_tone(out, _rng.randf_range(40.0, 50.0), 0.0, 0.6, 0.6, 0.14)
	var panel := _rng.randf_range(190.0, 240.0)
	for mode in [[1.0, 0.7, 0.2, 6.0], [1.6, 0.5, 0.15, 6.0], [2.5, 0.35, 0.1, 5.0]]:
		_add_mode(out, panel * mode[0] * _rng.randf_range(0.96, 1.04), 0.0, 1.2, mode[1], mode[2], mode[3])
	var at := 0.05
	for i in 5:
		at += _rng.randf_range(0.04, 0.09)
		_add_band_noise(out, at, 0.05, 0.4 * pow(0.7, i), 0.012, 900.0, 4500.0)
	return out


# A machine struck (a forklift's frame on its stops or a curb, its forks on
# the floor, the crane's hook block swinging into steel): a hard clank
# through a heavy frame, and its chains and loose fittings jingling after.
func _hit_machine() -> PackedFloat32Array:
	var out := _silence(0.7)
	_add_band_noise(out, 0.0, 0.015, 1.0, 0.004, 1500.0, 6000.0)
	var base := _rng.randf_range(220.0, 300.0)
	for mode in [[1.0, 1.0, 0.07, 7.0], [1.8, 0.7, 0.05, 7.0], [3.3, 0.45, 0.035, 6.0]]:
		_add_mode(out, base * mode[0] * _rng.randf_range(0.95, 1.05), 0.0, 0.5, mode[1], mode[2], mode[3])
	_add_tone(out, _rng.randf_range(60.0, 80.0), 0.0, 0.25, 0.5, 0.05)
	var at := 0.03
	for i in 6:
		at += _rng.randf_range(0.02, 0.05)
		_add_band_noise(out, at, 0.025, 0.3 * pow(0.75, i), 0.006, 2500.0, 6500.0)
	return out


# A slipper dragged and a bare knuckle on the deck: a scuff and a dry tap.
func _matron_step() -> PackedFloat32Array:
	var out := _silence(0.22)
	_add_band_noise(out, 0.0, 0.16, 0.8, 0.05, 900.0, 3800.0)
	_add_band_noise(out, 0.0, 0.01, 0.7, 0.003, 2000.0, 6500.0)
	_add_tone(out, _rng.randf_range(160.0, 230.0), 0.0, 0.06, 0.25, 0.012)
	return out


# A thin old throat tearing: a harsh rising shriek, cracking up and down,
# with the grit of a voice that has not been used in years.
func _matron_shriek() -> PackedFloat32Array:
	var out := _silence(1.5)
	var n := out.size()
	var phase := 0.0
	var grit := _band_noise_buffer(n, 1500.0, 5200.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		var u := t / 1.5
		var freq := lerpf(520.0, 1350.0, minf(1.0, u * 1.6)) * (1.0 + 0.05 * sin(TAU * 7.5 * t)) \
			* (1.0 + (0.18 if fmod(t * 11.0, 1.0) > 0.88 else 0.0))
		phase += TAU * freq / float(MIX_RATE)
		var env := sin(PI * clampf(u, 0.0, 1.0)) ** 0.6
		var voice := sin(phase) + 0.5 * sin(2.0 * phase) + 0.3 * sin(3.0 * phase + 1.0)
		out[i] = (voice * 0.35 + grit[i] * 0.9) * env
	return out


# A plate giving way: the bolt's crack, the groan of the tearing, the clang
# of the first strike on the frame.
func _plank_crack() -> PackedFloat32Array:
	var out := _silence(1.0)
	_add_band_noise(out, 0.0, 0.02, 1.0, 0.004, 500.0, 6500.0)
	_add_mode(out, _rng.randf_range(900.0, 1300.0), 0.0, 0.25, 0.6, 0.06, 7.0)
	_add_band_noise(out, 0.02, 0.6, 0.4, 0.25, 200.0, 1400.0)
	_add_mode(out, _rng.randf_range(110.0, 160.0), 0.35, 0.5, 0.7, 0.12, 4.0)
	return out


# Joints giving way, one after another: a run of wet, hollow cracks, each a
# knuckle-pop and a short dull knock, and a creak of something bending that
# should not.
func _bone_crack() -> PackedFloat32Array:
	var out := _silence(0.9)
	var at := 0.0
	for k in 6:
		_add_band_noise(out, at, 0.012, 1.0 - 0.08 * float(k), 0.003, 1200.0, 6000.0)
		_add_mode(out, _rng.randf_range(260.0, 420.0), at, 0.12, 0.7, 0.025, 5.0)
		_add_tone(out, _rng.randf_range(90.0, 140.0), at, 0.08, 0.3, 0.02)
		at += _rng.randf_range(0.05, 0.16)
	_add_band_noise(out, 0.0, 0.8, 0.12, 0.35, 300.0, 1400.0)
	return out


# --- the forklift ------------------------------------------------------------


# Its propane engine at 1500 rpm (played faster or slower for its speed): a
# four-cylinder's 50 firings a second, each a chuff through the muffler and a
# bark of exhaust, one cylinder a little weak -- the lope of a work engine --
# and the valve train ticking over it.
func _engine(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var period := 1.0 / 50.0
	var at := 0.0
	var k := 0
	while at < length - 0.05:
		var bark := _rng.randf_range(0.8, 1.0) * (0.8 if k % 4 == 2 else 1.0)
		_add_mode(out, 95.0, at, 0.05, 0.5 * bark, 0.012, 2.0)
		_add_band_noise(out, at, 0.03, 0.35 * bark, 0.006, 350.0, 2200.0)
		at += period
		k += 1
	var valves := _band_noise_buffer(out.size(), 2500.0, 5500.0)
	for i in out.size():
		out[i] += valves[i] * 0.06
	return out


# Its hydraulic pump, driven off the engine: the whine of its gear teeth
# meshing (12 teeth at 1500 rpm, 300 Hz, and the harmonics that carry on a
# small speaker) over the rush of oil through its ports. The whine is noise
# through narrow resonances that wander a little, as a loaded pump's does,
# not held sines (the bank's rule; the first pump was pure partials).
func _pump(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	for pair in [[300.0, 2.2], [600.0, 1.8], [900.0, 0.9], [1500.0, 0.7]]:
		var whine := _resonant_noise_buffer(n, pair[0], 0.01, 2.0)
		for i in n:
			out[i] += whine[i] * pair[1]
	var flow := _band_noise_buffer(n, 700.0, 3000.0)
	for i in n:
		out[i] += flow[i] * 0.3
	return out


# Its relief valve blowing off: oil forced through a seat that will not hold
# it, a screaming whistle that chatters as the poppet hammers on the seat.
func _relief(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var whistle := _resonant_noise_buffer(n, 2600.0, 0.04, 3.0)
	var rush := _band_noise_buffer(n, 1600.0, 4200.0)
	for i in n:
		var chatter := 0.6 + 0.4 * sin(TAU * 38.0 * float(i) / float(MIX_RATE))
		out[i] = (whistle[i] * 3.0 + rush[i] * 0.8) * chatter
	return out


# --- the forklift's and the yard crane's parts ---------------------------------


# A solid tire rolling at about 3 m/s (played faster or slower for the speed).
# On concrete, the rumble of the tread and the grit it crushes, tick by tick;
# on steel deck plate, the plate drumming under it and the seams it crosses.
func _tire(length: float, steel: bool) -> PackedFloat32Array:
	var out := _silence(length)
	_add_band_noise(out, 0.0, length, 0.55, 0.0, 120.0, 600.0)
	_add_band_noise(out, 0.0, length, 0.12, 0.0, 900.0, 3200.0)
	if steel:
		var n := out.size()
		for pair in [[190.0, 1.4], [420.0, 0.9], [760.0, 0.5]]:
			var drum := _resonant_noise_buffer(n, pair[0], 0.03, 1.0)
			for i in n:
				out[i] += drum[i] * pair[1]
		var at := 0.2
		while at < length - 0.1:
			_add_band_noise(out, at, 0.04, 0.6, 0.012, 300.0, 2500.0)
			at += 0.4
	else:
		for i in 48:
			_add_band_noise(out, _rng.randf_range(0.0, length - 0.01), 0.005, _rng.randf_range(0.08, 0.25),
				0.0015, 1500.0, 5500.0)
	return out


# A fork blade dragged over a floor: steel grinding on it, a harsh squeal
# that wanders as the blade sticks and slips, and grit torn up under it.
func _fork_scrape(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var squeal := _resonant_noise_buffer(n, 2300.0, 0.12, 3.0)
	var grind := _band_noise_buffer(n, 900.0, 5000.0)
	var slip := 0.0
	for i in n:
		if i % 400 == 0:
			slip = _rng.randf_range(0.45, 1.0)
		out[i] = (squeal[i] * 2.5 + grind[i] * 0.9) * slip
	for i in 30:
		_add_band_noise(out, _rng.randf_range(0.0, length - 0.01), 0.006, 0.3, 0.002, 2000.0, 6500.0)
	return out


# The mast's lift chains running over their sheaves at the lift's speed:
# the links clicking over the sheave teeth, twenty-four a second, and the
# carriage's rollers rumbling up the channels.
func _lift_chain(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	_add_band_noise(out, 0.0, length, 0.35, 0.0, 300.0, 900.0)
	var at := 0.0
	while at < length - 0.02:
		_add_band_noise(out, at + _rng.randf_range(0.0, 0.004), 0.012, _rng.randf_range(0.35, 0.55), 0.003,
			2200.0, 6000.0)
		at += 1.0 / 24.0
	return out


# A reverse alarm: an electronic beeper, so a tone (the bank's rule against
# held partials is for struck metal, which this is not): a square-ish note
# at 1.15 kHz, its odd harmonics, eased on and off, a half-second beep.
func _beep() -> PackedFloat32Array:
	var out := _silence(0.5)
	_add_tone(out, 1150.0, 0.0, 0.45, 0.5, 0.0)
	_add_tone(out, 3450.0, 0.0, 0.45, 0.16, 0.0)
	_add_tone(out, 5750.0, 0.0, 0.45, 0.08, 0.0)
	var edge := int(0.012 * MIX_RATE)
	var end := int(0.45 * MIX_RATE)
	for i in edge:
		var w := float(i) / float(edge)
		out[i] *= w
		out[end - 1 - i] *= w
	for i in range(end, out.size()):
		out[i] = 0.0
	return out


# The slewing ring's drives turning the jib: the electric motors' hum and
# whine and the planetary gears' mesh above them, over the ring's grind.
func _slew_motor(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	for pair in [[100.0, 1.6], [700.0, 1.8], [1400.0, 0.9], [2100.0, 0.4]]:
		var tone := _resonant_noise_buffer(n, pair[0], 0.005, 1.0)
		for i in n:
			out[i] += tone[i] * pair[1]
	_add_band_noise(out, 0.0, length, 0.25, 0.0, 300.0, 1800.0)
	return out


# The trolley's steel wheels running along the jib's chords: a rolling
# rumble, and the wheels knocking over the chords' joints in pairs.
func _trolley_run(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	_add_band_noise(out, 0.0, length, 0.5, 0.0, 150.0, 700.0)
	_add_band_noise(out, 0.0, length, 0.08, 0.0, 2500.0, 4500.0)
	var at := 0.3
	while at < length - 0.2:
		for k in 2:
			_add_band_noise(out, at + 0.12 * k, 0.03, 0.55, 0.01, 600.0, 3000.0)
			_add_mode(out, _rng.randf_range(380.0, 460.0), at + 0.12 * k, 0.08, 0.4, 0.02, 6.0)
		at += 0.8
	return out


# The hoist's winch hauling: its motor's whine rising through the gears,
# and the rope throbbing as each wrap lays onto the drum.
func _winch(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var whine := _resonant_noise_buffer(n, 520.0, 0.008, 1.0)
	var whine2 := _resonant_noise_buffer(n, 1040.0, 0.008, 1.0)
	var gears := _band_noise_buffer(n, 150.0, 650.0)
	for i in n:
		var wrap := 0.75 + 0.25 * sin(TAU * 6.0 * float(i) / float(MIX_RATE))
		out[i] = (whine[i] * 2.0 + whine2[i] * 1.0 + gears[i] * 0.5) * wrap
	return out


# The hook's chain sling and its master ring jingling as the block swings
# or is jerked: a few small links knocking, each choked at once.
func _jingle() -> PackedFloat32Array:
	var out := _silence(0.25)
	var at := 0.0
	for i in _rng.randi_range(4, 7):
		_add_mode(out, _rng.randf_range(2800.0, 6500.0), at, 0.08, _rng.randf_range(0.3, 0.6), 0.015, 10.0)
		_add_band_noise(out, at, 0.006, 0.25, 0.002, 2500.0, 7000.0)
		at += _rng.randf_range(0.015, 0.04)
	return out


# --- the ziplines ------------------------------------------------------------


# A trolley's two sheaves rolling a laid steel cable at 12 m/s (played
# faster or slower for the speed): the wheels bumping over the cable's
# strands, a buzz near 600 Hz and its overtones, the cable's own hum under
# it, and the air whispering past the wire.
func _zip(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	for pair in [[600.0, 0.10], [1200.0, 0.06], [1800.0, 0.03]]:
		_add_tone(out, pair[0], 0.0, length, pair[1], 0.0)
	_add_tone(out, 140.0, 0.0, length, 0.08, 0.0)
	var n := out.size()
	var air := _band_noise_buffer(n, 900.0, 4000.0)
	for i in n:
		var flutter := 0.8 + 0.2 * sin(TAU * 7.0 * float(i) / float(MIX_RATE))
		out[i] = out[i] * flutter + air[i] * 0.25
	return out


# The brake block's clamp dragged down the cable by the trolley: a squeal of
# rubber on wire that wavers as the clamp grabs and slips, over the rumble
# of the block hammering along the strands.
func _zip_brake(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var squeal := _resonant_noise_buffer(n, 1900.0, 0.06, 2.5)
	var rumble := _band_noise_buffer(n, 120.0, 500.0)
	for i in n:
		var grab := 0.55 + 0.45 * sin(TAU * 23.0 * float(i) / float(MIX_RATE))
		out[i] = squeal[i] * 2.2 * grab + rumble[i] * 0.9
	return out


# --- primitives --------------------------------------------------------------


func _variants(count: int, maker: Callable) -> Array:
	var list := []
	for i in count:
		list.append(_wav(maker.call()))
	return list


func _silence(seconds: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(int(seconds * MIX_RATE))
	return out


func _alpha(cutoff_hz: float) -> float:
	return 1.0 - exp(-TAU * cutoff_hz / float(MIX_RATE))


# Low-passed noise from `start` for `length` seconds, exponential decay `tau`.
func _add_noise(out: PackedFloat32Array, start: float, length: float, gain: float, tau: float,
		cutoff_hz: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	var a := _alpha(cutoff_hz)
	var lp := 0.0
	var env := gain * 2.0
	var decay := _decay(tau)
	for i in range(first, last):
		lp += a * (_rng.randf_range(-1.0, 1.0) - lp)
		out[i] += lp * env
		env *= decay


# Noise between two corner frequencies (a low-pass minus a lower low-pass,
# each two poles), decaying with `tau`.
func _add_band_noise(out: PackedFloat32Array, start: float, length: float, gain: float,
		tau: float, low_hz: float, high_hz: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	if last <= first:
		return
	var band := _band_noise_buffer(last - first, low_hz, high_hz)
	var env := gain * 2.0
	var decay := _decay(tau)
	for i in range(first, last):
		out[i] += band[i - first] * env
		env *= decay


func _band_noise_buffer(count: int, low_hz: float, high_hz: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(count)
	var a_hi := _alpha(high_hz)
	var a_lo := _alpha(low_hz)
	var h1 := 0.0
	var h2 := 0.0
	var l1 := 0.0
	var l2 := 0.0
	for i in count:
		var x := _rng.randf_range(-1.0, 1.0)
		h1 += a_hi * (x - h1)
		h2 += a_hi * (h1 - h2)
		l1 += a_lo * (x - l1)
		l2 += a_lo * (l1 - l2)
		out[i] = h2 - l2
	return out


# Noise through a narrow resonance whose centre drifts by +/- `wander` of
# itself at `drift_hz` cycles over the buffer's own length.
func _resonant_noise_buffer(count: int, center_hz: float, wander: float,
		drift_cycles: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(count)
	var low := 0.0
	var band := 0.0
	var q := 0.08
	for i in count:
		var u := float(i) / float(count)
		var f := center_hz * (1.0 + wander * sin(TAU * drift_cycles * u))
		var k := 2.0 * sin(PI * f / float(MIX_RATE))
		var x := _rng.randf_range(-1.0, 1.0)
		low += k * band
		var high := x - low - q * band
		band += k * high
		out[i] = band * 0.2
	return out


# A sine partial; tau 0 holds it at constant level. The sine runs as the
# two-term recurrence s[n+1] = 2 cos(w) s[n] - s[n-1] and the envelope as a
# running product: no sin or exp per sample, which is most of the bank's
# build time in GDScript.
func _add_tone(out: PackedFloat32Array, freq: float, start: float, length: float, gain: float,
		tau: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	var step := TAU * freq / float(MIX_RATE)
	var coupling := 2.0 * cos(step)
	var previous := -sin(step)
	var current := 0.0
	var env := gain
	var decay := _decay(tau)
	for i in range(first, last):
		out[i] += current * env
		var next := coupling * current - previous
		previous = current
		current = next
		env *= decay


# A struck mode: white noise through a band-pass resonance at `freq` of
# quality `q` (bandwidth freq / q), under an exponential decay `tau`. A low q
# and a short tau give a knock with a pitch colour, not a ringing tone.
func _add_mode(out: PackedFloat32Array, freq: float, start: float, length: float, gain: float,
		tau: float, q: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	if last <= first:
		return
	var noise := PackedFloat32Array()
	noise.resize(last - first)
	for i in noise.size():
		noise[i] = _rng.randf_range(-1.0, 1.0)
	var band := _resonate(noise, freq, q)
	var env := gain * 2.0
	var decay := _decay(tau)
	for i in range(first, last):
		out[i] += band[i - first] * env
		env *= decay


# `input` through a two-pole band-pass (RBJ, 0 dB peak) at `freq`, quality q.
func _resonate(input: PackedFloat32Array, freq: float, q: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(input.size())
	var w := TAU * minf(freq, 0.45 * MIX_RATE) / float(MIX_RATE)
	var alpha := sin(w) / (2.0 * q)
	var a0 := 1.0 + alpha
	var b0 := alpha / a0
	var a1 := -2.0 * cos(w) / a0
	var a2 := (1.0 - alpha) / a0
	var x1 := 0.0
	var x2 := 0.0
	var y1 := 0.0
	var y2 := 0.0
	for i in input.size():
		var x := input[i]
		var y := b0 * x - b0 * x2 - a1 * y1 - a2 * y2
		x2 = x1
		x1 = x
		y2 = y1
		y1 = y
		out[i] = y
	return out


# Per-sample factor of an exponential decay with time constant `tau`
# seconds; tau 0 means no decay.
func _decay(tau: float) -> float:
	return 1.0 if tau <= 0.0 else exp(-1.0 / (tau * float(MIX_RATE)))


func _wav(samples: PackedFloat32Array, loop_end: int = -1) -> AudioStreamWAV:
	var peak := 0.0
	for x in samples:
		peak = maxf(peak, absf(x))
	var scale := 0.9 / peak if peak > 0.9 else 1.0
	# A one-shot cut off mid-ring clicks (observed: grab ended at 0.73 of full
	# scale); the last 8 ms ramp it to silence. Loops are left whole.
	if loop_end <= 0:
		var ramp := mini(int(0.008 * MIX_RATE), samples.size() / 4)
		for k in ramp:
			samples[samples.size() - 1 - k] *= float(k) / float(ramp)
	var data := PackedByteArray()
	data.resize(samples.size() * 2)
	for i in samples.size():
		data.encode_s16(i * 2, int(clampf(samples[i] * scale, -1.0, 1.0) * 32767.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = MIX_RATE
	wav.stereo = false
	wav.data = data
	if loop_end > 0:
		wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
		wav.loop_begin = 0
		wav.loop_end = loop_end
	return wav


# Seamless loop: the tail is crossfaded into the head, then cut off.
func _loop(samples: PackedFloat32Array, fade_seconds: float) -> AudioStreamWAV:
	var fade := int(fade_seconds * MIX_RATE)
	var body := samples.size() - fade
	var out := samples.slice(0, body)
	for i in fade:
		var w := float(i) / float(fade)
		out[i] = out[i] * w + samples[body + i] * (1.0 - w)
	return _wav(out, out.size())
