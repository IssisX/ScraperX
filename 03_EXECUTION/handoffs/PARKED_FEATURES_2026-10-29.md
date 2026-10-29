# Features the owner asked for that are not built yet (2026-10-29)

Written by the outgoing integrator for the new one. These are the owner's own ideas, which
he said more than once and which were parked, not dropped. They are ranked by how well they
fit the work under way now: making the Matron intimate and interactive. Each one gives his
words, where the design lives, what exists in code today, and what is still open.
"Design with the owner" means you talk it through with him in two or three lines before
building.

## 1. Dread before she is ever seen
- **Owner, 2026-10-27:** *"Lets send ominous signals to player via various human senses and
  creative things. This granny drama should build a bit some how. Put this in your pocket for
  some time"*
- **Design:** the `00_START_HERE.md` row of 2026-10-27, "kept for later". It reaches every
  sense a phone has:
  - sound;
  - haptics;
  - light and the edge of the view;
  - the HUD itself;
  - things moved from where he left them.
  It follows the new soundscape.
- **Fit:** this is the missing half of "intimate". She stops being "a separate entity that
  runs into me every now and again", because she is felt long before the encounter.
- **Exists:** her soundscape (slice 1 plus the fixes). Haptics are already used in about 8
  scripts.
- **Open:** all of it. Build it with the stalk pick from the handoff:
  - she shadows him;
  - she freezes when he looks;
  - she creeps when he looks away.

## 2. Her chart: the tower learns the player
- **Owner, 2026-10-20:** *"an uncanny valley, weird, creepy vibe type freak out ... the player
  finding things in the game that they have said or have spent a long time looking at ...
  something intimately close to them"*; and on her hanging: *"it also hits intelligence
  centers in the brain not just fear centers (the way we want it!)"*
- **Design:** `03_EXECUTION/UNCANNY/HER_CHART.md`. It sets five tests every idea must pass:
  1. about you;
  2. exactly right;
  3. close to you;
  4. late and quiet;
  5. you are left unsure how it knew.
  The design covers:
  - her ward notes, found a page at a time, each line true of this player;
  - what he lingered on turning up again in her hoard;
  - his own voice heard back, by microphone and only with consent.
- **Dropped by the owner:** "your bodies tended". Don't build it (START_HERE, 2026-10-21).
- **Exists:** `ui/consent_screen.gd` and `settings_store.gd` refer to it. Check what is
  live before building more.
- **Open:** the pages, the gaze and linger record, and the returned objects.

## 3. Fight back for real: her limbs come off
- **Owner, 2026-10-20:** *"Would be nice to have real limbs and stuff rip off if hit by
  harpoon (hook gun) or put through any other damage that would do so. This Could be a very
  cool fixation point of the game that one would wanna constantly play it for."*
- **Design:** the START_HERE row of 2026-10-20, "design with the owner".
  - The hook gun (built, found on the 220 ring), the laser welder, the nail gun, or any blow
    strong enough tears a real limb from her body.
  - She comes back without what she lost.
- **Fit:** the punch (#23) is the first step. This answers his "no way to defend" (playtest,
  2026-10-27).
- **Open:** all of it. The tear must be physical (a force past a real joint limit), never a
  scripted pop.

## 4. She can die, and comes back worse
- **Owner, 2026-10-20:** *"the harpoon can kill her, if it's a kill-shot. She can die in many
  ways as shes flesh and blood ... lets her maybe revive or respawn"*. His pick for her
  return: *"Option 3, she's done this before"*.
- **Design:** the START_HERE row of 2026-10-20.
  - She already survived the accident the foreman covered up, so death only sets her back.
  - After a while she crawls back up the tower, visibly damaged and angrier.
- **Exists:** nothing in code (no kill-shot or death path).
- **Open:** all of it. It pairs with item 3.

## 5. Her voice: the merciless nurse
- **Owner, 2026-10-21:** her lines are a condescending, merciless nurse talking to a child,
  profane and emasculating. The last line, at the apron wipe, is *"Waste not."*
- **Design:** the START_HERE row of 2026-10-21, and
  `03_EXECUTION/MATRON_REDESIGN/MATRON_PHYSICS_INTEGRATION.md`.
- **Law:** human sounds are real. A voice is made the way the climber's fear yells were
  (`src/voice/generate_fall_voice.py`), and the owner picks the takes by ear.
- **Exists:** no voice lines are wired in the game scripts.
- **Open:** writing the lines, the takes, the owner's picks, and cues tied to events (grab,
  lift, tear, hang, let-go). This goes with L6.

## 6. She travels between many more decks
- **Owner, 2026-10-21:** *"I really want her able to travel between many more decks than
  that. We won't do that right now, but I need you to know that. I have said this several
  other times."* This is a standing row.
- **Design:** `03_EXECUTION/MATRON_REDESIGN/MULTI_DECK_INVENTORY.md`. Her general climbing
  skill (scan, planner, moves) is the way to get there; never per-route tables.
- **Open:** the ledge jump-hang-mantle (parked, #3 6473562010), ways up from the low decks
  now that she starts on deck 1, and the ladder pull.

## Beyond the Matron (the owner, 2026-10-20: *"Let's not spend all of our usage on one part of the game"*)

### 7. The second tower into play
- **Owner:** *"LETS GET THAT 2ND STRUCTURE INTO PLAY HERE!!!!"*, and *"perhaps its the
  Matrons tower or where she likes to be"*.
- **Design:** `03_EXECUTION/SECOND_TOWER/PROPOSAL.md` (from `verified_map.json`).
  - It is earned through tower one, by believable architecture only.
  - It is reached by a crossing of roughly 52-66 m.
- **Open:** the proposal, to be talked through.

### 8. PROPERTY DAMAGE on tower two
- **Owner:** *"you deliberately wreck the company's access structure to make your own way up.
  Then you have to climb what you've done"*. Also: *"The spectacular event changes the
  climbing problem. It doesn't replace the climbing with a ride"*, and *"No mandatory
  countdown. No invisible wall"*.
- **Design:** the START_HERE row of 2026-10-21.
  - A demolition ram, released by a physical act, folds a service assembly out onto steel
    stops.
  - A fixed bypass always remains, and the change persists.
- **Open:** all of it, to be talked through first.

### 9. The backup routes
- **Rule (2026-09-28):** stairs, ladders and easy paths are only the backup route; the route
  itself is the machines and the parkour climbs. Plan §2.6 rule 8.
- **Open:** "no backup path built yet".

## Already done (don't redo)
- Tracking her: the on-screen tracker (2026-10-27).
- Her locker low: #21.
- Her freeze at release.
- Her facing.
- Her motion interpolated (L1).
- The punch (#23) and her fatality (#20).
- The music box removed, with real sounds in its place.
