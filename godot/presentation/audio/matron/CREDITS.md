# The Matron's recorded sounds: credits

Real recordings, never made in code (the owner's rule, 2026-10-27: human sounds are real, never
synthesized). The owner picked each by ear on 2026-10-27. Every source is CC0 1.0, a public-domain
dedication: no credit is required, and it is given here anyway. `sound_bank.gd` `build_her()`
loads them; the `_loop_` files play as loops.

Each file is mono Ogg Vorbis at 22,050 Hz, cut from its sources (the part used is given),
high-passed, faded in and out (the loops: crossfaded end into head), and peak-normalized to
-1 dBFS; the laugh is slowed 10%, and the breaths and the effort have their hiss taken out.
A file built from several sources layers them, each matched in level first.

Where a Freesound recording was fetched from a mirror, the mirror's metadata gives its license
as CC0 1.0 (the Freesound page itself was not reachable from the build machine): the LAION
Freesound set (huggingface.co/datasets/benjamin-paine/freesound-laion-640k, its split and row
given) or FSD50K (huggingface.co/datasets/Fhrozen/FSD50k). OpenGameArt files are CC0 on their
own pages.

| File | Source | Title | Author | License | Part used (s of the source, placed at) |
|---|---|---|---|---|---|
| `matron_growl_loop_0.ogg` | https://freesound.org/people/angelkunev/sounds/560585/ | Z-Fem 01 - Roar | angelkunev | CC0 1.0 (LAION Freesound, train row 25604) | 0.00-1.05, at 0.00 |
|  | https://freesound.org/people/angelkunev/sounds/560591/ | Z-Fem 03 - Roar | angelkunev | CC0 1.0 (LAION Freesound, train row 70082) | 0.00-0.85, at 1.00 |
|  | https://freesound.org/people/angelkunev/sounds/560592/ | Z-Fem 02b - Grunt | angelkunev | CC0 1.0 (LAION Freesound, test row 42735) | 0.10-0.80, at 1.80 |
| `matron_growl_rage_loop_0.ogg` | https://freesound.org/people/lolamadeus/sounds/179345/ | Monster Zombie Scream | lolamadeus | CC0 1.0 (LAION Freesound, test row 15820) | 0.12-1.80 |
| `matron_strain_loop_0.ogg` | https://freesound.org/people/IENBA/sounds/536187/ | Leather Creak / Stretching | IENBA | CC0 1.0 (LAION Freesound, train row 273380) | 6.70-8.01 |
| `matron_shriek_0.ogg` | https://freesound.org/people/pushkin/sounds/241591/ | pandoramachineannalisaloefflerscreams_03.flac | pushkin | CC0 1.0 (FSD50K) | 0.30-1.70 |
| `matron_shriek_1.ogg` | https://freesound.org/people/pushkin/sounds/241592/ | pandoramachineannalisaloefflerscreams_02.flac | pushkin | CC0 1.0 (FSD50K) | 0.14-1.64 |
| `matron_laugh_0.ogg` | https://freesound.org/people/Reitanna/sounds/343988/ | evilish laugh.wav | Reitanna | CC0 1.0 (FSD50K) | 0.05-1.78 |
| `matron_effort_0.ogg` | https://freesound.org/people/hisoul/sounds/520277/ | Kali, Recharge to Attack, Scream, Furious 6 | hisoul | CC0 1.0 (LAION Freesound, train row 84884) | 0.30-1.62 |
| `matron_hit_0.ogg` | https://opengameart.org/content/40-wet-towel-clubpoundhitattack-sounds | 40 wet towel club/pound/hit/attack sounds (wet_towel_on_body-34.flac) | qubodup | CC0 1.0 | 0.00-0.45, at 0.00 |
|  | https://freesound.org/people/Reitanna/sounds/344004/ | heavy grunt | Reitanna | CC0 1.0 (LAION Freesound, train row 357390) | 0.06-0.52, at 0.03 |
|  | https://freesound.org/people/Reitanna/sounds/344010/ | growly | Reitanna | CC0 1.0 (LAION Freesound, train row 363999) | 0.20-0.75, at 0.40 |
| `matron_swallow_0.ogg` | https://freesound.org/people/Wizage/sounds/418853/ | clean wet gulps.wav | Wizage | CC0 1.0 (FSD50K) | 6.56-7.18 |
| `matron_rip_0.ogg` | https://freesound.org/people/JarredGibb/sounds/217307/ | Multiple Tears | JarredGibb | CC0 1.0 (LAION Freesound, train row 76114) | 0.00-0.90, at 0.00 |
|  | https://opengameart.org/content/fleshy-bone-breaksnap-sfx | Fleshy Bone Break/Snap SFX (Wet Break 3.wav) | zane-little-music | CC0 1.0 | 0.00-0.65, at 0.35 |
|  | https://opengameart.org/content/8-wet-squish-slurp-impacts | 8 wet squish, slurp impacts (impactsplat03) | qubodup (Independent nu ljudbank) | CC0 1.0 | 0.00-0.40, at 0.95 |
|  | https://freesound.org/people/ReiyaManor/sounds/351623/ | Water Drops.wav | ReiyaManor | CC0 1.0 (FSD50K) | 3.30-3.50, at 1.28 |
| `matron_rip_1.ogg` | https://freesound.org/people/JarredGibb/sounds/217309/ | Multiple tears 2 | JarredGibb | CC0 1.0 (LAION Freesound, train row 381965) | 2.10-2.85, at 0.00 |
|  | https://opengameart.org/content/fleshy-bone-breaksnap-sfx | Fleshy Bone Break/Snap SFX (Wet Break 7.wav) | zane-little-music | CC0 1.0 | 0.00-0.80, at 0.25 |
|  | https://freesound.org/people/JarredGibb/sounds/217317/ | Squelch 1 | JarredGibb | CC0 1.0 (LAION Freesound, train row 168) | 0.05-0.65, at 0.80 |
|  | https://freesound.org/people/ReiyaManor/sounds/351623/ | Water Drops.wav | ReiyaManor | CC0 1.0 (FSD50K) | 4.22-4.50, at 1.20 |
| `matron_breath_0.ogg` | https://freesound.org/people/Vampirella17/sounds/82429/ | Old lady breathing - panic | Vampirella17 | CC0 1.0 (LAION Freesound, test row 49349) | 0.25-1.83 |
| `matron_breath_1.ogg` | https://freesound.org/people/Vampirella17/sounds/82429/ | Old lady breathing - panic | Vampirella17 | CC0 1.0 (LAION Freesound, test row 49349) | 2.50-4.12 |
| `matron_breath_2.ogg` | https://freesound.org/people/Vampirella17/sounds/82429/ | Old lady breathing - panic | Vampirella17 | CC0 1.0 (LAION Freesound, test row 49349) | 8.70-10.40 |
| `matron_plant_0.ogg` | https://opengameart.org/content/12-wet-towel-hittingfallingpunching-floor-sounds | 12 wet towel hitting/falling/punching floor sounds (wet_towel_on_floor-05.flac) | qubodup | CC0 1.0 | 0.00-0.40 (whole) |
| `matron_plant_1.ogg` | https://opengameart.org/content/12-wet-towel-hittingfallingpunching-floor-sounds | 12 wet towel hitting/falling/punching floor sounds (wet_towel_on_floor-09.flac) | qubodup | CC0 1.0 | 0.00-0.63 (whole) |
| `matron_plant_2.ogg` | https://freesound.org/people/sagetyrtle/sounds/98155/ | washcloth | sagetyrtle | CC0 1.0 (LAION Freesound, test row 48691) | 4.65-5.00, at 0.00 |
|  | https://opengameart.org/content/37-hitspunches | 37 hits/punches (hit05) | qubodup (Independent nu ljudbank) | CC0 1.0 | 0.17-0.50, at 0.00, 10 dB under |

