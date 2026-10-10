# Recorded timber fracture excerpt

Author: LampEight. Publisher: Freesound. Sound ID: 400638.

Source: [Wood panel board cracking snapping](https://freesound.org/people/LampEight/sounds/400638/).
The primary sound page identifies the recording as **Creative Commons 0** and links to [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).
The author describes recording old wooden fencing being broken outdoors.

`400638_706234-hq.mp3` is the unchanged public high-quality preview.
`timber_snap_400638_preview.wav` is a **1.10-second excerpt decoded from that MP3 preview**, not the original 32-bit stereo WAV advertised on the source page.

The excerpt covers 5.10–6.20 seconds of the preview. FFmpeg 8.0.1-3ubuntu2 in the existing Ubuntu proot container trims and resets timestamps, applies a 1 ms fade-in and 20 ms fade-out, downmixes stereo to mono, removes metadata and writes 48 kHz signed 16-bit PCM. No gain adjustment or normalization is applied.

The final derivative has 52,800 mono samples, an absolute PCM peak of 12,480/32,768 and no full-scale samples. Acquisition, source and derivative hashes, exact conversion settings and rejected draft details are recorded in `provenance.json`. The separate environmental Termux FFmpeg startup failure was avoided using the existing Ubuntu tool; no dependency was installed.

No subjective audition or sound-quality claim is made. Signal validation does not establish final in-game audibility or a physically calibrated timber-fracture recording.
