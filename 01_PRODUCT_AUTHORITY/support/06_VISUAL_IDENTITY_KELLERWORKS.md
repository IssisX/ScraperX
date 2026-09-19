# Visual Identity — Kellerworks

**Status:** Product decision, provenance for WO-007 onward.
**Authority order:** subordinate to `00_GOVERNING_LAWS.md` and `01_SCRAPERX_GDD.md`. Nothing here contradicts either — the frozen GDD names no company, setting, climate, or time of day, so this fills open territory rather than overriding fixed text.

## Source

Cory-supplied concept reference, 2026-09-17: a daylight approach shot of a riveted steel-and-timber tower against snow peaks and a waterfall, carrying "Kellerworks" branding, propaganda-style banners, a numbered lift cage, and a working crane lifting a crate. Treated as art direction, not as a literal build target — the reference is a single hero angle; ScraperX is a persistent, walkable world, so its identity has to hold up from every angle and under the native machine's actual state, not just this one.

## Decisions this establishes

1. **Operator identity.** The tower is built and run by **Kellerworks**. Its mark is a plain angular chevron-K, rendered in signage, never as a licensed or copied logotype. Kellerworks speaks in short declarative propaganda: work is virtuous, height is destiny, the company built the mountain a home. Tone is sincere, not ironic — this is company-town optimism, not satire.
2. **Setting: alpine, not urban-night.** The tower rises out of a mountain valley, not a city block. Snow peaks and a waterfall are visible past the yard. This **supersedes** the ad hoc "urban night yard" setting improvised for WO-004/005 — that was scaffolding filled in without a reference; this document is the real one. The yard, plant, and traversal fixtures built in WO-002/003/006 keep their native geometry and world positions unchanged; only the backdrop, palette balance, and lighting move to match.
3. **Light: overcast daylight, sodium as accent.** The reference is lit by a bright, soft overcast sky — flat, shadow-hazy daylight, not night. Warm sodium/incandescent fittings (lamps, the fire box, banister lanterns) read as small warm accents against that cool daylight, exactly inverted from the night-dominant lighting WO-004 built. Weld-glow and vent-glow stay tied to real machine state per WO-006; they do not become the primary light source.
4. **Material identity, refined not replaced.** WO-004's palette (mill scale, oxidised steel, poured concrete, galvanised mesh, faded yellow, chipped hazard orange) is correct and stays. The reference adds heavy timber cladding as a secondary material — add it as an accent on the tower face and plant structure, not a replacement for steel.
5. **The machine is legible from outside.** The reference telegraphs function through exposed mechanism — a huge gear, a wound drum — visible mid-structure. ScraperX already has a real, authoritative machine (WO-006); this document licenses adding a **decorative** exposed gear/drum motif on the tower face whose rotation is driven by the real `machine_cycle_phase_seconds`, so it reads as the visible face of the actual plant rather than arbitrary set dressing. It carries no collision and no authority — Governing Law 26 still applies; it must never be mistaken for a second physics source.
6. **Scale is told through working machinery in frame.** A crane with a suspended load is present near the approach for scale, exactly as GDD §1 asks for ("built from large-scale, life-size machinery"). It is presentation-only ambient motion (a slow clock-driven sway), explicitly not claimed as simulated rigging — that stays reserved for native-authoritative freight per TDD §9, not yet built.
7. **Wayfinding signage.** Industrial lifts and machines carry stencilled identifiers (the reference's "LIFT A" / cage "3"). ScraperX adopts painted signage as presentation: a display designation distinct from the internal native entity ID, exactly as real plant equipment tags differ from asset numbers.

## Explicitly not decided here

- Whether every region of the tower is alpine, or whether higher altitude regions change climate (glacier, cloud layer, etc.) — later WOs' concern, GDD §3 already allows regional variation.
- Any real-world company, mark, or protected branding. "Kellerworks" and its chevron-K are original to this project.
- Multiplayer, or any lore beyond what a banner needs to say in six words.

## Traceability

Implemented by WO-007. Read this document before touching tower-face dressing, yard atmosphere, or banner/signage content so later work stays consistent with one identity instead of drifting per-author.
