# matron-physics on ScraperX-Claude: integration notes (2026-10-21)

`matron-physics` (head `26aac95`, branched at `82cb589`) is merged onto
ScraperX-Claude's head (`b9f7d14`: the broken bridge to the second tower and
the rigging reach) on the side branch `claude/matron-physics`, merge commit
`a481c5e`. The merge was clean; no sim physics was changed.

## Its own tests, merged and as delivered

| Group (`SCRAPERX_ONLY=`) | Result |
|---|---|
| `matron-moves` | pass (2) |
| `matron-arm` | pass (3) |
| `one-arm` | pass (1) |
| `water` | pass (1) |
| `matron-grab` | its grab cases pass (7); it then runs the fatality, which fails |
| `matron-fatality` | **fails: "every beat, in order"** |

The fatality fails the same way on `26aac95` as delivered (built and run
alone, same beats, same numbers), so the merge did not cause it:

```
beats 0@33.8 1@37.97 2@38.11 3@39.88 4@42.36 8@63.06 9@65.67
pins 1 leg tears 2 peak hip 6016.59 N, knots 0 kicks 0 hauls 0, parts 0, fell 0 m
```

She lays the body, pins it, takes both legs, takes the collar and unhooks
the skip lift's shackle from its bollard; then the Drag beat (4) never ties
the knot, Kick (5), Haul (6) and Fall (7) never happen, and after
`kFatalGiveUpSeconds` (20 s) it goes on to Wipe (8) and Black (9). The full
native suite stops there.

## What a trace of the Drag beat showed

1. **A 0.25 m spot she is held 2 mm short of.** `step_claimed`'s `go_to`
   counts her there only within 0.25 m of `kBollardStandX/Z`. Dragging the
   torso by its collar, she stops 0.252 m off it and walks no closer, so she
   never turns to the shackle: her hand stays at the torso. A threshold this
   tight decides the scene by millimetres (a different compiler or one
   changed contact can flip it), which may be why it passed where it was
   written.
2. **Past that, the torso does not reach the shackle.** Letting her work from
   wherever the shackle is within reach (1.8 m, inside her 2.1 m stoop) gets
   her hand moving: the collar comes from 2.46 m to about 1.9 m from the
   shackle and stops there, the hand about 0.3 m from her, not reaching
   further. It looks caught on something between the deck and the bollard
   eye, or held by her own arm's reach from where she stands; not resolved.

The owner (2026-10-21): another agent is reworking the arm tear and the
fatality now, so this is left to that work. The experiment's patch (the
reach-based stop and the trace) is not committed.

## Still to do here

- The owner's known risk: on the narrow 143 walkway she sometimes loses the
  ankle grip about 1 s into a carry (the body snagging on the rail, likely).
- Touch runs on the merge; her voice through the hosted voice system; the
  fatality's presentation; a phone build. ScraperX-Claude's CI runs the full
  native suite, so the merge waits there until the fatality passes.
