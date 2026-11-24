// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

bool testDrumSamplerAudio(te::AudioTrack& track)
{
    using namespace drumMaster;
    constexpr int rate = 48000, retrigger = 480, fade = 144;
    constexpr float amplitude = 0.125f;
    const ju::TemporaryFile file(".wav");
    ju::WavAudioFormat wav;
    auto stream = file.getFile().createOutputStream();
    if (!stream)
        return false;
    std::unique_ptr<ju::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), rate, 1, 16, {}, 0));
    if (!writer)
        return false;
    stream.release();
    ju::AudioBuffer<float> fixture(1, rate / 2);
    std::fill_n(fixture.getWritePointer(0), fixture.getNumSamples(), amplitude);
    if (!writer->writeFromAudioSampleBuffer(fixture, 0, fixture.getNumSamples()))
        return false;
    writer.reset();

    auto* player = sampler(track);
    if (!player)
        return false;
    auto lane = player->state.getChildWithName(laneType);
    const auto original = lane[sampleProperty];
    const auto originalOverlap = lane[overlapProperty];
    const ju::ScopeGuard restore { [&] {
        lane.setProperty(sampleProperty, original, nullptr);
        lane.setProperty(overlapProperty, originalOverlap, nullptr);
    } };
    lane.setProperty(sampleProperty, file.getFile().getFullPathName(), nullptr);
    const auto render = [&](bool overlap, int velocity, bool twice) {
        constexpr int frames = retrigger + fade + 100;
        lane.setProperty(overlapProperty, overlap, nullptr);
        player->initialise({ {}, rate, frames });
        te::MidiMessageArray midi;
        auto send = [&](int frame, int level) {
            auto message = ju::MidiMessage::noteOn(midiChannel, 36, static_cast<ju::uint8>(level));
            message.setTimeStamp(static_cast<double>(frame) / rate);
            midi.addMidiMessage(message, {});
        };
        send(0, velocity);
        if (twice)
            send(retrigger, velocity);
        ju::AudioBuffer<float> output(2, frames);
        output.clear();
        const te::PluginRenderContext context(&output, ju::AudioChannelSet::stereo(), 0, frames,
                                              &midi, 0.0, {}, true, false, false, false);
        player->applyToBuffer(context);
        player->deinitialise();
        return output;
    };
    const auto quiet = render(false, 32, false);
    const auto full = render(false, 127, false);
    const auto overlap = render(true, 127, true);
    const auto choke = render(false, 127, true);
    const auto at = [](const auto& buffer, int i) { return buffer.getSample(0, i); };
    std::fprintf(stderr, "MD0 sampler: velocity 32 = %.6f, 127 = %.6f; overlap ON after 3 ms = %.6f; "
                         "OFF at 0, 1.5, 3 ms = %.6f, %.6f, %.6f\n",
        at(quiet, 100), at(full, 100), at(overlap, retrigger + fade),
        at(choke, retrigger), at(choke, retrigger + fade / 2), at(choke, retrigger + fade));
    const auto near = [](float a, float b) { return std::abs(a - b) < 1.0e-5f; };
    return near(at(quiet, 100), amplitude * 32 / 127) && near(at(full, 100), amplitude)
        && near(at(overlap, retrigger + fade), 2 * amplitude)
        && near(at(choke, retrigger), 2 * amplitude)
        && near(at(choke, retrigger + fade / 2), 1.5f * amplitude)
        && near(at(choke, retrigger + fade - 1), amplitude * (1.0f + 1.0f / fade))
        && near(at(choke, retrigger + fade), amplitude);
}

