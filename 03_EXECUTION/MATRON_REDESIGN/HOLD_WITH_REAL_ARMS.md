# Her hold with real arms (2026-11-02)

## What Cory saw

Playtest, 11-02: *"when the matron goes to hang me over the edge, her arms are way, way, way, way too
long. You told me that it is impossible to do it without stretching her arms, which I don't see why
that is the case when it is possible in real life ... maybe she should knock me down first and then
grab my feet ... she should be able to sort of grab and swing me/player without needing retardly
stretched out arms. ... If hanging me over the edge is such a problem, then we don't have to do that
... help me develop an alternative thing that's at least as cool or even cooler."*

## Why her arms stretch (from the code, trunk `d9b35d7e`)

Her drawn arm is about 0.9 m long from shoulder to wrist (`UPPER_ARM` 0.44 + `FOREARM` 0.5;
`LIMB_REACH_M` 0.85, `godot/presentation/life/matron_view.gd`). Her shoulder is 1.80 m over her feet
(`kShoulderUpM`, `src/sim/matron.hpp`). The sim puts her hand much farther away than that, and the
drawing then pulls the arm out to reach it, up to 2.7 times its length (`HOLD_STRETCH_MAX`,
`godot/presentation/life/matron_rig.gd` `hold_reach`).

| Moment | Where the sim puts her hand | Shoulder to hand | Times a real arm |
| --- | --- | --- | --- |
| Grab: reaching for his ankle | up to `kStoopReachM` 2.1 m | 2.1 m | 2.3 |
| Carry: him upside down at her side | `kCarryHandUpM` 2.8 m over her feet | 1.0 m or more | 1.1+ |
| Heave: up over the rail | 3.15 m up, 0.55 m out | 1.46 m | 1.6 |
| Dangle: him below the rail | 1.35 m up, `kDangleOutM` 1.35 m out | 1.42 m | 1.6 |

## What is really impossible, and what is not

- **Impossible:** lifting a 1.8 m man upside down by the ankles so that his head clears a 1.09 m
  rail. His ankles would have to be about 2.9 m up (1.09 + 1.8), and with a real arm she reaches
  about 2.7 m (1.80 + 0.94). That is the heave the code does now, and it is why it stretches her.
- **Not impossible, and what people really do:** tip him over the rail with the rail as the pivot,
  and keep hold of his ankle as he goes over. Or pin him backward over the rail first. Every hand
  position below fits a real arm.

He was right: it can be done in real life, and in the game, with real arms. The earlier answer
("impossible without stretching") was about the heave the code does, not about holding someone over
an edge.

## The new hold (Claude's pick)

Five beats. Every hand stays within her real arm, and no beat needs a stretched limb.

1. **Down.** She knocks him off his feet first (a shove or a swat, her capped force). That is
   Cory's idea. Grabbing a man by the ankle while he stands is what forced the 2.1 m reach.
2. **The ankle and the drag.** Bent at the hips and knees, she takes the ankle of a man on the deck
   (her shoulder comes down to about 1.0 m, so a 0.9 m arm reaches the steel). She drags him by it to the
   rail on his back, his hands scraping for anything. Hand height about 0.8–1.0 m. He can grab a
   post, kick, or punch her shins: her drag is a capped pull against his grip, never a lock.
3. **The rail pin (new, and the cool part).** At the rail she hauls him up by the collar and belt
   (hands at 1.1–1.4 m, 0.4–0.6 m out) and bends him backward over the top rail. His back is on the
   rail, his head and shoulders are out over the drop, and her forearm is on his chest. Her face is
   inches from his, and her lines are spoken right into it. His fists reach her face here, so PUNCH
   lands for real (rule 33). **The arm tear moves here**, because his arm is within her real reach
   only in this beat.
4. **Over.** If he does not break her hold, she tips him over the rail. His body goes over, and she
   catches one ankle on top of the rail as his legs come over. The rail takes his weight against her
   wrist (hand about 1.1–1.2 m up, 0.35–0.5 m out from her middle).
5. **Hanging by one ankle.** He hangs head down against the outside of the deck. His free leg can
   kick her hand. If he crunches up (a sit-up, hanging), his hands reach his own ankle, so they
   reach her hand too, and PUNCH strikes her wrist or claws her fingers. Grabbing a rail post or the
   deck's edge first, then breaking her grip, is how he lives. That is a real escape, and it is
   never helpless (rule 34).

What stays as it is: no kill on touch; only his own blow frees him before the rip begins; never let
go over open air except in the fatality; Spare puts him back on her deck; the fatality stays full
intensity (rule 32); third-person cinematic from her grab to his release or death (rule 33).

## A different option, if he would rather drop the hang

**The swing.** She has his ankle while he is down and swings him in an arc like a sack, slamming him
into a column or the rail, then lets go across the deck or over the edge. Her arm stays real (it is
a hammer throw: his weight sits far from her hand, not her hand far from her). The pull it needs is
about 1.3 kN at 5 m/s, well inside her 8 kN budget. It is brutal and quick, but less intimate than
the rail pin, and it gives him less to do.

## Finish facts for whoever builds it

- Every frame of every hold, each bone of her arm is at most 5% over its rest length. These are the
  drawn-body checks of PR #43, which is also why #43 matters now. `HOLD_STRETCH_MAX` goes to 1.05.
- Her hand is within 5 cm of the held ankle, or of his collar or belt, in every beat (#43's hand
  check).
- All five beats are played through touch from the game's start on the production path, then
  checked by eye with the Godot runtime (rule 28), with screenshots of each beat.
- Forces stay within her capped budget. Nothing snaps or teleports (rule 29).
- The test checks that change go as their own protected PR with a plain before/after (rule 4).
