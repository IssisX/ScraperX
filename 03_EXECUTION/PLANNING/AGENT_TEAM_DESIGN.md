# How the agents should work together: a design from scratch (v2)

Written 2026-11-02 at the owner's request: *"invent the best way for multiple intelligent AIs to work
together on hard code problems ... Do not tune my current setup; assume all of it can be replaced."*
Revised the same day after his three corrections: *"There's also find-skills too. Don't rule out
anything. There has to be a better way for multiple AIs to collaborate. Only Claude can see it
really. The other ones don't do it right."*

This is a proposal, not a rule. Nothing here is built. Section 9 is what to build this week.

## 0. The three ideas

1. **One brain, many hands.** Agents from different vendors cannot share a context window, so
   "thinking together" through a shared thread never happens; they take turns. The fix is to put the
   joint reasoning in one place: a hosted Claude *orchestrator* that holds the whole problem in one
   context and calls the other agents as tools, with the same evidence, and reads their answers back
   into that one context. The others are hands and second opinions; the orchestrator is the room
   where the thinking happens. It runs on the API with a hard dollar budget, not on the $100 plan.
2. **A tracker that wakes agents by assignment, not a thread that waits for a paste.** Linear
   (already connected to this session) starts Codex and Devin when an issue is delegated to them,
   and its Agents API lets the orchestrator be an agent too. Assign, and it wakes. Nobody pastes.
3. **Claude is the only eye, so make the eye cheap and automatic.** CI already captures screenshots
   under xvfb. Every look-check becomes a fixed screenshot set that a Claude vision call judges for
   cents through the API, with a written rubric. Live Godot sessions on the plan are kept for
   scenes CI cannot capture yet, and each one becomes a CI capture the first time it is needed.

Everything the owner does today as router, relay and memory moves to Linear, the orchestrator or
a file. His phone is for decisions, taste and money.

## 1. What was considered, and what it is for

Nothing ruled out. Each row says what it adds and what it costs; section 2 picks.

| Option | What it adds | Cost | Verdict |
|---|---|---|---|
| **Linear as tracker and wake switch** | delegate an issue to Codex or Devin and they start; comments and progress come back on the issue; Agents API (developer preview, free) lets our own agent be delegated to; good phone app | Free plan: 250 issues, no AI credits. Basic about $10 a user a month. Linear's own coding sessions (Claude Code or Codex inside Linear) use prepaid AI credits, roughly $0.07 to $0.20 a run by third-party reports | **use** as the board and the wake switch |
| **Anthropic hosted agents (Managed Agents)** | a Claude agent that Anthropic runs: its own sandbox, scheduled runs, session budgets that pause at a dollar cap, workflows that fan out many sub-agents, a grader that iterates toward a rubric | $0.08 per running session-hour plus tokens at API rates; standard top tier $4 per million input, $20 per million output, cache reads about $0.20 | **use** as the orchestrator |
| **Claude GitHub Action on the plan** | `@claude` or a label wakes a stateless Claude run against the Max plan's caps, with the official `claude_code_oauth_token` input | $0 beyond the plan; one bug report of auth failing after a Pro-to-Max upgrade | **use** for stateless jobs that must stay on the plan; API key as the fallback |
| **Codex via Linear delegation** | reliable wake; `@codex` on GitHub issues is reported broken or ambiguous since August to October 2026 | included in the ChatGPT plan, with its review limit | **use**, through Linear, not GitHub mentions |
| **Devin** | a second wakeable builder with its own sandbox, delegated from Linear or Slack; Devin Review for PRs | $20 a month plus about $2.25 per ACU, about $9 per active hour | **optional**; add when Codex's limit is the bottleneck |
| **Grok through the xAI API** | a cheap second reviewer and hypothesis generator, called as a tool by the orchestrator | about $2 per million input, $6 per million output (third-party listings; confirm at docs.x.ai) | **use** as a tool |
| **Muse by email** | the Matron specialist; a free Apps Script bridge turns its replies into comments without the owner approving each | $0 | **use**, off every critical path |
| **Slack Code channels** | a channel where Claude, Devin and (per Slack) Codex can be tagged; a good phone view | Slack plan; whether one bot can wake another in a channel is unverified | **later**, for the owner's view only |
| **Claude Tag (Claude in Slack)** | a persistent Claude in a Slack channel the owner can talk to from his phone | plan-dependent | **later**, same reason |
| **Skills catalog** | searched the owner's catalog for multi-agent, orchestration, collaboration, swarm: nothing. Public catalogs have "orchestrator" and "agent-orchestration" skills, all Claude-only subagent fan-out, plus an "ask-codex" skill that lets a Claude session consult Codex | free | **not the answer**; the orchestrator's tools do the cross-vendor part. The `ask-codex` idea is reused as a tool |
| **Claude Code's own workflows and subagents** | fan-out inside one Claude session | plan tokens | **use inside the orchestrator** for parallel probes, never as the team |
| **GitHub issues as the board** (today) | nothing the above lacks | free | **retire**; GitHub keeps code, PRs, CI |

