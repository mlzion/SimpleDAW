# Effects foundation E0

E0 uses the pinned Tracktion Reverb and Compressor as real track inserts. The Edit owns instances, state, undo and graph construction through its existing PluginCache, PluginList and project serializer. The Compressor is a test fixture for chain behavior. The application adds a CTest entry point that runs without opening a SimpleDAW window or initializing an audio device.

## Reproduce the checks

Build SimpleDAW normally with `BUILD_TESTING=ON`, then run `ctest --test-dir <build> -C Release -R '^effects_foundation$' --output-on-failure`. On Linux, provide a graphical session or virtual X server for JUCE's application and message loop. The suite generates its own signals and project in a temporary directory.

- Native Reverb processing uses mono and stereo buffers at 44.1 and 48 kHz with 128- and 512-sample blocks. It checks defaults and bounds, unity dry after gain smoothing, wet output, the response to Room Size and Damping changes, finite samples and fresh silence.
- Tracktion stereo graph renders use a generated WAV at 44.1 kHz with 512-sample blocks. The suite checks insert identity, independent settings, bypass isolation and dry equivalence, order-dependent audio, removal, and Edit save/reopen with stable IDs, parameter values, order, bypass and repeatable audio after startup gain smoothing. Track renders set the target track bit explicitly because the pinned `toBitSet` helper sets every track bit.

## Proposed v0.23 and E1 integration contract

| Operation | Proposed contract |
| --- | --- |
| List and select | Read user inserts in Tracktion processing order. Address instances by active Edit, track `EditItemID` and plugin `EditItemID`. Resolve IDs again after undo, structural changes or a project switch. v0.23 supplies the active track. E1 owns the effect selection and editor binding. |
| Add and remove | On the message thread, create a fresh native instance through the Edit's PluginCache and insert it into the track's PluginList after instrument output. Preserve the fader and routing relationship. Remove through `Plugin::deleteFromParent` and clear the selection if its ID disappears. Group each command under Edit undo. |
| Count and order | Reserve eight user audio-effect slots per track. The instrument, LFOs, meter, fader and routing nodes are outside the count. Validate capacity before add or paste. Translate visible positions to the corresponding plugin children and preserve instance IDs and automation on reorder. Keep existing projects intact while legacy overflow and hosted insert policy are reviewed. |
| Read and write values | Resolve by plugin type and stable native parameter ID. Descriptors should define label, unit, range, default, increment and formatting. Bind writes on the message thread to the native setter or automatable parameter with notification. Keep stored base and effective playback values distinct when automation or modulation is introduced. |
| Bypass | Use `Plugin::isEnabled` and `Plugin::setEnabled` for individual bypass with native undo and persistence. A future track-wide bypass needs separate intent so it can restore each effect's individual setting. |
| Presets | Apply a versioned snapshot keyed by effect type and parameter ID to the same instance in one undo transaction. Save actual parameter values in the Edit. If approved, persist preset ID/version separately so edited presets can retain their provenance. |
| Persistence | Save and reopen using the current Tracktion Edit format. Tracktion owns instance IDs, list order, parameters and per-effect enable state. Editor selection persistence is a separate decision. |

### Reverb parameter details

Native IDs are `room size`, `damping`, `wet level`, `dry level`, `width` and `mode`. Each accepts 0–1. Defaults are 0.3, 0.5, 1/3, 0.5, 1 and 0 respectively. Room Size controls feedback, with a resulting change in decay. JUCE scales wet by three and dry by two. Wet and dry are independent in native state.

For a future new-instance wet percentage with Width 1 and Freeze off, the research proposes mapping normalized position `m` to native wet `m / 3` and dry `(1 - m) / 2`. The suite exercises the native endpoints. Existing instances can store independent gains, width and freeze, so the editor needs an approved policy before offering a single mix control for those instances.

## Later approvals and verification

- Approve Inspector and Effects Editor wiring, exact display units and increments, single-slider mix and legacy handling before exposing new controls.
- Approve the preset catalog, names after listening, and provenance and user-preset storage before shipping presets.
- Approve follow-on chain management, including track-wide bypass, hosted insert counting, legacy overflow and clipboard details. Decide whether to persist editor selection.
- Approve any improved Reverb, independent Decay, dedicated Plate or additional effect development. Native Reverb has no independent Decay or dedicated Plate.
- Windows x64 workflow dispatch requires separate approval under [AGENTS.md](../../AGENTS.md). Windows live acceptance still needs device playback, parameter changes during playback, bypass and quiet-tail behavior, CPU headroom and instrument-output coverage. The native Reverb's silence shortcut can skip below-threshold blocks, so continuous-tone checks cannot establish full impulse-tail behavior.
