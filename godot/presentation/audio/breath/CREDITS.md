# The climber's recorded breath: credits

His breath is recorded now, not generated in code (issue #416). Three
in-halves and three out-halves, cut from one man's breathing, so a cycle
sounds like one body: the in is the bright, sharp, falling burst (air
rushing in through the mouth), the out the dark, loud, rising burst (air
pushed out). That reading is from the recording's shape (the bright bursts
center near 2,000 Hz and fall, the dark ones near 1,400 Hz and rise); if a
listener's ear says the halves are swapped, it is a rename — the cuts are
per burst, nothing is re-cut.

Prepared by `tools/audio/matron_wake_breath_takes.py`, which re-downloads
the source below, refuses a changed byte (SHA-256 pinned in the script), and
reproduces these files. Each file is mono Ogg Vorbis at 22,050 Hz, cut from
its source, high-passed at 75 Hz, faded in over 6 ms and out over 40 ms,
and peak-normalized to -1.1 dBFS (0.88). The director paces the halves by
his effort, as it did the synthesized ones.

| File | Source | Title | Author | License | Part used (s of the source) |
|---|---|---|---|---|---|
| `breath_in_0.ogg` | https://freesound.org/people/craigsmith/sounds/482831/ | R17-51-Man Breathing.wav | craigsmith | CC0 1.0 | 6.90–7.24 |
| `breath_in_1.ogg` | https://freesound.org/people/craigsmith/sounds/482831/ | R17-51-Man Breathing.wav | craigsmith | CC0 1.0 | 14.60–14.94 |
| `breath_in_2.ogg` | https://freesound.org/people/craigsmith/sounds/482831/ | R17-51-Man Breathing.wav | craigsmith | CC0 1.0 | 19.70–20.04 |
| `breath_out_0.ogg` | https://freesound.org/people/craigsmith/sounds/482831/ | R17-51-Man Breathing.wav | craigsmith | CC0 1.0 | 5.75–6.17 |
| `breath_out_1.ogg` | https://freesound.org/people/craigsmith/sounds/482831/ | R17-51-Man Breathing.wav | craigsmith | CC0 1.0 | 13.40–13.82 |
| `breath_out_2.ogg` | https://freesound.org/people/craigsmith/sounds/482831/ | R17-51-Man Breathing.wav | craigsmith | CC0 1.0 | 15.95–16.37 |

The other breath candidates Cory has not picked — two per state (rest,
climbing, winded) — are in `03_EXECUTION/VOICE_PICKS/`
with `breath_wake_PICKS.md`. Until he picks, the bank ships the set above (the rest
recording); per-state sets are the follow-up once he picks.
