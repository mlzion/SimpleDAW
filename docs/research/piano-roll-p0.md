# Piano Roll P0 integration

Double-click an ordinary MIDI clip in the Timeline to open the Piano Roll. It shares the slide-up editing area with Effects Editor E1. Switching editors hides the previous panel and retains its valid target and selection. Timeline clip selection and Track List selection remain independent. The Inspector owns clip-wide parameters, including playback velocity, transpose, mute, color, and loop controls.

## Editing

- **Draw** starts enabled. Click an empty grid cell to create a note. Drag horizontally to set and remember its length. The initial length is a sixteenth note.
- **Select** enables a left-button marquee from empty grid space. Right-drag from empty space adds intersecting notes in either mode. A stationary right-click on a note toggles selection. Drag a selected note body to move the group. Drag the right edge of a single note or use UAT SIZE to resize.
- **Velocity** can be edited by right-dragging vertically on a note, dragging a handle in the velocity lane, or editing the numeric control. Right-drag leaves the selection intact and adjusts one MIDI velocity unit per pixel, clamped to 1–127. New notes use the last successfully adjusted velocity in the current project session. The initial value is 96, and project replacement resets it.
- **UAT** Cut, Copy, Paste, Delete, Duplicate, MOVE and single-note SIZE apply when Piano Roll has focus. Place the paste cursor by clicking the ruler. Duplicate offsets the group by its span from earliest start to latest end, so the first copy starts at the end of the selection. Paste and Duplicate preserve relative spacing, pitches, velocities and durations. A paste, duplicate or group move that exceeds the source bounds is rejected as a whole.
- **Snap** is independent of the arrangement, defaults to 1/16 and offers None, 1/4, 1/8, 1/16 and 1/32. None allows continuous pointer timing and uses a 1/96-quarter-note UAT step. Quantize aligns one selected start to the grid.
- **Navigation** uses both scroll axes and separate time and pitch zoom. Velocity bars share the note grid's horizontal coordinates. The ruler uses the meter at the clip start and counts from the editable source range.
- **Audition** previews piano keys and notes through the containing track's instrument using the clip's effective MIDI channel. Preview note-off uses the captured destination and channel. The track-routing amendment remains a separate milestone.

## Storage and scope

The Timeline identifies the opened clip by its Edit item ID. The editor uses note ValueTree identity and Tracktion's active MIDI sequence. Linked note edits apply to all family members in one Edit UndoManager transaction. Non-note events and clip properties remain in their respective sequences and clip states. Unclone makes subsequent note edits independent. Loops expose a single source pattern. Existing trimmed notes keep their source timing and only their visible portions are drawn.

Editing is available while stopped. Multi-clip, multi-iteration and take editing, ruler meter changes within a clip, controller and expression lanes, live editing, scale tools, additional transforms, MIDI track routing and the Drum Pattern Editor are separate work.

## Acceptance

Run the full Linux CTest suite, including `piano_roll`. Windows x64 Release `/MD` requires separate authorization. Test minimum-window layout, mixed MIN/MAX row hit testing, focus among both editors and the Inspector, audible audition, note termination, and playback after reopening through the intended audio device and instrument. Recheck MIDI clip transpose, mute and color, legacy REPLACE overlap, and E1 editing during playback against the v0.23.1 checklist.
