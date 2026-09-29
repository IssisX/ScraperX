---
name: learn
description: Distil this session into persistent guidance. Reads the whole thread (the owner's instructions, corrections, preferences, frustrations, and the debugging lessons found along the way) and writes them as project rules in .claude/rules/, or as a project skill when the lesson is a repeatable procedure. Use when the owner types /learn, optionally with a focus.
argument-hint: "[optional focus, e.g. 'the APK identity rule' or 'how I want reports written']"
disable-model-invocation: true
---

# /learn

Turn what this session taught into rules that load at the start of every future session on this
branch. Claude Code loads every `.claude/rules/*.md` at session start (a rule with `paths:`
frontmatter loads when a matching file is read), so a rule written here is followed next time
without anyone repeating it. Modelled on the Antigravity CLI's `/learn`, which distils "recent
corrections, user feedback, and debugging resolutions" into project rules or skills.

Focus given by the owner: $ARGUMENTS
With a focus, capture that (and only what it directly implies). Without one, sweep the whole thread.

## 1. Read before writing

1. Read every file in `.claude/rules/` and the list in `.claude/skills/`. New learning updates
   these in place; it never duplicates them.
2. Read the owner's directive ledger at the top of `00_START_HERE.md`. A directive already there
   becomes a rule only if it governs future behaviour (not a record of a finished job).
3. Sweep the thread, including any compaction summary, for:
   - **Instructions**: "from now on", "always", "never", "I want", "don't", standing rules.
   - **Corrections**: every time the owner said something was wrong, missing, lazy, or not what
     they asked. The rule is the behaviour that would have avoided the correction.
   - **Preferences**: how the owner wants replies (length, tone, structure, what to lead with),
     how often to report, what to ask before doing, what never to ask.
   - **Frustration**: anger, sarcasm, "wtf", repeated asks. Each is a correction with high
     priority; find the behaviour that caused it.
   - **Approvals**: a choice the owner confirmed ("yes", "good", "keep doing that") is a rule too.
   - **Debugging resolutions**: a root cause that could recur (a library defect, a flaky
     pattern, a trap in the build). The rule is the check or practice that catches it.
   - **Project facts that do not change**: branch names, identities, invariants, where things live.

## 2. Keep only what changes future behaviour

Drop: one-off task state (run numbers, current frontier, open decisions), anything already in the
owner's account-level preferences, and anything the code or git history already records. Never
store secrets, tokens, email addresses, or model identifiers. When two statements conflict, the
owner's newest wins; replace the old rule and say so in the report.

## 3. Write each rule the same way

```
- <Imperative rule, one line.>
  Why: <the reason, one line.> Source: <YYYY-MM-DD, the owner's words quoted or the event>.
```

The owner's own words go in quotes; do not paraphrase a quote. A rule must be checkable: "Look
things up before saying you don't know" is a rule, "be helpful" is not.

## 4. File it by topic

| File | Holds |
|---|---|
| `.claude/rules/owner.md` | How the owner wants me to work and talk: reporting, tone, autonomy, what to ask first |
| `.claude/rules/process.md` | Git, branches, pushing, CI, verification, evidence before claims |
| `.claude/rules/game.md` | ScraperX design law: mechanisms, the ascent, proof through the game's input |
| `.claude/rules/code.md` | Code and build conventions, library traps; add `paths:` frontmatter when a rule only matters for some files |
| `.claude/skills/<name>/SKILL.md` | A lesson that is a repeatable multi-step procedure, not a single rule |

Start each rules file with a one-line title and `Scope: the ScraperX-Claude branch.` Create a file
only when it has a rule to hold. Keep each file under 150 lines: merge near-duplicates and delete a
rule the owner has withdrawn, never silently.

## 5. Save it and report

1. Commit only `.claude/rules/` and `.claude/skills/` changes on the current branch, with the
   message `Learn: <what was captured>` and the branch's commit trailers, then push to the branch
   the session works on. The container is temporary; an unpushed rule is lost.
2. Report to the owner, in this order: rules **added**, rules **changed** (old → new), rules
   **removed or superseded**, each with its file. Then one line on anything seen but left out and
   why. No other commentary.
