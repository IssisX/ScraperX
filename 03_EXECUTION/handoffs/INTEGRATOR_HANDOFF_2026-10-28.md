# Integrator handoff, 2026-10-28 afternoon

The Claude integration session of 2026-10-27/28 hands over to a fresh session here,
because its context had grown so large that every step cost the owner too much usage.
Read this, `CLAUDE.md` and the dated rows at the top of `00_START_HERE.md` (they hold
every owner call of today), then the latest comments on issue #3. Times are Central.

## Who you are

You are the master integrator (owner, 2026-10-28: "PE has taken that spot of master
integrator, but it should be given back to Claude"). Trunk is `ScraperX-Claude`, never
`main`. You merge what is green on steps 1-19 of `wo000-delivery-spine.yml`, has a
second agent's LAND and no open blocker, protected PRs included (owner, 1:57 PM: "If
something is cool and all ready to merge, then automatically do it"). Land by merge in
a detached worktree, never force-push.

## Usage: the hard lesson of today

The owner hit his usage limit twice today, the second time at about 50% within an hour.
The cause: five helper agents at once, each with a large context, plus a very long
integrator context, plus review rounds that found more to fix. From now on:
- At most two helper agents at a time, and only on what the owner asked for.
- Briefs say "test only what the change reaches" and "report in at most N lines".
- Don't answer notifications that are echoes of your own comments.
- Check issue #3 once an hour; post only when something lands or an agent needs you.
- Prefer one lean second-agent review per change. Fix its findings once, re-probe, land.

## CI facts

- Pushing to `<agent>/integrate-*` runs the proof. Off trunk, step 20 ("Preserve Android
  updates") always fails with the `sign-apks.py:27` KeyError. Steps 1-19 green means green.
- A head commit containing `[skip ci]` is skipped on push. Dispatch the workflow on the
  integrate branch instead.
- Trunk runs publish `ScraperX-build-<sha>` (it contains the APK) and `ScraperX-APK-<sha>`.
- `touch_matron_pipe` fails in the cloud container even on trunk; CI passes it.
- In agent worktrees, use the main checkout's `tools/godot/godot` with `--path <worktree>/godot`.

## Trunk now

`ccfad3e4` (docs) on top of `9899ca3e`, which landed **L1, her movement continuous**:
- springs on every bone with a final ceiling;
- pole IK;
- her root following the sim;
- a planted crawl with real CC0 hand-plant cues;
- her hand on the held ankle;
- the twist-look instead of a spin in place, with her face holding its heading.

Also landed today:
- #18, one force budget (by the other Claude session);
- #22, the Photo ID and story radio files, inert until hooked up.

The next APK is trunk's own run for `9899ca3e`. The owner's phone build is still the
9:05 AM one (`d46af3ec`).

## Open work, in landing order

1. **PR #20, her fatality** (`claude/matron-fatality-hang-20261028`).
   - She hangs him by the ankle over a lethal drop. He gets a 2.5 s window to punch free. Then she lets go: a real fall, death, and a restore from the checkpoint.
   - Proof green at `4ad67473`.
   - In progress at handoff: a trunk merge of `9899ca3e`, and holding him within his 0.75 m punch reach during the window, then extending over the drop to let go. Fingers-to-let-go must stay 5.0-7.0 s at every catch spot.
   - Push the agent's head, prove it, get a lean review, land.
2. **PR #23, the punch** (`claude/matron-punch-20261028`; C by ScraperX Builder, finished by the integrator).
   - Lands after #20: on its own, its BREAK FREE frees nothing.
   - In progress at handoff: the owner's dedicated, always-visible PUNCH button ("having an appearing and disappearing punch button ... is going to be frustrating"):
     - Action goes back to the environment;
     - a whiff when nothing is in reach;
     - the same button breaks free when held;
     - greyed, never hidden, in the air or mid-lunge, so it never eats a lunge catch.
   - Then push, prove, a lean review, land.
3. **PR #21, her locker on deck 1** (Grok).
   - Re-merged onto `9899ca3e` as `5837c02f` on `claude/integrate-locker-on-deck-1-20261028`, proving.
   - Land on green. The owner said "fine" at 11:27 AM.
4. **S6, her contacts true to the physics** (`claude/matron-contact-truth-20261028` @ `d9c04100`, pushed and proving).
   - The view plants a limb only on a contact the sim owns. False contacts went from 29 / 99 frames to 0.
   - It has new checks, so it lands like a protected PR.
   - Not yet covered: the owner saw her "climbing in the air" on the big corner pipe (the standpipe, `on_pipe`). Add that pipe climb to S6 before landing: hands, feet and body on the pipe surface.

## Restart these, one or two at a time, after #20 and #23 land

Each was stopped before it changed a file. Its brief is below in short.

- **Third-person hold and fatality camera** (owner: "needs to be a third-person perspective when these arm rips and fatalities are taking place ... first person, which is wrong").
  - From her grab to his release or death, a third-person camera shows both of them, kept clear of steel.
  - His whole body is drawn from the native transform, inverted as he hangs (issue #3 comment 6474134525). Her drawn hand stays within 0.15 m of his drawn ankle.
  - PUNCH is visible.
  - The fatality: about 0.5 s of slow motion at the let-go, a pull-out, the fade done 1.0 s after the let-go, the sim stepped fast while black, then a fade in at the restore.
  - Prove it with render stills (xvfb, `gl_compatibility`, `--write-movie`, under `nice`).
- **L6, her sounds** (owner: "not repeat the same sound 3 times in a row when [she's] got [the] player").
  - Per `03_EXECUTION/MATRON_REDESIGN/HER_SOUND_VARIATION_SPEC.md`: shuffle bags plus a cross-family recent ring, so no clip of hers plays twice running in a hold.
  - Cues fire on events (grab, lift, tear, laugh, hang, let-go); no looped human sounds.
  - Families with under 3 clips get real CC0 recordings from the spec's shortlist, credited in `CREDITS.md`. Never synthesized.
- **Fast playtests** (owner: "Yes play test locally brilliant idea").
  - Install Android NDK `29.0.14206865` once and build `libscraperx_native.so` for arm64 the way CI does.
  - Put it in `godot/addons/scraperx_native/bin/android/arm64/` and zip the tree.
  - The owner opens it in the Godot 4.7 editor on his phone and plays within minutes, instead of waiting about 2 h for an APK. CI's Android library links only libc, libm and libdl.
  - Send a zip after each change he's waiting for.
- **PR #20 follow-up** (owner: the rip and fatality are "so long"; and his call "back off"):
  - the arm rip at 4-6 s from her fingers closing to the tear;
  - in the 30 s block after a tear or a blow-free she leaves him, with no shoves;
  - no set-down push off a deck's outer edge.
- **The ladder pull** (owner: "pull"). She pulls him off a ladder by the ankle when it is in her reach. It must be physical: her finite pull beats his grip's real strength, because her proximity never changes his grip rules.
- **Her sight** (issue #3 comment 6474372474). It is 360° today, from low rays and with no field of view. Give her a sensory frame from her head and eyes, with a field of view, and label hearing as hearing.
- **Her encounter, intimate and interactive** (owner: "She's still just this dumb entity with a clear one-track mind ... non-intimate non-interactive separate entity").
  - The integrator's pick, put to the owner and not yet answered: first she stalks him. She shadows him from nearby, freezes when he looks at her and creeps closer when he looks away. Then she waits at the top of the ladder he climbs.
  - FB Bot (Muse) owns the whole-Matron work by the owner's order, and hadn't posted by 2:20 PM. If it's still silent, run this yourself.

## Who holds what

- **FB Bot (Muse):** the whole Matron. Her hair, which passes through her body: the avoidance is two spheres, point-only (#3 6473523891 and 6473886666). The hands on the prop (6474175059). The collision capsule against the drawn body (6474339556). The held-load moment (6474489256).
- **Grok:**
  - #15 (walk from scan): its proof failed in the native suite on `35d23be3`.
  - The through-steel grip: `grok/hand-cannot-grip-through-steel-20261028`; a PR with a thin-solid test is owed.
  - The locker, PR #21.
- **PE:** #14 (the chute test). Merge trunk and re-prove; clear it on green.
- **ChatGPT:** does source-backed review only, and is the PR code-review bot. Don't give it coding.
- **The tools** (`claude/matron-film-tools-20261028` @ `736ee061`): `tools/matron_probe` records her final drawn body per frame and scores hair penetration. The footage tool is not built.

## The owner's standing rules (CLAUDE.md has the rest)

- Every message to him ends with what he does next. Decisions come as two or three short lines with your pick.
- A question he hasn't answered after 10 minutes is decided by your pick.
- He is on a phone, so give him short messages and no tables of numbers.
- Human sounds are real recordings, never synthesized in code.
- Test only what a change reaches; CI runs the rest.
