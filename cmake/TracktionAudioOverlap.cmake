# Add SimpleDAW's per-clip audio gate to the pinned Tracktion graph builder.
# Run after FetchContent resolves the source so source-archive overrides use the same hook.
set(graph_builder "${tracktion_SOURCE_DIR}/modules/tracktion_engine/playback/graph/tracktion_EditNodeBuilder.cpp")
file(READ "${graph_builder}" graph_source)
string(REPLACE "\r\n" "\n" graph_source "${graph_source}")
if(NOT graph_source MATCHES "SimpleDAW audio overlap hook")
    function(replace_graph_anchor before after)
        string(FIND "${graph_source}" "${before}" anchor)
        if(anchor EQUAL -1)
            message(FATAL_ERROR "Tracktion audio overlap hook requires the pinned graph builder")
        endif()
        string(REPLACE "${before}" "${after}" updated "${graph_source}")
        set(graph_source "${updated}" PARENT_SCOPE)
    endfunction()

    replace_graph_anchor("#define USE_DYNAMIC_OFFSET_CONTAINER_CLIP 1" [=[
#define USE_DYNAMIC_OFFSET_CONTAINER_CLIP 1
// SimpleDAW audio overlap hook
#include "AudioOverlap.h"
]=])
    set(graph_anchor [=[std::unique_ptr<tracktion::graph::Node> createNodeForClips (EditItemID trackID, const juce::Array<Clip*>& clips,
                                                            const TrackMuteState& trackMuteState, const CreateNodeParams& params)
{
]=])
    replace_graph_anchor("${graph_anchor}" [=[
#include "AudioOverlapGraph.inc"

std::unique_ptr<tracktion::graph::Node> createNodeForClips (EditItemID trackID, const juce::Array<Clip*>& clips,
                                                            const TrackMuteState& trackMuteState, const CreateNodeParams& params)
{
    const auto createArrangerNode = [&](Clip& clip)
    {
        auto node = createNodeForClip (clip, trackMuteState, params, ClipRole::arranger);
        if (auto* audio = dynamic_cast<AudioClipBase*> (&clip); node && audio)
        {
            auto ranges = simpleDaw::suppressedAudioRanges (*audio, clips, params.allowedClips);
            if (!ranges.empty())
                node = std::make_unique<SimpleDawOverlapNode> (std::move (node), params.processState, std::move (ranges));
        }
        return node;
    };
]=])
    replace_graph_anchor("createNodeForClip (*clip, trackMuteState, params, ClipRole::arranger)" "createArrangerNode (*clip)")
    replace_graph_anchor([=[            nodes.push_back (createFadeNodeForClip (*araClip, araClip->getEditTimeRange(), std::move (araNode), params));]=] [=[
        {
            auto node = createFadeNodeForClip (*araClip, araClip->getEditTimeRange(), std::move (araNode), params);
            auto ranges = simpleDaw::suppressedAudioRanges (*araClip, clips, params.allowedClips);
            if (!ranges.empty())
                node = std::make_unique<SimpleDawOverlapNode> (std::move (node), params.processState, std::move (ranges));
            nodes.push_back (std::move (node));
        }
]=])
    file(WRITE "${graph_builder}" "${graph_source}")
endif()
