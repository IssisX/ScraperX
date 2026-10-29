# Story recordings

Forty spoken dialogue cues, listed with their speaker and exact words in `manifest.json`. The game plays local Ogg Vorbis files; there is no hosted TTS dependency. Playback stays off until the owner approves the voices (`VOICE_RECORDINGS_APPROVED` in `presentation/audio/story_radio_player.gd`).

## Two kinds of take, during the changeover

The owner's rule (`CLAUDE.md`, "Human sounds are real"): a voice is a real recording, or a voice made the way the climber's fear yells were (`src/voice/generate_fall_voice.py`). The first library used stock Kokoro-82M voices, which that rule does not allow, so the cues are being remade with the approved method.

- **Remade with the approved method (4):** the three cues that fit this game's world (`receiving_iona_freight_one`, `receiving_iona_keep_the_height`, `receiving_iona_recovery`; the trigger audit, PR #31, `03_EXECUTION/STORY_RADIO_TRIGGERS.md`), and `double_ari_name`, made in the first trial run before the audit cut the job to those three.
- **Still the Kokoro takes (36):** the cues that refer to characters and systems this game does not have. They wait on the owner's story decision and are not remade until it is made. Their method is described below.

`manifest.json` says which is which: a speaker whose cues are all remade carries the new provenance as its `voices` entry (none is yet); one with cues of both kinds keeps the Kokoro entry for those, with the new provenance under `remade`, naming the cues it covers. Every remade cue's receipt has an `event_id` beginning `chatterbox:`.

## The approved method

`src/voice/generate_story_voice.py` (offline only; its header gives the setup and the commands):

- **Speech:** Chatterbox TTS 0.1.7 (Resemble AI, MIT), weights `ResembleAI/chatterbox` at revision `5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18`, on CPU. Exaggeration stays near Chatterbox's neutral 0.5 so the radio talk is calm and spoken: Iona and Ro 0.5, Ari 0.45, the terminal's RECORD and SOURCE playback 0.4; a few heated lines 0.55 to 0.65 (`LINE_DELIVERY`); nothing is shouted. cfg_weight 0.5, temperature 0.8.
- **Voice identity:** each speaker's voice comes from one reference passage ("Copy that. The night crew has finished on the north side, and the lift is locked at the second ring. Check in when you reach the landing.") spoken by a Piper voice from `rhasspy/piper-voices` (revision `5041a7a49ae5b3a0f30e6837769eb03fdbb231bd`, repository licence MIT):

  | Speaker | Piper voice | Its training data and licence |
  |---|---|---|
  | Iona (a woman, American) | `en_US-ljspeech-high` | LJ Speech (Keith Ito, Linda Johnson; LibriVox readings), public domain; trained from scratch, released public domain by Bryce Beattie |
  | Ro (a man, American), and the terminal's RECORD and SOURCE | `en_US-john-medium` | LibriVox recordings, public domain; the climber's own fall voice |
  | Ari (a man, American) | `en_US-bryce-medium` | Bryce Beattie's own recorded voice, released public domain |
  | Dev (a man, British) | none yet | Every British man's Piper voice is fine-tuned from Piper's `en_US-lessac-medium` voice, whose Blizzard 2013 Lessac data is not cleared for a commercial game, so the generator refuses Dev's cue and `handover_dev_received` stays a Kokoro take until a cleared voice is found |

  The voices were picked by measurement, since nothing here can listen: Whisper read each reference back; Ro (102 Hz median pitch) and Ari (127 Hz) differ by about four semitones, and Chatterbox's own speaker encoder scores Ari 0.74 alike to Ro, where one speaker's two passages score 0.95.
- **Check:** Whisper `small.en` (OpenAI, MIT) transcribes every take. A take is rejected unless the transcript holds at least 60% of the line's distinct words (numbers and units spelled out first, so "11 meters" counts as "eleven metres"), it ends within 1.0 s + 0.45 s a word, and it says no more than a quarter more words than the line. The best passing take ships; a line no take passes fails the run, to be made again with `--only` and `--takes`. Seeds are fixed per cue and take. Every take's transcript is in `generation_receipt.json`.
- **Processing (unchanged from the Kokoro library):** the kept take is trimmed, padded 0.15 s before and 0.35 s after, and written clean to `source/<cue>.wav` (24 kHz, excluded from Godot import with `.gdignore`). The radio filter is a 180 Hz highpass, 4800 Hz lowpass and loudness normalization to -18 LUFS / -2 dBTP; Ari and the handover terminal recordings use a 70 Hz highpass with the same normalization, keeping natural speech for spatial playback. Ogg Vorbis q5, 24 kHz mono. `manifest.json` gets each cue's duration, hash and 30 Hz speech envelope (`rms_30hz`, read by the mouth and the radio meter); the receipt gets the seed, every take and the clean and shipped hashes.

Remake the rest once the owner's story decision says which lines stay (from the repository root, with the venv and models of the generator's header):

```sh
tts/bin/python src/voice/generate_story_voice.py --models models --only <cue>,<cue>
```

Rerun only the radio chain from `source/*.wav`: `python3 src/voice/generate_story_voice.py --reprocess`. When the last Kokoro take is gone, delete `KOKORO_LICENSE.txt`.

## The Kokoro takes (36 cues, until remade)

Authored with Kokoro-82M's bundled voices: Iona `af_heart`; Ro `am_michael` (also the handover terminal's RECORD and SOURCE); Ari `am_adam`; Dev `bm_george`; speed 0.9, Dev in British English and the others American English. No reference recording or custom voice clone was supplied. The publicly exposed `predict` API at the third-party Pendrokar/Kokoro-TTS Space generated them on 2026-10-20 and 2026-10-21 with the old generator `tools/generate_story_radio.py` (on the `ChatGPT` branch; not on this trunk). The official model author declares Apache-2.0 and permits commercial deployment in the model card; `KOKORO_LICENSE.txt` preserves the Apache licence. The exact hosted weight revision is not exposed; the observed model-card and hosted app revisions are recorded in `manifest.json`.

Primary evidence:
- Model/license/training account: https://huggingface.co/hexgrad/Kokoro-82M
- Bundled voice ID: https://huggingface.co/hexgrad/Kokoro-82M/blob/main/VOICES.md
- Actual hosted API owner/source: https://huggingface.co/spaces/Pendrokar/Kokoro-TTS/blob/412be8aecb86220093fbccd9b4ce2ced5308568f/app.py

## Acceptance

`ci/verify_story_radio_package.py` checks every cue's Ogg against its manifest and receipt hashes, the receipt's text against the manifest's, a non-silent envelope, each voice entry's provenance and licence notice (both kinds), and, on the exported APK, the packaged manifest, notices and imported streams. Human listening and device playback are separate observations: the owner picks by ear.
