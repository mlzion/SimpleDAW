// SPDX-FileCopyrightText: 2026 the SimpleDAW contributors
// SPDX-License-Identifier: AGPL-3.0-only

#include <JuceHeader.h>
#include <numeric>

namespace te = tracktion;
namespace ju = juce;

namespace
{
constexpr int maxTracks = 24;

class SimpleDawUIBehaviour final : public te::UIBehaviour
{
public:
    void runTaskWithProgressBar(te::ThreadPoolJobWithProgress& job) override
    {
        while (job.runJob() == ju::ThreadPoolJob::jobNeedsRunningAgain)
        {
        }
    }
};

class SmokeTestEngineBehaviour final : public te::EngineBehaviour
{
public:
    bool autoInitialiseDeviceManager() override { return false; }
};

bool insertAudio(te::AudioTrack& track, const ju::File& file)
{
    te::AudioFile audio(track.edit.engine, file);
    if (!audio.isValid() || audio.getLength() <= 0.0)
        return false;

    return track.insertWaveClip(file.getFileNameWithoutExtension(), file,
        {{ {}, te::TimeDuration::fromSeconds(audio.getLength()) }, {}}, false) != nullptr;
}

template<typename PluginType>
bool insertBuiltIn(te::AudioTrack& track, bool synth = false)
{
    auto plugin = track.edit.getPluginCache().createNewPlugin(PluginType::xmlTypeName,
        te::PluginManager::createBuiltInPluginDescription<PluginType>(synth));
    if (plugin == nullptr)
        return false;

    track.pluginList.insertPlugin(plugin, 0, nullptr);
    return true;
}

bool exportWave(te::Edit& edit, const ju::File& destination)
{
    if (edit.getLength().inSeconds() <= 0.0)
        return false;

    ju::TemporaryFile output(destination);
    ju::WavAudioFormat wave;
    const te::Edit::ScopedRenderStatus rendering(edit, true);
    te::Renderer::Parameters options(edit);
    options.destFile = output.getFile();
    options.audioFormat = &wave;
    options.time = {{}, edit.getLength()};
    options.tracksToDo = te::toBitSet(te::getAllTracks(edit));
    options.canRenderInMono = false;
    options.useMasterPlugins = true;
    options.bitDepth = 24;
    return te::Renderer::renderToFile("Export stereo WAV", options) == output.getFile()
        && output.overwriteTargetFileWithTemporary();
}

bool smokeTest(te::Engine& engine)
{
    auto folder = ju::File::getSpecialLocation(ju::File::tempDirectory)
        .getNonexistentChildFile("SimpleDAW-test", {});
    if (folder.createDirectory().failed())
        return false;

    const ju::ScopeGuard cleanup{[&] { folder.deleteRecursively(); }};
    auto input = folder.getChildFile("input.wav");
    ju::WavAudioFormat wave;
    {
        std::unique_ptr<ju::OutputStream> stream = input.createOutputStream();
        if (!stream)
            return false;
        std::unique_ptr<ju::AudioFormatWriter> writer(
            wave.createWriterFor(stream.get(), 44100.0, 1, 16, {}, 0));
        if (!writer)
            return false;
        stream.release();
        ju::AudioBuffer<float> samples(1, 4410);
        for (int i = 0; i < samples.getNumSamples(); ++i)
            samples.setSample(0, i, 0.15f * std::sin(ju::MathConstants<float>::twoPi * i / 100.0f));
        if (!writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()))
            return false;
    }

    auto path = folder.getChildFile("test.tracktionedit");
    auto edit = te::createEmptyEdit(engine, path);
    edit->ensureNumberOfAudioTracks(2);
    const auto tracks = te::getAudioTracks(*edit);
    if (tracks.size() != 2 || !insertAudio(*tracks[0], input) || !insertAudio(*tracks[1], input)
        || !insertBuiltIn<te::ReverbPlugin>(*tracks[0])
        || !te::EditFileOperations(*edit).save(false, true, false))
        return false;

    edit.reset();
    edit = te::loadEditFromFile(engine, path);
    const auto reopened = te::getAudioTracks(*edit);
    if (reopened.size() != 2 || reopened[0]->getClips().size() != 1 || reopened[1]->getClips().size() != 1)
        return false;

    bool reverbReopened = false;
    for (auto* plugin : reopened[0]->pluginList.getPlugins())
        reverbReopened |= plugin->getPluginType() == te::ReverbPlugin::xmlTypeName;
    if (!reverbReopened)
        return false;

    auto output = folder.getChildFile("mix.wav");
    if (!exportWave(*edit, output))
        return false;

    std::unique_ptr<ju::AudioFormatReader> reader(wave.createReaderFor(output.createInputStream().release(), true));
    if (!reader || reader->numChannels != 2 || reader->sampleRate != 44100.0
        || reader->bitsPerSample != 24 || reader->lengthInSamples < 4410)
        return false;