Her palms and feet on the steel on all fours (`matron_plant_*`, 2026-10-28): each cut is slowed 15%
(pitch and speed), high-passed at 50 Hz and low-passed at 2.5 kHz so no click is left at its head,
faded in over 4 ms and out over 60 ms, and peak-normalized to -1 dBFS; `matron_sound.gd` plays them
quietly (-17 to -9 dB) and low (pitch 0.80-0.95), at most one every 0.34 s.

## More takes (2026-10-29)

So that no family of hers has fewer than three real takes, the other candidates of the round the
owner auditioned on 2026-10-27 (the files under `03_EXECUTION/wip_20261027/her_real/` on the
wip-notes branch, with their `provenance.csv`) are added byte for byte, as the picks above were; the
owner's pick stays take 0 (the shriek's and the rip's: takes 0 and 1). Prepared like those: cut,
high-passed, faded (the loops crossfaded), peak-normalized to -1 dBFS; some were slowed when cut, as
the laugh above was (varispeed, pitch and speed together): `matron_growl_loop_1` and `_2`,
`matron_laugh_2` and `matron_effort_1` by 10%, `matron_growl_rage_loop_2`, `matron_shriek_3` and
`matron_laugh_1` by 8%; `matron_effort_1` also has its hiss taken out. Each is its own performance,
not a pitched copy of another take. The breath already had three takes of the pick and gets none.
Croak, cluck and sniff are now real, individually recorded, nonoverlapping CC0 takes
documented in the Mouth takes section below. No source is pitch-shifted into
a second performance.

