# SimpleDAW — Ideas Sandbox

> **NON-CANONICAL DESIGN SANDBOX** — Ideas here are for discussion, not implementation authorization. The Master Spec and approved amendments remain authoritative.

Repository: `mlzion/SimpleDAW`  
Branch: `design-sandbox`

## Working agreement

- Capture feature and workflow ideas discussed in the dedicated SimpleDAW sandbox chat.
- Record the target subsystem/phase, status, rationale, and any unresolved decisions.
- Use **PROPOSED**, **PENDING**, **APPROVED**, or **DEFERRED**.
- **APPROVED** means approved for eventual specification promotion, **not** permission for Codex to implement.
- Update this sandbox file directly on `design-sandbox`, without a PR for every idea.
- At agreed milestones, promote selected decisions into canonical specs via reviewed PRs.
- Notify Codex immediately only if a change affects active implementation; otherwise batch approved changes for the relevant phase.
- Do not change application code from this sandbox workflow.

## Idea register

| ID | Idea | Subsystem / phase | Status | Notes |
| --- | --- | --- | --- | --- |
| IDEA-001 | Clip locking | Timeline / Clip Inspector; post-v0.23.1 timeline/clip-editing polish | APPROVED | Total editing lockdown; Inspector-only unlock; gray clip presentation. Specification promotion and implementation not yet authorized. |
| IDEA-002 | Audio Quantization Editor | Advanced Audio Editing; late-stage, post core-feature completion | APPROVED | Guitar, piano, vocals; automatic/manual timing correction, pitch preservation, non-destructive; technical feasibility investigation required. |

## Implementation phase index

This index groups sandbox-approved ideas by **recommended** phase for future planning. A recommendation is not a release commitment or implementation authorization. Update this index whenever an idea changes status, phase, or promotion state.

### Post-v0.23.1 — Timeline & Clip Editing Polish

| Idea | Sandbox status | Canonical spec | Implementation |
| --- | --- | --- | --- |
| [IDEA-001 — Clip Locking](#idea-001--clip-locking-2026-11-22) | APPROVED | Not promoted | Not authorized |

### Late-stage — Advanced Audio Editing (after core DAW feature completion)

| Idea | Sandbox status | Canonical spec | Implementation |
| --- | --- | --- | --- |
| [IDEA-002 — Audio Quantization Editor](#idea-002--audio-quantization-editor-2026-11-22) | APPROVED | Not promoted | Not authorized; feasibility review required |

### Unscheduled / awaiting phase assignment

No ideas currently assigned.

## Lifecycle and implementation history

Use the same idea ID throughout discussion, specification promotion, implementation, and release. **APPROVED** is a sandbox decision only; authorization to implement is separate.

| ID | Spec promotion (document / PR / commit) | Implementation authorization | Code PR / commit | Release / acceptance | Lifecycle |
| --- | --- | --- | --- | --- | --- |
| IDEA-001 | Not promoted | Not authorized | — | — | Sandbox approved |
| IDEA-002 | Not promoted | Not authorized | — | — | Sandbox approved; late-stage feasibility review pending |

When an idea progresses, record the canonical spec document and section, promotion PR/commit, explicit implementation authorization, implementation PR/commit, release version, and acceptance result. Never label an idea implemented solely because its sandbox status is APPROVED. Preserve history after delivery rather than removing completed ideas.

## Detailed discussions

Add an entry for each idea with its ID, date, status, scope, behavior, tradeoffs, and open questions.

### IDEA-001 — Clip Locking (2026-11-22)

**Status:** APPROVED (sandbox design decision; not yet canonical)  
**Subsystem:** Timeline / Clip Inspector / Universal Actions Toolbar  
**Recommended implementation phase:** **Post-v0.23.1 Timeline & Clip Editing polish**, after Inspector + E1 live acceptance and integration. Bundle with the next authorized clip-editing refinement phase, rather than interrupting active Inspector/E1 acceptance or the separate Piano Roll branch. Exact release version to be assigned when promoted into the Master Spec.

**Approved behavior:**
- Lock and Unlock are controlled from the Clip Inspector.
- A locked clip is completely protected from editing: no move, resize, trim, split, fade/property changes, cut, delete, copy, duplicate, or other clip-edit operations. Unlock first; no copy/paste exception.
- Locked clips cannot be added to an editable multi-selection set, including right-click toggle or drag-box selection.
- A normal click on a locked clip can target it for **Inspector viewing and unlocking only**; it does not add the clip to the editable selection set. This distinction must be preserved in the UAT action dispatcher.
- All editable Clip Inspector controls are disabled/grayed out except Unlock. Property values remain visible.
- On the timeline, locked clips are visually grayed/desaturated without adding another icon or dashed selection border. Waveforms/MIDI notes remain recognizable; existing selection/focus cues must remain unambiguous.
- Playback is unchanged.
- Clip lock state persists across save/reopen; locking and unlocking participate in Undo/Redo.
- Locking a clip does not freeze or block its parent track's mixer settings or effects.

**Implementation/acceptance notes:** Enforce the lock centrally across all edit entry points, not just by disabling Inspector buttons. Test direct gestures, UAT actions, right-click selection, marquee selection, group operations, save/reopen, and Undo/Redo. Locked clips must never be silently included in editable groups.

**Open implementation detail:** Exact UI rendering and internal separation between Inspector target and editable selection can be chosen during implementation without changing the approved behavior.

**Authority:** Approved for the ideas sandbox only. No Codex implementation or canonical-spec change is authorized until a future promotion decision.


### IDEA-002 — Audio Quantization Editor (2026-11-22)

**Status:** APPROVED (sandbox concept; implementation DEFERRED to late stage)  
**Subsystem:** Advanced Audio Editing / dedicated Audio Quantization Editor  
**Recommended implementation phase:** **Late-stage enhancement, after core DAW feature completion** (Mixer, Piano Roll, Pattern Editor, Automation Editor, and Effects). No release number assigned.

**Purpose:** Correct timing of recorded **guitar, acoustic piano, and vocals**. Drum performance quantization is not a primary use case because SimpleDAW's Drum Master uses MIDI-based drums.

**Approved high-level concept:**
- Provide a dedicated Audio Quantization Editor rather than crowding the Timeline or Inspector.
- Support automatic note-attack/transient or syllable-boundary detection where practical, plus manual timing-marker placement and correction.
- Align performance events to the musical grid with adjustable quantization strength.
- Use localized, pitch-preserving time stretching/elastic audio to preserve sustains and note decays as well as practical.
- Keep edits non-destructive and reversible, with preview, apply/cancel, and Undo support.
- Recognize that detection and natural-sounding correction vary with material; vocal and overlapping-note content may require manual intervention.

**Technical prerequisite:** Investigate Tracktion Engine's actual transient analysis, time-stretching, and potential localized warp-marker facilities, including licensing/dependency implications, before choosing an implementation. Do not assume the current engine directly supports the complete workflow.

**Delivery strategy:** If feasible, develop as a separate late-stage addendum and branch. Detailed UI, algorithms, and acceptance criteria remain for that phase.

**Authority:** Sandbox concept approved only. No change to canonical specs, active development branches, or Codex implementation authorization.

## Promotion log

Append a dated entry when an approved idea is transferred into canonical specifications, with the idea ID, target spec and section, PR/commit, and any outstanding implementation authorization. Then update the phase index and lifecycle table above. No promotions recorded yet.
