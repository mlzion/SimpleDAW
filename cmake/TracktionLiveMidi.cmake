# Keep the arrangement's MIDI sequence in source pitches. Map each emitted note
# after Tracktion has applied clip boundaries, looping, mute and timing.
function(simpledaw_patch_midi file before after)
    set(path "${tracktion_SOURCE_DIR}/modules/tracktion_engine/${file}")
    file(READ "${path}" source)
    string(REPLACE "\r\n" "\n" source "${source}")
    string(FIND "${source}" "${after}" applied)
    if(applied EQUAL -1)
        string(FIND "${source}" "${before}" anchor)
        if(anchor EQUAL -1)
            message(FATAL_ERROR "Live MIDI playback requires the pinned Tracktion source: ${file}")
        endif()
        string(REPLACE "${before}" "${after}" source "${source}")
        file(WRITE "${path}" "${source}")
    endif()
endfunction()

set(clip_header model/clips/tracktion_MidiClip.h)
set(clip_source model/clips/tracktion_MidiClip.cpp)
set(graph_source playback/graph/tracktion_EditNodeBuilder.cpp)
simpledaw_patch_midi("${clip_header}" [=[    MidiClip() = delete;]=] [=[
    // Shared with playback so graph nodes can read settings even during clip deletion.
    const std::shared_ptr<std::atomic<int>> simpleDawPlaybackSettings =
        std::make_shared<std::atomic<int>> ((24 << 8) | 100);
    void refreshSimpleDawPlaybackSettings();

    MidiClip() = delete;]=])
simpledaw_patch_midi("${clip_source}" [=[    : Clip (v, targetParent, id, Type::midi)
{
    auto um = getUndoManager();]=] [=[    : Clip (v, targetParent, id, Type::midi)
{
    refreshSimpleDawPlaybackSettings();
    auto um = getUndoManager();]=])
simpledaw_patch_midi("${clip_source}" [=[void MidiClip::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& id)
{
    if (tree == state)
    {
        if (id == IDs::mute)]=] [=[
void MidiClip::refreshSimpleDawPlaybackSettings()
{
    const auto transpose = juce::jlimit (-24, 24, int (state.getProperty ("simpleDawMidiTranspose", 0)));
    const auto velocity = juce::jlimit (0, 200, int (state.getProperty ("simpleDawMidiVelocity", 100)));
    simpleDawPlaybackSettings->store (((transpose + 24) << 8) | velocity, std::memory_order_relaxed);
}

void MidiClip::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& id)
{
    if (tree == state)
    {
        if (id == juce::Identifier ("simpleDawMidiTranspose") || id == juce::Identifier ("simpleDawMidiVelocity"))
        {
            refreshSimpleDawPlaybackSettings();
            return;
        }
        if (id == IDs::mute)]=])
simpledaw_patch_midi("${graph_source}" [=[#define USE_DYNAMIC_OFFSET_CONTAINER_CLIP 1]=] [=[
#define USE_DYNAMIC_OFFSET_CONTAINER_CLIP 1
#include "MidiPlaybackNotes.h"]=])
simpledaw_patch_midi("${graph_source}" [=[std::unique_ptr<tracktion::graph::Node> createNodeForMidiClip (MidiClip& clip, const TrackMuteState& trackMuteState,]=] [=[
#include "MidiPlaybackNode.inc"

std::unique_ptr<tracktion::graph::Node> createUnmappedNodeForMidiClip (MidiClip& clip, const TrackMuteState& trackMuteState,]=])
simpledaw_patch_midi("${graph_source}" [=[clip.getSequence().exportToPlaybackMidiSequence (clip, timeBase, generateMPE)]=] [=[MidiList::createDefaultPlaybackMidiSequence (clip.getSequence(), clip, timeBase, generateMPE)]=])
simpledaw_patch_midi("${graph_source}" [=[clip.getSequenceLooped().exportToPlaybackMidiSequence (clip, timeBase, generateMPE)]=] [=[MidiList::createDefaultPlaybackMidiSequence (clip.getSequenceLooped(), clip, timeBase, generateMPE)]=])
simpledaw_patch_midi("${graph_source}" [=[return createNodeForMidiClip (*midiClip, trackMuteState, params, role);]=] [=[
return graph::makeNode<SimpleDawMidiPlaybackNode> (
            createUnmappedNodeForMidiClip (*midiClip, trackMuteState, params, role), *midiClip);]=])
