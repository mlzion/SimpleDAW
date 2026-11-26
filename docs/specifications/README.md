# SimpleDAW specifications and development plan

The dated PDFs are historical reference documents. The index records subsequent implementation status and links to the current decisions.

| Document | Scope and status |
| --- | --- |
| [Master Specification](SimpleDAW_Master_Spec_With_Pending.pdf) | Consolidated working specification, 20 November 2026; approved Reviews 1–74, Review 75 pending, Reviews 76–90 proposed. 8 pages. |
| [Universal Actions Integration Specification](SimpleDAW_Universal_Actions_Integration_Spec.pdf) | Integration draft and decision register, 20 November 2026; separates accepted principles, pending details and recommendations. 3 pages. |
| [Development Phase Plan — Draft 0.1](SimpleDAW_Development_Phase_Plan_Draft_0_1.pdf) | Preliminary phases 0–12, dependencies and acceptance criteria, 20 November 2026. Phase identifiers are not application versions. 3 pages. |

## Current Master Specification amendment

- [MIDI track routing](midi-track-routing.md) — approved amendment to Master Specification sections 8 and 14. Governs Setup, both Inspectors, Piano Roll audition, MIDI import and saved-project compatibility.

## Implementation status and historical handoffs

- [v0.24.1 Piano Roll P0 acceptance](../research/piano-roll-p0.md#acceptance) — **ACCEPTED and merged** in PR #24. The owner passed P01–P12 and F01–F05, and the final Windows x64 Release `/MD` run passed 14/14 CTests.

- [v0.22 design decisions — Reviews 01–82](v0.22-design-decisions-reviews-01-82.md) — accepted UAT and later-phase recording design decisions; Review 83 pending. v0.22.1 is limited to the UAT foundation, not advanced recording features.
- [v0.21 acceptance and v0.22 handoff](v0.21-acceptance-v0.22-handoff.md) — records accepted live testing, documented limitations and candidate v0.22 work. This supplement does not supersede the dated PDFs or approve pending designs.

- [v0.22.2 toolbar and creation-controls handoff](v0.22-design-decisions-reviews-01-82.md#v0222-toolbar-and-creation-controls-handoff--accepted-2026-11-20) — approved CUT replacement for CREATE, permanent global Undo/Redo, minimum-window sizing, deferred circular + creation controls, and Inspector-specific Duplicate/Clone/Loop.

- [v0.22.2 expanded implementation task sheet](../tasks/v0.22.2-first-task-sheet.md) — historical task sheet. The v0.22.2 UAT and global Undo/Redo implementation merged in PR #10 and passed its recorded acceptance checks. [v0.22.2 acceptance](../testing/v0.22.2-live-test.md)
- [Clip Glue, Groups, Parent/Child Tracks and MIX/REPLACE (21 Nov 2026)](clip-glue-group-parent-overlap-2026-11-21.md) — accepted design supplement. Legacy REPLACE compatibility shipped with the Inspector acceptance. General clip Glue and Groups, parent/child tracks, and the full MIX/REPLACE design remain deferred. [Inspector acceptance](../testing/v0.23.1-live-test.md)


- [v0.23.1 Inspector, Clone/Loop and MIDI Import decisions](v0.23.1-inspector-and-midi-import.md) — Inspector, E1 and MIDI import shipped through PR #20. MD0 subsequently refined the UAT and clone and loop controls. The separate MIDI track-routing amendment remains deferred. [Current UAT matrix](../architecture.md#universal-actions-toolbar)
- [v0.23.1 Codex first-iteration task sheet](../tasks/v0.23.1-inspector-first-iteration.md) — historical implementation scope, E1 boundaries, and acceptance criteria for the Inspector release.

## How to use this set

Read the repository [instructions](../../AGENTS.md) and [project status](../../PROJECT_STATUS.md) before assigning work. Accepted design does not mean implemented, merged or tested. Pending and recommended items require decisions before implementation.

The Universal Actions supplement records newer interaction decisions; reconcile explicit replacements with the master specification while preserving approved functionality. Flag unresolved conflicts rather than inventing behavior. Maintain specification decisions in the SimpleDAW specification chat and bring approved outcomes from development/testing chats back into the shared documents through review.

The dated phase plan predates v0.21 acceptance. PR #3 later merged after Windows live acceptance. The initial audio-engine Phase 1 and the phase plan’s Universal Actions Phase 1 refer to different planning contexts.

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

## Context UI experimental development plan

| Version | Phase | Scope |
| --- | --- | --- |
| v0.26.1 | CUI0/CUI1 | Edit · Context UI Foundation, subject to verification of the button-order correction |
| v0.26.2 | CUI2 | Move |
| v0.26.3 | CUI3 | Size |
| v0.26.4 | CUI4 | Modify |

Edit opens first in v0.26.1. Move opens first once implemented. The eventual forward wheel order is Move → Size → Edit → Modify → Move. Each later phase requires separate authorization. [Context UI specification](../CONTEXT_UI_EXPERIMENT.md#approved-development-sequence) · [Acceptance record](../testing/context-ui-foundation.md) · [Draft PR #29](https://github.com/mlzion/SimpleDAW/pull/29)

## Deferred integration — MX0 Mixer Foundation

MX0 integration is deferred while CUI0/CUI1 takes priority on the experimental branch. Draft PR #28 retains the Mixer Foundation. Its foundation covers the mixer workspace, channel scrolling, pinned FX1, FX2 and Master strips, and native Tracktion mixer controls and meters. Integration against accepted MD0, Windows CI, and live acceptance await separate authorization. [MX0 Mixer Foundation PR #28](https://github.com/mlzion/SimpleDAW/pull/28)

FX return routing remains in MX1. Drum Mixer lane expansion remains in MX2, after the normal Mixer. Master Drum Track recording and monitoring remain in MD1. Experimental Context UI CUI0/CUI1 stays on its separate branch and draft PR. [Context UI PR #29](https://github.com/mlzion/SimpleDAW/pull/29)
