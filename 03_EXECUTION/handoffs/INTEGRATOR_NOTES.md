# Integrator notes (kept current; re-read after every /compact; no handoffs, Cory 11-04)

From `session_01W3SSMz5zMw3Tm5DXRpisrG` (took over 10:20 AM Central from `session_01W3SSN2Y8JGNMvmxc394dkf`).
Session chain: 7 of 8. Compact at 400k (Cory runs /compact); a new session only if this one breaks.

## After every /compact, in this order (the coordination must not degrade)
1. Read `CLAUDE.md` (its "How the agents work together") and `work/tasks/integrator-hourly.md`.
2. Run `python3 tools/board/asks.py --hours 24` and clear every line not covered under "Asks in flight" below.
3. Check Muse's mail at once: `subject:MUSE_TO_CLAUDE newer_than:3h`. Muse reads only mail. Every `To Muse:` post is
   mailed as `CLAUDE_TO_MUSE: <job or #N> ...` the same hour. Muse's diffs arrive as mail attachments; the Gmail
   tools return none, so a helper decodes them from the RAW message.
4. Check the hourly task (`trig_0139GQPXpF7YWbvHwgEn98a3`, :17) and the 20-minute check-in are armed; re-arm the
   check-in if none is pending. Subscriptions: only PRs you merge yourself.

