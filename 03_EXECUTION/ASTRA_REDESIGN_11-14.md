# Astra: from-scratch collaboration design, relayed by Cory 11-14

Held (Cory 11-14): not built. The current system is judged first; parts are cherry-picked only where it shows a gap.

To Claude, from Astra via Cory.

This is a proposed collaboration design, not an implemented or verified system. It was developed from the supplied constraints without inspecting the existing system.

CORE IDEA

Build one durable work record, one small controller, and one final test-and-merge lane. The controller repeatedly asks, “What remains true, and what action is still owed?” Emails and notifications merely draw attention to that record. Missing a notification cannot erase an assignment, question, pause, review request, or overdue obligation. Claude handles engineering judgment; ordinary coordination keeps running without its memory or attention.

Three limits must be explicit:

- Instantaneous agreement is impossible across these channels. The achievable guarantee is one authoritative assignment, with outdated submissions rejected.
- An unrecorded private pause cannot be distinguished from an unavailable agent. It can still be detected promptly as missing progress.
- Independent review can make hourly delivery impossible. Muse’s actual vendor must be established. Claude can review Muse only if their vendors differ. Claude-authored changes need an available reviewer from another vendor; exhausted Codex credits plus twice-daily Grok cannot satisfy that deadline.

The design also requires a small, independently hosted controller with authenticated email access and permission to open PRs and merge eligible changes. These are requirements, not capabilities established by the supplied facts. If they cannot be granted, the owner-free system is impossible. Do not conceal that gap behind another handoff to Cory.


1. WHERE EVERYTHING LIVES

Use a dedicated Git branch, coordination-v1, separate from game trunk. Its state.json is the controller’s durable record. Only the controller writes it. Completed records move into an archive on that branch.

This keeps coordination updates from constantly changing the game commit being tested.

Each fact has one authority:

- Job definition, acceptance condition, and permitted files: the job record in state.json.
- Owner and assignment version: the same job record.
- Pause, cancellation, or permission to proceed: the same job record.
- Start acknowledgement and latest work evidence: the same job record.
- Question, responsible answerer, answer, and acknowledgement: the question record in state.json.
- Submitted code: the registered branch and exact commit.
- Test result: the required CI run for the exact candidate.
- Review verdict: an explicit authenticated verdict on the PR, bound to its commit.
- Whether work landed: actual game-trunk history.

“Building,” “awaiting review,” and “ready to merge” are computed views of those facts, not independently maintained labels.

Emails and dashboard summaries include the record’s revision. They are copies for reading, never competing authorities. Every consequential submission names its job and assignment version. An expired or superseded assignment cannot advance.

Claude rereads only:

1. A short operating-policy file on game trunk.
2. The current coordination record.
3. The code and evidence needed for its present task.

Promises that exist only in Claude’s conversation do not count.


2. HOW WORK MOVES

Muse may hold ten assigned jobs, but only two jobs across the team may be actively building or awaiting landing. Muse builds one at a time. Claude’s helpers work within those jobs; they do not create four additional queues.

Jobs with overlapping permitted files cannot be active together. Shared CI lists therefore have one editor at a time. An unexpected need to touch another job’s files requires a recorded scope change before proceeding.

Every job gets one PR. Protected changes get separate, single-purpose jobs and PRs.

ASSIGN
Claude records a bounded job through the controller: owner, purpose, acceptance condition, permitted files, and protection category. The controller persists it and emails Muse.

START
Muse replies to the assignment with its job ID and acknowledgement. The controller records the start before treating it as active.

WORK
Muse supplies work evidence at least every ten minutes: a checkpoint commit, targeted-test result, or concrete finding. Machine uptime does not count.

PUSH
Muse pushes its registered fb-bot/ branch and sends a ready message naming the commit. The controller verifies the branch, assignment, and scope, then opens the PR.

PREPARE
When the final lane opens, the author incorporates current trunk once. Conflicts return to the author; the controller does not invent resolutions.

PROVE
Freeze the submitted head. Run the complete proof on its proposed integration with the pinned trunk commit.

REVIEW
Request an eligible independent reviewer for that same head while tests run.

MERGE
Recheck current assignment, pause state, trunk, submitted head, required checks, and approval. Merge only if all still qualify.

REPORT
Confirm the resulting trunk commit, archive the job, and email the outcome.

ASK
Persist the question before acknowledging receipt. Assign an answerer and retain its original age.

ANSWER
Persist the answer and deliver it repeatedly until acknowledged. “Received” or “working on it” does not satisfy the response obligation.

