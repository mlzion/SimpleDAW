# SimpleDAW — Ideas & Design Sandbox

> **NON-CANONICAL WORKING DOCUMENT.** This is an ideas and decisions log, not the Master Specification, a task authorization, or instructions for Codex. Entries are promoted into canonical specifications only through an explicitly approved documentation update.

Last updated: 2026-11-22

## How this document works

- **PROPOSED:** An idea to explore.
- **PENDING:** Discussed, with unresolved details.
- **APPROVED:** Design accepted by the project owner, but **not** authorized for implementation.
- **DEFERRED:** Retained for a later phase.
- **IMPLEMENTED:** Delivered and verified; cite the acceptance record when available.
- Each entry identifies its subsystem, target phase, and implementation status.
- Update this sandbox directly on the `design-sandbox` branch, without a PR for each idea.
- Keep application code and official specifications unchanged here. Promote approved requirements to `main` via a reviewed documentation PR at the relevant milestone.
- Notify Codex immediately only when a new decision affects work currently being implemented.

## Approved decisions awaiting specification promotion

### IDEA-001 — Contextual expansion of Piano Roll workspace
- **Subsystem:** Piano Roll / shared editor workspace
- **Target:** Piano Roll P1
- **Design status:** APPROVED
- **Implementation:** NOT IMPLEMENTED
- **Decision:** Double-clicking a MIDI clip opens the Piano Roll in the bottom workspace. Scroll the Track List and timeline together so the edited MIDI track appears at the top of the track display. Expand the editor upward to immediately below that track, leaving the edited clip visible above it. Preserve the transport, Universal Actions Toolbar, and other fixed controls. The note grid should comfortably show at least one full octave (12 semitones), with the velocity editor accessible and pitch scrolling/zoom maintained.
- **Close behavior:** Restore the previous track scroll position and workspace height.
- **Notes:** P0 is unchanged; evaluate during P1 after live acceptance.

### IDEA-002 — Contextual expansion of Drum Pattern Editor workspace
- **Subsystem:** Drum Master Track / Drum Pattern Editor / shared editor workspace
- **Target:** Drum Pattern Editor phase
- **Design status:** APPROVED
- **Implementation:** NOT IMPLEMENTED
- **Decision:** Double-clicking a Drum Master pattern expands the Pattern Editor upward to immediately below the pinned Drum Master Track, keeping the edited pattern visible. No track scrolling is needed because the Drum Master Track is pinned at the top.
- **Close behavior:** Restore the previous workspace height and track scroll position.
- **Notes:** Use the same general workspace behavior as the Piano Roll while retaining drum-specific interactions.

## Deferred architectural consideration

### IDEA-003 — Shared MIDI/drum grid extraction
- **Subsystem:** Piano Roll / Drum Pattern Editor architecture
- **Target:** Drum Pattern Editor implementation
- **Design status:** DEFERRED
- **Implementation:** NOT IMPLEMENTED
- **Decision:** Keep Piano Roll P0 focused. When drum development starts, consider extracting shared beat-to-pixel grid geometry, ordered-row rendering, snapping math, velocity mapping, and gesture preview-to-commit conventions. Keep piano-note and drum-hit gesture rules separate. Preserve one Undo transaction per committed gesture.
- **Notes:** Based on the P0 architecture review; no immediate refactor requested.

## New ideas inbox

Add spontaneous ideas here before discussion, then move them into subsystem sections as their status becomes clear.

| ID | Subsystem | Idea | Status | Target phase |
| --- | --- | --- | --- | --- |
| — | — | Awaiting new ideas | — | — |

## Promotion history

No sandbox entries have yet been promoted into canonical specifications.
