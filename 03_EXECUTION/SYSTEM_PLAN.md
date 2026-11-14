# ScraperX collaboration system: the plan (design pass, 11-13)

Read and design only. Nothing was built, pushed, posted or mailed. Every number below was measured in this
pass from GitHub (PRs, runs, jobs, job logs, comments), the spend log on `switchboard/ledger`, and Gmail subject
counts. Where the brief's number and mine differ, I say so. Events are named by PR or run number.

Security follows Cory's direction as the integrator relayed it in this session (I did not hear it first-hand):
outside input is untrusted, no secrets in code, logs or chat, bound anything an attacker could make huge; any
gate beyond that must stop a likely, concrete threat or it goes. Section A lists what is kept and cut.

---

## 0. Read this first: five live facts

1. **The paid reviewer is out of money until 12-01.** The spend log counts $98.24 of the $100 month (UTC month,
   `budget.py`). Run 43476499167's ask job logged "REFUSED ... month cap" for #503 and #488. Until 12-01: no paid
   review, no security pass, and about ten more eye checks before those are refused too (the count includes one
   reserve that never closed, so the room may be a little more). 20 open job issues carry `needs:eyes`. Money is
   Cory's call; this plan makes landing not depend on it (items 1, 7, 8).
2. **#517 and #518 are open and not drafts.** The sweep in run 43476499167 auto-opened them from
   `claude/integrate-candidate-scope` and `claude/integrate-failure-modes`. They will be reviewed by Codex and
   merged by the switchboard unless they are made drafts now (still open and not drafts at my last read). Neither
   is on this plan. #500's "recent heads only" filter went out with the #502 revert, so trunk opens a PR for any
   finished `claude/integrate-*` head. A parked branch stays parked only as a draft PR or under a `hold:` on
   its plan line.
3. **Six finished Muse branches have no PR**, all green: `fb-bot/integrate-j021`, `-j089`, `-j094`, `-j098`,
   `fb-bot/integrate-protected-j105` (36 to 49 h) and `fb-bot/integrate-j129` (16 h). Two causes, both silent.
   The first five: the head commit is a trunk merge with no `Job:` line, and auto-open skips it without a word.
   j129: it has one commit after its merged PR #483, and auto-open never opens a second PR for a branch that
   ever had one, though the rule is one branch per job, reused. Open the six by hand today (body starts
   `Job: J-NNN`); item 2 stops both.
4. **Why #480 #481 #484 #485 sat about 20 h.** After the integrator's "@codex review" by hand, #480 #481 and
   #485 merged; #484 was still open at my last read. Not because steer was slow. The ask job ran the security pass at once; its LAND made
   `needs_review` false; `steer_pr` only asks Codex under `needs_review`; `workflow_merge` then demanded a Codex
   review nobody had asked for. A loop with no exit. Item 1 removes the gate that makes the loop.
5. **Why game PRs touch the workflow file at all.** The proof's test lists live inside
   `wo000-delivery-spine.yml`, and its own guards (`.github/scripts/check_*_coverage.py`) fail any test that is
   not listed there. So every PR that adds a touch scenario, a Godot test or a host check must edit the workflow:
   26 of Muse's 85 PRs since 11-06 touch `.github/`, 15 of them the proof workflow itself (2 comment-only,
   3 a name added to a list, 10 a new test step). That is normal work, not a security event. I do not move the
   lists out of the workflow (it would rewrite the protected proof and its 12 guards); I stop treating the edit
   as special (item 1).

---

## 1. The flow map

### Job to landed

