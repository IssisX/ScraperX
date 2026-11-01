# M6 S0 — upright-feet measurement on a fresh `touch_matron_act` recording

Measurement only. No gameplay change, no test touched.

## Recording

- Date: 2026-11-01, afternoon America/Chicago (CST).
- Base: trunk `ScraperX-Claude @ e6f46188`, plus this branch's S0 probe slice
  (`upright_feet_report` in `tools/matron_probe/score.py`, commit `39bb7b27`).
  The native binary was built from that tree; the only difference from trunk
  is the scoring script, so the sim behavior measured is trunk behavior.
- Run: Godot 4.7 headless, `--fixed-fps 60`, probe `tools/matron_probe/probe.gd`,
  `--uitest=touch_matron_act`. Result: **UITEST PASS**, stages seen `[0, 1, 2]`,
  deaths 1, scheme gestures, swipes_up 7, swipes_down 2.
- Frames: 18,227 drawn frames of 20,142 records scored (stride 1, no replay,
  no held-still test).
- Raw recording (200 MB, not committed): `~/workspace/grok-once-probe-act/`
  (`frames.csv`, `bones.bin`, `mesh.bin`, `meta.json`).
  Full score log (not committed): `~/workspace/grok-once-score.log`.

## Result — M6 S0 upright feet only

Feet, not palms. All fours, kneeling, and turn lunges excluded.
Thresholds are the slice's: same 0.03 m / skate allowances as `limbs_report`.

| scenario | foot | frames | planted pairs | skate (worst) | median planted move | slides > 0.05 m (longest) | both-feet-float | foot float (worst) | sink (worst) |
|---|---|---|---|---|---|---|---|---|---|
| standing | foot_l | 11485 | 11025 | 5 (0.107 m, 2.13x allow) | 0.0000 m | 10 (0.721 m) | 198 | 216 (0.163 m) | 209 (0.212 m) |
| standing | foot_r | 11485 | 10223 | 8 (0.103 m, 2.05x allow) | 0.0000 m | 15 (1.237 m) | 198 | 995 (0.138 m) | 230 (0.212 m) |
| walking | foot_l | 3496 | 3000 | 12 (0.287 m, 3.21x allow) | 0.0369 m | 31 (54.732 m) | 224 | 365 (0.812 m) | 69 (0.085 m) |
| walking | foot_r | 3496 | 3018 | 11 (0.219 m, 3.87x allow) | 0.0363 m | 29 (27.693 m) | 224 | 365 (0.321 m) | 60 (0.095 m) |
| turning | foot_l | 207 | 104 | 1 (0.083 m, 1.66x allow) | 0.0009 m | 1 (0.273 m) | 47 | 47 (0.049 m) | 46 (0.126 m) |
| turning | foot_r | 207 | 90 | 0 (0.000 m) | 0.0017 m | 1 (0.077 m) | 47 | 83 (0.082 m) | 28 (0.115 m) |
| moving_support | — | no upright frames | — | — | — | — | — | — | — |

Planted `|gap|` median 0.0058–0.0161 m, worst 0.0299–0.0300 m in all rows.

## Reading (numbers above, not a new claim)

- Standing: planted feet are essentially still (median planted move 0.0000 m);
  the defect there is episodic — a handful of skate episodes just over 2x
  allowance and slide episodes up to ~1.2 m.
- Walking: the defect is structural, not episodic. Feet classified as planted
  still travel ~0.037 m per frame on median, skate worsts reach 3.21x/3.87x
  allowance, and the longest slide episodes total 54.7 m (foot_l) and
  27.7 m (foot_r) — the "walking foot drag is the real defect" from the
  2026-10-30 S0 branch, now measured on current trunk.
- Turning: small (207 frames), near-clean.

## Still owed, not claimed here

- `moving_support` has no upright frames in this act, so support-relative
  foot drift is still UNMEASURED by this probe (the probe records no support
  velocity; world displacement there is not support-relative drift). S4's
  cage-rate measurement covers exported support velocity matching, not foot
  drift. A moving-support act recording is the remaining S0 gap.
