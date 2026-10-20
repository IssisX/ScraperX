# Her chart: the tower that learns you

The owner, 2026-10-20: *"not THAT kind of freak the player out ... a more Halloween version ... Not
good. I am talking about an uncanny valley, weird, creepy vibe type freak out ... the player finding
things in the game that they have said or have spent a long time looking at or on, or something
intimately close to them. Could we get what the player is saying? People talk when they play games a
lot to themselves ... maybe they could hear some things they've said themselves too ... synthesize it
all together into a badass integration."*

This replaces the gore ideas in `LIVE_FABRICATION/DESIGN.md`'s horror layer (bent tools, a severed
arm, a face mask), which the owner rejected as Halloween. Design only; nothing is built.

## The idea in one line

She was the ward's matron. Matrons keep charts. **She is keeping one on you**, and the tower is how
she watches: what you look at, what you say, when you play, where you fell. You find her chart a
page at a time, in her hand, and it is right.

## What the player meets (no menus, no jump scares)

- **Her chart.** Pages turn up in the tower: on a clipboard in her locker, pinned in the Stores,
  under a body. Ward notes in her handwriting, plain and accurate:
  *"02:14. Still awake. Talks to himself when the climb goes badly. Looked at the gas bottles by
  the forklift for a long time. Afraid of the edge (stands well back)."*
  Every line is true of this player, this session. Early pages are vague; later pages are exact.
- **What you lingered on comes back.** The things you looked at longest turn up again where they
  should not be: the same gas bottle, the same dent, set upright and neat in her hoard; a crate you
  stared at standing alone on a deck 300 m up, turned the way you saw it.
- **Your bodies.** You already leave your body where you fall (up to eight stay). She tends them:
  later you find one moved, laid out straight, hands folded, a chart page under it.
- **Your voice.** In a quiet stretch, something you said to yourself twenty minutes ago comes up a
  pipe or out of a dropped radio, faint, in your own voice. Never loud, never explained. Later, a
  line you said is written in her chart, word for word.
- **Your time.** The tower's clocks keep your real time. Her chart notes when you play. If it is the
  middle of your night, the night shift is too.
- **Your face** (only with the badge photo, the owner's call): once, on the chart's first page,
  clipped to the corner. Not on bodies, not on posters: once is worse.
- **The Stores** (live fabrication): she orders things for you, from her chart. A delivery you did
  not ask for: the thing you stared at, made new, with a slip in her hand. *"For the one who likes
  to look."*

## Rules that keep it uncanny instead of cheap

1. **Accurate or silent.** A line or an echo appears only when it is exactly right. A wrong guess
   breaks the spell.
2. **Late, never instant.** Echoes come minutes or hours after the moment, never right away.
3. **Quiet and ambiguous.** Faint, at the edge of hearing or of the frame; the player is never sure
   until the chart confirms it.
4. **Restraint.** A handful of moments per hour, escalating slowly.
5. **Only what the player gives the game.** Their play, their words, their voice, their face if
   they choose, their clock. **Never** anything else on the phone (contacts, photos, location,
   other apps): off-limits, always.

## What it takes

| Piece | Comes from | Needs |
|---|---|---|
| What you look at | the camera's aim each frame, dwell per object (in-game only) | nothing new |
| When you play | the phone's clock | nothing new |
| Your bodies | the corpses the simulation already keeps | nothing new |
| Your voice echoes | the microphone, short clips cut on the phone by loudness (plain signal processing, not a model), kept on the phone, replayed filtered | the RECORD_AUDIO permission and the player's consent |
| Your words in her chart | the clips sent to our server for speech to text, then deleted | the player's consent; internet |
| Her saying your words in her voice (the end) | a voice model on our server | consent; internet |
| Your face | the badge photo | CAMERA; consent |
| Her deliveries | live fabrication | internet |

Consent is honest and up front, outside the fiction: what is heard, what is kept (clips stay on the
phone and expire), what goes to the server (text only if allowed), how to turn it off. The game is
whole without any of it: her chart then knows only your play. App-store rules on microphone use and
data disclosure apply (**to verify** before release).

How it sits with the simulation: all of it is presentation (pages, sounds, placed props), except
fabricated deliveries, which enter as signed, bounded data (`LIVE_FABRICATION/DESIGN.md`). The
native world's determinism and body order are untouched.

## Build order

1. **Her chart from play alone:** the attention tracker (dwell per object), the clock, chart pages
   written from both and placed in the tower, your bodies tended. No permission, no server.
2. **The echoes of what you looked at:** copies set in her hoard and on the decks.
3. **Your voice:** microphone clips replayed from the tower (needs RECORD_AUDIO, the owner's call).
4. **Your words:** speech to text on our server, into her chart.
5. **Her deliveries:** live fabrication from her chart.
6. **Your face:** the badge photo, once.
7. **The end:** she says your words.

## Decisions for the owner

1. Her chart as the spine of it (recommended).
2. The microphone: RECORD_AUDIO and consent (step 3), and speech to text on our server (step 4).
3. The camera for the badge photo (step 6).
4. How often: a handful of moments an hour (recommended), escalating.
