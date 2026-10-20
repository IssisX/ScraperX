---
name: learn
description: Record a lesson the owner just taught, in the owner's own words, as a directive every later session reads first. Use when the owner types /learn (with or without text), or says to remember, keep, or learn something about how to work, what they want, or what they do not want.
---

# Learn

The owner's directives table in `00_START_HERE.md` (section "Owner directives", newest first) is
how every session learns from the owner. This skill adds one row to it.

1. **The lesson.** With text after `/learn`, that text is the lesson. Without it, the lesson is the
   owner's most recent correction or preference in this conversation. Quote the owner's exact words,
   in italics; never paraphrase them into something they did not say. If two readings are possible,
   ask one short question first.
2. **Check it against the table.** Read the existing rows. If the lesson repeats one, update that
   row's status instead of adding a duplicate. If it contradicts one, say so in one line and ask
   which stands; once answered, mark the older row "superseded" with the date.
3. **Write the row** directly under the table's header line, today's date first:
   `| <date> | <what, in plain words, with the owner's quote> | <status: "in force", and what
   changes in how Claude works because of it> |`. American spelling, plain words, no internal labels.
4. **Commit and push** the change alone (it touches only a document, so the subject ends with
   `[skip ci]`), with the repository's commit trailer, to `ScraperX-Claude`.
5. **Confirm** in one line: the lesson as recorded, and the commit.
