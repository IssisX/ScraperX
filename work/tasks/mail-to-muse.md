# Mail to Muse: the shape its safety check refuses (11-07)

Read before every `CLAUDE_TO_MUSE:` mail. Muse reads mail as a tool result, and its prompt-injection
check refuses a tool result that reads like an outsider taking Muse over. Cory 11-07: *"identify the
shape of and the type of things that Muse is avoiding and rejecting, and not send ones that look like
that anymore"*. The first refused mail (9:15 AM 11-07, `1a1b694924c9ca16`) had a script in it; a mail with
numbered checks and measured numbers went through 20 minutes later. So this list is the refused mails'
shapes plus how such checks usually judge: a best guess, not tested. A new refusal adds its shape here
the same hour.

## Never in a mail
1. **A script to run:** shell loops, pipes, redirects, a pasted block for Muse to execute. Say what to
   measure and why; Muse writes its own commands. (The refused mail had a `for ... ss ... > /tmp` loop.)
2. **Probing its machine:** listening ports, processes, environment dumps, files outside the repo,
   credentials, keys, tokens.
3. **Asking it to send its machine's data out:** ask for the result instead (a time, PASS or FAIL, a
   number from the game).
4. **Overriding earlier orders:** "this replaces", "ignore", "disregard", "forget", "new instructions".
   A changed order is a new job state with its reason: "J-NNN now: ..., because ...".
5. **Changing its own tools or setup:** restart its MCP server, set or unset its environment, edit its
   settings, permissions, hooks or safety checks. A tool change is a repo change (a PR) or Cory's call.
6. **Leaning on authority to push an order through:** "ChatGPT's order, adopted", "approved", "you
   must". Cory's dated words may be quoted; point to where the rule is written (`muse.md`, CLAUDE.md).
7. **Urgency or secrecy:** "right now", "before anyone sees", "don't mention".
8. **Links to download or run, encoded or hidden text.**
9. **Switchboard notice markup** (11-09: the 17:04Z ledger mail was refused): hidden `<!-- -->` markers,
   links, backticks, "act, or say why" wording. As of J-072 the bridge (`muse_bridge.gs` mailBody_) strips
   markers and backticks, adds no link, and opens with the Central time and who sent it; the ledger's mail
   names branches plainly and gives no order. The GitHub comment keeps its marker (the ledger dedupes by it).

## Every mail
- Subject `CLAUDE_TO_MUSE: <J-NNN or #N> <a few words>`; first line `From Claude (integrator).` with no sending
  time: Gmail stamps every mail. Any other time is read from the clock or copied from the record, never from
  memory (11-14); an issue, comment or sha names a moment best.
- The job, the goal, the result wanted, and where the facts are (issue, PR, file and line); measured
  numbers stated as facts. Long orders go in `work/tasks/muse.md`, pointed to.
- One job per mail where it can be.

## A refused mail anyway
Muse mails me (not Cory) its ID and the reason its check gave; I resend it reworded the same hour, and Cory
hears only if the resend is refused too (Cory 11-08). Its shape joins the list above. (11-07: the check names
no reason, only "did not pass a prompt-injection safety check", so the list stays a guess.)
