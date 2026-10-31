# Her voice: the bank

Written by src/voice/generate_matron_voice.py. Each clip is Chatterbox TTS (MIT licence) speaking one
line from 03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md in the voice of a reference sentence spoken by
a Piper voice (an AI voice, never a recording of a real person), made older and rougher after the speech
(pitch -2.0 semitones, tempo 0.93, saturation drive 1.5; nothing mixed in). Whisper checked that the clip as
shipped says the line. Three deliveries of every line, so no line repeats soon.

## Voice

- Piper en_US-kristin-medium (https://huggingface.co/rhasspy/piper-voices/tree/5041a7a49ae5b3a0f30e6837769eb03fdbb231bd/en/en_US/kristin/medium), trained from scratch on LibriVox recordings, about 11.5 hours (https://librivox.org), licence: public domain
- speech: Chatterbox TTS 0.1.7, ResembleAI/chatterbox @ 5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18 (MIT licence)
- check: openai-whisper small.en

## Clips

| clip | line | delivery | exaggeration | cfg_weight | temperature | level dB | seed (take) | seconds | match | Whisper heard |
|---|---|---|---|---|---|---|---|---|---|---|
| matron_01_hushed.ogg | 1 'I can hear you breathing, sweetheart. Little piggy breaths.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 2668836540 (0) | 3.78 | 1.00 | 'I can hear you breathing sweetheart, little piggy breaths.' |
| matron_01_menace.ogg | 1 'I can hear you breathing, sweetheart. Little piggy breaths.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1373360735 (1) | 3.64 | 1.00 | 'I can hear you breathing sweetheart, little piggy breaths.' |
| matron_01_singsong.ogg | 1 'I can hear you breathing, sweetheart. Little piggy breaths.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3601864520 (0) | 3.39 | 1.00 | 'I can hear you breathing sweetheart, little piggy breaths.' |
| matron_02_hushed.ogg | 2 'Come out, come out. Nurse has your medicine.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 52673833 (1) | 3.64 | 1.00 | 'Come out, come out, nurse has your medicine.' |
| matron_02_menace.ogg | 2 'Come out, come out. Nurse has your medicine.' | menace | 0.3 | 0.35 | 0.6 | -14 | 3454753226 (0) | 3.29 | 1.00 | 'Come out, come out. Nurse has your medicine.' |
| matron_02_singsong.ogg | 2 'Come out, come out. Nurse has your medicine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2704197048 (0) | 3.26 | 1.00 | 'Come out, come out, nurse has your medicine.' |
| matron_03_hushed.ogg | 3 'You climb like you shit. Fast and frightened.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 2615600769 (0) | 3.53 | 1.00 | 'You climb like you shit. Fast and frightened.' |
| matron_03_menace.ogg | 3 'You climb like you shit. Fast and frightened.' | menace | 0.3 | 0.35 | 0.6 | -14 | 573125364 (0) | 3.77 | 1.00 | 'You climb like you shit. Fast and frightened.' |
| matron_03_singsong.ogg | 3 'You climb like you shit. Fast and frightened.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3565545723 (2) | 2.99 | 1.00 | 'You climb like you shit, fast and frightened.' |
| matron_04_hushed.ogg | 4 "Every floor I've ever mopped, I've mopped boys like you off it." | hushed | 0.25 | 0.2 | 0.7 | -23 | 2536807636 (2) | 4.66 | 1.00 | "Every floor I've ever mopped, I've mopped boys like you off it." |
| matron_04_menace.ogg | 4 "Every floor I've ever mopped, I've mopped boys like you off it." | menace | 0.3 | 0.35 | 0.6 | -14 | 3237292429 (0) | 4.42 | 1.00 | "Every floor I've ever mopped. I've mopped boys like you off it." |
| matron_04_singsong.ogg | 4 "Every floor I've ever mopped, I've mopped boys like you off it." | singsong | 0.85 | 0.3 | 0.9 | -14 | 1309892696 (0) | 4.19 | 1.00 | "Every floor I've ever mopped, I've mopped boys like you off it." |
| matron_05_hushed.ogg | 5 'Was that you? That little squeak? Do it again for Nurse.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 2029451242 (2) | 4.39 | 1.00 | 'Was that you, that little squeak? Do it again for nurse.' |
| matron_05_menace.ogg | 5 'Was that you? That little squeak? Do it again for Nurse.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1479569957 (1) | 4.29 | 1.00 | 'Was that you? That little squeak? Do it again for nurse.' |
| matron_05_singsong.ogg | 5 'Was that you? That little squeak? Do it again for Nurse.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1001934107 (2) | 3.86 | 1.00 | 'Was that you? That little squeak. Do it again for a nurse.' |
| matron_06_hushed.ogg | 6 "Hide-and-seek. I always win. Ask the others. You can't, can you." | hushed | 0.25 | 0.2 | 0.7 | -23 | 2110729669 (0) | 5.88 | 1.00 | "hide and seek. I always win. Ask the others. You can't, can you?" |
| matron_06_menace.ogg | 6 "Hide-and-seek. I always win. Ask the others. You can't, can you." | menace | 0.3 | 0.35 | 0.6 | -14 | 3288398256 (0) | 6.33 | 1.00 | "Hide and seek. I always win. Ask the others. You can't, can you?" |
| matron_06_singsong.ogg | 6 "Hide-and-seek. I always win. Ask the others. You can't, can you." | singsong | 0.85 | 0.3 | 0.9 | -14 | 992063357 (3) | 5.63 | 1.00 | "Hide and seek. I always win. Ask the others. You can't can you?" |
| matron_07_menace.ogg | 7 "There you are. Look at that face. Mommy's little fuck-up." | menace | 0.3 | 0.35 | 0.6 | -14 | 734171790 (0) | 4.67 | 1.00 | "There you are. Look at that face. Mommy's little fuck up." |
| matron_07_singsong.ogg | 7 "There you are. Look at that face. Mommy's little fuck-up." | singsong | 0.85 | 0.3 | 0.9 | -14 | 965587624 (0) | 3.56 | 1.00 | "There you are. Look at that face. Mommy's little fuck up." |
| matron_07_snarl.ogg | 7 "There you are. Look at that face. Mommy's little fuck-up." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2454169208 (3) | 3.49 | 1.00 | "There you are. Look at that face. Mommy's little fuck up." |
| matron_08_menace.ogg | 8 "Oh, don't run. You'll only die tired." | menace | 0.3 | 0.35 | 0.6 | -14 | 3670657283 (0) | 3.05 | 1.00 | "Oh, don't run. You'll only die tired." |
| matron_08_singsong.ogg | 8 "Oh, don't run. You'll only die tired." | singsong | 0.85 | 0.3 | 0.9 | -14 | 1282730432 (4) | 2.99 | 1.00 | "Oh, don't run. You'll only die tired." |
| matron_08_snarl.ogg | 8 "Oh, don't run. You'll only die tired." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2750417029 (1) | 2.72 | 1.00 | "Oh don't run, you'll only die tired." |
| matron_09_menace.ogg | 9 "Such a big boy, climbing so high. Who's going to wipe you off the bottom?" | menace | 0.3 | 0.35 | 0.6 | -14 | 889954877 (0) | 4.84 | 1.00 | "Such a big boy, climbing so high. Who's going to wipe you off the bottom?" |
| matron_09_singsong.ogg | 9 "Such a big boy, climbing so high. Who's going to wipe you off the bottom?" | singsong | 0.85 | 0.3 | 0.9 | -14 | 3502006710 (0) | 4.01 | 1.00 | "Such a big boy climbing so high, who's going to wipe you off the bottom?" |
| matron_09_snarl.ogg | 9 "Such a big boy, climbing so high. Who's going to wipe you off the bottom?" | snarl | 1.1 | 0.4 | 0.85 | -14 | 763514236 (2) | 5.36 | 1.00 | "such a big boy climbing so high who's going to wipe you off the bottom" |
| matron_10_menace.ogg | 10 'Look at you shaking. Did you wet yourself? You did. I can smell it from here.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1851494797 (1) | 6.05 | 1.00 | 'Look at you shaking. Did you wet yourself? You did. I can smell it from here.' |
| matron_10_singsong.ogg | 10 'Look at you shaking. Did you wet yourself? You did. I can smell it from here.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1684405843 (2) | 4.42 | 1.00 | 'Look at you shaking. Did you wet yourself? You did. I can smell it from here.' |
| matron_10_snarl.ogg | 10 'Look at you shaking. Did you wet yourself? You did. I can smell it from here.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 1292993006 (0) | 4.59 | 1.00 | 'Look at you shaking. Did you wet yourself? You did, I can smell it from here.' |
| matron_11_menace.ogg | 11 'Gotcha, gotcha, gotcha.' | menace | 0.3 | 0.35 | 0.6 | -14 | 412134153 (2) | 2.05 | 1.00 | 'Gotcha, gotcha, gotcha' |
| matron_11_singsong.ogg | 11 'Gotcha, gotcha, gotcha.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1638147999 (5) | 2.52 | 1.00 | 'Gotcha, gotcha, gotcha' |
| matron_11_snarl.ogg | 11 'Gotcha, gotcha, gotcha.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 762160443 (1) | 1.82 | 1.00 | 'Gotcha! Gotcha! Gotcha!' |
| matron_12_menace.ogg | 12 "Shhh. Shhh. Hold still or I'll hold you still." | menace | 0.3 | 0.35 | 0.6 | -14 | 1841530281 (5) | 4.12 | 1.00 | "Shh, shh, shh. Hold still or I'll hold you still." |
| matron_12_singsong.ogg | 12 "Shhh. Shhh. Hold still or I'll hold you still." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2287886540 (2) | 3.16 | 1.00 | "Shh! Shh! Hold still or I'll hold you still." |
| matron_12_snarl.ogg | 12 "Shhh. Shhh. Hold still or I'll hold you still." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2380908612 (2) | 3.69 | 1.00 | "Shh, shh, hold still or I'll hold you still." |
| matron_13_menace.ogg | 13 "Kick all you like, darling. I've had stronger legs than yours in this hand. Dead ones." | menace | 0.3 | 0.35 | 0.6 | -14 | 2238458510 (1) | 5.76 | 1.00 | "Kick all you like darling, I've had stronger legs than yours in this hand. Dead ones." |
| matron_13_singsong.ogg | 13 "Kick all you like, darling. I've had stronger legs than yours in this hand. Dead ones." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2331147545 (1) | 5.17 | 1.00 | "Kick all you like, darling. I've had stronger legs than yours in this hand. Dead ones." |
| matron_13_snarl.ogg | 13 "Kick all you like, darling. I've had stronger legs than yours in this hand. Dead ones." | snarl | 1.1 | 0.4 | 0.85 | -14 | 1945205042 (4) | 5.12 | 0.94 | "Pick all you like darling, I've had stronger legs than yours in this hand. Dead ones!" |
| matron_14_menace.ogg | 14 "Ankles like a little girl's. Snap, snap." | menace | 0.3 | 0.35 | 0.6 | -14 | 1739600375 (1) | 3.41 | 1.00 | 'Ankles like the little girls. Snap, snap.' |
| matron_14_singsong.ogg | 14 "Ankles like a little girl's. Snap, snap." | singsong | 0.85 | 0.3 | 0.9 | -14 | 1611530549 (6) | 2.49 | 1.00 | 'Ankles like a little girl. Snap, snap.' |
| matron_14_snarl.ogg | 14 "Ankles like a little girl's. Snap, snap." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2392409409 (7) | 2.83 | 1.00 | "Ankle's like a little girl's. Snap, snap." |
| matron_15_hushed.ogg | 15 "Look down. No, look. That's where the brave ones go." | hushed | 0.25 | 0.2 | 0.7 | -23 | 834506428 (1) | 4.87 | 1.00 | "Look down. No. Look. That's where the brave ones go." |
| matron_15_menace.ogg | 15 "Look down. No, look. That's where the brave ones go." | menace | 0.3 | 0.35 | 0.6 | -14 | 4285885023 (0) | 4.25 | 1.00 | "Look down. No, look. That's where the brave ones go." |
| matron_15_snarl.ogg | 15 "Look down. No, look. That's where the brave ones go." | snarl | 1.1 | 0.4 | 0.85 | -14 | 126272673 (0) | 3.71 | 1.00 | "Look down, no, look, that's where the brave ones go." |
| matron_16_hushed.ogg | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 2866856240 (4) | 3.98 | 1.00 | 'Say please, go on, beg nurse like a good little cunt.' |
| matron_16_menace.ogg | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | menace | 0.3 | 0.35 | 0.6 | -14 | 339923292 (0) | 4.37 | 1.00 | 'Say please, go on, beg nurse like a good little cunt.' |
| matron_16_snarl.ogg | 16 'Say please. Go on. Beg Nurse like a good little cunt.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 2801969630 (3) | 3.44 | 0.91 | 'Say please. Go on. Big nurse like a good little cunt.' |
| matron_17_hushed.ogg | 17 "Your mother isn't coming. Nobody's mother ever comes." | hushed | 0.25 | 0.2 | 0.7 | -23 | 893935233 (1) | 3.95 | 1.00 | "Your mother isn't coming. Nobody's mother ever comes." |
| matron_17_menace.ogg | 17 "Your mother isn't coming. Nobody's mother ever comes." | menace | 0.3 | 0.35 | 0.6 | -14 | 4219517538 (0) | 4.03 | 1.00 | "Your mother isn't coming. Nobody's mother ever comes." |
| matron_17_snarl.ogg | 17 "Your mother isn't coming. Nobody's mother ever comes." | snarl | 1.1 | 0.4 | 0.85 | -14 | 695266343 (0) | 4.10 | 1.00 | "Your mother isn't coming. Nobody's mother ever comes." |
| matron_18_hushed.ogg | 18 "You're heavier than you look. All that fear. It sits in the belly." | hushed | 0.25 | 0.2 | 0.7 | -23 | 3007591834 (0) | 4.58 | 1.00 | "You're heavier than you look, all that fear, it sits in the belly." |
| matron_18_menace.ogg | 18 "You're heavier than you look. All that fear. It sits in the belly." | menace | 0.3 | 0.35 | 0.6 | -14 | 176939503 (0) | 4.59 | 1.00 | "You're heavier than you look. All that fear, it sits in the belly." |
| matron_18_snarl.ogg | 18 "You're heavier than you look. All that fear. It sits in the belly." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2177766240 (1) | 3.97 | 1.00 | "You're heavier than you look. All that fear, it sits in the belly." |
| matron_19_menace.ogg | 19 'This is going to hurt me so much less than you.' | menace | 0.3 | 0.35 | 0.6 | -14 | 3846802129 (0) | 2.84 | 1.00 | 'This is going to hurt me so much less than you.' |
| matron_19_singsong.ogg | 19 'This is going to hurt me so much less than you.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 398632430 (0) | 2.74 | 1.00 | 'This is going to hurt me so much less than you.' |
| matron_19_snarl.ogg | 19 'This is going to hurt me so much less than you.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 3786553269 (0) | 2.55 | 1.00 | 'This is going to hurt me so much less than you.' |
| matron_20_menace.ogg | 20 "There. Now you'll never wave goodbye properly." | menace | 0.3 | 0.35 | 0.6 | -14 | 2159986458 (0) | 3.10 | 1.00 | "There. Now you'll never wave goodbye properly." |
| matron_20_singsong.ogg | 20 "There. Now you'll never wave goodbye properly." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2848286226 (7) | 3.25 | 1.00 | "There, now you'll never wave goodbye properly." |
| matron_20_snarl.ogg | 20 "There. Now you'll never wave goodbye properly." | snarl | 1.1 | 0.4 | 0.85 | -14 | 943894350 (7) | 2.92 | 0.86 | "There! Now you'll never make goodbye properly!" |
| matron_21_menace.ogg | 21 'Pop goes the weasel.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2171784456 (2) | 1.15 | 1.00 | 'Pop Goes The Weasel.' |
| matron_21_singsong.ogg | 21 'Pop goes the weasel.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3674443592 (1) | 1.26 | 1.00 | 'Pop goes the weasel.' |
| matron_21_snarl.ogg | 21 'Pop goes the weasel.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 3327794744 (1) | 2.09 | 0.75 | 'HAP! Goes the weasel.' |
| matron_22_menace.ogg | 22 "Ooh. Spirited. I'll enjoy taking that out of you." | menace | 0.3 | 0.35 | 0.6 | -14 | 2219513639 (0) | 3.98 | 1.00 | "Ooh. Spirited. I'll enjoy taking that out of you." |
| matron_22_singsong.ogg | 22 "Ooh. Spirited. I'll enjoy taking that out of you." | singsong | 0.85 | 0.3 | 0.9 | -14 | 3684330798 (0) | 3.94 | 1.00 | "Ooh, spirited, I'll enjoy taking that out of you." |
| matron_22_snarl.ogg | 22 "Ooh. Spirited. I'll enjoy taking that out of you." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2295337579 (0) | 4.21 | 1.00 | "Ooh, spirited! I'll enjoy taking that out of you." |
| matron_23_menace.ogg | 23 "Run along. I'll be right behind you. I'm always right behind you." | menace | 0.3 | 0.35 | 0.6 | -14 | 1804172313 (0) | 4.36 | 1.00 | "Run along, I'll be right behind you. I'm always right behind you." |
| matron_23_singsong.ogg | 23 "Run along. I'll be right behind you. I'm always right behind you." | singsong | 0.85 | 0.3 | 0.9 | -14 | 926421463 (1) | 4.72 | 1.00 | "Run along. I'll be right behind you. I'm always right behind you." |
| matron_23_snarl.ogg | 23 "Run along. I'll be right behind you. I'm always right behind you." | snarl | 1.1 | 0.4 | 0.85 | -14 | 3903600318 (1) | 3.50 | 1.00 | "Run along, I'll be right behind you. I'm always right behind you." |
| matron_24_menace.ogg | 24 "You hit like your father. Don't you dare cry about it." | menace | 0.3 | 0.35 | 0.6 | -14 | 2304075616 (0) | 3.59 | 1.00 | "You hit like your father. Don't you dare cry about it." |
| matron_24_singsong.ogg | 24 "You hit like your father. Don't you dare cry about it." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2864930157 (7) | 3.50 | 1.00 | "You hit like your father, don't you dare cry about it!" |
| matron_24_snarl.ogg | 24 "You hit like your father. Don't you dare cry about it." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2342559598 (5) | 3.51 | 1.00 | "You hit like your father! Don't you dare cry about it!" |
| matron_25_hushed.ogg | 25 'Clever boy. Clever, clever. Clever boys taste the same.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 3747141675 (0) | 5.50 | 1.00 | 'Clever boy, clever, clever, clever boys taste the same.' |
| matron_25_menace.ogg | 25 'Clever boy. Clever, clever. Clever boys taste the same.' | menace | 0.3 | 0.35 | 0.6 | -14 | 294665416 (1) | 4.95 | 1.00 | 'Clever boy, clever, clever, clever boys taste the same.' |
| matron_25_singsong.ogg | 25 'Clever boy. Clever, clever. Clever boys taste the same.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2936177825 (0) | 4.89 | 1.00 | 'Clever boy. Clever, clever. Clever boys taste the same.' |
| matron_26_hushed.ogg | 26 "I'll wait. I've got all the time in the world. You've got a body." | hushed | 0.25 | 0.2 | 0.7 | -23 | 879715112 (0) | 4.43 | 1.00 | "I'll wait. I've got all the time in the world. You've got a body." |
| matron_26_menace.ogg | 26 "I'll wait. I've got all the time in the world. You've got a body." | menace | 0.3 | 0.35 | 0.6 | -14 | 2376120157 (0) | 4.42 | 1.00 | "I'll wait. I've got all the time in the world. You've got a body." |
| matron_26_singsong.ogg | 26 "I'll wait. I've got all the time in the world. You've got a body." | singsong | 0.85 | 0.3 | 0.9 | -14 | 3634124369 (0) | 4.32 | 1.00 | "I'll wait. I've got all the time in the world. You've got a body!" |
| matron_27_hushed.ogg | 27 'Waste not.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 2896836736 (1) | 0.91 | 1.00 | 'Waste not.' |
| matron_27_menace.ogg | 27 'Waste not.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1650646115 (0) | 1.16 | 1.00 | 'waste not.' |
| matron_27_singsong.ogg | 27 'Waste not.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3713896349 (7) | 1.04 | 1.00 | 'Wasted not.' |
| matron_28_menace.ogg | 28 'Peekaboo. I see your little heart going.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2473166830 (0) | 2.84 | 1.00 | 'Peekaboo. I see your little heart going.' |
| matron_28_singsong.ogg | 28 'Peekaboo. I see your little heart going.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3752060003 (2) | 2.48 | 1.00 | 'Peekaboo! I see your little heart going!' |
| matron_28_snarl.ogg | 28 'Peekaboo. I see your little heart going.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 503224565 (0) | 2.32 | 1.00 | 'Peekaboo, I see your little heart going.' |
| matron_29_menace.ogg | 29 "You came all this way up just to meet me. That's sweet. That's very, very stupid." | menace | 0.3 | 0.35 | 0.6 | -14 | 2091642064 (0) | 5.62 | 1.00 | "He came all this way out just to meet me. That's sweet. That's very, very stupid." |
| matron_29_singsong.ogg | 29 "You came all this way up just to meet me. That's sweet. That's very, very stupid." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2852702496 (0) | 5.98 | 1.00 | "You came all this way up just to meet me. That's sweet, that's very, very stupid." |
| matron_29_snarl.ogg | 29 "You came all this way up just to meet me. That's sweet. That's very, very stupid." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2105719840 (1) | 5.62 | 1.00 | "You came all this way up just to meet me. That's sweet. That's very, very stupid." |
| matron_30_menace.ogg | 30 'Up we come. Upsy-daisy, piss-pants.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1180556483 (4) | 3.16 | 0.86 | 'Up we come, Hopsie Daisy Pissed Pants.' |
| matron_30_singsong.ogg | 30 'Up we come. Upsy-daisy, piss-pants.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 3929483828 (0) | 2.96 | 0.86 | 'Up we come, Opsy Daisy, piss pants.' |
| matron_30_snarl.ogg | 30 'Up we come. Upsy-daisy, piss-pants.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 1239699923 (0) | 2.64 | 0.86 | 'Up we come, Ozzy Daisy, Pissed Pants!' |
| matron_31_menace.ogg | 31 'Mine. Mine, mine, mine.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2935124964 (0) | 3.01 | 1.00 | 'Mine, Mine, Mine, Mine' |
| matron_31_singsong.ogg | 31 'Mine. Mine, mine, mine.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 1905419867 (0) | 2.78 | 1.00 | 'Mine, mine, mine, mine.' |
| matron_31_snarl.ogg | 31 'Mine. Mine, mine, mine.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 1587496336 (0) | 2.25 | 1.00 | 'Mine, Mine, Mine, Mine!' |
| matron_32_hushed.ogg | 32 'I could let go. I could. Should I? Say "ma\'am."' | hushed | 0.25 | 0.2 | 0.7 | -23 | 2332896260 (1) | 3.75 | 1.00 | "I could let go. I could. Should I? Say, ma'am." |
| matron_32_menace.ogg | 32 'I could let go. I could. Should I? Say "ma\'am."' | menace | 0.3 | 0.35 | 0.6 | -14 | 1170589927 (0) | 3.67 | 1.00 | "I could let go. I could. Should I? Say ma'am." |
| matron_32_snarl.ogg | 32 'I could let go. I could. Should I? Say "ma\'am."' | snarl | 1.1 | 0.4 | 0.85 | -14 | 1729263957 (0) | 4.07 | 1.00 | "I can let go. I could. Should I say ma'am?" |
| matron_33_hushed.ogg | 33 "Hush now. Nobody down there can hear you. I've checked." | hushed | 0.25 | 0.2 | 0.7 | -23 | 331904940 (0) | 4.08 | 1.00 | "Hush now. Nobody down there can hear you. I've checked." |
| matron_33_menace.ogg | 33 "Hush now. Nobody down there can hear you. I've checked." | menace | 0.3 | 0.35 | 0.6 | -14 | 2852627417 (0) | 4.10 | 1.00 | "Hush now. Nobody down there can hear you. I've checked." |
| matron_33_snarl.ogg | 33 "Hush now. Nobody down there can hear you. I've checked." | snarl | 1.1 | 0.4 | 0.85 | -14 | 2567620643 (6) | 3.33 | 0.90 | 'Hush now, nobody down there can hear you. I checked!' |
| matron_34_hushed.ogg | 34 "Such a long way. You'll have time to think about what a disappointment you were." | hushed | 0.25 | 0.2 | 0.7 | -23 | 4044647637 (0) | 4.76 | 1.00 | "such a long way. You'll have time to think about what a disappointment you were." |
| matron_34_menace.ogg | 34 "Such a long way. You'll have time to think about what a disappointment you were." | menace | 0.3 | 0.35 | 0.6 | -14 | 2799032716 (2) | 4.91 | 1.00 | "such a long way. You'll have time to think about what a disappointment you were." |
| matron_34_snarl.ogg | 34 "Such a long way. You'll have time to think about what a disappointment you were." | snarl | 1.1 | 0.4 | 0.85 | -14 | 336068831 (0) | 5.14 | 1.00 | "such a long way you'll have time to think about what a disappointment you were" |
| matron_35_menace.ogg | 35 "Hold this for me. Oh. You can't." | menace | 0.3 | 0.35 | 0.6 | -14 | 2803487646 (0) | 2.98 | 1.00 | "Hold this for me. Oh, you can't." |
| matron_35_singsong.ogg | 35 "Hold this for me. Oh. You can't." | singsong | 0.85 | 0.3 | 0.9 | -14 | 1922325796 (0) | 2.98 | 1.00 | "Hold this for me. Oh, you can't." |
| matron_35_snarl.ogg | 35 "Hold this for me. Oh. You can't." | snarl | 1.1 | 0.4 | 0.85 | -14 | 57873564 (0) | 2.60 | 1.00 | "Hold this for me. Oh, you can't!" |
| matron_36_menace.ogg | 36 "Quiet, quiet. Big boys don't scream. Little ones do. Scream for me." | menace | 0.3 | 0.35 | 0.6 | -14 | 1278109853 (0) | 6.53 | 1.00 | "Quiet, quiet, big boys don't scream, little ones do, scream for me." |
| matron_36_singsong.ogg | 36 "Quiet, quiet. Big boys don't scream. Little ones do. Scream for me." | singsong | 0.85 | 0.3 | 0.9 | -14 | 84578260 (0) | 5.66 | 1.00 | "Quiet, quiet, big boys don't scream, little ones do, scream for me." |
| matron_36_snarl.ogg | 36 "Quiet, quiet. Big boys don't scream. Little ones do. Scream for me." | snarl | 1.1 | 0.4 | 0.85 | -14 | 1308170447 (1) | 4.94 | 1.00 | "Quiet! Quiet! Big boys don't scream. Little ones do. Scream for me!" |
| matron_37_menace.ogg | 37 "Oh, you little shit. You'll pay for that in pieces." | menace | 0.3 | 0.35 | 0.6 | -14 | 2750153635 (0) | 3.26 | 1.00 | "Oh you little shit, you'll pay for that in pieces." |
| matron_37_singsong.ogg | 37 "Oh, you little shit. You'll pay for that in pieces." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2662319035 (0) | 3.71 | 1.00 | "Oh, you little shit. You'll pay for that in pieces." |
| matron_37_snarl.ogg | 37 "Oh, you little shit. You'll pay for that in pieces." | snarl | 1.1 | 0.4 | 0.85 | -14 | 1518472332 (1) | 2.98 | 1.00 | "Oh you little shit, you'll pay for that in pieces." |
| matron_38_menace.ogg | 38 'Fine. Go. Your legs will tire before my hands do.' | menace | 0.3 | 0.35 | 0.6 | -14 | 1390893102 (0) | 4.19 | 1.00 | 'Fine. Go. Your legs will tire before my hands do.' |
| matron_38_singsong.ogg | 38 'Fine. Go. Your legs will tire before my hands do.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2604428380 (1) | 4.51 | 1.00 | 'Fine, go, your legs will tire before my hands do' |
| matron_38_snarl.ogg | 38 'Fine. Go. Your legs will tire before my hands do.' | snarl | 1.1 | 0.4 | 0.85 | -14 | 2235286365 (1) | 3.60 | 1.00 | 'Fine, go. Your legs will tire before my hands do.' |
| matron_39_hushed.ogg | 39 'Tick, tock. Nurse never sleeps.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 1944967155 (1) | 2.92 | 1.00 | 'Tick tock, nurse never sleeps.' |
| matron_39_menace.ogg | 39 'Tick, tock. Nurse never sleeps.' | menace | 0.3 | 0.35 | 0.6 | -14 | 3173331728 (0) | 3.00 | 1.00 | 'Tick. Tock. Nurse never sleeps.' |
| matron_39_singsong.ogg | 39 'Tick, tock. Nurse never sleeps.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 10035251 (1) | 2.95 | 1.00 | 'Tick Tock! Nurse never sleeps!' |
| matron_40_hushed.ogg | 40 "I'll find you by the smell. Fear goes sour, sweetheart." | hushed | 0.25 | 0.2 | 0.7 | -23 | 3519338796 (0) | 3.56 | 1.00 | "I'll find you by the smell. Fear goes sour, sweetheart." |
| matron_40_menace.ogg | 40 "I'll find you by the smell. Fear goes sour, sweetheart." | menace | 0.3 | 0.35 | 0.6 | -14 | 2248502389 (2) | 3.88 | 1.00 | "I'll find you by the smell. Fear goes sour, sweetheart." |
| matron_40_singsong.ogg | 40 "I'll find you by the smell. Fear goes sour, sweetheart." | singsong | 0.85 | 0.3 | 0.9 | -14 | 2537827436 (0) | 4.02 | 1.00 | "I'll find you by the smell. Fear goes sour, sweetheart." |
| matron_41_hushed.ogg | 41 'Mind the stairs, darling. Mind them all the way down.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 1040626194 (0) | 3.77 | 1.00 | 'Mind the stairs darling, mind them all the way down.' |
| matron_41_menace.ogg | 41 'Mind the stairs, darling. Mind them all the way down.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2278123111 (0) | 3.53 | 1.00 | 'Mind the stairs darling, mind them all the way down.' |
| matron_41_singsong.ogg | 41 'Mind the stairs, darling. Mind them all the way down.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2078691477 (1) | 3.10 | 1.00 | 'Mind the stairs darling, mind them all the way down.' |
| matron_42_hushed.ogg | 42 'Was it your knee that cracked, or the steel? I can wait to find out.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 3576760593 (0) | 4.97 | 1.00 | 'Was it your knee that cracked, or the steel? I can wait to find out.' |
| matron_42_menace.ogg | 42 'Was it your knee that cracked, or the steel? I can wait to find out.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2196814920 (2) | 4.86 | 1.00 | 'Was it your knee that cracked, or the steel? I can wait to find out.' |
| matron_42_singsong.ogg | 42 'Was it your knee that cracked, or the steel? I can wait to find out.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2081584874 (4) | 3.93 | 0.93 | 'Was it your knee that cracked? Or the seal? I can wait to find out.' |
| matron_43_hushed.ogg | 43 'Tiptoe, tiptoe. I hear tiptoes in my sleep.' | hushed | 0.25 | 0.2 | 0.7 | -23 | 1307857593 (1) | 3.86 | 1.00 | 'Tiptoe, tiptoe, I hear tiptoes in my sleep.' |
| matron_43_menace.ogg | 43 'Tiptoe, tiptoe. I hear tiptoes in my sleep.' | menace | 0.3 | 0.35 | 0.6 | -14 | 2201794138 (0) | 3.64 | 1.00 | 'Tiptoe, tiptoe. I hear tiptoes in my sleep.' |
| matron_43_singsong.ogg | 43 'Tiptoe, tiptoe. I hear tiptoes in my sleep.' | singsong | 0.85 | 0.3 | 0.9 | -14 | 2547895818 (1) | 2.50 | 1.00 | 'Tiptoe, tiptoe, I hear tiptoes in my sleep.' |

Deliveries are what the settings are meant to give, not yet heard by anyone: `singsong` the sweet
voice; `hushed` lips to the ear (a hush played quiet, not an unvoiced whisper: Chatterbox has none);
`snarl` the mask off; `menace` no lift, no hurry.
