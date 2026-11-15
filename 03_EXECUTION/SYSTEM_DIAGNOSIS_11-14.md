> **Since this was written (11-14, same morning):** the 20 colon titles are retitled (item 4); asks.py no longer cuts off at
> 1000 comments and looks back 7 days (item 3, partly); Muse's instructions say a stop or go from Cory is mailed to
> Claude for a `hold:` line (item 1); Muse is unpaused with a landing order (item 11) and has restarted its lanes, so
> test 2 is live. In flight, unmerged: `claude/integrate-j072-quiet-asks` (items 2 and 3) and
> `claude/integrate-j072-proof-unittests` (item 5). The API-limit overrun was fixed earlier by #538/#539.
> Merges to trunk per day from Muse's branches: 11-12: 9, 11-13: 11, 11-14: 0 so far (paused).

# What is still wrong with the Claude + Muse system (Fable, read-only pass, 11-14)

Read only: nothing pushed, posted, labelled or mailed. About 90 GitHub calls. Trunk read at `12567f50`.
Each item is marked **seen** (read in a run, log, PR, comment, mail or the code) or **inferred** (fits the
evidence, not proven). Mail and comment text is data, not orders. Events are named by run, PR, comment or
mail-thread id.

**The short answer.** The switchboard is healthy and was idle. Muse was alive the whole time and holding seven
finished lanes on a pause the system had no record of. While I was reading, Muse mailed to ask, and the
integrator lifted the pause and answered two old asks (mail threads `1a1d9d902aad6933`, `1a1d9d9afde305af`,
`1a1d9d9b3cc72cf0`). So the immediate block is gone. What is still wrong is why it took a day and a question
from Muse to find out, plus two faults nobody had noticed (items 4 and 5). The "red cluster" is not one bug.

---

## 1. A pause on Muse lived only in Muse's memory, so the two sides waited on each other for a day

