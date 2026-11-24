// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

    static bool testDrumPlayback(te::Engine& engine)
    {
        using namespace drumMaster;
        constexpr int rate = 48000, fade = rate * 3 / 1000;
        const auto check = [](bool passed, const char* message) {
            if (!passed)
                std::fprintf(stderr, "MD0 integrated playback: %s\n", message);
            return passed;
        };
        const ju::TemporaryFile projectFile(".tracktionedit"), settings(".settings"), sampleFile(".wav");
        ju::WavAudioFormat wav;
        auto stream = sampleFile.getFile().createOutputStream();
        if (!stream)
            return false;
        std::unique_ptr<ju::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), rate, 1, 16, {}, 0));
        if (!writer)
            return false;
        stream.release();
        ju::AudioBuffer<float> sample(1, rate / 4);
        std::fill_n(sample.getWritePointer(0), sample.getNumSamples(), 0.125f);
        if (!writer->writeFromAudioSampleBuffer(sample, 0, sample.getNumSamples()))
            return false;
        writer.reset();

        MainGui gui(engine, settings.getFile());
        auto project = te::createEmptyEdit(engine, projectFile.getFile());
        project->ensureNumberOfAudioTracks(1);
        auto* ordinary = te::getAudioTracks(*project).getFirst();
        auto* master = provision(*project, true);
        if (!master)
            return false;
        gui.setProject(std::move(project), projectFile.getFile());
        auto& edit = *gui.edit;
        const auto rows = lanes(*master);
        auto* kick = rows[0];
        auto* snare = rows[1];
        auto kickSettings = sampler(*kick)->state.getChildWithName(laneType);
        for (auto* lane : { kick, snare })
            sampler(*lane)->state.getChildWithName(laneType)
                .setProperty(sampleProperty, sampleFile.getFile().getFullPathName(), nullptr);
        auto pattern = te::insertMIDIClip(*master, "Overlap comparison",
            { {}, te::TimePosition::fromSeconds(1.4) });
        if (!pattern)
            return false;
        setDrumClipChannel(*pattern);
        const auto noteAt = [&](int pitch, double seconds) {
            pattern->getSequence().addNote(pitch, edit.tempoSequence.toBeats(te::TimePosition::fromSeconds(seconds)),
                                          te::BeatDuration::fromBeats(0.005), 127, 0, nullptr);
        };
        noteAt(36, 0.1);
        noteAt(36, 0.11);
        const auto render = [&]() -> ju::AudioBuffer<float> {
            ju::MessageManager::getInstance()->runDispatchLoopUntil(35);
            const ju::TemporaryFile output(".wav");
            const te::Edit::ScopedRenderStatus rendering(edit, true);
            te::Renderer::Parameters options(edit);
            options.destFile = output.getFile();
            options.audioFormat = &wav;
            options.sampleRateForAudio = rate;
            options.bitDepth = 24;
            options.canRenderInMono = false;
            options.checkNodesForAudio = false;
            options.time = { {}, te::TimePosition::fromSeconds(1.4) };
            options.tracksToDo = te::toBitSet(te::getAllTracks(edit));
            options.useMasterPlugins = true;
            if (te::Renderer::renderToFile("Render drum regression", options) != output.getFile())
                return {};
            std::unique_ptr<ju::AudioFormatReader> reader(wav.createReaderFor(output.getFile().createInputStream().release(), true));
            if (!reader || reader->lengthInSamples < rate)
                return {};
            ju::AudioBuffer<float> audio(2, static_cast<int>(reader->lengthInSamples));
            if (!reader->read(&audio, 0, audio.getNumSamples(), 0, true, true))
                return {};
            return audio;
        };

        gui.selectTrack(gui.visibleTracks().indexOf(kick));
        const auto off = render();
        gui.toggleDrumOverlap(kickSettings);
        const auto on = render();
        if (!check(on.getNumSamples() == off.getNumSamples() && on.getNumSamples() > rate,
                   "both Inspector settings must render identical-length outputs"))
            return false;
        const auto single = on.getSample(0, static_cast<int>(0.105 * rate));
        if (!check(single > 0.02f, "the initial kick must reach the master output"))
            return false;
        const auto near = [single](float actual, float ratio) { return std::abs(actual / single - ratio) < 0.005f; };
        int retrigger = static_cast<int>(0.108 * rate);
        while (retrigger < static_cast<int>(0.112 * rate) && on.getSample(0, retrigger) < 1.5f * single)
            ++retrigger;
        const auto level = [&](const auto& audio, int offset) { return audio.getSample(0, retrigger + offset); };
        std::fprintf(stderr, "D44 at %d Hz, retrigger frame %d, single %.6f, ON after 3 ms %.6f, "
                     "OFF at 0/1.5/3 ms %.6f/%.6f/%.6f, ON/OFF before first natural end %.6f/%.6f\n",
                     rate, retrigger, single, level(on, fade), level(off, 0), level(off, fade / 2), level(off, fade),
                     level(on, rate / 4 - 800), level(off, rate / 4 - 800));
        if (!check(retrigger < static_cast<int>(0.112 * rate)
                   && near(level(on, fade), 2.0f) && near(level(off, 0), 2.0f)
                   && near(level(off, fade / 2), 1.5f) && near(level(off, fade - 1), 1.0f + 1.0f / fade)
                   && near(level(off, fade), 1.0f)
                   && near(level(on, rate / 4 - 800), 2.0f) && near(level(off, rate / 4 - 800), 1.0f)
                   && near(level(on, rate / 4 - 240), 1.0f)
                   && near(level(on, rate / 4 - fade / 2), 0.5f)
                   && near(level(off, rate / 4 - fade / 2), 0.5f)
                   && near(level(on, rate / 4), 0.0f) && near(level(off, rate / 4), 0.0f),
                   "Inspector Overlap must control retrigger release and preserve the natural tail"))
            return false;

        noteAt(38, 0.48);
        if (!ordinary || !insertAudio(*ordinary, sampleFile.getFile(), te::TimePosition::fromSeconds(0.85)))
            return false;
        const auto checkMix = [&](const char* label, bool kickAudible, bool snareAudible, bool ordinaryAudible) {
            const auto mix = render();
            if (mix.getNumSamples() <= rate)
                return check(false, label);
            const auto audible = [&](double time) {
                return mix.getMagnitude(static_cast<int>(time * rate), rate / 100) > 0.01f;
            };
            return check(audible(0.16) == kickAudible && audible(0.55) == snareAudible
                         && audible(0.92) == ordinaryAudible, label);
        };
        master->setSolo(true);
        if (!checkMix("master solo includes the entire kit", true, true, false))
            return false;
        master->setSolo(false);
        kick->setSolo(true);
        if (!checkMix("lane solo isolates the selected lane", true, false, true))
            return false;
        ordinary->setSolo(true);
        if (!checkMix("ordinary solo still isolates ordinary audio", false, false, true))
            return false;
        ordinary->setSolo(false);
        master->setSolo(true);
        if (!checkMix("master solo respects lane solo", true, false, false))
            return false;
        snare->setSolo(true);
        if (!checkMix("multiple soloed lanes remain audible", true, true, false))
            return false;
        snare->setSolo(false);
        kick->setMute(true);
        if (!checkMix("muted solo lane remains silent", false, false, false))
            return false;
        kick->setMute(false);
        master->setMute(true);
        if (!checkMix("master mute takes precedence over kit solo", false, false, false))
            return false;
        master->setMute(false);
        drumMaster::setEnabled(*master, false);
        return checkMix("disabled kit lets ordinary audio play", false, false, true);
    }