## 2. The design

### 2.1 Parts and where each runs

| Part | Runs on | Pays with |
|---|---|---|
| **Board**: one Linear issue per job; the owner's phone app | Linear | free or $10 a month |
| **Orchestrator**: one hosted Claude agent, registered as a Linear agent; woken by delegation, by a schedule, and by GitHub events through a small relay | Anthropic hosted agents, API | tokens plus $0.08 an hour, under a session budget |
| **Hands**: Codex (Linear delegation), Grok (API tool), Devin (optional), Muse (email) | each vendor's own cloud | each vendor's plan or API |
| **Proof**: the existing CI; a `probe` workflow for experiments; a `screens` job for the fixed screenshot set | GitHub Actions | free on a public repo; about $0.008 a Linux minute past free minutes on a private one |
| **Eye**: Claude vision calls on the screenshot set, by the orchestrator | API | cents per set |
| **Live eye**: a Claude Code cloud session with the Godot runtime, only for `eyes-live` jobs | the $100 plan | the plan's caps; at most one open |
| **Memory**: `work/CONTEXT.md` (200 lines), `work/LEDGER.md` (rulings), `work/jobs/<id>/` for a hard job's hypotheses and probes | the repo | free |
| **Merger**: a GitHub Actions workflow with no model in it | GitHub Actions | free |

### 2.2 What the orchestrator is

One agent definition, versioned in the repo as files. Its tools:

- `read_job`, `write_log`, `set_status` on Linear (its own agent identity; it is the delegate, the
  owner stays the assignee);
- `delegate(agent, job)`: delegates a Linear issue to Codex or Devin, or emails Muse;
- `ask(vendor, question, evidence)`: one call to Grok, Codex or Muse with a fixed evidence pack,
  answer returned into the orchestrator's context (the `ask-codex` idea, generalized);
- `run_probe(script)`: dispatches the probe workflow and waits for the result;
- `look(set)`: fetches a screenshot set from a CI run and judges it against the job's rubric;
- `git` and `gh` in its sandbox for reading code and opening PRs;
- `decide(question, options, pick)`: posts a three-line decision for the owner on the Linear issue
  and starts the ten-minute clock.

Each job is its own session with its own dollar budget. The session ends when the job's PR merges
or the budget pauses it; a paused session is a line in the owner's digest, never a silent stall.
Its memory between jobs is `work/`, not its own transcript, so no session grows for weeks.

### 2.3 How a job moves

1. **Born.** The owner, or any agent, creates a Linear issue with the `job` label. The
   orchestrator wakes (delegation webhook), reads the issue and `work/CONTEXT.md`, and writes the
   four fields on the issue: `invariant` (the finish fact), `files` (owned functions), `reach`
   (tests it reaches), `needs` (`build`, `android`, `eyes`, `eyes-live`, `security`, `matron`). An
   issue without an invariant cannot leave `Triage`.
2. **Assigned.** The orchestrator delegates by rule: needs matched against `work/agents.yml`,
   fewest open jobs, Codex before Devin before Grok-as-tool. Linear records the delegate; a second
   delegation of the same issue is refused by the orchestrator, which is single-threaded per issue.
   Duplicate claims are impossible, not discouraged.
3. **Worked.** The hand works in its own sandbox and posts progress on the Linear issue (Codex and
   Devin do this natively). The orchestrator reads the issue, never a history.
4. **Proved.** The PR says `Job: <id>`. CI runs on the branch. The `screens` job adds the fixed
   screenshot set when `needs` has `eyes`.
