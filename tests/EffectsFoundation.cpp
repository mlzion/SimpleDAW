// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <JuceHeader.h>
#include <cmath>
#include <cstdio>

namespace te = tracktion;
namespace ju = juce;

namespace
{
using Audio = ju::AudioBuffer<float>;

bool check(bool passed, const char* description)
{
    if (!passed)
        std::fprintf(stderr, "Effects foundation: %s\n", description);
    return passed;
}

bool near(float actual, float expected, float tolerance = 1.0e-6f)
{
    return std::abs(actual - expected) <= tolerance;
}

bool finite(const Audio& audio)
{
    if (audio.getNumSamples() == 0 || audio.getNumChannels() == 0)
        return false;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
        for (int sample = 0; sample < audio.getNumSamples(); ++sample)
            if (!std::isfinite(audio.getSample(channel, sample)))
                return false;
    return true;
}

bool equivalent(const Audio& a, const Audio& b, float tolerance, int firstSample = 0)
{
    if (!finite(a) || a.getNumChannels() != b.getNumChannels()
        || a.getNumSamples() != b.getNumSamples() || firstSample >= a.getNumSamples())
        return false;
    for (int channel = 0; channel < a.getNumChannels(); ++channel)
        for (int sample = firstSample; sample < a.getNumSamples(); ++sample)
            if (!near(a.getSample(channel, sample), b.getSample(channel, sample), tolerance))
                return false;
    return true;
}

Audio signal(int channels, int samples, double rate)
{
    Audio buffer(channels, samples);
    for (int channel = 0; channel < channels; ++channel)
        for (int i = 0; i < samples; ++i)
        {
            const double phase = ju::MathConstants<double>::twoPi * i / rate;
            buffer.setSample(channel, i, static_cast<float>(
                0.24 * std::sin(phase * (channel == 0 ? 173 : 197))
                + 0.13 * std::sin(phase * (channel == 0 ? 1543 : 1723))));
        }
    return buffer;
}

template <typename Plugin>
ju::ReferenceCountedObjectPtr<Plugin> add(te::AudioTrack& track, int index)
{
    const auto plugin = track.edit.getPluginCache().createNewPlugin(
        Plugin::xmlTypeName, te::PluginManager::createBuiltInPluginDescription<Plugin>(false));
    if (plugin)
        return dynamic_cast<Plugin*>(track.pluginList.insertPlugin(plugin->state, index).get());
    return {};
}

Audio process(te::Plugin& plugin, const Audio& input, double rate, int block)
{
    Audio output(input);
    plugin.baseClassInitialise({{}, rate, block});
    plugin.reset();
    const ju::ScopeGuard deinitialise{[&] { plugin.baseClassDeinitialise(); }};
    const auto channels = ju::AudioChannelSet::canonicalChannelSet(input.getNumChannels());
    for (int start = 0; start < output.getNumSamples(); start += block)
    {
        const int count = ju::jmin(block, output.getNumSamples() - start);
        plugin.applyToBuffer({&output, channels, start, count, nullptr, 0.0,
                              {}, true, false, true, false});
    }
    return output;
}

bool testReverb(te::ReverbPlugin& reverb)
{
    if (!check(near(reverb.getRoomSize(), 0.3f) && near(reverb.getDamp(), 0.5f)
               && near(reverb.getWet(), 1.0f / 3) && near(reverb.getDry(), 0.5f)
               && near(reverb.getWidth(), 1.0f) && near(reverb.getMode(), 0.0f)
               && reverb.isEnabled(), "native Reverb defaults"))
        return false;

    reverb.setRoomSize(-1.0f);
    reverb.setDamp(2.0f);
    reverb.setWet(-1.0f);
    reverb.setDry(2.0f);
    if (!check(near(reverb.getRoomSize(), 0.0f) && near(reverb.getDamp(), 1.0f)
               && near(reverb.getWet(), 0.0f) && near(reverb.getDry(), 1.0f),
               "native Reverb setters clamp out-of-range values"))
        return false;

    reverb.setRoomSize(2.0f);
    reverb.setDamp(-1.0f);
    reverb.setWet(2.0f);
    reverb.setDry(-1.0f);
    if (!check(near(reverb.getRoomSize(), 1.0f) && near(reverb.getDamp(), 0.0f)
               && near(reverb.getWet(), 1.0f) && near(reverb.getDry(), 0.0f),
               "native Reverb setters clamp the opposite endpoints"))
        return false;

    for (int channels : {1, 2})
        for (double rate : {44100.0, 48000.0})
            for (int block : {128, 512})
            {
                const int samples = static_cast<int>(rate);
                const auto input = signal(channels, samples, rate);
                Audio silence(channels, samples);
                silence.clear();

                reverb.setWet(0.0f);
                reverb.setDry(0.5f); // JUCE doubles the native dry value.
                const auto dry = process(reverb, input, rate, block);
                // JUCE smooths gain changes over 10 ms.
                const int settled = static_cast<int>(rate * 0.02);
                bool dryMatches = finite(dry);
                for (int channel = 0; channel < channels; ++channel)
                    for (int i = settled; i < samples; ++i)
                        dryMatches &= near(dry.getSample(channel, i), input.getSample(channel, i));

                reverb.setWet(1.0f / 3);
                reverb.setDry(0.0f);
                reverb.setRoomSize(0.25f);
                reverb.setDamp(0.2f);
                const auto wet = process(reverb, input, rate, block);
                reverb.setRoomSize(0.8f);
                const auto larger = process(reverb, input, rate, block);
                reverb.setDamp(0.85f);
                const auto damped = process(reverb, input, rate, block);
                const auto silent = process(reverb, silence, rate, block);
                const int responseStart = static_cast<int>(rate * 0.1);

                if (!check(dryMatches, "Reverb dry endpoint after gain smoothing")
                    || !check(finite(wet) && wet.getMagnitude(settled, samples - settled) > 0.001f
                              && !equivalent(wet, input, 0.001f), "Reverb wet endpoint produces processed audio")
                    || !check(finite(larger) && !equivalent(wet, larger, 0.001f, responseStart),
                              "Room Size changes wet output")
                    || !check(finite(damped) && !equivalent(larger, damped, 0.001f, responseStart),
                              "Damping changes wet output")
                    || !check(equivalent(silent, silence, 0.0f), "fresh Reverb remains silent on silence"))
                {
                    std::fprintf(stderr, "  %d channel(s), %.0f Hz, %d samples/block\n", channels, rate, block);
                    return false;
                }
            }
    return true;
}

bool testCompressorParameters(te::CompressorPlugin& compressor)
{
    if (!check(near(compressor.getThreshold(), ju::Decibels::decibelsToGain(-6.0f))
               && near(compressor.getRatio(), 0.5f) && compressor.isEnabled(),
               "native Compressor defaults"))
        return false;

    compressor.setThreshold(-1.0f);
    compressor.setRatio(-1.0f);
    if (!check(near(compressor.getThreshold(), 0.01f) && near(compressor.getRatio(), 0.05f),
               "Compressor public setters clamp low values"))
        return false;

    compressor.setThreshold(2.0f);
    compressor.setRatio(2.0f);
    return check(near(compressor.getThreshold(), 1.0f) && near(compressor.getRatio(), 0.95f),
                 "Compressor parameter range further limits the public ratio setter");
}

bool insertFixture(te::AudioTrack& track, const ju::File& file)
{
    return track.insertWaveClip("Effects input", file,
        {{ {}, te::TimePosition::fromSeconds(1.0) }, {}}, false) != nullptr;
}

bool writeFixture(const ju::File& file)
{
    ju::WavAudioFormat wave;
    auto stream = file.createOutputStream();
    if (!stream)
        return false;
    std::unique_ptr<ju::AudioFormatWriter> writer(wave.createWriterFor(stream.get(), 44100, 2, 24, {}, 0));
    if (!writer)
        return false;
    stream.release();
    return writer->writeFromAudioSampleBuffer(signal(2, 44100, 44100), 0, 44100);
}

Audio render(te::AudioTrack& track, const ju::File& destination)
{
    auto& edit = track.edit;
    ju::WavAudioFormat wave;
    const te::Edit::ScopedRenderStatus rendering(edit, true);
    te::Renderer::Parameters options(edit);
    options.destFile = destination;
    options.audioFormat = &wave;
    const int trackIndex = te::getAllTracks(edit).indexOf(&track);
    if (trackIndex < 0)
        return {};
    // The pinned toBitSet helper sets bits for every track in the Edit.
    options.tracksToDo.setBit(trackIndex);
    options.time = {{}, te::TimePosition::fromSeconds(1.0)};
    options.bitDepth = 24;
    options.sampleRateForAudio = 44100.0;
    options.blockSizeForAudio = 512;
    options.canRenderInMono = false;
    if (te::Renderer::renderToFile("Test effects", options) != destination)
        return {};

    auto stream = destination.createInputStream();
    if (!stream)
        return {};
    std::unique_ptr<ju::AudioFormatReader> reader(wave.createReaderFor(stream.release(), true));
    if (!reader || reader->numChannels != 2 || reader->lengthInSamples < 44100)
        return {};
    Audio result(2, 44100);
    if (!reader->read(&result, 0, result.getNumSamples(), 0, true, true))
        return {};
    return result;
}

bool testChain(te::Engine& engine, const ju::File& folder)
{
    const auto path = folder.getChildFile("effects.tracktionedit");
    const auto fixture = folder.getChildFile("input.wav");
    if (!check(writeFixture(fixture), "write stereo fixture"))
        return false;

    auto edit = te::createEmptyEdit(engine, path);
    edit->ensureNumberOfAudioTracks(2);
    const auto tracks = te::getAudioTracks(*edit);
    if (!check(tracks.size() == 2 && insertFixture(*tracks[0], fixture) && insertFixture(*tracks[1], fixture),
               "create tracks and insert fixtures"))
        return false;
    auto& track = *tracks[0];
    auto& otherTrack = *tracks[1];
    auto renderTrack = [&](te::AudioTrack& source, const char* name)
    {
        return render(source, folder.getChildFile(name));
    };
    const auto dry = renderTrack(track, "dry.wav");
    const auto otherDry = renderTrack(otherTrack, "other-dry.wav");
    if (!check(finite(dry) && dry.getMagnitude(0, dry.getNumSamples()) > 0.1f
               && equivalent(dry, otherDry, 1.0e-5f), "audible matching dry track renders"))
        return false;

    auto reverb = add<te::ReverbPlugin>(track, 0);
    auto compressor = add<te::CompressorPlugin>(track, 1);
    auto otherReverb = add<te::ReverbPlugin>(otherTrack, 0);
    if (!check(reverb && compressor && otherReverb && reverb->itemID != otherReverb->itemID,
               "create native effects with independent identities")
        || !testReverb(*reverb) || !testCompressorParameters(*compressor))
        return false;

    reverb->setRoomSize(0.8f);
    reverb->setDamp(0.25f);
    reverb->setWet(0.3f);
    reverb->setDry(0.2f);
    compressor->setThreshold(0.04f);
    compressor->setRatio(0.15f);
    compressor->attackMs.setParameter(2.0f, ju::sendNotification);
    compressor->outputDb.setParameter(6.0f, ju::sendNotification);
    otherReverb->setEnabled(false);

    constexpr float renderTolerance = 1.0e-5f;
    constexpr float signalDifference = 0.001f;
    const auto otherBypassed = renderTrack(otherTrack, "other-bypassed.wav");
    if (!check(near(otherReverb->getRoomSize(), 0.3f) && near(otherReverb->getDamp(), 0.5f)
               && near(otherReverb->getWet(), 1.0f / 3) && near(otherReverb->getDry(), 0.5f)
               && reverb->isEnabled() && compressor->isEnabled() && !otherReverb->isEnabled(),
               "other track retains default parameters and independent bypass state")
        || !check(equivalent(otherDry, otherBypassed, renderTolerance),
                  "bypassed second track matches its dry render"))
        return false;

    const auto reverbFirst = renderTrack(track, "reverb-first.wav");
    otherReverb->setEnabled(true);
    otherReverb->setRoomSize(0.1f);
    otherReverb->setDry(0.1f);
    const auto otherProcessed = renderTrack(otherTrack, "other-processed.wav");
    if (!check(finite(otherProcessed) && !equivalent(otherProcessed, otherDry, signalDifference)
               && near(reverb->getRoomSize(), 0.8f) && near(reverb->getDry(), 0.2f),
               "second Reverb processes its track using independent parameters"))
        return false;
    otherReverb->setEnabled(false);
    auto& list = track.pluginList;
    const auto compID = compressor->itemID;
    const auto reverbID = reverb->itemID;
    const auto otherID = otherReverb->itemID;
    list.state.moveChild(list.state.indexOf(compressor->state), list.state.indexOf(reverb->state),
                         &edit->getUndoManager());
    const auto compressorFirst = renderTrack(track, "compressor-first.wav");
    // Compare after the startup transient so gain smoothing cannot pass the order check.
    constexpr int settledStart = 4410;
    if (!check(list[0] == compressor.get() && list[1] == reverb.get()
               && finite(reverbFirst) && finite(compressorFirst)
               && !equivalent(reverbFirst, compressorFirst, signalDifference, settledStart),
               "Tracktion graph renders the two orders differently with the same instances"))
        return false;

    reverb->setEnabled(false);
    const auto compressorOnly = renderTrack(track, "compressor-only.wav");
    compressor->setEnabled(false);
    const auto bypassed = renderTrack(track, "bypassed.wav");
    reverb->setEnabled(true);
    const auto reverbOnly = renderTrack(track, "reverb-only.wav");
    compressor->setEnabled(true);
    const auto restored = renderTrack(track, "restored.wav");
    if (!check(finite(compressorOnly) && finite(reverbOnly)
               && !equivalent(compressorOnly, dry, signalDifference)
               && !equivalent(reverbOnly, dry, signalDifference)
               && !equivalent(compressorOnly, compressorFirst, signalDifference)
               && !equivalent(reverbOnly, compressorFirst, signalDifference),
               "each effect processes audio independently of the other's bypass")
        || !check(equivalent(bypassed, dry, renderTolerance), "both bypassed effects match the dry render")
        || !check(equivalent(restored, compressorFirst, renderTolerance), "re-enabling restores chain output"))
        return false;

    compressor->setEnabled(false);
    if (!check(te::EditFileOperations(*edit).save(false, true, false), "save effects project"))
        return false;

    reverb = nullptr;
    compressor = nullptr;
    otherReverb = nullptr;
    edit.reset();
    edit = te::loadEditFromFile(engine, path);
    if (!check(edit != nullptr, "reopen effects project"))
        return false;
    const auto reopened = te::getAudioTracks(*edit);
    if (!check(reopened.size() == 2, "restore both tracks"))
        return false;
    auto& reopenedList = reopened[0]->pluginList;
    auto& otherList = reopened[1]->pluginList;
    auto* savedCompressor = dynamic_cast<te::CompressorPlugin*>(reopenedList[0]);
    auto* savedReverb = dynamic_cast<te::ReverbPlugin*>(reopenedList[1]);
    auto* savedOther = dynamic_cast<te::ReverbPlugin*>(otherList[0]);
    if (!check(savedCompressor && savedReverb && savedOther && savedCompressor->itemID == compID
               && savedReverb->itemID == reverbID && savedOther->itemID == otherID,
               "reopen ordered effects with stable IDs on both tracks")
        || !check(!savedCompressor->isEnabled() && savedReverb->isEnabled() && !savedOther->isEnabled()
                  && near(savedCompressor->getThreshold(), 0.04f) && near(savedCompressor->getRatio(), 0.15f)
                  && near(savedCompressor->attackMs.getCurrentValue(), 2.0f)
                  && near(savedCompressor->outputDb.getCurrentValue(), 6.0f)
                  && near(savedReverb->getRoomSize(), 0.8f) && near(savedReverb->getDamp(), 0.25f)
                  && near(savedReverb->getWet(), 0.3f) && near(savedReverb->getDry(), 0.2f)
                  && near(savedOther->getRoomSize(), 0.1f) && near(savedOther->getDry(), 0.1f),
                  "reopen parameters and individual bypass state")
        || !check(equivalent(reverbOnly, renderTrack(*reopened[0], "reopened.wav"),
                              renderTolerance, settledStart),
                  "reopened settings reproduce the settled chain output"))
        return false;

    savedCompressor->setEnabled(true);
    if (!check(equivalent(compressorFirst, renderTrack(*reopened[0], "reopened-enabled.wav"),
                          renderTolerance, settledStart),
               "reopened Compressor processes with saved parameters"))
        return false;
    const int effectsBeforeRemoval = reopenedList.size();
    savedCompressor->deleteFromParent();
    savedReverb->deleteFromParent();
    return check(reopenedList.size() == effectsBeforeRemoval - 2 && otherList[0] == savedOther
                 && equivalent(dry, renderTrack(*reopened[0], "removed.wav"), renderTolerance),
                 "remove effects and restore dry audio on the target track");
}
} // namespace

bool runEffectsFoundationTest(te::Engine& engine)
{
    const auto folder = ju::File::getSpecialLocation(ju::File::tempDirectory)
        .getNonexistentChildFile("SimpleDAW-effects", {});
    if (!check(folder.createDirectory().wasOk(), "create temporary test directory"))
        return false;
    const ju::ScopeGuard cleanup{[&] { folder.deleteRecursively(); }};
    return testChain(engine, folder);
}
