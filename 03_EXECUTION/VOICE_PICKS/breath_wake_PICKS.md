# Her wake and his breath: candidates for Cory to pick by ear

Issue #416. Every file here is a cleared CC0 recording (source under each
name), cut and leveled the same way the bank is. Nothing here is
synthesized in code, and nothing here is a real person asked to record:
these are existing public-domain recordings, the same class as the bank's
recorded shrieks, growls and sniffs. Her spoken lines (words) are a
different set — those stay AI-generated the fear-yells way under the J-089
gate, never a recording of a real person.

## What ships now, until you pick

- **Her wake:** `matron_wake_0..3.ogg` in `godot/presentation/audio/matron/`. Nine gasps from
  superEGsonic's "gasps_female" (784717) over hinchinbrook's iron-door
  "Metallic Groan" (496836), four takes, each its own gasp and its own
  window of the groan, a 28 Hz sub swell under both (rendered offline,
  not a human sound). Take 0 is the first gasp; the rest are the other
  gasps in the same file, so no two wakes are alike.
- **His breath:** `breath_in_0..2.ogg` / `breath_out_0..2.ogg` in
  `godot/presentation/audio/breath/`, cut from craigsmith's "R17-51-Man Breathing" (482831), a
  regular breathing recording. The in is the bright, sharp, falling burst
  (air rushing in through the mouth); the out the dark, loud, rising burst
  (air pushed out). That reading is from the recording's shape (bright
  bursts center near 2,000 Hz and fall, dark ones near 1,400 Hz and rise);
  if your ear says the halves are swapped, it is a rename — the cuts are
  per burst, nothing is re-cut.

## What to listen for

- **Wake:** does the gasp land after the hush and before the groan peaks,
  so she sounds like she is waking, not like she is being hurt? Is the
  steel bed too loud against the gasp? The sub is there to carry the
  weight on a phone speaker — is it felt, or does it muddy the gasp?
- **Breath:** at rest, does the cycle sound like one body breathing, not
  two clips alternating? At climb and winded, does the effort read without
  sounding like a panting actor?

## The candidates

### Her wake — alternates

| File | What changes | Source |
|---|---|---|
| `wake_alt_gasp_a.ogg` | Softer, repeated gasps instead of the sharp ones (same iron groan bed) | Alivvie "gasping" (436781), CC0 |
| `wake_alt_bed.ogg` | Longer metal creak as the bed (same sharp gasps) | AyaDrevis "Long Metal creaking" (878048), CC0 |
| `wake_alt_gasp_b.ogg` | One single sharp gasp, 0.6 s, instead of a drawn-out one (same iron groan bed) | Reitanna "close call gasp.wav" (253773), CC0 |

### His breath — alternates, two per state

| File | State | Source | Note |
|---|---|---|---|
| `breath_rest_a.ogg` | Rest | craigsmith 482831, 5.5–9.0 s | The shipped recording, heard whole. |
| `breath_rest_b.ogg` | Rest | looplihood "Quiet Sleep Breathing" (874178), 44–48 s, CC0 | Very regular sleep breathing. |
| `breath_climb_a.ogg` | Climbing | ShaneF91 "heavy_breathing_1.wav" (386573), 0.5–4.0 s, CC0 | Strong, rhythmic heavy breathing. |
| `breath_climb_b.ogg` | Climbing | randbsoundbites "Heavy breathing" (844287), 2.0–6.0 s, CC0 | Same shape, but very quiet — needs a level lift if picked. |
| `breath_winded_a.ogg` | Winded | Sadiquecat "Catching breath (Genuine)" (792641), whole 2.38 s, CC0 | One genuine recovery breath, loud. |
| `breath_winded_b.ogg` | Winded | florianreichelt "heavy breathing while running" (456037), 17.0–20.5 s, CC0 | Long and loud — the preview itself clips, and there may be court sounds in it. Listen first. |

## How the picks are made

`tools/audio/matron_wake_breath_takes.py` re-downloads every source above,
refuses a changed byte (SHA-256 pinned in the script), and reproduces the
bank files and these candidates. Tell us the file names you like and the
bank is rebuilt from them; until then the shipped set above plays.

## Other human sounds still generated in code (issue item 4)

Grepped `sound_bank.gd` and the audio folder. What remains synthesized:

- `_bone_crack` — the climber's bones giving way (gore foley). Still
  generated in code; the issue's law covers gore foley, so this one is
  listed for your call, not fixed here.
- The five `_hit_*` generators — struck-body and struck-material impacts
  (mapped via `audio_director.gd` `IMPACT_CLIPS`). Whether those count as
  gore foley is a judgement call; listed here for it.
- `_glottal` and `_formants` — voice-synthesis helpers with no callers:
  dead code, not a live sound. Left in place; removing them is a separate
  cleanup, not this job.
- `_add_glide`'s one call is in `_her_step` (a footfall, not a voice).
  `_peak`, `_saturate` and `_rough` feed the non-human `_her_bed`.
- No other `.gd` in the audio folder synthesizes audio (the director and
  `matron_sound.gd` hold only data arrays).
