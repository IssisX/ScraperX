extends SceneTree
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
func _initialize() -> void:
    _run.call_deferred()
func _run() -> void:
    var controls := TouchControls.new()
    root.add_child(controls)
    for button in controls._buttons.values():
        button.shown = false
    controls._stick_zone = Rect2(0, 0, 100, 100)
    var down := InputEventScreenTouch.new()
    down.index = 0
    down.position = Vector2(50, 50)
    down.pressed = true
    controls.handle_touch(down)
    controls.handle_touch(down) # finger ID reuse must not retain a second owner
    var up := InputEventScreenTouch.new()
    up.index = 0
    up.position = down.position
    up.pressed = false
    controls.handle_touch(up)
    var reel = controls._buttons[&"slingshot_reel"]
    reel.shown = true
    reel.appear = 1.0
    reel.center = Vector2(200, 200)
    down.position = reel.center
    controls.handle_touch(down)
    up.position = reel.center
    controls.handle_touch(up)
    if reel.index != -1 or controls._look_index != -1 or controls._stick_index != -1:
        push_error("SCRAPERX_TOUCH_CAPTURE FAIL released finger retains control ownership")
        quit(1)
        return
    print("SCRAPERX_TOUCH_CAPTURE PASS repeated press and release clear every owner")
    quit(0)
