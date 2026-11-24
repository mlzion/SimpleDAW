# SimpleDAW v0.25.1 — Master Drum Track MD0 Integration

Draft PR #27 holds the v0.25.1 integration artifact and its CI results. The package is `SimpleDAW-v0.25.1-Windows-x64.zip`. The accepted Piano Roll P0 release remains v0.24.1. [MD0 integration draft](https://github.com/mlzion/SimpleDAW/pull/27)

The Master is a protected native audio track that owns the project's drum MIDI clips. The arrangement pins it above ordinary tracks and groups its child drum lanes beneath it. Each lane uses a native track and sampler. The Tracktion graph reads the Master's MIDI clips for each lane, then routes lane audio by Master track ID through the Master inserts, EQ, and fader. Each lane has an independent volume, pan, EQ, and effect chain. Collapsing the kit only changes the arrangement.

New projects start with five enabled lanes. Opening a legacy project provisions a disabled kit after the existing tracks, preserving their order and routing. Settings changes the Tracktion processing property for the Master and all lanes. This removes their nodes from the graph. Disabling the kit suspends its solo settings so they cannot affect ordinary tracks. Enabling restores them.

The lane Inspector rescans the executable-relative `Drum Samples` folder on open. Bundled and user-added library samples persist by filename. Files selected with Locate WAV use the existing project-relative media reference. The shipped WAVs are original synthesized samples distributed under CC0. The Windows distribution stages the library beside the executable.

FX1 and FX2 send controls use named native Tracktion auxiliary returns when present in an edit. The accepted application baseline has no global FX1 or FX2 return provisioning. Availability therefore depends on the project's routing.

## Inline pattern editor

Select a Master Drum Track clip to activate the sixteenth-note grid in the visible child lanes. The lanes show the same source notes when they share a GM mapping. Selecting a lane header keeps the current pattern selected. Other patterns remain visible as read-only hits.

The arrangement fits a one-bar 4/4 pattern to 16 steps and caps the visible span at 32 steps. Use the existing horizontal navigation buttons or horizontal scrolling to reach later steps. The grid and ruler share the arrangement time transform. Musical-grid positions use the tempo map's quarter-note coordinates, including in meters such as 7/8, where one bar has 14 sixteenth notes.

Left-click adds or removes a step, right-click toggles selection, and dragging a selection box selects occupied steps. Wheel over a selected step to adjust the selected group, or use UAT Delete to remove it. Double-click an occupied step to open its four 64th-note cells. All active subdivisions inherit the step velocity. The grid uses the Piano Roll MIDI mutation model for linked clips and Undo/Redo, and loops keep a shared source.

Choose **Insert > MIDI Clip** and a length of 1–4 bars on either the Master or an ordinary MIDI track. The menu checks the most recently used length in the current project session. Insertion uses the existing snap setting and the musical lengths of the selected bars. Import and recording retain their existing length behavior.

## Automated checks

The integrated Debian 13 x64 Release build with GCC 14.2 and Xvfb passed all 17 CTests. Piano Roll and the three MD0 suites passed three consecutive runs each. The initial foundation build passed all 15 CTests, and both foundation tests passed five consecutive runs.

- `piano_roll` re-runs the accepted Piano Roll P0 regression after integration.
- `drum_grid` covers 16- and 32-step views, four-bar scrolling, 7/8, click and box selection, the subdivision popup, velocity, linked Undo/Redo, and shared Insert length.
- `drum_voices` covers velocity, overlapping one-shots, non-overlap retrigger, release duration, and sample end.
- `drum_master` covers legacy migration, the starter kit, routing by Master ID, Undo/Redo, channel-10 rendering beyond Note Off, project reopen, the disabled processing state, and solo restoration. Its GUI checks cover the pinned Master, protected deletion, the ordinary track cap, Settings undo, and MIDI insertion.

## Review and follow-up

Verify live audition, relinking and project portability, disabling after audition, sample changes across repeated play/stop, and return routing with configured FX1 and FX2 buses. The current Settings toggle follows the application's stopped-only track editing convention. Confirm that the shipped Windows ZIP loads the starter samples when launched from a different working directory during a separately authorized Windows acceptance run.

Piano Roll P0 is integrated from PR #24. Include inline pointer gestures, focus handoff between Piano Roll and the drum grid, loop repeats, and the one-to-four-bar selector in live acceptance. The Drum Mixer and MD1 recording/monitoring remain separate work.
