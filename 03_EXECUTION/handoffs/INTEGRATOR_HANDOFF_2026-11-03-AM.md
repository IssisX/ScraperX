# Integrator handoff, 11-03 morning (9:45 AM Central)

From `session_01W3SSMy7jX4DQFaC8yJ3i4R` (context 689k of 1M) to a fresh integrator. Read `CLAUDE.md`,
then this. Cory is on a phone, money is tight (he loaded more ChatGPT credit today), and he wants
everything as autonomous and quick as possible. Answer questions only; don't act on a question.

## What runs by itself now
- **Switchboard** (`.github/workflows/switchboard.yml`, live on `main` since #145/#148/#149/#155/#156;
  code on trunk `tools/switchboard/`). Merges on green + a LAND from another vendor; asks ChatGPT for
  Claude PRs; Claude API reviews non-Claude PRs; daily Digest on the issue labeled `digest` (#154,
  renamable). First automatic merge: #147.
- **Agents read their instructions from `work/tasks/*.md`** (ChatGPT review + features, Grok answers,
  Grok Bot engineer, Muse, the API agent). Cory's tasks hold one-line pointers. A rule change is a
  repo edit, never a task edit. The board is the one open issue labeled `board` (#107).
- **Muse works by email only** (each GitHub action of Muse needs Cory's tap). Its mail carries
  diffs; the integrator puts them on `fb-bot/integrate-<job>` and opens the PR. Muse bridge (Apps
  Script) posts `MUSE_TO_CLAUDE:` mail to GitHub every 5 min.
- **Grok app task** (10 AM, 4 PM) answers "To Grok"; it can post on GitHub. **Grok Bot / PE / Builder
  are out of usage until Monday 11-09**; its engineer task is paused until then.

## Broken, being fixed
- **`ANTHROPIC_API_KEY` secret holds the Muse bridge script, not a key** (LocalProtocolError in run
  of 14:40 UTC). So no Claude API review or eye has ever run, and provisioning failed. Cory is having
  ChatGPT agent mode create key `scraperx-switchboard-2` and replace the secret. When he says "key
  done": dispatch `gh api -X POST repos/IssisX/ScraperX/actions/workflows/switchboard.yml/dispatches -f
  ref=main -F 'inputs[provision]=true'`, read the provision job log for the ids JSON, put
  `agent_id`, `environment_id`, `vault_id` into `work/budget.yml` `orchestrator`, push to trunk.
  `provision_agent.py` now checks both secrets first (sk-ant- shape; AGENT_GITHUB_TOKEN push access).
- **GitHub has never fired the switchboard's `schedule`** (0 schedule runs). The hourly scheduled
  task dispatches a live sweep each check until it does (step 0 in its prompt).

## The API agent (step C, built today)
Managed Agents, `claude-opus-5-5` effort high (Cory's call), $8 hard budget per session, day cap
$10 (keep it; money is tight), month $100. Starts when a job issue has `job` + `needs:orchestrator`.
Mounts the repo, follows `work/tasks/api-agent.md`, pushes `claude/integrate-<job>`, opens the PR.
**First job to label once provisioned:** the native defect from #135 ruling A (her pipe-rail hand up
to 1.17 m past her reach for 2-5 s in `pipe_rail_hold()`; fix in Mount/Dismount), plus B outside
holds (arms drawn past bones while Hunting/clinging/Shuffling). Make it a new job issue first.

## Open PRs and who has them
- Waiting on review (ChatGPT task or Claude API once the key works): #150 (steel + tower face), #151
  (motion probe), #153 (Muse, silent second hit; supersedes #92), #152 (ChatGPT, J-001 swat budget;
  supersedes #102), #141-#144 (env lanes A/B), #99, #104, #105, #106, #100, #95, #98.
- Protected PRs can't pass CI's rule-check gate (it stops host-build on BLOCK). Cory's call pending:
  "approve protected path" (a `*/integrate-protected-*` branch runs its tests; I wrote it but the
  safety classifier stopped me without his explicit OK) or "manual". #103 waits on it. #140 was
  merged this morning by the old 7:30 AM routine (now off).
- J-001 (#108) is ChatGPT's now. #102/#103 superseded by #152 once it lands; #103's protected checks
  get rebuilt on #152.

## Cory's standing calls today
Opus for the API agent; daily cap $10; archive finished sessions, keep the 5 most recent; the Digest
is his one daily view; reviews only cross-vendor; no Claude session without his clear yes and his
effort tap; Muse email-only; Grok gets hard problems only.

## Sessions
Claude sessions open: only the integrator. Old 7:30 AM "issue #3 review" routine: off (Cory).
Hourly check: `trig_01TZcCC4WFq73YsDkrsUxDG9` (bound to the old session; move it).
PR subscriptions to move: #91, #92, #95, #97, #98-#106, #141-#144, #150-#153.

## Muse's branch count
Muse said 25 unlanded branches. True in spirit: 42 of 63 `fb-bot/*` branches hold commits not on
trunk, nearly all from 10-29 to 11-01 experiments, most superseded. J-030 (#137, claim or close old
work) covers the cleanup; deleting branches needs Cory's yes.
