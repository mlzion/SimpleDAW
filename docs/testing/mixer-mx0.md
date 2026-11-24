# MX0 implementation and live acceptance

[Approved scope](../MIXER_MX0_FOUNDATION.md)

## Mixer workspace

Use **MIXER** to show the full-width panel under the arrangement and to return to the prior workspace. The Inspector and active editor keep their targets and selections during the switch. Opening an editor explicitly returns to that workspace. Mixer visibility and MIN/MAX mode are application preferences. Project replacement clears the mixer selection and resets horizontal scroll.

Ordinary channels follow project order and keep the same width in either height mode. Use the horizontal scrollbar, mouse wheel or horizontal trackpad scroll over the channel area. FX1 and FX2 are pinned and marked unavailable. Master is pinned and controls Tracktion's native master volume and pan. Its stereo meter reads from the active playback context's output tap. Track meters share the Inspector and arrangement's existing Tracktion meter clients. Each channel displays left and right peaks independently when signal is available.

Click a strip header or background to replace the mixer selection. Right-click toggles a strip. Dragging a selected channel's fader uses the selection captured at gesture start and applies a common dB offset in one Undo transaction. The group stops at its first limit to preserve offsets. Direct numeric editing adjusts the individual strip. Faders display −60 to +6 dB with a unity tick and use the engine's volume conversion. A legacy level outside this display range stays out of group gestures until adjusted directly.

Track volume, pan, mute and solo write native Tracktion parameters or track state. Arm and Monitor are available while transport is stopped and an enabled input is assigned. Both Inspector and Mixer act on the same destinations. Monitor adds an undoable on/off override to the native input destination and requests a graph refresh. A destination without an override inherits the device's Monitor setting. Arm is an independent destination flag. Assigning physical inputs remains in the Inspector.

MX0's base branch precedes the MD0 integration. The mixer recognizes MD0's track properties, follows its leading Drum Master placement when enabled, and suppresses the internal source tracks. Drum Master uses the standard channel controls with a reserved expansion indicator. MX1 owns FX buses and routing. MX2 owns drum-lane expansion and mixing.

## Regression coverage

CTest `mixer_foundation` exercises docking, E1 and Piano Roll restoration, fixed strips, wheel behavior, MIN/MAX, mixer selection, bounded relative fader movement and gesture Undo/Redo. It also checks shared Inspector values, independent hosted-input Arm and Monitor, stereo peak separation, Drum Master metadata, input persistence and removal of deleted strips.

## Windows x64 live acceptance

- Open and close the mixer from the arrangement, Inspector, E1 and Piano Roll. Check editor target, selection, scroll and focus on return. Use timeline, transport and edit actions while the mixer is open.
- At minimum and typical window sizes, switch MIN/MAX, scroll a 24-track project using the wheel, trackpad and scrollbar, and verify FX1, FX2 and Master stay in view. Check names, colours, track insertion, deletion and reordering.
- Compare Inspector and Mixer gain, pan, mute, solo, Arm and Monitor. Verify grouped fader offsets, headroom, lower limit, unity and Undo/Redo.
- Play stereo material with asymmetric channels. Check track and Master meter movement, volume and pan, and audible mute and solo.
- Use a real audio interface and MIDI controller to check Arm, Monitor and recording. With two tracks sharing an input, verify that changing Monitor on one affects only that track. Save, reopen and confirm gain, pan, switches, input states and mixer preferences.
- After MD0 integration, check the enabled Drum Master strip, disabled state, standard controls and reserved expansion indicator with a real kit.