Email routing uses the assignment’s registered reply thread and stable job identifier. Titles have no routing authority. Conflicting or missing identifiers create an unresolved routing incident; they never select a guessed destination.

The controller reads the mailbox directly. The existing Apps Script remains untouched and outside the new system’s critical path. Use a separate mailbox or demonstrably isolated mail route so the old bridge cannot process the new traffic.

Messages to Muse contain ordinary task information, not scripts, authority claims, urgency language, or instructions to override its safeguards.

THE MERGE RULE

One full proof runs at a time: 13 jobs. Other CI work receives a combined ceiling of seven jobs. The controller runs outside that pool.

Only the controller advances trunk after cutover. Claude retains its technical role but stops pushing straight to trunk.

A proof identifies the base commit, submitted head, and tested integration tree. Changing either input invalidates eligibility. The merge operation must produce that tested tree; this behavior is a mandatory acceptance check before deployment.

Once the proof and approval are valid, the next controller pass merges. There is no additional Claude permission step.

Target: merge within five minutes of the final required gate becoming satisfied.

REVIEW SELECTION

- Muse’s vendor differs from Anthropic: Claude reviews Muse’s changes.
- Muse uses Anthropic: Codex reviews Muse’s changes.
- Claude or its helpers author code: Codex reviews it.
- Grok provides additional analysis; it is not an hourly release dependency.
- The exhausted paid Claude API is not part of the design.

Record actual code contributors, not the account that opened the PR. Claude substantively editing a Muse change adds Claude’s vendor to its authorship. Approval must come from a vendor outside that set. Avoid mixed-vendor patches when no qualifying reviewer is available.

A review request, completed review run, or absence of complaints is not approval. The reviewer must provide an explicit verdict tied to the commit.


3. AUTOMATIC OPERATION AND SILENCE DETECTION

CONTROLLER RECONCILIATION
Every two minutes: read new inputs and current facts for active jobs; retry actions still owed.

ASSIGNMENT ACKNOWLEDGEMENT
Due within ten minutes. If absent, mark delivery/start overdue and request Claude’s attention.

ACTIVE WORK EVIDENCE
Required every ten minutes; stale at twenty. Mark the worker unavailable for that job and freeze advancement.

QUESTION RESPONSE
Aim for thirty minutes; escalate at forty-five. Keep requesting Claude’s attention until an actionable response exists.

REVIEW REQUEST
Reconcile every two minutes; overdue at ten. Check reviewer availability and retry or expose the actual blocker.

CI START
Overdue after ten minutes queued. Diagnose runner or capacity blockage.

FULL PROOF
Overdue after forty minutes. Diagnose the run; never substitute partial green.

ELIGIBLE MERGE
Check every two minutes. Merge or record the concrete reason it cannot merge.

CONTROLLER HEALTH
Independent check every five minutes; stale after ten. Restart through its supervisor and raise a persistent incident.

CLAUDE FALLBACK
Scheduled every thirty minutes. Read the small current record; handle outstanding obligations or exit.

The independent watchdog checks a successfully completed reconciliation, not whether a process exists. It can restart the controller and request attention, but cannot assign jobs or merge code.

A fixed, subscribed PR is Claude’s doorbell. It contains no authoritative work state and is never merged. The controller posts when Claude has actionable work, combines related requests, and repeats an unanswered request every ten minutes. A durable claim prevents simultaneous Claude sessions from doing the same work.

The scheduled fallback costs 48 short checks per day. That cost is real. It covers lost PR notifications using the wake channels supplied. Removing empty wakes requires an additional reliable capability to conditionally schedule Claude; this design does not assume that exists.

The one-hour response target means an actionable answer or an explicit engineering decision, not a promise that every root cause can be solved in an hour. An unresolved technical question remains visible after its first response.

The delivery target cannot cover unlimited bursts. Twenty changes require at least nine hours of serialized 27-minute proofs, before failures and overhead. Ten simultaneously finished changes cannot all land in one hour. Limit work in progress before that happens, and retain the original finish time when a deadline is missed.


4. AGENT OPERATING INSTRUCTIONS

MUSE

Build your assigned game work. Read the assignment email and reply in its thread using its job ID. Work on one active job at a time.

Use the registered fb-bot/ branch and stay within the assigned files. Send a checkpoint, test result, or concrete finding every ten minutes. A running machine is not a work report.

Honor Cory’s direct instructions. If he pauses you, stop work and record the pause before going idle whenever you can. Record any direct change of scope before implementing it. Never resume a recorded pause without a recorded instruction to resume.

Send questions in the assignment thread. Keep an unanswered or unresolved question open; acknowledge answers you receive.

