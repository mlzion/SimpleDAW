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

CUI2 adds explicit cursor and time-window targets using separate ruler handles. An explicitly selected ruler target takes precedence over retained clip selection when Timeline has focus. A normal arrangement click returns to clip commands. The cursor allows horizontal Move. The window allows horizontal Move and Delete. Movement respects timeline bounds, and window Move preserves duration and updates the existing loop markers. Boundary sizing is deferred. Legacy mixed tracks retain a Mixed identifier because they have neither an Audio nor MIDI-only type.
