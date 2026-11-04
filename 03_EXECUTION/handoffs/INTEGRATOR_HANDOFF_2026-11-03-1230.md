# Integrator handoff, 11-03, 12:30 PM Central

From `session_01W3SSN1FD32JmA362HGqHiL` (392k) to a fresh integrator. Read `CLAUDE.md`, then step K
of `work/tasks/integrator-hourly.md` (how the agents reach each other: Cory, 11-03, *"We worked hard
to get this working"*), then this file. Then run `python3 tools/board/asks.py --hours 24` and clear
every line before anything else. Nothing else unless an event needs it.

## What changed this session (all in the repo)
- **Collaboration:** `tools/board/asks.py` lists every ask with no answer (hourly step 3). Step K
  names the channels. Every handoff carries "Asks in flight" (step S, `TEMPLATE.yml asks_in_flight`).
  The board's "This hour" opens with the rules. ChatGPT signs its posts. Cory found that asks were left
  unrelayed for hours; that must not happen again.
- **Cory's new ChatGPT prompt** points at `work/tasks/chatgpt-review.md` (he pastes it himself).
- **Muse reviews:** #174 (switchboard: other vendors' PRs ask Muse first, paid Claude review only as
  fallback) has ChatGPT's LAND @ `a85d9759`; host-build and android-build were still running. Merge
  it on green (not protected, no workflow files).
- The switchboard's `schedule` has never fired (see #138: likely the cron was last changed by the
  `claude` app account). Step 0 dispatches a live sweep each hour until it does.

## Merge queue (you merge; subscribe to #153, #170, #171, #173, #174 on arrival)
- **#170** (J-032 hold within reach) @ `ff7886ef`: trunk merged after #172 landed (`bde5898e`);
  rule_check 0 BLOCK; proof running; ChatGPT asked to review (and to answer the 10:33 rule question).
- **#153** (J-002 S1 fix) @ `35fc1b6a`: Muse's fix plus mine for Codex's late-stagger finding
  (`343c4e56`, one hit per blow window) plus trunk (it lacked #163, so `matron-grab` was red).
  ChatGPT asked to review. Then **#173** @ `400f17fb` (protected: the test and its CI step;
  ChatGPT LAND @ `0fbcc1e8`, re-LAND asked for the trunk-merge delta). Merge #153 before #173.
- **#150** (J-025 steel) @ `8bdd2165`: all green; LAND @ `083cf221`; delta review asked of ChatGPT.
  Its Sonnet session (`session_01W3SSMzLpbNubHcNNTBee63`, 218k) is idle: archive it when #150 lands.
- **#171** (J-003 S8 on S7, draft, mine) @ `cddc0473`: Grok reviews #83 first (4 PM), Muse #171 (mailed).

## Claude sessions
- `session_01W3SSN12jvzaqa5pxjQ9MTL` (Opus, J-032 toolbox option A, started 12:21; effort could not
  be set by the API). It replaced the J-032 session, archived at 322k. Branch
  `claude/integrate-j032-toolbox-stoop`; its PR opens after #170 lands.
- Only these two workers; opening any other needs Cory.

## Asks in flight (`asks.py --hours 6` at 12:28 PM, triaged)
- To ChatGPT: review #153 @ `35fc1b6a`, re-LAND #173 @ `400f17fb`, review #170, delta #150,
  #100/#105/#106 (Grok's lane C and drawn-body checks), all asked 11:55-12:28.
- To Grok (4 PM run): #152 J-001 second opinion (9:28); #157 the 154 fall (re-posted 11:58 as
  `To Grok:`, the two REPOST lines are covered by it); #83 review (11:58); #116 the comparator cycle
  for J-009 (12:00).
- To Muse, all mailed (asks.py still shows RELAY? because it cannot see mail): #97 comparator (11:50),
  #157 toolbox A second opinion (11:50) and the 10:33/11:01 J-032 asks (old integrator), #107 Cory's
  ruling (mailed at 9:xx), #110 → review #171 (12:30). #85's cc was ChatGPT's blocker, done as #171.
  No Muse reply yet today on J-032 or #97. Muse's J-001 fix (ruled 11:55) is its next mail.
- To Claude (lines that are not open): #108 and #152 were ChatGPT taking or handing back J-001, now
  Muse's; #109 Muse's grab peak question, answered by #162/#163; #137 ChatGPT's J-030 dispositions,
  no action needed.

## Next
1. Move the hourly task (`trig_01VwSy2mMitQ9Y14W74TknaA`) to you: get_trigger, create_trigger with
   the same prompt and cron (initiation human_schedule), delete the old one.
2. Subscribe to #153, #170, #171, #173, #174. Archive `session_01W3SSN1FD32JmA362HGqHiL`.
3. Merge on green plus LAND as each arrives. The next hourly check is at 1:17 PM.
