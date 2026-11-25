// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only
#pragma once

#include "DrumVoices.h"

namespace drumMaster
{
const ju::Identifier masterProperty { "simpleDawDrumMaster" };
const ju::Identifier sourceProperty { "simpleDawDrumSource" };
const ju::Identifier collapsedProperty { "simpleDawDrumCollapsed" };
const ju::Identifier suspendedSoloProperty { "simpleDawDrumSuspendedSolo" };
const ju::Identifier laneType { "SIMPLEDAW_DRUM_LANE" };
const ju::Identifier sampleProperty { "sample" };
const ju::Identifier noteProperty { "note" };
const ju::Identifier overlapProperty { "overlap" };
constexpr int maxLanes = 16;
constexpr int midiChannel = 10;

ju::File libraryDirectory()
{
    return ju::File::getSpecialLocation(ju::File::currentExecutableFile)
        .getParentDirectory().getChildFile("Drum Samples");
}

ju::String sampleCategory(const ju::String& name)
{
    for (const auto* prefix : { "Kick", "Snare", "HH", "LTom", "MTom", "HTom", "Crash", "Ride", "Clap", "Perc" })
        if (name.startsWithIgnoreCase(ju::String(prefix) + "_"))
            return prefix;
    return "Other";
}

ju::Array<ju::File> scanLibrary()
{
    auto files = libraryDirectory().findChildFiles(ju::File::findFiles, false);
    for (int i = files.size(); --i >= 0;)
        if (!files[i].hasFileExtension("wav"))
            files.remove(i);
    files.sort();
    return files;
}

bool isMaster(const te::Track& track)
{
    return static_cast<bool>(track.state[masterProperty]);
}

bool isLane(const te::Track& track)
{
    return te::EditItemID::fromProperty(track.state, sourceProperty).isValid();
}

bool isDrum(const te::Track& track)
{
    return isMaster(track) || isLane(track);
}

ju::Array<te::AudioTrack*> lanes(te::AudioTrack& master)
{
    ju::Array<te::AudioTrack*> children;
    for (auto* track : te::getAudioTracks(master.edit))
        if (te::EditItemID::fromProperty(track->state, sourceProperty) == master.itemID)
            children.add(track);
    return children;
}

te::AudioTrack* find(te::Edit& edit)
{
    for (auto* track : te::getAudioTracks(edit))
        if (isMaster(*track))
            return track;
    return nullptr;
}

class Sampler final : public te::Plugin, private ju::AsyncUpdater
{
public:
    static constexpr auto xmlTypeName = "simpleDawDrumSampler";
    static const char* getPluginName() { return "Drum Sampler"; }

    explicit Sampler(te::PluginCreationInfo info) : Plugin(info) {}
    ~Sampler() override
    {
        cancelPendingUpdate();
        notifyListenersOfDeletion();
    }

    ju::String getName() const override { return getPluginName(); }
    ju::String getPluginType() override { return xmlTypeName; }
    ju::String getSelectableDescription() override { return getName(); }
    bool isSynth() override { return true; }
    bool takesMidiInput() override { return true; }
    bool takesAudioInput() override { return true; }
    bool producesAudioWhenNoAudioInput() override { return true; }
    int getNumOutputChannelsGivenInputs(int) override { return 2; }
    void initialise(const te::PluginInitialisationInfo& info) override
    {
        outputRate = info.sampleRate;
        refreshSample();
    }
    void deinitialise() override
    {
        ju::SpinLock::ScopedLockType lock(sampleLock);
        voices.reset();
        auditionRequested.store(false);
        wasPlaying = false;
    }
    void midiPanic() override
    {
        auditionRequested.store(false);
        stopRequested.store(true);
    }
    void reset() override { midiPanic(); }
    void restorePluginStateFromValueTree(const ju::ValueTree&) override { triggerAsyncUpdate(); }

