# SCRAPERX — EXECUTION PROTOCOL v1.1

**Status:** Engineering + requester execution authority  
**Product / repository:** `ScraperX`  
**Depends on:** `00_GOVERNING_LAWS.md`, `01_SCRAPERX_GDD.md`, `02_ASCENT_ATLAS.md`  
**Purpose:** Keep implementation narrow, evidence-driven, and faithful to ScraperX. Bind the person issuing work to the same spine as the model doing it.

---

## 1. AUTHORITY STACK

For implementation work, resolve conflicts in this order:

1. **Governing Laws** — non-negotiable project constraints.
2. **GDD** — product truth: what ScraperX must be.
3. **Ascent Atlas** — spatial/content truth for the same game: datum, bands, braids, modules, kernel slice.
4. **This Execution Protocol** — execution and claim rules, subordinate to product authority.
5. **Technical Architecture / TDD** — implementation ownership and system boundaries.
6. **Current source, tests, build configuration, and runtime evidence** — implementation truth.
7. **The one open ticket** — the exact bounded change being executed. That is a
   **Kernel Work Order** (`WO-000`–`WO-013`, `03_EXECUTION/KERNEL/`, closed set) or an
   **Ascent Slice** (`AS-*`, `03_EXECUTION/ASCENT/`; AS-001–015 are retired). The two are different classes of
   work and never share an identifier. `00_START_HERE.md` §4 is the vocabulary.

Writes land on **`ChatGPT`** only. Other branches are readable for provenance and are
never authorities.

Supporting decision/provenance documents are consulted only when a product decision needs tracing. They are not routine implementation context.

No work order may silently override a higher authority. If it conflicts, stop that mechanism and surface the conflict.

Do not create a parallel content brief outside `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`. If a band, module, or chain is missing, amend the atlas. Do not bolt on a second map.

---

## 2. CONTEXT LOADING RULE

Do **not** dump the entire project package into every coding task.

Every implementation task loads:

- this Execution Protocol;
- the one open ticket;
- the relevant source/tests/config;
- the specific Governing Law, GDD, and atlas sections that constrain the task;
- the relevant TDD sections.

Load additional GDD/atlas/support material only when the task crosses into it.

Historical WO references describe the atlas that existed when those records were written. New mechanism work loads the current Atlas datum, relevant candidate, receiving-support, recovery and acceptance sections. Do not revive the retired band queue through an old section reference.

The goal is **small active context under one global authority tree**, not reduced authority and not a second package.

---

## 3. TICKET CONTRACT

