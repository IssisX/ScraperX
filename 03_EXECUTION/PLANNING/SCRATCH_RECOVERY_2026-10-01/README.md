# ScraperX source recovery archive

This archive preserves actual retained source payloads from the disconnected scratch session. Recovery is in progress. The source filesystem remains inaccessible. This is an archival copy outside the active game; no recovered file has been silently substituted into the running project.

Read [MANIFEST.json](MANIFEST.json) for each file's original path, Git blob SHA, completeness and provenance. Earlier snapshots and partial fragments are labeled individually. A complete earlier snapshot is not the final edited version. The full modified source tree is not yet backed up.

The detailed [recovery handoff](../CHATGPT_SCRATCH_RECOVERY_2026-10-01.md) preserves requirements, implementation facts, prior receipts and the running to-do. It is not a substitute for missing code. Keep the existing project authorities in charge.

## Recovery order

1. Secure each exact surviving source or patch payload and verify its GitHub readback.
2. Reconstruct final files only when a complete retained base and every successful edit are available; record gaps otherwise.
3. Recover the original scratch filesystem if the platform restores access, and archive tracked and untracked files before edits.
4. Reconcile recovered files with the original base in an isolated checkout; distinguish exact recovery from new implementation.
5. Rebuild and rerun the actual native and touch paths before claiming the candidate works.
