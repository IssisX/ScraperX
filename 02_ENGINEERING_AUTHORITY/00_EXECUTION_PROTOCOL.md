# SCRAPERX — EXECUTION PROTOCOL v1.1

**Project-wide motto: “The input state determines the output.”** For every change, identify authoritative inputs, the causal transition and observable output. Verification must distinguish changed input/state from an incorrect or canned result. This applies across gameplay, machines, touch controls, presentation and feedback.

**Owner collaboration preference:** Interpret rough requests thoughtfully and turn their underlying intent into coherent, polished behavior. Use engineering and design judgment; do not mechanically copy a proposed mechanism that would weaken the objective. Explain material tradeoffs, absorb routine choices, make authorized edits as needed and finish useful work without repeated permission prompts. Keep decision/progress updates concise. Ask only when missing information materially changes the result.


**Status:** Engineering + requester execution authority  
**Product / repository:** `ScraperX`  
**Depends on:** `00_GOVERNING_LAWS.md`, `01_SCRAPERX_GDD.md`, `02_ASCENT_ATLAS.md`  
**Purpose:** Keep implementation narrow, evidence-driven, and faithful to ScraperX; turn the owner’s intent into concrete work and accurate evidence without assigning project administration to the owner.

---

## 1. AUTHORITY STACK

Apply the owner’s latest explicit instruction first when it changes an earlier project decision. Reconcile the affected owners below; do not use their old wording to block that instruction. Otherwise resolve project-document conflicts in this order:

1. **Governing Laws** — non-negotiable project constraints.
2. **GDD** — product truth: what ScraperX must be.
3. **Ascent Atlas** — spatial/content truth for the same game: datum, active opening route, authored receiving supports, upper-route placement and summit destination.
4. **This Execution Protocol** — execution and claim rules, subordinate to product authority.
5. **Technical Architecture / TDD** — implementation ownership and system boundaries.
6. **Current source, tests, build configuration, and runtime evidence** — implementation truth.
7. **The current bounded task** — the exact authorized change being executed. New ascent
   implementation uses an **Ascent Slice** (`AS-*`, `03_EXECUTION/ASCENT/`; AS-001–015 are retired).
   **Kernel Work Orders** (`WO-000`–`WO-013`, `03_EXECUTION/KERNEL/`) are closed historical
   records, not open implementation tickets. `00_START_HERE.md` §4 is the vocabulary;
   §3 below covers directly authorized resets and §15 covers documentation audits.

Writes land on **`ChatGPT`** only. Use this branch's documents and source for routine work.
Consult another branch only to answer a specific unresolved provenance or reuse question;
its content never becomes current authority merely by being newer or further along.

Supporting decision/provenance documents are consulted only when a product decision needs tracing. They are not routine implementation context.

An agent-authored work order cannot silently redefine product authority. Resolve an apparent conflict against the latest explicit owner instruction and current evidence; ask only when a material ambiguity cannot be resolved from them. Source/tests establish what exists, not whether an obsolete design remains desired.

Do not create a parallel content brief outside `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`. If a band, module, or chain is missing, amend the atlas. Do not bolt on a second map.

---

## 2. CONTEXT LOADING RULE

Do **not** dump the entire project package into every coding task.

Every implementation task loads:

- this Execution Protocol;
- the current ticket or directly authorized bounded task;
- the relevant source/tests/config;
- the specific Governing Law, GDD, and atlas sections that constrain the task;
- the relevant TDD sections.

Load additional GDD/atlas/support material only when the task crosses into it.

Historical WO references describe the atlas that existed when those records were written. New mechanism work loads the current Atlas datum, relevant candidate, receiving-support, recovery and acceptance sections. Do not revive the retired band queue through an old section reference.

The goal is **small active context under one global authority tree**, not reduced authority and not a second package.

### 2.1 Specialist agents and hooks

Use available specialists proactively within the current implementation slice. Recon/mapper agents locate owners and call paths; diagnosticians isolate failures; architects and ScraperX mechanics resolve causal design; implementers own a defined file slice; documentation auditors reconcile affected authorities; reviewers and verifiers independently assess correctness and evidence. Use the closest available capability when a named role is absent.

Give each agent a bounded objective, relevant context, an evidence boundary and explicit write ownership. Independent log/artifact work, targeted reference checks and documentation audits may run while the parent continues integration. Preserve concurrent edits, avoid duplicate scans and runtime checks, and coordinate shared build/runtime resources. Supporting agents do not start additional gameplay slices. The parent owns integration and the single publication path under §17.

Use installed context and protection hooks where supported. Inspect their configuration and distinguish helper checks from observed live dispatch; report absent or unproven coverage honestly. Hooks support the existing ownership and preservation rules rather than adding routine permission gates.

---

