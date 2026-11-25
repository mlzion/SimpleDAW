# Context UI CUI0 baseline audit

Audit baseline `661e10b` on `experiment/context-ui` (SimpleDAW 0.24.1). Timeline, Track List, Inspector, Piano Roll P0 and Effects Editor E1 are present in this tree. Master Drum Track, Pattern Editor, Mixer and Automation Editor have no editing workspace here.

## Menus and command bindings

| Menu or entry point | Current entries | Default bindings |
| --- | --- | --- |
| File | New, Open, Save, Save As, Import Audio, Import MIDI, Export Audio, Exit | Ctrl+S for Save. Other entries unbound. |
| Edit | Undo, Redo, Cut, Copy, Paste, Duplicate, Delete, Locate File | Delete for Delete. Other entries unbound, including Ctrl+Z/Y and Ctrl+X/C/V. |
| Insert | Audio Track, MIDI Track, MIDI Item, Add MIDI Synth, Add Reverb, Load VST3 | Unbound |
| Options | Audio and MIDI Devices, Arm Audio Input, Arm MIDI Input, Audio Monitoring Report | Unbound |
| Help | About SimpleDAW | Unbound |
| Transport buttons | Play / Stop, Record | Space and R respectively. These commands are registered outside the menu. |

Menu items use the JUCE command manager. Space and Delete act on key-down and defer to text input. Delete can clear a Timeline time selection and its loop markers when there are no selected clips. Menu Paste can report incompatible placement even where an actual paste is unavailable. The popup matches the UAT instead, where Delete requires an item selection and Paste requires valid placement. Locate File requires a missing audio source in the selected Timeline clip. Undo and Redo require stopped playback and recording plus the relevant history entry. [Baseline menus, key map and command routing](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L2484-L2616) · [History](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L2708-L2729) · [Time-selection clearing](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L4172-L4186)

## UAT availability by effective editing area

| Area | Edit | Move, Size and Modify |
| --- | --- | --- |
| Timeline | Cut and Copy accept one or more ordinary unlinked, ungrouped audio or MIDI clips. Delete accepts selected clips. Paste uses the Timeline clipboard, snapped edit cursor, destination track and valid placement. | Move uses the existing clip-movement plan. Size requires one clip and a valid size change. Split and Trim require a valid clip and range. Duplicate and the reserved position are disabled. |
| Track List | Copy and confirmed Delete require a selected track. Paste uses a separate track clipboard, selected anchor, valid output and track limit. Cut is disabled. | Up and Down require an eligible neighboring track. Left, Right, Size and Modify are disabled. |
| Piano Roll | Cut, Copy and Delete require selected notes in the editable clip. Paste uses the note clipboard, note edit cursor and valid note range. | Move and Size require a valid note change. Duplicate requires a valid destination in the clip. Split, Trim and the reserved position are disabled. |
| Inspector | All UAT edit actions are disabled. Inspector Copy Settings and Apply Settings use separate Inspector controls and clipboard. | Disabled |
| Effects Editor | All UAT edit actions are disabled. Effect parameters are edited through E1 controls. | Disabled |

The UAT buttons call `dispatchAction` directly. `canDispatch` gates editing during playback and recording and delegates note eligibility to `PianoRoll::canApply`. `dispatchAction` owns clipboard routing, confirmation, selection updates and the existing UndoManager transactions. CUI1 calls these same functions for the Edit page. [Baseline UAT controls](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L324-L336) · [Eligibility and dispatch](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L3949-L4170) · [Piano Roll edit handlers](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/PianoRoll.h#L315-L360)

## Mouse bindings

| Area | Baseline bindings |
| --- | --- |
| Timeline | Left-click a clip to select it, right-click a clip to toggle selection, drag a clip to move it, and left-drag empty space to marquee-select. Click the ruler to seek, drag the ruler or Shift-drag the Timeline for a time range. Double-click a MIDI clip to open the Piano Roll. Vertical wheel scrolls track rows and horizontal wheel scrolls time. |
| Track List | Left- or right-click a track header to select one track independently of the Timeline clip selection. Left-click empty header space clears the track selection. Shares arrangement wheel handling. |
| Piano Roll | Left-click or drag in Draw or Select mode to create, select or manipulate notes. Stationary right-click toggles note selection. Right-drag a note adjusts velocity and right-drag empty space adds notes to selection. Click keys to audition. Wheel scrolls pitch and horizontal wheel or Shift+wheel scrolls time. |
| Inspector and Effects Editor | Left/right input establishes the corresponding effective area. Existing sliders, text fields, lists and buttons retain their control-specific behavior. Effect parameter sliders disable wheel changes. |

Middle-click has no application edit binding on these workspace surfaces at baseline. Escape has no application-wide command binding. [Baseline arrangement input](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L137-L149) · [Workspace focus and pointer handling](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/MainGui.inc#L5508-L5830) · [Piano Roll gestures](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/PianoRoll.h#L784-L985) · [Effects controls](https://github.com/mlzion/SimpleDAW/blob/661e10b/src/EffectsEditor.h#L275-L295)

## CUI0 and CUI1 trial bindings

- **Options → Experimental Context UI** switches modes for the current session. Every launch starts in classic mode.
- Middle-click a workspace surface to open Edit against the existing effective editing area and selection. Existing controls embedded within a workspace retain their own input handling.
- The popup opens near the pointer inside the visible application and the pointer monitor's work area. Wheel cycles Edit and the labeled non-operational Wheel test page while the popup is open.
- Middle-click, right-click, outside click or Escape dismisses. Outside clicks inside the application are consumed. The popup also dismisses when its context changes or the application loses focus.
- Cut, Delete, Copy and Paste use the UAT rules in the table. The permanent Edit group is hidden in experimental mode. The remaining permanent groups, history, Inspector, navigation, transport and menus are available.
