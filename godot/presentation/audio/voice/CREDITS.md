# The climber's fear voice: credits

The yells, swearing and relief the game plays on large falls (GDD 8.4 /
Governing Law 8). Every clip is generated offline by
`src/voice/generate_fall_voice.py` — nothing here is a recording of a real
person, and nothing of the generator runs in the game or in CI.

Source of every clip:

- Speech: Chatterbox TTS 0.1.7 (Resemble AI, MIT licence), weights
  `ResembleAI/chatterbox` at revision `5bb1f6ee58e50c3b8d408bc82a6d3740c2db6e18`.
  Its `exaggeration` drives the delivery from level to yelled.
- Voice identity: a reference sentence spoken by Piper's `en_US-john-medium`
  voice, itself trained on LibriVox public-domain recordings.
- Check: Whisper `small.en` (OpenAI, MIT) transcribes each take; a take is
  rejected unless the transcript holds at least 60% of its line's distinct
  words and it ends within ~0.45 s a word, so no clip ships that says
  something else, nothing at all, or babbles on.

The lines are the project's own, written for the game. Seeds are fixed per
line and take, so a rerun on the same packages and CPU reproduces the same
selection. `voice_lines.gd` (written by the same generator) lists the tiers
and the caption each clip speaks.

| File | Tier | Line |
|---|---|---|
| `break_arm_00.ogg` | break_arm | Aaah! My arm! My arm! |
| `break_arm_01.ogg` | break_arm | Fuck! My fucking arm! |
| `break_arm_02.ogg` | break_arm | Shit, my shoulder, my shoulder! |
| `break_arm_03.ogg` | break_arm | Ah, it's broken, it's broken! |
| `break_arm_04.ogg` | break_arm | Oh fuck, I can't feel my hand! |
| `break_leg_00.ogg` | break_leg | Aaah! My ankle! My fucking ankle! |
| `break_leg_01.ogg` | break_leg | Fuck! My leg, my leg! |
| `break_leg_02.ogg` | break_leg | Shit, it's broken, it's broken! |
| `break_leg_03.ogg` | break_leg | Oh god, my foot, oh god! |
| `break_leg_04.ogg` | break_leg | Ah, fuck, I heard it snap! |
| `pain_00.ogg` | pain | Ow! Fuck! |
| `pain_01.ogg` | pain | Oof. Ow, shit. |
| `pain_02.ogg` | pain | Ah, my ankles! |
| `pain_03.ogg` | pain | Ugh. That was stupid. |
| `pain_04.ogg` | pain | Ow, ow, ow, ow. |
| `pain_05.ogg` | pain | Argh, son of a bitch! |
| `panic_00.ogg` | panic | Oh shit, oh shit, oh shit! |
| `panic_01.ogg` | panic | No no no no no no! |
| `panic_02.ogg` | panic | Fuck, fuck, fuck, fuck! |
| `panic_03.ogg` | panic | Oh fuck, oh fuck, oh fuck! |
| `panic_04.ogg` | panic | Shit shit shit shit shit! |
| `panic_05.ogg` | panic | Nope nope nope nope nope! |
| `panic_06.ogg` | panic | Oh god, oh god, oh god! |
| `panic_07.ogg` | panic | This was a terrible idea! |
| `panic_08.ogg` | panic | Oh, come on! Come on! |
| `panic_09.ogg` | panic | I should have taken the stairs! |
| `panic_10.ogg` | panic | Grab something, grab something! |
| `panic_11.ogg` | panic | Why? Why? Why? |
| `panic_12.ogg` | panic | Oh, motherfucker! |
| `relief_00.ogg` | relief | Oh, thank god. Thank god. |
| `relief_01.ogg` | relief | Ha. Ha ha. Okay. I'm alive. |
| `relief_02.ogg` | relief | Holy shit. Holy shit. |
| `relief_03.ogg` | relief | Never doing that again. |
| `relief_04.ogg` | relief | Okay. Okay. Breathe. |
| `relief_05.ogg` | relief | Oh, thank fuck for that. |
| `terror_00.ogg` | terror | Aaaah! Aaaaaaaah! |
| `terror_01.ogg` | terror | Aaaah! Fuck! Fuck! |
| `terror_02.ogg` | terror | I'm gonna die, I'm gonna die, I'm gonna die! |
| `terror_03.ogg` | terror | Mother fucker! |
| `terror_04.ogg` | terror | Not like this! Not like this! |
| `terror_05.ogg` | terror | Somebody catch me! |
| `terror_06.ogg` | terror | Holy shit, holy shit, holy shit! |
| `terror_07.ogg` | terror | Why did I climb this fucking thing? |
| `terror_08.ogg` | terror | Oh god, I'm sorry, Mom! |
| `terror_09.ogg` | terror | Aaaah! Shit! Shit! |
| `terror_10.ogg` | terror | Help! Help me! Help! |
| `terror_11.ogg` | terror | No! No! Aaaaah! |
| `yelp_00.ogg` | yelp | Whoa, whoa, whoa! |
| `yelp_01.ogg` | yelp | Oh shit, no! |
| `yelp_02.ogg` | yelp | Oh fuck, no! |
| `yelp_03.ogg` | yelp | Oh crap, oh crap! |
| `yelp_04.ogg` | yelp | Nope, nope, nope! |
| `yelp_05.ogg` | yelp | Oh no, oh no! |
| `yelp_06.ogg` | yelp | Shit, shit, shit! |
| `yelp_07.ogg` | yelp | Whoa, hey, hey! |
| `yelp_08.ogg` | yelp | Ah, shit, no! |
| `yelp_09.ogg` | yelp | Fuck, fuck! |
