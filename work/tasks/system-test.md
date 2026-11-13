# System test: every collaboration path, driven and checked (J-072, 11-13)

One real-world run that decides "system done". Each check: the real action, what must be seen, then
PASS / FAIL / PENDING with evidence (run id, comment link). Test branches and issues are named
`systest`; no game code, no real job is touched. A FAIL gets its root fixed on its own
`claude/integrate-<name>` branch (Job: J-072) and only that check re-runs.

Sweep = a live "Switchboard sweep" run (ask, then steer). Inbox = #282. Board = the open `board` issue (#384).

| # | Action | Expected, observable | Piece |
|---|--------|----------------------|-------|
| 1 | Muse sends an ASK (git channel once #507 lands; mail before) | one TO-CLAUDE line on #282 within one sweep, at once (wakes) | trunk, #507 |
| 2 | Muse sends an FYI | never a #282 post alone; rides as a `digest` line on the next waking post | #498, #507 |
| 3 | A red proof on a `claude/integrate-systest-red` branch with no PR | RED-BRANCH once per head (Claude's: #282; Muse's: CLAUDE_TO_MUSE on the board) after INBOX_BRANCH_RED | trunk |
| 4 | Trunk moves proof-read files under an open green PR with a LAND | the switch builds `switchboard/candidate-<pr>` (head + trunk), dispatches the proof, merges only when that is green | #502/#505 |
| 5 | Post `To Muse:` on a systest issue | relayed to Muse exactly once (one CLAUDE_TO_MUSE / one git-channel message), never again on later sweeps | #507 |
| 6 | Plan version for Grok changes | one `To Grok: plan <ver>` notice on the board with its marker, not re-posted next sweep | #502 |
| 7 | A job's owner asks and waits | `state:waiting` label on the job issue and "waiting on X's answer ([ask])" on its board line | #502 |
| 8 | A full sweep | its `GITHUB` line under the hourly budget, `CALLS` line present, GraphQL query counted | J-072 rate-hourly |
| 9 | A save-layout / budget-list change on a systest branch | `rule_check.py base head` prints BLOCK (protected) | J-139 |
| 10 | Commits pushed on a branch whose PR merged | the switch opens a new PR for the new head (not "head already had a PR") | J-072 rate-hourly |
| 11 | Any sweep with only digest kinds | no #282 comment that holds only the digest | #498 |
| 12 | A new `claude/integrate-*` branch pushed | the switch opens its PR within one sweep (auto-PR) | #500 |
| 13 | Per-job clocks, lettered parts | `PLAN-LATE` counts from each job's own start; J-NNNa lines read as their own rows | #506 |
| 14 | Cache-out on a refused listing | cache-out job green, run not failed | #503 |

## Results

- 9 PASS (J-139 head ba3f7fca, local-only systest commit, never pushed): a swapped `w.pod` pair in
  `PhysicsWorld::write_save` and matron-grab GrabSpring peak 2600 -> 2900 N gave `BLOCK save-layout-changed` and
  `BLOCK budget-audit-loosened`, exit 2. Control: trunk's rule_check on the same commits, 0 BLOCK (the gap J-139 closes).
- 6 PASS so far (dup check pending): sweep 43453938239 steer posted `To Grok: plan 4e44e6ea416b` once, comment
  6858074624 on #384, with its marker; Builder's 6858073786 likewise.
- 7 PASS (observed live, not driven): J-001 waits on Grok's ask (#108 comment 6774721075); #108 carries
  `state:waiting`; board #384 line "J-001 #108 held; waiting on Grok's answer ([ask](...), 3 d)".
- 11 PASS so far: on #282 since #498 landed only waking posts (6856712417, 6858117862; the latter carries the digest
  line under RED-TWICE/FIX/QUEUE-LOW). The digest-only 6854673147 predates the fix.
- 3 PASS (Muse's branches, live): steer of 43453938239 sent RED-BRANCH once per head for fb-bot/integrate-j080,
  -j118, -j124, -protected-j040, -j044c-gains2 as CLAUDE_TO_MUSE on #384 with failing tests read.
- 4 PENDING: candidate built live: `switchboard/candidate-501` = a6d46023 (parents #501 head b0c06aae + trunk
  1956137b), proof 43457224724 dispatched, queued; the merge waits on that proof.
- 8 PENDING (trunk, before J-072 rate-hourly): ask job 30/30 of its run budget (1 read refused), steer 912/1000,
  4923 left this hour; no GraphQL line yet (that PR is not open).
