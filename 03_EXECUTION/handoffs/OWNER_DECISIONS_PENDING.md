# Decisions waiting on the owner (2026-10-29)

Short on purpose: he reads these on a phone. Each one is what it is, Claude's pick, and the one word
he answers with. Ordered by how much his answer unblocks. If he does not answer in 10 minutes, the
pick decides and the issue says so (`CLAUDE.md`). Detail lives in `03_EXECUTION/GAP_LIST.md` and
`00_START_HERE_ARCHIVE.md`, "Owner calls recovered ...". Checked against trunk `503958c`.

## Questions

**1. Whose story is on the radio?**
The live foreman captions use Miller (the climber) and Brenner (the foreman); the 40 recorded radio
lines and the Photo ID page use Ro, Iona, Ari and Dev. Only one can be the story.
Pick: the foreman, because his own story answer (10-19) puts the foreman on the radio, the live captions
already speak it, and 36 of the 40 recorded lines cannot ship anyway (rule 40). The Photo ID page's
heading would change with it.
Answer: **radio** or **foreman**.

**2. Write her lines now?**
She has no voice lines at all (rule 43); the fatality is mute but for grunts, and "Waste not." is
nowhere in the game.
Pick: yes. A helper drafts about twelve lines in the voice he described for him to read, and
nothing is generated until he has approved the words.
Answer: **write** or **wait**.

**3. When she dies in a fall, does she come back?**
Today a long fall kills her for the rest of the run, even after you die and restart.
Pick: she comes back at the next checkpoint as she was, so the threat cannot be thrown away. His
"she's done this before" version (she returns worse) comes later.
Answer: **back** or **gone**.

**4. Which waiting feature do we talk through next?**
His parked list: dread before she is seen, her limbs coming off, her death and return, tower two,
the chiller drop and glass tube, lift governors.
Pick: dread, because her stalk is already built and the missing half is haptics, light and the
edge of the view, which makes her felt before she is seen.
Answer: **dread**, **limbs**, **return**, **tower**, **chiller** or **governors**.

**5. What happens at the top?**
Landing on the summit gives a "THE TOP" toast and then nothing. He said the ending is his.
Pick: the ending he already told us: seat the crew's last beam, the foreman's last line, then stop.
Answer: **beam**, or say what you want instead.

**6. How far does she wreck your work?**
He asked for her to shove stock he placed off edges and to jam machines he set. She only takes two
toolboxes and unrigs one cage today.
Pick: stock first (a general push, no per-machine code), machines later.
Answer: **stock** or **machines**.

**7. Which pressure idea first?**
New ideas for the upper tower, where nothing lived-in or zipline reaches past 396 m: gust fronts,
the falling walkway, the drum run, wreckage slides, rope down with the hook.
Pick: gust fronts, because wind already pushes jumps and the hook's rope, so the streak of mist
before a gust is the only new thing to build.
Answer: **gusts**, **walkway**, **drums**, **slides** or **rope**.

**8. The gravity balance at 462 to 487 m: place it or skip it?**
It repeats the counter-weight lift he already has, and the climb has no machine there.
Pick: skip it; spend that stretch on a new kind of machine.
Answer: **skip** or **place**.

**9. Real levers on the forklift?**
He asked to grab the forklift's real lift lever and have the forks rise, instead of a button.
Pick: the forklift only, as a trial, before any other machine.
Answer: **forklift** or **later**.

**10. A best time or ghost for the climb?**
The idea was an agent's, from a movement study; his own line is only that movement alone should be a
reason to play, and the moves are built.
Pick: no.
Answer: **no** or **yes**.

**11. Store details (name, price, code public or not, credits).**
Nothing needs them until a release; a store build also needs his release key in GitHub's secret
settings, which only he can add.
Pick: later.
Answer: **later**.

## Already settled (dropped, with the evidence)

- **Ledge pull-up speed** (was F1): built fast, 0.42 s (`physics_world_tuning.hpp:421`,
  `kMantleDurationSeconds`); no complaint since. He can overturn it by saying so.
- **Motors on the three tall machines** (F2): rule 54 ("machines need not be man-powered") answers
  it; they run by Action (`vertical_route.cpp:407-418`).
- **Platforms that dip** (F3): an engineering matter now; the dip is generic
  (`physics_world_movement.cpp:1085`) and `touch_crane` runs in CI (`wo000-delivery-spine.yml:386`).
- **The rest of his round-two takes**: added on 2026-10-29 (`matron/CREDITS.md`, "More takes").
- **The C2 cradle ladder**: the pick was "leave it", and nothing changed.
- **Her stages and her mind** (A1, decision 9): decided 10-18 and 10-19 and built
  (`matron.cpp:1863-1896`).
- **Her sight and stalk**: merged to trunk (`65e7f86`, `matron_stalk.cpp`).
- **Her chart's permissions**: INTERNET, RECORD_AUDIO and CAMERA are approved with the player's
  consent in game (rule 57); only the export setting is missing, which is work.
- **Her many ways to die, the hook gun as a weapon**: parked by rule 64; nothing to decide until he
  picks from question 4.

## Not decisions (work for an agent; see `GAP_LIST.md`)

The ladder pull, the cluck and croak still cued, the five perception leaks, a helmet lamp, the mix note
(ambience and footsteps too loud, 10-27), stock above 396 m, and the pipe scenario missing from CI.
