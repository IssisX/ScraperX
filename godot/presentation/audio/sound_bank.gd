extends RefCounted
# Every sound the presentation plays, synthesized once at startup from
# seeded noise, filters and decaying partials. The seed is fixed, so the bank
# is identical on every run and every device. Human sounds are the exception
# (below): they are recordings.
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
# The human fear voice on large falls (GDD 8.4) is not synthesized here: it is
# generated speech, presentation/audio/voice/, played by the director. Nor
# are the Matron's voice and what her hands do to a body: those are real
# recordings, presentation/audio/matron/, read by build_her.

const MIX_RATE := 22050
const STEP_VARIANTS := 4

var clips := {}   # StringName -> Array[AudioStream] (variants)
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
	clips[&"radio_open"] = [_wav(_radio_open())]
	clips[&"radio_close"] = [_wav(_radio_close())]
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
	# The Matron's locker and catwalk (sim/matron.hpp), after everything else
	# for the same reason. Her own body is her own bank (build_her).
	clips[&"locker_knock"] = _variants(3, _locker_knock)
	clips[&"bolt_pop"] = _variants(2, _bolt_pop)
	clips[&"locker_burst"] = [_wav(_locker_burst())]
	clips[&"plank_crack"] = _variants(2, _plank_crack)
	# A body's bones giving way: the climber's.
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
	clips[&"magnet_loop"] = [_loop(_magnet_hum(1.6), 0.3)]
	# The chiller drop (AS-007 E), made last so every clip before it is as it was.
	clips[&"air_roar_loop"] = [_loop(_air_roar(1.8), 0.3)]
	clips[&"vent_blast_loop"] = [_loop(_vent_blast(1.6), 0.3)]
	clips[&"catch_bang"] = [_wav(_catch_bang())]
	clips[&"seat_thump"] = [_wav(_seat_thump())]
	# Water (sim physics_world_water.cpp), made last so every clip before it
	# is as it was: the body going in, and a stroke through it.
	clips[&"splash"] = _variants(2, _splash)
	clips[&"swim_stroke"] = _variants(3, _swim_stroke)
	# Her waking dread, made last so every clip before it is as it was.
	clips[&"matron_wake"] = _variants(4, _matron_wake)
	build_msec = Time.get_ticks_msec() - started


# The Matron's own bank, made on a second instance after the world's. She
# sounds like a body: breath, throat, mouth, weight and hands, and no tune,
# bell or chord. Her stage is in which of them plays (audio_director.gd
# matron_layers): breath a pest, a growl dangerous, the rage growl over a
# rough bed fully changed.
#
# Her voice, her sniff, and what her hands
# do to a body, are real recorded CC0 takes in HER_DIR (sources and exact
# recording intervals in CREDITS.md). Only nonhuman beds, footfalls and
# joints remain made here. Her synthesized mouth loop stays retired.
const HER_DIR := "res://presentation/audio/matron/"
# Her recordings by clip name: how many takes, each <name>_<take>.ogg. A name
# ending _loop plays as a loop (each file's end is crossfaded into its head).
# The owner's pick (2026-10-27) is take 0 (the shriek's and the rip's two cuts:
# takes 0 and 1); the rest, added 2026-10-29, are the other takes auditioned
# in that round, so no recorded family of hers has fewer than 3.
const HER_RECORDED := {&"matron_growl_loop": 3, &"matron_growl_rage_loop": 3, &"matron_strain_loop": 3,
	&"matron_breath": 3, &"matron_shriek": 6, &"matron_laugh": 3, &"matron_effort": 3, &"matron_hit": 3,
	&"matron_swallow": 3, &"matron_rip": 6, &"matron_plant": 3,
	&"matron_sniff": 4}


