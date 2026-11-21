# Phase 1 architecture

The JUCE application owns a Tracktion Engine and a single active Edit. Tracktion manages the transport, device callbacks, recording, audio/MIDI clips, and the plugin processing graph. Its audio tracks can hold either audio or MIDI. JUCE's device selector controls the same audio device manager that Tracktion uses for playback and recording.

The prototype uses a button panel and a track selector. UI actions resolve the selected track, configure input routing and plugin chains, and start or stop the transport on the message thread. The application adds no audio callback of its own. Dialogs, file selection, persistence, input enumeration, and plugin scanning execute outside the real-time callback. Tracktion handles callback processing and recording buffering. Third-party plugins supply their own processing code.

Project files use Tracktion's .tracktionedit serialization. The application uses EditFileOperations to save an Edit associated with the chosen path. Recordings and imported media remain separate files. The prototype relies on Tracktion's media path handling and provides no project packaging or recovery UI.

Export stops playback, renders the Edit offline through Tracktion, and moves the resulting temporary WAV into place only if rendering succeeds. The prototype uses 44.1 kHz, 24-bit stereo and includes track and master plugins. The render covers the Edit length and runs synchronously, so the window waits for export to finish.

The Windows CTest runs the executable in smoke-test mode. It generates a short input WAV, imports it onto two tracks, inserts a native effect, saves and reloads the Edit, and checks the channel count, length, and signal level of an offline render. This test uses synthetic audio and does not depend on an attached interface.

## Universal Actions Toolbar

The v0.22.1 toolbar displays a fixed action inventory independently of transport. Each action button checks the same dispatcher used by Edit menu Copy, Paste and clip Delete. Edit menu Undo and Redo use the project's Tracktion UndoManager. The existing Delete shortcut also clears the Timeline ruler range when the Timeline has focus and its clip selection is empty.

The dispatcher resolves the most recently clicked editing area. If it closes, fallback checks available selected areas in priority order (editing windows, Inspector, Timeline, Tracks), then defaults to Timeline. The Inspector and read-only Effects workspace can take focus. Only the Timeline and Tracks surfaces currently supply selected object IDs. Clicking or box-selecting in either leaves the other selection intact. Left boxes replace selection and intersect visible objects. The existing single-clip drag and ruler range remain available. The Tracks area also preserves the application's primary track destination for existing Insert commands.

| UAT action | v0.22.1 availability |
| --- | --- |
| Copy | One ordinary, unlinked audio or MIDI clip in Timeline while stopped. Uses a project-local Tracktion clipboard snapshot. |
| Paste | A clipboard snapshot and a destination in Timeline while stopped, with at most one selected clip. Uses the snapped edit cursor, or the exact cursor with snap off, and selects the new clip. Both SimpleDAW track menu types create Tracktion audio tracks, which can host WAV and MIDI clips. |
| Delete | One Timeline clip while stopped. Non-ripple removal uses the existing missing-audio confirmation and one undo transaction. |
| Create | Deferred until generic creation semantics are supported. Existing explicit Insert commands remain available. |
| Move and Size | Deferred. Existing single-clip mouse movement stays available. |
| Split and Trim | Deferred until timeline structural editing is implemented. |
| Reserved MODIFY positions | Disabled and unassigned. |

Advanced multiselection edits, additive right-drag boxes, track operations, editor object selection, punch and take work are outside this phase. See the [approved decisions](specifications/v0.22-design-decisions-reviews-01-82.md) and [Windows live checklist](testing/v0.22.1-live-test.md).