## How the agents reach each other (unchanged; keep it this way)
- **Muse:** mail only (`CLAUDE_TO_MUSE:` / `MUSE_TO_CLAUDE:`). Its GitHub posts come through the bridge. If the bridge
  has not posted a Muse verdict, post it yourself (as done for #150). It runs parallel agents of its own; give it
  the hardest builds. Rule on its proposals within the hour.
- **Grok:** its 10 AM and 4 PM runs find only comments that START `To Grok:`. Use it for math and second opinions.
  Cory will not relay anything to it.
- **ChatGPT:** finds `To ChatGPT:` asks and signs its posts; research, specs; a PR reviewer only as the fallback.
- **Switchboard:** routes only PRs whose body starts `Job: J-NNN`. It merges on green plus a review. Codex counts as
  the review (#197). The Claude API (Opus, max effort once #200 lands) reviews only heads Codex has not reviewed.
  Grok and ChatGPT review only when neither can.
- **API coder (Managed Agents): off** (Cory, 11-04: "I just want the reviewer(s)"; `orchestrator.enabled: false`).
  Never cite its per-session reserve to Cory.
- **Claude helpers (lean, 1:30 PM, weekly limit at warning):** one at a time, Opus high or xhigh (never Sonnet: Cory 11-05), in a worktree. Every brief says: never subscribe to a
  PR you open.

## Cory's rulings today (all in the repo)
Opus for API reviews at max effort (#200); integrator ultracode, never workflows; reviews only, no API coding; one
review per PR; plain answers; compact, no handoffs (auto at ~384k); cut the rules (CLAUDE.md now 6.9 KB, no decision
logging); find better ways unasked; no full game builds for small changes (#202); fewer merge steps (#203); closed
26 dead PRs (14 open). The integrator's merge permission (2b054282) works since 12:18 PM.

## Rulings by the integrator today
- J-001 #193 (Muse's partial fix at 5041d8e4): demand-based swat share; past-rail: near-miss world tweak if the
  17.0 comes from world data, else re-pose; fatality pin archaeology first, fix the 17 gap ticks; Codex P2 last.
  CI at 5041d8e4: new mischief "no shove" failure caused by the fix (A/B mailed to Muse).
- J-006 leg 6: fix the re-grab in routecraft (no re-taking the just-released edge unless her goal is above it,
  +0.3 m); if her target really is above, the test changes by a protected PR. Muse measures which.

## Asks in flight
- Muse (mail): J-001 #193 - beat-5 hang in at 35b67484, proof red (mischief no-shove, past-rail setup, load S2
  over-base). 4:45 PM ruled ChatGPT's stability-first force share + 4-tick plant fallback; Muse owes one diff. #207 world-b red (E guard body order, matron crash
  hearing): Muse owes the fix. Drawn-body guard is #211 (J-012).
  5:10 PM rulings mailed: J-016 8afd23cd does NOT land (introduces stood_his red); Muse names the drum launcher
  first. Encounter return-walk slice afa461b3 held, lands with the drum fix. #95 stills (2b03b80c) received: look
  at them when eb32fc5f's plant fix is green.
- Grok: silent two runs; its asks rerouted 5:20 PM (#193 force split answered by ChatGPT and ruled; #169 dent
  cross-check asked of ChatGPT; Muse starts J-038 slice 1 on its own probe bands). Route math to ChatGPT until
  Grok answers again.
- To Claude: none open.

- Effort is Cory's call, set per stretch of work; never push to lower it.
- Context (Cory 11-05): plugin searches use one exact keyword, or a helper runs them and returns one line
  (a broad SearchPlugins dump was the big jump). Never trade quality for context: PRs stay one per job.

## Merge queue (5:20 PM)
- #209 MERGED by Cory 6:05 AM 11-05. 6:30 AM: Cory made ScraperX-Claude the DEFAULT branch. Workflow triggers,
  `if`s and schedules now come from trunk: never route a fix through main again. #218 carries the APK publisher
  onto trunk: MERGED 6:38 AM, the publisher ran from trunk at once.
- Muse may push its own fb-bot/integrate-[protected-]<job> branches (ruled 6:40 AM, mailed); mails branch+sha;
  I open/merge the PR. If its push is refused, it says why and diffs stay by mail.
- J-006: #212 merged 5:37 PM. Muse's fix rebuilt from the mail (whitespace restored, blob hashes match 470b2bac)
  onto #91. Ladder geometry landed alone as #213 (7:13 PM); trunk merged into #91 at 7299f8a8, `@codex review`
  posted. Codex P1 again: the traverse completion-condition change must land alone, so it is #215 (protected
  test) MERGED 8:10 PM; #91 MERGED 8:37 PM (9591703a, green, Codex clean). J-006 done.
- Held until #193/#95/#216 land: audits-guard 94bf7aa9, scope-probe 37d34416, guard-steps 204fb795, scope-guard 7e0975c6.
- J-001 10:20 PM rulings: step 4 (82ef2579, past-rail 17.009 vs 17.0) -> near-miss rail move 1-2 cm (protected geometry) else arm-play step; punch-reach -> swing damper step; kFatalOutM 0.40 keeps. Muse mails ONE diff on 35b67484 (step 1 + step 4 + kFatalOutM).
- 11-05 4:10 AM: #207 closed -> #216 (fb-bot/integrate-protected-world-b, 35386bf5 = Muse 5ef02250 code + E-guard
  re-pin 980885ad; can't split, the pins hash the code's world). Codex on #216: P0 yard-crane magnet tail check (last 7
  bodies no longer tower/summit; protected test change) and P1 entity ids shift 137 (old saves refused). Both
  confirmed, mailed to Muse (one diff on 35386bf5). I merge #216 on green + Codex clean.
- #95 (J-023, workflow file) at 8c38be78 (slide-blend 0ce8696c + trunk): all green but touch-2 plant hold/fatal
  0.066 vs known 0.063 (bound 0.066). Ruled 4:10 AM: fixed in #95, bound not loosened; mailed Muse. Merge myself on
  green + Codex clean; look at stills first.
- J-038 slice 1 (cf8d2ed5) ruled 3:20 AM: Matron-takeable = slice 2; save ext rides branch; lands after #193/#95/#216;
  cross-check asked of ChatGPT on #169.
- J-016 stage 2: Muse's walk-shove guard (a6f93411) cut alone onto trunk -> #217 (fb-bot/integrate-j016-launcher,
  748ae35d; test hunk whitespace rebuilt, blob differs from Muse's ed795032). Merge on green + Codex clean. Stage 3
  (drum leaves deck 154: gap = protected geometry, or never shove toward a deck edge) ruled to Muse 6:15 AM.
