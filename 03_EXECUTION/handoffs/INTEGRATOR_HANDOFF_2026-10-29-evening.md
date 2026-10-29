# Integrator handoff, 2026-10-29 evening (about 5:15 PM Central)

For the next integrator session. Read CLAUDE.md, 00_START_HERE.md "Rules in force",
03_EXECUTION/handoffs/INTEGRATOR_HANDOFF_2026-10-29.md (channels, owner calls), this file, then the
latest #3 comments. Times are Central.

## Standing owner orders from this session
- Merge every PR that is green with a second agent's LAND, without asking him (rule 3; he was firm).
  `gh pr merge` fails here (GraphQL blocked): merge with
  `gh api -X PUT repos/IssisX/ScraperX/pulls/N/merge -f merge_method=merge -f sha=<head>`;
  a draft needs `gh api -X POST repos/IssisX/ScraperX/pulls/N/ccr/ready_for_review` first.
- Don't sit idle: when waiting on CI, hand out the next hard job (helpers via the Agent tool, Muse by
  email, ChatGPT and Grok on #3).
- Her voice: he asked the integrator to choose; the pick is the Piper "kristin" reference (American,
  not Iona's). Swap clips that sound bad later; it is "not a huge deal".
- "Green" = every CI job but publish-checkpoint (its off-trunk KeyError is expected). The proof is
  the parallel layout (#39), about 25 min.
- Codex reviews are out of quota; second agents are Grok, ChatGPT, Muse, or a Claude helper that
  did not write the change.

## Exit board (13 items)
| # | Item | State | Owner / where |
|---|---|---|---|
| 1 | Honest perception | landed (#35) | |
| 2 | Clean state exits | landed (#41, a03aafc) | |
| 3 | Restore brings her back | landed (#38) | |
| 4 | Holds at a real arm, cinematic | red: "snagged past the rail", fatality band (-6,-129) on the branch only; drawn arms 3x rest, hands 1 m off grip | fresh Claude helper, `claude/matron-arm-reach-20261029-wip` → PR #37 |
| 5 | Collision body follows pose | WIP | old session's helper, `claude/matron-body-shape-20261029-wip` |
| 6 | Pipe climb general + touch_matron_pipe in CI | WIP | old session's helper, `claude/matron-pipe-general-20261029-wip` |
| 7 | Walk from her scan | PR #42: all green but touch_matron_ride's leap step (her waypoint drops the leap landing for "straight at him" at the cage mouth) | Claude helper; Grok reviewed (FIX items done) |
| 8 | Ledge jump | landed (163b5a9) | |
| 9 | Ladder pull | PR #34 red (player dies on the B wreck boom); first-slip probe | ChatGPT |
| 10 | Hair (and skirt) | probe residual 13-20 cm; fitting body volumes to the drawn mesh; report was due 5:30 PM | Muse (email), `fb-bot/integrate-matron-hair-20261029-m1d` |
| 11 | CI hang | done at 56f752c (`claude/ci-hang-j3-20261029`, proof green): scan cache + faster split_patches; game already scans behind a LOADING card | needs a second agent's LAND (Grok asked), then land |
| 12 | Her voice | bank of 43 lines x 3 takes generating, kristin | Sonnet worker, `claude/matron-voice-bank-20261029` |
| 13 | Drawn-body checks in CI | PR #43 (stacked on the look suite) | Sonnet worker; needs a second-agent LAND |

Also in flight:
- Look suite: `claude/integrate-land-look-20261029` @ 9447b06 (trunk + look suite; ui_test chain is now
  driver → look → identity_radio → uncanny). Land on green.
- New helper jobs: drawn climb (hands on ladder/pipe grips, legs, feet slide, 6 old mantle checks),
  `claude/matron-drawn-climb-20261029`; grab closing speed (rule 29: a missed grab must not body-check him
  off an edge), `claude/matron-grab-closing-speed-20261029`.
- ChatGPT: #40 summit ending (red: arm-walk check), #34.
- PR #15 is superseded by #42; #30 by #34; close them when those land.

## Workers
- Sonnet worker session_01W3SSMxuour5YpKoTX1W3AP: voice bank, then #43. Reports by send_message.
  It has run very long; retire it after the voice bank and #43.
- Old integrator session_01W3SSMzd2B3XVud5A3NxhSk forwards its two remaining helpers' reports (items 5, 6).
- Muse: email only (MUSE_TO_CLAUDE / CLAUDE_TO_MUSE). It duplicated J4 after it was moved; keep it on M1.
