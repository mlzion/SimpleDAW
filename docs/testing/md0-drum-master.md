# SimpleDAW v0.25.1 — Master Drum Track MD0 Integration

Draft PR #27 contains the v0.25.1 MD0 integration. The owner reported that the prior corrective Windows retest passed except for the inconclusive D44 listening check. The final correction pass below awaits a separately authorized Windows build and live retest. The accepted Piano Roll P0 release remains v0.24.1. [MD0 integration draft](https://github.com/mlzion/SimpleDAW/pull/27)

The Master is a protected native audio track that owns the project's drum MIDI clips. The arrangement pins it above ordinary tracks and groups its child drum lanes beneath it. Each lane uses a native track and sampler. The Tracktion graph reads the Master's MIDI clips for each lane, then routes lane audio by Master track ID through the Master inserts, EQ, and fader. Each lane has an independent volume, pan, EQ, and effect chain. Collapsing the kit only changes the arrangement.

New projects start with five enabled lanes. Opening a legacy project provisions a disabled kit after the existing tracks, preserving their order and routing. Settings changes the Tracktion processing property for the Master and all lanes. This removes their nodes from the graph. Disabling the kit suspends its solo settings so they cannot affect ordinary tracks. Enabling restores them.

The lane Inspector rescans the executable-relative `Drum Samples` folder on open. Bundled and user-added library samples persist by filename. Files selected with Locate WAV use the existing project-relative media reference. The shipped WAVs are original synthesized samples distributed under CC0. The Windows distribution stages the library beside the executable.

FX1 and FX2 send controls use named native Tracktion auxiliary returns when present in an edit. The accepted application baseline has no global FX1 or FX2 return provisioning. Availability therefore depends on the project's routing.

## Drum Pattern Editor

Single-click selects a Master Drum Track clip in the arrangement. Double-click opens its Drum Pattern Editor in the slide-up workspace shared with Piano Roll and the Effects Editor. The editor binds to the clip by ID, so visiting a lane Inspector preserves the editor target. X returns focus to the arrangement, restores its previous zoom and scroll position, and keeps the drum clip selected.

The editor fits a one-bar 4/4 pattern to 16 steps and caps the visible span at 32 steps. Use the horizontal navigation buttons or horizontal scrolling to reach later steps. The editor has its own grid and ruler time transform. Musical-grid positions use the tempo map's quarter-note coordinates, including in meters such as 7/8, where one bar has 14 sixteenth notes.

Left-click adds or removes a step, right-click toggles selection, and dragging a selection box selects occupied steps. While stopped, use the vertical wheel anywhere in the focused editor after selecting hits, or use UAT Delete to remove them. Replay the pattern to hear the edited velocity. The editor displays the selected velocity or range beside the pointer. A wheel event makes an undoable 4–16 point change, subject to the MIDI range of 1–127. Group adjustments preserve each parent step’s velocity relationship and apply its resulting value to every active subdivision. Double-click an occupied step to open its four 64th-note cells. All active subdivisions inherit the step velocity. The grid uses the Piano Roll MIDI mutation model for linked clips and Undo/Redo, and loops keep a shared source.

Choose **Insert > MIDI Clip** and a length of 1–4 bars on either the Master or an ordinary MIDI track. The menu checks the most recently used length in the current project session. Insertion uses the existing snap setting and the musical lengths of the selected bars. Import and recording retain their existing length behavior.

## Automated checks

The integrated Debian 13 x64 Release build with GCC 14.2 and Xvfb passed all 17 CTests. Piano Roll and the three MD0 suites passed three consecutive runs each. The initial foundation build passed all 15 CTests, and both foundation tests passed five consecutive runs.

- `piano_roll` re-runs the accepted Piano Roll P0 regression after integration.
- `drum_grid` covers 16- and 32-step views, four-bar scrolling, 7/8, click and box selection, the subdivision popup, velocity, linked Undo/Redo, and shared Insert length.
- `drum_voices` covers velocity, overlapping one-shots, non-overlap retrigger, release duration, and sample end.
- `drum_master` covers legacy migration, the starter kit, routing by Master ID, Undo/Redo, channel-10 rendering beyond Note Off, project reopen, the disabled processing state, and solo restoration. Its GUI checks cover the pinned Master, protected deletion, the ordinary track cap, Settings undo, and MIDI insertion.

## Windows acceptance corrections

