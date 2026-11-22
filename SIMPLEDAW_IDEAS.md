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
| IDEA-001 | Clip locking | Timeline / Clip Inspector | PROPOSED | Prevent moving or editing locked clips; show lock icon and/or dashed border. Deletion policy pending. |

## Detailed discussions

Add an entry for each idea with its ID, date, status, scope, behavior, tradeoffs, and open questions.

### IDEA-001 — Clip Locking (2026-11-22)

**Status:** PROPOSED  
**Subsystem:** Timeline / Clip Inspector

**User request:** Add a lock feature to clips. Locked clips cannot be moved or edited. Indicate locking with a lock icon or dashed borders on the clip.

**Proposed behavior for discussion:**
- Lock/unlock from the Clip Inspector.
- Locked clips remain visible and playable.
- Prevent move, resize, split, trim, and other clip edits while locked.
- Show a visible lock icon, dashed border, or both on the timeline.

**Pending decisions:** Whether delete/cut must be blocked until unlock; how duplication/copying of locked clips behaves; whether lock is retained on duplicates; whether locked clips may still be selected; exact visual treatment and undo behavior.

**Implementation:** Not authorized. This entry is non-canonical until promoted to the specification.


## Promotion log

Record when an approved idea is transferred into canonical specifications, including PR or commit references.
