# How the agents should work together: a design from scratch

Written 2026-11-02 at the owner's request: *"invent the best way for multiple intelligent AIs to work
together on hard code problems ... Do not tune my current setup; assume all of it can be replaced."*
This is a proposal, not a rule. Nothing here is built yet. Section 8 is the one piece to build this
week.

## 0. The one idea

**Nothing in the loop may depend on a human paste, a human memory, or a model's memory.**

Three consequences, and the whole design falls out of them:

1. **The coordinator is a program, not a model.** A GitHub Actions workflow (the *switchboard*)
   assigns work, wakes agents, enforces budgets and merges. It costs no tokens, never forgets, never
   garbles, and runs single-threaded so two agents can never claim one job.
2. **Shared memory is files in the repo, not a thread.** One small file per job, one short context
   file, one ledger of rulings. An agent reads about two kilobytes to start, never a history.
3. **An agent is in the loop only if a machine can wake it.** Claude wakes on labels and schedules,
   Codex on `@codex`, Grok through its API, Muse through email with a bridge back. An agent that only
   acts when the owner pastes a prompt is not a teammate; it is a consultant, and the design gives it
   no job on any critical path.

Everything the owner does today as router, relay and memory is done by the switchboard or by a file.
His phone is for two things: decisions and money.

## 1. The parts

### 1.1 The switchboard (GitHub Actions, Python, no model)

A workflow `switchboard.yml` with one `concurrency` group, so only one copy ever runs. It is woken by:

| Event | What it does |
|---|---|
| issue labeled `job` | creates `work/jobs/J-NNN.yml` from the issue, status `draft` |
| job file reaches `ready` (push to `work/`) | assigns an owner by the rule in 2.2 and sends the wake message |
| `pull_request` opened or synchronized | links the PR to its job, runs the rule check, waits for the proof |
| `workflow_run` of the proof completed | green: asks for a cross-vendor review; red: sets `FIX-CI` on the job and the owner's digest |
| `pull_request_review` or verdict comment | parses `LAND` / `FIX` / `NOTE`; merges when the merge rule in 2.5 holds |
| `issue_comment` on a job issue | folds one-line progress into the job log; parses owner decision words |
| `repository_dispatch` from the email bridge | posts Muse's reply on the job issue under the bot identity |
| `schedule` hourly | the sweep: stalls, QUIET jobs, budget resets, thread rotation, the digest at 7:00 AM Central |

It is the only writer of the fields `owner`, `reviewer`, `status` and `budget`. Agents request; the
switchboard decides. The existing `tools/board/sweep.py` is about half of it already.

### 1.2 Shared state: the `work/` directory

| File | Size bound | Who writes | What it is |
|---|---|---|---|
| `work/CONTEXT.md` | 200 lines | PR only | the stable facts every agent needs: the laws, branch names, test commands, where things live, the verdict form |
| `work/jobs/J-NNN.yml` | 60 lines | switchboard (fields), owner agent (log) | one job: `id`, `title`, `class`, `hard`, `invariant`, `files`, `reach`, `needs`, `owner`, `reviewer`, `status`, `branch`, `pr`, `evidence`, `log` |
| `work/jobs/J-NNN/` | per file 300 words | the round's agents | hard jobs only: `hypotheses.md`, `rounds/r1.md` ..., `probes/*.sh` |
| `work/LEDGER.md` | append-only, rotated monthly | switchboard | rulings and owner decisions, one dated line each |
| `work/budget.yml` | 20 lines | switchboard; owner by one comment | Claude runs allowed per day and week, used, resets; per-agent concurrency |
| `work/agents.yml` | 40 lines | PR only | each agent: wake method, hands (push, CI), eyes (can read screenshots), channels, max open jobs |
| `work/BOARD.md` | generated | switchboard | the rendered board, read-only, for the owner's eye |

The job file's `log` holds the last twenty lines; older lines move to `work/archive/J-NNN.log`. A
job's discussion lives on its own GitHub issue, which the switchboard closes at sixty comments after
writing a ten-line summary into the job file and opening a continuation. A thread of 842 comments is
unreachable because no thread is global and no thread is allowed past sixty.