| File | Source | Title | Author | License | Part used (s of the source, placed at) |
|---|---|---|---|---|---|
| `matron_growl_loop_1.ogg` | https://freesound.org/people/Reitanna/sounds/343979/ | the most disgusting growl | Reitanna | CC0 1.0 (LAION Freesound, train row 400470) | 1.66-4.42 |
| `matron_growl_loop_2.ogg` | https://freesound.org/people/Reitanna/sounds/241545/ | painful growl | Reitanna | CC0 1.0 (LAION Freesound, train row 397946) | 0.06-1.25, at 0.00 |
|  | https://freesound.org/people/Reitanna/sounds/241580/ | mmm growl | Reitanna | CC0 1.0 (LAION Freesound, train row 415524) | 0.06-1.36, at 1.27 |
| `matron_growl_rage_loop_1.ogg` | https://freesound.org/people/hisoul/sounds/520269/ | Kali, Attack, Scream, Fanatic 3 | hisoul | CC0 1.0 (LAION Freesound, train row 76528) | 0.00-1.00, at 0.00 |
|  | https://freesound.org/people/hisoul/sounds/520282/ | Zisa, Attack, Growl, High Pitched, Angry 7 | hisoul | CC0 1.0 (LAION Freesound, train row 286519) | 0.02-0.74, at 0.95 |
| `matron_growl_rage_loop_2.ogg` | https://freesound.org/people/Reitanna/sounds/344005/ | gruesome growl | Reitanna | CC0 1.0 (LAION Freesound, train row 307103) | 0.00-0.95, at 0.00 |
|  | https://freesound.org/people/Reitanna/sounds/344010/ | growly | Reitanna | CC0 1.0 (LAION Freesound, train row 363999) | 0.12-1.10, at 0.98 |
| `matron_strain_loop_1.ogg` | https://freesound.org/people/OldCouches/sounds/559003/ | Slow Leather Stretch | OldCouches | CC0 1.0 (LAION Freesound, train row 108550) | 0.55-1.86 |
| `matron_strain_loop_2.ogg` | https://freesound.org/people/mickdow/sounds/253327/ | Rubber Stretch Rip 1 | mickdow | CC0 1.0 (LAION Freesound, train row 228057) | 5.55-6.86 |
| `matron_shriek_2.ogg` | https://freesound.org/people/Reitanna/sounds/241515/ | scream.wav | Reitanna | CC0 1.0 (FSD50K) | 0.17-1.67 |
| `matron_shriek_3.ogg` | https://freesound.org/people/Reitanna/sounds/344028/ | cracky ah.wav | Reitanna | CC0 1.0 (FSD50K) | 0.02-1.00 |
| `matron_shriek_4.ogg` | https://opengameart.org/content/evil-screech-1 | Evil Screech 1 | nocturnalvanguard (AuraVoice) | CC0 1.0 | 1.85-2.98 |
| `matron_shriek_5.ogg` | https://opengameart.org/content/evil-screech-1 | Evil Screech 1 | nocturnalvanguard (AuraVoice) | CC0 1.0 | 3.78-4.85 |
| `matron_laugh_1.ogg` | https://freesound.org/people/Reitanna/sounds/343987/ | evil laugh2.wav | Reitanna | CC0 1.0 (FSD50K) | 0.02-1.80 |
| `matron_laugh_2.ogg` | https://freesound.org/people/sagetyrtle/sounds/73123/ | 0524laugh | sagetyrtle | CC0 1.0 (LAION Freesound, test row 26371) | 157.05-158.80 |
| `matron_effort_1.ogg` | https://freesound.org/people/Reitanna/sounds/344045/ | long grunt | Reitanna | CC0 1.0 (LAION Freesound, train row 391850) | 0.00-1.00 |
| `matron_effort_2.ogg` | https://opengameart.org/content/female-hurt-grunts-groans | Female Hurt Grunts & Groans | nocturnalvanguard (AuraVoice) | CC0 1.0 | 4.88-6.15 |
| `matron_hit_1.ogg` | https://opengameart.org/content/37-hitspunches | 37 hits/punches (hit05) | qubodup (Independent nu ljudbank) | CC0 1.0 | 0.17-0.67, at 0.00 |
|  | https://freesound.org/people/hisoul/sounds/520275/ | Zisa, Pain, Shout Short, Impact 6 | hisoul | CC0 1.0 (LAION Freesound, train row 281519) | 0.00-0.45, at 0.02 |
|  | https://freesound.org/people/hisoul/sounds/520282/ | Zisa, Attack, Growl, High Pitched, Angry 7 | hisoul | CC0 1.0 (LAION Freesound, train row 286519) | 0.08-0.63, at 0.38 |
| `matron_hit_2.ogg` | https://freesound.org/people/sagetyrtle/sounds/98155/ | washcloth | sagetyrtle | CC0 1.0 (LAION Freesound, test row 48691) | 4.65-5.00, at 0.00 |
|  | https://opengameart.org/content/12-wet-towel-hittingfallingpunching-floor-sounds | 12 wet towel hitting/falling/punching floor sounds (wet_towel_on_floor-11.flac) | qubodup | CC0 1.0 | 0.00-0.45, at 0.00 |
|  | https://opengameart.org/content/female-rpg-voice-starter-pack | Female RPG Voice Starter Pack (Type 3/damaged1.wav) | cicifyre | CC0 1.0 | 0.00-0.30, at 0.03 |
|  | https://freesound.org/people/angelkunev/sounds/560592/ | Z-Fem 02b - Grunt | angelkunev | CC0 1.0 (LAION Freesound, test row 42735) | 0.15-0.70, at 0.33 |
| `matron_swallow_1.ogg` | https://opengameart.org/content/liquid-bottle-drink-set | Liquid Bottle Drink Set (swallow-05.flac) | qubodup | CC0 1.0 | 0.06-0.76 |
| `matron_swallow_2.ogg` | https://freesound.org/people/Wizage/sounds/418853/ | clean wet gulps.wav | Wizage | CC0 1.0 (FSD50K) | 1.84-2.40 (a different gulp of the file `matron_swallow_0` is cut from) |
| `matron_rip_2.ogg` | https://freesound.org/people/IENBA/sounds/607909/ | Gore Cabbage | IENBA | CC0 1.0 (LAION Freesound, train row 279895) | 0.40-1.60, at 0.00 |
|  | https://freesound.org/people/Globbie/sounds/493512/ | HOR Gore Celeri 01 | Globbie | CC0 1.0 (LAION Freesound, train row 97825) | 0.40-0.75, at 0.30 |
|  | https://freesound.org/people/JarredGibb/sounds/217317/ | Squelch 1 | JarredGibb | CC0 1.0 (LAION Freesound, train row 168) | 0.05-0.85, at 0.55 |
|  | https://freesound.org/people/Iamgiorgio/sounds/371830/ | I7_dripping ceiling.mp3 | Iamgiorgio | CC0 1.0 (FSD50K) | 1.15-1.48, at 1.15 |
| `matron_rip_3.ogg` | https://freesound.org/people/IENBA/sounds/607909/ | Gore Cabbage | IENBA | CC0 1.0 (LAION Freesound, train row 279895) | 11.40-12.60, at 0.00 |
|  | https://freesound.org/people/Globbie/sounds/493521/ | HOR Gore Celeri 13 | Globbie | CC0 1.0 (LAION Freesound, train row 290072) | 0.12-0.72, at 0.25 |
|  | https://freesound.org/people/JarredGibb/sounds/217313/ | Squelch 2 | JarredGibb | CC0 1.0 (LAION Freesound, train row 206192) | 0.10-1.00, at 0.60 |
|  | https://freesound.org/people/Iamgiorgio/sounds/371830/ | I7_dripping ceiling.mp3 | Iamgiorgio | CC0 1.0 (FSD50K) | 1.70-2.00, at 1.20 |
| `matron_rip_4.ogg` | https://freesound.org/people/ProductionNow/sounds/234237/ | Eating Juicy Meat.wav | ProductionNow | CC0 1.0 (FSD50K) | 0.20-1.50, at 0.00 |
|  | https://freesound.org/people/ABouch/sounds/174943/ | Breaking Frozen Celery | ABouch | CC0 1.0 (LAION Freesound, train row 193542) | 18.05-18.45, at 0.45 |
|  | https://freesound.org/people/Iamgiorgio/sounds/371830/ | I7_dripping ceiling.mp3 | Iamgiorgio | CC0 1.0 (FSD50K) | 2.28-2.50, at 1.28 |
| `matron_rip_5.ogg` | https://freesound.org/people/ProductionNow/sounds/234237/ | Eating Juicy Meat.wav | ProductionNow | CC0 1.0 (FSD50K) | 5.14-6.44, at 0.00 |
|  | https://freesound.org/people/ABouch/sounds/174943/ | Breaking Frozen Celery | ABouch | CC0 1.0 (LAION Freesound, train row 193542) | 19.12-19.52, at 0.35 |
|  | https://freesound.org/people/spookymodem/sounds/249811/ | Crunching Bones.wav | spookymodem | CC0 1.0 (FSD50K) | 0.75-1.35, at 0.70 |
|  | https://freesound.org/people/Iamgiorgio/sounds/371830/ | I7_dripping ceiling.mp3 | Iamgiorgio | CC0 1.0 (FSD50K) | 3.40-3.63, at 1.27 |

