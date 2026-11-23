// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

class SimpleDawMidiPlaybackNode final : public graph::Node
{
public:
    SimpleDawMidiPlaybackNode(std::unique_ptr<graph::Node> source, const MidiClip& clip)
        : input(std::move(source)), settings(clip.simpleDawPlaybackSettings) {}

    graph::NodeProperties getNodeProperties() override
    {
        auto properties = input->getNodeProperties();
        properties.nodeID = static_cast<size_t>(hash(0x7c3c061u, properties.nodeID));
        return properties;
    }

    std::vector<graph::Node*> getDirectInputNodes() override { return { input.get() }; }
    bool isReadyToProcess() override { return input->hasProcessed(); }

    void prepareToPlay(const graph::PlaybackInitialisationInfo& info) override
    {
        if (auto* previous = graph::findNodeWithIDIfNonZero<SimpleDawMidiPlaybackNode>(
                info.nodeGraphToReplace, getNodeProperties().nodeID);
            previous && previous->settings == settings)
            notes = previous->notes;
    }

    void process(ProcessContext& context) override
    {
        const auto& inputMidi = input->getProcessedOutput().midi;
        auto& output = context.buffers.midi;
        output.isAllNotesOff = inputMidi.isAllNotesOff;
        const auto values = settings->load(std::memory_order_relaxed);
        const auto transpose = (values >> 8) - 24;
        const auto velocity = values & 255;
        for (auto event : inputMidi)
        {
            const auto sendNoteOff = [&](juce::MidiMessage off) {
                output.addMidiMessage(std::move(off), event.mpeSourceID);
            };
            if (notes->map(event, transpose, velocity, sendNoteOff))
                output.add(std::move(event));
        }
    }

private:
    std::unique_ptr<graph::Node> input;
    std::shared_ptr<std::atomic<int>> settings;
    std::shared_ptr<simpleDaw::MidiPlaybackNotes> notes = std::make_shared<simpleDaw::MidiPlaybackNotes>();
};
