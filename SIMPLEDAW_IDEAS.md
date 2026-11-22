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


## Promotion log

Record when an approved idea is transferred into canonical specifications, including PR or commit references.
