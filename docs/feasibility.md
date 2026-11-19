# Phase 1 feasibility

## Dependency choice

Tracktion Engine v3.2.0 requires C++20. Its RecordingDemo, MidiRecordingDemo and PluginDemo exercise the Edit, transport, input-device and plugin APIs used here. The release pins JUCE at `19edd538429c93d277bf95b55aaa7e3eb545f951`. The build uses this pair to avoid combining Tracktion with a newer, independently released JUCE major version. CMake loads only the library modules. [Tracktion release](https://github.com/Tracktion/tracktion_engine/releases/tag/v3.2.0) · [Pinned JUCE source](https://github.com/juce-framework/JUCE/tree/19edd538429c93d277bf95b55aaa7e3eb545f951)

## Verification

| Capability | Implementation | Verification needed |
| --- | --- | --- |
| Device enumeration, sample rate and buffer selection | JUCE device selector | Windows interface and driver checks |
| ASIO | Opt-in build with external SDK headers | Compile with SDK and test a vendor driver on Windows 11 |
| Audio recording and multitrack playback | Tracktion transport and input routing | Record and replay real takes, check latency and dropouts |
| MIDI input and recording | Physical MIDI routing and built-in synth | Record and replay notes from a real controller |
| VST3 | Scan a selected file or bundle and insert a discovered plugin | Exercise x64 effect and instrument plugins, including reopen |
| Reverb, save/reopen and stereo WAV | Implemented with an automated smoke test | Windows workflow result |
| MP3 export | Investigated | Select an encoder and validate output and distribution terms |
| MIDI control surfaces | Investigated | Pick a target controller and verify transport, faders and feedback |

The development workspace could inspect source through the GitHub connector. Its Git proxy was unreachable and CMake was unavailable. It could not compile the application or run CTest locally. The Windows workflow is the build and smoke-test gate. It cannot establish physical interface, ASIO latency, end-to-end MIDI or third-party VST3 compatibility.

## Open questions

- VST3 discovery is limited to a user-selected path. In-process scanning can stall or crash if a plugin misbehaves. Validate plugin parameters and state after project reopen before adding a library scan or plugin editor.
- JUCE's LAME format adapter invokes an external encoder. A Windows Media Foundation encoder or a separately packaged LAME integration can be evaluated after WAV export is stable. Compare availability, output quality and distribution terms for a chosen implementation. [JUCE LAME adapter](https://docs.juce.com/master/classjuce_1_1LAMEEncoderAudioFormat.html)
- Tracktion Engine provides control-surface infrastructure for transport, faders and feedback. Begin with one physical controller and map a small set of commands. Verify reconnect, MIDI port ownership and interactions with note input. [Tracktion Engine features](https://github.com/Tracktion/tracktion_engine/blob/v3.2.0/FEATURES.md)

## Next milestone

Run a Windows 11 acceptance pass with a vendor ASIO interface, a MIDI keyboard and a known VST3 effect. Record two audio takes and one MIDI take, save and reopen the Edit, then compare stereo export with playback. Measure round-trip latency and watch for dropouts at common buffer settings before expanding the interface.
