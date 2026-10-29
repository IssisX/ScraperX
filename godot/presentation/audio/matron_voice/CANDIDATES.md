# Her voice: candidates

Written by src/voice/generate_matron_voice.py. Each clip is Chatterbox TTS (MIT licence) speaking one
line from 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md in the voice of a reference sentence spoken by
a Piper voice (an AI voice, never a recording of a real person); Whisper checked that it says the line.
Nothing else touches the voice (it is levelled and faded). Candidates only: nothing here is wired into
the game.

## Voices

- **kristin**: a woman, American; Piper en_US-kristin-medium (https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/kristin/medium), trained from scratch on LibriVox recordings, about 11.5 hours (https://librivox.org), licence: public domain
- **cori**: a woman, British; Piper en_GB-cori-high (https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_GB/cori/high), trained from scratch on LibriVox recordings, about 24 hours (https://librivox.org), licence: public domain
- **ljspeech**: a woman, American (also the story radio's Iona); Piper en_US-ljspeech-high (https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/ljspeech/high), trained from scratch on LJ Speech, LibriVox readings (https://keithito.com/LJ-Speech-Dataset/), licence: public domain

## Clips

| clip | voice | line | delivery | exaggeration | cfg_weight | temperature | level dB | seed (take) | seconds | match | Whisper heard |
|---|---|---|---|---|---|---|---|---|---|---|---|
| matron_02_cori_singsong.ogg | cori | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1025944760 (0) | 2.19 | 1.00 | 'Come out, come out, nurse has your medicine.' |
| matron_02_cori_whisper.ogg | cori | 2 'Come out, come out. Nurse has your medicine.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 3599251043 (0) | 3.08 | 1.00 | 'Come out, come out. Nurse has your medicine.' |
| matron_02_kristin_singsong.ogg | kristin | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2704197048 (0) | 2.83 | 1.00 | 'Come out, come out. Nurse has your medicine.' |
| matron_02_kristin_whisper.ogg | kristin | 2 'Come out, come out. Nurse has your medicine.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 3622281127 (0) | 3.06 | 1.00 | 'Come out, come out, nurse has your medicine.' |
| matron_02_ljspeech_singsong.ogg | ljspeech | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3474491133 (1) | 1.75 | 1.00 | 'Come out, come out, nurse has your medicine.' |
| matron_02_ljspeech_whisper.ogg | ljspeech | 2 'Come out, come out. Nurse has your medicine.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 2325888305 (0) | 3.62 | 1.00 | 'come out, come out. Nurse has your medicine.' |
| matron_16_cori_menace.ogg | cori | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 4213381556 (2) | 3.95 | 1.00 | 'Say please. Go on. Beg nurse like a good little cunt.' |
| matron_16_cori_snarl.ogg | cori | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 2329098789 (0) | 3.18 | 1.00 | 'Say please. Go on. Beg nurse like a good little cunt.' |
| matron_16_kristin_menace.ogg | kristin | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 339923292 (0) | 4.19 | 1.00 | 'Say please, go on, beg nurse like a good little cunt.' |
| matron_16_kristin_snarl.ogg | kristin | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 1040971876 (0) | 3.51 | 1.00 | 'Say please, go on, beg nurse like a good little cunt.' |
| matron_16_ljspeech_menace.ogg | ljspeech | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1225225162 (0) | 4.88 | 1.00 | 'Say please. Go on. Beg nurse like a good little cunt.' |
| matron_16_ljspeech_snarl.ogg | ljspeech | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 330460318 (0) | 3.48 | 1.00 | 'Say please, go on. Beg nurse like a good little cunt.' |
| matron_27_cori_menace.ogg | cori | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2365789323 (2) | 1.01 | 1.00 | 'Waste not.' |
| matron_27_cori_whisper.ogg | cori | 27 'Waste not.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 2101556692 (3) | 0.85 | 1.00 | 'waste not.' |
| matron_27_kristin_menace.ogg | kristin | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1650646115 (0) | 0.81 | 1.00 | 'waste not.' |
| matron_27_kristin_whisper.ogg | kristin | 27 'Waste not.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 2017594734 (0) | 0.79 | 1.00 | 'waste not.' |
| matron_27_ljspeech_menace.ogg | ljspeech | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1059553013 (0) | 1.28 | 1.00 | 'waste not' |
| matron_27_ljspeech_whisper.ogg | ljspeech | 27 'Waste not.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 4168756999 (3) | 0.73 | 1.00 | 'waste not.' |

Deliveries are what the settings are meant to give, not yet heard by anyone: `singsong` the sweet
voice; `whisper` lips to the ear (a hush played quiet, not an unvoiced whisper: Chatterbox has none);
`snarl` the mask off; `menace` no lift, no hurry.
