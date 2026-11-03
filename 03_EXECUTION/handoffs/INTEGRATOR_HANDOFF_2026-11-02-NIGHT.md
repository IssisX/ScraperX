# Integrator handoff, 11-02 night (9:25 PM Central)

From `session_01W3SSMzZ5J6den1ZDWvNmu1` (its memory grew too large) to a fresh integrator. Read
`CLAUDE.md` first; then this; then `03_EXECUTION/handoffs/OWNER_DECISIONS_PENDING.md` top section
(today's calls). Cory, 9:20 PM: *"lets be more efficient through the night - were using too much
quota"*. So tonight: no new Claude sessions, act only on events, keep posts few.

## Where state lives now
- **Every job is its own GitHub issue**: label `job`, J-001..J-031 = #108..#138, with `owner:*`,
  `state:*`, `needs:*` labels and the four fields in the body. `work/BOARD.md` is generated from
  them (`python3 tools/board/gen_board.py > work/BOARD.md`). Agents post progress on their job's
  issue.
- **#107** "Board: live jobs (2)" holds rulings only (#87 and #3 are closed records).
- PE and Builder specs: `03_EXECUTION/PLANNING/PE_BUILDER_JOBS.md`.
- Fable's design (adopted step by step): `03_EXECUTION/PLANNING/AGENT_TEAM_DESIGN.md` (v3, on
  branch `ccr-8d35fbc3-j1613y`). Step A (one issue per job) is done; step B is PR #139.

## Rules learned today (all in OWNER_DECISIONS_PENDING.md)
- Check every fact before saying it; never from memory. Cory is on a phone: short, plain, one
  `🟣 YOUR MOVE` line; never repeat what he knows; make his part frictionless.
- **No Claude session opens without his clear yes.** A new session starts at medium effort and only
  he can change it: tell him the level to tap in the same message. The integrator can't set effort.
- Codex is out of quota: no `@codex`, never plan around it. Reviews: ChatGPT (through his app; give
  him one copyable prompt) or Muse (email). Claude reviews only security, with his yes.
- Merge on a cross-vendor LAND + green, automatically (his 3:20 PM yes): `gh api -X PUT
  repos/IssisX/ScraperX/pulls/N/merge -f merge_method=merge -f sha=<full sha>`. A conflicted PR:
  merge trunk into its branch (no rebase), re-prove. A FIX without a deterministic repro is a NOTE;
  test a doubtful one yourself (example: the `FRAGCOORD` FIX on #93, refuted by a Godot run).
- Usage: when he gives a 5-hour or weekly %, give the pace and a recommendation on every lever. Weekly
  resets Monday 11 AM; heavy days are made up with light ones.

## Claude sessions open
- **hold 2** `session_01W3SSN3RF5jqAH5nig7JNPb` (J-001 #108): Cory said **collar** at 9:20 PM (she
  grabs his collar, not his ankle; the fatality runs 5.0–6.9 s everywhere). It pushes #102
  (behavior) and #103 (protected checks), posts stills, reports, and stops. **Archive it when it
  reports.** #102/#103 then need ChatGPT's review. #103 carries BLOCK lines (its check changes), so
  the CI gate stops its own run; its proof is #102's run plus #103's local results.

## Waiting on Cory
- **switchboard fix**: the security review's FIX on #139 (an outsider's LAND comment counts on this
  public repo; fix: count only allowlisted logins). Asked "fix" = reopen the switchboard session
  `session_01W3SSN24QgH9v1ojdMWcY1w` (archived; it knows the code). Not answered yet.
- **the ChatGPT review prompt** (#143, #144, #141, #142, #140, #139): not yet pasted as far as
  anyone can tell (no reviews landed). Re-offer it once, in the morning.
- After #139 merges: a one-file PR into `main` with `switchboard.yml` (triggers fire from the
  default branch); his two setup steps are at the top of #139. Retire nothing until the switch has
  run green twice.

## Open PRs (9:25 PM)
- Waiting on ChatGPT's review: #140 (crash from above, protected), #141 + #142 (lane B), #143 + #144
  (lane A; #144 stacked on #143), #100 (drawn-body checks; changes a workflow file, so the workflow
  token can't merge it, but the integrator's `gh api` merge can), #98 + #99 (PE 1), #104 (PE 3).
- In work: #91 (ChatGPT, grade ladder: `step_journey()` path found), #97 (ChatGPT), #101 (PE 2),
  #89 (Grok, skirt as bone-driven panels), #92 (Muse, silent second hit), #95 (Muse, stills),
  #105/#106 (Grok, lane C), determinism (Grok, J-009: waits on #140, #97, J-008 and the hold PRs).
- Landed today: #45, #88, #93, #94, #96 (main), #86 (closed #66, #67, #71).

## The hourly check
`trig_01Ngj8bEZCwem5DXZtxxUi5D`: move it to the new session (create one with the same prompt bound
to it, then delete this one), and move PR subscriptions (#91, #92, #95, #98-#106, #139-#144).