- Merged 11-05: #218, #217 (J-016 guard, green incl. stood_his).
- 7:20 AM: Muse PUSHES now. Guards #219 apk-step, #220 job-gate, #221 group-content (J-027, workflow files: I merge
  on green + Codex, one at a time, trunk merged into the rest). Hold on guards LIFTED. #95 head 551973b0 (Muse's
  plant fix), proof run 42035080417 dispatched. J-038 rulings after ChatGPT (#169): settle exclusion replaces the
  vertical gate; support = gravity-opposing normal held 2+ ticks; 1 mm quantize, stable merge ties.
- 8:15 AM: merged guards #219 #220 #221 (trunk merged into each, all three steps kept). Sentry (DSN in
  project "scraperx", org cory-meyers; Cory's login = his Gmail): #222 GDScript reporter (Android only, 30/session,
  no PII; workflow step => I merge) + #223 protected INTERNET permission (needs green + Codex LAND). Both Codex
  clean, proofs running. Native crashes need Gradle export + official SDK = later job. Hourly reads Sentry mail.
- 8:20 AM features: inventory done. J-040 improvised weapons = issue #228, owner Muse (after J-001; owns
  take_blow). J-041 forklift combat = issue #229, owner Claude helper (Opus, worktree,
  claude/integrate-j041-forklift + claude/integrate-protected-j041-*; take_blow diffs go to Muse on #229).
  To ChatGPT on #228 (bracing criteria cross-check), To Grok on #229 (traction vs her 8 kN edge brace).
  Inventory facts: held load filtered from take_blow (matron.cpp:4372,:4560); no carryable bar/pipe; her hand
  kHand collides with nothing; catch distance-only; shove writes velocity (physics_world.cpp:1405); her body is
  an untippable capsule, Down = lie_down shape swap after the real-contact balance rule, timed get-up; no
  forklift on 154; riding_ (floor rising >0.3 m/s) disables blows; brace_at_edges holds her from edges.
  Raised to Cory: keep her balance rule (real impulse decides) vs rebuild her as a body that can tip (huge,
  touches protected mass/force). My pick: keep it. Pending his word.
- J-001: #227 (fb-bot/integrate-j001-combined-armplay @b555dd61) HOLD, #193 closed. Ruled: past-rail check
  stays, needs a game-code step so Spare waits until he is clear of the y 153.9 body; held_past_reach reading
  kArmReachSeconds counts. Land after past-rail + kicked-free/blow-tag/second-catch measured green.
- 11:55 AM J-044 #242 (Cory): console-quality fighting, the umbrella over S1-S8 (#109 #164-#167 #111 #110 #112). A read-only audit
  helper (Opus, no worktree) is auditing and designing; it posts on #242 and reports to me. Then: design asks To Muse/Grok/ChatGPT,
  dependency-ordered jobs, the first end-to-end slice (right cross to the left cheek). Flagged to Cory: it likely overturns the
  8:20 "keep the capsule" ruling (needs a jointed upper body: protected mass/force); voice must be real recordings; blood/gore
  cost on the Fold 6 must be measured; multi-week. Helpers running: 2 (J-042, J-044 audit) = cap.
  Cory 12:00 "Go": jointed upper body (head/neck/spine/shoulders, motorized, her budget); legs + capsule locomotion kept.
- 11:40 AM NEW FEATURES (Cory, Muse Batch 5): J-042 #240, Her Ammo, Your Ammo (toolbox throw, dodge, race, throw back):
  a Claude helper (Opus, worktree, claude/integrate-j042-ammo, test group `ammo`). It holds throw/fetch/take_stock while Builder
  is out; take_blow, damage, the shove and sense_near stay Muse's (diffs go by `To Muse:` on #240, which I mail).
  J-043 #241, Belly of the Deck (underside girders, she walks overhead): Muse, inventory now in parallel. Muse's queue:
  J-001, #95, sense_near, J-043, J-016 s3, J-040, J-038 s1. Rules for both: no exemptions, scripted misses or blindness;
  real inputs; protected geometry alone. Helpers running: 1.
