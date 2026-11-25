# SimpleDAW specifications and development plan

These PDFs are the shared reference set for specification reviews and phased development. Preserve the dated PDFs as supplied; use Git history and reviewed documentation PRs for revisions.

| Document | Scope and status |
| --- | --- |
| [Master Specification](SimpleDAW_Master_Spec_With_Pending.pdf) | Consolidated working specification, 20 November 2026; approved Reviews 1–74, Review 75 pending, Reviews 76–90 proposed. 8 pages. |
| [Universal Actions Integration Specification](SimpleDAW_Universal_Actions_Integration_Spec.pdf) | Integration draft and decision register, 20 November 2026; separates accepted principles, pending details and recommendations. 3 pages. |
| [Development Phase Plan — Draft 0.1](SimpleDAW_Development_Phase_Plan_Draft_0_1.pdf) | Preliminary phases 0–12, dependencies and acceptance criteria, 20 November 2026. Phase identifiers are not application versions. 3 pages. |

## Current Master Specification amendment

- [MIDI track routing](midi-track-routing.md) — approved amendment to Master Specification sections 8 and 14. Governs Setup, both Inspectors, Piano Roll audition, MIDI import and saved-project compatibility.

## Subsequent acceptance and change handoffs

- [v0.24.1 Piano Roll P0 acceptance](../research/piano-roll-p0.md#acceptance) — **ACCEPTED and merged** in PR #24. The owner passed P01–P12 and F01–F05, and the final Windows x64 Release `/MD` run passed 14/14 CTests.

- [v0.22 design decisions — Reviews 01–82](v0.22-design-decisions-reviews-01-82.md) — accepted UAT and later-phase recording design decisions; Review 83 pending. v0.22.1 is limited to the UAT foundation, not advanced recording features.
- [v0.21 acceptance and v0.22 handoff](v0.21-acceptance-v0.22-handoff.md) — records accepted live testing, documented limitations and candidate v0.22 work. This supplement does not supersede the dated PDFs or approve pending designs.

- [v0.22.2 toolbar and creation-controls handoff](v0.22-design-decisions-reviews-01-82.md#v0222-toolbar-and-creation-controls-handoff--accepted-2026-11-20) — approved CUT replacement for CREATE, permanent global Undo/Redo, minimum-window sizing, deferred circular + creation controls, and Inspector-specific Duplicate/Clone/Loop.

- [v0.22.2 expanded implementation task sheet](../tasks/v0.22.2-first-task-sheet.md) — approved combined EDIT/MOVE/SIZE + Undo/Redo scope; not yet implemented or tested.\n\n
- [Clip Glue, Groups, Parent/Child Tracks and MIX/REPLACE (21 Nov 2026)](clip-glue-group-parent-overlap-2026-11-21.md) — **ACCEPTED design, not implemented**; newer supplement governing clip actions, group badges, track hierarchy, bus routing, and audible overlap/waveform precedence. Planned for later phases; does not alter v0.22.2 scope.


- [v0.23.1 Inspector, Clone/Loop and MIDI Import decisions](v0.23.1-inspector-and-midi-import.md) — **ACCEPTED DESIGN, NOT IMPLEMENTED**; authority for Track, Audio Clip and MIDI Clip Inspectors and clone/loop semantics, as amended by the MIDI track routing decision; supersedes conflicting older descriptions. Historical Master Spec PDF is preserved.
- [v0.23.1 Codex first-iteration task sheet](../tasks/v0.23.1-inspector-first-iteration.md) — implementation scope, E1 integration boundaries, exclusions, acceptance tests and Windows /MD build policy. Merge documentation before starting implementation.

## How to use this set

Read the repository [instructions](../../AGENTS.md) and [project status](../../PROJECT_STATUS.md) before assigning work. Accepted design does not mean implemented, merged or tested. Pending and recommended items require decisions before implementation.

The Universal Actions supplement records newer interaction decisions; reconcile explicit replacements with the master specification while preserving approved functionality. Flag unresolved conflicts rather than inventing behavior. Maintain specification decisions in the SimpleDAW specification chat and bring approved outcomes from development/testing chats back into the shared documents through review.

The phase plan records v0.21 as implemented on its development branch with live acceptance pending at its review date. Its references to accepting/merging PR #3 are future phase tasks, not authorization in this documentation PR. Check current repository and PR evidence before starting a phase. The initial audio-engine Phase 1 and the new plan's Phase 1 Universal Actions foundation are different planning contexts.

## Verification of this import

All three original PDFs were parsed and all 14 pages rendered for visual inspection. The imported files are byte-for-byte copies; no PDF content was rewritten. No application builds, automated application tests or hardware acceptance tests ran for this documentation import.

| File | SHA-256 |
| --- | --- |
| `SimpleDAW_Master_Spec_With_Pending.pdf` | `09e221586f2e3686becad6863152e74e3459f51d39168a2cf601bc74033da7f0` |
| `SimpleDAW_Universal_Actions_Integration_Spec.pdf` | `d1e19f22134f135be581438f6ac4e576fd5da9c24cc6fe850f74297ba613c04b` |
| `SimpleDAW_Development_Phase_Plan_Draft_0_1.pdf` | `c58a7109b0c645a13c0016c07b5946376254f7ee19fffb1a6330b88a8df800b9` |

## MD0 Master Drum Track — accepted and merged

MD0 shipped in v0.25.1 after Piano Roll P0. The accepted implementation covers the Master, channel-10 lanes, sample library, Drum Master routing, shared 1–4-bar MIDI insertion and the slide-up Drum Pattern Editor. Its final UAT focus rules, Inspector Loop controls, and clone roles supersede the corresponding earlier foundation proposals. [MD0 specification](md0-master-drum-track-2026-11-23.md) · [Acceptance and final controls](../testing/md0-drum-master.md#final-acceptance) · [Merged PR #27](https://github.com/mlzion/SimpleDAW/pull/27)

Drum Mixer remains sequenced after the normal Mixer. MDT recording and monitoring remain scoped to MD1. The dated phase-plan PDF remains the original planning record.
