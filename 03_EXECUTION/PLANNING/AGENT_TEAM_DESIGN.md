# How the agents should work together: a design from scratch (v3)

Written 2026-11-02 at the owner's request: *"invent the best way for multiple intelligent AIs to work
together on hard code problems ... Do not tune my current setup; assume all of it can be replaced."*
Revised the same day after his corrections: *"Don't rule out anything ... Only Claude can see it
really. The other ones don't do it right."* and *"Linear needs more money, and that's not what I
want to do right now ... Can we do this without linear?"* Yes: v3 uses GitHub for the board and the
wakes, and his monthly Anthropic API credit for the thinking.

This is a proposal, not a rule. Nothing here is built. Section 9 is what to build this week.

## 0. The three ideas

1. **One brain, many hands.** Agents from different vendors cannot share a context window, so
   "thinking together" through a shared thread never happens; they take turns. The fix is to put the
   joint reasoning in one place: a hosted Claude *orchestrator* that holds the whole problem in one
   context and calls the other agents as tools, with the same evidence, and reads their answers back
   into that one context. The others are hands and second opinions; the orchestrator is the room
   where the thinking happens. It runs on the API credit with a hard dollar budget per job, not on
   the $100 plan.
2. **GitHub wakes everything; nobody pastes.** One GitHub issue per job, with labels. A small
   GitHub Actions workflow is the webhook receiver: when an issue is labeled, it starts an
   orchestrator session through the API with the issue as the message. Codex wakes on `@codex`
   mentions the orchestrator posts. Muse wakes by email the orchestrator sends, and a free bridge
   posts its replies back. The owner's phone does decisions, taste and money, nothing else.
3. **Claude is the only eye, so make the eye cheap and automatic.** CI already captures screenshots
   under xvfb. Every look-check becomes a fixed screenshot set that a Claude vision call judges for
   cents through the API, against a written rubric. Live Godot sessions on the plan are kept for
   scenes CI cannot capture yet, and each one becomes a CI capture the first time it is needed.

## 1. What was considered, and what it is for

Nothing ruled out. Each row says what it adds and what it costs; section 2 picks.

| Option | What it adds | Cost | Verdict |
|---|---|---|---|
| **GitHub issues plus Actions as the board and the wake switch** | one issue per job; labels as state; `issues.labeled`, `pull_request`, `workflow_run` and a cron as triggers; a workflow step starts an orchestrator session through the API | $0; Actions minutes free on a public repo, about $0.008 a Linux minute past the free minutes on a private one | **use** |
| **Anthropic hosted agents (Managed Agents)** | a Claude agent that Anthropic runs: its own sandbox, scheduled runs, session budgets that pause at a dollar cap, workflows that fan out sub-agents, a grader that iterates toward a rubric | $0.08 per running session-hour plus tokens at API rates (standard top tier $4 per million input, $20 per million output, cache reads about $0.20); paid from the monthly API credit | **use** as the orchestrator |
| **Claude GitHub Action on the plan** | a label wakes a stateless Claude run against the plan's caps, with the official `claude_code_oauth_token` input | $0 beyond the plan; one bug report of auth failing after a Pro-to-Max upgrade | **use** only for `plan-only` jobs; the API key is the default |
| **Codex through GitHub mentions** | `@codex review` on a PR runs Code Review reliably by every report; `@codex` on an issue to start a task is reported failing or ambiguous since August to October 2026 | included in the ChatGPT plan, with its review limit | **use** for reviews; test the issue task once; if it fails, Codex is reviewer-only and the orchestrator builds |
| **Codex through the OpenAI API** | the orchestrator calls Codex as a tool and is its hands | OpenAI API credit, per token; not in the plan | **later**, only if the owner adds OpenAI credit |
| **Linear as tracker and wake switch** | delegation wakes Codex and Devin natively; a good phone app | Free plan has no AI credits; the rest costs money | **not now** (owner, 11-02); GitHub does both jobs |
| **Devin** | a second wakeable builder with its own sandbox and GitHub app; Devin Review | $20 a month plus about $2.25 per ACU, about $9 per active hour | **not now**; money |
| **Grok through the xAI API** | a cheap second reviewer and hypothesis generator, called as a tool | about $2 per million input, $6 per million output (third-party listings; confirm at docs.x.ai) | **use** as a tool, $3 a day cap |
| **Muse by email** | the Matron specialist; a free Apps Script bridge turns its replies into issue comments without the owner approving each | $0 | **use**, off every critical path |
| **Slack Code channels, Claude Tag** | a channel on his phone where Claude and others can be tagged | Slack plan; bot-to-bot wakes unverified | **later**, for his view only |
| **Skills catalog** | searched the owner's catalog for multi-agent, orchestration, collaboration, swarm: nothing. Public catalogs have "orchestrator" and "agent-orchestration" skills, all Claude-only subagent fan-out, and an "ask-codex" skill that lets a Claude session consult Codex | free | **not the answer**; the orchestrator's tools do the cross-vendor part; `ask-codex` is reused as a tool |
| **Claude Code's own workflows and subagents** | fan-out inside one Claude session | tokens | **use inside the orchestrator** for parallel probes, never as the team |
| **A shared GitHub issue as the board** (today) | nothing the above lacks | free | **retire**; one issue per job instead |