## Mouth takes: croak, cluck, sniff (2026-10-29)

These replace the three formerly code-generated sound families, which have been
removed from `sound_bank.gd`. Each take is a different, nonoverlapping human
performance from its CC0 recording (not a rate-, pitch-, time- or segment-shift
of another take). Each is at most 1.2 s. The first three croaks are from a
woman recording genuine phlegmy throat-clear attempts; her unaltered human
source recordings are the candidate until the owner chooses by ear. The
fourth croak is a separate throat performance from the spec shortlist. The
four clucks are isolated tongue clicks, **never bird recordings**. Three
sniffs are separate nose-inhales from a three-performance recording and the
fourth is independent.

Freesound pages and uploader names below establish source provenance and CC0
1.0 dedications. The actual bytes fetched for preparation were Freesound's
public **HQ Ogg previews** (not the original uncompressed WAV downloads).
Their exact SHA-256 hashes and URL paths are pinned in
`tools/audio/matron_mouth_takes.py`; source changes are rejected. The
delivery files are short mono 22,050 Hz Ogg Vorbis takes like this bank's
existing recordings, decoded and cut at the listed source times, with only
the existing convention's light 75 Hz high-pass, brief boundary fades and
peak-gain alignment. **No voice synthesis, layering, pitch shift or
time-stretch.** The cuts are natural nonoverlapping performance intervals;
do not equate source preview bytes with the encoded delivery file bytes.
The owner still auditions and may replace individual takes.

