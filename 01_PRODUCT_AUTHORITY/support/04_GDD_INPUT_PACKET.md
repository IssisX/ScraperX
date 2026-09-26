# ScraperX — GDD Input Packet

**Status:** PRE-GDD INPUT — CLOSURE COMPLETE  
**Purpose:** Consolidate only frozen laws, established product requirements, and direct user decisions. This packet does not permit unsupported inference.

## Authoritative inputs
- `00_GOVERNING_LAWS.md`
- `01_CURRENT_REQUIREMENTS_LEDGER.md`
- `02_PRODUCT_DECISIONS.md`
- `03_OPEN_DECISIONS_AND_TENSIONS.md`
- `05_PRE_GDD_CLOSURE_DECISIONS.md`

## Directly selected design decisions
- **During normal traversal, what camera perspective should ScraperX use?** — First-person
- **What should the first 10–15 minutes actually do?** — Drop directly into a real physical problem
- **What is the durable reason the player keeps climbing?** — A concrete summit objective
- **What should the large-scale ascent topology feel like?** — Several braided ascent routes
- **What should the parkour controller feel like in your hands?** — Fast and precise
- **How should traversable geometry communicate itself?** — World geometry and physical intuition
- **How much should fatigue limit ordinary parkour?** — Situational fatigue only
- **How often should ordinary ascent expose the player to the outside?** — Frequent exterior traversal
- **What should be the most common reason upward progress is physically blocked?** — A missing physical capability or tool
- **How often should major ascent problems require more than one macro system?** — Strongly region-dependent
- **How much of the accessible tower should be structurally alterable in consequential ways?** — Nearly all reachable structural elements matter
- **When controlling a large machine, how literal should operation be?** — Richer local station controls
- **How free should the player be when creating rigging and load connections?** — Mixed attachment model
- **Where should new physical capability mainly come from?** — Things like forklifts or cranes can do a lot. The player should have ability to lift and push and move things into place too.
- **How central should deliberate damage be as a problem-solving verb?** — Constructive manipulation first
- **How harsh should committed physical mistakes usually be?** — Live with it unless you can physically recover
- **How should the skyscraper’s meaningful physical layout be authored?** — Authored modules with controlled recombination
- **How populated should ordinary tower regions feel?** — Strongly varies by layer
- **How much autonomous behavior should inhabitants have beyond immediate missions?** — Systemic workers/operators
- **How should objectives usually enter play?** — Hybrid authored + discovered
- **When a local objective becomes impossible or is failed, what should usually happen?** — Important mission failure usually restores checkpoint
- **How much conventional combat belongs in ScraperX?** — No conventional combat
- **How strongly authored should story presentation be?** — Mostly environmental and situational
- **Outside of falling reactions, how defined and talkative should the player character be?** — Contextual personality
- **How should checkpoint commits usually be encountered during ascent?** — Frequent automatic commits
- **What kind of equipment should the parachute be?** — Always-carried reusable system
- **How free should the player be to ignore the current main upward objective?** — Highly self-directed tower exploration
- **What technological language should the tower primarily speak?** — Contemporary heavy industry at impossible scale
- **What should dominate the emotional tone when the player is not actively falling?** — Kinetic industrial adventure
- **How much non-diegetic guidance should ordinary play show?** — Sparse contextual HUD

## Direct closure decisions

- **What concrete state completes the main ascent?** — Physically reach the summit.
- **Without machinery, what scale of objects should the player physically move?** — Slightly heroic but still grounded.
- **If an objective becomes impossible because of committed world changes while the player is still alive, what should happen?** — Explicit hard-fail missions may rollback.
- **When does “authored modules with controlled recombination” change the tower layout?** — Development-time assembly only.

## Clarifications produced by closure

- **Summit objective:** the concrete campaign completion state is physically reaching the summit.
- **Manual vs machinery capability:** unaided manipulation is slightly heroic but still grounded; major lifting/pushing capability is supplied by appropriate world machinery and mechanisms.
- **Failure semantics:** persistent committed consequence remains the default; checkpoint rollback is reserved for explicitly designated hard-fail missions.
- **Tower authorship:** controlled module recombination is a development-time construction method. The shipped/playthrough tower layout is not runtime-recombined by that decision.

## Unresolved pre-GDD blockers
- None identified by the completed interview + closure pass.

## Non-blocking follow-through
- Fast first-person parkour plus frequent extreme-height exposure creates a motion-comfort/accessibility requirement for later embodiment/presentation engineering. It does not invalidate the selected movement or exterior-exposure design.

## Synthesis rule
Only frozen laws, established product requirements, direct questionnaire selections, and direct closure decisions may be promoted into the GDD.

Where an earlier selection is ambiguous, the explicit closure decision controls its interpretation.

No implementation architecture, historical material, package choice, lore, mechanic, or inferred requirement may be added merely to make the GDD feel complete.