## 2. The design

### 2.1 Parts and where each runs

| Part | Runs on | Pays with |
|---|---|---|
| **Board**: one GitHub issue per job; labels `job`, `hard`, `needs:*`, `owner:*`, `state:*`; `work/BOARD.md` generated for the owner's eye | GitHub | free |
| **Switch**: `switchboard.yml`, the webhook receiver and merger, no model in it | GitHub Actions | free |
| **Orchestrator**: one hosted Claude agent per job session, started by the switch through the API | Anthropic hosted agents | the monthly API credit, under a session budget |
| **Hands**: the orchestrator's own sandbox; Codex (GitHub mentions); Grok (API tool); Muse (email) | each vendor's cloud | each vendor's plan or API |
| **Proof**: the existing CI; a `probe` workflow for experiments; a `screens` job for the fixed screenshot set | GitHub Actions | free on a public repo |
| **Eye**: Claude vision calls on the screenshot set, by the orchestrator | API | cents per set |
| **Live eye**: a Claude Code cloud session with the Godot runtime, only for `eyes-live` jobs | the $100 plan | the plan's caps; at most one open |
| **Memory**: `work/CONTEXT.md` (200 lines), `work/LEDGER.md` (rulings), `work/jobs/<id>/` for a hard job's hypotheses and probes | the repo | free |

### 2.2 What the orchestrator is

One agent definition, versioned in the repo as files. Each job is its own session with its own
dollar budget; it starts from the issue and `work/CONTEXT.md`, never from a transcript, so no
session grows for weeks. Its tools:

- `read_issue`, `comment`, `set_label` on GitHub (a bot identity; the owner stays the assignee);
- `wake(agent, job)`: posts the `@codex` mention, or emails Muse, with the generated wake message;
- `ask(vendor, question, evidence)`: one call to Grok, Codex or Muse with a fixed evidence pack,
  the answer returned into the orchestrator's context;
- `run_probe(script)`: dispatches the probe workflow and waits for its result;
- `look(set)`: fetches a screenshot set from a CI run and judges it against the job's rubric;
- `git` and `gh` in its sandbox, for reading code, building, and opening PRs;
- `decide(question, options, pick)`: posts a three-line decision for the owner on the issue and
  starts the ten-minute clock.

### 2.3 How a job moves

1. **Born.** The owner, or any agent, opens an issue with the `job` label; the owner can do it with
   one line from his phone. The switch starts an orchestrator session. It writes the four fields on
   the issue: `invariant` (the finish fact), `files` (owned functions), `reach` (tests it reaches),
   `needs` (`build`, `android`, `eyes`, `eyes-live`, `security`, `matron`). An issue without an
   invariant stays `state:triage`.
2. **Assigned.** The orchestrator sets one `owner:*` label by rule: needs matched against
   `work/agents.yml`; Codex for C++ and GDScript builds when its task wake works, else itself; Muse
   for `matron` design; Grok never owns, only answers. The switch rejects a second `owner:*` label
   on an issue. Duplicate claims are impossible, not discouraged.
3. **Worked.** The hand works in its own sandbox on `<agent>/J-NNN` and posts one line per real
   step on the issue. The orchestrator reads the issue, never a history.
