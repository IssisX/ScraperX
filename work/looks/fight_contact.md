# Look rubric: his blow landing on the Matron (J-068)

Frames, captured by the proof step "Render the eye's fight and ladder stills":

- `screenshots/fight_punch_contact.png` and `screenshots/fight_punch_reaction.png`: 960x828 under
  xvfb (`gl_compatibility`, 60 fps fixed), scenario `look_matron_fight` (`ui_test_look.gd`). On her
  154 m deck, changed (stage 2, low on all fours), her hands held so she cannot grab, 1.2 m in front
  of him. He looks down at her head and presses PUNCH on the touch button; the native judges reach and
  landing. `contact` is his eye (the game camera) on the tick the blow lands, the HUD hidden;
  `reaction` is 0.12 s later, while she staggers. His body is first person only: his forearms and fists
  are what show of him.
- `screenshots/injury/injury_face_front.png` and `screenshots/injury/injury_cheek_front.png`: 960x720,
  `injury_shots_test.gd --out`, a studio deck on a dark background. Her face carries J-059's marks at
  the doses landed blows give (both eyes at 150 N·s: the black-eye ring and the swollen upper lid; the
  cheek bruises). The game does not yet feed her injuries from real blows (the native per-region feed is
  J-044g), so these two frames judge how the damage looks on her face, not that a fight produced it.
  When the feed lands, move them to the fight scenario.

Written 2026-11-08 from frames rendered locally on trunk. Every line must hold in its frame; each is one
thing you can point at. What fails is what a person would call fake.

1. `contact`: his fist touches her: knuckles at her head, neck, shoulders or back, with no visible gap
   between fist and her body. A blow landing from out of reach (fist short of her, or her too far away
   to touch) fails.
2. `contact`: his arm is extended toward her from the lower edge of the frame, attached at his side,
   not floating or cut off in mid-air.
3. `contact`: no interpenetration: his fist or forearm does not sink into her head or torso, and her
   body does not pass through the deck or the rail.
4. `reaction`: she visibly reacts: her head, arms or torso have moved from where they were in
   `contact` (a stagger, a flinch, arms thrown out). The same pose as `contact` fails.
5. `reaction`: his fists are back toward him, not still buried in her; her limbs stay attached and
   inside the frame's sense of one body (no limb stretched or detached).
6. `face_front`: dark rings sit round her eyes on the skin of the socket, with a swollen, reddened
   upper lid; the eyeballs still show inside the rings. No patch floats off her face or overhangs its
   edge.
7. `cheek_front`: a bruise (purple-red, darker at its middle) lies on each cheek along the jaw line and
   follows the face's curve; it reads as bruised skin, not a flat painted patch.
8. In every frame: no missing-texture color (bright magenta or checkerboard) on her or on him.
