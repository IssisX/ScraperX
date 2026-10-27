extends SceneTree
func _initialize() -> void:
	var d := AudioEffectDistortion.new()
	d.mode = AudioEffectDistortion.MODE_WAVESHAPE
	d.drive = 0.45
	d.post_gain = -3.0
	print("DIST mode=%d drive=%.2f post=%.1f keep_hf=%.0f pre=%.1f" % [d.mode, d.drive, d.post_gain, d.keep_hf_hz, d.pre_gain])
	var p := AudioStreamPlayer3D.new()
	print("P3D cutoff=%.0f filter_db=%.1f max_db=%.1f unit=%.1f model=%d area_mask=%d paused=%s" % [p.attenuation_filter_cutoff_hz, p.attenuation_filter_db, p.max_db, p.unit_size, p.attenuation_model, p.area_mask, str(p.stream_paused)])
	print("LOWPRIO ratio=%s maxthreads=%s" % [str(ProjectSettings.get_setting("threading/worker_pool/low_priority_thread_ratio")), str(ProjectSettings.get_setting("threading/worker_pool/max_threads"))])
	p.free()
	quit(0)
