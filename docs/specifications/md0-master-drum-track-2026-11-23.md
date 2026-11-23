# SimpleDAW — Master Drum Track MD0 specification
Date: 2026-11-23
Status: ACCEPTED DESIGN; NOT IMPLEMENTED; CODING AND WINDOWS CI NOT AUTHORIZED
Scope: MD0 foundation. This supplement does not silently override existing UAT, Inspector, routing, or MIDI channel specifications.

## Structure and lifecycle — ACCEPTED
- One permanent, protected Master Drum Track (MDT) per project, pinned first in the Track List when enabled. It is a distinct internal track type, not an ordinary audio/MIDI track. It and its child lanes do not count toward the 24 ordinary-track limit.
- New projects automatically provision MDT, enabled, with five starter lanes: Kick, Snare, Hi-hat, Tom, Crash. Legacy projects lacking MDT provision one in a disabled state without altering existing tracks.
- **Settings > Enable Drum Master** is the *only* enable/disable UI. Checked = visible/active; unchecked = MDT and child lanes hidden, MIDI dispatch, sample voices, effects and bus processing bypassed, active voices stopped, state retained. Save per project. Aim for effectively zero ongoing drum processing CPU cost while disabled; verify the actual audio path is skipped, not merely muted.
- Enabled MDT is collapsible: hide/show children without disabling playback. Master follows ordinary MIN/MAX; children use fixed MIN height.
- MDT cannot be deleted through UAT or track operations. Only one MDT is allowed.

## MIDI data and lanes — ACCEPTED
- Up to 16 child drum lanes, initially five. Each is a filtered editable view of the MIDI notes in the selected MDT clip, **not** an independent MIDI clip/source. Master owns MIDI clips. Drum Master playback uses fixed MIDI channel 10 and GM percussion note numbers.
- Illustrative starter assignments: kick 36, snare 38, closed hi-hat 42, tom 45, crash 49; exact tom variant can follow starter kit. Lane GM note assignment editable in Inspector.
- Moving or pasting ordinary MIDI clips to MDT via existing UAT actions converts all note events to channel 10, retaining note pitch, timing, duration and velocity; preserve other source events where supported by existing MIDI model. Unsupported note numbers remain in the clip and silent until a matching lane exists. Undoable, with no destructive loss of source data.
- Add Lane creates a blank lane; Delete Lane removes its sampler/settings but **preserves MIDI notes**, which remain silent if unmatched. Reorder lanes without changing note mapping. Undo/Redo.
- Duplicate Lane creates a lane below source with independent sample and processing settings, unique name, and initially the **same GM note** (layering by default). User may reassign GM note to separate triggers. Multiple lanes sharing a note trigger together and show/edit the same underlying MIDI hits.
- Each lane has one active sample, editable name/color, GM note, gain/pan/mute/solo, EQ/effects, overlap ON/OFF. Child Inspector reuses compatible MIDI Track Inspector controls, omitting Arm/Monitor; selecting a lane Inspector must not change selected pattern.
- Master Inspector acts as audio bus with gain/pan/mute/solo, EQ/effects, output to Main Master and normal FX1/FX2 sends; no instrument VST/sample selector. MDT recording/monitoring controls are deferred to MD1.

## Sample library — ACCEPTED
- Windows ZIP includes a `Drum Samples/` directory alongside `SimpleDAW.exe`; resolve relative to executable, not process working directory. Bundle multiple licensed/distributable WAV choices for common drums, including five defaults.
- Users may copy WAVs into that folder. Scan at startup and rescan whenever sample selector opens; no restart required. Case-insensitive filename prefixes categorize: `Kick_`, `Snare_`, `HH_`, `LTom_`, `MTom_`, `HTom_`, `Crash_`, `Ride_`, `Clap_`, `Perc_`; unknown filenames appear under Other. Alphabetic ordering.
- Each lane selects/auditions one active WAV. Sample changes do not modify MIDI. Persist references using existing media conventions; missing files show warning/relink, never silently replace. Verify project portability and package asset licensing.
- Sample library categorization is independent of lane GM note mapping.

## Clip creation and inline editing — ACCEPTED
- UAT Insert on selected MDT creates a new MIDI pattern at Timeline cursor, length selectable 1, 2, 3, or 4 bars. **Same configurable Insert MIDI Clip length applies to all ordinary MIDI tracks**; remember last choice during project session. Existing snap behavior. Imported and recorded clips are not limited to four bars.
- Existing UAT duplicate = independent source; clone = linked MIDI source; unclone = independent; loop = repeated source within clip. Preserve Undo/Redo and linked edit semantics.
- Selecting an MDT clip automatically activates inline child-lane pattern editing. Without a selected MDT clip, lanes show hits read-only. Selecting a different pattern switches target; child Inspector selection does not replace clip target.
- In 4/4, auto-zoom to **16 sixteenth-note steps for a one-bar pattern**, or **32 sixteenth-note steps (two bars) maximum visible at once** for longer patterns. Horizontal scroll for 3–4 bars; maintain alignment with ruler. Interpret step count according to time signature, not as 32 quarter-note beats.
- Left-click empty 16th cell inserts; left-click occupied cell removes; right-click toggles selection; drag-box multi-select; mouse wheel over selected hits adjusts their shared/individual 16th-hit velocity as applicable.
- Double-click a hit opens four-cell 64th subdivision popup above it, initially ON/OFF/OFF/OFF for an unsubdivided hit. Four tiny indicator dots show subdivisions in the collapsed 16th cell; dots are not click targets. Each active subdivision is a standard channel-10 MIDI note; all four inherit the parent 16th velocity, not independent 64th velocities.
- Respect UAT focus, no required keyboard modifiers/context menus; all edits undoable and preserve linked-source behavior.

## Sampler and routing — ACCEPTED
- One-shot sample playback: MIDI Note On triggers; Note Off does not terminate; velocity scales level; sample ends naturally.
- Per-lane Overlap ON permits multiple voices; OFF stops previous voice on retrigger. Apply approximately **3 ms internal anti-click fade** when a voice is interrupted, including stop/disable as applicable. No user fade control.
- Stop transport stops drum voices; disabling MDT stops voices and bypasses the whole drum audio/MIDI path. Lane mute/solo follows normal semantics.
- Each lane independently processes audio then sums into Drum Master bus; Master processes kit and routes to Main Master with standard FX1/FX2 sends. Keep architecture compatible with future Drum Mixer and MD1 live MIDI monitoring.

## Deferred and pending
- DEFERRED: dedicated Drum Mixer until standard Mixer is established; then reuse channel strips, metering, controls and routing.
- DEFERRED MD1: MIDI recording, Arm/Monitor and live input triggering/monitoring through Master (channel 10); integrate with normal count-in/punch/overdub workflows. MD0 engine should accommodate this later.
- PENDING technical verification: precise Tracktion/JUCE data model, MIDI event preservation on conversion, sample asset licensing, reliable zero-processing bypass, time-signature edge cases, sample relink/portability, legacy project migration. Report incompatibilities; do not silently change approved UX.
