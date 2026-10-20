# Her chart: the tower that learns you

The owner, 2026-10-20: *"not THAT kind of freak the player out ... a more Halloween version ... Not
good. I am talking about an uncanny valley, weird, creepy vibe type freak out ... the player finding
things in the game that they have said or have spent a long time looking at or on, or something
intimately close to them. Could we get what the player is saying? People talk when they play games a
lot to themselves ... maybe they could hear some things they've said themselves too ... synthesize it
all together into a badass integration."*

This replaces the gore ideas in `LIVE_FABRICATION/DESIGN.md`'s horror layer (bent tools, a severed
arm, a face mask), which the owner rejected as Halloween. Design only; nothing is built.

## The five tests every idea must pass (the owner, 2026-10-20: *"What I'm looking for is very particular"*; agreed: *"That is fantastic"*)

1. **About you,** not players in general.
2. **Exactly right,** so you cannot explain it away.
3. **Close to you:** your words, your voice, your gaze, your face, your room, your habits.
4. **Late and quiet,** never a jump scare.
5. **You are left unsure how it knew.**

An idea that fails one of them does not go in.

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
  Where the idea came from, and where it goes (the owner, 2026-10-20): *"The game's fear-yelling
  matched perfectly with what i said myself as i was playing once, and it felt like it heard me,
  converted it into IT'S VOICE, and played it."* The aim is that: what you say, said back in the
  game's own voice (speech to text and a voice on our server: build steps 4 and 7). Your own voice
  faint from a pipe is the first step of it.
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

**Built (2026-10-20):** steps 1 and 3, all but your bodies tended. What the eyes rest on
(`attention.gd`, native `look target`), the tower's clocks on the phone's own time, her chart's
pages left late and rarely, and, with consent asked once at first launch, your voice heard back
faint from the structure. HER CHART (FAST) on the START list brings a page and an echo of your
voice within about a minute (`touch_her_chart_fast`; also `touch_her_chart`, `touch_mic_consent`,
`touch_voice_echo`).

**Her hearing from the real room, built (2026-10-20):** the microphone's loudness over the room's
own quiet goes to the simulation like a held control (`Simulation::set_room_loudness`) and she
hears it as the player's own noise, where the player is: a hush is not heard, a mutter only close
by, a bark as far as a hard landing, and near her on the hunt it starts her Turn (the spine
cracking, the head turning the wrong way). The game's own sound jumping masks what the microphone
hears for a moment. Native `matron room`; `touch_room_hearing` (a bark through the microphone's
path wakes her). The levels are to tune on the phone.

## Decisions for the owner

1. Her chart as the spine of it (recommended).
2. The microphone: RECORD_AUDIO and consent (step 3), and speech to text on our server (step 4).
3. The camera for the badge photo (step 6).
4. How often: a handful of moments an hour (recommended), escalating.

## The tower's senses (the owner, 2026-10-20: *"There's so many amazing and extraordinary things that can be done, that you haven't even begun to think of"*)

The step past mirroring: the tower **hears, sees, feels, remembers and reaches out**, and the game
is played through those senses, not only shown them.

**Hears**
- **Her hearing is your room.** The native simulation already models what she hears (steps,
  loudness, carry). The microphone's level, cut to loudness with plain signal processing, joins it
  as an input at the boundary (recorded like a touch, so determinism holds). Stealth becomes real:
  hold your breath; a dog barks, she turns.
- **The foreman, live.** Speak to him on the radio; speech to text, a language model holding the
  tower's story and the player's chart, and his voice, all on our server. He knows the canon, lies
  about her accident, slips. Late, she takes the line and answers in the player's own words.
- **The Stores intercom:** a requisition spoken instead of written.

**Sees** (the badge photo, CAMERA, consent)
- Her chart describes the player from the photo (a vision model on the server).
- Once, with explicit consent and the owner's say: the room behind the player, framed in a window
  in the tower.

**Feels**
- **Flinches and stillness** from the phone's motion sensor (no permission): *"Startles easily.
  Holds very still when she's near."* Quiet moments are timed to the stillest stretches.
- **The fold** (an Android add-on reads the hinge): closing the phone is closing your eyes; when it
  opens, she is closer.

**Remembers and reaches out**
- **Time away:** the tower moves on while the game is closed; a page notes the absence exactly.
- **Her ward round:** a night notification when the player is not playing (an Android add-on and
  the notification permission, the owner's call).
- **Other climbers:** anonymous bodies where real players died, through our server.

Claude's order: her hearing from the room (after the microphone lands), flinches in the chart (no
permission), the foreman live (after the server).

## The owner's picks (2026-10-20)

- Yes: her hearing from the real room; the foreman live; spoken requisitions; the room behind the badge
  photo, once, with explicit consent.
- Motion sensing only if unmistakable: a flinch counts only when it lines up with a named scare, and the
  chart quotes it with its time and the event (*"21:47:12. Jumped when the pipe banged behind him."*);
  anything less clear is not written.
- No: night notifications, the fold, other players' bodies.
- The server's daily budget: Claude sets it from measured cost and reports it.