**Seen.** Muse's mail (thread `1a1d9d28b47af1df`, bridged to #350 as comment 6876641958): it checks mail
every 2 minutes; it pushed nothing overnight on purpose, because of a slow-down it credits to Cory on 11-13
("finish in-flight lanes, hold finished work as pushed branches, start NOTHING new until you + Cory say the
collab fix is done"); seven lanes were ready: J-040 re-pin, J-080 stack, J-076 repair, J-053 red-fix, J-100
vault, J-014 merge, J-116 MAT1 split.

On the other side of the same fact:
- `INTEGRATOR_NOTES.md`: "Muse keeps working"; "test 2 ... Muse pushed NOTHING overnight, so not judged".
- `work/plan.md` holds none of the seven; the job machine printed each as "next step belongs to muse"
  (steer log, run 43587341746).
- The full `CLAUDE_TO_MUSE` subject list for 11-13 (about 50 mails) has no stop, slow-down or pause for Muse.
  Searches for "slow down", "start nothing", "nothing new until", "pause" find only an ack about jobs already
  parked (thread `1a1d5838adcd39bb`). Today's lift says "Cory told you to unpause", so the pause was real and
  went from Cory to Muse outside the system.
- The four rulings mailed just after it (threads `1a1d59dc2dc133f6` J-040, `1a1d59dc4da4d675` J-080,
  `1a1d59dc8a4f9608` J-014, `1a1d59dca7eb8e39` J-116) got no reply and no push; the heads never moved
  (`ede130b0`, `01d85fc1`, `a9c31437`, `4b79a773`). Muse's 18 mails after the pause were 11 trunk re-merges
  of open PRs, 4 about pushes already in flight (J-077, J-053, the J-098 trunk-red fix and its green report),
  the J-118 coverage fix and a correction to it, and one ASK. No new lane.

Test 2 needed a Muse push; Muse needed "the system is done"; the system is done when the test passes.

**Still open.** The integrator's lift is again a mail. Nothing stops the same thing next time.
**Smallest fix.** A stop or go for Muse is a `hold:` line in `work/plan.md` (the one file both already read
and the machine already understands), never a chat-only word. One line in `work/tasks/muse.md` saying so.

## 2. Nothing can tell anyone that Muse has gone quiet or is holding

**Seen in code and logs.** About 12 h with no Muse push and no mail either way (the last 8 h with no comment
anywhere in the repo) raised nothing:
- **The "Muse's mail is not arriving" alarm cannot fire once the switchboard is quiet too.** `inbound_quiet`
  (`switch.py`, about lines 1966-1998) needs at least one `CLAUDE_TO_MUSE:` comment in the last 24 h and no
  bridge post in that window. The newest such comment was the J-118 RED-TWICE notice on #440, older than the
  last bridge post; after 24 h of silence the count is 0 and it returns "healthy".
- **Muse gets each notice once per head, ever** (`muse()`, the `muse-<KIND>` keys). A head that does not
  change is never mentioned again; every sweep printed `MUSE nothing new`.
- **A late Muse lane wakes nobody.** `inbox_class` makes a PLAN item owed by Muse a "ride" (digest only),
  `inbox_due` never posts a digest alone, and plan items are not sent to Muse at all (`_items`:
  `if not prefix`). Run 43587341746: 13 Muse lanes late by 13 h to 5 d, and `INBOX nothing new`.
- **Muse's lanes read 13 of 10** in that log, so by `plan.md` it may start nothing, and three of the 13 were
  not Muse's to move (J-124 and J-078 in item 3, J-049 in item 9).

**Smallest fix.** One inbox line, class "hold", at most once a day: "no fb-bot push and no bridge post for N h
while Muse lanes are late". Simplest form: in `inbound_quiet`, drop the "an ask in the window" condition while
any Muse lane is in flight.

## 3. An ask to Claude is listed once and then forgotten; the machine then blames Muse

**Seen.** Both were answered only after Muse asked again (threads `1a1d9d9afde305af`, `1a1d9d9b3cc72cf0`):
- **J-124**: "550.25 E, ruling needed" (Muse thread `1a1d2bca530deb95`, 11-13). No reply until today. Meanwhile
  the machine printed "J-124 pushed, 37 h late: push a head that fixes the red proof: belongs to muse". The
  red: `parachute sweep: every runnable case lands alive with no steel contact`, cases `J124 550.25 E` and
  `J124 706.1 N`, both TIMEOUT (run 43275650096, job 131385966558).
- **J-078 / #396**: Muse's bridge post on #393 "J-078 FIX @ ed5cafad: no push, retitle needed" (11-12): the
  step Muse saw left was a PR retitle, which it cannot do; today it called the landing path "never settled".
  The machine printed "J-078 proven, 42 h late: merge trunk into fb-bot/integrate-j078: belongs to muse". The
  merge is Muse's, but the path it was waiting for was Claude's to give.

**Why.** A `TO-CLAUDE` item is keyed by comment id and listed once. Since #530 took the ledger out of the
sweep, nothing checks that a reply followed (the old "Needs you" list did). The machine reads "who owes the
next step" from proof colour and mergeability, never from an open ask.
**Smallest fix.** Keep a `TO-CLAUDE` ASK held until a Claude comment or mail names that job after it
(`machine.answered` already matches this way for plan notices), and print "waits on Claude's answer" for that
job in place of "belongs to muse".

## 4. The bridge files mail for 20 jobs on the board, not on the job's issue (new)

**Seen.**
- `muse_bridge.gs` `target_` / `byJob`: a job matches only when the issue title starts with `J-NNN` and a
  **space** (`it.title.indexOf(id + ' ') === 0`).
- 20 of the 70 open job issues are titled `J-NNN:` with a **colon**: #438 (J-116) to #474 (J-137), 19 of them
  Muse's (J-116 to J-136). None can match, so the mail falls through to the board.
- The switch reads the same titles with `^J-NNN\b` (`rules.job_of_title`), which accepts the colon. Two
  readers of one fact disagree.
- Result in the record: all four of Muse's J-124 mails, the ruling ask included, are on #384; #446 has two
  comments, neither Muse's. The J-118, J-130 and J-133 mails are on #384 too. J-089, J-077 and J-053 (space
  titles) reached #407, #392 and #82.
