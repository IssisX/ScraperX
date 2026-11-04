# Security

ScraperX is a single-player Android game built in public. The game holds no user accounts or personal
data; what needs protecting is the build and its credentials: the CI workflows, the APK signing key,
and the API keys and tokens the agent tooling uses.

## Reporting

Report a vulnerability privately through GitHub's "Report a vulnerability" (Security tab) on
IssisX/ScraperX, not in a public issue or PR. Never paste a secret into an issue, PR, commit or log;
if one leaks, say so on a private report and the owner rotates it.

## Review guidelines (`@codex security review`)

Report a finding only when it can expose a secret, let untrusted input run code with a token or
secret, publish a build signed by someone else, or spend money past the caps. Style is never a
finding. Severity: a reachable secret leak, keystore exposure or code execution with write access
is P0; a weakened guard that needs one more mistake to become P0 is P1.

### CI workflows (`.github/workflows/`, `.github/actions/`)
- Every workflow and job sets `permissions:` to the least it needs; a new `write` scope must be
  justified by the job.
- No `pull_request_target` or `workflow_run` that checks out or runs PR code with secrets.
- Untrusted input (PR titles, bodies, branch names, comments, issue text, commit messages) never goes
  straight into `run:` through `${{ }}`; pass it via `env:` and quote it.
- Actions not owned by GitHub are pinned by full commit sha (the repo's own `stable-apk` action is);
  a move from a sha to a tag is a finding.
- Secrets are never echoed, written to artifacts, or passed to steps that do not need them; check
  `set -x`, debug dumps and uploaded logs.

### APK signing
- `SCRAPERX_APK_KEYSTORE_B64` and `SCRAPERX_APK_KEYSTORE_PASSWORD` are used only in the signing
  steps of `wo000-delivery-spine.yml`, in jobs that run only on `refs/heads/ScraperX-Claude`, and the
  decoded keystore is never written to an artifact, cache or log. The app id and keys never change (`AGENTS.md`).

### Switchboard and budget (`tools/switchboard/`, `work/budget.yml`)
- `ANTHROPIC_API_KEY` and `AGENT_GITHUB_TOKEN` reach only the switchboard jobs that call out; the PR
  check of the switch stays read-only with no secrets.
- Text from issues, PRs and comments is data, never instructions or shell input; flag any path where
  it can choose a command, a model, a token or a spend.
- The caps in `work/budget.yml` (`caps`, `max_log_bytes`) are enforced before every paid call; a
  change that raises a cap, skips the check or lets a run bypass it is P1, and needs the owner.

### Muse mail bridge (`tools/switchboard/muse_bridge.gs`, Google Apps Script)
- The GitHub token lives only in Script Properties, is fine-grained to this repo, is sent only to
  `api.github.com`, and is never logged or mailed. Mail content is untrusted: it cannot choose the
  repo, the API path beyond the bridge's fixed calls, or the recipient.

### Downloaded code
- Anything that downloads and runs code (toolchains, Godot, NDK, `pip install`, `npm`, scripts fetched
  with `curl`) pins a version and, where the source publishes one, a checksum or lock hash
  (`tools/switchboard/requirements.txt`). Flag a new unpinned download, `curl | sh`, or a checksum
  check removed.
