# Look rubric: the Matron climbing down a ladder (J-068)

Frames: `screenshots/ladder_grade_ladder_<shot>.png`, 960x828 under xvfb (`gl_compatibility`,
60 fps fixed), scenario `look_matron_grade_ladder` (`ui_test_look.gd`), by the proof step "Render the
eye's fight and ladder stills". She is set on the 11 m first floor by the grade ladder (x 12.0,
z -123.6) and the simulation walks her down it to grade on its own scanned plan; nothing poses her
figure. Each still is from a camera placed about her drawn body, the simulation held for it:

- `down`: mid-descent, about 7 m up (camera at azimuth 160 from her drawn facing).
- `down_side`: the same tick from her side (azimuth 90): hands and feet against the rungs edge on.
- `down_low`: about 4 m up on the same descent, from the same side as `down`.

Written 2026-11-08 from frames rendered locally on trunk. Every line must hold in its frame; each is one
thing you can point at. What fails is what a person would call fake.

1. Her hands are closed round rungs: fingers wrapped over a bar. Open hands, hands in the air above or
   beside a rung, or a hand inside a rung's steel fails.
2. Her feet stand on rungs: a sole on a bar. A foot dangling with no rung under it, or poking through
   the ladder, fails.
3. In `down_side` her body is on the ladder, close in front of its rungs, hanging from her hands; a body
   hanging beside the ladder, or a limb reaching off it into open air, fails.
4. No interpenetration: her torso, skirt and legs do not pass through the rails or rungs.
5. `down` and `down_side` are one moment from two cameras: the same hand on the same rung, the same
   foot placement in both. A hand on a rung in one and in the air in the other fails.
6. No warp between `down` and `down_low`: she is lower on the same ladder in `down_low`, in a climbing
   pose of the same kind, facing the same way; not turned round, not jumped off it, not on another ladder.
7. The ladder and the tower steel round it are drawn (no void where they stand); no missing-texture
   color (bright magenta or checkerboard).