4. **Proved.** The PR says `Job: J-NNN`. CI runs on the branch; the `screens` job adds the fixed
   screenshot set when `needs` has `eyes`.
5. **Reviewed.** The switch asks a different vendor from the author (Codex reviews the
   orchestrator's and Muse's work; Grok, through the orchestrator, reviews Codex's; Claude reviews
   `security` only). The eye runs on the screenshot set and posts its judgment with the rubric line.
6. **Merged.** By the switch, no model, no person: proof green on the head, one `LAND` from a
   different vendor on that head, the eye's `LAND` when `eyes` is a need, no open `FIX`, no
   rule-check `BLOCK`. Protected changes the same way (owner, 10-28, 10-29). The owner can overturn
   any merge; the ledger records it.

The verdict form: `LAND`; `FIX` with a deterministic failure (command, input, expected, observed),
otherwise downgraded to `NOTE`; `NOTE` never blocks (owner, 10-31).

### 2.4 The eye

The owner's standard: *"Only Claude can see it really."* So the eye is Claude, and the design's job
is to make Claude's looking cheap, repeatable and off his plan:

- **A fixed screenshot set per scene**, captured by CI: same scenes, same frames, same resolution,
  the trunk's set beside the PR's. Today's `godot-audits-and-mix` job already does this for the
  approach and the summit; each `LOOK` job adds its scene once.
- **A rubric per set**, in words, in `work/looks/<scene>.md`: what must be true (her hand on the
  hold, no skirt through the rail, the camera framing the five beats).
- **The judgment**: one Claude vision call per set, from the orchestrator, through the API: about
  thirty thousand input tokens for twenty images, around ten cents at the standard top tier. It
  answers `LAND` or `FIX` with the rubric line and the frame.
- **Taste** (does this look right, not is it correct) goes to the owner as two images and one
  question. Nothing else does.
- **Live** looking (driving the Godot runtime, new scenes) stays on a Claude Code cloud session on
  the plan, at most one open, closed when the job closes. Each live scene becomes a CI capture the
  first time, so the live need shrinks weekly.

### 2.5 Thinking together: the modes

The orchestrator holds the room; the others are consulted into it. Bounded by construction: a
question to a vendor is one call with a fixed evidence pack and a three-hundred-word answer in the
form `CLAIM / EVIDENCE / NEXT PROBE / CHANGES MY MIND`.