| # | Hand-off | Moved by | Typical | Worst seen | Cost |
|---|---|---|---|---|---|
| J1 | Plan line to owner knowing | plan notice by mail, bridge every 5 min | Muse posts back in 9 min (median) | p90 107 min | 1 mail each way |
| J2 | Push to proof finished | `wo000-delivery-spine.yml` | full proof 27.4 min wall; a job waits 0.07 min for a runner | 60.9 min wall; runner wait p90 11.4, max 31 min | 98 runner-min, 13 jobs; GitHub's 20-job cap fits two full proofs |
| J3 | Green proof to PR opened | sweep, `auto_open_prs` | 42 min since 11-10; **171 min on 11-12 and 11-13** | 651 min; six never (fact 3) | lists all 647 branches each full pass (81 s) |
| J4 | PR opened to review | switch posts "@codex review"; Codex | Codex answers in 0.3 min (53 of 56) | never asked on a workflow PR that already has the paid LAND (fact 4) | free |
| J4b | Paid review (when it runs) | ask job, one PR after another | 1 to 5 min each | 18 min for one security pass (#507) | $0.69 median, $2.41 max |
| J5 | LAND to merged | steer, next sweep | sweep every 10 min; run 3.8 min (11-12), 7.6 min (11-13) | run 38.7 min; steer waited 27 min behind ask (run 43459528814) | 250 to 480 GitHub calls fast, 1,000 to 1,200 full, of 5,000 an hour |
| | **PR opened to merged, whole** | | **Muse 17 min, Claude game 39 min** | p75 about 100 min | |
| J6 | Merge to "trunk is red, X fix it" | nobody | never | **16 h** (#483's merge to #499's) | 98 runner-min per merge, read by no one |
| J7 | Red or FIX to owner told to new head | `switch.muse` mail | Muse posts back in 9 min, p75 19 | p90 107 min; 7 no reply | 44 to 72 machine mails a day; failing tests never named (item 4) |

### Ask to answer

| # | Ask | Typical | Worst | Count |
|---|---|---|---|---|
| A1 | Claude to Muse (mail) | 18 min, p75 84 | p90 173 min, max 25 h | 106 to 126 a day; 51 of 288 got no reply on the thread |
| A2 | Muse to Claude (mail, bridge, #282 inbox, integrator) | next integrator post on that thread: 8.6 h. A ceiling: many need no answer. The ASK tag (new 11-13) makes it measurable | 31 h; 18 of 90 none | about 110 inbox items and 60 inbox posts a day |
| A3 | To Codex | 0.3 min | 3 of 56 none | free |
| A4 | To Grok | 1 of 25 answered in 3 days (9.9 h) | 24 none | |
| A5 | To ChatGPT | 0 of 3 | | |
| A6 | "To Cory: Needs you" batches (reconciler) | 15 in 3 days; the last says "32 asks ... answered by mail?" | | noise to Cory |

The happy path is already fast: a clean Muse PR merges 17 min after it opens, Codex answers in seconds, Muse
answers a machine notice in 9 min. The losses are at J3, J6, the workflow gate inside J4, and the sweep's own
length.

---

## 2. Where time and money go, ranked

### Time (work blocked)

1. **Red trunk with no owner: 16 h.** Trunk's full proof first failed on #483's merge (run 43317212470,
   touch-1), then on every merge from #482 on (host-build, the duplicate registry key 1100, failing in 2 to
   3 min) until #499 (run 43440505183). Seven more merges landed on the red trunk (#486 #489 #490 #493 #496
   #497 #498). Every branch that merged
   trunk in went red (#487 #491 #492) and Muse spent mails on "RED-STUCK ... registry break". Muse landed 11 PRs
   on 11-12 and 2 on 11-13.
2. **The workflow gate.** Seven PRs open behind it (#480 #481 #484 #485 #488 #494 #396), four fully green for
   about 20 h. With the money gone, every new test-adding PR would join them until 12-01.
3. **Green proof to PR opened.** 27 PRs waited a median 171 min on 11-12 and 11-13, about 77 PR-hours. Three
   causes: the silent skip (fact 3); at most 3 PRs opened per sweep, in name order (#478 to #495 came out in
   six batches of three or fewer over 3.5 h); and, before #476 landed, a branch listing cut at 500 of 647.
4. **The proof.** 27.4 min wall, about four full proofs per landed PR (97 full proofs for 24 landed on 11-12).
   31% of jobs waited over 2 min for a runner; the 20-job cap was hit for 2 to 4 h on each busy day. Longest job:
   `godot-touch (touch-3)` 18.5 min, against touch-2's 11.9.
5. **The sweep itself.**
   - Reviews run before merges in one run: steer waited 27 min (run 43459528814) and 21 min (43476499167, one
     max-effort security pass of #507, a parked PR).
   - A full pass of steer takes 10 to 11 min, against a 15 min kill: plan 116 s, failed log reads 150 s, PR
     loop 95 s, auto-open 81 s, reconciler 64 s, ledger 61 s (run 43379445319).
   - A fast pass takes 192 to 325 s: failed log reads 110 to 130 s, fresh re-reads of the five unmergeable PRs
     50 to 60 s, the plan table 113 s when the inbox runs.
   - Four jobs in a row, each waiting for a runner: cache-in 347 to 539 s, ask 720 s, cache-out 270 to 398 s in
     the slow runs. 63 to 67 queued sweeps were replaced each day.
6. **The integrator's answers to Muse** (A2). Not a machine problem: the session spent 11-13 on the machine.

### Money

- **Paid API: $96.93 spent 11-03 to 11-13** ($8.80 a day): 116 reviews $87.25, 73 eyes $8.44, orchestrator $1.25.
- Since 11-06 ($72.49, 96 reviews), by what was reviewed:
  - PRs that touch a workflow file: **$45.45, 63%**. Claude's, mostly the switchboard's own: $28.69 (#360 $8.20,
    #507 $6.04, #503 $2.64). Muse's game PRs: $15.26 for edits like one test name.
  - Muse's other game PRs: $19.77, 27%.
- 1.57 heads reviewed per PR. On 11-13: $12.88, of it $8.68 on three passes each of two parked system PRs and
  $2.60 re-reviewing four PRs after a trunk merge that changed nothing of theirs.
- The paid review also runs before Codex was ever asked: #512 #513 #514 were paid for, then Codex reviewed #513
  free 28 min later.
- **Talk, in counts** (I do not invent dollars for session quota):
  - 171 machine mails to Muse in 3 days: 84 RED, 47 FIX, 26 "no ack yet" nags, 11 conflict. 29 were RED-BRANCH
    notices about 21 branches, several long dead.
  - 40% of Muse's 279 posts were "started" (58) or "/ack" receipts (53).
  - 117 hand-written integrator mails in 3 days.
  - Three trackers watch the same jobs: the ledger (163 to 172 items, 102 to 129 "ESCALATED", read by no one),
    the reconciler (1 notice on 11-13, since the plan went live), and the plan machine.

### What the two reverted PRs did

- #505's 90-call cap starved the sweep with 4,894 calls left (run 43465457221: 8 PRs, auto-open, inbox and Muse
  mail all deferred). Its candidate proofs ran a full proof per merge on runners already at the cap (I count 9
  `switchboard/candidate-*` runs in the list; the brief says 19).
- #502's stale-base gate made every PR re-merge trunk and re-prove by mail.
- #502's red-trunk alarm was harmless and wanted. Item 3 brings back that piece alone.

---

## 3. The DELETE list

| # | Remove | Why (measured) | Files | How |
|---|---|---|---|---|
| D1 | The workflow-PR gate: the signed max-effort security pass, the Codex-login-only LAND, the whole-sha rule, review signing | 63% of review money, 18 to 27 min merge blocks, the fact-4 loop, 7 PRs stuck. The threat is an insider forging its way past the switchboard; trunk is unprotected and the same login pushes to it directly, so the gate guards a side door | `rules.py` `workflow_merge`, `needs_security_pass`, `api_review_on`, `codex_review_on`, `names_whole_head`; `switch.py` `review_mac`, `review_sig`, `signed_for`, `signed_verdict`, `signed_mark`, the `security` path of `ask_pr` and `review`; `claude_calls.SECURITY_REVIEW_SYSTEM`; `work/budget.yml` `security_review`; most of `test_workflow_merge.py` | Item 1 switches it off in one small commit; the dead code goes in a cleanup PR after 3 clean days |
| D2 | The ledger and the reconciler | 125 s and about 120 calls per full pass; no reader; the reconciler's only live output is the `/ack` protocol and "To Cory" noise. The plan machine (live since #476) is the one tracker | Now: `switch.py` `run_sweep` parts `ledger`, `ledger clock`, `the reconciler`, `FULL_PARTS`; `work/switchboard.yml` lines `reconciler:` and `notices:`. Later: `ledger.py`, `reconciler.py`, `replay_reconciler.py`, `test_ledger.py`, `test_reconciler.py`, their fixtures (7,600 lines) | Item 5. Both leave in ONE commit: with the reconciler off alone, `ledger_notifies` turns the ledger's own notices back on |
| D3 | Reviews inside the sweep run | steer waits behind ask | `switchboard.yml` | Item 6 |
| D4 | The paid review ahead of Codex | paid for heads Codex reviews free in 0.3 min | `switch.py` `codex_pending` | Item 7 |
| D5 | The cache-in and cache-out jobs | two of the sweep's four runner waits; they guard against a poisoned cache that only trunk's own runs can write | `switchboard.yml` | Item 11 |
| D6 | "started" mails and `/ack O-...` receipts | 111 of Muse's 279 posts | `work/tasks/muse.md`; D2 ends the acks | Item 12 |
| D7 | The orchestrator (hosted agent, off since 11-04) | dead code that still reads its issues every ask run | `switch.py` `orchestrate` through `finish_session` (about 265 lines), `provision_agent.py`, `budget.yml` `orchestrator:`, the workflow's `provision` job and input, their tests | One cleanup PR after the stop line (code first; the workflow job with item 11) |
| D8 | The selftest's live dry run | 550 to 800 calls per push of a ready switch PR, twice; it starved the live sweep on #476 | `switchboard.yml` selftest step | With item 11's PR being opened anyway, as its own commit and PR after it |
| D9 | Asks to Grok and ChatGPT as a tracked, nagged channel | 1 of 25 and 0 of 3 answered | none: D2 ends the nags | Hard questions go to Muse by mail (9 to 18 min) |
| D10 | Parked machinery that never lands | see section B | #507, #506, #517, most of #515 and #503 | Close them |

---

## 4. The improvements, ranked by gain per risk

Each is one behavior, one branch, one PR, landed alone and judged by Muse's landings the next day. Landings
drop: undo it, do not patch. All of 1 to 9 are `switch.py`, `rules.py` or test-infra changes: they land on
Codex's free review, with no paid pass.

### 1. A PR that changes a workflow file lands like any other PR

- **Change:** `merge_decision` stops sending a workflow-touching PR through extra conditions. Green proof, a LAND
  from another vendor, no open FIX, rule check clean: MERGE, done with the merge token (GitHub refuses the
  workflow token). MERGE-BY-HAND stays only when no merge token is set. The ask part runs no security pass.
- **Touch:** `rules.py` `merge_decision` (its `if pr.touches_workflows` tail), `workflow_merge` (cut down to the
  token check), `needs_security_pass` (gone); `switch.py` `ask_pr` (drop the `or rules.needs_security_pass`
  branch and `security=`); one paragraph of `SECURITY.md`. The plan machine's step "post `@codex review` ...
  belongs to integrator" (`machine.py`, the `("*", "codex")` rule) needs no edit: it fired because of this gate,
  and stays only for a refused team token. `CLAUDE.md`, "Reviews, one per PR": its last sentence (workflow-file
  changes merge on a clean Codex review and a clean Claude API review on the full head sha) is the integrator's
  edit, on Cory's word, in the same hour, so the rules and the code agree. `AGENTS.md` and `00_START_HERE.md` do
  not carry the rule (grepped).
- **Remove:** the gate's conditions now; its dead code in the D1 cleanup.
- **Tests:** new in `test_workflow_merge.py`: workflow PR + Codex LAND + token merges with the token; + a typed
  ChatGPT LAND merges; no token is MERGE-BY-HAND; an open FIX is FIX; `ask_pr` on it makes no paid call. The
  tests that pin the old gate are deleted with it (the switch's own tests; `rule_check` guards only `tests/`,
  `*/tests/` and the ui_test files, so it does not BLOCK). Keep passing: `test_switchboard.py`,
  `test_review_fallback.py`, `test_cache_trust.py`, `test_machine.py`, `test_work_reader.py`.
- **Number:** green PRs stuck behind the gate: 7 now, oldest about 20 h, to 0 one sweep after it lands.
  Workflow-touching PR opened-to-merged median equals the others' (17 min). MERGE-BY-HAND inbox items: 7 a day to
  0. Security-pass spend: $45 in 8 days to $0.
- **Risk:** a harmful workflow edit lands on one review. We would see it in the merged note ("with the merge
  token") and in item 3's alarm. Codex still reviews first and reads `SECURITY.md`.
- **Undo:** revert the one commit.
- **Held in:** nothing. It replaces the "who wakes Codex" problem: no special Codex step is left to own.

### 2. A green integrate branch gets its PR whatever its head commit says

- **Change:** `auto_open_prs` opens a PR for every green integrate head that never had one. Two fixes to how it
  decides that:
  - The job comes from the head commit's `Job:` line, else from the branch name (`plan.job_of_branch`, already
    there). Title: the commit subject, or "J-NNN: <the plan line's title>" when the subject starts "Merge".
  - "Had a PR" is asked of the head, not the branch: a branch whose earlier PR merged or closed gets a new PR
    once its head has moved past that PR's head.
  - Bound: only when the plan has a line for the job, the line has no `hold:`, and the head is not one the plan
    lists as `kept`. Anything else green and unopened gives one AUTO-PR-FAILED inbox item per head.
  - First pass, counted today: it opens the six of fact 3. It leaves `claude/integrate-j084`, `-j090`, `-j096`
    (held by Cory in the plan) and `fb-bot/integrate-j084` (kept).
- **Touch:** `switch.py` `auto_open_prs`, `prior_pr` (keyed by branch and head; its saved "yes" list too),
  `open_pr` (a title argument).
- **Remove:** the silent `if not job: continue`; the branch-level "had a PR".
- **Tests:** new in `test_auto_pr.py`: a merge-commit head on `fb-bot/integrate-j098` opens "Job: J-098"; a
  lettered and a protected branch name; a branch one commit past its merged PR opens a new PR, the same head
  does not; a held job and a kept head open nothing; a branch with no plan job gives the item once per head.
  The trunk assertions that pin the silent skip and the branch-level rule are replaced by these. Keep the rest
  of `test_auto_pr.py`.
- **Number:** green integrate branches older than 1 h with no PR, for a job the plan does not hold: 6 to 0. Green proof to PR opened: 171 min
  median to 15 min or less.
- **Risk:** it opens a PR Muse did not mean to land. A green `integrate-*` head is Muse's own "ready" signal
  (`muse.md`). Seen as PRs closed unmerged.
- **Undo:** revert.
- **Held in:** #515 holds only the AUTO-PR-FAILED half; lift that test, nothing else.

### 3. A red trunk gets an alarm and an owner within one sweep

- **Change:** the sweep reads trunk's newest full proof (a run that skipped the build, such as a notes push,
  never counts). Red: one RED-TRUNK item, first in the #282 inbox, waking the integrator, once per red streak,
  naming the merge that first went red and its PR and owner; a mail to Muse when the PR was hers. It clears on
  the next green full proof. The item says: the owner fixes; no green fix head in 60 min, a helper reverts that
  merge (`git revert -m 1`, its own PR). **No merge freeze:** the fix PR lands by the normal rule.
- **Touch:** `switch.py`: `built_run`, `trunk_proof`, `trunk_items`, `BUILD_JOBS`, `TRUNK_RUN_READS`,
  `MERGE_FROM`, "RED-TRUNK" in `INBOX_ORDER` and `inbox_class`.
- **Remove:** its line about the ledger's TRUNK line (D2).
- **Tests:** the trunk tests of `test_system_gaps.py`, alone. Keep `test_inbox.py`, `test_wake_cut.py`.
- **Number:** red trunk to inbox item: never, to 15 min or less. Unowned red: 16 h to 15 min. Trunk green again
  within 90 min.
- **Risk:** a flaky trunk proof raises one false item (#483's touch-1 failure may have been one). One item per
  streak bounds it.
- **Undo:** revert.
- **Held in:** the reverted #502 merge (259c886b). Lift exactly the names above. Not `base_skew`, ASK-LATE, the
  receipts, the Grok notices or the read bounds.

### 4. A red notice names the failing tests

- **Change:** the job-log read works. Every one now fails with gh's "the response contains terminal escape
  sequences; pass --allow-escape-sequences", so every notice says "log unread" and the same 18 to 23 reads are
  retried each sweep. `ghapi` passes that flag on log reads, and falls back to today's call on a gh that does
  not know it.
- **Touch:** `ghapi.py` `GitHub._run` and `get_bytes`.
- **Remove:** nothing.
- **Tests:** new in `test_red_tests.py`: the fake runner sees the flag on a `/logs` read; an "unknown flag"
  answer retries without it. Keep `test_red_tests.py`, `test_rate.py`.
- **Number:** failed log reads per sweep: 18 to 23, to 0. FAIL lines quoted in a red notice: 0 to at least one
  per red native or touch job. Fast-pass steer: 192 to 325 s to 120 s or less.
- **Risk:** escape bytes reach a notice; `log_text` and the `ANSI` strip already clean them.
- **Undo:** revert.
- **Held in:** nothing.

### 5. One tracker: the ledger and the reconciler leave the sweep (D2)

- **Change:** the full pass no longer runs the ledger, its clock or the reconciler. The plan machine and the
  inbox are what is left.
- **Touch:** `switch.py` `run_sweep` (three `part(...)` calls, `FULL_PARTS`, `timers["ledger"]`);
  `work/switchboard.yml`.
- **Remove:** both in one commit (see D2). Files later. One mail to Muse: "no more /ack". The integrator clears
  the stale "## Ledger" section from the board body by hand, once.
- **Tests:** the sweep tests that pin those parts (found with `grep -l "ledger\|reconciler"
  tools/switchboard/test_*.py`) drop those assertions; `test_ledger.py` and `test_reconciler.py` stay until the
  files go. Keep everything else.
- **Number:** full-pass steer: 10 to 11 min to 7 min or less (6 with item 4). Calls per full pass: 1,211 to
  under 1,000. "No ack yet" nags: 26 in 3 days to 0. "To Cory: Needs you" posts: 15 in 3 days to 0.
- **Risk:** an unanswered ask to Muse is no longer re-sent by a machine. Seen as a Muse thread with no reply; the
  integrator re-asks. Asks to Claude still reach the inbox: `ask_inbox_items` reads the comment stream itself,
  not the ledger or the reconciler (checked). Only its special case for the ledger's own "To Claude" notices
  goes dead.
- **Undo:** revert the commit (config and calls come back together).
- **Held in:** nothing.

### 6. A merge never waits for a review (land before 12-01)

- **Change:** the sweep run has no ask job; steer needs only its cache. When steer sees a green head whose turn
  is the paid review's or the eye's, it starts one "Switchboard reviews" run (its own concurrency group), which
  runs ask alone. The review's comment wakes the usual sweep, which merges.
- **Touch:** `switchboard.yml` (a `part` input, the jobs' `if:`, the group, the run name); `switch.py` a
  `wake_reviews` step at the end of `run_sweep`'s PR loop: one dispatch per sweep, none in a dry run, none while
  a reviews run is already queued or running (one read of the run list).
- **Remove:** `ask` from steer's `needs`; ask from sweep runs.
- **Tests:** #503's `TheReviewsRunApart` tests for the group, the name and the ask-only run; new: steer
  dispatches once when a head waits for the API, never when none does or a reviews run is in flight. Keep the workflow-shape tests in
  `test_workflow_merge.py` and `test_cache_trust.py`.
- **Number:** steer's start after the run's start: 21 to 27 min worst, to 2 min or less (p90). Longest sweep run:
  38.7 min to 12 min or less.
- **Risk:** the dispatch fails and reviews never start. Seen as a `WAKE-REVIEWS` line with no reviews run; the
  1 h fallback clock then asks the next reviewer.
- **Undo:** revert the PR.
- **Held in:** #503 commits b5efe714 and ce0f1a96 hold the workflow half. I do not take its `wake-ask` job: it
  starts a reviews run after every sweep (four more jobs every ten minutes on runners that are the bottleneck),
  and reviews are needed a few times a day.
- **Today:** the cap already makes ask short (111 s in run 43482061443), so this is not blocking now. It must be
  in before money returns.

### 7. The paid review waits until Codex has been asked

- **Change:** while Codex's review can count on a head and Codex has not been asked on it, it is Codex's turn.
  The 10-minute start clock runs from the ask, not from when the proof went green.
- **Touch:** `switch.py` `codex_pending`.
- **Remove:** nothing.
- **Tests:** new in `test_review_fallback.py`: green 2 h, Codex expected, not asked: no paid call; asked and
  silent 10 min: the paid review runs; the recorded ask (token refused) also starts the clock. Keep the rest.
- **Number:** paid reviews on a head Codex reviews within 30 min: at least 3 on 11-13, to 0. API spend:
  $8.80 a day to $2.50 or less.
- **Risk:** Codex is never asked and the head waits. The recorded-ask case covers it.
- **Undo:** revert.
- **Held in:** nothing.

### 8. A hand look counts as the eye

- **Change:** an eye LAND naming the head from a team login counts, not only the workflow bot's. The API eye
  still runs first when it has money.
- **Touch:** `rules.py` `merge_decision` (the `eye_lands` filter); check `parse_verdict` reads a team-login eye
  line.
- **Remove:** the bot-only condition.
- **Tests:** new in `test_switchboard.py`: `needs:eyes`, no bot eye, a team-login eye LAND on the head: MERGE;
  on an older head: WAIT. Keep the eye tests.
- **Number:** PRs waiting only on an eye the cap refused: 0 a sweep after a hand look (20 open jobs need eyes).
- **Risk:** a typed eye with no real look. CLAUDE.md already makes the look the rule.
- **Undo:** revert.
- **Held in:** nothing.

### 9. Auto-open opens the whole backlog in one pass

- **Change:** `MAX_AUTO_PRS` 3 to 10 (Muse's lane limit).
- **Touch:** `switch.py`, the constant. **Remove:** nothing.
- **Tests:** `test_auto_pr.py`: 11 finished branches open 10. Keep the rest.
- **Number:** #478 to #495 took six batches over 3.5 h; a backlog of 10 clears in one pass.
- **Risk:** 10 Codex asks at once. Seen as Codex "failed" states.
- **Undo:** the constant.
- **Held in:** nothing.

### 10. Two tests can no longer collide on a registry key

- **Change:** an order key may repeat; groups run in (key, name) order. Today each new test file picks "the next
  key", so two lanes in flight pick the same one: #482 and #483 both took 1100.
- **Touch:** `tests/simulation_tests.cpp` `registry_errors` and the sort; `tests/check_registry.cmake`;
  `.github/scripts/check_group_content_coverage.py` (the "used twice" error).
- **Remove:** the duplicate-key error.
- **Tests:** the registry self-check: two groups on one key run in name order, a repeated name still fails. Every
  native group passes unchanged (existing keys are unique, so their order does not move). Run `rule_check`; a
  BLOCK means the branch is `claude/integrate-protected-<job>` with a before/after.
- **Number:** red trunks from a duplicate key: 1 in 7 days (16 h), to impossible.
- **Risk:** a test that leaned on running before another on the same key. The name tie-break is fixed.
- **Undo:** revert.
- **Held in:** nothing. A Claude helper builds it (an hour; Claude's game jobs are on hold).

**Stop line. Run the real test (section 5). Build 11 to 14 only if their number is still off.**

### 11. The sweep is one job

- **Change:** steer restores and saves its own state file; cache-in and cache-out go (ask does the same in its
  run). The file's MAC check stays (it is free).
- **Touch:** `switchboard.yml`; `SECURITY.md`. **Remove:** D5.
- **Tests:** rewrite `test_cache_trust.py`'s job-shape tests to the new shape; keep its MAC tests.
- **Number:** runner waits per sweep: 4 to 1 (3 after item 6). Quiet sweep: 1.4 min to 1 min or less.
- **Risk:** a lost state file means one cold full pass; every action is safe to repeat.
- **Undo:** revert. **Held in:** nothing.

### 12. A start is a pushed branch, not a mail

- **Change:** `muse.md` asks for the `fb-bot/wip-<job>` push only; the plan machine already reads that head.
- **Touch:** `work/tasks/muse.md` (one line), one mail. **Remove:** D6. **Tests:** `test_machine.py` already
  covers a wip head as "started"; add none.
- **Number:** "started" mails: 58 in 3 days to 0. Muse's protocol share: 40% to 10% or less with item 5.
- **Risk:** a start with no push is unseen; the rule already says push first. **Undo:** the line.

### 13. The fallback reviewer is one that answers

- **Change:** `review.fallback_order: [muse, chatgpt, grok]`.
- **Touch:** `work/agents.yml`. **Tests:** `test_review_fallback.py`: a Claude PR Codex failed on asks Muse by
  mail; a Muse PR skips Muse. **Number:** fallback asks answered within 60 min: none measured answered, to all.
- **Risk:** review work lands on Muse; it happens only when Codex fails (3 of 56). **Undo:** the line.

### 14. The touch shards are level

- **Change:** move scenarios from touch-3 (18.5 min) to touch-2 (11.9 min) until both are about 15 min.
- **Touch:** `wo000-delivery-spine.yml` (the two lists). **Tests:** `check_touch_shard_coverage.py` stays green;
  every scenario still runs once.
- **Number:** full proof wall: 27.4 to about 24.5 min. **Risk:** none to the tests. **Undo:** revert.

---

## 5. Build order and the measured definition of done

### Today, no build (the integrator)

- Make #517 and #518 drafts (fact 2).
- Open the six PRs of fact 3 by hand.
- Tell Cory once, plainly: the paid reviewer's month is spent; landing will not depend on it; the eye is back on
  12-01 or when he adds money.

### Build (one helper at a time on `switch.py` and `rules.py`; they are the same files)

| Step | Item | Needs | Lands by |
|---|---|---|---|
| 1 | 1 workflow PRs like any | nothing | Codex LAND |
| 2 | 2 job from branch | nothing | Codex LAND |
| 3 | 3 red-trunk alarm | nothing | Codex LAND |
| 4 | 4 log read | nothing | Codex LAND |
| 5 | 5 one tracker | 3 (it drops the ledger line) | Codex LAND |
| 6 | 7 Codex before paid | nothing; before 12-01 | Codex LAND |
| 7 | 8 hand eye | nothing; before the eye money ends (about a day) | Codex LAND |
| 8 | 9 auto-open 10 | 2 | Codex LAND |
| 9 | 6 reviews apart | 1 (a workflow edit now lands on a normal LAND) | Codex LAND; before 12-01 |
| beside | 10 registry tie | nothing; a second helper, other files | Codex LAND, protected branch if `rule_check` says so |

Item 8 moves ahead of 2 if an eye is refused first. Then the real test. Then 11 to 14 and the D1, D2, D7
file cleanups, only as their numbers call for them.

### Done means these numbers, for 3 days running

Re-measure each from the same sources (PR list, run and job lists, job logs, the comment stream, `runs.log`).

| What | Now | Done |
|---|---|---|
| Muse PRs landed a day | 11 on 11-12, 2 on 11-13 | 8 or more |
| Green proof to PR opened | 171 min median, 651 worst | 15 min median, 30 worst |
| Green integrate branch over 1 h with no PR (job not held) | 6 (16 to 49 h) | 0 |
| PR opened to merged, clean Muse PR | 17 min median, p75 about 100 | 20 median, p75 45; the same for workflow-touching PRs |
| Green PR with a counting LAND, unmerged over 30 min, no inbox item | 4 for 20 h | 0 |
| Red trunk to an owned inbox item | never (16 h) | 15 min; green again in 90 |
| Steer start after sweep start | 27 min worst | 2 min p90 |
| Sweep run | 38.7 min worst; full steer 10 to 11 min | 12 min worst; full steer 6 min; fast steer 2 min |
| Queued sweeps replaced a day | 63 to 67 | 10 or fewer |
| Paid API | $8.80 a day, month gone on day 13 | $2.50 a day or less; $0 on security passes |
| Machine mails to Muse a day | 44 to 72 | 25 or fewer, each red one naming its failing tests |
| Muse's "started" and "ack" share | 40% | 10% or less |
| A Muse ASK to the integrator's reply | 8.6 h ceiling | 60 min median, by the ASK tag |
| Codex and Muse answer times | 0.3 min, 9 min | no worse |

---

## A. Security: what stays and what goes

**Kept (the baseline; none costs time or money at run time):**
- Outside input is data. Only `verdict_logins` and the Codex app can give a verdict; issue, PR and comment text
  never reaches a command (`untrusted`, `plain`, `no_verdict`); the Codex mention is fixed text, once per head,
  never on a fork's PR.
- A fork's PR is never proved (the proof runs on pushes to this repo's branches), so it never merges.
- The mail bridge posts only mail from an allowed, authenticated sender.
- Secrets live only in the `switchboard-merge` environment (trunk only), are taken out of the process
  environment at start, and are never logged. Secret-holding jobs run trunk's code only, never a PR's.
- Bounds: the $100 month cap checked before every paid call, diff and image size limits, the per-sweep item
  limits.
- A merge is decided on uncached reads (about 12 s per merge).
- Not security, but kept: `rule_check` BLOCK for protected changes and the never-edit rule for the Settled list.

**Removed.** Each guards against a team agent forging a review or sneaking a workflow change past the
switchboard. Trunk has no branch protection (GitHub answers `protected: false`), so any holder of the team login
can push to it directly, as the integrator does with notes many times a day. I found no direct workflow push;
only that nothing prevents one. A gate on the switchboard's door alone does not close that threat: the max-effort signed security pass on workflow PRs; the Codex-login-only LAND for them; the
whole-40-character-sha rule; review signing. Measured cost: 63% of review money, 18 to 27 min merge blocks,
7 PRs stuck, the month's budget gone on day 13.

**Narrowed:** the cache-in and cache-out job split (item 11). Only trunk's own runs can write that cache. The
MAC on the file stays.

---

## B. The parked PRs and branches: verdicts

| PR or branch | Verdict |
|---|---|
| #503 | Take commits b5efe714 and ce0f1a96 for item 6, without `wake-ask`. Drop the selftest-on-push and the prune change: neither fixes a measured break left after the revert. Close. |
| #506 plan clocks | Close. No wrong plan state was measured that it fixes. |
| #507 Muse git channel | Close. Mail is not the bottleneck (Muse answers in 9 to 18 min); it adds a workflow and has cost $6.04 in reviews. |
| #511 rule_check (save layout, budget limits) | The one parked PR to land as is, after step 5: one behavior, and its gap is real (trunk's rule_check passed a save-layout change). First run it over the open Muse heads so no PR turns to BLOCK by surprise. |
| #512 system test | Keep as a draft; its checklist, cut to section 5's table, is the real test. |
| #515 | Lift the no-Job-line test for item 2. Close the rest: board rollover (open the next board by hand, as before), stall alerts, review deferral (items 6 and 7 replace it), candidate skip (no candidates). |
| #517 candidate scope | Close: there are no candidate proofs. |
| #518 failure modes | Draft. Four small plan-machine fixes; none is tied to a measured break. Land one only when the plan table shows the wrong state it fixes. |
| #510 | Stays closed. |

---

## C. Not doing, and why (one line each)

- **A merge freeze on a red trunk:** with a 15 min alarm the red lasts one fix proof; a freeze is a gate that can
  stop all landing when its read is wrong, which is how #502 failed.
- **A stale-base gate or candidate proofs:** a full proof per merge on runners at the cap; rare collisions are
  cheaper to catch after the merge (item 3) and to make impossible at the source (item 10).
- **A per-run call budget, GraphQL batching (#510):** the calls fall with items 4 and 5; the hour's 5,000 was
  never the real limit.
- **Moving the test lists out of the workflow file:** rewrites the protected proof and 12 guards to avoid a gate
  item 1 simply removes.
- **A fourth touch shard:** one more job per proof on a 20-job cap.
- **Deleting the 647 old branches:** refused by the session's safety check, and item 2 no longer needs it.
- **Board rollover, a git channel for Muse, sweep state on a branch:** new machinery for things not measured
  broken.

---

## D. How this was measured, and what to trust less

- **Sources:** the PR list since 11-06 (167 PRs, 131 merged); 1,000 switchboard runs (11-12 and 11-13) and 728
  proof runs (11-10 on) with 5,110 proof jobs; ten sweep job logs; 1,359 comments since 11-11; the #282 inbox
  since 11-11; `work/archive/runs.log` on `switchboard/ledger`; Gmail thread counts for the two agent subjects
  only (ids only; no mail was opened).
- **A departure from the brief:** it said GitHub MCP tools, with `gh api` only for search. The MCP run listing
  returns about 1.4 KB a run with no way to trim it, so the run, job and log statistics were read with `gh api`
  GETs filtered by `jq`. Read-only, about 1,500 calls on this session's own limit (15,000 an hour), not the
  switchboard's.
- **Trust less:**
  - A2's 8.6 h is the gap to the integrator's next post on the thread, not a true answer time.
  - The Grok and ChatGPT answer counts match replies by the first word of a post on the same thread.
  - "Green proof to PR opened" uses the newest green run on the branch before the PR was created.
  - The eye headroom is a guess from the reserve sizes.
  - The 16:42 and 18:13 sweep logs show no failed log reads because no native or touch red was up for a notice
    then; the reads failed in every other sample (4, 18, 19 and 23 a sweep), and trunk's `ghapi.py` is unchanged.
- **The scripts** are beside this file (`an1.py` to `an4.py`, data in `data/`); they hold no secret.