    ju::File resolveSample(const ju::ValueTree& lane) const
    {
        const auto reference = lane[sampleProperty].toString();
        if (reference.isEmpty())
            return {};
        if (reference.startsWith("library:"))
        {
            const auto name = reference.substring(8);
            return name.isNotEmpty() && !name.containsAnyOf("/\\:") && name != "." && name != ".."
                ? libraryDirectory().getChildFile(name) : ju::File{};
        }
        return te::SourceFileReference::findFileFromString(edit, reference);
    }

    void audition()
    {
        refreshSample();
        auditionRequested.store(true);
    }

    void applyToBuffer(const te::PluginRenderContext& context) override
    {
        if (!context.destBuffer || context.destBuffer->getNumChannels() == 0 || context.bufferNumSamples <= 0)
            return;

        ju::SpinLock::ScopedTryLockType lock(sampleLock);
        if (!lock.isLocked())
            return;

        auto* left = context.destBuffer->getWritePointer(0, context.bufferStartSample);
        auto* right = context.destBuffer->getNumChannels() > 1
            ? context.destBuffer->getWritePointer(1, context.bufferStartSample) : nullptr;
        auto* midi = context.bufferForMidiMessages;
        const bool playing = context.isPlaying || context.isRendering;

        if (stopRequested.exchange(false) || (wasPlaying && !playing) || (midi && midi->isAllNotesOff))
            voices.release();
        wasPlaying = playing;

        const auto preview = auditionRequested.exchange(false);
        if (preview && sample)
            voices.trigger(1.0f, false);

        int rendered = 0;
        if (playing && midi && sample)
            for (const auto& event : *midi)
                if (event.isNoteOn() && event.getChannel() == midiChannel && event.getNoteNumber() == note)
                {
                    const auto offset = ju::jlimit(rendered, context.bufferNumSamples,
                        ju::roundToInt(event.getTimeStamp() * outputRate));
                    render(left, right, rendered, offset);
                    rendered = offset;
                    voices.trigger(event.getFloatVelocity(), overlap);
                }
        render(left, right, rendered, context.bufferNumSamples);
        if (midi)
            midi->clear();
    }

private:
    struct Sample
    {
        ju::AudioBuffer<float> audio;
        double rate = 0;
    };

    static std::shared_ptr<const Sample> load(const ju::File& file)
    {
        if (!file.hasFileExtension("wav") || !file.existsAsFile())
            return {};
        ju::WavAudioFormat wav;
        auto stream = file.createInputStream();
        if (!stream)
            return {};
        const std::unique_ptr<ju::AudioFormatReader> reader(wav.createReaderFor(stream.release(), true));
        // A bounded in-memory one-shot cache keeps file access off the audio thread.
        if (!reader || reader->sampleRate <= 0 || reader->lengthInSamples <= 0
            || reader->lengthInSamples > 4'000'000)
            return {};
        auto result = std::make_shared<Sample>();
        result->rate = reader->sampleRate;
        result->audio.setSize(2, static_cast<int>(reader->lengthInSamples));
        return reader->read(&result->audio, 0, result->audio.getNumSamples(), 0, true, true)
            ? result : nullptr;
    }

    void refreshSample()
    {
        const auto lane = state.getChildWithName(laneType);
        const auto file = resolveSample(lane);
        const auto modified = file.getLastModificationTime();
        const bool reload = file != loadedFile || modified != loadedTime;
        auto next = reload ? load(file) : nullptr;
        ju::SpinLock::ScopedLockType lock(sampleLock);
        if (reload)
        {
            sample.swap(next);
            loadedFile = file;
            loadedTime = modified;
        }
        note = static_cast<int>(lane.getProperty(noteProperty, -1));
        overlap = static_cast<bool>(lane.getProperty(overlapProperty, false));
        if (reload || preparedRate != outputRate)
        {
            voices.prepare(outputRate);
            preparedRate = outputRate;
        }
    }

    void valueTreeChanged() override
    {
        Plugin::valueTreeChanged();
        triggerAsyncUpdate();
    }