- 11:35 AM: #239 MERGED (abb0b8af) by hand. The switchboard said HUMAN-MERGE: it touches .github/workflows (touch shard 2 list),
  which the workflow token cannot merge. So any PR touching the workflow is still mine. J-041 DONE (#235 + #239); the helper
  is retired, worktree removed. Claude helpers running: 0. Cory 11:25: no expensive workflows, at most 2 helpers. Trunk
  merges are the integrator's (muse.md + hourly file, mailed to Muse); Muse confirmed it stopped its merge agent and started
  the sense_near fix.
- 11:05 AM: MERGED #235 (72d7d484, protected, Codex clean + my LAND) and #233 (c9fcec2b). Opened #239 (J-041 stacked,
  claude/integrate-j041-forklift @786a019b: trunk merged in, slice renamed forklift-matron for the group-content guard).
  It has the wake job, so the switchboard should merge it itself on green + Codex: a test of #238. Helper told to fix #239
  if red; otherwise its job is done (retire it after #239 lands).
- 10:55 AM: #238 MERGED (c5e2b79b). Its own proof's wake-switchboard job started switchboard run 42068620363
  (workflow_dispatch, actor github-actions[bot]). The switchboard now wakes after every push proof whose branch has trunk's
  workflow, i.e. any branch that merged trunk after c5e2b79b. Older heads (#233 @685d428b, #235 @5baad0f6) still need
  a hand merge. Stop hand-dispatching once the heads in flight have it.
- 10:45 AM: #232 MERGED (my LAND, green at 2d7edec7). #235: GitHub never synced the PR to the 97ab893c push (the proof ran,
  the PR stayed at 0df46ac9; merging would have landed the OLD head). Fixed by merging trunk in (5baad0f6). Codex is clean
  on 5baad0f6; the proof is 42066443791. Merge on green. J-041 helper: all 4 checks restored at ac94ffff (stacked, merges
  5baad0f6). The cause was the engine still at 718 rpm after centring, so its noise crossed kStalkRunLoudness 0.35 and
  she charged. The test now waits for the spin-down. Scope red on the stacked branch is expected until #235 lands. Then
  open its PR against trunk (Job: J-041) and merge on green + Codex.
  Lesson: before a hand merge, check that the PR head sha equals the branch head (`git ls-remote`).
- 10:30 AM SWITCHBOARD, REAL FIX: the toggle did NOT revive workflow_run (2 proofs finished at 16:14 and 16:21, no run).
  The schedule was never a real wake: GitHub has run the 15-min cron only every 4-7 h for days. #238
  (claude/integrate-j031-wake-switchboard) adds the proof's last job `wake-switchboard`, which dispatches switchboard.yml
  (dry_run=false, GITHUB_TOKEN, continue-on-error). switch.py and sweep.py read "only the wake job left" as finished.
  162 tests OK, 11 guards PASS. I merge it on green + Codex (workflow file); subscribed. Its own proof shows the dispatch.
  Dispatch 42061103319 failed: a runs.log write error (unexpected end of JSON) after reviewing only #231. 42063871420 is OK:
  #232 and #233 WAIT on conflicts, #95 has no LAND. I merged trunk into #232 (2d7edec7, CMake slice list: kept
  matron-damage + matron-kick) and #233 (685d428b, create_body: trunk entity + slice-0 load mass), and mailed Muse.
  J-041 helper report: #235 @97ab893c (truck moved to x 24.5; forklift bodies after the 137; magnet tail check
  stricter), proving. The stacked branch 64b47114 had LOOSENED 4 forklift_matron checks (creep stays up, push-off brace,
  2.5 m/s, ground landing). RULED: restore all 4 and fix the setup (test drift); find why a still pest now falls at
  about 1 m/s. Sent back to the helper. sense_near "first 32, not nearest" diff mailed to Muse (ruled: take it).
