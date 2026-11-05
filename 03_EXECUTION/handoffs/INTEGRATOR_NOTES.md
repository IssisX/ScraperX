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
- **Claude helpers (lean, 1:30 PM, weekly limit at warning):** one at a time, Sonnet, in a worktree, until the limit resets. Every brief says: never subscribe to a
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
- Muse (mail): J-001 #193 - beat-5 hang in at 35b67484, proof red (mischief no-shove, past-rail setup, load S2
  over-base). 4:45 PM ruled ChatGPT's stability-first force share + 4-tick plant fallback; Muse owes one diff. #207 world-b red (E guard body order, matron crash
  hearing): Muse owes the fix. Drawn-body guard is #211 (J-012).
  5:10 PM rulings mailed: J-016 8afd23cd does NOT land (introduces stood_his red); Muse names the drum launcher
  first. Encounter return-walk slice afa461b3 held, lands with the drum fix. #95 stills (2b03b80c) received: look
  at them when eb32fc5f's plant fix is green.
- Grok: silent two runs; its asks rerouted 5:20 PM (#193 force split answered by ChatGPT and ruled; #169 dent
  cross-check asked of ChatGPT; Muse starts J-038 slice 1 on its own probe bands). Route math to ChatGPT until
  Grok answers again.
- To Claude: none open.

## Merge queue (5:20 PM)
- #209 (main copy of the Codex-receipt wake fix): the safety check refuses merges into main; Cory asked once
  at 3:13 PM. Don't re-ask. Trunk twin #210 merged.
- J-006: #212 merged 5:37 PM. Muse's fix rebuilt from the mail (whitespace restored, blob hashes match 470b2bac)
  onto #91. Ladder geometry landed alone as #213 (7:13 PM); trunk merged into #91 at 7299f8a8, `@codex review`
  posted; switchboard lands #91 on green + Codex. rule_check world-geometry BLOCK merged (#214).
- #95 (J-023 foot plant, workflow file): trunk merged at eb32fc5f, stills posted, proof dispatched. Merge myself
  on green + Codex clean.
- Merged today: #83, #100, #211, #212, #213, #214, #150, #171 (into #83), #179, #185, #196, #197, #199-#206, #208, #210. Closed #43, #103.
- Lean mode (weekly limit at warning, Cory silent = pick): one Sonnet helper at a time.

## Known here
- `.claude/worktrees/` goes in `.git/info/exclude` in a fresh clone. Worktrees in use: proof-scope, merge-protected, j025.
