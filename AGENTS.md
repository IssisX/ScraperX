# Rules for every agent in this repo

The rules in force are in `00_START_HERE.md` ("Rules in force"), which is the authority; the owner's
dated words are in `00_START_HERE_ARCHIVE.md`. `CLAUDE.md` spells out the rules that keep being
broken, and they apply to every agent, whatever its name. Read it first.

The short version:

- Write branch `ScraperX-Claude`. Never push to `main`. No force-push.
- Never edit `MANIFEST.txt`, `SHA256SUMS.txt` or `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md`, and
  never change the app id, keys or repository visibility.
- Her climbing is general skills only: no per-route, per-deck or per-transition code. Gravity on.
  Every force on her comes from her one budget; no velocity writes; at most 0.20 m per 1/90 s tick.
- Never weaken, skip or delete a test. Test checks, world geometry and her force/physics limits
  change only by a PR into `ScraperX-Claude`, holding just that change; the integrator merges it
  once green with a second agent's LAND, and the owner can overturn any merge.
- Android builds are clang C++17 with `-Werror`: no structured bindings captured by lambdas.
- Test only what the change reaches (`SCRAPERX_ONLY=<group>`); CI's proof run
  (`<agent>/integrate-<job>` branches) is the final proof.
- American spelling. No AI model names in repo files. Docs-only commits end with `[skip ci]`.
- Lower-cost agents may run tests, builds and the rule check, but may not delete branches, force-push or land work.

Tools: `REPO_MAP.md` (where things are), `tools/rule_check/rule_check.py` (run before landing),
`03_EXECUTION/REVIEW_CHECKLIST.md` (the second agent's review), `03_EXECUTION/handoffs/TEMPLATE.yml`
(the one handoff form, posted on issue #3 addressed to the agent it is for). Coordination and current assignments: the
[issue #3 board](https://github.com/IssisX/ScraperX/issues/3). Historical owner evidence is in
`00_START_HERE_ARCHIVE.md`; the active rules and unresolved conflicts are in `00_START_HERE.md`.
Check that tools exist in the current session; an old tool instruction is not evidence of availability.