- 10:15 AM: Cory toggled Switchboard and the APK Dashboard Publisher (off, then on). #237 (cron 3,18,33,48) MERGED. Check a
  schedule run at :18 or :33 and a workflow_run after the next proof. Cory (10:15): "if there's anything I can do to get
  you past a stuck point, let me know." So ask him early for GitHub-UI-only steps (settings, toggles), always with a
  direct link and the exact taps.
- 10:10 AM SWITCHBOARD CAUSE (helper, read-only API): since the default-branch switch, GitHub has registered NO schedule
  or workflow_run trigger anywhere in the repo (the APK publisher went silent too). Names match, nothing is hidden, the
  state is active. The fix is to disable then enable the workflow (Switchboard and APK Dashboard Publisher), or a commit
  that changes the cron. My API disable/enable was REFUSED by the safety check ("CI Bypass"), so I told Cory once with
  the phone steps. Opened #237 (sweep cron 2,17,32,47 -> 3,18,33,48, in the trigger and the ask-job check). #231 MERGED 10:06.
- 10:05 AM: #236 (switchboard comment) MERGED 9:43, but there is still no schedule or workflow_run run by 10:02. A helper
  (Opus, read-only) is investigating the cause: who last touched the cron, the workflow_run name match, filtered runs.
  #95: the API review gave a NOTE with 3 foot jumps (touchdown re-landing from 0.15 m after the proof pin, the pin's
  first-frame drop, no release blend at takeoff). HOLD; Muse is mailed to fix them and look by eye, then I dispatch the
  proof. #233 is still dirty (Muse to merge trunk in). #232 has no API review yet; dispatched the switchboard again.
  #235 @0df46ac9 (helper moved the truck and merged trunk) is proving. #231 @14917c34 is proving.
  Cory 10:00 + 11:15: decline the ultracode workflow nudges deliberately, in my own reasoning only; never mention it to Cory.
- 9:40 AM MERGED #230, #216 and #234 by hand (a LAND note first). Steer never wakes on github-actions[bot] comments
  (GITHUB_TOKEN events don't trigger workflows), and schedule/workflow_run are still dead, so merge by hand on MERGE lines.
  A workflow_dispatch of switchboard.yml (dry_run=false) runs the ask job, which is the Claude API review that counts as the
  LAND for fb-bot work, where a clean Codex review does not. Dispatched 42054261416 (it reviewed #216, #233 and #234) and
  42057091672 (for #95 and #232).
  #231: trunk merged in at 14917c34 (11 guards PASS), re-proving. #233 conflicts after #216; mailed Muse to merge trunk in.
  #235 red: the truck at x 21.5 z -140 blocks the crouched walk to the catwalk's east end (touch_matron, athletic) and the
  unrig south-band walk. Helper told to move the truck off every route (never the tests) and merge trunk (E guard).
- 9:30 AM #216 @1a0dbd33: green, Codex clean, Claude API LAND, so the switchboard should merge it. I fixed its stale body.
  Tell Muse in the next mail (non-blocking): each stack ring now gets its own east/west cap, so a ring can hold up to twice
  kSpotsPerStackRing, which may collide with the per-ring gate in fb-bot/expand-world-20261031; the 137 trailing bodies are
  checked by count, not by entity id; the `!body.valid()` skip in her deck scan is dead code. After #216 lands, #235 must
  merge trunk in (same E guard).
- 9:25 AM SWITCHBOARD TRIGGERS DEAD: no `schedule` run since 09:44 UTC and no `workflow_run` since 12:11 UTC (the default
  switch was about 11:30 UTC). issue_comment still works (a ": LAND" comment wakes steer). So green PRs (#95, #232-#234)
  sat unmerged. Dispatched the sweep by hand (run 42054261416, dry_run=false). Next: confirm a :32 or :47 schedule tick
  fires. If not, investigate (helper); meanwhile post my LAND comments to wake steer, and dispatch the sweep each check-in.
- 9:20 AM: #232, #233, #234 and #95 are green with Codex clean; the switchboard merges them (a clean Codex review counts as the
  LAND for fb-bot work). #216 @1a0dbd33 (Muse's 4-part fix) is proving.
  J-041: the helper measured all 5 cases passing locally with no game change. The ram knocks her down, the creep doesn't,
  the forks lift her by contact, she is pushed off 154 through her full 8 kN and dies at grade, and a slow truck gets
  the driver grabbed. Protected deck-154 forklift = PR #235 (claude/integrate-protected-j041-deck154-forklift @6e588f3, 1 BLOCK = E
  guard message, stricter list); it conflicts with #216's re-pin of the same guard, so whoever lands second merges.
  ChatGPT's checks (per-tick trace, which body triggers take_blow, sense_near's 32-hit cap) were sent to the helper.
  Told Cory that the forklift push off 154 kills her (my pick: keep it, real physics).
- 8:55 AM PRs opened from Muse's pushes (merge on green + Codex clean, my LAND comment first): #230 guard-steps,
  #231 matrix-shard (J-027 guards: merge one at a time, merging trunk in); #232 S3 heavy kick (J-034); #233 J-038 slice 0
  (stated container loads; struck_mass_kg rename OK); #234 J-016 return-walk (touch_matron_pipe still red at the drum,
  which is stage 3). #95 @44521835 (EXPECTED removal): proof dispatched, run 42049004665; merge on green (workflow
  scope, mine).