bool runDrumMasterTest(te::Engine& engine)
{
    using namespace drumMaster;
    const auto check = [](bool passed, const char* failure) {
        if (!passed)
            std::fprintf(stderr, "MD0: %s\n", failure);
        return passed;
    };

    const ju::TemporaryFile legacyFile(".tracktionedit");
    auto legacy = te::createEmptyEdit(engine, legacyFile.getFile());
    legacy->ensureNumberOfAudioTracks(1);
    auto* ordinary = te::getAudioTracks(*legacy).getFirst();
    const auto ordinaryID = ordinary->itemID;
    auto* master = provision(*legacy, false);
    if (!check(master && !master->isProcessing(false) && master == provision(*legacy, true),
               "Legacy projects must provision one disabled singleton"))
        return false;
    if (!check(te::findAudioTrackForID(*legacy, ordinaryID) == ordinary,
               "Legacy audio track identity must survive migration"))
        return false;

    const ju::TemporaryFile newFile(".tracktionedit");
    auto project = te::createEmptyEdit(engine, newFile.getFile());
    master = provision(*project, true);
    if (!check(master && master->isProcessing(false) && lanes(*master).size() == 5,
               "New projects must provide five lanes on an enabled Master"))
        return false;
    for (auto* track : lanes(*master))
    {
        auto* player = sampler(*track);
        if (!check(player && player->resolveSample(player->state.getChildWithName(laneType)).existsAsFile(),
                   "Starter WAV missing"))
            return false;
        if (!check(track->getOutput().outputsToDestTrack(*master) && track->getClips().isEmpty(),
                   "Lanes must route to the master and share its MIDI source"))
            return false;
    }

    if (!check(testDrumSamplerAudio(*lanes(*master).getFirst()), "Sample amplitude and retrigger fade"))
        return false;

    auto& history = project->getUndoManager();
    history.beginNewTransaction("Add layer");
    if (!check(addLane(*master, "Layer", 36) && lanes(*master).size() == 6
               && history.undo() && lanes(*master).size() == 5
               && history.redo() && lanes(*master).size() == 6, "Lane edits must support Undo and Redo"))
        return false;

    auto clip = te::insertMIDIClip(*master, "Shared", { {}, te::TimePosition::fromSeconds(1) });
    if (!check(clip != nullptr, "Could not create MIDI pattern"))
        return false;
    clip->setMidiChannel(te::MidiChannel(midiChannel));
    clip->getSequence().addNote(36, te::BeatPosition::fromBeats(0.1), te::BeatDuration::fromBeats(0.01),
                                100, 0, &history);
    const ju::TemporaryFile exportFile(".wav");
    if (!check(exportWave(*project, exportFile.getFile()), "Could not render drum track"))
        return false;
    ju::WavAudioFormat wav;
    const std::unique_ptr<ju::AudioFormatReader> reader(
        wav.createReaderFor(exportFile.getFile().createInputStream().release(), true));
    if (!check(reader != nullptr, "Could not read the export"))
        return false;
    ju::AudioBuffer<float> audio(2, static_cast<int>(reader->lengthInSamples));
    if (!check(reader->read(&audio, 0, audio.getNumSamples(), 0, true, true)
               && audio.getMagnitude(0, audio.getNumSamples()) > 0.01f
               && audio.getMagnitude(static_cast<int>(reader->sampleRate * 0.07), 1000) > 0.01f,
               "A channel-10 note must produce a one-shot beyond its Note Off"))
        return false;

    auto* kick = lanes(*master).getFirst();
    kick->setSolo(true);
    history.beginNewTransaction("Disable drums");
    setEnabled(*master, false);
    if (!check(!project->areAnyTracksSolo(), "A disabled drum solo must not silence ordinary tracks"))
        return false;
    if (!check(te::EditFileOperations(*project).save(false, true, false), "Project save failed"))
        return false;
    auto reopened = te::loadEditFromFile(engine, newFile.getFile());
    auto* saved = reopened ? find(*reopened) : nullptr;
    if (!check(saved && !saved->isProcessing(false) && lanes(*saved).size() == 6,
               "Project reopen must restore disabled state and lanes"))
        return false;
    for (auto* track : lanes(*saved))
        if (!check(!track->isProcessing(false) && sampler(*track), "Disabled lanes must bypass track processing"))
            return false;
    setEnabled(*saved, true);
    return check(lanes(*saved).getFirst()->isSolo(false), "Enable must restore the saved solo state");
}