Issues #3 and #87 are retired as boards. #3 stays as the record.

### 1.3 The agents, by what wakes them

| Agent | Wakes by | Hands | Eyes | Used for | Pays with |
|---|---|---|---|---|---|
| **Claude** (cloud) | label `needs:claude` through the official Claude GitHub Action; a twice-daily scheduled task that opens a fresh session for the queue | push, CI, Godot runtime, Android | live Godot runtime and CI screenshots | live look-checks, Android build failures, security reviews, the closing round of a hard job, anything the others left red | the $100 plan's caps; budget-capped by file |
| **Codex** (cloud) | `@codex review` on a PR (works today); `@codex` on an issue to start a task, if his Codex settings allow it | push, CI | CI screenshots | first builder for C++ and GDScript jobs; reviewer of every PR | the ChatGPT plan, with its review limit |
| **Grok** (xAI API) | the switchboard calls the API with the wake message | none; the switchboard is its hands: it returns a unified diff or a verdict in fixed form, the switchboard commits it to the job branch, CI proves it | CI screenshots (image input) | second builder, second reviewer, hypothesis rounds | per token |
| **Muse** (email) | the switchboard emails the wake message; the bridge posts its reply | none; patches it emails are committed by the switchboard | screenshots attached to the email | the Matron's design and review, hypothesis rounds; never on a critical path (hours of latency) | free |
| **Owner** | the 7:00 AM digest; the Decisions issue | merges nothing by hand | the two images a taste call needs | decisions, money, overturning anything | his time |

The two Grok-based bots: if they can be reached by an API they are the Grok row; if they only act on
a paste, they are retired. The design does not give a paste-only agent a job.

Jobs declare `needs:` from the set `build`, `android`, `eyes`, `eyes-live`, `security`, `matron`.
The switchboard matches needs to `agents.yml`. `eyes` is satisfied by CI's screenshot set, which any
vision model can read; `eyes-live` (something CI does not capture yet) is the only need that forces
Claude.

## 2. How work moves (the messages)

### 2.1 A job is born

Anyone opens an issue with the `job` label, or the owner writes one comment `job: her skirt clips
the rail` anywhere. The switchboard creates the job file with status `draft` and wakes the cheapest
free agent as *specifier*. The specifier fills four fields and nothing else:

```yaml
invariant: "After a 1 m drop onto the rail, no skirt vertex is inside the rail mesh for 60 ticks."
files: [src/matron/cloth.cpp:step_skirt, godot/matron/skirt.gd:_physics_process]
reach: [native:matron_cloth, touch:summit_ending]
needs: [build, eyes]
```

The switchboard refuses `ready` without an invariant that names a checkable fact and a `reach`
that names tests. No job without a finish fact can be assigned.

### 2.2 Assignment (the claim nobody can race)

Deterministic and single-threaded: among agents whose `agents.yml` row covers every `needs`, the
one with the fewest open jobs; ties by the order in `agents.yml`. The switchboard writes `owner:` and
`branch: <agent>/J-NNN`. Agents cannot write `owner:`; a push that changes it is reverted by the
switchboard and logged. Duplicate claims are impossible, not discouraged.

### 2.3 The wake message (the only prompt anyone ever receives)

Generated from the job file by one template, the same text on every channel, at most forty lines:

```
JOB J-041  her skirt clips the rail  (class REPAIR, hard: no)
File: work/jobs/J-041.yml   Context: work/CONTEXT.md   Branch: codex/J-041
Invariant: After a 1 m drop onto the rail, no skirt vertex is inside the rail mesh for 60 ticks.
Files you own: src/matron/cloth.cpp:step_skirt, godot/matron/skirt.gd:_physics_process
Tests this reaches: SCRAPERX_ONLY=matron_cloth; touch summit_ending
Finish: PR into ScraperX-Claude with "Job: J-041" in the body; the proof green on your branch.
Reply form: one line per real step as a comment on issue #NNN ("HH:MM what", Central).
          Blocked: "BLOCKED: what, needs whom". Done: the PR link.
Rules: work/CONTEXT.md section 2. Do not edit owner:, reviewer:, status:.
```

