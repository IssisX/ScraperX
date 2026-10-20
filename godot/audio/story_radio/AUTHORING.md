# Iona radio speech

Seven actual spoken dialogue recordings authored with Kokoro-82M's bundled voices: Iona uses female `af_heart`; Ro uses male `am_michael`. Both use speed 0.9 and American English. The two gallery emergency-contact lines are `vale_contact_ro` and `vale_contact_iona`. No reference recording or custom voice clone was supplied. The game plays local Ogg Vorbis files and has no hosted TTS dependency.

The publicly exposed `predict` API at the third-party Pendrokar/Kokoro-TTS Space generated these assets on 2026-10-20. The official model author declares Apache-2.0 and expressly permits commercial deployment in its model card. `KOKORO_LICENSE.txt` preserves the Apache license. The exact hosted weight revision is not exposed; the observed model-card revision and hosted app revision are recorded in `manifest.json`. Licensing evidence supports model use; it is not a claim of exclusive rights or independent legal clearance.

Primary evidence:
- Model/license/training account: https://huggingface.co/hexgrad/Kokoro-82M
- Bundled voice ID: https://huggingface.co/hexgrad/Kokoro-82M/blob/main/VOICES.md
- Actual hosted API owner/source: https://huggingface.co/spaces/Pendrokar/Kokoro-TTS/blob/412be8aecb86220093fbccd9b4ce2ced5308568f/app.py
- API protocol: https://www.gradio.app/guides/querying-gradio-apps-with-curl

`source/*.wav` retains clean generation output, excluded from Godot import with `.gdignore`. The radio filter uses a 180 Hz highpass, 4800 Hz lowpass and loudness normalization to -18 LUFS / -2 dBTP. No crackles or tones cover the speech. `generation_receipt.json` records generation event IDs, exact input text, and clean/processed file hashes.

From the repository root, regenerate or reprocess:

```sh
python tools/generate_story_radio.py
```

On this Termux host the Codex launcher sets an older C++ library path. The observed working command selects the installed current libc++ while retaining Termux exec:

```sh
env LD_PRELOAD=/data/data/com.termux/files/usr/lib/libc++_shared.so:/data/data/com.termux/files/usr/lib/libtermux-exec.so python tools/generate_story_radio.py
```

Existing clean files with matching receipt text and voice are reused. Accepted Ogg files with matching receipt hashes and manifest entries are preserved without re-encoding; `--only <cue_id>` generates or processes one cue and merges it into the manifest. Delete only the desired cue's clean WAV to regenerate it; generation is stochastic and the hosted model is not pinned. Reprocessing needs ffmpeg and ffprobe (observed 8.1.2); generation uses Python standard library only.

Acceptance here covers real generated audio, Ogg decode, sample rate/channel count, duration, manifest text/path integrity and hashes. No human listening review, game playback or device playback was performed by the asset author.