The correction build passed all 17 CTests on Debian 13 x64 Release with GCC 14.2 and Xvfb. Windows 11 retest remains pending.

D09 and D44 use a deterministic 500 ms constant WAV at 48 kHz and two MIDI triggers 10 ms apart. The sampler measured `0.031496` at velocity 32 and `0.125000` at velocity 127. Overlap ON sustained `0.250000` after the second trigger. Overlap OFF measured `0.250000`, `0.187500`, and `0.125000` at 0, 1.5, and 3 ms after retrigger, respectively. The previous voice reached zero after 144 samples. The overlap implementation needed no change.

The drum-grid regression uses wheel input with a small horizontal component and checks visible numeric feedback, selected and unselected hits, parent velocity across active subdivisions, and linked Undo/Redo. It also covers one-, three-, and four-bar viewport behavior, the X focus handoff, independent duplicates, and save/reopen.


- D33 uses the existing lane-copy implementation through UAT Duplicate. Lane copy keeps the GM note and makes a separate copy of the lane state. The GM note dialog uses plain ASCII for the input range.
- D41 uses the missing audio clip’s gray diagonal hatch in the lane name area only. The indicator checks the existing sample reference during painting, so a restored WAV or manual relink clears it.
- D57 and D58 use the underlying ordinary-track order for numbering, excluding the Master and child lanes. The Track List draws child headers 18 pixels to the right with their original row height, right-aligned controls, and hit target.
- D48 regressions exercise established timeline operations on drum clips, including conversion from ordinary MIDI channel 3 to channel 10 with pitch, onset, duration, and velocity preserved on Move and Paste. Move Undo restores the original channel. Cross-track moves resolve the clip again by ID before applying destination edits because Tracktion can replace the clip object as its parent changes.

## Targeted Windows 11 retest

1. **D09 and D44.** Select one hit, then a group that includes a subdivided hit. Adjust velocity with the wheel, check the numeric value or range, and replay the pattern before and after Undo/Redo. Trigger a long sample twice 10 ms apart and compare Overlap ON and OFF.
2. **D28 and D48.** Set a recognizable timeline zoom and scroll position. Single-click and double-click a drum clip, check 16 and 32 visible steps, and scroll three- and four-bar patterns. Close with X and confirm view, focus, and selection. Move, duplicate, clone/unclone, loop, trim and resize. Edit linked and independent patterns, then Undo/Redo and save/reopen. Confirm ordinary MIDI clips still open Piano Roll.
3. **D33 and D34.** Duplicate with the UAT and compare the recorded GM note, the independent sample and effects settings, and behavior at 16 lanes. Undo and redo. Open and confirm the readable GM note dialog.
4. **D41, D57, and D58.** Remove one referenced sample, confirm the hatch affects only that lane's name, then restore or relink it. Expand, collapse, disable and re-enable the Master. Confirm ordinary numbering stays 1–24, and the child headers remain indented and selectable at MIN height.

## D59 and D61 audio-device corrections

**D59.** The Windows 11 retest confirmed that both endpoints were set to None and that choosing the devices restored playback. Tracktion can allocate a context and set its playing flag before a device supplies audio callbacks. In a local probe, selected output with callbacks withheld also left the flag true at zero seconds. SimpleDAW now checks that the selected output has an open, running device, configured and active channels, and valid stream parameters before changing the transport. Playback also stops if that output disappears mid-run. The status bar directs the user to Audio and MIDI Devices, and stopped transport controls remain available.

**D61.** The JUCE selector previously rebuilt its controls from inside their setting-change handlers. Updating the rate and buffer lists used a notifying clear, which could queue another change during each refresh. The device dialog also ran a nested modal loop from its menu command. The selector now coalesces refreshes on the message thread after handlers return and uses silent control updates. The same modal dialog opens without a nested loop and uses its native title bar and close button. Device access still uses JUCE and Tracktion's existing setup paths.

Tracktion writes JUCE's chosen configuration to `audio_device_setup` and reads it during startup, with its existing fallback for unavailable named endpoints. An explicitly saved None selection can persist. The corrections keep that preference and explain why Play is unavailable instead of silently selecting a device.

The device regression drives repeated choices through the message queue with a live Edit context, verifies rate and buffer fallback without a second notification, closes the dialog through its close handler, and reinitializes from Tracktion's saved XML. With the old selector compiled into an otherwise identical test binary, the device regression timed out after 10 seconds. Playback tests exercise output loss, the stopped interlocks and warning, and non-silent ordinary WAV playback after reopening a hosted output. The routing regression exercises recording and monitoring through hosted callbacks.

