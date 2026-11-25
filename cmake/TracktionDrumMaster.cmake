# The master owns the MIDI clips. Each native lane track reads the same
# arrangement source before its own instrument and audio effects.
simpledaw_patch_midi("playback/graph/tracktion_EditNodeBuilder.cpp" [=[
    std::unique_ptr<Node> node = createClipsNode (at, *clipsMuteState, params);]=] [=[
    std::unique_ptr<Node> node;
    const auto drumSource = EditItemID::fromProperty (at.state, juce::Identifier ("simpleDawDrumSource"));
    if (drumSource.isValid())
    {
        if (auto* master = findAudioTrackForID (at.edit, drumSource); master != nullptr && master->isProcessing (true))
            node = createNodeForClips (at.itemID, master->getClips(), *clipsMuteState, params);
    }
    else if (! bool (at.state.getProperty ("simpleDawDrumMaster", false)))
    {
        node = createClipsNode (at, *clipsMuteState, params);
    }]=])

# Use the owning track ID, so ordinary-track reordering cannot retarget a lane.
simpledaw_patch_midi("model/tracks/tracktion_TrackOutput.cpp" [=[
AudioTrack* TrackOutput::getDestinationTrack() const
{
    if (destTrackID.isValid())]=] [=[
AudioTrack* TrackOutput::getDestinationTrack() const
{
    const auto drumSource = EditItemID::fromProperty (owner.state, juce::Identifier ("simpleDawDrumSource"));
    if (drumSource.isValid())
    {
        auto* master = findAudioTrackForID (owner.edit, drumSource);
        return master != &owner && master != nullptr && bool (master->state.getProperty ("simpleDawDrumMaster", false))
            ? master : nullptr;
    }

    if (destTrackID.isValid())]=])

simpledaw_patch_midi("model/tracks/tracktion_TrackOutput.cpp" [=[
    return t.itemID == destTrackID;]=] [=[
    if (owner.state.hasProperty ("simpleDawDrumSource"))
        return getDestinationTrack() == &t;

    return t.itemID == destTrackID;]=])

simpledaw_patch_midi("model/tracks/tracktion_TrackOutput.cpp" [=[
OutputDevice* TrackOutput::getOutputDevice (bool traceThroughDestTracks) const
{
    auto& dm = owner.edit.engine.getDeviceManager();]=] [=[
OutputDevice* TrackOutput::getOutputDevice (bool traceThroughDestTracks) const
{
    if (owner.state.hasProperty ("simpleDawDrumSource"))
    {
        if (traceThroughDestTracks)
            if (auto* master = getDestinationTrack())
                return master->getOutput().getOutputDevice (true);

        return {};
    }

    auto& dm = owner.edit.engine.getDeviceManager();]=])

# A lane solo filters its kit locally. Only the master participates in global solo.
simpledaw_patch_midi("model/edit/tracktion_Edit.cpp" [=[
    return findTrackForPredicate (*this, [] (Track& t) { return t.isSolo (true); }) != nullptr;]=] [=[
    return findTrackForPredicate (*this, [] (Track& t)
    {
        return ! t.state.hasProperty ("simpleDawDrumSource") && t.isSolo (true);
    }) != nullptr;]=])

simpledaw_patch_midi("model/tracks/tracktion_AudioTrack.cpp" [=[
    if (areAnyTracksSolo && isInputTrackSolo (*this))
        return true;]=] [=[
    if (state.hasProperty ("simpleDawDrumSource"))
    {
        if (auto* master = output->getDestinationTrack())
        {
            const auto inputs = master->getInputTracks();
            const bool hasLaneSolo = std::any_of (inputs.begin(), inputs.end(), [] (auto* lane)
            {
                return lane->state.hasProperty ("simpleDawDrumSource") && lane->isSolo (false);
            });
            if ((hasLaneSolo && ! isSolo (false))
                || (areAnyTracksSolo && ! master->isSolo (true) && ! master->isSoloIsolate (true)))
                return false;
        }
        return ! isMuted (true);
    }

    if (areAnyTracksSolo && ! state.hasProperty ("simpleDawDrumMaster") && isInputTrackSolo (*this))
        return true;]=]
)
