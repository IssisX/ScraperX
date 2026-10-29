# Her voice: candidates

Written by src/voice/generate_matron_voice.py. Each clip is Chatterbox TTS (MIT licence) speaking one
line from 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md in the voice of a reference recording of a
real older woman; Whisper checked that it says the line. Nothing else touches the voice (it is levelled
and faded). Candidates only: nothing here is wired into the game.

## Voices

- **a**: an older woman, speaker a2d6af23d32b, age given as seventies
  - source: Common Voice 17.0 en, clips common_voice_en_22275758 and common_voice_en_22275732 (mirror file en/validated-00111-of-00138.parquet)
  - licence: CC0 1.0 (public domain dedication), per the Common Voice dataset card
- **b**: an older woman, speaker d935ca6ce2fc, age given as seventies
  - source: Common Voice 17.0 en, clips common_voice_en_19666687 and common_voice_en_19665124 (mirror file en/validated-00041-of-00138.parquet)
  - licence: CC0 1.0 (public domain dedication), per the Common Voice dataset card
- **c**: an older woman with a lower voice, speaker a812084d7919, age given as seventies
  - source: Common Voice 17.0 en, clips common_voice_en_22323251 and common_voice_en_22434891 (mirror file en/validated-00111-of-00138.parquet)
  - licence: CC0 1.0 (public domain dedication), per the Common Voice dataset card

## Clips

| clip | voice | line | delivery | exaggeration | cfg_weight | temperature | level dB | seed (take) | seconds | match | Whisper heard |
|---|---|---|---|---|---|---|---|---|---|---|---|
| matron_02_a_singsong.ogg | a | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1478657896 (1) | 2.62 | 1.00 | 'Come out, come out. Nurse has your medicine.' |
| matron_02_a_whisper.ogg | a | 2 'Come out, come out. Nurse has your medicine.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 1652170810 (0) | 4.34 | 1.00 | 'Come out, come out. Nurse has your medicine.' |
| matron_02_b_singsong.ogg | b | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1397002789 (0) | 2.69 | 1.00 | 'Come out, come out. Nurse has your medicine.' |
| matron_02_b_whisper.ogg | b | 2 'Come out, come out. Nurse has your medicine.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 3752873204 (0) | 3.07 | 1.00 | 'come out, come out. Nurse has your medicine.' |
| matron_02_c_singsong.ogg | c | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3461051219 (0) | 2.31 | 1.00 | 'Come out, come out. Nurse Has Your Medicine.' |
| matron_02_c_whisper.ogg | c | 2 'Come out, come out. Nurse has your medicine.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 36083057 (0) | 5.99 | 1.00 | 'Come out. Come out. Nurse has your medicine.' |
| matron_16_a_menace.ogg | a | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2715774657 (0) | 5.40 | 1.00 | 'Say please. Go on. Begg nurse like a good little cunt.' |
| matron_16_a_snarl.ogg | a | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 1937462489 (0) | 4.80 | 1.00 | 'Say please, go on, beg nurse like a good little cunt.' |
| matron_16_b_menace.ogg | b | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1796354713 (1) | 5.31 | 1.00 | 'Say please. Go on, beg nurse like a good little cunt.' |
| matron_16_b_snarl.ogg | b | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 82150953 (0) | 2.89 | 1.00 | 'Say please, go on, beg nurse like a good little cunt.' |
| matron_16_c_menace.ogg | c | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 3246644106 (0) | 6.57 | 1.00 | 'Say please. Go on. Beg nurse like a good little cunt.' |
| matron_16_c_snarl.ogg | c | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 2671823430 (0) | 3.66 | 1.00 | 'Say please, go on. Beg nurse like a good little cunt.' |
| matron_27_a_menace.ogg | a | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 3623838718 (0) | 1.46 | 1.00 | 'waste not.' |
| matron_27_a_whisper.ogg | a | 27 'Waste not.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 4132011816 (0) | 1.12 | 1.00 | 'waste not.' |
| matron_27_b_menace.ogg | b | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 4080819850 (3) | 1.05 | 1.00 | 'waste not.' |
| matron_27_b_whisper.ogg | b | 27 'Waste not.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 340578128 (7) | 1.48 | 1.00 | 'waste not.' |
| matron_27_c_menace.ogg | c | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2966295212 (4) | 2.41 | 1.00 | 'Waste not.' |
| matron_27_c_whisper.ogg | c | 27 'Waste not.' | whisper | 0.25 | 0.2 | 0.7 | -23 | 273317788 (4) | 2.44 | 1.00 | 'Waste not.' |

Deliveries are what the settings are meant to give, not yet heard by anyone: `singsong` the sweet
voice; `whisper` lips to the ear (a hush played quiet, not an unvoiced whisper: Chatterbox has none);
`snarl` the mask off; `menace` no lift, no hurry.
