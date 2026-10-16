# Opening native lag repair — 2026-10-16

Owner foreground report: nine native steps cost251.877ms p50/304.154ms p95, with149last/835peak draws. Its frame delta was capped, so the quoted7.7FPS is not reliable elapsed-wall FPS. The recorder now uses monotonic call boundaries and excludes explicit pause intervals; its focused and actual Main tests pass.

## Supported cause

Current67b92b1 Android native was profiled directly. Axial cloth damping is substantial, but a grouped-phase prototype failed its consistent-wall-time acceptance (CPU13%lower, median6%lower, p9534%higher). It is preserved privately and not shipped. A worker-count candidate was not measured because the benchmark child lacked the required eligible performance-core budget. No pool, solver, force, cloth resolution, iteration or90Hz timing change is shipped.

The full opening exposed the actual integration defect: static crane dock1451 physically collides with catch membrane1963 near(-35.6288,3.77413,-143.945). The isolated catch fixture can settle; in the real world the cloth remained fast and its coupled rigid stages kept moving after1080ticks. The sleep rejection was real speed, not a sticky touch flag or missing sleep member. Correctly refusing sleep was essential.

## Repair

Set back the support plane bordering the catch by1m on each freight dock, including its footings, columns, braces and longitudinal beam. The retained full-width transverse members carry the overhang. Both original11m/22m decks,8m widths, guardrails, open mouths, dock positions, native identities and ring overlaps remain. Visuals and collision use the same native parts. No contact filtering, frozen body, extra damping or hidden support is introduced.

Actual Android component comparison: baseline sustained idle32–41ms/tick; repaired assembly reaches natural equilibrium and subsequent idle runs around8–10ms/tick on the same allowed CPU pair. Per-window CPU and wall traces are in CRANE_CATCH_LAG_REPAIR.json. Scheduling/thermal variation affects these proxy numbers; they are not foreground phone FPS. Initial natural settling still costs time and is a separate remaining startup concern.

The geometry regression rejects the original intruding dock part and accepts the repair, while checking retained decks and mouths. The full-world idle regression requires sustained material-point rest rather than merely passing a build. Ordinary arrival→cab→finite lift→grade exit remains an affected acceptance path. Exact-source CI/APK evidence is recorded separately after publication; no matching-APK or device responsiveness claim follows from this prototype.
