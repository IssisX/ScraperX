# Her motion as drawn: joint rates and hands on holds (rule 37)

Rule 37: her movement is continuous, drawn between ticks, no snapped poses,
joints at most 20 rad/s, hands reaching each hold. This measures it from the
real game path, as `tools/matron_probe/` does for her body and hair. It
measures; it changes nothing in the game.

## Record (headless, about the scenario's own length at 60 fps)

    tools/godot/godot --headless --fixed-fps 60 --path godot \
        -s "$PWD/tools/matron_motion/record.gd" -- --uitest=touch_matron_act --motion-out=<dir>

Any touch scenario works. `--motion-seconds=<s>` stops early. The scenario
plays as in CI, with its own touches. Output in `<dir>`:
- `meta.json`: her bones and their parents, the hand contact points, the fps;
- `frames.csv`: one row a frame, with her state, her drawn place, both wrists
  and knuckles as the rig places them, and what the simulation says each hand
  holds;
- `rotations.bin`: for each frame she is drawn, her skeleton's world rotation
  and every bone's rotation in its parent's frame.

The pose is read from the skeleton the renderer skins, after every view pass
(the probe's own ordering: last in the frame, deferred to its last flush).

## Score (numpy)

    python3 tools/matron_motion/score.py <dir> [<dir> ...]

Writes `<dir>/score/summary.txt` (also printed), plus three tables:
- `joints.csv`: the fastest joint on each drawn frame;
- `bones.csv`: each bone's fastest frame;
- `holds.csv`: one row a hold, with how near her hand came and how long it
  stayed.

- **Joints:** the angle a bone turns in its parent's frame between two drawn
  frames in a row, over the frame's time. Every bone counts but her hair
  (moved by the hair solver, not a joint) and her eyes (on a line of their
  own). Her fingers count. Her whole body's turn in the world has its own
  line. A frame where her drawn place moves over 0.5 m, or her pose changes
  in a frame of no time, is listed as a snap.
- **Holds:** a run of frames in which the simulation says a hand holds one
  point (`contact_hand`, `contact_hand2`, `climb_hand`, `climb_hand2`, and the
  ankle in her holds), the point staying within 3 cm.
  - Reached: her nearer knuckles come within 0.10 m of it (the probe's
    `GRIP_M`).
  - Kept: from 0.3 s in, the share of drawn frames her nearer knuckles are
    within 0.10 m.
