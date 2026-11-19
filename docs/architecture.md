# Phase 1 architecture

The JUCE application owns a Tracktion Engine and a single active Edit. Tracktion manages the transport, device callbacks, recording, audio/MIDI clips, and the plugin processing graph. Its audio tracks can hold either audio or MIDI. JUCE's device selector controls the same audio device manager that Tracktion uses for playback and recording.

The prototype uses a button panel and a track selector. UI actions resolve the selected track, configure input routing and plugin chains, and start or stop the transport on the message thread. The application adds no audio callback of its own. Dialogs, file selection, persistence, input enumeration, and plugin scanning execute outside the real-time callback. Tracktion handles callback processing and recording buffering. Third-party plugins supply their own processing code.

Project files use Tracktion's .tracktionedit serialization. The application uses EditFileOperations to save an Edit associated with the chosen path. Recordings and imported media remain separate files. The prototype relies on Tracktion's media path handling and provides no project packaging or recovery UI.

Export stops playback, renders the Edit offline through Tracktion, and moves the resulting temporary WAV into place only if rendering succeeds. The prototype uses 44.1 kHz, 24-bit stereo and includes track and master plugins. The render covers the Edit length.

The Windows CTest runs the executable in smoke-test mode. It generates a short input WAV, imports it onto two tracks, inserts a native effect, saves and reloads the Edit, and checks the channel count, length, and signal level of an offline render. This test uses synthetic audio and does not depend on an attached interface.
