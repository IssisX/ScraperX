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
26 dead PRs (14 open). The integrator's merge permission (2b054282) works since 12:18 PM.

## Rulings by the integrator today
- J-001 #193 (Muse's partial fix at 5041d8e4): demand-based swat share; past-rail: near-miss world tweak if the
  17.0 comes from world data, else re-pose; fatality pin archaeology first, fix the 17 gap ticks; Codex P2 last.
  CI at 5041d8e4: new mischief "no shove" failure caused by the fix (A/B mailed to Muse).
- J-006 leg 6: fix the re-grab in routecraft (no re-taking the just-released edge unless her goal is above it,
  +0.3 m); if her target really is above, the test changes by a protected PR. Muse measures which.

## Asks in flight
- Muse (mail): J-001 step 4 hold re-pose (body x <= 16.9 at ArmPlay entry) and beat 5 hang to 2.5 s, both ruled
  1:10 PM, diffs on top of #193; step 1 running; Codex P2 last. J-038 ruled 12:55 PM (slice 0 next after the
  fight; slice 1 waits for Grok). J-006 leg 6; J-016 drum; #95 stills.
- Grok (4 PM run): #193 force split; #100 (a helper is fixing touch-2 meanwhile); J-038 dent formula on #169.
- ChatGPT: #171 next step (asked 1 PM); J-030 (#137).
- To Claude: none open.

## Merge queue (1:10 PM)
- Merge myself on green + Codex clean: #203 (never-edit files stay blocked), #205 (CI runs only reachable tests;
  touches the signing job).
- Switchboard: #204 (Supersedes #N closes #N), #206 (tests-probe), #207 (world-b; 2/2 commit coming), #150
  (STOW-replaces-JUMP fix coming via a J-039 PR merged into it).
- #193 proving at 9a4f8cad. Merged today: #179, #185, #196, #197, #199, #200, #201, #202. Closed: #43, #103.

## Known here
- `.claude/worktrees/` goes in `.git/info/exclude` in a fresh clone. Worktrees in use: proof-scope, merge-protected, j025.
