extends SceneTree
const Bank := preload("res://presentation/audio/sound_bank.gd")
func _initialize() -> void:
    var args := OS.get_cmdline_user_args()
    if args.size() != 1:
        quit(2)
        return
    var bank := Bank.new()
    bank.build()
    var count := 0
    var failed := false
    for cue in bank.clips:
        var variants: Array = bank.clips[cue]
        for i in variants.size():
            var path := args[0].path_join("%s_%d.wav" % [cue, i])
            var clip: AudioStreamWAV = variants[i]
            var error := clip.save_to_wav(path)
            if error != OK:
                push_error("Export failed: %s code %d" % [path,error])
                failed = true
            count += 1
    print("FOOTSTEP_BANK_EXPORT clips=%d build_ms=%d engine=%s" % [count,bank.build_msec,Engine.get_version_info().string])
    quit(1 if failed else 0)