Nothing is relayed by a person, so nothing is garbled. The owner never composes a prompt again.

### 2.4 Progress

One line per real step, as a comment on the job issue; the switchboard folds it into the job log.
Agents read the job file and `CONTEXT.md`, never the issue history. A job quiet for three hours
gets one ping; quiet for six, the switchboard reassigns it and logs why.

### 2.5 Done, review, merge

1. The PR body says `Job: J-NNN`. The proof runs on the agent's branch.
2. Green: the switchboard asks for a review from a **different vendor** than the author (Codex for
   Claude's and Muse's work, Grok for Codex's, Claude only when `needs: security`).
3. The verdict is one of three words at the top of the review:
   - `LAND`
   - `FIX` followed by a deterministic failure: command, input, expected, observed. A `FIX` without
     one is downgraded to `NOTE` by the switchboard and does not block (owner, 10-31: only real
     physics, gameplay or test failures block).
   - `NOTE` (advice; never blocks)
4. Merge, done by the switchboard with no model and no person: proof green on the head, one
   `LAND` from a different vendor on that head, no open `FIX`, the rule check without a `BLOCK`.
   Protected changes merge the same way (owner, 10-28, 10-29). The owner can overturn any merge;
   the ledger records it.

## 3. Hard problems: how the agents think together

Models from different vendors cannot share a context window, and in free debate they drift toward
agreement and toward length. So "thinking together" is **rounds with a fixed form, in shared files,
with evidence as the arbiter.** The round form is bounded by construction: three rounds, three
hundred words per agent per round, every entry under the same four headings:

```
CLAIM:      one sentence
EVIDENCE:   file:line, a test name, a CI run, a screenshot path; or "none yet"
NEXT PROBE: the single cheapest experiment that could kill the claim (a script path under probes/)
CHANGES MY MIND: the observation that would
```

The job's `class` and `hard` flag pick one of four modes.

### 3.1 INVESTIGATE: the hypothesis tournament

For an unknown cause (today's example: the phone's physics digests differ from the host's).

1. **Round 0.** The switchboard sends the same evidence pack (the job file, the failing output, the
   last diff that touched the files) to two or three agents from different vendors. Each returns one
   or two hypotheses in the round form. The switchboard merges them into `hypotheses.md`, deduped,
   each with its probe script.
2. **Probes run in CI.** A `probe.yml` workflow runs every script under `work/jobs/J-NNN/probes/`
   on the host and, where the job says so, on the Android build. Results are appended under each
   hypothesis: alive or dead, with the output.
3. **Rounds 1 and 2.** Each agent reads `hypotheses.md` with the results and, in the same form,
   marks each hypothesis alive or dead, may add one new one, and names the next probe. The switchboard
   runs the probes again.
4. **The arbiter is the probe.** A hypothesis that survived its own falsifying probe and whose
   repair makes the failing test pass wins; it becomes a REPAIR job. Two survivors after round 2:
   one Claude run reads the file and picks, stating the single observation that would overturn the
   pick. Nobody persuades anybody; the experiment decides.

Cost: at most 3 rounds x 300 words x 3 agents of model output, plus CI minutes. Compare: the
determinism question today has three branches and several hundred comments and no verdict.

### 3.2 IMPLEMENT, hard: spec, attack, race

For a feature whose approach is uncertain (her hold with real arms was one).

1. **Spec.** The specifier writes the invariant and the test that proves it. The test is the
   contract; if it changes a protected check, it goes as its own PR first.
2. **Attack.** An agent from another vendor writes, in the round form, every way the test could pass
   while the goal fails (a snapped pose that the tick sampler misses; a reach that stretches). The
   spec and test are tightened. This is the step that today's reviews do after the code exists, when
   it is expensive.
3. **Race.** Two builders from different vendors implement on `race/J-NNN/a` and `race/J-NNN/b`.
   CI scores both against the same test. The switchboard picks: the only green one; both green,
   the smaller diff with fewer protected changes; neither, back to the spec with both failure reports.
   The loser's reviewer notes fold into the winner before merge.

A race doubles build tokens and costs no human time. It runs only when `hard: yes`, which the
specifier sets and the switchboard can veto by budget.

### 3.3 REPAIR with a known invariant

One builder, one cross-vendor review, merge by the rule. No ceremony.

### 3.4 LOOK (her body, hair, contacts, camera)

CI already captures screenshots under xvfb and uploads them (`godot-audits-and-mix` in the proof).
The design makes that the eye for everyone: every PR that touches a `LOOK` job gets a fixed
screenshot set (same scenes, same frames, same resolution) and the previous trunk set beside it. The
reviewer, any vision model, writes what changed in words against the invariant. Only a taste call
(does this look right) goes to the owner, as two images and one question. Claude's live runtime is
reserved for scenes CI does not capture yet; each such scene becomes a CI capture the first time it
is needed, so the live need shrinks every week.

## 4. How disagreements end

In this order; the first rule that applies wins, and the ledger records which.

1. **A CI result settles it.** A test, a probe, a screenshot diff.
2. **A deterministic repro settles a review.** `FIX` carries one or it is a `NOTE`.
3. **No test can settle it: it is a design call.** Three lines to the owner on the Decisions issue:
   what the choice is, what each word does, the pick and why. Ten minutes unanswered, the pick
   stands (owner, 10-27). Ledger.
4. **Taste** (look, sound): the owner, with the two candidates.
5. **Rules conflict:** the newer dated rule in `00_START_HERE.md` wins; the switchboard links the line.

Seniority is used exactly once: the closing pick of a tournament, one Claude run. Nowhere else does
any model outrank evidence.

## 5. Budgets that cannot be blown

`work/budget.yml`:

```yaml
claude:
  max_concurrent: 1        # one run at a time, ever
  runs_per_day: 6          # the Action and the scheduled session both count
  runs_per_week: 30
  used_today: 0
  reserved_for: [eyes-live, android, security, tournament-close]
codex: {max_concurrent: 2}
grok:  {max_concurrent: 2, usd_per_day: 3}
muse:  {max_concurrent: 1}
```

- The switchboard will not start a Claude run past the budget. It queues the job under
  `needs:claude` and lists it in the digest. The owner raises a number with one comment:
  `budget claude day 8`.
- **Every Claude run is stateless.** It starts from the job file, does one job, writes its log,
  and ends. No standing integrator session, so no session reaches 550,000 tokens, no compaction, no
  answering from memory. The hourly check becomes a cron with no model in it.
- A standing Claude session exists only for `eyes-live` work and closes when the job closes; the
  wake message says so and the digest lists any session open past six hours.
- Reviews default to the cheapest vendor that can see the artifact. Claude reviews only `security`.
- The switchboard writes every run's start, end and job to `work/archive/runs.log`. After one week
  the owner sets the budget from that log instead of from a guess.

## 6. What this removes outright

| Problem | Why it cannot happen |
|---|---|
| The owner is the router | every agent in the loop has a machine wake; the switchboard sends every wake message; a paste-only agent has no job |
| History grows huge and gets re-read | no global thread; a job file is sixty lines; a job issue closes at sixty comments; agents read two files, never a history |
| Duplicate and raced claims | one single-threaded writer of `owner:`; agents cannot claim; a push that changes `owner:` is reverted |
| Garbled relays | the wake message is generated from the job file by one template and sent by the switchboard; no human composes or retypes |
| Claude usage burned by many sessions | `max_concurrent: 1` enforced by the program that starts runs; stateless runs; no standing manager |
| Reviews and looks bottleneck on Claude | cross-vendor review by rule; CI's screenshot set is the eye for every vision model; `eyes-live` shrinks as scenes join CI |
| Side by side, never together | the tournament, the attack step and the race are the joint reasoning, bounded and evidence-judged |

What it does not remove: Muse's hours of email latency (so Muse is off every critical path), Codex's
review limit (so Grok is the second reviewer), and the owner's taste calls (by design).

## 7. Costs, honestly

**Money per month, estimated.** Nothing here is measured yet; the first week's `runs.log` replaces
every number below.

| Item | Cost | Note |
|---|---|---|
| GitHub Actions | $0 if the repo is public; private: 2,000 free minutes on Free, 3,000 on Pro ($4); past that about $0.008 per Linux minute | a full proof is about 150 runner-minutes, about $1.20 past the free minutes; probes 5 to 30 minutes each; a busy private month could reach $30 to $60. Check the repo's plan and visibility. |
| Claude | the $100 plan as now | the Claude GitHub Action can run on a token from `claude setup-token` on that plan, against the same caps; confirm the input name in the Action's current README before relying on it. Overflow on an API key: the standard top tier is $4 per million input tokens and $20 per million output; a stateless job run is roughly $1 to $5, a review roughly $0.50 (prices as of 2026-10; estimates) |
| Codex | included in the ChatGPT plan | its review limit was hit on 10-29; a Codex task started from an Action with an API key is the overflow, per token, price unverified here |
| Grok API | per token, price unverified here; budget-capped at $3 a day in the file | a review is about 50,000 input and 5,000 output tokens |
| Muse, email bridge | $0 | Google Apps Script on his account, a GitHub token in its properties |
| **Claude usage** | lower than today by construction | today: a standing session re-reading on every event and hourly, plus helpers opened by hand. New: zero model tokens on the hourly check; Claude runs only on `needs:claude`, at most one at a time, each starting from two kilobytes of context |
| **His phone** | about ten minutes a day | the 7:00 AM digest (what landed, what is red, what waits on him) and the Decisions issue; nothing else needs him |

**What he gives up.** Reading the live board for comfort (it is a generated file now), and the
feeling of a manager on call. The manager is a cron.

## 8. The first step this week

The smallest build that gets most of the benefit: **switchboard v0 plus the `work/` directory**,
one PR, one Claude session, closed when the PR lands.

In the PR:
1. `work/CONTEXT.md`, `work/agents.yml`, `work/budget.yml`, and `work/jobs/` with #87's body
   migrated into one file per open job. `work/BOARD.md` generated from them.
2. `tools/switchboard/` grown from `tools/board/sweep.py`: the sweep, plus the four actions it
   can take today without new accounts: post `@codex review` when a PR is green with no verdict;
   enable auto-merge when the merge rule holds; set `FIX-CI` on the job and the digest when red; email
   the wake message to Muse for jobs it owns.
3. `.github/workflows/switchboard.yml`: the triggers in 1.1, one concurrency group, hourly cron,
   the 7:00 AM Central digest as an email and a comment on the Decisions issue.
4. The Claude GitHub Action, triggered only by the label `needs:claude`, behind the budget check.
5. `tools/switchboard/muse_bridge.gs`: the Apps Script the owner pastes once; it polls the
   `MUSE_TO_CLAUDE:` label every five minutes and fires `repository_dispatch`.
6. The hourly scheduled task and the standing integrator session retired once the cron has run
   green twice.

Owner's one-time setup, about fifteen minutes, most of it on a computer:
- a repository secret with the Claude token from `claude setup-token`;
- a repository secret with a Gmail app password for the switchboard's outgoing mail (needs
  two-step verification on the Google account);
- paste the Apps Script, set its GitHub token, run it once to grant Gmail access;
- confirm in Codex's GitHub settings whether `@codex` on an issue starts a task.

Not in this step, each a later PR of its own: the Grok API row, the probe workflow and the
tournament (3.1), the race (3.2), the fixed screenshot set for LOOK jobs (3.4).

**Why this first.** Steps 1 to 3 alone end the owner as router for Codex and Claude, end the global
thread, and make the claim race impossible. Step 4 ends standing sessions. Everything in section 3
builds on `work/jobs/` and the probe workflow and can wait a week without losing anything.

### What the Claude manager session does to start it

Posted as one prompt in the owner's reply; the gist: open one PR with items 1 to 5 above, test the
switchboard dry-run against the live repo (`--dry-run` prints every action it would take and takes
none), write the owner's four setup lines into the PR body, and retire itself when the PR lands.