Linux x64 Release passed all 17 CTests under Xvfb. The device setup, GUI corrections, and Piano Roll suites also passed three consecutive runs each. Windows native device and dialog behavior awaits live verification.

### Windows 11 retest for D59 and D61

- Set the output to None, close Settings with X, and press Play. Verify the warning, a stopped transport, and usable UAT and Audio and MIDI Devices controls.
- Reopen Settings and change the input, output, sample rate, buffer size, and enabled channels several times. Check that the displayed choice follows the device and that X closes the dialog each time.
- Quit and reopen SimpleDAW. Verify that the selected endpoints and stream settings survive startup. Play an ordinary WAV, then verify recording and input monitoring with the selected interface.

## Final correction pass

- **D44** exercises the lane Inspector Overlap action and renders the same channel-10 pattern through the lane sampler, Drum Master bus, and Tracktion WAV renderer. Two notes trigger 10 ms apart on a constant 250 ms sample at 48 kHz. The test compares amplitudes at retrigger and throughout both the interrupted and natural releases. The sampler implementation is unchanged.
- **D60** keeps the slide-up editor and its 32-step viewport. The ruler distinguishes bars, beats, and sixteenth notes. Scrollbars appear when the clip or kit exceeds the viewport. The bottom velocity panel follows the chosen lane and previews its value beside the pointer. A drag commits one undoable edit across the hit’s subdivisions. The arrangement clip pulses while its editor is visible, including while inspecting a lane.
- **D62** scopes lane solos to the kit. Master solo isolates the kit from ordinary tracks. With both enabled, only soloed lanes play. Lane and master mute still take precedence. The regression renders separate kick, snare, and ordinary WAV passages to check the mix, including disabled-kit isolation.
- **D63** puts Duplicate, Clone, Unclone, Split, Trim, and Loop in the timeline UAT. Loop count remains a clip parameter in the Inspector. The Track List UAT duplicates ordinary tracks or drum lanes. Both note editors offer Cut, Copy, Paste, and Delete through the UAT and keep their mouse gestures. In the Drum Pattern Editor, click the ruler to set the paste position while retaining selection. Paste retains the copied pitches, timing, velocities, and subdivisions.

D44 measured a single-voice output of `0.088493`. Overlap ON held at `0.176986` through the retrigger and until the original voice’s natural release. Overlap OFF measured `0.176986`, `0.132740`, and `0.088493` at 0, 1.5, and 3 ms after retrigger. At 48 kHz that is 144 samples. Both renders retained the final voice’s natural fade and reached silence at its end.

Linux x64 Release passed all 17 CTests under Xvfb. The drum grid, drum master, drum voices, Inspector clone, Inspector loop, Piano Roll, and UAT suites each passed three consecutive runs. Windows live validation of the final correction pass remains pending.

## D63 design refinement

The owner accepted D60, D62, and the three D63 live retests on the preceding Windows build. D44 was accepted from the measured integrated render. This follow-up implements the control changes approved during that retest.

- The clip Inspector provides Loop ON/OFF beside the repeat count. Looping remains an explicit toggle and uses the existing repeat, resizing and Undo/Redo behavior.
- Timeline has MOVE, EDIT, SIZE and MODIFY. MODIFY contains Duplicate, Clone/Unclone, Split and Trim. The single Clone/Unclone button reflects the selected clip's link state. Track List Duplicate remains available for ordinary tracks and drum lanes.
- Piano Roll has MOVE, EDIT and SIZE. MOVE slides notes by the current snap increment or transposes pitch by a semitone. SIZE adjusts the start or end of a single selected note. Established mouse editing remains available.
- The Drum Pattern Editor has MOVE and EDIT. Horizontal moves shift one sixteenth note. Vertical moves follow kit row order. Each move preserves step spacing, velocity and subdivision timing and is a single undoable edit. Commands that would leave the pattern or kit, merge occupied hits, or move from an ambiguous layered source are disabled.

Linux x64 Release passed all 17 CTests under Xvfb. Universal Actions, Inspector Loop, Inspector Clone, Drum Grid, Drum Master and Piano Roll each passed three consecutive runs. The refinement awaits Windows live verification.
