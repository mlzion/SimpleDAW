# Master Specification amendment — MIDI track routing

Approved design, 22 November 2026. This amendment supersedes the MIDI routing and channel provisions in Master Specification sections 8 and 14, and governs the Track Inspector, Clip Inspector, Piano Roll, Setup and MIDI import. The dated PDFs remain the historical baseline. The routing amendment is pending implementation.

## Track routing

- Each MIDI track has a selectable MIDI input device and an input channel of **Omni** or **1–16**. Omni accepts all channel messages from the selected device. Setup handles device availability and enablement. The track owns source selection and channel filtering. Keep existing Arm and Monitor controls and their recording and monitoring semantics.
- Each MIDI track has an output channel of **1–16**, default **Channel 1**. Clip playback, monitored channel messages and Piano Roll audition use the track output channel through the existing internal instrument or VSTi path. Keep the instrument's downstream audio routing.
- The Clip Inspector and Piano Roll expose no MIDI channel controls. A clip follows the output channel of its containing track, including after moving, pasting or cloning to another track. A clone's shared source does not determine its destination channel. Channel remapping at output leaves stored musical data intact.
- External hardware MIDI output and MIDI THRU are deferred. Monitoring to an internal instrument continues through the existing Monitor path.

## MIDI file import and project compatibility

- Keep **File → Import MIDI** at the timeline cursor and **Insert → MIDI Clip** for blank clips. Single-channel material imported to an existing selected MIDI track plays through that track's configured output channel. Retain original channel information as source data for compatibility, without exposing it as a clip routing setting.
- Preserve multichannel material by separating each source MIDI track into parts by original channel, including a file with several channels in a single MIDI track. Each part goes to its own MIDI track configured to the source channel, including Channel 10 when present. Preserve relative timing and ordering, pitches, durations, velocities, controller and program changes, pressure and pitch bend. Preserve file-wide timing and other non-channel data once at the appropriate source or project scope. Preserve events the current engine cannot render as source data. Channel separation preserves the parts and messages, while audible reproduction depends on the loaded instruments.
- Multichannel track creation supersedes the earlier blanket prohibition on automatic tracks for this import case. The import is one undoable operation. If the track limit, timeline limits or parser prevent a lossless import, explain the reason before changing the edit. Until the multichannel path exists, reject unsupported input clearly rather than flatten channels or silently discard events.
- Open existing projects with their clips, instruments, event data, timing, trim, loop and link state intact. Honor saved track routing and existing input assignments where present. For legacy tracks with one effective playback channel, initialize the track output to that channel. Use Channel 1 only when there is no prior channel to preserve. Where a legacy track used more than one effective playback channel, preserve the distinct parts on separate tracks with matching output channels and retain their instrument and routing context. Preserve original channel data and relationships during conversion. If conversion cannot preserve the project within supported limits, report the conflict and keep the original project available without a partial migration or silent overwrite. Once saved in the new format, reopen using the persisted track routing without repeating migration.

## Subsystem contracts

- **Track Inspector** owns input device, input channel and output channel. The existing audio destination selector is separate from MIDI channel selection.
- **Clip Inspector** retains MIDI velocity and transpose. **Piano Roll** retains note editing and auditions through the containing track. Keep note-off on the same channel as its corresponding audition note-on, including when the selection or routing changes.
- **Setup** lists OS-visible MIDI inputs and their enabled state. Refresh the existing device inventory without restarting SimpleDAW. A missing selected device retains its saved identity and should appear unavailable until it returns.
