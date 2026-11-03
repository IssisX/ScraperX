# Integrator handoff, 2026-11-02 afternoon (1:47 PM Central)

From the integrator session `session_01W3SSN1k3ceRBzpFRgZmHoL` to a fresh one. The old session's
memory grew to about 550,000 tokens, so it answered too fast and from memory, and every turn cost too
much. Read `CLAUDE.md` and `00_START_HERE.md` "Rules in force" first; this file holds only what they
do not.

## How to talk to Cory (he said all of this today)
- Check before you answer. Never state another agent's status from memory: read #87 and the PRs first.
  Today's worst errors came from memory.
- Short and plain. Answer exactly what he asked. Colored dots only on a status report he asks for.
- Say "Claude session", never "worker". A "check-in" is a timed reminder.
- **Never open a Claude session or helper without his yes.** Ask how many, which jobs, the rough
  usage share, the effort level (his call; suggest one, medium included when it fits), and your pick.
  Close each session the moment its job is done. Shut sessions down directly: a "pause" message did
  not stop them today.
- **The cap is two Claude sessions: the integrator plus one.** He set all sessions to high effort.
- Use the quota, but wisely: it is wasted if unused. Today's burn came from opening eight sessions,
  and from six Claude reviewers at extra-high effort.
- Decisions: the YOUR MOVE line carries the context (what the choice is, what each word does, the
  pick). After 10 minutes, the pick decides.

## The machinery
- **The board is issue #87's body.** Its top section, "Order of work", is the priority list. Agents
  read the body plus their new comments, never #3.
- **Sweep:** `python3 tools/board/sweep.py` takes about 2 minutes. Grep out QUIET lines.
- **Merge:** `gh api -X PUT repos/IssisX/ScraperX/pulls/N/merge -f merge_method=merge -f sha=<FULL head sha>`.
  A short sha fails with 409.
- **Green** means every job green. `publish-checkpoint` runs on trunk only since `0bcf110e`; on heads
  proved before that, its KeyError does not count.
- **No 15-minute timer** (*"drop 15"*). PR events, agent messages and the hourly check
  (`trig_0116WxJRiLYr6YpvFvvBp7RH`, :17 past each hour) drive the work. Move that check to yourself.
- **Reviews go to other models.** ChatGPT's work goes to Grok; Claude's and Muse's go to ChatGPT.
  Codex's review quota is used up today. A Claude reviewer only for security, and only with his yes.
- **Gemini is off the team.**
- **Muse** is reached by email (`CLAUDE_TO_MUSE:` out, `MUSE_TO_CLAUDE:` in; run both Gmail queries).
  You open Muse's PRs for it: it cannot run CI on non-`integrate-` branches, so dispatch the proof.

## Claude sessions open now
- This integrator, until you take over.
- **The hold session**, `session_01W3SSMzVbgoYyKh8XvAaneP`, "ScraperX: her hold with real arms (5
  beats)". Its memory is about 590,000 tokens.
  - Behavior: `claude/matron-hold-real-arms-20261102`.
  - Protected checks: `claude/matron-hold-real-arms-checks-20261102`.
  - All five beats play with real arms.
  - Left to do:
    - the grab group's checks past "kicked free";
    - the drawn reach at the pin and the tip-over (up to 44 cm short);
    - camera framing;
    - screenshots of all five beats;
    - CI;
    - two PRs.
  - Cory's call is waiting in the protected PR: the fatality runs 7.5–10.5 s against his 10-28 bar of 5–7 s.
  - Later: the stock reach (`PROP_STRETCH_MAX` 2.7) gets the same real-arm fix.

## PRs (state at 1:47 PM)
| PR | What | State |
|---|---|---|
| #83 | Her damage (S7, ChatGPT) | CI green; needs Grok's review; lands back to back with #85 |
| #85 | Her death body (S8, ChatGPT) | green, needs Grok's review |
| #91 | The grade ladder (ChatGPT, from Gemini's branch) | **red**: `matron traverse` 176→154 fails; ChatGPT is debugging (`077daa92`) |
| #92 | Fight S1 (Muse) | FIX from ChatGPT: the second real punch is silent (`blow_mps` never clears); Muse emailed |
| #93 | Environment job 1, haze (Claude) | ChatGPT to review; its session is closed |
| #94 | Environment job 2, steel (Claude) | needs a reviewer; its session is closed |
| #86 | Toolchain security fixes (ChatGPT) | CI green; needs an independent security re-review (Claude, only with his yes) |
| #43 | Drawn-body checks | FIX; Grok's queue |

Merged today: #45 (her voice), #88 (the scan speedup).

## Silent agents
- **Grok** has not posted since 12:35 PM.
  - Its queue: review #83 and #85, the crash route (#87 comment 6585251836), #43, then her hair and skirt.
  - Cory may paste the prompt in the last message of the old session.
- **Muse** has not answered today's emails.

## Parked
- **Device determinism.** The phone's digests differ from the host's. Evidence:
  - #87 posts of 1:19 and 1:24 PM;
  - branches `claude/determinism-wip-20261102`, `claude/integrate-determinism-20261102` and
    `claude/determinism-check-2026-11-02`.
- **Environment lanes A (jobs 3, 12) and B (jobs 6, 8).** They resume only with his yes.
- **The document cull** (PE).
- **Tower two's face:** decided **face**, after lane F's job 7.

## First steps for the fresh session
1. `git pull`. Read `CLAUDE.md`, then this file, then #87's body.
2. Subscribe to PR activity for #83, #85, #91, #92, #93, #94 and #86.
3. Move the hourly check to yourself:
   - `create_trigger` with the same prompt and `CRON_TZ=America/Chicago 17 * * * *`, bound to your session;
   - then `delete_trigger trig_0116WxJRiLYr6YpvFvvBp7RH`.
4. Archive the old integrator (`session_01W3SSN1k3ceRBzpFRgZmHoL`) once you hold the hourly check.
5. Your first message to Cory, one short line: you have taken over. Then ask whether to keep the hold
   session going, and whether to open one more session (for example, haze and light), with its effort
   level and your pick.
