# Effects Editor E1

E1 opens a fixed, slide-up panel over the full-height timeline. The persistent editor workspace and launcher buttons have been removed. The Track List, Inspector and transport stay accessible. The header names the track and effect, numbers instances of the same effect type in processing order, and displays the instance's current bypass state. The editor starts closed with every project.

## Inspector integration

For an effect-row double-click, call `MainGui::openEffectsEditor(trackID, pluginID)` with the Tracktion track and plugin `EditItemID` from the active Edit. The function returns whether a supported editor was opened. Single-click uses normal Inspector selection. Existing selection and chain operations stay with Inspector. The editor resolves the pair within the active Edit for each update and write. Reorder may change the header number. Deletion or project replacement closes the editor. Bypass continues to use Tracktion's enable state through Inspector.

The editor takes the existing Editing Windows action context when opened or clicked. The current Universal Actions have no applicable effect-parameter operation, so they are disabled while that context has focus. The existing Undo and Redo controls remain available under their usual transport rules. Undo and Redo refresh the live native parameter from the restored Tracktion state even when the editor is closed.

## Native parameters

- **Reverb** exposes Room Size, Damping, Width, Wet Level, Dry Level and Freeze. The percentage readouts represent each normalized native parameter. Wet and Dry show the engine's scaled gain in dB and retain both independent values, including existing settings with both paths active. For full dry, mute Wet and set Dry to 0 dB for unity gain. For full wet, mute Dry and set Wet to the desired gain.
- **Compressor** exposes Threshold from −40 to 0 dB, Ratio from approximately 1.05:1 through infinity, Attack from 0.3 to 200 ms, Release from 10 to 300 ms, and Output Gain from −10 to +24 dB. Ratio increases toward infinity using Tracktion's native inverse-ratio parameter. The sidechain configuration is outside the E1 editor. The pinned Compressor offers no native Wet/Dry parameter.
- Sliders show the stored parameter settings. Tracktion continues to evaluate any existing automation or modulation in its playback context. E0's Edit serialization stores the parameters, effect order and bypass state.

## Automated checks

Run `ctest --test-dir <build> -C Release --output-on-failure` with a display server on Linux. The `effects_editor` test calls the Inspector-facing entry point directly and drives the JUCE controls. It checks both native effects, bound and unbound IDs, independent instances, reorder, bypass, project round-trip, deletion, undo/redo, editor lifecycle, minimum window layout, and hosted audio playback. The `inspector_effects` test drives the Inspector list model through the real editor handoff and exercises parameter isolation, Inspector bypass and removal, and save/reopen. The `effects_foundation` test continues to check native DSP and project graph rendering.

## Integrated Windows x64 /MD acceptance

Use the integrated [v0.23.1 Windows acceptance checklist](v0.23.1-live-test.md) for the Inspector and Effects Editor. Windows CI and live testing are pending.
