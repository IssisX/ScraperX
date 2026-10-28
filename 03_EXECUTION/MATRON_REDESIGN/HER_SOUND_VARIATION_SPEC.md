# Her sound: variation, restraint, and events

**L3 design for the audio owner to review and implement. No runtime behavior or assets changed.**
Base: `ScraperX-Claude` at `3af6fbcc2a8b173f90f5a1b6cf2fd09be3366253`.
Brief: [issue #3, PE orchestration](https://github.com/IssisX/ScraperX/issues/3#issuecomment-6469372644), 2026-10-28.
The owner's request to do this section authorizes this document. **Update (integrator, 2026-10-28 9:20 AM Central):** the owner approved the merged fatality of that brief's section 3 at 8:20 AM. Its numbers are a 2.5 s escape window, 6-8 s from the second tear to the fade, and no re-grab for 30 s after a tear. Her gripping arm stretches to hold him in the hang. The design below for it now applies, bound to L5's and L4's edges once they exist.

## Result to aim for

Her mouth follows something her body does: a breath catches when she lifts, a grip complains when it takes weight, and a tear sounds when the body actually parts. Long holds contain space. Extra pitch jitter cannot make one recognizable gulp into several performances.

Keep the approved recordings and the full intensity of the catch, arm tears, and fatality. Add distinct recorded takes, change selection and timing, and retire continuous human mouth loops. No synthesized human voice or body/gore foley, no purchased assets. Overall ambience and footstep mix changes wait for the next playtest.

All numbers below are **proposed audio parameters**, for listening review by the audio owner. They change no force, reach, state duration, escape window, or save rule. No sound may extend an action to finish its playback.

## What the base actually does

These are source observations, not a listening diagnosis:

| Source at the base | Observation and implication |
|---|---|
| `godot/presentation/audio/sound_bank.gd`, `HER_RECORDED`, `build_her`, `pick` | 14 recorded files across 10 named families (both the declared counts and the actual Ogg files total 14; the brief says 15). `pick` samples with replacement. Croak, cluck and sniff each have one generated variant; step and joints have three. `_her_mouth(2.0)` makes the mouth loop. Selection needs history, and those human sounds need recorded replacements. |
| `godot/presentation/audio/audio_director.gd`, `matron_layers`, `_her_level`, `_her_swap` | Holding enables the mouth loop; stage enables growl/rage loops. A paused loop remembers its playback position. There is no maximum audible mouth/growl span. Breath repeats when a countdown expires. |
| `godot/presentation/audio/matron_sound.gd`, `update`, `hear_hold`, `_due`, `_tick_due` | Idle and effort timers generate cues. A queued swallow/laugh remembers a position and later checks only `_on`; it can outlive the action that caused it. Catch, takes, grabs, tears and claims can also cluster several voice cues. |
| `audio_director.gd`, `_her_cue`, `_her_free` | Cue counters increment before bank readiness, selection, voice availability or playback. A counted cue proves a request, not a sound heard. There are three body and two voice players. |
| `src/bridge/scraperx_simulation.cpp`, `get_matron_hold_state` | Existing outputs include `grabs`, `carries`, `dangles`, `armplay_beat`, `arm_tear_count`, `wrist_strain_ns`, `wrist_tear_ns`, `grip_load_n`, `held`, `claimed`, `fatality_beat_edges` and physical fatality counters. Most hold cues need no new simulation output. |

Keep existing cue names and their diagnostic counters meaningful. Add playback evidence beside them when implementing; do not change a test's expected counts as part of this document.

## Selection and event contract

1. **Distinct takes.** Each admitted take has a stable ID, source URL, source interval, family, recording/performance identity and safe onset/tail markers. Different encodes, overlaps of the same utterance, pitch shifts and different composites sharing the same foreground utterance are the same take for repeat prevention. Separate non-overlapping performances in a long recording may count separately after audition.
2. **One selection owner.** Keep history in the Matron audio path, shared by one-shots and the bounded growl/strain phrases. Do not modify selection for every world sound. Keep per-family shuffled bags and the last H actually started take IDs. Exclude that history before selecting; refill without repeating across the bag boundary. Select among eligible takes with a seeded audio-local RNG for reproducible tests.
3. **Aliases share history.** A grunt used for both effort and hit cannot bypass the rule by changing its cue name. Exclude the last foreground vocal performance across all mouth/voice families as well as each family's H-history. A foreground event has one mouth performance, even if several counters refer to it.
4. **Short banks are explicit.** Effective H is `min(H, N-1)` for N distinct usable takes. With N=1, the first play is possible; the next same-family play is silent until another take exists. Never label pitch variation as a second take or clear history because a state changed. For optional cues, an empty eligible set means silence. Required tear/contact feedback must have an independently recorded eligible body layer; report a missing required layer as a bank validation failure. Do not silently repeat it or substitute a synthesized voice.
5. **Commit on start.** Reserve a candidate, obtain a playback slot, start, then update its history and bag. Rejected, muted or missing clips do not consume selection history. A started clip that is later interrupted still counts. On camera/range changes keep history; pause freezes it. On checkpoint restore cancel pending cues, stop active Matron phrases and baseline counters/state before processing new edges. Retain history in the existing audio lifetime; if that owner is recreated, carry its small history across the reset. Do not change native save serialization for this.
6. **Events authorize; clocks only inhibit.** An elapsed cooldown, clip end or steady state never creates a mouth cue. Repetition requires a fresh action edge: lift, visible breath phase, grip load onset, sniff action, contact or tear. A scheduled response carries its encounter generation, event ID, expected state/beat and live attachment. Cancel it when those no longer match; resolve its position at playback, not queue time. Optional late responses expire after 250 ms. Body transients start on the event frame, without random scheduling delay.
7. **No hidden events.** Entering a state supplies at most one opportunity. For continuing breath, sniff, cluck or effort, use an actual matching animation phase/contact marker. These markers are **proposed presentation work**, not outputs the base already has. If an action is not drawn or its marker is unavailable, omit that optional cue rather than invent a timer or change her simulation behavior.

### Per-family starting values

N is the minimum distinct take count for the completed bank, including approved takes already present. Counts are production targets, not a claim that the candidate sources below yield that many usable cuts. P is the playback pitch/rate multiplier relative to an approved take. G is random gain in dB relative to that cue's approved nominal level, before the existing bus/spatial mix. O is additional start offset in milliseconds **inside annotated pre-onset silence only**; if there is no safe pre-roll, O=0. H is history depth; C is minimum time between audible starts in seconds. All times are audio presentation time excluding pause.

| Family | N | P | G dB | O ms | H | C s |
|---|---:|---|---|---|---:|---:|
| Breath | 6 | 0.96–1.04 | −2–0 | 0–20 | 3 | 1.8 |
| Effort | 6 | 0.96–1.04 | −1.5–0 | 0–10 | 3 | 1.2 |
| Growl | 4 | 0.94–1.02 | −2–0 | 0–15 | 2 | 3.5 |
| Rage | 4 | 0.96–1.03 | −1–0 | 0–10 | 2 | 4.0 |
| Strain | 4 | 0.96–1.04 | −2–0 | 0–15 | 2 | 0.8 |
| Swallow | 4 | 0.97–1.03 | −2–0 | 0–5 | 2 | 8.0 |
| Laugh | 3 | 0.97–1.02 | −2–0 | 0–10 | 2 | 12.0 |
| Shriek | 4 | 0.97–1.03 | −1–0 | 0–5 | 2 | 4.0 |
| Rip | 4 | 0.97–1.03 | −1–0 | 0 | 2 | 0.25 |
| Hit | 4 | 0.97–1.04 | −1.5–0 | 0 | 2 | 0.35 |
| Croak | 4 | 0.96–1.02 | −2–0 | 0–10 | 2 | 7.0 |
| Cluck | 4 | 0.98–1.03 | −2–0 | 0–5 | 2 | 6.0 |
| Sniff | 4 | 0.97–1.04 | −2–0 | 0–10 | 2 | 5.0 |
| Step/contact | 8 per active surface/contact class | 0.96–1.04 | −2–0 | 0 | 3 | 0.065 |
| Joints | 6 | 0.96–1.03 | −2–0 | 0 | 3 | 0.7 |

Apply P once. Do not multiply a new jitter by `_pitch()`, a hunt boost or another random rate. The base's strain sweep `0.7–1.6` and hunt step multiplier `1.25` are outside these proposals. Let measured load change strain level within its approved envelope, not sweep a throat or leather sound into a cartoon squeak. Playback rate also changes duration: enforce spans using actual output duration. No independent runtime time-stretch is required. Reject a take that only fits through extreme processing.

Cooldowns suppress optional decoration; they never suppress a new physical arm/leg tear or landed impact. Such events use a fresh eligible take and log a cooldown exception with the physical event ID. If several physical ticks arrive in one render frame, use counter deltas/ordered edges to account for them, coalesce coincident body sounds with explicit logged event IDs, and drop obsolete optional responses. Do not play an accumulated burst of old laughs at the latest position.

### What fires each family

An entry below is permission to select a cue, not an instruction to fill every beat. The event list covers catch, Carry, Dangle, ArmPlay and the existing fatality. Optional choices still obey cooldown, history and voice occupancy.

| Family | Authoritative or proposed action edge; behavior in the hold/fatality |
|---|---|
| Breath | `carries` rising: one short lift exhale. Later Carry/Dangle: a visible chest exhale marker only. ArmPlay pull onset: choose breath **or** effort. Fatality: one matched exertion/recovery exhale; none simply because a beat is taking time. Continuing breath markers are proposed. |
| Effort | Entry to ArmPlay pull (`armplay_beat` becomes 1), once; optional fresh regrip/lift marker, never every 1.4 s. Existing fatality: measured leg-pull onset or `brake_kick_count` rising. Need a confirmed hand/load onset for repeated pulls. |
| Growl | Hunting acquisition/action edge or a visible jaw/throat action; outside a busy hold voice slot. Catch/Carry/Dangle do not enable an endless stage loop. ArmPlay may take one phrase instead of effort, not both. |
| Rage | Turn burst **entry**, once per Turn episode; a new major attack action may offer another after cooldown. Do not start on every high-rage frame or loop under the fatality. Choose rage or shriek for one vocal action. |
| Strain | Held grip load rising across an audio-only onset threshold; ArmPlay pull entry or wrist strain rising through an audio-only band. Continue only while that load is present. Drop/tear, `held=false`, and beat exit end the span immediately. |
| Swallow | ArmPlay face beat (`armplay_beat` becomes 2), only at a matching visible throat/face action; otherwise omit. Existing fatality Wipe may use one on its matching action. Never every few idle seconds and never automatically after a sniff. |
| Laugh | At most one optional response per encounter, on ArmPlay face/throw action or a completed fatality action. Choose one opportunity; do not laugh separately on `takes`, tear, release and `claims`. The base's 0.6 s delayed laugh is not mandatory. |
| Shriek | Turn attack entry or a new fall/violent action, once. A passive carry/hang never needs it. Existing fatality can use one at a major exertion action if no rage/effort voice already occupies it. |
| Rip | `arm_tear_count` or `leg_tear_count` rising: body layer at the actual separation. No rip on ArmPlay entry, strain threshold, or a fatality beat that timed out. |
| Hit | `staggers`/`knockdowns` rising, or a landed player strike event supplied by the punch owner: one reaction, deduplicated if they describe the same blow. Fatality pin/landing uses confirmed contact counters (`knee_pin_count`, `fall_landing_count`), with its physical impact layer. |
| Croak | A visible throat/croak action after search/listen transition. At most once per such action; Carry/Dangle may use one only if that action is drawn. No listening-dwell or idle timer. Marker is proposed. |
| Cluck | `grabs` rising with confirmed grip: optional short recorded tongue action if drawn; suppress a second request from `takes`. Alternatively the visible ArmPlay face action, if it did not choose swallow/sniff. Never a bird recording. |
| Sniff | Catch inspection or ArmPlay face action, once when her nose actually approaches; searching uses a visible sniff/head action. `catches` alone is insufficient if no sniff is drawn. Marker is proposed. |
| Step/contact | Each new **planted** limb contact from L1's proposed gait/contact marker, with surface and limb position. In Carry, only the supporting feet; in Dangle/ArmPlay, only a real replant. Catch uses a grip/contact recording at the hand, not a pitched footstep. No contact cue while airborne. |
| Joints | A new contortion reversal, loaded regrip, or rise from Down with a matching pose marker. One localized creak/crack, never every crawl stride or at a fixed beat frequency. In fatality, only on an actual articulated load/release. Marker is proposed. |

For the hold, existing `state`, `grip_on`, `held`, `matron_hand`, `matron_hand2`, `grip_load_n` and wrist fields are enough to gate initial spans and positions. Suggested **audio-only** wrist bands are strain/tear threshold crossings at 0.25 and 0.65, re-armed below 0.15 and 0.50. Use the native `wrist_tear_ns` denominator; do not change it. Bands grant at most one optional texture each per pull, not another physical tear. Require `held`/correct beat and a real grip for grip textures; movement or an animation angle alone is insufficient evidence of loading.

If L1 does not expose limb plants, phase and load markers, L6 must request them from their owner. That dependency is explicit; distance/stride counters are not proof of a planted hand. Do not replace a known contact with a free-running clock to finish the sound pass.

## Layers and silence

Use the existing three body and two voice players. On the Matron voice pool allow one foreground mouth performance at a time, plus at most one quieter breath tail when anatomically plausible. These limits do not silence the player's independent pain/voice path. A physical tear/impact has priority over optional cloth/joints; optional voice gives way to a current required reaction. Do not increase pool size to hide congestion.

| Action | Mouth and breath | Cloth, bone and grip | Deliberate space |
|---|---|---|---|
| Catch confirmed | A short caught inhale **or** cluck, if her action shows it. | Hand/ankle contact at `matron_hand`; brief cloth compression. No bone break. | Let contact speak first; avoid stacking sniff, cluck and laugh. |
| Carry lift and walk | One low exhale on lift, then breath only on matched chest phases. | A short cloth drag on lift; planted feet; grip texture only on load onset/change. | Keep at least 50% of each 6 s Carry/Dangle window free of her mouth/voice; leave the tower and player audible. |
| Dangle held | One restrained strain/breath action, if loaded and drawn. | At most a short hand strain on a new load or brace, then natural tail. | A steady held pose stays quiet. No perpetual gurgle under a long hang. |
| ArmPlay reach and pull | Choose one effort exhale at the pull. | Cloth draws taut; one leather/grip strain phrase; no crack before physical failure. | Leave a 150–300 ms opening immediately before the expected separation by inhibiting optional voices; never delay the sim to create it. |
| Arm actually tears | Keep the player's existing cry. Her optional voice yields. | A single authored composite: recorded dry crack, fibrous tear and short wet body tail at separation. Existing rip composites already contain these; do not add a second bone layer over them. | Optional mouth stays out for 300–600 ms after separation. No automatic laugh erasing the impact. |
| ArmPlay face/swing/throw | On a visible action choose **one** sniff, swallow or restrained laugh for the whole response. | Clothing follows the throw/contact if present. | If the action finishes early, drop its delayed response. |
| Ordinary release/spare | Usually a breath tail or nothing. | Cloth/contact only at the supported landing actually confirmed. | Cancel held strain and deferred taunts. This design grants no open-air ordinary drop. |

The quiet windows concern optional Matron voice, not a reduction in gore, player pain or necessary collision feedback. No overall ambience/footstep mix pass is included. Position mouth/breath at her live head, strain at the loaded hand, cloth at the moving cloth/body, bone/rip at the separation, and contacts at the planted limb. Follow those attachments through slow motion; do not leave a laugh at her old position.

### Existing fatality: event map usable before any redesign approval

`Fatal` is defined in `src/sim/matron.hpp`: Lay, Pin, Leg0, Leg1, Drag, Kick, Haul, Fall, Wipe, Black. `fatality_beat_edges` preserves beat transitions; physical counters confirm the act. A beat entry can authorize posture/cloth, but never a break or impact that did not happen.

| Native fact | Layer and limit |
|---|---|
| Lay/Pin entry; `knee_pin_count` rises | Cloth down, then one confirmed body impact on pin. One low breath; no repeated crunch while waiting. |
| Leg0/Leg1 pull; `leg_tear_count` rises | One load strain phrase for each pull and a **different** rip composite per actual tear. Preserve the player's existing leg-break reaction. |
| Drag with actual body motion | Recorded cloth drag in bounded phrases. Stop when motion stops or the beat ends; no synthetic human/gore layer. |
| `brake_kick_count`, `haul_count`, `knot_part_count`, `fall_landing_count` rise | Their actual mechanism/contact layers; one effort at kick if free. Only knot parting/landing warrants its rupture/impact sound. Do not replay on each frame of Haul/Fall. |
| Wipe action; `waste_not_count` rises | One recorded cloth wipe and, if visibly matched, swallow. Preserve the existing line's own event/voice path; do not duplicate it. |
| Black entry / restore | Cancel her mouth and strain; allow 0.8 s of her silence if this beat lasts that long. No invented breath from her during a black screen. Restore invalidates all old cues immediately. |

### Merged fatality: conditional design only

If the coordinator posts approval of orchestration section 3, use L5's confirmed ankle-lift, loaded-hang, hold-break, release and restore edges and L4's director phases. Those semantic edges are **requested contracts**, not claimed bridge fields. The sound owner binds them once defined.

Lift: cloth draws and one breath; hang: sparse loaded grip texture then silence; actual pre-rip escape blow: impact/release and immediate cancellation of her pending cues; actual tear: the recorded separation composite. Release: a short cloth slip, **no new rip just because he falls**. Shaft pull-out: air/shaft perspective only; do not replay or stretch a mouth syllable to fill the camera move. Fade/restore cancels the encounter's sounds. No new laugh or scream loop during slow motion.

This specifies no 2–3 s escape window, re-grab cooldown or total fatality duration. Those decisions remain with the coordinator and simulation owner. Until approval, preserve `rip_begun()`'s existing rule. Sound cannot open an escape, release the player or cause checkpoint restore.

### Audible-span budgets

Treat human loops as finite recorded phrases, even if their old cue names end in `_loop`:

| Material | Maximum continuous audible span | May start again only when |
|---|---:|---|
| Mouth (croak/cluck/sniff/swallow) | One natural take, at most 1.2 s | Another matching mouth action occurs and C/history allow it. |
| Breath | One complete inhale/exhale phrase, at most 2.2 s | A fresh visible breath phase occurs and C allows it. |
| Growl / rage | 1.8 s / 2.2 s | A fresh attack/jaw action occurs; stage or clip end is insufficient. |
| Loaded grip strain / cloth drag | 1.4 s / 1.5 s | A distinct regrip/load/motion action occurs, with at least 0.6 s of no such texture between phrases. |
| Laugh / shriek | 1.5 s / 2.0 s | A fresh eligible event; laugh is still at most once per encounter. |

Choose/cut an offline auditioned phrase that fits naturally at its slowest permitted P; do not cut a vowel mid-action at the cap. Human spans do not repeat their interior at runtime. Use safe 10–30 ms take-edge fades and 50–100 ms release fades for load textures; cancel them promptly on loss of load. Pausing cannot reset a phrase's used span or resume an obsolete mouth halfway through a different action. Nonhuman air/steel beds retain their existing world behavior.

## CC0 recording shortlist

Metadata checked against the linked **uploader pages on 2026-10-28**: each row displays Creative Commons 0, and duration is that of the full source, not a promised usable cut. License links are [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/). These are **sourcing/audition candidates**, not downloaded, auditioned, imported or newly approved sounds. Described physical recordings/performances are distinguished from designed foley. Reuse the existing approved source material first; do not relicense material from a pack's promotional link or assume its other files are CC0.

| ID | Specific recording / creator and source URL | License | Full duration s | What to audition it for |
|---|---|---|---:|---|
| S01 | [Old lady breathing – panic / Vampirella17](https://freesound.org/people/Vampirella17/sounds/82429/) | CC0 1.0 | 53.335 | Existing breath source. Seek additional separate breaths in the same performance; preserve her identity. |
| S02 | [Kali, Breath Heavy, Wheeze_2 / hisoul](https://freesound.org/people/hisoul/sounds/520280/) | CC0 1.0 | 7.422 | Recorded female breath/wheeze alternatives; compare timbre to S01. |
| S03 | [Zisa, Relief, Moan, Tired, Exhale_2 / hisoul](https://freesound.org/people/hisoul/sounds/520276/) | CC0 1.0 | 1.001 | Short exertion/recovery exhale. |
| S04 | [soft grunt / Reitanna](https://freesound.org/people/Reitanna/sounds/252235/) | CC0 1.0 | 0.290 | A distinct light effort or hit vocal, one shared take identity. |
| S05 | [heavy grunt / Reitanna](https://freesound.org/people/Reitanna/sounds/344004/) | CC0 1.0 | 0.545 | Already part of the hit source; possible heavier effort, sharing history. |
| S06 | [Kali, Recharge to Attack, Scream, Furious_6 / hisoul](https://freesound.org/people/hisoul/sounds/520277/) | CC0 1.0 | 1.676 | Existing effort source; full take or shorter natural action. |
| S07 | [Zisa, Attack, Growl Double, Bark, Low Pitched / hisoul](https://freesound.org/people/hisoul/sounds/520283/) | CC0 1.0 | 0.952 | Recorded performer growl; reject if the bark makes her sound like an animal. |
| S08 | [Zisa, Attack, Growl, High Pitched, Angry_7 / hisoul](https://freesound.org/people/hisoul/sounds/520282/) | CC0 1.0 | 0.778 | Rage accent, kept as a short performance, never repeated as a bed. |
| S09 | [Scream, female x2 / peridactyloptrix](https://freesound.org/people/peridactyloptrix/sounds/208893/) | CC0 1.0 | 7.475 | Two recorded alto scream candidates; source reports room reverb. |
| S10 | [Woman Screaming / DanJFilms](https://freesound.org/people/DanJFilms/sounds/845164/) | CC0 1.0 | 3.604 | Additional recorded shriek; use only if a natural phrase fits the span. |
| S11 | [Laugh – female / giordrix](https://freesound.org/people/giordrix/sounds/511172/) | CC0 1.0 | 25.429 | Separate restrained breath-laugh candidates. Reject cheerful/game-show phrasing. |
| S12 | [short female laugh/chuckle / amoyssiadis](https://freesound.org/people/amoyssiadis/sounds/397673/) | CC0 1.0 | 1.010 | A compact laugh alternative; audition for menace, not a comic giggle. |
| S13 | [clean wet gulps / Wizage](https://freesound.org/people/Wizage/sounds/418853/) | CC0 1.0 | 7.911 | Existing swallow source; separate natural gulps, no cyclic gurgle. |
| S14 | [Swallowing / NightDrawr](https://freesound.org/people/NightDrawr/sounds/751816/) | CC0 1.0 | 2.385 | Additional single/double human gulps. |
| S15 | [Cloth_Noise / drewsimko](https://freesound.org/people/drewsimko/sounds/682869/) | CC0 1.0 | 1.583 | Recorded fabric rub for the lift, hold and wipe. |
| S16 | [Fabric flaps / PelicanPolice](https://freesound.org/people/PelicanPolice/sounds/580967/) | CC0 1.0 | 38.954 | Multiple shirt movements for lift/throw contacts; isolate distinct actions. |
| S17 | [Leather Creak / Stretching / IENBA](https://freesound.org/people/IENBA/sounds/536187/) | CC0 1.0 | 9.216 | Existing strain source: a recorded twisted leather belt, not a human vocal. Find additional distinct tension actions. |
| S18 | [Tear 3 / JarredGibb](https://freesound.org/people/JarredGibb/sounds/217318/) | CC0 1.0 | 1.194 | Recorded gore-foley candidate for a distinct separation layer; verify the texture by ear. |
| S19 | [FabricRipping2Stereo / SomewhatImmoral](https://freesound.org/people/SomewhatImmoral/sounds/688081/) | CC0 1.0 | 89.521 | Multiple recorded fabric tears for the fibrous component; not wet-body foley by itself. |
| S20 | [Large Bone Crunch / personwhois](https://freesound.org/people/personwhois/sounds/458974/) | CC0 1.0 | 1.903 | Designed foley made from recorded broken pasta. Dry fracture layer only; not a claim of recording human bone. |
| S21 | [Voices, Mouth, Tongue Cluck, Click, Clack / Vein_Adams](https://freesound.org/people/Vein_Adams/sounds/711535/) | CC0 1.0 | 5.320 | Recorded tongue actions for cluck/mouth; keep single clicks out of the gait. |
| S22 | [to clear one's throat / florianreichelt](https://freesound.org/people/florianreichelt/sounds/447496/) | CC0 1.0 | 3.797 | Human throat texture for croak; reject a recognizable polite cough or incompatible voice. |
| S23 | [Clearing the throat / CreepyKing](https://freesound.org/people/CreepyKing/sounds/842294/) | CC0 1.0 | 1.233 | Another recorded throat-clear alternative, subject to the same audition. |
| S24 | [Sniffing / padpad](https://freesound.org/people/padpad/sounds/516473/) | CC0 1.0 | 6.188 | Uploader describes three recorded sniff variations. |
| S25 | [Sniff / OwlStorm](https://freesound.org/people/OwlStorm/sounds/151218/) | CC0 1.0 | 1.009 | A fourth nose-inhale candidate; compare with S24's recording perspective. |
| S26 | [Footsteps on Metal / IENBA](https://freesound.org/people/IENBA/sounds/698697/) | CC0 1.0 | 10.305 | Recorded isolated stair contacts for feet. Do not use shoe clicks as palm contacts. |
| S27 | [VSH-30-hand-slap-metal pipe-short / newagesoup](https://freesound.org/people/newagesoup/sounds/369088/) | CC0 1.0 | 1.895 | Recorded hand-on-metal contact for planted palms; source has stairwell processing/reverb to audition. |
| S28 | [Cracking joints / lucantunes](https://freesound.org/people/lucantunes/sounds/390962/) | CC0 1.0 | 44.577 | Separate recorded joint cracks for rare contortion/load reversals. |
| S29 | [cracking joints-02 / bajko](https://freesound.org/people/bajko/sounds/387897/) | CC0 1.0 | 3.521 | Additional hand-joint recording. |

S02–S08's hisoul pages describe recordings with singers/voice performers, rather than code-generated voices. S17/S19/S20 are physical-object foley for strain/tear/crack layers; do not pass them off as human tissue recordings. Existing approved growl/rage and shriek/laugh sources in `audio/matron/CREDITS.md` remain reusable; this table does not revoke them or assert that every old source was independently rechecked.

The shortlist does **not yet prove** four compatible growls, four rage takes, six efforts, four croaks/clucks or eight contacts in every active surface/contact class. The audio owner audits usable take counts after audition, first taking new non-overlapping performances from existing long recordings. If still short, find additional CC0 recordings before admitting that family at the production minimum; do not duplicate an utterance or treat a pitched copy as filling the gap. Optional families can remain sparse/silent while being filled. Required tear/contact feedback needs its bank complete before L6 is called done.

When importing, the audio owner records uploader, title, original URL, CC0 link, retrieval date, source file hash, exact source intervals and offline edits in `godot/presentation/audio/matron/CREDITS.md`, with a stable take/performance identity in the bank's chosen metadata. Original-source download may require a Freesound account; no login or download is performed for this design. Do not replace a verified source with an unverified mirror or an AI recreation if download is blocked. Retain the current mono/22,050 Hz delivery convention unless the audio owner measures a reason to change it. Align perceived levels per family before applying G, preserve transient headroom, and audition on the phone speaker as well as headphones.

## Checks for L6 to implement

These are **test specifications, not executed tests or changes to existing test checks**. Runtime and listening acceptance remain untested in this docs-only lane. A cue-count PASS cannot substitute for playback/asset evidence.

| Check | Input and assertion |
|---|---|
| Selection | For each complete family, 1,000 eligible triggers with cooldown satisfied over at least 10 fixed audio RNG seeds: no adjacent repeated take, no member of its last H audible takes, including every bag refill. Each shuffled bag visits every take when no other eligibility rule excludes it. |
| Degenerate banks / aliases | Empty, singleton, N=2 and N<=H banks terminate with the documented effective history; singleton never repeats after its first play. Reencoded copies and effort/hit aliases of the same performance do not evade history. Optional exhaustion is logged silence; missing required feedback is a reported failure. |
| Playback truth | Drive mute, missing file, no free player and interruption. Log requested, suppressed (reason), selected, playback-started, interrupted and finished separately. Only playback-started commits history. Every required separation event has started body feedback or fails validation. |
| Ownership of events | Hold each snapshot constant for 30 s in Carry, Dangle and ArmPlay: no new mouth cue without a fresh authorized event/animation marker. Repeated render frames of one counter do not replay; one new edge plays at most once. A rapid counter/beat advance coalesces only as documented, with every consumed event ID accounted for. |
| Timing and spans | At 20, 30, 60 and 120 rendered fps, with the same recorded sim/action events, emitted events/order match. Check P/G/O, C and output-duration caps after rate changes. No human phrase loops internally or restarts because its cap or cooldown elapsed. Carry/Dangle mouth occupancy <=50% in each complete 6 s window. |
| Cancellation | Queue a response, then lose grip, leave beat, hide, pause, die, fade or restore. Stale responses never play. Resume only a still-valid action within its remaining span. Restoring a lower counter value baselines it without phantom tears/laughs; existing history still prevents an immediate repeat. |
| Layer collision | Trigger catch + takes + grabs, then a tear + claim, and a strike + stagger + knockdown. One semantic vocal action each, physical feedback preserved, no double bone layer inside an already-composite rip, no optional voice stealing a required body slot. |
| Contact | Airborne crawl frames produce no step/contact; each audible plant references an actual L1 contact ID, surface and limb location. A held steady pose produces no repeated joint cracks. No step pitch multiplier outside the table. |
| Asset admission | Count distinct auditioned performance IDs, check source/license/cut records, positive durations, safe starts/tails, required minima and no human/gore fallback synthesis. Demonstrate an audible missing-bank failure rather than passing on cue counts alone. |
| Listening gate | Phone-watchable real-play capture: chase/Turn, catch→Carry→Dangle→ArmPlay, repeated encounter, actual tear, current fatality, pause/restore. Listen for repeat recognition, gait clicks, pitch comedy, displaced voices and crowded mouth layers. Keep intentional silence and full tear impact. The owner judges immersion; hashes and meters do not prove it. |

If the merged fatality is approved, add its hang→escape or hang→release→fade→restore capture and cancellation assertions once L4/L5 supply the edges. Do not build or test the unapproved sequence through this specification.

## Handoff boundary

Only this file belongs to L3. The audio owner reviews the parameter proposals, auditions/adopts recordings, owns all audio/credit edits and implements L6 after its dependencies. Gait/breath/contact marker requests go to the movement owner. This document does not authorize any test, physics, fatality-rule, save, mix or camera change, and makes no claim that the APK's sound is fixed.