    void handleAsyncUpdate() override { refreshSample(); }

    void render(float* left, float* right, int begin, int end)
    {
        if (!sample || !voices.isActive())
            return;
        const auto length = sample->audio.getNumSamples();
        const auto increment = sample->rate / outputRate;
        const auto read = [&](int channel, int index) { return sample->audio.getSample(channel, index); };
        for (int frame = begin; frame < end && voices.isActive(); ++frame)
        {
            float l = 0, r = 0;
            voices.render(l, r, length, increment, read);
            left[frame] += right ? l : (l + r) * 0.5f;
            if (right)
                right[frame] += r;
        }
    }

    ju::SpinLock sampleLock;
    ju::File loadedFile;
    ju::Time loadedTime;
    std::shared_ptr<const Sample> sample;
    simpleDaw::DrumVoices voices;
    int note = -1;
    bool overlap = false;
    double preparedRate = 0;
    std::atomic<bool> auditionRequested { false };
    std::atomic<bool> stopRequested { false };
    double outputRate = 44100.0;
    bool wasPlaying = false;
};

Sampler* sampler(te::AudioTrack& lane)
{
    for (auto* plugin : lane.pluginList.getPlugins())
        if (auto* instrument = dynamic_cast<Sampler*>(plugin))
            return instrument;
    return nullptr;
}

te::AudioTrack* addLane(te::AudioTrack& master, const ju::String& name, int note,
                        const ju::String& reference = {})
{
    if (lanes(master).size() >= maxLanes)
        return nullptr;
    auto& edit = master.edit;
    auto track = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr);
    if (!track)
        return nullptr;
    track->state.setProperty(sourceProperty, master.itemID, nullptr);
    track->state.setProperty(te::IDs::process, master.isProcessing(false), nullptr);
    track->setName(name);

    ju::ValueTree instrument(te::IDs::PLUGIN), lane(laneType);
    instrument.setProperty(te::IDs::type, Sampler::xmlTypeName, nullptr);
    lane.setProperty(noteProperty, note, nullptr);
    lane.setProperty(sampleProperty, reference, nullptr);
    instrument.addChild(lane, -1, nullptr);
    track->pluginList.insertPlugin(instrument, 0);
    return track.get();
}

void setEnabled(te::AudioTrack& master, bool enabled)
{
    auto tracks = lanes(master);
    tracks.insert(0, &master);
    auto& history = master.edit.getUndoManager();
    for (auto* track : tracks)
    {
        if (!enabled)
        {
            if (auto* instrument = sampler(*track))
                instrument->midiPanic();
            // Tracktion counts solo on non-processing tracks, so suspend it
            // alongside the graph and restore it with the other kit settings.
            if (track->isSolo(false))
            {
                track->state.setProperty(suspendedSoloProperty, true, &history);
                track->setSolo(false);
            }
        }
        else if (static_cast<bool>(track->state[suspendedSoloProperty]))
        {
            track->setSolo(true);
            track->state.removeProperty(suspendedSoloProperty, &history);
        }
        track->setProcessing(enabled);
    }
    master.edit.markAsChanged();
}

te::AudioTrack* provision(te::Edit& edit, bool newProject)
{
    if (auto* existing = find(edit))
        return existing;

    auto track = edit.insertNewAudioTrack(te::TrackInsertPoint::getEndOfTracks(edit), nullptr);
    if (!track)
        return nullptr;
    track->state.setProperty(masterProperty, true, nullptr);
    track->state.setProperty(te::IDs::process, newProject, nullptr);
    track->setName("Master Drum Track");
    addLane(*track, "Kick", 36, "library:Kick_Classic.wav");
    addLane(*track, "Snare", 38, "library:Snare_Classic.wav");
    addLane(*track, "Hi-hat", 42, "library:HH_Closed.wav");
    addLane(*track, "Tom", 45, "library:LTom_Classic.wav");
    addLane(*track, "Crash", 49, "library:Crash_Classic.wav");
    return track.get();
}
}
