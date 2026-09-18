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
4. **Technical Architecture / TDD** — implementation ownership and system boundaries.
5. **Current source, tests, build configuration, and runtime evidence** — implementation truth.
6. **Current Work Order** — the exact bounded change being executed.

Supporting decision/provenance documents are consulted only when a product decision needs tracing. They are not routine implementation context.

No work order may silently override a higher authority. If it conflicts, stop that mechanism and surface the conflict.

Do not create a parallel content brief outside `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`. If a band, module, or chain is missing, amend the atlas. Do not bolt on a second map.

---

## 2. CONTEXT LOADING RULE

Do **not** dump the entire project package into every coding task.

Every implementation task loads:

- this Execution Protocol;
- the current Work Order;
- the relevant source/tests/config;
- the specific Governing Law, GDD, and atlas sections that constrain the task;
- the relevant TDD sections.

Load additional GDD/atlas/support material only when the task crosses into it.

WO-000 does not need the atlas. WO-001–004 need Atlas §§2 and 9. WO-005–008 need Atlas §§4, 7–9, 12.

The goal is **small active context under one global authority tree**, not reduced authority and not a second package.

---

## 3. WORK ORDER CONTRACT

Every coding task must begin with a bounded Work Order containing:

**Objective** — one concrete player-visible or system-visible capability.

**Existing truth** — what source/runtime/tests already prove.

**Authority** — exact Governing Law / GDD / TDD sections that constrain the change.

**Owner** — the subsystem that owns the consequential state being changed.

**Allowed seam** — the smallest existing interface/state/solver boundary through which the capability should be added.

**Forbidden shortcuts** — plausible wrong implementations that would fake, duplicate, or bypass authority.

**Proof path** — the actual build/runtime/device path that must demonstrate the result.

**Completion** — observable conditions that end the task.

If those fields cannot be stated clearly, the task is not ready to code.

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

- `000` … `008` or `Execute WO-NNN. Stop at its completion condition.`
- `status`
- `fix:` + the broken evidence
- `amend:` + one atlas module / one TDD gate / one WO field
- `Record this claim at class implemented|built|installed|executed|observed|Fold.`

One current work order. One change class. One proof path.

The requester does **not** paste Laws, GDD, atlas, TDD, or the WO file when the model already has this package. Pasting is an implementation-AI duty, not a Fold-thumb duty.

### Illegal asks

Reject these and name the current WO instead:

- build the game / the tower / the 1.6 km climb / “make it causal”;
- dump the whole package as one prompt and expect a world;
- write another protocol, atlas, GDD, or work-order pack while the current WO is unexecuted;
- start B01–B11 content before WO-008 is proven;
- treat desktop, web, video, or a screenshot as Fold proof.

Asking how to operate from the Fold is legal. Using process-chat to avoid `000` after that answer is not.

### Document rule

New prose is legal only when the current work order cannot name owner, seam, or proof without it.

If the current WO can be executed, execute it. Do not derive process.

### Model rule

If the requester issues an illegal ask, do not comply and do not soothe. Point at the current WO. If they insist on a whole-tower build, that is a Project-Guardian reject, not a bigger prompt.

Load files yourself. Return one artifact or one status block. Do not assign copy-paste homework.

---

## 16. FOLD WORKSTATION RULE

Galaxy Fold 6 is both the proof device and, until a desktop exists, the only console.

Therefore:

- commands must be thumb-legal (see `00_START_HERE.md` Fold-only operator);
- deliverables are one downloadable artifact or a short status, not a reading list;
- remote/CI build is the intended compile path (TDD §20.3);
- “open these eight markdown files and paste them” is a protocol defect, not a user defect.

Friction may be reduced. Scope may not. WO-000 is still the first code.
