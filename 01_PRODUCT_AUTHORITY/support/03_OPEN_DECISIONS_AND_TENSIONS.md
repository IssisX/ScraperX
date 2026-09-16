# ScraperX — Open Decisions and Tensions

**Status:** PRE-GDD CLOSURE RECORD

## Unresolved pre-GDD product decisions
- None among the decisions reviewed by the current closure process.

## Resolved tension

### Failure semantics — RESOLVED
The earlier tension between persistent committed consequences and mission rollback is closed by the direct closure decision:

- Ordinary committed physical mistakes remain part of the world and must be physically recovered from where possible.
- **Explicitly designated hard-fail missions may restore the committed checkpoint.**
- Mission rollback is therefore an authored exception, not the default way ScraperX escapes inconvenient consequences.

Source decision: `05_PRE_GDD_CLOSURE_DECISIONS.md` / `mission_failure_semantics`.

## Non-blocking implementation constraint

### First-person motion comfort
Fast, precise first-person parkour combined with frequent exterior height exposure creates a real comfort/accessibility burden.

This does **not** reopen the selected movement or exterior-exposure decisions and is **not a GDD blocker**. Later embodiment/presentation work must solve camera motion, control response, accessibility options, and visual stability without weakening physical traversal truth.

## Reviewed custom capability answer

The questionnaire recorded:

> Things like forklifts or cranes can do a lot. The player should have ability to lift and push and move things into place too.

The closure decision now bounds the manual side:

- **Unaided manual manipulation:** slightly heroic but still grounded.
- **Larger capability:** comes from physically operating and exploiting machinery such as forklifts, cranes, and other appropriate mechanisms.

No additional capability is inferred beyond those direct selections.