5. **Reviewed.** The merger asks a different vendor from the author for a review (Codex reviews
   Claude's and Muse's work; Grok, through the orchestrator, reviews Codex's; Claude reviews only
   `security`). The eye runs on the screenshot set and posts its judgment, with the rubric line it
   failed, as a review.
6. **Merged.** By the merger, no model, no person: proof green on the head, one `LAND` from a
   different vendor on that head, the eye's `LAND` when `eyes` is a need, no open `FIX`, no
   rule-check `BLOCK`. Protected changes the same way (owner, 10-28, 10-29). The owner can overturn
   any merge; the ledger records it.

The verdict form: `LAND`; `FIX` with a deterministic failure (command, input, expected, observed),
otherwise downgraded to `NOTE`; `NOTE` never blocks (owner, 10-31).

### 2.4 The eye

The owner's standard: *"Only Claude can see it really."* So the eye is Claude, and the design's job
is to make Claude's looking cheap, repeatable and off his quota:

- **A fixed screenshot set per scene**, captured by CI: same scenes, same frames, same resolution,
  the trunk's set beside the PR's. Today's `godot-audits-and-mix` job already does this for the
  approach and the summit; each `LOOK` job adds its scene once.
- **A rubric per set**, written in words in `work/looks/<scene>.md`: what must be true (her hand
  on the hold, no skirt through the rail, the camera framing the five beats).
- **The judgment** is one Claude vision call per set, from the orchestrator, through the API: about
  thirty thousand input tokens for twenty images, around ten cents at the standard top tier. It
  answers `LAND` or `FIX` with the rubric line and the frame.
- **Taste** (does this look right, not is it correct) goes to the owner as two images and one
  question. Nothing else does.
- **Live** looking (driving the Godot runtime, new scenes) stays on a Claude Code cloud session on
  the plan, at most one open, closed when the job closes. Each live scene becomes a CI capture the
  first time, so the live need shrinks weekly.

### 2.5 Thinking together: the modes

The orchestrator holds the room. The others are consulted into it. Bounded by construction: a
question to a vendor is one call with a fixed evidence pack and a three-hundred-word answer in the
form `CLAIM / EVIDENCE / NEXT PROBE / CHANGES MY MIND`.

