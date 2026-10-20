#!/usr/bin/env python3
"""Check that the exported APK carries its exact story copy and voice streams."""

import json
from pathlib import Path
import re
import sys
import zipfile


def verify(apk: Path) -> int:
    source = Path(__file__).resolve().parents[1] / 'godot/audio/story_radio/manifest.json'
    expected = json.loads(source.read_text())
    with zipfile.ZipFile(apk) as package:
        actual = json.loads(package.read('assets/audio/story_radio/manifest.json'))
        if actual != expected:
            raise ValueError('story manifest: exported copy differs from source')
        if not package.read('assets/audio/story_radio/KOKORO_LICENSE.txt'):
            raise ValueError('story voice license: expected nonempty packaged notice')
        for key, cue in expected['cues'].items():
            resource = cue['file']
            if resource != f'res://audio/story_radio/{key}.ogg':
                raise ValueError(f'story cue {key}: unexpected resource path {resource!r}')
            imported = 'assets/' + resource.removeprefix('res://') + '.import'
            remap = package.read(imported).decode()
            path = re.search(r'^path="res://([^"\n]+)"$', remap, re.MULTILINE)
            if path is None:
                raise ValueError(f'story cue {key}: {imported} has no imported stream path')
            stream = 'assets/' + path[1]
            if package.getinfo(stream).file_size == 0:
                raise ValueError(f'story cue {key}: empty packaged stream {stream}')
    return len(expected['cues'])


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: verify_story_radio_package.py APK')
    artifact = Path(sys.argv[1])
    try:
        count = verify(artifact)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit(f'STORY_RADIO_PACKAGE FAIL {artifact}: {error}') from error
    print(f'STORY_RADIO_PACKAGE PASS cues={count} manifest=exact imported_streams=present')
