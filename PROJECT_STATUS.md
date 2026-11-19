# SimpleDAW project status

## Current development

Phase 1 audio engine and repository documentation have merged into main. The v0.21 Main GUI Foundation is on `feature/v0.21-main-gui` and awaits Windows review. [Phase 1 PR #1](https://github.com/mlzion/SimpleDAW/pull/1) · [Repository documentation PR #2](https://github.com/mlzion/SimpleDAW/pull/2)

The JUCE window adds a two-row transport, a shared track-list and arrangement view, and collapsible resizable Inspector and workspace panels. The view reads Tracktion's Edit and uses engine track, clip, metering and transport APIs. The shared row layout keeps vertical scrolling and MIN/MAX track heights synchronized. JUCE application properties persist panel sizes and visibility.

Both Audio Track and MIDI Track commands create Tracktion audio tracks, which support both kinds of clips. The MIDI label is a workflow distinction. Input arming and plugin setup remain accessible from Options and Insert. Project format and the source-paired Windows /MD distribution are carried forward. [Phase 1 architecture](docs/architecture.md) · [Windows distribution](docs/windows-download.md)

## Verification

Windows 2022 x64 Release at `2f45cdc` built successfully with /MD. CTest passed both tests and the executable passed PE import inspection and isolated startup. The workflow published the executable with matching source and notices. [Windows build and tests](https://github.com/mlzion/SimpleDAW/actions/runs/44616864521) · [Feature-branch artifact](https://github.com/mlzion/SimpleDAW/actions/runs/44616864521/artifacts/14576730794)

The existing generated-audio CTest covers import, project reopen, reverb persistence and non-silent stereo export. The v0.21 GUI CTest covers component initialization, track creation and selection, transport seek and loop state, timeline zoom, MIDI item insertion, project reopen and panel settings. Windows CI runs both tests and also inspects PE imports and launches the packaged executable from an isolated directory. Consult the workflow result for the exact commit. [Windows workflow](https://github.com/mlzion/SimpleDAW/actions/workflows/windows.yml)

Live Windows 11, hardware recording, count-in and punch behavior, playback with real devices, and installed VST3 compatibility await user verification. [Live test checksheet](docs/testing/v0.21-live-test.md)

## Known limitations

- Inspector content and Mixer, Effects, Piano Roll and Drum Editor content are placeholders. Track indicators are read-only. Drum Master processing is planned for v0.26.
- Cut, Copy, Paste and Duplicate are disabled. Basic clip moves work within a lane while stopped. The timeline draws the media of finalized recordings after Stop. Complex looping and stretched media can have simplified previews.
- WAV import uses the selected track at project start. Save As creates a copy in the current folder so relative media paths still resolve. Source media remain in their original locations.
- Count-in is off or one bar. Punch and Loop share the ruler selection. If a range has not been selected or set in the project, Loop uses the arrangement length or at least one bar.
- VST3 scans run in-process and WAV export waits synchronously. The distributed build uses the system Microsoft x64 v14 runtime. The optional ASIO build remains separate. [Feasibility notes](docs/feasibility.md) · [Licensing](docs/licensing.md)
