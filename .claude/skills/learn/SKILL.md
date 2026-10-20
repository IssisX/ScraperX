---
name: learn
description: Study the whole session (or everything since the last /learn) and turn what the owner taught, and what was learned the hard way, into lasting rules and skills, proposed first and written after the owner's yes. Use when the owner types /learn (with or without text), or asks to remember, keep or learn something.
---

# Learn

Do all the work the owner would otherwise do by saying "learn this, learn that". Not every
sentence is a lesson: decipher what the owner meant and what will change how the next session
works, and leave the rest.

## 1. Gather

- **The owner's words:** `python3 -I .claude/skills/learn/owner_messages.py --since <last>` (from
  the repository root), where `<last>` is the `learned_through` timestamp in
  `.claude/skills/learn/state.json`; omit `--since` for the whole session, or when the owner says
  `/learn all`. It prints only what the human typed, in order (tool output, reminders, hook and
  agent messages, compaction summaries and pictures are left out; a picture shows as
  "[a picture]").
- **What was learned the hard way:** `git log --since=<last> --format='%h %s%n%b'` on the
  working branch. Commit messages here record root causes; a fix after a failed attempt (a red CI
  run, a test that relied on a bug, a wrong reading of the owner) is a lesson candidate.
- **Text after /learn:** that is the lesson, first in line; still check it as below.
- When the gathered text is long, hand the reading to one subagent with this file's step 2 as its
  brief and take back its candidate list; do not read tool output wholesale.

## 2. Decipher (the judgment)

From the gathered material, find:

- **Corrections:** where the owner said Claude got it wrong (a misreading, a wrong assumption, an
  unwanted action). The lesson is the rule that would have prevented it, with its root cause.
- **Preferences and standing directions:** how the owner wants to work, talk, be asked, be
  reported to (format, tone, units, time zone), what is approved, parked or never wanted.
- **Product direction:** the design choices the owner made and why (the intent behind the words).
- **Hard-won engineering fixes:** a failed attempt and its eventual fix whose cause will recur.

Then:

- **Merge** repeats into one lesson; when the owner's stance changed, keep the latest and say so.
- **Drop** one-off task requests already done, chatter, and anything that changes nothing next
  session.
- **Quote** the owner's exact words for each owner lesson (italics), never a paraphrase passed
  off as a quote. If a lesson can be read two ways, ask one short question.
- **Classify** each as a **rule** (a guardrail or constraint) or a **skill** (a reusable procedure
  of several steps).

## 3. Place, without duplicates

- **Owner rules:** a row in `00_START_HERE.md`'s "Owner directives" table, newest first:
  `| <date> | <plain words, with the owner's quote> | <status: in force, and what changes> |`.
  If a row already says it, update that row's status; if a lesson contradicts a row, mark the old
  row superseded (with the date) only after the owner confirms.
- **Engineering rules** that belong to a plan or a subsystem: that document's rules section
  (for example `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` section 2.6 for the climb).
- **Skills:** `.claude/skills/<name>/SKILL.md` (a short frontmatter `name` and `description`, then
  the steps). Update an existing skill that was used and fell short instead of adding another.

## 4. Propose, then write

Show the owner a concise numbered list, grouped (corrections, preferences, product, engineering,
skills), each item one or two lines: the lesson, its quote, and where it goes (new or updated).
Ask one question: apply all, or which numbers. Write only what is approved. Then:

- set `learned_through` in `.claude/skills/learn/state.json` to the newest gathered timestamp;
- commit the documents alone (subject ending `[skip ci]`), with the repository's commit trailer,
  and push to `ScraperX-Claude`;
- confirm in one line: what was written, and the commit.
