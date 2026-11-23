# Codex task sheet — MD0 Master Drum Track foundation
Date: 2026-11-23
Status: PREPARED FOR REVIEW; IMPLEMENTATION NOT AUTHORIZED
Source of truth: [MD0 specification](../specifications/md0-master-drum-track-2026-11-23.md)

## Preconditions / isolation
1. Do not start coding until owner explicitly authorizes MD0. Do not trigger Windows x64 Release /MD CI, merge, or modify the Piano Roll P0 integration branch without separate approval.
2. Confirm latest accepted main and PR #24 Piano Roll integration status. Use an isolated MD0 feature branch once authorized; reconcile integration only after Piano Roll Windows acceptance/merge as directed.
3. Preserve existing UAT, Inspector, effect, track-limit, MIDI routing and source-paired Windows /MD packaging conventions.

## Implementation work packages (ordered, subject to review)
- MD0-A: protected singleton MDT model, legacy project migration, Settings enable/disable, collapse, persistence and 24-track exclusion; prove disabled path is skipped.
- MD0-B: child lane model (max 16), GM mapping, duplicate/layering, add/delete/reorder, independent sample/audio settings, Inspector bindings and Undo/Redo.
- MD0-C: WAV library discovery relative to executable, ZIP packaging and legal sample assets, categorization, audition, user additions, rescan, persistence/relink.
- MD0-D: channel-10 MIDI dispatch and one-shot per-lane sampler, velocity, overlap, 3 ms click suppression, audio bus and FX sends; no recording/monitoring.
- MD0-E: shared MIDI UAT Insert 1–4-bar length selector (ordinary MIDI + MDT), clip transfer conversion, clone/duplicate/loop and linked Undo.
- MD0-F: inline MDT pattern editing, 16th step grid (up to 32 steps visible in 4/4), horizontal scroll, 64th popup/dots, selection and velocity interactions.
- MD0-G: automated tests, documentation of deviations and Windows live acceptance checklist. No Windows CI without explicit approval.

## Test expectations
- Singleton and protected deletion; new/legacy project defaults; enable/disable persists, hides and bypasses processing, no unintended ordinary-track-count use.
- Starter five lanes and samples; 16-lane limit; add/delete/undo; duplicate with same GM note layers audio and shares MIDI notes, remapping separates triggers.
- Sample prefix discovery (case-insensitive), user-added file rescan, unknown type, audition, missing/relink, save/reopen, Windows ZIP relative path.
- GM note routing channel 10, MIDI clip conversion preserving notes/timing/velocity, unsupported notes silent but retained, Undo/Redo.
- One-shot ignores Note Off, velocity response, overlap on/off, 3 ms anti-click fade, Stop and Disable voice termination, mute/solo, bus FX sends and no disabled CPU audio path.
- Shared Insert MIDI Clip 1–4 bars, snap, last selection, ordinary MIDI regression; imported/recorded long clips unaffected.
- Inline edit only with selected pattern, 16/32 visible 16th steps in 4/4, horizontal scroll for longer patterns, 64th popup and indicator dots, parent velocity, linked clone and Undo.
- Linux automated suites and repeatability; Windows /MD build and live audio/GUI acceptance only after explicit authorization.

## Explicit exclusions
No Drum Mixer, no MD1 record/monitor, no general MIDI routing implementation beyond required MDT interoperability, no unrelated Piano Roll edits, no new effects, no unapproved Windows CI or merge.

## Handoff requirement
Codex should first report API feasibility, risks, work-package boundaries and proposed tests; do not interpret this task sheet as permission to implement.
