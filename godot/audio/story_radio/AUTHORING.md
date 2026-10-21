# Story recordings

Thirty-two actual spoken dialogue recordings authored with Kokoro-82M's bundled voices: Iona uses `af_heart`; Ro uses `am_michael`; Ari uses `am_adam`. Four `arrival_iona_*` lines connect the working yard witness, personnel approach, mesh climb and first-ring onward route. `service_iona_inspection_rail` names the existing rail and hanging ladder after supported arrival on the front 22 m ring. `service_iona_swing_stair` names the actual stair and conditional release-chain operation after firm front-ring tower contact at 33 m. The handover terminal's RECORD and SOURCE recordings use `am_michael`, matching Ro's recorded voice. All use speed 0.9 and American English. The two gallery emergency-contact lines are `vale_contact_ro` and `vale_contact_iona`; six `handover_*` lines cover the disputed record, Ro and Iona's response, and the source survey. No reference recording or custom voice clone was supplied. The game plays local Ogg Vorbis files and has no hosted TTS dependency.

The publicly exposed `predict` API at the third-party Pendrokar/Kokoro-TTS Space generated the initial library on 2026-10-20 and the four arrival lines plus inspection-rail and swinging-stair directions on 2026-10-21. New generation receipts include UTC timestamps. The official model author declares Apache-2.0 and expressly permits commercial deployment in its model card. `KOKORO_LICENSE.txt` preserves the Apache license. The exact hosted weight revision is not exposed; the observed model-card revision and hosted app revision are recorded in `manifest.json`. Licensing evidence supports model use; it is not a claim of exclusive rights or independent legal clearance.

Primary evidence:
- Model/license/training account: https://huggingface.co/hexgrad/Kokoro-82M
- Bundled voice ID: https://huggingface.co/hexgrad/Kokoro-82M/blob/main/VOICES.md
- Actual hosted API owner/source: https://huggingface.co/spaces/Pendrokar/Kokoro-TTS/blob/412be8aecb86220093fbccd9b4ce2ced5308568f/app.py
- API protocol: https://www.gradio.app/guides/querying-gradio-apps-with-curl

`source/*.wav` retains clean generation output, excluded from Godot import with `.gdignore`. The radio filter uses a 180 Hz highpass, 4800 Hz lowpass and loudness normalization to -18 LUFS / -2 dBTP. Ari and the handover terminal recordings use a 70 Hz highpass with the same normalization, preserving natural speech for spatial playback. The manifest explicitly identifies RECORD and SOURCE placement as the handover terminal speaker. No crackles or tones cover the speech. `generation_receipt.json` records generation event IDs, exact input text, and clean/processed file hashes.

From the repository root, regenerate or reprocess:

```sh
python tools/generate_story_radio.py
```

On this Termux host the working generation environment uses the existing Ubuntu FFmpeg wrappers; the Termux FFmpeg executable currently fails to resolve a C++ symbol. Select the wrappers without changing installed packages:

```sh
PATH=/data/data/com.termux/files/usr/tmp/scraperx-portable-line-20261020/character-prototype/bin:$PATH python tools/generate_story_radio.py
```

Existing clean files with matching receipt text and voice are reused. Accepted Ogg files with matching receipt hashes and manifest entries are preserved without re-encoding; `--only <cue_id>` generates or processes one cue and merges it into the manifest. Delete only the desired cue's clean WAV to regenerate it; generation is stochastic and the hosted model is not pinned. Reprocessing needs ffmpeg and ffprobe; generation uses Python standard library only.

Asset acceptance covers real generated audio, Ogg decode, sample rate/channel count, duration, manifest text/path integrity and hashes. Integrated cue-selection/playback evidence is tracked with the corresponding journey in the live checkpoint. Human listening and device playback remain separate observations.