## 3. TICKET CONTRACT

Every coding task has one bounded objective.

The agent derives and records that objective from the owner’s natural-language request. The owner need not supply a ticket or use prescribed wording. A larger authorized objective proceeds through verified slices without silently shrinking to the first slice. An explicit owner-directed removal/reset may define that objective directly; it does not require a fictitious new mechanism ticket. New mechanism implementation uses one bounded ticket. Kernel Work Orders use the fields below
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

**Player problem** — what the player must notice, decide and change; why it differs from recent encounters; how the physical result contributes to the receiving route. Use GDD §16 and the existing mechanical close rather than creating another content authority.

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

`IMPLEMENTED` is a statement about source, never about proof. Behaviour claims require
the exercised path and observations appropriate to the claim under §6 and §11; a passing
isolated test or negative control does not by itself prove a complete route.

For new ascent tickets, `UNAUTHORED` means no slice contract exists, `PLANNED` means a
contract exists but implementation has not started, `IN PROGRESS` includes partial or
locally staged work, and `IMPLEMENTED` means the ticket's source is integrated into the
active `ChatGPT` game. An ignored snapshot, integration patch or another branch's source
does not establish integration. `DELIVERED` describes evidence for an exact candidate;
it is not a source lifecycle value. Record that evidence in Start Here and dated delivery
records, without promoting it to proof of device execution.

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

Reuse verified bodies, shapes, joints and owner interfaces when they fit the derived assembly. Reusing engineering does not justify repeating the same player problem. Compare the intended experience against GDD §16 before selecting a familiar mechanism.

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

For visual evidence, explicitly invoke the available capture path, verify that genuine images were produced, open them and inspect the relevant approach, action/contact, motion and supported arrival. Record the source/build, scenario and views actually inspected. A capture filename or passing log is not visual inspection. Screenshots can be collected independently of APK builds; neither proves the other. Still frames alone cannot prove impulse transfer or continuity, so pair them with runtime state traces and normal-input execution.

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

Read the actual installed instructions for `superpowers`, `causal-mechanism-compiler` and `threespine` when the task calls for them. Apply the compiler before implementing a consequential assembly and ThreeSpine to its coupled dynamics/contact/traversal calculations; do not use skill names as evidence of completed work. Recover head, branch, local changes, active route and authoritative object identity before changing code. When a request targets a specific object that cannot be uniquely located, obtain the missing locator; when the owner authorizes a new obstacle generally, select a bounded design instead of inventing a missing-reference blocker.

Minimize unsupported shortcuts: trace construction, collision, motion type, attachment, player contact, support tracking and rendered pose; repair the earliest broken causal connection. Distinguish measured facts, derivations, chosen design parameters and unresolved quantities. Valid reduced models remain permitted under Governing Laws §25: name assumptions, applicability and error/verification limits, and compare the consequential result with the implemented owner. A familiar pattern or plausible number is not evidence that this assembly closes.

For ascent, report supported walking-surface elevation above grade, and identify the
route: newly authored campaign, ordinary fallback, or regression fixture. Capsule-centre
height, bridge-tip height, another branch's route and future destination targets are
different measurements. State source integration, observed traversal, APK production
and device execution separately. Current phase and delivery status belongs in
`00_START_HERE.md` §2; layout belongs in the Atlas and future order in the ascent plan.

---

## 12. STOP RULE

Stop adding to a bounded slice when its completion condition is proven. If the owner has authorized a larger objective, continue with the next necessary slice within that objective; a slice boundary is not permission to abandon the remaining authorized work. Report an exact blocker when progress genuinely depends on missing information or unavailable execution.

Do not continue adding polish, adjacent systems, cleanup, architecture, or speculative improvements unless they are required to make the current capability correct.

If a newly discovered defect blocks the objective, repair it.

If it does not block the objective, record it separately and keep it outside the current slice. Continue any remaining authorized objective under the first paragraph of this section.

---

## 13. TDD PRODUCTION RULE

The Technical Architecture / TDD follows the established game and its latest explicit owner amendments. An implementation technique cannot redefine product direction by itself.

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

Check a proposal against current owner intent, physical feasibility and the verified route. Explain a concrete conflict and offer a viable assembly or implementation when needed. The owner may change earlier product decisions; this rule does not authorize overriding that choice with stale documentation. Success requires the intended capability, an honest causal implementation and evidence through the relevant runtime and delivery path.

---

## 15. REQUESTER PROTOCOL

The agent is responsible for turning natural-language direction into bounded, reviewable work. The requester may work entirely from the Fold; requests need no formal syntax and no document-management homework.

### Legal asks

These are optional shorthand examples, not a whitelist or a prerequisite for authorization:

