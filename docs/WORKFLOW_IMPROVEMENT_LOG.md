# SimpleDAW — Workflow Improvement Log

Living experimental-branch decision log. Entries are approved design requirements unless otherwise marked; implementation requires separate phase authorization. Do not interrupt active CUI0/CUI1 integration or change main/MX0.

## W01 — Piano Roll automatic vertical scrolling
Status: APPROVED; implementation pending.
Observation: opening Piano Roll by double-clicking a MIDI clip can obscure the clip being edited.
Requirement: after editor layout, automatically scroll synchronized Track List and Timeline vertically as far as necessary to keep the edited clip visible above Piano Roll. Preserve horizontal navigation, selection and existing highlighting. Test clips near bottom, different track heights and already-visible clips. Consider other docked editors separately.

## W02 — Master Drum Track positioning
Status: APPROVED; implementation/verification pending.
Requirement: Master Drum Track remains above ordinary Track #1, cannot be reordered among ordinary tracks, and participates in normal vertical scrolling so it can scroll entirely out of view. Child lanes remain attached to parent. Verify existing behavior before changing code; preserve lane reorder within parent and existing numbering rules.

## W03 — Selectable timeline edit cursor and time window
Status: APPROVED concept; implementation pending; selection gesture and visual details pending live design review.
Requirement: make the timeline edit cursor and time selection window selectable command targets for both UAT and CUI. Provide visible selection feedback (color change or pulse). Cursor supports horizontal Move; time window supports horizontal Move preserving duration, Size at either boundary, and Delete to clear the window only (never enclosed media). Respect existing snap increments including 10 ms when OFF, timeline bounds and existing transport restrictions. Do not target the playback cursor. Proposed dedicated ruler hit targets must preserve existing ruler click-to-position, drag-to-create, clip selection and time-selection-dependent operations. Disable irrelevant commands; preserve loop semantics and define focus/selection precedence before implementation. Review with CUI3 Move/Size and Edit integration; no changes during active CUI0/CUI1 work.
