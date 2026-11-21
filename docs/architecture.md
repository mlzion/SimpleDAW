# Phase 1 architecture

The JUCE application owns a Tracktion Engine and a single active Edit. Tracktion manages the transport, device callbacks, recording, audio/MIDI clips, and the plugin processing graph. Its audio tracks can hold either audio or MIDI. JUCE's device selector controls the same audio device manager that Tracktion uses for playback and recording.

The prototype uses a button panel and a track selector. UI actions resolve the selected track, configure input routing and plugin chains, and start or stop the transport on the message thread. The application adds no audio callback of its own. Dialogs, file selection, persistence, input enumeration, and plugin scanning execute outside the real-time callback. Tracktion handles callback processing and recording buffering. Third-party plugins supply their own processing code.

Project files use Tracktion's .tracktionedit serialization. The application uses EditFileOperations to save an Edit associated with the chosen path. Recordings and imported media remain separate files. The prototype relies on Tracktion's media path handling and provides no project packaging or recovery UI.

Export stops playback, renders the Edit offline through Tracktion, and moves the resulting temporary WAV into place only if rendering succeeds. The prototype uses 44.1 kHz, 24-bit stereo and includes track and master plugins. The render covers the Edit length and runs synchronously, so the window waits for export to finish.

The Windows CTest runs the executable in smoke-test mode. It generates a short input WAV, imports it onto two tracks, inserts a native effect, saves and reloads the Edit, and checks the channel count, length, and signal level of an offline render. This test uses synthetic audio and does not depend on an attached interface.

## Universal Actions Toolbar

The four fixed 2×2 action groups use the same dispatcher as the Edit menu. The global UNDO and REDO buttons use the project's Tracktion UndoManager through the same history handler as the menu. Both are available independently of editing area while transport is stopped. The Delete shortcut also clears the ruler range when the Timeline has focus and its clip selection is empty.

The dispatcher resolves the most recently clicked editing area. If it closes, fallback checks available selected areas in priority order (editing windows, Inspector, Timeline, Tracks), then defaults to Timeline. The Inspector and read-only Effects workspace can take focus. Only the Timeline and Tracks surfaces currently supply selected object IDs. Clicking or box-selecting in either leaves the other selection intact. Left boxes replace selection and intersect visible objects. The existing single-clip drag and ruler range remain available. The Tracks area also preserves the application's primary track destination for existing Insert commands.

| Action | v0.22.2 availability |
| --- | --- |
| CUT / COPY / PASTE | Timeline WAV and MIDI clips while stopped. Clipboard data is project-local. Paste starts at the snapped or exact cursor on the destination lane and preserves group start offsets and visible lane gaps. MIDI and tempo-synced lengths keep their beat durations. Both track menu types create Tracktion audio tracks that accept WAV and MIDI. Paste requires existing usable lanes and selects all pasted clips. Cut and Paste are separate transactions. |
| DELETE | Selected clips are removed without ripple. Missing audio and track deletion require confirmation. Track deletion uses Tracktion's track removal and routing cleanup. |
| MOVE | Clips move one existing audio lane vertically or one grid step horizontally. Timing and lane spacing are preserved. Tracks move vertically as a contiguous block among top-level audio tracks. |
| SIZE | Clip START and END each move earlier with − and later with +. START updates the source offset to keep source alignment. Invalid bounds, missing media, loops, multi-take clips and unsupported audio modes reject the entire group. |
| MODIFY | SPLIT, TRIM and the two reserved positions remain disabled. |

Clip and track actions resolve all selected IDs and preflight their destinations or bounds before mutation. Each edit has one history transaction. Horizontal MOVE and SIZE apply the same time delta to the group. Snap On uses one selected grid division at the earliest affected boundary. Snap Off uses exactly 10 ms per press regardless of tempo. Track settings clipboard mapping and track-height controls await clarification. [Approved decisions](specifications/v0.22-design-decisions-reviews-01-82.md) · [Windows live checklist](testing/v0.22.2-live-test.md)
