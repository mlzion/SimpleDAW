# SimpleDAW project status

## Windows build policy

Windows x64 development, acceptance and distribution use `/MD`. The acceptance workflow contains one `/MD` job and runs only by explicit, authorized manual dispatch. `/MT` remains available for requested local diagnostics. The source package, notices and separately installed Microsoft runtime requirements continue to apply. The policy branch passed all five CTests on local Linux, including Effects Foundation E0, and static checks of workflow triggers and package contents. Windows CI was not run for this update. [Build policy](docs/build/windows-build-policy.md) · [Diagnostic commands](docs/msvc-dynamic-runtime.md#requested-mt-diagnostics)

## v0.22.1 Universal Actions Toolbar

v0.22.1 replaces the Edit toolbar with the permanent EDIT, MOVE, SIZE and MODIFY groups. Timeline and Tracks have independent mouse selections and visible focus. Inspector and read-only Effects can take focus. Single-clip Copy, Paste and Delete use Tracktion clipboard and undo operations. Windows x64 CI for `ccc2153` passed all four CTests on /MD and /MT, including the new `universal_actions` suite and the three v0.21 regression suites. Both builds passed version, runtime-import and isolated startup checks. The /MD artifact includes the executable and corresponding source. [Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/44856382800) · [Windows x64 artifact](https://github.com/mlzion/SimpleDAW/actions/runs/44856382800/artifacts/14688326869)

User-reported Windows live acceptance is complete with 45 PASS, 0 PARTIAL, 0 FAIL and 0 UNTESTED across the grouped acceptance checks. The record identifies individual checklist subconditions that were not independently verified. Editing the boundaries of an existing ruler time selection remains a carry-forward usability item. CREATE, MOVE, SIZE, SPLIT, TRIM and reserved positions are disabled. Advanced multiselection edits, unfinished editor functionality and recording/take changes are deferred. [Architecture and action availability](docs/architecture.md#universal-actions-toolbar) · [Windows live acceptance record](docs/testing/v0.22.1-live-test.md)

## v0.22.2 — accepted Windows baseline

PR #10 implements the PR #12 simplification. Timeline groups retain Cut, Copy, Paste, Delete and Move. SIZE requires a single Timeline clip, and MODIFY remains disabled. Horizontal MOVE and single-clip SIZE use 10 ms with Snap Off. Tracks use single selection, confirmed Delete and one-row reorder. UAT COPY/PASTE duplicates the complete track below the current selection. Inspector COPY SETTINGS / APPLY SETTINGS transfers selective mixer, effect and output settings while preserving destination automation and recording inputs. [Task sheet](docs/tasks/v0.22.2-first-task-sheet.md) · [Architecture](docs/architecture.md#universal-actions-toolbar)

The owner accepted `f1b872b3880abff19679458132e8574eddb5097c` on Windows. R1–R4 and audio playback passed. Live MIDI playback is deferred until MIDI file import is implemented. Windows x64 `/MD` and `/MT` each passed four CTests, version verification, runtime-import inspection and isolated startup. The GitHub Actions Node.js 20 deprecation warning is nonblocking maintenance. The v0.22.2 acceptance applies to this tested commit. Windows integration verification for the combined v0.22.2 and Effects Foundation E0 tree remains pending. Windows x64 `/MD` is the standard build and distribution runtime. [Acceptance record](docs/testing/v0.22.2-live-test.md) · [Windows CI](https://github.com/mlzion/SimpleDAW/actions/runs/44961160114) · [Build policy](docs/build/windows-build-policy.md)

### Targeted UAT rework

R1 shares the 24-track limit between Insert and Paste and reveals newly pasted rows. R3 keeps independent selections and derives highlights and Inspector context from focus. R2 enforces a 1/16-bar minimum for SIZE shrink in musical beats, with the original start meter and extension allowed for existing short clips. R4 persists fixed Audio/MIDI track types and rejects incompatible placement atomically. Untyped homogeneous tracks are inferred, and mixed legacy tracks are reported with their clips preserved.

All four CTests passed in a local Linux build with pinned JUCE and Tracktion. The cases cover track 24 and its persistence, focus and selection, size boundaries across tempo changes, compatible and incompatible Paste/Move/drag, Undo/Redo, and legacy track typing. Track renaming and compact number/type labels are deferred to v0.23. Preferred labels are `1A Track 1`, `2A Track 2` and `3M Track 3`. Preserve existing user-defined track names when implementing the format. MIDI file import and live MIDI playback verification remain pending. [Rework task sheet](docs/tasks/v0.22.2-uat-rework-task-sheet.md) · [Acceptance checklist](docs/testing/v0.22.2-live-test.md)

## Current development

Phase 1 audio engine and repository documentation have merged into main. The v0.21 Main GUI Foundation merged in PR #3 after user-reported Windows live acceptance (2026-11-20), with documented deferrals. [Phase 1 PR #1](https://github.com/mlzion/SimpleDAW/pull/1) · [Repository documentation PR #2](https://github.com/mlzion/SimpleDAW/pull/2)

The JUCE window adds a two-row transport, a shared track-list and arrangement view, and collapsible resizable Inspector and workspace panels. The view reads Tracktion's Edit and uses engine track, clip, metering and transport APIs. The shared row layout keeps vertical scrolling and MIN/MAX track heights synchronized. JUCE application properties persist panel sizes and visibility.

Both Audio Track and MIDI Track commands create Tracktion audio tracks, which support both kinds of clips. The MIDI label is a workflow distinction. Input arming and plugin setup remain accessible from Options and Insert. Project format and the source-paired Windows /MD distribution are carried forward. [Phase 1 architecture](docs/architecture.md) · [Windows distribution](docs/windows-download.md)

## Additional approved future design — 2026-11-21

Clip GLUE, GROUP/UNGROUP, one-level Parent/Child Track hierarchy with audio parent buses and Track List drag/drop, and clip MIX/REPLACE overlap precedence/visual indicators are accepted **design only**, not implemented or tested. See [decision supplement](docs/specifications/clip-glue-group-parent-overlap-2026-11-21.md). This does not change open v0.22.2 implementation scope. No Windows CI or application testing was requested or run for this documentation update.

## Specifications and planning

PR #4 merged the [specification and phase-plan index](docs/specifications/README.md) and three original PDFs. The import verified SHA-256 hashes and visually inspected all 14 pages. The phase plan is Draft 0.1. Its phase identifiers differ from the earlier audio-engine Phase 1. Pending design decisions remain listed in the index. [Planning PR #4](https://github.com/mlzion/SimpleDAW/pull/4) · [Historical project status](https://github.com/mlzion/SimpleDAW/blob/2987bf4/PROJECT_STATUS.md)

## PR #3 corrections

Physical inputs retain independent track destinations. The assignment menu and temporary channel badges read Tracktion's enabled state. Timeline moves and ranges preview during drags, recording thumbnails use Tracktion's live cache, and finalized media can draw from its source while a playback proxy is unavailable. Both audio import commands use the snapped edit cursor.

Missing WAV sources remain in the Edit with relink and confirmed delete actions. New, Open and Exit prompt for unsaved edits. The read-only Effects workspace shows the selected track's actual chain. Missing external plugins remain in the project and are reported on open. [Live retest checksheet](docs/testing/v0.21-live-test.md)

The audio monitoring report exposes the active device, buffer, reported delays, monitor routing and plugin latency without changing engine behavior. The driver/interface and physical loopback have yet to be measured. [Monitoring investigation](docs/testing/v0.21-monitoring.md)

The `project()` version in `CMakeLists.txt` supplies the application and Windows properties. A zero patch on a three-part version is omitted from the title and release names. CMake generates the executable path and artifact name for CI. For v0.21, the executable is `SimpleDAW-v0.21.exe` and GitHub supplies `SimpleDAW-v0.21-Windows-x64.zip`. [Windows download guide](docs/windows-download.md)

Removing an input destination now requests Tracktion's graph update explicitly, including when another track still uses the input. Existing audio assignments can be removed during playback. Space and Delete are window shortcuts that act on key-down and respect text focus. Delete uses the existing item action first, or clears the ruler range and loop markers when no item is selected. [Final live acceptance checks](docs/testing/v0.21-live-test.md)

## Verification

The final correction workflow at `53425ec` passed all three Windows x64 CTests, version properties, PE import inspection, and isolated startup for `/MD` and `/MT`. The correction tests cover hosted input meter checks across route removal, Space transport dispatch, and Delete selection precedence, undo, and text protection. [Final correction CI](https://github.com/mlzion/SimpleDAW/actions/runs/44679304999) · [Current PR checks](https://github.com/mlzion/SimpleDAW/pull/3/checks)

The existing generated-audio CTest covers import, project reopen, reverb persistence and non-silent stereo export. The v0.21 GUI CTest covers component initialization, track creation and selection, transport seek and loop state, timeline zoom, MIDI item insertion, project reopen and panel settings. Windows CI runs these tests and also inspects PE imports and launches the packaged executable from an isolated directory. The correction CTest adds drag previews, snapped placement, dirty-state transitions, independent effects, unavailable plugin retention, missing-audio relinking, waveform reload and multiple recording targets on one hosted input. Consult the workflow result for the exact commit. [Windows workflow](https://github.com/mlzion/SimpleDAW/actions/workflows/windows.yml)

User-reported Windows 11 live acceptance completed on 2026-11-20: 43 functional checks passed, D03 and E01 partial, F03 MIDI hardware test not performed. C06 final punch capture passed, but its live waveform preview is misaligned and deferred to v0.22. The post-recording Save/Delete prompt is also proposed for v0.22. The tested build is probably workflow #49 (not locally verified). Hardware: HD USB Audio Device, Windows Audio Exclusive Mode, 48 kHz / 480 samples; actual round-trip latency was not measured. VST3 amp modelers and built-in reverb passed, exact plugin names unrecorded. See the updated live acceptance checksheet. [Live test checksheet](docs/testing/v0.21-live-test.md)

## Known limitations

- Inspector content and Mixer, Piano Roll and Drum Editor content are placeholders. Effects shows the chain read-only. The read-only 1 and 2 input badges are temporary until Inspector routing is available. Drum Master processing is planned for v0.26.
- Duplicate remains disabled. Single-clip mouse dragging works within a lane while stopped. Complex looping, stretched media and overlapping takes can have simplified previews. Locating audio updates the selected clip source and matching take references.
- Save As creates a copy in the current folder so relative media paths still resolve. Source media remain in their original locations.
- Count-in is off or one bar. Punch and Loop share the ruler selection. If a range has not been selected or set in the project, Loop uses the arrangement length or at least one bar.
- VST3 scans run in-process and WAV export waits synchronously. The distributed build uses the system Microsoft x64 v14 runtime. The optional ASIO build remains separate. [Feasibility notes](docs/feasibility.md) · [Licensing](docs/licensing.md)

## Effects foundation E0

The isolated effects branch adds a windowless CTest path for the pinned Tracktion Reverb and Compressor. Native plugin lists, parameters, bypass and Edit serialization supply the foundation. The Compressor serves as a second insert for chain verification. The proposed Inspector/Effects Editor contract retains eight user audio-effect slots per track and records the remaining decisions for E1. [E0 tests and integration contract](docs/testing/effects-foundation-e0.md)

Local Debian 13 x64 Release build with Clang 19 and Xvfb passed all five CTests: `project_round_trip`, `main_gui`, `gui_corrections`, `universal_actions` and `effects_foundation`. Windows CI has not been dispatched and live device acceptance is pending.

## Effects Editor E1

The E1 feature branch adds a reusable slide-up editor for native Tracktion Reverb and Compressor. It binds to track and plugin IDs in the active Edit, edits native parameter state during playback, displays bypass, and closes on target deletion or project replacement. Reverb uses independent Wet and Dry levels. Inspector owns chain actions and will call the editor on an effect-row double-click. [Integration contract and acceptance checklist](docs/testing/effects-editor-e1.md)

**PASS** — Debian 13 x64 Release build with Clang 19 and Xvfb passed all six CTests, including E0 and the new `effects_editor` suite. Five additional consecutive E1 runs passed. **PENDING** — Inspector integration and separately authorized integrated Windows x64 /MD build and live acceptance.