    ju::AudioBuffer<float> samples(2, 4410);
    return reader->read(&samples, 0, samples.getNumSamples(), 0, true, true)
        && samples.getMagnitude(0, samples.getNumSamples()) > 0.01f;
}

class MainGui final : public ju::Component, private ju::Timer, private ju::MenuBarModel,
                      private ju::ApplicationCommandTarget
{
    #include "MainGui.inc"

private:
    using FileAction = std::function<void(const ju::File&)>;

    void report(const ju::String& text)
    {
        status.setText(text, ju::dontSendNotification);
    }

    bool ready()
    {
        if (!edit)
            report("Create or open a project first");
        else if (edit->getTransport().isRecording())
            report("Stop recording first");
        else if (edit->getTransport().isPlaying())
            report("Stop playback first");
        else
            return true;
        return false;
    }

    void chooseFile(const ju::String& title, const ju::String& filter, int flags, FileAction action)
    {
        chooser = std::make_unique<ju::FileChooser>(title,
            ju::File::getSpecialLocation(ju::File::userDocumentsDirectory), filter);
        chooser->launchAsync(flags, [action = std::move(action)](const ju::FileChooser& dialog) {
            if (auto file = dialog.getResult(); file != ju::File{})
                action(file);
        });
    }

    void chooseProject(bool create)
    {
        if (edit && !ready())
            return;

        chooseFile(create ? "Create project" : "Open project", "*.tracktionedit",
            ju::FileBrowserComponent::canSelectFiles
                | (create ? ju::FileBrowserComponent::saveMode : ju::FileBrowserComponent::openMode),
            [this, create](const ju::File& choice) {
                const auto file = create ? choice.withFileExtension("tracktionedit") : choice;
                if (create && file.exists())
                    return report("Choose a new project filename");
                if (!create)
                {
                    const auto xml = ju::parseXML(file);
                    if (!xml || !xml->hasTagName("EDIT"))
                        return report("Could not read this project");
                }
                if (edit && !save())
                    return;

                auto next = create ? te::createEmptyEdit(engine, file) : te::loadEditFromFile(engine, file);
                if (!next)
                    return report("Could not open project");
                if (create)
                {
                    next->ensureNumberOfAudioTracks(1);
                    if (!te::EditFileOperations(*next).save(false, true, false))
                        return report("Could not create project file");
                }
                setProject(std::move(next), file);
                report("Project ready");
            });
    }

    te::AudioTrack* selectedTrack()
    {
        if (!ready())
            return nullptr;
        auto* track = trackAt(selection);
        if (!track)
            report("Select a track first");
        return track;
    }

    void configureDevices()
    {
        if (edit && !ready())
            return;

        ju::DialogWindow::LaunchOptions dialog;
        dialog.dialogTitle = "Audio and MIDI devices";
        dialog.dialogBackgroundColour = ju::Colours::darkgrey;
        dialog.content.setOwned(new ju::AudioDeviceSelectorComponent(engine.getDeviceManager().deviceManager,
            0, 32, 2, 32, true, false, true, false));
        dialog.content->setSize(480, 560);
        dialog.runModal();
    }

    void armInput(bool midi)
    {
        auto* track = selectedTrack();
        if (!track)
            return;

        auto& devices = engine.getDeviceManager();
        if (midi)
        {
            for (const auto& input : devices.getMidiInDevices())
                if (input->getDeviceType() == te::InputDevice::physicalMidiDevice)
                    input->setEnabled(true);
        }
        else
        {
            for (int i = 0; i < devices.getNumWaveInDevices(); ++i)
                if (auto* input = devices.getWaveInDevice(i))
                    input->setEnabled(true);
        }

        edit->getTransport().ensureContextAllocated();
        ju::PopupMenu menu;
        ju::Array<te::InputDeviceInstance*> choices;
        const auto type = midi ? te::InputDevice::physicalMidiDevice : te::InputDevice::waveDevice;
        for (auto* instance : edit->getAllInputDevices())
        {
            auto& device = instance->getInputDevice();
            if (device.getDeviceType() == type)
            {
                choices.add(instance);
                menu.addItem(choices.size(), device.getName());
            }
        }

        if (choices.isEmpty())
            return report("No input available. Check Devices and try again");

        const int choice = menu.show();
        if (choice == 0)
            return;

        auto* instance = choices[choice - 1];
        if (instance->isRecordingEnabled(track->itemID))
        {
            instance->setRecordingEnabled(track->itemID, false);
            return report("Input disarmed");
        }

        const auto target = instance->setTarget(track->itemID, true, &edit->getUndoManager(), 0);
        if (!target)
            return report(target.error());

        instance->setRecordingEnabled(track->itemID, true);
        const auto inputName = instance->getInputDevice().getName();
        edit->restartPlayback();
        report(inputName + " armed on " + track->getName());
    }

    void record()
    {
        if (!ready())
            return;

        if (edit->recordingPunchInOut && !timeSelection)
            return report("Drag across the ruler to select the punch range");
        if (edit->recordingPunchInOut)
            edit->getTransport().setLoopRange(*timeSelection);
        if (paused)
            editCursor = edit->getTransport().getPosition();

        for (auto* instance : edit->getAllInputDevices())
            for (auto* track : te::getAudioTracks(*edit))
                if (instance->isRecordingEnabled(track->itemID))
                {
                    edit->getTransport().record(false);
                    report(edit->getTransport().isRecording() ? "Recording" : "Could not start recording");
                    return;
                }

        report("Arm an audio or MIDI input first");
    }

    void stop()
    {
        if (!edit)
            return;

        const bool recording = edit->getTransport().isRecording();
        edit->getTransport().stop(false, false);
        edit->getTransport().setPosition(editCursor);
        paused = false;
        if (recording)
            save();
    }

    bool save()
    {
        if (!ready())
            return false;

        const bool saved = te::EditFileOperations(*edit).save(false, true, false);
        report(saved ? "Project saved" : "Could not save project");
        return saved;
    }

    void importWave()
    {
        if (!selectedTrack())
            return;

        chooseFile("Import WAV at start of selected track", "*.wav",
            ju::FileBrowserComponent::openMode | ju::FileBrowserComponent::canSelectFiles,
            [this](const ju::File& file) {
                if (auto* track = selectedTrack())
                    report(insertAudio(*track, file) ? "WAV imported" : "Could not import WAV");
            });
    }

    void loadVst3()
    {
        if (!selectedTrack())
            return;

        chooseFile("Select an installed VST3", "*.vst3",
            ju::FileBrowserComponent::openMode | ju::FileBrowserComponent::canSelectFiles
                | ju::FileBrowserComponent::canSelectDirectories,
            [this](const ju::File& file) {
                auto* track = selectedTrack();
                if (!track || !file.hasFileExtension("vst3"))
                    return;

                auto& manager = engine.getPluginManager();
                ju::OwnedArray<ju::PluginDescription> descriptions;
                for (auto* format : manager.pluginFormatManager.getFormats())
                    if (format->getName() == "VST3")
                        manager.knownPluginList.scanAndAddFile(file.getFullPathName(), false, descriptions, *format);

                if (descriptions.isEmpty())
                    return report("No VST3 found at this path");

                ju::PopupMenu menu;
                for (int i = 0; i < descriptions.size(); ++i)
                    menu.addItem(i + 1, descriptions[i]->name);

                if (const int choice = menu.show(); choice > 0)
                {
                    auto plugin = edit->getPluginCache().createNewPlugin(
                        te::ExternalPlugin::xmlTypeName, *descriptions[choice - 1]);
                    if (plugin == nullptr || plugin->getWrappedAudioProcessor() == nullptr)
                        return report("Could not load VST3");

                    track->pluginList.insertPlugin(plugin, 0, nullptr);
                    report("Inserted " + descriptions[choice - 1]->name);
                }
            });
    }

    void exportProject()
    {
        if (!ready())
            return;

        chooseFile("Export stereo WAV", "*.wav",
            ju::FileBrowserComponent::saveMode | ju::FileBrowserComponent::canSelectFiles,
            [this](const ju::File& file) {
                if (file.withFileExtension("wav").exists())
                    return report("Choose a new WAV filename");
                if (ready())
                    report(exportWave(*edit, file.withFileExtension("wav")) ? "WAV exported" : "WAV export failed");
            });
    }

    std::unique_ptr<ju::FileChooser> chooser;
};

