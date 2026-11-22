# Piano Roll P0

Double-click a MIDI clip in the arrangement to open the Piano Roll in the shared bottom workspace. The editor uses the existing workspace tabs, splitter and focus routing. It opens at a practical height and initially centers the first note or middle C. Switching focus back to the arrangement preserves the note selection for the open clip.

Ableton Live places a beat ruler above a chromatic grid, aligns velocity handles below note starts and previews pitches through the track instrument. Logic Pro uses direct note blocks, a selectable grid and right-edge resizing. SimpleDAW uses those layouts with its existing mouse selection and toolbar conventions. [Ableton Live manual](https://www.ableton.com/en/live-manual/12/editing-midi/) · [Logic Pro Piano Roll](https://support.apple.com/guide/logicpro/piano-roll-editor-lgcped471027/10.7/mac/11.0) · [Logic Pro grid snapping](https://support.apple.com/en-mt/guide/logicpro/lgcpa9051d7a/10.7/mac/11.0)

## Editing and navigation

- **Draw** starts enabled. Click an empty grid cell to insert a note at velocity 96. A horizontal draw drag sets its length and remembers it for the next click. The initial length is a sixteenth note.
- **Select** allows a left-button marquee from empty space. Right-click toggles a note and right-drag from empty space adds intersecting notes in either mode. Click a note to select it alone. Drag from an already selected body to move the group. For a single note, drag its right edge to resize or use UAT SIZE when the block is narrow.
- **UAT** Cut, Copy, Paste, Delete and Move apply to the active note selection. Up and Down move a semitone. SIZE applies to one note. MODIFY remains disabled. Click the ruler to position the paste cursor. Paste aligns the earliest copied note there and retains relative time and pitch. Cut, paste, group movement and deletion each use one undo transaction. Out-of-bounds group moves and pastes are rejected atomically.
- **Snap** is independent of the arrangement and defaults to 1/16. None gives continuous pointer timing and a 1/96-quarter-note toolbar increment. With snap enabled, toolbar arrows step by one division and pointer moves snap from the grabbed note. Quantize aligns one selected start to the current grid.
- **Velocity** is editable by dragging its handle in the lane or entering a numeric value in the header. Each applies to one note. Click a piano key to audition it, or audition a note by drawing, selecting or dragging it to a new pitch. Sound uses the clip track's instrument, channel and existing audio output configuration.
- **Navigation** uses the two scrollbars, wheel or trackpad, and separate time and pitch zoom buttons. Time and velocity share the same horizontal coordinates. The ruler is clip-relative and uses the meter at the clip start.

## Architecture and scope

`NoteGrid` maps beat and pitch rows to screen coordinates. `GridNote` is the view data passed through drawing, hit testing and gesture previews. `MidiNoteEditor` adapts those values to Tracktion's active MIDI sequence. A Drum Pattern Editor can reuse the geometry and preview data with named drum rows and its own storage adapter.

The arrangement keeps the clip ID. The piano roll uses note ValueTree identity across selection and history changes and commits to Tracktion through the existing Edit UndoManager. Notes, controller events and clip properties are saved through the existing project format. Looping clips display the source loop once. Existing trimmed notes retain their original timing and are clipped visually at the visible source range. Drawing and temporal changes stay inside that range. Editing is available while stopped, consistent with the current UAT.

P0 defers editing across multiple clips, loop iterations and takes, meter changes within the ruler, controller and expression lanes, live editing during playback, scale tools, and additional MIDI transforms. MIDI import, instrument hosting and the Drum Pattern Editor have their own workstreams.

## Automated verification

Debian 13 x64, Clang 19, Release and Xvfb passed all six CTests with the pinned JUCE and Tracktion revisions. The new `piano_roll` suite covers workspace opening, UAT focus and group bounds, snapped drawing, remembered length, marquee and group drag, edge resize, velocity, cut/paste/delete, atomic Undo/Redo, audition messages on MIDI channel 5, and reopening the saved clip.

## Windows live acceptance

- Use a MIDI track with a synth. Double-click an empty and a populated clip with the workspace open and closed. Check minimum-window sizing, splitter travel, tab changes and arrangement focus.
- Draw and resize notes, drag a chord, edit velocity in both controls, scroll both axes, zoom and switch snap. Check note outlines and UAT enablement before and after changing focus.
- Audition keys and moved notes through the intended audio device and instrument. Confirm short previews release cleanly, including after closing the workspace or switching clips.
- Play the edited clip, undo and redo, save, close and reopen. Confirm note playback and the arrangement MIDI preview match the edit.
