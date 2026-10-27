export const meta = {
  name: 'matron-soundscape',
  description: 'Replace the Matron music box with a violent, gross, eerie procedural soundscape: understand, design, build, verify',
  phases: [
    { title: 'Understand', detail: '3 parallel readers: audio system, her moments, horror sound techniques' },
    { title: 'Design', detail: 'one soundscape spec from the three reports' },
    { title: 'Build', detail: 'one builder in its own worktree, local commits only' },
    { title: 'Verify', detail: 'code review and a measured listen-by-numbers' },
  ],
}

const REPO = '/home/user/ScraperX'
const SP = '/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad'
const BRANCH = 'claude/integrate-matron-soundscape-20261027'

const OWNER = `The owner (Cory) after his playtest, 2026-10-27, verbatim: "theres some stupid nursery rhyme sounding music box shit playing when i was near her - UMM, THIS BITCH NEEDS TO BE SUPER NASTY - SHES VIOLENT, SHES A DISGUSTING CREEPY EERIE GROSS MONSTER BITCH!! (not literally a monster or am i referring to size). A stupid music box or some nice sounding shit is the wrong answer. Think of a whole soundscape, a whole sound thing system you can do here. There's such potential and opportunity here."
Context: the Matron is a 220 kg woman who stalks the player up a construction tower (physics game, Godot 4.7 + a C++ native sim). He plays on a phone (Galaxy Fold), often on its speaker, sometimes headphones. Laws that hold for her: the capture, the arm tear and the fatality stay at full intensity, no gore toggle, never softened. "It's a game: weapons and violence are welcome, and no moralizing." Machines make sounds; people have voices: she is a person, so breath, mouth and throat sounds are hers.`

const FACTS = `Already found: the music box is \`matron_lullaby_loop\` (godot/presentation/audio/sound_bank.gd:67 and _lullaby at :618, "a music box that has run down: a plain minor phrase in tinkling bells"), played as a loop that follows her by godot/presentation/audio/audio_director.gd (:151, :332, :826) and driven per frame from godot/presentation/life/matron_view.gd (:1483-1500: heard from far off, slowing as she nears, warping while she is broken, still at her post). Her figure also carries a "music_box" prop (godot/presentation/life/matron_look.gd:169). The whole audio bank is synthesized at startup from seeded noise and filters on a worker thread (no sample files except the player's 47 recorded fall-voice lines); it is voiced for a phone speaker (identity above 300 Hz, low thump only for weight); the startup log prints SCRAPERX_AUDIO_BANK clips=67 msec=~2300 on this box. The native sim exposes her state to Godot as a Dictionary (ScraperXSimulation.get_matron_state(), src/bridge/scraperx_simulation.cpp:1646 onward).`

