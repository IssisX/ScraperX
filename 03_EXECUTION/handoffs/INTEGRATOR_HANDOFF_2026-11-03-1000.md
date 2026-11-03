# Integrator handoff, 11-03, 9:50 AM Central

From `session_01W3SSMyS2bn5QXud1gxTNsE` (514k context, $20.57 in its first hour) to a fresh integrator.
Read `CLAUDE.md` first: it now holds today's standing orders. Then this file. Nothing else unless an
event needs it.

## Cory's standing orders today (all in CLAUDE.md)
- **Until he says "stop": go with your recommendation on every call at once.** Record the call in
  `OWNER_DECISIONS_PENDING.md` and tell him in one line. Still his: money above `work/budget.yml`'s caps,
  and Claude sessions beyond the two workers running now.
- **Never ask him to merge, approve or review.** The switchboard merges on green proof plus a LAND from
  another vendor. You merge protected PRs (`<agent>/integrate-protected-*` branches, live since #160) and
  workflow PRs. This session's safety checker refuses CI-gate changes and branch deletion: don't try
  them, and don't ask him.
- **Hard problems are solved together.** Pair a second-opinion agent from another vendor on any job at
  FIX-CI twice or BLOCKED, and relay `To Muse:` posts by email.
- **Session size:** hand off at 400k context. Subscribe only to PRs you opened or must merge yourself.

## What works (proven today)
- API key, provisioning (ids in `work/budget.yml`), and the switchboard starting the hosted API agent:
  session `sesn_019GBFzJ45H1QkXckaHCXYfy` on #136 read the repo and posted. Its git push is not proven
  yet. It still reserves $8 against the $10 day cap until #159 lands.
- The Claude API reviews and the eye run on their own (#104 reviewed and merged by the switchboard; the
  eye ran on #150).
- ChatGPT reviews when given the rule-check and CI facts. #161 puts those facts in every ask.

## Open, and who has it
- **#159** (Sonnet, J-031): watches sessions, records actual cost, releases a failed start, reconciles
  an unknown start. ChatGPT FIXed it; fixed at `2a37010`; needs a fresh verdict, which the switchboard
  asks for.
- **#161** (Sonnet, J-031): reviewer fallback (ChatGPT, then Grok) plus facts in every ask. ChatGPT LAND
  at `64e9f160`; the switchboard merges it.
- **Sonnet session `session_01W3SSMyYRKpViLbG64k13Wt`:** now tracing #152's first failing tick of
  `matron load S2 over-base` for ChatGPT (local logging only); posts on #152 as `To ChatGPT:`.
  Retire it once #159 and #161 have merged and the trace is posted.
- **Opus session `session_01W3SSN34J1RJhqKQTmZtAfE`:** J-032 (#157). Ruling: a tuck vault and
  step-off over the pipe rail (conditions on #157). Retire it when its PR lands.
- **#152** (ChatGPT, J-001): red on two checks that predate it (`matron load S2 over-base`,
  `matron crash ... warier`). The swat sign fix is in (`50e1bf49`). Grok was asked for a second
  opinion; ChatGPT's diagnosis is on #152.
- **#162** (Muse, J-002 grab check, protected): CI running on `aae852fe`. Merge it yourself on green
  proof plus a LAND. Muse still owes the late-require diff (add it to this branch), then #153 can go green.
- **#103** (J-001 protected checks): restack onto `chatgpt/integrate-protected-j001-<words>` when
  ChatGPT gets there.
- **#141** (lane B steel): ChatGPT LAND; the switchboard merges it after a fresh proof (its head is
  behind trunk). #150 got an eye FIX: HUD labels washed out over bright snow and the crane, probably
  older than #150; triage it.
- **Muse:** leads the fight S2-S5 end to end, then environment lane D (`work/tasks/muse.md`, email of
  9:28 AM). Apply its diffs on `fb-bot/integrate-<job>` or `fb-bot/integrate-protected-<job>`; open
  a job issue for each S<n> scope it sends.
- **Next API-agent job:** tomorrow, or as soon as #159 lands and frees today's reserve. Pick a
  bounded job; one at a time.
- **55 dead `fb-bot/*` branches:** this session can't delete them. The list with shas is on #137.

## Moving in
1. `get_trigger trig_017HojpFWYmBDYdXPU8T62WT`; create the same prompt and cron bound to you; delete the old one.
2. Subscribe to #162 only (you must merge it), plus any PR you open.
3. Archive `session_01W3SSMyS2bn5QXud1gxTNsE`.
4. First message to Cory: one line saying you took over, then `🟣 **YOUR MOVE:** Nothing to do.`
