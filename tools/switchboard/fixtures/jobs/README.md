# Job state machine fixtures: real timelines

Real evidence from IssisX/ScraperX, read on 11-12 with the GitHub REST API (read only, every page) and from the
trunk history of `work/tasks/muse.md`. The job state machine is checked against these. Each file is one case
where the old system failed or worked. These files hold evidence and judgment only. They contain no code.

| File | What happened |
| --- | --- |
| `j109-built-twice.json` | J-015 and J-109 were taken by mail only. Label and muse.md still said Muse, so Muse's handover push and Claude's helper built the same work. |
| `j038-built-twice.json` | The same hole on J-038's #372 red: two heads carried the same fix. #469 replaced #372 and landed. |
| `j118-j137-split.json` | The case that worked. J-137 was split off J-118 with an owner label at birth, the muse.md line and the mail, all within 10 minutes. |
| `j059-343-stacked-deadlock-scope.json` | #343 had a stacked protected fix that was local only, a FIX deadlock (the fix waited on the PR it unblocks), then two scope BLOCKs. |
| `j106-local-head-not-trunk.json` | A "baseline" failure on a local-only sha was taken for a trunk failure, and a job was opened and queued on it. |
| `j079-473-unasked-codex.json` | A workflow PR waited at MERGE-BY-HAND on a Codex review nobody had asked for. |
| `j119-j124-j125-start-notices.json` | "start" notices went to three jobs still queued behind running lanes. |
| `j126-clean-planned-to-closed.json` | A clean run from planned to closed in about 2 hours. |

## Shape (`schema: scraperx-job-fixture/1`)

- `events`: ordered by time. `i` is the position and `id` is a stable key. Each event has `t` (UTC), `kind`,
  `actor` (GitHub login, or the commit author's name for `commit`), `job` (a list), `refs` (issue, pr, branch,
  sha8, run, thread), and `subject`/`text`, each cut to 200 characters. `lines` holds up to 3 lines of a long body
  that name the case's jobs.
- `expected`: the state the machine should show after the event `at` (with `at_i` its index), for `job`. Each
  entry gives `owner` (as the plan should hold it), `next.who`/`next.what` (who owes the next step) and `why`.
  `flags` name the failure. `claimed_by` is set when a mail claims a job the plan gives to someone else.
  States: planned, told, acknowledged, started, pushed, proven, reviewed, landed, closed, held, blocked,
  conflict. A merge conflict is shown as `conflict` with the `merge_conflict` flag. `next.who: nobody` means
  the notice at that event should not have been sent.
- `probes`: point checks made at fetch time, for example `GET commits/<sha>` returning 422.
- `lessons`: one line for each hole the case shows.

Event kinds: `issue_opened`, `pr_opened`, `label_added`/`label_removed` (only owner:, state:, job, wake:muse
and needs:), `plan_edit` (a trunk commit that changed where muse.md puts the job: its section and the clause
naming it), `mail_out` (the bridge's record of the integrator's mail to Muse), `mail_in` (Muse's mail, posted
by the bridge), `notice_to_muse`/`notice_to_claude` (the switchboard's posts), `post` (any other trusted
comment), `verdict` (the eye or the automatic Claude review), `codex_ask`, `codex_state`, `codex_review`,
`codex_finding`, `commit`, `push`, `proof`, `ready_for_review`, `merged`, `closed`, `merge_note`, `inbox`.

## What is fact and what is judgment

- Events are API facts. Two fields are derived, and each is marked where it appears. A `push` time is the
  creation time of the push-triggered proof run (`inferred` says so). A `commit` time is the committer date,
  which is not a push. `proof` takes the run's completion time. Cancelled runs are dropped, though their push
  still counts.
- Every `expected` entry is judgment: the state the new machine should compute from the events up to that
  point. An entry also carries `inferred` where it rests on more than the events.
- Trust: comments count only from `work/agents.yml` `verdict_logins` (IssisX, github-actions[bot]). The Codex
  app (`chatgpt-codex-connector[bot]`) is read only as Codex state. Every other login is dropped and counted in
  `trust.dropped_untrusted_comments` (only linear-code[bot] link-backs here). Every agent posts as IssisX, so
  the author comes from the first line: `**Muse (via email bridge): ...**` is Muse, and `CLAUDE_TO_MUSE: ...`
  with the record line is the integrator's mail. The bridge removing `wake:muse` is the receipt that a notice
  was mailed.
- Times in mail text are Central, as written. Event `t` is UTC.