When ready, push your branch and report its exact commit. When the final test lane opens, incorporate current trunk once. Resolve your own conflicts. Do not change a submitted head during proof or review unless you explicitly withdraw that submission first.

Keep all required tests intact. Protected changes belong in their own jobs and PRs. Full builds run in CI; do not exhaust your machine trying to reproduce the entire proof locally.

If an assignment is stale, unclear, or outside your permitted scope, report that condition. Do not guess ownership or silently wait.

CLAUDE

At every wake, read the operating policy and current coordination record. Treat conversation memory as background, not work state.

Handle overdue questions, unavailable workers, and blocked releases first. Then review eligible Muse changes and assign bounded work. Record decisions, promises, answers, and ownership changes before ending the session.

Use helpers for bounded investigation and work within the two active jobs. Keep overlapping files under one active job. Give Muse clear acceptance conditions; take difficult diagnosis yourself.

Keep actual code authorship accurate. Do not approve a change containing code authored by your vendor. Do not turn your patch into a Muse-authored patch merely by having Muse push it.

Let the controller create PRs, request routine reviews, run proofs, merge eligible changes, retry deliveries, and report outcomes. Do not push directly to trunk or maintain a second work board.

For each obligation, give an actionable answer or record the exact blocker and next action. Do not choose multi-hour waits for an active handoff. Keep pauses intact until explicitly released.

Never weaken tests or bypass an unavailable independent reviewer. Ask Cory only for a genuine game decision or a permission that neither agent has—not to relay ordinary coordination.

CORY

Make game decisions through your usual channel. You do not relay messages, approve routine posts, track jobs, or restart the process.

Initial infrastructure permissions must be provisioned once. If the platform requires Cory’s approval for every controller post, this design cannot meet the requirement.


5. AUTOMATION SIZE

Build one controller and one tiny independent watchdog. No agent framework, distributed scheduler, shadow board, custom workflow language, or title-based command system.

Rough production line count:

- Durable record, validation, assignment versions, duplicate handling: 200–300.
- Email intake, delivery, acknowledgements: 150–250.
- Branch/PR/check/review/merge integration: 300–450.
- Scheduling decisions, deadlines, recovery: 150–250.
- Independent watchdog: 40–80.
- Total: 840–1,330.

Allow another 600–1,000 lines of meaningful tests. These are estimates, not a promise that a line limit proves simplicity.

Vendor identities, protected paths, required checks, deadlines, branch registrations, and resource limits are configuration. Operating instructions are plain text.

Persist intended actions before performing them. On restart, inspect whether the external effect already happened before retrying. That prevents duplicate PRs and mistaken duplicate merges after a crash.

Poll only registered active work, retain email cursors, and archive completed jobs. Budget normal controller traffic below 300 GitHub API calls per hour, verified during rehearsal. Monitor the shared token’s remaining allowance; below 1,000, stop optional reads and preserve capacity for active obligations. No historical rescans every few minutes.


6. THE ELEVEN FAILURES, REPLAYED

1. PRIVATE PAUSE; EVERYTHING FALSELY GREEN
Muse records the pause. If it cannot, absence of fresh work evidence marks it unavailable. A generic heartbeat cannot hide this.
Detection: twenty minutes for an unreported stop.

2. MISSED ONE-SHOT SIGNAL
The obligation remains in the durable record; reconciliation retries delivery and attention requests.
Detection: next two-minute pass.

3. QUESTIONS UNANSWERED FOR TWO DAYS
Original question age persists; repeated doorbells plus the scheduled Claude fallback.
Detection: escalation at forty-five minutes.

4. OWNERSHIP/STATUS COPIED INTO SEVERAL PLACES
One owner/version record; other statuses derived from code, checks, and reviews. Stale submissions fail validation.
Detection: at submission.

5. COLON ROUTES EMAILS INCORRECTLY
Titles ignored; registered thread/ID required. Ambiguity becomes a visible incident.
Detection: within two minutes of intake.

6. AUTOMATION MERGED WITHOUT ITS TESTS
Automation files trigger required controller tests in addition to the normal proof. The established gate remains authoritative while its replacement is tested.
Detection: before merge.

7. LONG-LIVED BRANCHES REPEATEDLY COLLIDE
Two active jobs, exclusive overlapping-file scopes, and one trunk incorporation at final-lane admission.
Detection: before granting conflicting work.

8. API EXHAUSTION
Active-only reads, bounded polling, measured request budget, remaining-quota alarm.
Detection: each reconciliation.

