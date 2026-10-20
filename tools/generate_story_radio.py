#!/usr/bin/env python3
"""Generate story speech with the public Pendrokar Kokoro Gradio API.

Offline authoring only; Python stdlib plus ffmpeg/ffprobe. No runtime service.
The hosted model uses fixed bundled voice IDs, never an uploaded reference.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import urllib.request

BASE = 'https://pendrokar-kokoro-tts.hf.space'
API = BASE + '/gradio_api/call/predict'
CUES = {
    'receiving_iona_come_look': 'Ro. You’re in Receiving. Your name’s still on the handover. Come look at what it calls empty.',
    'receiving_iona_freight_one': 'Freight One is eleven metres up. Find firm footing there. The crane yard is ahead.',
    'freight_one_iona_crown': 'Our freight line still shares the construction return at the Crown. They remove that, we lose the way up. We need it separated.',
    'receiving_iona_keep_the_height': 'You’ve found your way up. Keep the height. The Crown is still above you.',
    'receiving_iona_recovery': 'Find your footing. You can try another way up.',
    'vale_contact_ro': 'You kept me as your emergency contact?',
    'vale_contact_iona': 'They asked for next of kin. Not a fucking recommendation.',
    'double_ro_who': 'Who the fuck are you?',
    'double_ari_name': 'Ari Ward.',
    'double_ro_face': "That's my face.",
    'double_ari_job': 'I was hired to attend your handovers.',
    'double_ro_empty': 'You told them the rooms were empty.',
    'double_ari_told': 'I was told they were.',
    'double_ro_when': 'When did you find out?',
    'double_ari_before': 'Before the last recording.',
    'double_ro_name': 'Stop using my name.',
    'double_ari_will': 'I will.',
    'double_ro_and_face': 'And the face?',
    'double_ari_cannot': "That's not something I can hand back.",
    'double_iona_here': "Ro. I'm here.",
    'handover_record_assurance': 'No occupied areas remain within the handover boundary.',
    'handover_ro_denial': "I didn't record that.",
    'handover_iona_revision': 'Your survey says access withdrawn. Occupancy not assessed. They changed the conclusion and kept your name.',
    'handover_ro_witness': 'Who delivered the final handover?',
    'handover_iona_gallery': 'Ari Ward. Barrel Helix landing, two hundred and fifty-three metres. Find them. Ask what they were told.',
    'handover_source_survey': 'Temporary construction access withdrawn. Occupancy not assessed.',
}
CUE_SPEAKERS = {'vale_contact_ro': 'RO', 'double_ro_who': 'RO', 'double_ari_name': 'ARI', 'double_ro_face': 'RO', 'double_ari_job': 'ARI', 'double_ro_empty': 'RO', 'double_ari_told': 'ARI', 'double_ro_when': 'RO', 'double_ari_before': 'ARI', 'double_ro_name': 'RO', 'double_ari_will': 'ARI', 'double_ro_and_face': 'RO', 'double_ari_cannot': 'ARI', 'double_iona_here': 'IONA'}
CUE_SPEAKERS.update({
    'handover_record_assurance': 'RECORD',
    'handover_ro_denial': 'RO',
    'handover_iona_revision': 'IONA',
    'handover_ro_witness': 'RO',
    'handover_iona_gallery': 'IONA',
    'handover_source_survey': 'SOURCE',
})
VOICE_IDS = {'IONA': 'af_heart', 'RO': 'am_michael', 'ARI': 'am_adam',
             'RECORD': 'am_michael', 'SOURCE': 'am_michael'}
DIRECT_SPEAKERS = {'ARI', 'RECORD', 'SOURCE'}
FILTER = 'highpass=f=180,lowpass=f=4800,loudnorm=I=-18:TP=-2:LRA=9'
DIRECT_FILTER = 'highpass=f=70,loudnorm=I=-18:TP=-2:LRA=9'


def fetch(url):
    with urllib.request.urlopen(url, timeout=180) as response:
        return response.read()


def generate(text, voice_id):
    payload = {'data': [text, voice_id, 0.9]}
    request = urllib.request.Request(API, json.dumps(payload).encode(),
                                     {'Content-Type': 'application/json'})
    with urllib.request.urlopen(request, timeout=45) as response:
        event_id = json.load(response)['event_id']
    print('generation_event', event_id, flush=True)
    event = ''
    with urllib.request.urlopen(API + '/' + event_id, timeout=180) as response:
        for raw_line in response:
            line = raw_line.decode().strip()
            if line.startswith('event: '):
                event = line[7:]
            elif line.startswith('data: ') and event == 'error':
                raise RuntimeError('Kokoro generation failed: ' + line[6:])
            elif line.startswith('data: ') and event == 'complete':
                result = json.loads(line[6:])[0]
                return fetch(result['url']), event_id
    raise RuntimeError('Kokoro stream ended without a completed audio file')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only', choices=CUES, help='Generate one cue for a bounded trial')
    args = parser.parse_args()
    directory = Path(__file__).resolve().parents[1] / 'godot/audio/story_radio'
    source = directory / 'source'
    source.mkdir(parents=True, exist_ok=True)
    (source / '.gdignore').write_text('')
    receipt_path = directory / 'generation_receipt.json'
    receipt = json.loads(receipt_path.read_text()) if receipt_path.exists() else {}
    manifest_path = directory / 'manifest.json'
    existing_manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    cues = existing_manifest.get('cues', {}).copy()
    for cue_id, text in CUES.items():
        if args.only and args.only != cue_id:
            continue
        speaker = CUE_SPEAKERS.get(cue_id, 'IONA')
        voice_id = VOICE_IDS[speaker]
        clean = source / (cue_id + '.wav')
        if not clean.exists():
            speech, event_id = generate(text, voice_id)
            clean.write_bytes(speech)
            receipt[cue_id] = {'event_id': event_id, 'text': text,
                               'speaker': speaker, 'voice_id': voice_id,
                               'clean_sha256': hashlib.sha256(speech).hexdigest()}
            receipt_path.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + '\n')
        elif receipt.get(cue_id, {}).get('text') != text:
            raise RuntimeError('Existing source receipt does not match cue text: ' + cue_id)
        target = directory / (cue_id + '.ogg')
        accepted = receipt[cue_id]
        if accepted.get('voice_id', 'af_heart') != voice_id:
            raise RuntimeError('Existing source voice does not match: ' + cue_id)
        if hashlib.sha256(clean.read_bytes()).hexdigest() != accepted['clean_sha256']:
            raise RuntimeError('Existing clean audio hash does not match: ' + cue_id)
        if (target.exists() and accepted.get('radio_sha256') ==
                hashlib.sha256(target.read_bytes()).hexdigest() and cue_id in cues):
            if cues[cue_id]['text'] != text or cues[cue_id]['speaker'] != speaker:
                raise RuntimeError('Existing manifest does not match cue: ' + cue_id)
            print(cue_id, 'reused accepted audio', flush=True)
            continue
        subprocess.run(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-y',
                        '-i', str(clean), '-af', DIRECT_FILTER if speaker in DIRECT_SPEAKERS else FILTER, '-ar', '24000', '-ac', '1',
                        '-c:a', 'libvorbis', '-q:a', '5', str(target)], check=True)
        duration = float(subprocess.check_output(
            ['ffprobe', '-v', 'error', '-show_entries', 'format=duration',
             '-of', 'default=noprint_wrappers=1:nokey=1', str(target)], text=True))
        cues[cue_id] = {'file': 'res://audio/story_radio/' + target.name,
                        'speaker': speaker, 'text': text, 'duration_s': duration}
        receipt[cue_id]['radio_sha256'] = hashlib.sha256(target.read_bytes()).hexdigest()
        print(cue_id, duration, target.stat().st_size, flush=True)
    receipt_path.write_text(json.dumps(receipt, indent=2, ensure_ascii=False) + '\n')
    manifest = {
        'schema_version': 1,
        'voices': {'IONA': {
            'generation': {'model': 'hexgrad/Kokoro-82M', 'host': BASE,
                           'api': API, 'voice_id': 'af_heart', 'speed': 0.9,
                           'device': 'CPU', 'language': 'American English',
                           'reference_audio': None, 'radio_filter': FILTER,
                           'sample_rate_hz': 24000},
            'provenance': {'space_owner': 'Pendrokar (third-party public deployment)',
                           'space_revision': '412be8aecb86220093fbccd9b4ce2ced5308568f',
                           'model_card_revision_observed': 'f3ff3571791e39611d31c381e3a41a3af07b4987',
                           'hosted_weights_revision': 'Not exposed by hosted API',
                           'generated_utc_date': '2026-10-20',
                           'clean_sources': 'source/*.wav (excluded from Godot import)',
                           'receipt': 'generation_receipt.json',
                           'listening_review': 'Not performed; ffprobe confirms audio properties only'},
            'license': {'model': 'Apache-2.0', 'notice': 'KOKORO_LICENSE.txt',
                        'commercial_use': 'The official Kokoro-82M model card declares Apache-2.0 and expressly welcomes commercial deployments. Bundled af_heart voice is selected by ID; no custom voice clone or uploaded reference. Dialogue is original project text. No exclusive rights or clearance beyond published model terms are asserted.'},
            'source': ['https://huggingface.co/hexgrad/Kokoro-82M',
                       'https://huggingface.co/hexgrad/Kokoro-82M/blob/main/VOICES.md',
                       'https://huggingface.co/spaces/Pendrokar/Kokoro-TTS/blob/412be8aecb86220093fbccd9b4ce2ced5308568f/app.py',
                       'https://www.gradio.app/guides/querying-gradio-apps-with-curl']
        }},
        'cues': cues,
    }
    import copy
    manifest['voices'].update(existing_manifest.get('voices', {}))
    for speaker in ['RO', 'ARI', 'RECORD', 'SOURCE']:
        if not any(cue['speaker'] == speaker for cue in cues.values()):
            continue
        voice = copy.deepcopy(manifest['voices']['IONA'])
        voice['generation']['voice_id'] = VOICE_IDS[speaker]
        voice['generation']['radio_filter'] = DIRECT_FILTER if speaker in DIRECT_SPEAKERS else FILTER
        voice['generation']['placement'] = (
            'handover terminal speaker / spatial' if speaker in {'RECORD', 'SOURCE'}
            else 'native actor head / spatial' if speaker == 'ARI' else 'player radio')
        voice['license']['commercial_use'] = voice['license']['commercial_use'].replace('af_heart', VOICE_IDS[speaker])
        manifest['voices'][speaker] = voice
    # Analyze the actual shipped recording. Mouth motion samples this envelope
    # by the active stream's playback position; no invented speech oscillator.
    import struct, math
    for cue_id, cue in cues.items():
        target = directory / (cue_id + '.ogg')
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        if cue.get('rms_audio_sha256') == digest and isinstance(cue.get('rms_30hz'), list):
            continue
        pcm = subprocess.check_output(['ffmpeg', '-hide_banner', '-loglevel', 'error', '-i', str(target), '-f', 'f32le', '-ar', '24000', '-ac', '1', '-'])
        samples = [item[0] for item in struct.iter_unpack('<f', pcm)]
        envelope = []
        for start in range(0, len(samples), 800):
            window = samples[start:start + 800]
            rms = math.sqrt(sum(value * value for value in window) / len(window))
            envelope.append(round(min(1.0, rms * 5.0), 6))
        cue['rms_30hz'] = envelope
        cue['rms_audio_sha256'] = digest
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + '\n')


if __name__ == '__main__':
    main()
