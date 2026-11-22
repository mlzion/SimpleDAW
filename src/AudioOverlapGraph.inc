// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

class SimpleDawOverlapNode final : public tracktion::graph::Node, private TracktionEngineNode
{
public:
    SimpleDawOverlapNode(std::unique_ptr<tracktion::graph::Node> source, ProcessState& state,
                        std::vector<TimeRange> ranges)
        : TracktionEngineNode(state), input(std::move(source)), suppressed(std::move(ranges))
    {
        setOptimisations({ tracktion::graph::ClearBuffers::no, tracktion::graph::AllocateAudioBuffer::yes });
    }

    tracktion::graph::NodeProperties getNodeProperties() override
    {
        auto properties = input->getNodeProperties();
        if (properties.nodeID != 0)
            hash_combine(properties.nodeID, size_t(839346334));
        return properties;
    }

    std::vector<tracktion::graph::Node*> getDirectInputNodes() override { return { input.get() }; }
    bool isReadyToProcess() override { return input->hasProcessed(); }

    void process(ProcessContext& context) override
    {
        auto source = input->getProcessedOutput();
        auto audio = context.buffers.audio;
        context.buffers.midi.copyFrom(source.midi);
        const auto block = getEditTimeRange();
        const auto frames = static_cast<int>(audio.getNumFrames());
        if (block.isEmpty() || frames == 0
            || std::none_of(suppressed.begin(), suppressed.end(), [&](auto range) { return range.overlaps(block); }))
        {
            setAudioOutput(input.get(), source.audio);
            return;
        }
        tracktion::graph::copyIfNotAliased(audio, source.audio);
        const auto frameAt = [&](TimePosition time) {
            const auto phase = juce::jlimit(0.0, 1.0, (time - block.getStart()) / block.getLength());
            return static_cast<choc::buffer::FrameCount>(juce::roundToInt(phase * frames));
        };
        for (auto range : suppressed)
            if (range.overlaps(block))
                audio.getFrameRange({ frameAt(range.getStart()), frameAt(range.getEnd()) }).clear();
    }

private:
    std::unique_ptr<tracktion::graph::Node> input;
    const std::vector<TimeRange> suppressed;
};