- `Execute AS-NNN. Stop at its completion condition.` — the normal ask
- `Author AS-NNN.` — write the plan, no code
- `status`
- `fix:` + the broken evidence
- `amend:` + one atlas module / one TDD gate / one ticket field
- `Record this claim at class implemented|built|installed|executed|observed|Fold.`

One current implementation slice. One change class. One proof path. A cross-document planning audit is not multiple concurrent mechanism implementations.

The requester does **not** paste Laws, GDD, atlas, TDD, or the WO file when the model already has this package. Pasting is an implementation-AI duty, not a Fold-thumb duty.

### Requests that need bounded execution

Resolve these into a concrete objective and an ordered set of complete slices. Preserve the scope explicitly authorized; do not pretend a broad goal is already designed or complete:

- build the game / the tower / the 1.6 km climb / “make it causal”;
- dump the whole package as one prompt and expect a world;
- write another protocol, atlas, GDD, or ticket pack while the open ticket is unexecuted;
- start a band the open slice does not own;
- treat desktop, web, video, or a screenshot as Fold proof.

Asking how to operate from the Fold is legal. Resume the authorized bounded task after
answering; the closed WO-000 delivery work is not a prerequisite to repeat.

### Document rule

New prose is justified when a ticket cannot name owner, seam or proof, or when the user explicitly requests a documentation audit/reconciliation.

For implementation requests, execute a ready ticket; do not substitute process writing. For an authorized planning audit, repair contradictory document owners together without changing gameplay.

Preserve filenames, headings/section numbers, ticket fields, AS/WO/REQ/KX and entity identifiers, and existing link targets during reconciliation. Edit the substance in its current owner. Keep historical observations tied to their original source and date. Check affected references and compare headings, IDs and local links before publishing; do not renumber or repurpose identities to make the prose cleaner. Any necessary reference change needs an explicit compatibility review.

### Model rule

Develop rough, incomplete or weak ideas into coherent proposals using the owner’s intent and the verified world. Compare viable alternatives, resolve practical consequences and exercise independent engineering judgment within the authorized scope. Record which requirements came directly from the owner, which conclusions are derived, which parameters are chosen and which questions remain unresolved. Derived design guidance may improve an idea without being presented as a new owner decision or an already proven capability. Ordinary design work belongs to the agent; ask only for information or choices whose absence materially prevents sound progress.

Use the owner’s intent to establish a concrete bounded result. A new explicit owner decision may supersede an old content plan; update affected owners together. Do not use retired paperwork to prevent an authorized removal or to demand that the user perform project administration.

Load files yourself. Return one artifact or one status block. Do not assign copy-paste homework.

---

## 16. FOLD WORKSTATION RULE

Galaxy Fold 6 is both the proof device and, until a desktop exists, the only console.

Therefore:

- requests may be ordinary language or a short ticket reference; the agent resolves the execution contract;
- deliverables are one downloadable artifact or a short status, not a reading list;
- every APK build runs through GitHub Actions (TDD §20.3); local native checks remain useful evidence within their stated platform limits;
- “open these eight markdown files and paste them” is a protocol defect, not a user defect.

Use an agent-owned clone of `IssisX/ScraperX` on `ChatGPT`, in temporary storage outside personal folders; verify its path, remote, branch, head and working tree before writes, and preserve others’ changes. A starting cwd does not identify the project. Do not use the owner’s original files or Android Downloads as a development tree or artifact destination. If an operation genuinely needs another location, explain that reason first. Follow the current environment’s permissions. Keep one implementation slice active while preserving the authorized larger objective.

## 17. Candidate and documentation publication

Run applicable local checks before publication. Code candidates go only to `ChatGPT`,
one at a time; no intentional concurrent APK candidates. Follow the exact run to
terminal status. Estimate from comparable completed workflow/job paths, check around
that estimate plus two minutes, and keep inspecting the same run if unfinished.
A timer is not proof. On RED repair the first causal failure from its job/log;
on GREEN verify all expected jobs and the APK/artifact. Close the slice, then follow §12 for any remaining authorized objective.

Documentation-only checks establish source/reference consistency, derivations and
formatting, not new gameplay behavior or a new APK. Retain historical gameplay
proof at its actual commit. Updating a plan does not repair a runtime defect it
records, and none of these claim distinctions waive required code-candidate gates.

## 18. Ground-reset proof boundary

The normal scene and explicit regression fixtures are separate proof targets. Native default construction must omit all retired campaign bodies/cables; normal presentation and exported collision must omit their geometry. Retained old tests must select fixtures explicitly and must never be reported as current campaign progress. Preserve controller tests and retain the default-world removal/input proof and rendered capture. Source integration establishes `IMPLEMENTED`; acceptance still requires the actual normal-input chain, supported arrival and applicable delivery gates. Never report an implemented but unverified mechanism as an accepted playable delivery.
