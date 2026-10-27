extends "res://presentation/audio/sound_bank.gd"
# The world bank as the spec leaves it (no lullaby; breath, step, shriek moved to her bank), for timing only.


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
	# The Matron's (sim/matron.hpp), after everything else for the same reason.
	clips[&"locker_knock"] = _variants(3, _locker_knock)
	clips[&"bolt_pop"] = _variants(2, _bolt_pop)
	clips[&"locker_burst"] = [_wav(_locker_burst())]
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