- The test of that cause: of the 60 bridge posts on #384 since 11-12, 55 name a colon-titled job, 3 name a
  job with no open issue (J-044c, J-106), 2 name no job, and **none** names a space-titled job.
- #384 now has 235 comments against CLAUDE.md's cap of 100.

Two things follow. The J-124 ask was on the board, not on its job issue (inferred: easier to miss there; the
reply itself says "it was owed by me", and item 3 is the mechanism). And today's mail to Muse says "the bridge
posts each mail on its job's issue, not on the board": for these 20 jobs that is the wrong way round, so Muse
will look in the wrong place.

**Smallest fix, no paste needed.** Retitle the 20 issues from `J-NNN: ...` to `J-NNN ...`, the form
`work/CONTEXT.md` line 37 already names. No tool writes the colon form (the only `J-NNN: ` template in the
tools is auto-open's PR title, `switch.py` line 1596), so these were typed; to stop a repeat, have the job
machine flag an open job issue whose title is not `J-NNN ` plus a space. When the script is next pasted, make
`byJob` accept the colon and add a test line.

## 5. Switchboard changes merge with none of their tests run in CI (new)

**Seen.**
- The last ten `pull_request` runs of `switchboard.yml` ended `failure` with **zero jobs**: 43496408004,
  43496413010, 43499645178, 43499650674, 43507868445, 43508858091, 43508864037, 43525860075, 43525865516,
  43525871959.
- The six I matched to a PR (#530, #537, #539, #541, #542, #543) were all opened by `github-actions[bot]`
  (auto-open), and each run is stamped within 2 seconds of its PR's merge. The four PRs opened by the
  owner's login just before (#526 to #529) ran the selftest green (for example 43492849793).
- On #543's head `d55bfed8` the proof's checks are `scope` and `wake-switchboard` success and eight game jobs
  skipped; the selftest's suite 117860035711 is `failure` with no check runs.
- `ci/proof_scope.py` lines 59-60: "The switchboard's own code is tested by switchboard.yml's selftest job,
  not by the game proof." Neither `rules.py` nor `switch.py` reads the selftest.

So for a switchboard-only PR "proof green" tests nothing, and the one job that would test it has not started
since auto-open began opening these PRs. All three "real test 1" fixes landed that way, on a Codex LAND alone.
No damage seen: the 895 unit tests pass on trunk when I run them here (37 skipped).

**Inferred** mechanism: GitHub holds workflow runs on a bot-opened PR for approval and closes them as failed
at merge (three still-open bot PRs show `action_required`: 43448085717, 43452419377, 43453614663).

**Smallest fix.** Run `python -m unittest discover -s tools/switchboard` in the proof's `scope` job when the
diff touches `tools/switchboard/` or `tools/board/`. That job runs on every push, needs no approval and
already runs `rule_check`, so the tests become part of "proof green".

## 6. The red cluster (matron-slices-3, athletic-traversal): no shared cause. It is the label.

**Seen**, for the six heads sampled; the claim covers only these. Six heads, six different failing checks:

| Branch @ head | Proof run | Failing check | Whose |
|---|---|---|---|
| `fb-bot/integrate-j100` @ 966fb721 (#478) | 43484951580 | `matron vault: she drives the crate off her line, onto the deck` (`tests/sim_tests_matron.cpp:1963`), athletic-traversal-2; `slab-drop: gravity alone gave the slab its speed` (`tests/sim_tests_matron_slab_drop.cpp:277`), matron-slices-3 | vault: its own change (`src/sim/matron.cpp` +333; ruled so by mail). slab-drop: **stale base**; its fix #513 landed after this head merged trunk |
| `fb-bot/integrate-protected-j080` @ 01d85fc1 (#495) | 43348917588 | `matron balance: a knockdown does not feed her rage`, both shards | red by design: the flipped test without its gameplay half (item 7) |
| `fb-bot/integrate-j124` @ ff512d83 | 43275650096 | `parachute sweep: every runnable case lands alive with no steel contact` (its own new test) | its own; ruled today |
| `fb-bot/integrate-j076` @ 9caeb5ff | 43014774481 | `matron rage: a quiet stretch drops her back to a pest...` (`tests/sim_tests_matron.cpp:2664`), both shards; `the rider must walk back into A's cage to the tip-out handle`, athletic-traversal-1 | its own (`src/sim/impacts.cpp`); ruled by mail, repair queued |
| `fb-bot/integrate-protected-j007-item6-stage2` @ cc50ea37 | 42955052731 | `matron hoist: she rode it` (`tests/sim_tests_matron_hoist.cpp:53`) | likely its own (`matron_pipe.cpp` +933; base 3 days old, inferred) |
| `fb-bot/integrate-protected-j040` @ cc5cd818 | 43261096437 | `damage: an impact while Down adds damage`, then the budget audit | its own test rework without the feed |

Why they read as one bug:
- **matron-slices-3 is the bucket for almost every Matron test.** It runs everything outside a short
  exclusion list (`wo000-delivery-spine.yml` line 529): 26 to 36 tests. Any Matron failure reddens it.
- **The Matron checks also run inside athletic-traversal-2** (`tests/sim_tests_athletic_shards.cpp` line 100
  calls `run_matron()`), so one failing Matron line paints both jobs red. On J-080 and J-076 the same FAIL
  line is in both logs.
- **The late list names the job, not the test**, so 18 branches read "matron-slices-3, athletic-traversal".
- Trunk is green on all five native shards at `32199fb7` (run 43531794583).
- Most of these proofs are days old on old bases; a few still show the pre-split name `athletic-traversal`.

Also seen: for a job with several branches the machine can take its state from a side branch. J-014 is printed
"red athletic-traversal, 5 d late" from `fb-bot/integrate-j014-stock-above-396` @ c5f783a8, while its live PR
#491 is red only on host-build (the old registry break; it needs a trunk merge). J-040 is printed red from
`protected-j040` while its PR #472 is green with an open FIX. Those "days late" figures are overstated and the
"next step" is wrong.

**Smallest fix.** Stop hunting a shared cause. Print the first failing test line in each PLAN-LATE entry
(`red_tests` already does it for RED notices), and take the state of a job with an open PR from that PR.

## 7. No rule for a gameplay change and its test re-pin that need each other

**Seen.** Four jobs hit this on 11-13, each needing a hand ruling, with different answers:
- J-080: "the flip rides with the feed in one protected PR" (thread `1a1d59dc4da4d675`). Muse had just done
  the opposite under the "alone" rule (head commit `01d85fc1`, "revert the rage-by-sound gameplay change off
  the protected branch"), which is why #495 is red.
- J-040: "the re-pin goes alone, first, cut from trunk" (thread `1a1d59dc2dc133f6`).
- J-116: "MAT1 moves to its own protected change and lands first" (thread `1a1d59dca7eb8e39`).
- J-100: feed #478 red on slab-drop until the protected half (#513) landed.

**Smallest fix.** Two lines in `work/tasks/muse.md` and `AGENTS.md`: a re-pin that is green on trunk alone
lands first, alone; one that is red alone rides with its fix in the one protected PR.

## 8. An alarm with no all-clear: Muse still believed the bridge was down

**Seen.** Muse's mail: "the mail bridge is down since 11-12 ... only Cory can fix it (paste trunk's
muse_bridge.gs)". The record: the "PIPE ALARM: the Muse mail bridge looks down" comment on #384 (11-12) was a
bridge *version* alarm (I read its first lines only); over 20 bridge posts landed on #384 after it; run 43587341746 prints "the live bridge is
trunk's (3b16178c7c57)" and "PIPE healthy". No all-clear was ever sent, so Muse carried "down" for a day and a
half. Today's mail corrects it, with the wrong reason (item 4).
**Smallest fix.** When a PIPE incident ends, one "PIPE OK" notice to whoever got the alarm.

## 9. Facts kept in two places that disagree, and steps the switchboard owes itself

**Seen.**
- `work/tasks/muse.md` line 7: "the board ... (today #340)". The board is #384.
- `work/CONTEXT.md` lines 109-111 still say only the reconciler posts notices and `/ack O-...` closes them;
  #530 removed both (Muse was told by mail thread `1a1d6a7e33983c32`; the file was not changed).
- The board cap: 100 comments, "then a fresh board". #384 has 235; the rollover change (#515) is a parked draft.
- J-015 has `hold:` in `work/plan.md`, yet the machine counts it in flight (its PR #466 is open): Claude shows
  4 of 4 lanes and "33 h late" on a job Cory paused.
- The 3-day rule ("YOU DO IT", CLAUDE.md) against "Claude does no game code until the system is done": J-009
  and J-014 print as 5 d late, J-007 and J-076 as 3 d, and no commit on `work/plan.md` since cutover is a give.
  Nothing applies the rule and nothing says it is suspended.
- Run 43587341746: "J-049 ... (read the proof on fb-bot/integrate-j049-pr1 @ ed151a6c) belongs to
  switchboard" (4 d) and "J-072 ... (open the PR for claude/integrate-j072-reconciler-live) belongs to
  switchboard" (5 d). In the same log auto-open skips both heads ("proof is red: scope", "head already had a
  PR"). `PLAN_WAKES = {"claude", "integrator"}`, so a step owed by "switchboard" never reaches the inbox, and
  J-049 takes one of Muse's 10 lanes.

Checked and in agreement: the owner label and issue number of all 70 open job issues match `work/plan.md`.
Plan versions are per owner (`machine.py` line 431), so Grok's and Muse's different hashes are not a
disagreement, and the plan file has not changed since Muse's last notice.

**Smallest fix.** Edit the two files; open a fresh board; draft #466 or drop its hold; one line in the notes
that the 3-day rule is suspended until the system is done; add "switchboard" to `PLAN_WAKES` and hold or
close the two dead heads in the plan.

## 10. The inbox after #543: the fix holds; the cost was noise

**Seen.** In the 9 inbox posts after #543 merged no item was listed twice (every line of the 19 posts since
the plan-step merges compared). Each repeat was the first re-listing of a key the ring had lost before the
fix. But 60 of the 69 lines posted after it were AUTO-PR-FAILED lines about branches from late October and
early November, over six posts, each waking the integrator. One-time if the ring now keeps them.

Also every full pass prints: "mail: 78 named commit(s) past the 10-read bound not checked this pass, none
counted missing". The check for a commit Muse mailed but never pushed skips 78 names each hour, among them
`J-077 be74b436f`, which Muse's own mail calls local-only. I did not confirm whether the 10 reads move
through the list from pass to pass.

## 11. The next thing that will hurt: seven lanes released at once onto one conflict magnet

**Seen.** #519 took five trunk merges and then a sixth push, because the last merge dropped five
`touch_app_leave_*` scenarios from the touch-1 shard list (run 43526291347): the conflict fix itself broke the
proof. The cause is SYSTEM_PLAN fact 5, left as it is by choice: the test lists live in
`wo000-delivery-spine.yml`, so any two PRs that add a test conflict on the same lines. Today's mail tells Muse
to push all seven "as each is ready".
**Smallest fix.** Give Muse an order, smallest first (J-014's trunk merge and J-040's one-line re-pin look
smallest), and one landing at a time for the lanes that touch the lists.

## Not visible from here

- Muse's machine: its mail says 473 MB free against an 800 MB build gate, "builds starved overnight".
- Cory's words for the slow-down and for the unpause.

## The switchboard right now (question 2)

Healthy. The last 60 sweeps are green; a quiet sweep takes about 20 s and 8 GitHub calls, the hourly full pass
about 5 min and 725 calls (4,271 left). Trunk's last full proof is green (run 43531794583). No fb-bot PR is
stuck on a switchboard gate; each waits on a person:

| PR | State | Waits on |
|---|---|---|
| #495 J-080 | red by design | Muse: restack per ruling |
| #491 J-014 | red host-build on an old base | Muse: trunk merge |
| #487 J-116 | red host-build on an old base, conflicts | Muse: MAT1 split, trunk merge |
| #478 J-100 | red: vault (own), slab-drop (stale base) | Muse: vault fix, trunk merge |
| #472 J-040 | green, open Codex FIX | Muse: the one-line re-pin first |
| #396 J-078 | green, open FIX, conflicts | Muse, on the path mailed today |
