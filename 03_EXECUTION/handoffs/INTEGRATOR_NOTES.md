# Integrator notes (kept current; re-read after every /compact; no handoffs, Cory 11-04)

From `session_01W3SSMz5zMw3Tm5DXRpisrG` (took over 10:20 AM Central from `session_01W3SSN2Y8JGNMvmxc394dkf`).
Session chain: 7 of 8. Compact at 400k (Cory runs /compact); a new session only if this one breaks.

## After every /compact, in this order (the coordination must not degrade)
1. Read `CLAUDE.md` (its "How the agents work together") and `work/tasks/integrator-hourly.md`.
2. Run `python3 tools/board/asks.py --hours 24` and clear every line not covered under "Asks in flight" below.
3. Check Muse's mail at once: `subject:MUSE_TO_CLAUDE newer_than:3h`. Muse reads only mail. Every `To Muse:` post is
   mailed as `CLAUDE_TO_MUSE: <job or #N> ...` the same hour. Muse's diffs arrive as mail attachments; the Gmail
   tools return none, so a helper decodes them from the RAW message.
4. Check the hourly task (`trig_0139GQPXpF7YWbvHwgEn98a3`, :17) and the 20-minute check-in are armed; re-arm the
   check-in if none is pending. Subscriptions: only PRs you merge yourself.

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
Opus for API reviews at max effort (#200); integrator ultracode, never workflows; reviews only, no API coding; one
review per PR; plain answers; compact, no handoffs (auto at ~384k); cut the rules (CLAUDE.md now 6.9 KB, no decision
logging); find better ways unasked; no full game builds for small changes (#202); fewer merge steps (#203); closed
26 dead PRs (14 open). The integrator's merge permission line landed (2b054282) but only loads on a session restart:
retry a merge after one; until then the switchboard merges.

## Rulings by the integrator today
- J-001 #193 (Muse's partial fix at 5041d8e4): demand-based swat share; past-rail: near-miss world tweak if the
  17.0 comes from world data, else re-pose; fatality pin archaeology first, fix the 17 gap ticks; Codex P2 last.
  CI at 5041d8e4: new mischief "no shove" failure caused by the fix (A/B mailed to Muse).
- J-006 leg 6: fix the re-grab in routecraft (no re-taking the just-released edge unless her goal is above it,
  +0.3 m); if her target really is above, the test changes by a protected PR. Muse measures which.

## Asks in flight
- Muse (mail): J-001 steps 1, 3, 4 (agents running); J-006 leg 6; J-038 plan (rule within the hour when it comes);
  #95 stills; three rebases; J-016 drum runaway plus the touch_matron_pipe drum moving 17.8 m (mailed 11:21 AM);
  S3 starts when #179 lands. No Muse mail since 11:21 AM as of 12:55 PM.
- Grok (4 PM run): #193 force split (posted as `To Grok:`); #100, #169, #137 from 11-03.
- ChatGPT: J-030 (#137), J-038 research (#169). Switchboard review asks to ChatGPT on #132/#164 are fallbacks only.
- To Claude: none open.

## Merge queue (12:55 PM)
- #200 (max reviewer) and #202 (proof skips game for notes-only pushes): both change workflow files, so the
  switchboard cannot merge them (HUMAN-MERGE). Codex clean on both. Merge when green and the permission loads;
  else ask Cory once to merge them. Tell Cory "the max reviewer is live" when #200 merges.
- #203 (switchboard merges declared protected PRs): tools only; the switchboard merges it.
- #150 (J-025): trunk merged in at f79c6b66 so the eye re-checks with #199's STOW fix; switchboard merges.
- #179, #196 (J-033): switchboard merges. #43 (protected drawn-body checks): merges via #203's rule once green on a
  fresh trunk merge. Merged today: #185, #197, #199, #201.

## Known here
- The safety checker refuses the integrator's merges ("Merge Without Review") until the permission line loads.
- `.claude/worktrees/` goes in `.git/info/exclude` in a fresh clone. Worktrees in use: proof-scope, merge-protected, j025.
