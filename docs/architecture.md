# Phase 1 architecture

The JUCE application owns a Tracktion Engine and a single active Edit. Tracktion manages the transport, device callbacks, recording, audio/MIDI clips, and the plugin processing graph. Its audio tracks can hold either audio or MIDI. JUCE's device selector controls the same audio device manager that Tracktion uses for playback and recording.

The prototype uses a button panel and a track selector. UI actions resolve the selected track, configure input routing and plugin chains, and start or stop the transport on the message thread. The application adds no audio callback of its own. Dialogs, file selection, persistence, input enumeration, and plugin scanning execute outside the real-time callback. Tracktion handles callback processing and recording buffering. Third-party plugins supply their own processing code.

Project files use Tracktion's .tracktionedit serialization. The application uses EditFileOperations to save an Edit associated with the chosen path. Recordings and imported media remain separate files. The prototype relies on Tracktion's media path handling and provides no project packaging or recovery UI.

Export stops playback, renders the Edit offline through Tracktion, and moves the resulting temporary WAV into place only if rendering succeeds. The prototype uses 44.1 kHz, 24-bit stereo and includes track and master plugins. The render covers the Edit length and runs synchronously, so the window waits for export to finish.

The Windows CTest runs the executable in smoke-test mode. It generates a short input WAV, imports it onto two tracks, inserts a native effect, saves and reloads the Edit, and checks the channel count, length, and signal level of an offline render. This test uses synthetic audio and does not depend on an attached interface.

## MIDI routing and Setup

In v0.24.1, Piano Roll audition uses the MIDI clip’s effective channel and the containing track’s instrument path. The Clip Inspector edits clip properties. Track-owned MIDI device selection and input/output channels are deferred under the approved routing amendment. [MIDI track routing amendment](specifications/midi-track-routing.md)

Setup uses JUCE’s device selector and lists OS-visible MIDI inputs. The selector enumerates on open and re-enumerates on AudioDeviceManager change notifications. Tracktion maintains its own hardware MIDI list, polling every four seconds by default and disabling automatic scans if an input or output enumeration exceeds 200 ms. Reopening Setup refreshes its list independently of Tracktion’s list. See the [branch and Setup inspection](research/midi-track-routing-review.md) for implementation findings.

## Universal Actions Toolbar

The four fixed 2×2 action groups use the same dispatcher as the Edit menu. The global UNDO and REDO buttons use the project's Tracktion UndoManager through the same history handler as the menu. Both are available independently of editing area while transport is stopped. The Delete shortcut also clears the ruler range when the Timeline has focus and its clip selection is empty.

The dispatcher uses the active editing area. Timeline, Track List, Piano Roll, and Drum Pattern Editor retain separate selections. Toolbar clicks preserve that focus. The Inspector shows the current track or clip and owns its property controls. Clipboard data stays in the current project. Selection, transport state, valid placement, and bounds determine which commands are enabled.

| UAT group | Timeline | Piano Roll | Drum Pattern Editor |
| --- | --- | --- | --- |
| EDIT | Cut, Copy, Paste, Delete clips | Cut, Copy, Paste, Delete notes | Cut, Copy, Paste, Delete hits |
| MOVE | Move selected clips in time or across compatible tracks | Move notes in time by the snap increment or transpose by semitone | Move hits one sixteenth note or to an adjacent kit lane |
| SIZE | Adjust the start or end of a single eligible clip | Adjust the start or end of a single eligible note | Disabled |
| MODIFY | Duplicate, Clone/Unclone, Split, Trim | Disabled | Disabled |

Track List focus supports track copy, paste, delete, and sibling movement subject to the existing eligibility rules. Duplicate works for ordinary tracks and drum lanes. The clip-specific Clone/Unclone, Split, and Trim commands are disabled in Track List focus. Global Undo and Redo are available while stopped.

The clip Inspector owns Loop ON/OFF and repeat count. Timeline resizing and existing mouse gestures remain available. Timeline Duplicate creates an independent clip. Clone creates a linked child from an independent clip or an existing parent. The same button reads Unclone for a child and makes it independent. The Inspector and MAX timeline show a gold crown for a parent and a chain link for a child. Independent clips have no badge, and MIN hides timeline clone badges. Family and order persist in the Edit. Deleting a parent promotes the oldest surviving child while preserving content and clip positions. A lone survivor is independent. Undo restores the original family roles.

Track UAT COPY snapshots the complete track into a project-local clipboard. PASTE inserts the snapshot below the current selected track, remaps object IDs, preserves compatible input assignment and arm state, and creates a unique name. The source and insertion anchor are independent. Insert and Paste share the same 24-track guard and scroll new tracks into view. At 24 tracks or when the original input cannot be duplicated faithfully, paste leaves the Edit intact.

Inspector COPY SETTINGS / APPLY SETTINGS uses a separate project-local snapshot. It transfers mixer values, effects and output routing. Existing input, arm, monitoring, name, color, height, clips, mute and solo are preserved. Compatible destination automation follows matching effect identities and parameter IDs. Repeated effects match by occurrence when their counts agree. A missing automation target or feedback route rejects the apply before mutation.


Track creation writes the track type to the Tracktion edit state. Untyped tracks with existing clips are classified from their contents. Empty untyped tracks keep prior MIDI creation labels where available and otherwise become Audio. Mixed legacy tracks are reported and retain their existing clips. New placement and duplication are blocked for mixed legacy tracks. Clip Paste, vertical Move, drag and Insert preflight type compatibility before committing. Recording also checks the types of armed destinations.

## Master Drum Track MD0

The protected singleton Master owns the drum MIDI clips. The Tracktion graph reads those clips for up to 16 child lanes, each with its own channel-10 GM note mapping, one-shot WAV sampler, and audio processing. Lane output routes by Master track ID to the Drum Master bus. FX1 and FX2 sends use named Tracktion auxiliary returns when present. Overlap OFF releases the previous voice over 3 ms at retrigger. Lane solo filters the kit, and Master solo isolates it from ordinary tracks.

Settings enables or disables the kit and its processing graph. Collapse only affects visibility. The Master and child lanes are excluded from the 24 ordinary tracks. Shared MIDI clip insertion supports 1–4 bars. Double-clicking a drum clip opens the slide-up Drum Pattern Editor with 16th-note steps, 64th-note subdivisions, and an aligned velocity panel. The viewport displays up to 32 steps and scrolls for longer patterns. Linked edits share the MIDI mutation and Undo/Redo infrastructure with Piano Roll. Drum Mixer and MD1 recording and monitoring remain deferred. [MD0 acceptance record](testing/md0-drum-master.md#final-acceptance)