**INVESTIGATE, the hypothesis tournament** (today's case: the phone's physics digests differ from
the host's).
1. The orchestrator writes the evidence pack and asks two or three vendors for hypotheses, each with
   a probe script; it writes its own too.
2. It dedupes them into `work/jobs/<id>/hypotheses.md` and runs every probe through the probe
   workflow (host, and Android when the job says so).
3. It reads the results, marks each hypothesis alive or dead, and asks again with the results.
   Three rounds at most.
4. The arbiter is the probe: the hypothesis that survived its own falsifier and whose repair makes
   the failing test pass becomes a REPAIR job. Two survivors after round three: the orchestrator
   picks and writes the one observation that would overturn the pick.

**IMPLEMENT, hard: spec, attack, race.**
1. The orchestrator writes the invariant and the test that proves it; a protected test goes as its
   own PR first.
2. It asks a different vendor to attack: every way the test could pass while the goal fails. The
   spec and test are tightened before any code exists.
3. Two hands from different vendors (the orchestrator and Codex) build on two branches; CI scores
   both against the same test. The orchestrator picks: the only green one; both green, the smaller
   diff with fewer protected changes; neither, back to step 1 with both failure reports.

**REPAIR with a known invariant:** one hand, one cross-vendor review, merge by rule.

**LOOK:** the eye in 2.4, and the owner for taste.

### 2.6 How disagreements end

In this order; the first rule that applies wins, and the ledger records which.

1. A CI result settles it: a test, a probe, the eye's rubric line.
2. A deterministic repro settles a review. `FIX` carries one or it is a `NOTE`.
3. No test can settle it: a design call. Three lines to the owner on the issue: the choice, what
   each word does, the pick and why. Ten minutes unanswered, the pick stands (owner, 10-27).
4. Taste: the owner, with the two candidates.
5. Rules conflict: the newer dated rule in `00_START_HERE.md` wins.

The orchestrator's own judgment is used once: the closing pick of a tournament. Nowhere else does a
model outrank evidence.

### 2.7 Budgets that cannot be blown

- **The API credit** is the orchestrator's whole purse: a monthly cap on the account at the credit
  amount, and a session budget per job (start at $8 for a hard job, $2 for a review). At a cap it
  pauses and says so; the owner raises a number with one comment on the issue: `budget 12`.
- **The plan** is touched only by live-eye sessions (at most one open) and by the Claude GitHub
  Action for `plan-only` jobs; `work/budget.yml` caps both per day and the switch enforces it.
- **Every run is stateless**: a session starts from the issue and `work/CONTEXT.md`, does one job,
  ends. No session reaches 550,000 tokens; no answer comes from memory.
- **Vendors** are called through tools with a per-day cap each in `work/budget.yml`.
- Every run's start, end, job and cost goes to `work/archive/runs.log`; after one week the budgets
  are set from the log, not from a guess.

## 3. What this removes outright

| Problem | Why it cannot happen |
|---|---|
| The owner is the router | a label starts the orchestrator; the orchestrator posts every wake; Muse's replies arrive by bridge; no agent in the loop needs a paste |
| History grows huge and gets re-read | no global thread; a job is one issue, closed at sixty comments after a ten-line summary; the orchestrator's memory is `work/`, not its transcript |
| Duplicate and raced claims | one `owner:*` label per issue, set by one orchestrator session, a second one rejected by the switch |
| Garbled relays | the wake is the issue itself; the evidence pack is generated; no human composes or retypes |
| Claude usage burned by many sessions | the joint reasoning runs on the API credit under a dollar cap; the plan holds at most one live-eye session, enforced by the switch |
| Reviews and looks bottleneck on Claude | cross-vendor review by rule; the eye is a ten-cent API call on a CI artifact, not a session |
| Side by side, never together | the orchestrator holds one context for the problem and consults the others into it; tournaments, attacks and races are bounded and evidence-judged |

What it does not remove: Muse's hours of latency (off every critical path), Codex's review limit
(Grok is the second reviewer), the chance that Codex's issue-task wake stays broken (then Codex
reviews and the orchestrator builds), and the owner's taste calls (by design).

## 4. Costs, honestly

Estimates; nothing is measured yet. The first week's `runs.log` replaces every number.

| Item | Per month, estimated | Note |
|---|---|---|
| Anthropic API | within the monthly credit if it is $100 | a hard job session about $5 to $8 (three to four hours running, a few million mostly cached input tokens, under two hundred thousand output); a review about $1; an eye judgment about $0.10. About twelve hard jobs a month fit. Set the account's monthly cap at the credit so it can never exceed it |
| The $100 plan | as now | spent on live-eye sessions and `plan-only` jobs instead of a standing manager |
| Grok API | $5 to $30 | capped at $3 a day; skip it entirely for the first week if money is tight; the orchestrator asks Codex instead |
| Codex | as now | the review limit stays |
| GitHub Actions | $0 public; $30 to $60 busy private | a proof is about 150 runner-minutes; probes and screens 5 to 30 minutes |
| Muse bridge, email | $0 | Apps Script on his Google account |
| **New money** | $0 to $30 | Grok only, and only if he wants it |
| **His phone** | about ten minutes a day | the 7:00 AM digest on one issue, decisions on the job issues, taste calls with two images |

The honest trade: the quota problem becomes a dollar problem capped at the credit. The plan's caps
stay the limit on live looking only.

## 5. What each message contains

- **Wake**: the issue with the four fields; for Codex an `@codex` mention linking it; for Muse an
  email carrying it.
- **Evidence pack (orchestrator to a vendor)**: the invariant, the failing output, the diff that
  last touched the files, the probe results so far; under four thousand words, generated.
- **Answer (vendor to orchestrator)**: `CLAIM / EVIDENCE / NEXT PROBE / CHANGES MY MIND`, three
  hundred words.
- **Progress (hand to issue)**: one line per real step, `HH:MM what`, Central.
- **Verdict (reviewer or eye to PR)**: `LAND`, `FIX` plus repro, or `NOTE`.
- **Decision (orchestrator to owner)**: three lines and a word to answer with.
- **Digest (switch to owner, 7:00 AM Central, on the Digest issue)**: landed, red, waiting on him,
  paused by budget, sessions open.