| File | Source URL | Uploader | License | Exact source interval (seconds) |
|---|---|---|---|---|
| `matron_croak_0.ogg` | https://freesound.org/people/RavenWolfProds/sounds/586979/ | RavenWolfProds | CC0 1.0 | 0.38–1.08 |
| `matron_croak_1.ogg` | https://freesound.org/people/RavenWolfProds/sounds/586979/ | RavenWolfProds | CC0 1.0 | 10.50–11.18 |
| `matron_croak_2.ogg` | https://freesound.org/people/RavenWolfProds/sounds/586979/ | RavenWolfProds | CC0 1.0 | 22.32–23.45 |
| `matron_croak_3.ogg` | https://freesound.org/people/CreepyKing/sounds/842294/ | CreepyKing | CC0 1.0 | 0.16–1.17 |
| `matron_cluck_0.ogg` | https://freesound.org/people/Vein_Adams/sounds/711535/ | Vein_Adams | CC0 1.0 | 0.14–0.39 |
| `matron_cluck_1.ogg` | https://freesound.org/people/Vein_Adams/sounds/711535/ | Vein_Adams | CC0 1.0 | 0.92–1.20 |
| `matron_cluck_2.ogg` | https://freesound.org/people/Vein_Adams/sounds/711535/ | Vein_Adams | CC0 1.0 | 1.73–2.00 |
| `matron_cluck_3.ogg` | https://freesound.org/people/Vein_Adams/sounds/711535/ | Vein_Adams | CC0 1.0 | 4.03–4.36 |
| `matron_sniff_0.ogg` | https://freesound.org/people/padpad/sounds/516473/ | padpad | CC0 1.0 | 0.62–1.33 |
| `matron_sniff_1.ogg` | https://freesound.org/people/padpad/sounds/516473/ | padpad | CC0 1.0 | 2.51–3.35 |
| `matron_sniff_2.ogg` | https://freesound.org/people/padpad/sounds/516473/ | padpad | CC0 1.0 | 4.35–5.20 |
| `matron_sniff_3.ogg` | https://freesound.org/people/OwlStorm/sounds/151218/ | OwlStorm | CC0 1.0 | 0.05–0.59 |

Source shortlist S22 (`florianreichelt`, 447496) and S23
(`CreepyKing`, 842294) were also acquired and checked for natural
performance intervals. S22 has only one isolated throat-clear and is not
part of the chosen female-leaning bank. S23 supplies one additional source
performance as `croak_3`; it is not divided into fake variants.
