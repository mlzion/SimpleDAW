# Preserve device defaults for legacy destinations and give explicit mixer
# monitoring its own undoable state on the native input destination.
set(input_device playback/devices/tracktion_InputDevice.cpp)
simpledaw_patch_midi("${input_device}" [=[        switch (owner.getMonitorMode())]=] [=[
        const juce::Identifier monitorOverride ("simpleDawMonitorOverride");
        if (dest->state.hasProperty (monitorOverride))
            return bool (dest->state[monitorOverride]);

        switch (owner.getMonitorMode())]=])
simpledaw_patch_midi("${input_device}" [=[    if (v.getParent() == state)
        if (id == IDs::armed || id == IDs::targetID)
            handleDestinationChange (EditItemID::fromVar (v[IDs::targetID]),
                                     id == IDs::targetID);]=] [=[    if (v.getParent() != state)
        return;
    if (id == juce::Identifier ("simpleDawMonitorOverride"))
        edit.restartPlayback();
    else if (id == IDs::armed || id == IDs::targetID)
        handleDestinationChange (EditItemID::fromVar (v[IDs::targetID]), id == IDs::targetID);]=])
