# SimpleDAW — Mixer MX0 Foundation (Approved)

Status: APPROVED for independent foundation implementation, 2026-11-23.
Branch: `feature/mixer-mx0`. This document scopes MX0; it does not authorize merge or Windows CI.

## Intent
A unified full-width bottom-docked mixer workspace for final song balancing and metering. The mixer is NOT an Editor window. When shown, hide the Inspector and editor panels while retaining their state and selections; timeline and transport remain usable. Restore the previous workspace when hidden. Avoid changing underlying audio behavior simply by switching workspaces.

## MX0 implementation scope
1. Full-width mixer beneath timeline, with show/hide and global MIN/MAX vertical height modes. MIN emphasizes compact essentials; MAX provides longer, readable stereo meters and faders. Strip widths remain stable.
2. Horizontally scroll ordinary channel strips; pin FX1, FX2 and Master at the right edge. If FX return buses do not yet exist, visibly mark FX1/FX2 as unavailable rather than creating misleading functional routing.
3. One strip per existing project track, respecting the 24-track limit; track order, name, color, type, and selection follow project state. Reuse existing track parameter state and commands, not duplicate mixer-specific values.
4. Functional track volume, pan, stereo metering (where available), mute, solo, arm, monitor. Changes in Mixer and Inspector must reflect the same state immediately. Use existing Undo/Redo conventions and audio-thread-safe parameter update mechanisms.
5. Standard logarithmic dB fader presentation with unity reference and suitable headroom, consistent with the engine's existing gain behavior. Meter readability takes precedence over excessive control density.
6. Multi-selection: left click selects; right click toggles selection, matching SimpleDAW conventions. Relative fader movement preserves dB offsets for eligible selected strips, with sensible bounds and one undoable gesture. Do not accidentally alter track-list or timeline selection semantics.
7. Mouse/pointer horizontal navigation and visible horizontal scrollbar; ensure pointer/wheel interaction does not inadvertently adjust a fader or knob. Master remains visible when scrolling.
8. Drum Master appears as an ordinary strip in normal track order. Reserve an expand/collapse affordance, but do not fabricate drum lane strips or fake audio controls in MX0. The later MX2 phase adds actual drum lane strips immediately to the right of Drum Master; those lanes do not count against the 24-track limit.
9. Preserve existing playback/recording behavior, focus hierarchy outside mixer, persistence and project reopening. Do not regress Inspector, E1, Piano Roll P0 or MD0.

## Deferred
MX1: three-band EQ in strips, effects access, actual FX1/FX2 send/return buses and routing.
MX2: expandable per-drum-lane strips with independently wired gain/pan/mute/solo/sends and Drum Master summed output. Use shared strip components where practical.

## Engineering and acceptance
- Inspect current JUCE/Tracktion architecture and existing Inspector state before coding. Report any gaps, especially stereo meter taps, bus return ownership, selection focus and gain conversion.
- Implement incrementally on `feature/mixer-mx0` only; open a draft PR. Do not modify MD0 branch or merge.
- Run Linux build and CTests and add focused mixer state/layout tests. Windows x64 Release /MD CI and Windows live acceptance require separate authorization.
- Live acceptance should verify synchronized Inspector controls, meter movement, fader relative adjustments, scroll/pinning, MIN/MAX, workspace restoration, track changes, Drum Master placeholder, playback/recording and save/reopen.
- Prefer functional features over decorative controls; document unsupported operations explicitly.

## Implementation handoff to Codex
Start with a read-only architecture audit and brief implementation plan, then implement MX0 on this branch. Raise genuine blockers, but otherwise proceed with foundation and Linux verification. Keep draft PR unmerged pending review.