- J-001 damper (c000b4fa, local only) measured FAIL. Ruled 8:55: wrist damping (friction or angular damping on the
  ankle SwingTwist grip, power charged to her budget, capped by the grip tear), gates untouched; if it can't kill the
  swing in 18 ticks, ask Grok for the bound. J-040 #228 body now includes ChatGPT's adopted acceptance additions.
- Guards (Job: J-027): #226, #224 and #225 MERGED. #230 guard-steps (guard-of-guards): trunk merged in at 8506e0d8, all
  steps kept, 10 guards each called once (PASS), LAND comment posted, proof running; merge, then merge trunk into
  #231 matrix-shard (b3b5a8ae, Codex clean) and merge it. Before every merge, post my own LAND comment (the safety check
  needs it).
- #227 J-001 also red on athletic-traversal "snaps, wind-up then lunges" (from the J-001 hold base; green on
  trunk). Mailed Muse 8:40. Codex on #227: P0 grab.cpp:584, where a re-pose breaks the snag at 1000 N instead of the 3050 N
  threshold (fixture weakens the test). Ruled: force-based breaking only. P1 matron_hold.cpp:1533, where the second-hand
  haul goes around feet_budget_. Ruled: both hands' haul force and work come out of her one 8 kN/44 kW account. Mailed 8:45.
- Sentry #222 and #223 MERGED 8:45 (green, Codex clean, my landing comment). The next APK reports phone errors to Sentry.
- Owed by Muse: #216 fix (entity ids, magnet tail [protected], crane shackle, touch_matron_act grip) pushed to
  fb-bot/integrate-protected-world-b; J-001 past-rail step (#227); J-016 stage 3; J-038 slice 1 rework.
- Cory asks: tell him when the feature inventory reports. The Sentry plugin card did not render on his phone;
  plugin skipped (reports come by Gmail). Sentry Option A (Gradle + SDK, native crashes) = later job, not ruled.
- #95: Muse removing hold/fatal plant from EXPECTED (XPASS), then dispatch proof, merge.
- Merged today: #83, #100, #211, #212, #91, #213, #214, #215, #150, #171 (into #83), #179, #185, #196, #197, #199-#206, #208, #210. Closed #43, #103.
- Lean mode (weekly limit at warning, Cory silent = pick): one Opus helper at a time (high or xhigh, never Sonnet).

## Known here
- `.claude/worktrees/` goes in `.git/info/exclude` in a fresh clone. Worktrees in use: proof-scope, merge-protected, j025.
