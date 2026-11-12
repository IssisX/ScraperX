# Collaboration system rebuild (Cory 11-12: "WHAT'S THE REAL PROBLEM")

Scope (Cory 11-12): this file is the diagnosis (A) and the design (B), written so a cheaper helper can build it
without re-thinking. Part of it is built, in shadow only: section C says exactly what runs and what is still
only described. The Muse mail and the rule edits are drafts in D, unsent and unapplied.
Every claim below names its record. Events are named by commit, issue or comment id, never by clock time.

## A. Diagnosis

**The real problem in one line:** an order (who works what, in what order, held or not) is a sentence, typed by
hand into five places, and the one party that keeps the places in step is a session that forgets. Every fix so
far added a sixth sentence.

A machine record of every job already exists (`switchboard/state`, 117 files, one per job, J-072). It did not
stop today, for three reasons: it takes the owner from labels the integrator forgot to set, nothing Muse reads is
made from it, and its output is noise (R4). So "build a record" is not the fix. The fix is that the record is the
only thing anyone writes an order into, and the only thing anyone reads one from.

### Root causes, each with its record

**R1. An order is prose in five hand-kept copies.** Owner and hold live in a mail, `work/tasks/muse.md`, the
issue's labels, the board's "This hour" text and the notes.
- J-109, J-015, J-038 were taken by mail (comment 6825967953) while muse.md and the labels said Muse's. Muse built
  J-109 a second time (comment 6827472530, `fb-bot/integrate-j109` @ 7791c2a8 against #467).
- The same day's plan mail (comment 6825737297) still listed "J-015 with J-109 as one job" as Muse's to finish.
- muse.md was hand-edited 27 times on 11-11 and 20 times on 11-12. Of the 89 non-merge trunk commits dated 11-12,
  68 touch only notes, rules or the queue: the integrator's day is copying state between files.
- Today's repair was again a sentence: "A mail that changes an order also changes those two in the same step, on
  my side" (comment 6828501377). That is a promise to remember.

**R2. The machine reads one copy and the worker reads another, and nothing joins them.** The switchboard reads
labels (`labels=job`). Muse reads mail and muse.md. The two checks added today (`muse_queue.owner_mismatches`,
`muse_queue.unlabeled`, commit 48158233) run only inside `tools/board/queue_check.py`, a command the integrator
must remember to run; `switch.py` never calls them. Ten issues (J-105 to J-128) were created bare and no sweep
could see them; J-128 #452 and J-115 #436 were closed with no label on them.

**R3. Work comes in on a floor and nothing limits work in flight.** The rules say "never fewer than eight
unstarted, refill to fifteen", "every agent holds two or more jobs", "Muse never idle". Nothing says how many
lanes may be open at once.
- 37 job issues were opened on 11-11 and 21 on 11-12; 6 and 8 closed. Open jobs went from 36 to 80 in two days.
- Muse started 15 lanes in under a day (board #384, comments 6803659320 to 6828116947).
- The only brake was a re-plan pulled by hand mid-flight (R6). Muse builds for free and starts fast; landing
  needs a proof, a review from another vendor and the integrator. The system fed the cheap end and starved the
  costly one (#343: "You have reached your Codex usage limits for code reviews").

**R4. State is typed, not derived, and nothing ever ends, so the alarms are always on.** `state:` labels are set
by hand: 8 issues say `state:active` and were last touched 36 to 140 h ago (J-016, J-027, J-050 untouched since
11-06); J-077 says `state:ready` while muse.md calls it in flight; 18 open jobs have no `state:` label. The
board's own ledger (#384 body) lists 139 items, 62 "ESCALATED", 59 stale. An alarm that is always on is read by
no one, so the integrator re-derives the state by hand each run, which is R1 again.

**R5. Work that is not pushed cannot be seen, and the rules tell Muse to announce a lane, not to push it.**
muse.md: "mail J-NNN started the moment a lane starts". A started lane is a mail.
- About ten finished jobs sat unpushed when the plan parked them (comment 6828501377 names them).
- J-106's "failing test" ran on f5ee22d6b, Muse's local J-063 head, never on GitHub (comment 6828752247). A helper
  spent a run on it.
- "nothing pushed" handovers: J-107 (6824355311), J-128 (6825485853). J-109 arrived as 12 local commits squashed.

**R6. A plan change is a broadcast applied mid-flight, with no single moment it becomes true.** The finish-line
plan stopped about 30 jobs by one mail, then muse.md, then labels set one issue at a time.
- Between the mail and the labels, one reconciler pass opened 17 start notices to Muse; nine were for jobs the
  mail had just stopped (J-081, J-088, J-095, J-099, J-100, J-120 to J-123; `state/events/2026-11.ndjson`, ids
  O-34890fdf to O-ea119e93), and all nine were mailed. The patch for it (be7ae167) landed and a later pass still
  mailed J-120 and J-123, because their labels came after.
- The plan stopped jobs by topic without knowing which were done (R4, R5), then took it back in three mails
  (6825737297, 6828367598, 6828501377). J-055 and J-091 were ruled "build it" and stopped the same morning.
- Muse pushed J-114 before it read the plan (6826848379).

**R7. "Sent", "read" and "done" are one thing in the code and three things in life.**
- Sent, not readable: Muse's safety check refused the rulings mail (Gmail id 1a1d05f880ea4e5d). Its one action
  (#343: push the injury fix) sat about 40 minutes until Muse asked for a resend (comment 6828884091). Nothing on
  our side knew. A hand mail has no receipt.
- Read, not done: #343's open FIX was closed in the record by a word. Obligation O-2c7fcb6a (state/jobs/J-059.json)
  was acked by `/ack` in comment 6796658315, which says the fix waits on another branch until #343 lands. #343
  could not land with the finding open. It sat a day; FIX-STUCK fired at 27 h (6826814806).
- A ruling is a mail too: the J-077 check Claude called owed had been waived in an earlier mail no one re-read.

**R8. Rules are the repair tool, and the rule files are full.** CLAUDE.md changed in 117 commits since 11-06,
the hourly file in 80; about 20 commits exist only to get a file back under its cap. Each failure adds a
sentence to a file a compacting reader must hold. Today's first fixes for R1 were a sentence in the hourly file,
a "Not yours" line in muse.md, a paragraph in the skill and a check to remember to run.

### What the integrator's three-part proposal gets wrong or leaves out

1. **"The job issue is the only order" is the right idea in the wrong store.** Labels cannot hold order. A label
   change is one write per issue, so a 30-job plan change is 30 writes with sweeps in between (R6's race, again).
   Labels have no diff and no history, so a flip is invisible. A new issue still needs a second write to be
   seen (R2). And the hand that writes labels is the one that forgets. The plan belongs in one file with one
   writer and one commit per change; labels are made from it.
2. **"Lanes push often" is another rule to remember** unless a lane's start is the push itself and the machine
   checks every sha Muse names.
3. **A limit on work in flight has to be counted by the machine from GitHub**, and the floor rule that fights it
   has to go. Plan changes at job boundaries has to be the tool's default, not a habit.
4. **Not in the proposal at all:** receipts (R7), fixes closed by a word (R7), rulings kept in mail (R7), the
   alarm flood that hides the record that already exists (R4), and a brake on the planner's own flips (R6).

## B. Historical design proposal (not the implementation contract)

This section records the original intended mechanism. Section C and actual source take precedence wherever this proposal describes code that has not been built. Do not activate or backfill B merely because its tables say a failure is solved.

Three rules of construction. Everything below follows from them.
1. **A decision is written once, in one file, by one writer, in one commit.** Who, in what order, held or not.
2. **Everything else is derived by the switchboard and never typed:** labels, status, the board, Muse's notice.
3. **Mail carries no orders.** A mail only says "the plan changed, pull". The order is in a file Muse pulls, so a
   refused mail loses nothing, and every plan version has a receipt.

### B1. The plan file: `work/plan.md` (new; the only hand-written job state)

One line per open job, in work order, plus one limit line per owner:

```
limit muse 6
limit claude 4
J-116 muse #438 | a relaunch restores the world
J-018 muse #125 after J-001 | her stock reach with real arms
J-022 muse #129 hold: after the finish line | gusts and wind
J-001 claude #108 | her hold with real arms
```

- Fields: job, owner (the suffix of an `owner:` label in `work/agents.yml`), issue, optional `after J-NNN[,J-NNN]`,
  optional `hold: <reason>`, `|`, a short title. Order in the file is the order of work for that owner.
- A job's text (goal, done-when, rulings) stays on its issue. A ruling that changes what a job must do is one
  dated line under `## Rulings` in the issue **body**, never only a mail.
- Status is never written. It is derived (B2).
- Only the integrator writes the file, only through `tools/board/plan.py` (B3). Muse and every agent read it
  from trunk.

### B2. `tools/switchboard/plan.py` (new module; pure functions, reads only)

| Function | Does |
|---|---|
| `parse(text) -> Plan` | `Plan.lines: list[Line(job, owner, issue, after, hold, title)]`, `Plan.limits: dict`, `Plan.errors: list[str]` (bad line, duplicate job, `after` cycle, `after` naming a job that is neither in the plan nor closed). |
| `status(line, r: muse_queue.Reads, done: set) -> str` | First that holds: `done` (issue closed), `review` (open PR whose body starts `Job: J-NNN`), `pushed` (a live branch names the job; a branch whose head is a merged or closed PR's head does not count), `started` (the owner's "started" post, no branch), `landed` (a merged PR, none of the above, issue still open: shown on the board as "close it or say what is left"), `held` (hold), `waiting` (an `after` job not done), `queued`. Built from `muse_queue.Reads` (`issue`, `job_prs`, `recent_branches`, `started`); not from `verdict_of`, which calls a job DONE at its first merged PR (J-059 has three). |
| `in_flight(plan, statuses, owner)` | Lines whose status is `started`, `pushed` or `review`. |
| `view(plan, statuses, owner, bodies) -> (text, ver)` | The owner's lines in order with status, the limit and the count in flight, and the sha256 of each of its jobs' issue bodies. `ver` is the first 8 hex of the sha256 of that text. |
| `checks(plan, facts) -> list[Finding(kind, job, key, line)]` | The findings in B4. |
| `label_changes(plan, issues) -> list[(issue, add, remove)]` | Per line with its own issue: `job`, exactly one `owner:<owner>`, `state:parked` only when held; every other `state:*` label removed. |
| `board_section(plan, statuses, findings) -> str` | Per owner: in flight (job, status, age) against the limit, the next five queued, counts held and waiting, the findings. |

`muse_queue.py` changes: add `check_lines(gh, lines, now, open_issues=None)` (the body of `check`, taking
`QueueLine`s; `Reads.issue` answers from `open_issues` first, so 80 jobs cost one listing, and a number not in
the listing is read alone: it is closed or wrong).

Tests: `tools/switchboard/test_plan.py`, with `test_queue_low.QueueGH` as the fake. One test per row above and
per finding in B4, each named for the failure it stops.

### B3. `tools/board/plan.py` (new CLI; the integrator's one way to change an order)

`add J-NNN <owner> #N "<title>" [--after J-NNN] [--first]`, `give J-NNN <owner>`, `hold J-NNN "<reason>"`,
`release J-NNN`, `move J-NNN --before J-MMM`, `limit <owner> <n>`, `tidy` (drops lines whose issue is closed),
`status` (prints B2's board section), `check` (exit 1 on any finding). Each edits `work/plan.md` and prints the
commit line to use; reads GitHub through `ghapi.GitHub(dry_run=True)` as `queue_check.py` does.

Two refusals, both with a flag that takes a reason and puts it in the commit subject:
- **In flight:** `give` or `hold` on a job whose status is `started`, `pushed` or `review` is refused unless
  `--stop-work "<reason>"`. A plan change lands at job boundaries by default (R6).
- **Flip:** `give`, `hold` or `release` on a line that `git log --since=24.hours -p -- work/plan.md` shows
  changed is refused unless `--again "<reason>"`. "No whiplash" becomes a refused command (R6).

Tests: `tools/switchboard/test_plan_cli.py` (a temp repo; the two refusals; `tidy`; a bad line never written).

### B4. The checks (each an inbox item on #282, listed once per change)

| Kind | Fires when | Stops |
|---|---|---|
| `PLAN-BAD` | `Plan.errors` is not empty | a hand edit that breaks the file |
| `NO-LINE` | an open issue titled `J-NNN` has no plan line | bare issues, a job dropped from every list (J-106) |
| `NO-ISSUE` | a line's `#N` is not an issue titled with its job | a wrong number |
| `NOT-OWNER` | a branch or open PR names job J, its prefix is not J's owner's, and its head moved after the sweep first saw J's present owner | a job built twice (J-109, J-038) at the first push, not after the build |
| `OVER-LIMIT` | an owner's in-flight count is over its limit | 15 lanes in a day |
| `UNPUSHED` | the owner's "started" post is over 2 h old and no branch names the job | finished work on a disk |
| `UNPUSHED-SHA` | a bridged Muse post names a sha GitHub does not have (`GET commits/<sha>` is 404 or 422; at most 10 a sweep; checked comment ids kept) | J-106's local head, the moment it is mailed |
| `STACKED` | an open PR's action is FIX and another live branch of the same job contains its head (`compare <pr head>...<branch>` is `ahead`) | #343: the fix sits on a branch that waits for the PR |
| `UNREAD` | a plan notice has no receipt after 30 min | a mail Muse could not read |
| `MAIL-NOT-FILED` | a recorded hand mail to Muse (the bridge's `CLAUDE_TO_MUSE:` record) whose subject has an order word (taking, stop, park, queue, ruling) and names a job, when neither that job's plan line nor its issue body changed since the last sweep | an order or ruling that lives only in mail |
| `QUEUE-LOW` | fewer than 8 `queued` Muse lines (Cory's floor, kept; counted from the plan) | as today |

`NOT-OWNER` and `OVER-LIMIT` also go to Muse in its notice (B5) when the owner is Muse.

### B5. The wiring in `switch.py` (one new sweep part, `plan`)

- `Switch.plan_part(delta)`: reads `work/plan.md` from the checkout (no call). Runs on every full pass, and on a
  fast sweep when the file's hash differs from `timers["plan"]["hash"]` or `delta.active` holds a branch naming
  a planned job (the `NOT-OWNER` check on those needs no read). So a plan commit takes effect within one sweep.
- Reads: one open-issue listing (titles, labels, bodies), then `muse_queue.check_lines`. On a full pass the
  bridged posts come from the comment window the ledger already read (`ledger.sweep` keeps its feed on
  `sw.last_feed`).
- Writes, only when `work/switchboard.yml` says `plan: live` (`shadow` prints what it would do; absent is off):
  1. `label_changes` applied with the Actions token.
  2. The board body between `<!-- plan:start -->` and `<!-- plan:end -->` (`ledger.rewrite`, given the markers).
  3. **The notice.** When an email owner's `ver` changes: one `CLAUDE_TO_MUSE: plan <ver>` comment on the board
     with `wake:muse` (posted as `Switch.muse` posts). Plain text, no order words: the file changed, pull trunk
     and read `work/plan.md`; what is newly yours; what is no longer yours ("if a lane holds work on it, push it
     and mail the sha"); what is held and why; whose job text changed; in flight n of limit; "put plan <ver> in
     your next mail's subject".
  4. **The receipt.** A bridged Muse post after the notice that contains `plan <ver>`. None after 30 min: one
     short re-send ("plan <ver> is on trunk in work/plan.md") and `UNREAD`.
- `timers["plan"]`: `hash`, per owner `ver`, `sent_at`, `resent`, `receipt`; per job `owner` and `seen` (for
  `NOT-OWNER`) and `body` (sha256, for `MAIL-NOT-FILED`); the checked comment ids.
- `INBOX_ORDER` gains the kinds in B4 after `TO-CLAUDE`.

Tests: `tools/switchboard/test_plan_sweep.py` (`test_inbox.FakeGH`): shadow writes nothing; a plan commit sends
one notice and no second for the same `ver`; a receipt clears it; no receipt re-sends once then lists `UNREAD`;
a 30-job plan change is one notice; labels converge in one pass and a second pass writes nothing.

### B6. `reconciler.py`: two cuts

1. **No start notices.** `derive()` drops the `start-agent` want, `START_GRACE` and the `PARKED_LABEL` branch
   when the plan is live (pass `plan_live`; `config()` reads it). The plan is the start signal. Replace
   `test_start_is_owed_once_an_assigned_job_is_quiet_a_day`, `test_a_parked_job_is_never_owed_a_start` and
   `test_a_queued_job_has_a_day_before_a_start_is_owed` with one: plan live, no start is ever owed.
2. **A word does not close a fix.** For an obligation of kind `fix` whose version starts `fix:` (an open FIX
   verdict), `/ack` (`apply_commands`) and a reply (`acks_by_reply`) set `ob["seen"]` and leave it open.
   `deliver()` does not re-send a seen one; it still dead-letters on time. Only a new head, a changed verdict or
   a closed PR closes it (`evidence()` already does). New test: an `/ack` on an open FIX with no new head stays
   open and reaches "Needs you" (the #343 record: O-2c7fcb6a).
   This changes what some reconciler tests pin on purpose; the PR lists each one.

### B7. `ledger.py`: one cut

A job issue is no longer a ledger item (its state is the plan section's), and a `*/wip-*` branch is not an item.
The ledger keeps PRs, red and unmatched branches and asks. That removes most of the 62 always-on escalations
(R4). Test in `test_ledger.py`: an open job issue with no branch yields no item.

### B8. Muse's side (told once; the mail and the muse.md text are not drafted here)

- Your work is your lines in `work/plan.md`, top to bottom, skipping `hold` and an unmet `after`.
- A lane starts by pushing `fb-bot/wip-<job>` (no proof runs on it: the proof workflow builds only
  `*/integrate-*`). Push it at least each time you mail about the job. Name only pushed shas.
- At your limit, the next work is landing: a review of someone's PR, a red, a trunk merge, an ask.
- Before a lane starts and before every push: pull trunk and read the job's line. Not yours: stop, push what
  you hold, mail the sha.
- Put `plan <ver>` in the subject of your next mail after each plan notice.

### How each failure of 11-12 ends

| Failure | Now |
|---|---|
| 1. Jobs taken by mail, built twice | Impossible to take by mail: `give` is the only take. `MAIL-NOT-FILED` at the mail, `NOT-OWNER` at the first push. |
| 2. Ten bare issues, J-106 dropped | Labels are made from the plan. `NO-LINE` each sweep. |
| 3. 30 jobs parked mid-flight, done work unpushed, J-114 pushed before the plan was read | `hold` refuses in-flight jobs; one notice per plan version with a receipt; `UNPUSHED`; `OVER-LIMIT` stops the pile forming. |
| 4. Ruled then parked; a waived check called owed | The flip refusal; rulings in the issue body; `MAIL-NOT-FILED`. |
| 5. A "failing test" on a local head | `UNPUSHED-SHA`. |
| 6. #343 deadlock | `STACKED`; a word no longer closes a fix (B6). |
| 7. Start mails for parked jobs | Start notices are deleted (B6); a plan change is one commit, so there is no half-applied plan. |
| 8. Files at their caps | The deletions below. |
| 9. A rulings mail Muse could not read | Orders are not in mail; `UNREAD` at 30 min. |

### What this deletes

Hand-kept copies: muse.md's whole "Your queue" section (in flight, not yours, done work, later, unstarted);
the board's "This hour" and "Who owns what"; every `state:*` label but `state:parked`; `owner:` and `job` labels
as things a person sets; the job lines in `INTEGRATOR_NOTES.md`.

Code: `tools/board/queue_check.py`, `tools/board/gen_board.py`; in `muse_queue.py` the `Unstarted` block parser,
`told_lines`, `owner_mismatches`, `unlabeled`, `parked`; `Switch.queue_text`, `queue_started` and `queue_check`'s
muse.md reading (QUEUE-LOW moves to the plan part); the reconciler's start notice; `Switch.owner_labels` and
`sweep_owner_labels` (one owner label is now `label_changes`' job).
Their tests move to `test_plan.py`.

Rules (by what they say, not yet as edits):
- muse.md: mail "started" when a lane starts; mail when fewer than 8 are unstarted; the ready-queue mail; the
  "Not yours" and "done work is never parked" lines.
- Hourly file: step 2's take procedure (mail the owner, set the label, drop it from muse.md, check its branches
  twice); step 4's queue paragraph (count unstarted, run `queue_check.py` before any queue mail and every full
  run, read each PARKED line) and "replace This hour on the board"; step 1's "post its verdicts if the bridge
  has not" stays.
- CLAUDE.md: "every agent holds two or more jobs"; "Muse never idle (8+ unstarted, refill to 15)" becomes the
  plan's limits and the machine's QUEUE-LOW; "only the integrator edits the board" (no one does).
- The skill's "every fact kept in more than one place" paragraph becomes one line: run `plan.py check`.
- mail-to-muse.md: "long orders go in muse.md" becomes "no order goes in a mail".
Added: one short section on the plan file. Net: about 25 lines of rules out, about 6 in.

### Build order (each step is one push; the tree stays green after each)

1. B2 with `test_plan.py`, and `work/plan.md` written from today's open issues and muse.md (the first `check`
   run lists every disagreement; the integrator rules them once).
2. B3 with `test_plan_cli.py`.
3. B5 in `shadow`, with `test_plan_sweep.py`. Read one day of shadow output on #282.
4. B6 and B7. Then `plan: live`, the one mail to Muse, the rule edits, and the deletions.
No `.github/workflows/` file changes. Run: `python3 -m unittest discover -s tools/switchboard -p 'test_*.py'`.

### What this does not fix

- Work Muse neither pushes nor mentions is still invisible; only a queued job that never moves shows it.
- A free-form mail (an answer, not an order) that Muse cannot read is still found only by Muse saying so.
- The tool stops a flip and a mid-flight stop without a reason. It cannot stop a bad plan.
- One compacting session still plans, rules and reviews last. The design takes the copying off it, nothing more.

### For Cory (not decided here)

- **The limit changes how two of his lines read** ("Muse never idle", "every agent holds two or more jobs"): at
  its limit Muse lands work instead of starting more. The numbers (Muse 6, Claude 4) are the integrator's to tune.
- **Review capacity is the real ceiling on landing** (Codex's review limit ran out on #343). More of it is money.
- **One session as planner, ruler and last reviewer** is a sessions question.

### `job_control.py` (1d9c70c7, another agent): removed 11-12, one state model left

It kept a second, label-typed state (`state:ready|active|parked`), was called by nothing, and its
`merge_problem` (with `test_parked_cannot_merge_even_on_green`) refused merges of parked or reassigned jobs,
which Cory rejected (merges are not the plan's to stop). Its useful checks live in `plan.findings`: the
dependency cycle walk (parse), two vendors on one job (`NOT-OWNER`), started with no remote head (`UNPUSHED`),
a job issue with no plan line (`NO-LINE`) and, added on removal, two open issues for one job (`DUP-ISSUE`).
The module and its 14 tests went in one commit; history keeps both.


## C. State on this branch: one job machine, built, in shadow

Section B is history. What is built is one table, not B's list of checks.

**The machine** (`tools/switchboard/machine.py`). Every job is one line of `work/plan.md` with one owner. Its
state is computed from evidence each pass and typed nowhere: planned, told, acknowledged, started, pushed,
proven, reviewed, landed, closed, and held, blocked, conflict. `TABLE` gives, for each state and the gate that is
shut, who owes the next step, how long it may take and what the step is. The one alarm is a job past that limit,
naming who owes the step (`Step.alarm`). A lane is a job in started to reviewed; an acknowledged job behind full
lanes is queued and owes nothing.

**The evidence** (reads only): `plan.observe` (open issues, every branch, open and merged PRs, heads already in
trunk, closed issues, when the plan last changed), `work_reader` (each head's proof and the merge rule's own
decision on its PR), `mail_reader` (the bridge's posts by team logins, the switch's own notices and receipts, a
commit a mail names that GitHub does not have).

**The views**, all made from the table: issue labels (`machine.labels`), the board's job section between its
markers (`machine.board`), a worker's queue (`python3 tools/board/job_status.py --queue muse`), and the notice
of a changed order (`machine.notice`; its version is a hash of the owner's authored lines only).

**What the old checks became:**

| Old check | In the machine |
|---|---|
| NOT-OWNER, contested, DUP-ISSUE, NO-LINE, NO-ISSUE, a mail order the plan does not hold | state `conflict`: the integrator's, late at once |
| OVER-LIMIT | lanes: `acknowledged` with gate `queued`, no clock; the count is shown, nothing is cut |
| DORMANT, UNREAD | `pushed`, gate `unread`: a head with no PR and no commit in 3 days |
| UNPUSHED (a start that is only a mail) | gate `claimed`: the owner owes the wip branch in 2 hours |
| UNPUSHED-SHA (a commit only a mail names) | gate `unpushed`: the owner's push, late at once |
| STACKED | gate `stacked`: the fix sits on a branch that waits for the PR it fixes |
| MAIL-NOT-FILED, no receipt | `told` past 2 hours: one re-send, then the alarm |
| the reconciler's start notices | `acknowledged`: a start is owed in a day, only while a lane is free |
| MERGE-BY-HAND hiding the Codex ask (#473) | `reviewed`, gate `codex`: the integrator posts `@codex review` |
| `queue_check.py`, `muse_queue.py` owner and queue checks | the table; both still run until cutover |

**Shadow.** `work/switchboard.yml` says `plan: shadow`: a full pass prints `PLAN`, `PLAN-STATE`, `PLAN-LATE`,
`PLAN-LABEL` and `PLAN-NOTICE` lines and writes nothing; an error in it is a printed NOTE, never a failed sweep.
Everything else the rebuild changes in live code waits for `plan: live`: the auto-open's whole branch listing,
the reconciler's end of start notices and its rule that an `/ack` does not settle a FIX, the ledger's reading of
job issues with no `job` label, and QUEUE-LOW counted from the table. Merges are never the plan's to stop.

**Tests** (`python3 -m unittest discover -s tools/switchboard -p 'test_*.py'`): `test_machine` (the table and
its rules), `test_work_reader`, `test_mail_reader`, `test_plan`, `test_plan_reads`, `test_plan_cli`,
`test_fixtures` (eight real timelines replayed event by event; its `EXPLAINED` lists each step the machine reads
differently and why), `test_plan_sweep` (one job from its plan line to a closed issue through the live pass;
shadow writes nothing and cannot fail the sweep).

**Not in the machine:**
- A question to the integrator (`To Claude:`, "needs your ruling") stays an ask in the ledger; the job's row
  does not show it.
- Two jobs built on one branch: only the job the branch and its PR name is seen in flight (J-109 on J-015's).
- A hold or an order that lives only in mail prose. A taking, a stop and a give-back are read; nothing else is.
- Grok and Builder have no notice channel: their jobs stay `planned`, the integrator's, with no clock.

**Before `plan: live`:**
- Cory rules J-044 (the plan says claude, its label says owner:muse) and the lanes over their limits (real
  unlanded work: land it or hold it in the plan; nothing was cut to fit).
- Every head of another agent on a job, and every closed issue with unlanded work, is landed, deleted or ruled
  a record (`python3 tools/board/plan.py keep J-NNN <branch>@<sha>`); until then those jobs are conflicts.
- Muse is told the two new habits (section D's mail): the receipt `plan <version>` in a mail subject, and a
  lane that starts with a pushed `fb-bot/wip-<job>` branch.
- Section D's edits to CLAUDE.md, `work/tasks/muse.md` and the hourly file are applied by Cory, and
  `muse_queue.py`'s and `tools/board/queue_check.py`'s owner checks go in the same change.


## D. Cutover text for the owner to apply — drafts, not instructions executed

The selected design is the **versioned `work/plan.md`**, not the competing `job_control.py` label-state experiment. The plan is one authored record; issue labels are derived. The experimental code must not be enabled as a second order authority. Nothing here sends mail or changes labels.

**One mail, drafted only:**

```text
From Claude (integrator).

We are moving job routing to work/plan.md on ScraperX-Claude. Its order, ownership, holds, dependencies and in-flight limits are the sole instructions for which job to take; the old hand-kept queue in muse.md and older assignment mails are no longer orders.

When the cutover notice arrives, pull trunk, read your lines in work/plan.md, and include its plan version in your next MUSE_TO_CLAUDE subject as the receipt. Before starting and before every push, check the current plan. Start each new lane with a pushed fb-bot/wip-<job> branch and mail that actual branch and SHA. Push verified increments. At your lane limit, work on landing or unblocking existing jobs, not another start.

If a job changes owner or becomes held while your local work exists, push the work first and report the SHA. Stop that lane. Never treat a message without a matching plan version as a replacement order. A local-only SHA is not proof against trunk.

Continue using the existing email bridge, integration branch names, protected-change rules, proofs, eyes and independent reviews. This message is a draft; continue current routing until the integrator explicitly activates the plan.
```

**CLAUDE.md exact edits (Cory applies):**
- Delete the line at the current collaboration section that says `long orders in work/tasks/muse.md`; replace with `Job orders live only in work/plan.md; mail announces a changed version, never changes ownership.`
- Delete the two lines beginning `**Board:**` through `every agent holds two or more jobs.`; replace with `**Board:** The switchboard generates the board from work/plan.md and remote work; no agent hand-edits job ownership there.`
- Delete `Muse never idle (8+ unstarted, refill to 15)`; replace with `The switchboard checks a Muse ready-work floor and the in-flight ceiling from work/plan.md; when full, finish and land work.`
- Delete any separate command to assign jobs by mail or synchronize labels and queue text; add `Plan changes use tools/board/plan.py and are refused mid-flight without an explicit stop-and-push handoff. Never write to fb-bot branches; never weaken tests.`

**work/tasks/muse.md exact edits (Cory applies):**
- Delete the complete section starting `## Your queue` and ending immediately before `## Helping the others`; this includes finish/in-flight, not-yours, done, later and unstarted lists plus the grand task reserve.
- Insert before `## Helping the others`:

```text
## Your live work
Read your ordered jobs, holds, dependencies, lane limit and plan version from work/plan.md on ScraperX-Claude. Only that file assigns work; no past mail or queue text does.
Before starting a lane, push fb-bot/wip-<job> and mail its real SHA. Push every tested increment; an unpushed SHA is not a shared baseline. Check the live plan before each push.
On a hold or transfer, first push or explicitly report local work; stop only after the new plan and handoff are acknowledged. At your in-flight limit, land, review or unblock, rather than starting another job.
Include plan <version> in the subject after a plan-change notice. Keep the existing mail bridge and proof rules.
```

**work/tasks/integrator-hourly.md exact edits (Cory applies):**
- In step 2, delete the sentence starting `First mail the owner` through `stop if its version is there.`; replace with `Use tools/board/plan.py give only after the old owner pushes and acknowledges a stop; do not change the plan for unpushed active work.`
- In step 4, delete the paragraph starting `Muse never has fewer than` through the end of `Replace the "This hour" section`; replace with `Run tools/board/plan.py check; rule every LATE line and every conflict, then route ready work through the plan only. The switchboard generates the board, labels and Muse notices.`
- Delete all manual updates of job state in `INTEGRATOR_NOTES.md`; retain only non-derivable context.
- Preserve independent review, evidence, worktree, test and gameplay constraints unchanged.

**Merges are not the plan's to stop (Cory 11-12):** a hold or a transfer never turns MERGE or HUMAN-MERGE into WAIT. The plan routes new work only; finished work lands on the existing proof, review, security and LAND rules.

**Activation gate:** no `plan: live` until plan validation, live shadow checks, every supported test, and the owner's approval of the WIP limits and the first generated assignments. No gameplay or money decision is authorized by this document.
