#!/usr/bin/env python3
"""Check that the exported APK carries its exact story copy and voice streams."""

import hashlib
import json
import math
from pathlib import Path
import re
import sys
import zipfile


# Keep the shipped opening/discovery contract while requiring the complete double.
REQUIRED_CUES = {
    'receiving_iona_come_look': 'IONA',
    'receiving_iona_freight_one': 'IONA',
    'freight_one_iona_crown': 'IONA',
    'receiving_iona_keep_the_height': 'IONA',
    'receiving_iona_recovery': 'IONA',
    'vale_contact_ro': 'RO',
    'vale_contact_iona': 'IONA',
    'double_ro_who': 'RO',
    'double_ari_name': 'ARI',
    'double_ro_face': 'RO',
    'double_ari_job': 'ARI',
    'double_ro_empty': 'RO',
    'double_ari_told': 'ARI',
    'double_ro_when': 'RO',
    'double_ari_before': 'ARI',
    'double_ro_name': 'RO',
    'double_ari_will': 'ARI',
    'double_ro_and_face': 'RO',
    'double_ari_cannot': 'ARI',
    'double_iona_here': 'IONA',
}


def verify_recordings(directory: Path, manifest: dict) -> None:
    receipt = json.loads((directory / 'generation_receipt.json').read_text())
    cues = manifest['cues']
    for key, speaker in REQUIRED_CUES.items():
        if key not in cues or cues[key]['speaker'] != speaker:
            raise ValueError(f'story cue {key}: required {speaker} recording is missing')
    for speaker in set(REQUIRED_CUES.values()):
        voice = manifest['voices'][speaker]
        if not all(voice.get(field) for field in ('generation', 'provenance', 'license', 'source')):
            raise ValueError(f'story voice {speaker}: missing generation/license provenance')
    for key, cue in cues.items():
        audio = directory / f'{key}.ogg'
        digest = hashlib.sha256(audio.read_bytes()).hexdigest()
        if cue.get('rms_audio_sha256') != digest or receipt[key]['radio_sha256'] != digest:
            raise ValueError(f'story cue {key}: recording/envelope/receipt hash mismatch')
        if receipt[key]['text'] != cue['text'] or not receipt[key].get('event_id'):
            raise ValueError(f'story cue {key}: recording receipt does not match dialogue')
        envelope = cue.get('rms_30hz')
        if (not isinstance(envelope, list) or not envelope or
                any(type(value) not in (int, float) or not math.isfinite(value) or
                    not 0 <= value <= 1 for value in envelope) or not any(envelope)):
            raise ValueError(f'story cue {key}: expected finite non-silent recorded RMS envelope')


def verify(apk: Path) -> int:
    source = Path(__file__).resolve().parents[1] / 'godot/audio/story_radio/manifest.json'
    expected = json.loads(source.read_text())
    verify_recordings(source.parent, expected)
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
    print(f'STORY_RADIO_PACKAGE PASS cues={count} manifest=exact imported_streams=present recording_hashes=exact')
