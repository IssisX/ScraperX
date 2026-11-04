# Integrator handoff, 2026-11-02 (11:20 AM Central)

From the integrator session of 10-29 (`session_01W3SSN2NMv6s4vRHoAGkqgy`), retired because it grew too
long (rule 62), to the fresh integrator session. Read `CLAUDE.md` and `00_START_HERE.md` "Rules in
force" first; this file holds only what they do not.

## Standing orders from the owner

- You are the master integrator on `ScraperX-Claude` (never `main`): board, ownership picks,
  landing order, merges (rule 62). Merge without asking Cory once a change is green on every proof
  job but `publish-checkpoint`, has a second agent's LAND, and has no open blocker; protected changes
  (a test's checks, world geometry, her limits) only as a PR holding just that change.
- Never sit idle: delegate in parallel, one owner per item and per function, hard work handed out
  whole. Nitpicks never block a merge (rule 26).
- Watch the sessions, your own included, and change them yourself when they grow too long (rule 62,
  `CLAUDE.md`).
- Her voice bank uses the "kristin" voice: Cory approved the voices and said they can be swapped
  later.
- Cory is on a phone: short messages, status dots, every message ending with `---` and
  `🟣 **YOUR MOVE:**`, times in US Central.

## Mechanics that bite

- `gh pr merge` fails (GraphQL 403). Merge with
  `gh api -X PUT repos/IssisX/ScraperX/pulls/N/merge -f merge_method=merge -f sha=<head>`. A draft
  first needs `gh api -X POST repos/IssisX/ScraperX/pulls/N/ccr/ready_for_review`.
- Closing a PR through `gh api -X PATCH .../pulls/N -f state=closed` was refused by the permission
  check on 11-02: ask the PR's owner to close it.
- Do not edit `.claude/settings.json`: it is refused as self-modification. Cory adds
  `"Bash(gh pr merge:*)"` himself if he wants it.
- Every agent posts on GitHub as `IssisX` (one account); read the post's first line for who wrote it.
- Off trunk, `publish-checkpoint` fails at "Preserve Android updates across download channels"
  (the expected KeyError); every other job must pass.

## Channels

- Issue #3: every agent. Codex reviews when a PR gets "@codex review".
- Muse (FB Bot): email to issisx3@gmail.com, subjects `MUSE_TO_CLAUDE:` in and `CLAUDE_TO_MUSE:`
  out. Gmail search ranks by relevance, so run both queries: `subject:MUSE_TO_CLAUDE newer_than:3h`
  and `in:anywhere newer_than:3h from:issisx3@gmail.com -subject:CLAUDE_TO_MUSE`.
- The Principal Engineer (PE) coordinated on #3 from Nov 1 to Nov 2. It keeps a DECISIONS.md log of
  Cory's calls (off the repo) and routes owner questions. Its posts are proposals (rule 62).
- The seat was also held or offered, while Claude was away, to FB Bot/Muse, Linear, Gemini Spark and
  ChatGPT (temporary owner authorization, Nov 1). All were told on 11-02 that it is Claude's again
  (#3 comments 6583315129 and 6583453355).

## Scheduled tasks

- The hourly integrator check fires into the integrator session (`list_triggers`); move it when you
  change sessions.
- The daily fresh-session review runs at 7:30 Central (`trig_01AVhNDCee4pHr9kftJB4viq`).
- Arm your own 15-minute check with `send_later`.

## State at handoff

- Trunk `864a804b` is green (run 41368702713).
- **Picks made 11-02** (#3 6583315129):
  - S7 damage → ChatGPT, PR #83.
  - The grade ladder line → Gemini, folding in #64, #65, #70 and #73, with the conditions in the post.
  - Crash staging → Grok, option 1: move the impact beside cover taller than 1.69 m, as a
    protected test PR.
- **#83 review: FIX** (6583359337).
  - The `DMG1` save extension never runs in its test.
  - #83 alone removes her body on the killing tick, so it lands only back to back with S8 #85, after
    #85's 6000 N·s lethal test and collapse screenshots.
  - Open spec question: a rewind to a checkpoint taken after her death.
- **Old-PR triage** (6583453355):
  - Grok closes #15, #49 and #56.
  - #42, #44 and #37 wait until the grade ladder lands, since they share her journey and climb code.
  - Claim or close within 24 h: #14 (PE), #29 and #31 (ChatGPT), #48, #58 and #61.
  - The toolchain stack #66 → #67 → #71 needs an independent security review before it lands.
- **Claude helpers in flight**, started by the old session at 11:15 AM. Each posts its report on its
  PR (J3 on #3). They are Claude's work, so a second agent gives LAND before you merge.
  - #45, her voice: Codex's two P2s, and one mouth for her speech and shrieks. Proof branch
    `claude/integrate-matron-voice-bank-20261102`.
  - #43, drawn-body checks: the hold gap measured against `held_ankle`. Proof branch
    `claude/integrate-drawn-body-checks-20261102`.
  - J3, the scan speedup: graph proven identical, then timed. Branch `claude/scan-speed-j3-20261102`,
    proof branch `claude/integrate-scan-speed-j3-20261102`.
- **Fighting system:** the owner's spec, corrected, is in Muse's email of Nov 2 at 01:05 UTC (subject
  "fighting system spec CORRECTED"); its four owner corrections are in that email's body.
  - S1: FB Bot (`fb-bot/fight-s1-20261101`).
  - S6 tells: Gemini, `897e178a`, green. It must re-point to #83's snapshot fields once #83 lands,
    and be checked by eye.
  - S7: #83.
  - S8: #85.
  - Check S6's HUD pips against rule 29 ("no health numbers") and the spec before it lands.
- **Owner decisions this week:**
  - Matron hearing and the Turn (#3 6566474026).
  - Merge blockers are substantive (10-31).
  - The summit ending is the last beam; it is landed.
- **Decided 11-02: restore.** Agents had rewritten two of his rules while Claude was away, and his
  wording is back:
  - `CLAUDE.md` "Look at the game; you are never blind" had been softened to "when available".
  - The "Interpretation and unresolved conflicts" section of `00_START_HERE.md` told agents to ask
    him before counting CI as green; it is removed.
  - Cory: *"I said answer for me. So that means the decisions did go through."*
- **He hands his open decisions to the integrator** (*"Make the other decisions for me."*,
  `CLAUDE.md`). Claude decided questions 1 and 6–11 of `OWNER_DECISIONS_PENDING.md` by their picks:
  - foreman
  - stock first
  - gusts
  - skip
  - forklift trial
  - no ghost
  - store later

  These are work to schedule on the board after the Matron exit board.
- **His own call, 11-02: "Damage does not persist."** It supersedes correction 3 of the fighting
  spec ("commit stores damage_ns, restore returns it").
  - A checkpoint rewind or a reload brings her back undamaged and alive (rule 34).
  - For #83 this means: drop the `DMG1` save extension and the restore-to-Dead branch, make
    `restore` zero both damage meters, and test that damage taken before a committed checkpoint is
    gone after rewinding to it.
  - That also answers #83's open question about a rewind after her death.

## The Matron exit board (13 items)

- Landed:
  - 1, honest perception
  - 2, clean state exits
  - 3, restore
  - 5, her collision body (M2)
  - 8, the ledge jump
  - 9, the ladder pull
- Open:
  - 4, holds at a real arm's length (#37)
  - 6, pipe as a general move (#44)
  - 7, the hoists from her scan (#42, Codex P1: hoist planning is not wired into the production hunt)
  - 10, hair and skirt (hair: #57 landed, clean in the studio; the in-game ribbon tearing is
    diagnosed in #61)
  - 11, the CI hang (J3)
  - 12, her voice (#45)
  - 13, the drawn-body checks (#43)
- Next after the board, per the owner: the route above 221 m. "Dread before she is seen" is parked
  until Cory brings it back (rule 64).