## 6. What the owner gives up

Reading a live board for comfort (`work/BOARD.md` is generated), the feeling of a manager on call
(the manager is a hosted agent that wakes on labels), and the idea that a shared thread is
collaboration.

## 7. Where the earlier versions were wrong

- v1 made the coordinator a GitHub Actions script and kept the joint reasoning in files that agents
  take turns reading. That removes the router but never gets two models into one context.
- v1 let any vision model judge screenshots. The owner's experience says no; the eye is Claude only.
- v2 used Linear as the board and the wake switch. It costs money the owner does not want to spend
  now; GitHub does both jobs at no cost, with one loss: Codex's task wake depends on a GitHub
  mention that is reported unreliable, where Linear's delegation is documented.

## 8. Open checks before building

Each is one lookup; none blocks step A.
- The monthly API credit: confirm the amount and the reset day in the Anthropic Console, and set
  the account's monthly cap to it.
- Hosted agents: confirm a session can be started from a GitHub Actions step with the API key
  (one `sessions.create` call), and that the session budget pauses as documented.
- `@codex` on an issue: test once; it decides whether Codex builds or only reviews.
- The Claude GitHub Action on the plan: one report of auth failing after a Pro-to-Max upgrade; test
  once with a trivial job before relying on it.
- Grok's current API price at docs.x.ai, only if Grok is added.

## 9. The first step this week

**Step A, today, no code and no money: the board moves to one issue per job.**
1. One GitHub issue per open job from #87's body, with the four fields in the body and the labels
   `job`, `hard` where it applies, `needs:*`, `owner:*`, `state:*`. This session can do it.
2. #87 retired as a board: one comment points to the `job` label. `work/BOARD.md` generated once.
3. `@codex` posted on one small job's issue to test the task wake; the result decides Codex's role.

**Step B, this week, one PR, one Claude Code session closed when it lands: the switch, the eye and
the bridge.**
1. `work/CONTEXT.md`, `work/agents.yml`, `work/budget.yml`, `work/looks/` with the rubric for the
   two scenes CI already captures.
2. `.github/workflows/switchboard.yml`: on `issues.labeled` starts an orchestrator session through
   the API (one call, the key in a repository secret); on proof completion and reviews, the merge
   rule in 2.3 step 6, the cross-vendor review request, the eye's judgment on the screenshot
   artifact, and the 7:00 AM Central digest on a Digest issue.
3. `tools/switchboard/muse_bridge.gs`: the Apps Script for Muse's replies.
4. The hourly scheduled task and the standing integrator session retired once the switch has run
   green twice.

**Step C, next week, the orchestrator**: the hosted agent definition as files, with the `ask`,
`run_probe` and `look` tools, run first on the determinism question as a tournament, under an $8
budget. That is the first time the team thinks together.

Owner's one-time setup, about fifteen minutes: the API key as a repository secret with the monthly
cap set; a Gmail app password secret for the bridge's outgoing mail; the Apps Script pasted with a
GitHub token.

## Sources checked on 2026-11-02

- Codex GitHub mentions, reported state: https://github.com/openai/codex/issues/54382 ,
  https://github.com/openai/codex/issues/34425 , https://github.com/openai/codex/issues/42098
- Hosted agents pricing and workflows (third-party): https://www.truefoundry.com/blog/claude-managed-agents-pricing ,
  https://imisofts.com/blog/claude-managed-agents-pricing/ ,
  https://alphasignal.ai/news/anthropic-s-managed-agents-gets-budget-caps-geo-pinning-and-smarter-advisor
- Claude GitHub Action on a subscription: https://code.claude.com/docs/en/github-actions ,
  https://github.com/anthropics/claude-code-action/issues/1281
- Linear agents (considered, not used): https://linear.app/developers/agents ,
  https://linear.app/legal/ai-addendum
- Devin pricing (third-party): https://www.eesel.ai/blog/cognition-ai-pricing
- Grok API pricing (third-party): https://www.morphllm.com/grok-api-pricing
- Slack Code channels: https://cryptobriefing.com/slack-vibe-coding-ai-agents/
- Public skill catalogs: https://skillselion.com/skills/erichowens/some_claude_skills/orchestrator ,
  https://vibeindex.ai/skills/yeachan-heo/oh-my-claudecode/ask-codex
