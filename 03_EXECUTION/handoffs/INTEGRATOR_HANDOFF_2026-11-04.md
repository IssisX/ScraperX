# Integrator handoff, 11-04 (kept current while the session runs)

From `session_01W3SSMz5zMw3Tm5DXRpisrG` (took over 10:20 AM Central from `session_01W3SSN2Y8JGNMvmxc394dkf`).
Session chain: 7 of 8. Compact at 400k (Cory runs /compact); a new session only if this one breaks.

## First, in this order (the coordination must not degrade)
1. Read `CLAUDE.md`, then steps K, D, S, C and 7 of `work/tasks/integrator-hourly.md`.
2. Run `python3 tools/board/asks.py --hours 24` and clear every line not covered under "Asks in flight" below.
3. Check Muse's mail at once: `subject:MUSE_TO_CLAUDE newer_than:3h`. Muse reads only mail. Every `To Muse:` post is
   mailed as `CLAUDE_TO_MUSE: <job or #N> ...` the same hour. Muse's diffs arrive as mail attachments; the Gmail
   tools return none, so a helper decodes them from the RAW message.
4. Move the hourly scheduled task (`trig_0139GQPXpF7YWbvHwgEn98a3`, cron `CRON_TZ=America/Chicago 17 0,3,7-23 * * *`,
   prompt: a pointer to `work/tasks/integrator-hourly.md`): create it bound to the new session, delete the old one.
   Re-arm the 20-minute `send_later` check-in and delete this session's pending one.
5. Re-subscribe only to the PRs you merge yourself (step 7). Archive this session once it says it is done.

## How the agents reach each other (unchanged; keep it this way)
- **Muse:** mail only (`CLAUDE_TO_MUSE:` / `MUSE_TO_CLAUDE:`). Its GitHub posts come through the bridge. If the bridge
  has not posted a Muse verdict, post it yourself (as done for #150). It runs parallel agents of its own; give it
  the hardest builds. Rule on its proposals within the hour.
- **Grok:** its 10 AM and 4 PM runs find only comments that START `To Grok:`. Use it for math and second opinions.
  Cory will not relay anything to it.
- **ChatGPT:** finds `To ChatGPT:` asks and signs its posts; research, specs; a PR reviewer only as the fallback.
- **Switchboard:** routes only PRs whose body starts `Job: J-NNN`. It merges on green plus a review. Codex counts as
  the review (#197). The Claude API (Opus, max effort once #200 lands) reviews only heads Codex has not reviewed.
  Grok and ChatGPT review only when neither can.
- **API coder (Managed Agents): off** (Cory, 11-04: "I just want the reviewer(s)"; `orchestrator.enabled: false`).
  Never cite its per-session reserve to Cory.
- **Claude helpers:** at most two active (three if acute), each in a worktree. Every brief says: never subscribe to a
  PR you open.

## Cory's rulings today (all in the repo)
Opus for API reviews, then max effort (#200); integrator effort ultracode, never workflows; reviews only, no API
coding; one review per PR; plain answers, technical only when he asks; watch growth and cut it.

## Rulings by the integrator today (`OWNER_DECISIONS_PENDING.md`)
- J-001 #193 (Muse's partial fix pushed at 5041d8e4): demand-based swat share; past-rail: near-miss world tweak if
  the 17.0 comes from world data, else re-pose; fatality pin archaeology first, fix the 17 gap ticks; Codex P2 last.
  CI at 5041d8e4: new mischief "no shove" failure, caused by the fix (A/B mailed).
- J-006 leg 6: fix the re-grab in routecraft (no re-taking the just-released edge unless her goal is above it,
  +0.3 m); if her target really is above, the test changes by a protected PR. Muse measures which.

## Asks in flight (update before handing off)
- Muse (mail): J-001 steps 1, 3, 4 (agents running); J-006 leg 6; J-038 plan (rule within the hour when it comes);
  #95 stills; three rebases; J-016 drum runaway plus the touch_matron_pipe drum moving 17.8 m (mailed 11:21 AM);
  S3 starts when #179 lands.
- Grok (4 PM run): #193 force split (posted as `To Grok:`); #100, #169, #137 from 11-03.
- ChatGPT: J-030 (#137), J-038 research (#169).
- To Claude: none open.

## Merge queue
#200 (max reviewer; Codex clean @1ee3b723, merge on green, then tell Cory it is live); #150 (J-025, Muse LAND,
re-proof @893a1837); switchboard merges #179 then #196 (J-033) and #201 (J-027 gap note); #43 (protected drawn-body
checks, sweep says MERGE, mergeable_state unstable: check, merge trunk in, re-proof). #198 (J-039 STOW/PUNCH): the
API session finished ($1.12); check whether it opened a PR.

## Known here
- The safety checker refused the integrator's merge of #179 (Muse's branch); let the switchboard merge such PRs.
- `.claude/worktrees/` goes in `.git/info/exclude` in a fresh clone.
