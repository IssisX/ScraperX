# Integrator notes

Only what has no other home. A PR's state is on GitHub, a job's on its issue, a ruling in its spec or issue, Cory's
calls in CLAUDE.md's settled list, Muse's orders in `work/tasks/muse.md`; never also here. At most 30 lines. Older: `INTEGRATOR_NOTES_LOG_2026-11-07.md`.

Read through: 1794460539 (2026-11-11 last quiet run)

## Now (11-11)
State is in job issues and Muse's queue (`queue_check.py`, floor 8, refill 15). Three helpers, the cap (Cory 11-11):
- J-001 #108: hold @ 8e5c715e, protected j093 @ aaed1791; 1 left (snag setup): ruled her pin holds him over the drop,
  plus a kicking touch check: xhigh helper.
- J-096 #414 planks slice 1 (trunk merged 22c88c85, proof rerunning); my eye check before it lands. J-084 #402
  whole climb PASS (trunk merged c35a6062); touch-2 900 s limit is a workflow change. J-082 #400 STOPPED by Cory.
- J-090 #408 hook her pass 1 (trunk merged 708b3c3b); pass 2 after J-001.
  Helpers: J-092 #434 red-proof fix a50537520df752690 (body order vs magnet guard). J-108 bait lift: Cory said move it (11-12); new helper resumed it. J-001: Fable helper (11-12). J-043 cause found (beam), Muse builds. J-107 #426 taken from Muse (helper, high): the proof of the new split. Next free helper slot: count Muse's failed tries by job type over two weeks, then write the routing checklist every new job is scored against (Cory 11-12). After it: switchboard hole, a clean Codex receipt (codex-review-receipt, 'Reviewed commit') with no fresh state comment is not counted as LAND (#343 sat 24 h; rules.py parse_codex_state).
  J-092 landed #434; its CI follow-up #447 (touch_slab_trap in touch-2 + rule_check tests) has green proof, Codex clean, API LAND. Next workflow PR: `persist-credentials: false` on the scope checkout (API note on #447; token is contents:read, low risk).
- J-085 #403 DONE (#435, #437). J-126 #449 DONE (#450); follow-up J-127 #451 mailed to Muse. J-126 helper worktree build (~3 GB) left: its deletes were refused, not mine to do.
  (pipe from deck 1, tops out over the deck 6 housing, climbs in every stage, her other skills linked too).
- Argon (Gemini): only via Cory pasting a prompt; hardest physics. Grok or ChatGPT run an idea workflow on request from a copyable prompt in chat (11-11). 367 archived branches: deleting waits on Cory.

- 11-12 in flight: J-108 (re-pin + proofs; then I open its 2 PRs); J-001 grip-charge helper; J-128 helper; switchboard helper (4 stall ways: receipt-only LAND #343/#453, unanswered Codex ask #389 -> fall back to API review, unresolved finding with no owner step #396 -> Muse mailed, 502s). J-107 cause found (J-054 inserts stock mid-table; Muse builds; #454). Finish plan ON TRUNK with my 6 answers (Cory handed them to me 11-12); Muse queue = finish list + J-129..J-134 (#455-#460); J-001: no cut date, lands in green slices; next fail -> Argon ask (it had Fable already). Its open check: is her stage lost on relaunch (F8). Helper types are model: inherit; pass model opus if this session ever runs on another model.

## Traps
- My hand merges and branch deletes are refused by the safety check; never work around it.
- Gmail is Cory's own main inbox: his filter puts agent mail in a side label (11-11); search only the agent subjects; never read, trash, archive or touch any mail there, agent mail included (11-07, 11-11: "do not ever touch my main inbox"; the connector cannot trash anyway, and a prompt per mail is not acceptable to him).
- Never fix/fixes/close/resolve before #N in a commit: GitHub closes the issue.
- Worktrees: `rm` their untracked files, then plain `git worktree remove <path>`; `--force`, `branch -D`, `rm -rf` are refused.
- Trunk into a conflicting PR: CMake slice lists keep trunk's and append the branch's.
- sweep.py shows only the latest verdict and misses an earlier unfixed P1 from another vendor.
- The proxy refuses the secrets API; the keys are in the `switchboard-merge` environment. No settings asks to Cory this weekend.