phase('Understand')
const [arch, moments, craft] = await parallel([
  () => agent(`${FACTS}

Read-only. Map this game's audio system so a designer can replace the Matron's sound without breaking anything. Read godot/presentation/audio/sound_bank.gd, godot/presentation/audio/audio_director.gd, the audio parts of godot/presentation/life/matron_view.gd and godot/presentation/main.gd, and godot/presentation/uncanny/voice_keeper.gd. Report, with file:line for each claim:
1. How a clip is synthesized (sample rate, the DSP helpers that exist: noise, filters, envelopes, loops), how long the bank takes and on which thread, memory per clip.
2. How playback works: players (positional or not), buses and effects, the Matron's loop and her one-shots, the dread system (matron_dread, _update_dread), ducking, captions for sounds, haptics hooks.
3. Every place the Matron's audio is driven from her state, and what data it reads.
4. Every test or CI check that touches audio or her sounds: grep tests/, godot/presentation/ui/ui_test*.gd and .github/workflows/wo000-delivery-spine.yml (the "Measure the mix a player hears" step, audio_mix, audio_fall) for clip names, bus names, loudness numbers and audio counters. Name exactly what a change to her sounds could break.
5. Budgets to respect: startup synthesis time, per-frame cost, polyphony.
Under 900 words, plain facts.`, { label: 'read:audio-system', phase: 'Understand' }),
  () => agent(`${FACTS}

Read-only. Inventory every moment of the Matron that could make a sound, from what the game can actually see. Read the get_matron_state() Dictionary in src/bridge/scraperx_simulation.cpp (from :1646) and the Matron's View/Snapshot fields in src/sim/simulation.hpp, then godot/presentation/life/matron_view.gd to see which fields the presentation already uses and how it detects events (counters that tick up, state changes). Her states are in src/sim/matron.hpp (enum class State: Dormant, Straining in the locker, Emerging, Hunting, Searching, Shuffling, Guarding, Peering, Unrigging, Staggering, Down, Grab, Carry, Dangle, ArmPlay, Claimed, Spare, Release, Dead), her rage stages (Pest, Dangerous, Changed), the Turn (windup, lunge), climbing, the pipe ride, cable slides, knockdowns, blows. For each moment give: the exact field(s) and the test that detects it (a counter change, a state edge, a threshold), how often it happens, and what she is physically doing then (a hand on a rung, her weight landing, her breath under strain, flesh tearing in the arm play, the fatality's beats in src/sim/matron_fatality.cpp). Include the gaps: moments with no field the presentation can read. Under 900 words.`, { label: 'read:her-moments', phase: 'Understand' }),
  () => agent(`${OWNER}

Research and design brief, no code. How do the best horror games and films make a stalker sound violent, disgusting, creepy, eerie and gross rather than spooky-cute? Think of approaches like Resident Evil 7's family, Alien: Isolation's creature, Outlast, The Last of Us's infected, Silent Hill's soundscapes, and the sound of The Thing. Turn them into a brief this game can build. The game synthesizes every sound procedurally in GDScript from noise, oscillators, filters and envelopes at startup (no sample files), and plays on a phone speaker (little below ~200 Hz; use harmonics and distortion to imply weight, the missing-fundamental trick; save true sub-bass for headphones).
Give:
1. The principles (proximity layering; silence and its breaking; the human made wrong: breath, wet mouth, throat; the body: weight, joints, flesh; the violence moment: tearing, cracking, the scream that is not hers; dissonance instead of melody; never anything pretty).
2. A palette of 12 to 18 concrete sounds for her (e.g., wet ragged breathing that quickens on the hunt, a guttural throat growl, saliva clicks and wet swallows, knuckle and joint cracks, bare heavy footfalls with a slap, a drag of something on steel, a hum through her teeth, the locker strain and burst, a rip of flesh and the crack of bone for the arm tear, the fatality's beats, her reaction to a blow), each with a procedural recipe a GDScript sample synthesizer can build: the sources, filters (with frequencies), envelopes (with times), modulation, waveshaping, granular tricks, durations and loop points, and the band it must live in for a phone speaker.
3. A system: how layers stack by distance and her state (far: a presence; near: her body; contact: violence), how they move in 3D, how rage escalates them, when it goes silent, and what must never play (the music box, anything melodic or sweet).
Under 1,400 words.`, { label: 'research:horror-sound', phase: 'Understand' }),
])

phase('Design')
const spec = await agent(`${OWNER}

${FACTS}

You design the Matron's soundscape for this game. Three reports follow: the audio system, her moments, and a horror sound brief. Write ONE implementable spec, replacing the music box entirely (the loop, its driver, and her music-box prop if it exists only as decoration; say exactly what to delete).

The spec must give:
1. The sound list: each clip's name, its synthesis recipe in terms of the existing DSP helpers in sound_bank.gd (or the small new helpers to add, defined precisely), duration, loop or one-shot, the band it lives in, its loudness target relative to existing clips.
2. The triggers: for each sound, the exact get_matron_state() field and test (state edge, counter change, threshold) from the moments report, and its rate limit.
3. The system: how it plays (positional on her, the attenuation numbers, buses, ducking against the player's fall voice and the ambience), how layers stack by distance and state, how her rage stage escalates them, the silences.
4. Budgets: added startup synthesis time (keep the whole bank's added time under ~600 ms on this box), memory, per-frame cost (no allocation per frame).
5. What must not break: every test or CI check the audio report named, and how each stays green.
6. An implementation plan by file and function, in order, small enough for one engineer in one sitting (it is fine to deliver the first, strongest slice now and list the rest as next steps).
American spelling. Under 1,800 words.

=== AUDIO SYSTEM REPORT ===
${arch}

=== HER MOMENTS REPORT ===
${moments}

=== HORROR SOUND BRIEF ===
${craft}`, { label: 'design:soundscape-spec', phase: 'Design' })