**INVESTIGATE, the hypothesis tournament** (today's case: the phone's physics digests differ from
the host's).
1. The orchestrator writes the evidence pack and asks two or three vendors for hypotheses, in the
   form above, each with a probe script.
2. It dedupes them into `work/jobs/<id>/hypotheses.md` and runs every probe through the probe
   workflow (host, and Android when the job says so).
3. It reads the results, marks each hypothesis alive or dead, and asks the vendors again with the
   results. Three rounds at most.
4. The arbiter is the probe: the hypothesis that survived its own falsifier and whose repair makes
   the failing test pass becomes a REPAIR job. Two survivors after round three: the orchestrator
   picks and writes the one observation that would overturn the pick.

**IMPLEMENT, hard: spec, attack, race.**
1. The orchestrator writes the invariant and the test that proves it; a protected test goes as its
   own PR first.
2. It asks a different vendor to attack: every way the test could pass while the goal fails. The
   spec and test are tightened before any code exists.
3. Two hands from different vendors build on two branches; CI scores both against the same test.
   The orchestrator picks: the only green one; both green, the smaller diff with fewer protected
   changes; neither, back to step 1 with both failure reports.

**REPAIR with a known invariant:** one hand, one cross-vendor review, merge by rule.

**LOOK:** the eye in 2.4, and the owner for taste.

### 2.6 How disagreements end

In this order; the first rule that applies wins, and the ledger records which.

1. A CI result settles it: a test, a probe, the eye's rubric line.
2. A deterministic repro settles a review. `FIX` carries one or it is a `NOTE`.
3. No test can settle it: a design call. Three lines to the owner on the Linear issue: the choice,
   what each word does, the pick and why. Ten minutes unanswered, the pick stands (owner, 10-27).
4. Taste: the owner, with the two candidates.
5. Rules conflict: the newer dated rule in `00_START_HERE.md` wins.

The orchestrator's own judgment is used once: the closing pick of a tournament. Nowhere else does a
model outrank evidence.

### 2.7 Budgets that cannot be blown

- **The orchestrator** has a session budget in dollars per job (start at $8 for a hard job, $2 for
  a review) and a monthly cap on the API account. At the cap it pauses and says so. The owner raises
  a number with one comment on the issue: `budget 12`.
- **The plan** is touched only by live-eye sessions (at most one open) and by the Claude GitHub
  Action for jobs marked `plan-only`; `work/budget.yml` caps both per day and the merger enforces it.
- **Every run is stateless**: a job session starts from the Linear issue and `work/CONTEXT.md`,
  does one job, ends. No session reaches 550,000 tokens; no answer comes from memory.
- **Vendors** are called through tools with a per-day cap each in `work/budget.yml` (Grok $3 a
  day; Codex by its plan; Devin by ACU count).
- Every run's start, end, job and cost goes to `work/archive/runs.log`; after one week the budgets
  are set from the log, not from a guess.

## 3. What this removes outright

| Problem | Why it cannot happen |
|---|---|
| The owner is the router | delegation in Linear wakes Codex and Devin; the orchestrator wakes on delegation and schedule; Muse's replies arrive by bridge; no agent in the loop needs a paste |
| History grows huge and gets re-read | no global thread; a job is one Linear issue; the orchestrator's memory is `work/`, not its transcript; each session starts from two kilobytes |
| Duplicate and raced claims | one delegate per issue, set by one single-threaded orchestrator; Linear refuses the rest |
| Garbled relays | the wake is the issue itself; the evidence pack is generated; no human composes or retypes |
| Claude usage burned by many sessions | the joint reasoning runs on the API under a dollar cap, not on the plan; the plan holds at most one live-eye session, enforced by the merger |
| Reviews and looks bottleneck on Claude | cross-vendor review by rule; the eye is a ten-cent API call on a CI artifact, not a session |
| Side by side, never together | the orchestrator holds one context for the problem and consults the others into it; tournaments, attacks and races are bounded and evidence-judged |

What it does not remove: Muse's hours of latency (off every critical path), Codex's review limit
(Grok and Devin are the second reviewers), and the owner's taste calls (by design).

## 4. Costs, honestly

Estimates; nothing is measured yet. The first week's `runs.log` replaces every number.

| Item | Per month, estimated | Note |
|---|---|---|
| Anthropic API account | $40 to $120 | a hard job session about $5 to $8 (three to four hours running, a few million mostly cached input tokens, under two hundred thousand output); a review about $1; an eye judgment about $0.10; set a monthly cap on the account first |
| The $100 plan | as now | now spent on live-eye sessions and plan-only jobs instead of a standing manager |
| Linear | $0 to $10 | Free plan holds 250 issues; Basic if that runs out or Loops are wanted |
| Grok API | $5 to $30 | capped at $3 a day in the file |
| Codex | as now | the review limit stays; Devin takes overflow if added |
| Devin (optional) | $20 plus about $9 per active hour | only if Codex's limit becomes the bottleneck |
| GitHub Actions | $0 public; $30 to $60 busy private | a proof is about 150 runner-minutes; probes and screens 5 to 30 minutes |
| Muse bridge, email | $0 | Apps Script on his Google account |
| **His phone** | about ten minutes a day | the morning digest on Linear, the decisions on the issues, taste calls with two images |

The honest trade: this converts his quota problem into a dollar problem with a cap he sets. The
plan's caps stay the limit on live looking only.

## 5. What each message contains

- **Wake (delegation)**: the Linear issue with the four fields; nothing else is sent.
- **Evidence pack (orchestrator to a vendor)**: the invariant, the failing output, the diff that
  last touched the files, the probe results so far; under four thousand words, generated.
- **Answer (vendor to orchestrator)**: `CLAIM / EVIDENCE / NEXT PROBE / CHANGES MY MIND`, three
  hundred words.
- **Progress (hand to issue)**: one line per real step, `HH:MM what`, Central.
- **Verdict (reviewer or eye to PR)**: `LAND`, `FIX` plus repro, or `NOTE`.
- **Decision (orchestrator to owner)**: three lines and a word to answer with.
- **Digest (orchestrator to owner, 7:00 AM Central)**: landed, red, waiting on him, paused by
  budget, sessions open.

## 6. What the owner gives up

Reading a live board for comfort (Linear is the board now), the feeling of a manager on call (the
manager is a hosted agent that wakes on events), and the illusion that a shared thread is
collaboration.

## 7. Where the previous design (v1) was wrong

- It made the coordinator a GitHub Actions script and kept the joint reasoning in files that agents
  take turns reading. That removes the router but never gets two models into one context. The
  orchestrator does.
- It let any vision model judge screenshots. The owner's experience says no; the eye is Claude only,
  made cheap through the API.
- It used `@codex` on GitHub, which is reported unreliable since August 2026. Linear delegation is
  the documented path.

## 8. Open checks before building

Each is one lookup, none blocks the first step:
- Linear's Agents API is a developer preview; confirm the delegation webhook works on the Free
  plan, or budget Basic.
- The orchestrator on hosted agents needs an Anthropic Console account with prepaid credits and a
  monthly cap; confirm the cap is set before the first session.
- The Claude GitHub Action on the plan: one report of auth failing after a Pro-to-Max upgrade; test
  once with a trivial job before relying on it.
- Grok's current API price at docs.x.ai.
- Whether Codex's Linear delegation can push to `ScraperX-Claude` side branches with its current
  GitHub app permissions.

## 9. The first step this week

Two things, in this order, each small, each useful on its own.

**Step A, today, no code: Linear becomes the board and Codex's wake.**
1. Create a Linear team `ScraperX` with the open jobs from issue #87's body, the four fields in the
   description, labels `job`, `hard`, `needs:*`.
2. Connect Codex to Linear (the owner, in Codex's settings: GitHub and Linear). Delegate one small
   job to Codex and watch it start without a paste.
3. Retire #87 as a board; one comment there points to Linear.

**Step B, this week, one PR, one Claude Code session closed when it lands: the merger, the eye and
the bridge.**
1. `work/CONTEXT.md`, `work/agents.yml`, `work/budget.yml`, `work/looks/` with the rubric for the
   two scenes CI already captures.
2. `.github/workflows/merger.yml`: on proof completion and reviews, the merge rule in 2.3 step 6,
   the cross-vendor review request, the eye's judgment (one API call on the screenshot artifact, API
   key in a repository secret), and the 7:00 AM Central digest posted to Linear.
3. `tools/switchboard/muse_bridge.gs`: the Apps Script for Muse's replies.
4. The hourly scheduled task and the standing integrator session retired once the merger has run
   green twice.

**Step C, next week, the orchestrator**: the hosted agent definition as files, registered as a
Linear agent, with the `ask`, `run_probe` and `look` tools, run first on the determinism question
as a tournament, under an $8 budget. That is the first time the team thinks together.

Owner's one-time setup, about twenty minutes: an Anthropic Console account with a monthly cap and
an API key as a repository secret; Codex connected to Linear; a Gmail app password secret for the
bridge's outgoing mail; the Apps Script pasted with a GitHub token.

## Sources checked on 2026-11-02

- Linear agents and delegation: https://linear.app/integrations/agents ,
  https://linear.app/developers/agents , https://linear.app/changelog/2026-06-11-coding-sessions
- Linear pricing and credits (third-party trackers): https://omidsaffari.com/blog/linear-pricing ,
  https://linear.app/legal/ai-addendum
- Codex GitHub mentions, reported state: https://github.com/openai/codex/issues/54382 ,
  https://github.com/openai/codex/issues/34425 , https://github.com/openai/codex/issues/42098
- Hosted agents pricing and workflows (third-party): https://www.truefoundry.com/blog/claude-managed-agents-pricing ,
  https://imisofts.com/blog/claude-managed-agents-pricing/ ,
  https://alphasignal.ai/news/anthropic-s-managed-agents-gets-budget-caps-geo-pinning-and-smarter-advisor
- Claude GitHub Action on a subscription: https://code.claude.com/docs/en/github-actions ,
  https://github.com/anthropics/claude-code-action/issues/1281
- Devin pricing (third-party): https://www.eesel.ai/blog/cognition-ai-pricing ,
  https://aitoolpick.org/blog/devin-pricing-2026/
- Grok API pricing (third-party): https://www.morphllm.com/grok-api-pricing ,
  https://www.cloudzero.com/blog/grok-pricing/
- Slack Code channels: https://cryptobriefing.com/slack-vibe-coding-ai-agents/ ,
  https://claude.com/fr-fr/blog/claude-code-and-slack
- Public skill catalogs: https://skillselion.com/skills/erichowens/some_claude_skills/orchestrator ,
  https://vibeindex.ai/skills/yeachan-heo/oh-my-claudecode/ask-codex
