# CUI2 movement audit

Audit input `208cff7`. The popup uses the existing stopped-transport gate and UAT dispatcher.

| Effective editing context | Existing UAT movement | CUI2 snap |
| --- | --- | --- |
| Timeline clips | Left/Right move the selected group by a common time delta. Up/Down use the existing placement plan, skipping drum lanes and validating destination type, capacity, bounds and frozen tracks. | Timeline snap affects horizontal movement. |
| Track List | Up/Down reorder eligible siblings only. The protected Master cannot move. Ordinary tracks cannot cross the kit, and lanes can reorder only within their kit. Left/Right are unavailable. | Unavailable |
| Piano Roll | Left/Right shift selected notes by Piano Roll snap (1/96 beat with snap off). Up/Down transpose by one semitone. Existing note and clip limits apply. | Independent Piano Roll choices |
| Drum Pattern Editor | Left/Right shift hits one sixteenth. Up/Down move between eligible kit lanes. Occupied destinations and ambiguous lane mappings are rejected. Subdivisions, spacing and velocity are preserved. | Fixed at 1/16 |
| Inspector and Effects Editor | Movement unavailable. | Unavailable |

[Existing clip placement and movement](https://github.com/mlzion/SimpleDAW/blob/208cff7/src/MainGui.inc#L3793-L3970) · [Track sibling rules](https://github.com/mlzion/SimpleDAW/blob/208cff7/src/MainGui.inc#L4055-L4067) · [Piano Roll](https://github.com/mlzion/SimpleDAW/blob/208cff7/src/PianoRoll.h#L433-L452) · [Drum movement](https://github.com/mlzion/SimpleDAW/blob/208cff7/src/DrumGrid.inc#L615-L671)

The Master already appears first in the visible row list, followed by its lanes. Vertical scrolling treats it as a normal row, so it can leave the viewport. CUI2 retains that code and adds regression coverage. [Visible track order](https://github.com/mlzion/SimpleDAW/blob/208cff7/src/MainGui.inc#L3047-L3067) · [Vertical scrolling](https://github.com/mlzion/SimpleDAW/blob/208cff7/src/MainGui.inc#L5385-L5409)

## Revised W03 and F06 review

The former ruler target took precedence over even a valid, retained Timeline clip selection. The revised ruler handles act directly and leave the existing clip selection available to UAT and the popup. The edit cursor remains the paste and split reference, and the committed window remains available to looping and time-window-dependent operations. Existing keyboard/menu Delete fallback with no clip selection remains available. Legacy mixed tracks retain the Mixed identifier.

| Workspace | Edit dispatch checked alongside Move |
| --- | --- |
| Timeline | Cut, Copy, Paste and Delete use the clip selection, existing clipboard, compatible destination and existing undo behavior. The ruler no longer intercepts these commands or directional clip moves. |
| Track List | Copy/Paste duplicate eligible tracks. Delete stays subject to track protection. Cut remains unavailable. Up/Down use the existing eligible-sibling rules. |
| Piano Roll | Edit commands use the selected notes and note clipboard. Movement stays on selected notes with the independent grid. |
| Drum Pattern Editor | Edit commands use the selected cells and drum clipboard. Move preserves existing lane validation and fixed horizontal snap. The Master and its lanes retain their Track List protections. |
| Inspector and Effects Editor | UAT Edit and Move are unavailable with editor focus. Inspector actions, including clip Clone, are independent. |

## v0.26.3 interaction audit

Audit input `cc2c558`. The accepted v0.26.2 bindings exposed the following differences before W06.

| Workspace | Existing behavior | W06 implementation |
| --- | --- | --- |
| Timeline and Track List | One arrangement component owns the shared vertical viewport. Timeline right press toggled clip selection immediately. Shift in the clip area created a range, and clicking empty clip space could seek. Clip drag used the existing placement engine. | Right release toggles the latched object after a click. A right-wheel gesture started over empty Timeline space pans horizontally. Ruler handling is isolated from clip input and preserves effective focus. Direct clip drags use a common relative snap displacement and reuse placement or source-resize validation. |
| Inspector | A vertical JUCE viewport hosts the controls. The volume slider could consume wheel input. | Disable slider wheel value changes and let the wheel reach the viewport. Context UI retains wheel ownership while open. |
| Piano Roll | Draw/Select, body movement, single-note trailing-edge resize and right drag already existed. Right drag could affect an unselected note and velocity bars changed one note. The note editor permits pitch/time overlaps. | Keep overlap behavior and the existing note transaction. Selected edges resize the group. Selected-note and velocity-bar drags apply one bounded velocity delta. Right-wheel pan starts in empty space. |
| Drum Pattern Editor | Left click toggled cells, left drag boxed, and the wheel changed selected velocities. UAT movement rejected occupied destinations. The velocity panel showed one lane at a time. | Draw/Select and direct drag share the editor selection. Direct drag atomically replaces occupied unselected cells while retaining subdivision data. UAT movement retains its existing validation. The bottom panel displays active hit velocities and paints selected bars last. |
| Command groups | Permanent Size buttons called the single-item sizing operations. There was no active popup Size page. | Remove Size buttons and retain the underlying sizing operations. Experimental mode continues to open on Move and cycle with Edit. |

Direct clip resizing reuses existing source, loop, frozen-track, and linked-family restrictions. A group gesture with incompatible sizing plans for the same linked family makes no edit. Live content edits remain subject to the existing playback and recording gates.