phase('Build')
const build = await agent(`You build the Matron's new soundscape in the ScraperX repo, from the spec below. Work in your worktree. First: \`git fetch -q origin ScraperX-Claude && git checkout -b ${BRANCH} origin/ScraperX-Claude\`. Commit locally on that branch as you go (clear messages, American spelling, no AI model names in any file; end each commit message with the two trailer lines "Co-Authored-By: <the session attribution line>" and "Claude-Session: https://claude.ai/code/session_01U92sp7quAMGdsPtBesvKaj"). NEVER push, never touch other branches, never edit tests' checks (tests/ and godot/presentation/ui/ui_test*.gd requires and thresholds), MANIFEST.txt, SHA256SUMS.txt, 01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md, export presets or the app id. GDScript only unless the spec truly needs a native field; if it does, stop and report instead of changing C++.

Deliver the spec's first, strongest slice completely: the music box gone, and her new sounds playing from her state. Then, before you finish:
- A parse and run check through the Godot MCP tools (the owner's rule: run the game through them, not the bare binary). The worktree needs the native library to run: copy ${REPO}/godot/addons/scraperx_native/bin/linux/libscraperx_native.so into the same path in your worktree and write godot/.godot/extension_list.cfg containing the line res://addons/scraperx_native/scraperx_native.gdextension (both are build outputs; do not commit them). Then run_project on your worktree's godot/ folder, get_debug_output, and confirm: no SCRIPT ERROR, and the SCRAPERX_AUDIO_BANK line's clip count and msec (report both, before and after your change; the bank must not grow by more than ~600 ms). stop_project when done. If run_project reports another session is active, wait a minute and retry, up to 5 times.
- With run_script in that running game, read each new clip's samples from the sound bank and return per clip: duration, peak, RMS dBFS, and the share of energy below 200 Hz, 200 Hz to 2 kHz, and above 2 kHz (a simple DFT or band-pass sums is fine). Save the numbers to ${SP}/soundscape_clip_stats.txt.
Report: the commits (SHA and one line each), what is in the slice and what is left for later, the before/after bank numbers, the clip stats, and anything in the spec you changed and why. Under 700 words.

=== SPEC ===
${spec}`, { label: 'build:soundscape', phase: 'Build', isolation: 'worktree' })

phase('Verify')
const [review, listen] = await parallel([
  () => agent(`Second-agent review ("write, then break") of the Matron soundscape commits on the local branch ${BRANCH} (in this repo's object store; a worktree may hold it). Read-only: do not edit or commit. Diff it against origin/ScraperX-Claude (\`git log --oneline origin/ScraperX-Claude..${BRANCH}\` and \`git diff origin/ScraperX-Claude...${BRANCH}\`). Use 03_EXECUTION/REVIEW_CHECKLIST.md (git show origin/ScraperX-Claude:03_EXECUTION/REVIEW_CHECKLIST.md) where it applies, and run python3 -I ${SP}/rc/rule_check.py origin/ScraperX-Claude ${BRANCH}.
Try to break it: GDScript errors (shadowed names in long functions such as main.gd _process, typed-variable mismatches, null nodes before _ready), per-frame allocation or string building in _process paths, players created per event instead of pooled, a sound that can stick on or loop forever after she dies, despawns or a scene reload, ducking that never releases, anything that changes a test's check or a CI-checked audio number (the "Measure the mix a player hears" step, audio_mix, audio_fall), anything melodic or sweet left for her, startup time growth over ~600 ms, American spelling, no AI model names.
Report: the rule_check last line, each finding as FAIL file:line with a concrete path (state, what happens, what should happen) or PASS, then LAND or DO NOT LAND. Under 600 words.

The builder's report, for orientation only (verify everything yourself):
${build}`, { label: 'verify:code-review', phase: 'Verify' }),
  () => agent(`${OWNER}

You judge whether the Matron's new sounds meet the owner's ask, by the numbers, since nobody here can listen. Read the builder's report below and ${SP}/soundscape_clip_stats.txt, and read the synthesis code for each new clip on the local branch ${BRANCH} (git show ${BRANCH}:godot/presentation/audio/sound_bank.gd and the other changed files). For each clip say: would a player hear it as violent, gross, creepy or eerie, or could any part read as pretty, musical, tonal-sweet or comic (a clean sine, a consonant interval, a regular melody, a cartoon boing)? Does it carry on a phone speaker (most energy 200 Hz to 4 kHz; any sub-bass also implied by harmonics)? Is it too quiet against the existing mix, or likely to clip? Is anything the brief called essential missing (breath, throat, wet mouth, weight, joints, the tear and crack of the arm rip, silence before the burst)?
Give a short verdict per clip and a ranked list of the three changes that would make her nastiest next. Under 600 words.

Builder's report:
${build}`, { label: 'verify:listen-by-numbers', phase: 'Verify' }),
])

return { spec, build, review, listen }