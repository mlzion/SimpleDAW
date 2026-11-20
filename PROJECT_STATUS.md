# SimpleDAW project status

Snapshot reviewed on **2026-11-19**. Update this file with verified changes at the end of each development assignment.

## Specification and planning update — 2026-11-20

This documentation-only change adds the [specification and phase-plan index](docs/specifications/README.md) and three original PDFs. All files were parsed, all 14 pages rendered for visual inspection, and copied bytes verified with SHA-256. No application tests or live hardware acceptance ran for this import.

The local `main` history at `d036d97` confirms PRs #1 (audio engine) and #2 (repository documentation) merged. The older snapshot below predates those merges. The current `origin/feature/v0.21-main-gui` status records v0.21 GUI implementation and automated verification evidence, with Windows/hardware acceptance still pending; consult [PR #3](https://github.com/mlzion/SimpleDAW/pull/3) and its branch status for current details. This change does not certify live acceptance or complete a development phase.

The supplied phase plan is Draft 0.1; accepted requirements, pending decisions, implementation and verification remain separate. Its phase numbers are planning identifiers and differ from the earlier audio-engine Phase 1. Next: review this documentation PR, then confirm v0.21 live acceptance before subsequent implementation assignments. The historical status below is retained as a dated record.

## Project Overview

SimpleDAW is a digital audio workstation for musicians who want an approachable way to record and finish a project. Its design favors clear controls and a simple workflow. The current prototype is described on the Phase 1 branch. [Prototype README](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/README.md)

## Current Development Phase

**Phase 1 — Audio engine feasibility.** The documented objectives are to demonstrate basic recording, MIDI and plugin hosting, project persistence, and stereo WAV export, and to validate a portable Windows x64 test build. The prototype is available for review and testing. The phase has no recorded acceptance or merge.

## Completed Work

- `main` has the initial README and ignore rules. No feature or documentation pull request had merged at the initial review. [Main history](https://github.com/mlzion/SimpleDAW/commits/main/)
- Windows CI built and published a Phase 1 test ZIP from the branch for PR #1. The ZIP pairs the `/MD` executable with its exact source archive, build instructions and licensing notices. This is a branch artifact. [Publishing run](https://github.com/mlzion/SimpleDAW/actions/runs/44478494087) · [Windows download](https://github.com/mlzion/SimpleDAW/actions/runs/44478494087/artifacts/14507831861)

## Work in Progress

- `feature/phase1-audio-engine` — [PR #1](https://github.com/mlzion/SimpleDAW/pull/1), open against `main`. The branch implements device selection, audio and MIDI recording, multitrack playback, selected-path VST3 hosting, built-in reverb, project save and reopen, and stereo WAV export. Release and development builds pass CI. Review and hands-on acceptance are outstanding.
- `docs/project-memory` — [PR #2](https://github.com/mlzion/SimpleDAW/pull/2), open for review. Establishes shared repository context and documentation maintenance rules.
- Separate GUI work is outside this assignment. Its branch, PR, scope and status are awaiting confirmation from a repository reference.

## Important Design Decisions

- CMake and MSVC build a JUCE application with pinned JUCE 8 and Tracktion Engine v3.2.0. Tracktion owns the active Edit, transport, audio/MIDI graph, and recording. Application UI actions run on the message thread. [Phase 1 architecture](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/architecture.md)
- The Phase 1 GUI uses a compact button panel, device controls and a track selector. Future GUI specifications are awaiting confirmation in the repository. Preserve the approved GUI scope for the active assignment.
- Project edits use Tracktion's `.tracktionedit` format. Recordings and referenced media are kept separately from the edit. WAV export renders the edit offline. [Phase 1 architecture](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/architecture.md)
- PR #1 uses AGPLv3 for SimpleDAW's original code. Windows downloads pair the single-file `/MD` executable with matching source and notices. A compatible Microsoft Visual C++ v14 x64 Redistributable is installed separately if needed. `/MT` is for development checks. ASIO is optional and excluded from the packaged build. [Licensing](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/licensing.md) · [Download guide](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/windows-download.md)

## Testing Status

- **Passed on the hosted Windows runner.** Both `/MD` and `/MT` x64 Release built from extracted matching source at `2611123`, passed CTest 1/1, PE import inspection, and isolated normal startup. The smoke test covers generated audio on two tracks, reverb persistence after project reopen, and non-silent stereo WAV export. The runner had the Microsoft runtime installed. [Windows CI run](https://github.com/mlzion/SimpleDAW/actions/runs/44478494087)
- **Failed qualification.** The MinGW runtime probe ran, but pinned JUCE 8.0.6 rejected MinGW while building its helper from the archive. No alternative SimpleDAW executable was produced. MSVC is the documented Windows toolchain. [LLVM/MinGW investigation](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/llvm-mingw.md)
- **Awaiting hands-on testing.** Clean Windows 11 standard-user launch, device and buffer selection, physical audio recording and live playback, ASIO, MIDI capture, and installed VST3 compatibility. [Verification matrix](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/feasibility.md)

## Known Issues

- VST3 scanning runs in the application process. A stalled or failing plugin can affect the app. Offline export waits synchronously, and project packaging and recovery UI are outside the current prototype. [Feasibility](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/feasibility.md) · [Architecture](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/architecture.md)
- MP3 export and MIDI control-surface support are investigation items. [Feasibility](https://github.com/mlzion/SimpleDAW/blob/feature/phase1-audio-engine/docs/feasibility.md)

## Next Steps

1. Review PR #1 and test the published build on a clean Windows 11 x64 machine with the Microsoft runtime installed.
2. Verify the physical audio, MIDI and plugin workflows. Test ASIO separately with the optional SDK build and a vendor interface.
3. Record acceptance results and remaining issues before treating Phase 1 as complete. Confirm any follow-on GUI work through its own specification or PR.

## Recent Updates

- **2026-11-19** — Initialized the repository on `main` and opened PR #1 for the Phase 1 prototype. [Initial commit](https://github.com/mlzion/SimpleDAW/commit/99155b1a46e873d8fe9439755adddaeefe008d08) · [PR #1](https://github.com/mlzion/SimpleDAW/pull/1)
- **2026-11-19** — Added AGPLv3 licensing and corresponding-source verification, investigated MinGW, and passed the MSVC `/MD` and `/MT` comparison. [Licensing commit](https://github.com/mlzion/SimpleDAW/commit/005893df49e5fb2956f25a438234dc077975c41f) · [MSVC verification](https://github.com/mlzion/SimpleDAW/actions/runs/44475680816)
- **2026-11-19** — Made `/MD` the Windows release default and published the first source-paired Windows test artifact from PR #1. [Publishing run](https://github.com/mlzion/SimpleDAW/actions/runs/44478494087)

- **2026-11-19** — Opened the documentation-only project memory PR. [PR #2](https://github.com/mlzion/SimpleDAW/pull/2)
