---
name: learn
description: Study the whole session (or everything since the last /learn) and turn what the owner taught, and what was learned the hard way, into lasting rules and skills, written and pushed in the same run and then reported, with anything unsure brought up instead of written. Use when the owner types /learn (with or without text), or asks to remember, keep or learn something.
---

# Learn

Do all the work the owner would otherwise do by saying "learn this, learn that", in this run:
the owner does not pick or approve the lessons (the owner, 2026-10-20: *"do all of the work that
the user would have to do"*; and when a run stopped to ask: *"I think that should be done by
now"*). What is written becomes the owner's word for every later session, so it can help a lot or
do real harm: *"if anything feels off sounds off or youre unsure about something - bring it up"*.
Not every sentence is a lesson: decipher what the owner meant and what will change how the next
session works, and leave the rest.

**It always says that it ran.** The report opens with this line, filled in from what was
gathered, and nothing comes before it:

> 🧠 **/learn ran:** studied N of your messages (first to last, Central) and M commits.

It ends with one line:

> ✅ **/learn done:** what was written and where, the commit, pushed (and what waits on the owner).

## 1. Gather

- **The owner's words:** `python3 -I .claude/skills/learn/owner_messages.py --since <last>` (from
  the repository root), where `<last>` is the `learned_through` timestamp in
  `.claude/skills/learn/state.json`; omit `--since` for the whole session, or when the owner says
  `/learn all`. It prints only what the human typed, in order (tool output, reminders, hook and
  agent messages, compaction summaries and pictures are left out; a picture shows as
  "[a picture]").
- **Pasted text** (a handoff or prompt another session wrote, pasted by the owner) is what the
  owner chose to pass on, not the owner's own words: a lesson from it is credited as pasted.
- **What was learned the hard way:** `git log --since=<last> --format='%h %s%n%b'` on the
  working branch. Commit messages here record root causes; a fix after a failed attempt (a red CI
  run, a test that relied on a bug, a wrong reading of the owner) is a lesson candidate.
- **Text after /learn:** that is the lesson, first in line; still check it as below.
- **The opening line's facts:** the script's message count, its first and last timestamps in
  Central (`TZ=America/Chicago date -d <timestamp>`), and the number of commits.
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
  off as a quote.
- **Classify** each as a **rule** (a guardrail or constraint) or a **skill** (a reusable procedure
  of several steps).
- **No target count.** The list is as long as the material makes it: never aim for, round to or
  pad to a number, and never cut a real lesson to keep it short. Merging true repeats is the only
  way two lessons become one.
- **Check it both ways:** read the gathered messages once more, first to last, against the draft.
  Every correction, standing direction, approval and refusal must be in the draft, already in
  force (name the row), or a one-off; nothing in the draft may be padding (a one-off, a duplicate
  of a row, or a lesson that changes nothing next session).
- **Unsure goes to the owner, not into the table.** A lesson that can be read two ways, reaches
  past the owner's words (what was said of one thing applied to everything), contradicts a row,
  or could do harm if it were wrong (a permission, a test or a bound, a branch or push rule, a
  safety gate) is brought up in the report as a one-line question and written after the answer.
  A sure lesson that needs a limit to be safe is written with the limit.

## 3. Place, without duplicates

- **Owner rules:** a row in `00_START_HERE.md`'s "Owner directives" table, newest first:
  `| <date> | <the lesson as a concise, well-said passage in plain words, with the owner's quote> | <status: in force, and what changes> |`
  (the owner, 2026-10-20: *"i like when you up-interpret my rough input into beautifully said
  concise passages"*; the passage keeps the owner's exact meaning, the quote keeps the words).
  If a row already says it, update that row's status instead of adding one.
- **Engineering rules** that belong to a plan or a subsystem: that document's rules section
  (for example `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` section 2.6 for the climb).
- **Skills:** `.claude/skills/<name>/SKILL.md` (a short frontmatter `name` and `description`, then
  the steps). Update an existing skill that was used and fell short instead of adding another.
- **Fixes in code** a lesson calls for (a CI check, a test, a script) are built as ordinary work in
  their own commit, checked locally, so CI runs them; never in the documents' `[skip ci]` commit.

## 4. Write, then report

- Write every sure lesson where step 3 places it.
- Set `learned_through` in `.claude/skills/learn/state.json` to the newest gathered timestamp.
- Commit the documents alone (subject ending `[skip ci]`), with the repository's commit trailer,
  and push to `ScraperX-Claude`; then build the code fixes.
- Report, concisely: the opening line; **written**, a numbered list grouped as corrections,
  preferences, product, engineering and skills, each item one or two lines (the lesson, its quote,
  where it went); **bringing up**, each unsure point as a one-line question; **already in force**
  (said again, a row holds it) and **left out** (one-offs done, questions answered), so nothing is
  dropped silently; then the done line.