func build_her() -> void:
	var started := Time.get_ticks_msec()
	# Ogg pages, read whole and decoded as they play (the climber's voice
	# lines are loaded the same way, audio_director.gd _load_voice).
	for clip in HER_RECORDED:
		var takes: Array = []
		for take in int(HER_RECORDED[clip]):
			var path := "%s%s_%d.ogg" % [HER_DIR, clip, take]
			var stream := AudioStreamOggVorbis.load_from_file(path)
			if stream == null:
				push_error("matron bank: cannot read %s" % path)
				continue
			stream.loop = String(clip).ends_with("_loop")
			takes.append(stream)
		if not takes.is_empty():
			clips[clip] = takes
	_rng.seed = 0x4D4154
	var bed := _her_bed(3.4)
	clips[&"matron_bed_loop"] = [_loop(bed, 0.4)]
	_rough(bed, 38.0, 0.6)
	clips[&"matron_bed_rough_loop"] = [_loop(bed, 0.4)]
	clips[&"matron_step"] = _variants(3, _her_step)
	clips[&"matron_joints"] = _variants(3, _her_joints)
	build_msec = Time.get_ticks_msec() - started


# One of a clip's variants (the world's are all made here; hers are made here
# or read from her recordings).
func pick(name: StringName) -> AudioStream:
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


# A handheld radio's squelch opening: the channel's click, then a burst of
# band-limited hiss that dies down as the carrier takes hold.
func _radio_open() -> PackedFloat32Array:
	var out := _silence(0.45)
	_add_band_noise(out, 0.0, 0.015, 0.9, 0.003, 900.0, 6000.0)
	_add_mode(out, 1500.0, 0.0, 0.03, 0.35, 0.004, 2.0)
	_add_band_noise(out, 0.01, 0.4, 0.55, 0.11, 1200.0, 3800.0)
	return out


# The channel closing: a short click with a wisp of hiss after it.
func _radio_close() -> PackedFloat32Array:
	var out := _silence(0.18)
	_add_band_noise(out, 0.0, 0.012, 0.8, 0.0025, 900.0, 5000.0)
	_add_band_noise(out, 0.01, 0.15, 0.25, 0.04, 1500.0, 3500.0)
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


