# MD0 Master Drum Track foundation

The Master is a protected native audio track that owns the project's drum MIDI clips. The arrangement pins it above ordinary tracks and groups its child drum lanes beneath it. Each lane uses a native track and sampler. The Tracktion graph reads the Master's MIDI clips for each lane, then routes lane audio by Master track ID through the Master inserts, EQ, and fader. Each lane has an independent volume, pan, EQ, and effect chain. Collapsing the kit only changes the arrangement.

New projects start with five enabled lanes. Opening a legacy project provisions a disabled kit after the existing tracks, preserving their order and routing. Settings changes the Tracktion processing property for the Master and all lanes. This removes their nodes from the graph. Disabling the kit suspends its solo settings so they cannot affect ordinary tracks. Enabling restores them.

The lane Inspector rescans the executable-relative `Drum Samples` folder on open. Bundled and user-added library samples persist by filename. Files selected with Locate WAV use the existing project-relative media reference. The shipped WAVs are original synthesized samples distributed under CC0. The Windows distribution stages the library beside the executable.

FX1 and FX2 send controls use named native Tracktion auxiliary returns when present in an edit. The accepted application baseline has no global FX1 or FX2 return provisioning. Availability therefore depends on the project's routing.

## Automated checks

The Debian 13 x64 Release build with GCC 14.2 and Xvfb passed all 15 CTests. Both MD0 tests also passed five consecutive runs. Windows live acceptance remains pending separate authorization.

- `drum_voices` covers velocity, overlapping one-shots, non-overlap retrigger, release duration, and sample end.
- `drum_master` covers legacy migration, the starter kit, routing by Master ID, Undo/Redo, channel-10 rendering beyond Note Off, project reopen, the disabled processing state, and solo restoration. Its GUI checks cover the pinned Master, protected deletion, the ordinary track cap, Settings undo, and MIDI insertion.

## Review and follow-up

Verify live audition, relinking and project portability, disabling after audition, sample changes across repeated play/stop, and return routing with configured FX1 and FX2 buses. The current Settings toggle follows the application's stopped-only track editing convention. Confirm that the shipped Windows ZIP loads the starter samples when launched from a different working directory during a separately authorized Windows acceptance run.

Inline pattern editing, the 16th-step grid, 64th subdivisions, and the shared 1–4-bar MIDI Insert selector depend on acceptance and integration of Piano Roll P0. The Drum Mixer and MD1 recording/monitoring remain separate work.
