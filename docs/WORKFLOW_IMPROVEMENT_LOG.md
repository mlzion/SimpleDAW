# SimpleDAW — Workflow Improvement Log

Living experimental-branch decision log. Entries are approved design requirements unless otherwise marked; implementation requires separate phase authorization. CUI0/CUI1 is accepted. W01–W05 are authorized for v0.26.2, except W03 boundary sizing deferred to v0.26.3. Do not change main/MX0.

## W01 — Piano Roll automatic vertical scrolling
Status: IMPLEMENTED in v0.26.2; Windows verification pending.
Observation: opening Piano Roll by double-clicking a MIDI clip can obscure the clip being edited.
Requirement: after editor layout, automatically scroll synchronized Track List and Timeline vertically as far as necessary to keep the edited clip visible above Piano Roll. Preserve horizontal navigation, selection and existing highlighting. Test clips near bottom, different track heights and already-visible clips. Consider other docked editors separately.

## W02 — Master Drum Track positioning
Status: Existing positioning verified by v0.26.2 Linux regression coverage.
Requirement: Master Drum Track remains above ordinary Track #1, cannot be reordered among ordinary tracks, and participates in normal vertical scrolling so it can scroll entirely out of view. Child lanes remain attached to parent. Verify existing behavior before changing code; preserve lane reorder within parent and existing numbering rules.

## W03 — Selectable timeline edit cursor and time window
Status: Selection, Move and Delete implemented in v0.26.2. Boundary sizing is deferred to CUI3. Ruler handle dimensions remain subject to Windows live acceptance.
Requirement: make the timeline edit cursor and time selection window selectable command targets for both UAT and CUI. Provide visible selection feedback (color change or pulse). Cursor supports horizontal Move; time window supports horizontal Move preserving duration, Size at either boundary, and Delete to clear the window only (never enclosed media). Respect existing snap increments including 10 ms when OFF, timeline bounds and existing transport restrictions. Do not target the playback cursor. Proposed dedicated ruler hit targets must preserve existing ruler click-to-position, drag-to-create, clip selection and time-selection-dependent operations. Disable irrelevant commands; preserve loop semantics and define focus/selection precedence before implementation. Implement selection, Move and Delete in CUI2; Size in CUI3.

## W04 — Timeline snap increments and startup default
Status: IMPLEMENTED in v0.26.2, including the integrated Move snap selector. Windows verification pending.
Ensure Timeline snap options include 1 Bar and 1/2 (half note), alongside 1/4, 1/8, 1/16, 1/32 and OFF (10 ms). Verify existing options before adding duplicates. Set 1 Bar as the default on program startup. Preserve meter-aware bar length and existing editor-specific snap behavior unless separately approved. Use compact left/right snap arrows above Move buttons, with bidirectional rollover and persistent popup. Future Size page reuses selector; separate Snap page deferred.

## W05 — Track List visual cleanup
Status: IMPLEMENTED in v0.26.2; Windows verification pending.
Remove Input 1 and Input 2 display icons from Track List rows; retain input configuration and routing functionality in Inspector. Restore approved compact ordinary track title format: track number immediately followed by A (Audio) or M (MIDI), space, user-assigned name (example: 3A Guitar 1). Do not append redundant Audio/MIDI type labels. Derive identifier from current track order and type; update after insert/delete/reorder, without changing stored user name. Preserve Master Drum Track and child lane numbering exemptions and existing functional controls. Implementation authorized in CUI2.