# Air driven through the chiller drop's throttle under the car's floor: a
# low roar with a throaty band where a phone's speaker carries it, the rush of
# the stream over it, beating as the flow tears at the orifice's edge.
func _air_roar(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var throat := _band_noise_buffer(n, 220.0, 900.0)
	var rush := _band_noise_buffer(n, 900.0, 3200.0)
	for i in n:
		var beat := 0.8 + 0.2 * sin(TAU * 11.0 * float(i) / float(MIX_RATE))
		out[i] = (throat[i] * 2.0 + rush[i] * 0.7) * beat
	_add_tone(out, 70.0, 0.0, length, 0.15, 0.0)
	return out


# The vent collar's blast at the shaft's mouth: broadband hiss of air leaving
# the slots, spitting water.
func _vent_blast(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var hiss := _band_noise_buffer(n, 1200.0, 8000.0)
	var body := _band_noise_buffer(n, 300.0, 1400.0)
	for i in n:
		out[i] = hiss[i] * 1.8 + body[i] * 0.9
	for c in int(length * 40.0):
		_add_band_noise(out, _rng.randf_range(0.0, length - 0.02), 0.01, _rng.randf_range(0.1, 0.35), 0.003,
			2500.0, 7000.0)
	return out


# The chiller's catch letting go overhead: a heavy pin snapping out of its
# keeper, a bang through the frame, and the frame ringing dully after it.
func _catch_bang() -> PackedFloat32Array:
	var out := _silence(0.8)
	_add_band_noise(out, 0.0, 0.03, 1.0, 0.006, 700.0, 5000.0)
	_add_tone(out, 55.0, 0.0, 0.5, 0.8, 0.12)
	var base := _rng.randf_range(150.0, 180.0)
	for mode in [[1.0, 1.0, 0.18, 4.0], [2.2, 0.6, 0.12, 4.0], [3.7, 0.35, 0.07, 3.5]]:
		_add_mode(out, base * mode[0], 0.0, 0.7, mode[1], mode[2], mode[3])
	_add_band_noise(out, 0.01, 0.3, 0.4, 0.08, 400.0, 2500.0)
	return out


# The 5 t chiller eased onto its stop: a deep, soft thump and the duct
# booming low around it.
func _seat_thump() -> PackedFloat32Array:
	var out := _silence(1.2)
	_add_tone(out, 42.0, 0.0, 0.9, 1.0, 0.25)
	_add_mode(out, _rng.randf_range(90.0, 110.0), 0.0, 1.1, 0.7, 0.35, 5.0)
	_add_band_noise(out, 0.0, 0.4, 0.5, 0.12, 250.0, 1200.0)
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


# Her waking: dread from wrongness, not loudness. A beat of near-silence; the
# tower's steel taking a load it should not (a stick-slip groan, the
# friction pulsing at an uneven rate, under a slow resonant strain); a sub
# swell at 28 Hz with its 56/84 Hz partials, so a phone still carries its
# weight in harmonics; a wet, uneven intake of breath with saliva ticks, mixed
# dry so it reads as close; and two tones 6 Hz apart under a faint tritone,
# beating like something out of tune. Every variant draws new timings, pitches
# and breath shapes, so no two wakes are alike.
func _matron_wake() -> PackedFloat32Array:
	var length := 3.4
	var out := _silence(length)
	var n := out.size()
	var hush := 0.3 + _rng.randf_range(0.0, 0.15)
	var first := int(hush * MIX_RATE)
	# Steel under strain: a groan sliding down, its friction pulsing unevenly.
	var strain := _resonant_noise_buffer(n, _rng.randf_range(170.0, 240.0), 0.25, _rng.randf_range(0.6, 1.3))
	var grind := _band_noise_buffer(n, 400.0, 2400.0)
	var g_from := _rng.randf_range(190.0, 240.0)
	var g_to := g_from * _rng.randf_range(0.68, 0.8)
	var phase := 0.0
	var slip := 0.0
	var slip_rate := _rng.randf_range(8.0, 13.0)
	for i in range(first, n):
		var u := float(i - first) / float(n - first)
		var swell := sin(PI * minf(1.0, u * 1.15)) ** 1.4
		phase += TAU * lerpf(g_from, g_to, u) / float(MIX_RATE)
		slip += slip_rate * (1.0 + 0.6 * sin(TAU * 0.7 * u * length)) / float(MIX_RATE)
		var catch := 1.0 if fmod(slip, 1.0) < 0.35 else 0.25
		var saw := fmod(phase / TAU, 1.0) * 2.0 - 1.0
		out[i] += (saw * 0.22 * catch + grind[i] * 0.35 * catch + strain[i] * 2.2) * swell
	# The sub swell and its partials.
	var sub := _silence(length)
	_add_tone(sub, 28.0, hush, length - hush, 0.5, 0.0)
	_add_tone(sub, 56.0, hush, length - hush, 0.22, 0.0)
	_add_tone(sub, 84.0, hush, length - hush, 0.12, 0.0)
	# Out of tune: two tones beating, a tritone over them.
	var root := _rng.randf_range(290.0, 330.0)
	_add_tone(sub, root, hush + 0.4, length - hush - 0.4, 0.07, 0.0)
	_add_tone(sub, root + _rng.randf_range(5.0, 7.0), hush + 0.4, length - hush - 0.4, 0.07, 0.0)
	_add_tone(sub, root * 1.4142, hush + 0.9, length - hush - 0.9, 0.035, 0.0)
	for i in range(first, n):
		var u := float(i - first) / float(n - first)
		out[i] += sub[i] * minf(1.0, u * 2.5) * (1.0 - u) ** 0.5
	# The breath, too close: an uneven wet intake that catches twice.
	var b_at := hush + _rng.randf_range(0.9, 1.4)
	var b_len := _rng.randf_range(1.1, 1.5)
	var breath := _resonant_noise_buffer(int(b_len * MIX_RATE), _rng.randf_range(850.0, 1100.0), 0.35, 1.7)
	var air := _band_noise_buffer(breath.size(), 1800.0, 6000.0)
	var b0 := int(b_at * MIX_RATE)
	var catch1 := _rng.randf_range(0.3, 0.45)
	var catch2 := _rng.randf_range(0.6, 0.75)
	for j in breath.size():
		var i := b0 + j
		if i >= n:
			break
		var v := float(j) / float(breath.size())
		var env := sin(PI * v) ** 0.8
		if absf(v - catch1) < 0.03 or absf(v - catch2) < 0.025:
			env *= 0.15
		out[i] += (breath[j] * 3.0 + air[j] * 0.25) * env
	for k in _rng.randi_range(4, 7):
		_add_band_noise(out, b_at + _rng.randf_range(0.0, b_len), 0.006, 0.25, 0.002, 3000.0, 7000.0)
	var peak := 0.0
	for i in n:
		peak = maxf(peak, absf(out[i]))
	if peak > 0.0:
		for i in n:
			out[i] *= 0.9 / peak
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


# --- the Matron's own (build_her) ------------------------------------------------


# Far: the pressure she puts on the air. Saws at 146.67 and 151 Hz (4.33 Hz
# apart) and 207.33 Hz (a tritone up), each a whole number of cycles in the
# 3.0 s loop, driven (SAT 3), held to 450-2200 Hz, under air 400-1400 Hz
# that swells twice a loop like slow breathing.
func _her_bed(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var saws := PackedFloat32Array()
	saws.resize(n)
	var pa := _rng.randf()
	var pb := _rng.randf()
	var pc := _rng.randf()
	var ia := (440.0 / 3.0) / float(MIX_RATE)
	var ib := 151.0 / float(MIX_RATE)
	var ic := (622.0 / 3.0) / float(MIX_RATE)
	for i in n:
		pa = fmod(pa + ia, 1.0)
		pb = fmod(pb + ib, 1.0)
		pc = fmod(pc + ic, 1.0)
		saws[i] = (pa * 2.0 - 1.0) * 0.3 + (pb * 2.0 - 1.0) * 0.3 + (pc * 2.0 - 1.0) * 0.22
	_saturate(saws, 3.0)
	var body := _bandpass(_bandpass(saws, 450.0, 2200.0), 450.0, 2200.0)
	var air := _band_noise_buffer(n, 400.0, 1400.0)
	var cycle := int(1.5 * MIX_RATE)
	for i in n:
		var u := float(i % cycle) / float(cycle)
		var swell := 4.0 * u * (1.0 - u)
		out[i] = body[i] * (0.75 + 0.25 * swell) + air[i] * 1.4 * swell * swell
	return out


# A 220 kg footfall: the slap, a 60 Hz thump with its harmonics, the deck's
# low modes (boom, never bell), the flesh settling after, and one time in
# three the sole peeling off the steel.
func _her_step() -> PackedFloat32Array:
	var out := _silence(0.4)
	_add_band_noise(out, 0.0, 0.006, 1.2, 0.0015, 700.0, 5000.0)
	_add_thump(out, 0.0, 60.0 * _rng.randf_range(0.9, 1.1), 0.15, 0.07)
	for f in [190.0, 430.0, 720.0]:
		_add_mode(out, f * _rng.randf_range(0.92, 1.08), 0.0, 0.25, 0.9, _rng.randf_range(0.05, 0.09), 3.0)
	_add_band_noise(out, 0.08, 0.03, 0.3, 0.008, 300.0, 2000.0)
	_add_band_noise(out, 0.15, 0.03, 0.2, 0.008, 300.0, 2000.0)
	_rough(out, 11.0, 0.25)
	if _rng.randf() < 0.34:
		_add_glide(out, 0.06, 0.025, 1.5, 900.0, 2000.0, 0.3)
	return out


# Joints: 3-8 cavitation pops, the gaps closing 160 -> 40 ms, each a click,
# a 3 kHz tick and a knock 250-450 Hz; gristle grinding between.
func _her_joints() -> PackedFloat32Array:
	var out := _silence(0.7)
	var gap := 0.16
	var at := 0.02
	for k in _rng.randi_range(3, 8):
		if at > 0.62:
			break
		_add_band_noise(out, at, 0.002, 1.0, 0.0007, 2000.0, 6000.0)
		_add_mode(out, 3000.0 * _rng.randf_range(0.85, 1.15), at, 0.03, 0.5, 0.004, 8.0)
		_add_mode(out, _rng.randf_range(250.0, 450.0), at, 0.1, 0.6, 0.02, 4.0)
		at += gap * _rng.randf_range(0.8, 1.2)
		gap = maxf(0.04, gap * 0.7)
	var grist := _resonate(_slip(out.size(), 25.0, 35.0), 1200.0, 2.0)
	for i in out.size():
		out[i] += grist[i] * 0.5 * (1.0 - float(i) / float(out.size()))
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


# The lifting magnet switched on: its coil buzzing at the rectifier's ripple
# (100 Hz) and the overtones a phone's speaker still plays, steady, and its
# contactor's faint hiss above.
func _magnet_hum(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	for pair in [[100.0, 1.2], [200.0, 1.6], [300.0, 1.0], [600.0, 0.5]]:
		var tone := _resonant_noise_buffer(n, pair[0], 0.002, 1.0)
		for i in n:
			out[i] += tone[i] * pair[1]
	_add_band_noise(out, 0.0, length, 0.04, 0.0, 1500.0, 3500.0)
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


# --- her primitives (build_her) ----------------------------------------------


# A throat: glottal pulses (a smoothstep opening over 60% of the period, a
# sharper closing over 20%, closed for the rest), differentiated as the lips
# radiate it, then tilted by a one-pole low-pass at `tilt_hz`. f0 glides
# exponentially f0_a -> f0_b. Per period: f0 x (1 +- jitter), amplitude x
# (1 +- shimmer), and every second period x (1 - sub) (period doubling).
# `breath`: noise 1-4 kHz gated by the open phase.
func _glottal(n: int, f0_a: float, f0_b: float, jitter: float, shimmer: float, sub: float,
		tilt_hz: float, breath: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(n)
	var asp := _band_noise_buffer(n, 1000.0, 4000.0) if breath > 0.0 else PackedFloat32Array()
	var a := _alpha(tilt_hz)
	var glide := pow(f0_b / f0_a, 1.0 / float(maxi(n, 1)))
	var f0 := f0_a
	var phase := 0.0
	var k := 0
	var f_mul := 1.0
	var amp := 1.0
	var g_prev := 0.0
	var lp := 0.0
	for i in n:
		var f := f0 * f_mul
		phase += f / float(MIX_RATE)
		if phase >= 1.0:
			phase -= 1.0
			k += 1
			f_mul = 1.0 + jitter * _rng.randf_range(-1.0, 1.0)
			amp = (1.0 + shimmer * _rng.randf_range(-1.0, 1.0)) * ((1.0 - sub) if k % 2 == 1 else 1.0)
		var g := 0.0
		if phase < 0.6:
			var s := phase / 0.6
			g = s * s * (3.0 - 2.0 * s)
		elif phase < 0.8:
			var s := (phase - 0.6) / 0.2
			g = 1.0 - s * s * (3.0 - 2.0 * s)
		g *= amp
		var d := (g - g_prev) * float(MIX_RATE) / (7.5 * f)
		g_prev = g
		lp += a * (d - lp)
		out[i] = lp + (asp[i] * breath * g if breath > 0.0 else 0.0)
		f0 *= glide
	return out


# A throat's tract: three formants in parallel (Q 5/6/8, each times `q_mul`;
# gains 1/0.6/0.35), normalized to a peak of 1.
func _formants(src: PackedFloat32Array, f1: float, f2: float, f3: float, q_mul: float = 1.0) -> PackedFloat32Array:
	var c := PackedFloat32Array()   # per formant: b0, a1, a2, gain
	for spec in [[f1, 5.0, 1.0], [f2, 6.0, 0.6], [f3, 8.0, 0.35]]:
		var w := TAU * minf(float(spec[0]), 0.45 * MIX_RATE) / float(MIX_RATE)
		var alpha := sin(w) / (2.0 * float(spec[1]) * q_mul)
		var a0 := 1.0 + alpha
		c.append_array([alpha / a0, -2.0 * cos(w) / a0, (1.0 - alpha) / a0, float(spec[2])])
	var out := PackedFloat32Array()
	out.resize(src.size())
	var x1 := 0.0
	var x2 := 0.0
	var p1 := 0.0
	var p2 := 0.0
	var q1 := 0.0
	var q2 := 0.0
	var r1 := 0.0
	var r2 := 0.0
	var peak := 0.0
	for i in src.size():
		var x := src[i]
		var dx := x - x2
		var p := c[0] * dx - c[1] * p1 - c[2] * p2
		var q := c[4] * dx - c[5] * q1 - c[6] * q2
		var r := c[8] * dx - c[9] * r1 - c[10] * r2
		x2 = x1
		x1 = x
		p2 = p1
		p1 = p
		q2 = q1
		q1 = q
		r2 = r1
		r1 = r
		var y := p * c[3] + q * c[7] + r * c[11]
		out[i] = y
		peak = maxf(peak, absf(y))
	if peak > 0.0:
		var s := 1.0 / peak
		for i in out.size():
			out[i] *= s
	return out


func _peak(buf: PackedFloat32Array, to: float) -> void:
	var peak := 0.0
	for x in buf:
		peak = maxf(peak, absf(x))
	if peak > 0.0:
		var s := to / peak
		for i in buf.size():
			buf[i] *= s


# Soft clipping, in place, after a normalization to peak 1 (symmetric: an
# asymmetric curve demodulates a flutter into the lows).
func _saturate(buf: PackedFloat32Array, drive: float) -> void:
	_peak(buf, 1.0)
	var scale := 1.0 / tanh(drive)
	for i in buf.size():
		buf[i] = tanh(drive * buf[i]) * scale


# Roughness, in place: amplitude flutter, a triangle at `rate_hz` (each cycle
# +-20%), `depth` 0..1.
func _rough(buf: PackedFloat32Array, rate_hz: float, depth: float) -> void:
	var ph := 0.0
	var r := rate_hz
	for i in buf.size():
		ph += r / float(MIX_RATE)
		if ph >= 1.0:
			ph -= 1.0
			r = rate_hz * _rng.randf_range(0.8, 1.2)
		buf[i] *= 1.0 - depth * absf(ph * 2.0 - 1.0)


# The input between two corners (the band noise's filter, on a signal).
func _bandpass(input: PackedFloat32Array, low_hz: float, high_hz: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(input.size())
	var a_hi := _alpha(high_hz)
	var a_lo := _alpha(low_hz)
	var h1 := 0.0
	var h2 := 0.0
	var l1 := 0.0
	var l2 := 0.0
	for i in input.size():
		var x := input[i]
		h1 += a_hi * (x - h1)
		h2 += a_hi * (h1 - h2)
		l1 += a_lo * (x - l1)
		l2 += a_lo * (l1 - l2)
		out[i] = h2 - l2
	return out


# Noise through a band-pass sweeping f_from -> f_to (exponential), damping
# 1/Q (its coefficient updated every 16 samples), in one parabolic swell
# 4u(1-u).
func _add_glide(out: PackedFloat32Array, start: float, length: float, gain: float, f_from: float,
		f_to: float, damping: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	if last <= first:
		return
	var count := last - first
	var step := pow(f_to / f_from, 1.0 / float(count))
	var f := f_from
	var low := 0.0
	var band := 0.0
	var norm := 0.6 * damping * gain   # resonance gain is about 1/damping
	var k := 0.0
	for i in range(first, last):
		var j := i - first
		if j % 16 == 0:
			k = 2.0 * sin(PI * f / float(MIX_RATE))
		low += k * band
		var high := _rng.randf_range(-1.0, 1.0) - low - damping * band
		band += k * high
		var u := float(j) / float(count)
		out[i] += band * norm * 4.0 * u * (1.0 - u)
		f *= step


# Poisson noise bursts (crackle, mucus, fibers): rate ramps rate_a -> rate_b
# per second over the span; each burst ms_lo..ms_hi long in [low, high] Hz at
# a random level up to `gain`.
func _add_grains(out: PackedFloat32Array, start: float, length: float, rate_a: float, rate_b: float,
		ms_lo: float, ms_hi: float, low_hz: float, high_hz: float, gain: float) -> void:
	var t := start
	while t < start + length:
		var dur := _rng.randf_range(ms_lo, ms_hi) / 1000.0
		_add_band_noise(out, t, dur, gain * _rng.randf_range(0.25, 1.0), dur * 0.35, low_hz, high_hz)
		var rate := lerpf(rate_a, rate_b, (t - start) / length)
		t += -log(maxf(_rng.randf(), 0.0001)) / rate


# Stick-slip: a click (+a, -0.75a, +0.5a, -0.25a) of random level 0.4..1 at
# intervals of 1/rate x 0.7..1.3, the rate ramping rate_a -> rate_b over the
# buffer. Run it through _resonate for a creak, a squeal, a grind.
func _slip(n: int, rate_a: float, rate_b: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(n)
	var i := 0
	while i < n:
		var a := _rng.randf_range(0.4, 1.0)
		for k in 4:
			if i + k < n:
				out[i + k] += a * (1.0 - 0.25 * float(k)) * (1.0 if k % 2 == 0 else -1.0)
		var rate := lerpf(rate_a, rate_b, float(i) / float(n))
		i += maxi(4, int(float(MIX_RATE) / (rate * _rng.randf_range(0.7, 1.3))))
	return out


# Weight a phone can carry: a thump at f0 with its harmonics 2..8 (gain
# 1/k^0.7), so a speaker that drops everything under 300 Hz still hears it.
func _add_thump(out: PackedFloat32Array, start: float, f0: float, gain: float, tau: float) -> void:
	for k in range(1, 9):
		_add_tone(out, f0 * float(k), start, tau * 6.0, gain / pow(float(k), 0.7), tau)


# A body going into deep water: the slap of the surface (broadband, a few
# milliseconds), the crown thrown up and falling back as a wash of hiss, and
# the air dragged under ringing low as bubbles under it all.
func _splash() -> PackedFloat32Array:
	var out := _silence(1.4)
	_add_band_noise(out, 0.0, 0.03, 0.9, 0.008, 200.0, 6000.0)
	_add_band_noise(out, 0.02, 1.2, 0.45, 0.35, 900.0, 7000.0)
	_add_band_noise(out, 0.05, 0.9, 0.35, 0.25, 80.0, 600.0)
	for b in 9:
		var start := int(_rng.randf_range(0.06, 0.8) * MIX_RATE)
		var f0 := _rng.randf_range(180.0, 520.0)
		var env := _rng.randf_range(0.15, 0.3)
		var decay := _decay(_rng.randf_range(0.02, 0.05))
		var phase := 0.0
		for i in range(start, mini(out.size(), start + int(0.25 * MIX_RATE))):
			phase += TAU * f0 * (1.0 + float(i - start) / float(MIX_RATE) * 2.0) / float(MIX_RATE)
			out[i] += sin(phase) * env
			env *= decay
	return out


# A stroke through water at the surface: a soft rushing push, low and short.
func _swim_stroke() -> PackedFloat32Array:
	var out := _silence(0.5)
	_add_band_noise(out, 0.0, 0.45, 0.35, 0.16, 300.0, 3000.0)
	_add_band_noise(out, 0.03, 0.3, 0.2, 0.08, 60.0, 400.0)
	return out
