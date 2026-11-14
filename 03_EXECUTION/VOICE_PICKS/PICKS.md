# Her voice: the picks (J-089)

Pick her voice by ear. Four voices, two clips each: the same neutral sentence in every voice, then
the same line of hers (line 2, "Come out, come out. Nurse has your medicine."). Judge the voice
itself, not the age of it: the picked voice gets the bank's age treatment afterwards (pitched down 2
semitones, slowed, a rasp), and the whole bank is then made in it the way the fear yells were
(`src/voice/generate_matron_voice.py`). These clips are Piper reference-voice previews, not the final
takes. The gate stays closed and her lines play silent in the game until the pick is made.

Reply on #407 with the name. The current bank voice is candidate 1, the baseline.

## 1. kristin — the bank's voice now

- `kristin_reference.ogg` (8.7 s), `kristin_line02.ogg` (2.5 s)
- Piper en_US-kristin-medium, https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/kristin/medium
- Dataset: LibriVox recordings, about 11.5 hours, public domain; a woman, American. The integrator's
  pick of 2026-10-29. Listen for: the voice the 129 clips already use — is it her, or only close?

## 2. amy

- `amy_reference.ogg` (10.1 s), `amy_line02.ogg` (3.4 s)
- Piper en_US-amy-medium, https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/amy/medium
- Dataset: amy. Listen for: a different American woman's register against kristin — sweeter, or thinner?

## 3. lessac

- `lessac_reference.ogg` (7.9 s), `lessac_line02.ogg` (2.5 s)
- Piper en_US-lessac-medium, https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/lessac/medium
- Dataset: lessac. Listen for: the most level, nurse-like delivery of the four — does it carry the
  condescension, or only the calm?

## 4. ljspeech

- `ljspeech_reference.ogg` (8.7 s), `ljspeech_line02.ogg` (2.7 s)
- Piper en_US-ljspeech-medium, https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/ljspeech/medium
- Dataset: LJSpeech (public domain). Listen for: the clearest diction of the four — does it sound like
  a nurse talking to a child, or like a reader?

## How these were made

Rendered 2026-11-11 with piper-tts 1.8.0, medium quality, 22050 Hz, from the pinned
piper-voices revision above. The neutral sentence is the reference sentence
`src/voice/generate_matron_voice.py` uses for the bank. Piper voices are trained from scratch on
their dataset; the integrator confirms the licence of the picked voice before the bank is regenerated
in it. When the pick lands, the gate opens (`MATRON_VOICE_APPROVED` in
`godot/presentation/audio/audio_director.gd`) and the bank is remade in the picked voice.
