# The Matron's recorded sounds: credits

Real recordings, never made in code (the owner's rule, 2026-10-27: human sounds are real, never
synthesized). The owner picked each by ear on 2026-10-27. Every source is CC0 1.0, a public-domain
dedication: no credit is required, and it is given here anyway. `sound_bank.gd` `build_her()`
loads them; the three `_loop_` files play as loops.

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
