# Piano Roll P0 integration

Double-click an ordinary MIDI clip in the Timeline to open the Piano Roll. It shares the slide-up editing area with Effects Editor E1. Switching editors hides the previous panel and retains its valid target and selection. Timeline clip selection and Track List selection remain independent. The Inspector owns clip-wide parameters, including playback velocity, transpose, mute, color, and loop controls.

## Editing

- **Draw** starts enabled. Click an empty grid cell to create a note. Drag horizontally to set and remember its length. The initial length is a sixteenth note.
- **Select** enables a left-button marquee from empty grid space. Right-drag from empty space adds intersecting notes in either mode. A stationary right-click on a note toggles selection. Drag a selected note body to move the group. Drag the right edge of a single note or use UAT SIZE to resize.
- **Velocity** can be edited by right-dragging vertically on a note, dragging a handle in the velocity lane, or editing the numeric control. Right-drag leaves the selection intact and adjusts one MIDI velocity unit per pixel, clamped to 1–127. New notes use the last successfully adjusted velocity in the current project session. The initial value is 96, and project replacement resets it.
- **UAT** Cut, Copy, Paste, Delete, Duplicate, MOVE and single-note SIZE apply when Piano Roll has focus. Place the paste cursor by clicking the ruler. Duplicate offsets the group by its span from earliest start to latest end, so the first copy starts at the end of the selection. Paste and Duplicate preserve relative spacing, pitches, velocities and durations. A paste, duplicate or group move that exceeds the source bounds is rejected as a whole.
- **Snap** is independent of the arrangement, defaults to 1/16 and offers None, 1/4, 1/8, 1/16 and 1/32. None allows continuous pointer timing and uses a 1/96-quarter-note UAT step. Quantize aligns one selected start to the grid.
- **Navigation** uses the bottom toolbar and both scrollbars. With Piano Roll focused, the shared controls scroll time or pitch, zoom horizontally, and expose pitch zoom. Timeline, Track List, Inspector and E1 focus keep the Timeline controls and their own view state. The ruler uses the meter at clip start and counts from the editable source range.
- **Cursors** share the grid with the velocity lane. Clicking locates the stationary edit and paste cursor. Snap aligns the cursor to the closest available grid line, and None uses free timing. The amber playback cursor follows the Timeline transport inside the playing clip and wraps within the source pattern. Manual horizontal scrolling or zoom pauses Piano Roll FOLLOW for two seconds. Stopping or toggling FOLLOW clears that pause.
- **Feedback** includes a floating MIDI value during either mouse velocity drag. The Timeline pulses the bound MIDI clip using the existing Inspector pulse timing while Piano Roll has focus.
- **Audition** previews piano keys and notes through the containing track's instrument using the clip's effective MIDI channel. Preview note-off uses the captured destination and channel. The track-routing amendment remains a separate milestone.

## Storage and scope

The Timeline identifies the opened clip by its Edit item ID. The editor uses note ValueTree identity and Tracktion's active MIDI sequence. Linked note edits apply to all family members in one Edit UndoManager transaction. Non-note events and clip properties remain in their respective sequences and clip states. Unclone makes subsequent note edits independent. Loops expose a single source pattern. Existing trimmed notes keep their source timing and only their visible portions are drawn.

Editing is available while stopped. Multi-clip, multi-iteration and take editing, ruler meter changes within a clip, controller and expression lanes, live editing, scale tools, additional transforms, MIDI track routing and the Drum Pattern Editor are separate work.

## Acceptance

**ACCEPTED.** The owner reported 12/12 P01–P12 passes on baseline `73b9554` and 5/5 F01–F05 passes on final commit `885631ed4ea5aafcf9b6bac8929e1d4d665a4cbe`. No acceptance defects remain. [Baseline Windows run](https://github.com/mlzion/SimpleDAW/actions/runs/45196843591)

The final Windows x64 Release `/MD` run passed 14/14 CTests, runtime import verification, and isolated startup. The final Linux Release suite also passed 14/14 CTests. PR #24 merged at `05d2c37`. [Final Windows run](https://github.com/mlzion/SimpleDAW/actions/runs/45291914568) · [Accepted ZIP](https://github.com/mlzion/SimpleDAW/actions/runs/45291914568/artifacts/14875361245) · [PR #24](https://github.com/mlzion/SimpleDAW/pull/24)

### Focused Windows acceptance checks

| Check | Expected result | Result |
| --- | --- | --- |
| F01 — Edit and playback cursors | Click between grid lines with Snap on and with None. The stationary edit cursor snaps or moves freely as configured. Play across clip start and end, stop, and play a trimmed loop with a nonzero source start. The amber cursor appears only during the active clip, wraps every source loop, and leaves the edit cursor in place. With FOLLOW on, scroll offscreen manually and verify the brief hold before follow resumes. Toggle FOLLOW off and verify scrolling stays manual. | PASS |
| F02 — Velocity | Try stationary right-click, right-drag on a note, and velocity-handle drag at low and high velocities. The readout tracks the drag from 1 to 127 and stays clear of the note. Release or switch focus mid-drag and verify the readout disappears. Insert a note after a committed adjustment and confirm the remembered velocity. | PASS |
| F03 — Navigation | At minimum window size, try page and bar scroll, pitch scroll, horizontal and pitch zoom, and direct scrollbars. Transfer focus through Timeline, Tracks, Inspector, and E1. Verify the toolbar targets the Timeline in those areas and that each view retains its zoom and scroll state. Check that UAT stays scoped to the active editor. | PASS |
| F04 — Clip pulse | Open two MIDI clips in succession and switch between Piano Roll, E1, and the Inspector. Verify the pulse transfers to the currently edited clip, stops on focus loss, and resumes on return. Check the original Inspector pulse, selection borders and clip colors. Delete the edited clip and replace the project. | PASS |
| F05 — Regression spot checks | Audition through the intended instrument and output. Verify guide-note release on playback and editor changes. Edit a linked MIDI clip, undo and redo, save and reopen, then confirm Timeline preview and playback. | PASS |