Every coding task has one bounded objective. An explicit owner-directed removal/reset may define that objective directly; it does not require a fictitious new mechanism ticket. New mechanism implementation uses one bounded ticket. Kernel Work Orders use the fields below
(`03_EXECUTION/TEMPLATES/KERNEL_WORK_ORDER_TEMPLATE.md`). Ascent Slices use these fields **plus** the
mechanical close in `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §8, which is not optional for them.

Every ticket must contain:

**Objective** — one concrete player-visible or system-visible capability.

**Existing truth** — what source/runtime/tests already prove.

**Authority** — exact Governing Law / GDD / TDD sections that constrain the change.

**Owner** — the subsystem that owns the consequential state being changed.

**Allowed seam** — the smallest existing interface/state/solver boundary through which the capability should be added.

**Forbidden shortcuts** — plausible wrong implementations that would fake, duplicate, or bypass authority.

**Proof path** — the actual build/runtime/device path that must demonstrate the result.

**Completion** — observable conditions that end the task.

If those fields cannot be stated clearly, the task is not ready to code.

The kernel is a **closed set**. No new `WO-*` is ever created; new work is an `AS-*` slice.

### 3.1 Ticket header

Every ticket opens with the same five lines, so that its state can be read without reading the
ticket. Three of them are **separate fields and never collapse into one status word**:

```
**Kernel Work Order:** `WO-___`        (or **Ascent Slice:** `AS-___`)
**Lifecycle:**          IMPLEMENTED | IN PROGRESS | PLANNED | UNAUTHORED
**Provenance:**         re-derived here | imported, NOT re-derived
**Implementation gate:** what must be true before source work may start
**Evidence:**           pointer to the ledger, never a copy of it
```

| Field | Answers | Changes when |
|---|---|---|
| `Lifecycle` | does source exist for this ticket? | someone writes code |
| `Provenance` | were its numbers derived against *this* tree? | someone re-authors it |
| `Evidence` | what has actually been observed? | **every CI run** |

Because those fields change at different rates, current integrated-run status lives once
in the `00_START_HERE.md` ledgers. Tickets point there and may retain explicitly dated
historical observations/causal diagnoses. Historical records are not active geometry or
parallel current-status instructions. READY/BLOCKED is separate from lifecycle and proof.

`IMPLEMENTED` is a statement about source, never about proof. A green falsifier is the only thing
that speaks for behaviour, and §11 governs what it is allowed to say.

`Provenance: imported, NOT re-derived` is a **hard gate**: the ticket's constants describe another
branch's world. It must be re-authored against this tree before any of it is coded.

The fourteen kernel files predate this header and keep their original `**Status:**` line. They are
historical execution records; rewriting them would retcon a proof record. New tickets use the
header above.

---

## 4. VERTICAL-SLICE RULE

Implement the **smallest complete causal slice**, not the smallest amount of code.

A valid slice should normally connect:

**player/input → authoritative state change → physical/system consequence → presentation/interaction result → verification**

A slice may cross several files or subsystems if the causal path genuinely requires it.

A slice must not opportunistically expand into unrelated systems because they are nearby.

Prefer one reusable primitive that unlocks several later behaviors over several disconnected features.

---

## 5. ONE OWNER PER CONSEQUENCE

Before writing code, identify who owns every consequential fact touched by the task.

Examples include:

- support/contact validity;
- machine position and capability;
- structural deformation/failure;
- rigging state;
- process/isolation state;
- route validity;
- mission predicates;
- checkpoint/persistence state.

Presentation may request and render. It may not independently decide an outcome owned elsewhere.

If two systems currently resolve the same consequential fact, treat that as an architectural defect before adding more behavior.

---

## 6. NO PROXY COMPLETION

The following do **not** prove a feature works:

- UI controls existing;
- animation playing;
- a variable changing;
- a unit test passing when the real runtime path differs;
- a desktop build when Android behavior is claimed;
- an APK being produced when execution is claimed;
- logs saying an event occurred when the world did not exhibit the consequence;
- a scripted effect standing in for authoritative mechanics.

Evidence must fit the claim.

Use:

- source/config for implementation facts;
- tests for invariant/contract facts;
- build output for build facts;
- runtime observation/logs for behavior;
- actual Fold-class execution for Fold behavior.

Never upgrade one evidence class into another.

---

## 7. SCRAPERX-SPECIFIC REJECTION TESTS

Reject or redesign an implementation if it:

- turns the tower into disconnected disposable spaces;
- weakens fast, physical parkour into generic FPS locomotion;
- cancels meaningful moving-support momentum;
- prevents legitimate falls with invisible safety;
- turns the parachute into magical recovery;
- scripts a causal consequence that the authoritative world should produce;
- makes mission/UI state override physical world truth;
- introduces unlimited-force or teleporting machinery;
- converts structural/process systems into cosmetic meters;
- deletes strategically useful aftermath to hide cost;
- converts the game toward conventional combat;
- protects an intended route against a legitimate physical sequence break;
- adds solver sophistication with no material player/system capability;
- creates a second authority because integration was convenient;
- cannot survive the real Android shipping path.

An exciting feature does not receive an exemption.

---

## 8. CHANGE CLASSIFICATION

Before execution, classify the task:

### BUG / REGRESSION
Reproduce → capture evidence → identify owner → test plausible causes → repair owner/seam → rerun the same failing path.

Do not optimize or redesign before the mechanism is known.

### FEATURE / CHANGE
Preserve existing contracts unless the objective requires changing them → identify owner/seam → add the smallest reusable capability → prove the actual path.

### ARCHITECTURE
Architecture changes require demonstrated inability of the current boundary to support a required capability, correctness property, or shipping constraint.

“Cleaner,” “more modern,” or “more sophisticated” alone is insufficient.

---

## 9. EXTERNAL TECHNOLOGY GATE

Libraries, physics engines, extensions, middleware, and packages are candidates, not trophies.

Adopt one only when it:

1. solves a defined ScraperX requirement;
2. has a bounded ownership role;
3. does not create duplicate consequential authority;
4. survives the actual Android build/runtime path;
5. materially improves capability, correctness, robustness, performance, or production leverage;
6. beats the simpler alternative with evidence.

Do not select Jolt—or replace Jolt—by reputation alone.

---

## 10. PERFORMANCE RULE

The current target is sustained **45 FPS on Galaxy Fold 6-class Android hardware** under representative play.

When over budget, first reduce:

- visual cost;
- redundant detail;
- inactive-region update frequency;
- active extent;
- nonconsequential simulation detail.

Do not erase strategically meaningful world truth merely to hit the frame target.

Optimization must preserve authoritative state and later reactivation.

---

## 11. CLAIM DISCIPLINE

Every meaningful completion report distinguishes:

**Implemented** — source changed.

**Built** — target artifact compiled successfully.

**Installed** — artifact installed on target.

**Executed** — relevant path ran.

**Observed** — expected behavior was actually seen.

**Verified on Fold** — correct behavior was observed on the real target class.

Never collapse these into “done.”

Unknown remains unknown.

---

## 12. STOP RULE

Stop the task when the Work Order completion condition is proven.

Do not continue adding polish, adjacent systems, cleanup, architecture, or speculative improvements unless they are required to make the current capability correct.

If a newly discovered defect blocks the objective, repair it.

If it does not block the objective, record it separately and stop.

---

## 13. TDD PRODUCTION RULE

The Technical Architecture / TDD must be derived from the frozen game, not used to redefine it.

The TDD must establish:

- consequential-state ownership;
- Godot / native / external-physics boundaries;
- update and synchronization contracts;
- persistence representation;
- streaming/sleep/reactivation rules;
- deterministic/reproducibility requirements where needed;
- Android build/deployment architecture;
- subsystem verification strategy.

It must not add gameplay merely because an implementation technique makes that gameplay convenient.

---

## 14. PROJECT-GUARDIAN RULE

When a requested implementation would damage the frozen ScraperX objective, the correct response is to **reject the damaging mechanism**, explain the concrete conflict, and preserve the legitimate underlying goal through a compatible alternative.

Compliance is not success.

The success criterion is a real ScraperX capability that survives its authorities, runtime, and shipping path.

---

## 15. REQUESTER PROTOCOL

This section binds Cory, and anyone briefing an implementation model.

The requester’s workstation may be the Fold. That is normal. It is the shipping target. Tedium is not a license to skip work orders. It is a license to stop pretending the human is a desktop file clerk.

### Legal asks

- `Execute AS-NNN. Stop at its completion condition.` — the normal ask
- `Author AS-NNN.` — write the plan, no code
- `status`
- `fix:` + the broken evidence
- `amend:` + one atlas module / one TDD gate / one ticket field
- `Record this claim at class implemented|built|installed|executed|observed|Fold.`

One current implementation slice. One change class. One proof path. A cross-document planning audit is not multiple concurrent mechanism implementations.

The requester does **not** paste Laws, GDD, atlas, TDD, or the WO file when the model already has this package. Pasting is an implementation-AI duty, not a Fold-thumb duty.

### Requests that need bounded execution

Resolve these into the smallest complete authorized slice; do not silently start the whole tower:

- build the game / the tower / the 1.6 km climb / “make it causal”;
- dump the whole package as one prompt and expect a world;
- write another protocol, atlas, GDD, or ticket pack while the open ticket is unexecuted;
- start a band the open slice does not own;
- treat desktop, web, video, or a screenshot as Fold proof.

Asking how to operate from the Fold is legal. Using process-chat to avoid `000` after that answer is not.

### Document rule

New prose is justified when a ticket cannot name owner, seam or proof, or when the user explicitly requests a documentation audit/reconciliation.

For implementation requests, execute a ready ticket; do not substitute process writing. For an authorized planning audit, repair contradictory document owners together without changing gameplay.

### Model rule

Use the owner’s intent to establish a concrete bounded result. A new explicit owner decision may supersede an old content plan; update affected owners together. Do not use retired paperwork to prevent an authorized removal or to demand that the user perform project administration.

Load files yourself. Return one artifact or one status block. Do not assign copy-paste homework.

---

## 16. FOLD WORKSTATION RULE

Galaxy Fold 6 is both the proof device and, until a desktop exists, the only console.

Therefore:

- commands must be thumb-legal: one line naming one ticket;
- deliverables are one downloadable artifact or a short status, not a reading list;
- remote/CI build is the intended compile path (TDD §20.3);
- “open these eight markdown files and paste them” is a protocol defect, not a user defect.

Friction may be reduced. Scope may not. One open ticket at a time, always.

## 17. Candidate and documentation publication

Run applicable local checks before publication. Code candidates go only to `ChatGPT`,
one at a time; no intentional concurrent APK candidates. Follow the exact run to
terminal status. Estimate from comparable completed workflow/job paths, check around
that estimate plus two minutes, and keep inspecting the same run if unfinished.
A timer is not proof. On RED repair the first causal failure from its job/log;
on GREEN verify all expected jobs and the APK/artifact. Stop at the slice boundary.

Documentation-only checks establish source/reference consistency, derivations and
formatting, not new gameplay behavior or a new APK. Retain historical gameplay
proof at its actual commit. Updating a plan does not repair a runtime defect it
records, and none of these claim distinctions waive required code-candidate gates.

## 18. Ground-reset proof boundary

The normal scene and explicit regression fixtures are separate proof targets. Native default construction must omit all retired campaign bodies/cables; normal presentation and exported collision must omit their geometry. Retained old tests must select fixtures explicitly and must never be reported as current campaign progress. Preserve controller tests; add a default-world removal/input proof and a rendered capture. A planned mechanism remains unimplemented until its actual normal-input chain and receiving support are demonstrated.
