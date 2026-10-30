#!/usr/bin/env python3
"""Check that the exported APK carries its exact story copy and voice streams."""

import hashlib
import json
import math
from pathlib import Path
import re
import sys
import zipfile


# Preserve opening/discovery and double dialogue; require the terminal handover recordings.
REQUIRED_CUES = {
    'arrival_iona_empty_record': 'IONA',
    'arrival_iona_personnel_way': 'IONA',
    'arrival_iona_mesh_lip': 'IONA',
    'arrival_iona_first_ring': 'IONA',
    'service_iona_inspection_rail': 'IONA',
    'service_iona_swing_stair': 'IONA',
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
    'double_ro_original_scope': 'RO',
    'double_ari_original_scope': 'ARI',
    'double_ro_dev_has_scope': 'RO',
    'double_ari_dev_has_scope': 'ARI',
    'double_ro_when': 'RO',
    'double_ari_before': 'ARI',
    'double_ro_name': 'RO',
    'double_ari_will': 'ARI',
    'double_ro_and_face': 'RO',
    'double_ari_cannot': 'ARI',
    'double_iona_here': 'IONA',
    'handover_record_assurance': 'RECORD',
    'handover_ro_denial': 'RO',
    'handover_iona_revision': 'IONA',
    'handover_ro_witness': 'RO',
    'handover_iona_gallery': 'IONA',
    'handover_source_survey': 'SOURCE',
    'handover_ro_send_scope': 'RO',
    'handover_dev_received': 'DEV',
    'handover_ro_ari_first': 'RO',
    'handover_iona_ari_first': 'IONA',
}


def voice_entries(voice: dict) -> list:
    return [voice] + ([voice['remade']] if 'remade' in voice else [])


def verify_recordings(directory: Path, manifest: dict) -> None:
    receipt = json.loads((directory / 'generation_receipt.json').read_text())
    cues = manifest['cues']
    for key, speaker in REQUIRED_CUES.items():
        if key not in cues or cues[key]['speaker'] != speaker:
            raise ValueError(f'story cue {key}: required {speaker} recording is missing')
    for speaker in set(REQUIRED_CUES.values()):
        # A speaker partly remade carries the new method's provenance under 'remade'.
        for voice in voice_entries(manifest['voices'][speaker]):
            if not all(voice.get(field) for field in ('generation', 'provenance', 'license', 'source')):
                raise ValueError(f'story voice {speaker}: missing generation/license provenance')
            notice = directory / voice['license'].get('notice', '')
            if not notice.is_file() or not notice.read_text().strip():
                raise ValueError(f'story voice {speaker}: license notice {notice.name!r} is missing or empty')
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
    root = Path(__file__).resolve().parents[1]
    source = root / 'godot/audio/story_radio/manifest.json'
    expected = json.loads(source.read_text())
    verify_recordings(source.parent, expected)

    # The owner's approval gate: this APK may carry cue provenance and
    # captions, but no unapproved human speech streams or remaps.
    player = (root / 'godot/presentation/audio/story_radio_player.gd').read_text()
    if not re.search(r'^const VOICE_RECORDINGS_APPROVED := false$', player, re.MULTILINE):
        raise ValueError('story voice approval gate changed; review the APK exclusion contract')

    with zipfile.ZipFile(apk) as package:
        actual = json.loads(package.read('assets/audio/story_radio/manifest.json'))
        if actual != expected:
            raise ValueError('story manifest: exported copy differs from source')
        # Trunk has both original and partly remade voice provenance.
        notices = sorted({entry['license']['notice'] for voice in expected['voices'].values()
                          for entry in voice_entries(voice)})
        for notice in notices:
            if not package.read('assets/audio/story_radio/' + notice):
                raise ValueError(f'story voice license: expected nonempty packaged notice {notice}')
        names = package.namelist()
        forbidden = []
        keys = set(expected['cues'])
        for key, cue in expected['cues'].items():
            if cue['file'] != f'res://audio/story_radio/{key}.ogg':
                raise ValueError(f'story cue {key}: unexpected resource path {cue["file"]!r}')
        for name in names:
            if name.startswith('assets/audio/story_radio/') and (
                    name.endswith('.ogg') or name.endswith('.ogg.import')):
                forbidden.append(name)
            if name.startswith('assets/.godot/imported/'):
                filename = name.rsplit('/', 1)[-1]
                if any(filename.startswith(f'{key}.ogg-') for key in keys):
                    forbidden.append(name)
        if forbidden:
            raise ValueError('unapproved story voice included in APK: ' +
                             ', '.join(sorted(forbidden)[:8]))
    return len(expected['cues'])


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('usage: verify_story_radio_package.py APK')
    artifact = Path(sys.argv[1])
    try:
        count = verify(artifact)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        raise SystemExit(f'STORY_RADIO_PACKAGE FAIL {artifact}: {error}') from error
    print(f'STORY_RADIO_PACKAGE PASS cues={count} manifest=exact voice_streams=excluded recording_hashes=exact')
