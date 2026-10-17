#!/usr/bin/env python3
"""Prepare an observed x86_64 export without changing the shipping preset."""
import argparse
from pathlib import Path


def prepare(project: Path) -> None:
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
