# Phase 1 feasibility

## Portable Windows x64 executable

CMake builds Tracktion Engine v3.2.0 and its pinned JUCE 8 revision (`19edd538429c93d277bf95b55aaa7e3eb545f951`) into `SimpleDAW.exe`. The distributed Windows Release target uses the dynamic MSVC runtime and requires the [Microsoft Visual C++ v14 x64 Redistributable](https://aka.ms/vc14/vc_redist.x64.exe) if a compatible version is absent. Windows-provided DLLs and API sets supply operating-system functionality, including graphics and audio. The optional ASIO host backend uses an installed x64 driver. Installed VST3 plugins can bring their own dependencies. These components are supplied by Windows or the hardware and plugin vendors. [Tracktion release](https://github.com/Tracktion/tracktion_engine/releases/tag/v3.2.0) · [Pinned JUCE source](https://github.com/juce-framework/JUCE/tree/19edd538429c93d277bf95b55aaa7e3eb545f951)

The Windows workflow checks load-time and delay-load imports reported by `dumpbin /dependents` and verifies the `/MD` imports against the installed Visual C++ runtime. The verifier also supports system-only import checks for explicitly requested local `/MT` diagnostics. It stages only the EXE, verifies that its directory denies writes, restricts PATH to System32, and runs both the smoke test and normal window startup from a different working directory. Import inspection cannot inventory DLLs explicitly loaded at runtime. The GitHub runner is a Windows Server image with Visual Studio installed. A standard-user launch on a clean Windows 11 machine without Visual Studio and with Microsoft's separately installed redistributable remains an acceptance test.

Tracktion writes preferences under the user's application-data directory. The user chooses the project and export paths. Recordings are managed by Tracktion with the edit's media, while imported WAV files are referenced separately. Move the executable independently of these files. The project still needs writable data storage and does not package its media. [Tracktion property storage](https://github.com/Tracktion/tracktion_engine/blob/v3.2.0/modules/tracktion_engine/utilities/tracktion_PropertyStorage.cpp)

## Verification coverage

| Capability | Current coverage | Hands-on verification |
| --- | --- | --- |
| Audio device, sample rate and buffer selection | Implemented via the JUCE device selector in the standard build. Startup test opens the app window. | Open Devices, switch backends and channels, and apply a supported rate and buffer. |
| ASIO | Conditional code path using an externally supplied SDK. The standard CI build excludes ASIO. | Compile against the selected SDK and use an installed vendor driver on Windows 11. |
| Audio record and playback | Implemented through the Tracktion transport and input routing. The smoke test imports generated audio. | Arm an input, record a take, stop, and listen to playback. Check latency and dropouts. |
| Multiple audio tracks | The smoke test creates two tracks, reopens both with their clips, and renders the edit offline. | Record and listen to multiple tracks through a physical interface. |
| MIDI input | Physical MIDI routing, record controls and a built-in synth are implemented. | Record and replay notes from a physical keyboard. |
| VST3 | The standard build includes selected-path scanning and loading. | Scan, load and exercise an installed x64 effect and instrument. Reopen the project and verify plugin state. |
| Built-in reverb | The smoke test inserts reverb, verifies it persisted across reopen, and renders the edit. | Audition the audible effect and compare it with dry playback. |
| Project save and reopen | Automated round trip checks track, clip and reverb persistence. | Reopen a project with actual recordings and confirm media is available. |
| Stereo WAV export | The automated test checks a non-silent output with two channels, 44.1 kHz, 24-bit depth and a minimum duration. | Compare the exported mix with playback from the real project. |

The [Windows workflow](https://github.com/mlzion/SimpleDAW/actions/workflows/windows.yml) records the build, dependency inspection, and runtime checks. [Licensing](licensing.md) describes the release package and runtime prerequisite.

## Known limitations

- VST3 scanning inspects a user-selected path in the application process. A failing or stalled plugin can affect that process.
- The smoke test renders offline with generated media. Device configuration, MIDI capture, real-time playback and third-party plugin compatibility need hands-on checks.
- MP3 export and MIDI control surfaces remain investigation items. JUCE can invoke an external LAME encoder, and Tracktion provides control-surface infrastructure. [JUCE LAME adapter](https://docs.juce.com/master/classjuce_1_1LAMEEncoderAudioFormat.html) · [Tracktion Engine features](https://github.com/Tracktion/tracktion_engine/blob/v3.2.0/FEATURES.md)
