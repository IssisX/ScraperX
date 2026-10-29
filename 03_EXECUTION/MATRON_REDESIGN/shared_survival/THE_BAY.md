# The bay: shared survival, as built (2026-10-21)

The owner, of the other agent's shared-survival core (its handoff, archived: `03_EXECUTION/ARCHIVE_INDEX.md`):
asked whether it was good, the answer was yes; then *"Go ahead and build
it, man."* and *"Do what you can to tie it all up with polishing"*. The
power panel the handoff proposed is left out: pushing a button is not a
way up. The Matron is un-parked for this encounter only.

## What the player meets

On the 154 m deck's north band, between where a climber comes up the high
riser and the ladder to the 176 ring, the band is cut through from the
shaft's edge to the tower's face: **the bay**. A steel leaf spans it,
hinged on its east edge and held level on its upper stop by a motor. A fan
in the bay's east wall runs on the same supply (one district). Under the
leaf's west end, 1.78 m down, a service bracket on the bay's west wall,
with rungs up to the deck. A chain hangs from a pipe bridge over the leaf,
its shackle made fast on a cleat on the east rail's end post.

- **The walkway.** Driven, the leaf is a floor: the player and she cross
  it without it moving (native `bay walkway`: at most 1e-4 rad down).
- **The fan's cover.** Running, its noise covers three quarters of a
  climber's noise beside it, less with distance, none past 7 m: run over
  the leaf and she may not hear it.
- **Her trap.** On her deck west of the bay with the player across it to
  the east, heard or seen, she does not cross after them. She steps off
  the deck's edge onto the bracket, places herself under the leaf's west
  end and puts her hands up. Hands in the district stop its motor and fan;
  the leaf comes down onto her palms and she holds it level, as bait. Her
  palms give about 930 N: enough for the leaf, not for the leaf and a
  climber. Stepped onto, it goes down about 1 to 2.2 m out from the hinge,
  onto her, and the climber drops through the bay to the 143 m deck
  (alive: an 11 m fall). She lets go and climbs out by the rungs after
  them. Three times a run at most.
- **The way across.** UNHOOK the chain off its cleat, take a step onto the
  leaf by its hinge (her hands hold that much), HOOK it onto the leaf's
  eye: the chain carries the leaf (about 2.3 kN), her trap is beaten, she
  lets go, and the leaf now carries the player over its far half. Her
  hands are free to hunt again.
- **The restart.** The district starts again by itself 3 s after no hands
  are in it, its guard is shut and its drives (the fan coasting down too)
  are still: a restart, never a pull or a button. The motor brings the
  leaf back up level.
- **Drawn, heard, felt.** Her brace pose (arms up, palms under the edge,
  trembling with the load); the drive's hum, the fan's roar coasting down,
  the leaf groaning on her palms, clanks for the trip, the chain hooked on
  and the restart; on the leaf while only her palms hold it, a pulse in the
  hand every 0.6 s.

Nothing is a flag: the leaf is held by its motor, her hands, the chain or
nothing, and falls when it is held by nothing.

## Where it lives

| Part | File |
|---|---|
| The bay, its leaf, district, chain, bracket | `src/sim/bay.hpp`, `src/sim/bay.cpp` |
| Her trap, the leaf on her lines, the bracket's level and rungs | `src/sim/matron.hpp`, `src/sim/matron.cpp` (`trap_step`, `trap_lets_go`) |
| Finite palms, the district's drives | `src/sim/shared_survival.hpp/.cpp` (the other agent's core; `restore_running` added) |
| The cut in the deck slab | `src/sim/physics_world.cpp` (`build_stack`) |
| Saved with the checkpoint | `Bay::State`, in the save format key |
| Bridge | `get_bay_state()`, and `bracing`, `support_n`, `trap` in `get_matron_state()` |
| Drawing, sound, feel, labels | `main_world.gd` (the deck cut), `life/matron_view.gd` (brace), `audio/audio_director.gd` (`update_bay`), `main_machines.gd` / `main_feel.gd` (`_sample_bay`, `_feel_bay`), `main_context.gd` (UNHOOK CHAIN, HOOK ONTO THE WALKWAY'S EYE) |

## Tests

- Native `SCRAPERX_ONLY=bay` (`tests/sim_tests_bay.cpp`): the walkway; her
  trap staged from the deck spawn on player inputs, held, stepped onto,
  the fall lived through, her climb out, the restart; the chain beating it;
  a save made while she holds loading the bay as at her release (the
  checkpoint is not renewed while she is out).
- `tests/shared_survival_tests.cpp`: the core's own tests; its Matron check
  now proves she braces nothing out of her trap.
- Touch `touch_bay`: the whole of it by touch input events, the labels
  read on screen, the trip, hook and restart heard.

## Found and fixed on the way

- The leaf's hand rails first ran to its west end: turning down, their
  tops swung west over the deck's edge and held the leaf up at a quarter
  radian. They now stop 0.3 m short.
- The deck's rail at the bay's west edge was open from the rungs' head all
  the way to the leaf (1.3 m): a walk onto the leaf at an angle went off the
  edge beside it, 11 m down. Now only the rungs' head is open (her width
  and a hand's either side), and a short rail stands between it and the
  leaf.
- On a line's bare cable she could hang for good over the shaft's edge
  behind a trolley left short in the buffer; she now lets go where her
  swing lands her on the band.

## Not done

- Not played on the owner's phone yet; no frame-rate figure for the bay.
- No new start in START for the bay: it is met on the route at 154 m.