class Application final : public ju::JUCEApplication
{
public:
    const ju::String getApplicationName() override { return "SimpleDAW"; }
    const ju::String getApplicationVersion() override { return "0.21.0"; }

    void initialise(const ju::String& command) override
    {
        if (te::PluginManager::startChildProcessPluginScan(command))
            return;

        const bool testing = command == "--smoke-test" || command == "--gui-test";
        std::unique_ptr<te::EngineBehaviour> engineBehaviour;
        if (testing)
            engineBehaviour = std::make_unique<SmokeTestEngineBehaviour>();
        engine = std::make_unique<te::Engine>(testing ? "SimpleDAWSmokeTest" : "SimpleDAW",
            std::make_unique<SimpleDawUIBehaviour>(), std::move(engineBehaviour));
        if (testing)
        {
            setApplicationReturnValue((command == "--gui-test" ? MainGui::test(*engine) : smokeTest(*engine)) ? 0 : 1);
            quit();
            return;
        }

        window = std::make_unique<Window>();
        window->setUsingNativeTitleBar(true);
        window->setContentOwned(new MainGui(*engine), true);
        window->setResizable(true, false);
        window->setResizeLimits(1080, 640, 7680, 4320);
        window->centreWithSize(window->getWidth(), window->getHeight());
        window->setVisible(true);
    }

    void shutdown() override
    {
        window.reset();
        engine.reset();
    }

private:
    struct Window final : ju::DocumentWindow
    {
        Window() : DocumentWindow("SimpleDAW v0.21", ju::Colours::black, allButtons) {}
        void closeButtonPressed() override { ju::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<Window> window;
};
}

START_JUCE_APPLICATION(Application)
