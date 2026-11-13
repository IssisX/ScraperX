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
- 7 PASS (observed, not driven): J-001 waits on Grok's ask (#108 comment 6774721075); #108 carries
  `state:waiting`; board #384 line "J-001 #108 held; waiting on Grok's answer ([ask](...), 3 d)".
- 11 PASS so far: on #282 since #498 landed only waking posts (6856712417, 6858117862; the latter carries the digest
  line under RED-TWICE/FIX/QUEUE-LOW). The digest-only 6854673147 predates the fix.
- 3 PASS (observed, Muse's branches; Claude's branch driven, PENDING): steer of 43453938239 sent RED-BRANCH once per head for fb-bot/integrate-j080,
  -j118, -j124, -protected-j040, -j044c-gains2 as CLAUDE_TO_MUSE on #384 with failing tests read.
- 4 PENDING: candidate built live: `switchboard/candidate-501` = a6d46023 (parents #501 head b0c06aae + trunk
  1956137b), proof 43457224724 dispatched, queued; the merge waits on that proof.
- 8 PENDING (trunk, before J-072 rate-hourly): ask job 30/30 of its run budget (1 read refused), steer 912/1000,
  4923 left this hour; no GraphQL line yet (that PR is not open).
- Driven 16:14Z: `claude/integrate-systest-red` @8377c03d (a failing test class added to test_rule_check.py only,
  never to merge) -> proof 43460678681; expect RED-BRANCH on #282 an hour after it goes red (3). Thread #509 made
  for the To Muse relay (5), posted once #507 is live (its first run looks back 30 min only).
- Seen: a claude/integrate-* head with no `Job:` line is skipped in silence every sweep (no inbox line); this
  branch's first head had none, so this commit carries one (12 tests the auto-open on it).
- Seen 16:15Z: runs queue long: sweep 43459528814 cache-in queued 7+ min, candidate-501 proof queued 25+ min,
  5 trunk proofs queued, with only 8 jobs running; stale queued runs 37642339310 (10-07), 43098147869 (11-11).

- 4 PASS (observed): sweep 43459528814 steer: `CANDIDATE #501 green: switchboard/candidate-501 @ a6d46023 ... is
  green (43457224724)`, then `MERGE #501 ... LAND by Codex`; #501 merged 16:45:37Z as a35a3a21. Kept, not rebuilt,
  across a notes-only trunk move.
- 8 FAIL (trunk code): that fast sweep's steer spent its 90-call run budget on 15 changed PRs' facts, then
  `RATE LIMIT` left auto-open, inbox, muse, queue check and pipe health for later, with 4894 of 5000 left this hour
  (`GITHUB 91 call(s) ... 14 read(s) refused`). Root: a fixed per-run cap, PR facts read first over REST. The fix
  in flight is the J-072 rate-hourly branch (hourly pace + one GraphQL read of the changed PRs); its own PR is not
  open yet because this same starvation deferred the auto-open to the next full pass. Re-run 8 once it lands.
- 6 one notice still (6858074624); no full pass since, so not yet re-checked by a plan pass.

## Re-run recipes for what is still PENDING (anyone can run these)
Logs: `gh api repos/IssisX/ScraperX/actions/jobs/<steer or ask job id>/logs`, grep the tag named.
- 1, 2, 5 (after #507 lands, within 30 min of its first muse-git run or later): on #509 post a team-login comment
  opening `To Muse:` asking for one ASK and one FYI message naming #509. Expect one switchboard comment on #509
  with `muse-git:relayed id=<that comment>` and no second one on later runs (5); then the ASK as one TO-CLAUDE
  line on #282 at once (1) and the FYI only as a `digest` entry riding a later waking #282 post (2).
- 3: steer of the first sweep after the red proof is 1 h old (43460678681 red 16:29Z, so after 17:29Z): grep
  `RED-BRANCH` and `claude/integrate-systest-red`; expect one line on #282, none on later sweeps (same head).
- 4: `CANDIDATE   #501` in each steer. Trunk moved notes only since 1956137b, so the candidate is kept (not
  rebuilt); proof 43457224724 green -> `#501` merges; red -> a RED for the owner and no merge.
- 6: bot comments on #384 starting `To Grok: plan 4e44e6ea416b` stay exactly one (6858074624).
- 8, 10 (after the J-072 rate-hourly PR merges): a full sweep's `GITHUB` line names `N GraphQL` and stays under
  `HOUR` pace; for 10, once the system-test PR merges, push one more commit with `Job: J-072` here: expect a NEW
  PR for it and the log line `commit(s) past merged #N`.
- 12: steer: `claude/integrate-system-test`, `-rate-hourly`, `-rule-check-protected` each get a PR (green, Job
  line); `-systest-red` gets none (no Job line, red).
- 13 (after #506): `PLAN-LATE` hours count from each job's own start; a J-NNNa line is its own row in PLAN-STATE.
- 14 (after #503): cache-out green on a sweep whose listing is refused (the 403 on the cache listing).

## Holes seen (not this test's pieces)
- Board #384 holds 206 comments, past CLAUDE.md's 100 cap; nothing rolls it to a fresh board.
- One check costs one sweep, about 35 min end to end (ask 14 min, steer 10, then cache-out), and runs queue
  25+ min with 8 jobs running, so each path's reaction time is slower than the 10 min bridge clock implies.
- Cleanup owed when done: branch `claude/integrate-systest-red` (a helper's delete is refused), issue #509.