9. OLD WORK CREATES ALERT FLOODS
Only explicitly registered jobs in the new rollout are monitored. Old branches remain inert.
Detection: excluded from startup.

10. CLAUDE FORGETS PROMISES OR WAITS SIX HOURS
Promises and deadlines live outside its memory. Deadline values are fixed configuration, not improvised sleep choices.
Detection: next reconciliation or wake.

11. AUTOMATION GROWS INTO A FRAGILE SUBSYSTEM
One writer, one record, one release lane, small adapters, fault tests, and no duplicate management systems.
Detection: reviewed on each automation change.

The last item is a restraint, not mathematical immunity. Small programs still contain bugs. The independent watchdog and failure tests keep those bugs from silently masquerading as health.


7. FIRST-WEEK PRE-MORTEM

LIKELY FAILURE 1
Muse does not consistently acknowledge, checkpoint, or record pauses.

Start and progress deadlines expose this within ten to twenty minutes. Fix its operating behavior before increasing active work; do not lengthen the timers to hide it.

LIKELY FAILURE 2
Independent review is unavailable or does not produce a usable approval.

Verify the actual reviewer and approval format before admitting work that needs it. Exhausted capacity becomes an explicit release blocker. Never count Grok’s eventual visit as hourly coverage.

LIKELY FAILURE 3
The new controller and old automation both still believe they can advance trunk.

Enforce a real permission cutoff. Unexpected trunk movement invalidates the current candidate and raises an incident immediately.


8. ONE ACCEPTANCE TEST

Run one three-hour live rehearsal on an explicitly isolated rehearsal trunk, using the real agents, mailbox, reviewer, and complete CI proof.

Use two small changes that touch the same shared file, plus an intentionally broken automation change.

During the run:

- Drop the first assignment notification.
- Duplicate a ready email and damage its subject formatting.
- Privately pause Muse without recording the pause.
- Have Muse ask Claude a question.
- Restart the controller and start Claude with fresh conversation context.
- Submit an automation change whose own test fails.

Pass only if the evidence shows all of the following:

- No simultaneous permission to edit the shared file.
- The silent pause becomes visible within twenty minutes.
- The question receives an actionable response within one hour.
- Restart and duplicate delivery create no duplicate PR or merge.
- Each successful change lands within five minutes of its last required gate.
- The broken automation change cannot land.
- No owner relay, routine approval, or manual rescue is needed.
- Actual API usage stays within budget.

Use timestamped records, CI runs, review verdicts, and trunk commits as proof. A dashboard saying “healthy” is not evidence.

This demonstrates the operating path and the specified failure recovery. It does not prove delivery during a provider outage.


9. CUTOVER WITHOUT TURNING OLD LEFTOVERS INTO NEW WORK

1. BUILD ALONGSIDE THE OLD SYSTEM
Use the separate coordination branch, isolated mailbox, and rehearsal trunk. The old system continues landing game work.

2. ESTABLISH THE PERMISSION BOUNDARY
Identify which credentials and mechanisms can advance trunk. This is a cutover prerequisite, not a request to understand or repair the old implementation. If old writers cannot be disabled, do not activate the new merger.

3. PASS THE REHEARSAL
Reserve CI capacity explicitly so the rehearsal does not flood the existing release path.

4. CREATE AN EMPTY NEW ROLLOUT
Import no old statuses, alarms, ownership claims, or branch inventory.

5. CARRY OVER ONLY EXPLICITLY SELECTED CURRENT WORK
Claude and Muse name at most two real in-progress changes, with their actual commits, remaining work, and owners. Create fresh records. Everything else remains parked and unmonitored.

6. HAND OVER AT A RELEASE BOUNDARY
Let the old system finish its current landing. Disable its write authority before enabling the new merger. Rotate the shared credential if necessary to fence unknown old processes; the replacement retains the same API budget.

7. START FROM ACTUAL TRUNK
Reuse an existing proof only if its precise inputs and required evidence match. Otherwise run the full proof again.

8. LEAVE OLD BRANCHES ALONE
Claude need not delete them. Old work re-enters only through an explicit new assignment.

Work continues through preparation and rehearsal. The actual ownership transfer needs a short, controlled handover; promising overlapping trunk writers with zero risk would be dishonest.

If the new system fails during handover, freeze its merger first. Restore the old merger only through a deliberate exclusive transfer. Never run both as a recovery tactic.


STATUS

VERDICT: DONE — design supplied; no system deployed.
VERIFIED: UNVERIFIED — the live rehearsal remains required.
NEXT: Establish controller permissions, Muse’s vendor, and sufficient independent-review capacity.
