# The story radio's voices: credits

Every remade line is a voice made the way the climber's fear yells were (the owner's rule, `CLAUDE.md`, "Human sounds are real"), by `src/voice/generate_story_voice.py`: Chatterbox TTS speaks it, in a voice taken from a reference passage spoken by a Piper voice, and Whisper checked that it says its line. Nothing of Piper or Whisper ships; the licence texts are in `VOICE_LICENSES.txt`. How the files are made: `AUTHORING.md`.

## Tools (offline, on CPU; none runs in the game or in CI)

| Tool | Version | Licence | Use |
|---|---|---|---|
| Chatterbox TTS (Resemble AI), https://github.com/resemble-ai/chatterbox | chatterbox-tts 0.1.7; weights `ResembleAI/chatterbox` revision `5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18` | MIT | speech; marks its output with Resemble AI's inaudible Perth watermark |
| Piper (OHF-Voice), https://github.com/OHF-Voice/piper1-gpl | piper-tts 1.8.0 | GPL-3.0-or-later (the program; it only spoke the reference passages) | the reference passages |
| Piper voices, https://huggingface.co/rhasspy/piper-voices | revision `5041a7a49ae5b3a0f30e6837769eb03fdbb231bd` | MIT (the repository); each voice's data below | voice identity |
| Whisper (OpenAI), https://github.com/openai/whisper | openai-whisper 20250625, model `small.en` | MIT | checks every take |
| PyTorch | 2.6.0 (CPU) | BSD-3-Clause | runs the models |
| FFmpeg | 6.1.1 | LGPL/GPL | the radio filter and the Ogg Vorbis encode |

## Reference voices

| Speaker | Piper voice | Training data | Data licence |
|---|---|---|---|
| Iona | `en_US-ljspeech-high` | LJ Speech, https://keithito.com/LJ-Speech-Dataset/ (Keith Ito, 2017; Linda Johnson's LibriVox readings); model trained from scratch and released public domain by Bryce Beattie, https://brycebeattie.com/files/tts/ | Public domain |
| Ro; the handover terminal's RECORD and SOURCE | `en_US-john-medium` (the climber's fall voice) | LibriVox recordings, https://librivox.org, about 12.5 hours (Bryce Beattie) | Public domain |
| Ari | `en_US-bryce-medium` | Bryce Beattie's own recorded voice, https://brycebeattie.com/files/tts/ | Public domain |

## Remade files

Each is mono Ogg Vorbis at 24 kHz. "Whisper heard" is the shipped file read back by Whisper `small.en`.

| File | Speaker | Line | Whisper heard |
|---|---|---|---|
| `receiving_iona_freight_one.ogg` | Iona | Freight One is eleven metres up. Find firm footing there. The crane yard is ahead. | Freight one is 11 meters up. Find firm footing there. The crane yard is ahead. |
| `receiving_iona_keep_the_height.ogg` | Iona | You’ve found your way up. Keep the height. The Crown is still above you. | You found your way up. Keep the height. The crown is still above you. |
| `receiving_iona_recovery.ogg` | Iona | Find your footing. You can try another way up. | Find your footing. You can try another way up. |
| `double_ari_name.ogg` | Ari | Ari Ward. | Ari Ward |

Dev, a British man, has no voice yet: every British man's Piper voice is fine-tuned from Piper's `en_US-lessac` voice, whose Blizzard 2013 data is not cleared for a commercial game, so his line stays a Kokoro take until a cleared voice is found.

The other 36 cues are still the stock Kokoro-82M takes (Apache-2.0, `KOKORO_LICENSE.txt`), which the owner's rule does not allow; they stay gated off until the owner's story decision says which lines are remade.
