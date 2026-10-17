#!/usr/bin/env python3
"""Prepare an observed x86_64 export without changing the shipping preset."""
import argparse
from pathlib import Path


def prepare(project: Path) -> None:
    touch = project / 'presentation/ui/touch_controls.gd'
    text = touch.read_text()
    # Observe after the real owner has handled each event. No generated input,
    # alternative controller or shipping-code instrumentation is installed.
    for old, new in (
        ('return _touch_down(touch.index, touch.position)',
         'var handled_down := _touch_down(touch.index, touch.position)\n\t\t\tget_node("/root/AndroidCompatibilityProbe").record_touch(event, self)\n\t\t\treturn handled_down'),
        ('return _touch_up(touch.index)',
         'var handled_up := _touch_up(touch.index)\n\t\tget_node("/root/AndroidCompatibilityProbe").record_touch(event, self)\n\t\treturn handled_up'),
        ('return _touch_drag(drag.index, drag.position, drag.relative)',
         'var handled_drag := _touch_drag(drag.index, drag.position, drag.relative)\n\t\tget_node("/root/AndroidCompatibilityProbe").record_touch(event, self)\n\t\treturn handled_drag'),
    ):
        if text.count(old) != 1:
            raise ValueError(f'Prepare emulator observer: expected one touch owner anchor {old!r}')
        text = text.replace(old, new)
    touch.write_text(text)
    main = project / 'presentation/main.gd'
    text = main.read_text()
    anchor = '\tvar native_wall_usec := Time.get_ticks_usec() - native_started_usec'
    if text.count(anchor) != 1:
        raise ValueError('Prepare emulator observer: expected one native-step receipt anchor')
    text = text.replace(anchor, anchor + '\n\tget_node("/root/AndroidCompatibilityProbe").record_step(world_move, _native.get_tick_index(), steps_advanced)')
    main.write_text(text)
    preset = project / 'export_presets.cfg'
    text = preset.read_text()
    for old, new in (
        ('name="Android arm64"', 'name="Android emulator"'),
        ('architectures/arm64-v8a=true', 'architectures/arm64-v8a=false'),
        ('architectures/x86_64=false', 'architectures/x86_64=true'),
    ):
        if text.count(old) != 1:
            raise ValueError(f'Prepare Android emulator export: expected one {old!r}')
        text = text.replace(old, new)
    preset.write_text(text)
    config = project / 'project.godot'
    text = config.read_text()
    if '[autoload]' in text:
        raise ValueError('Prepare Android emulator export: existing autoload section needs explicit integration')
    text += '\n[autoload]\n\nAndroidCompatibilityProbe="*res://tests/android_compatibility_probe.gd"\n'
    config.write_text(text)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path('godot'))
    prepare(parser.parse_args().project)
